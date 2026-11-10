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
//	Game completion, final screen animation.
//


#include "d_main.h"
#include "doomstat.h"
#include "hu_stuff.h"
#include "v_video.h"

#include "dmcdat.h"
#include "fetch.h"

#include "f_finale.h"
#include "fe.h"
#include "r_screen.h"
#include "r_strip.h"

// The state of the finale is in the front-end scratch (port/game/fe.h).
// F_Ticker and F_Responder are resident (port/game/fe_glue.c).
#define F	(fe_scr.u.fin)

#define	TEXTSPEED	3
#define	TEXTWAIT	250

//
// F_OvlStart
//
void F_OvlStart (void)
{
    char	part[32];
    uint32_t	len;
    int		n;
    int		i;

    gameaction = ga_nothing;
    gamestate = GS_FINALE;
    viewactive = false;
    automapactive = false;

    // The image has the text of episode 1 and its 64x64 background flat.
    len = 0;
    if (gameepisode == 1)
    {
	do
	{
	    n = str_part(DMC_STR_E1TEXT, len, part, sizeof(part));
	    len += n;
	} while (n == (int)sizeof(part));
    }
    F.endcount = len*TEXTSPEED + TEXTWAIT;
    F.stage = F_STAGE_TEXT;
    F.count = 0;

    for (i = 0; i < HU_FONTSIZE; i++)
	F.width[i] = V_PatchWidth(HU_FONT(i));
}


//
// F_TextWrite
// The screen is composed in pieces (port/render/r_screen.h): the record
// pass hashes the glyphs with their widths from the table, the compose
// pass tiles the flat into the piece and draws the glyphs that reach it.
//

_Static_assert(R_FE_BUFBYTES / SCREENHEIGHT <= 8, "a piece of the whole screen");

// The 64x64 flat tiled over the piece
static void F_Background (void)
{
    byte	px[8];
    int		n = r_bandwin.x1 - r_bandwin.x0 + 1;
    int		a = r_bandwin.x0 & 63;
    int		k = n < 64 - a ? n : 64 - a;
    int		r;
    int		x;
    int		y;

    for (r = 0; r < 64; r++)
    {
	flat_finale(r * 64 + a, px, k);
	if (k < n)
	    flat_finale(r * 64, px + k, n - k);
	for (y = (r - r_bandwin.y0) & 63; y < r_bandwin.h; y += 64)
	    for (x = 0; x < n; x++)
		r_bandwin.buf[x * r_bandwin.h + y] = px[x];
    }
}

static int F_Glyph (int c, int x, int y)
{
    int		w = F.width[c];

    if (x + w > SCREENWIDTH)
	return -1;
    if (r_bandmode == BAND_HASH)
	R_ScreenMark(x, x + w - 1, y, y, (uint32_t)c << 18 | (uint32_t)x << 9 | (uint32_t)y);
    else if (x <= r_bandwin.cx1 && x + w > r_bandwin.cx0)
	V_DrawPatch(x, y, HU_FONT(c));
    return w;
}

static void F_TextWrite (void)
{
    int		count;

    if (r_bandmode == BAND_COMPOSE)
	F_Background();

    // draw some of the text onto the screen
    count = ((signed int) F.count - 10) / TEXTSPEED;
    if (count > 0 && gameepisode == 1)
	scr_text(DMC_STR_E1TEXT, 0, 10, 10, 11, count, F_Glyph);
}

// A picture over the whole screen. The E1 packs have no CREDIT, the
// title page takes its place.
static void F_DrawPicture (int ui)
{
    int		p = V_UIPatch(ui);

    if (p < 0)
	p = V_UIPatch(DMC_UI_TITLEPIC);
    V_DrawPatch(0, 0, p);
}

// Ultimate Doom ends episode 1 on CREDIT
static void F_ArtScreen (void)
{
    F_DrawPicture(DMC_UI_CREDIT);
}

static void F_Page (void)
{
    F_DrawPicture(pagepic);
}

//
// F_OvlDrawer
//
void F_OvlDrawer (void)
{
    R_ScreenDraw(FE_SCR_FINALE + F.stage, &scr_full,
		 F.stage == F_STAGE_TEXT ? F_TextWrite : F_ArtScreen, NULL);
}

//
// F_OvlPage
// The page of the title loop (D_PageDrawer)
//
void F_OvlPage (void)
{
    R_ScreenDraw(FE_SCR_PAGE, &scr_full, F_Page, NULL);
}
