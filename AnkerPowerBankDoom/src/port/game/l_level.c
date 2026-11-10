// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Entry table of the LEVEL overlay (l_level.h), linked at FRAME_BASE (mk/fw.mk).

#include "doomdef.h"
#include "p_setup.h"

#include "l_level.h"
#include "ovl.h"

// g_game.c declares these in the file only
void G_DoLoadLevel(void);
void G_DoNewGame(void);
void G_DoPlayDemo(void);
void G_DoWorldDone(void);

OVL_TABLE(LEVEL, OVL_ENTRY(LEVEL_E_LOAD, level_fn, G_DoLoadLevel),
          OVL_ENTRY(LEVEL_E_NEWGAME, level_fn, G_DoNewGame),
          OVL_ENTRY(LEVEL_E_PLAYDEMO, level_fn, G_DoPlayDemo),
          OVL_ENTRY(LEVEL_E_WORLDDONE, level_fn, G_DoWorldDone),
          OVL_ENTRY(LEVEL_E_INIT, level_fn, P_Init));
