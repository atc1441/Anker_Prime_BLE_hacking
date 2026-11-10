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
//	Implements special effects:
//	Texture animation, height or lighting changes
//	 according to adjacent sectors, respective
//	 utility functions, etc.
//	Line Tag handling. Line and Sector triggers.
//


#include <stdlib.h>

#include "doomdef.h"
#include "doomstat.h"

#include "deh_main.h"
#include "i_system.h"
#include "z_zone.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "fetch.h"

#include "r_local.h"
#include "p_local.h"

#include "g_game.h"

#include "s_sound.h"

// State.
#include "r_state.h"

// Data.
#include "sounds.h"

#include "things.h"




//
// UTILITIES
//



//
// getSide()
// Will return the side index
//  given the number of the current sector,
//  the line number, and the side (0/1) that you want.
//
sideidx_t
getSide
( int		currentSector,
  int		line,
  int		side )
{
    int		first;

    P_SecLines(currentSector, &first);
    return P_LineSide(P_SecLine(first + line), side);
}


//
// getSector()
// Will return the sector index
//  given the number of the current sector,
//  the line number and the side (0/1) that you want.
//
secidx_t
getSector
( int		currentSector,
  int		line,
  int		side )
{
    int		first;
    lineidx_t	l;

    P_SecLines(currentSector, &first);
    l = P_SecLine(first + line);
    return side ? P_LineBackSector(l) : P_LineFrontSector(l);
}


//
// twoSided()
// Given the sector number and the line number,
//  it will tell you whether the line is two-sided or not.
//
int
twoSided
( int	sector,
  int	line )
{
    int		first;
    line_t	l;

    P_SecLines(sector, &first);
    P_GetLine(P_SecLine(first + line), &l);
    return l.flags & ML_TWOSIDED;
}




//
// getNextSector()
// Return the sector next to current.
// IDX_NONE if not two-sided line
//
secidx_t
getNextSector
( lineidx_t	line,
  secidx_t	sec )
{
    line_t	l;

    P_GetLine(line, &l);
    if (!(l.flags & ML_TWOSIDED))
	return IDX_NONE;
		
    if (l.frontsector == sec)
	return l.backsector;
	
    return l.frontsector;
}



//
// P_FindLowestFloorSurrounding()
// FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
fixed_t	P_FindLowestFloorSurrounding(secidx_t sec)
{
    int			i;
    int			first;
    int			count;
    secidx_t		other;
    fixed_t		floor = P_FloorHeight(sec);
	
    count = P_SecLines(sec, &first);
    for (i=0 ;i < count ; i++)
    {
	other = getNextSector(P_SecLine(first + i),sec);

	if (other == IDX_NONE)
	    continue;
	
	if (P_FloorHeight(other) < floor)
	    floor = P_FloorHeight(other);
    }
    return floor;
}



//
// P_FindHighestFloorSurrounding()
// FIND HIGHEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
fixed_t	P_FindHighestFloorSurrounding(secidx_t sec)
{
    int			i;
    int			first;
    int			count;
    secidx_t		other;
    fixed_t		floor = -500*FRACUNIT;
	
    count = P_SecLines(sec, &first);
    for (i=0 ;i < count ; i++)
    {
	other = getNextSector(P_SecLine(first + i),sec);
	
	if (other == IDX_NONE)
	    continue;
	
	if (P_FloorHeight(other) > floor)
	    floor = P_FloorHeight(other);
    }
    return floor;
}



//
// P_FindNextHighestFloor
// FIND NEXT HIGHEST FLOOR IN SURROUNDING SECTORS
// Note: this should be doable w/o a fixed array.

// Thanks to entryway for the Vanilla overflow emulation.

// 20 adjoining sectors max!
#define MAX_ADJOINING_SECTORS     20

fixed_t
P_FindNextHighestFloor
( secidx_t sec,
  int       currentheight )
{
    int         i;
    int         h;
    int         min;
    int         first;
    int         count;
    secidx_t    other;
    fixed_t     height = currentheight;
    fixed_t     heightlist[MAX_ADJOINING_SECTORS + 2];

    count = P_SecLines(sec, &first);
    for (i=0, h=0; i < count; i++)
    {
        other = getNextSector(P_SecLine(first + i),sec);

        if (other == IDX_NONE)
            continue;
        
        if (P_FloorHeight(other) > height)
        {
            // Emulation of memory (stack) overflow
            if (h == MAX_ADJOINING_SECTORS + 1)
            {
                height = P_FloorHeight(other);
            }
            else if (h == MAX_ADJOINING_SECTORS + 2)
            {
                // Fatal overflow: game crashes at 22 textures
                I_Error("Sector with more than 22 adjoining sectors. "
                        "Vanilla will crash here");
            }

            heightlist[h++] = P_FloorHeight(other);
        }
    }
    
    // Find lowest height in list
    if (!h)
    {
        return currentheight;
    }
        
    min = heightlist[0];
    
    // Range checking? 
    for (i = 1; i < h; i++)
    {
        if (heightlist[i] < min)
        {
            min = heightlist[i];
        }
    }

    return min;
}

//
// FIND LOWEST CEILING IN THE SURROUNDING SECTORS
//
fixed_t
P_FindLowestCeilingSurrounding(secidx_t sec)
{
    int			i;
    int			first;
    int			count;
    secidx_t		other;
    fixed_t		height = INT_MAX;
	
    count = P_SecLines(sec, &first);
    for (i=0 ;i < count ; i++)
    {
	other = getNextSector(P_SecLine(first + i),sec);

	if (other == IDX_NONE)
	    continue;

	if (P_CeilingHeight(other) < height)
	    height = P_CeilingHeight(other);
    }
    return height;
}


//
// FIND HIGHEST CEILING IN THE SURROUNDING SECTORS
//
fixed_t	P_FindHighestCeilingSurrounding(secidx_t sec)
{
    int		i;
    int		first;
    int		count;
    secidx_t	other;
    fixed_t	height = 0;
	
    count = P_SecLines(sec, &first);
    for (i=0 ;i < count ; i++)
    {
	other = getNextSector(P_SecLine(first + i),sec);

	if (other == IDX_NONE)
	    continue;

	if (P_CeilingHeight(other) > height)
	    height = P_CeilingHeight(other);
    }
    return height;
}



//
// RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
//
int
P_FindSectorFromTag
( int		tag,
  int		start )
{
    dmc_range_t	r;
    int		i;
    int		s;
    int		count;

    // The tag lists hold the sectors of a tag in ascending order.
    // Specials that use tags never have tag 0 (the converter checks).
    if (!tag)
	return -1;
    count = P_TagSectors(tag, &r);
    for (i=0 ; i<count ; i++)
    {
	s = P_TagSector(tag, &r, i);
	if (s > start)
	    return s;
    }
    
    return -1;
}

int
P_FindSectorFromLineTag
( lineidx_t	line,
  int		start )
{
    return P_FindSectorFromTag(P_LineTag(line), start);
}




//
// Find minimum light from an adjacent sector
//
int
P_FindMinSurroundingLight
( secidx_t	sector,
  int		max )
{
    int		i;
    int		min;
    int		first;
    int		count;
    secidx_t	check;
	
    min = max;
    count = P_SecLines(sector, &first);
    for (i=0 ; i < count ; i++)
    {
	check = getNextSector(P_SecLine(first + i),sector);

	if (check == IDX_NONE)
	    continue;

	if (P_LightLevel(check) < min)
	    min = P_LightLevel(check);
    }
    return min;
}



//
// MOVER RESERVATION
// An activation gets all of its movers or none: the
// count of sectors it would start moving is checked against the free
// mover slots first. A deferred activation keeps its line special and
// is a counted event.
//
static int P_TaggedFree (lineidx_t line)
{
    int		secnum = -1;
    int		n = 0;

    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
	n += P_SecMover(secnum) == NOMOVER;
    return n;
}

// n more movers fit, or the activation is deferred (a counted event)
boolean P_MoverCheck (int n)
{
    if (n <= P_MoversFree())
	return true;
    O_EVENT(EV_MOVER_DEFERRED);
    return false;
}

boolean P_MoverReserveTagged (lineidx_t line)
{
    return P_MoverCheck(P_TaggedFree(line));
}

// Specials that start no mover (lights 12 13 17 35 79-81 104 138 139,
// teleports 39 97 125 126, exits 11 51 52 124, stops 54 57 74 89,
// scrolling 48), and doors that reserve after their key test (manual
// 1 26-28 31-34 117 118 in EV_VerticalDoor, locked 99 133-137 in
// EV_DoLockedDoor): one bit per special.
static const byte noreserve[18] =
{
    0x02, 0x38, 0x02, 0x9c, 0x8f, 0x00, 0x59, 0x02, 0x00,
    0x84, 0x03, 0x02, 0x0a, 0x01, 0x60, 0x70, 0xe0, 0x0f
};

boolean P_MoverReserve (lineidx_t line, int special)
{
    int		n;

    if (special < 8 * 18 && (noreserve[special >> 3] >> (special & 7) & 1))
	return true;
    switch (special)
    {
      case 7: case 8:
	n = P_StairDemand(line, build8);
	break;
      case 100: case 127:
	n = P_StairDemand(line, turbo16);
	break;

      // a donut moves two sectors per tagged one, at most
      case 9:
	n = 2 * P_TaggedFree(line);
	break;

      default:
	n = P_TaggedFree(line);
	break;
    }
    return P_MoverCheck(n);
}


//
// EVENTS
// Events are operations triggered by using, crossing,
// or shooting special lines, or by timed thinkers.
//

//
// P_CrossSpecialLine - TRIGGER
// Called every time a thing origin is about
//  to cross a line with a non 0 special.
//
void
P_CrossSpecialLine
( int		linenum,
  int		side,
  mobj_t*	thing )
{
    lineidx_t	line;
    int		ok;

    line = linenum;
#ifdef DMC_STATS
    HS_Event (HS_E_CROSS, thing);
#endif
    
    //	Triggers that other things can activate
    if (!MO_PLAYER(thing))
    {
	// Things that should NOT trigger specials...
	// (the BFG and the cacodemon's shot are not in E1)
	switch(thing->type)
	{
	  case MT_ROCKET:
#ifdef DMC_HAVE_MT_PLASMA
	  case MT_PLASMA:
#endif
	  case MT_TROOPSHOT:
	  case MT_BRUISERSHOT:
	    return;
	    break;
	    
	  default: break;
	}
		
	ok = 0;
	switch(P_LineSpecial(line))
	{
	  case 39:	// TELEPORT TRIGGER
	  case 97:	// TELEPORT RETRIGGER
	  case 125:	// TELEPORT MONSTERONLY TRIGGER
	  case 126:	// TELEPORT MONSTERONLY RETRIGGER
	  case 4:	// RAISE DOOR
	  case 10:	// PLAT DOWN-WAIT-UP-STAY TRIGGER
	  case 88:	// PLAT DOWN-WAIT-UP-STAY RETRIGGER
	    ok = 1;
	    break;
	}
	if (!ok)
	    return;
    }
#ifdef DMC_STATS
    HS_Event (HS_E_HANDLED, thing);
#endif

    // all movers of the activation or none: the special stays
    if (!P_MoverReserve(line, P_LineSpecial(line)))
	return;
    
    // Note: could use some const's here.
    switch (P_LineSpecial(line))
    {
	// TRIGGERS.
	// All from here to RETRIGGERS.
      case 2:
	// Open Door
	EV_DoDoor(line,vld_open);
	P_SetLineSpecial(line, 0);
	break;

      case 3:
	// Close Door
	EV_DoDoor(line,vld_close);
	P_SetLineSpecial(line, 0);
	break;

      case 4:
	// Raise Door
	EV_DoDoor(line,vld_normal);
	P_SetLineSpecial(line, 0);
	break;
	
      case 5:
	// Raise Floor
	EV_DoFloor(line,raiseFloor);
	P_SetLineSpecial(line, 0);
	break;
	
      case 6:
	// Fast Ceiling Crush & Raise
	EV_DoCeiling(line,fastCrushAndRaise);
	P_SetLineSpecial(line, 0);
	break;
	
      case 8:
	// Build Stairs
	EV_BuildStairs(line,build8);
	P_SetLineSpecial(line, 0);
	break;
	
      case 10:
	// PlatDownWaitUp
	EV_DoPlat(line,downWaitUpStay,0);
	P_SetLineSpecial(line, 0);
	break;
	
      case 12:
	// Light Turn On - brightest near
	EV_LightTurnOn(line,0);
	P_SetLineSpecial(line, 0);
	break;
	
      case 13:
	// Light Turn On 255
	EV_LightTurnOn(line,255);
	P_SetLineSpecial(line, 0);
	break;
	
      case 16:
	// Close Door 30
	EV_DoDoor(line,vld_close30ThenOpen);
	P_SetLineSpecial(line, 0);
	break;
	
      case 17:
	// Start Light Strobing
	EV_StartLightStrobing(line);
	P_SetLineSpecial(line, 0);
	break;
	
      case 19:
	// Lower Floor
	EV_DoFloor(line,lowerFloor);
	P_SetLineSpecial(line, 0);
	break;
	
      case 22:
	// Raise floor to nearest height and change texture
	EV_DoPlat(line,raiseToNearestAndChange,0);
	P_SetLineSpecial(line, 0);
	break;
	
      case 25:
	// Ceiling Crush and Raise
	EV_DoCeiling(line,crushAndRaise);
	P_SetLineSpecial(line, 0);
	break;
	
      case 30:
	// Raise floor to shortest texture height
	//  on either side of lines.
	EV_DoFloor(line,raiseToTexture);
	P_SetLineSpecial(line, 0);
	break;
	
      case 35:
	// Lights Very Dark
	EV_LightTurnOn(line,35);
	P_SetLineSpecial(line, 0);
	break;
	
      case 36:
	// Lower Floor (TURBO)
	EV_DoFloor(line,turboLower);
	P_SetLineSpecial(line, 0);
	break;
	
      case 37:
	// LowerAndChange
	EV_DoFloor(line,lowerAndChange);
	P_SetLineSpecial(line, 0);
	break;
	
      case 38:
	// Lower Floor To Lowest
	EV_DoFloor( line, lowerFloorToLowest );
	P_SetLineSpecial(line, 0);
	break;
	
      case 39:
	// TELEPORT!
	EV_Teleport( line, side, thing );
	P_SetLineSpecial(line, 0);
	break;

      case 40:
	// RaiseCeilingLowerFloor
	EV_DoCeiling( line, raiseToHighest );
	EV_DoFloor( line, lowerFloorToLowest );
	P_SetLineSpecial(line, 0);
	break;
	
      case 44:
	// Ceiling Crush
	EV_DoCeiling( line, lowerAndCrush );
	P_SetLineSpecial(line, 0);
	break;
	
      case 52:
	// EXIT!
	G_ExitLevel ();
	break;
	
      case 53:
	// Perpetual Platform Raise
	EV_DoPlat(line,perpetualRaise,0);
	P_SetLineSpecial(line, 0);
	break;
	
      case 54:
	// Platform Stop
	EV_StopPlat(line);
	P_SetLineSpecial(line, 0);
	break;

      case 56:
	// Raise Floor Crush
	EV_DoFloor(line,raiseFloorCrush);
	P_SetLineSpecial(line, 0);
	break;

      case 57:
	// Ceiling Crush Stop
	EV_CeilingCrushStop(line);
	P_SetLineSpecial(line, 0);
	break;
	
      case 58:
	// Raise Floor 24
	EV_DoFloor(line,raiseFloor24);
	P_SetLineSpecial(line, 0);
	break;

      case 59:
	// Raise Floor 24 And Change
	EV_DoFloor(line,raiseFloor24AndChange);
	P_SetLineSpecial(line, 0);
	break;
	
      case 104:
	// Turn lights off in sector(tag)
	EV_TurnTagLightsOff(line);
	P_SetLineSpecial(line, 0);
	break;
	
      case 108:
	// Blazing Door Raise (faster than TURBO!)
	EV_DoDoor (line,vld_blazeRaise);
	P_SetLineSpecial(line, 0);
	break;
	
      case 109:
	// Blazing Door Open (faster than TURBO!)
	EV_DoDoor (line,vld_blazeOpen);
	P_SetLineSpecial(line, 0);
	break;
	
      case 100:
	// Build Stairs Turbo 16
	EV_BuildStairs(line,turbo16);
	P_SetLineSpecial(line, 0);
	break;
	
      case 110:
	// Blazing Door Close (faster than TURBO!)
	EV_DoDoor (line,vld_blazeClose);
	P_SetLineSpecial(line, 0);
	break;

      case 119:
	// Raise floor to nearest surr. floor
	EV_DoFloor(line,raiseFloorToNearest);
	P_SetLineSpecial(line, 0);
	break;
	
      case 121:
	// Blazing PlatDownWaitUpStay
	EV_DoPlat(line,blazeDWUS,0);
	P_SetLineSpecial(line, 0);
	break;
	
      case 124:
	// Secret EXIT
	G_SecretExitLevel ();
	break;
		
      case 125:
	// TELEPORT MonsterONLY
	if (!MO_PLAYER(thing))
	{
	    EV_Teleport( line, side, thing );
	    P_SetLineSpecial(line, 0);
	}
	break;
	
      case 130:
	// Raise Floor Turbo
	EV_DoFloor(line,raiseFloorTurbo);
	P_SetLineSpecial(line, 0);
	break;
	
      case 141:
	// Silent Ceiling Crush & Raise
	EV_DoCeiling(line,silentCrushAndRaise);
	P_SetLineSpecial(line, 0);
	break;
	
	// RETRIGGERS.  All from here till end.
      case 72:
	// Ceiling Crush
	EV_DoCeiling( line, lowerAndCrush );
	break;

      case 73:
	// Ceiling Crush and Raise
	EV_DoCeiling(line,crushAndRaise);
	break;

      case 74:
	// Ceiling Crush Stop
	EV_CeilingCrushStop(line);
	break;
	
      case 75:
	// Close Door
	EV_DoDoor(line,vld_close);
	break;
	
      case 76:
	// Close Door 30
	EV_DoDoor(line,vld_close30ThenOpen);
	break;
	
      case 77:
	// Fast Ceiling Crush & Raise
	EV_DoCeiling(line,fastCrushAndRaise);
	break;
	
      case 79:
	// Lights Very Dark
	EV_LightTurnOn(line,35);
	break;
	
      case 80:
	// Light Turn On - brightest near
	EV_LightTurnOn(line,0);
	break;
	
      case 81:
	// Light Turn On 255
	EV_LightTurnOn(line,255);
	break;
	
      case 82:
	// Lower Floor To Lowest
	EV_DoFloor( line, lowerFloorToLowest );
	break;
	
      case 83:
	// Lower Floor
	EV_DoFloor(line,lowerFloor);
	break;

      case 84:
	// LowerAndChange
	EV_DoFloor(line,lowerAndChange);
	break;

      case 86:
	// Open Door
	EV_DoDoor(line,vld_open);
	break;
	
      case 87:
	// Perpetual Platform Raise
	EV_DoPlat(line,perpetualRaise,0);
	break;
	
      case 88:
	// PlatDownWaitUp
	EV_DoPlat(line,downWaitUpStay,0);
	break;
	
      case 89:
	// Platform Stop
	EV_StopPlat(line);
	break;
	
      case 90:
	// Raise Door
	EV_DoDoor(line,vld_normal);
	break;
	
      case 91:
	// Raise Floor
	EV_DoFloor(line,raiseFloor);
	break;
	
      case 92:
	// Raise Floor 24
	EV_DoFloor(line,raiseFloor24);
	break;
	
      case 93:
	// Raise Floor 24 And Change
	EV_DoFloor(line,raiseFloor24AndChange);
	break;
	
      case 94:
	// Raise Floor Crush
	EV_DoFloor(line,raiseFloorCrush);
	break;
	
      case 95:
	// Raise floor to nearest height
	// and change texture.
	EV_DoPlat(line,raiseToNearestAndChange,0);
	break;
	
      case 96:
	// Raise floor to shortest texture height
	// on either side of lines.
	EV_DoFloor(line,raiseToTexture);
	break;
	
      case 97:
	// TELEPORT!
	EV_Teleport( line, side, thing );
	break;
	
      case 98:
	// Lower Floor (TURBO)
	EV_DoFloor(line,turboLower);
	break;

      case 105:
	// Blazing Door Raise (faster than TURBO!)
	EV_DoDoor (line,vld_blazeRaise);
	break;
	
      case 106:
	// Blazing Door Open (faster than TURBO!)
	EV_DoDoor (line,vld_blazeOpen);
	break;

      case 107:
	// Blazing Door Close (faster than TURBO!)
	EV_DoDoor (line,vld_blazeClose);
	break;

      case 120:
	// Blazing PlatDownWaitUpStay.
	EV_DoPlat(line,blazeDWUS,0);
	break;
	
      case 126:
	// TELEPORT MonsterONLY.
	if (!MO_PLAYER(thing))
	    EV_Teleport( line, side, thing );
	break;
	
      case 128:
	// Raise To Nearest Floor
	EV_DoFloor(line,raiseFloorToNearest);
	break;
	
      case 129:
	// Raise Floor Turbo
	EV_DoFloor(line,raiseFloorTurbo);
	break;
    }
}



//
// P_ShootSpecialLine - IMPACT SPECIALS
// Called when a thing shoots a special line.
//
void
P_ShootSpecialLine
( mobj_t*	thing,
  lineidx_t	line )
{
    int		ok;
#ifdef DMC_STATS
    HS_Event (HS_E_SHOOT, thing);
#endif
    
    //	Impacts that other things can activate.
    if (!MO_PLAYER(thing))
    {
	ok = 0;
	switch(P_LineSpecial(line))
	{
	  case 46:
	    // OPEN DOOR IMPACT
	    ok = 1;
	    break;
	}
	if (!ok)
	    return;
    }
#ifdef DMC_STATS
    HS_Event (HS_E_HANDLED, thing);
#endif

    if (!P_MoverReserve(line, P_LineSpecial(line)))
	return;

    switch(P_LineSpecial(line))
    {
      case 24:
	// RAISE FLOOR
	EV_DoFloor(line,raiseFloor);
	P_ChangeSwitchTexture(line,0);
	break;
	
      case 46:
	// OPEN DOOR
	EV_DoDoor(line,vld_open);
	P_ChangeSwitchTexture(line,1);
	break;
	
      case 47:
	// RAISE FLOOR NEAR AND CHANGE
	EV_DoPlat(line,raiseToNearestAndChange,0);
	P_ChangeSwitchTexture(line,0);
	break;
    }
}



//
// P_PlayerInSpecialSector
// Called every tic frame
//  that the player origin is in a special sector
//
void P_PlayerInSpecialSector (player_t* player)
{
    secidx_t	sector;
	
    sector = player->mo->sector;

    // Falling, not all the way down yet?
    if (player->mo->z != P_FloorHeight(sector))
	return;	

    // Has hitten ground.
    switch (P_SecSpecial(sector))
    {
      case 5:
	// HELLSLIME DAMAGE
	if (!player->powers[pw_ironfeet])
	    if (!(leveltime&0x1f))
		P_DamageMobj (player->mo, NULL, NULL, 10);
	break;
	
      case 7:
	// NUKAGE DAMAGE
	if (!player->powers[pw_ironfeet])
	    if (!(leveltime&0x1f))
		P_DamageMobj (player->mo, NULL, NULL, 5);
	break;
	
      case 16:
	// SUPER HELLSLIME DAMAGE
      case 4:
	// STROBE HURT
	if (!player->powers[pw_ironfeet]
	    || (P_Random()<5) )
	{
	    if (!(leveltime&0x1f))
		P_DamageMobj (player->mo, NULL, NULL, 20);
	}
	break;
			
      case 9:
	// SECRET SECTOR
	player->secretcount++;
	P_SecOvl(sector)->special = 0;
	break;
			
      case 11:
	// EXIT SUPER DAMAGE! (for E1M8 finale)
	player->cheats &= ~CF_GODMODE;

	if (!(leveltime&0x1f))
	    P_DamageMobj (player->mo, NULL, NULL, 20);

	if (player->health <= 10)
	    G_ExitLevel();
	break;
			
      default:
	I_Error ("P_PlayerInSpecialSector: "
		 "unknown special %i",
		 P_SecSpecial(sector));
	break;
    };
}




//
// P_UpdateSpecials
// Animate planes, scroll walls, etc.
//
void P_UpdateSpecials (void)
{
    dmc_anim_t	a;
    int		i;
    int		k;


    //	ANIMATE FLATS AND TEXTURES GLOBALLY
    // The cycles of the image (ANIMS): frame first + i of a cycle shows
    // first + (leveltime/speed + phase + i) % frames, phase stands for
    // the vanilla number of the first frame.
    for (k = 0, i = 0; k < anim_count(); k++)
    {
	anim_get(k, &a);
	if (a.kind == DMC_ANIM_SWITCH)
	    continue;
	anim_offset(i++, (leveltime/a.speed + a.phase)
		    % (a.last - a.first + 1));
    }

    
    //	ANIMATE LINE SPECIALS
    // Scrolling walls (special 48) follow leveltime, see P_GetSide.

    
    //	DO BUTTONS
    for (i = 0; i < MAXBUTTONS; i++)
	if (buttonlist[i].btimer)
	{
	    buttonlist[i].btimer--;
	    if (!buttonlist[i].btimer)
	    {
		switch(buttonlist[i].where)
		{
		  case top:
		    P_SetSideTexture(P_LineSide(buttonlist[i].line, 0), 0,
				     buttonlist[i].btexture);
		    break;
		    
		  case middle:
		    P_SetSideTexture(P_LineSide(buttonlist[i].line, 0), 1,
				     buttonlist[i].btexture);
		    break;
		    
		  case bottom:
		    P_SetSideTexture(P_LineSide(buttonlist[i].line, 0), 2,
				     buttonlist[i].btexture);
		    break;
		}
		// vanilla passes the address of the soundorg field
		S_StartSound(NULL,sfx_swtchn);
		memset(&buttonlist[i],0,sizeof(button_t));
	    }
	}
}


//
// Donut overrun emulation
//
// Derived from the code from PrBoom+.  Thanks go to Andrey Budko (entryway)
// as usual :-)
//

#define DONUT_FLOORHEIGHT_DEFAULT 0x00000000
#define DONUT_FLOORPIC_DEFAULT 0x16

static void DonutOverrun(fixed_t *s3_floorheight, short *s3_floorpic)
{
    // floor pics are per-map flat indices: the vanilla flat 0x16 (the
    // Windows 98 dump, -donut is not supported) becomes one beyond this
    // map's flats, its first flat
    int numflats = M.nflat + 1;

    *s3_floorheight = (fixed_t) DONUT_FLOORHEIGHT_DEFAULT;
    *s3_floorpic = DONUT_FLOORPIC_DEFAULT < numflats ? DONUT_FLOORPIC_DEFAULT : 1;
}


//
// Special Stuff that can not be categorized
//
int EV_DoDonut(lineidx_t	line)
{
    secidx_t		s1;
    secidx_t		s2;
    secidx_t		s3;
    int			secnum;
    int			rtn;
    int			i;
    int			first;
    int			count;
    floormove_t*	floor;
    fixed_t s3_floorheight;
    short s3_floorpic;

    secnum = -1;
    rtn = 0;
    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
	s1 = secnum;

	// ALREADY MOVING?  IF SO, KEEP GOING...
	if (P_SecMover(s1) != NOMOVER)
	    continue;

	rtn = 1;
	P_SecLines(s1, &first);
	s2 = getNextSector(P_SecLine(first),s1);

        // Vanilla Doom does not check if the linedef is one sided.  The
        // game does not crash, but reads invalid memory and causes the
        // sector floor to move "down" to some unknown height.
        // DOSbox prints a warning about an invalid memory access.
        //
        // I'm not sure exactly what invalid memory is being read.  This
        // isn't something that should be done, anyway.
        // Just print a warning and return.

        if (s2 == IDX_NONE)
	    break;

	count = P_SecLines(s2, &first);
	for (i = 0; i < count; i++)
	{
	    s3 = P_LineBackSector(P_SecLine(first + i));

	    if (s3 == s1)
		continue;

            if (s3 == IDX_NONE)
            {
                // e6y
                // s3 is NULL, so
                // s3->floorheight is an int at 0000:0000
                // s3->floorpic is a short at 0000:0008
                // Trying to emulate

                DonutOverrun(&s3_floorheight, &s3_floorpic);
            }
            else
            {
                s3_floorheight = P_FloorHeight(s3);
                s3_floorpic = P_FloorPic(s3);
            }

	    //	Spawn rising slime
	    floor = &P_NewMover (MV_FLOOR, s2)->floor;
	    floor->type = donutRaise;
	    floor->crush = false;
	    floor->direction = 1;
	    floor->speed8 = TO8(FLOORSPEED / 2);
	    floor->texture = s3_floorpic;
	    floor->newspecial = 0;
	    floor->dest8 = TO8(s3_floorheight);
	    
	    //	Spawn lowering donut-hole
	    floor = &P_NewMover (MV_FLOOR, s1)->floor;
	    floor->type = lowerFloor;
	    floor->crush = false;
	    floor->direction = -1;
	    floor->speed8 = TO8(FLOORSPEED / 2);
	    floor->dest8 = TO8(s3_floorheight);
	    break;
	}
    }
    return rtn;
}



