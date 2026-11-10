/* Drawing helpers for the pre-game pages.

   The glass is 240x240 and sits on columns VIEW_X0..VIEW_X1 of the
   320-wide engine screen, so x = 0 of the engine is off the left edge.
   Everything here takes x relative to the text margin, which is why the
   pages no longer start hard against the bezel. ui.c implements both
   this and the vendor ui_* API on top of it. */

#ifndef PAGEDRAW_H
#define PAGEDRAW_H

#include <stdint.h>

#include "gd32.h"

#define PG_PAD 8
#define PG_X0 (VIEW_X0 + PG_PAD)
#define PG_W (PANEL_W - 2 * PG_PAD)
#define PG_COLS (PG_W / 8)
#define PG_BOT (PANEL_H - 1)

/* Full glass width, for bars that should run edge to edge. */
#define PG_EDGE_X0 VIEW_X0
#define PG_EDGE_W PANEL_W

void pg_clear(uint16_t bg);
void pg_fill(int x, int y, int w, int h, uint16_t c);
void pg_edge_fill(int y, int h, uint16_t c);
void pg_frame(int x, int y, int w, int h, uint16_t c);
/* Draws at the margin-relative x, returns the x after the last glyph. */
int pg_text(int x, int y, uint16_t fg, uint16_t bg, int scale, const char *s);
int pg_textf(int x, int y, uint16_t fg, uint16_t bg, int scale, const char *f, ...)
    __attribute__((format(printf, 6, 7)));
void pg_center(int y, uint16_t fg, uint16_t bg, int scale, const char *s);
void pg_centerf(int y, uint16_t fg, uint16_t bg, int scale, const char *f, ...)
    __attribute__((format(printf, 5, 6)));
/* One line with its own background, cleared to the full text width. */
void pg_line(int y, uint16_t fg, uint16_t bg, const char *f, ...)
    __attribute__((format(printf, 4, 5)));
/* Label left, value right, on one 8-pixel line. */
void pg_pair(int y, uint16_t lfg, const char *label, uint16_t vfg, const char *f, ...)
    __attribute__((format(printf, 5, 6)));
/* pct100 is 0..10000. */
void pg_bar(int x, int y, int w, int h, uint16_t pct100, uint16_t fill, uint16_t rest,
             uint16_t frame);

#endif
