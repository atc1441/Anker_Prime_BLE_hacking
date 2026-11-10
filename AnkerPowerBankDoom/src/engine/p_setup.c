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
//	Set up a level from its map block in the NOR image:
//	RAM state of sectors and lines, things, specials.
//




#include "z_zone.h"

#include "i_swap.h"
#include "m_argv.h"
#include "m_bbox.h"

#include "g_game.h"

#include "i_system.h"

#include "doomdef.h"
#include "p_local.h"

#include "s_sound.h"

#include "doomstat.h"

#include "r_sky.h"

#include "fetch.h"
#include "m_random.h"
#include "st_stuff.h"
#include "hu_stuff.h"
#include "sounds.h"
#include "things.h"

void G_PlayerReborn (int player);
#ifdef DMC_FW
// port/game/i_dmc.c: the caches after the frame region of the plan
void dmc_place_caches (void);
#endif


//
// P_LoadSectors
// The overlays of the mutable sectors start from their NOR records.
//
static void P_LoadSectors (void)
{
    int			i;
    dmc_sector_t	ms;
    secovl_t*		o;
#ifdef DMC_PARITY
    dmc_secextra_t	mx;
#endif

    for (i=0 ; i<numsectors ; i++)
    {
	map_sector(i, &ms);
#ifdef DMC_PARITY
	map_secextra(i, &mx);
	PAR_Sector(i, &ms, &mx);
#endif
	if (ms.overlay == DMC_NONE8)
	    continue;
	o = &secovl[ms.overlay];
	o->floor8 = ms.floor << 3;
	o->ceil8 = ms.ceiling << 3;
	o->light = ms.light;
	o->flags = (ms.light >> 8) & SO_LIGHTHI;
	o->special = ms.special;
	o->floorpic = ms.floorpic;
	o->ceilpic = ms.ceilingpic;
	o->mover = NOMOVER;
    }
}


#ifdef DMC_PARITY
//
// P_CheckThings
// The parity build compares every thing record with the WAD.
//
static void P_CheckThings (void)
{
    int			i;
    dmc_thing_t		mt;
    mapthing_t          spawnthing;
    int			type;

    for (i=0 ; i<M.nthings ; i++)
    {
	map_thing(i, &mt);
	type = O_ThingType(&mt);
	spawnthing.x = mt.x;
	spawnthing.y = mt.y;
	spawnthing.angle = mt.angle45 * 45;
	spawnthing.type = type == MT_PLAYER ? 1 : dmc_mobj_doomednum[type];
	spawnthing.options = mt.options;
	PAR_Thing(i, &spawnthing);
    }
    PAR_ThingsEnd(M.nthings);
}
#endif


//
// P_LoadLineState
// The sides of scrolling walls.
//
static void P_LoadLineState (void)
{
    int		i;

    // P_UpdateSpecials moves the front side of every special 48 line
    if (M.nscrollers > MAXSCROLLSIDES)
	I_Error ("P_LoadLineState: %i scrolling walls", M.nscrollers);
    numscrollsides = 0;
    for (i=0 ; i<M.nscrollers ; i++)
	scrollsides[numscrollsides++] = P_LineSide(map_u16(DMC_ST_SPECIALS, i), 0);
}


//
// P_LoadBlockMap
//
static void P_LoadBlockMap (void)
{
    bmaporgx = M.bmap_ox<<FRACBITS;
    bmaporgy = M.bmap_oy<<FRACBITS;
    bmapwidth = M.bmap_cols;
    bmapheight = M.bmap_rows;
}


//
// P_SetupLevel
//
void
P_SetupLevel
( int		episode,
  int		map,
  int		playermask,
  skill_t	skill)
{
    int		i;
    int		k;

#ifdef DMC_STATS
    // the statistics of the level that ends
    O_StatsWrite ();
#endif
	
    totalkills = totalitems = totalsecret = wminfo.maxfrags = 0;
    wminfo.partime = 180;
    for (i=0 ; i<NUMPLAYERS ; i++)
    {
	players[i].killcount = players[i].secretcount 
	    = players[i].itemcount = 0;
    }

    // Initial height of PointOfView
    // will be set by player think.
    players[consoleplayer].viewz = 1; 

    // Make sure all sounds are stopped before Z_FreeTags.
    S_Start ();			

    Z_FreeTags (PU_LEVEL, PU_PURGELEVEL-1);

    // find the map block
    k = map_find(episode, map);
    if (k < 0)
	I_Error ("P_SetupLevel: E%iM%i is not in this pack", episode, map);
	
    leveltime = 0;
	
    map_load(k, DMC_SKILL_GROUP(skill));
    fetch_reset();
    flat_reset();
#ifdef DMC_FW
    // the record cache and the flat slots follow the frame region of the
    // map's plan, before the level arena is cleared
    dmc_place_caches ();
#endif
    numlines = M.nlines;
    numsides = M.nsides;
    numsegs = M.nsegs;
    numsubsectors = M.nssectors;
    numnodes = M.nnodes;
    numsectors = M.nsectors;
    skytexture = M.sky + 1;

    #ifdef DMC_PARITY
    PAR_SetupLevel (episode, map);
    P_CheckThings ();
#endif

    // the level arena of the plan of this map and skill
    O_ArenaInit ();
    P_ClearMovers ();
    if (players[consoleplayer].attacker)
	players[consoleplayer].attacker = O_DETACHED;

    P_LoadBlockMap ();
    P_LoadSectors ();
    P_LoadLineState ();

    O_SpawnLevel ();
    
    // set up world state
    P_SpawnSpecials ();
}



//
// LEVEL START SPAWNERS
// Only called while a level is set up, with the setup code in the
// LEVEL overlay.
//

//
// P_SpawnMapMobj
// A map thing as a full object: at its NOR list position, linked into
// segment 1 by O_AddSeg1.
//
mobj_t*
P_SpawnMapMobj
( fixed_t	x,
  fixed_t	y,
  fixed_t	z,
  mobjtype_t	type,
  secidx_t	sec )
{
    mobj_t*	mobj = P_NewMobj(x, y, type, false);

    if (!mobj)
	I_Error("P_SpawnMapMobj: no page for type %i", type);
    mobj->sector = sec;
    P_SpawnZ(mobj, z);
    return mobj;
}


//
// P_SpawnPlayerThing
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged
//  between levels.
//
void P_SpawnPlayerThing (int i, const dmc_thing_t* t)
{
    player_t*		p;
    mobj_t*		mobj;

    p = &players[0];

    if (p->playerstate == PST_REBORN)
	G_PlayerReborn (0);

    mobj = P_SpawnMapMobj (t->x << FRACBITS, t->y << FRACBITS, ONFLOORZ, MT_PLAYER, t->sector);
    O_AddSeg1(mobj, i);

    mobj->angle	= ANG45 * t->angle45;
    mobj->flags2 |= MF2_PLAYER;
    mobj->health = p->health;

    p->mo = mobj;
    p->playerstate = PST_LIVE;	
    p->refire = 0;
    p->message = NULL;
    p->damagecount = 0;
    p->bonuscount = 0;
    p->extralight = 0;
    p->fixedcolormap = 0;
    p->viewheight = VIEWHEIGHT;

    // setup gun psprite
    P_SetupPsprites (p);
    
    // wake up the status bar
    ST_Start ();
    // wake up the heads up text
    HU_Start ();		
}




//
// Sector specials of the level start
//

//
// P_SpawnFireFlicker
//
void P_SpawnFireFlicker (secidx_t	sector)
{
    light_t*	flick;
    int		light = P_LightLevel(sector);
	
    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    P_SecOvl(sector)->special = 0; 
	
    flick = P_NewLight(LT_FLICKER, sector);
    P_SetMinMax(flick, P_FindMinSurroundingLight(sector,light)+16, light);
    flick->count = 4;
}



//
// P_SpawnLightFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
//
void P_SpawnLightFlash (secidx_t	sector)
{
    light_t*	flash;
    int		light = P_LightLevel(sector);

    // nothing special about it during gameplay
    P_SecOvl(sector)->special = 0;	
	
    flash = P_NewLight(LT_FLASH, sector);
    P_SetMinMax(flash, P_FindMinSurroundingLight(sector,light), light);
    flash->count = (P_Random()&64)+1;
}



void P_SpawnGlowingLight(secidx_t	sector)
{
    light_t*	g;
    int		light = P_LightLevel(sector);
	
    g = P_NewLight(LT_GLOW, sector);
    P_SetMinMax(g, P_FindMinSurroundingLight(sector,light), light);

    P_SecOvl(sector)->special = 0;
}
//
// Spawn a door that closes after 30 seconds
//
void P_SpawnDoorCloseIn30 (secidx_t sec)
{
    vldoor_t*	door;
	
    door = &P_NewMover (MV_DOOR, sec)->door;

    P_SecOvl(sec)->special = 0;

    door->direction = 0;
    door->type = vld_normal;
    door->speed8 = TO8(VDOORSPEED);
    door->topcountdown = 30 * TICRATE;
}

//
// Spawn a door that opens after 5 minutes
//
void
P_SpawnDoorRaiseIn5Mins
( secidx_t	sec,
  int		secnum )
{
    vldoor_t*	door;
	
    door = &P_NewMover (MV_DOOR, sec)->door;
    
    P_SecOvl(sec)->special = 0;

    door->direction = 2;
    door->type = vld_raiseIn5Mins;
    door->speed8 = TO8(VDOORSPEED);
    door->top8 = TO8(P_FindLowestCeilingSurrounding(sec) - 4*FRACUNIT);
    door->topwait = VDOORWAIT;
    door->topcountdown = 5 * 60 * TICRATE;
}


//
// P_InitPicAnims
// Texture and flat cycles of the image (ANIMS section, global ids in
// the WAD order of their frames), read every tic by P_UpdateSpecials.
// The data layer keeps their frame ranges for the renderer.
//
void P_InitPicAnims (void)
{
    if (anim_ranges () < 0)
	I_Error ("P_InitPicAnims: bad cycle");
}



//
// SPECIAL SPAWNING
//

//
// P_SpawnSpecials
// After the map has been loaded, scan for specials
//  that spawn thinkers
//
// Parses command line parameters.
void P_SpawnSpecials (void)
{
    int		special;
    int		sector;
    int		i;

    //	Init special SECTORs.
    for (i=0 ; i<numsectors ; i++)
    {
	special = P_SecSpecial(i);
	if (!special)
	    continue;
	
	sector = i;
	switch (special)
	{
	  case 1:
	    // FLICKERING LIGHTS
	    P_SpawnLightFlash (sector);
	    break;

	  case 2:
	    // STROBE FAST
	    P_SpawnStrobeFlash(sector,FASTDARK,0);
	    break;
	    
	  case 3:
	    // STROBE SLOW
	    P_SpawnStrobeFlash(sector,SLOWDARK,0);
	    break;
	    
	  case 4:
	    // STROBE FAST/DEATH SLIME
	    P_SpawnStrobeFlash(sector,FASTDARK,0);
	    P_SecOvl(sector)->special = 4;
	    break;
	    
	  case 8:
	    // GLOWING LIGHT
	    P_SpawnGlowingLight(sector);
	    break;
	  case 9:
	    // SECRET SECTOR
	    totalsecret++;
	    break;
	    
	  case 10:
	    // DOOR CLOSE IN 30 SECONDS
	    P_SpawnDoorCloseIn30 (sector);
	    break;
	    
	  case 12:
	    // SYNC STROBE SLOW
	    P_SpawnStrobeFlash (sector, SLOWDARK, 1);
	    break;

	  case 13:
	    // SYNC STROBE FAST
	    P_SpawnStrobeFlash (sector, FASTDARK, 1);
	    break;

	  case 14:
	    // DOOR RAISE IN 5 MINUTES
	    P_SpawnDoorRaiseIn5Mins (sector, i);
	    break;
	    
	  case 17:
	    P_SpawnFireFlicker(sector);
	    break;
	}
    }

    
    //	Init line EFFECTs
    // The scrolling walls (special 48) are set up at level load.

    
    //	Init other misc stuff: activeplats, activeceilings and the
    //	buttons start cleared in the level arena.
}


//
// P_Init
//
void P_Init (void)
{
    P_InitPicAnims ();
    R_InitSprites ();
}
