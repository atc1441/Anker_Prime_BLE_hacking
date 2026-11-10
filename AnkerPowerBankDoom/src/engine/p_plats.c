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
//	Plats (i.e. elevator platforms) code, raising/lowering.
//

#include <stdio.h>

#include "i_system.h"
#include "z_zone.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "sounds.h"


byte*		activeplats;



//
// Move a plat up and down
//
void T_PlatRaise(plat_t* plat)
{
    result_e	res;
    void*	org = P_SecSoundOrg(plat->sector);
	
    switch(plat->status)
    {
      case up:
	res = T_MovePlane(plat->sector,
			  FX8(plat->speed8),
			  FX8(plat->high8),
			  plat->crush,0,1);
					
	if (plat->type == raiseAndChange
	    || plat->type == raiseToNearestAndChange)
	{
	    if (!(leveltime&7))
		S_StartSound(org, sfx_stnmov);
	}
	
				
	if (res == crushed && (!plat->crush))
	{
	    plat->count = plat->wait;
	    plat->status = down;
	    S_StartSound(org, sfx_pstart);
	}
	else
	{
	    if (res == pastdest)
	    {
		plat->count = plat->wait;
		plat->status = waiting;
		S_StartSound(org, sfx_pstop);

		switch(plat->type)
		{
		  case blazeDWUS:
		  case downWaitUpStay:
		    P_RemoveActivePlat(plat);
		    break;
		    
		  case raiseAndChange:
		  case raiseToNearestAndChange:
		    P_RemoveActivePlat(plat);
		    break;
		    
		  default:
		    break;
		}
	    }
	}
	break;
	
      case	down:
	res = T_MovePlane(plat->sector,FX8(plat->speed8),FX8(plat->low8),false,0,-1);

	if (res == pastdest)
	{
	    plat->count = plat->wait;
	    plat->status = waiting;
	    S_StartSound(org,sfx_pstop);
	}
	break;
	
      case	waiting:
	if (!--plat->count)
	{
	    if (P_FloorHeight(plat->sector) == FX8(plat->low8))
		plat->status = up;
	    else
		plat->status = down;
	    S_StartSound(org,sfx_pstart);
	}
      case	in_stasis:
	break;
    }
}


//
// Do Platforms
//  "amount" is only used for SOME platforms.
//
int
EV_DoPlat
( lineidx_t	line,
  plattype_e	type,
  int		amount )
{
    plat_t*	plat;
    int		secnum;
    int		rtn;
    secidx_t	sec;
    secovl_t*	sp;
    fixed_t	low;
    fixed_t	high;
    void*	org;
	
    secnum = -1;
    rtn = 0;

    
    //	Activate all <type> plats that are in_stasis
    switch(type)
    {
      case perpetualRaise:
	P_ActivateInStasis(P_LineTag(line));
	break;
	
      default:
	break;
    }
	
    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
	sec = secnum;

	if (P_SecMover(sec) != NOMOVER)
	    continue;
	
	// Find lowest & highest floors around sector
	rtn = 1;
	plat = &P_NewMover(MV_PLAT, sec)->plat;
	sp = P_SecOvl(sec);
	org = P_SecSoundOrg(sec);
		
	plat->type = type;
	plat->crush = false;
	plat->tag = P_LineTag(line);
	
	switch(type)
	{
	  case raiseToNearestAndChange:
	    plat->speed8 = TO8(PLATSPEED/2);
	    sp->floorpic = P_FloorPic(P_LineFrontSector(line));
	    plat->high8 = TO8(P_FindNextHighestFloor(sec,FX8(sp->floor8)));
	    plat->wait = 0;
	    plat->status = up;
	    // NO MORE DAMAGE, IF APPLICABLE
	    sp->special = 0;		

	    S_StartSound(org,sfx_stnmov);
	    break;
	    
	  case raiseAndChange:
	    plat->speed8 = TO8(PLATSPEED/2);
	    sp->floorpic = P_FloorPic(P_LineFrontSector(line));
	    plat->high8 = TO8(FX8(sp->floor8) + amount*FRACUNIT);
	    plat->wait = 0;
	    plat->status = up;

	    S_StartSound(org,sfx_stnmov);
	    break;
	    
	  case downWaitUpStay:
	    plat->speed8 = TO8(PLATSPEED * 4);
	    low = P_FindLowestFloorSurrounding(sec);

	    if (low > FX8(sp->floor8))
		low = FX8(sp->floor8);

	    plat->low8 = TO8(low);
	    plat->high8 = sp->floor8;
	    plat->wait = TICRATE*PLATWAIT;
	    plat->status = down;
	    S_StartSound(org,sfx_pstart);
	    break;
	    
	  case blazeDWUS:
	    plat->speed8 = TO8(PLATSPEED * 8);
	    low = P_FindLowestFloorSurrounding(sec);

	    if (low > FX8(sp->floor8))
		low = FX8(sp->floor8);

	    plat->low8 = TO8(low);
	    plat->high8 = sp->floor8;
	    plat->wait = TICRATE*PLATWAIT;
	    plat->status = down;
	    S_StartSound(org,sfx_pstart);
	    break;
	    
	  case perpetualRaise:
	    plat->speed8 = TO8(PLATSPEED);
	    low = P_FindLowestFloorSurrounding(sec);

	    if (low > FX8(sp->floor8))
		low = FX8(sp->floor8);

	    high = P_FindHighestFloorSurrounding(sec);

	    if (high < FX8(sp->floor8))
		high = FX8(sp->floor8);

	    plat->low8 = TO8(low);
	    plat->high8 = TO8(high);
	    plat->wait = TICRATE*PLATWAIT;
	    plat->status = P_Random()&1;

	    S_StartSound(org,sfx_pstart);
	    break;
	}
	P_AddActivePlat(plat);
    }
    return rtn;
}



void P_ActivateInStasis(int tag)
{
    int		i;
    plat_t*	plat;
	
    for (i = 0;i < MAXPLATS;i++)
    {
	if (activeplats[i] == NOMOVER)
	    continue;
	plat = &movers[activeplats[i]].plat;
	if (plat->tag == tag
	    && plat->status == in_stasis)
	{
	    plat->status = plat->oldstatus;
	}
    }
}

void EV_StopPlat(lineidx_t line)
{
    int		j;
    int		tag = P_LineTag(line);
    plat_t*	plat;
	
    for (j = 0;j < MAXPLATS;j++)
    {
	if (activeplats[j] == NOMOVER)
	    continue;
	plat = &movers[activeplats[j]].plat;
	if (plat->status != in_stasis
	    && plat->tag == tag)
	{
	    plat->oldstatus = plat->status;
	    plat->status = in_stasis;
	}
    }
}

void P_AddActivePlat(plat_t* plat)
{
    int		i;
    
    for (i = 0;i < MAXPLATS;i++)
	if (activeplats[i] == NOMOVER)
	{
	    activeplats[i] = P_MoverSlot(plat);
	    return;
	}
    I_Error ("P_AddActivePlat: no more plats!");
}

void P_RemoveActivePlat(plat_t* plat)
{
    int		i;
    for (i = 0;i < MAXPLATS;i++)
	if (activeplats[i] == P_MoverSlot(plat))
	{
	    P_SecOvl(plat->sector)->mover = NOMOVER;
	    P_RemoveMover(plat);
	    activeplats[i] = NOMOVER;
	    
	    return;
	}
    I_Error ("P_RemoveActivePlat: can't find plat!");
}
