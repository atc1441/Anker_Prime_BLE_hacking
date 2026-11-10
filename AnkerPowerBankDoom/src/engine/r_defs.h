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
//      Refresh/rendering module, shared data struct definitions.
//


#ifndef __R_DEFS__
#define __R_DEFS__


// Screenwidth.
#include "doomdef.h"

// Some more or less basic data types
// we depend on.
#include "m_fixed.h"

// We rely on the thinker data struct
// to handle sound origins in sectors.
#include "d_think.h"
// SECTORS do store MObjs anyway.
#include "p_mobj.h"

#include "i_video.h"

#include "v_patch.h"

#include "rec.h"




// Silhouette, needed for clipping Segs (mainly)
// and sprites representing things.
#define SIL_NONE		0
#define SIL_BOTTOM		1
#define SIL_TOP			2
#define SIL_BOTH		3

#define MAXDRAWSEGS		256





//
// INTERNAL MAP TYPES
//  used by play and refresh
//

// A sound origin of a sector (P_SecSoundOrg): the sound code reads
// x and y as the first fields of an mobj_t.
typedef struct
{
    fixed_t		x;
    fixed_t		y;
    fixed_t		z;

} degenmobj_t;

// The sector state lives in the level arena (p_mapdata.h): a status
// byte per sector and an overlay for the sectors that can change.




//
// The SideDef, decoded from the map block.
//

typedef struct
{
    // add this to the calculated texture column
    fixed_t	textureoffset;
    
    // add this to the calculated texture top
    fixed_t	rowoffset;

    // Texture numbers (global id + 1, 0 is no texture).
    short	toptexture;
    short	bottomtexture;
    short	midtexture;

} side_t;



//
// Move clipping aid for LineDefs.
//
typedef enum
{
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE

} slopetype_t;



//
// A LineDef, decoded from the map block (copy-out view).
//
typedef struct line_s
{
    // Vertices, from v1 to v2.
    fixed_t	v1x;
    fixed_t	v1y;
    fixed_t	v2x;
    fixed_t	v2y;

    // Precalculated v2 - v1 for side checking.
    fixed_t	dx;
    fixed_t	dy;

    // Animation related.
    short	flags;
    short	special;
    short	tag;		// TAGS index, 0 none

    // Neat. Another bounding box, for the extent
    //  of the LineDef.
    fixed_t	bbox[4];

    // To aid move clipping.
    slopetype_t	slopetype;

    // Front and back sector, IDX_NONE when there is no back side.
    secidx_t	frontsector;
    secidx_t	backsector;

} line_t;




//
// The LineSeg, decoded from the map block.
//
typedef struct
{
    fixed_t	v1x;
    fixed_t	v1y;
    fixed_t	v2x;
    fixed_t	v2y;
    
    fixed_t	offset;

    angle_t	angle;

    lineidx_t	linedef;
    int		side;		// side of the line the seg is on
    int		hasmid;		// its sidedef has a middle texture

    // backsector is IDX_NONE for one sided lines
    secidx_t	backsector;
    
} seg_t;



//
// BSP node, decoded from the map block.
//
typedef struct
{
    // Partition line.
    fixed_t	x;
    fixed_t	y;
    fixed_t	dx;
    fixed_t	dy;

    // If NF_SUBSECTOR its a subsector.
    unsigned short children[2];
    
} node_t;




// PC direct to screen pointers
//B UNUSED - keep till detailshift in r_draw.c resolved
//extern byte*	destview;
//extern byte*	destscreen;





//
// OTHER TYPES
//

// This could be wider for >8 bit display.
// Indeed, true color support is posibble
//  precalculating 24bpp lightmap/colormap LUT.
//  from darkening PLAYPAL to all black.
// Could even us emore than 32 levels.
typedef byte	lighttable_t;	




//
// A drawseg of the current strip (32 bytes). x1 and x2 are the strip
// part of the fragment, fx1 and the scales belong to the whole fragment,
// so sprite clipping and masked middles see vanilla values for any strip
// width. The clip arrays are offsets into openings such that
// openings[off + x] holds column x, unless clipflags says the drawseg
// has none (vanilla NULL) or uses a constant array.
//
#define DS_TOPCLIP	1	// sprtopclip in openings
#define DS_TOPFULL	2	// sprtopclip is screenheightarray
#define DS_BOTCLIP	4	// sprbottomclip in openings
#define DS_BOTFULL	8	// sprbottomclip is negonearray
#define DS_MASKED	16	// maskedtexturecol in openings

// bsilheight and tsilheight in 1/8 units, with the vanilla sentinels
#define SIL_MAXH	0x7fff	// INT_MAX
#define SIL_MINH	(-0x8000)	// INT_MIN

typedef struct drawseg_s
{
    segidx_t		curline;
    byte		x1;		// strip-local columns
    byte		x2;
    short		fx1;		// first column of the whole fragment
    byte		silhouette;	// 0=none, 1=bottom, 2=top, 3=both
    byte		clipflags;
    fixed_t		scale1;
    fixed_t		scale2;
    fixed_t		scalestep;
    short		bsilheight;	// do not clip sprites above this
    short		tsilheight;	// do not clip sprites below this
    short		sprtopclip;
    short		sprbottomclip;
    short		maskedtexturecol;
    short		pad;

} drawseg_t;



// A vissprite_t is a thing that will be drawn
// in the current strip (36 bytes).
#define VS_SHADOW	0xff	// colormap: fuzz draw

typedef struct vissprite_s
{
    byte		x1;		// strip-local columns
    byte		x2;

    // COLORMAP row, VS_SHADOW for the shadow draw
    byte		colormap;

    // translation bits of the thing (MF_TRANSLATION >> MF_TRANSSHIFT)
    byte		translation;

    // for line side calculation
    fixed_t		gx;
    fixed_t		gy;		

    // global bottom / top for silhouette clipping
    fixed_t		gz;
    fixed_t		gzt;

    // horizontal position of x1
    fixed_t		startfrac;
    
    fixed_t		scale;
    
    // negative if flipped
    fixed_t		xiscale;	

    unsigned short	patch;
    unsigned short	pad;
    
} vissprite_t;


//	
// Sprites are patches with a special naming convention
//  so they can be recognized by R_InitSprites.
// The base name is NNNNFx or NNNNFxFx, with
//  x indicating the rotation, x = 0, 1-7.
// The sprite and frame specified by a thing_t
//  is range checked at run time.
// A sprite is a patch_t that is assumed to represent
//  a three dimensional object and may have multiple
//  rotations pre drawn.
// Horizontal flipping is used to save space,
//  thus NNNNF2F5 defines a mirrored patch.
// Some sprites will only have one picture used
// for all views: NNNNF0
//
typedef struct
{
    // If false use 0 for any position.
    // Note: as eight entries are available,
    //  we might as well insert the same name eight times.
    boolean	rotate;

    // Lump to use for view angles 0-7.
    short	lump[8];

    // Flip bit (1 = flip) to use for view angles 0-7.
    byte	flip[8];
    
} spriteframe_t;



//
// A sprite definition:
//  a number of animation frames.
//
typedef struct
{
    int			numframes;
    spriteframe_t*	spriteframes;

} spritedef_t;



//
// Now what is a visplane, anyway?
// A visplane of the current strip: one top and bottom row per strip
// column, with pads before and after, follows the header in the pool.
// A slot is taken at the first column a wall marks (lazy allocation).
//
typedef struct
{
  short		height;		// 1/8 units, 0 for the sky
  byte		picnum;		// per-map flat index
  byte		lightlevel;	// clamped to 255
  byte		minx;		// strip-local columns
  byte		maxx;

} visplane_t;




#endif
