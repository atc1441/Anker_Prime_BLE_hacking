// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 VapeDoom port
//
// No USB on this board.
//
// The vape's connector goes to the part's device controller, but this port
// needs neither of the things the DMC-100 uses USB for: the game data is put
// on the flash over SWD, which is how the firmware gets there in the first
// place, and there is no gamepad. The engine's play loop still starts and
// stops the controller driver, so those three calls exist and do nothing.

#include <stdint.h>

void dji_start(void);
void dji_stop(void);
void dji_tic(void);

void dji_start(void) {}
void dji_stop(void) {}
void dji_tic(void) {}
