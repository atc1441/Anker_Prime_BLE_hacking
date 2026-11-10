// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Front-end screens without a framebuffer (port/render/r_screen.c, linked into each front-end
// overlay). A screen is what its drawer draws with V_DrawPatch (and marks and fills of its
// own). The drawer runs once in the record pass (BAND_HASH, r_strip.h), which hashes its draw
// calls per group of 8 columns, then once per piece of every run of groups whose hash is not
// the one sent last (fe_scr.hash, port/game/fe.h), in the compose pass of the status band
// (BAND_COMPOSE). A screen that does not change costs the record pass only.

#pragma once

#include <stdint.h>

// Window of the 320x200 screen a picture covers, y1 - y0 + 1 a multiple of 4
typedef struct {
    int16_t x0, x1, y0, y1;
} scr_win_t;

// The whole screen
extern const scr_win_t scr_full;

// Draws screen id (FE_SCR_*) into win: the groups whose draw calls changed, all of the window
// when the screen, the window or the palette changed. Returns 1. box, when not NULL, receives
// the box of the draw calls on the screen (the record pass), and when it does not fit in win
// nothing is drawn and 0 returned: the caller draws again into a window that holds it.
int R_ScreenDraw(int id, const scr_win_t *win, void (*draw)(void), scr_win_t *box);

// The HU font index of character c (a lower case letter takes its capital), -1 when the font has
// no glyph for it. The text writers of the front end move on 4 columns for such a character.
int scr_glyph(int c);

// Lays out at most count characters (newlines included, count < 0 for all) of string id of the
// image from byte off on, as the text writers of vanilla do: a newline goes back to x and lh
// rows down, a character without a glyph moves on 4 columns. Every other character goes to
// glyph(c, x, y) with c its HU font index, which returns the width of the glyph, or -1 when the
// text ends there.
void scr_text(int id, uint32_t off, int x, int y, int lh, int count,
              int (*glyph)(int c, int x, int y));
