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
//	System specific interface stuff.
//


#ifndef __R_MAIN__
#define __R_MAIN__

#include "d_player.h"
#include "r_data.h"




//
// POV related.
//
extern fixed_t		viewcos;
extern fixed_t		viewsin;

extern int		viewwindowx;
extern int		viewwindowy;



extern int		centerx;
extern int		centery;

extern fixed_t		centerxfrac;
extern fixed_t		centeryfrac;
extern fixed_t		projection;

extern int		validcount;

extern int		linecount;
extern int		loopcount;


//
// Lighting LUT.
// Used for z-depth cuing per column/row,
//  and other lighting effects (sector ambient, flash).
//

// Lighting constants.
// Now why not 32 levels here?
#define LIGHTLEVELS	        16
#define LIGHTSEGSHIFT	         4

#define MAXLIGHTSCALE		48
#define LIGHTSCALESHIFT		12
#define MAXLIGHTZ	       128
#define LIGHTZSHIFT		20

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMCOLORMAPS		32

extern lighttable_t*	colormaps;
extern int		extralight;
extern lighttable_t*	fixedcolormap;

// The vanilla scalelight and zlight tables are not kept: the row of
// COLORMAP is computed from the light level (0-15) and the scale or
// distance index exactly as R_ExecuteSetViewSize and R_InitLightTables
// fill them (the view is always 320 pixels wide).
#define R_STARTMAP(l)		((LIGHTLEVELS-1-(l))*4)

extern const uint8_t		dmc_light_dist[MAXLIGHTZ];

static inline lighttable_t* R_ScaleLight (int lightnum, unsigned index)
{
    int	level = R_STARTMAP(lightnum) - (int)(index >> 1);

    if (level < 0)
	level = 0;
    if (level >= NUMCOLORMAPS)
	level = NUMCOLORMAPS-1;
    return colormaps + level*256;
}

static inline lighttable_t* R_ZLight (int lightnum, unsigned index)
{
    int	level = R_STARTMAP(lightnum) - dmc_light_dist[index];

    if (level < 0)
	level = 0;
    if (level >= NUMCOLORMAPS)
	level = NUMCOLORMAPS-1;
    return colormaps + level*256;
}

// Light level 0-15 of a sector light plus extralight, clamped.
static inline int R_LightNum (int lightlevel)
{
    int	lightnum = (lightlevel >> LIGHTSEGSHIFT)+extralight;

    if (lightnum < 0)
	return 0;
    if (lightnum >= LIGHTLEVELS)
	return LIGHTLEVELS-1;
    return lightnum;
}

// Blocky/low detail mode.
//B remove this?
//  0 = high, 1 = low
extern	int		detailshift;	


//
// Function pointers to switch refresh/drawing functions.
// Used to select shadow mode etc.
//
extern void		(*colfunc) (void);


//
// Utility functions.
int
R_PointOnSide
( fixed_t	x,
  fixed_t	y,
  node_t*	node );

int
R_PointOnSegSide
( fixed_t	x,
  fixed_t	y,
  seg_t*	line );

angle_t
R_PointToAngle
( fixed_t	x,
  fixed_t	y );

angle_t
R_PointToAngle2
( fixed_t	x1,
  fixed_t	y1,
  fixed_t	x2,
  fixed_t	y2 );

fixed_t
R_PointToDist
( fixed_t	x,
  fixed_t	y );


fixed_t R_ScaleFromGlobalAngle (angle_t visangle);

ssecidx_t
R_PointInSubsector
( fixed_t	x,
  fixed_t	y );

secidx_t
R_PointInSector
( fixed_t	x,
  fixed_t	y );

void
R_AddPointToBox
( int		x,
  int		y,
  fixed_t*	box );



//
// REFRESH - the actual rendering functions.
//

// Called by G_Drawer.
void R_RenderPlayerView (player_t *player);

// Called by startup code.
void R_Init (void);

// Called by M_Responder.
void R_SetViewSize (int blocks, int detail);
void R_ExecuteSetViewSize (void);

// View angle of column x (vanilla xtoviewangle) and the column of a
// fine angle (vanilla viewangletox), from the tables in flash.
extern const unsigned short*	r_xtva;

static inline angle_t R_XToViewAngle (int x)
{
    return ((angle_t)r_xtva[x] << 19) - ANG90;
}

int R_ViewAngleToX (unsigned angle);

#endif
