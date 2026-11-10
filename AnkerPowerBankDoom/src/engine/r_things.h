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
//	Rendering of moving objects, sprites.
//


#ifndef __R_THINGS__
#define __R_THINGS__



#define MAXVISSPRITES  	128

// Pass 1 records and the vissprites of the strip (port/render/r_frame.c)
extern void*		r_sprs;
extern int		r_nsprs;
extern byte*		r_secbits;
extern vissprite_t*	vissprites;
extern vissprite_t*	vissprite_p;
extern vissprite_t*	vissprites_end;
extern byte*		r_sortidx;
extern short*		clipbot;
extern short*		cliptop;

// vars for R_DrawMaskedColumn
extern short*		mfloorclip;
extern short*		mceilingclip;
extern int		mfloorconst;
extern int		mceilconst;
extern fixed_t		spryscale;
extern fixed_t		sprtopscreen;

extern fixed_t		pspritescale;
extern fixed_t		pspriteiscale;


// Posts as the image stores them {topdelta, length, pixels}, 0xff ends.
void R_DrawMaskedColumn (const byte* column);
void R_SetMaskClip (drawseg_t* ds);

void R_AddSprites (secidx_t sec);
void R_InitSprites (void);
void R_ClearSprites (void);
int R_DrawMasked (void);

#endif
