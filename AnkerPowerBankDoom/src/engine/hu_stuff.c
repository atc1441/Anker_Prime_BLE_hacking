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
// DESCRIPTION:  Heads-up displays
//


#include <ctype.h>

#include "doomdef.h"
#include "doomkeys.h"

#include "z_zone.h"

#include "deh_main.h"
#include "i_swap.h"
#include "i_video.h"

#include "hu_stuff.h"
#include "hu_lib.h"
#include "m_controls.h"
#include "m_misc.h"
#include "dmcdat.h"
#include "p_mapdata.h"
#include "v_video.h"

#include "s_sound.h"

#include "doomstat.h"

#include "r_strip.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

//
// Locally used constants, shortcuts.
//
#define HU_TITLE	(mapnames[(gameepisode-1)*9+gamemap-1])
#define HU_TITLEHEIGHT	1
#define HU_TITLEX	0
#define HU_TITLEY	(167 - V_PatchHeight(HU_FONT(0)))


const char* const	player_names[] =
{
    HUSTR_PLRGREEN,
    HUSTR_PLRINDIGO,
    HUSTR_PLRBROWN,
    HUSTR_PLRRED
};

static player_t*	plr;
// The firmware has no automap (port/game/fe_glue.c has a stub), so it keeps
// no map title line.
#ifndef DMC_FW
static hu_textline_t	w_title;
#endif

// Chat is not part of this port (single player only).
boolean			chat_on;

static boolean		message_on;
boolean			message_dontfuckwithme;
static boolean		message_nottobefuckedwith;

static hu_stext_t	w_message;
static int		message_counter;

extern int		showMessages;

static boolean		headsupactive = false;

//
// Builtin map names.
// The actual names can be found in DStrings.h.
//

char*	mapnames[] =	// DOOM shareware/registered/retail (Ultimate) names.
{

    HUSTR_E1M1,
    HUSTR_E1M2,
    HUSTR_E1M3,
    HUSTR_E1M4,
    HUSTR_E1M5,
    HUSTR_E1M6,
    HUSTR_E1M7,
    HUSTR_E1M8,
    HUSTR_E1M9,

    HUSTR_E2M1,
    HUSTR_E2M2,
    HUSTR_E2M3,
    HUSTR_E2M4,
    HUSTR_E2M5,
    HUSTR_E2M6,
    HUSTR_E2M7,
    HUSTR_E2M8,
    HUSTR_E2M9,

    HUSTR_E3M1,
    HUSTR_E3M2,
    HUSTR_E3M3,
    HUSTR_E3M4,
    HUSTR_E3M5,
    HUSTR_E3M6,
    HUSTR_E3M7,
    HUSTR_E3M8,
    HUSTR_E3M9,

    HUSTR_E4M1,
    HUSTR_E4M2,
    HUSTR_E4M3,
    HUSTR_E4M4,
    HUSTR_E4M5,
    HUSTR_E4M6,
    HUSTR_E4M7,
    HUSTR_E4M8,
    HUSTR_E4M9,

    "NEWLEVEL",
    "NEWLEVEL",
    "NEWLEVEL",
    "NEWLEVEL",
    "NEWLEVEL",
    "NEWLEVEL",
    "NEWLEVEL",
    "NEWLEVEL",
    "NEWLEVEL"
};

// List of names for levels in commercial IWADs
// (doom2.wad, plutonia.wad, tnt.wad).  These are stored in a
// single large array; WADs like pl2.wad have a MAP33, and rely on
// the layout in the Vanilla executable, where it is possible to
// overflow the end of one array into the next.


// The heads-up font is in the UIDIR of the image (HU_FONT).
void HU_Init(void)
{
}

void HU_Stop(void)
{
    headsupactive = false;
}

void HU_Start(void)
{

#ifndef DMC_FW
    char*	s;
#endif

    if (headsupactive)
	HU_Stop();

    plr = &players[consoleplayer];
    message_on = false;
    message_dontfuckwithme = false;
    message_nottobefuckedwith = false;

    // create the message widget
    HUlib_initSText(&w_message,
		    HU_MSGX, HU_MSGY, HU_MSGHEIGHT,
		    DMC_UI_STCFN033,
		    HU_FONTSTART, &message_on);

#ifndef DMC_FW
    // create the map title widget
    HUlib_initTextLine(&w_title,
		       HU_TITLEX, HU_TITLEY,
		       DMC_UI_STCFN033,
		       HU_FONTSTART);
    
    // level name from the image
    s = P_String(M.name_string);
    
    while (*s)
	HUlib_addCharToTextLine(&w_title, *(s++));
#endif

    headsupactive = true;

}

//
// HU_Drawer
// The message line is in the status band (port/render/r_band.c): it is
// marked for the compositor when what it shows changed. The automap
// title of the host build goes to the temporary screen.
//
void HU_Drawer(void)
{
    static unsigned	shown;
    unsigned		sig = 0;
    int			i;
    char*		c;

    if (*w_message.on)
    {
	sig = 2166136261u;
	for (i = 0; i < w_message.h; i++)
	    for (c = w_message.l[i].l; ; c++)
	    {
		sig = (sig ^ (unsigned char) *c) * 16777619u;
		if (!*c)
		    break;
	    }
	sig |= 1;
    }
    if (sig != shown)
    {
	shown = sig;
	R_BandMarkHud();
    }

#ifndef DMC_FW
    if (automapactive)
	HUlib_drawTextLine(&w_title, false);
#endif

}

// The message line for the band compositor.
void HU_DrawBand(void)
{
    HUlib_drawSText(&w_message);
}

void HU_Erase(void)
{

    HUlib_eraseSText(&w_message);
#ifndef DMC_FW
    HUlib_eraseTextLine(&w_title);
#endif

}

void HU_Ticker(void)
{

    // tick down message counter if message is up
    if (message_counter && !--message_counter)
    {
	message_on = false;
	message_nottobefuckedwith = false;
    }

    if (showMessages || message_dontfuckwithme)
    {

	// display message if necessary
	if ((plr->message && !message_nottobefuckedwith)
	    || (plr->message && message_dontfuckwithme))
	{
	    HUlib_addMessageToSText(&w_message, 0, plr->message);
	    plr->message = 0;
	    message_on = true;
	    message_counter = HU_MSGTIMEOUT;
	    message_nottobefuckedwith = message_dontfuckwithme;
	    message_dontfuckwithme = 0;
	}

    } // else message_on = false;

}

boolean HU_Responder(event_t *ev)
{

    boolean		eatkey = false;

    if (ev->type != ev_keydown)
	return false;

    if (ev->data1 == key_message_refresh)
    {
	message_on = true;
	message_counter = HU_MSGTIMEOUT;
	eatkey = true;
    }

    return eatkey;

}
