// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The LEVEL overlay: level setup (engine/p_setup.c, the level arena and
// the map things of port/obj/level.c) with the status bar, HUD and sound setup it runs, the
// level transitions of g_game.c that lead to it, and P_Init, which reads the animation
// ranges. Resident code calls it through port/game/i_dmc.c only. The end of a level
// (G_DoCompleted) runs in the WI overlay (port/game/l_wi.c).

#pragma once

enum {
    LEVEL_E_LOAD,       // G_DoLoadLevel
    LEVEL_E_NEWGAME,    // G_DoNewGame
    LEVEL_E_PLAYDEMO,   // G_DoPlayDemo
    LEVEL_E_WORLDDONE,  // G_DoWorldDone
    LEVEL_E_INIT,       // P_Init
};

typedef void level_fn(void);
