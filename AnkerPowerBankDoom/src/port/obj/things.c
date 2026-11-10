// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Map thing tiers, views and the thinker passes.

#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "m_bbox.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"

#include "things.h"

byte*		o_tier;
byte*		o_sleep;
byte*		o_monbits;
unsigned short*	o_zclip;
drop_t*		o_drops;
int		o_ndrops;
int		o_r0;
int		o_cursor = -1;

byte		o_detached;

// segment 1: promoted map things sorted by map index, segment 2
unsigned short		seg1;
unsigned short		seg2;
unsigned short		seg2tail;

#if defined(DMC_STATS) || defined(DMC_HASHORDER)
unsigned		o_seqnext;
#endif

extern int		prndindex;

// Types of dropped items, 2 bits in a drop record.
static const unsigned char droptypes[4] = { MT_CLIP, MT_SHOTGUN, MT_CHAINGUN, MT_CLIP };



//
// TIERS
//
int O_Tier (int i)
{
    return (o_tier[i >> 2] >> ((i & 3) << 1)) & 3;
}

void O_SetTier (int i, int t)
{
    int		s = (i & 3) << 1;

    o_tier[i >> 2] = (o_tier[i >> 2] & ~(3 << s)) | t << s;
}

static boolean IsMonster (int i)
{
    return (o_monbits[i >> 3] >> (i & 7)) & 1;
}

// Sleeping monster byte of map thing i: its ordinal among the monsters.
static const byte nibblebits[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };

static byte* SleepByte (int i)
{
    int		n = 0;
    int		k;
    int		b;

    for (k = 0; k <= i >> 3; k++)
    {
	b = o_monbits[k];
	if (k == i >> 3)
	    b &= (1 << (i & 7)) - 1;
	n += nibblebits[b & 15] + nibblebits[b >> 4];
    }
    return &o_sleep[n];
}

mobjtype_t O_ThingType (const dmc_thing_t* t)
{
    return t->type;
}

// rndtable entry idx, through P_Random
static int RndAt (int idx)
{
    int		save = prndindex;
    int		r;

    prndindex = (idx - 1) & 0xff;
    r = P_Random();
    prndindex = save;
    return r;
}

// Z-clip slot of map thing i, -1 when it has none.
static int ZClipSlot (int i)
{
    int		lo = 0;
    int		hi = map_u16(DMC_ST_ZCLIP, 0) - 1;
    int		mid;
    int		v;

    while (lo <= hi)
    {
	mid = (lo + hi) >> 1;
	v = map_u16(DMC_ST_ZCLIP, mid + 1);
	if (v == i)
	    return mid;
	if (v < i)
	    lo = mid + 1;
	else
	    hi = mid - 1;
    }
    return -1;
}


//
// VIEWS
//

// State and tics of a static thing after elapsed thinker runs: its spawn
// cycle has no actions (the converter checks), so they follow from the
// spawn phase alone.
static void StaticState (mobj_t* v, int k, int elapsed)
{
    int		s = MI_SPAWNSTATE(v->type);
    int		tics = ST_TICS(&dmc_states[s]);
    int		len;
    int		n;
    int		c;

    if (tics > 0)
    {
	tics = 1 + RndAt(k + 2) % tics;
	if (elapsed >= tics)
	{
	    elapsed -= tics;
	    s = dmc_states[s].nextstate;

	    // whole turns of a cycle back to this state
	    for (len = 0, n = 0, c = s; n < 16 && ST_TICS(&dmc_states[c]) > 0; n++)
	    {
		len += ST_TICS(&dmc_states[c]);
		c = dmc_states[c].nextstate;
		if (c == s)
		{
		    elapsed %= len;
		    break;
		}
	    }
	    for (;;)
	    {
		tics = ST_TICS(&dmc_states[s]);
		if (tics < 0 || elapsed < tics)
		    break;
		elapsed -= tics;
		s = dmc_states[s].nextstate;
	    }
	}
    }
    v->statenum = s;
    v->tics = tics < 0 ? -1 : tics - elapsed;
}

//
// O_ViewThing
// The full object vanilla would have for map thing i, static or
// sleeping.
//
void O_ViewThing (int i, mobj_t* v)
{
    dmc_thing_t		t;
    dmc_sector_t	r;
    int			z;
    int			b;
    int			s;
    int			k;

    map_thing(i, &t);
    memset(v, 0, sizeof(*v));
    v->type = O_ThingType(&t);
    v->x = t.x << FRACBITS;
    v->y = t.y << FRACBITS;
    v->angle = ANG45 * t.angle45;
    v->flags = MI_FLAGS(v->type);
    if (t.options & DMC_OPT_AMBUSH)
	v->flags |= MF_AMBUSH;
    v->health = MI_SPAWNHEALTH(v->type);
    v->reactiontime = MI_ACTOR(v->type)->reactiontime;
    v->sector = t.sector;
    v->flags2 = MF2_VIEW | (i >> 8) << MF2_IDXSHIFT;
    v->spawnlo = i;
    v->link = H_MAP(i);

    // spawn heights of its sector, or those of the last height clip
    z = ZClipSlot(i);
    if (z >= 0)
    {
	v->floor8 = o_zclip[2 * z];
	v->ceil8 = o_zclip[2 * z + 1];
    }
    else
    {
	map_sector(t.sector, &r);
	v->floor8 = r.floor << 3;
	v->ceil8 = r.ceiling << 3;
    }
    v->z = MI_FLAGS(v->type) & MF_SPAWNCEILING ? MO_CEILZ(v) - MI_HEIGHT(v->type) : MO_FLOORZ(v);

    if (O_Tier(i) == T_SLEEP)
    {
	b = *SleepByte(i);
	s = MI_SPAWNSTATE(v->type);
	if (b & SL_STAND2)
	    s = dmc_states[s].nextstate;
	v->statenum = s;
	v->tics = b & SL_TICS;
	v->lastlook = (b >> SL_LOOKSHIFT) & 3;
	if (b & SL_TARGET)
	    v->target = O_Hnd(players[0].mo);
	return;
    }
    k = o_r0 + t.rngk[DMC_SKILL_GROUP(gameskill)];
    v->lastlook = RndAt(k + 1) % MAXPLAYERS;
    StaticState(v, k, leveltime + (i < o_cursor));
}

void O_ViewDrop (int s, mobj_t* v)
{
    drop_t*	d = &o_drops[s];

    memset(v, 0, sizeof(*v));
    v->type = droptypes[DROP_TYPE(d)];
    v->x = d->x;
    v->y = d->y;
    v->z = (fixed_t)d->z8 << 13;
    v->floor8 = d->z8;
    v->ceil8 = P_CeilingHeight(DROP_SEC(d)) >> 13;
    v->flags = MI_FLAGS(v->type) | MF_DROPPED;
    v->health = MI_SPAWNHEALTH(v->type);
    v->statenum = MI_SPAWNSTATE(v->type);
    v->tics = ST_TICS(&dmc_states[v->statenum]);
    v->reactiontime = MI_ACTOR(v->type)->reactiontime;
    v->lastlook = DROP_LOOK(d);
    v->sector = DROP_SEC(d);
    v->flags2 = MF2_VIEW | NOT_MAPTHING >> 8 << MF2_IDXSHIFT;
    v->spawnlo = NOT_MAPTHING & 0xff;
    v->link = H_DROP(s);
}

//
// O_ViewStore
// Keeps the height clip of a view (P_ThingHeightClip): the z-clip slot
// of a map thing, the z of a drop. A thing that has no slot or hangs
// from the ceiling is promoted instead.
//
void O_ViewStore (mobj_t* v)
{
    unsigned	h = v->link;
    mobj_t*	mo;
    mobj_t	w;
    int		i;
    int		z;

    if (H_KIND(h) == HK_DROP)
    {
	o_drops[H_IDX(h)].z8 = TO8(v->z);
	return;
    }
    i = H_IDX(h) - 1;
    if (O_Tier(i) == T_PROMOTED)
    {
	// promoted since the view was made: the clip goes to the object
	mo = O_NorObject(i);
	mo->z = v->z;
	mo->floor8 = v->floor8;
	mo->ceil8 = v->ceil8;
	return;
    }
    z = ZClipSlot(i);
    if (z >= 0 && !(v->flags & MF_SPAWNCEILING) && v->z == MO_FLOORZ(v))
    {
	o_zclip[2 * z] = v->floor8;
	o_zclip[2 * z + 1] = v->ceil8;
	return;
    }
    // unchanged: nothing to keep
    O_ViewThing(i, &w);
    if (w.z == v->z && w.floor8 == v->floor8 && w.ceil8 == v->ceil8)
	return;
#ifdef DMC_STATS
    o_stats.clip_promoted++;
#endif
    if (O_Promote(v, 0) == NULL)
	O_EVENT(EV_OVERRUN);
}


//
// HANDLES
//
unsigned O_Hnd (mobj_t* mo)
{
    if (!mo)
	return H_NONE;
    if (mo->flags2 & MF2_VIEW)
	return mo->link;
    return H_OBJ(O_PAGEIDX(mo));
}

mobj_t* O_Target (unsigned h)
{
    if (h == H_NONE || h == H_DANGLE)
	return NULL;
    return O_PAGE(H_IDX(h));
}

mobj_t* O_Get (unsigned h, mobj_t* view)
{
    int		i;
    int		t;

    switch (H_KIND(h))
    {
      case HK_OBJ:
	return O_PAGE(H_IDX(h));
      case HK_MAP:
	if (h == H_NONE)
	    return NULL;
	i = H_IDX(h) - 1;
	t = O_Tier(i);
	// absent: a promoted thing that has moved since, or gone
	if (t == T_PROMOTED || t == T_ABSENT)
	    return O_NorObject(i);
	O_ViewThing(i, view);
	return view;
      case HK_DROP:
	if (o_drops[H_IDX(h)].secinfo == DROP_DEAD)
	    return NULL;
	O_ViewDrop(H_IDX(h), view);
	return view;
    }
    return NULL;
}

mobj_t* O_NorObject (int i)
{
    unsigned	h;
    mobj_t*	mo;

    for (h = seg1; h; h = mo->link)
    {
	mo = O_PAGE(H_IDX(h));
	if (MO_SPAWNIDX(mo) == i)
	    return mo->flags2 & MF2_REMOVED ? NULL : mo;
    }
    return NULL;
}

// Inserts a promoted map thing into segment 1 by map index.
void O_Seg1Insert (mobj_t* mo)
{
    unsigned short*	l = &seg1;
    int			i = MO_SPAWNIDX(mo);

    while (*l && MO_SPAWNIDX(O_PAGE(H_IDX(*l))) < i)
	l = &O_PAGE(H_IDX(*l))->link;
    mo->link = *l;
    *l = O_Hnd(mo);
}

//
// O_Promote
// A full object for the view of a static or sleeping map thing, at the
// view's NOR list position. wake: a wake-up, which must leave the
// reserve pages.
//
mobj_t* O_Promote (mobj_t* v, int wake)
{
    int		i = H_IDX(v->link) - 1;
    int		p;
    mobj_t*	mo;

#ifdef DMC_STATS
    if (H_KIND(v->link) != HK_MAP || O_Tier(i) == T_ABSENT)
	I_Error("O_Promote: %x", v->link);
#endif
    if (O_Tier(i) == T_PROMOTED)
	return O_NorObject(i);
    p = O_AllocPage(wake);
    if (p < 0)
	return NULL;
    mo = O_PAGE(p);
    *mo = *v;
    mo->flags2 = (mo->flags2 & ~MF2_VIEW) | MF2_NORPOS;
    mo->snext = mo->bnext = 0;
    O_Seg1Insert(mo);
    O_SetTier(i, T_PROMOTED);
    return mo;
}

//
// O_Own
// The full object to write to: the page itself, or the promoted view.
//
mobj_t* O_Own (mobj_t* mo)
{
    mobj_t*	p;

    if (!mo || !(mo->flags2 & MF2_VIEW))
	return mo;
    p = O_Promote(mo, 0);
    if (!p)
	O_EVENT(EV_OVERRUN);
    return p;
}

// Removes a view: a static picked up or a drop crunched or picked up.
static void RemoveView (mobj_t* v)
{
    unsigned	h = v->link;
    drop_t*	d;

    if (H_KIND(h) == HK_DROP)
    {
	d = &o_drops[H_IDX(h)];
	O_UnlinkChains(h, d->x, d->y, DROP_SEC(d), v->flags);
	d->secinfo = DROP_DEAD;
	return;
    }
#ifdef DMC_STATS
    if (O_Tier(H_IDX(h) - 1) != T_STATIC)
	I_Error("RemoveView: %x", h);
#endif
    O_SetTier(H_IDX(h) - 1, T_ABSENT);
}

//
// O_Remove
// P_RemoveMobj: a full object leaves its chains and is freed when the
// thinker pass reaches it.
//
void O_Remove (mobj_t* mo)
{
    if (mo->flags2 & MF2_VIEW)
    {
	RemoveView(mo);
	return;
    }
    P_UnsetThingPosition(mo);
    S_StopSound(mo);
    mo->flags2 |= MF2_REMOVED;
}

// Link field of a segment 2 entry.
static unsigned short* LinkOf (unsigned l)
{
    if ((l & 0xc000) == L_OBJ)
	return &O_PAGE(l & 0x3fff)->link;
    if ((l & 0xc000) == L_MOVER)
	return &movers[l & 0x3fff].door.link;
    return &lights[l & 0x3fff].link;
}

//
// O_NextObject
// The full objects of both segments in thinker order, removed ones
// included, NULL starts and ends.
//
mobj_t* O_NextObject (mobj_t* mo)
{
    unsigned	l;

    if (!mo)
	l = seg1 ? seg1 : seg2;
    else
    {
	l = mo->link;
	if (!l && MO_SPAWNIDX(mo) != NOT_MAPTHING)
	    l = seg2;
    }
    while (l && (l & 0xc000) != L_OBJ)
	l = *LinkOf(l);
    return l ? O_PAGE(l & 0x3fff) : NULL;
}

//
// O_EvictTransient
// Last resort when the reserve pages are gone: the oldest puff, blood
// or fog leaves the game at once (a counted event).
//
boolean O_EvictTransient (void)
{
    unsigned short*	l = &seg2;
    unsigned		last = 0;
    unsigned		h;
    mobj_t*		mo;

    for (; (h = *l) != 0; l = LinkOf(h))
    {
	mo = O_PAGE(h & 0x3fff);
	if ((h & 0xc000) == L_OBJ && !(mo->flags2 & MF2_REMOVED)
	    && (mo->type == MT_PUFF || mo->type == MT_BLOOD || mo->type == MT_TFOG))
	{
	    O_Remove(mo);
	    *l = mo->link;
	    if (seg2tail == h)
		seg2tail = last;
	    O_FreePage(h & 0x3fff);
	    O_EVENT(EV_EVICTED);
	    return true;
	}
	last = h;
    }
    return false;
}

// The object is gone for good: a zombie while live objects target it.
// Only shootable things (the player, monsters, lost souls) hurt others,
// so only they can be a target or the player's attacker.
static void Release (unsigned h)
{
    mobj_t*	mo = O_PAGE(H_IDX(h));

    if (MI_FLAGS(mo->type) & MF_SHOOTABLE && O_Referenced(h, true))
    {
	mo->link = o_zombies;
	o_zombies = h;
#ifdef DMC_STATS
	o_stats.zombies++;
#endif
	return;
    }
    O_FreePage(H_IDX(h));
}

boolean O_RestingCorpse (const mobj_t* mo)
{
    return (mo->flags & MF_CORPSE) && !(mo->flags2 & MF2_PLAYER) && mo->tics == -1
	&& !mo->momx && !mo->momy && !mo->momz && mo->z == MO_FLOORZ(mo);
}

boolean O_Referenced (unsigned h, boolean dangle)
{
    mobj_t*	mo;
    unsigned	l;

    if (players[0].attacker == O_PAGE(H_IDX(h)))
	return true;
    for (mo = O_NextObject(NULL); mo; mo = O_NextObject(mo))
	if (mo->target == h && !(mo->flags2 & MF2_REMOVED) && !O_RestingCorpse(mo))
	    return true;
    if (dangle)
    {
	for (mo = O_NextObject(NULL); mo; mo = O_NextObject(mo))
	    if (mo->target == h)
		mo->target = H_DANGLE;
	for (l = o_zombies; l; l = mo->link)
	{
	    mo = O_PAGE(H_IDX(l));
	    if (mo->target == h)
		mo->target = H_DANGLE;
	}
    }
    return false;
}


//
// THINKERS
//
void O_LinkThinker (unsigned link)
{
    *(seg2tail ? LinkOf(seg2tail) : &seg2) = link;
    seg2tail = link;
#if defined(DMC_STATS) || defined(DMC_HASHORDER)
    if ((link & 0xc000) == L_OBJ && o_seq_page)
	o_seq_page[link & 0x3fff] = ++o_seqnext;
#endif
}

#if defined(DMC_STATS) || defined(DMC_HASHORDER)
static void O_SeqDrop (int s)
{
    if (o_seq_drop)
	o_seq_drop[s] = ++o_seqnext;
}
#endif

//
// SleepTick
// P_MobjThinker of a sleeping monster: count down, switch stand state
// and run A_Look on a view. A wake-up promotes it.
//
static void SleepTick (int i, byte* b)
{
    mobj_t	v;
    mobj_t*	mo;
    int		s;

    if (--*b & SL_TICS)
	return;
    *b ^= SL_STAND2;
    O_ViewThing(i, &v);
    s = (*b & SL_STAND2) ? dmc_states[MI_SPAWNSTATE(v.type)].nextstate
	: MI_SPAWNSTATE(v.type);
    *b |= ST_TICS(&dmc_states[s]);
    v.statenum = s;
    v.tics = ST_TICS(&dmc_states[s]);
    if (P_LookSee(&v))
    {
	mo = O_Promote(&v, 1);
	if (mo)
	{
	    P_SeeYou(mo);
	    return;
	}
	O_EVENT(EV_WAKE_DEFERRED);
    }
    *b = (*b & (SL_TICS | SL_STAND2)) | v.lastlook << SL_LOOKSHIFT
	| (v.target ? SL_TARGET : 0);
}

typedef void (*thinkfn_t) (void*);

static const thinkfn_t moverthink[] =
{
    NULL, (thinkfn_t) T_VerticalDoor, (thinkfn_t) T_MoveFloor,
    (thinkfn_t) T_PlatRaise, (thinkfn_t) T_MoveCeiling
};

static const thinkfn_t lightthink[] =
{
    (thinkfn_t) T_LightFlash, (thinkfn_t) T_StrobeFlash,
    (thinkfn_t) T_Glow, (thinkfn_t) T_FireFlicker
};

static void RunObject (unsigned short** l)
{
    unsigned	h = **l;
    mobj_t*	mo = O_PAGE(H_IDX(h));

    if (mo->flags2 & MF2_REMOVED)
    {
	**l = mo->link;
	Release(h);
	return;
    }
    o_cursor = MO_SPAWNIDX(mo);
    P_MobjThinker(mo);
    *l = &mo->link;
}

//
// O_RunThinkers
// Segment 1 merges the promoted list with the sleeping monsters by map
// index, statics need no pass. Segment 2 runs in creation order.
// Removed entries are unlinked when the pass reaches them, newly
// created ones run in the same pass.
//
void O_RunThinkers (void)
{
    unsigned short*	l = &seg1;
    unsigned short*	prev;
    unsigned		h;
    unsigned		last;
    unsigned		k;
    int			i;
    int			ord = 0;
    int			w;

    // segment 1
    for (w = 0; w < (M.nthings + 7) >> 3; w++)
    {
	if (!o_monbits[w])
	    continue;
	for (i = w << 3; i < (w << 3) + 8; i++)
	{
	    if (!IsMonster(i))
		continue;
	    while (*l && MO_SPAWNIDX(O_PAGE(H_IDX(*l))) < i)
		RunObject(&l);
	    if (O_Tier(i) == T_SLEEP)
	    {
		o_cursor = i;
		SleepTick(i, &o_sleep[ord]);
		// a wake-up already had its turn
		while (*l && MO_SPAWNIDX(O_PAGE(H_IDX(*l))) == i)
		    l = &O_PAGE(H_IDX(*l))->link;
	    }
	    ord++;
	}
    }
    while (*l)
	RunObject(&l);

    // segment 2
    o_cursor = 0x7fffffff;
    prev = &seg2;
    last = 0;
    while ((h = *prev) != 0)
    {
	k = h & 0xc000;
	i = h & 0x3fff;
	if (k == L_OBJ ? O_PAGE(i)->flags2 & MF2_REMOVED
	    : k == L_MOVER && movers[i].door.kind & MV_REMOVED)
	{
	    *prev = *LinkOf(h);
	    if (seg2tail == h)
		seg2tail = last;
	    if (k == L_OBJ)
		Release(H_OBJ(i));
	    else
		P_FreeMover(i);
	    continue;
	}
	if (k == L_OBJ)
	    P_MobjThinker(O_PAGE(i));
	else if (k == L_MOVER)
	    moverthink[movers[i].door.kind](&movers[i]);
	else
	    lightthink[lights[i].kind & LT_TYPE](&lights[i]);
	prev = LinkOf(h);
	last = h;
    }
}

//
// DROPS
//
mobj_t* O_SpawnDrop (fixed_t x, fixed_t y, mobjtype_t type)
{
    drop_t*	d;
    mobj_t*	mo;
    int		s;
    int		k;
    secidx_t	sec;

    for (k = 0; k < 3 && droptypes[k] != type; k++)
	;
    s = o_promote_all ? -1 : O_DropAlloc();
    if (s < 0)
    {
	// exact bound: one drop per dropper of the skill
	if (!o_promote_all)
	    O_EVENT(EV_OVERRUN);
	mo = P_SpawnMobj(x, y, ONFLOORZ, type);
	if (mo)
	    mo->flags |= MF_DROPPED;
	return mo;
    }
    // P_SpawnMobj draws lastlook, the item rests on its sector floor
    d = &o_drops[s];
    sec = R_PointInSector(x, y);
    d->x = x;
    d->y = y;
    d->z8 = TO8(P_FloorHeight(sec));
    d->secinfo = sec | k << 12 | (P_Random() % MAXPLAYERS) << 14;
    O_LinkChains(H_DROP(s), x, y, sec, MI_FLAGS(type));
#if defined(DMC_STATS) || defined(DMC_HASHORDER)
    O_SeqDrop(s);
#endif
    return NULL;
}

#ifdef DMC_STATS
int O_LiveDrops (void)
{
    int		n = 0;
    int		s;

    for (s = 0; s < o_ndrops; s++)
	n += o_drops[s].secinfo != DROP_DEAD;
    return n;
}

int O_AwakeMonsters (void)
{
    mobj_t*	mo;
    int		n = 0;

    for (mo = O_NextObject(NULL); mo; mo = O_NextObject(mo))
	n += !(mo->flags2 & MF2_REMOVED) && (mo->flags & MF_COUNTKILL) && mo->health > 0;
    return n;
}
#endif


//
// CORPSES
//

//
// O_CheckCorpse
// A corpse at rest that no live object targets becomes a cosmetic
// record (masked in the lockstep hash), its page is freed by the pass.
//
void O_CheckCorpse (mobj_t* mo)
{
    unsigned	h;

    if (o_promote_all || o_keep_corpses || !O_RestingCorpse(mo) || (mo->flags2 & MF2_REMOVED))
	return;
    h = O_Hnd(mo);
    if (O_Referenced(h, false) || !O_CorpseAdd(mo))
	return;
    P_UnsetThingPosition(mo);
    mo->flags2 |= MF2_REMOVED;
}

// A corpse record of the moved sector that no longer fits it turns to
// gibs. Corpse heights are 14 or 16 units, the record keeps no type: 14
// is used. The heights of the other sectors did not change.
static void Crush (corpse_t* c, unsigned h, void* sec)
{
    (void)h;
    if (CORPSE_SEC(c) == *(secidx_t *) sec
	&& P_CeilingHeight(CORPSE_SEC(c)) - P_FloorHeight(CORPSE_SEC(c)) < 14 * FRACUNIT)
	c->statenum = S_GIBS;
}

//
// O_CrushCorpses
// P_ChangeSector for the corpse records: one that no longer fits its
// sector turns to gibs, as vanilla's PIT_ChangeSector does. Only sectors
// with corpse records (SS_CORPSE) are scanned.
//
void O_CrushCorpses (secidx_t sec)
{
    if (secstat[sec] & SS_CORPSE)
	O_ForCorpses(Crush, &sec);
}


//
// O_OtherAlive
// Another live thing of the type of mo (A_BossDeath): a full object, or
// any static or sleeping map thing.
//
boolean O_OtherAlive (mobj_t* mo)
{
    dmc_thing_t	t;
    mobj_t*	o;
    int		i;

    for (o = O_NextObject(NULL); o; o = O_NextObject(o))
	if (o != mo && !(o->flags2 & MF2_REMOVED) && o->type == mo->type && o->health > 0)
	    return true;
    for (i = 0; i < M.nthings; i++)
    {
	if (O_Tier(i) != T_STATIC && O_Tier(i) != T_SLEEP)
	    continue;
	map_thing(i, &t);
	if (O_ThingType(&t) == mo->type)
	    return true;
    }
    return false;
}


#ifdef DMC_STATS
//
// O_Z8
// Host: heights are kept in 1/8 units, a value that
// is not would be truncated.
//
short O_Z8 (fixed_t v)
{
    if (v & 0x1fff || (v >> 13) != (short) (v >> 13))
	I_Error("O_Z8: height %i/65536 is not in 1/8 units", v);
    return v >> 13;
}


//
// O_TraceCorpses
// Host: vanilla keeps corpses at rest in the blockmap, so a trace that
// adds things also crosses the ones here kept as records. When those
// would have taken the trace past 128 intercepts, vanilla's overrun
// emulation would have run and the engine's not: counted (corpse_trace).
// The largest corpse radius (30) and no cell test make it a cautious
// count.
//
typedef struct
{
    divline_t*	tr;
    int		n;
} tracectx_t;

static void TraceCorpse (corpse_t* c, unsigned h, void* ctx)
{
    tracectx_t*	t = ctx;
    fixed_t	r = 30 * FRACUNIT;
    fixed_t	x = c->x << FRACBITS;
    fixed_t	y = c->y << FRACBITS;
    int		pos = (t->tr->dx ^ t->tr->dy) > 0;
    fixed_t	x1, x2, y1, y2;
    divline_t	dl;

    (void)h;
    dl.x = x - r;
    dl.y = pos ? y + r : y - r;
    dl.dx = 2 * r;
    dl.dy = pos ? -2 * r : 2 * r;
    if (P_PointOnDivlineSide(dl.x, dl.y, t->tr) == P_PointOnDivlineSide(dl.x + dl.dx, dl.y + dl.dy, t->tr))
	return;
    if (P_InterceptVector(t->tr, &dl) < 0)
	return;
    // within the trace's box
    x1 = t->tr->x;
    x2 = t->tr->x + t->tr->dx;
    y1 = t->tr->y;
    y2 = t->tr->y + t->tr->dy;
    if (x + r < (x1 < x2 ? x1 : x2) || x - r > (x1 < x2 ? x2 : x1)
	|| y + r < (y1 < y2 ? y1 : y2) || y - r > (y1 < y2 ? y2 : y1))
	return;
    t->n++;
}

void O_TraceCorpses (int intercepts)
{
    tracectx_t	t = { &trace, 0 };

    if (intercepts > MAXINTERCEPTS_ORIGINAL)
	return;
    O_ForCorpses(TraceCorpse, &t);
    if (intercepts + t.n > MAXINTERCEPTS_ORIGINAL)
	o_stats.corpse_trace++;
}


#endif


#if defined(DMC_STATS) || defined(DMC_HASHORDER)
//
// ITERATION IN THINKER ORDER
// Every hashed object as vanilla's thinker list has it (host, and the
// firmware's verification pass while it keeps the creation order).
//
void O_ForEach (objfn_t fn, void* ctx)
{
    unsigned	l = seg1;
    mobj_t*	mo;
    mobj_t	v;
    int		i;
    int		t;
    int		d = 0;

    for (i = 0; i < M.nthings; i++)
    {
	while (l && MO_SPAWNIDX(O_PAGE(H_IDX(l))) < i)
	{
	    mo = O_PAGE(H_IDX(l));
	    if (!(mo->flags2 & MF2_REMOVED))
		fn(mo, l, ctx);
	    l = mo->link;
	}
	t = O_Tier(i);
	if (t == T_STATIC || t == T_SLEEP)
	{
	    O_ViewThing(i, &v);
	    fn(&v, H_MAP(i), ctx);
	}
    }
    for (; l; l = mo->link)
    {
	mo = O_PAGE(H_IDX(l));
	if (!(mo->flags2 & MF2_REMOVED))
	    fn(mo, l, ctx);
    }

    // segment 2 objects, drops by creation order
    for (l = seg2; l; )
    {
	if ((l & 0xc000) == L_OBJ)
	{
	    mo = O_PAGE(l & 0x3fff);
#if defined(DMC_STATS) || defined(DMC_HASHORDER)
	    for (; d < o_ndrops && o_seq_drop[d] < o_seq_page[l & 0x3fff]; d++)
		if (o_drops[d].secinfo != DROP_DEAD)
		{
		    O_ViewDrop(d, &v);
		    fn(&v, H_DROP(d), ctx);
		}
#endif
	    if (!(mo->flags2 & MF2_REMOVED))
		fn(mo, H_OBJ(l & 0x3fff), ctx);
	    l = mo->link;
	}
	else if ((l & 0xc000) == L_MOVER)
	    l = movers[l & 0x3fff].door.link;
	else
	    l = lights[l & 0x3fff].link;
    }
    for (; d < o_ndrops; d++)
	if (o_drops[d].secinfo != DROP_DEAD)
	{
	    O_ViewDrop(d, &v);
	    fn(&v, H_DROP(d), ctx);
	}
}

#endif
