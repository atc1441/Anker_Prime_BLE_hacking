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
//	Refresh, visplane stuff (floor, ceilings).
//


#ifndef __R_PLANE__
#define __R_PLANE__


#include "r_data.h"



// Pools of the current strip (port/render/r_frame.c).
extern short*		openings;
extern short*		lastopening;
extern short*		openings_end;

// Clip arrays of the strip, indexed by the absolute column.
extern short*		floorclip;
extern short*		ceilingclip;

// Visplanes of the strip, r_planesize bytes each in r_planemem.
extern byte*		r_planemem;
extern int		r_planesize;
extern int		r_planew;	// columns of a visplane
extern int		r_numplanes;
extern int		r_maxplanes;
extern byte*		spanstart;	// strip-local column per row

// Floors and ceilings in the flat's average colour (no visplanes).
extern int		r_solid;

// top and bottom rows of a visplane, by strip-local column, with a
// pad before the first and after the last column
#define VP_TOP(pl)	((byte *) (pl) + sizeof(visplane_t) + 1)
#define VP_BOTTOM(pl)	(VP_TOP(pl) + r_planew + 2)

// The floor or ceiling a wall fragment marks: what R_FindPlane got and
// the visplane it marked last. picnum -1 is no plane.
typedef struct
{
    fixed_t	height;
    int		picnum;
    int		lightlevel;
    visplane_t*	cur;
    int		solid;	// solid colour, -1 until needed

} rplane_t;

extern rplane_t		floorplane;
extern rplane_t		ceilingplane;

void R_PlanesFrame (void);
void R_ClearPlanes (void);

void
R_FindPlane
( rplane_t*	rp,
  fixed_t	height,
  int		picnum,
  int		lightlevel );

void
R_MarkPlane
( rplane_t*	rp,
  int		x,
  int		top,
  int		bottom );

void
R_MapPlane
( int		y,
  int		x1,
  int		x2 );

void
R_MakeSpans
( int		x,
  int		t1,
  int		b1,
  int		t2,
  int		b2 );

void R_DrawPlanes (void);


#endif
