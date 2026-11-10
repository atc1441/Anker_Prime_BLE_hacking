// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The LINES overlay: the line specials that a thing triggers by
// crossing, shooting or using a line, with the EV_* builders and helpers only they call
// (engine/p_spec.c, p_switch.c, p_doors.c, p_plats.c, p_ceilng.c, p_lights.c, p_telept.c,
// p_floor.c). It loads in the middle of a tic, behind the strip buffers once the pump has
// sent them and below the tic scratch and the creation order of the verification pass
// (OVL_WIN_TIC). Resident code calls it through port/game/i_dmc.c only.

#pragma once

#include "doomdef.h"
#include "p_local.h"

enum { LINES_E_CROSS, LINES_E_SHOOT, LINES_E_USE };

typedef void lines_cross_fn(int linenum, int side, mobj_t *thing);
typedef void lines_shoot_fn(mobj_t *thing, lineidx_t line);
typedef boolean lines_use_fn(mobj_t *thing, lineidx_t line, int side);
