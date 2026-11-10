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
//	Refresh of things, i.e. objects represented by sprites.
//	Pass 1 records the visible things {handle, x1, x2} in vanilla
//	vissprite order, every strip projects those that overlap it again
//	and clips them to the strip with vanilla arithmetic.
//




#include <stdio.h>
#include <stdlib.h>


#include "doomdef.h"

#include "i_system.h"
#include "fetch.h"

#include "r_local.h"

#include "doomstat.h"

#include "r_strip.h"
#include "rview.h"



#define MINZ				(FRACUNIT*4)
#define BASEYCENTER			100



//
// Sprite rotation 0 is facing the viewer,
//  rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//  which increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//
fixed_t		pspritescale;
fixed_t		pspriteiscale;


//
// Pass 1: visible things in vanilla vissprite order, in the frame
// region. x1 and x2 are vanilla's clamped columns, the light level of
// the thing's sector is in the high bits of x1.
//
typedef struct
{
    unsigned short	handle;
    unsigned short	x1;
    unsigned short	x2;

} rsprite_t;

void*		r_sprs;
int		r_nsprs;
byte*		r_secbits;	// sectors whose things are added (pass-1 scratch)

// light level of the view sector for the weapon
static int	psplight;


//
// Vissprites of the strip.
//
vissprite_t*	vissprites;
vissprite_t*	vissprite_p;
vissprite_t*	vissprites_end;
byte*		r_sortidx;	// sorted vissprite indices
short*		clipbot;	// by absolute column
short*		cliptop;



//
// R_InitSprites
// Called at startup.
// Sprite frames are in the image (SPRDEF), indexed by the
// converter's dense sprite numbers, which the engine uses.
//
static int	numdense;

void R_InitSprites (void)
{
    numdense = spr_count();
}


//
// R_SpriteFrame
// Patch of a sprite frame as seen from the view point (thing NULL:
// the first rotation), flip receives the mirror flag. Returns -1 when
// the frame does not exist.
//
static int
R_SpriteFrame
( int		sprite,
  int		frame,
  const rthing_t* thing,
  boolean*	flip )
{
    dmc_sprframe_t	f;
    unsigned		e;
    unsigned		rot;
    angle_t		ang;

    if (sprite >= numdense
	|| spr_frame(sprite, frame & FF_FRAMEMASK, &f) < 0)
	return -1;

    e = f.lump[0];
    rot = 0;
    if ((e & DMC_SPR_ROTATE) && thing)
    {
	// choose a different rotation based on player view
	ang = R_PointToAngle (thing->x, thing->y);
	rot = (ang-thing->angle+(unsigned)(ANG45/2)*9)>>29;
	e = f.lump[rot];
    }
    *flip = (e & DMC_SPR_FLIP) != 0;
#ifdef DMC_PARITY
    PAR_SpriteFrame(sprite, frame, rot, e & DMC_SPR_PATCH_MASK, *flip);
#endif
    return e & DMC_SPR_PATCH_MASK;
}



//
// R_ClearSprites
// Called at frame start.
//
void R_ClearSprites (void)
{
    r_nsprs = 0;
    memset (r_secbits, 0, (numsectors + 7) >> 3);

    // light of the view sector for the weapon
    psplight = R_LightNum (rv_light (R_PointInSector (viewx, viewy)));
}


//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
// A NULL clip array is the constant mfloorconst or mceilconst.
//
short*		mfloorclip;
short*		mceilingclip;
int		mfloorconst;
int		mceilconst;

fixed_t		spryscale;
fixed_t		sprtopscreen;

#ifdef DMC_ATTR
// Host frame test: bytes of the vanilla layout {topdelta, length, pad,
// pixels, pad} from the pixels of post on, beyond them vanilla reads the
// rest of its patch lump.
static int R_PostBytes (const byte* post)
{
    int		n = post[1] + 1;

    for (post += post[1] + 2; *post != 0xff; post += post[1] + 2)
	n += post[1] + 4;
    return n + 1;
}
#endif

//
// R_DrawPostWindow
// A post whose pixels the drawer does not read within the post alone
// (rows & 127 beyond its length or before its start): the drawer reads
// 128 bytes of the vanilla layout from its pixels on. The pads are the
// first and last pixel of a post (true of every Freedoom patch), the
// bytes after the column end read 0.
//
static void R_DrawPostWindow (const byte* post)
{
    byte	win[128];
    int		n = 0;
    int		k;
    int		len;

#define PUT(v)	do { if (n < 128) win[n++] = (v); } while (0)
    for (;;)
    {
	len = post[1];
	for (k = 0; k < len; k++)
	    PUT(post[2 + k]);
	PUT(post[1 + len]);
	post += len + 2;
	if (*post == 0xff)
	    break;
	PUT(post[0]);
	PUT(post[1]);
	PUT(post[2]);
    }
    PUT(0xff);
#undef PUT
    memset (win + n, 0, 128 - n);
    dc_source = win;
    colfunc ();
}

void R_DrawMaskedColumn (const byte* column)
{
    int		topscreen;
    int 	bottomscreen;
    int		fc;
    int		cc;
    int64_t	len;
    int64_t	last;
    fixed_t	basetexturemid;
    fixed_t	frac;
	
    basetexturemid = dc_texturemid;
    fc = mfloorclip ? mfloorclip[dc_x] : mfloorconst;
    cc = mceilingclip ? mceilingclip[dc_x] : mceilconst;
    // rows (frac >> 16) & 127 of the post
    dc_fracshift = 9;
    dc_rowshift = 25;
	
    for ( ; column[0] != 0xff ; column += column[1] + 2) 
    {
	// calculate unclipped screen coordinates
	//  for post
	topscreen = sprtopscreen + spryscale*column[0];
	bottomscreen = topscreen + spryscale*column[1];

	dc_yl = (topscreen+FRACUNIT-1)>>FRACBITS;
	dc_yh = (bottomscreen-1)>>FRACBITS;
		
	if (dc_yh >= fc)
	    dc_yh = fc-1;
	if (dc_yl <= cc)
	    dc_yl = cc+1;

	if (dc_yl <= dc_yh)
	{
	    dc_texturemid = basetexturemid - (column[0]<<FRACBITS);
#ifdef DMC_ATTR
	    dc_texheight = R_PostBytes (column);
	    dc_ispost = 1;
#endif

	    // Drawn by either R_DrawColumn
	    //  or (SHADOW) R_DrawFuzzColumn,
	    //  from the image when every row is one of the post.
	    len = (int64_t) (column[1] < 128 ? column[1] : 128) << FRACBITS;
	    frac = dc_texturemid + (dc_yl-centery)*dc_iscale;
	    last = (int64_t) frac + (int64_t) (dc_yh-dc_yl)*dc_iscale;
	    if (frac >= 0 && frac < len && last >= 0 && last < len)
	    {
		dc_source = (byte *) column + 2;
		colfunc ();
	    }
	    else
		R_DrawPostWindow (column);
	}
    }
	
    dc_texturemid = basetexturemid;
}


//
// R_SetMaskClip
// The clip arrays of a drawseg for R_DrawMaskedColumn.
//
void R_SetMaskClip (drawseg_t* ds)
{
    mfloorconst = -1;
    mceilconst = viewheight;
    mfloorclip = ds->clipflags & DS_BOTCLIP ? openings + ds->sprbottomclip : NULL;
    mceilingclip = ds->clipflags & DS_TOPCLIP ? openings + ds->sprtopclip : NULL;
}



//
// R_DrawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
static void R_DrawVisSprite (vissprite_t* vis)
{
    int			texturecolumn;
    fixed_t		frac;
    int			x2 = r_s0 + vis->x2;

    if (vis->colormap == VS_SHADOW)
    {
	// NULL colormap = shadow draw
	colfunc = R_DrawFuzzColumn;
    }
    else
    {
	dc_colormap = colormaps + vis->colormap*256;
	if (vis->translation)
	{
	    colfunc = R_DrawTranslatedColumn;
	    dc_translation = vis->translation;
	}
    }
	
    dc_iscale = abs(vis->xiscale)>>detailshift;
    dc_texturemid = vis->gzt - viewz;
    frac = vis->startfrac;
    spryscale = vis->scale;
    sprtopscreen = centeryfrac - FixedMul(dc_texturemid,spryscale);
	
    R_PostsBegin (0, vis->patch);
    for (dc_x=r_s0+vis->x1 ; dc_x<=x2 ; dc_x++, frac += vis->xiscale)
    {
	texturecolumn = frac>>FRACBITS;
	R_QueuePost (texturecolumn);
	RSTAT(sprcols, 1);
    }
    R_PostsEnd ();

    colfunc = R_DrawColumn;
}



//
// R_ProjectSprite
// Vanilla projection of a thing. Returns 0 when it is not visible,
// else its columns x1..x2 clamped as vanilla does. With vis the
// thing is also clipped to the strip and stored there.
//
static int
R_ProjectSprite
( const rthing_t*	thing,
  int			lightnum,
  int*			px1,
  int*			px2,
  vissprite_t*		vis )
{
    fixed_t		tr_x;
    fixed_t		tr_y;
    
    fixed_t		gxt;
    fixed_t		gyt;
    
    fixed_t		tx;
    fixed_t		tz;

    fixed_t		xscale;
    
    int			x1;
    int			x2;
    int			xl;
    int			xh;

    int			lump;
    dmc_patch_t		patch;
    fixed_t		spritewidth;
    fixed_t		spriteoffset;
    fixed_t		spritetopoffset;
    
    boolean		flip;
    
    int			index;

    fixed_t		iscale;
    
    // transform the origin point
    tr_x = thing->x - viewx;
    tr_y = thing->y - viewy;
	
    gxt = FixedMul(tr_x,viewcos); 
    gyt = -FixedMul(tr_y,viewsin);
    
    tz = gxt-gyt; 

    // thing is behind view plane?
    if (tz < MINZ)
	return 0;
    
    xscale = FixedDiv(projection, tz);
	
    gxt = -FixedMul(tr_x,viewsin); 
    gyt = FixedMul(tr_y,viewcos); 
    tx = -(gyt+gxt); 

    // too far off the side?
    if (abs(tx)>(tz<<2))
	return 0;
    
    // decide which patch to use for sprite relative to player
    lump = R_SpriteFrame (thing->sprite, thing->frame, thing, &flip);
    if (lump < 0)
	I_Error ("R_ProjectSprite: invalid sprite frame %i : %i ",
		 thing->sprite, thing->frame);
    R_PatchHeader (lump, &patch);
    spritewidth = patch.width << FRACBITS;
    spriteoffset = patch.leftoffset << FRACBITS;
    spritetopoffset = patch.topoffset << FRACBITS;
    
    // calculate edges of the shape
    tx -= spriteoffset;	
    x1 = (centerxfrac + FixedMul (tx,xscale) ) >>FRACBITS;

    // off the right side?
    if (x1 > viewwidth)
	return 0;
    
    tx +=  spritewidth;
    x2 = ((centerxfrac + FixedMul (tx,xscale) ) >>FRACBITS) - 1;

    // off the left side
    if (x2 < 0)
	return 0;

    *px1 = x1 < 0 ? 0 : x1;
    *px2 = x2 >= viewwidth ? viewwidth-1 : x2;
    if (!vis)
	return 1;
    
    // store information in a vissprite, clipped to the strip
    xl = x1 < r_s0 ? r_s0 : x1;
    xh = x2 >= r_s1 ? r_s1-1 : x2;
    if (xl > xh)
	return 0;
    vis->translation = (thing->flags & MF_TRANSLATION) >> MF_TRANSSHIFT;
    vis->scale = xscale<<detailshift;
    vis->gx = thing->x;
    vis->gy = thing->y;
    vis->gz = thing->z;
    vis->gzt = thing->z + spritetopoffset;
    vis->x1 = xl - r_s0;
    vis->x2 = xh - r_s0;
    iscale = FixedDiv (FRACUNIT, xscale);

    if (flip)
    {
	vis->startfrac = spritewidth-1;
	vis->xiscale = -iscale;
    }
    else
    {
	vis->startfrac = 0;
	vis->xiscale = iscale;
    }

    if (xl > x1)
	vis->startfrac += vis->xiscale*(xl-x1);
    vis->patch = lump;
    
    // get light level
    if (thing->flags & MF_SHADOW)
    {
	// shadow draw
	vis->colormap = VS_SHADOW;
    }
    else if (fixedcolormap)
    {
	// fixed map
	vis->colormap = (fixedcolormap - colormaps) >> 8;
    }
    else if (thing->frame & FF_FULLBRIGHT)
    {
	// full bright
	vis->colormap = 0;
    }
    
    else
    {
	// diminished light
	index = xscale>>(LIGHTSCALESHIFT-detailshift);

	if (index >= MAXLIGHTSCALE) 
	    index = MAXLIGHTSCALE-1;

	vis->colormap = (R_ScaleLight (lightnum, index) - colormaps) >> 8;
    }	
    return 1;
}


//
// R_RecordThing
// Pass 1: a visible thing takes a vissprite slot as in vanilla, slots
// beyond MAXVISSPRITES are dropped.
//
static void R_RecordThing (const rthing_t* thing, void* ctx)
{
    rsprite_t*	rec;
    int		x1;
    int		x2;

    if (!R_ProjectSprite (thing, *(int *) ctx, &x1, &x2, NULL))
	return;
    if (r_nsprs == R_MAXSPRS)
    {
	RSTAT(sprites_dropped, 1);
	return;
    }
    rec = (rsprite_t *) r_sprs + r_nsprs++;
    rec->handle = thing->handle;
    rec->x1 = x1 | *(int *) ctx << 9;
    rec->x2 = x2;
    RSTAT(sprites, 1);
}


//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
void R_AddSprites (secidx_t secnum)
{
    int			lightnum;

    // BSP is traversed by subsector.
    // A sector might have been split into several
    //  subsectors during BSP building.
    // Thus we check whether its already added.
    if (r_secbits[secnum >> 3] & (1 << (secnum & 7)))
	return;		

    // Well, now it will be done.
    r_secbits[secnum >> 3] |= 1 << (secnum & 7);
	
    lightnum = R_LightNum (rv_light (secnum));

    // Handle all things in sector.
    rv_sector_things (secnum, R_RecordThing, &lightnum);
}


//
// R_StripSprites
// The recorded things that overlap the strip, in recorded order.
// Returns 0 when the pool is full and the strip can split.
//
static int R_StripSprites (void)
{
    rsprite_t*	rec = (rsprite_t *) r_sprs;
    rthing_t	thing;
    int		i;
    int		x1;
    int		x2;

    vissprite_p = vissprites;
    for (i = 0; i < r_nsprs; i++, rec++)
    {
	if (rec->x2 < r_s0 || (rec->x1 & 511) >= r_s1)
	    continue;
	if (!rv_thing (rec->handle, &thing))
	    continue;
	if (vissprite_p == vissprites_end)
	{
	    if (!r_vanilla)
		return 0;
	    RSTAT(vanilla_drops, 1);
	    break;
	}
	if (R_ProjectSprite (&thing, rec->x1 >> 9, &x1, &x2, vissprite_p))
	{
	    vissprite_p++;
	    RSTAT(vissprites, 1);
	}
    }
    RSTAT_MAX(hw_vissprites, vissprite_p - vissprites);
    return 1;
}


//
// R_DrawPSprite
//
static void R_DrawPSprite (pspdef_t* psp)
{
    fixed_t		tx;
    int			x1;
    int			x2;
    int			xl;
    int			xh;
    int			lump;
    boolean		flip;
    vissprite_t*	vis;
    vissprite_t		avis;
    dmc_patch_t		patch;
    fixed_t		spritewidth;
    fixed_t		spriteoffset;
    fixed_t		spritetopoffset;
    
    // decide which patch to use
    lump = R_SpriteFrame (psp->state->sprite, ST_FRAME(psp->state), NULL, &flip);
    if (lump < 0)
	I_Error ("R_ProjectSprite: invalid sprite frame %i : %i ",
		 psp->state->sprite, ST_FRAME(psp->state));
    R_PatchHeader (lump, &patch);
    spritewidth = patch.width << FRACBITS;
    spriteoffset = patch.leftoffset << FRACBITS;
    spritetopoffset = patch.topoffset << FRACBITS;
    
    // calculate edges of the shape
    tx = psp->sx-160*FRACUNIT;
	
    tx -= spriteoffset;	
    x1 = (centerxfrac + FixedMul (tx,pspritescale) ) >>FRACBITS;

    // off the right side
    if (x1 > viewwidth)
	return;		

    tx +=  spritewidth;
    x2 = ((centerxfrac + FixedMul (tx, pspritescale) ) >>FRACBITS) - 1;

    // off the left side
    if (x2 < 0)
	return;

    // clipped to the strip
    xl = x1 < r_s0 ? r_s0 : x1;
    xh = x2 >= r_s1 ? r_s1-1 : x2;
    if (xl > xh)
	return;
    
    // store information in a vissprite
    vis = &avis;
    vis->translation = 0;
    // texturemid is gzt - viewz
    vis->gzt = (BASEYCENTER<<FRACBITS)+FRACUNIT/2-(psp->sy-spritetopoffset) + viewz;
    vis->x1 = xl - r_s0;
    vis->x2 = xh - r_s0;
    vis->scale = pspritescale<<detailshift;
    
    if (flip)
    {
	vis->xiscale = -pspriteiscale;
	vis->startfrac = spritewidth-1;
    }
    else
    {
	vis->xiscale = pspriteiscale;
	vis->startfrac = 0;
    }
    
    if (xl > x1)
	vis->startfrac += vis->xiscale*(xl-x1);

    vis->patch = lump;

    if (viewplayer->powers[pw_invisibility] > 4*32
	|| viewplayer->powers[pw_invisibility] & 8)
    {
	// shadow draw
	vis->colormap = VS_SHADOW;
    }
    else if (fixedcolormap)
    {
	// fixed color
	vis->colormap = (fixedcolormap - colormaps) >> 8;
    }
    else if (ST_FRAME(psp->state) & FF_FULLBRIGHT)
    {
	// full bright
	vis->colormap = 0;
    }
    else
    {
	// local light
	vis->colormap = (R_ScaleLight (psplight, MAXLIGHTSCALE-1) - colormaps) >> 8;
    }
	
    R_DrawVisSprite (vis);
    RSTAT(pspcols, vis->x2 - vis->x1 + 1);
}



//
// R_DrawPlayerSprites
//
static void R_DrawPlayerSprites (void)
{
    int		i;
    pspdef_t*	psp;
    
    // clip to screen bounds
    mfloorclip = NULL;
    mfloorconst = viewheight;
    mceilingclip = NULL;
    mceilconst = -1;
    
    // add all active psprites
    for (i=0, psp=viewplayer->psprites;
	 i<NUMPSPRITES;
	 i++,psp++)
    {
	if (psp->state)
	    R_DrawPSprite (psp);
    }
}




//
// R_SortVisSprites
// By scale, ties in vissprite order as the vanilla selection sort.
//
static void R_SortVisSprites (void)
{
    int		count = vissprite_p - vissprites;
    int		i;
    int		j;
    byte	k;

    for (i = 0; i < count; i++)
    {
	k = i;
	for (j = i; j > 0 && vissprites[r_sortidx[j-1]].scale > vissprites[k].scale; j--)
	    r_sortidx[j] = r_sortidx[j-1];
	r_sortidx[j] = k;
    }
}


static int R_SilDecode (short h)
{
    if (h == SIL_MAXH)
	return INT_MAX;
    if (h == SIL_MINH)
	return INT_MIN;
    return h << (FRACBITS-3);
}


//
// R_DrawSprite
// Columns are absolute, the drawsegs of the strip keep the scales
// of their whole fragment.
//
static void R_DrawSprite (vissprite_t* spr)
{
    drawseg_t*		ds;
    int			x;
    int			r1;
    int			r2;
    int			x1 = r_s0 + spr->x1;
    int			x2 = r_s0 + spr->x2;
    int			dx1;
    int			dx2;
    fixed_t		scale;
    fixed_t		lowscale;
    int			silhouette;
    seg_t		seg;
		
    for (x = x1 ; x<=x2 ; x++)
	clipbot[x] = cliptop[x] = -2;
    
    // Scan drawsegs from end to start for obscuring segs.
    // The first drawseg that has a greater scale
    //  is the clip seg.
    for (ds=ds_p-1 ; ds >= drawsegs ; ds--)
    {
	dx1 = r_s0 + ds->x1;
	dx2 = r_s0 + ds->x2;

	// determine if the drawseg obscures the sprite
	if (dx1 > x2
	    || dx2 < x1
	    || (!ds->silhouette
		&& !(ds->clipflags & DS_MASKED)) )
	{
	    // does not cover sprite
	    continue;
	}
			
	r1 = dx1 < x1 ? x1 : dx1;
	r2 = dx2 > x2 ? x2 : dx2;

	if (ds->scale1 > ds->scale2)
	{
	    lowscale = ds->scale2;
	    scale = ds->scale1;
	}
	else
	{
	    lowscale = ds->scale1;
	    scale = ds->scale2;
	}
		
	if (scale < spr->scale
	    || ( lowscale < spr->scale
		 && (P_GetSeg (ds->curline, &seg),
		     !R_PointOnSegSide (spr->gx, spr->gy, &seg)) ) )
	{
	    // masked mid texture?
	    if (ds->clipflags & DS_MASKED)	
		R_RenderMaskedSegRange (ds, r1, r2);
	    // seg is behind sprite
	    continue;			
	}

	
	// clip this piece of the sprite
	silhouette = ds->silhouette;
	
	if (spr->gz >= R_SilDecode(ds->bsilheight))
	    silhouette &= ~SIL_BOTTOM;

	if (spr->gzt <= R_SilDecode(ds->tsilheight))
	    silhouette &= ~SIL_TOP;
			
	if (silhouette & SIL_BOTTOM)
	{
	    // bottom sil
	    for (x=r1 ; x<=r2 ; x++)
		if (clipbot[x] == -2)
		    clipbot[x] = ds->clipflags & DS_BOTFULL
			? -1 : openings[ds->sprbottomclip + x];
	}
	if (silhouette & SIL_TOP)
	{
	    // top sil
	    for (x=r1 ; x<=r2 ; x++)
		if (cliptop[x] == -2)
		    cliptop[x] = ds->clipflags & DS_TOPFULL
			? viewheight : openings[ds->sprtopclip + x];
	}
		
    }
    
    // all clipping has been performed, so draw the sprite

    // check for unclipped columns
    for (x = x1 ; x<=x2 ; x++)
    {
	if (clipbot[x] == -2)		
	    clipbot[x] = viewheight;

	if (cliptop[x] == -2)
	    cliptop[x] = -1;
    }
		
    mfloorclip = clipbot;
    mceilingclip = cliptop;
    R_DrawVisSprite (spr);
}




//
// R_DrawMasked
// The things, masked middles and the weapon of the strip. Returns 0
// when the vissprites do not fit and the strip can split.
//
int R_DrawMasked (void)
{
    drawseg_t*		ds;
    int			i;
	
    if (!R_StripSprites ())
	return 0;
    R_SortVisSprites ();

    // draw all vissprites back to front
    for (i = 0; i < vissprite_p - vissprites; i++)
	R_DrawSprite (&vissprites[r_sortidx[i]]);
    
    // render any remaining masked mid textures
    for (ds=ds_p-1 ; ds >= drawsegs ; ds--)
	if (ds->clipflags & DS_MASKED)
	    R_RenderMaskedSegRange (ds, r_s0 + ds->x1, r_s0 + ds->x2);
    
    // draw the psprites on top of everything
    //  but does not draw on side views
    if (!viewangleoffset)		
	R_DrawPlayerSprites ();
    return 1;
}
