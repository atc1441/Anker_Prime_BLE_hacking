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
//	Refresh module, drawing LineSegs from BSP.
//


#ifndef __R_SEGS__
#define __R_SEGS__




// Drawsegs of the current strip (port/render/r_frame.c).
extern drawseg_t*	drawsegs;
extern drawseg_t*	ds_p;
extern drawseg_t*	drawsegs_end;

void
R_RenderMaskedSegRange
( drawseg_t*	ds,
  int		x1,
  int		x2 );

// Draws the strip part of pass-1 fragment i, whose value is frag.
void R_ReplayWall (uint32_t frag, int i);

// Called at the end of every pass 1.
void R_FragCacheClear (void);

// Openings of the strip: n values, the index of the first, -1 when
// the pool is full.
int R_NewOpenings (int n);


#endif
