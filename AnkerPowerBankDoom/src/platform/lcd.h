// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// ST7789V 240x320 panel on SPI2 (72 MHz), used in landscape.
//
// Coordinates are landscape: x 0..319 left to right, y 0..239 top to bottom, with the meter
// held clamp-left (MADCTL 0x80). Flip (MADCTL 0x40) turns the picture by 180 degrees for the
// clamp-right grip. Screen pixel (x, y) is panel column y, row x, so the panel's fast address
// counter runs down one screen column: every pixel stream is column-major, each column top
// to bottom, columns left to right.
//
// Regions: 3D view y 0..199, HUD message line y 200..207, status bar y 208..239.
// Pixels are RGB565.
//
// Two ways to draw:
// - lcd_begin/lcd_push/lcd_end and the helpers built on them are polled (CPU writes SPI).
// - The pump streams 8-bit indexed columns through the 256-entry palette LUT with DMA1 ch5
//   in two half-column buffers, re-armed from the transfer-complete interrupt.
// A polled call waits for an open pump window to drain and closes it first.

#pragma once

#include <stdint.h>

#define LCD_W 320
#define LCD_H 240
#define LCD_VIEW_H 200
#define LCD_HUD_Y 200
#define LCD_BAR_Y 208

static inline uint16_t rgb565(int r, int g, int b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xFF) >> 3));
}

// Init profiles. PIMORONI sets power, VCOM and gamma (values from pimoroni-pico, MIT).
// MINIMAL keeps the chip's reset values except MADCTL, COLMOD and INVON.
enum { LCD_INIT_MINIMAL = 0, LCD_INIT_PIMORONI = 1 };
#ifndef LCD_INIT_DEFAULT
#define LCD_INIT_DEFAULT LCD_INIT_PIMORONI
#endif

// Hardware reset, init sequence, display on, screen cleared to black. About 270 ms.
void lcd_init(int profile);
int lcd_profile(void);
// 0: MADCTL 0x80 (clamp left), 1: MADCTL 0x40 (rotated 180 degrees).
void lcd_flip(int flip);
int lcd_flipped(void);
void lcd_backlight(int on);

// Polled drawing. A window covers x0..x1, y0..y1 inclusive, pixels column-major.
void lcd_begin(int x0, int y0, int x1, int y1);
void lcd_push(uint16_t c);
void lcd_push_n(const uint16_t *px, uint32_t n);
void lcd_end(void);
void lcd_fill(int x0, int y0, int x1, int y1, uint16_t c);
// 8x8 font (ASCII 32..126, others drawn as '?'), scale 1..4, fg on bg. Glyphs that do not
// fit on the screen are skipped, drawing stops at the right edge. Returns the x after the
// last glyph.
int lcd_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale);
// Stops the pump and any DMA on SPI2, raises CS and returns SPI2 to 8-bit frames. Safe in
// fault handlers.
void lcd_recover(void);

// ---- Pump ----
// pump_submit, pump_sync and pump_end wait for the DMA interrupt, so they must not be
// called from an interrupt handler or with interrupts masked. A wait with no pump interrupt
// for 100 ms ends in plat_panic.

// Palette LUT used by the pump, RGB565 per palette index.
extern uint16_t pump_lut[256];

#define PUMP_DOUBLE 1  // low detail: every submitted column is sent twice

// Opens the window x0..x1, y0..y1 (one RAMWR). Column height h = y1 - y0 + 1 must be a
// multiple of 4 and at most 200, other windows go to plat_panic. The 3D view is
// pump_begin(0, 319, 0, 199, ...).
void pump_begin(int x0, int x1, int y0, int y1, int flags);
// Queues ncols columns of h palette indices each, column-major and contiguous. Blocks while
// 4 strips are pending. Restarts the DMA if it ran dry. The strip memory belongs to the pump
// until it is no longer pending (see pump_sync).
void pump_submit(const uint8_t *cols, int ncols);
// Waits until at most max strips are pending, so a buffer submitted earlier can be reused.
// A strip stops being pending once its last pixel has been converted to RGB565.
void pump_sync(int max);
// Strips submitted and not yet converted.
int pump_pending(void);
// Waits for everything submitted to reach the panel, then closes the window. The panel
// wraps writes past the end of a window to its start.
void pump_end(void);

struct pump_stats {
    uint32_t irqs;      // half-column interrupts
    uint32_t cycles;    // DWT cycles spent in the interrupt, summed
    uint32_t max;       // longest interrupt in cycles
    uint32_t restarts;  // pump_submit restarts after running dry
    uint32_t fills;     // half columns (h / 2 pixels) converted by the interrupt
};
extern volatile struct pump_stats pump_stats;
