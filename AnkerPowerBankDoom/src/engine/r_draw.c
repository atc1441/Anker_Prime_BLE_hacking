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
//	The actual span/column drawing functions.
//	Here find the main potential for optimization,
//	 e.g. inline assembly, different algorithms.
//




#include "doomdef.h"

#include "i_system.h"

#include "r_local.h"

// State.
#include "doomstat.h"

#include "r_strip.h"

#ifdef DMC_ATTR
#include <stdio.h>
#include <stdlib.h>
#endif


//
// All drawing of the 3D view goes into the current strip buffer
// (port/render/r_frame.c), column-major: logical column x starts at
// r_stripbuf + (x - r_bufx0) * R_VIEWH. In low detail a logical column
// is sent twice by the LCD pump, so both detail modes share the drawers.
// The inner loops are in port/render/r_inner.c.
//


int		viewwidth;
int		scaledviewwidth;
int		viewheight;
int		viewwindowx;
int		viewwindowy; 

 
//
// R_DrawColumn
// Source is the top of the column to scale.
//
lighttable_t*		dc_colormap; 
int			dc_x; 
int			dc_yl; 
int			dc_yh; 
fixed_t			dc_iscale; 
fixed_t			dc_texturemid;

// first pixel in a column (possibly virtual) 
byte*			dc_source;		

// Row of dc_source a pixel reads: the texture position shifted left by
// dc_fracshift, its top 32 - dc_rowshift bits. A column of 1 << b stored
// rows at mip level L has dc_fracshift 16 - L - b and dc_rowshift 32 - b,
// the row is then (frac >> (16 + L)) & ((1 << b) - 1). Posts and full
// columns (b 7, L 0) read rows (frac >> 16) & 127 as vanilla. Set by
// every caller of colfunc.
int			dc_fracshift;
int			dc_rowshift;

// player translation 1-3 (vanilla translationtables row + 1)
int			dc_translation;


static byte* R_ColumnDest (void)
{
#ifdef RANGECHECK 
    if (dc_x < r_s0
	|| dc_x >= r_s1
	|| dc_yl < 0
	|| dc_yh >= R_VIEWH) 
	I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x); 
#endif 
    return r_stripbuf + (dc_x - r_bufx0) * R_VIEWH + dc_yl;
}


#ifdef DMC_ATTR
// Host frame test: class of every pixel a column drawer writes. A wall
// or sky pixel whose row is beyond the stored texture height, and a
// sprite pixel beyond the rebuilt post column, read data vanilla does
// not have (r_data.c sets dc_texheight and dc_ispost).
static void R_AttrColumn (int cls)
{
    byte*	dest = r_attrbuf + (dc_x - r_bufx0) * R_VIEWH;
    int		y;
    int		row;
    static int	px = -1, py, pt;

    // DMC_ATTR_PIXEL=x,y,tic prints what the column drawer read there
    if (px == -1)
    {
	char*	e = getenv("DMC_ATTR_PIXEL");

	px = -2;
	if (e)
	    sscanf(e, "%d,%d,%d", &px, &py, &pt);
    }
    if (px >= 0 && gametic == pt && py >= dc_yl && py <= dc_yh
	&& px >> detailshift == dc_x)
    {
	row = ((dc_texturemid + (py-centery)*dc_iscale) >> FRACBITS) & 127;
	fprintf (stderr, "pixel %d,%d tic %d: yl %d yh %d texturemid %d iscale %d row %d"
		 " src [%d] strip %d..%d\n", px, py, pt, dc_yl, dc_yh,
		 dc_texturemid, dc_iscale, row,
		 dc_source[((unsigned) (dc_texturemid + (py-centery)*dc_iscale)
			    << dc_fracshift) >> dc_rowshift], r_s0, r_s1);
    }

    for (y = dc_yl; y <= dc_yh; y++)
    {
	row = ((dc_texturemid + (y-centery)*dc_iscale) >> FRACBITS) & 127;
	if (cls == ATTR_COLUMN && row >= dc_texheight)
	    dest[y] = dc_ispost ? ATTR_POSTREAD : ATTR_BEYOND;
	else
	    dest[y] = cls;
    }
}
#define ATTR_COL(c)	R_AttrColumn(c)
#else
#define ATTR_COL(c)	((void)0)
#endif


//
// A column is a vertical slice/span from a wall texture that,
//  given the DOOM style restrictions on the view orientation,
//  will always have constant z depth.
// Thus a special case loop for very fast rendering can
//  be used. It has also been used with Wolfenstein 3D.
// 
void R_DrawColumn (void) 
{ 
    int			count; 
    byte*		dest; 
 
    count = dc_yh - dc_yl; 

    // Zero length, column does not exceed a pixel.
    if (count < 0) 
	return; 

    dest = R_ColumnDest ();
    R_ColumnLoop (dest, count+1,
		  (unsigned) (dc_texturemid + (dc_yl-centery)*dc_iscale) << dc_fracshift,
		  (unsigned) dc_iscale << dc_fracshift, dc_rowshift, dc_source, dc_colormap);
    RSTAT(colpx, count+1);
    ATTR_COL(ATTR_COLUMN);
} 


//
// R_DrawTranslatedColumn
// Used to draw player sprites
//  with the green colorramp mapped to others.
// Could be used with different translation
//  tables, e.g. the lighter colored version
//  of the BaronOfHell, the HellKnight, uses
//  identical sprites, kinda brightened up.
//
void R_DrawTranslatedColumn (void) 
{ 
    int			count; 
    byte*		dest; 
 
    count = dc_yh - dc_yl; 
    if (count < 0) 
	return; 

    dest = R_ColumnDest ();
    R_TransColumnLoop (dest, count+1, dc_texturemid + (dc_yl-centery)*dc_iscale,
		       dc_iscale, dc_source, dc_colormap, dc_translation);
    RSTAT(colpx, count+1);
    ATTR_COL(ATTR_COLUMN);
} 


//
// Spectre/Invisibility.
// Framebuffer postprocessing: a pixel takes the colour one row above or
// below it through colormap 6. The position in the vanilla offset table
// is (x * 13 + y + gametic) mod 50, which does not depend on the strip
// width (vanilla steps one global position per pixel).
//
void R_DrawFuzzColumn (void) 
{ 
    int			count; 

    // Adjust borders. Low... 
    if (!dc_yl) 
	dc_yl = 1;

    // .. and high.
    if (dc_yh == viewheight-1) 
	dc_yh = viewheight - 2; 
		 
    count = dc_yh - dc_yl; 

    // Zero length.
    if (count < 0) 
	return; 

    R_FuzzLoop (R_ColumnDest (), count+1,
		(int) (((unsigned) dc_x * 13 + dc_yl + gametic) % 50),
		colormaps + 6*256);
    RSTAT(fuzzpx, count+1);
#ifdef DMC_ATTR
    memset (r_attrbuf + (dc_x - r_bufx0) * R_VIEWH + dc_yl, ATTR_FUZZ, count+1);
#endif
} 


//
// R_DrawSpan 
// With DOOM style restrictions on view orientation,
//  the floors and ceilings consist of horizontal slices
//  or spans with constant z depth.
// However, rotation around the world z axis is possible,
//  thus this mapping, while simpler and faster than
//  perspective correct texture mapping, has to traverse
//  the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
//  and the inner loop has to step in texture space u and v.
//
int			ds_y; 
int			ds_x1; 
int			ds_x2;

lighttable_t*		ds_colormap; 

// packed position of ds_x1 and step, see R_MapPlane
unsigned		ds_position;
unsigned		ds_step;

// start of a 32*32 tile image 
byte*			ds_source;	
byte			ds_solid;


#ifdef DMC_ATTR
static void R_AttrSpan (void)
{
    int		x;

    for (x = ds_x1; x <= ds_x2; x++)
	r_attrbuf[(x - r_bufx0) * R_VIEWH + ds_y] = ATTR_SPAN;
}
#endif


static byte* R_SpanDest (void)
{
#ifdef RANGECHECK
    if (ds_x2 < ds_x1
	|| ds_x1 < r_s0
	|| ds_x2 >= r_s1
	|| (unsigned)ds_y >= R_VIEWH)
    {
	I_Error( "R_DrawSpan: %i to %i at %i",
		 ds_x1,ds_x2,ds_y);
    }
#endif
    return r_stripbuf + (ds_x1 - r_bufx0) * R_VIEWH + ds_y;
}


//
// Draws the actual span.
// u = (xfrac >> 17) & 31, v = (yfrac >> 17) & 31 of the 32x32 flat.
//
void R_DrawSpan (void) 
{ 
    R_SpanLoop (R_SpanDest (), ds_x2 - ds_x1 + 1, ds_position, ds_step,
		ds_source, ds_colormap);
    RSTAT(spanpx, ds_x2 - ds_x1 + 1);
#ifdef DMC_ATTR
    R_AttrSpan ();
#endif
}

//
// R_DrawSpanSolid
// Solid floors (no flat slots): the flat's average colour through
// the same light row as the texels.
//
void R_DrawSpanSolid (void)
{
    R_FillRow (R_SpanDest (), ds_x2 - ds_x1 + 1, ds_colormap[ds_solid]);
    RSTAT(spanpx, ds_x2 - ds_x1 + 1);
#ifdef DMC_ATTR
    R_AttrSpan ();
#endif
}
