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
//	BSP traversal, handling of LineSegs for rendering.
//	Pass 1 of the strip renderer: the BSP is walked once per frame at
//	full width, R_StoreWallRange becomes a record of the fragment
//	{seg, x1, x2}, R_AddSprites records the visible things.
//




#include "doomdef.h"

#include "m_bbox.h"

#include "i_system.h"

#include "r_main.h"
#include "r_bsp.h"
#include "r_plane.h"
#include "r_things.h"
#include "r_segs.h"

// State.
#include "doomstat.h"
#include "r_state.h"

#include "r_strip.h"
#include "rview.h"



seg_t		curline;	// seg being drawn, decoded
segidx_t	curlineidx;
side_t		sidedef;
line_t		linedef;
secidx_t	frontsector;
secidx_t	backsector;	// IDX_NONE for one sided lines

// sector values the pass reads more than once
rsector_t	frontsec;


//
// Pass 1 lists, in the frame region (port/render/r_frame.c).
//
uint32_t*	r_frags;	// seg << 18 | x1 << 9 | x2
int		r_nfrags;
unsigned short*	r_bspstack;
byte*		r_segrun;	// seg run of a subsector (pass-1 scratch)


void R_ReadSector (int sec, rsector_t* s)
{
    rv_sector(sec, s);
}


//
// R_RecordWallRange
// Vanilla R_StoreWallRange in pass 1: the fragment is kept for the
// strips, dropped beyond MAXDRAWSEGS, and marks the line for the map.
//
static void
R_RecordWallRange
( int	start,
  int	stop )
{
    // don't overflow and crash
    if (r_nfrags == R_MAXFRAGS)
    {
	RSTAT(frags_dropped, 1);
	return;
    }

#ifdef RANGECHECK
    if (start >=viewwidth || start > stop)
	I_Error ("Bad R_RenderWallRange: %i to %i", start , stop);
#endif

    r_frags[r_nfrags++] = (uint32_t) curlineidx << 18 | (uint32_t) start << 9 | (uint32_t) stop;
    RSTAT(frags, 1);

    // mark the segment as visible for auto map
    P_MapLine(curline.linedef);
}



//
// ClipWallSegment
// Clips the given range of columns
// and includes it in the new clip list.
//
// newend is one past the last valid seg
cliprange_t*	newend;
cliprange_t*	solidsegs;	// R_MAXSOLID entries in the frame region


//
// R_ClipSolidWallSegment
// Does handle solid walls,
//  e.g. single sided LineDefs (middle texture)
//  that entirely block the view.
// 
static void
R_ClipSolidWallSegment
( int			first,
  int			last )
{
    cliprange_t*	next;
    cliprange_t*	start;

    // Find the first range that touches the range
    //  (adjacent pixels are touching).
    start = solidsegs;
    while (start->last < first-1)
	start++;

    if (first < start->first)
    {
	if (last < start->first-1)
	{
	    // Post is entirely visible (above start),
	    //  so insert a new clippost.
	    R_RecordWallRange (first, last);
	    if (newend == solidsegs + R_MAXSOLID)
		I_Error ("R_ClipSolidWallSegment: more than %i ranges", R_MAXSOLID);
	    next = newend;
	    newend++;
	    
	    while (next != start)
	    {
		*next = *(next-1);
		next--;
	    }
	    next->first = first;
	    next->last = last;
	    return;
	}
		
	// There is a fragment above *start.
	R_RecordWallRange (first, start->first - 1);
	// Now adjust the clip size.
	start->first = first;	
    }

    // Bottom contained in start?
    if (last <= start->last)
	return;			
		
    next = start;
    while (last >= (next+1)->first-1)
    {
	// There is a fragment between two posts.
	R_RecordWallRange (next->last + 1, (next+1)->first - 1);
	next++;
	
	if (last <= next->last)
	{
	    // Bottom is contained in next.
	    // Adjust the clip size.
	    start->last = next->last;	
	    goto crunch;
	}
    }
	
    // There is a fragment after *next.
    R_RecordWallRange (next->last + 1, last);
    // Adjust the clip size.
    start->last = last;
	
    // Remove start+1 to next from the clip list,
    // because start now covers their area.
  crunch:
    if (next == start)
    {
	// Post just extended past the bottom of one post.
	return;
    }
    

    while (next++ != newend)
    {
	// Remove a post.
	*++start = *next;
    }

    newend = start+1;
}



//
// R_ClipPassWallSegment
// Clips the given range of columns,
//  but does not includes it in the clip list.
// Does handle windows,
//  e.g. LineDefs with upper and lower texture.
//
static void
R_ClipPassWallSegment
( int	first,
  int	last )
{
    cliprange_t*	start;

    // Find the first range that touches the range
    //  (adjacent pixels are touching).
    start = solidsegs;
    while (start->last < first-1)
	start++;

    if (first < start->first)
    {
	if (last < start->first-1)
	{
	    // Post is entirely visible (above start).
	    R_RecordWallRange (first, last);
	    return;
	}
		
	// There is a fragment above *start.
	R_RecordWallRange (first, start->first - 1);
    }

    // Bottom contained in start?
    if (last <= start->last)
	return;			
		
    while (last >= (start+1)->first-1)
    {
	// There is a fragment between two posts.
	R_RecordWallRange (start->last + 1, (start+1)->first - 1);
	start++;
	
	if (last <= start->last)
	    return;
    }
	
    // There is a fragment after *next.
    R_RecordWallRange (start->last + 1, last);
}



//
// R_ClearClipSegs
//
void R_ClearClipSegs (void)
{
    solidsegs[0].first = -0x7fff;
    solidsegs[0].last = -1;
    solidsegs[1].first = viewwidth;
    solidsegs[1].last = 0x7fff;
    newend = solidsegs+2;
}

//
// R_AddLine
// Clips the given segment
// and adds any visible pieces to the line list.
//
static void R_AddLine (segidx_t idx, const dmc_seg_t* rec)
{
    seg_t*		line = &curline;
    rsector_t		back;
    int			x1;
    int			x2;
    angle_t		angle1;
    angle_t		angle2;
    angle_t		span;
    angle_t		tspan;
    
    P_DecodeSeg(rec, line);
    curlineidx = idx;
    RSTAT(segs, 1);
#ifdef DMC_PARITY
    PAR_Seg(idx, line);
#endif

    // OPTIMIZE: quickly reject orthogonal back sides.
    angle1 = R_PointToAngle (line->v1x, line->v1y);
    angle2 = R_PointToAngle (line->v2x, line->v2y);
    
    // Clip to view edges.
    // OPTIMIZE: make constant out of 2*clipangle (FIELDOFVIEW).
    span = angle1 - angle2;
    
    // Back side? I.e. backface culling?
    if (span >= ANG180)
	return;		

    angle1 -= viewangle;
    angle2 -= viewangle;
	
    tspan = angle1 + clipangle;
    if (tspan > 2*clipangle)
    {
	tspan -= 2*clipangle;

	// Totally off the left edge?
	if (tspan >= span)
	    return;
	
	angle1 = clipangle;
    }
    tspan = clipangle - angle2;
    if (tspan > 2*clipangle)
    {
	tspan -= 2*clipangle;

	// Totally off the left edge?
	if (tspan >= span)
	    return;	
	angle2 = -clipangle;
    }
    
    // The seg is in the view range,
    // but not necessarily visible.
    angle1 = (angle1+ANG90)>>ANGLETOFINESHIFT;
    angle2 = (angle2+ANG90)>>ANGLETOFINESHIFT;
    x1 = R_ViewAngleToX (angle1);
    x2 = R_ViewAngleToX (angle2);

    // Does not cross a pixel?
    if (x1 == x2)
	return;				
	
    backsector = line->backsector;

    // Single sided line?
    if (backsector == IDX_NONE)
	goto clipsolid;		

    R_ReadSector (backsector, &back);

    // Closed door.
    if (back.ceilingheight <= frontsec.floorheight
	|| back.floorheight >= frontsec.ceilingheight)
	goto clipsolid;		

    // Window.
    if (back.ceilingheight != frontsec.ceilingheight
	|| back.floorheight != frontsec.floorheight)
	goto clippass;	
		
    // Reject empty lines used for triggers
    //  and special events.
    // Identical floor and ceiling on both sides,
    // identical light levels on both sides,
    // and no middle texture.
    if (back.ceilingpic == frontsec.ceilingpic
	&& back.floorpic == frontsec.floorpic
	&& back.lightlevel == frontsec.lightlevel
	&& !curline.hasmid)
    {
	return;
    }
    
				
  clippass:
    R_ClipPassWallSegment (x1, x2-1);	
    return;
		
  clipsolid:
    R_ClipSolidWallSegment (x1, x2-1);
}


//
// R_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true
//  if some part of the bbox might be visible.
//
static const byte checkcoord[12][4] =
{
    {3,0,2,1},
    {3,0,2,0},
    {3,1,2,0},
    {0},
    {2,0,2,1},
    {0,0,0,0},
    {3,1,3,0},
    {0},
    {2,0,3,1},
    {2,1,3,1},
    {2,1,3,0}
};


static boolean R_CheckBBox (fixed_t*	bspcoord)
{
    int			boxx;
    int			boxy;
    int			boxpos;

    fixed_t		x1;
    fixed_t		y1;
    fixed_t		x2;
    fixed_t		y2;
    
    angle_t		angle1;
    angle_t		angle2;
    angle_t		span;
    angle_t		tspan;
    
    cliprange_t*	start;

    int			sx1;
    int			sx2;
    
    // Find the corners of the box
    // that define the edges from current viewpoint.
    if (viewx <= bspcoord[BOXLEFT])
	boxx = 0;
    else if (viewx < bspcoord[BOXRIGHT])
	boxx = 1;
    else
	boxx = 2;
		
    if (viewy >= bspcoord[BOXTOP])
	boxy = 0;
    else if (viewy > bspcoord[BOXBOTTOM])
	boxy = 1;
    else
	boxy = 2;
		
    boxpos = (boxy<<2)+boxx;
    if (boxpos == 5)
	return true;
	
    x1 = bspcoord[checkcoord[boxpos][0]];
    y1 = bspcoord[checkcoord[boxpos][1]];
    x2 = bspcoord[checkcoord[boxpos][2]];
    y2 = bspcoord[checkcoord[boxpos][3]];
    
    // check clip list for an open space
    angle1 = R_PointToAngle (x1, y1) - viewangle;
    angle2 = R_PointToAngle (x2, y2) - viewangle;
	
    span = angle1 - angle2;

    // Sitting on a line?
    if (span >= ANG180)
	return true;
    
    tspan = angle1 + clipangle;

    if (tspan > 2*clipangle)
    {
	tspan -= 2*clipangle;

	// Totally off the left edge?
	if (tspan >= span)
	    return false;	

	angle1 = clipangle;
    }
    tspan = clipangle - angle2;
    if (tspan > 2*clipangle)
    {
	tspan -= 2*clipangle;

	// Totally off the left edge?
	if (tspan >= span)
	    return false;
	
	angle2 = -clipangle;
    }


    // Find the first clippost
    //  that touches the source post
    //  (adjacent pixels are touching).
    angle1 = (angle1+ANG90)>>ANGLETOFINESHIFT;
    angle2 = (angle2+ANG90)>>ANGLETOFINESHIFT;
    sx1 = R_ViewAngleToX (angle1);
    sx2 = R_ViewAngleToX (angle2);

    // Does not cross a pixel.
    if (sx1 == sx2)
	return false;			
    sx2--;
	
    start = solidsegs;
    while (start->last < sx2)
	start++;
    
    if (sx1 >= start->first
	&& sx2 <= start->last)
    {
	// The clippost contains the new span.
	return false;
    }

    return true;
}



//
// R_Subsector
// Add sprites of things in sector.
// Draw one or more line segments.
// The planes are found again for every fragment in the strips.
//
static void R_Subsector (int num)
{
    int			count;
    int			i;
    segidx_t		first;
    secidx_t		sector;
    dmc_seg_t*		run = (dmc_seg_t *) r_segrun;
	
#ifdef RANGECHECK
    if (num>=numsubsectors)
	I_Error ("R_Subsector: ss %i with numss = %i",
		 num,
		 numsubsectors);
#endif

    RSTAT(subsectors, 1);
    // all segs of the subsector in one read
    count = map_seg_run(num, run, MAP_SEGRUN_MAX, &first, &sector);
    frontsector = sector;
    R_ReadSector (frontsector, &frontsec);
#ifdef DMC_PARITY
    PAR_SegRun(num, run, count, first);
#endif

    R_AddSprites (frontsector);	

    for (i = 0; i < count; i++)
	R_AddLine (first + i, &run[i]);
}




//
// RenderBSPNode
// Renders all subsectors below a given node.
// Iterative: the nodes whose back side is still to be checked are on
// an explicit stack, and are read again when they come off it.
// Just call with BSP root.
void R_RenderBSPNode (int bspnum)
{
    node_t	bsp;
    fixed_t	box[4];
    int		side;
    int		sp = 0;

    // a map without nodes has one subsector
    if (bspnum < 0)
    {
	R_Subsector (0);
	return;
    }

    for (;;)
    {
	// Recursively divide front space.
	while (!(bspnum & NF_SUBSECTOR))
	{
	    if (sp == R_BSPSTACK)
		I_Error ("R_RenderBSPNode: BSP deeper than %i", R_BSPSTACK);
	    P_GetNode(bspnum, &bsp);
	    RSTAT(nodes, 1);
	    r_bspstack[sp++] = bspnum;
	    bspnum = bsp.children[R_PointOnSide (viewx, viewy, &bsp)];
	}
	R_Subsector (bspnum&(~NF_SUBSECTOR));

	// Possibly divide back space, nearest node first.
	for (;;)
	{
	    if (!sp)
		return;
	    bspnum = r_bspstack[--sp];
	    P_GetNode(bspnum, &bsp);
	    side = R_PointOnSide (viewx, viewy, &bsp);
	    P_GetNodeBox(bspnum, side^1, box);
	    if (R_CheckBBox (box))	
	    {
		bspnum = bsp.children[side^1];
		break;
	    }
	}
    }
}


//
// R_RenderBSPPass
// Pass 1 of a frame.
//
void R_RenderBSPPass (void)
{
    r_nfrags = 0;
    R_ClearClipSegs ();
    R_ClearSprites ();

    // The head node is the last node output.
    R_RenderBSPNode (numnodes-1);
    R_ColumnCtxClear ();
    R_FragCacheClear ();
}
