// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Status band compositor. The band under the 3D view is
// the HUD message line (LCD y 200..207) and the status bar (y 208..239).
// There is no band buffer: the vanilla status bar code runs twice per frame.
//   record   ST_Drawer with V_DrawPatch and V_CopyRect marking the screen
//            columns they would touch (st_lib redraws a widget only when its
//            value changed), HU_Drawer marking the message line when its
//            text changed
//   compose  for each run of dirty columns, cut into pieces that fit a strip
//            buffer: the background and every widget in its current state
//            drawn clipped to the piece (ST_DrawBand, HU_DrawBand), then sent
//            through the pump
// A palette change redraws the whole band, so tints cover the whole screen.
// The front-end screens (port/render/r_screen.c) use the same compose pass,
// their record pass hashes the draw calls per group of 8 columns.

#include <string.h>

#include "doomdef.h"
#include "hu_stuff.h"
#include "i_video.h"
#include "st_stuff.h"

#include "fe.h"
#include "lcd.h"
#include "ovl.h"
#include "r_strip.h"

int r_bandmode;
r_bandwin_t r_bandwin;

static uint32_t bar_dirty[(SCREENWIDTH + 31) / 32];
static int hud_dirty, band_all = 1, hud_shown, bar_shown;

void R_BandMark(int x0, int x1)
{
    if (x0 < 0)
        x0 = 0;
    if (x1 >= SCREENWIDTH)
        x1 = SCREENWIDTH - 1;
    for (int x = x0; x <= x1; x++)
        bar_dirty[x >> 5] |= 1u << (x & 31);
}

void R_BandMarkHud(void)
{
    hud_dirty = 1;
}

void R_ScreenMark(int x0, int x1, int y0, int y1, uint32_t key)
{
    uint32_t *hash = (uint32_t *)r_bandwin.buf;
    if (x0 < r_bandwin.x0)
        r_bandwin.x0 = x0;
    if (x1 > r_bandwin.x1)
        r_bandwin.x1 = x1;
    if (y0 < r_bandwin.y0)
        r_bandwin.y0 = y0;
    if (y1 > r_bandwin.h)
        r_bandwin.h = y1;
    if (x0 < r_bandwin.cx0)
        x0 = r_bandwin.cx0;
    if (x1 > r_bandwin.cx1)
        x1 = r_bandwin.cx1;
    if (y0 < r_bandwin.cy0)
        y0 = r_bandwin.cy0;
    if (y1 > r_bandwin.cy1)
        y1 = r_bandwin.cy1;
    if (x0 > x1 || y0 > y1)
        return;
    // FNV-1a over the keys of the calls, a word at a time, per group
    for (x0 >>= 3; x0 <= x1 >> 3; x0++)
        hash[x0] = (hash[x0] ^ key) * 16777619u;
}

void R_PaletteChanged(void)
{
    R_PumpIdle();
    band_all = 1;
    fe.flags &= ~FE_HASHED;
}

void R_BandCompose(int x0, int x1, int lcdy, int y0, int h, void (*draw)(void))
{
    R_PumpWindow(x0, x1, lcdy, lcdy + h - 1, 0);
    for (int a = x0; a <= x1;) {
        int bytes;
        uint8_t *buf = R_BandBuffer(&bytes);
        int b = a + bytes / h - 1;
        if (b > x1)
            b = x1;
        memset(buf, 0, (size_t)(b - a + 1) * h);
        if (draw) {
            r_bandwin.buf = buf;
            r_bandwin.x0 = a;
            r_bandwin.x1 = b;
            r_bandwin.y0 = y0;
            r_bandwin.h = h;
            r_bandwin.cx0 = a;
            r_bandwin.cx1 = b;
            r_bandwin.cy0 = y0;
            r_bandwin.cy1 = y0 + h - 1;
            r_bandmode = BAND_COMPOSE;
            draw();
            r_bandmode = BAND_OFF;
        }
        pump_submit(buf, b - a + 1);
        RSTAT(band_cols, b - a + 1);
        RSTAT(band_pieces, 1);
        a = b + 1;
    }
}

void R_BandFlush(int hud, int bar)
{
    // A frame with the 3D view composes in its strip buffers, which lie in the overlay window.
    // The NetUpdate of D_Display may have loaded an overlay since R_DrawFrame (a key that opens
    // the menu), the pieces overwrite it.
    if (fe.flags & FE_VIEW)
        ovl_invalidate();
    // the layout first: a change of it ends the pump window
    R_FrameSetup();
    if (band_all) {
        band_all = 0;
        // a part that is shown is composed whole, one that is not is cleared again
        hud_shown = !hud;
        bar_shown = !bar;
    }
    if (hud && (hud_dirty || !hud_shown))
        R_BandCompose(0, SCREENWIDTH - 1, LCD_HUD_Y, HU_MSGY, LCD_BAR_Y - LCD_HUD_Y, HU_DrawBand);
    else if (!hud && hud_shown)
        R_BandCompose(0, SCREENWIDTH - 1, LCD_HUD_Y, HU_MSGY, LCD_BAR_Y - LCD_HUD_Y, NULL);
    hud_shown = hud;
    hud_dirty = 0;

    if (bar && !bar_shown)
        R_BandMark(0, SCREENWIDTH - 1);
    if (bar) {
        for (int x = 0; x < SCREENWIDTH;) {
            if (!(bar_dirty[x >> 5] & (1u << (x & 31)))) {
                x++;
                continue;
            }
            int e = x;
            while (e + 1 < SCREENWIDTH && (bar_dirty[(e + 1) >> 5] & (1u << ((e + 1) & 31))))
                e++;
            R_BandCompose(x, e, LCD_BAR_Y, ST_Y, ST_HEIGHT, ST_DrawBand);
            x = e + 1;
        }
    } else if (bar_shown) {
        R_BandCompose(0, SCREENWIDTH - 1, LCD_BAR_Y, ST_Y, ST_HEIGHT, NULL);
    }
    bar_shown = bar;
    memset(bar_dirty, 0, sizeof(bar_dirty));
}
