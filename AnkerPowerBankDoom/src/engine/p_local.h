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
//	Play functions, animation, global header.
//


#ifndef __P_LOCAL__
#define __P_LOCAL__

#ifndef __R_LOCAL__
#include "r_local.h"
#endif

#define FLOATSPEED		(FRACUNIT*4)


#define MAXHEALTH		100
#define VIEWHEIGHT		(41*FRACUNIT)

// mapblocks are used to check movement
// against lines and things
#define MAPBLOCKUNITS	128
#define MAPBLOCKSIZE	(MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT	(FRACBITS+7)
#define MAPBMASK		(MAPBLOCKSIZE-1)
#define MAPBTOFRAC		(MAPBLOCKSHIFT-FRACBITS)


// player radius for movement checking
#define PLAYERRADIUS	16*FRACUNIT

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS		32*FRACUNIT

#define GRAVITY		FRACUNIT
#define MAXMOVE		(30*FRACUNIT)

#define USERANGE		(64*FRACUNIT)
#define MELEERANGE		(64*FRACUNIT)
#define MISSILERANGE	(32*64*FRACUNIT)

// follow a player exlusively for 3 seconds
#define	BASETHRESHOLD	 	100



//
// P_TICK
//

// The thinker lists live in the object store (port/obj/things.c).


//
// P_PSPR
//
void P_SetupPsprites (player_t* curplayer);
void P_MovePsprites (player_t* curplayer);
void P_DropWeapon (player_t* player);


//
// P_USER
//
void	P_PlayerThink (player_t* player);


//
// P_MOBJ
//
#define ONFLOORZ		INT_MIN
#define ONCEILINGZ		INT_MAX

// Time interval for item respawning.




mobj_t*
P_SpawnMobj
( fixed_t	x,
  fixed_t	y,
  fixed_t	z,
  mobjtype_t	type );

mobj_t*
P_SpawnMapMobj
( fixed_t	x,
  fixed_t	y,
  fixed_t	z,
  mobjtype_t	type,
  secidx_t	sec );

void	P_SpawnPlayerThing (int i, const dmc_thing_t* t);
mobj_t*	P_NewMobj (fixed_t x, fixed_t y, mobjtype_t type, int transient);
void	P_SpawnZ (mobj_t* mobj, fixed_t z);
void 	P_RemoveMobj (mobj_t* th);
boolean	P_SetMobjState (mobj_t* mobj, statenum_t state);
void 	P_MobjThinker (mobj_t* mobj);
fixed_t	P_Radius (mobj_t* mo);
fixed_t	P_Height (mobj_t* mo);

void	P_SpawnPuff (fixed_t x, fixed_t y, fixed_t z);
void 	P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, int damage);
mobj_t* P_SpawnMissile (mobj_t* source, mobj_t* dest, mobjtype_t type);
void	P_SpawnPlayerMissile (mobj_t* source, mobjtype_t type);


//
// P_ENEMY
//
void P_NoiseAlert (mobj_t* target, mobj_t* emmiter);
// A_Look up to the decision to wake, and the wake-up itself.
boolean P_LookSee (mobj_t* actor);
void P_SeeYou (mobj_t* actor);
void A_Look (mobj_t* actor);


//
// P_MAPUTL
//
typedef struct
{
    fixed_t	x;
    fixed_t	y;
    fixed_t	dx;
    fixed_t	dy;
    
} divline_t;

typedef struct
{
    fixed_t	frac;		// along trace line
    unsigned char isaline;
    union {
	unsigned short	thing;	// handle
	lineidx_t	line;
    }			d;
} intercept_t;

// Extended MAXINTERCEPTS, to allow for intercepts overrun emulation.

#define MAXINTERCEPTS_ORIGINAL 128
#define MAXINTERCEPTS          (MAXINTERCEPTS_ORIGINAL + 61)

// The intercepts of a trace are kept in the tic scratch as a fraction
// and a reference each: a thing handle, or IC_LINE | line.
#define IC_LINE		0xc000
extern int		numintercepts;

typedef boolean (*traverser_t) (intercept_t *in);

fixed_t P_AproxDistance (fixed_t dx, fixed_t dy);
int 	P_PointOnLineSide (fixed_t x, fixed_t y, line_t* line);
int 	P_PointOnDivlineSide (fixed_t x, fixed_t y, divline_t* line);
void 	P_MakeDivline (line_t* li, divline_t* dl);
fixed_t P_InterceptVector (divline_t* v2, divline_t* v1);
int 	P_BoxOnLineSide (fixed_t* tmbox, line_t* ld);

// P_LineOpening sets opentop, openbottom, openrange and lowfloor (tic
// scratch).
void 	P_LineOpening (line_t* linedef);

boolean P_BlockLinesIterator (int x, int y, boolean(*func)(lineidx_t) );
boolean P_BlockThingsIterator (int x, int y, boolean(*func)(mobj_t*) );

#define PT_ADDLINES		1
#define PT_ADDTHINGS	2
#define PT_EARLYOUT		4

boolean
P_PathTraverse
( fixed_t	x1,
  fixed_t	y1,
  fixed_t	x2,
  fixed_t	y2,
  int		flags,
  boolean	(*trav) (intercept_t *));

void P_UnsetThingPosition (mobj_t* thing);
void P_SetThingPosition (mobj_t* thing);


//
// P_MAP
//

// floatok, tmfloorz, tmceilingz and ceilingline are in the tic scratch.

// fraggle: I have increased the size of this buffer.  In the original Doom,
// overrunning past this limit caused other bits of memory to be overwritten,
// affecting demo playback.  However, in doing so, the limit was still
// exceeded.  So we have to support more than 8 specials.
//
// We keep the original limit, to detect what variables in memory were
// overwritten (see SpechitOverrun())

#define MAXSPECIALCROSS 		20
#define MAXSPECIALCROSS_ORIGINAL	8

boolean P_CheckPosition (mobj_t *thing, fixed_t x, fixed_t y);
boolean P_TryMove (mobj_t* thing, fixed_t x, fixed_t y);
boolean P_TeleportMove (mobj_t* thing, fixed_t x, fixed_t y);
void	P_SlideMove (mobj_t* mo);
boolean P_CheckSight (mobj_t* t1, mobj_t* t2);
void 	P_UseLines (player_t* player);

boolean P_ChangeSector (secidx_t sector, boolean crunch);

// linetarget, who got hit, is a handle (H_NONE for none) in the tic
// scratch: O_Get(linetarget, &view) gives the thing.

fixed_t
P_AimLineAttack
( mobj_t*	t1,
  angle_t	angle,
  fixed_t	distance );

void
P_LineAttack
( mobj_t*	t1,
  angle_t	angle,
  fixed_t	distance,
  fixed_t	slope,
  int		damage );

void
P_RadiusAttack
( mobj_t*	spot,
  mobj_t*	source,
  int		damage );



//
// TIC SCRATCH
// Work areas and transient globals of the game logic. Nothing in it
// outlives the call that sets it, so it lives in the frame region while
// tics run: dmc_tic is placed at FRAME_BASE + 6912 by the link script
// of the device, the renderer and the overlays use the space between
// tics. The host poisons it before every tic (P_Ticker).
//
#define SNDQUEUE	64
#define SIGHTSTACK	64
#define TICSCRATCH	1456

typedef struct
{
    // Scalars first: Thumb has short loads for the first 124 bytes.
    // P_CheckPosition, P_TryMove, P_SlideMove
    mobj_t*		tmthing;
    int			tmflags;
    fixed_t		tmx;
    fixed_t		tmy;
    fixed_t		tmfloorz;
    fixed_t		tmceilingz;
    fixed_t		tmdropoffz;
    boolean		floatok;	// move ok within tmfloorz - tmceilingz
    fixed_t		tmbbox[4];
    lineidx_t		ceilingline;	// lowers the ceiling (sky hack walls)
    lineidx_t		bestslideline;
    lineidx_t		secondslideline;
    unsigned short	linetarget;
    int			numspechit;

    // P_LineOpening, P_PathTraverse
    fixed_t		opentop;
    fixed_t		openbottom;
    fixed_t		openrange;
    fixed_t		lowfloor;
    divline_t		trace;

    fixed_t		bestslidefrac;
    fixed_t		secondslidefrac;
    mobj_t*		slidemo;
    fixed_t		tmxmove;
    fixed_t		tmymove;

    // P_AimLineAttack, P_LineAttack, P_RadiusAttack, P_ChangeSector
    mobj_t*		shootthing;
    fixed_t		shootz;
    int			la_damage;
    fixed_t		attackrange;
    fixed_t		aimslope;
    mobj_t*		bombsource;
    mobj_t*		bombspot;
    int			bombdamage;
    boolean		crushchange;
    boolean		nofit;

    lineidx_t		spechit[MAXSPECIALCROSS];
    // the flood of P_NoiseAlert and P_CrossBSPNode never nest
    union
    {
	unsigned short	sound[SNDQUEUE];
	unsigned short	sight[SIGHTSTACK];
    }			q;
    fixed_t		icfrac[MAXINTERCEPTS];
    unsigned short	icref[MAXINTERCEPTS];
} ticscratch_t;

#if __SIZEOF_POINTER__ == 4
_Static_assert(sizeof(ticscratch_t) <= TICSCRATCH, "the tic scratch is 1,456 B");
#endif

extern ticscratch_t	dmc_tic;

#define spechit		(dmc_tic.spechit)
#define trace		(dmc_tic.trace)
#define opentop		(dmc_tic.opentop)
#define openbottom	(dmc_tic.openbottom)
#define openrange	(dmc_tic.openrange)
#define lowfloor	(dmc_tic.lowfloor)
#define tmbbox		(dmc_tic.tmbbox)
#define tmthing		(dmc_tic.tmthing)
#define tmflags		(dmc_tic.tmflags)
#define tmx		(dmc_tic.tmx)
#define tmy		(dmc_tic.tmy)
#define tmfloorz	(dmc_tic.tmfloorz)
#define tmceilingz	(dmc_tic.tmceilingz)
#define tmdropoffz	(dmc_tic.tmdropoffz)
#define floatok		(dmc_tic.floatok)
#define numspechit	(dmc_tic.numspechit)
#define bestslidefrac	(dmc_tic.bestslidefrac)
#define secondslidefrac	(dmc_tic.secondslidefrac)
#define slidemo		(dmc_tic.slidemo)
#define tmxmove		(dmc_tic.tmxmove)
#define tmymove		(dmc_tic.tmymove)
#define ceilingline	(dmc_tic.ceilingline)
#define bestslideline	(dmc_tic.bestslideline)
#define secondslideline	(dmc_tic.secondslideline)
#define linetarget	(dmc_tic.linetarget)
#define shootthing	(dmc_tic.shootthing)
#define shootz		(dmc_tic.shootz)
#define la_damage	(dmc_tic.la_damage)
#define attackrange	(dmc_tic.attackrange)
#define aimslope	(dmc_tic.aimslope)
#define bombsource	(dmc_tic.bombsource)
#define bombspot	(dmc_tic.bombspot)
#define bombdamage	(dmc_tic.bombdamage)
#define crushchange	(dmc_tic.crushchange)
#define nofit		(dmc_tic.nofit)


//
// P_SETUP
//
extern int		bmapwidth;
extern int		bmapheight;	// in mapblocks
extern fixed_t		bmaporgx;
extern fixed_t		bmaporgy;	// origin of block map



//
// P_INTER
//
extern const int	maxammo[NUMAMMO];
extern const int	clipammo[NUMAMMO];

void
P_TouchSpecialThing
( mobj_t*	special,
  mobj_t*	toucher );

void
P_DamageMobj
( mobj_t*	target,
  mobj_t*	inflictor,
  mobj_t*	source,
  int		damage );


//
// P_SPEC
//
#include "p_spec.h"


#endif	// __P_LOCAL__
