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
//	Implements special effects:
//	Texture animation, height or lighting changes
//	 according to adjacent sectors, respective
//	 utility functions, etc.
//


#ifndef __P_SPEC__
#define __P_SPEC__



//      Define values for map objects
#define MO_TELEPORTMAN          14


// at game start
void    P_InitPicAnims (void);

// Texture and flat cycles are read from the image (ANIMS) every tic.
// P_InitPicAnims has the data layer keep their frame ranges
// (anim_ranges), P_UpdateSpecials sets the frame offset of leveltime
// (anim_offset), the renderer asks anim_frame.


// at map load
void    P_SpawnSpecials (void);

// every tic
void    P_UpdateSpecials (void);

// when needed
boolean
P_UseSpecialLine
( mobj_t*	thing,
  lineidx_t	line,
  int		side );

void
P_ShootSpecialLine
( mobj_t*	thing,
  lineidx_t	line );

void
P_CrossSpecialLine
( int		linenum,
  int		side,
  mobj_t*	thing );

#ifdef DMC_STATS
// host statistics (host/stats.c): events of the tic, thing or NULL
enum
{
    HS_E_THINK,		// P_Ticker starts its thinkers
    HS_E_THINK_END,	// and has run them
    HS_E_CROSS,		// P_CrossSpecialLine called (a LINES entry on the meter)
    HS_E_SHOOT,		// P_ShootSpecialLine called (a LINES entry)
    HS_E_USE,		// P_UseSpecialLine past the tests of the resident wrapper
    HS_E_HANDLED,	// cross or shoot: the tests of other things let it through
    HS_E_OPENING,	// P_LineOpening
    HS_E_SIGHT		// P_CheckSight
};
void HS_Event (int ev, const mobj_t* thing);
#endif

void    P_PlayerInSpecialSector (player_t* player);

int
twoSided
( int		sector,
  int		line );

secidx_t
getSector
( int		currentSector,
  int		line,
  int		side );

sideidx_t
getSide
( int		currentSector,
  int		line,
  int		side );

fixed_t P_FindLowestFloorSurrounding(secidx_t sec);
fixed_t P_FindHighestFloorSurrounding(secidx_t sec);

fixed_t
P_FindNextHighestFloor
( secidx_t	sec,
  int		currentheight );

fixed_t P_FindLowestCeilingSurrounding(secidx_t sec);
fixed_t P_FindHighestCeilingSurrounding(secidx_t sec);

int
P_FindSectorFromLineTag
( lineidx_t	line,
  int		start );

int
P_FindSectorFromTag
( int		tag,
  int		start );

int
P_FindMinSurroundingLight
( secidx_t	sector,
  int		max );

secidx_t
getNextSector
( lineidx_t	line,
  secidx_t	sec );


//
// SPECIAL
//
int EV_DoDonut(lineidx_t line);



//
// P_LIGHTS
//

// A light thinker (8 B in the level arena): flash, strobe, glow or
// fire flicker. Light levels are 9 bits, bit 8 sits in kind.
typedef struct
{
    unsigned short	link;
    secidx_t		sector;
    unsigned char	kind;
    unsigned char	count;
    unsigned char	minlight;
    unsigned char	maxlight;

} light_t;

_Static_assert(sizeof(light_t) == 8, "a light thinker is 8 B");

#define LT_FLASH	0
#define LT_STROBE	1
#define LT_GLOW		2
#define LT_FLICKER	3
#define LT_TYPE		3
#define LT_MINHI	0x04	// minlight bit 8
#define LT_MAXHI	0x08	// maxlight bit 8
#define LT_SLOW		0x10	// strobe: SLOWDARK, else FASTDARK
#define LT_UP		0x20	// glow: rising

#define LT_MIN(l)	((l)->minlight | ((l)->kind & LT_MINHI) << 6)
#define LT_MAX(l)	((l)->maxlight | ((l)->kind & LT_MAXHI) << 5)

#define GLOWSPEED			8
#define STROBEBRIGHT		5
#define FASTDARK			15
#define SLOWDARK			35

void    P_SpawnFireFlicker (secidx_t sector);
void    P_SetMinMax (light_t* l, int min, int max);
void    T_FireFlicker (light_t* flick);
void    T_LightFlash (light_t* flash);
void    P_SpawnLightFlash (secidx_t sector);
void    T_StrobeFlash (light_t* flash);

void
P_SpawnStrobeFlash
( secidx_t	sector,
  int		fastOrSlow,
  int		inSync );

void    EV_StartLightStrobing(lineidx_t line);
void    EV_TurnTagLightsOff(lineidx_t line);

void
EV_LightTurnOn
( lineidx_t	line,
  int		bright );

void    T_Glow(light_t* g);
void    P_SpawnGlowingLight(secidx_t sector);




//
// P_SWITCH
//
typedef struct
{
    char	name1[9];
    char	name2[9];
    short	episode;
    
} switchlist_t;


typedef enum
{
    top,
    middle,
    bottom

} bwhere_e;


typedef struct
{
    lineidx_t		line;
    unsigned short	btexture;
    unsigned short	btimer;
    unsigned char	where;
    unsigned char	pad;

} button_t;

_Static_assert(sizeof(button_t) == 8, "a button is 8 B");




 // max # of wall switches in a level
#define MAXSWITCHES		50

 // 4 players, 4 buttons each at once, max.
#define MAXBUTTONS		16

 // 1 second, in ticks. 
#define BUTTONTIME      35             

extern button_t*	buttonlist;	// MAXBUTTONS in the level arena

void
P_ChangeSwitchTexture
( lineidx_t	line,
  int		useAgain );



//
// MOVERS
// Doors, floors, plats and ceilings share 20 B slots of the level
// arena. Heights are in 1/8 units, speeds in 1/8 units per tic.
//
#define MV_DOOR		1
#define MV_FLOOR	2
#define MV_PLAT		3
#define MV_CEILING	4
#define MV_KIND		7
#define MV_REMOVED	0x80	// freed when the thinker pass reaches it

#define MOVERBYTES	20
#define MAXMOVERS	32

#define FX8(v)		((fixed_t)(v) << 13)	// 1/8 units to fixed
#ifdef DMC_STATS
// host: I_Error unless v is in 1/8 units and fits 16 bits (port/obj)
short O_Z8 (fixed_t v);
#define TO8(v)		O_Z8(v)
#else
#define TO8(v)		((short)((v) >> 13))
#endif

//
// P_PLATS
//
typedef enum
{
    up,
    down,
    waiting,
    in_stasis

} plat_e;



typedef enum
{
    perpetualRaise,
    downWaitUpStay,
    raiseAndChange,
    raiseToNearestAndChange,
    blazeDWUS

} plattype_e;



typedef struct
{
    unsigned short	link;
    secidx_t		sector;
    unsigned char	kind;
    unsigned char	type;		// plattype_e
    signed char		direction;	// unused
    unsigned char	speed8;
    short		low8;
    short		high8;
    short		wait;
    short		count;
    unsigned char	status;		// plat_e
    unsigned char	oldstatus;
    unsigned char	crush;
    unsigned char	tag;		// TAGS index
    
} plat_t;



#define PLATWAIT		3
#define PLATSPEED		FRACUNIT
#define MAXPLATS		30


extern byte*	activeplats;	// MAXPLATS mover slots, NOMOVER free

void    T_PlatRaise(plat_t*	plat);

int
EV_DoPlat
( lineidx_t	line,
  plattype_e	type,
  int		amount );

void    P_AddActivePlat(plat_t* plat);
void    P_RemoveActivePlat(plat_t* plat);
void    EV_StopPlat(lineidx_t line);
void    P_ActivateInStasis(int tag);


//
// P_DOORS
//
typedef enum
{
    vld_normal,
    vld_close30ThenOpen,
    vld_close,
    vld_open,
    vld_raiseIn5Mins,
    vld_blazeRaise,
    vld_blazeOpen,
    vld_blazeClose

} vldoor_e;



typedef struct
{
    unsigned short	link;
    secidx_t		sector;
    unsigned char	kind;
    unsigned char	type;		// vldoor_e

    // 1 = up, 0 = waiting at top, -1 = down, 2 = initial wait
    signed char		direction;
    unsigned char	speed8;
    short		top8;
    
    // tics to wait at the top
    unsigned short	topwait;
    // (keep in case a door going down is reset)
    // when it reaches 0, start going down
    unsigned short	topcountdown;
    
} vldoor_t;



#define VDOORSPEED		FRACUNIT*2
#define VDOORWAIT		150

void
EV_VerticalDoor
( lineidx_t	line,
  mobj_t*	thing );

int
EV_DoDoor
( lineidx_t	line,
  vldoor_e	type );

int
EV_DoLockedDoor
( lineidx_t	line,
  vldoor_e	type,
  mobj_t*	thing );

void    T_VerticalDoor (vldoor_t* door);
void    P_SpawnDoorCloseIn30 (secidx_t sec);

void
P_SpawnDoorRaiseIn5Mins
( secidx_t	sec,
  int		secnum );







//
// P_CEILNG
//
typedef enum
{
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    fastCrushAndRaise,
    silentCrushAndRaise

} ceiling_e;



typedef struct
{
    unsigned short	link;
    secidx_t		sector;
    unsigned char	kind;
    unsigned char	type;		// ceiling_e

    // 1 = up, 0 = waiting, -1 = down
    signed char		direction;
    unsigned char	speed8;
    short		bottom8;
    short		top8;
    unsigned char	crush;

    // ID
    unsigned char	tag;		// TAGS index
    signed char		olddirection;
    
} ceiling_t;





#define CEILSPEED		FRACUNIT
#define CEILWAIT		150
#define MAXCEILINGS		30

extern byte*	activeceilings;	// MAXCEILINGS mover slots, NOMOVER free

int
EV_DoCeiling
( lineidx_t	line,
  ceiling_e	type );

void    T_MoveCeiling (ceiling_t* ceiling);
void    P_AddActiveCeiling(ceiling_t* c);
void    P_RemoveActiveCeiling(ceiling_t* c);
int	EV_CeilingCrushStop(lineidx_t line);
void    P_ActivateInStasisCeiling(lineidx_t line);


//
// P_FLOOR
//
typedef enum
{
    // lower floor to highest surrounding floor
    lowerFloor,
    
    // lower floor to lowest surrounding floor
    lowerFloorToLowest,
    
    // lower floor to highest surrounding floor VERY FAST
    turboLower,
    
    // raise floor to lowest surrounding CEILING
    raiseFloor,
    
    // raise floor to next highest surrounding floor
    raiseFloorToNearest,

    // raise floor to shortest height texture around it
    raiseToTexture,
    
    // lower floor to lowest surrounding floor
    //  and change floorpic
    lowerAndChange,
  
    raiseFloor24,
    raiseFloor24AndChange,
    raiseFloorCrush,

     // raise to next highest floor, turbo-speed
    raiseFloorTurbo,       
    donutRaise,
    raiseFloor512
    
} floor_e;




typedef enum
{
    build8,	// slowly build by 8
    turbo16	// quickly build by 16
    
} stair_e;



typedef struct
{
    unsigned short	link;
    secidx_t		sector;
    unsigned char	kind;
    unsigned char	type;		// floor_e
    signed char		direction;
    unsigned char	speed8;
    unsigned char	crush;
    unsigned char	newspecial;
    unsigned char	texture;
    unsigned char	pad;
    short		dest8;

} floormove_t;

typedef union
{
    plat_t		plat;
    vldoor_t		door;
    ceiling_t		ceiling;
    floormove_t		floor;

} mover_t;

_Static_assert(sizeof(mover_t) == MOVERBYTES, "a mover is a 20 B slot");

extern mover_t*	movers;		// mover slots of the level arena

// A free slot of the given kind for sector sec, NULL when the pool is
// full (port/obj/movers.c). Callers reserve first with P_MoversFree.
mover_t* P_NewMover (int kind, secidx_t sec);
void P_RemoveMover (void* m);
int P_MoversFree (void);
int P_MoverSlot (void* m);




#define FLOORSPEED		FRACUNIT

typedef enum
{
    ok,
    crushed,
    pastdest
    
} result_e;

result_e
T_MovePlane
( secidx_t	sector,
  fixed_t	speed,
  fixed_t	dest,
  boolean	crush,
  int		floorOrCeiling,
  int		direction );

int
EV_BuildStairs
( lineidx_t	line,
  stair_e	type );

int P_StairDemand (lineidx_t line, stair_e type);
boolean P_MoverReserve (lineidx_t line, int special);
boolean P_MoverReserveTagged (lineidx_t line);
boolean P_MoverCheck (int n);

int
EV_DoFloor
( lineidx_t	line,
  floor_e	floortype );

void T_MoveFloor( floormove_t* floor);

//
// P_TELEPT
//
int
EV_Teleport
( lineidx_t	line,
  int		side,
  mobj_t*	thing );

#endif
