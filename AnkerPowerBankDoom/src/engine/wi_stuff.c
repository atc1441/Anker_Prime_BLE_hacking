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
//	Intermission screens.
//


#include "m_misc.h"
#include "m_random.h"

#include "deh_main.h"
#include "i_swap.h"
#include "i_system.h"

#include "dmcdat.h"

#include "g_game.h"

#include "r_local.h"
#include "s_sound.h"

#include "doomstat.h"

// Data.
#include "sounds.h"

// Needs access to LFB.
#include "v_video.h"

#include "wi_stuff.h"

#include "fe.h"
#include "r_screen.h"

//
// Data needed to add patches to full screen intermission pics.
// Patches are statistics messages, and animations.
// Loads of by-pixel layout and placement, offsets etc.
//


//
// Different vetween registered DOOM (1994) and
//  Ultimate DOOM - Final edition (retail, 1995?).
// This is supposedly ignored for commercial
//  release (aka DOOM II), which had 34 maps
//  in one episode. So there.
#define NUMEPISODES	4
#define NUMMAPS		9


// in tics
//U #define PAUSELEN		(TICRATE*2) 
//U #define SCORESTEP		100
//U #define ANIMPERIOD		32
// pixel distance from "(YOU)" to "PLAYER N"
//U #define STARDIST		10 
//U #define WK 1


// GLOBAL LOCATIONS
#define WI_TITLEY		2
#define WI_SPACINGY    		33

// SINGPLE-PLAYER STUFF
#define SP_STATSX		50
#define SP_STATSY		50

#define SP_TIMEX		16
#define SP_TIMEY		(SCREENHEIGHT-32)


// Single player only: the net game and deathmatch screens are left out.


typedef enum
{
    ANIM_ALWAYS,
    ANIM_RANDOM,
    ANIM_LEVEL

} animenum_t;

typedef struct
{
    short	x;
    short	y;
    
} point_t;


//
// Animation.
// The description is constant (animdef_t), the state of the
// animations of the episode is in the intermission state below.
// The pack has no animation pictures (the Freedoom WIA lumps are
// empty), so the animations keep only their timing and random numbers.
//
typedef struct
{
    byte	type;	// animenum_t

    // period in tics between animations
    byte	period;

    // number of animation frames
    byte	nanims;

    // location of animation
    byte	x;
    byte	y;

    // ALWAYS: n/a,
    // RANDOM: period deviation (<256),
    // LEVEL: level
    byte	data1;

    // ALWAYS: n/a,
    // RANDOM: random base period,
    // LEVEL: n/a
    byte	data2; 

} animdef_t;


static const point_t lnodes[NUMEPISODES][NUMMAPS] =
{
    // Episode 0 World Map
    {
	{ 185, 164 },	// location of level 0 (CJ)
	{ 148, 143 },	// location of level 1 (CJ)
	{ 69, 122 },	// location of level 2 (CJ)
	{ 209, 102 },	// location of level 3 (CJ)
	{ 116, 89 },	// location of level 4 (CJ)
	{ 166, 55 },	// location of level 5 (CJ)
	{ 71, 56 },	// location of level 6 (CJ)
	{ 135, 29 },	// location of level 7 (CJ)
	{ 71, 24 }	// location of level 8 (CJ)
    },

    // Episode 1 World Map should go here
    {
	{ 254, 25 },	// location of level 0 (CJ)
	{ 97, 50 },	// location of level 1 (CJ)
	{ 188, 64 },	// location of level 2 (CJ)
	{ 128, 78 },	// location of level 3 (CJ)
	{ 214, 92 },	// location of level 4 (CJ)
	{ 133, 130 },	// location of level 5 (CJ)
	{ 208, 136 },	// location of level 6 (CJ)
	{ 148, 140 },	// location of level 7 (CJ)
	{ 235, 158 }	// location of level 8 (CJ)
    },

    // Episode 2 World Map should go here
    {
	{ 156, 168 },	// location of level 0 (CJ)
	{ 48, 154 },	// location of level 1 (CJ)
	{ 174, 95 },	// location of level 2 (CJ)
	{ 265, 75 },	// location of level 3 (CJ)
	{ 130, 48 },	// location of level 4 (CJ)
	{ 279, 23 },	// location of level 5 (CJ)
	{ 198, 48 },	// location of level 6 (CJ)
	{ 140, 25 },	// location of level 7 (CJ)
	{ 281, 136 }	// location of level 8 (CJ)
    }

};


//
// Animation locations for episode 0 (1).
// Using patches saves a lot of space,
//  as they replace 320x200 full screen frames.
//

#define ANIM(type, period, nanims, x, y, nexttic)            \
   { (type), (period), (nanims), (x), (y), (nexttic), 0 }


static const animdef_t epsd0animinfo[] =
{
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 224, 104, 0),
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 184, 160, 0),
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 112, 136, 0),
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 72, 112, 0),
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 88, 96, 0),
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 64, 48, 0),
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 192, 40, 0),
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 136, 16, 0),
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 80, 16, 0),
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 64, 24, 0),
};

static const animdef_t epsd1animinfo[] =
{
    ANIM(ANIM_LEVEL, TICRATE/3, 1, 128, 136, 1),
    ANIM(ANIM_LEVEL, TICRATE/3, 1, 128, 136, 2),
    ANIM(ANIM_LEVEL, TICRATE/3, 1, 128, 136, 3),
    ANIM(ANIM_LEVEL, TICRATE/3, 1, 128, 136, 4),
    ANIM(ANIM_LEVEL, TICRATE/3, 1, 128, 136, 5),
    ANIM(ANIM_LEVEL, TICRATE/3, 1, 128, 136, 6),
    ANIM(ANIM_LEVEL, TICRATE/3, 1, 128, 136, 7),
    ANIM(ANIM_LEVEL, TICRATE/3, 3, 192, 144, 8),
    ANIM(ANIM_LEVEL, TICRATE/3, 1, 128, 136, 8),
};

static const animdef_t epsd2animinfo[] =
{
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 104, 168, 0),
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 40, 136, 0),
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 160, 96, 0),
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 104, 80, 0),
    ANIM(ANIM_ALWAYS, TICRATE/3, 3, 120, 32, 0),
    ANIM(ANIM_ALWAYS, TICRATE/4, 3, 40, 0, 0),
};

static const byte NUMANIMS[NUMEPISODES] =
{
    arrlen(epsd0animinfo),
    arrlen(epsd1animinfo),
    arrlen(epsd2animinfo),
};

static const animdef_t *const animdefs[NUMEPISODES] =
{
    epsd0animinfo,
    epsd1animinfo,
    epsd2animinfo
};

_Static_assert(arrlen(epsd0animinfo) == arrlen(fe_scr.u.wi.ctr),
	       "the state holds the animations of episode 0, which has the most");


//
// GENERAL DATA
//

//
// Locally used stuff.
//

// States for single-player
#define SP_KILLS		0
#define SP_ITEMS		2
#define SP_SECRET		4
#define SP_FRAGS		6 
#define SP_TIME			8 
#define SP_PAR			ST_TIME

#define SP_PAUSE		1

// in seconds
#define SHOWNEXTLOCDELAY	4
//#define SHOWLASTLOCDELAY	SHOWNEXTLOCDELAY


// The state of the intermission is in the front-end scratch (port/game/fe.h),
// its parameters in wminfo (g_game.c).
#define W			(fe_scr.u.wi)

// used to accelerate or skip a stage
#define acceleratestage	(W.acceleratestage)

// wbs->pnum
#define me			(wminfo.pnum)

 // specifies current state
#define state			(W.state)

// contains information passed into intermission
#define wbs			(&wminfo)

#define plrs			(wminfo.plyr)

// used for general timing
#define cnt			(W.cnt)

// used for timing of background animation
#define bcnt			(W.bcnt)

#define cnt_kills		(W.cnt_kills)
#define cnt_items		(W.cnt_items)
#define cnt_secret		(W.cnt_secret)
#define cnt_time		(W.cnt_time)
#define cnt_par			(W.cnt_par)
#define cnt_pause		(W.cnt_pause)
#define sp_state		(W.sp_state)

// vanilla keeps the pointer from one intermission to the next
#define snl_pointeron		(fe.flags & FE_SNL)

static void WI_setPointer(int on)
{
    fe.flags = on ? fe.flags | FE_SNL : fe.flags & ~FE_SNL;
}


//
//	GRAPHICS
// The pictures are UIDIR entries of the image, looked up where they
// are drawn. A picture the pack does not have is -1 and draws nothing.
//

// You Are Here graphic
static const short yah[3] = { DMC_UI_WIURH0, DMC_UI_WIURH1, -1 };

// splat
static const short splat[2] = { DMC_UI_WISPLAT, -1 };

 // Name graphics of each level (centered)
static int WI_levelName(int n)
{
    return wbs->epsd ? -1 : V_UIPatch(DMC_UI_WILV00 + n);
}

//
// CODE
//

// slam background
void WI_slamBackground(void)
{
    V_DrawPatch(0, 0, wbs->epsd ? -1 : V_UIPatch(DMC_UI_WIMAP0));
}

// The ticker is used to detect keys
//  because of timing issues in netgames.
boolean WI_Responder(event_t* ev)
{
    return false;
}


// Draws "<Levelname> Finished!"
void WI_drawLF(void)
{
    int y = WI_TITLEY;
    int lname = WI_levelName(wbs->last);
    int finished = V_UIPatch(DMC_UI_WIF);

    // draw <LevelName> 
    V_DrawPatch((SCREENWIDTH - V_PatchWidth(lname))/2, y, lname);

    // draw "Finished!"
    y += (5*V_PatchHeight(lname))/4;

    V_DrawPatch((SCREENWIDTH - V_PatchWidth(finished)) / 2, y, finished);
}



// Draws "Entering <LevelName>"
void WI_drawEL(void)
{
    int y = WI_TITLEY;
    int entering = V_UIPatch(DMC_UI_WIENTER);
    int lname = WI_levelName(wbs->next);

    // draw "Entering"
    V_DrawPatch((SCREENWIDTH - V_PatchWidth(entering))/2,
		y,
                entering);

    // draw level
    y += (5*V_PatchHeight(lname))/4;

    V_DrawPatch((SCREENWIDTH - V_PatchWidth(lname))/2,
		y, 
                lname);

}

// ui: the pictures to try in turn, -1 ends the list
void
WI_drawOnLnode
( int		n,
  const short	ui[] )
{

    int		i;
    int		c;
    int		left;
    int		top;
    int		right;
    int		bottom;
    boolean	fits = false;

    i = 0;
    do
    {
	c = V_UIPatch(ui[i]);
	left = lnodes[wbs->epsd][n].x - V_PatchLeftOffset(c);
	top = lnodes[wbs->epsd][n].y - V_PatchTopOffset(c);
	right = left + V_PatchWidth(c);
	bottom = top + V_PatchHeight(c);

	if (left >= 0
	    && right < SCREENWIDTH
	    && top >= 0
	    && bottom < SCREENHEIGHT)
	{
	    fits = true;
	}
	else
	{
	    i++;
	}
    } while (!fits && i!=2 && V_UIPatch(ui[i]) >= 0);

    // the E1 positions always fit
    if (fits && i<2)
    {
	V_DrawPatch(lnodes[wbs->epsd][n].x,
                    lnodes[wbs->epsd][n].y,
		    c);
    }
}



void WI_initAnimatedBack(void)
{
    int		i;
    const animdef_t*	d;

    if (gamemode == commercial)
	return;

    if (wbs->epsd > 2)
	return;

    for (i=0;i<NUMANIMS[wbs->epsd];i++)
    {
	d = &animdefs[wbs->epsd][i];

	// init variables
	W.ctr[i] = -1;

	// specify the next time to draw it
	if (d->type == ANIM_ALWAYS)
	    W.nexttic[i] = bcnt + 1 + (M_Random()%d->period);
	else if (d->type == ANIM_RANDOM)
	    W.nexttic[i] = bcnt + 1 + d->data2+(M_Random()%d->data1);
	else if (d->type == ANIM_LEVEL)
	    W.nexttic[i] = bcnt + 1;
    }

}

void WI_updateAnimatedBack(void)
{
    int		i;
    const animdef_t*	d;

    if (gamemode == commercial)
	return;

    if (wbs->epsd > 2)
	return;

    for (i=0;i<NUMANIMS[wbs->epsd];i++)
    {
	d = &animdefs[wbs->epsd][i];

	if (bcnt == W.nexttic[i])
	{
	    switch (d->type)
	    {
	      case ANIM_ALWAYS:
		if (++W.ctr[i] >= d->nanims) W.ctr[i] = 0;
		W.nexttic[i] = bcnt + d->period;
		break;

	      case ANIM_RANDOM:
		W.ctr[i]++;
		if (W.ctr[i] == d->nanims)
		{
		    W.ctr[i] = -1;
		    W.nexttic[i] = bcnt+d->data2+(M_Random()%d->data1);
		}
		else W.nexttic[i] = bcnt + d->period;
		break;
		
	      case ANIM_LEVEL:
		// gawd-awful hack for level anims
		if (!(state == StatCount && i == 7)
		    && wbs->next == d->data1)
		{
		    W.ctr[i]++;
		    if (W.ctr[i] == d->nanims) W.ctr[i]--;
		    W.nexttic[i] = bcnt + d->period;
		}
		break;
	    }
	}

    }

}

//
// Draws a number.
// If digits > 0, then use that many digits minimum,
//  otherwise only use as many as necessary.
// Returns new x position.
//

int
WI_drawNum
( int		x,
  int		y,
  int		n,
  int		digits )
{

    int		fontwidth = V_PatchWidth(V_UIPatch(DMC_UI_WINUM0));
    int		neg;
    int		temp;

    if (digits < 0)
    {
	if (!n)
	{
	    // make variable-length zeros 1 digit long
	    digits = 1;
	}
	else
	{
	    // figure out # of digits in #
	    digits = 0;
	    temp = n;

	    while (temp)
	    {
		temp /= 10;
		digits++;
	    }
	}
    }

    neg = n < 0;
    if (neg)
	n = -n;

    // if non-number, do not draw it
    if (n == 1994)
	return 0;

    // draw the new number
    while (digits--)
    {
	x -= fontwidth;
	V_DrawPatch(x, y, V_UIPatch(DMC_UI_WINUM0 + n % 10));
	n /= 10;
    }

    // draw a minus sign if necessary
    if (neg)
	V_DrawPatch(x-=8, y, V_UIPatch(DMC_UI_WIMINUS));

    return x;

}

void
WI_drawPercent
( int		x,
  int		y,
  int		p )
{
    if (p < 0)
	return;

    V_DrawPatch(x, y, V_UIPatch(DMC_UI_WIPCNT));
    WI_drawNum(x, y, p, -1);
}



//
// Display level completion time and par,
//  or "sucks" message if overflow.
//
void
WI_drawTime
( int		x,
  int		y,
  int		t )
{

    int		div;
    int		n;
    int		colon = V_UIPatch(DMC_UI_WICOLON);
    int		sucks;

    if (t<0)
	return;

    if (t <= 61*59)
    {
	div = 1;

	do
	{
	    n = (t / div) % 60;
	    x = WI_drawNum(x, y, n, 2) - V_PatchWidth(colon);
	    div *= 60;

	    // draw
	    if (div==60 || t / div)
		V_DrawPatch(x, y, colon);
	    
	} while (t / div);
    }
    else
    {
	// "sucks"
	sucks = V_UIPatch(DMC_UI_WISUCKS);
	V_DrawPatch(x - V_PatchWidth(sucks), y, sucks); 
    }
}


void WI_initNoState(void)
{
    state = NoState;
    acceleratestage = 0;
    cnt = 10;
}

void WI_updateNoState(void) {

    WI_updateAnimatedBack();

    if (!--cnt)
    {
        // Don't call WI_End yet.  G_WorldDone doesnt immediately 
        // change gamestate, so WI_Drawer is still going to get
        // run until that happens.  If we do that after WI_End
        // (which unloads all the graphics), we're in trouble.
	//WI_End();
	G_WorldDone();
    }

}

void WI_initShowNextLoc(void)
{
    state = ShowNextLoc;
    acceleratestage = 0;
    cnt = SHOWNEXTLOCDELAY * TICRATE;

    WI_initAnimatedBack();
}

void WI_updateShowNextLoc(void)
{
    WI_updateAnimatedBack();

    if (!--cnt || acceleratestage)
	WI_initNoState();
    else
	WI_setPointer((cnt & 31) < 20);
}

void WI_drawShowNextLoc(void)
{

    int		i;
    int		last;

    WI_slamBackground();

    if ( gamemode != commercial)
    {
  	if (wbs->epsd > 2)
	{
	    WI_drawEL();
	    return;
	}
	
	last = (wbs->last == 8) ? wbs->next - 1 : wbs->last;

	// draw a splat on taken cities.
	for (i=0 ; i<=last ; i++)
	    WI_drawOnLnode(i, splat);

	// splat the secret level?
	if (wbs->didsecret)
	    WI_drawOnLnode(8, splat);

	// draw flashing ptr
	if (snl_pointeron)
	    WI_drawOnLnode(wbs->next, yah); 
    }

    // draws which level you are entering..
    if ( (gamemode != commercial)
	 || wbs->next != 30)
	WI_drawEL();  

}

void WI_drawNoState(void)
{
    WI_setPointer(true);
    WI_drawShowNextLoc();
}

void WI_initStats(void)
{
    state = StatCount;
    acceleratestage = 0;
    sp_state = 1;
    cnt_kills = cnt_items = cnt_secret = -1;
    cnt_time = cnt_par = -1;
    cnt_pause = TICRATE;

    WI_initAnimatedBack();
}

void WI_updateStats(void)
{

    WI_updateAnimatedBack();

    if (acceleratestage && sp_state != 10)
    {
	acceleratestage = 0;
	cnt_kills = (plrs[me].skills * 100) / wbs->maxkills;
	cnt_items = (plrs[me].sitems * 100) / wbs->maxitems;
	cnt_secret = (plrs[me].ssecret * 100) / wbs->maxsecret;
	cnt_time = plrs[me].stime / TICRATE;
	cnt_par = wbs->partime / TICRATE;
	S_StartSound(0, sfx_barexp);
	sp_state = 10;
    }

    if (sp_state == 2)
    {
	cnt_kills += 2;

	if (!(bcnt&3))
	    S_StartSound(0, sfx_pistol);

	if (cnt_kills >= (plrs[me].skills * 100) / wbs->maxkills)
	{
	    cnt_kills = (plrs[me].skills * 100) / wbs->maxkills;
	    S_StartSound(0, sfx_barexp);
	    sp_state++;
	}
    }
    else if (sp_state == 4)
    {
	cnt_items += 2;

	if (!(bcnt&3))
	    S_StartSound(0, sfx_pistol);

	if (cnt_items >= (plrs[me].sitems * 100) / wbs->maxitems)
	{
	    cnt_items = (plrs[me].sitems * 100) / wbs->maxitems;
	    S_StartSound(0, sfx_barexp);
	    sp_state++;
	}
    }
    else if (sp_state == 6)
    {
	cnt_secret += 2;

	if (!(bcnt&3))
	    S_StartSound(0, sfx_pistol);

	if (cnt_secret >= (plrs[me].ssecret * 100) / wbs->maxsecret)
	{
	    cnt_secret = (plrs[me].ssecret * 100) / wbs->maxsecret;
	    S_StartSound(0, sfx_barexp);
	    sp_state++;
	}
    }

    else if (sp_state == 8)
    {
	if (!(bcnt&3))
	    S_StartSound(0, sfx_pistol);

	cnt_time += 3;

	if (cnt_time >= plrs[me].stime / TICRATE)
	    cnt_time = plrs[me].stime / TICRATE;

	cnt_par += 3;

	if (cnt_par >= wbs->partime / TICRATE)
	{
	    cnt_par = wbs->partime / TICRATE;

	    if (cnt_time >= plrs[me].stime / TICRATE)
	    {
		S_StartSound(0, sfx_barexp);
		sp_state++;
	    }
	}
    }
    else if (sp_state == 10)
    {
	if (acceleratestage)
	{
	    S_StartSound(0, sfx_sgcock);

	    if (gamemode == commercial)
		WI_initNoState();
	    else
		WI_initShowNextLoc();
	}
    }
    else if (sp_state & 1)
    {
	if (!--cnt_pause)
	{
	    sp_state++;
	    cnt_pause = TICRATE;
	}
    }

}

void WI_drawStats(void)
{
    // line height
    int lh;	

    lh = (3*V_PatchHeight(V_UIPatch(DMC_UI_WINUM0)))/2;

    WI_slamBackground();

    WI_drawLF();

    V_DrawPatch(SP_STATSX, SP_STATSY, V_UIPatch(DMC_UI_WIOSTK));
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY, cnt_kills);

    V_DrawPatch(SP_STATSX, SP_STATSY+lh, V_UIPatch(DMC_UI_WIOSTI));
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY+lh, cnt_items);

    V_DrawPatch(SP_STATSX, SP_STATSY+2*lh, V_UIPatch(DMC_UI_WISCRT2));
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY+2*lh, cnt_secret);

    V_DrawPatch(SP_TIMEX, SP_TIMEY, V_UIPatch(DMC_UI_WITIME));
    WI_drawTime(SCREENWIDTH/2 - SP_TIMEX, SP_TIMEY, cnt_time);

    if (wbs->epsd < 3)
    {
	V_DrawPatch(SCREENWIDTH/2 + SP_TIMEX, SP_TIMEY, V_UIPatch(DMC_UI_WIPAR));
	WI_drawTime(SCREENWIDTH - SP_TIMEX, SP_TIMEY, cnt_par);
    }

}

void WI_checkForAccelerate(void)
{
    int   i;
    player_t  *player;

    // check for button presses to skip delays
    for (i=0, player = players ; i<MAXPLAYERS ; i++, player++)
    {
	if (playeringame[i])
	{
	    if (player->cmd.buttons & BT_ATTACK)
	    {
		if (!player->attackdown)
		    acceleratestage = 1;
		player->attackdown = true;
	    }
	    else
		player->attackdown = false;
	    if (player->cmd.buttons & BT_USE)
	    {
		if (!player->usedown)
		    acceleratestage = 1;
		player->usedown = true;
	    }
	    else
		player->usedown = false;
	}
    }
}



// Updates stuff each tick
void WI_OvlTicker(void)
{
    // counter for general background animation
    bcnt++;  

    WI_checkForAccelerate();

    switch (state)
    {
      case StatCount:
	WI_updateStats();
	break;
	
      case ShowNextLoc:
	WI_updateShowNextLoc();
	break;
	
      case NoState:
	WI_updateNoState();
	break;
    }

}

static void WI_drawState (void)
{
    switch (state)
    {
      case StatCount:
	WI_drawStats();
	break;
	
      case ShowNextLoc:
	WI_drawShowNextLoc();
	break;
	
      case NoState:
	WI_drawNoState();
	break;
    }
}

// The screen is composed in pieces (port/render/r_screen.h), each state
// is a screen of its own.
void WI_OvlDrawer (void)
{
    R_ScreenDraw(FE_SCR_WI + state, &scr_full, WI_drawState, NULL);
}


void WI_initVariables(void)
{

#ifdef RANGECHECKING
    if (gamemode != commercial)
    {
      if ( gamemode == retail )
	RNGCHECK(wbs->epsd, 0, 3);
      else
	RNGCHECK(wbs->epsd, 0, 2);
    }
    else
    {
	RNGCHECK(wbs->last, 0, 8);
	RNGCHECK(wbs->next, 0, 8);
    }
    RNGCHECK(wbs->pnum, 0, MAXPLAYERS);
    RNGCHECK(wbs->pnum, 0, MAXPLAYERS);
#endif

    acceleratestage = 0;
    cnt = bcnt = 0;

    if (!wbs->maxkills)
	wbs->maxkills = 1;

    if (!wbs->maxitems)
	wbs->maxitems = 1;

    if (!wbs->maxsecret)
	wbs->maxsecret = 1;

    if ( gamemode != retail )
      if (wbs->epsd > 2)
	wbs->epsd -= 3;
}

// A new intermission of wminfo
void WI_OvlStart(void)
{
    WI_initVariables();
    WI_initStats();
}
