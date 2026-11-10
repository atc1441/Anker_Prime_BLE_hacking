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
//  Refresh module, data I/O, caching, retrieval of graphics
//  by name.
//


#ifndef __R_DATA__
#define __R_DATA__

#include "r_defs.h"
#include "r_state.h"


// Column buffers and their pipelines (r_data.c). Walls and sky: queue
// every column, end the strip's walls with R_ColumnsEnd. Masked columns:
// a run of R_QueuePost between R_PostsBegin and R_PostsEnd. Every queued
// column is drawn with the dc_* values it was queued with.
void R_ColumnsBegin (void);
void R_ColumnsEnd (int draw);
void R_QueueColumn (int tex, int col, int sky);
void R_PostsBegin (int masked, int id);
void R_QueuePost (int col);
void R_PostsEnd (void);
// Empties the column contexts of pass 2 (end of pass 1).
void R_ColumnCtxClear (void);
// Posts of a picture column {topdelta, length, pixels}, 0xff ends them.
byte* R_PatchPosts (int patch, int col);
// NULL without flat slots (plan D), ds_solid then holds the colour.
byte* R_GetFlat (int picnum);
fixed_t R_TextureHeight (int tex);
void R_PatchHeader (int patch, dmc_patch_t* h);

extern int	numtextures;
#ifdef DMC_ATTR
extern int	dc_texheight;
extern int	dc_ispost;
#endif
extern int	r_walllod;
#ifdef DMC_HOST
// --column-check (host/main_host.c): what the renderer reads through its
// column contexts is checked against the data layer's own requests, and
// its cached fragment setups against the setups computed again.
extern int	r_columncheck;
#endif
extern int	numflats;

// Called by R_Init.
void R_InitData (void);

#endif
