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
//	Moving object handling. Spawn functions.
//

#include <stdio.h>

#include "i_system.h"
#include "z_zone.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"
#include "sounds.h"

#include "st_stuff.h"
#include "hu_stuff.h"

#include "s_sound.h"

#include "doomstat.h"

#include "things.h"


void G_PlayerReborn (int player);


//
// P_Radius, P_Height
// From the type: a corpse has a quarter of the height, gibs none.
//
fixed_t P_Radius (mobj_t* mo)
{
    return mo->flags2 & MF2_CRUSHED ? 0 : MI_RADIUS(mo->type);
}

fixed_t P_Height (mobj_t* mo)
{
    if (mo->flags2 & MF2_CRUSHED)
	return 0;
    if (mo->flags & MF_CORPSE)
	return MI_HEIGHT(mo->type) >> 2;
    return MI_HEIGHT(mo->type);
}


//
// P_InfoSpeed
// The converter stores speeds in map units, those of missiles were
// fixed point in vanilla.
//
fixed_t P_InfoSpeed (mobjtype_t type)
{
    int		speed = MI_ACTOR(type)->speed;

    return MI_FLAGS(type) & MF_MISSILE ? speed << FRACBITS : speed;
}


//
// P_SetMobjState
// Returns true if the mobj is still present.
//
boolean
P_SetMobjState
( mobj_t*	mobj,
  statenum_t	state )
{
    const state_t*	st;

#ifdef DMC_STATS
    // a view must be promoted before its state changes
    if (mobj->flags2 & MF2_VIEW)
	I_Error("P_SetMobjState: view of %x", mobj->link);
#endif
    do
    {
	if (state == S_NULL)
	{
	    mobj->statenum = S_NULL;
	    P_RemoveMobj (mobj);
	    return false;
	}

	st = &dmc_states[state];
	mobj->statenum = state;
	mobj->tics = ST_TICS(st);

	// Modified handling.
	// Call action functions when the state is set
	if (st->action)		
	    p_actions[st->action].acp1(mobj);	
	
	state = st->nextstate;
    } while (!mobj->tics);
				
    return true;
}


//
// P_ExplodeMissile  
//
void P_ExplodeMissile (mobj_t* mo)
{
    mo->momx = mo->momy = mo->momz = 0;

    P_SetMobjState (mo, MINFO(mo)->deathstate);

    mo->tics -= P_Random()&3;

    if (mo->tics < 1)
	mo->tics = 1;

    mo->flags &= ~MF_MISSILE;

    if (MINFO(mo)->deathsound)
	S_StartSound (mo, MINFO(mo)->deathsound);
}


//
// P_XYMovement  
//
#define STOPSPEED		0x1000
#define FRICTION		0xe800

void P_XYMovement (mobj_t* mo) 
{ 	
    fixed_t 	ptryx;
    fixed_t	ptryy;
    player_t*	player;
    fixed_t	xmove;
    fixed_t	ymove;
			
    if (!mo->momx && !mo->momy)
    {
	if (mo->flags & MF_SKULLFLY)
	{
	    // the skull slammed into something
	    mo->flags &= ~MF_SKULLFLY;
	    mo->momx = mo->momy = mo->momz = 0;

	    P_SetMobjState (mo, MI_SPAWNSTATE(mo->type));
	}
	return;
    }
	
    player = MO_PLAYER(mo);
		
    if (mo->momx > MAXMOVE)
	mo->momx = MAXMOVE;
    else if (mo->momx < -MAXMOVE)
	mo->momx = -MAXMOVE;

    if (mo->momy > MAXMOVE)
	mo->momy = MAXMOVE;
    else if (mo->momy < -MAXMOVE)
	mo->momy = -MAXMOVE;
		
    xmove = mo->momx;
    ymove = mo->momy;
	
    do
    {
	if (xmove > MAXMOVE/2 || ymove > MAXMOVE/2)
	{
	    ptryx = mo->x + xmove/2;
	    ptryy = mo->y + ymove/2;
	    xmove >>= 1;
	    ymove >>= 1;
	}
	else
	{
	    ptryx = mo->x + xmove;
	    ptryy = mo->y + ymove;
	    xmove = ymove = 0;
	}
		
	if (!P_TryMove (mo, ptryx, ptryy))
	{
	    // blocked move
	    if (player)
	    {	// try to slide along it
		P_SlideMove (mo);
	    }
	    else if (mo->flags & MF_MISSILE)
	    {
		// explode a missile
		if (ceilingline != IDX_NONE &&
		    P_LineBackSector(ceilingline) != IDX_NONE &&
		    P_CeilingPic(P_LineBackSector(ceilingline)) == skyflatnum)
		{
		    // Hack to prevent missiles exploding
		    // against the sky.
		    // Does not handle sky floors.
		    P_RemoveMobj (mo);
		    return;
		}
		P_ExplodeMissile (mo);
	    }
	    else
		mo->momx = mo->momy = 0;
	}
    } while (xmove || ymove);
    
    // slow down
    if (player && player->cheats & CF_NOMOMENTUM)
    {
	// debug option for no sliding at all
	mo->momx = mo->momy = 0;
	return;
    }

    if (mo->flags & (MF_MISSILE | MF_SKULLFLY) )
	return; 	// no friction for missiles ever
		
    if (mo->z > MO_FLOORZ(mo))
	return;		// no friction when airborne

    if (mo->flags & MF_CORPSE)
    {
	// do not stop sliding
	//  if halfway off a step with some momentum
	if (mo->momx > FRACUNIT/4
	    || mo->momx < -FRACUNIT/4
	    || mo->momy > FRACUNIT/4
	    || mo->momy < -FRACUNIT/4)
	{
	    if (MO_FLOORZ(mo) != P_FloorHeight(mo->sector))
		return;
	}
    }

    if (mo->momx > -STOPSPEED
	&& mo->momx < STOPSPEED
	&& mo->momy > -STOPSPEED
	&& mo->momy < STOPSPEED
	&& (!player
	    || (player->cmd.forwardmove== 0
		&& player->cmd.sidemove == 0 ) ) )
    {
	// if in a walking frame, stop moving
	if ( player&&(unsigned)(player->mo->statenum - S_PLAY_RUN1) < 4)
	    P_SetMobjState (player->mo, S_PLAY);
	
	mo->momx = 0;
	mo->momy = 0;
    }
    else
    {
	mo->momx = FixedMul (mo->momx, FRICTION);
	mo->momy = FixedMul (mo->momy, FRICTION);
    }
}

//
// P_ZMovement
//
void P_ZMovement (mobj_t* mo)
{
    fixed_t	dist;
    fixed_t	delta;
    player_t*	player = MO_PLAYER(mo);
    mobj_t*	target;
    
    // check for smooth step up
    if (player && mo->z < MO_FLOORZ(mo))
    {
	player->viewheight -= MO_FLOORZ(mo)-mo->z;

	player->deltaviewheight
	    = (VIEWHEIGHT - player->viewheight)>>3;
    }
    
    // adjust height
    mo->z += mo->momz;
	
    if ( mo->flags & MF_FLOAT
	 && (target = P_Target(mo)) != NULL)
    {
	// float down towards target if too close
	if ( !(mo->flags & MF_SKULLFLY)
	     && !(mo->flags & MF_INFLOAT) )
	{
	    dist = P_AproxDistance (mo->x - target->x,
				    mo->y - target->y);
	    
	    delta =(target->z + (P_Height(mo)>>1)) - mo->z;

	    if (delta<0 && dist < -(delta*3) )
		mo->z -= FLOATSPEED;
	    else if (delta>0 && dist < (delta*3) )
		mo->z += FLOATSPEED;			
	}
	
    }
    
    // clip movement
    if (mo->z <= MO_FLOORZ(mo))
    {
	// hit the floor

	// The version of Ultimate Doom bounces a lost soul that
	// hits the floor.
	if (mo->flags & MF_SKULLFLY)
	{
	    // the skull slammed into something
	    mo->momz = -mo->momz;
	}
	
	if (mo->momz < 0)
	{
	    if (player
		&& mo->momz < -GRAVITY*8)	
	    {
		// Squat down.
		// Decrease viewheight for a moment
		// after hitting the ground (hard),
		// and utter appropriate sound.
		player->deltaviewheight = mo->momz>>3;
		S_StartSound (mo, sfx_oof);
	    }
	    mo->momz = 0;
	}
	mo->z = MO_FLOORZ(mo);

	if ( (mo->flags & MF_MISSILE)
	     && !(mo->flags & MF_NOCLIP) )
	{
	    P_ExplodeMissile (mo);
	    return;
	}
    }
    else if (! (mo->flags & MF_NOGRAVITY) )
    {
	if (mo->momz == 0)
	    mo->momz = -GRAVITY*2;
	else
	    mo->momz -= GRAVITY;
    }
	
    if (mo->z + P_Height(mo) > MO_CEILZ(mo))
    {
	// hit the ceiling
	if (mo->momz > 0)
	    mo->momz = 0;
	{
	    mo->z = MO_CEILZ(mo) - P_Height(mo);
	}

	if (mo->flags & MF_SKULLFLY)
	{	// the skull slammed into something
	    mo->momz = -mo->momz;
	}
	
	if ( (mo->flags & MF_MISSILE)
	     && !(mo->flags & MF_NOCLIP) )
	{
	    P_ExplodeMissile (mo);
	    return;
	}
    }
} 



//
// P_MobjThinker
// Nightmare respawn is not supported.
//
void P_MobjThinker (mobj_t* mobj)
{
    // momentum movement
    if (mobj->momx
	|| mobj->momy
	|| (mobj->flags&MF_SKULLFLY) )
    {
	P_XYMovement (mobj);

	if (mobj->flags2 & MF2_REMOVED)
	    return;		// mobj was removed
    }
    if ( (mobj->z != MO_FLOORZ(mobj))
	 || mobj->momz )
    {
	P_ZMovement (mobj);
	
	if (mobj->flags2 & MF2_REMOVED)
	    return;		// mobj was removed
    }

    
    // cycle through states,
    // calling action functions at transitions
    if (mobj->tics != -1)
    {
	mobj->tics--;
		
	// you can cycle through multiple states in a tic
	if (!mobj->tics)
	    if (!P_SetMobjState (mobj, dmc_states[mobj->statenum].nextstate) )
		return;		// freed itself
    }
    O_CheckCorpse(mobj);
}


//
// P_NewMobj
// A cleared page with the fields of P_SpawnMobj, NULL when no page is
// left (the P_Random call is made anyway).
//
mobj_t*
P_NewMobj
( fixed_t	x,
  fixed_t	y,
  mobjtype_t	type,
  int		transient )
{
    mobj_t*	mobj;
    int		lastlook;
    int		p;

    lastlook = P_Random () % MAXPLAYERS;
    p = O_AllocPage(transient);
    if (p < 0)
	return NULL;
    mobj = O_PAGE(p);
	
    mobj->type = type;
    mobj->x = x;
    mobj->y = y;
    mobj->flags = MI_FLAGS(type);
    mobj->health = MI_SPAWNHEALTH(type);
    mobj->reactiontime = MI_ACTOR(type)->reactiontime;
    mobj->lastlook = lastlook;
    mobj->flags2 = NOT_MAPTHING >> 8 << MF2_IDXSHIFT;
    mobj->spawnlo = NOT_MAPTHING & 0xff;

    // do not set the state with P_SetMobjState,
    // because action routines can not be called yet
    mobj->statenum = MI_SPAWNSTATE(type);
    mobj->tics = ST_TICS(&dmc_states[mobj->statenum]);
    return mobj;
}

void P_SpawnZ (mobj_t* mobj, fixed_t z)
{
    fixed_t	floorh;
    fixed_t	ceilh;

    P_SectorHeights(mobj->sector, &floorh, &ceilh);
    MO_SETFLOORZ(mobj, floorh);
    MO_SETCEILZ(mobj, ceilh);

    if (z == ONFLOORZ)
	mobj->z = MO_FLOORZ(mobj);
    else if (z == ONCEILINGZ)
	mobj->z = MO_CEILZ(mobj) - MI_HEIGHT(mobj->type);
    else 
	mobj->z = z;
}


//
// P_SpawnMobj
// A full object at the end of the thinker list. Puffs, blood and fog
// are cosmetic: without a page beyond the reserve they are skipped (a
// counted event) and NULL is returned. Without any page at all a missile
// is lost too (an overrun).
//
mobj_t*
P_SpawnMobj
( fixed_t	x,
  fixed_t	y,
  fixed_t	z,
  mobjtype_t	type )
{
    mobj_t*	mobj;
    int		transient;

    transient = type == MT_PUFF || type == MT_BLOOD || type == MT_TFOG;
    mobj = P_NewMobj(x, y, type, transient);
    if (!mobj)
    {
	O_EVENT(transient ? EV_TRANSIENT_SKIPPED : EV_OVERRUN);
	return NULL;
    }

    // set subsector and/or block links
    P_SetThingPosition (mobj);
    P_SpawnZ(mobj, z);
    O_LinkThinker(L_OBJ | O_PAGEIDX(mobj));

    return mobj;
}


//
// P_RemoveMobj
// Items are not queued for respawning, that is deathmatch 2 only.
//
void P_RemoveMobj (mobj_t* mobj)
{
    O_Remove(mobj);
}




//
// GAME SPAWN FUNCTIONS
//


//
// P_SpawnPuff
// Without a page the puff is skipped, its P_Random calls are made.
//

void
P_SpawnPuff
( fixed_t	x,
  fixed_t	y,
  fixed_t	z )
{
    mobj_t*	th;
    int		r;
	
    z += ((P_Random()-P_Random())<<10);

    th = P_SpawnMobj (x,y,z, MT_PUFF);
    r = P_Random()&3;
    if (!th)
	return;
    th->momz = FRACUNIT;
    th->tics -= r;

    if (th->tics < 1)
	th->tics = 1;
	
    // don't make punches spark on the wall
    if (attackrange == MELEERANGE)
	P_SetMobjState (th, S_PUFF3);
}



//
// P_SpawnBlood
// 
void
P_SpawnBlood
( fixed_t	x,
  fixed_t	y,
  fixed_t	z,
  int		damage )
{
    mobj_t*	th;
    int		r;
	
    z += ((P_Random()-P_Random())<<10);
    th = P_SpawnMobj (x,y,z, MT_BLOOD);
    r = P_Random()&3;
    if (!th)
	return;
    th->momz = FRACUNIT*2;
    th->tics -= r;

    if (th->tics < 1)
	th->tics = 1;
		
    if (damage <= 12 && damage >= 9)
	P_SetMobjState (th,S_BLOOD2);
    else if (damage < 9)
	P_SetMobjState (th,S_BLOOD3);
}



//
// P_CheckMissileSpawn
// Moves the missile forward a bit
//  and possibly explodes it right there.
//
void P_CheckMissileSpawn (mobj_t* th)
{
    th->tics -= P_Random()&3;
    if (th->tics < 1)
	th->tics = 1;
    
    // move a little forward so an angle can
    // be computed if it immediately explodes
    th->x += (th->momx>>1);
    th->y += (th->momy>>1);
    th->z += (th->momz>>1);

    if (!P_TryMove (th, th->x, th->y))
	P_ExplodeMissile (th);
}

//
// P_SpawnMissile
//
mobj_t*
P_SpawnMissile
( mobj_t*	source,
  mobj_t*	dest,
  mobjtype_t	type )
{
    mobj_t*	th;
    angle_t	an;
    int		dist;
    const dmc_actorinfo_t* info = MI_ACTOR(type);
    fixed_t	speed = P_InfoSpeed(type);

    th = P_SpawnMobj (source->x,
		      source->y,
		      source->z + 4*8*FRACUNIT, type);
    if (!th)
    {
	// lost (an overrun), its P_Random calls are still made
	if (dest->flags & MF_SHADOW)
	    P_Random(), P_Random();
	P_Random();
	return NULL;
    }
    
    if (info->seesound)
	S_StartSound (th, info->seesound);

    th->target = O_Hnd(source);	// where it came from
    an = R_PointToAngle2 (source->x, source->y, dest->x, dest->y);

    // fuzzy player
    if (dest->flags & MF_SHADOW)
	an += (P_Random()-P_Random())<<20;	

    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->momx = FixedMul (speed, finecosine(an));
    th->momy = FixedMul (speed, finesine(an));
	
    dist = P_AproxDistance (dest->x - source->x, dest->y - source->y);
    dist = dist / speed;

    if (dist < 1)
	dist = 1;

    th->momz = (dest->z - source->z) / dist;
    P_CheckMissileSpawn (th);
	
    return th;
}


//
// P_SpawnPlayerMissile
// Tries to aim at a nearby monster
//
void
P_SpawnPlayerMissile
( mobj_t*	source,
  mobjtype_t	type )
{
    mobj_t*	th;
    angle_t	an;
    
    fixed_t	x;
    fixed_t	y;
    fixed_t	z;
    fixed_t	slope;
    const dmc_actorinfo_t* info = MI_ACTOR(type);
    fixed_t	speed = P_InfoSpeed(type);
    
    // see which target is to be aimed at
    an = source->angle;
    slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
    
    if (!linetarget)
    {
	an += 1<<26;
	slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);

	if (!linetarget)
	{
	    an -= 2<<26;
	    slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
	}

	if (!linetarget)
	{
	    an = source->angle;
	    slope = 0;
	}
    }
		
    x = source->x;
    y = source->y;
    z = source->z + 4*8*FRACUNIT;
	
    th = P_SpawnMobj (x,y,z, type);
    if (!th)
    {
	// lost (an overrun), P_CheckMissileSpawn's P_Random call is made
	P_Random();
	return;
    }

    if (info->seesound)
	S_StartSound (th, info->seesound);

    th->target = O_Hnd(source);
    th->angle = an;
    th->momx = FixedMul( speed,
			 finecosine(an>>ANGLETOFINESHIFT));
    th->momy = FixedMul( speed,
			 finesine(an>>ANGLETOFINESHIFT));
    th->momz = FixedMul( speed, slope);

    P_CheckMissileSpawn (th);
}
