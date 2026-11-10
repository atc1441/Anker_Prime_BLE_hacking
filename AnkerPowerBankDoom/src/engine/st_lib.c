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
//	The status bar widget code.
//


#include <stdio.h>
#include <ctype.h>

#include "deh_main.h"
#include "doomdef.h"

#include "z_zone.h"
#include "v_video.h"

#include "i_swap.h"
#include "i_system.h"

#include "dmcdat.h"

#include "st_stuff.h"
#include "st_lib.h"
#include "r_local.h"

#include "r_strip.h"


// in AM_map.c
extern boolean		automapactive; 




//
// Hack display negative frags.
//  The stminus lump is looked up when drawn.
//


// ?
void STlib_initNum(const st_number_t* n)
{
    n->s->old	= 0;
}


// 
// A fairly efficient way to draw a number
//  based on differences from the old number.
// Note: worth the trouble?
//
void
STlib_drawNum
( const st_number_t*	n,
  boolean	refresh )
{

    int		numdigits = n->width;
    int		num = *n->num;
    
    int		p0 = V_UIPatch(n->p[0]);
    int		w = V_PatchWidth(p0);
    int		h = V_PatchHeight(p0);
    int		x = n->x;
    
    int		neg;

    n->s->old = *n->num;

    neg = num < 0;

    if (neg)
    {
	if (numdigits == 2 && num < -9)
	    num = -9;
	else if (numdigits == 3 && num < -99)
	    num = -99;
	
	num = -num;
    }

    // clear the area
    x = n->x - numdigits*w;

    if (n->y - ST_Y < 0)
	I_Error("drawNum: n->y - ST_Y < 0");

    // the compose pass of the status band starts from the background
    if (r_bandmode != BAND_COMPOSE)
	V_CopyRect(x, n->y - ST_Y, st_backing_screen, w*numdigits, h, x, n->y);

    // if non-number, do not draw it
    if (num == 1994)
	return;

    x = n->x;

    // in the special case of 0, you draw 0
    if (!num)
	V_DrawPatch(x - w, n->y, p0);

    // draw the new number
    while (num && numdigits--)
    {
	x -= w;
	V_DrawPatch(x, n->y, V_UIPatch(n->p[ num % 10 ]));
	num /= 10;
    }

    // draw a minus sign if necessary
    if (neg)
	V_DrawPatch(x - 8, n->y, V_UIPatch(DMC_UI_STTMINUS));
}


//
// Vanilla redraws a number every frame. The status band only has to
// send what changed, and an unchanged number draws the same pixels.
void
STlib_updateNum
( const st_number_t*	n,
  boolean		refresh )
{
    if (*n->on && (refresh || n->s->old != *n->num))
	STlib_drawNum(n, refresh);
}


void
STlib_updatePercent
( const st_percent_t*	per,
  int			refresh )
{
    if (refresh && *per->n.on)
	V_DrawPatch(per->n.x, per->n.y, V_UIPatch(per->p));
    
    STlib_updateNum(&per->n, refresh);
}



void STlib_initMultIcon(const st_multicon_t* mi)
{
    mi->s->old		= -1;
    mi->s->erased	= false;
}



void
STlib_updateMultIcon
( const st_multicon_t*	mi,
  boolean		refresh )
{
    int			w;
    int			h;
    int			x;
    int			y;
    int			op;

    if (*mi->on && (mi->s->old != *mi->inum || refresh) && (*mi->inum != -1))
    {
	// The band compositor draws the icon as vanilla last did: after
	// an erase only if that draw erased (the arms numbers sit on the
	// STARMS panel until they first change).
	if (r_bandmode != BAND_COMPOSE)
	    mi->s->erased = mi->s->old != -1;
	if (mi->s->erased)
	{
	    op = V_UIPatch(mi->p[mi->s->old]);
	    x = mi->x - V_PatchLeftOffset(op);
	    y = mi->y - V_PatchTopOffset(op);
	    w = V_PatchWidth(op);
	    h = V_PatchHeight(op);

	    if (y - ST_Y < 0)
		I_Error("updateMultIcon: y - ST_Y < 0");

	    V_CopyRect(x, y-ST_Y, st_backing_screen, w, h, x, y);
	}
	V_DrawPatch(mi->x, mi->y, V_UIPatch(mi->p[*mi->inum]));
	mi->s->old = *mi->inum;
    }
}



void STlib_initBinIcon(const st_binicon_t* bi)
{
    bi->s->old	= false;
}



void
STlib_updateBinIcon
( const st_binicon_t*	bi,
  boolean		refresh )
{
    int			x;
    int			y;
    int			w;
    int			h;
    int			p;

    if (*bi->on
     && (bi->s->old != *bi->val || refresh))
    {
	p = V_UIPatch(bi->p);
	x = bi->x - V_PatchLeftOffset(p);
	y = bi->y - V_PatchTopOffset(p);
	w = V_PatchWidth(p);
	h = V_PatchHeight(p);

	if (y - ST_Y < 0)
	    I_Error("updateBinIcon: y - ST_Y < 0");

	if (*bi->val)
	    V_DrawPatch(bi->x, bi->y, p);
	else if (r_bandmode != BAND_COMPOSE)
	    V_CopyRect(x, y-ST_Y, st_backing_screen, w, h, x, y);

	bi->s->old = *bi->val;
    }

}

