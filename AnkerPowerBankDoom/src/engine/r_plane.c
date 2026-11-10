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
//	Here is a core component: drawing the floors and ceilings,
//	 while maintaining a per column clipping list only.
//	Moreover, the sky areas have to be determined.
//	Strip visplanes: a visplane covers the columns of one strip and is
//	taken from the pool when a wall first marks one of its columns.
//	Spans are row-anchored: the texture position of column x on row y
//	depends on x and y only, so floors look the same for every strip
//	width. Sky columns and solid floors are drawn when they are marked.
//


#include <stdio.h>
#include <stdlib.h>

#include "i_system.h"

#include "doomdef.h"
#include "doomstat.h"

#include "r_local.h"
#include "r_sky.h"

#include "fetch.h"
#include "fixed.h"
#include "r_strip.h"
#include "trig_tables.h"


rplane_t		floorplane;
rplane_t		ceilingplane;

//
// opening
//
short*			openings;
short*			lastopening;
short*			openings_end;


//
// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1
//
short*			floorclip;
short*			ceilingclip;

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//
byte*			spanstart;

// Visplanes of the strip
byte*			r_planemem;
int			r_planesize;
int			r_planew;
int			r_numplanes;
int			r_maxplanes;
int			r_solid;

//
// texture mapping
//
static int		planelight;
static fixed_t		planeheight;

static fixed_t		basexscale;
static fixed_t		baseyscale;

// row anchor: view angle and distance scale of column 0
static fixed_t		cos0;
static fixed_t		sin0;
static fixed_t		distscale0;


//
// R_PlanesFrame
// At the begining of each frame.
//
void R_PlanesFrame (void)
{
    angle_t	angle;
    angle_t	x0angle = R_XToViewAngle(0);

    // left to right mapping
    angle = (viewangle-ANG90)>>ANGLETOFINESHIFT;
	
    // scale will be unit scale at SCREENWIDTH/2 distance
    basexscale = FixedDiv (finecosine(angle),centerxfrac);
    baseyscale = -FixedDiv (finesine(angle),centerxfrac);

    angle = (viewangle + x0angle)>>ANGLETOFINESHIFT;
    cos0 = finecosine(angle);
    sin0 = finesine(angle);
    distscale0 = FixedDiv (FRACUNIT, abs(finecosine(x0angle>>ANGLETOFINESHIFT)));
}


//
// R_MapPlane
// Row-anchored: the packed texture position is that of column 0 of the
// row plus x steps, so a span gives every column the same texel
// whichever column it starts at. x1 and x2 are strip-local.
//
// BASIC PRIMITIVE
//
void
R_MapPlane
( int		y,
  int		x1,
  int		x2 )
{
    fixed_t	distance;
    fixed_t	length;
    fixed_t	xfrac;
    fixed_t	yfrac;
    fixed_t	xstep;
    fixed_t	ystep;
    unsigned	index;
    unsigned	position;
	
    distance = FixedMulI (planeheight, dmc_yslope_tab[y]);
    xstep = FixedMulI (distance,basexscale);
    ystep = FixedMulI (distance,baseyscale);
    length = FixedMulI (distance,distscale0);
    xfrac = viewx + FixedMulI(cos0, length);
    yfrac = -viewy - FixedMulI(sin0, length);

    position = ((xfrac << 10) & 0xffff0000)
             | ((yfrac >> 6)  & 0x0000ffff);
    ds_step = ((xstep << 10) & 0xffff0000)
            | ((ystep >> 6)  & 0x0000ffff);

    ds_x1 = r_s0 + x1;
    ds_x2 = r_s0 + x2;
    ds_y = y;
    ds_position = position + (unsigned)ds_x1 * ds_step;

    if (fixedcolormap)
	ds_colormap = fixedcolormap;
    else
    {
	index = distance >> LIGHTZSHIFT;
	
	if (index >= MAXLIGHTZ )
	    index = MAXLIGHTZ-1;

	ds_colormap = R_ZLight (planelight, index);
    }
	
    RSTAT(spans, 1);
    if (ds_source)
	R_DrawSpan ();
    else
	R_DrawSpanSolid ();
}


//
// R_ClearPlanes
// At begining of a strip.
//
void R_ClearPlanes (void)
{
    int		i;
    
    // opening / clipping determination
    for (i=r_s0 ; i<r_s1 ; i++)
    {
	floorclip[i] = viewheight;
	ceilingclip[i] = -1;
    }

    r_numplanes = 0;
    lastopening = openings;
}




//
// R_FindPlane
// A fragment's floor or ceiling. The visplane is found when a
// column is marked.
//
void
R_FindPlane
( rplane_t*	rp,
  fixed_t	height,
  int		picnum,
  int		lightlevel )
{
    if (picnum == skyflatnum)
    {
	height = 0;			// all skys map together
	lightlevel = 0;
    }
    if (lightlevel > 255)
	lightlevel = 255;
	
    rp->height = height;
    rp->picnum = picnum;
    rp->lightlevel = lightlevel;
    rp->cur = NULL;
    rp->solid = -1;
}


//
// R_SkyColumn
// Sky is allways drawn full bright,
//  i.e. colormaps[0] is used.
// Because of this hack, sky is not affected
//  by INVUL inverse mapping.
//
static void R_SkyColumn (int x, int top, int bottom)
{
    dc_iscale = pspriteiscale>>detailshift;
    dc_colormap = colormaps;
    dc_texturemid = skytexturemid;
    dc_yl = top;
    dc_yh = bottom;
    dc_x = x;
    R_QueueColumn (skytexture, (viewangle + R_XToViewAngle(x))>>ANGLETOSKYSHIFT, 1);
    RSTAT(skycols, 1);
    RSTAT(skypx, bottom - top + 1);
}


//
// R_SolidPlaneColumn
// Solid floors: the flat's average colour through the light row of
// every row's distance, as the spans would draw it.
//
static void R_SolidPlaneColumn (rplane_t* rp, int x, int top, int bottom)
{
    if (rp->solid < 0)
	rp->solid = flat_avg(anim_frame(P_MapFlat(rp->picnum), 1));
    R_SolidColumn (r_stripbuf + (x - r_bufx0) * R_VIEWH + top, top, bottom - top + 1,
		   abs(rp->height - viewz), rp->solid,
		   R_STARTMAP(R_LightNum(rp->lightlevel)), colormaps, fixedcolormap);
    RSTAT(solidcols, 1);
    RSTAT(solidpx, bottom - top + 1);
#ifdef DMC_ATTR
    memset (r_attrbuf + (x - r_bufx0) * R_VIEWH + top, ATTR_SPAN, bottom - top + 1);
#endif
}


//
// R_NewPlane
// A visplane of the strip for the key of rp with column xl free.
//
static visplane_t* R_NewPlane (rplane_t* rp, int xl)
{
    visplane_t*	pl;
    int		i;
    short	h = rp->height >> (FRACBITS-3);

    for (i = 0; i < r_numplanes; i++)
    {
	pl = (visplane_t *) (r_planemem + i * r_planesize);
	if (pl->height == h
	    && pl->picnum == rp->picnum
	    && pl->lightlevel == rp->lightlevel
	    && VP_TOP(pl)[xl] == 0xff)
	    return pl;
    }
    if (r_numplanes == r_maxplanes)
	return NULL;
    pl = (visplane_t *) (r_planemem + r_numplanes++ * r_planesize);
    RSTAT_MAX(hw_visplanes, r_numplanes);
    pl->height = h;
    pl->picnum = rp->picnum;
    pl->lightlevel = rp->lightlevel;
    pl->minx = xl;
    pl->maxx = xl;
    memset (VP_TOP(pl) - 1, 0xff, r_planew + 2);
    memset (VP_BOTTOM(pl) - 1, 0, r_planew + 2);
    return pl;
}


//
// R_MarkPlane
// Rows top..bottom of column x (top <= bottom) belong to the plane.
//
void
R_MarkPlane
( rplane_t*	rp,
  int		x,
  int		top,
  int		bottom )
{
    visplane_t*	pl;
    int		xl = x - r_s0;

    if (rp->picnum == skyflatnum)
    {
	R_SkyColumn (x, top, bottom);
	return;
    }
    if (r_solid)
    {
	R_SolidPlaneColumn (rp, x, top, bottom);
	return;
    }

    pl = rp->cur;
    if (!pl || VP_TOP(pl)[xl] != 0xff)
    {
	pl = R_NewPlane (rp, xl);
	if (!pl)
	{
	    // a strip of one column fills the rest in the solid colour
	    if (r_vanilla)
	    {
		RSTAT(vanilla_drops, 1);
		R_SolidPlaneColumn (rp, x, top, bottom);
	    }
	    else
		r_overflow = 1;
	    return;
	}
	rp->cur = pl;
    }
    VP_TOP(pl)[xl] = top;
    VP_BOTTOM(pl)[xl] = bottom;
    if (xl < pl->minx)
	pl->minx = xl;
    if (xl > pl->maxx)
	pl->maxx = xl;
}


//
// R_MakeSpans
//
void
R_MakeSpans
( int		x,
  int		t1,
  int		b1,
  int		t2,
  int		b2 )
{
    while (t1 < t2 && t1<=b1)
    {
	R_MapPlane (t1,spanstart[t1],x-1);
	t1++;
    }
    while (b1 > b2 && b1>=t1)
    {
	R_MapPlane (b1,spanstart[b1],x-1);
	b1--;
    }
	
    while (t2 < t1 && t2<=b2)
    {
	spanstart[t2] = x;
	t2++;
    }
    while (b2 > b1 && b2>=t2)
    {
	spanstart[b2] = x;
	b2--;
    }
}


//
// R_DrawPlane
//
static void R_DrawPlane (visplane_t* pl)
{
    byte*	top = VP_TOP(pl);
    byte*	bottom = VP_BOTTOM(pl);
    int		x;
    int		stop;

    RSTAT(planes, 1);

    // regular flat, 32x32 from the image, or a solid colour
    ds_source = R_GetFlat(pl->picnum);

    planeheight = abs((pl->height << (FRACBITS-3))-viewz);
    planelight = R_LightNum (pl->lightlevel);

    top[pl->maxx+1] = 0xff;
    top[pl->minx-1] = 0xff;
		
    stop = pl->maxx + 1;

    for (x=pl->minx ; x<= stop ; x++)
    {
	R_MakeSpans(x,top[x-1],
		    bottom[x-1],
		    top[x],
		    bottom[x]);
    }
}


//
// R_DrawPlanes
// At the end of each strip. Planes never overlap, so the order does
// not matter: those whose flat was drawn last come first, which keeps
// the flat cache from loading a flat twice.
//
void R_DrawPlanes (void)
{
    static short	lastflat = -1;
    visplane_t*		pl;
    int			i;
    int			pass;
    int			first;

    first = lastflat;
    for (pass = 0; pass < 2; pass++)
    {
	for (i = 0; i < r_numplanes; i++)
	{
	    pl = (visplane_t *) (r_planemem + i * r_planesize);
	    if ((pl->picnum == first) != !pass)
		continue;
	    R_DrawPlane (pl);
	    lastflat = pl->picnum;
	}
    }
}
