//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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
//	Gamma correction LUT stuff.
//	Functions to draw patches (by post) directly to screen.
//	Functions to blit a block to the screen.
//	The status band has no screen buffer: there V_DrawPatch and
//	V_CopyRect mark dirty columns (record pass) or draw clipped into a
//	band strip (compose pass), see port/render/r_band.c. The front-end
//	screens are composed the same way, their record pass hashes the
//	V_DrawPatch calls (port/render/r_screen.c). The menus and the
//	automap of the host still draw into the temporary screen buffer
//	I_VideoBuffer.
//

#include <stdio.h>
#include <string.h>

#include "i_system.h"

#include "doomtype.h"

#include "deh_str.h"
#include "i_swap.h"
#include "i_video.h"
#include "m_bbox.h"
#include "m_misc.h"
#include "v_video.h"
#include "fetch.h"
#include "r_data.h"
#include "z_zone.h"

#include "st_stuff.h"

#include "r_strip.h"


// TODO: There are separate RANGECHECK defines for different games, but this
// is common code. Fix this.
#define RANGECHECK

// The screen buffer that the v_video.c code draws to.

static byte *dest_screen = NULL;



//
// V_MarkRect 
// Nothing reads a dirty box in this port: the status band compositor
// keeps its own dirty columns (port/render/r_band.c).
// 
void V_MarkRect(int x, int y, int width, int height) 
{ 
} 
 

//
// V_CopyRect 
// In the band a copy from the backing screen is the status bar
// background inside the rectangle.
// 
void V_CopyRect(int srcx, int srcy, byte *source,
                int width, int height,
                int destx, int desty)
{ 
    byte *src;
    byte *dest; 
    r_bandwin_t win;
 
    if (r_bandmode == BAND_RECORD)
    {
        R_BandMark(destx, destx + width - 1);
        return;
    }
    if (r_bandmode == BAND_COMPOSE)
    {
        win = r_bandwin;
        if (destx > r_bandwin.cx0)
            r_bandwin.cx0 = destx;
        if (destx + width - 1 < r_bandwin.cx1)
            r_bandwin.cx1 = destx + width - 1;
        if (desty > r_bandwin.cy0)
            r_bandwin.cy0 = desty;
        if (desty + height - 1 < r_bandwin.cy1)
            r_bandwin.cy1 = desty + height - 1;
        if (r_bandwin.cx0 <= r_bandwin.cx1 && r_bandwin.cy0 <= r_bandwin.cy1)
            ST_BandBackground();
        r_bandwin = win;
        return;
    }

#ifdef RANGECHECK 
    if (srcx < 0
     || srcx + width > SCREENWIDTH
     || srcy < 0
     || srcy + height > SCREENHEIGHT 
     || destx < 0
     || destx + width > SCREENWIDTH
     || desty < 0
     || desty + height > SCREENHEIGHT)
    {
        I_Error ("Bad V_CopyRect");
    }
#endif 

    if (!dest_screen || !source)
        return;

    V_MarkRect(destx, desty, width, height); 
 
    src = source + SCREENWIDTH * srcy + srcx; 
    dest = dest_screen + SCREENWIDTH * desty + destx; 

    for ( ; height>0 ; height--) 
    { 
        memcpy(dest, src, width); 
        src += SCREENWIDTH; 
        dest += SCREENWIDTH; 
    } 
} 
 
//
// Pictures are patches of the image, by patch id (fetch.h). A negative
// id is a picture the pack does not have and draws nothing.
//

int V_PatchWidth(int patch)
{
    dmc_patch_t h;

    if (patch < 0)
        return 0;
    R_PatchHeader(patch, &h);
    return h.width;
}

int V_PatchHeight(int patch)
{
    dmc_patch_t h;

    if (patch < 0)
        return 0;
    R_PatchHeader(patch, &h);
    return h.height;
}

int V_PatchLeftOffset(int patch)
{
    dmc_patch_t h;

    if (patch < 0)
        return 0;
    R_PatchHeader(patch, &h);
    return h.leftoffset;
}

int V_PatchTopOffset(int patch)
{
    dmc_patch_t h;

    if (patch < 0)
        return 0;
    R_PatchHeader(patch, &h);
    return h.topoffset;
}

int V_UIPatch(int ui)
{
    return ui_patch(ui);
}

//
// V_DrawPatch
// Masks a column based masked pic to the screen. 
// Posts are {topdelta, length, pixels[length]}, 0xFF ends a column.
//

static void V_DrawPatchCols(int x, int y, int patch, int flip)
{ 
    int count;
    int col;
    int w;
    int top;
    int row0;
    int rows;
    int x1;
    byte *column;
    byte *desttop;
    byte *dest;
    byte *source;
    dmc_patch_t h;

    if (patch < 0)
        return;

    R_PatchHeader(patch, &h);
    y -= h.topoffset;
    x -= h.leftoffset;
    w = h.width;

    if (r_bandmode == BAND_RECORD)
    {
        R_BandMark(x, x + w - 1);
        return;
    }

    if (r_bandmode == BAND_HASH)
    {
        // the call is the picture, where it goes and which way round
        R_ScreenMark(x, x + w - 1, y, y + h.height - 1,
                     (uint32_t)patch << 19 | (uint32_t)flip << 18
                     | (uint32_t)(x & 0x1ff) << 9 | (uint32_t)(y & 0x1ff));
        return;
    }

    if (r_bandmode == BAND_COMPOSE)
    {
        // columns cx0..cx1 and rows cy0..cy1 of the band strip
        x1 = x + w - 1;
        col = x < r_bandwin.cx0 ? r_bandwin.cx0 - x : 0;
        if (x1 > r_bandwin.cx1)
            x1 = r_bandwin.cx1;
        row0 = r_bandwin.cy0 - r_bandwin.y0;
        rows = r_bandwin.cy1 - r_bandwin.y0 + 1;
        for (x += col; x <= x1; x++, col++)
        {
            column = R_PatchPosts(patch, flip ? w-1-col : col);
            desttop = r_bandwin.buf + (x - r_bandwin.x0) * r_bandwin.h;
            for ( ; column[0] != 0xff; column += column[1] + 2)
            {
                source = column + 2;
                count = column[1];
                top = y + column[0] - r_bandwin.y0;
                for ( ; count--; top++, source++)
                    if (top >= row0 && top < rows)
                        desttop[top] = *source;
            }
        }
        return;
    }

#ifdef RANGECHECK
    if (x < 0
     || x + h.width > SCREENWIDTH
     || y < 0
     || y + h.height > SCREENHEIGHT)
    {
        I_Error("Bad V_DrawPatch x=%i y=%i patch.width=%i patch.height=%i topoffset=%i leftoffset=%i", x, y, h.width, h.height, h.topoffset, h.leftoffset);
    }
#endif

    if (!dest_screen)
        return;

    V_MarkRect(x, y, h.width, h.height);

    col = 0;
    desttop = dest_screen + y * SCREENWIDTH + x;

    for ( ; col<w ; x++, col++, desttop++)
    {
        column = R_PatchPosts(patch, flip ? w-1-col : col);

        // step through the posts in a column
        while (column[0] != 0xff)
        {
            source = column + 2;
            dest = desttop + column[0]*SCREENWIDTH;
            count = column[1];

            while (count--)
            {
                *dest = *source++;
                dest += SCREENWIDTH;
            }
            column += column[1] + 2;
        }
    }
}

void V_DrawPatch(int x, int y, int patch)
{
    V_DrawPatchCols(x, y, patch, 0);
}

//
// V_DrawPatchFlipped
// Masks a column based masked pic to the screen.
// Flips horizontally, e.g. to mirror face.
//

void V_DrawPatchFlipped(int x, int y, int patch)
{
    V_DrawPatchCols(x, y, patch, 1);
}



//
// V_DrawPatchDirect
// Draws directly to the screen on the pc. 
//

void V_DrawPatchDirect(int x, int y, int patch)
{
    V_DrawPatch(x, y, patch); 
} 

//
// V_DrawBlock
// Draw a linear block of pixels into the view buffer.
//

void V_DrawBlock(int x, int y, int width, int height, byte *src) 
{ 
    byte *dest; 
 
#ifdef RANGECHECK 
    if (x < 0
     || x + width >SCREENWIDTH
     || y < 0
     || y + height > SCREENHEIGHT)
    {
	I_Error ("Bad V_DrawBlock");
    }
#endif 

    if (!dest_screen)
        return;
 
    V_MarkRect (x, y, width, height); 
 
    dest = dest_screen + y * SCREENWIDTH + x; 

    while (height--) 
    { 
	memcpy (dest, src, width); 
	src += width; 
	dest += SCREENWIDTH; 
    } 
} 

//
// V_Init
// 
void V_Init (void) 
{ 
    // no-op!
    // There used to be separate screens that could be drawn to; these are
    // now handled in the upper layers.
}

// Set the buffer that the code draws to.

void V_UseBuffer(byte *buffer)
{
    dest_screen = buffer;
}

// Restore screen buffer to the i_video screen buffer.

void V_RestoreBuffer(void)
{
    dest_screen = I_VideoBuffer;
}

