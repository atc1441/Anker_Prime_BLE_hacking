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
//	Status bar code.
//	Does the face/direction indicator animatin.
//	Does palette indicators as well (red pain/berserk, bright pickup)
//



#include <stdio.h>

#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "m_misc.h"
#include "m_random.h"
#include "dmcdat.h"

#include "deh_main.h"
#include "deh_misc.h"
#include "doomdef.h"
#include "doomkeys.h"

#include "g_game.h"

#include "st_stuff.h"
#include "st_lib.h"
#include "r_local.h"

#include "p_local.h"
#include "p_inter.h"

#include "am_map.h"

#include "s_sound.h"

// Needs access to LFB.
#include "v_video.h"

// State.
#include "doomstat.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

//
// STATUS BAR DATA
//


// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS		1
#define STARTBONUSPALS		9
#define NUMREDPALS			8
#define NUMBONUSPALS		4
// Radiation suit, green shift.
#define RADIATIONPAL		13

// N/256*100% probability
//  that the normal face state will change
#define ST_FACEPROBABILITY		96

// For Responder
#define ST_TOGGLECHAT		KEY_ENTER

// Location of status bar
#define ST_X				0
#define ST_X2				104

#define ST_FX  			143
#define ST_FY  			169

// Should be set to patch width
//  for tall numbers later on
#define ST_TALLNUMWIDTH		(V_PatchWidth(V_UIPatch(DMC_UI_STTNUM0)))

// Number of status faces.
#define ST_NUMPAINFACES		5
#define ST_NUMSTRAIGHTFACES	3
#define ST_NUMTURNFACES		2
#define ST_NUMSPECIALFACES		3

#define ST_FACESTRIDE \
          (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES		2

#define ST_NUMFACES \
          (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES)

#define ST_TURNOFFSET		(ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET		(ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET		(ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET		(ST_EVILGRINOFFSET + 1)
#define ST_GODFACE			(ST_NUMPAINFACES*ST_FACESTRIDE)
#define ST_DEADFACE			(ST_GODFACE+1)

#define ST_FACESX			143
#define ST_FACESY			168

#define ST_EVILGRINCOUNT		(2*TICRATE)
#define ST_STRAIGHTFACECOUNT	(TICRATE/2)
#define ST_TURNCOUNT		(1*TICRATE)
#define ST_OUCHCOUNT		(1*TICRATE)
#define ST_RAMPAGEDELAY		(2*TICRATE)

#define ST_MUCHPAIN			20


// Location and size of statistics,
//  justified according to widget type.
// Problem is, within which space? STbar? Screen?
// Note: this could be read in by a lump.
//       Problem is, is the stuff rendered
//       into a buffer,
//       or into the frame buffer?

// AMMO number pos.
#define ST_AMMOWIDTH		3	
#define ST_AMMOX			44
#define ST_AMMOY			171

// HEALTH number pos.
#define ST_HEALTHWIDTH		3	
#define ST_HEALTHX			90
#define ST_HEALTHY			171

// Weapon pos.
#define ST_ARMSX			111
#define ST_ARMSY			172
#define ST_ARMSBGX			104
#define ST_ARMSBGY			168
#define ST_ARMSXSPACE		12
#define ST_ARMSYSPACE		10

// Frags pos.
#define ST_FRAGSX			138
#define ST_FRAGSY			171	
#define ST_FRAGSWIDTH		2

// ARMOR number pos.
#define ST_ARMORWIDTH		3
#define ST_ARMORX			221
#define ST_ARMORY			171

// Key icon positions.
#define ST_KEY0WIDTH		8
#define ST_KEY0HEIGHT		5
#define ST_KEY0X			239
#define ST_KEY0Y			171
#define ST_KEY1WIDTH		ST_KEY0WIDTH
#define ST_KEY1X			239
#define ST_KEY1Y			181
#define ST_KEY2WIDTH		ST_KEY0WIDTH
#define ST_KEY2X			239
#define ST_KEY2Y			191

// Ammunition counter.
#define ST_AMMO0WIDTH		3
#define ST_AMMO0HEIGHT		6
#define ST_AMMO0X			288
#define ST_AMMO0Y			173
#define ST_AMMO1WIDTH		ST_AMMO0WIDTH
#define ST_AMMO1X			288
#define ST_AMMO1Y			179
#define ST_AMMO2WIDTH		ST_AMMO0WIDTH
#define ST_AMMO2X			288
#define ST_AMMO2Y			191
#define ST_AMMO3WIDTH		ST_AMMO0WIDTH
#define ST_AMMO3X			288
#define ST_AMMO3Y			185

// Indicate maximum ammunition.
// Only needed because backpack exists.
#define ST_MAXAMMO0WIDTH		3
#define ST_MAXAMMO0HEIGHT		5
#define ST_MAXAMMO0X		314
#define ST_MAXAMMO0Y		173
#define ST_MAXAMMO1WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO1X		314
#define ST_MAXAMMO1Y		179
#define ST_MAXAMMO2WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO2X		314
#define ST_MAXAMMO2Y		191
#define ST_MAXAMMO3WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO3X		314
#define ST_MAXAMMO3Y		185

// pistol
#define ST_WEAPON0X			110 
#define ST_WEAPON0Y			172

// shotgun
#define ST_WEAPON1X			122 
#define ST_WEAPON1Y			172

// chain gun
#define ST_WEAPON2X			134 
#define ST_WEAPON2Y			172

// missile launcher
#define ST_WEAPON3X			110 
#define ST_WEAPON3Y			181

// plasma gun
#define ST_WEAPON4X			122 
#define ST_WEAPON4Y			181

 // bfg
#define ST_WEAPON5X			134
#define ST_WEAPON5Y			181

// WPNS title
#define ST_WPNSX			109 
#define ST_WPNSY			191

 // DETH title
#define ST_DETHX			109
#define ST_DETHY			191

//Incoming messages window location
//UNUSED
// #define ST_MSGTEXTX	   (viewwindowx)
// #define ST_MSGTEXTY	   (viewwindowy+viewheight-18)
#define ST_MSGTEXTX			0
#define ST_MSGTEXTY			0
// Dimensions given in characters.
#define ST_MSGWIDTH			52
// Or shall I say, in lines?
#define ST_MSGHEIGHT		1

#define ST_OUTTEXTX			0
#define ST_OUTTEXTY			6

// Width, in characters again.
#define ST_OUTWIDTH			52 
 // Height, in lines. 
#define ST_OUTHEIGHT		1

#define ST_MAPTITLEX \
    (SCREENWIDTH - ST_MAPWIDTH * ST_CHATFONTWIDTH)

#define ST_MAPTITLEY		0
#define ST_MAPHEIGHT		1

// graphics are drawn to a backing screen and blitted to the real screen
byte                   *st_backing_screen;
	    
// main player in game
static player_t*	plyr; 

// ST_Start() has just been called
static boolean		st_firsttime;

// lump number for PLAYPAL

// used for timing
static unsigned int	st_clock;

// used for making messages go away
static int		st_msgcounter=0;

// used when in chat 
static st_chatstateenum_t	st_chatstate;

// whether in automap or first-person
static st_stateenum_t	st_gamestate;

// whether left-side main status bar is active
static boolean		st_statusbaron;

// whether status bar chat is active
static boolean		st_chat;

// value of st_chat before message popped up
static boolean		st_oldchat;

// whether chat window has the cursor on
static boolean		st_cursoron;

// !deathmatch
static boolean		st_notdeathmatch; 

// !deathmatch && st_statusbaron
static boolean		st_armson;

// !deathmatch
static boolean		st_fragson; 

// The status bar pictures are UIDIR entries of the image, the widgets
// keep their UI ids (DMC_UI_*) and look the patches up when they draw.
// The id lists are global: the widgets point at them, and the firmware's
// LEVEL overlay that sets the widgets up reaches them through the resident
// image (mk/fw.mk).
#define ST_UI10(b)	(b), (b)+1, (b)+2, (b)+3, (b)+4, (b)+5, (b)+6, (b)+7, (b)+8, (b)+9

// 0-9, tall numbers
const unsigned char	st_tallnum[10] = { ST_UI10(DMC_UI_STTNUM0) };

// 0-9, short, yellow (,different!) numbers
const unsigned char	st_shortnum[10] = { ST_UI10(DMC_UI_STYSNUM0) };

// 3 key-cards, 3 skulls
const unsigned char	st_keys[NUMCARDS] =
{
    DMC_UI_STKEYS0, DMC_UI_STKEYS0+1, DMC_UI_STKEYS0+2,
    DMC_UI_STKEYS0+3, DMC_UI_STKEYS0+4, DMC_UI_STKEYS0+5
};

// face status patches: per pain level the straight faces, turn right,
// turn left, ouch, evil grin and pissed off, then god mode and dead
#define ST_FACES(i) \
    DMC_UI_STFST00 + (i)*ST_NUMSTRAIGHTFACES, \
    DMC_UI_STFST00 + (i)*ST_NUMSTRAIGHTFACES + 1, \
    DMC_UI_STFST00 + (i)*ST_NUMSTRAIGHTFACES + 2, \
    DMC_UI_STFTR00 + (i), DMC_UI_STFTL00 + (i), DMC_UI_STFOUCH0 + (i), \
    DMC_UI_STFEVL0 + (i), DMC_UI_STFKILL0 + (i)
const unsigned char	st_faces[ST_NUMFACES] =
{
    ST_FACES(0), ST_FACES(1), ST_FACES(2), ST_FACES(3), ST_FACES(4),
    DMC_UI_STFGOD0, DMC_UI_STFDEAD0
};

// weapon ownership patches: gray #, yellow #
const unsigned char	st_arms[6][2] =
{
    { DMC_UI_STGNUM2, DMC_UI_STYSNUM0+2 }, { DMC_UI_STGNUM2+1, DMC_UI_STYSNUM0+3 },
    { DMC_UI_STGNUM2+2, DMC_UI_STYSNUM0+4 }, { DMC_UI_STGNUM2+3, DMC_UI_STYSNUM0+5 },
    { DMC_UI_STGNUM2+4, DMC_UI_STYSNUM0+6 }, { DMC_UI_STGNUM2+5, DMC_UI_STYSNUM0+7 }
};

_Static_assert(DMC_UI_STFDEAD0 < 256 && DMC_UI_STKEYS0 + NUMCARDS <= 256,
	       "status bar UI ids must fit a byte");

 // number of frags so far in deathmatch
static int	st_fragscount;

// used to use appopriately pained face
static int	st_oldhealth = -1;

// used for evil grin
static unsigned char	oldweaponsowned[NUMWEAPONS]; 

 // count until face changes
static int	st_facecount = 0;

// current face index, used by w_faces
static int	st_faceindex = 0;

// holds key-type for each key box on bar
static int	keyboxes[3]; 

// a random number per tick
static int	st_randomnumber;  

// ammo of the ready weapon, 1994 ("n/a") for none: w_ready shows it
static int	st_readyammo;

//
// The widgets (st_lib.h): a single player game has its player in
// players[0] (G_DoPlayDemo, G_InitNew), so they are constant. What each
// last drew is in st_states.
//
#define PL	players[0]

enum
{
    SS_READY, SS_FRAGS, SS_HEALTH, SS_ARMSBG, SS_ARMS, SS_FACES = SS_ARMS + 6,
    SS_KEYS, SS_ARMOR = SS_KEYS + 3, SS_AMMO, SS_MAXAMMO = SS_AMMO + 4,
    SS_COUNT = SS_MAXAMMO + 4
};
static st_state_t	st_states[SS_COUNT];

// ready-weapon widget
static const st_number_t	w_ready =
{ ST_AMMOX, ST_AMMOY, ST_AMMOWIDTH, &st_readyammo, &st_statusbaron,
  st_tallnum, &st_states[SS_READY] };

 // in deathmatch only, summary of frags stats
static const st_number_t	w_frags =
{ ST_FRAGSX, ST_FRAGSY, ST_FRAGSWIDTH, &st_fragscount, &st_fragson,
  st_tallnum, &st_states[SS_FRAGS] };

// health widget
static const st_percent_t	w_health =
{ { ST_HEALTHX, ST_HEALTHY, 3, &PL.health, &st_statusbaron, st_tallnum,
    &st_states[SS_HEALTH] }, DMC_UI_STTPRCNT };

// arms background
static const st_binicon_t	w_armsbg =
{ ST_ARMSBGX, ST_ARMSBGY, &st_notdeathmatch, &st_statusbaron, DMC_UI_STARMS,
  &st_states[SS_ARMSBG] };

// weapon ownership widgets
#define ST_ARM(i) \
{ ST_ARMSX+((i)%3)*ST_ARMSXSPACE, ST_ARMSY+((i)/3)*ST_ARMSYSPACE, \
  (int *) &PL.weaponowned[(i)+1], &st_armson, st_arms[i], &st_states[SS_ARMS + (i)] }
static const st_multicon_t	w_arms[6] =
{
    ST_ARM(0), ST_ARM(1), ST_ARM(2), ST_ARM(3), ST_ARM(4), ST_ARM(5)
};

// face status widget
static const st_multicon_t	w_faces =
{ ST_FACESX, ST_FACESY, &st_faceindex, &st_statusbaron, st_faces,
  &st_states[SS_FACES] };

// keycard widgets
static const st_multicon_t	w_keyboxes[3] =
{
    { ST_KEY0X, ST_KEY0Y, &keyboxes[0], &st_statusbaron, st_keys, &st_states[SS_KEYS] },
    { ST_KEY1X, ST_KEY1Y, &keyboxes[1], &st_statusbaron, st_keys, &st_states[SS_KEYS + 1] },
    { ST_KEY2X, ST_KEY2Y, &keyboxes[2], &st_statusbaron, st_keys, &st_states[SS_KEYS + 2] }
};

// armor widget
static const st_percent_t	w_armor =
{ { ST_ARMORX, ST_ARMORY, 3, &PL.armorpoints, &st_statusbaron, st_tallnum,
    &st_states[SS_ARMOR] }, DMC_UI_STTPRCNT };

// ammo widgets
static const st_number_t	w_ammo[4] =
{
    { ST_AMMO0X, ST_AMMO0Y, ST_AMMO0WIDTH, &PL.ammo[0], &st_statusbaron, st_shortnum,
      &st_states[SS_AMMO] },
    { ST_AMMO1X, ST_AMMO1Y, ST_AMMO1WIDTH, &PL.ammo[1], &st_statusbaron, st_shortnum,
      &st_states[SS_AMMO + 1] },
    { ST_AMMO2X, ST_AMMO2Y, ST_AMMO2WIDTH, &PL.ammo[2], &st_statusbaron, st_shortnum,
      &st_states[SS_AMMO + 2] },
    { ST_AMMO3X, ST_AMMO3Y, ST_AMMO3WIDTH, &PL.ammo[3], &st_statusbaron, st_shortnum,
      &st_states[SS_AMMO + 3] }
};

// max ammo widgets
static const st_number_t	w_maxammo[4] =
{
    { ST_MAXAMMO0X, ST_MAXAMMO0Y, ST_MAXAMMO0WIDTH, &PL.maxammo[0], &st_statusbaron,
      st_shortnum, &st_states[SS_MAXAMMO] },
    { ST_MAXAMMO1X, ST_MAXAMMO1Y, ST_MAXAMMO1WIDTH, &PL.maxammo[1], &st_statusbaron,
      st_shortnum, &st_states[SS_MAXAMMO + 1] },
    { ST_MAXAMMO2X, ST_MAXAMMO2Y, ST_MAXAMMO2WIDTH, &PL.maxammo[2], &st_statusbaron,
      st_shortnum, &st_states[SS_MAXAMMO + 2] },
    { ST_MAXAMMO3X, ST_MAXAMMO3Y, ST_MAXAMMO3WIDTH, &PL.maxammo[3], &st_statusbaron,
      st_shortnum, &st_states[SS_MAXAMMO + 3] }
};

//
// STATUS BAR CODE
//
void ST_Stop(void);

void ST_refreshBackground(void)
{

    // The band compositor draws the background (ST_DrawBand), this
    // only marks the whole bar for it.
    if (st_statusbaron)
	V_CopyRect(ST_X, 0, st_backing_screen, ST_WIDTH, ST_HEIGHT, ST_X, ST_Y);

}


// Respond to keyboard input events.
// Cheats are not part of this port.
boolean
ST_Responder (event_t* ev)
{
  // Filter automap on/off.
  if (ev->type == ev_keyup
      && ((ev->data1 & 0xffff0000) == AM_MSGHEADER))
  {
    switch(ev->data1)
    {
      case AM_MSGENTERED:
	st_gamestate = AutomapState;
	st_firsttime = true;
	break;
	
      case AM_MSGEXITED:
	//	fprintf(stderr, "AM exited\n");
	st_gamestate = FirstPersonState;
	break;
    }
  }

  return false;
}



int ST_calcPainOffset(void)
{
    int		health;
    static int	lastcalc;
    static int	oldhealth = -1;
    
    health = plyr->health > 100 ? 100 : plyr->health;

    if (health != oldhealth)
    {
	lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
	oldhealth = health;
    }
    return lastcalc;
}


//
// This is a not-very-pretty routine which handles
//  the face states and their timing.
// the precedence of expressions is:
//  dead > evil grin > turned head > straight ahead
//
void ST_updateFaceWidget(void)
{
    int		i;
    angle_t	badguyangle;
    angle_t	diffang;
    static int	lastattackdown = -1;
    static int	priority = 0;
    boolean	doevilgrin;

    if (priority < 10)
    {
	// dead
	if (!plyr->health)
	{
	    priority = 9;
	    st_faceindex = ST_DEADFACE;
	    st_facecount = 1;
	}
    }

    if (priority < 9)
    {
	if (plyr->bonuscount)
	{
	    // picking up bonus
	    doevilgrin = false;

	    for (i=0;i<NUMWEAPONS;i++)
	    {
		if (oldweaponsowned[i] != plyr->weaponowned[i])
		{
		    doevilgrin = true;
		    oldweaponsowned[i] = plyr->weaponowned[i];
		}
	    }
	    if (doevilgrin) 
	    {
		// evil grin if just picked up weapon
		priority = 8;
		st_facecount = ST_EVILGRINCOUNT;
		st_faceindex = ST_calcPainOffset() + ST_EVILGRINOFFSET;
	    }
	}

    }
  
    if (priority < 8)
    {
	if (plyr->damagecount
	    && plyr->attacker
	    && plyr->attacker != plyr->mo)
	{
	    // being attacked
	    priority = 7;
	    
	    if (plyr->health - st_oldhealth > ST_MUCHPAIN)
	    {
		st_facecount = ST_TURNCOUNT;
		st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
	    }
	    else
	    {
		badguyangle = R_PointToAngle2(plyr->mo->x,
					      plyr->mo->y,
					      plyr->attacker->x,
					      plyr->attacker->y);
		
		if (badguyangle > plyr->mo->angle)
		{
		    // whether right or left
		    diffang = badguyangle - plyr->mo->angle;
		    i = diffang > ANG180; 
		}
		else
		{
		    // whether left or right
		    diffang = plyr->mo->angle - badguyangle;
		    i = diffang <= ANG180; 
		} // confusing, aint it?

		
		st_facecount = ST_TURNCOUNT;
		st_faceindex = ST_calcPainOffset();
		
		if (diffang < ANG45)
		{
		    // head-on    
		    st_faceindex += ST_RAMPAGEOFFSET;
		}
		else if (i)
		{
		    // turn face right
		    st_faceindex += ST_TURNOFFSET;
		}
		else
		{
		    // turn face left
		    st_faceindex += ST_TURNOFFSET+1;
		}
	    }
	}
    }
  
    if (priority < 7)
    {
	// getting hurt because of your own damn stupidity
	if (plyr->damagecount)
	{
	    if (plyr->health - st_oldhealth > ST_MUCHPAIN)
	    {
		priority = 7;
		st_facecount = ST_TURNCOUNT;
		st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
	    }
	    else
	    {
		priority = 6;
		st_facecount = ST_TURNCOUNT;
		st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
	    }

	}

    }
  
    if (priority < 6)
    {
	// rapid firing
	if (plyr->attackdown)
	{
	    if (lastattackdown==-1)
		lastattackdown = ST_RAMPAGEDELAY;
	    else if (!--lastattackdown)
	    {
		priority = 5;
		st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
		st_facecount = 1;
		lastattackdown = 1;
	    }
	}
	else
	    lastattackdown = -1;

    }
  
    if (priority < 5)
    {
	// invulnerability
	if ((plyr->cheats & CF_GODMODE)
	    || plyr->powers[pw_invulnerability])
	{
	    priority = 4;

	    st_faceindex = ST_GODFACE;
	    st_facecount = 1;

	}

    }

    // look left or look right if the facecount has timed out
    if (!st_facecount)
    {
	st_faceindex = ST_calcPainOffset() + (st_randomnumber % 3);
	st_facecount = ST_STRAIGHTFACECOUNT;
	priority = 0;
    }

    st_facecount--;

}

void ST_updateWidgets(void)
{
    static const int	largeammo = 1994; // means "n/a"
    int		i;

    // the ammo of the ready weapon, which may have changed
    //  if (w_ready.data != plyr->readyweapon)
    //  {
    if (weaponinfo[plyr->readyweapon].ammo == am_noammo)
	st_readyammo = largeammo;
    else
	st_readyammo = plyr->ammo[weaponinfo[plyr->readyweapon].ammo];
    //{
    // static int tic=0;
    // static int dir=-1;
    // if (!(tic&15))
    //   plyr->ammo[weaponinfo[plyr->readyweapon].ammo]+=dir;
    // if (plyr->ammo[weaponinfo[plyr->readyweapon].ammo] == -100)
    //   dir = 1;
    // tic++;
    // }

    // if (*w_ready.on)
    //  STlib_updateNum(&w_ready, true);
    // refresh weapon change
    //  }

    // update keycard multiple widgets
    for (i=0;i<3;i++)
    {
	keyboxes[i] = plyr->cards[i] ? i : -1;

	if (plyr->cards[i+3])
	    keyboxes[i] = i+3;
    }

    // refresh everything if this is him coming back to life
    ST_updateFaceWidget();

    // used by the w_armsbg widget
    st_notdeathmatch = !deathmatch;
    
    // used by w_arms[] widgets
    st_armson = st_statusbaron && !deathmatch; 

    // used by w_frags widget
    st_fragson = deathmatch && st_statusbaron; 
    st_fragscount = 0;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (i != consoleplayer)
	    st_fragscount += plyr->frags[i];
	else
	    st_fragscount -= plyr->frags[i];
    }

    // get rid of chat window if up because of message
    if (!--st_msgcounter)
	st_chat = st_oldchat;

}

void ST_Ticker (void)
{

    st_clock++;
    st_randomnumber = M_Random();
    ST_updateWidgets();
    st_oldhealth = plyr->health;

}

static int st_palette = 0;

void ST_doPaletteStuff(void)
{

    int		palette;
    
    int		cnt;
    int		bzc;

    cnt = plyr->damagecount;

    if (plyr->powers[pw_strength])
    {
	// slowly fade the berzerk out
  	bzc = 12 - (plyr->powers[pw_strength]>>6);

	if (bzc > cnt)
	    cnt = bzc;
    }
	
    if (cnt)
    {
	palette = (cnt+7)>>3;
	
	if (palette >= NUMREDPALS)
	    palette = NUMREDPALS-1;

	palette += STARTREDPALS;
    }

    else if (plyr->bonuscount)
    {
	palette = (plyr->bonuscount+7)>>3;

	if (palette >= NUMBONUSPALS)
	    palette = NUMBONUSPALS-1;

	palette += STARTBONUSPALS;
    }

    else if ( plyr->powers[pw_ironfeet] > 4*32
	      || plyr->powers[pw_ironfeet]&8)
	palette = RADIATIONPAL;
    else
	palette = 0;

    if (palette != st_palette)
    {
	st_palette = palette;
	I_SetPalette (palette);
    }

}

void ST_drawWidgets(boolean refresh)
{
    int		i;

    // used by w_arms[] widgets
    st_armson = st_statusbaron && !deathmatch;

    // used by w_frags widget
    st_fragson = deathmatch && st_statusbaron; 

    STlib_updateNum(&w_ready, refresh);

    for (i=0;i<4;i++)
    {
	STlib_updateNum(&w_ammo[i], refresh);
	STlib_updateNum(&w_maxammo[i], refresh);
    }

    STlib_updatePercent(&w_health, refresh);
    STlib_updatePercent(&w_armor, refresh);

    STlib_updateBinIcon(&w_armsbg, refresh);

    for (i=0;i<6;i++)
	STlib_updateMultIcon(&w_arms[i], refresh);

    STlib_updateMultIcon(&w_faces, refresh);

    for (i=0;i<3;i++)
	STlib_updateMultIcon(&w_keyboxes[i], refresh);

    STlib_updateNum(&w_frags, refresh);

}

void ST_doRefresh(void)
{

    st_firsttime = false;

    // draw status bar background to off-screen buff
    ST_refreshBackground();

    // and refresh all widgets
    ST_drawWidgets(true);

}

void ST_diffDraw(void)
{
    // update all widgets
    ST_drawWidgets(false);
}

//
// Status band compositor (port/render/r_band.c): the background inside
// the current clip rectangle, and the whole bar in its current state.
//
void ST_BandBackground (void)
{
    V_DrawPatch(ST_X, ST_Y, V_UIPatch(DMC_UI_STBAR));

    // face backgrounds are for multiplayer only
}

void ST_DrawBand (void)
{
    ST_BandBackground();
    ST_drawWidgets(true);
}

void ST_Drawer (boolean fullscreen, boolean refresh)
{
  
    st_statusbaron = (!fullscreen) || automapactive;
    st_firsttime = st_firsttime || refresh;

    // Do red-/gold-shifts from damage/items
    ST_doPaletteStuff();

    // If just after ST_Start(), refresh all
    if (st_firsttime) ST_doRefresh();
    // Otherwise, update as little as possible
    else ST_diffDraw();

}

// Nothing to load: the widgets look their pictures up (UI ids above).
void ST_loadGraphics(void)
{
}

void ST_loadData(void)
{
    ST_loadGraphics();
}

void ST_unloadGraphics(void)
{
}

void ST_unloadData(void)
{
    ST_unloadGraphics();
}

void ST_initData(void)
{

    int		i;

    st_firsttime = true;
    plyr = &players[consoleplayer];

    st_clock = 0;
    st_chatstate = StartChatState;
    st_gamestate = FirstPersonState;

    st_statusbaron = true;
    st_oldchat = st_chat = false;
    st_cursoron = false;

    st_faceindex = 0;
    st_palette = -1;

    st_oldhealth = -1;

    for (i=0;i<NUMWEAPONS;i++)
	oldweaponsowned[i] = plyr->weaponowned[i];

    for (i=0;i<3;i++)
	keyboxes[i] = -1;

}



// The widgets are constant (above): this sets them to what they show
// before their first draw.
void ST_createWidgets(void)
{

    int i;

    // ready weapon ammo
    st_readyammo = weaponinfo[plyr->readyweapon].ammo == am_noammo ? 1994
	: plyr->ammo[weaponinfo[plyr->readyweapon].ammo];
    STlib_initNum(&w_ready);

    // health percentage
    STlib_initNum(&w_health.n);

    // arms background
    STlib_initBinIcon(&w_armsbg);

    // weapons owned
    for(i=0;i<6;i++)
	STlib_initMultIcon(&w_arms[i]);

    // frags sum
    STlib_initNum(&w_frags);

    // faces
    STlib_initMultIcon(&w_faces);

    // armor percentage - should be colored later
    STlib_initNum(&w_armor.n);

    // keyboxes 0-2
    for (i=0;i<3;i++)
	STlib_initMultIcon(&w_keyboxes[i]);

    // ammo count and max ammo count (all four kinds)
    for (i=0;i<4;i++)
    {
	STlib_initNum(&w_ammo[i]);
	STlib_initNum(&w_maxammo[i]);
    }

}

// global: the firmware's overlays reach it through the resident image
// (mk/fw.mk)
boolean		st_stopped = true;


void ST_Start (void)
{

    if (!st_stopped)
	ST_Stop();

    ST_initData();
    ST_createWidgets();
    st_stopped = false;

}

void ST_Stop (void)
{
    if (st_stopped)
	return;

    I_SetPalette (0);

    st_stopped = true;
}

void ST_Init (void)
{
    ST_loadData();
    // no backing screen, the band compositor redraws the background
    st_backing_screen = NULL;
}

