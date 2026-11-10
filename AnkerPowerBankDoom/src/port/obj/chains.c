// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright (C) 2026 Artur Bieniek
//
// Blockmap and sector chains. Vanilla links a thing
// at the head of its chain, so a chain is the things linked since the
// level started, most recent first, followed by the map things still at
// their spawn position in reverse spawn order. The first part is a RAM
// hash chain (128 cell and 64 sector buckets, filtered by cell or
// sector), the second the CELLTHINGS or SECTHINGS list of the map block
// filtered by tier.

#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "p_local.h"

#include "things.h"

unsigned short*	o_bhash;
unsigned short*	o_shash;

#define BHASH(c)	((c) & 127)
#define SHASH(s)	((s) & 63)

static int CellOf (fixed_t x, fixed_t y)
{
    unsigned	bx = (x - bmaporgx) >> MAPBLOCKSHIFT;
    unsigned	by = (y - bmaporgy) >> MAPBLOCKSHIFT;

    if (bx >= (unsigned) bmapwidth || by >= (unsigned) bmapheight)
	return -1;
    return by * bmapwidth + bx;
}

static unsigned short* NextOf (unsigned h, int blk)
{
    if (H_KIND(h) == HK_OBJ)
	return blk ? &O_PAGE(H_IDX(h))->bnext : &O_PAGE(H_IDX(h))->snext;
    return blk ? &o_drops[H_IDX(h)].bnext : &o_drops[H_IDX(h)].snext;
}

// Unlinks h, its own next link stays for an iteration in progress.
static boolean Unlink (unsigned short* l, unsigned h, int blk)
{
    while (*l && *l != h)
	l = NextOf(*l, blk);
    if (!*l)
	return false;
    *l = *NextOf(h, blk);
    return true;
}

// Links object or drop h of the given flags at the heads of its chains.
void O_LinkChains (unsigned h, fixed_t x, fixed_t y, secidx_t sec, int flags)
{
    int		c;

    if (!(flags & MF_NOSECTOR))
    {
	*NextOf(h, 0) = o_shash[SHASH(sec)];
	o_shash[SHASH(sec)] = h;
    }
    if (!(flags & MF_NOBLOCKMAP) && (c = CellOf(x, y)) >= 0)
    {
	*NextOf(h, 1) = o_bhash[BHASH(c)];
	o_bhash[BHASH(c)] = h;
    }
}

void O_UnlinkChains (unsigned h, fixed_t x, fixed_t y, secidx_t sec, int flags)
{
    int		c;

    boolean	ok = true;

    if (!(flags & MF_NOSECTOR))
	ok = Unlink(&o_shash[SHASH(sec)], h, 0);
    if (!(flags & MF_NOBLOCKMAP) && (c = CellOf(x, y)) >= 0)
	ok &= Unlink(&o_bhash[BHASH(c)], h, 1);
#ifdef DMC_STATS
    if (!ok)
	I_Error("O_UnlinkChains: %x not in its chains", h);
#endif
}


//
// P_UnsetThingPosition
// Unlinks a thing from block map and sectors. A map thing at its
// spawn position just leaves the NOR lists.
//
void P_UnsetThingPosition (mobj_t* thing)
{
#ifdef DMC_STATS
    if (thing->flags2 & MF2_VIEW)
	I_Error("P_UnsetThingPosition: view of %x", thing->link);
#endif
    if (thing->flags2 & MF2_NORPOS)
    {
	thing->flags2 &= ~MF2_NORPOS;
	O_SetTier(MO_SPAWNIDX(thing), T_ABSENT);
	return;
    }
    O_UnlinkChains(O_Hnd(thing), thing->x, thing->y, thing->sector, thing->flags);
}


//
// P_SetThingPosition
// Links a thing into both a block and a subsector
// based on it's x y.
//
void P_SetThingPosition (mobj_t* thing)
{
#ifdef DMC_STATS
    if (thing->flags2 & MF2_VIEW)
	I_Error("P_SetThingPosition: view of %x", thing->link);
#endif
    thing->sector = R_PointInSector(thing->x, thing->y);
    O_LinkChains(O_Hnd(thing), thing->x, thing->y, thing->sector, thing->flags);
}


#ifdef DMC_STATS
// Host trap: a callback must not write to a view. A view that still
// exists must read back the same: what was written is in its record
// (P_ThingHeightClip). A promoted or removed one is not checked.
static void CheckView (const mobj_t* v, const mobj_t* before)
{
    mobj_t	w;
    unsigned	h = v->link;

    (void)before;
    if (H_KIND(h) == HK_MAP ? (O_Tier(H_IDX(h) - 1) == T_STATIC || O_Tier(H_IDX(h) - 1) == T_SLEEP)
	: o_drops[H_IDX(h)].secinfo != DROP_DEAD)
    {
	O_Get(h, &w);
	// a drop keeps no ceilingz, nothing reads it at rest
	if (H_KIND(h) == HK_DROP)
	    w.ceil8 = v->ceil8;
	if (memcmp(&w, v, sizeof(w)))
	    I_Error("write to the view of %x", h);
    }
}
#define VIEWCALL(r, fn, v) \
    do { mobj_t b_ = *(v); (r) = (fn)(v); CheckView((v), &b_); } while (0)
#else
#define VIEWCALL(r, fn, v)	((r) = (fn)(v))
#endif


//
// Chain
// The things of a blockmap cell (blk) or of a sector, in chain order.
// The next link is read after the call, as vanilla does.
//
static boolean Chain (int blk, int key, thingfn_t func)
{
    unsigned	h;
    unsigned	k;
    uint32_t	n;
    uint32_t	j;
    int		i;
    int		t;
    boolean	r;
    mobj_t*	mo;
    mobj_t	v;
    drop_t*	d;
    unsigned short* next;

    // things linked since the level started
    for (h = blk ? o_bhash[BHASH(key)] : o_shash[SHASH(key)]; h; h = *next)
    {
	next = NextOf(h, blk);
	if (H_KIND(h) == HK_OBJ)
	{
	    mo = O_PAGE(H_IDX(h));
	    if ((blk ? CellOf(mo->x, mo->y) : mo->sector) != key)
		continue;
	    r = func(mo);
	}
	else
	{
	    d = &o_drops[H_IDX(h)];
	    if ((blk ? CellOf(d->x, d->y) : DROP_SEC(d)) != key)
		continue;
	    O_ViewDrop(H_IDX(h), &v);
	    VIEWCALL(r, func, &v);
	}
	if (!r)
	    return false;
    }

    // map things at their spawn position
    k = blk ? map_thinglist(DMC_ST_CELLTHINGS, bmapwidth * bmapheight, key, &n)
	: map_thinglist(DMC_ST_SECTHINGS, numsectors, key, &n);
    for (j = 0; j < n; j++)
    {
	i = map_u16(blk ? DMC_ST_CELLTHINGS : DMC_ST_SECTHINGS, k + j);
	t = O_Tier(i);
	if (t == T_STATIC || t == T_SLEEP)
	{
	    O_ViewThing(i, &v);
	    VIEWCALL(r, func, &v);
	}
	else if (t == T_PROMOTED)
	    r = func(O_NorObject(i));
	else
	    continue;
	if (!r)
	    return false;
    }
    return true;
}


//
// P_BlockThingsIterator
//
boolean
P_BlockThingsIterator
( int			x,
  int			y,
  boolean(*func)(mobj_t*) )
{
    if (x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight)
	return true;
    return Chain(1, y * bmapwidth + x, func);
}


//
// O_SectorThings
// Things of a sector in vanilla thinglist order, without the corpse
// records (the renderer adds those).
//
boolean O_SectorThings (secidx_t sec, thingfn_t fn)
{
    return Chain(0, sec, fn);
}
