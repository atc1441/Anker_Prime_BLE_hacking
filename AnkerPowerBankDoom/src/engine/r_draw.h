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


#ifndef __R_DRAW__
#define __R_DRAW__




extern lighttable_t*	dc_colormap;
extern int		dc_x;
extern int		dc_yl;
extern int		dc_yh;
extern fixed_t		dc_iscale;
extern fixed_t		dc_texturemid;

// first pixel in a column
extern byte*		dc_source;		
// row of dc_source: (texture position << dc_fracshift) >> dc_rowshift
extern int		dc_fracshift;
extern int		dc_rowshift;


// player translation 1-3, 0 none
extern int		dc_translation;


// The column drawers, both detail modes.
void 	R_DrawColumn (void);

// The Spectre/Invisibility effect.
void 	R_DrawFuzzColumn (void);

// Draw with color translation tables,
//  for player sprite rendering,
//  Green/Red/Blue/Indigo shirts.
void	R_DrawTranslatedColumn (void);

extern int		ds_y;
extern int		ds_x1;
extern int		ds_x2;

extern lighttable_t*	ds_colormap;

// packed texture position of ds_x1 and its step (R_MapPlane)
extern unsigned		ds_position;
extern unsigned		ds_step;

// start of a 32*32 tile image
extern byte*		ds_source;		
// colour index of a solid span
extern byte		ds_solid;


// Span blitting for rows, floor/ceiling.
// No Sepctre effect needed.
void 	R_DrawSpan (void);

// One colour through the light row, both detail modes (solid floors).
void	R_DrawSpanSolid (void);



#endif
