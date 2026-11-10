// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright (C) 2026 Artur Bieniek
//
// DESCRIPTION:
//	Accessors for the static map data of the current level.
//

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "m_bbox.h"
#include "p_local.h"

#include "p_mapdata.h"
#include "fetch.h"

int		numsegs;
int		numsectors;
int		numsubsectors;
int		numnodes;
int		numlines;
int		numsides;

byte*		secstat;
secovl_t*	secovl;
byte*		linevalid;
byte*		linemapped;
byte*		specialcleared;
byte*		switchflip;

sideidx_t	scrollsides[MAXSCROLLSIDES];
byte		numscrollsides;

// Blockmap size in mapblocks and origin (p_setup.c sets them, which is
// overlay code without data of its own on the device).
int		bmapwidth;
int		bmapheight;
fixed_t		bmaporgx;
fixed_t		bmaporgy;


//
// SECTORS
// A mutable sector keeps its state in its overlay, the rest of the
// sectors never change and are read from NOR.
//
secovl_t* P_SecOvl (secidx_t s)
{
    dmc_sector_t	r;

    map_sector(s, &r);
    // a converter gap in the mutable set would write past the overlays
    if (r.overlay == DMC_NONE8)
	I_Error("P_SecOvl: sector %i is not mutable", s);
    return &secovl[r.overlay];
}

secovl_t* P_SectorRead (secidx_t s, dmc_sector_t* r)
{
    map_sector(s, r);
    return r->overlay == DMC_NONE8 ? NULL : &secovl[r->overlay];
}

fixed_t P_FloorHeight (secidx_t s)
{
    dmc_sector_t	r;
    secovl_t*		o = P_SectorRead(s, &r);

    return P_RecFloorHeight(o, &r);
}

fixed_t P_CeilingHeight (secidx_t s)
{
    dmc_sector_t	r;
    secovl_t*		o = P_SectorRead(s, &r);

    return P_RecCeilingHeight(o, &r);
}

void P_SectorHeights (secidx_t s, fixed_t* floorh, fixed_t* ceilh)
{
    dmc_sector_t	r;
    secovl_t*		o = P_SectorRead(s, &r);

    *floorh = P_RecFloorHeight(o, &r);
    *ceilh = P_RecCeilingHeight(o, &r);
}

int P_LightLevel (secidx_t s)
{
    dmc_sector_t	r;
    secovl_t*		o = P_SectorRead(s, &r);

    return P_RecLightLevel(o, &r);
}

int P_SecSpecial (secidx_t s)
{
    dmc_sector_t	r;
    secovl_t*		o = P_SectorRead(s, &r);

    return o ? o->special : r.special;
}

int P_FloorPic (secidx_t s)
{
    dmc_sector_t	r;
    secovl_t*		o = P_SectorRead(s, &r);

    return P_RecFloorPic(o, &r);
}

int P_CeilingPic (secidx_t s)
{
    dmc_sector_t	r;
    secovl_t*		o = P_SectorRead(s, &r);

    return P_RecCeilingPic(o, &r);
}

int P_SecMover (secidx_t s)
{
    dmc_sector_t	r;
    secovl_t*		o = P_SectorRead(s, &r);

    return o ? o->mover : NOMOVER;
}

void P_SetFloorHeight (secidx_t s, fixed_t h)
{
    P_SecOvl(s)->floor8 = TO8(h);
}

void P_SetCeilingHeight (secidx_t s, fixed_t h)
{
    P_SecOvl(s)->ceil8 = TO8(h);
}

void P_SetLightLevel (secidx_t s, int light)
{
    secovl_t*	o = P_SecOvl(s);

    o->light = light;
    o->flags = (o->flags & ~SO_LIGHTHI) | ((light >> 8) & SO_LIGHTHI);
}

// Sound origins of sectors for the sound code, which keeps the pointer
// while the sound plays, compares it to tell origins apart and reads x
// and y through it (the first fields of mobj_t). The PC speaker plays
// one sound at a time, so a ring of four is plenty.
#define NUMSOUNDORGS	4
static fixed_t		soundorgxy[NUMSOUNDORGS][2];
static unsigned short	soundorgsec[NUMSOUNDORGS];
static byte		nextsoundorg;

void* P_SecSoundOrg (secidx_t s)
{
    dmc_secextra_t	x;
    int			i;

    for (i = 0; i < NUMSOUNDORGS; i++)
	if (soundorgsec[i] == s + 1)
	    return soundorgxy[i];
    i = nextsoundorg++ & (NUMSOUNDORGS - 1);
    map_secextra(s, &x);
    soundorgsec[i] = s + 1;
    soundorgxy[i][0] = x.soundorg_x << FRACBITS;
    soundorgxy[i][1] = x.soundorg_y << FRACBITS;
    return soundorgxy[i];
}


//
// LINE TRAVERSALS
// One bit per line, cleared when a traversal starts (vanilla validcount).
//
void P_NewLineCheck (void)
{
    memset(linevalid, 0, (numlines + 7) >> 3);
}

boolean P_LineChecked (lineidx_t l)
{
    byte*	b = &linevalid[l >> 3];
    int		m = 1 << (l & 7);

    if (*b & m)
	return true;
    *b |= m;
    return false;
}


//
// LINES
// Every line record goes through P_LineRec, which the parity build
// checks field by field against the WAD.
//
static void P_LineRec (lineidx_t i, dmc_line_t* r)
{
    map_line(i, r);
#ifdef DMC_PARITY
    PAR_LineRec(i, r);
#endif
}

static int P_SlotSpecial (const dmc_line_t* r)
{
    if (r->slot == DMC_NONE8
	|| (specialcleared[r->slot >> 3] & (1 << (r->slot & 7))))
	return 0;
    return r->special;
}

void P_GetLine (lineidx_t i, line_t* l)
{
    dmc_line_t	r;

    P_LineRec(i, &r);
    l->v1x = r.x1 << FRACBITS;
    l->v1y = r.y1 << FRACBITS;
    l->dx = r.dx << FRACBITS;
    l->dy = r.dy << FRACBITS;
    l->v2x = l->v1x + l->dx;
    l->v2y = l->v1y + l->dy;
    l->flags = r.flags;
    if (linemapped[i >> 3] & (1 << (i & 7)))
	l->flags |= ML_MAPPED;
    l->special = P_SlotSpecial(&r);
    l->tag = r.tag;
    l->frontsector = r.front;
    l->backsector = r.back;

    if (l->v1x < l->v2x)
    {
	l->bbox[BOXLEFT] = l->v1x;
	l->bbox[BOXRIGHT] = l->v2x;
    }
    else
    {
	l->bbox[BOXLEFT] = l->v2x;
	l->bbox[BOXRIGHT] = l->v1x;
    }
    if (l->v1y < l->v2y)
    {
	l->bbox[BOXBOTTOM] = l->v1y;
	l->bbox[BOXTOP] = l->v2y;
    }
    else
    {
	l->bbox[BOXBOTTOM] = l->v2y;
	l->bbox[BOXTOP] = l->v1y;
    }

    // FixedDiv (dy, dx) > 0 exactly when the signs agree
    if (!l->dx)
	l->slopetype = ST_VERTICAL;
    else if (!l->dy)
	l->slopetype = ST_HORIZONTAL;
    else if ((l->dx > 0) == (l->dy > 0))
	l->slopetype = ST_POSITIVE;
    else
	l->slopetype = ST_NEGATIVE;

#ifdef DMC_PARITY
    PAR_Line(i, l);
#endif
}

int P_LineTag (lineidx_t i)
{
    dmc_line_t	r;

    if (i & LINE_TAGONLY)
	return i & ~LINE_TAGONLY;
    P_LineRec(i, &r);
    return r.tag;
}

int P_LineSpecial (lineidx_t i)
{
    dmc_line_t	r;
    int		special;

    P_LineRec(i, &r);
    special = P_SlotSpecial(&r);
#ifdef DMC_PARITY
    PAR_LineSpecial(i, special);
#endif
    return special;
}

// Line specials only ever change to 0 (one-shot triggers), so a bit
// per special-line slot holds the state.
void P_SetLineSpecial (lineidx_t i, int special)
{
    dmc_line_t	r;
    int		bit;

#ifdef DMC_PARITY
    PAR_SetLineSpecial(i, special);
#endif
    P_LineRec(i, &r);
#ifdef DMC_STATS
    // game code only clears specials
    if (special)
	I_Error("P_SetLineSpecial: line %i cannot take special %i", i, special);
#endif
    if (r.slot == DMC_NONE8)
	return;
    bit = 1 << (r.slot & 7);
    specialcleared[r.slot >> 3] |= bit;
}

sideidx_t P_LineSide (lineidx_t i, int side)
{
    dmc_linesides_t	r;

    map_linesides(i, &r);
#ifdef DMC_PARITY
    PAR_LineSide(i, side, r.side[side]);
#endif
    return r.side[side];
}

secidx_t P_LineFrontSector (lineidx_t i)
{
    dmc_line_t	r;

    P_LineRec(i, &r);
    return r.front;
}

secidx_t P_LineBackSector (lineidx_t i)
{
    dmc_line_t	r;

    P_LineRec(i, &r);
    return r.back;
}

void P_MapLine (lineidx_t i)
{
    linemapped[i >> 3] |= 1 << (i & 7);
}


//
// STRINGS
//
char* P_String (int id)
{
    // the longest message or level name of E1 has 43 characters
    static char	buf[48];

    return (char *) str_get(id, buf, sizeof(buf));
}


//
// SIDES
//
int P_MapTexture (int maptex)
{
    return maptex ? map_u16(DMC_ST_TEXXLAT, maptex) + 1 : 0;
}

int P_MapFlat (int mapflat)
{
    return map_u16(DMC_ST_FLATXLAT, mapflat);
}

void P_GetSide (sideidx_t i, side_t* s)
{
    dmc_side_t	r;
    int		k;
    int		switched = 0;

    map_side(i, &r);
    s->textureoffset = r.textureoffset << FRACBITS;
    s->rowoffset = r.rowoffset << FRACBITS;
    s->toptexture = P_MapTexture(r.top);
    s->midtexture = P_MapTexture(r.mid);
    s->bottomtexture = P_MapTexture(r.bottom);

    // a switch part shows the other texture of its pair, ids 2k, 2k + 1
    if (r.switch_slot != DMC_NONE8)
    {
	short*	part[3] = { &s->toptexture, &s->midtexture, &s->bottomtexture };

	for (k = 0; k < 3; k++)
	    if (switchflip[(3 * r.switch_slot + k) >> 3] & (1 << ((3 * r.switch_slot + k) & 7)))
		*part[k] = ((*part[k] - 1) ^ 1) + 1, switched = 1;
    }

    // Scrolling walls move one unit per tic since the level started,
    // as P_UpdateSpecials did to the side.
    for (k = 0; k < numscrollsides; k++)
	if (scrollsides[k] == i)
	    s->textureoffset += leveltime << FRACBITS;

#ifdef DMC_PARITY
    PAR_Side(i, s, switched);
#endif
}

void P_SetSideTexture (sideidx_t i, int part, int texture)
{
    dmc_side_t	r;
    int		base;
    int		k;

    map_side(i, &r);
    base = P_MapTexture(part == 0 ? r.top : part == 1 ? r.mid : r.bottom);
#ifdef DMC_STATS
    if (r.switch_slot == DMC_NONE8
	|| (texture != base && texture != ((base - 1) ^ 1) + 1))
	I_Error("P_SetSideTexture: side %i cannot show texture %i", i, texture);
#endif
    k = 3 * r.switch_slot + part;
    if (texture == base)
	switchflip[k >> 3] &= ~(1 << (k & 7));
    else
	switchflip[k >> 3] |= 1 << (k & 7);
}


//
// SEGS
//
void P_DecodeSeg (const dmc_seg_t* r, seg_t* s)
{
    s->v1x = r->v1x << FRACBITS;
    s->v1y = r->v1y << FRACBITS;
    s->v2x = r->v2x << FRACBITS;
    s->v2y = r->v2y << FRACBITS;
    s->offset = r->offset << FRACBITS;
    s->angle = (angle_t)r->angle << 16;
    s->linedef = r->line & DMC_SEG_LINE_MASK;
    s->side = (r->line & DMC_SEG_SIDE) != 0;
    s->hasmid = (r->line & DMC_SEG_HASMID) != 0;
    s->backsector = r->backsector;
}

void P_GetSeg (segidx_t i, seg_t* s)
{
    dmc_seg_t	r;

    map_seg(i, &r);
    P_DecodeSeg(&r, s);
#ifdef DMC_PARITY
    PAR_Seg(i, s);
#endif
}

secidx_t P_SegFrontSector (const seg_t* s)
{
    dmc_line_t	r;

    P_LineRec(s->linedef, &r);
    return s->side ? r.back : r.front;
}

sideidx_t P_SegSide (const seg_t* s)
{
    return P_LineSide(s->linedef, s->side);
}


//
// SUBSECTORS, NODES
//
secidx_t P_SubsecSector (ssecidx_t s)
{
    dmc_ssector_t	r;

    map_ssector(s, &r);
#ifdef DMC_PARITY
    PAR_Subsec(s, r.sector);
#endif
    return r.sector;
}

int P_SubsecSegs (ssecidx_t s, int* first)
{
    dmc_ssector_t	r;

    map_ssector(s, &r);
    *first = r.firstseg;
#ifdef DMC_PARITY
    PAR_SubsecSegs(s, r.firstseg, r.numsegs);
#endif
    return r.numsegs;
}

void P_GetNode (nodeidx_t i, node_t* n)
{
    dmc_node_t	r;

    map_node(i, &r);
    n->x = r.x << FRACBITS;
    n->y = r.y << FRACBITS;
    n->dx = r.dx << FRACBITS;
    n->dy = r.dy << FRACBITS;
    n->children[0] = r.child[0];
    n->children[1] = r.child[1];
#ifdef DMC_PARITY
    PAR_Node(i, n);
#endif
}

void P_GetNodeBox (nodeidx_t i, int child, fixed_t* box)
{
    dmc_nodebb_t	r;
    int			k;

    map_nodebb(i, &r);
    for (k = 0; k < 4; k++)
	box[k] = r.bbox[child * 4 + k] << FRACBITS;
#ifdef DMC_PARITY
    PAR_NodeBox(i, child, box);
#endif
}


//
// SECTORS
//
int P_SecTag (secidx_t s)
{
    dmc_sector_t	r;

    map_sector(s, &r);
#ifdef DMC_PARITY
    PAR_SecTag(s, r.tag);
#endif
    return r.tag;
}

int P_SecLines (secidx_t s, int* first)
{
    dmc_sector_t	r;

    map_sector(s, &r);
    *first = r.firstline;
#ifdef DMC_PARITY
    PAR_SecLines(s, r.firstline, r.linecount);
#endif
    return r.linecount;
}

lineidx_t P_SecLine (int k)
{
    lineidx_t	l = map_u16(DMC_ST_SECLINES, k);

#ifdef DMC_PARITY
    PAR_SecLine(k, l);
#endif
    return l;
}

void P_SecBlockBox (secidx_t s, int* box)
{
    dmc_secextra_t	r;
    int			k;

    map_secextra(s, &r);
    for (k = 0; k < 4; k++)
	box[k] = r.blockbox[k];
#ifdef DMC_PARITY
    PAR_BlockBox(s, box);
#endif
}

int P_TagSectors (int t, dmc_range_t* r)
{
    map_range(DMC_ST_TAGS, t, r);
#ifdef DMC_PARITY
    PAR_TagSectors(t, r->count);
#endif
    return r->count;
}

secidx_t P_TagSector (int t, const dmc_range_t* r, int k)
{
    secidx_t	s = map_tagsec(r, k);

#ifdef DMC_PARITY
    PAR_TagSector(t, k, s);
#endif
    return s;
}

int P_Rejected (int s1, int s2)
{
    int		pnum = s1 * numsectors + s2;
    int		bit = (map_reject(pnum >> 3) >> (pnum & 7)) & 1;

#ifdef DMC_PARITY
    PAR_Reject(s1, s2, bit);
#endif
    return bit;
}


//
// BLOCKMAP
//
int P_BlockListStart (int x, int y)
{
    // offsets count words of the vanilla lump, the stream starts at word 4
    return map_blockmap(y * bmapwidth + x) - 4;
}

int P_BlockWord (int k)
{
    int		w = map_blockmap(k);

#ifdef DMC_PARITY
    PAR_BlockWord(k, w);
#endif
    return w;
}
