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
//	DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//	plus functions to determine game mode (shareware, registered),
//	parse command line parameters, configure game parameters (turbo),
//	and call the startup functions.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deh_main.h"
#include "doomdef.h"
#include "doomstat.h"

#include "dstrings.h"
#include "sounds.h"

#include "z_zone.h"
#include "dmcdat.h"
#include "img.h"
#include "s_sound.h"
#include "v_video.h"

#include "f_finale.h"

#include "m_argv.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"

#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"

#include "g_game.h"

#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"
#include "am_map.h"

#include "p_setup.h"
#include "r_local.h"

#include "d_main.h"

#include "fe.h"
#include "lcd.h"
#include "r_strip.h"

//
// D-DoomLoop()
// Not a globally visible function,
//  just included for source reference,
//  called by D_DoomMain, never exits.
// Manages timing and IO,
//  calls all ?_Responder, ?_Ticker, and ?_Drawer,
//  calls I_GetTime, I_StartFrame, and I_StartTic
//
void D_DoomLoop (void);

boolean		devparm;	// started game with -devparm
boolean         nomonsters;	// checkparm of -nomonsters
boolean         respawnparm;	// checkparm of -respawn
boolean         fastparm;	// checkparm of -fast

extern  boolean	inhelpscreens;

skill_t		startskill;
int             startepisode;
int		startmap;
boolean		autostart;

boolean		advancedemo;


//
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
//
void D_ProcessEvents (void)
{
    event_t*	ev;
	
    while ((ev = D_PopEvent()) != NULL)
    {
	if (M_Responder (ev))
	    continue;               // menu ate the event
	G_Responder (ev);
    }
}




//
// D_Display
//  draw current display, possibly wiping it from the previous
// The 3D view goes to the LCD in strips (port/render/r_frame.c) and the
// status bar and message line to the status band (port/render/r_band.c),
// neither needs a screen buffer. The pages, the intermission and the
// finale are composed in pieces (port/render/r_screen.c) by the front
// end's drawers (port/game/fe_glue.c), which return at once while the
// menu is up: the menu freezes the picture under it. The automap and the
// pause picture of the host still draw into the temporary screen buffer,
// which I_FinishUpdate sends when tempscreen is set.
//

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t     wipegamestate = GS_DEMOSCREEN;
extern  boolean setsizeneeded;
extern  int             showMessages;
extern  int		r_viewtemp;
boolean			tempscreen;

void D_Display (void)
{
    static  boolean		inhelpscreensstate = false;
    static  gamestate_t		oldgamestate = -1;
    int				y;
    boolean			wipe;
    boolean			redrawsbar;
    boolean			inlevel;
    boolean			view;

    redrawsbar = false;
    
    // change the view size if needed
    if (setsizeneeded)
    {
		R_ExecuteSetViewSize ();
		oldgamestate = -1;                      // force background redraw
    }

    // a wipe is due (not drawn by this port)
    wipe = gamestate != wipegamestate;
    inlevel = gamestate == GS_LEVEL && gametic;
    view = inlevel && !automapactive && !menuactive;

    // the view under the pause picture goes to the temporary screen, so
    // it can be drawn over it
    tempscreen = inlevel && (automapactive || paused);
    r_viewtemp = tempscreen;

    // clean up border stuff: before the drawers, so a picture is composed
    // in the palette it is shown in
    if (gamestate != oldgamestate && gamestate != GS_LEVEL)
    	I_SetPalette (0);

    fe_frame (view);

    if (inlevel)
    	HU_Erase();
    
    // do buffered drawing
    switch (gamestate)
    {
      case GS_LEVEL:
		if (!gametic)
			break;
		if (automapactive)
			AM_Drawer ();
		if (wipe)
			redrawsbar = true;
		if (inhelpscreensstate && !inhelpscreens)
			redrawsbar = true;              // just put away the help screen
		// the status bar is always shown, under the view
		r_bandmode = BAND_RECORD;
		ST_Drawer (false, redrawsbar );
		r_bandmode = BAND_OFF;
		break;

      case GS_INTERMISSION:
		WI_Drawer ();
		break;

      case GS_FINALE:
		F_Drawer ();
		break;

      case GS_DEMOSCREEN:
		D_PageDrawer ();
		break;
    }
    
    // draw buffered stuff to screen
    I_UpdateNoBlit ();
    
    // draw the view directly
    if (view)
    	R_RenderPlayerView (&players[displayplayer]);

    if (inlevel)
    	HU_Drawer ();
    
    inhelpscreensstate = inhelpscreens;
    oldgamestate = wipegamestate = gamestate;
    
    // draw pause pic
    if (paused)
    {
		if (automapactive)
			y = 4;
		else
			y = viewwindowy+4;
		V_DrawPatchDirect(viewwindowx + (scaledviewwidth - 68) / 2, y,
							  V_UIPatch(DMC_UI_M_PAUSE));
    }


    // menus go directly to the screen
    M_Drawer ();          // menu is drawn even on top of everything
    NetUpdate ();         // send out any new accumulation

    // The melt wipe is not drawn. Its setup takes one M_Random value
    // per screen column, take the same values to keep rndindex in step.
    if (wipe)
    {
	for (y = 0; y < SCREENWIDTH; y++)
	    M_Random ();
    }

    // the dirty parts of the status band
    R_BandFlush (inlevel, inlevel);

    // the compose buffers are in the tic scratch: sent before the next tic
    if (!view)
	pump_sync (0);

    I_FinishUpdate ();              // page flip or blit buffer
}

//
//  D_DoomLoop
//
void D_DoomLoop (void)
{
    TryRunTics();

    I_InitGraphics();

    V_RestoreBuffer();
    R_ExecuteSetViewSize();

    D_StartGameLoop();

    for (;;)
    {
	// frame syncronous IO operations
	I_StartFrame ();

	TryRunTics (); // will run at least one tic

	S_UpdateSounds (players[consoleplayer].mo);// move positional sounds

	// Update display, next frame, with current state.
	D_Display ();
    }
}



//
//  DEMO LOOP
//
int             demosequence;
int             pagetic;
int                     pagepic;	// dmc_ui picture


//
// D_PageTicker
// Handles timing for warped projection
//
void D_PageTicker (void)
{
    if (--pagetic < 0)
	D_AdvanceDemo ();
}



//
// D_PageDrawer
// The FINALE overlay draws the page, a picture the pack does not have
// shows as TITLEPIC.
//
void D_PageDrawer (void)
{
    /* The title and the credits are streamed to the panel a column at a
       time. Drawing the same page again takes longer than the whole
       attract countdown, so the picture stays up and the tic clock runs. */
    static int shown = -2;
    static boolean menu_was;
    if (menu_was && !menuactive)
        shown = -2;
    menu_was = menuactive;
    if (shown == pagepic)
        return;
    shown = pagepic;
    fe_page();
}


//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
void D_AdvanceDemo (void)
{
    advancedemo = true;
}


//
// This cycles through the demo sequences.
// Only the single player demo is part of the sequence, the other
// Freedoom demos are multiplayer.
//
void D_DoAdvanceDemo (void)
{
    players[consoleplayer].playerstate = PST_LIVE;  // not reborn
    advancedemo = false;
    usergame = false;               // no save / end game here
    paused = false;
    gameaction = ga_nothing;

    demosequence = (demosequence+1)%3;
    
    switch (demosequence)
    {
      case 0:
	pagetic = 170;
	gamestate = GS_DEMOSCREEN;
	pagepic = DMC_UI_TITLEPIC;
	break;
      case 1:
	G_DeferedPlayDemo(DEH_String("demo1"));
	break;
      case 2:
	pagetic = 200;
	gamestate = GS_DEMOSCREEN;
	pagepic = DMC_UI_CREDIT;
	break;
    }
}



//
// D_StartTitle
//
void D_StartTitle (void)
{
    gameaction = ga_nothing;
    demosequence = -1;
    D_AdvanceDemo ();
}

//
// D_DoomMain
//
void D_DoomMain (void)
{
    int p;
    char *demolumpname = NULL;

    Z_Init ();

    //!
    // @vanilla
    //
    // Disable monsters.
    //
	
    nomonsters = M_CheckParm ("-nomonsters");

    //! 
    // @vanilla
    //
    // Developer mode.  F1 saves a screenshot in the current working
    // directory.
    //

    devparm = M_CheckParm ("-devparm");

    // init subsystems
    V_Init ();

    // The game data is the NOR image (DOOM.DAT and DOOMX.DAT).
    // Freedoom: Phase 1 is the only supported game data, gamemode,
    // gamemission and gameversion are fixed in doomstat.h.

    p = img_init ();
    if (p != IMG_OK)
	I_Error("No game data in the NOR image (error %i)", p);

#ifdef DMC_PARITY
    // accessor parity: the WAD-loaded vanilla map data for comparison
    PAR_Init ();
#endif

    //!
    // @arg <demo>
    // @category demo
    // @vanilla
    //
    // Play back the demo named demo.lmp.
    //

    p = M_CheckParmWithArgs ("-playdemo", 1);

    if (!p)
    {
        //!
        // @arg <demo>
        // @category demo
        // @vanilla
        //
        // Play back the demo named demo.lmp, determining the framerate
        // of the screen.
        //
	p = M_CheckParmWithArgs("-timedemo", 1);

    }

    if (p)
    {
        // DEMOn names the n-th demo of the image, a name with a dot a
        // .lmp file (G_DoPlayDemo)
        demolumpname = myargv[p + 1];
    }


    I_InitTimer();
    I_InitSound(true);

    // get skill / episode / map from parms
    startskill = sk_medium;
    startepisode = 1;
    startmap = 1;
    autostart = false;

    //!
    // @arg <skill>
    // @vanilla
    //
    // Set the game skill, 1-5 (1: easiest, 5: hardest).  A skill of
    // 0 disables all monsters.
    //

    p = M_CheckParmWithArgs("-skill", 1);

    if (p)
    {
	startskill = myargv[p+1][0]-'1';
	autostart = true;
    }

    //!
    // @arg <n>
    // @vanilla
    //
    // Start playing on episode n (1-4)
    //

    p = M_CheckParmWithArgs("-episode", 1);

    if (p)
    {
	startepisode = myargv[p+1][0]-'0';
	startmap = 1;
	autostart = true;
    }

    //!
    // @arg [<x> <y> | <xy>]
    // @vanilla
    //
    // Start a game immediately, warping to ExMy (Doom 1) or MAPxy
    // (Doom 2)
    //

    p = M_CheckParmWithArgs("-warp", 1);

    if (p)
    {
        startepisode = myargv[p+1][0]-'0';

        if (p + 2 < myargc)
        {
            startmap = myargv[p+2][0]-'0';
        }
        else
        {
            startmap = 1;
        }
        autostart = true;
    }

    M_Init ();

    R_Init ();

    P_Init ();

    S_Init (sfxVolume * 8);

    D_CheckNetGame ();

    HU_Init ();

    ST_Init ();

    p = M_CheckParmWithArgs("-playdemo", 1);
    if (p)
    {
		singledemo = true;              // quit after one demo
		G_DeferedPlayDemo (demolumpname);
		D_DoomLoop ();
        return;
    }

    p = M_CheckParmWithArgs("-timedemo", 1);
    if (p)
    {
		G_TimeDemo (demolumpname);
		D_DoomLoop ();
        return;
    }

    if (autostart)
	G_InitNew (startskill, startepisode, startmap);
    else
	D_StartTitle ();                // start up intro loop

    D_DoomLoop ();
}
