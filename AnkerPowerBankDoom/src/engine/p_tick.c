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
//	Archiving: SaveGame I/O.
//	Thinker, Ticker.
//


#include <string.h>

#include "z_zone.h"
#include "p_local.h"

#include "doomstat.h"

#include "fe.h"
#include "things.h"


int	leveltime;


//
// P_Ticker
// The thinkers run in the object store (port/obj/things.c).
//

void P_Ticker (void)
{
    int		i;
    
    // run the tic
    if (paused)
	return;
		
    // pause if in menu and at least one tic has been run
    if ( !netgame
	 && menuactive
	 && !demoplayback
	 && players[consoleplayer].viewz != 1)
    {
	return;
    }
    
    // the tic scratch holds the hashes of the front end's screen
    fe.flags &= ~FE_HASHED;
#ifdef DMC_STATS
    // host: nothing in the tic scratch outlives a tic
    memset (&dmc_tic, 0xa5, sizeof(dmc_tic));
    HS_Event (HS_E_THINK, NULL);
#endif

    for (i=0 ; i<MAXPLAYERS ; i++)
	if (playeringame[i])
	    P_PlayerThink (&players[i]);
			
    O_RunThinkers ();
    P_UpdateSpecials ();
#ifdef DMC_STATS
    HS_Event (HS_E_THINK_END, NULL);
    O_Stats ();
#endif

    // for par times
    leveltime++;	
    o_cursor = -1;
}
