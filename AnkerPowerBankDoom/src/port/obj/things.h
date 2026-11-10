// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Object store and level arena.
//
// Tiers of a map thing (2 bits each): absent (not spawned, removed, or
// promoted and moved off its NOR list position), static (items and
// decorations, state derived from type, spawn phase and leveltime),
// sleeping monster (1 B), and promoted (a full object still at its NOR
// position). Full objects are 60 B pages. Dropped items at rest are 16 B
// records, corpses at rest 8 B records, seven to a page.
//
// Thinker order: segment 1 is the map things in index order (sleeping
// monsters and the promoted list, sorted by map index), segment 2 the
// level start lights, then movers, lights and objects in creation order.
// Blockmap and sector chains are RAM hash chains (most recently linked
// first) followed by the NOR per-cell or per-sector lists.

#ifndef DMC_THINGS_H
#define DMC_THINGS_H

#include <stddef.h>

#include "doomtype.h"
#include "d_loop.h"
#include "p_local.h"

// Handles: bits 15-14 kind, bits 13-0 index, 0 is no object.
#define H_NONE		0
#define H_DANGLE	0xffffu	// an object that no longer exists
#define HK_MAP		0
#define HK_OBJ		1
#define HK_DROP		2
#define HK_CORPSE	3
#define H_KIND(h)	((unsigned)(h) >> 14)
#define H_IDX(h)	((h) & 0x3fff)
#define H_MAP(i)	((i) + 1)
#define H_OBJ(p)	(0x4000 | (p))
#define H_DROP(s)	(0x8000 | (s))
#define H_CORPSE(p, k)	(0xc000 | (p) << 3 | (k))

// Segment 2 links: an object page, a mover or a light slot, 0 ends.
#define L_OBJ		0x4000
#define L_MOVER		0x8000
#define L_LIGHT		0xc000

// Tiers
#define T_ABSENT	0
#define T_STATIC	1
#define T_SLEEP		2
#define T_PROMOTED	3

// Sleeping monster byte: tics, stand state, lastlook, target is player
#define SL_TICS		0x0f
#define SL_STAND2	0x10
#define SL_LOOKSHIFT	5
#define SL_TARGET	0x80

#define PAGE_BYTES	60
#define PAGE_RESERVE	8	// pages kept for the player, missiles and damage
#define CORPSES_PER_PAGE 7

extern byte*		o_pages;
#define O_PAGE(p)	((mobj_t *)(o_pages + PAGE_BYTES * (p)))
#define O_PAGEIDX(mo)	((int)(((byte *)(mo) - o_pages) / PAGE_BYTES))

// Dropped item at rest (16 B): sector in bits 0-11 of secinfo, the drop
// type (clip, shotgun, chaingun) in bits 12-13, lastlook in bits 14-15.
typedef struct
{
    fixed_t		x;
    fixed_t		y;
    short		z8;
    unsigned short	bnext;
    unsigned short	snext;
    unsigned short	secinfo;	// DROP_DEAD: picked up
} drop_t;

_Static_assert(sizeof(drop_t) == 16, "a drop record is 16 B");

#define DROP_SEC(d)	((d)->secinfo & 0xfff)
#define DROP_TYPE(d)	(((d)->secinfo >> 12) & 3)
#define DROP_LOOK(d)	((d)->secinfo >> 14)
#define DROP_DEAD	0xffff

// Corpse at rest (8 B), cosmetic: map units, sector in bits 0-11 of
// secinfo, bit 15 MF_SHADOW (a spectre).
typedef struct
{
    short		x;
    short		y;
    unsigned short	statenum;
    unsigned short	secinfo;
} corpse_t;

_Static_assert(sizeof(corpse_t) == 8, "a corpse record is 8 B");

#define CORPSE_SEC(c)	((c)->secinfo & 0xfff)
#define CORPSE_SHADOW	0x8000

// Page of corpse records: seven records, the FIFO link and the count.
// flags2 is MF2_REMOVED at the place of an object's flags2, so a handle
// of the page reads as no object.
typedef struct
{
    corpse_t		c[CORPSES_PER_PAGE];
    unsigned short	next;
    unsigned char	flags2;
    unsigned char	count;
} corpsepage_t;

_Static_assert(sizeof(corpsepage_t) == PAGE_BYTES, "seven corpses fill a page");
_Static_assert(offsetof(corpsepage_t, flags2) == offsetof(mobj_t, flags2),
	       "a corpse page reads as a removed object");

// Counted events, must stay 0 in recorded play.
enum
{
    EV_WAKE_DEFERRED,	// no non-reserve page for a wake-up
    EV_TRANSIENT_SKIPPED,	// puff, blood or fog without a page
    EV_MOVER_DEFERRED,	// mover pool full, line special kept
    EV_EVICTED,		// reserve gone, the oldest transient evicted
    EV_OVERRUN,		// no page at all for a promotion or missile
    EV_COUNT
};

typedef struct
{
    unsigned short events[EV_COUNT];
#ifdef DMC_STATS
    // host statistics: the level, set at level start, and peaks
    int		episode, map, skill, plan;
    int		arena, fixed, pages;
    int		peak_full;	// pages holding full objects
    int		peak_awake;	// promoted monsters alive
    int		peak_corpses;	// corpse records
    int		peak_drops;	// drop records
    int		peak_movers;
    int		peak_pages;	// pages in use (objects and corpses)
    int		clip_promoted;	// statics promoted by a height clip
    int		corpse_evicted;	// corpse pages evicted
    int		zombies;	// removed objects kept for a reference
    int		boss;		// A_BossDeath lowered the tag 666 floor
    int		first_event;	// gametic of the first counted event, -1
    int		corpse_trace;	// traces vanilla's corpses take past 128
#endif
} ostats_t;

extern ostats_t		o_stats;

// Counts an event, the host keeps the tic of the first one of the level.
#ifdef DMC_STATS
#define O_EVENT(e) \
    (o_stats.events[e]++, o_stats.first_event < 0 ? (o_stats.first_event = gametic) : 0)
#else
#define O_EVENT(e)	(o_stats.events[e]++)
#endif

// Host debug modes: every map thing a full object, drops and corpses
// too (o_promote_all), corpses at rest stay full objects (o_keep_corpses,
// the frame test of the other views).
#ifdef DMC_STATS
extern boolean		o_promote_all;
extern boolean		o_keep_corpses;
#else
#define o_promote_all	0
#define o_keep_corpses	0
#endif


//
// level.c: level start (LEVEL overlay code).
//
// The level arena ends at O_ARENA_END: the end of SRAM on the meter, an
// array with room for the promote everything mode on the host.
#ifdef DMC_STATS
#define O_ARENA_MAX	(512 * 1024)
extern int		o_arena_words[O_ARENA_MAX / 4];
#define O_ARENA_END	((byte *) o_arena_words + O_ARENA_MAX)
#else
extern byte		__ram_end[];
#define O_ARENA_END	__ram_end
#endif

// Carves the level arena of the loaded map's plan, returns the bytes of
// its level-fixed part.
int	O_ArenaInit (void);
// Bytes at the end of the arena that the pages leave free and O_ArenaInit
// does not clear, normally 0: the firmware's timed demo pass keeps its
// statistics there through the levels of a demo (port/game/dmc_demo.c).
extern int	o_arena_tail;
void	O_PagesInit (byte* base, int n);
void	O_SpawnLevel (void);
void	O_AddSeg1 (mobj_t* mo, int i);
void	O_ResetThinkers (void);
void	P_ClearMovers (void);


//
// pages.c: page pool and drop slots.
//
// Free list, corpse page FIFO (oldest first), zombie list.
typedef struct
{
    unsigned short	npages;
    unsigned short	nfree;
    unsigned short	ncorpsepages;
    unsigned short	ncorpses;
    unsigned short	freehead;	// page + 1, oldest free page
    unsigned short	freetail;
    unsigned short	chead;		// oldest corpse page + 1
    unsigned short	ctail;
    unsigned short	zombies;	// object handles through link
} opool_t;

extern opool_t		o_pool;
#define o_zombies	o_pool.zombies

// A free page, -1 when none. wake: a wake-up or a cosmetic transient,
// which must leave the reserve.
int	O_AllocPage (int wake);
void	O_FreePage (int p);
void	O_SweepPages (void);
int	O_DropAlloc (void);
boolean	O_CorpseAdd (mobj_t* mo);
void	O_ForCorpses (void (*fn) (corpse_t* c, unsigned h, void* ctx), void* ctx);
corpse_t* O_Corpse (unsigned h);
void	O_Stats (void);


//
// chains.c: blockmap and sector chains.
//
extern unsigned short*	o_bhash;	// 128 heads
extern unsigned short*	o_shash;	// 64 heads
typedef boolean (*thingfn_t) (mobj_t*);
boolean	O_SectorThings (secidx_t sec, thingfn_t fn);
void	O_LinkChains (unsigned h, fixed_t x, fixed_t y, secidx_t sec, int flags);
void	O_UnlinkChains (unsigned h, fixed_t x, fixed_t y, secidx_t sec, int flags);


//
// things.c: tiers, views, promotion, thinkers.
//
extern byte*		o_tier;
extern byte*		o_sleep;
extern byte*		o_monbits;
extern unsigned short*	o_zclip;	// floor8, ceil8 per slot
extern drop_t*		o_drops;
extern int		o_ndrops;
extern int		o_r0;		// P_Random index at level load

// Elapsed thinker runs of a static thing: leveltime, plus one when the
// current thinker pass has gone past its map index.
extern int		o_cursor;	// segment 1 position, -1 before the pass

// The player's attacker of the previous level: vanilla keeps the
// dangling pointer, the hash reports it as gone. Never dereferenced.
extern byte		o_detached;
#define O_DETACHED	((mobj_t *) &o_detached)

int	O_Tier (int i);
void	O_SetTier (int i, int t);
mobjtype_t O_ThingType (const dmc_thing_t* t);

// Handle of a page or view, full object or view of a handle.
unsigned O_Hnd (mobj_t* mo);
mobj_t*	O_Get (unsigned h, mobj_t* view);
// Full object of a target handle, NULL when none or dangling.
mobj_t*	O_Target (unsigned h);
#define P_Target(mo)	O_Target((mo)->target)

// Two pointers to the same thing: a page, or views of one handle.
#define O_SAME(a, b) \
    ((a) == (b) || ((a)->flags2 & (b)->flags2 & MF2_VIEW && (a)->link == (b)->link))

// Views of map thing i (static or sleeping) and of a drop record, and
// the height clip of a view back into its record.
void	O_ViewThing (int i, mobj_t* v);
void	O_ViewDrop (int s, mobj_t* v);
void	O_ViewStore (mobj_t* v);

// Promotion before any write: full object of a page or view.
mobj_t*	O_Own (mobj_t* mo);
mobj_t*	O_Promote (mobj_t* view, int wake);
// Full object of promoted map thing i still at its NOR position.
mobj_t*	O_NorObject (int i);
void	O_Seg1Insert (mobj_t* mo);

void	O_RunThinkers (void);
void	O_LinkThinker (unsigned link);
void	O_Remove (mobj_t* mo);
mobj_t*	O_SpawnDrop (fixed_t x, fixed_t y, mobjtype_t type);
void	O_CheckCorpse (mobj_t* mo);
void	O_CrushCorpses (secidx_t sec);
boolean	O_EvictTransient (void);

// A live, non-candidate object targets h, or the player's attacker is
// h. Without such a pin and with dangle, other targets of h become
// H_DANGLE (h is about to go).
boolean	O_Referenced (unsigned h, boolean dangle);
boolean	O_RestingCorpse (const mobj_t* mo);

// The full objects of both segments, NULL starts and ends.
mobj_t*	O_NextObject (mobj_t* mo);
// Another live thing of the type of mo (A_BossDeath).
boolean	O_OtherAlive (mobj_t* mo);


//
// movers.c: mover and light slots.
//
extern light_t*	lights;
extern int	numlights;
int	P_MoversUsed (void);
void	P_FreeMover (int slot);
light_t* P_NewLight (int kind, secidx_t sec);


#if defined(DMC_STATS) || defined(DMC_HASHORDER)
// Every hashed object in thinker order (lockstep hash) and the creation
// order of segment 2 objects and drops it needs. Firmware builds with
// DMC_HASHORDER keep that order while o_seq_page and o_seq_drop point at
// memory for the plan's pages and drop slots (set before the first tic of
// the level), NULL keeps none.
typedef void (*objfn_t) (mobj_t* mo, unsigned h, void* ctx);
void	O_ForEach (objfn_t fn, void* ctx);
extern unsigned*	o_seq_page;
extern unsigned*	o_seq_drop;
// The last creation number given, a load renumbers them densely
// (port/game/g_save.c)
extern unsigned		o_seqnext;
#endif
#ifdef DMC_STATS
// Host statistics
int	O_LiveDrops (void);
int	O_AwakeMonsters (void);
void	O_StatsWrite (void);
void	O_TraceCorpses (int intercepts);
extern void*		o_statsfile;	// FILE* of --obj-stats
extern int		o_page_cap;	// --obj-pages
extern int		o_mover_cap;	// --obj-movers
extern int		o_reserve;	// --obj-reserve, PAGE_RESERVE by default
#define O_RESERVE	o_reserve
#else
#define O_RESERVE	PAGE_RESERVE
#endif

#endif
