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
// DESCRIPTION: Door animation code (opening/closing)
//



#include "z_zone.h"
#include "doomdef.h"
#include "deh_main.h"
#include "i_system.h"
#include "p_local.h"

#include "s_sound.h"


// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

#include "things.h"




//
// VERTICAL DOORS
//

//
// T_VerticalDoor
//
void T_VerticalDoor (vldoor_t* door)
{
    result_e	res;
    void*	org = P_SecSoundOrg(door->sector);
	
    switch(door->direction)
    {
      case 0:
	// WAITING
	if (!--door->topcountdown)
	{
	    switch(door->type)
	    {
	      case vld_blazeRaise:
		door->direction = -1; // time to go back down
		S_StartSound(org, sfx_bdcls);
		break;
		
	      case vld_normal:
		door->direction = -1; // time to go back down
		S_StartSound(org, sfx_dorcls);
		break;
		
	      case vld_close30ThenOpen:
		door->direction = 1;
		S_StartSound(org, sfx_doropn);
		break;
		
	      default:
		break;
	    }
	}
	break;
	
      case 2:
	//  INITIAL WAIT
	if (!--door->topcountdown)
	{
	    switch(door->type)
	    {
	      case vld_raiseIn5Mins:
		door->direction = 1;
		door->type = vld_normal;
		S_StartSound(org, sfx_doropn);
		break;
		
	      default:
		break;
	    }
	}
	break;
	
      case -1:
	// DOWN
	res = T_MovePlane(door->sector,
			  FX8(door->speed8),
			  P_FloorHeight(door->sector),
			  false,1,door->direction);
	if (res == pastdest)
	{
	    switch(door->type)
	    {
	      case vld_blazeRaise:
	      case vld_blazeClose:
		P_SecOvl(door->sector)->mover = NOMOVER;
		P_RemoveMover (door);  // unlink and free
		S_StartSound(org, sfx_bdcls);
		break;
		
	      case vld_normal:
	      case vld_close:
		P_SecOvl(door->sector)->mover = NOMOVER;
		P_RemoveMover (door);  // unlink and free
		break;
		
	      case vld_close30ThenOpen:
		door->direction = 0;
		door->topcountdown = TICRATE*30;
		break;
		
	      default:
		break;
	    }
	}
	else if (res == crushed)
	{
	    switch(door->type)
	    {
	      case vld_blazeClose:
	      case vld_close:		// DO NOT GO BACK UP!
		break;
		
	      default:
		door->direction = 1;
		S_StartSound(org, sfx_doropn);
		break;
	    }
	}
	break;
	
      case 1:
	// UP
	res = T_MovePlane(door->sector,
			  FX8(door->speed8),
			  FX8(door->top8),
			  false,1,door->direction);
	
	if (res == pastdest)
	{
	    switch(door->type)
	    {
	      case vld_blazeRaise:
	      case vld_normal:
		door->direction = 0; // wait at top
		door->topcountdown = door->topwait;
		break;
		
	      case vld_close30ThenOpen:
	      case vld_blazeOpen:
	      case vld_open:
		P_SecOvl(door->sector)->mover = NOMOVER;
		P_RemoveMover (door);  // unlink and free
		break;
		
	      default:
		break;
	    }
	}
	break;
    }
}


//
// P_DoorKey
// The player has the card or the skull key of a lock (the skull follows
// its card by 3 in card_t), else the message and the oof sound.
//
static boolean P_DoorKey (player_t* p, int card, int msg)
{
    if (p->cards[card] || p->cards[card + 3])
	return true;
    p->message = P_String(msg);
    S_StartSound(NULL,sfx_oof);
    return false;
}


//
// EV_DoLockedDoor
// Move a locked door up/down
//

int
EV_DoLockedDoor
( lineidx_t	line,
  vldoor_e	type,
  mobj_t*	thing )
{
    player_t*	p;
    int		ok = 1;
	
    p = MO_PLAYER(thing);
	
    if (!p)
	return 0;
		
    switch(P_LineSpecial(line))
    {
      case 99:	// Blue Lock
      case 133:
	ok = P_DoorKey(p, it_bluecard, DMC_STR_PD_BLUEO);
	break;
	
      case 134: // Red Lock
      case 135:
	ok = P_DoorKey(p, it_redcard, DMC_STR_PD_REDO);
	break;
	
      case 136:	// Yellow Lock
      case 137:
	ok = P_DoorKey(p, it_yellowcard, DMC_STR_PD_YELLOWO);
	break;	
    }
    if (!ok)
	return 0;

    // all movers of the activation or none, after the key test
    if (!P_MoverReserveTagged(line))
	return 0;
    return EV_DoDoor(line,type);
}


int
EV_DoDoor
( lineidx_t	line,
  vldoor_e	type )
{
    int		secnum,rtn;
    secidx_t	sec;
    vldoor_t*	door;
    fixed_t	top;
    void*	org;
	
    secnum = -1;
    rtn = 0;
    
    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
	sec = secnum;
	if (P_SecMover(sec) != NOMOVER)
	    continue;
		
	
	// new door thinker
	rtn = 1;
	door = &P_NewMover (MV_DOOR, sec)->door;
	door->type = type;
	door->topwait = VDOORWAIT;
	door->speed8 = TO8(VDOORSPEED);
	org = P_SecSoundOrg(sec);
		
	switch(type)
	{
	  case vld_blazeClose:
	    top = P_FindLowestCeilingSurrounding(sec);
	    top -= 4*FRACUNIT;
	    door->top8 = TO8(top);
	    door->direction = -1;
	    door->speed8 = TO8(VDOORSPEED * 4);
	    S_StartSound(org, sfx_bdcls);
	    break;
	    
	  case vld_close:
	    top = P_FindLowestCeilingSurrounding(sec);
	    top -= 4*FRACUNIT;
	    door->top8 = TO8(top);
	    door->direction = -1;
	    S_StartSound(org, sfx_dorcls);
	    break;
	    
	  case vld_close30ThenOpen:
	    door->top8 = TO8(P_CeilingHeight(sec));
	    door->direction = -1;
	    S_StartSound(org, sfx_dorcls);
	    break;
	    
	  case vld_blazeRaise:
	  case vld_blazeOpen:
	    door->direction = 1;
	    top = P_FindLowestCeilingSurrounding(sec);
	    top -= 4*FRACUNIT;
	    door->top8 = TO8(top);
	    door->speed8 = TO8(VDOORSPEED * 4);
	    if (top != P_CeilingHeight(sec))
		S_StartSound(org, sfx_bdopn);
	    break;
	    
	  case vld_normal:
	  case vld_open:
	    door->direction = 1;
	    top = P_FindLowestCeilingSurrounding(sec);
	    top -= 4*FRACUNIT;
	    door->top8 = TO8(top);
	    if (top != P_CeilingHeight(sec))
		S_StartSound(org, sfx_doropn);
	    break;
	    
	  default:
	    break;
	}
		
    }
    return rtn;
}


//
// EV_VerticalDoor : open a door manually, no tag value
//
void
EV_VerticalDoor
( lineidx_t	line,
  mobj_t*	thing )
{
    player_t*	player;
    secidx_t	sec;
    vldoor_t*	door;
    mover_t*	m;
    int		side;
    void*	org;
	
    side = 0;	// only front sides can be used

    //	Check for locks
    player = MO_PLAYER(thing);
		
    switch(P_LineSpecial(line))
    {
      case 26: // Blue Lock
      case 32:
	if ( !player || !P_DoorKey(player, it_bluecard, DMC_STR_PD_BLUEK) )
	    return;
	break;
	
      case 27: // Yellow Lock
      case 34:
	if ( !player || !P_DoorKey(player, it_yellowcard, DMC_STR_PD_YELLOWK) )
	    return;
	break;
	
      case 28: // Red Lock
      case 33:
	if ( !player || !P_DoorKey(player, it_redcard, DMC_STR_PD_REDK) )
	    return;
	break;
    }
	
    // if the sector has an active thinker, use it
    sec = side ? P_LineFrontSector(line) : P_LineBackSector(line);
    if (sec == IDX_NONE)
	I_Error("EV_VerticalDoor: DR special type on 1-sided linedef");
    org = P_SecSoundOrg(sec);

    if (P_SecMover(sec) != NOMOVER)
    {
	m = &movers[P_SecMover(sec)];
	door = &m->door;
	switch(P_LineSpecial(line))
	{
	  case	1: // ONLY FOR "RAISE" DOORS, NOT "OPEN"s
	  case	26:
	  case	27:
	  case	28:
	  case	117:
	    // The mover may be no door. The oracle reads the field at
	    // the place of the door's direction, a plat's count in its
	    // 64-bit layout. It sets a moving plat's wait (Chocolate's
	    // choice), and a plat in stasis, whose thinker function is
	    // NULL, gets the door's direction there: its count. Floors
	    // and ceilings are left alone here.
	    if (m->door.kind == MV_PLAT)
	    {
		if (m->plat.count == -1)
		    m->plat.count = 1;
		else if (player)
		{
		    if (m->plat.status == in_stasis)
			m->plat.count = -1;
		    else
			m->plat.wait = -1;
		}
		return;
	    }
	    if (m->door.kind == MV_DOOR && door->direction == -1)
		door->direction = 1;	// go back up
	    else
	    {
		if (!player)
		    return;		// JDC: bad guys never close doors

		if (m->door.kind == MV_DOOR)
		    door->direction = -1;	// start going down immediately
	    }
	    return;
	}
    }
	
    // a new mover, reserved after the key test
    if (!P_MoverCheck(1))
	return;

    // for proper sound
    switch(P_LineSpecial(line))
    {
      case 117:	// BLAZING DOOR RAISE
      case 118:	// BLAZING DOOR OPEN
	S_StartSound(org,sfx_bdopn);
	break;
	
      case 1:	// NORMAL DOOR SOUND
      case 31:
	S_StartSound(org,sfx_doropn);
	break;
	
      default:	// LOCKED DOOR SOUND
	S_StartSound(org,sfx_doropn);
	break;
    }
	
    
    // new door thinker
    door = &P_NewMover (MV_DOOR, sec)->door;
    door->direction = 1;
    door->speed8 = TO8(VDOORSPEED);
    door->topwait = VDOORWAIT;

    switch(P_LineSpecial(line))
    {
      case 1:
      case 26:
      case 27:
      case 28:
	door->type = vld_normal;
	break;
	
      case 31:
      case 32:
      case 33:
      case 34:
	door->type = vld_open;
	P_SetLineSpecial(line, 0);
	break;
	
      case 117:	// blazing door raise
	door->type = vld_blazeRaise;
	door->speed8 = TO8(VDOORSPEED*4);
	break;
      case 118:	// blazing door open
	door->type = vld_blazeOpen;
	P_SetLineSpecial(line, 0);
	door->speed8 = TO8(VDOORSPEED*4);
	break;
    }
    
    // find the top and bottom of the movement range
    door->top8 = TO8(P_FindLowestCeilingSurrounding(sec) - 4*FRACUNIT);
}


