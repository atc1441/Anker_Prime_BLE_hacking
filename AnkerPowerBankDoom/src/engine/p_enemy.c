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
//	Enemy thinking, AI.
//	Action Pointer Functions
//	that are associated with states/frames. 
//

#include <stdio.h>
#include <stdlib.h>

#include "m_random.h"
#include "i_system.h"

#include "doomdef.h"
#include "p_local.h"

#include "s_sound.h"

#include "g_game.h"

// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "sounds.h"

#include "things.h"


typedef enum
{
    DI_EAST,
    DI_NORTHEAST,
    DI_NORTH,
    DI_NORTHWEST,
    DI_WEST,
    DI_SOUTHWEST,
    DI_SOUTH,
    DI_SOUTHEAST,
    DI_NODIR,
    NUMDIRS
    
} dirtype_t;


//
// P_NewChaseDir related LUT.
//
static const unsigned char opposite[] =
{
  DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST,
  DI_EAST, DI_NORTHEAST, DI_NORTH, DI_NORTHWEST, DI_NODIR
};

static const unsigned char diags[] =
{
    DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST, DI_SOUTHEAST
};


void A_Fall (mobj_t *actor);


//
// ENEMY THINKING
// Enemies are allways spawned
// with targetplayer = -1, threshold = 0
// Most monsters are spawned unaware of all players,
// but some can be made preaware
//


//
// P_NoiseAlert
// If a monster yells at a player,
// it will alert other monsters to the player.
//
// The flood gives the sectors reached without crossing a sound blocking
// line label 1 and those reached across one label 2: vanilla's
// recursion (P_RecursiveSound) ends with the smallest label over all
// paths, so labels are relaxed in any order until none improves.
// SNDADJ pairs sectors joined by two-sided lines, flagged when every
// such line blocks sound. A 64-entry worklist (tic scratch), overflow
// sets a queued bit that a sweep with a cursor finds until none is left.
//
static int		sndcount;	// in the worklist
static int		sndqueued;	// queued bits set
static int		sndsweep;	// sweep cursor

static void P_SoundPush (int s)
{
    if (sndcount < SNDQUEUE)
	dmc_tic.q.sound[sndcount++] = s;
    else if (!(secstat[s] & SS_QUEUED))
    {
	secstat[s] |= SS_QUEUED;
	sndqueued++;
    }
}

static int P_SoundPop (void)
{
    int		s;

    if (sndcount)
	return dmc_tic.q.sound[--sndcount];
    if (!sndqueued)
	return -1;
    for (s = sndsweep; !(secstat[s] & SS_QUEUED); )
	if (++s == numsectors)
	    s = 0;
    secstat[s] &= ~SS_QUEUED;
    sndqueued--;
    sndsweep = s;
    return s;
}

// Sound passes between two sectors unless the opening is closed.
static boolean P_SoundOpen (int s, int o)
{
    fixed_t	top;
    fixed_t	bottom;
    fixed_t	otop;
    fixed_t	obottom;

    P_SectorHeights(s, &bottom, &top);
    P_SectorHeights(o, &obottom, &otop);
    if (otop < top)
	top = otop;
    if (obottom > bottom)
	bottom = obottom;
    return top - bottom > 0;
}

// Label 1 or 2 of a sector, 3 when not reached.
static int P_SoundLabel (int s)
{
    return secstat[s] & SS_TRAV1 ? 1 : secstat[s] & SS_TRAV2 ? 2 : 3;
}

static void P_SoundMark (int s, int label)
{
    secstat[s] = (secstat[s] & ~(SS_TRAV1 | SS_TRAV2)) | SS_TARGET
	| (label == 1 ? SS_TRAV1 : SS_TRAV2);
    P_SoundPush(s);
}

void
P_NoiseAlert
( mobj_t*	target,
  mobj_t*	emmiter )
{
    dmc_range_t	r;
    int		s;
    int		k;
    int		o;
    int		label;

    // the sound target of a sector is a bit: single player only
    if (target != players[0].mo)
	I_Error("P_NoiseAlert: target is not the player");

    for (s = 0; s < numsectors; s++)
	secstat[s] &= ~(SS_TRAV1 | SS_TRAV2 | SS_QUEUED);
    sndcount = sndqueued = sndsweep = 0;

    P_SoundMark(emmiter->sector, 1);
    while ((s = P_SoundPop()) >= 0)
    {
	map_range(DMC_ST_SNDADJ, s, &r);
	for (k = 0; k < r.count; k++)
	{
	    o = map_u16(DMC_ST_SNDADJ, 2 * numsectors + r.first + k);
	    label = P_SoundLabel(s) + (o >> 15);
	    o &= ~DMC_SNDADJ_BLOCK;
	    if (label < P_SoundLabel(o) && P_SoundOpen(s, o))
		P_SoundMark(o, label);
	}
    }
}


//
// P_CheckMeleeRange
//
boolean P_CheckMeleeRange (mobj_t*	actor)
{
    mobj_t*	pl;
    fixed_t	dist;
	
    pl = P_Target(actor);
    if (!pl)
	return false;
		
    dist = P_AproxDistance (pl->x-actor->x, pl->y-actor->y);

    if (dist >= MELEERANGE-20*FRACUNIT+MI_RADIUS(pl->type))
	return false;
	
    if (! P_CheckSight (actor, pl) )
	return false;
							
    return true;		
}

//
// P_CheckMissileRange
//
boolean P_CheckMissileRange (mobj_t* actor)
{
    fixed_t	dist;
    mobj_t*	target = P_Target(actor);
	
    if (! P_CheckSight (actor, target) )
	return false;
	
    if ( actor->flags & MF_JUSTHIT )
    {
	// the target just hit the enemy,
	// so fight back!
	actor->flags &= ~MF_JUSTHIT;
	return true;
    }
	
    if (actor->reactiontime)
	return false;	// do not attack yet
		
    // OPTIMIZE: get this from a global checksight
    dist = P_AproxDistance ( actor->x-target->x,
			     actor->y-target->y) - 64*FRACUNIT;
    
    if (!MINFO(actor)->meleestate)
	dist -= 128*FRACUNIT;	// no melee attack, so fire more

    dist >>= 16;

    // The cyberdemon and the spider mastermind are not in E1.
#ifdef DMC_HAVE_MT_SKULL
    if (actor->type == MT_SKULL)
    {
	dist >>= 1;
    }
#endif
    
    if (dist > 200)
	dist = 200;
		
    if (P_Random () < dist)
	return false;
		
    return true;
}


//
// P_Move
// Move in the current direction,
// returns false if the move is blocked.
//
static const fixed_t xspeed[8] = {FRACUNIT,47000,0,-47000,-FRACUNIT,-47000,0,47000};
static const fixed_t yspeed[8] = {0,47000,FRACUNIT,47000,0,-47000,-FRACUNIT,-47000};

boolean P_Move (mobj_t*	actor)
{
    fixed_t	tryx;
    fixed_t	tryy;
    
    lineidx_t	ld;
    
    // warning: 'catch', 'throw', and 'try'
    // are all C++ reserved words
    boolean	try_ok;
    boolean	good;
    fixed_t	speed;
		
    if (actor->movedir == DI_NODIR)
	return false;
		
    if ((unsigned)actor->movedir >= 8)
	I_Error ("Weird actor->movedir!");
		
    speed = P_InfoSpeed(actor->type);
    tryx = actor->x + speed*xspeed[actor->movedir];
    tryy = actor->y + speed*yspeed[actor->movedir];

    try_ok = P_TryMove (actor, tryx, tryy);

    if (!try_ok)
    {
	// open any specials
	if (actor->flags & MF_FLOAT && floatok)
	{
	    // must adjust height
	    if (actor->z < tmfloorz)
		actor->z += FLOATSPEED;
	    else
		actor->z -= FLOATSPEED;

	    actor->flags |= MF_INFLOAT;
	    return true;
	}
		
	if (!numspechit)
	    return false;
			
	actor->movedir = DI_NODIR;
	good = false;
	while (numspechit--)
	{
	    ld = spechit[numspechit];
	    // if the special is not a door
	    // that can be opened,
	    // return false
	    if (P_UseSpecialLine (actor, ld,0))
		good = true;
	}
	return good;
    }
    else
    {
	actor->flags &= ~MF_INFLOAT;
    }
	
	
    if (! (actor->flags & MF_FLOAT) )	
	actor->z = MO_FLOORZ(actor);
    return true; 
}


//
// TryWalk
// Attempts to move actor on
// in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor
// returns FALSE
// If move is either clear or blocked only by a door,
// returns TRUE and sets...
// If a door is in the way,
// an OpenDoor call is made to start it opening.
//
boolean P_TryWalk (mobj_t* actor)
{	
    if (!P_Move (actor))
    {
	return false;
    }

    actor->movecount = P_Random()&15;
    return true;
}


void P_NewChaseDir (mobj_t*	actor)
{
    fixed_t	deltax;
    fixed_t	deltay;
    
    dirtype_t	d[3];
    
    int		tdir;
    dirtype_t	olddir;
    
    dirtype_t	turnaround;
    mobj_t*	target = P_Target(actor);

    if (!target)
	I_Error ("P_NewChaseDir: called with no target");
		
    olddir = actor->movedir;
    turnaround=opposite[olddir];

    deltax = target->x - actor->x;
    deltay = target->y - actor->y;

    if (deltax>10*FRACUNIT)
	d[1]= DI_EAST;
    else if (deltax<-10*FRACUNIT)
	d[1]= DI_WEST;
    else
	d[1]=DI_NODIR;

    if (deltay<-10*FRACUNIT)
	d[2]= DI_SOUTH;
    else if (deltay>10*FRACUNIT)
	d[2]= DI_NORTH;
    else
	d[2]=DI_NODIR;

    // try direct route
    if (d[1] != DI_NODIR
	&& d[2] != DI_NODIR)
    {
	actor->movedir = diags[((deltay<0)<<1)+(deltax>0)];
	if (actor->movedir != (int) turnaround && P_TryWalk(actor))
	    return;
    }

    // try other directions
    if (P_Random() > 200
	||  abs(deltay)>abs(deltax))
    {
	tdir=d[1];
	d[1]=d[2];
	d[2]=tdir;
    }

    if (d[1]==turnaround)
	d[1]=DI_NODIR;
    if (d[2]==turnaround)
	d[2]=DI_NODIR;
	
    if (d[1]!=DI_NODIR)
    {
	actor->movedir = d[1];
	if (P_TryWalk(actor))
	{
	    // either moved forward or attacked
	    return;
	}
    }

    if (d[2]!=DI_NODIR)
    {
	actor->movedir =d[2];

	if (P_TryWalk(actor))
	    return;
    }

    // there is no direct path to the player,
    // so pick another direction.
    if (olddir!=DI_NODIR)
    {
	actor->movedir =olddir;

	if (P_TryWalk(actor))
	    return;
    }

    // randomly determine direction of search
    if (P_Random()&1) 	
    {
	for ( tdir=DI_EAST;
	      tdir<=DI_SOUTHEAST;
	      tdir++ )
	{
	    if (tdir != (int) turnaround)
	    {
		actor->movedir =tdir;
		
		if ( P_TryWalk(actor) )
		    return;
	    }
	}
    }
    else
    {
	for ( tdir=DI_SOUTHEAST;
	      tdir != (DI_EAST-1);
	      tdir-- )
	{
	    if (tdir != (int) turnaround)
	    {
		actor->movedir = tdir;
		
		if ( P_TryWalk(actor) )
		    return;
	    }
	}
    }

    if (turnaround !=  DI_NODIR)
    {
	actor->movedir =turnaround;
	if ( P_TryWalk(actor) )
	    return;
    }

    actor->movedir = DI_NODIR;	// can not move
}


//
// P_LookForPlayers
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//
boolean
P_LookForPlayers
( mobj_t*	actor,
  boolean	allaround )
{
    int		c;
    int		stop;
    player_t*	player;
    angle_t	an;
    fixed_t	dist;

    c = 0;
    stop = (actor->lastlook-1)&3;
	
    for ( ; ; actor->lastlook = (actor->lastlook+1)&3 )
    {
	if (!playeringame[actor->lastlook])
	    continue;
			
	if (c++ == 2
	    || actor->lastlook == stop)
	{
	    // done looking
	    return false;	
	}
	
	player = &players[actor->lastlook];

	if (player->health <= 0)
	    continue;		// dead

	if (!P_CheckSight (actor, player->mo))
	    continue;		// out of sight
			
	if (!allaround)
	{
	    an = R_PointToAngle2 (actor->x,
				  actor->y, 
				  player->mo->x,
				  player->mo->y)
		- actor->angle;
	    
	    if (an > ANG90 && an < ANG270)
	    {
		dist = P_AproxDistance (player->mo->x - actor->x,
					player->mo->y - actor->y);
		// if real close, react anyway
		if (dist > MELEERANGE)
		    continue;	// behind back
	    }
	}
		
	actor->target = O_Hnd(player->mo);
	return true;
    }

    return false;
}


//
// ACTION ROUTINES
//

//
// A_Look
// Stay in state until a player is sighted.
// P_LookSee decides, P_SeeYou wakes the monster up (a sleeping monster
// runs P_LookSee on its view and is promoted before P_SeeYou).
//
boolean P_LookSee (mobj_t* actor)
{
    mobj_t*	targ;
	
    actor->threshold = 0;	// any shot will wake up
    targ = secstat[actor->sector] & SS_TARGET ? players[0].mo : NULL;

    if (targ
	&& (targ->flags & MF_SHOOTABLE) )
    {
	actor->target = O_Hnd(targ);

	if ( actor->flags & MF_AMBUSH )
	{
	    if (P_CheckSight (actor, targ))
		return true;
	}
	else
	    return true;
    }
	
	
    return P_LookForPlayers (actor, false);
}

void P_SeeYou (mobj_t* actor)
{
    const dmc_actorinfo_t* info = MINFO(actor);

    // go into chase state
    if (info->seesound)
    {
	int		sound;
		
	switch (info->seesound)
	{
	  case sfx_posit1:
	  case sfx_posit2:
	  case sfx_posit3:
	    sound = sfx_posit1+P_Random()%3;
	    break;

	  case sfx_bgsit1:
	  case sfx_bgsit2:
	    sound = sfx_bgsit1+P_Random()%2;
	    break;

	  default:
	    sound = info->seesound;
	    break;
	}

	// The bosses at full volume are not in E1.
	S_StartSound (actor, sound);
    }

    P_SetMobjState (actor, info->seestate);
}

void A_Look (mobj_t* actor)
{
    if (P_LookSee(actor))
	P_SeeYou(actor);
}


//
// A_Chase
// Actor has a melee attack,
// so it tries to close as fast as possible
//
void A_Chase (mobj_t*	actor)
{
    int		delta;
    mobj_t*	target;
    const dmc_actorinfo_t* info = MINFO(actor);

    if (actor->reactiontime)
	actor->reactiontime--;
				

    // modify target threshold
    target = P_Target(actor);
    if  (actor->threshold)
    {
	if (!target
	    || target->health <= 0)
	{
	    actor->threshold = 0;
	}
	else
	    actor->threshold--;
    }
    
    // turn towards movement direction if not there yet
    if (actor->movedir < 8)
    {
	actor->angle &= (7<<29);
	delta = actor->angle - (actor->movedir << 29);
	
	if (delta > 0)
	    actor->angle -= ANG90/2;
	else if (delta < 0)
	    actor->angle += ANG90/2;
    }

    if (!target
	|| !(target->flags&MF_SHOOTABLE))
    {
	// look for a new target
	if (P_LookForPlayers(actor,true))
	    return; 	// got a new target
	
	P_SetMobjState (actor, MI_SPAWNSTATE(actor->type));
	return;
    }
    
    // do not attack twice in a row
    if (actor->flags & MF_JUSTATTACKED)
    {
	actor->flags &= ~MF_JUSTATTACKED;
	// no nightmare or -fast (G_InitNew)
	P_NewChaseDir (actor);
	return;
    }
    
    // check for melee attack
    if (info->meleestate
	&& P_CheckMeleeRange (actor))
    {
	if (info->attacksound)
	    S_StartSound (actor, info->attacksound);

	P_SetMobjState (actor, info->meleestate);
	return;
    }
    
    // check for missile attack
    if (info->missilestate)
    {
	if (actor->movecount)
	{
	    goto nomissile;
	}
	
	if (!P_CheckMissileRange (actor))
	    goto nomissile;
	
	P_SetMobjState (actor, info->missilestate);
	actor->flags |= MF_JUSTATTACKED;
	return;
    }

    // ?
  nomissile:
    // possibly choose another target
    if (netgame
	&& !actor->threshold
	&& !P_CheckSight (actor, target) )
    {
	if (P_LookForPlayers(actor,true))
	    return;	// got a new target
    }
    
    // chase towards player
#ifdef DMC_STATS
    // movecount has 16 bits here, vanilla's int would go on
    if (actor->movecount == -32768)
	I_Error ("A_Chase: movecount wraps");
#endif
    if (--actor->movecount<0
	|| !P_Move (actor))
    {
	P_NewChaseDir (actor);
    }
    
    // make active sound
    if (info->activesound
	&& P_Random () < 3)
    {
	S_StartSound (actor, info->activesound);
    }
}


//
// A_FaceTarget
//
void A_FaceTarget (mobj_t* actor)
{	
    mobj_t*	target = P_Target(actor);

    if (!target)
	return;
    
    actor->flags &= ~MF_AMBUSH;
	
    actor->angle = R_PointToAngle2 (actor->x,
				    actor->y,
				    target->x,
				    target->y);
    
    if (target->flags & MF_SHADOW)
	actor->angle += (P_Random()-P_Random())<<21;
}


//
// A_PosAttack
//
void A_PosAttack (mobj_t* actor)
{
    int		angle;
    int		damage;
    int		slope;
	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    angle = actor->angle;
    slope = P_AimLineAttack (actor, angle, MISSILERANGE);

    S_StartSound (actor, sfx_pistol);
    angle += (P_Random()-P_Random())<<20;
    damage = ((P_Random()%5)+1)*3;
    P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
}

void A_SPosAttack (mobj_t* actor)
{
    int		i;
    int		angle;
    int		bangle;
    int		damage;
    int		slope;
	
    if (!actor->target)
	return;

    S_StartSound (actor, sfx_shotgn);
    A_FaceTarget (actor);
    bangle = actor->angle;
    slope = P_AimLineAttack (actor, bangle, MISSILERANGE);

    for (i=0 ; i<3 ; i++)
    {
	angle = bangle + ((P_Random()-P_Random())<<20);
	damage = ((P_Random()%5)+1)*3;
	P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
    }
}


void A_SpidRefire (mobj_t* actor)
{	
    // keep firing unless target got out of sight
    A_FaceTarget (actor);

    if (P_Random () < 10)
	return;

    if (!P_Target(actor)
	|| P_Target(actor)->health <= 0
	|| !P_CheckSight (actor, P_Target(actor)) )
    {
	P_SetMobjState (actor, MINFO(actor)->seestate);
    }
}


//
// A_TroopAttack
//
void A_TroopAttack (mobj_t* actor)
{
    int		damage;
	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    if (P_CheckMeleeRange (actor))
    {
	S_StartSound (actor, sfx_claw);
	damage = (P_Random()%8+1)*3;
	P_DamageMobj (P_Target(actor), actor, actor, damage);
	return;
    }

    
    // launch a missile
    P_SpawnMissile (actor, P_Target(actor), MT_TROOPSHOT);
}


void A_SargAttack (mobj_t* actor)
{
    int		damage;

    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    if (P_CheckMeleeRange (actor))
    {
	damage = ((P_Random()%10)+1)*4;
	P_DamageMobj (P_Target(actor), actor, actor, damage);
    }
}

// A_HeadAttack: the cacodemon is not in E1.

void A_CyberAttack (mobj_t* actor)
{	
    if (!actor->target)
	return;
		
    A_FaceTarget (actor);
    P_SpawnMissile (actor, P_Target(actor), MT_ROCKET);
}


void A_BruisAttack (mobj_t* actor)
{
    int		damage;
	
    if (!actor->target)
	return;
		
    if (P_CheckMeleeRange (actor))
    {
	S_StartSound (actor, sfx_claw);
	damage = (P_Random()%8+1)*10;
	P_DamageMobj (P_Target(actor), actor, actor, damage);
	return;
    }
    
    // launch a missile
    P_SpawnMissile (actor, P_Target(actor), MT_BRUISERSHOT);
}


//
// SkullAttack
// Fly at the player like a missile.
//
#define	SKULLSPEED		(20*FRACUNIT)

void A_SkullAttack (mobj_t* actor)
{
    mobj_t*		dest;
    angle_t		an;
    int			dist;

    dest = P_Target(actor);	
    if (!dest)
	return;
		
    actor->flags |= MF_SKULLFLY;

    S_StartSound (actor, MINFO(actor)->attacksound);
    A_FaceTarget (actor);
    an = actor->angle >> ANGLETOFINESHIFT;
    actor->momx = FixedMul (SKULLSPEED, finecosine(an));
    actor->momy = FixedMul (SKULLSPEED, finesine(an));
    dist = P_AproxDistance (dest->x - actor->x, dest->y - actor->y);
    dist = dist / SKULLSPEED;
    
    if (dist < 1)
	dist = 1;
    actor->momz = (dest->z+(P_Height(dest)>>1) - actor->z) / dist;
}


void A_Scream (mobj_t* actor)
{
    int		sound;
	
    switch (MINFO(actor)->deathsound)
    {
      case 0:
	return;
		
      case sfx_podth1:
      case sfx_podth2:
      case sfx_podth3:
	sound = sfx_podth1 + P_Random ()%3;
	break;
		
      case sfx_bgdth1:
      case sfx_bgdth2:
	sound = sfx_bgdth1 + P_Random ()%2;
	break;
	
      default:
	sound = MINFO(actor)->deathsound;
	break;
    }

    // The bosses at full volume are not in E1.
    S_StartSound (actor, sound);
}


void A_XScream (mobj_t* actor)
{
    S_StartSound (actor, sfx_slop);	
}

void A_Pain (mobj_t* actor)
{
    if (MINFO(actor)->painsound)
	S_StartSound (actor, MINFO(actor)->painsound);	
}


void A_Fall (mobj_t *actor)
{
    // actor is on ground, it can be walked over
    actor->flags &= ~MF_SOLID;

    // So change this if corpse objects
    // are meant to be obstacles.
}


//
// A_Explode
//
void A_Explode (mobj_t* thingy)
{
    P_RadiusAttack(thingy, P_Target(thingy), 128);
}

// Check whether the death of the specified monster type is allowed
// to trigger the end of episode special action.
//
// This behavior changed in v1.9, the most notable effect of which
// was to break uac_dead.wad

// Logic of Ultimate Doom for episode 1, the only one of this port:
// the barons of E1M8.
static boolean CheckBossEnd(mobjtype_t motype)
{
    return gameepisode == 1 && gamemap == 8 && motype == MT_BRUISER;
}

//
// A_BossDeath
// Possibly trigger special effects
// if on first boss level
//
void A_BossDeath (mobj_t* mo)
{
    lineidx_t	junk;
    int		i;
		
    if (!CheckBossEnd(mo->type))
    {
        return;
    }

    // make sure there is a player alive for victory
    for (i=0 ; i<MAXPLAYERS ; i++)
	if (playeringame[i] && players[i].health > 0)
	    break;
    
    if (i==MAXPLAYERS)
	return;	// no one left alive, so do not end game
    
    // scan the remaining thinkers to see
    // if all bosses are dead
    if (O_OtherAlive(mo))
	return;		// other boss not dead
	
    // victory! a line with nothing but tag 666
    junk = P_TagOnlyLine(map_tagindex(666));
    if (P_MoverReserve(junk, 38))
    {
#ifdef DMC_STATS
	o_stats.boss++;
#endif
	EV_DoFloor (junk, lowerFloorToLowest);
    }
}


void A_Hoof (mobj_t* mo)
{
    S_StartSound (mo, sfx_hoof);
    A_Chase (mo);
}

void A_Metal (mobj_t* mo)
{
    S_StartSound (mo, sfx_metal);
    A_Chase (mo);
}


void A_PlayerScream (mobj_t* mo)
{
    // Default death sound.
    int		sound = sfx_pldeth;
	
    S_StartSound (mo, sound);
}
