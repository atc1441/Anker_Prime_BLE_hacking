//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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
//	System specific interface stuff.
//


#ifndef __D_TICCMD__
#define __D_TICCMD__

#include "doomtype.h"


// The data sampled per tick (single player)
// and transmitted to other peers (multiplayer).
// Mainly movements/button commands per game tick,
// plus a checksum for internal state consistency.

typedef struct
{
    signed char	forwardmove;	// *2048 for move
    signed char	sidemove;	// *2048 for move
    short angleturn;            // <<16 for angle delta
    byte chatchar;
    byte buttons;
    // villsa [STRIFE] according to the asm,
    // consistancy is a short, not a byte
    byte consistancy;           // checks for net game

    // The Strife, Heretic and Hexen fields are not part of this port.
} ticcmd_t;



#endif
