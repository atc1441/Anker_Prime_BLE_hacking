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
//	Refresh module, BSP traversal and handling.
//


#ifndef __R_BSP__
#define __R_BSP__



// Sector values read through rview.h
typedef struct rsector_s
{
    fixed_t	floorheight;
    fixed_t	ceilingheight;
    short	floorpic;
    short	ceilingpic;
    short	lightlevel;

} rsector_t;

void R_ReadSector (int sec, rsector_t* s);

extern seg_t		curline;
extern segidx_t		curlineidx;
extern side_t		sidedef;
extern line_t		linedef;
extern secidx_t		frontsector;
extern secidx_t		backsector;
extern rsector_t	frontsec;

// Pass 1 lists in the frame region
extern uint32_t*	r_frags;
extern int		r_nfrags;
extern unsigned short*	r_bspstack;
extern byte*		r_segrun;
typedef	struct
{
    short	first;
    short	last;
    
} cliprange_t;

extern cliprange_t*	solidsegs;

void R_ClearClipSegs (void);
void R_RenderBSPNode (int bspnum);
void R_RenderBSPPass (void);

#endif
