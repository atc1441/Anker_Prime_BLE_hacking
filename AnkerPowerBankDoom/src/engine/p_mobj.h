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
//	Map Objects, MObj, definition and handling.
//


#ifndef __P_MOBJ__
#define __P_MOBJ__

// Basics.
#include "tables.h"
#include "m_fixed.h"

// We need the thinker_t stuff.
#include "d_think.h"

// We need the WAD data structure for Map things,
// from the THINGS lump.
#include "doomdata.h"

// States are tied to finite states are
//  tied to animation frames.
// Needs precompiled tables/data structures.
#include "info.h"
#include "sounds.h"

// Opaque map index types.
#include "rec.h"






//
// NOTES: mobj_t
//
// mobj_ts are used to tell the refresh where to draw an image,
// tell the world simulation when objects are contacted,
// and tell the sound driver how to position a sound.
//
// The refresh uses the next and prev links to follow
// lists of things in sectors as they are being drawn.
// The sprite, frame, and angle elements determine which patch_t
// is used to draw the sprite if it is visible.
// The sprite and frame values are allmost allways set
// from state_t structures.
// The statescr.exe utility generates the states.h and states.c
// files that contain the sprite/frame numbers from the
// statescr.txt source file.
// The xyz origin point represents a point at the bottom middle
// of the sprite (between the feet of a biped).
// This is the default origin position for patch_ts grabbed
// with lumpy.exe.
// A walking creature will have its z equal to the floor
// it is standing on.
//
// The sound code uses the x,y, and subsector fields
// to do stereo positioning of any sound effited by the mobj_t.
//
// The play simulation uses the blocklinks, x,y,z, radius, height
// to determine when mobj_ts are touching each other,
// touching lines in the map, or hit by trace lines (gunshots,
// lines of sight, etc).
// The mobj_t->flags element has various bit flags
// used by the simulation.
//
// Every mobj_t is linked into a single sector
// based on its origin coordinates.
// The subsector_t is found with R_PointInSubsector(x,y),
// and the sector_t can be found with subsector->sector.
// The sector links are only used by the rendering code,
// the play simulation does not care about them at all.
//
// Any mobj_t that needs to be acted upon by something else
// in the play world (block movement, be shot, etc) will also
// need to be linked into the blockmap.
// If the thing has the MF_NOBLOCK flag set, it will not use
// the block links. It can still interact with other things,
// but only as the instigator (missiles will run into other
// things, but nothing can run into a missile).
// Each block in the grid is 128*128 units, and knows about
// every line_t that it contains a piece of, and every
// interactable mobj_t that has its origin contained.  
//
// A valid mobj_t is a mobj_t that has the proper subsector_t
// filled in for its xy coordinates and is linked into the
// sector from which the subsector was made, or has the
// MF_NOSECTOR flag set (the subsector_t needs to be valid
// even if MF_NOSECTOR is set), and is linked into a blockmap
// block or has the MF_NOBLOCKMAP flag set.
// Links should only be modified by the P_[Un]SetThingPosition()
// functions.
// Do not change the MF_NO? flags while a thing is valid.
//
// Any questions?
//

//
// Misc. mobj flags
//
typedef enum
{
    // Call P_SpecialThing when touched.
    MF_SPECIAL		= 1,
    // Blocks.
    MF_SOLID		= 2,
    // Can be hit.
    MF_SHOOTABLE	= 4,
    // Don't use the sector links (invisible but touchable).
    MF_NOSECTOR		= 8,
    // Don't use the blocklinks (inert but displayable)
    MF_NOBLOCKMAP	= 16,                    

    // Not to be activated by sound, deaf monster.
    MF_AMBUSH		= 32,
    // Will try to attack right back.
    MF_JUSTHIT		= 64,
    // Will take at least one step before attacking.
    MF_JUSTATTACKED	= 128,
    // On level spawning (initial position),
    //  hang from ceiling instead of stand on floor.
    MF_SPAWNCEILING	= 256,
    // Don't apply gravity (every tic),
    //  that is, object will float, keeping current height
    //  or changing it actively.
    MF_NOGRAVITY	= 512,

    // Movement flags.
    // This allows jumps from high places.
    MF_DROPOFF		= 0x400,
    // For players, will pick up items.
    MF_PICKUP		= 0x800,
    // Player cheat. ???
    MF_NOCLIP		= 0x1000,
    // Player: keep info about sliding along walls.
    MF_SLIDE		= 0x2000,
    // Allow moves to any height, no gravity.
    // For active floaters, e.g. cacodemons, pain elementals.
    MF_FLOAT		= 0x4000,
    // Don't cross lines
    //   ??? or look at heights on teleport.
    MF_TELEPORT		= 0x8000,
    // Don't hit same species, explode on block.
    // Player missiles as well as fireballs of various kinds.
    MF_MISSILE		= 0x10000,	
    // Dropped by a demon, not level spawned.
    // E.g. ammo clips dropped by dying former humans.
    MF_DROPPED		= 0x20000,
    // Use fuzzy draw (shadow demons or spectres),
    //  temporary player invisibility powerup.
    MF_SHADOW		= 0x40000,
    // Flag: don't bleed when shot (use puff),
    //  barrels and shootable furniture shall not bleed.
    MF_NOBLOOD		= 0x80000,
    // Don't stop moving halfway off a step,
    //  that is, have dead bodies slide down all the way.
    MF_CORPSE		= 0x100000,
    // Floating to a height for a move, ???
    //  don't auto float to target's height.
    MF_INFLOAT		= 0x200000,

    // On kill, count this enemy object
    //  towards intermission kill total.
    // Happy gathering.
    MF_COUNTKILL	= 0x400000,
    
    // On picking up, count this item object
    //  towards intermission item total.
    MF_COUNTITEM	= 0x800000,

    // Special handling: skull in flight.
    // Neither a cacodemon nor a missile.
    MF_SKULLFLY		= 0x1000000,

    // Don't spawn this object
    //  in death match mode (e.g. key cards).
    MF_NOTDMATCH    	= 0x2000000,

    // Player sprites in multiplayer modes are modified
    //  using an internal color lookup table for re-indexing.
    // If 0x4 0x8 or 0xc,
    //  use a translation table for player colormaps
    MF_TRANSLATION  	= 0xc000000,
    // Hmm ???.
    MF_TRANSSHIFT	= 26

} mobjflag_t;


// Map Object definition: a full object of the object store, one
// 60 B page (port/obj/things.h). Links and references are u16 handles,
// radius, height, sprite and frame follow from type, state and flags2.
typedef struct mobj_s
{
    // Info for drawing: position.
    fixed_t		x;
    fixed_t		y;
    fixed_t		z;

    // Momentums, used to update position.
    fixed_t		momx;
    fixed_t		momy;
    fixed_t		momz;

    angle_t		angle;	// orientation
    int			flags;

    // The closest interval over all contacted Sectors, 1/8 units.
    short		floor8;
    short		ceil8;

    unsigned short	statenum;	// index into dmc_states[]
    short		tics;	// state tic counter
    short		health;
    short		movecount;	// when 0, select a new dir

    // Thing being chased/attacked (or 0),
    // also the originator for missiles: a handle.
    unsigned short	target;

    // Sector of the subsector the thing is in.
    secidx_t		sector;

    // Links in the sector and blockmap chains and in the
    // thinker lists (a view keeps its own handle in link).
    unsigned short	snext;
    unsigned short	bnext;
    unsigned short	link;

    unsigned char	type;
    unsigned char	movedir:4;	// 0-7, 8 none
    unsigned char	lastlook:2;	// player number last looked for

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    unsigned char	reactiontime;

    // If >0, the target will be chased
    // no matter what (even if shot)
    unsigned char	threshold;

    unsigned char	flags2;
    unsigned char	spawnlo;	// map thing index bits 0-7
} mobj_t;

_Static_assert(sizeof(mobj_t) == 60, "a full object is one 60 B page");

// flags2
#define MF2_PLAYER	0x01	// the player's object
#define MF2_NORPOS	0x02	// map thing still at its NOR list position
#define MF2_CRUSHED	0x04	// gibbed by a crusher, radius and height 0
#define MF2_REMOVED	0x08	// freed when the thinker pass reaches it
#define MF2_VIEW	0x10	// read-only view, link holds its handle
#define MF2_IDXSHIFT	6	// map thing index bits 8-9

// Spawn index of an object that is not a map thing.
#define NOT_MAPTHING	0x3ff

#define MO_SPAWNIDX(mo) \
    ((mo)->spawnlo | ((mo)->flags2 >> MF2_IDXSHIFT) << 8)

//
// Info tables of the E1 roster with dense ids, generated by
// tools/conv/info.py (gen/info.c): dmc_states, dmc_mobjinfo for every
// type and dmc_actorinfo for the types below DMC_NUMACTORS. The other
// types have the actor values of p_noactor (the converter checks).
//
typedef dmc_state_t	state_t;

#define ST_TICS(st)	DMC_STATE_TICS(st)	// -1: for ever
#define ST_FRAME(st)	DMC_STATE_FRAME(st)	// with FF_FULLBRIGHT

// Function of dmc_state_t.action, called with the mobj or with the
// player and the psprite (info.c).
extern const actionf_t		p_actions[DMC_NUMACTIONS];

#define MI_FLAGS(t)		((int)DMC_MOBJ_FLAGS(&dmc_mobjinfo[t]))
#define MI_SPAWNSTATE(t)	((statenum_t)dmc_mobjinfo[t].spawnstate)
#define MI_SPAWNHEALTH(t)	((int)dmc_mobjinfo[t].spawnhealth)
#define MI_RADIUS(t)		((fixed_t)dmc_mobjinfo[t].radius << FRACBITS)
#define MI_HEIGHT(t)		((fixed_t)dmc_mobjinfo[t].height << FRACBITS)

// Actor fields of a type (states, painchance, mass, damage,
// reactiontime, sounds), P_InfoSpeed gives the speed.
extern const dmc_actorinfo_t	p_noactor;

#define MI_ACTOR(t) \
    ((unsigned)(t) < DMC_NUMACTORS ? &dmc_actorinfo[t] : &p_noactor)
#define MINFO(mo)	MI_ACTOR((mo)->type)

fixed_t P_InfoSpeed (mobjtype_t type);

// floorz and ceilingz in fixed point
#define MO_FLOORZ(mo)	((fixed_t)(mo)->floor8 << 13)
#define MO_CEILZ(mo)	((fixed_t)(mo)->ceil8 << 13)
#define MO_SETFLOORZ(mo, v)	((mo)->floor8 = TO8(v))
#define MO_SETCEILZ(mo, v)	((mo)->ceil8 = TO8(v))

// The player of the object, single player only.
#define MO_PLAYER(mo)	((mo)->flags2 & MF2_PLAYER ? &players[0] : NULL)



#endif
