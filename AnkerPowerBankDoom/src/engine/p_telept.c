//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// Modified 2026 for the DMC-100 port.
//
// DESCRIPTION:
//	Teleportation.
//




#include "doomdef.h"
#include "doomstat.h"

#include "s_sound.h"

#include "p_local.h"


// Data.
#include "sounds.h"

// State.
#include "r_state.h"

#include "things.h"



//
// TELEPORTATION
// The destination of a tagged sector is the first MT_TELEPORTMAN of the
// skill spawned in it (TELEDEST, sorted by sector and map index), what
// vanilla's thinker scan finds.
//
int
EV_Teleport
( lineidx_t	line,
  int		side,
  mobj_t*	thing )
{
    int		i;
    int		tag;
    mobj_t*	fog;
    unsigned	an;
    dmc_range_t	r;
    dmc_teledest_t d;
    dmc_thing_t	t;
    int		k;
    int		n;
    fixed_t	oldx;
    fixed_t	oldy;
    fixed_t	oldz;
    fixed_t	mx;
    fixed_t	my;
    angle_t	mangle;

    // don't teleport missiles
    if (thing->flags & MF_MISSILE)
	return 0;		

    // Don't teleport if hit back of line,
    //  so you can get out of teleporter.
    if (side == 1)		
	return 0;	

    
    // the sectors of the tag, in sector order
    tag = P_LineTag(line);
    for (k = 0; tag && k < P_TagSectors(tag, &r); k++)
    {
	i = P_TagSector(tag, &r, k);
	for (n = 0; n < M.nteledest; n++)
	{
	    rec_get(M.st[DMC_ST_TELEDEST] + 4 * n, &d, 4);
	    if (d.sector != i || O_Tier(d.thing) == T_ABSENT)
		continue;

	    map_thing(d.thing, &t);
	    mx = t.x << FRACBITS;
	    my = t.y << FRACBITS;
	    mangle = ANG45 * t.angle45;

	    oldx = thing->x;
	    oldy = thing->y;
	    oldz = thing->z;
				
	    if (!P_TeleportMove (thing, mx, my))
		return 0;

	    thing->z = MO_FLOORZ(thing);

	    if (MO_PLAYER(thing))
		MO_PLAYER(thing)->viewz = thing->z+MO_PLAYER(thing)->viewheight;

	    // spawn teleport fog at source and destination
	    fog = P_SpawnMobj (oldx, oldy, oldz, MT_TFOG);
	    S_StartSound (fog, sfx_telept);
	    an = mangle >> ANGLETOFINESHIFT;
	    fog = P_SpawnMobj (mx+20*finecosine(an), my+20*finesine(an)
			       , thing->z, MT_TFOG);

	    // emit sound, where?
	    S_StartSound (fog, sfx_telept);
		
	    // don't move for a bit
	    if (MO_PLAYER(thing))
		thing->reactiontime = 18;	

	    thing->angle = mangle;
	    thing->momx = thing->momy = thing->momz = 0;
	    return 1;
	}
    }
    return 0;
}
