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
//	All the clipping: columns, horizontal spans, sky columns.
//	Pass 2 of the strip renderer: every pass-1 fragment that overlaps
//	the strip is set up as vanilla R_StoreWallRange does for the whole
//	fragment, the steppers are advanced to the strip and only the strip
//	columns are drawn.
//

#include <stdio.h>
#include <stdlib.h>

#include "i_system.h"

#include "doomdef.h"
#include "doomstat.h"

#include "r_local.h"
#include "r_sky.h"
#include "fetch.h"

#include "r_strip.h"
#include "trig.h"


// OPTIMIZE: closed two sided lines as single sided

// True if any of the segs textures might be visible.
boolean		segtextured;	

// False if the back side is the same plane.
boolean		markfloor;	
boolean		markceiling;

boolean		maskedtexture;
int		toptexture;
int		bottomtexture;
int		midtexture;


angle_t		rw_normalangle;
// angle to line origin
int		rw_angle1;	

//
// regular wall
//
int		rw_x;
int		rw_stopx;
angle_t		rw_centerangle;
fixed_t		rw_offset;
fixed_t		rw_distance;
fixed_t		rw_scale;
fixed_t		rw_scalestep;
fixed_t		rw_midtexturemid;
fixed_t		rw_toptexturemid;
fixed_t		rw_bottomtexturemid;

int		worldtop;
int		worldbottom;
int		worldhigh;
int		worldlow;

fixed_t		pixhigh;
fixed_t		pixlow;
fixed_t		pixhighstep;
fixed_t		pixlowstep;

fixed_t		topfrac;
fixed_t		topstep;

fixed_t		bottomfrac;
fixed_t		bottomstep;


// light level 0-15 of the wall (vanilla walllights row)
int		walllight;

// by absolute column
short*		maskedtexturecol;

// back sector of the fragment
static rsector_t	backsec;


// Drawsegs of the strip.
drawseg_t*	drawsegs;
drawseg_t*	ds_p;
drawseg_t*	drawsegs_end;


//
// R_NewOpenings
//
int R_NewOpenings (int n)
{
    int		first;

    if (openings_end - lastopening < n)
    {
	if (!r_vanilla)
	    r_overflow = 1;
	return -1;
    }
    first = lastopening - openings;
    lastopening += n;
    return first;
}


// Heights in 1/8 units: every sector height is a multiple of 1/8.
static short R_SilHeight (int h)
{
    if (h == INT_MAX)
	return SIL_MAXH;
    if (h == INT_MIN)
	return SIL_MINH;
    return h >> (FRACBITS-3);
}


static int R_WallLight (int lightlevel)
{
    int		lightnum = (lightlevel >> LIGHTSEGSHIFT)+extralight;

    if (curline.v1y == curline.v2y)
	lightnum--;
    else if (curline.v1x == curline.v2x)
	lightnum++;

    if (lightnum < 0)		
	return 0;
    if (lightnum >= LIGHTLEVELS)
	return LIGHTLEVELS-1;
    return lightnum;
}


//
// R_RenderMaskedSegRange
// x1 and x2 are columns of the strip.
//
void
R_RenderMaskedSegRange
( drawseg_t*	ds,
  int		x1,
  int		x2 )
{
    unsigned	index;
    int		texnum;
    rsector_t	front;
    
    // Calculate light table.
    // Use different light tables
    //   for horizontal / vertical / diagonal. Diagonal?
    // OPTIMIZE: get rid of LIGHTSEGSHIFT globally
    P_GetSeg(ds->curline, &curline);
    curlineidx = ds->curline;
    P_GetLine(curline.linedef, &linedef);
    frontsector = curline.side ? linedef.backsector : linedef.frontsector;
    backsector = curline.backsector;
    R_ReadSector (frontsector, &front);
    R_ReadSector (backsector, &backsec);
    P_GetSide(P_SegSide(&curline), &sidedef);
    texnum = anim_frame(sidedef.midtexture, 0);
	
    walllight = R_WallLight (front.lightlevel);

    maskedtexturecol = openings + ds->maskedtexturecol;

    rw_scalestep = ds->scalestep;		
    spryscale = ds->scale1 + (x1 - ds->fx1)*rw_scalestep;
    R_SetMaskClip (ds);
    
    // find positioning
    if (linedef.flags & ML_DONTPEGBOTTOM)
    {
	dc_texturemid = front.floorheight > backsec.floorheight
	    ? front.floorheight : backsec.floorheight;
	dc_texturemid = dc_texturemid + R_TextureHeight(texnum) - viewz;
    }
    else
    {
	dc_texturemid =front.ceilingheight<backsec.ceilingheight
	    ? front.ceilingheight : backsec.ceilingheight;
	dc_texturemid = dc_texturemid - viewz;
    }
    dc_texturemid += sidedef.rowoffset;
			
    if (fixedcolormap)
	dc_colormap = fixedcolormap;
    
    // draw the columns
    R_PostsBegin (1, texnum);
    for (dc_x = x1 ; dc_x <= x2 ; dc_x++)
    {
	// calculate lighting
	if (maskedtexturecol[dc_x] != SHRT_MAX)
	{
	    if (!fixedcolormap)
	    {
		index = spryscale>>LIGHTSCALESHIFT;

		if (index >=  MAXLIGHTSCALE )
		    index = MAXLIGHTSCALE-1;

		dc_colormap = R_ScaleLight (walllight, index);
	    }
			
	    sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
	    dc_iscale = 0xffffffffu / (unsigned)spryscale;
	    
	    // draw the texture
	    R_QueuePost (maskedtexturecol[dc_x]);
	    RSTAT(maskedcols, 1);
	    maskedtexturecol[dc_x] = SHRT_MAX;
	}
	spryscale += rw_scalestep;
    }
    R_PostsEnd ();
	
}




//
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked
//  texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling
//  textures.
// CALLED: CORE LOOPING ROUTINE.
//
#define HEIGHTBITS		12
#define HEIGHTUNIT		(1<<HEIGHTBITS)

// finetangent[angle] of a wall column, inline. Angles from FINEANGLES/2
// on read past the vanilla table into finesine, which follows it.
static inline fixed_t R_ColumnTangent (unsigned angle)
{
    if (angle >= FINEANGLES/2)
	return finesine(angle - FINEANGLES/2);
    return dmc_finetangent_inline(angle);
}

static void R_RenderSegLoop (void)
{
    angle_t		angle;
    unsigned		index;
    int			yl;
    int			yh;
    int			mid;
    fixed_t		texturecolumn;
    int			top;
    int			bottom;

    for ( ; rw_x < rw_stopx ; rw_x++)
    {
	// mark floor / ceiling areas
	yl = (topfrac+HEIGHTUNIT-1)>>HEIGHTBITS;

	// no space above wall?
	if (yl < ceilingclip[rw_x]+1)
	    yl = ceilingclip[rw_x]+1;
	
	if (markceiling)
	{
	    top = ceilingclip[rw_x]+1;
	    bottom = yl-1;

	    if (bottom >= floorclip[rw_x])
		bottom = floorclip[rw_x]-1;

	    if (top <= bottom)
		R_MarkPlane (&ceilingplane, rw_x, top, bottom);
	}
		
	yh = bottomfrac>>HEIGHTBITS;

	if (yh >= floorclip[rw_x])
	    yh = floorclip[rw_x]-1;

	if (markfloor)
	{
	    top = yh+1;
	    bottom = floorclip[rw_x]-1;
	    if (top <= ceilingclip[rw_x])
		top = ceilingclip[rw_x]+1;
	    if (top <= bottom)
		R_MarkPlane (&floorplane, rw_x, top, bottom);
	}
	
	// texturecolumn and lighting are independent of wall tiers
	if (segtextured)
	{
	    // calculate texture offset
	    angle = (rw_centerangle + R_XToViewAngle(rw_x))>>ANGLETOFINESHIFT;
	    texturecolumn = rw_offset-FixedMul(R_ColumnTangent(angle),rw_distance);
	    texturecolumn >>= FRACBITS;
	    // calculate lighting
	    index = rw_scale>>LIGHTSCALESHIFT;

	    if (index >=  MAXLIGHTSCALE )
		index = MAXLIGHTSCALE-1;

	    dc_colormap = fixedcolormap ? fixedcolormap : R_ScaleLight (walllight, index);
	    dc_x = rw_x;
	    dc_iscale = 0xffffffffu / (unsigned)rw_scale;
	    RSTAT(wallcols, 1);
	}
        else
        {
            // purely to shut up the compiler

            texturecolumn = 0;
        }
	
	// draw the wall tiers
	if (midtexture)
	{
	    // single sided line
	    dc_yl = yl;
	    dc_yh = yh;
	    dc_texturemid = rw_midtexturemid;
	    R_QueueColumn (midtexture, texturecolumn, 0);
	    RSTAT(wallpx, yh >= yl ? yh - yl + 1 : 0);
	    ceilingclip[rw_x] = viewheight;
	    floorclip[rw_x] = -1;
	}
	else
	{
	    // two sided line
	    if (toptexture)
	    {
		// top wall
		mid = pixhigh>>HEIGHTBITS;
		pixhigh += pixhighstep;

		if (mid >= floorclip[rw_x])
		    mid = floorclip[rw_x]-1;

		if (mid >= yl)
		{
		    dc_yl = yl;
		    dc_yh = mid;
		    dc_texturemid = rw_toptexturemid;
		    R_QueueColumn (toptexture, texturecolumn, 0);
		    RSTAT(wallpx, mid - yl + 1);
		    ceilingclip[rw_x] = mid;
		}
		else
		    ceilingclip[rw_x] = yl-1;
	    }
	    else
	    {
		// no top wall
		if (markceiling)
		    ceilingclip[rw_x] = yl-1;
	    }
			
	    if (bottomtexture)
	    {
		// bottom wall
		mid = (pixlow+HEIGHTUNIT-1)>>HEIGHTBITS;
		pixlow += pixlowstep;

		// no space above wall?
		if (mid <= ceilingclip[rw_x])
		    mid = ceilingclip[rw_x]+1;
		
		if (mid <= yh)
		{
		    dc_yl = mid;
		    dc_yh = yh;
		    dc_texturemid = rw_bottomtexturemid;
		    R_QueueColumn (bottomtexture, texturecolumn, 0);
		    RSTAT(wallpx, yh - mid + 1);
		    floorclip[rw_x] = mid;
		}
		else
		    floorclip[rw_x] = yh+1;
	    }
	    else
	    {
		// no bottom wall
		if (markfloor)
		    floorclip[rw_x] = yh+1;
	    }
			
	    if (maskedtexture)
	    {
		// save texturecol
		//  for backdrawing of masked mid texture
		maskedtexturecol[rw_x] = texturecolumn;
	    }
	}
		
	rw_scale += rw_scalestep;
	topfrac += topstep;
	bottomfrac += bottomstep;
    }
}




//
// Fragment setup cache (pass 2): the part of the setup of a fragment
// that does not depend on the strip, kept for the fragment's later
// strips. It lives in memory no strip uses: from the end of the recorded
// sprites (6 B each) to the column ring in the upper half of the pass-1
// BSP stack, then the tail of the fragment list. Fragment i takes slot i
// modulo the slots there are, the first ones in the sprite part. The
// slots of the fragments are emptied at the end of every pass 1
// (R_FragCacheClear).
//
typedef struct
{
    uint32_t	key;		// fragment + 1, 0 for an empty slot
    fixed_t	distance;	// rw_distance
    fixed_t	offset;		// rw_offset before the side and seg offsets, sign applied
    fixed_t	scale1;
    fixed_t	scale2;
    fixed_t	step;
} r_fragc_t;

_Static_assert(sizeof(r_fragc_t) == 24, "fragment cache entry size");

static r_fragc_t* R_FragSlot (int i)
{
    byte*	a = (byte *) r_sprs + ((6 * r_nsprs + 3) & ~3);
    byte*	ring = (byte *) r_bspstack + R_BSPSTACK * 2 - 64;
    int		n1 = (ring - a) / (int) sizeof(r_fragc_t);

    i %= n1 + (R_MAXFRAGS - r_nfrags) * 4 / (int) sizeof(r_fragc_t);
    if (i < n1)
	return (r_fragc_t *) a + i;
    return (r_fragc_t *) (r_frags + r_nfrags) + (i - n1);
}


//
// R_FragCacheClear
// Empties the slots of the fragments: the end of pass 1, pass 2 follows.
//
void R_FragCacheClear (void)
{
    int		i;

    for (i = 0; i < r_nfrags; i++)
	R_FragSlot (i)->key = 0;
}


// The setup of the fragment [start, stop] of curline that does not
// depend on the strip, computed as vanilla R_StoreWallRange does. It
// needs rw_normalangle and rw_angle1 and sets rw_distance.
static void R_FragSetup (r_fragc_t* f, int start, int stop)
{
    fixed_t		hyp;
    fixed_t		sineval;
    angle_t		distangle, offsetangle;

    // calculate rw_distance for scale calculation
    offsetangle = abs(rw_normalangle-rw_angle1);

    if (offsetangle > ANG90)
	offsetangle = ANG90;

    distangle = ANG90 - offsetangle;
    hyp = R_PointToDist (curline.v1x, curline.v1y);
    sineval = finesine(distangle>>ANGLETOFINESHIFT);
    f->distance = rw_distance = FixedMul (hyp, sineval);

    // rw_offset of the line origin, before the side and seg offsets
    // (only used by textured lines)
    offsetangle = rw_normalangle-rw_angle1;

    if (offsetangle > ANG180)
	offsetangle = -offsetangle;

    if (offsetangle > ANG90)
	offsetangle = ANG90;

    sineval = finesine(offsetangle >>ANGLETOFINESHIFT);
    f->offset = FixedMul (hyp, sineval);

    if (rw_normalangle-rw_angle1 < ANG180)
	f->offset = -f->offset;

    // calculate scale at both ends and step
    f->scale1 = R_ScaleFromGlobalAngle (viewangle + R_XToViewAngle(start));

    if (stop > start )
    {
	f->scale2 = R_ScaleFromGlobalAngle (viewangle + R_XToViewAngle(stop));
	f->step = (f->scale2 - f->scale1) / (stop-start);
    }
    else
    {
	// a single column never steps
	f->scale2 = f->scale1;
	f->step = 0;
    }
}


#ifdef DMC_HOST
// --column-check: a setup taken from the cache against the one computed
// again, which leaves the trig counters as they were.
static void R_FragCheck (const r_fragc_t* fc, int start, int stop)
{
    r_fragc_t	f;
    uint32_t	trig[DMC_TRIG_KINDS];

    memcpy (trig, dmc_trig_calls, sizeof(trig));
    rw_angle1 = R_PointToAngle (curline.v1x, curline.v1y);
    R_FragSetup (&f, start, stop);
    memcpy (dmc_trig_calls, trig, sizeof(trig));
    if (f.distance != fc->distance || f.offset != fc->offset
	|| f.scale1 != fc->scale1 || f.scale2 != fc->scale2 || f.step != fc->step)
	I_Error ("R_FragCheck: seg %d columns %d to %d differ from the cached setup",
		 curlineidx, start, stop);
}
#endif


//
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
// The setup is the vanilla one for the whole fragment, the columns
// drawn are those of the strip. Advancing the steppers by the columns
// left of the strip is integer arithmetic, so every value matches what
// vanilla has at that column. The part of the setup that does not
// depend on the strip comes from slot fc when it holds key, else it is
// computed and stored there.
//
static void
R_StoreWallRange
( int		start,
  int		stop,
  r_fragc_t*	fc,
  uint32_t	key )
{
    fixed_t		vtop;
    int			x1;
    int			x2;
    int			n;
    int			off;
    int			bsil;
    int			tsil;
    int			lost;
    drawseg_t*		ds;

    x1 = start < r_s0 ? r_s0 : start;
    x2 = stop >= r_s1 ? r_s1-1 : stop;

    // don't overflow and crash
    if (ds_p == drawsegs_end)
    {
	if (r_vanilla)
	    RSTAT(vanilla_drops, 1);
	else
	    r_overflow = 1;
	return;		
    }
    ds = ds_p;
    RSTAT(replays, 1);
		
    P_GetSide(P_SegSide(&curline), &sidedef);

    rw_normalangle = curline.angle + ANG90;
    if (fc->key != key)
    {
	R_FragSetup (fc, start, stop);
	fc->key = key;
    }
    else
    {
	RSTAT(fragc_hits, 1);
#ifdef DMC_HOST
	if (r_columncheck)
	    R_FragCheck (fc, start, stop);
#endif
    }
    rw_distance = fc->distance;

    ds->x1 = x1 - r_s0;
    ds->x2 = x2 - r_s0;
    ds->fx1 = start;
    ds->curline = curlineidx;
    ds->clipflags = 0;
    bsil = tsil = 0;

    ds->scale1 = rw_scale = fc->scale1;
    ds->scale2 = fc->scale2;
    ds->scalestep = rw_scalestep = fc->step;

    // calculate texture boundaries
    //  and decide if floor / ceiling marks are needed
    worldtop = frontsec.ceilingheight - viewz;
    worldbottom = frontsec.floorheight - viewz;
	
    midtexture = toptexture = bottomtexture = maskedtexture = 0;
	
    if (backsector == IDX_NONE)
    {
	// single sided line
	midtexture = anim_frame(sidedef.midtexture, 0);
	// a single sided line is terminal, so it must mark ends
	markfloor = markceiling = true;
	if (linedef.flags & ML_DONTPEGBOTTOM)
	{
	    vtop = frontsec.floorheight +
		R_TextureHeight(sidedef.midtexture);
	    // bottom of texture at bottom
	    rw_midtexturemid = vtop - viewz;	
	}
	else
	{
	    // top of texture at top
	    rw_midtexturemid = worldtop;
	}
	rw_midtexturemid += sidedef.rowoffset;

	ds->silhouette = SIL_BOTH;
	ds->clipflags = DS_TOPFULL | DS_BOTFULL;
	bsil = INT_MAX;
	tsil = INT_MIN;
    }
    else
    {
	R_ReadSector (backsector, &backsec);

	// two sided line
	ds->silhouette = 0;
	
	if (frontsec.floorheight > backsec.floorheight)
	{
	    ds->silhouette = SIL_BOTTOM;
	    bsil = frontsec.floorheight;
	}
	else if (backsec.floorheight > viewz)
	{
	    ds->silhouette = SIL_BOTTOM;
	    bsil = INT_MAX;
	    // ds_p->sprbottomclip = negonearray;
	}
	
	if (frontsec.ceilingheight < backsec.ceilingheight)
	{
	    ds->silhouette |= SIL_TOP;
	    tsil = frontsec.ceilingheight;
	}
	else if (backsec.ceilingheight < viewz)
	{
	    ds->silhouette |= SIL_TOP;
	    tsil = INT_MIN;
	    // ds_p->sprtopclip = screenheightarray;
	}
		
	if (backsec.ceilingheight <= frontsec.floorheight)
	{
	    ds->clipflags |= DS_BOTFULL;
	    bsil = INT_MAX;
	    ds->silhouette |= SIL_BOTTOM;
	}
	
	if (backsec.floorheight >= frontsec.ceilingheight)
	{
	    ds->clipflags |= DS_TOPFULL;
	    tsil = INT_MIN;
	    ds->silhouette |= SIL_TOP;
	}
	
	worldhigh = backsec.ceilingheight - viewz;
	worldlow = backsec.floorheight - viewz;
		
	// hack to allow height changes in outdoor areas
	if (frontsec.ceilingpic == skyflatnum 
	    && backsec.ceilingpic == skyflatnum)
	{
	    worldtop = worldhigh;
	}
	
			
	if (worldlow != worldbottom 
	    || backsec.floorpic != frontsec.floorpic
	    || backsec.lightlevel != frontsec.lightlevel)
	{
	    markfloor = true;
	}
	else
	{
	    // same plane on both sides
	    markfloor = false;
	}
	
			
	if (worldhigh != worldtop 
	    || backsec.ceilingpic != frontsec.ceilingpic
	    || backsec.lightlevel != frontsec.lightlevel)
	{
	    markceiling = true;
	}
	else
	{
	    // same plane on both sides
	    markceiling = false;
	}
	
	if (backsec.ceilingheight <= frontsec.floorheight
	    || backsec.floorheight >= frontsec.ceilingheight)
	{
	    // closed door
	    markceiling = markfloor = true;
	}
	

	if (worldhigh < worldtop)
	{
	    // top texture
	    toptexture = anim_frame(sidedef.toptexture, 0);
	    if (linedef.flags & ML_DONTPEGTOP)
	    {
		// top of texture at top
		rw_toptexturemid = worldtop;
	    }
	    else
	    {
		vtop =
		    backsec.ceilingheight
		    + R_TextureHeight(sidedef.toptexture);
		
		// bottom of texture
		rw_toptexturemid = vtop - viewz;	
	    }
	}
	if (worldlow > worldbottom)
	{
	    // bottom texture
	    bottomtexture = anim_frame(sidedef.bottomtexture, 0);

	    if (linedef.flags & ML_DONTPEGBOTTOM )
	    {
		// bottom of texture at bottom
		// top of texture at top
		rw_bottomtexturemid = worldtop;
	    }
	    else	// top of texture at top
		rw_bottomtexturemid = worldlow;
	}
	rw_toptexturemid += sidedef.rowoffset;
	rw_bottomtexturemid += sidedef.rowoffset;
	
	// allocate space for masked texture tables
	if (sidedef.midtexture)
	{
	    // masked midtexture
	    off = R_NewOpenings (x2 - x1 + 1);
	    if (off >= 0)
	    {
		maskedtexture = true;
		ds->maskedtexturecol = off - x1;
		ds->clipflags |= DS_MASKED;
		maskedtexturecol = openings + off - x1;
	    }
	    else if (!r_vanilla)
		return;
	}
    }
    
    // calculate rw_offset (only needed for textured lines)
    segtextured = midtexture | toptexture | bottomtexture | maskedtexture;

    if (segtextured)
    {
	rw_offset = fc->offset;
	rw_offset += sidedef.textureoffset + curline.offset;
	rw_centerangle = ANG90 + viewangle - rw_normalangle;
	
	// calculate light table
	//  use different light tables
	//  for horizontal / vertical / diagonal
	// OPTIMIZE: get rid of LIGHTSEGSHIFT globally
	if (!fixedcolormap)
	    walllight = R_WallLight (frontsec.lightlevel);
    }
    
    // if a floor / ceiling plane is on the wrong side
    //  of the view plane, it is definitely invisible
    //  and doesn't need to be marked.
    
  
    if (frontsec.floorheight >= viewz)
    {
	// above view plane
	markfloor = false;
    }
    
    if (frontsec.ceilingheight <= viewz 
	&& frontsec.ceilingpic != skyflatnum)
    {
	// below view plane
	markceiling = false;
    }

    
    // calculate incremental stepping values for texture edges
    worldtop >>= 4;
    worldbottom >>= 4;
	
    topstep = -FixedMul (rw_scalestep, worldtop);
    topfrac = (centeryfrac>>4) - FixedMul (worldtop, rw_scale);

    bottomstep = -FixedMul (rw_scalestep,worldbottom);
    bottomfrac = (centeryfrac>>4) - FixedMul (worldbottom, rw_scale);
	
    if (backsector != IDX_NONE)
    {	
	worldhigh >>= 4;
	worldlow >>= 4;

	if (worldhigh < worldtop)
	{
	    pixhigh = (centeryfrac>>4) - FixedMul (worldhigh, rw_scale);
	    pixhighstep = -FixedMul (rw_scalestep,worldhigh);
	}
	
	if (worldlow > worldbottom)
	{
	    pixlow = (centeryfrac>>4) - FixedMul (worldlow, rw_scale);
	    pixlowstep = -FixedMul (rw_scalestep,worldlow);
	}
    }

    // step to the first column of the strip
    n = x1 - start;
    if (n)
    {
	rw_scale += n*rw_scalestep;
	topfrac += n*topstep;
	bottomfrac += n*bottomstep;
	if (toptexture)
	    pixhigh += n*pixhighstep;
	if (bottomtexture)
	    pixlow += n*pixlowstep;
    }
    
    // render it
    rw_x = x1;
    rw_stopx = x2+1;
    R_RenderSegLoop ();

    
    // save sprite clipping info (a one-column strip drops a silhouette
    // whose clip array does not fit, lost has its bit)
    lost = 0;
    if ( ((ds->silhouette & SIL_TOP) || maskedtexture)
	 && !(ds->clipflags & DS_TOPFULL))
    {
	off = R_NewOpenings (rw_stopx - x1);
	if (off < 0)
	{
	    if (!r_vanilla)
		return;
	    ds->silhouette &= ~SIL_TOP;
	    lost |= SIL_TOP;
	}
	else
	{
	    memcpy (openings + off, ceilingclip+x1, 2*(rw_stopx-x1));
	    ds->sprtopclip = off - x1;
	    ds->clipflags |= DS_TOPCLIP;
	}
    }
    
    if ( ((ds->silhouette & SIL_BOTTOM) || maskedtexture)
	 && !(ds->clipflags & DS_BOTFULL))
    {
	off = R_NewOpenings (rw_stopx - x1);
	if (off < 0)
	{
	    if (!r_vanilla)
		return;
	    ds->silhouette &= ~SIL_BOTTOM;
	    lost |= SIL_BOTTOM;
	}
	else
	{
	    memcpy (openings + off, floorclip+x1, 2*(rw_stopx-x1));
	    ds->sprbottomclip = off - x1;
	    ds->clipflags |= DS_BOTCLIP;
	}
    }

    if (maskedtexture && !((ds->silhouette | lost)&SIL_TOP))
    {
	ds->silhouette |= SIL_TOP;
	tsil = INT_MIN;
    }
    if (maskedtexture && !((ds->silhouette | lost)&SIL_BOTTOM))
    {
	ds->silhouette |= SIL_BOTTOM;
	bsil = INT_MAX;
    }
    ds->bsilheight = R_SilHeight (bsil);
    ds->tsilheight = R_SilHeight (tsil);
    ds_p++;
}


//
// R_ReplayWall
// Sets up a pass-1 fragment as R_AddLine and R_Subsector did, then
// R_StoreWallRange draws its strip part.
//
void R_ReplayWall (uint32_t frag, int i)
{
    int		start = (frag >> 9) & 511;
    int		stop = frag & 511;
    r_fragc_t*	fc = R_FragSlot (i);

    curlineidx = frag >> 18;
    P_GetSeg(curlineidx, &curline);
    P_GetLine(curline.linedef, &linedef);
    frontsector = curline.side ? linedef.backsector : linedef.frontsector;
    backsector = curline.backsector;
    R_ReadSector (frontsector, &frontsec);
    // only the setup the cache does not hold reads rw_angle1
    if (fc->key != (uint32_t) i + 1)
	rw_angle1 = R_PointToAngle (curline.v1x, curline.v1y);

    if (frontsec.floorheight < viewz)
	R_FindPlane (&floorplane, frontsec.floorheight,
		     frontsec.floorpic, frontsec.lightlevel);
    else
	floorplane.picnum = -1;
    
    if (frontsec.ceilingheight > viewz 
	|| frontsec.ceilingpic == skyflatnum)
	R_FindPlane (&ceilingplane, frontsec.ceilingheight,
		     frontsec.ceilingpic, frontsec.lightlevel);
    else
	ceilingplane.picnum = -1;

    R_StoreWallRange (start, stop, fc, (uint32_t) i + 1);
}
