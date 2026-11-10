// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Level start (LEVEL overlay code): the level arena of
// the map's plan and the map things in spawn order.

#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "m_random.h"
#include "p_local.h"

#include "things.h"

#ifdef DMC_HOST_FRAME
#include "fe.h"
#include "ovl.h"
extern byte		frame_mem[];
#endif

// The level arena ends at the end of SRAM and starts where the plan's
// arena bytes do, after the frame region and the
// caches of the plan. The host has an array with room for the promote
// everything mode.
#ifdef DMC_STATS
#define ARENA_MAX	O_ARENA_MAX
int			o_arena_words[O_ARENA_MAX / 4];
int			o_page_cap;
int			o_mover_cap;
#else
#define ARENA_MAX	14800	// plan D
#endif
#define ARENA_END	O_ARENA_END


extern int		prndindex;
extern unsigned short	seg1;
extern unsigned short	seg2;
extern unsigned short	seg2tail;
extern unsigned		o_moverused;

// A map thing spawned as a full object at its NOR position.
void O_AddSeg1 (mobj_t* mo, int i)
{
    mo->flags2 = (mo->flags2 & ~(3 << MF2_IDXSHIFT)) | (i >> 8) << MF2_IDXSHIFT | MF2_NORPOS;
    mo->spawnlo = i;
    O_Seg1Insert(mo);
    O_SetTier(i, T_PROMOTED);
}

void P_ClearMovers (void)
{
    o_moverused = 0;
    numlights = 0;
}

void O_ResetThinkers (void)
{
    seg1 = seg2 = seg2tail = 0;
    o_cursor = -1;
}

//
// O_PagesInit
// Every page free, lowest first, no corpses, no zombies.
//
void O_PagesInit (byte* base, int n)
{
    int		i;

#ifdef DMC_HOST_FRAME
    // --verify-model (dmc_render 0): under the tic scratch, as the meter's
    // verification pass keeps it (port/game/dmc_demo.c)
    if (!dmc_render)
    {
	i = n + M.plan.drop_slots + 1;
	o_seq_page = (unsigned *) ((byte *) &dmc_tic - 4 * i);
	if ((byte *) o_seq_page < frame_mem + OVL_WIN_TIC)
	    I_Error ("hash order: %d slots", i);
	memset (o_seq_page, 0, 4 * i);
	o_seq_drop = o_seq_page + n;
    }
    else
#endif
#ifdef DMC_STATS
    {
	o_seq_page = realloc(o_seq_page, sizeof(unsigned) * n);
	o_seq_drop = realloc(o_seq_drop, sizeof(unsigned) * (M.plan.drop_slots + 1));
    }
#endif
    o_pages = base;
    memset(&o_pool, 0, sizeof(o_pool));
    o_pool.npages = n;
    o_pool.freehead = n ? 1 : 0;
    o_pool.freetail = n;
    for (i = 0; i < n; i++)
    {
	O_PAGE(i)->link = i + 1 < n ? i + 2 : 0;
	O_PAGE(i)->flags2 = MF2_REMOVED;
    }
    o_pool.nfree = n;
    // The host counts the events of each level, the firmware those of a
    // demo pass, which clears them when it starts (port/game/dmc_demo.c).
#ifdef DMC_STATS
    memset(&o_stats, 0, sizeof(o_stats));
    o_stats.episode = gameepisode;
    o_stats.map = gamemap;
    o_stats.skill = gameskill + 1;
    o_stats.plan = M.plan.plan;
    o_stats.pages = n;
    o_stats.first_event = -1;
#endif
}

// size bytes at cursor *p of the 4 B aligned arena, aligned to align
static byte* Take (byte** p, int size, int align)
{
    byte*	r;

    r = (byte *) (((uintptr_t) *p + align - 1) & ~(uintptr_t) (align - 1));
    *p = r + size;
    return r;
}

//
// O_ArenaInit
// Carves the level-fixed part of the arena for the loaded map and skill
// group, the rest becomes pages.
//
int O_ArenaInit (void)
{
    dmc_plan_t*	pl = &M.plan;
    byte*	arena;
    byte*	p;
    int		size;
    int		fixed;
    int		npages;

    size = o_promote_all ? ARENA_MAX : pl->arena_bytes;
    if (size > ARENA_MAX || size & 3)
	I_Error("O_ArenaInit: arena of %i bytes", size);
    // records keep 12 sector bits, objects 10 map thing bits
    if (M.nsectors >= 4096 || M.nthings >= NOT_MAPTHING)
	I_Error("O_ArenaInit: %i sectors, %i things", M.nsectors, M.nthings);
    arena = ARENA_END - size;
    // not the tail, where the firmware keeps data across level changes
    memset(arena, 0, size - o_arena_tail);
    p = arena;

    // 4 B aligned first, then 2 B, then bytes
    o_drops = (drop_t *) Take(&p, sizeof(drop_t) * pl->drop_slots, 4);
    movers = (mover_t *) Take(&p, MOVERBYTES * pl->mover_slots, 2);
    lights = (light_t *) Take(&p, sizeof(light_t) * pl->light_slots, 2);
    o_zclip = (unsigned short *) Take(&p, 4 * pl->zclip_slots, 2);
    o_bhash = (unsigned short *) Take(&p, 2 * 128, 2);
    o_shash = (unsigned short *) Take(&p, 2 * 64, 2);
    secovl = (secovl_t *) Take(&p, sizeof(secovl_t) * M.nmutable, 2);
    buttonlist = (button_t *) Take(&p, sizeof(button_t) * MAXBUTTONS, 2);
    secstat = Take(&p, M.nsectors, 1);
    linevalid = Take(&p, (M.nlines + 7) >> 3, 1);
    linemapped = Take(&p, (M.nlines + 7) >> 3, 1);
    specialcleared = Take(&p, (M.nspeclines + 7) >> 3, 1);
    switchflip = Take(&p, 16, 1);
    o_tier = Take(&p, (M.nthings + 3) >> 2, 1);
    o_sleep = Take(&p, pl->monsters, 1);
    o_monbits = Take(&p, (M.nthings + 7) >> 3, 1);
    activeplats = Take(&p, MAXPLATS, 1);
    activeceilings = Take(&p, MAXCEILINGS, 1);
    if (3 * M.nswitches > 8 * 16)
	I_Error("O_ArenaInit: %i switch sides", M.nswitches);
    fixed = p - arena;

    // pages fill the rest from a 4 B boundary
    o_pages = Take(&p, 0, 4);
    npages = (arena + size - o_arena_tail - o_pages) / PAGE_BYTES;
#ifdef DMC_STATS
    // host stress runs: fewer pages and mover slots than the plan
    if (o_page_cap && npages > o_page_cap)
	npages = o_page_cap;
    if (o_mover_cap && pl->mover_slots > o_mover_cap)
	pl->mover_slots = o_mover_cap;
#endif
    if (npages < 1 + PAGE_RESERVE || npages > 0x3fff)
	I_Error("O_ArenaInit: %i pages", npages);

    O_PagesInit(o_pages, npages);
#ifdef DMC_STATS
    o_stats.arena = size;
    o_stats.fixed = fixed;
#endif
    o_ndrops = 0;
    memset(activeplats, NOMOVER, MAXPLATS);
    memset(activeceilings, NOMOVER, MAXCEILINGS);
    return fixed;
}

//
// LEVEL START
//
static mobj_t* SpawnFull (int i, const dmc_thing_t* t, mobjtype_t type)
{
    mobj_t*	mo;
    fixed_t	z;

    z = (MI_FLAGS(type) & MF_SPAWNCEILING) ? ONCEILINGZ : ONFLOORZ;
    mo = P_SpawnMapMobj(t->x << FRACBITS, t->y << FRACBITS, z, type, t->sector);
    O_AddSeg1(mo, i);
    return mo;
}

//
// O_SpawnLevel
// P_LoadThings: every map thing of the skill in map order, with the
// P_Random calls of P_SpawnMapThing. Monsters sleep, items and
// decorations stay static, the player and anything that would move at
// once are full objects.
//
void O_SpawnLevel (void)
{
    dmc_thing_t	t;
    mobjtype_t	type;
    int		flags;
    int		look;
    mobj_t*	mo;
    int		g = DMC_SKILL_GROUP(gameskill);
    int		ord = 0;
    int		lastlook;
    int		tics;
    int		i;
    int		k;
    int		n;

    // the spawn phases of the image count every monster of the skill
    if (nomonsters)
	I_Error("-nomonsters is not supported");
    O_ResetThinkers();
    o_r0 = prndindex;

    // z-clip slots start at the spawn heights of their sector
    n = map_u16(DMC_ST_ZCLIP, 0);
    for (k = 0; k < n; k++)
    {
	map_thing(map_u16(DMC_ST_ZCLIP, k + 1), &t);
	o_zclip[2 * k] = P_FloorHeight(t.sector) >> 13;
	o_zclip[2 * k + 1] = P_CeilingHeight(t.sector) >> 13;
    }

    for (i = 0; i < M.nthings; i++)
    {
	map_thing(i, &t);
	type = O_ThingType(&t);
	flags = MI_FLAGS(type);
	look = dmc_states[MI_SPAWNSTATE(type)].action == DMC_ACT_A_Look;
	if (type == MT_PLAYER)
	{
	    P_SpawnPlayerThing(i, &t);
	    continue;
	}
	if (!(t.options & DMC_GROUP_BIT(g)))
	    continue;
#ifdef DMC_STATS
	if (prndindex != ((o_r0 + t.rngk[g]) & 0xff))
	    I_Error("O_SpawnLevel: thing %i spawn phase %i, the image says %i", i,
		    (prndindex - o_r0) & 0xff, t.rngk[g]);
#endif
	if (flags & MF_COUNTKILL)
	    totalkills++;
	if (flags & MF_COUNTITEM)
	    totalitems++;

	if (o_promote_all
	    || ((flags & MF_SPAWNCEILING) && !(flags & MF_NOGRAVITY)))
	{
	    mo = SpawnFull(i, &t, type);
	    if (mo->tics > 0)
		mo->tics = 1 + (P_Random () % mo->tics);
	    mo->angle = ANG45 * t.angle45;
	    if (t.options & DMC_OPT_AMBUSH)
		mo->flags |= MF_AMBUSH;
	    if (look)
		o_monbits[i >> 3] |= 1 << (i & 7), ord++;
	    continue;
	}

	// P_SpawnMobj and P_SpawnMapThing draw the same numbers
	lastlook = P_Random() % MAXPLAYERS;
	tics = ST_TICS(&dmc_states[MI_SPAWNSTATE(type)]);
	if (tics > 0)
	    tics = 1 + P_Random() % tics;
	if (look)
	{
	    if (ord >= M.plan.monsters)
		I_Error("O_SpawnLevel: more monsters than the plan's %i", M.plan.monsters);
	    o_monbits[i >> 3] |= 1 << (i & 7);
	    o_sleep[ord++] = tics | lastlook << SL_LOOKSHIFT;
	    O_SetTier(i, T_SLEEP);
	}
	else
	    O_SetTier(i, T_STATIC);
    }
}


