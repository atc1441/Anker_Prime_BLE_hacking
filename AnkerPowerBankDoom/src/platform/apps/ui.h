// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Text pages for the firmware's own screens (installer, data errors, results) and for
// RCPROBE: a header bar, 21 rows of 40 characters and a footer, all in the 320x240
// landscape screen.

#pragma once

#include <stdint.h>

#define UI_ROWS 21
#define UI_ROW_Y(r) (12 + 10 * (r))

#define C_BLACK  0x0000u
#define C_WHITE  0xFFFFu
#define C_GREY   0x8410u
#define C_DGREY  0x4208u
#define C_RED    0xF800u
#define C_GREEN  0x07E0u
#define C_BLUE   0x001Fu
#define C_YELLOW 0xFFE0u
#define C_CYAN   0x07FFu
#define C_MAGENTA 0xF81Fu
#define C_ORANGE 0xFD20u

void ui_clear(void);
void ui_header(const char *title, int page, int npages);
void ui_footer(const char *s);
// Draws one row with its background, clearing the rest of the row
void ui_row(int r, uint16_t fg, const char *f, ...) __attribute__((format(printf, 3, 4)));
void ui_text_row(int y, uint16_t fg, uint16_t bg, const char *s);

// POWER long-press check, call it in the idle loop. Returns 1 when POWER has been held alone
// for ms milliseconds. A hold that started before the first release after boot (the press
// that switched the meter on) does not count.
int ui_power_held(uint32_t ms);

// Integer formatting helpers: v / 100 with two decimals, and cycles to microseconds * 100
void ui_fix2(char *out, int cap, uint32_t v100);
uint32_t ui_cyc_to_us100(uint32_t cycles);
