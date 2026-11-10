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
//	Floor animation: raising stairs.
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
// FLOORS
//

//
// Move a plane (floor or ceiling) and check for crushing
//
result_e
T_MovePlane
( secidx_t	sector,
  fixed_t	speed,
  fixed_t	dest,
  boolean	crush,
  int		floorOrCeiling,
  int		direction )
{
    boolean	flag;
    fixed_t	lastpos;
    secovl_t*	sec = P_SecOvl(sector);
	
    switch(floorOrCeiling)
    {
      case 0:
	// FLOOR
	switch(direction)
	{
	  case -1:
	    // DOWN
	    if (FX8(sec->floor8) - speed < dest)
	    {
		lastpos = FX8(sec->floor8);
		sec->floor8 = TO8(dest);
		flag = P_ChangeSector(sector,crush);
		if (flag == true)
		{
		    sec->floor8 = TO8(lastpos);
		    P_ChangeSector(sector,crush);
		    //return crushed;
		}
		return pastdest;
	    }
	    else
	    {
		lastpos = FX8(sec->floor8);
		sec->floor8 -= TO8(speed);
		flag = P_ChangeSector(sector,crush);
		if (flag == true)
		{
		    sec->floor8 = TO8(lastpos);
		    P_ChangeSector(sector,crush);
		    return crushed;
		}
	    }
	    break;
						
	  case 1:
	    // UP
	    if (FX8(sec->floor8) + speed > dest)
	    {
		lastpos = FX8(sec->floor8);
		sec->floor8 = TO8(dest);
		flag = P_ChangeSector(sector,crush);
		if (flag == true)
		{
		    sec->floor8 = TO8(lastpos);
		    P_ChangeSector(sector,crush);
		    //return crushed;
		}
		return pastdest;
	    }
	    else
	    {
		// COULD GET CRUSHED
		lastpos = FX8(sec->floor8);
		sec->floor8 += TO8(speed);
		flag = P_ChangeSector(sector,crush);
		if (flag == true)
		{
		    if (crush == true)
			return crushed;
		    sec->floor8 = TO8(lastpos);
		    P_ChangeSector(sector,crush);
		    return crushed;
		}
	    }
	    break;
	}
	break;
									
      case 1:
	// CEILING
	switch(direction)
	{
	  case -1:
	    // DOWN
	    if (FX8(sec->ceil8) - speed < dest)
	    {
		lastpos = FX8(sec->ceil8);
		sec->ceil8 = TO8(dest);
		flag = P_ChangeSector(sector,crush);

		if (flag == true)
		{
		    sec->ceil8 = TO8(lastpos);
		    P_ChangeSector(sector,crush);
		    //return crushed;
		}
		return pastdest;
	    }
	    else
	    {
		// COULD GET CRUSHED
		lastpos = FX8(sec->ceil8);
		sec->ceil8 -= TO8(speed);
		flag = P_ChangeSector(sector,crush);

		if (flag == true)
		{
		    if (crush == true)
			return crushed;
		    sec->ceil8 = TO8(lastpos);
		    P_ChangeSector(sector,crush);
		    return crushed;
		}
	    }
	    break;
						
	  case 1:
	    // UP
	    if (FX8(sec->ceil8) + speed > dest)
	    {
		lastpos = FX8(sec->ceil8);
		sec->ceil8 = TO8(dest);
		flag = P_ChangeSector(sector,crush);
		if (flag == true)
		{
		    sec->ceil8 = TO8(lastpos);
		    P_ChangeSector(sector,crush);
		    //return crushed;
		}
		return pastdest;
	    }
	    else
	    {
		lastpos = FX8(sec->ceil8);
		sec->ceil8 += TO8(speed);
		flag = P_ChangeSector(sector,crush);
// UNUSED
#if 0
		if (flag == true)
		{
		    sec->ceil8 = TO8(lastpos);
		    P_ChangeSector(sector,crush);
		    return crushed;
		}
#endif
	    }
	    break;
	}
	break;
		
    }
    return ok;
}


//
// MOVE A FLOOR TO IT'S DESTINATION (UP OR DOWN)
//
void T_MoveFloor(floormove_t* floor)
{
    result_e	res;
    secovl_t*	sec = P_SecOvl(floor->sector);
	
    res = T_MovePlane(floor->sector,
		      FX8(floor->speed8),
		      FX8(floor->dest8),
		      floor->crush,0,floor->direction);
    
    if (!(leveltime&7))
	S_StartSound(P_SecSoundOrg(floor->sector), sfx_stnmov);
    
    if (res == pastdest)
    {
	sec->mover = NOMOVER;

	if (floor->direction == 1)
	{
	    switch(floor->type)
	    {
	      case donutRaise:
		sec->special = floor->newspecial;
		sec->floorpic = floor->texture;
	      default:
		break;
	    }
	}
	else if (floor->direction == -1)
	{
	    switch(floor->type)
	    {
	      case lowerAndChange:
		sec->special = floor->newspecial;
		sec->floorpic = floor->texture;
	      default:
		break;
	    }
	}
	P_RemoveMover(floor);

	S_StartSound(P_SecSoundOrg(floor->sector), sfx_pstop);
    }

}

//
// HANDLE FLOOR TYPES
//
int
EV_DoFloor
( lineidx_t	line,
  floor_e	floortype )
{
    int			secnum;
    int			rtn;
    int			i;
    int			first;
    int			count;
    secidx_t		sec;
    floormove_t*	floor;
    fixed_t		speed;
    fixed_t		dest = 0;

    secnum = -1;
    rtn = 0;
    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
	sec = secnum;
		
	// ALREADY MOVING?  IF SO, KEEP GOING...
	if (P_SecMover(sec) != NOMOVER)
	    continue;
	
	// new floor thinker
	rtn = 1;
	floor = &P_NewMover (MV_FLOOR, sec)->floor;
	floor->type = floortype;
	floor->crush = false;
	speed = FLOORSPEED;

	switch(floortype)
	{
	  case lowerFloor:
	    floor->direction = -1;
	    dest = P_FindHighestFloorSurrounding(sec);
	    break;

	  case lowerFloorToLowest:
	    floor->direction = -1;
	    dest = P_FindLowestFloorSurrounding(sec);
	    break;

	  case turboLower:
	    floor->direction = -1;
	    speed = FLOORSPEED * 4;
	    dest = P_FindHighestFloorSurrounding(sec);
	    if (dest != P_FloorHeight(sec))
		dest += 8*FRACUNIT;
	    break;

	  case raiseFloorCrush:
	    floor->crush = true;
	  case raiseFloor:
	    floor->direction = 1;
	    dest = P_FindLowestCeilingSurrounding(sec);
	    if (dest > P_CeilingHeight(sec))
		dest = P_CeilingHeight(sec);
	    dest -= (8*FRACUNIT)*
		(floortype == raiseFloorCrush);
	    break;

	  case raiseFloorTurbo:
	    floor->direction = 1;
	    speed = FLOORSPEED*4;
	    dest = P_FindNextHighestFloor(sec,P_FloorHeight(sec));
	    break;

	  case raiseFloorToNearest:
	    floor->direction = 1;
	    dest = P_FindNextHighestFloor(sec,P_FloorHeight(sec));
	    break;

	  case raiseFloor24:
	    floor->direction = 1;
	    dest = P_FloorHeight(sec) + 24 * FRACUNIT;
	    break;
	  case raiseFloor512:
	    floor->direction = 1;
	    dest = P_FloorHeight(sec) + 512 * FRACUNIT;
	    break;

	  case raiseFloor24AndChange:
	    floor->direction = 1;
	    dest = P_FloorHeight(sec) + 24 * FRACUNIT;
	    P_SecOvl(sec)->floorpic = P_FloorPic(P_LineFrontSector(line));
	    P_SecOvl(sec)->special = P_SecSpecial(P_LineFrontSector(line));
	    break;

	  case raiseToTexture:
	  {
	      int	minsize = INT_MAX;
	      side_t	side;
				
	      floor->direction = 1;
	      // Vanilla also counts texture 0 for a missing bottom texture.
	      // The image does not keep it, so missing textures are skipped.
	      // No E1 map uses specials 30 and 96, the data layer test
	      // fails a pack that does.
	      count = P_SecLines(sec, &first);
	      for (i = 0; i < count; i++)
	      {
		  if (twoSided (secnum, i) )
		  {
		      P_GetSide(getSide(secnum,i,0), &side);
		      if (side.bottomtexture > 0)
			  if (R_TextureHeight(side.bottomtexture) < 
			      minsize)
			      minsize = 
				  R_TextureHeight(side.bottomtexture);
		      P_GetSide(getSide(secnum,i,1), &side);
		      if (side.bottomtexture > 0)
			  if (R_TextureHeight(side.bottomtexture) < 
			      minsize)
			      minsize = 
				  R_TextureHeight(side.bottomtexture);
		  }
	      }
	      dest = P_FloorHeight(sec) + minsize;
	  }
	  break;
	  
	  case lowerAndChange:
	    floor->direction = -1;
	    dest = P_FindLowestFloorSurrounding(sec);
	    floor->texture = P_FloorPic(sec);

	    // as in vanilla the bound follows sec, which changes below
	    for (i = 0; i < P_SecLines(sec, &first); i++)
	    {
		if ( twoSided(secnum, i) )
		{
		    if (getSector(secnum,i,0) == secnum)
		    {
			sec = getSector(secnum,i,1);

			if (P_FloorHeight(sec) == dest)
			{
			    floor->texture = P_FloorPic(sec);
			    floor->newspecial = P_SecSpecial(sec);
			    break;
			}
		    }
		    else
		    {
			sec = getSector(secnum,i,0);

			if (P_FloorHeight(sec) == dest)
			{
			    floor->texture = P_FloorPic(sec);
			    floor->newspecial = P_SecSpecial(sec);
			    break;
			}
		    }
		}
	    }
	  default:
	    break;
	}
	floor->speed8 = TO8(speed);
	floor->dest8 = TO8(dest);
    }
    return rtn;
}




//
// BUILD A STAIRCASE!
// count: count the sectors that would start moving, without moving
// them (the all-or-nothing mover reservation).
//
static int
P_Stairs
( lineidx_t	line,
  stair_e	type,
  boolean	count )
{
    int			secnum;
    int			height;
    int			i;
    int			newsecnum;
    int			texture;
    int			ok;
    int			rtn;
    int			first;
    int			nlines;
    line_t		l;
    
    secidx_t		sec;
    secidx_t		tsec;

    floormove_t*	floor;
    
    fixed_t		stairsize = 0;
    fixed_t		speed = 0;

    secnum = -1;
    rtn = 0;
    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
	sec = secnum;
		
	// ALREADY MOVING?  IF SO, KEEP GOING...
	if (P_SecMover(sec) != NOMOVER || (secstat[sec] & SS_QUEUED))
	    continue;
	
	switch(type)
	{
	  case build8:
	    speed = FLOORSPEED/4;
	    stairsize = 8*FRACUNIT;
	    break;
	  case turbo16:
	    speed = FLOORSPEED*4;
	    stairsize = 16*FRACUNIT;
	    break;
	}
	height = P_FloorHeight(sec) + stairsize;

	// new floor thinker
	if (count)
	{
	    rtn++;
	    secstat[sec] |= SS_QUEUED;
	}
	else
	{
	    rtn = 1;
	    floor = &P_NewMover (MV_FLOOR, sec)->floor;
	    floor->direction = 1;
	    floor->speed8 = TO8(speed);
	    floor->dest8 = TO8(height);
	}
		
	texture = P_FloorPic(sec);
	
	// Find next sector to raise
	// 1.	Find 2-sided line with same sector side[0]
	// 2.	Other side is the next sector to raise
	do
	{
	    ok = 0;
	    nlines = P_SecLines(sec, &first);
	    for (i = 0;i < nlines;i++)
	    {
		P_GetLine(P_SecLine(first + i), &l);
		if ( !(l.flags & ML_TWOSIDED) )
		    continue;
					
		tsec = l.frontsector;
		newsecnum = tsec;
		
		if (secnum != newsecnum)
		    continue;

		tsec = l.backsector;
		newsecnum = tsec;

		if (P_FloorPic(tsec) != texture)
		    continue;
					
		height += stairsize;

		if (P_SecMover(tsec) != NOMOVER || (secstat[tsec] & SS_QUEUED))
		    continue;
					
		sec = tsec;
		secnum = newsecnum;
		if (count)
		{
		    rtn++;
		    secstat[sec] |= SS_QUEUED;
		}
		else
		{
		    floor = &P_NewMover (MV_FLOOR, sec)->floor;
		    floor->direction = 1;
		    floor->speed8 = TO8(speed);
		    floor->dest8 = TO8(height);
		}
		ok = 1;
		break;
	    }
	} while(ok);
    }
    if (count)
	for (i = 0; i < numsectors; i++)
	    secstat[i] &= ~SS_QUEUED;
    return rtn;
}

int
EV_BuildStairs
( lineidx_t	line,
  stair_e	type )
{
    return P_Stairs(line, type, false);
}

int P_StairDemand (lineidx_t line, stair_e type)
{
    return P_Stairs(line, type, true);
}
