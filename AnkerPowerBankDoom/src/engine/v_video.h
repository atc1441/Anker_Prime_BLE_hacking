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
//	Gamma correction LUT.
//	Functions to draw patches (by post) directly to screen.
//	Functions to blit a block to the screen.
//


#ifndef __V_VIDEO__
#define __V_VIDEO__

#include "doomtype.h"

// Needed because we are refering to patches.
#include "v_patch.h"

//
// VIDEO
//

#define CENTERY			(SCREENHEIGHT/2)



// haleyjd 08/28/10: implemented for Strife support
// haleyjd 08/28/10: Patch clipping callback, implemented to support Choco
// Strife.



// Allocates buffer screens, call before R_Init.
void V_Init (void);

// Draw a block from the specified source screen to the screen.

void V_CopyRect(int srcx, int srcy, byte *source,
                int width, int height,
                int destx, int desty);

// Pictures are patch ids of the image, a negative id draws nothing.
void V_DrawPatch(int x, int y, int patch);
void V_DrawPatchFlipped(int x, int y, int patch);
void V_DrawPatchDirect(int x, int y, int patch);
int V_PatchWidth(int patch);
int V_PatchHeight(int patch);
int V_PatchLeftOffset(int patch);
int V_PatchTopOffset(int patch);
// Patch id of a dmc_ui picture, -1 when the pack does not have it.
int V_UIPatch(int ui);

// Draw a linear block of pixels into the view buffer.

void V_DrawBlock(int x, int y, int width, int height, byte *src);

void V_MarkRect(int x, int y, int width, int height);

// Temporarily switch to using a different buffer to draw graphics, etc.

void V_UseBuffer(byte *buffer);

// Return to using the normal screen buffer to draw graphics.

void V_RestoreBuffer(void);

#endif

