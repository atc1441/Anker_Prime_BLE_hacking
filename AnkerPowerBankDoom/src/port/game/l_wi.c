// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Entry table of the WI overlay (fe.h), linked at FRAME_BASE (mk/fw.mk). On the meter it also
// ends a level: G_DoCompleted runs here and not in LEVEL, whose window reaches the creation
// order of the verification pass (dmc_demo.c), which the hash still needs through the
// intermission.

#include "doomdef.h"
#include "doomstat.h"
#include "wi_stuff.h"

#include "fe.h"
#include "ovl.h"

// g_game.c declares it in the file only
void G_DoCompleted(void);

OVL_TABLE(WI, OVL_ENTRY(WI_E_START, fe_fn, WI_OvlStart),
          OVL_ENTRY(WI_E_TICKER, fe_fn, WI_OvlTicker),
          OVL_ENTRY(WI_E_DRAW, fe_fn, WI_OvlDrawer),
          OVL_ENTRY(WI_E_COMPLETED, fe_fn, G_DoCompleted));
