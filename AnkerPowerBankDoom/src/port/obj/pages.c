// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The page pool: full objects and corpse pages
// share 60 B pages, the oldest corpse page is evicted when no page is
// free. A removed object that a live object still targets keeps its page
// as a zombie, as vanilla's freed zone block keeps its contents, until
// nothing refers to it. Drop record slots.

#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "p_local.h"

#include "things.h"

byte*			o_pages;
opool_t			o_pool;
int			o_arena_tail;	// resident: the firmware sets it (things.h)

#define npages		o_pool.npages
#define nfree		o_pool.nfree
#define freehead	o_pool.freehead
#define freetail	o_pool.freetail
#define chead		o_pool.chead
#define ctail		o_pool.ctail
#define ncorpsepages	o_pool.ncorpsepages
#define ncorpses	o_pool.ncorpses

ostats_t		o_stats;
#ifdef DMC_STATS
boolean			o_promote_all;
boolean			o_keep_corpses;

// Host: the tic scratch is a plain array. The device link script places
// dmc_tic in the frame region (p_local.h), so does the frame model of the
// host (mk/engine.mk).
#ifndef DMC_HOST_FRAME
ticscratch_t		dmc_tic;
#endif
#endif


#if defined(DMC_STATS) || defined(DMC_HASHORDER)
unsigned*		o_seq_page;
unsigned*		o_seq_drop;
#endif
#ifdef DMC_STATS
#include <stdio.h>
int			o_reserve = PAGE_RESERVE;
void*			o_statsfile;

//
// O_StatsWrite
// Host: one line per level with its plan, the counted events and the
// peaks (tests/host/ramcheck.py and the corpus runs read it).
//
void O_StatsWrite (void)
{
    ostats_t*	st = &o_stats;

    if (!o_statsfile || !st->episode)
	return;
    fprintf(o_statsfile, "level E%dM%d skill %d plan %c leveltime %d arena %d fixed %d pages %d"
	    " plan_pages %d need_play %d monsters %d drops %d movers %d lights %d zclip %d"
	    " ev_wake %d ev_transient %d ev_mover %d ev_evicted %d ev_overrun %d"
	    " peak_full %d peak_awake %d peak_corpses %d peak_drops %d peak_movers %d"
	    " peak_pages %d clip_promoted %d corpse_evicted %d zombies %d boss %d first_event %d"
	    " corpse_trace %d kills %d/%d\n",
	    st->episode, st->map, st->skill, 'A' + st->plan, leveltime,
	    st->arena, st->fixed, st->pages, M.plan.pages,
	    M.plan.need_play, M.plan.monsters, M.plan.drop_slots, M.plan.mover_slots,
	    M.plan.light_slots, M.plan.zclip_slots,
	    st->events[EV_WAKE_DEFERRED], st->events[EV_TRANSIENT_SKIPPED],
	    st->events[EV_MOVER_DEFERRED], st->events[EV_EVICTED], st->events[EV_OVERRUN],
	    st->peak_full, st->peak_awake, st->peak_corpses, st->peak_drops,
	    st->peak_movers, st->peak_pages, st->clip_promoted, st->corpse_evicted, st->zombies,
	    st->boss, st->first_event, st->corpse_trace, players[0].killcount, totalkills);
    fflush(o_statsfile);
}
#endif

//
// O_FreePage
// Free pages are used oldest first and keep x and y: a sound still
// playing from a freed object keeps its place (the sound code holds
// the pointer) until the page is reused.
//
void O_FreePage (int p)
{
    O_PAGE(p)->link = 0;
    if (freetail)
	O_PAGE(freetail - 1)->link = p + 1;
    else
	freehead = p + 1;
    freetail = p + 1;
    nfree++;
}

static int PopFree (void)
{
    int		p = freehead - 1;

    freehead = O_PAGE(p)->link;
    if (!freehead)
	freetail = 0;
    nfree--;
    memset(O_PAGE(p), 0, PAGE_BYTES);
    return p;
}

// Evicts the oldest corpse page, its corpses disappear.
static boolean EvictCorpses (void)
{
    corpsepage_t*	c;
    int			p;

    if (!chead)
	return false;
    p = chead - 1;
    c = (corpsepage_t *) O_PAGE(p);
    chead = c->next;
    if (!chead)
	ctail = 0;
    ncorpses -= c->count;
    ncorpsepages--;
    O_FreePage(p);
#ifdef DMC_STATS
    o_stats.corpse_evicted++;
#endif
    return true;
}

//
// O_AllocPage
// A free page, or -1. A wake-up or a cosmetic transient (wake) must
// leave the reserve: free and evictable pages above O_RESERVE.
//
int O_AllocPage (int wake)
{
    if (wake && !o_promote_all && nfree + ncorpsepages <= O_RESERVE)
    {
	O_SweepPages();
	if (nfree + ncorpsepages <= O_RESERVE)
	    return -1;
    }
    if (!nfree)
	O_SweepPages();
    if (!nfree && !EvictCorpses() && (wake || !O_EvictTransient()))
	return -1;
    return PopFree();
}

//
// O_CorpseAdd
// Keeps a corpse at rest as a cosmetic record. Uses the newest corpse
// page, a free page, or evicts the oldest corpse page. Without one the
// corpse stays a full object and its next tic tries again.
//
boolean O_CorpseAdd (mobj_t* mo)
{
    corpsepage_t*	c = NULL;
    corpse_t*		r;
    int			p;

    if (ctail)
	c = (corpsepage_t *) O_PAGE(ctail - 1);
    if (!c || c->count == CORPSES_PER_PAGE)
    {
	if (!nfree)
	    EvictCorpses();
	if (!nfree)
	    return false;
	p = PopFree();
	c = (corpsepage_t *) O_PAGE(p);
	c->flags2 = MF2_REMOVED;
	if (ctail)
	    ((corpsepage_t *) O_PAGE(ctail - 1))->next = p + 1;
	else
	    chead = p + 1;
	ctail = p + 1;
	ncorpsepages++;
    }
    r = &c->c[c->count++];
    r->x = mo->x >> FRACBITS;
    r->y = mo->y >> FRACBITS;
    r->statenum = mo->statenum;
    r->secinfo = mo->sector | (mo->flags & MF_SHADOW ? CORPSE_SHADOW : 0);
    secstat[mo->sector] |= SS_CORPSE;
    ncorpses++;
    return true;
}

// Corpse records in page order, oldest first: calls fn for each.
void O_ForCorpses (void (*fn) (corpse_t* c, unsigned h, void* ctx), void* ctx)
{
    corpsepage_t*	c;
    int			p;
    int			k;

    for (p = chead; p; p = c->next)
    {
	c = (corpsepage_t *) O_PAGE(p - 1);
	for (k = 0; k < c->count; k++)
	    fn(&c->c[k], H_CORPSE(p - 1, k), ctx);
    }
}

corpse_t* O_Corpse (unsigned h)
{
    corpsepage_t*	c;
    int			p = H_IDX(h) >> 3;

    if (p >= npages)
	return NULL;
    c = (corpsepage_t *) O_PAGE(p);
    return (h & 7) < c->count ? &c->c[h & 7] : NULL;
}

int O_DropAlloc (void)
{
    if (o_ndrops >= M.plan.drop_slots)
	return -1;
    return o_ndrops++;
}

//
// O_SweepPages
// Frees the zombies nothing refers to any more.
//
void O_SweepPages (void)
{
    unsigned short*	link = &o_zombies;
    mobj_t*		mo;
    unsigned		h;

    while ((h = *link) != 0)
    {
	mo = O_PAGE(H_IDX(h));
	if (O_Referenced(h, true))
	{
	    link = &mo->link;
	    continue;
	}
	*link = mo->link;
	O_FreePage(H_IDX(h));
    }
}

#ifdef DMC_STATS
//
// O_Stats
// Host: peaks of the level, called once per tic.
//
void O_Stats (void)
{
    ostats_t*	s = &o_stats;
    int		n;

    n = npages - nfree - ncorpsepages;
    if (n > s->peak_full)
	s->peak_full = n;
    if (npages - nfree > s->peak_pages)
	s->peak_pages = npages - nfree;
    if (ncorpses > s->peak_corpses)
	s->peak_corpses = ncorpses;
    n = O_LiveDrops();
    if (n > s->peak_drops)
	s->peak_drops = n;
    n = P_MoversUsed();
    if (n > s->peak_movers)
	s->peak_movers = n;
    n = O_AwakeMonsters();
    if (n > s->peak_awake)
	s->peak_awake = n;
}
#endif

