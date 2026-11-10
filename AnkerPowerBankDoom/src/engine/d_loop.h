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
//	Main loop stuff.
//

#ifndef __D_LOOP__
#define __D_LOOP__

#include "d_ticcmd.h"

// Ticcmds are built at most 5 tics ahead, see BuildNewTic.
#define BACKUPTICS 8

// Single player setup that a net game would negotiate.
void D_CheckNetGame (void);

// Create any new ticcmds.
void NetUpdate (void);

//? how many ticks to run?
void TryRunTics (void);

// Called at start of game loop to initialize timers
void D_StartGameLoop(void);

// The next ticcmd is the one of the next tic, timers from now
void D_ResetTics(void);

extern boolean singletics;
extern int gametic;

#endif
