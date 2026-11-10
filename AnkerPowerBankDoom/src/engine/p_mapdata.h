// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// DESCRIPTION:
//	Accessors for the static map data of the current level.
//	Records come from the map block in the NOR image through the
//	record cache and are decoded into copy-out views. The mutable
//	per-level state (sector heights and lists, line specials,
//	switch textures, validcount) is kept in RAM arrays.
//

#ifndef __P_MAPDATA__
#define __P_MAPDATA__

#include "r_defs.h"

// Counts of the loaded map.
extern int		numsegs;
extern int		numsectors;
extern int		numsubsectors;
extern int		numnodes;
extern int		numlines;
extern int		numsides;

// Mutable state, carved from the level arena (port/obj/pages.c).
//
// Sectors: one status byte each, a 10 B overlay for the sectors that can
// change (the converter's mutable set), everything else from NOR.
typedef struct
{
    short		floor8;		// 1/8 units
    short		ceil8;
    unsigned char	light;		// bits 0-7, bit 8 in flags
    unsigned char	special;
    unsigned char	floorpic;
    unsigned char	ceilpic;
    unsigned char	mover;		// mover slot, 0xff none
    unsigned char	flags;
} secovl_t;

_Static_assert(sizeof(secovl_t) == 10, "a sector overlay is 10 B");

#define SO_LIGHTHI	1

// Status byte bits
#define SS_TRAV1	0x01	// sound flood label 1 (no block line crossed)
#define SS_TRAV2	0x02	// sound flood label 2
#define SS_TARGET	0x04	// soundtarget is the player
#define SS_QUEUED	0x08	// sound flood work pending
#define SS_CORPSE	0x10	// corpse records in the sector

#define NOMOVER		0xff

extern byte*		secstat;
extern secovl_t*	secovl;
extern byte*		linevalid;	// line checked in this traversal
extern byte*		linemapped;	// ML_MAPPED, one bit per line
extern byte*		specialcleared;	// per special-line slot, bit set when cleared
extern byte*		switchflip;	// 3 bits per switch slot: part shows its pair

// The front sides of the scrolling walls (special 48), E1M6 has 9.
#define MAXSCROLLSIDES	16
extern sideidx_t	scrollsides[MAXSCROLLSIDES];
extern byte		numscrollsides;

// Sector fields (the overlay of a mutable sector, NOR otherwise)
fixed_t P_FloorHeight (secidx_t s);
fixed_t P_CeilingHeight (secidx_t s);
int P_LightLevel (secidx_t s);
int P_SecSpecial (secidx_t s);
int P_FloorPic (secidx_t s);
int P_CeilingPic (secidx_t s);
// Mover slot of a sector, NOMOVER when none (vanilla specialdata)
int P_SecMover (secidx_t s);
// Both heights from one record read
void P_SectorHeights (secidx_t s, fixed_t* floorh, fixed_t* ceilh);

// The record of a sector and its overlay, NULL for a sector that never
// changes. The helpers below take a field from the two, so a caller that
// needs several fields reads the record once.
secovl_t* P_SectorRead (secidx_t s, dmc_sector_t* r);

static inline fixed_t P_RecFloorHeight (const secovl_t* o, const dmc_sector_t* r)
{
    return o ? (fixed_t)o->floor8 << 13 : r->floor << FRACBITS;
}

static inline fixed_t P_RecCeilingHeight (const secovl_t* o, const dmc_sector_t* r)
{
    return o ? (fixed_t)o->ceil8 << 13 : r->ceiling << FRACBITS;
}

static inline int P_RecLightLevel (const secovl_t* o, const dmc_sector_t* r)
{
    return o ? o->light | (o->flags & SO_LIGHTHI) << 8 : r->light;
}

static inline int P_RecFloorPic (const secovl_t* o, const dmc_sector_t* r)
{
    return o ? o->floorpic : r->floorpic;
}

static inline int P_RecCeilingPic (const secovl_t* o, const dmc_sector_t* r)
{
    return o ? o->ceilpic : r->ceilingpic;
}

// Overlay of a mutable sector, I_Error for any other.
secovl_t* P_SecOvl (secidx_t s);
void P_SetFloorHeight (secidx_t s, fixed_t h);
void P_SetCeilingHeight (secidx_t s, fixed_t h);
void P_SetLightLevel (secidx_t s, int light);

// Sound origin of a sector (vanilla soundorg), from a small ring.
void* P_SecSoundOrg (secidx_t s);

// Line traversal marks (vanilla validcount): P_NewLineCheck starts a
// traversal, P_LineChecked tests and marks a line.
void P_NewLineCheck (void);
boolean P_LineChecked (lineidx_t l);

// Lines
// A tag-only line (A_BossDeath's tag 666 trigger) for the EV_ functions
// that use nothing but the tag.
#define LINE_TAGONLY	0x8000
#define P_TagOnlyLine(t)	(LINE_TAGONLY | (t))

void P_GetLine (lineidx_t i, line_t* l);
int P_LineTag (lineidx_t i);
int P_LineSpecial (lineidx_t i);
void P_SetLineSpecial (lineidx_t i, int special);
sideidx_t P_LineSide (lineidx_t i, int side);
secidx_t P_LineFrontSector (lineidx_t i);
secidx_t P_LineBackSector (lineidx_t i);
void P_MapLine (lineidx_t i);

// Sides
void P_GetSide (sideidx_t i, side_t* s);
// part: 0 top, 1 middle, 2 bottom
void P_SetSideTexture (sideidx_t i, int part, int texture);

// Segs
void P_DecodeSeg (const dmc_seg_t* r, seg_t* s);
void P_GetSeg (segidx_t i, seg_t* s);
secidx_t P_SegFrontSector (const seg_t* s);
sideidx_t P_SegSide (const seg_t* s);

// Subsectors and nodes
secidx_t P_SubsecSector (ssecidx_t s);
int P_SubsecSegs (ssecidx_t s, int* first);
void P_GetNode (nodeidx_t i, node_t* n);
void P_GetNodeBox (nodeidx_t i, int child, fixed_t* box);

// Sectors: static part
int P_SecTag (secidx_t s);
int P_SecLines (secidx_t s, int* first);
lineidx_t P_SecLine (int k);
void P_SecBlockBox (secidx_t s, int* box);

// Tags: sectors of TAGS index t in ascending order
int P_TagSectors (int t, dmc_range_t* r);
secidx_t P_TagSector (int t, const dmc_range_t* r, int k);

// REJECT bit of a sector pair
int P_Rejected (int s1, int s2);

// Blockmap: first word of the line list of a cell and one word
int P_BlockListStart (int x, int y);
int P_BlockWord (int k);

// A string of the image (DMC_STR_*), in a buffer that the next call
// overwrites (player messages are consumed by the next HU_Ticker).
char* P_String (int id);

// Texture number (global id + 1, 0 none) of a per-map texture index
int P_MapTexture (int maptex);
// Global flat id of a per-map flat index (not the sky)
int P_MapFlat (int mapflat);

#ifdef DMC_PARITY
// Loads the WAD of -iwad and builds the vanilla map structures of each
// level for the comparisons below.
void PAR_Init (void);
void PAR_SetupLevel (int episode, int map);

// Host accessor-parity mode (host/parity.c): every decoded record is
// compared with the WAD-loaded vanilla structures.
void PAR_LineRec (lineidx_t i, const dmc_line_t* r);
void PAR_Line (lineidx_t i, const line_t* l);
void PAR_LineSpecial (lineidx_t i, int special);
void PAR_SetLineSpecial (lineidx_t i, int special);
void PAR_LineSide (lineidx_t i, int side, sideidx_t sd);
void PAR_Side (sideidx_t i, const side_t* s, int switched);
void PAR_Seg (segidx_t i, const seg_t* s);
void PAR_SegRun (ssecidx_t s, const dmc_seg_t* r, int n, int first);
void PAR_Subsec (ssecidx_t s, secidx_t sec);
void PAR_SubsecSegs (ssecidx_t s, int first, int count);
void PAR_Node (nodeidx_t i, const node_t* n);
void PAR_NodeBox (nodeidx_t i, int child, const fixed_t* box);
void PAR_SecTag (secidx_t s, int tag);
void PAR_SecLines (secidx_t s, int first, int count);
void PAR_SecLine (int k, lineidx_t l);
void PAR_BlockBox (secidx_t s, const int* box);
void PAR_TagSectors (int t, int count);
void PAR_TagSector (int t, int k, secidx_t s);
void PAR_Reject (int s1, int s2, int bit);
void PAR_BlockWord (int k, int w);
void PAR_PointInSubsector (fixed_t x, fixed_t y, int ss);
void PAR_Sector (secidx_t s, const dmc_sector_t* r, const dmc_secextra_t* x);
void PAR_Thing (int i, const mapthing_t* t);
void PAR_ThingsEnd (int count);
void PAR_TexEntry (int tex, const dmc_texent_t* e);
void PAR_Patch (int patch, const dmc_patch_t* h);
// rot -1: frame without rotations or the first rotation
void PAR_SpriteFrame (int sprite, int frame, int rot, int patch, int flip);
// sfx after the link, r the snd_get result
void PAR_Sound (int sfx, int r, uint32_t loff, uint16_t len);
#endif

#endif
