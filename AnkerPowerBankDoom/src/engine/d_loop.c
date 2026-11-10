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
//     Main loop code.
//

#include <string.h>

#include "doomdef.h"
#include "doomstat.h"

#include "d_event.h"
#include "d_loop.h"
#include "d_main.h"
#include "d_ticcmd.h"
#include "g_game.h"
#include "m_menu.h"

#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"

// The complete set of data for a particular tic.
// Single player only: one ticcmd per tic.

static ticcmd_t ticdata[BACKUPTICS];

// The index of the next tic to be made (with a call to BuildTiccmd).

static int maketic;

// The number of tics run so far.

int gametic;

// The ticcmd of the tic being run, read by G_Ticker.

ticcmd_t *netcmds;

// When set to true, a single tic is run each time TryRunTics() is called.
// This is used for -timedemo mode.

boolean singletics = false;

// Current time in tics. Single player uses the old sync code, which has
// no offset adjustment.

static int GetAdjustedTime(void)
{
    return (I_GetTimeMS() * TICRATE) / 1000;
}

static boolean BuildNewTic(void)
{
    ticcmd_t cmd;

    I_StartTic ();
    D_ProcessEvents ();

    // Always run the menu

    M_Ticker ();

    if (maketic - gametic >= 5)
       return false;

    //printf ("mk:%i ",maketic);
    memset(&cmd, 0, sizeof(ticcmd_t));
    G_BuildTiccmd(&cmd, maketic);

    ticdata[maketic % BACKUPTICS] = cmd;

    ++maketic;

    return true;
}

//
// D_WillBuildTic
// The next BuildNewTic builds a ticcmd (the input of port/game/i_input.c
// waits for it).
//
boolean D_WillBuildTic(void)
{
    return maketic - gametic < 5;
}

//
// NetUpdate
// Builds ticcmds for console player,
// sends out a packet
//
int      lasttime;

void NetUpdate (void)
{
    int nowtime;
    int newtics;
    int	i;

    // If we are running with singletics (timing a demo), this
    // is all done separately.

    if (singletics)
        return;

    // check time
    nowtime = GetAdjustedTime();
    newtics = nowtime - lasttime;

    lasttime = nowtime;

    // build new ticcmds for console player

    for (i=0 ; i<newtics ; i++)
    {
        if (!BuildNewTic())
        {
            break;
        }
    }
}

//
// Start game loop
//
// Called after the screen is set but before the game starts running.
//

void D_StartGameLoop(void)
{
    lasttime = GetAdjustedTime();
}

//
// D_ResetTics
// The next ticcmd is built for the next tic to run, from now on in real
// time (the meter's play mode after tics that the demo passes ran
// without TryRunTics).
//
void D_ResetTics(void)
{
    maketic = gametic;
    lasttime = GetAdjustedTime();
}

//
// D_CheckNetGame
// Works out player numbers among the net participants
// Single player: player 0 alone.
//
void D_CheckNetGame (void)
{
    int i;

    consoleplayer = 0;

    for (i = 0; i < MAXPLAYERS; ++i)
    {
        playeringame[i] = i == 0;
    }
}

static void RunTic(ticcmd_t *cmd)
{
    extern boolean advancedemo;

    netcmds = cmd;

    if (advancedemo)
        D_DoAdvanceDemo ();

    G_Ticker ();
}

//
// TryRunTics
//
void TryRunTics (void)
{
    int	entertic;
    static int oldentertics;
    int realtics;
    int	availabletics;
    int	counts;

    // get real tics
    entertic = I_GetTime();
    realtics = entertic - oldentertics;
    oldentertics = entertic;

    // in singletics mode, run a single tic every time this function
    // is called.

    if (singletics)
    {
        BuildNewTic();
    }
    else
    {
        NetUpdate ();
    }

    availabletics = maketic - gametic;

    // decide how many tics to run

    if (realtics < availabletics-1)
        counts = realtics+1;
    else if (realtics < availabletics)
        counts = realtics;
    else
        counts = availabletics;

    if (counts < 1)
	counts = 1;

    // wait for new tics if needed

    while (maketic < gametic + counts)
    {
	NetUpdate ();

	if (maketic < gametic)
	    I_Error ("TryRunTics: lowtic < gametic");

	// Don't stay in this loop forever.  The menu is still running,
	// so return to update the screen

	if (I_GetTime() - entertic > 0)
	{
	    return;
	}

        I_Sleep(1);
    }

    // run the count * ticdup dics
    while (counts--)
    {
        RunTic(&ticdata[gametic % BACKUPTICS]);
	gametic++;

	NetUpdate ();	// check for new console commands
    }
}
