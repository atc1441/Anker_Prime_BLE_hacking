// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Front-end screens (r_screen.h). Linked into each front-end overlay, not resident. The record
// pass keeps its hashes in a compose buffer and the dirty groups in 8 B, the text walker reads
// its string through 32 B, so no screen needs more stack than that.

#include "doomdef.h"
#include "hu_stuff.h"

#include "fe.h"
#include "fetch.h"
#include "lcd.h"
#include "r_screen.h"
#include "r_strip.h"

#define GROUPS (SCREENWIDTH / 8)
#define FNV_PRIME 16777619u

const scr_win_t scr_full = {0, SCREENWIDTH - 1, 0, SCREENHEIGHT - 1};

int R_ScreenDraw(int id, const scr_win_t *win, void (*draw)(void), scr_win_t *box)
{
    uint32_t dirty[2] = {0, 0};
    int bytes, g, e;

    // a piece of the band may still be on its way out of a compose buffer
    pump_sync(0);
    uint32_t *hash = (uint32_t *)(void *)R_BandBuffer(&bytes);
    // every group starts from the screen and its window
    uint32_t h0 = (2166136261u ^ (uint32_t)id) * FNV_PRIME;
    h0 = (h0 ^ ((uint32_t)win->x0 | (uint32_t)win->x1 << 16)) * FNV_PRIME;
    h0 = (h0 ^ ((uint32_t)win->y0 | (uint32_t)win->y1 << 16)) * FNV_PRIME;
    for (g = 0; g < GROUPS; g++)
        hash[g] = h0;
    r_bandwin.buf = (uint8_t *)hash;
    r_bandwin.cx0 = win->x0;
    r_bandwin.cx1 = win->x1;
    r_bandwin.cy0 = win->y0;
    r_bandwin.cy1 = win->y1;
    r_bandwin.x0 = SCREENWIDTH;
    r_bandwin.x1 = -1;
    r_bandwin.y0 = SCREENHEIGHT;
    r_bandwin.h = -1;
    r_bandmode = BAND_HASH;
    draw();
    r_bandmode = BAND_OFF;
    if (box) {
        box->x0 = (int16_t)(r_bandwin.x0 < 0 ? 0 : r_bandwin.x0);
        box->x1 = (int16_t)(r_bandwin.x1 >= SCREENWIDTH ? SCREENWIDTH - 1 : r_bandwin.x1);
        box->y0 = (int16_t)(r_bandwin.y0 < 0 ? 0 : r_bandwin.y0);
        box->y1 = (int16_t)(r_bandwin.h >= SCREENHEIGHT ? SCREENHEIGHT - 1 : r_bandwin.h);
        if (box->x0 <= box->x1 && box->y0 <= box->y1
            && (box->x0 < win->x0 || box->x1 > win->x1 || box->y0 < win->y0 || box->y1 > win->y1))
            return 0;
    }

    // the hashes are kept before the compose pass takes the buffer
    for (g = win->x0 >> 3; g <= win->x1 >> 3; g++) {
        if (!(fe.flags & FE_HASHED) || hash[g] != fe_scr.hash[g])
            dirty[g >> 5] |= 1u << (g & 31);
        fe_scr.hash[g] = hash[g];
    }
    fe.flags |= FE_HASHED;

    // each run of dirty groups is one window of the pump
    for (g = 0; g < GROUPS; g = e + 1) {
        e = g;
        if (!(dirty[g >> 5] >> (g & 31) & 1))
            continue;
        while (e + 1 < GROUPS && dirty[(e + 1) >> 5] >> ((e + 1) & 31) & 1)
            e++;
        R_BandCompose(8 * g > win->x0 ? 8 * g : win->x0, 8 * e + 7 < win->x1 ? 8 * e + 7 : win->x1,
                      win->y0, win->y0, win->y1 - win->y0 + 1, draw);
    }
    return 1;
}

int scr_glyph(int c)
{
    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
    c -= HU_FONTSTART;
    return c < 0 || c >= HU_FONTSIZE ? -1 : c;
}

void scr_text(int id, uint32_t off, int x, int y, int lh, int count,
              int (*glyph)(int c, int x, int y))
{
    char buf[32];
    int i = 0, n = 0, more = 1, cx = x;

    while (count--) {
        if (i == n) {
            if (!more)
                return;
            n = str_part(id, off, buf, sizeof(buf));
            more = n == (int)sizeof(buf);
            off += (uint32_t)n;
            i = 0;
            if (!n)
                return;
        }
        int c = buf[i++];
        if (c == '\n') {
            cx = x;
            y += lh;
            continue;
        }
        c = scr_glyph(c);
        if (c < 0) {
            cx += 4;
            continue;
        }
        int w = glyph(c, cx, y);
        if (w < 0)
            return;
        cx += w;
    }
}
