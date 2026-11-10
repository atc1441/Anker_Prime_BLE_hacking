// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// LCD pump of lcd.h for the ARM size build of the engine, doing nothing.

#include "lcd.h"

uint16_t pump_lut[256];

void pump_begin(int x0, int x1, int y0, int y1, int flags)
{
    (void)x0;
    (void)x1;
    (void)y0;
    (void)y1;
    (void)flags;
}

void pump_submit(const uint8_t *cols, int ncols)
{
    (void)cols;
    (void)ncols;
}

void pump_sync(int max)
{
    (void)max;
}

void pump_end(void)
{
}
