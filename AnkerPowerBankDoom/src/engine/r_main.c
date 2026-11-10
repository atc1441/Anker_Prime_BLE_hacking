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
//	Rendering main loop and setup functions,
//	 utility functions (BSP, geometry, trigonometry).
//	See tables.c, too.
//





#include <stdlib.h>


#include "doomdef.h"
#include "d_loop.h"

#include "m_bbox.h"
#include "m_menu.h"

#include "r_local.h"
#include "r_sky.h"
#include "p_local.h"

#include "r_strip.h"
#include "trig.h"
#include "trig_tables.h"





int			viewangleoffset;

// increment every time a check is made
int			validcount = 1;		


lighttable_t*		fixedcolormap;

int			centerx;
int			centery;

fixed_t			centerxfrac;
fixed_t			centeryfrac;
fixed_t			projection;

// just for profiling purposes
int			framecount;	

fixed_t			viewx;
fixed_t			viewy;
fixed_t			viewz;

angle_t			viewangle;

fixed_t			viewcos;
fixed_t			viewsin;

player_t*		viewplayer;

// 0 = high, 1 = low
int			detailshift;	

//
// precalculated math tables
//
angle_t			clipangle;

// xtoviewangle of the detail mode, in flash (gen/view.c)
const unsigned short*	r_xtva;

// bumped light from gun blasts
int			extralight;			



void (*colfunc) (void);



//
// R_AddPointToBox
// Expand a given bbox
// so that it encloses a given point.
//
void
R_AddPointToBox
( int		x,
  int		y,
  fixed_t*	box )
{
    if (x< box[BOXLEFT])
	box[BOXLEFT] = x;
    if (x> box[BOXRIGHT])
	box[BOXRIGHT] = x;
    if (y< box[BOXBOTTOM])
	box[BOXBOTTOM] = y;
    if (y> box[BOXTOP])
	box[BOXTOP] = y;
}


//
// R_PointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
int
R_PointOnSide
( fixed_t	x,
  fixed_t	y,
  node_t*	node )
{
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;
	
    if (!node->dx)
    {
	if (x <= node->x)
	    return node->dy > 0;
	
	return node->dy < 0;
    }
    if (!node->dy)
    {
	if (y <= node->y)
	    return node->dx < 0;
	
	return node->dx > 0;
    }
	
    dx = (x - node->x);
    dy = (y - node->y);
	
    // Try to quickly decide by looking at sign bits.
    if ( (node->dy ^ node->dx ^ dx ^ dy)&0x80000000 )
    {
	if  ( (node->dy ^ dx) & 0x80000000 )
	{
	    // (left is negative)
	    return 1;
	}
	return 0;
    }

    left = FixedMul ( node->dy>>FRACBITS , dx );
    right = FixedMul ( dy , node->dx>>FRACBITS );
	
    if (right < left)
    {
	// front side
	return 0;
    }
    // back side
    return 1;			
}


int
R_PointOnSegSide
( fixed_t	x,
  fixed_t	y,
  seg_t*	line )
{
    fixed_t	lx;
    fixed_t	ly;
    fixed_t	ldx;
    fixed_t	ldy;
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;
	
    lx = line->v1x;
    ly = line->v1y;
	
    ldx = line->v2x - lx;
    ldy = line->v2y - ly;
	
    if (!ldx)
    {
	if (x <= lx)
	    return ldy > 0;
	
	return ldy < 0;
    }
    if (!ldy)
    {
	if (y <= ly)
	    return ldx < 0;
	
	return ldx > 0;
    }
	
    dx = (x - lx);
    dy = (y - ly);
	
    // Try to quickly decide by looking at sign bits.
    if ( (ldy ^ ldx ^ dx ^ dy)&0x80000000 )
    {
	if  ( (ldy ^ dx) & 0x80000000 )
	{
	    // (left is negative)
	    return 1;
	}
	return 0;
    }

    left = FixedMul ( ldy>>FRACBITS , dx );
    right = FixedMul ( dy , ldx>>FRACBITS );
	
    if (right < left)
    {
	// front side
	return 0;
    }
    // back side
    return 1;			
}


//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table.

//




angle_t
R_PointToAngle
( fixed_t	x,
  fixed_t	y )
{	
    x -= viewx;
    y -= viewy;
    
    if ( (!x) && (!y) )
	return 0;

    if (x>= 0)
    {
	// x >=0
	if (y>= 0)
	{
	    // y>= 0

	    if (x>y)
	    {
		// octant 0
		return tantoangle( SlopeDiv(y,x));
	    }
	    else
	    {
		// octant 1
		return ANG90-1-tantoangle( SlopeDiv(x,y));
	    }
	}
	else
	{
	    // y<0
	    y = -y;

	    if (x>y)
	    {
		// octant 8
		return -tantoangle(SlopeDiv(y,x));
	    }
	    else
	    {
		// octant 7
		return ANG270+tantoangle( SlopeDiv(x,y));
	    }
	}
    }
    else
    {
	// x<0
	x = -x;

	if (y>= 0)
	{
	    // y>= 0
	    if (x>y)
	    {
		// octant 3
		return ANG180-1-tantoangle( SlopeDiv(y,x));
	    }
	    else
	    {
		// octant 2
		return ANG90+ tantoangle( SlopeDiv(x,y));
	    }
	}
	else
	{
	    // y<0
	    y = -y;

	    if (x>y)
	    {
		// octant 4
		return ANG180+tantoangle( SlopeDiv(y,x));
	    }
	    else
	    {
		 // octant 5
		return ANG270-1-tantoangle( SlopeDiv(x,y));
	    }
	}
    }
    return 0;
}


angle_t
R_PointToAngle2
( fixed_t	x1,
  fixed_t	y1,
  fixed_t	x2,
  fixed_t	y2 )
{	
    viewx = x1;
    viewy = y1;
    
    return R_PointToAngle (x2, y2);
}


fixed_t
R_PointToDist
( fixed_t	x,
  fixed_t	y )
{
    int		angle;
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	temp;
    fixed_t	dist;
    fixed_t     frac;
	
    dx = abs(x - viewx);
    dy = abs(y - viewy);
	
    if (dy>dx)
    {
	temp = dx;
	dx = dy;
	dy = temp;
    }

    // Fix crashes in udm1.wad

    if (dx != 0)
    {
        frac = FixedDiv(dy, dx);
    }
    else
    {
	frac = 0;
    }
	
    angle = (tantoangle(frac>>DBITS)+ANG90) >> ANGLETOFINESHIFT;

    // use as cosine
    dist = FixedDiv (dx, finesine(angle) );	
	
    return dist;
}




//
// R_ScaleFromGlobalAngle
// Returns the texture mapping scale
//  for the current line (horizontal span)
//  at the given angle.
// rw_distance must be calculated first.
//
fixed_t R_ScaleFromGlobalAngle (angle_t visangle)
{
    fixed_t		scale;
    angle_t		anglea;
    angle_t		angleb;
    int			sinea;
    int			sineb;
    fixed_t		num;
    int			den;

    // UNUSED
#if 0
{
    fixed_t		dist;
    fixed_t		z;
    fixed_t		sinv;
    fixed_t		cosv;
	
    sinv = finesine((visangle-rw_normalangle)>>ANGLETOFINESHIFT);	
    dist = FixedDiv (rw_distance, sinv);
    cosv = finecosine((viewangle-visangle)>>ANGLETOFINESHIFT);
    z = abs(FixedMul (dist, cosv));
    scale = FixedDiv(projection, z);
    return scale;
}
#endif

    anglea = ANG90 + (visangle-viewangle);
    angleb = ANG90 + (visangle-rw_normalangle);

    // both sines are allways positive
    sinea = finesine(anglea>>ANGLETOFINESHIFT);	
    sineb = finesine(angleb>>ANGLETOFINESHIFT);
    num = FixedMul(projection,sineb)<<detailshift;
    den = FixedMul(rw_distance,sinea);

    if (den > num>>16)
    {
	scale = FixedDiv (num, den);

	if (scale > 64*FRACUNIT)
	    scale = 64*FRACUNIT;
	else if (scale < 256)
	    scale = 256;
    }
    else
	scale = 64*FRACUNIT;
	
    return scale;
}



//
// R_ViewAngleToX
// Vanilla viewangletox[angle] with the fencepost cases removed,
// decoded from the bitset tables of gen/view.c.
//
int R_ViewAngleToX (unsigned angle)
{
    return dmc_viewangletox(detailshift, angle);
}



//
// R_SetViewSize
// Do not really change anything here,
//  because it might be in the middle of a refresh.
// The change will take effect next refresh.
//
boolean		setsizeneeded;
int		setblocks;
int		setdetail;


void
R_SetViewSize
( int		blocks,
  int		detail )
{
    setsizeneeded = true;
    setblocks = blocks;
    setdetail = detail;
}


//
// R_ExecuteSetViewSize
// The 3D view always fills the 320x200 window above the status band,
// only the detail mode changes. Projection tables are in flash and the
// light tables are computed (R_ScaleLight, R_ZLight).
//
void R_ExecuteSetViewSize (void)
{
    setsizeneeded = false;

    scaledviewwidth = SCREENWIDTH;
    viewheight = SCREENHEIGHT;
    
    detailshift = setdetail;
    viewwidth = scaledviewwidth>>detailshift;
	
    centery = viewheight/2;
    centerx = viewwidth/2;
    centerxfrac = centerx<<FRACBITS;
    centeryfrac = centery<<FRACBITS;
    projection = centerxfrac;

    colfunc = R_DrawColumn;

    r_xtva = detailshift ? dmc_xtva_low : dmc_xtva_high;
    clipangle = R_XToViewAngle(0);
    
    // psprite scales
    pspritescale = FRACUNIT*viewwidth/SCREENWIDTH;
    pspriteiscale = FRACUNIT*SCREENWIDTH/viewwidth;
}



//
// R_Init
//



void R_Init (void)
{
    R_InitData ();
    // viewwidth / viewheight / detailLevel are set by the defaults

    R_SetViewSize (screenblocks, detailLevel);
    R_ExecuteSetViewSize ();
    R_InitSkyMap ();
	
    framecount = 0;
}


//
// R_PointInSubsector
// The walk starts at the node stored for the blockmap cell of the
// point (GRID stream): every node above it has the whole cell, grown
// by two units, strictly on one side, so the result is the same as
// from the root.
//
ssecidx_t
R_PointInSubsector
( fixed_t	x,
  fixed_t	y )
{
    node_t	node;
    int		side;
    int		nodenum;
    int		bx;
    int		by;

    // single subsector is a special case
    if (!numnodes)				
	return 0;

    bx = (x - bmaporgx) >> MAPBLOCKSHIFT;
    by = (y - bmaporgy) >> MAPBLOCKSHIFT;
    if (bx >= 0 && bx < bmapwidth && by >= 0 && by < bmapheight)
	nodenum = map_grid(by * bmapwidth + bx);
    else
	nodenum = numnodes-1;

    while (! (nodenum & NF_SUBSECTOR) )
    {
	P_GetNode(nodenum, &node);
	side = R_PointOnSide (x, y, &node);
	nodenum = node.children[side];
    }
	
#ifdef DMC_PARITY
    PAR_PointInSubsector(x, y, nodenum & ~NF_SUBSECTOR);
#endif
    return nodenum & ~NF_SUBSECTOR;
}


//
// R_PointInSector
//
secidx_t
R_PointInSector
( fixed_t	x,
  fixed_t	y )
{
    return P_SubsecSector(R_PointInSubsector(x, y));
}



//
// R_SetupFrame
//
void R_SetupFrame (player_t* player)
{		
    viewplayer = player;
    viewx = player->mo->x;
    viewy = player->mo->y;
    viewangle = player->mo->angle + viewangleoffset;
    extralight = player->extralight;

    viewz = player->viewz;
    
    viewsin = finesine(viewangle>>ANGLETOFINESHIFT);
    viewcos = finecosine(viewangle>>ANGLETOFINESHIFT);
	
    if (player->fixedcolormap)
	fixedcolormap = colormaps + player->fixedcolormap*256;
    else
	fixedcolormap = 0;
		
    framecount++;
}



//
// R_RenderView
// Pass 1 over the BSP, then the strips (port/render/r_frame.c).
// r_viewtemp sends the strips to the temporary screen instead of the
// LCD (the pause picture over the view, host only).
//
int		r_viewtemp;

void R_RenderPlayerView (player_t* player)
{	
    R_SetupFrame (player);
    R_DrawFrame (r_viewtemp);
}
