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
//	Handle Sector base lighting effects.
//	Muzzle flash?
//



#include "z_zone.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"


// State.
#include "r_state.h"

#include "things.h"

// Light thinkers are 8 B slots (light_t), linked at the end of the
// thinker list by P_NewLight. The level start lights come first.

void P_SetMinMax (light_t* l, int min, int max)
{
    l->minlight = min;
    l->maxlight = max;
    l->kind |= ((min >> 8) & 1) * LT_MINHI | ((max >> 8) & 1) * LT_MAXHI;
}


//
// FIRELIGHT FLICKER
//

//
// T_FireFlicker
//
void T_FireFlicker (light_t* flick)
{
    int	amount;
	
    if (--flick->count)
	return;
	
    amount = (P_Random()&3)*16;
    
    if (P_LightLevel(flick->sector) - amount < LT_MIN(flick))
	P_SetLightLevel(flick->sector, LT_MIN(flick));
    else
	P_SetLightLevel(flick->sector, LT_MAX(flick) - amount);

    flick->count = 4;
}



//
// BROKEN LIGHT FLASHING
//


//
// T_LightFlash
// Do flashing lights.
//
void T_LightFlash (light_t* flash)
{
    if (--flash->count)
	return;
	
    if (P_LightLevel(flash->sector) == LT_MAX(flash))
    {
	P_SetLightLevel(flash->sector, LT_MIN(flash));
	flash->count = (P_Random()&7)+1;
    }
    else
    {
	P_SetLightLevel(flash->sector, LT_MAX(flash));
	flash->count = (P_Random()&64)+1;
    }

}




//
// STROBE LIGHT FLASHING
//


//
// T_StrobeFlash
//
void T_StrobeFlash (light_t*		flash)
{
    if (--flash->count)
	return;
	
    if (P_LightLevel(flash->sector) == LT_MIN(flash))
    {
	P_SetLightLevel(flash->sector, LT_MAX(flash));
	flash->count = STROBEBRIGHT;
    }
    else
    {
	P_SetLightLevel(flash->sector, LT_MIN(flash));
	flash->count = flash->kind & LT_SLOW ? SLOWDARK : FASTDARK;
    }

}



//
// P_SpawnStrobeFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
//
void
P_SpawnStrobeFlash
( secidx_t	sector,
  int		fastOrSlow,
  int		inSync )
{
    light_t*	flash;
    int		light = P_LightLevel(sector);
    int		min;
	
    flash = P_NewLight(LT_STROBE | (fastOrSlow == SLOWDARK ? LT_SLOW : 0), sector);
    min = P_FindMinSurroundingLight(sector, light);
		
    if (min == light)
	min = 0;
    P_SetMinMax(flash, min, light);

    // nothing special about it during gameplay
    P_SecOvl(sector)->special = 0;	

    if (!inSync)
	flash->count = (P_Random()&7)+1;
    else
	flash->count = 1;
}


//
// Start strobing lights (usually from a trigger)
//
void EV_StartLightStrobing(lineidx_t	line)
{
    int		secnum;
	
    secnum = -1;
    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
	if (P_SecMover(secnum) != NOMOVER)
	    continue;
	
	P_SpawnStrobeFlash (secnum,SLOWDARK, 0);
    }
}



//
// TURN LINE'S TAG LIGHTS OFF
//
void EV_TurnTagLightsOff(lineidx_t line)
{
    int			i;
    int			j;
    int			min;
    int			first;
    int			count;
    int			tag;
    dmc_range_t		r;
    secidx_t		sector;
    secidx_t		tsec;
	
    // the sectors of the line's tag, in sector order
    tag = P_LineTag(line);
    for (j = 0; tag && j < P_TagSectors(tag, &r); j++)
    {
	sector = P_TagSector(tag, &r, j);
	{
	    min = P_LightLevel(sector);
	    count = P_SecLines(sector, &first);
	    for (i = 0;i < count; i++)
	    {
		tsec = getNextSector(P_SecLine(first + i),sector);
		if (tsec == IDX_NONE)
		    continue;
		if (P_LightLevel(tsec) < min)
		    min = P_LightLevel(tsec);
	    }
	    P_SetLightLevel(sector, min);
	}
    }
}


//
// TURN LINE'S TAG LIGHTS ON
//
void
EV_LightTurnOn
( lineidx_t	line,
  int		bright )
{
    int		i;
    int		j;
    int		first;
    int		count;
    int		tag;
    dmc_range_t	r;
    secidx_t	sector;
    secidx_t	temp;
	
    // the sectors of the line's tag, in sector order
    tag = P_LineTag(line);
    for (i = 0; tag && i < P_TagSectors(tag, &r); i++)
    {
	sector = P_TagSector(tag, &r, i);
	{
	    // bright = 0 means to search
	    // for highest light level
	    // surrounding sector
	    if (!bright)
	    {
		count = P_SecLines(sector, &first);
		for (j = 0;j < count; j++)
		{
		    temp = getNextSector(P_SecLine(first + j),sector);

		    if (temp == IDX_NONE)
			continue;

		    if (P_LightLevel(temp) > bright)
			bright = P_LightLevel(temp);
		}
	    }
	    P_SetLightLevel(sector, bright);
	}
    }
}

    
//
// Spawn glowing light
//

void T_Glow(light_t*	g)
{
    int		light = P_LightLevel(g->sector);

    if (g->kind & LT_UP)
    {
	// UP
	light += GLOWSPEED;
	if (light >= LT_MAX(g))
	{
	    light -= GLOWSPEED;
	    g->kind &= ~LT_UP;
	}
    }
    else
    {
	// DOWN
	light -= GLOWSPEED;
	if (light <= LT_MIN(g))
	{
	    light += GLOWSPEED;
	    g->kind |= LT_UP;
	}
    }
    P_SetLightLevel(g->sector, light);
}


