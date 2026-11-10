// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Game pad driver for the DJI RC-N1 remote controller (RC231) on the meter's USB port, see
// dmc_dji.c. Play mode only (fe_play.c), the meter build only (the Renode variant has no
// OTGFS model).

#pragma once

#include <stdint.h>

// The driver on: usbd_init (25 ms), the OTGFS interrupt, the pull-up. The bus then belongs to
// the interrupt handler.
void dji_start(void);
// Interrupt off, controller off, pad_lost(). Nothing when the driver is off.
void dji_stop(void);
// Main loop part, called from I_StartTic: the switch to the accessory identity, the fallback
// to the phone identity and the pad timeout.
void dji_tic(void);
// Received bulk OUT data: every DUML v1 frame that lies within it is checked, the RC's push
// 06/05 and the reply 06/01 (DJI_POLL) go to pad_set().
void dji_rx(const uint8_t *p, uint32_t n);
// The OTGFS interrupt (IRQ 67)
void OTGFS1_IRQHandler(void);
