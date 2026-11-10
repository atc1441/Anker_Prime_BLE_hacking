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
// DESCRIPTION:  Ceiling aninmation (lowering, crushing, raising)
//



#include "z_zone.h"
#include "doomdef.h"
#include "p_local.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "sounds.h"

//
// CEILINGS
//


byte*		activeceilings;


//
// T_MoveCeiling
//

void T_MoveCeiling (ceiling_t* ceiling)
{
    result_e	res;
    void*	org = P_SecSoundOrg(ceiling->sector);
	
    switch(ceiling->direction)
    {
      case 0:
	// IN STASIS
	break;
      case 1:
	// UP
	res = T_MovePlane(ceiling->sector,
			  FX8(ceiling->speed8),
			  FX8(ceiling->top8),
			  false,1,ceiling->direction);
	
	if (!(leveltime&7))
	{
	    switch(ceiling->type)
	    {
	      case silentCrushAndRaise:
		break;
	      default:
		S_StartSound(org, sfx_stnmov);
		// ?
		break;
	    }
	}
	
	if (res == pastdest)
	{
	    switch(ceiling->type)
	    {
	      case raiseToHighest:
		P_RemoveActiveCeiling(ceiling);
		break;
		
	      case silentCrushAndRaise:
		S_StartSound(org, sfx_pstop);
	      case fastCrushAndRaise:
	      case crushAndRaise:
		ceiling->direction = -1;
		break;
		
	      default:
		break;
	    }
	    
	}
	break;
	
      case -1:
	// DOWN
	res = T_MovePlane(ceiling->sector,
			  FX8(ceiling->speed8),
			  FX8(ceiling->bottom8),
			  ceiling->crush,1,ceiling->direction);
	
	if (!(leveltime&7))
	{
	    switch(ceiling->type)
	    {
	      case silentCrushAndRaise: break;
	      default:
		S_StartSound(org, sfx_stnmov);
	    }
	}
	
	if (res == pastdest)
	{
	    switch(ceiling->type)
	    {
	      case silentCrushAndRaise:
		S_StartSound(org, sfx_pstop);
	      case crushAndRaise:
		ceiling->speed8 = TO8(CEILSPEED);
	      case fastCrushAndRaise:
		ceiling->direction = 1;
		break;

	      case lowerAndCrush:
	      case lowerToFloor:
		P_RemoveActiveCeiling(ceiling);
		break;

	      default:
		break;
	    }
	}
	else // ( res != pastdest )
	{
	    if (res == crushed)
	    {
		switch(ceiling->type)
		{
		  case silentCrushAndRaise:
		  case crushAndRaise:
		  case lowerAndCrush:
		    ceiling->speed8 = TO8(CEILSPEED / 8);
		    break;

		  default:
		    break;
		}
	    }
	}
	break;
    }
}


//
// EV_DoCeiling
// Move a ceiling up/down and all around!
//
int
EV_DoCeiling
( lineidx_t	line,
  ceiling_e	type )
{
    int		secnum;
    int		rtn;
    secidx_t	sec;
    ceiling_t*	ceiling;
    fixed_t	bottom;
	
    secnum = -1;
    rtn = 0;
    
    //	Reactivate in-stasis ceilings...for certain types.
    switch(type)
    {
      case fastCrushAndRaise:
      case silentCrushAndRaise:
      case crushAndRaise:
	P_ActivateInStasisCeiling(line);
      default:
	break;
    }
	
    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
	sec = secnum;
	if (P_SecMover(sec) != NOMOVER)
	    continue;
	
	// new door thinker
	rtn = 1;
	ceiling = &P_NewMover (MV_CEILING, sec)->ceiling;
	ceiling->crush = false;
	
	switch(type)
	{
	  case fastCrushAndRaise:
	    ceiling->crush = true;
	    ceiling->top8 = TO8(P_CeilingHeight(sec));
	    ceiling->bottom8 = TO8(P_FloorHeight(sec) + (8*FRACUNIT));
	    ceiling->direction = -1;
	    ceiling->speed8 = TO8(CEILSPEED * 2);
	    break;

	  case silentCrushAndRaise:
	  case crushAndRaise:
	    ceiling->crush = true;
	    ceiling->top8 = TO8(P_CeilingHeight(sec));
	  case lowerAndCrush:
	  case lowerToFloor:
	    bottom = P_FloorHeight(sec);
	    if (type != lowerToFloor)
		bottom += 8*FRACUNIT;
	    ceiling->bottom8 = TO8(bottom);
	    ceiling->direction = -1;
	    ceiling->speed8 = TO8(CEILSPEED);
	    break;

	  case raiseToHighest:
	    ceiling->top8 = TO8(P_FindHighestCeilingSurrounding(sec));
	    ceiling->direction = 1;
	    ceiling->speed8 = TO8(CEILSPEED);
	    break;
	}
		
	ceiling->tag = P_SecTag(sec);
	ceiling->type = type;
	P_AddActiveCeiling(ceiling);
    }
    return rtn;
}


//
// Add an active ceiling
//
void P_AddActiveCeiling(ceiling_t* c)
{
    int		i;
    
    for (i = 0; i < MAXCEILINGS;i++)
    {
	if (activeceilings[i] == NOMOVER)
	{
	    activeceilings[i] = P_MoverSlot(c);
	    return;
	}
    }
}



//
// Remove a ceiling's thinker
//
void P_RemoveActiveCeiling(ceiling_t* c)
{
    int		i;
	
    for (i = 0;i < MAXCEILINGS;i++)
    {
	if (activeceilings[i] == P_MoverSlot(c))
	{
	    P_SecOvl(c->sector)->mover = NOMOVER;
	    P_RemoveMover (c);
	    activeceilings[i] = NOMOVER;
	    break;
	}
    }
}



//
// Restart a ceiling that's in-stasis
//
void P_ActivateInStasisCeiling(lineidx_t line)
{
    int		i;
    int		tag = P_LineTag(line);
    ceiling_t*	c;
	
    for (i = 0;i < MAXCEILINGS;i++)
    {
	if (activeceilings[i] == NOMOVER)
	    continue;
	c = &movers[activeceilings[i]].ceiling;
	if (c->tag == tag
	    && c->direction == 0)
	{
	    c->direction = c->olddirection;
	}
    }
}



//
// EV_CeilingCrushStop
// Stop a ceiling from crushing!
//
int	EV_CeilingCrushStop(lineidx_t	line)
{
    int		i;
    int		rtn;
    int		tag = P_LineTag(line);
    ceiling_t*	c;
	
    rtn = 0;
    for (i = 0;i < MAXCEILINGS;i++)
    {
	if (activeceilings[i] == NOMOVER)
	    continue;
	c = &movers[activeceilings[i]].ceiling;
	if (c->tag == tag
	    && c->direction != 0)
	{
	    c->olddirection = c->direction;
	    c->direction = 0;		// in-stasis
	    rtn = 1;
	}
    }
    

    return rtn;
}
