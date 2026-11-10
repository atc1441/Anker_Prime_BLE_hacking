// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Mover slots: doors, floors, plats and ceilings in
// 20 B slots of the level arena, at most MAXMOVERS. An activation
// reserves all of its movers or none (P_MoverReserve before the EV_*
// call).

#include <string.h>

#include "doomdef.h"
#include "i_system.h"
#include "p_local.h"

#include "things.h"

mover_t*	movers;
unsigned	o_moverused;	// slot bits, a removed slot stays used until unlinked

int P_MoversFree (void)
{
    int		n = 0;
    int		i;

    for (i = 0; i < M.plan.mover_slots; i++)
	n += !(o_moverused & (1u << i));
    return n;
}

int P_MoversUsed (void)
{
    return M.plan.mover_slots - P_MoversFree();
}

int P_MoverSlot (void* m)
{
    return (mover_t *) m - movers;
}

//
// P_NewMover
// A cleared slot linked at the end of the thinker list, its sector
// pointing at it.
//
mover_t* P_NewMover (int kind, secidx_t sec)
{
    mover_t*	m;
    int		i;

    for (i = 0; i < M.plan.mover_slots; i++)
	if (!(o_moverused & (1u << i)))
	    break;
#ifdef DMC_STATS
    if (i == M.plan.mover_slots)
	I_Error("P_NewMover: no slot, the activation was not reserved");
#endif
    o_moverused |= 1u << i;
    m = &movers[i];
    memset(m, 0, sizeof(*m));
    m->door.kind = kind;
    m->door.sector = sec;
    P_SecOvl(sec)->mover = i;
    O_LinkThinker(L_MOVER | i);
    return m;
}

// The thinker pass unlinks the slot and calls P_FreeMover.
void P_RemoveMover (void* m)
{
    ((mover_t *) m)->door.kind |= MV_REMOVED;
}

void P_FreeMover (int slot)
{
    o_moverused &= ~(1u << slot);
}
