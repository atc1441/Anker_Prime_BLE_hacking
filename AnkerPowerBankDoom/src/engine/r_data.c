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
//	Graphics data of the NOR image: texture columns, flats,
//	sprite frames and patches, the colormap in flash.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i_system.h"
#include "z_zone.h"

#include "doomdef.h"
#include "m_misc.h"
#include "r_local.h"
#include "p_local.h"

#include "doomstat.h"
#include "r_sky.h"

#include "r_data.h"

#include "cache.h"
#include "fetch.h"
#include "img.h"
#include "nor.h"
#include "r_strip.h"
#include "trig_tables.h"


// Texture numbers are global ids + 1, 0 is no texture.
int		numtextures;
int		numflats;

// for global animation

lighttable_t	*colormaps;

#ifdef DMC_ATTR
// Host frame test: rows of the column being drawn that hold texture data.
// The vanilla drawer can read further (rows & 127), there the image has
// the rows repeated where vanilla has the bytes that follow the column.
// dc_ispost is set when the column is a post of a sprite or masked
// texture (R_DrawMaskedColumn), where dc_texheight is the bytes of the
// vanilla layout from the post on.
int		dc_texheight;
int		dc_ispost;
#endif

// wall LOD by the vertical texel step
int		r_walllod = 1;

#ifdef DMC_HOST
int		r_columncheck;
#endif


//
// R_TextureHeight
// Height of a texture in fixed point, 0 for no texture.
//
fixed_t R_TextureHeight (int tex)
{
    dmc_texent_t	e;

    if (!tex)
	return 0;
    tex_entry(tex - 1, &e);
#ifdef DMC_PARITY
    PAR_TexEntry(tex - 1, &e);
#endif
    return e.height << FRACBITS;
}


//
// Column buffers. Walls and the sky use two of 128 B, masked columns
// (sprites, masked mids) and pictures two of 256 B over the same bytes:
// the walls of a strip are drawn before its masked columns, and what the
// buffers hold is forgotten at every switch (R_ColumnsBegin).
// A column is queued with its request sent and drawn when the next one
// is queued, so its NOR read runs while the column before it is drawn.
// Reads go straight to the NOR bus when it is free (R_ColRead), one at a
// time: the request of a column first ends the read of the column queued
// before it, which is then drawn while the new read runs.
//
static byte	colmem[2][FETCH_COLBUF] __attribute__((aligned(4)));

#define WALLBUF(i)	(colmem[0] + 128*(i))
#define POSTBUF(i)	(colmem[i])

// What a buffer holds: the request key, the stored rows start..start+len-1
// (around the stored height) and the ticket of the read filling it, -1
// for the direct read that may still be on the bus.
typedef struct
{
    int32_t	key;		// -1: nothing
    uint16_t	start;
    uint16_t	len;
    int		ticket;
} colbuf_t;

static colbuf_t	cbuf[2];
static int	clast;		// buffer of the column queued last

// A queued column. No pointers, so the size is the same on 64-bit hosts:
// the colormap is kept as its offset in colormaps.
typedef struct
{
    fixed_t		texturemid;
    fixed_t		iscale;
    fixed_t		topscreen;	// sprtopscreen, spryscale
    fixed_t		yscale;
    short		x;
    short		yl;
    short		yh;
    uint16_t		cmap;		// offset of the row in colormaps, 0xffff for NULL
    byte		buf;
    byte		post;
    byte		level;		// walls: mip level, log2 of the stored rows
    byte		bits;
#ifdef DMC_ATTR
    short		texheight;
#endif
} rcol_t;

// The column ring: the queued column and the one being queued, in two
// slots at the end of the pass-1 BSP stack, which no strip uses.
#define RING		((rcol_t *) ((byte *) r_bspstack + R_BSPSTACK * 2 - 64))

_Static_assert(2 * sizeof(rcol_t) <= 64, "the column ring must fit 64 B");

static rcol_t*	rq;		// slot of the queued column
static rcol_t*	rf;		// the other slot
static int	queued;		// rq holds a column

//
// Column contexts (pass 2): what a wall, sky or masked run needs per
// column, filled when a texture or patch is first drawn in the frame. They
// live in the pass-1 solidsegs list, which no strip uses, and are emptied
// at the end of every pass 1 (R_ColumnCtxClear).
//

// Where the data of a context is: its logical offset, the NOR address of
// that offset and the bytes readable there in one piece.
typedef struct
{
    uint32_t	loff;
    uint32_t	phys;
    uint32_t	run;
} r_colwin_t;

typedef struct
{
    uint16_t	tex;		// texture number, 0 for an empty slot
    uint8_t	base;		// stored base level
    uint8_t	nlevels;	// stored levels
    uint8_t	log2_w;
    uint8_t	log2_sh;
    int16_t	height;		// for the host frame test (DMC_ATTR)
    r_colwin_t	win;		// from the base level's first column
} r_texctx_t;

typedef struct
{
    uint16_t	id;		// patch id or texture number of the run
    uint8_t	masked;		// 1: masked texture, 0: patch, 2: empty
    uint8_t	shift;		// stored column = column >> shift
    int32_t	wmask;		// texture width mask, -1 for patches
    uint16_t	ncols;		// stored columns of a patch
    uint16_t	pad;
    uint32_t	colinfo;	// logical offset of the colinfo words
    r_colwin_t	win;		// from where the colinfo offsets count
} r_postctx_t;

typedef struct
{
    r_texctx_t	tex[4];		// walls and the sky
    r_postctx_t	post;		// the masked run
    uint32_t	next;		// texture slot the next fill takes
} r_colctx_t;

// No pointers: the same sizes on the meter and on 64-bit hosts.
_Static_assert(sizeof(r_texctx_t) == 20 && sizeof(r_postctx_t) == 28,
	       "column context sizes");
_Static_assert(sizeof(r_colctx_t) <= R_MAXSOLID * sizeof(cliprange_t),
	       "column contexts must fit solidsegs");

#define COLCTX	((r_colctx_t *) solidsegs)

#ifdef DMC_RSTATS
static uint32_t	stat_tx;
static uint32_t	stat_bytes;
static uint32_t	stat_px;

// NOR time of the request sent since R_StatReq (0), in cycles at the costs
// measured on the meter (2.8 us per transaction, 8 MB/s).
static uint32_t R_StatReq (int end)
{
    uint32_t	tx;
    uint32_t	bytes;

    R_StatsNor(&tx, &bytes);
    if (!end)
    {
	stat_tx = tx;
	stat_bytes = bytes;
	return 0;
    }
    RSTAT(colreq, 1);
    RSTAT(coltx, tx - stat_tx);
    RSTAT(colbytes, bytes - stat_bytes);
    return (tx - stat_tx) * 403 + (bytes - stat_bytes) * 18;
}

// A request of cost cycles was sent, then the queued column (if any) was
// drawn: the drawer waits for what that column did not cover.
static void R_StatWait (uint32_t cost, int drawn)
{
    uint32_t	over = drawn ? 260 + 18 * (r_stats.colpx + r_stats.fuzzpx - stat_px) : 0;

    if (cost > over)
	RSTAT(colwait, cost - over);
}
#define STAT_PX()	(stat_px = r_stats.colpx + r_stats.fuzzpx)
#else
#define STAT_PX()	((void)0)
#endif


//
// R_ColumnCtxClear
// Empties the column contexts: the end of pass 1, pass 2 follows.
//
void R_ColumnCtxClear (void)
{
    memset (COLCTX, 0, sizeof(r_colctx_t));
    COLCTX->post.masked = 2;
}


// The context of wall or sky texture tex, filled in the next slot when
// no slot holds it.
static r_texctx_t* R_TexCtx (int tex)
{
    r_texctx_t*		t = COLCTX->tex;
    dmc_texent_t	e;
    int			i;

    for (i = 0; i < 4; i++)
	if (t[i].tex == tex)
	    return &t[i];

    t += COLCTX->next++ & 3;
    tex_entry(tex - 1, &e);
#ifdef DMC_PARITY
    PAR_TexEntry(tex - 1, &e);
#endif
    if (!e.nlevels)
	dmc_fatal("texture level not stored");
    t->tex = tex;
    t->base = tex_base(&e);
    t->nlevels = e.nlevels;
    t->log2_w = e.log2_w;
    t->log2_sh = e.log2_sh;
    t->height = e.height;
    t->win.loff = img.sect[DMC_SECT_TEXDATA].off + e.data;
    t->win.phys = img_phys_run(t->win.loff, &t->win.run);
    RSTAT(ctx_fills, 1);
    return t;
}


#ifdef DMC_HOST
// --column-check: the offset and length a context gives against those of the
// data layer's own requests.
static void R_CheckCtx (const char* what, int id, int col, uint32_t off, uint32_t len,
			uint32_t want, uint32_t wantlen)
{
    if (off != want || len != wantlen)
	I_Error ("R_CheckCtx: %s %d column %d from %u, %u B, the data layer reads %u, %u B",
		 what, id, col, (unsigned) off, (unsigned) len, (unsigned) want, (unsigned) wantlen);
}
#endif


// Reads n bytes at rel of the data of w into dst for the buffer the
// queued column does not use and returns the ticket: -1 when the read went
// on the bus at once (direct set and the bus free), else that of the
// queued read. The direct read of the queued column ends first, so at
// most one buffer holds ticket -1 and nor_col_wait ends exactly its read.
static int R_ColRead (const r_colwin_t* w, uint32_t rel, byte* dst, unsigned n, int direct)
{
    nor_col_wait ();
    if (cbuf[clast].ticket < 0)
	cbuf[clast].ticket = 0;
    if (direct && rel + n <= w->run && !nor_col_start (w->phys + rel, dst, n))
    {
#ifdef DMC_STATS
	img_stat_hook (w->loff + rel, n, 1);
#endif
	RSTAT(col_direct, 1);
	return -1;
    }
    return img_read_async (w->loff + rel, dst, n, NOR_PRIO_COL);
}


//
// R_ColumnsBegin
// Forgets the buffers: walls or masked columns come next.
//
void R_ColumnsBegin (void)
{
    int		i;

    queued = 0;
    rq = RING;
    rf = rq + 1;
    // the direct read on the bus, then the queued ones
    nor_col_wait ();
    for (i = 0; i < 2; i++)
    {
	if (cbuf[i].ticket > 0)
	    fetch_wait(cbuf[i].ticket);
	cbuf[i].key = -1;
	cbuf[i].ticket = 0;
    }
    fetch_reset();
}


// The dc_* values and the sprite scale of the column being queued.
static void R_SaveColumn (rcol_t* c)
{
    c->x = dc_x;
    c->cmap = dc_colormap ? dc_colormap - colormaps : 0xffff;
    c->iscale = dc_iscale;
    c->texturemid = dc_texturemid;
    c->yl = dc_yl;
    c->yh = dc_yh;
    c->topscreen = sprtopscreen;
    c->yscale = spryscale;
}


// The dc_* values and the sprite scale of c.
static void R_LoadColumn (const rcol_t* c)
{
    dc_x = c->x;
    dc_colormap = c->cmap == 0xffff ? NULL : colormaps + c->cmap;
    dc_iscale = c->iscale;
    dc_texturemid = c->texturemid;
    dc_yl = c->yl;
    dc_yh = c->yh;
    sprtopscreen = c->topscreen;
    spryscale = c->yscale;
}


// The queued column is drawn and, when next is set, the column in rf is
// queued in its place. The dc_* globals are those of that column again
// afterwards.
static void R_DrawQueued (int next)
{
    rcol_t*	p = rq;
    colbuf_t*	b;
    int		bits;
    byte*	src;
    int		had = queued;

    queued = next;
    if (next)
    {
	rq = rf;
	rf = p;
    }
    if (!had)
	return;

    // -1: the direct read on the bus is this column's
    b = &cbuf[p->buf];
    if (b->ticket < 0)
	nor_col_wait ();
    else
	fetch_wait(b->ticket);
    b->ticket = 0;
    R_LoadColumn(p);
    if (p->post)
	R_DrawMaskedColumn(POSTBUF(p->buf));
    else
    {
	src = WALLBUF(p->buf);
	bits = p->bits;
	// one stored row: the drawer reads two
	if (!bits)
	{
	    src[1] = src[0];
	    bits = 1;
	}
	dc_source = src;
	dc_fracshift = 16 - p->level - bits;
	dc_rowshift = 32 - bits;
#ifdef DMC_ATTR
	dc_texheight = p->texheight;
	dc_ispost = 0;
#endif
	colfunc ();
    }
    if (next)
	R_LoadColumn(rq);
}


//
// R_ColumnsEnd
// Draws the queued column, or drops it (a strip that overflowed).
//
void R_ColumnsEnd (int draw)
{
    if (draw)
	R_DrawQueued(0);
    queued = 0;
}


//
// R_QueueColumn
// The wall or sky column dc_x, rows dc_yl..dc_yh (dc_texturemid,
// dc_iscale, dc_colormap) of texture tex, column col. Its rows are
// requested at the mip level its vertical step asks for, then the column
// queued before it is drawn.
//
void R_QueueColumn (int tex, int col, int sky)
{
    r_texctx_t*	t;
    rcol_t*	c;
    colbuf_t*	b;
    byte*	buf;
    uint32_t	rel;
    int		level;
    int		lod;
    int		rows;
    int		r0;
    int		y0;
    int		y1;
    int		mask;
    int		a;
    int		len;
    int		i;
    int		cx;
    int32_t	key;
    fixed_t	frac;
#ifdef DMC_RSTATS
    uint32_t	cost = 0;
    int		drawn = queued;
#endif

    if (dc_yh < dc_yl)
	return;

    t = R_TexCtx(tex);
    level = t->base;

    // mip level: 1-2 texels per pixel is level 0, 2-4 level 1, more level 2
    if (r_walllod && !sky)
    {
	lod = dc_iscale >= 4*FRACUNIT ? 2 : dc_iscale >= 2*FRACUNIT;
	if (lod > level + t->nlevels - 1)
	    lod = level + t->nlevels - 1;
	if (lod > level)
	    level = lod;
    }
    if (level == t->base + 2)
	RSTAT(col_lv2, 1);

    // rows r0 .. r0 + rows - 1 as the drawer steps through them
    frac = dc_texturemid + (dc_yl - centery) * dc_iscale;
    r0 = (frac >> FRACBITS) & 127;
    if ((int64_t)(dc_yh - dc_yl) * dc_iscale >= (128 << FRACBITS))
	rows = 128;
    else
	rows = (int)((((int64_t)(frac & (FRACUNIT - 1)) + (int64_t)(dc_yh - dc_yl) * dc_iscale)
		      >> FRACBITS) + 1);
    if (rows > 128)
	rows = 128;

    // stored rows y0..y1 at the level, around the stored height
    c = rf;
    c->bits = t->log2_sh - level;
    mask = (1 << c->bits) - 1;
    y0 = r0 >> level;
    y1 = (r0 + rows - 1) >> level;
    a = y0 & mask;
    len = y1 - y0 + 1;
    if (len > mask)
    {
	a = 0;
	len = mask + 1;
    }
    cx = (col >> level) & ((1 << (t->log2_w - level)) - 1);
    key = tex << 14 | level << 11 | cx;

    for (i = 0; i < 2; i++)
    {
	b = &cbuf[i];
	if (b->key == key && ((a - b->start) & mask) + len <= b->len)
	    break;
    }
    if (i == 2)
    {
	// the buffer the queued column does not use
	i = !clast;
	b = &cbuf[i];
	buf = WALLBUF(i);
	rel = tex_level_off(t->log2_w, t->log2_sh, t->base, level) + (cx << c->bits);
#ifdef DMC_HOST
	if (r_columncheck)
	    R_CheckCtx ("texture", tex, col, t->win.loff + rel, 0,
			tex_col_peek(tex - 1, col >> level, level), 0);
#endif
#ifdef DMC_RSTATS
	R_StatReq(0);
#endif
	// a read around the stored height is queued in two pieces: requests
	// of one priority complete in order, the last ticket covers both
	if (a + len <= mask + 1)
	    b->ticket = R_ColRead (&t->win, rel + a, buf + a, len, 1);
	else
	{
	    RSTAT(col_wrap, 1);
	    R_ColRead (&t->win, rel + a, buf + a, mask + 1 - a, 0);
	    b->ticket = img_read_async(t->win.loff + rel, buf, a + len - (mask + 1), NOR_PRIO_COL);
	}
	b->key = key;
	b->start = a;
	b->len = len;
#ifdef DMC_RSTATS
	cost = R_StatReq(1);
#endif
    }
    else
	RSTAT(colhit, 1);

    R_SaveColumn(c);
    c->buf = i;
    c->post = 0;
    c->level = level;
#ifdef DMC_ATTR
    c->texheight = t->height;
#endif
    clast = i;
    STAT_PX();
    R_DrawQueued(1);
#ifdef DMC_RSTATS
    R_StatWait(cost, drawn);
#endif
}


//
// R_PostsBegin
// A run of masked columns of masked texture or patch id (the columns of
// one sprite or of one masked range). The post context is kept when it
// already holds that texture or patch.
//
void R_PostsBegin (int masked, int id)
{
    r_postctx_t*	p = &COLCTX->post;
    dmc_texent_t	e;
    dmc_patch_t		h;
    uint32_t		base;

    if (p->id == id && p->masked == masked)
	return;
    p->id = id;
    p->masked = masked;
    if (masked)
    {
	tex_entry(id - 1, &e);
	if (!(e.flags & DMC_TF_MASKED))
	    dmc_fatal("texture not masked");
	base = img.sect[DMC_SECT_TEXDATA].off;
	p->wmask = (1 << e.log2_w) - 1;
	p->shift = tex_base(&e);
	p->ncols = 0xffff;
	p->colinfo = base + e.colinfo;
	p->win.loff = base + e.data;
    }
    else
    {
	base = patch_loff(id);
	rec_get(base, &h, sizeof(h));
#ifdef DMC_PARITY
	PAR_Patch(id, &h);
#endif
	p->wmask = -1;
	p->shift = h.xshift;
	p->ncols = h.ncols;
	p->colinfo = base + h.colinfo;
	p->win.loff = base;
    }
    p->win.phys = img_phys_run(p->win.loff, &p->win.run);
    RSTAT(post_fills, 1);
}


//
// R_QueuePost
// The masked column dc_x (dc_colormap, dc_iscale, dc_texturemid,
// sprtopscreen, spryscale) of column col of the run's texture or patch.
//
void R_QueuePost (int col)
{
    r_postctx_t*	p = &COLCTX->post;
    rcol_t*	c;
    int		i;
    unsigned	k = ((unsigned) col & p->wmask) >> p->shift;
    uint32_t	ci;
    int32_t	key = p->masked << 30 | p->id << 12 | k;
#ifdef DMC_RSTATS
    uint32_t	cost = 0;
    int		drawn = queued;
#endif

    for (i = 0; i < 2 && cbuf[i].key != key; i++)
	;
    if (i == 2)
    {
	i = !clast;
#ifdef DMC_RSTATS
	R_StatReq(0);
#endif
	if (k >= p->ncols)
	    dmc_fatal("patch column out of range");
	ci = rec_u32(p->colinfo + 4 * k);
#ifdef DMC_HOST
	if (r_columncheck)
	{
	    uint32_t	n;
	    uint32_t	want = p->masked ? tex_masked_peek(p->id - 1, col, &n)
	                                 : spr_col_peek(p->id, col, &n);
	    R_CheckCtx (p->masked ? "masked texture" : "patch", p->id, col,
			p->win.loff + DMC_COLINFO_OFF(ci), DMC_COLINFO_LEN(ci), want, n);
	}
#endif
	cbuf[i].key = key;
	cbuf[i].ticket = R_ColRead (&p->win, DMC_COLINFO_OFF(ci), POSTBUF(i),
				    DMC_COLINFO_LEN(ci), 1);
#ifdef DMC_RSTATS
	cost = R_StatReq(1);
#endif
    }
    else
	RSTAT(colhit, 1);

    c = rf;
    R_SaveColumn(c);
    c->buf = i;
    c->post = 1;
    clast = i;
    STAT_PX();
    R_DrawQueued(1);
#ifdef DMC_RSTATS
    R_StatWait(cost, drawn);
#endif
}


//
// R_PostsEnd
// Draws the last column of the run.
//
void R_PostsEnd (void)
{
    R_DrawQueued(0);
}


//
// R_PatchPosts
// Posts of a picture column as stored in the image
// {topdelta, length, pixels}, for V_DrawPatch.
//
byte* R_PatchPosts (int patch, int col)
{
    int		len;

    fetch_wait(spr_col_request(patch, col, POSTBUF(0), &len));
    return POSTBUF(0);
}


//
// R_PatchHeader
//
void R_PatchHeader (int patch, dmc_patch_t* h)
{
    patch_header(patch, h);
#ifdef DMC_PARITY
    PAR_Patch(patch, h);
#endif
}


//
// R_GetFlat
//
byte* R_GetFlat (int picnum)
{
    int		flat = anim_frame(P_MapFlat(picnum), 1);
    byte*	texels = (byte *) flat_get(flat);

    // no flat slots: solid floors in the flat's average colour
    if (!texels)
	ds_solid = flat_avg(flat);
    return texels;
}


//
// R_InitData
//
void R_InitData (void)
{
    numtextures = tex_count() + 1;
    numflats = flat_count();

    // texture and flat animations: anim_frame (fetch.h)

    // COLORMAP rows 0-32 are in flash
    colormaps = (lighttable_t *) dmc_colormap;
}
