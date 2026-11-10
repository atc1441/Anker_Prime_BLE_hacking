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
// DESCRIPTION:  none
//



#include <string.h>
#include <stdlib.h>

#include "doomdef.h" 
#include "doomkeys.h"
#include "doomstat.h"

#include "deh_main.h"
#include "deh_misc.h"

#include "z_zone.h"
#include "f_finale.h"
#include "m_argv.h"
#include "m_controls.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"
#include "i_system.h"
#include "i_timer.h"
#include "rec.h"
#include "i_video.h"

#include "p_setup.h"
#include "p_tick.h"

#include "d_main.h"

#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"

// Needs access to LFB.
#include "v_video.h"

#include "fetch.h"
#include "img.h"
#include "fe.h"
#include "pad.h"

#include "p_local.h" 

#include "s_sound.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

// SKY handling - still the wrong place.
#include "r_data.h"
#include "r_sky.h"



#include "g_game.h"


void	G_ReadDemoTiccmd (ticcmd_t* cmd); 
void	G_PlayerReborn (int player); 
 
void	G_DoReborn (int playernum); 
 
void	G_DoLoadLevel (void); 
void	G_DoNewGame (void); 
void	G_DoLoadGame (void); 
void	G_DoPlayDemo (void); 
void	G_DoCompleted (void); 
void	G_DoVictory (void); 
void	G_DoWorldDone (void); 
void	G_DoSaveGame (void); 
 
// Gamestate the last time G_Ticker was called.

gamestate_t     oldgamestate; 
 
gameaction_t    gameaction; 
gamestate_t     gamestate; 
skill_t         gameskill; 
boolean		respawnmonsters;
int             gameepisode; 
int             gamemap; 

boolean         paused; 
boolean         sendpause;             	// send a pause event next tic 
boolean         usergame;               // ok to save / end game 
 
boolean         timingdemo;             // if true, exit with report on completion 
int             starttime;          	// for comparative timing purposes  	 
 
boolean         viewactive; 
 
unsigned char   playeringame[MAXPLAYERS]; 
player_t        players[NUMPLAYERS]; 

unsigned char   turbodetected[MAXPLAYERS];
 
int             consoleplayer;          // player taking events and displaying 
int             displayplayer;          // view being displayed 
int             levelstarttic;          // gametic at level start 
int             totalkills, totalitems, totalsecret;    // for intermission 
 
boolean         longtics;               // cph's doom 1.91 longtics hack
boolean         demoplayback; 
#define DEMOMARKER		0x80

// Demo bytes are read from the image, or from a .lmp file (demofile),
// through a 16 byte window. A load during a demo sets the offset (the
// test loads of port/game/g_save.c).
uint32_t	demo_p;		// logical image or file offset of the next byte
uint32_t	demoend;
uint32_t	demobufoff;
static byte	demobuf[16];
static boolean	demofile;

static byte G_DemoPeek (void)
{
    if (demo_p >= demoend)
	return DEMOMARKER;
    if (demo_p < demobufoff || demo_p >= demobufoff + sizeof(demobuf))
    {
	uint32_t n = demoend - demo_p < sizeof(demobuf) ? demoend - demo_p : sizeof(demobuf);

	demobufoff = demo_p;
	if (demofile)
	    dmc_demo_read (demobufoff, demobuf, n);
	else
	    img_read (demobufoff, demobuf, n);
    }
    return demobuf[demo_p - demobufoff];
}

static byte G_DemoByte (void)
{
    byte	b = G_DemoPeek ();

    demo_p++;
    return b;
}

boolean         singledemo;            	// quit after playing a demo from cmdline 
 
boolean         precache = true;        // if true, load all graphics at start 

 
wbstartstruct_t wminfo;               	// parms for world map / intermission 
 
#define MAXPLMOVE		(forwardmove[1]) 
 
#define TURBOTHRESHOLD	0x32

static const fixed_t forwardmove[2] = {0x19, 0x32}; 
static const fixed_t sidemove[2] = {0x18, 0x28}; 
static const fixed_t angleturn[3] = {640, 1280, 320};    // + slow turn 

static const int *const weapon_keys[] = {
    &key_weapon1,
    &key_weapon2,
    &key_weapon3,
    &key_weapon4,
    &key_weapon5,
    &key_weapon6,
    &key_weapon7,
    &key_weapon8
};

#define SLOWTURNTICS	6 
 
#define NUMKEYS		256 

// One bit per key (32 B in the fixed core of the firmware)
static unsigned int gamekeydown[NUMKEYS / 32];
#define KEYDOWN(k)	((gamekeydown[(k) >> 5] >> ((k) & 31)) & 1)
static int      turnheld;		// for accelerative turning 
 
//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer. 
// If recording a demo, write it out 
// 
/* The GD32 build fills this from the one physical button. Other builds
   leave it undefined, and an undefined weak symbol is a null pointer. */
void gd32_tic_hook(ticcmd_t *cmd) __attribute__((weak));

void G_BuildTiccmd (ticcmd_t* cmd, int maketic) 
{ 
    int		i; 
    boolean	strafe;
    int		speed;
    int		tspeed; 
    int		forward;
    int		side;

    memset(cmd, 0, sizeof(ticcmd_t));

    strafe = KEYDOWN(key_strafe); 

    speed = key_speed >= NUMKEYS
         || KEYDOWN(key_speed)
         || (dmc_pad.buttons & PAD_RUN);
 
    forward = side = 0;
    
    // use two stage accelerative turning
    // on the keyboard
    if (KEYDOWN(key_right)
	|| KEYDOWN(key_left)) 
	turnheld += 1; 
    else 
	turnheld = 0; 

    if (turnheld < SLOWTURNTICS) 
	tspeed = 2;             // slow turn 
    else 
	tspeed = speed;
    
    // let movement keys cancel each other out
    if (strafe) 
    { 
	if (KEYDOWN(key_right)) 
	{
	    // fprintf(stderr, "strafe right\n");
	    side += sidemove[speed]; 
	}
	if (KEYDOWN(key_left)) 
	{
	    //	fprintf(stderr, "strafe left\n");
	    side -= sidemove[speed]; 
	}
    } 
    else 
    { 
	if (KEYDOWN(key_right)) 
	    cmd->angleturn -= angleturn[tspeed]; 
	if (KEYDOWN(key_left)) 
	    cmd->angleturn += angleturn[tspeed]; 
    } 
 
    if (KEYDOWN(key_up)) 
    {
	// fprintf(stderr, "up\n");
	forward += forwardmove[speed]; 
    }
    if (KEYDOWN(key_down)) 
    {
	// fprintf(stderr, "down\n");
	forward -= forwardmove[speed]; 
    }

    if (KEYDOWN(key_strafeleft))
    {
        side -= sidemove[speed];
    }

    if (KEYDOWN(key_straferight))
    {
        side += sidemove[speed]; 
    }

    // buttons
    if (KEYDOWN(key_fire)) 
	cmd->buttons |= BT_ATTACK; 
 
    if (KEYDOWN(key_use))
    { 
	cmd->buttons |= BT_USE;
    } 

    // Check weapon keys.

    for (i=0; i<arrlen(weapon_keys); ++i)
    {
        int key = *weapon_keys[i];

        if (KEYDOWN(key))
        {
            cmd->buttons |= BT_CHANGE;
            cmd->buttons |= i<<BT_WEAPONSHIFT;
            break;
        }
    }

    // the game pad's sticks, fire and use (port/game/pad.h)
    if (dmc_pad.buttons & PAD_PRESENT && !demoplayback && !menuactive)
    {
	forward += II_PadAxis(dmc_pad.axis[PAD_LY], forwardmove[speed]);
	side += II_PadAxis(dmc_pad.axis[PAD_LX], sidemove[speed]);
	cmd->angleturn -= II_PadAxis(dmc_pad.axis[PAD_RX], angleturn[1]);
	cmd->buttons |= dmc_pad.buttons & (PAD_FIRE | PAD_USE);
    }

    if (forward > MAXPLMOVE) 
	forward = MAXPLMOVE; 
    else if (forward < -MAXPLMOVE) 
	forward = -MAXPLMOVE; 
    if (side > MAXPLMOVE) 
	side = MAXPLMOVE; 
    else if (side < -MAXPLMOVE) 
	side = -MAXPLMOVE; 
 
    cmd->forwardmove += forward; 
    cmd->sidemove += side;
    
    // special buttons
    if (sendpause) 
    { 
	sendpause = false; 
	cmd->buttons = BT_SPECIAL | BTS_PAUSE; 
    } 
 
    // sendsave and savegameslot are fe.slot (port/game/fe.h)
    if (fe.slot & FE_SENDSAVE) 
    { 
	fe.slot &= ~FE_SENDSAVE; 
	cmd->buttons = BT_SPECIAL | BTS_SAVEGAME | (fe.slot<<BTS_SAVESHIFT); 
    } 

    if (gd32_tic_hook)
        gd32_tic_hook(cmd);
} 
 

//
// G_DoLoadLevel 
//
void G_DoLoadLevel (void) 
{ 
    int             i; 

    // Set the sky map.
    // First thing, we have a dummy sky texture name,
    //  a flat. The data is in the WAD only because
    //  we look for an actual index, instead of simply
    //  setting one.

    // F_SKY1 is per-map flat index 0 in every map block
    skyflatnum = 0;

#ifdef DMC_STATS
    // host: no front-end state outlives a level setup (fe.h)
    memset (&dmc_tic, 0xa5, sizeof(dmc_tic));
#endif

    levelstarttic = gametic;        // for time calculation
    
    if (wipegamestate == GS_LEVEL) 
	wipegamestate = -1;             // force a wipe 

    gamestate = GS_LEVEL; 

    for (i=0 ; i<NUMPLAYERS ; i++) 
    { 
	turbodetected[i] = false;
	if (playeringame[i] && players[i].playerstate == PST_DEAD) 
	    players[i].playerstate = PST_REBORN; 
	memset (players[i].frags,0,sizeof(players[i].frags)); 
    } 
		 
    P_SetupLevel (gameepisode, gamemap, 0, gameskill);    
    displayplayer = consoleplayer;		// view the guy you are playing    
    gameaction = ga_nothing; 
    Z_CheckHeap ();
    
    // clear cmd building stuff

    memset (gamekeydown, 0, sizeof(gamekeydown));
    fe.slot &= ~FE_SENDSAVE;
    sendpause = paused = false;
} 

//
// G_Responder  
// Get info needed to make ticcmd_ts for the players.
// 
boolean G_Responder (event_t* ev) 
{ 
    // any other key pops up menu if in demos
    if (gameaction == ga_nothing && !singledemo && 
	(demoplayback || gamestate == GS_DEMOSCREEN) 
	) 
    { 
	if (ev->type == ev_keydown) 
	{ 
	    M_StartControlPanel (); 
	    return true; 
	} 
	return false; 
    } 

    if (gamestate == GS_LEVEL) 
    { 
	if (HU_Responder (ev)) 
	    return true;	// chat ate the event 
	if (ST_Responder (ev)) 
	    return true;	// status window ate it 
	if (AM_Responder (ev)) 
	    return true;	// automap ate it 
    } 
	 
    if (gamestate == GS_FINALE) 
    { 
	if (F_Responder (ev)) 
	    return true;	// finale ate the event 
    } 

    switch (ev->type) 
    { 
      case ev_keydown: 
	if (ev->data1 == key_pause) 
	{ 
	    sendpause = true; 
	}
        else if (ev->data1 <NUMKEYS) 
        {
	    gamekeydown[ev->data1 >> 5] |= 1u << (ev->data1 & 31);
        }

	return true;    // eat key down events 
 
      case ev_keyup: 
	if (ev->data1 <NUMKEYS) 
	    gamekeydown[ev->data1 >> 5] &= ~(1u << (ev->data1 & 31));
	return false;   // always let key up events filter down 
 
      default: 
	break; 
    } 
 
    return false; 
} 
 
 
 
//
// G_Ticker
// Make ticcmd_ts for the players.
//
void G_Ticker (void) 
{ 
    int		i;
    ticcmd_t*	cmd;
    
    // do player reborns if needed
    for (i=0 ; i<MAXPLAYERS ; i++) 
	if (playeringame[i] && players[i].playerstate == PST_REBORN) 
	    G_DoReborn (i);
    
    // do things to change the game state
    while (gameaction != ga_nothing) 
    { 
	switch (gameaction) 
	{ 
	  case ga_loadlevel: 
	    G_DoLoadLevel (); 
	    break; 
	  case ga_newgame: 
	    G_DoNewGame (); 
	    break; 
	  case ga_loadgame: 
	    G_DoLoadGame (); 
	    break; 
	  case ga_savegame: 
	    G_DoSaveGame (); 
	    break; 
	  case ga_playdemo: 
	    G_DoPlayDemo (); 
	    break; 
	  case ga_completed: 
	    G_DoCompleted (); 
	    break; 
	  case ga_victory: 
	    F_StartFinale (); 
	    break; 
	  case ga_worlddone: 
	    G_DoWorldDone (); 
	    break; 
	  case ga_wistart: 
	    gameaction = ga_nothing; 
	    WI_Start (&wminfo); 
	    break; 
	  case ga_nothing: 
	    break; 
	} 
    }
    
    // get commands
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (playeringame[i]) 
	{ 
	    cmd = &players[i].cmd; 

	    memcpy(cmd, &netcmds[i], sizeof(ticcmd_t));

	    if (demoplayback) 
		G_ReadDemoTiccmd (cmd); 
	    
	    // check for turbo cheats

            // check ~ 4 seconds whether to display the turbo message. 
            // store if the turbo threshold was exceeded in any tics
            // over the past 4 seconds.  offset the checking period
            // for each player so messages are not displayed at the
            // same time.

            if (cmd->forwardmove > TURBOTHRESHOLD)
            {
                turbodetected[i] = true;
            }

            if ((gametic & 31) == 0 
             && ((gametic >> 5) % MAXPLAYERS) == i
             && turbodetected[i])
            {
                static char turbomessage[20];
                extern const char *const player_names[4];
                M_snprintf(turbomessage, sizeof(turbomessage),
                           "%s is turbo!", player_names[i]);
                players[consoleplayer].message = turbomessage;
                turbodetected[i] = false;
            }
	}
    }
    
    // check for special buttons
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (playeringame[i]) 
	{ 
	    if (players[i].cmd.buttons & BT_SPECIAL) 
	    { 
		switch (players[i].cmd.buttons & BT_SPECIALMASK) 
		{ 
		  case BTS_PAUSE: 
		    paused ^= 1; 
		    break; 
					 
		  case BTS_SAVEGAME:
		    fe.slot =  
			(players[i].cmd.buttons & BTS_SAVEMASK)>>BTS_SAVESHIFT; 
		    gameaction = ga_savegame; 
		    break; 
		} 
	    } 
	}
    }

    // Have we just finished displaying an intermission screen?

    if (oldgamestate == GS_INTERMISSION && gamestate != GS_INTERMISSION)
    {
        WI_End();
    }

    oldgamestate = gamestate;
    
    // do main actions
    switch (gamestate) 
    { 
      case GS_LEVEL: 
	P_Ticker (); 
	ST_Ticker (); 
	AM_Ticker (); 
	HU_Ticker ();            
	break; 
	 
      case GS_INTERMISSION: 
	WI_Ticker (); 
	break; 
			 
      case GS_FINALE: 
	F_Ticker (); 
	break; 
 
      case GS_DEMOSCREEN: 
	D_PageTicker (); 
	break;
    }        
} 
 
 
//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_InitPlayer 
// Called at the start.
// Called by the game initialization functions.
//
void G_InitPlayer (int player) 
{
    // clear everything else to defaults
    G_PlayerReborn (player); 
}
 
 

//
// G_PlayerFinishLevel
// Can when a player completes a level.
//
void G_PlayerFinishLevel (int player) 
{ 
    player_t*	p; 
	 
    p = &players[player]; 
	 
    memset (p->powers, 0, sizeof (p->powers)); 
    memset (p->cards, 0, sizeof (p->cards)); 
    p->mo->flags &= ~MF_SHADOW;		// cancel invisibility 
    p->extralight = 0;			// cancel gun flashes 
    p->fixedcolormap = 0;		// cancel ir gogles 
    p->damagecount = 0;			// no palette changes 
    p->bonuscount = 0; 
} 
 

//
// G_PlayerReborn
// Called after a player dies 
// almost everything is cleared and initialized 
//
void G_PlayerReborn (int player) 
{ 
    player_t*	p; 
    int		i; 
    int		frags[MAXPLAYERS]; 
    int		killcount;
    int		itemcount;
    int		secretcount; 
	 
    memcpy (frags,players[player].frags,sizeof(frags)); 
    killcount = players[player].killcount; 
    itemcount = players[player].itemcount; 
    secretcount = players[player].secretcount; 
	 
    p = &players[player]; 
    memset (p, 0, sizeof(*p)); 
 
    memcpy (players[player].frags, frags, sizeof(players[player].frags)); 
    players[player].killcount = killcount; 
    players[player].itemcount = itemcount; 
    players[player].secretcount = secretcount; 
 
    p->usedown = p->attackdown = true;	// don't do anything immediately 
    p->playerstate = PST_LIVE;       
    p->health = deh_initial_health;     // Use dehacked value
    p->readyweapon = p->pendingweapon = wp_pistol; 
    p->weaponowned[wp_fist] = true; 
    p->weaponowned[wp_pistol] = true; 
    p->ammo[am_clip] = deh_initial_bullets; 
	 
    for (i=0 ; i<NUMAMMO ; i++) 
	p->maxammo[i] = maxammo[i]; 
		 
}

//
// G_DoReborn 
// 
void G_DoReborn (int playernum) 
{ 
    // reload the level from scratch
    gameaction = ga_loadlevel;  
} 
 

// DOOM Par Times
static const int pars[4][10] = 
{ 
    {0}, 
    {0,30,75,120,90,165,180,180,30,165}, 
    {0,90,90,90,120,90,360,240,30,170}, 
    {0,90,45,90,150,90,90,165,30,135} 
}; 

// DOOM II Par Times, episode 4 reads them (see G_DoCompleted)
static const int cpars[10] =
{
    30,90,120,120,90,150,120,120,270,90,	//  1-10
};
 

//
// G_DoCompleted 
//
boolean		secretexit; 
extern char*	pagename; 
 
void G_ExitLevel (void) 
{ 
    secretexit = false; 
    gameaction = ga_completed; 
} 

// Here's for the german edition.
void G_SecretExitLevel (void) 
{ 
    secretexit = true; 
    gameaction = ga_completed; 
} 
 
void G_DoCompleted (void) 
{ 
    int             i; 
	 
    gameaction = ga_nothing; 
 
    for (i=0 ; i<MAXPLAYERS ; i++) 
	if (playeringame[i]) 
	    G_PlayerFinishLevel (i);        // take away cards and stuff 
	 
    if (automapactive) 
	AM_Stop (); 
	
    switch(gamemap)
    {
      case 8:
        gameaction = ga_victory;
        return;
      case 9: 
        for (i=0 ; i<NUMPLAYERS ; i++) 
            players[i].didsecret = true; 
        break;
    }
    
	 
    wminfo.didsecret = players[consoleplayer].didsecret; 
    wminfo.epsd = gameepisode -1; 
    wminfo.last = gamemap -1;
    
    // wminfo.next is 0 biased, unlike gamemap
    {
	if (secretexit) 
	    wminfo.next = 8; 	// go to secret level 
	else if (gamemap == 9) 
	{
	    // returning from secret level 
	    switch (gameepisode) 
	    { 
	      case 1: 
		wminfo.next = 3; 
		break; 
	      case 2: 
		wminfo.next = 5; 
		break; 
	      case 3: 
		wminfo.next = 6; 
		break; 
	      case 4:
		wminfo.next = 2;
		break;
	    }                
	} 
	else 
	    wminfo.next = gamemap;          // go to next level 
    }
		 
    wminfo.maxkills = totalkills; 
    wminfo.maxitems = totalitems; 
    wminfo.maxsecret = totalsecret; 
    wminfo.maxfrags = 0; 

    // Set par time. Doom episode 4 doesn't have a par time, so this
    // overflows into the cpars array. It's necessary to emulate this
    // for statcheck regression testing.
    if (gameepisode < 4)
	wminfo.partime = TICRATE*pars[gameepisode][gamemap];
    else
        wminfo.partime = TICRATE*cpars[gamemap];

    wminfo.pnum = consoleplayer; 
 
    for (i=0 ; i<NUMPLAYERS ; i++) 
    { 
	wminfo.plyr[i].in = playeringame[i]; 
	wminfo.plyr[i].skills = players[i].killcount; 
	wminfo.plyr[i].sitems = players[i].itemcount; 
	wminfo.plyr[i].ssecret = players[i].secretcount; 
	wminfo.plyr[i].stime = leveltime; 
	memcpy (wminfo.plyr[i].frags, players[i].frags 
		, sizeof(wminfo.plyr[i].frags)); 
    } 
 
    gamestate = GS_INTERMISSION; 
    viewactive = false; 
    automapactive = false; 

    // G_Ticker starts the intermission next: on the meter this runs in
    // an overlay, and overlays never call resident code that loads one
    // (port/ovl/ovl.h)
    gameaction = ga_wistart;
} 


//
// G_WorldDone 
//
void G_WorldDone (void) 
{ 
    gameaction = ga_worlddone; 

    if (secretexit) 
	players[consoleplayer].didsecret = true; 
} 
 
void G_DoWorldDone (void) 
{        
    // The map table of the pack names the next map. When the pack
    // does not have it (the E1M1 pack), the game ends after the
    // intermission and the title loop starts.
    if ((secretexit ? M.secret_next : M.next) == DMC_NEXT_NOT_IN_PACK)
    {
	D_StartTitle ();
	return;
    }
    gamestate = GS_LEVEL; 
    gamemap = wminfo.next+1; 
    G_DoLoadLevel (); 
    gameaction = ga_nothing; 
    viewactive = true; 
} 
 


//
// G_LoadGame
// Called by the menu task: slot of SAVE.DAT, loaded by G_DoLoadGame
// (port/game/fe_glue.c).
//
void G_LoadGame (int slot) 
{ 
    fe.slot = slot; 
    gameaction = ga_loadgame; 
} 


//
// G_SaveGame
// Called by the menu task. The save writes its description itself.
//
void G_SaveGame (int slot) 
{ 
    fe.slot = slot | FE_SENDSAVE; 
} 
 


//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set. 
//
skill_t	d_skill; 
int     d_episode; 
int     d_map; 
 
void
G_DeferedInitNew
( skill_t	skill,
  int		episode,
  int		map) 
{ 
    d_skill = skill; 
    d_episode = episode; 
    d_map = map; 
    gameaction = ga_newgame; 
} 


void G_DoNewGame (void) 
{
    demoplayback = false; 
    playeringame[1] = playeringame[2] = playeringame[3] = 0;
    respawnparm = false;
    fastparm = false;
    nomonsters = false;
    consoleplayer = 0;
    G_InitNew (d_skill, d_episode, d_map); 
    gameaction = ga_nothing; 
} 


void
G_InitNew
( skill_t	skill,
  int		episode,
  int		map )
{
    int             i;

    paused = false;

    /*
    // Note: This commented-out block of code was added at some point
    // between the DOS version(s) and the Doom source release. It isn't
    // found in disassemblies of the DOS version and causes IDCLEV and
    // the -warp command line parameter to behave differently.
    // This is left here for posterity.

    // This was quite messy with SPECIAL and commented parts.
    // Supposedly hacks to make the latest edition work.
    // It might not work properly.
    if (episode < 1)
      episode = 1;

    if ( gamemode == retail )
    {
      if (episode > 4)
	episode = 4;
    }
    else if ( gamemode == shareware )
    {
      if (episode > 1)
	   episode = 1;	// only start episode 1 on shareware
    }
    else
    {
      if (episode > 3)
	episode = 3;
    }
    */

    // Nightmare, -fast and -respawn are not in this port, and the port must
    // play like vanilla: such a game or demo stops here instead of diverging.
    if (skill > sk_hard || fastparm || respawnparm)
	I_Error("G_InitNew: nightmare, -fast and -respawn are not supported");

    if (episode == 0)
    {
        episode = 4;
    }

    if (map < 1)
	map = 1;

    if (map > 9)
      map = 9;

    M_ClearRandom ();

    respawnmonsters = false;

    // force players to be initialized upon first level load
    for (i=0 ; i<NUMPLAYERS ; i++)
	players[i].playerstate = PST_REBORN;

    usergame = true;                // will be set false if a demo
    paused = false;
    demoplayback = false;
    automapactive = false;
    viewactive = true;
    gameepisode = episode;
    gamemap = map;
    gameskill = skill;

    viewactive = true;

    // Set the sky to use.
    //
    // Note: This IS broken, but it is how Vanilla Doom behaves.
    // See http://doomwiki.org/wiki/Sky_never_changes_in_Doom_II.
    //
    // Because we set the sky here at the start of a game, not at the
    // start of a level, the sky texture never changes unless we
    // restore from a saved game.  This was fixed before the Doom
    // source release, but this IS the way Vanilla DOS Doom behaves.

    // The sky texture is a field of the map table entry, P_SetupLevel
    // sets it (every map of episode 1 has SKY1).

    G_DoLoadLevel ();
}


//
// DEMO RECORDING 
// 



void G_ReadDemoTiccmd (ticcmd_t* cmd) 
{ 
    if (G_DemoPeek() == DEMOMARKER) 
    {
	// end of demo data stream 
	G_CheckDemoStatus (); 
	return; 
    } 
    cmd->forwardmove = ((signed char)G_DemoByte()); 
    cmd->sidemove = ((signed char)G_DemoByte()); 

    // If this is a longtics demo, read back in higher resolution

    if (longtics)
    {
        cmd->angleturn = G_DemoByte();
        cmd->angleturn |= (G_DemoByte()) << 8;
    }
    else
    {
        cmd->angleturn = ((unsigned char) G_DemoByte())<<8; 
    }

    cmd->buttons = (unsigned char)G_DemoByte(); 
} 

// Get the demo version code appropriate for the version set in gameversion.
// Ultimate Doom is a variant of v1.9.
int G_VanillaVersionCode(void)
{
    return 109;
}

//
// G_PlayDemo 
//

char*	defdemoname; 
 
void G_DeferedPlayDemo (char* name) 
{ 
    defdemoname = name; 
    gameaction = ga_playdemo; 
} 

void G_DoPlayDemo (void) 
{ 
    skill_t skill; 
    int             i, episode, map; 
    int demoversion;
	 
    gameaction = ga_nothing; 
    // DEMOn is demo n-1 of the image, a name with a dot a .lmp file
    demofile = false;
    for (i = 0; defdemoname[i]; i++)
	if (defdemoname[i] == '.')
	    demofile = true;
    demo_p = 0;
    if (demofile ? dmc_demo_open(defdemoname, &demoend) < 0
	: strncasecmp(defdemoname, "demo", 4)
	  || demo_get(atoi(defdemoname + 4) - 1, &demo_p, &demoend) < 0)
    {
	if (singledemo)
	    I_Error("G_DoPlayDemo: no demo %s", defdemoname);
	// the attract loop skips demos the pack does not have
	D_AdvanceDemo ();
	return;
    }
    demoend += demo_p;
    demobufoff = demoend; 

    demoversion = G_DemoByte();

    if (demoversion == G_VanillaVersionCode())
    {
        longtics = false;
    }
    else if (demoversion == DOOM_191_VERSION)
    {
        // demo recorded with cph's modified "v1.91" doom exe
        longtics = true;
    }
    // Upstream prints a warning for other versions and plays anyway.
    
    skill = G_DemoByte(); 
    episode = G_DemoByte(); 
    map = G_DemoByte(); 
    /* A pack that does not contain this map skips the demo instead of
       stopping on the fault screen. */
    if (map_find(episode, map) < 0) {
        D_AdvanceDemo();
        return;
    }
    if (G_DemoByte())
	I_Error("G_DoPlayDemo: deathmatch demos are not supported");
    respawnparm = G_DemoByte();
    fastparm = G_DemoByte();
    nomonsters = G_DemoByte();
    consoleplayer = G_DemoByte();
	
    for (i=0 ; i<MAXPLAYERS ; i++) 
	playeringame[i] = G_DemoByte(); 

    if (consoleplayer != 0 || !playeringame[0]
     || playeringame[1] || playeringame[2] || playeringame[3])
    {
	I_Error("G_DoPlayDemo: only single player demos are supported");
    }

    // don't spend a lot of time in loadlevel 
    precache = false;
    G_InitNew (skill, episode, map); 
    precache = true; 
    starttime = I_GetTime (); 

    usergame = false; 
    demoplayback = true; 
} 

//
// G_TimeDemo 
//
void G_TimeDemo (char* name) 
{
    //!
    // @vanilla 
    //
    // Disable rendering the screen entirely.
    //


    timingdemo = true; 
    singletics = true; 

    defdemoname = name; 
    gameaction = ga_playdemo; 
} 
 
 
/* 
=================== 
= 
= G_CheckDemoStatus 
= 
= Called after a death or level completion to allow demos to be cleaned up 
= Returns true if a new demo loop action will take place 
=================== 
*/ 
 
boolean G_CheckDemoStatus (void) 
{ 
    int             endtime; 
	 
    if (timingdemo) 
    { 
        int realtics;

	endtime = I_GetTime (); 
        realtics = endtime - starttime;

        // Prevent recursive calls
        timingdemo = false;
        demoplayback = false;

	I_Error ("timed %i gametics in %i realtics",
                 gametic, realtics);
    } 
	 
    if (demoplayback) 
    { 
	demoplayback = false; 
	playeringame[1] = playeringame[2] = playeringame[3] = 0;
	respawnparm = false;
	fastparm = false;
	nomonsters = false;
	consoleplayer = 0;
        
        if (singledemo) 
            I_Quit (); 
        else 
            D_AdvanceDemo (); 

	return true; 
    } 
 
    return false; 
} 
 
 
 
