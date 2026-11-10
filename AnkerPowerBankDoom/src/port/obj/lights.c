// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Light thinker slots: 8 B each in the level arena,
// as many as the map's level start lights and strobe triggers. Lights
// are never removed, slots are taken in order.

#include "doomdef.h"
#include "i_system.h"
#include "p_local.h"

#include "things.h"

light_t*	lights;
int		numlights;

light_t* P_NewLight (int kind, secidx_t sec)
{
    light_t*	l;

#ifdef DMC_STATS
    if (numlights >= M.plan.light_slots)
	I_Error("P_NewLight: more than %i lights", M.plan.light_slots);
#endif
    l = &lights[numlights];
    l->kind = kind;
    l->sector = sec;
    O_LinkThinker(L_LIGHT | numlights++);
    return l;
}
