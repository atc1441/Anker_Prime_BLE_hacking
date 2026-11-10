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
//	LineOfSight/Visibility checks, uses REJECT Lookup Table.
//



#include "doomdef.h"

#include "i_system.h"
#include "p_local.h"

// State.
#include "r_state.h"

// P_CrossSubsector keeps its opening in locals of these names.
#undef opentop
#undef openbottom

//
// P_CheckSight
//
fixed_t		sightzstart;		// eye z of looker
fixed_t		topslope;
fixed_t		bottomslope;		// slopes to top and bottom of target

divline_t	strace;			// from t1 to t2
fixed_t		t2x;
fixed_t		t2y;


//
// P_DivlineSide
// Returns side 0 (front), 1 (back), or 2 (on).
//
int
P_DivlineSide
( fixed_t	x,
  fixed_t	y,
  divline_t*	node )
{
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;

    if (!node->dx)
    {
	if (x==node->x)
	    return 2;
	
	if (x <= node->x)
	    return node->dy > 0;

	return node->dy < 0;
    }
    
    if (!node->dy)
    {
	if (x==node->y)
	    return 2;

	if (y <= node->y)
	    return node->dx < 0;

	return node->dx > 0;
    }
	
    dx = (x - node->x);
    dy = (y - node->y);

    left =  (node->dy>>FRACBITS) * (dx>>FRACBITS);
    right = (dy>>FRACBITS) * (node->dx>>FRACBITS);
	
    if (right < left)
	return 0;	// front side
    
    if (left == right)
	return 2;
    return 1;		// back side
}


//
// P_InterceptVector2
// Returns the fractional intercept point
// along the first divline.
// This is only called by the addthings and addlines traversers.
//
fixed_t
P_InterceptVector2
( divline_t*	v2,
  divline_t*	v1 )
{
    fixed_t	frac;
    fixed_t	num;
    fixed_t	den;
	
    den = FixedMul (v1->dy>>8,v2->dx) - FixedMul(v1->dx>>8,v2->dy);

    if (den == 0)
	return 0;
    //	I_Error ("P_InterceptVector: parallel");
    
    num = FixedMul ( (v1->x - v2->x)>>8 ,v1->dy) + 
	FixedMul ( (v2->y - v1->y)>>8 , v1->dx);
    frac = FixedDiv (num , den);

    return frac;
}

//
// P_CrossSubsector
// Returns true
//  if strace crosses the given subsector successfully.
//
boolean P_CrossSubsector (int num)
{
    seg_t		seg;
    line_t		l;
    line_t*		line = &l;
    int			s1;
    int			s2;
    int			count;
    int			segnum;
    fixed_t		opentop;
    fixed_t		openbottom;
    divline_t		divl;
    fixed_t		frac;
    fixed_t		slope;
    fixed_t		frontfloor;
    fixed_t		frontceil;
    fixed_t		backfloor;
    fixed_t		backceil;
	
#ifdef RANGECHECK
    if (num>=numsubsectors)
	I_Error ("P_CrossSubsector: ss %i with numss = %i",
		 num,
		 numsubsectors);
#endif

    // check lines
    count = P_SubsecSegs(num, &segnum);

    for ( ; count ; segnum++, count--)
    {
	P_GetSeg(segnum, &seg);

	// allready checked other side?
	if (P_LineChecked(seg.linedef))
	    continue;

	P_GetLine(seg.linedef, &l);
	s1 = P_DivlineSide (line->v1x, line->v1y, &strace);
	s2 = P_DivlineSide (line->v2x, line->v2y, &strace);

	// line isn't crossed?
	if (s1 == s2)
	    continue;
	
	divl.x = line->v1x;
	divl.y = line->v1y;
	divl.dx = line->v2x - line->v1x;
	divl.dy = line->v2y - line->v1y;
	s1 = P_DivlineSide (strace.x, strace.y, &divl);
	s2 = P_DivlineSide (t2x, t2y, &divl);

	// line isn't crossed?
	if (s1 == s2)
	    continue;	

        // Backsector may be NULL if this is an "impassible
        // glass" hack line.

        if (line->backsector == IDX_NONE)
        {
            return false;
        }

	// stop because it is not two sided anyway
	// might do this after updating validcount?
	if ( !(line->flags & ML_TWOSIDED) )
	    return false;
	
	// crosses a two sided line
	P_SectorHeights(seg.side ? line->backsector : line->frontsector, &frontfloor, &frontceil);
	P_SectorHeights(seg.backsector, &backfloor, &backceil);

	// no wall to block sight with?
	if (frontfloor == backfloor
	    && frontceil == backceil)
	    continue;	

	// possible occluder
	// because of ceiling height differences
	if (frontceil < backceil)
	    opentop = frontceil;
	else
	    opentop = backceil;

	// because of ceiling height differences
	if (frontfloor > backfloor)
	    openbottom = frontfloor;
	else
	    openbottom = backfloor;
		
	// quick test for totally closed doors
	if (openbottom >= opentop)	
	    return false;		// stop
	
	frac = P_InterceptVector2 (&strace, &divl);
		
	if (frontfloor != backfloor)
	{
	    slope = FixedDiv (openbottom - sightzstart , frac);
	    if (slope > bottomslope)
		bottomslope = slope;
	}
		
	if (frontceil != backceil)
	{
	    slope = FixedDiv (opentop - sightzstart , frac);
	    if (slope < topslope)
		topslope = slope;
	}
		
	if (topslope <= bottomslope)
	    return false;		// stop				
    }
    // passed the subsector ok
    return true;		
}



//
// P_CrossBSPNode
// Returns true
//  if strace crosses the given node successfully.
// Iterative: a node waits on the stack (with the side of the start
// point) while its first child is crossed and is fetched again when
// popped. The E1 maps have at most 44 levels, the converter checks 64.
//
#define stack		(dmc_tic.q.sight)

boolean P_CrossBSPNode (int bspnum)
{
    node_t		node;
    int			sp = 0;
    int			side;

    for (;;)
    {
	while (!(bspnum & NF_SUBSECTOR))
	{
	    P_GetNode(bspnum, &node);
    
	    // decide which side the start point is on
	    side = P_DivlineSide (strace.x, strace.y, (divline_t *)&node);
	    if (side == 2)
		side = 0;	// an "on" should cross both sides

	    // cross the starting side
	    if (sp == SIGHTSTACK)
		I_Error ("P_CrossBSPNode: BSP deeper than %i", SIGHTSTACK);
	    stack[sp++] = bspnum | side << 15;
	    bspnum = node.children[side];
	}

	if (bspnum == -1)
	{
	    if (!P_CrossSubsector (0))
		return false;
	}
	else if (!P_CrossSubsector (bspnum&(~NF_SUBSECTOR)))
	    return false;

	// the side crossed fine, go back up
	for (;;)
	{
	    if (!sp)
		return true;
	    sp--;
	    side = stack[sp] >> 15;
	    P_GetNode(stack[sp] & 0x7fff, &node);

	    // the partition plane is crossed here
	    if (side != P_DivlineSide (t2x, t2y, (divline_t *)&node))
	    {
		// cross the ending side
		bspnum = node.children[side^1];
		break;
	    }
	    // the line doesn't touch the other side
	}
    }
}


//
// P_CheckSight
// Returns true
//  if a straight line between t1 and t2 is unobstructed.
// Uses REJECT.
//
boolean
P_CheckSight
( mobj_t*	t1,
  mobj_t*	t2 )
{
    int		s1;
    int		s2;
    
#ifdef DMC_STATS
    HS_Event (HS_E_SIGHT, NULL);
#endif
    // First check for trivial rejection.

    // Determine subsector entries in REJECT table.
    s1 = t1->sector;
    s2 = t2->sector;

    // Check in REJECT table.
    if (P_Rejected(s1, s2))
    {
	// can't possibly be connected
	return false;	
    }

    // An unobstructed LOS is possible.
    // Now look from eyes of t1 to any part of t2.

    P_NewLineCheck();
	
    sightzstart = t1->z + P_Height(t1) - (P_Height(t1)>>2);
    topslope = (t2->z+P_Height(t2)) - sightzstart;
    bottomslope = (t2->z) - sightzstart;
	
    strace.x = t1->x;
    strace.y = t1->y;
    t2x = t2->x;
    t2y = t2->y;
    strace.dx = t2->x - t1->x;
    strace.dy = t2->y - t1->y;

    // the head node is the last node output
    return P_CrossBSPNode (numnodes-1);	
}


