// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Map records of the current map block, read through the
// record cache and copied out. The index types are the opaque handles the
// engine uses for static map structures.

#ifndef DMC_REC_H
#define DMC_REC_H

#include <stdint.h>

#include "cache.h"
#include "dmcdat.h"
#include "img.h"

typedef uint16_t lineidx_t;
typedef uint16_t sideidx_t;
typedef uint16_t secidx_t;
typedef uint16_t segidx_t;
typedef uint16_t nodeidx_t;
typedef uint16_t ssecidx_t;

#define IDX_NONE 0xFFFFu

typedef struct {
    uint32_t st[DMC_NSTREAMS];  // logical offsets of the streams
    uint32_t tagval;            // logical offset of the TAGS value table
    uint16_t nlines, nsides, nsegs, nssectors, nnodes, nsectors, nthings;
    uint16_t ntags, nseclines, nteledest, nscrollers, nmutable, nswitches, nspeclines;
    int16_t bmap_ox, bmap_oy;
    uint16_t bmap_cols, bmap_rows, root;
    uint8_t ntex, nflat;
    uint8_t episode, map, next, secret_next;
    uint16_t sky;               // global texture id
    uint16_t name_string;       // STRINGS id of the level name
    uint16_t par_seconds;
    dmc_plan_t plan;            // plan of the loaded skill group
} dmc_map_t;

extern dmc_map_t M;

// Index of the map table entry for ExMy, -1 when the pack does not have it.
int map_find(int episode, int map);

// Loads the directory of map table entry k for rngk skill group g.
void map_load(int k, int g);

#define MAP_REC(s, i, sz) (M.st[s] + (uint32_t)(sz) * (i))

static inline void map_node(nodeidx_t i, dmc_node_t *o)
{
    rec_get(MAP_REC(DMC_ST_NODES, i, 16), o, 16);
}

static inline void map_nodebb(nodeidx_t i, dmc_nodebb_t *o)
{
    rec_get(MAP_REC(DMC_ST_NODEBB, i, 16), o, 16);
}

static inline void map_ssector(ssecidx_t i, dmc_ssector_t *o)
{
    rec_get(MAP_REC(DMC_ST_SSECTORS, i, 8), o, 8);
}

static inline void map_seg(segidx_t i, dmc_seg_t *o)
{
    rec_get(MAP_REC(DMC_ST_SEGS, i, 16), o, 16);
}

static inline void map_line(lineidx_t i, dmc_line_t *o)
{
    rec_get(MAP_REC(DMC_ST_LINES, i, 16), o, 16);
}

static inline void map_linesides(lineidx_t i, dmc_linesides_t *o)
{
    rec_get(MAP_REC(DMC_ST_LINESIDES, i, 4), o, 4);
}

static inline void map_side(sideidx_t i, dmc_side_t *o)
{
    rec_get(MAP_REC(DMC_ST_SIDES, i, 8), o, 8);
}

static inline void map_sector(secidx_t i, dmc_sector_t *o)
{
    rec_get(MAP_REC(DMC_ST_SECTORS, i, 16), o, 16);
}

static inline void map_secextra(secidx_t i, dmc_secextra_t *o)
{
    rec_get(MAP_REC(DMC_ST_SECEXTRA, i, 8), o, 8);
}

static inline void map_thing(unsigned i, dmc_thing_t *o)
{
    rec_get(MAP_REC(DMC_ST_THINGS, i, 12), o, 12);
}

// Element k of a stream of u16 values.
static inline uint16_t map_u16(int st, uint32_t k)
{
    return rec_u16(M.st[st] + 2 * k);
}

static inline uint8_t map_u8(int st, uint32_t k)
{
    uint8_t v;
    rec_get(M.st[st] + k, &v, 1);
    return v;
}

static inline void map_range(int st, uint32_t k, dmc_range_t *o)
{
    rec_get(M.st[st] + 4 * k, o, 4);
}

// Blockmap: word k of the vanilla lump from byte 8 on (cell offsets, then
// the line lists, each starting with the 0 entry and ending with 0xFFFF).
static inline uint16_t map_blockmap(uint32_t k)
{
    return map_u16(DMC_ST_BLOCKMAP, k);
}

// REJECT byte k, GRID start node of a blockmap cell.
static inline uint8_t map_reject(uint32_t k)
{
    return map_u8(DMC_ST_REJECT, k);
}

static inline uint16_t map_grid(uint32_t cell)
{
    return map_u16(DMC_ST_GRID, cell);
}

// Static thing lists of a blockmap cell or a sector (reverse spawn order):
// the k-th thing index of the list is map_u16(st, n + 1 + first + k) with
// first = map_u16(st, i) and count = map_u16(st, i + 1) - first, n the
// number of cells or sectors.
static inline uint16_t map_thinglist(int st, uint32_t n, uint32_t i, uint32_t *count)
{
    uint16_t first = map_u16(st, i);
    *count = (uint32_t)map_u16(st, i + 1) - first;
    return (uint16_t)(n + 1 + first);
}

// Tag lists: sector count and k-th sector of tag index t, tag value of t.
static inline uint16_t map_tagsec(const dmc_range_t *r, unsigned k)
{
    return rec_u16(M.st[DMC_ST_TAGS] + 4u * (M.ntags + 1u) + 2u * (r->first + k));
}

static inline uint16_t map_tagval(unsigned t)
{
    return rec_u16(M.tagval + 2 * t);
}

// Tag index of a tag value, 0 when the map has no such tag.
unsigned map_tagindex(int value);

// Segs of a subsector the engine reads in one run (r_bsp.c buffer). The E1
// maps have at most 20, the data layer test checks every map of a pack.
#define MAP_SEGRUN_MAX 32

// Segs of subsector s in one NOR transaction into buf (at most max records).
// Returns the count, *first receives the index of the first seg.
int map_seg_run(ssecidx_t s, dmc_seg_t *buf, int max, segidx_t *first, secidx_t *sector);

#endif
