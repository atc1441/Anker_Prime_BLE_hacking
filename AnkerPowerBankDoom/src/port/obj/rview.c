// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The renderer's view of the level state (rview.h): sector heights,
// flats and light, and the things of a sector as copies, whatever their
// tier. Corpse records come last in a sector, drawn at its floor.

#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"

#include "rview.h"
#include "things.h"

fixed_t rv_floorh (int sec)
{
    return P_FloorHeight(sec);
}

fixed_t rv_ceilh (int sec)
{
    return P_CeilingHeight(sec);
}

int rv_floorpic (int sec)
{
    return P_FloorPic(sec);
}

int rv_ceilpic (int sec)
{
    return P_CeilingPic(sec);
}

int rv_light (int sec)
{
    return P_LightLevel(sec);
}

void rv_sector (int sec, rsector_t* s)
{
    dmc_sector_t	r;
    secovl_t*		o = P_SectorRead(sec, &r);

    s->floorheight = P_RecFloorHeight(o, &r);
    s->ceilingheight = P_RecCeilingHeight(o, &r);
    s->floorpic = P_RecFloorPic(o, &r);
    s->ceilingpic = P_RecCeilingPic(o, &r);
    s->lightlevel = P_RecLightLevel(o, &r);
}

static void Fill (rthing_t* r, const mobj_t* mo, unsigned h)
{
    r->x = mo->x;
    r->y = mo->y;
    r->z = mo->z;
    r->angle = mo->angle;
    r->flags = mo->flags;
    r->sprite = dmc_states[mo->statenum].sprite;
    r->frame = ST_FRAME(&dmc_states[mo->statenum]);
    r->handle = h;
}

static void FillCorpse (rthing_t* r, const corpse_t* c, unsigned h)
{
    r->x = c->x << FRACBITS;
    r->y = c->y << FRACBITS;
    r->z = P_FloorHeight(CORPSE_SEC(c));
    r->angle = 0;
    r->flags = MF_CORPSE | (c->secinfo & CORPSE_SHADOW ? MF_SHADOW : 0);
    r->sprite = dmc_states[c->statenum].sprite;
    r->frame = ST_FRAME(&dmc_states[c->statenum]);
    r->handle = h;
}

static rthing_fn	cur_fn;
static void*		cur_ctx;
static int		cur_sec;

static boolean Thing (mobj_t* mo)
{
    rthing_t	r;

    Fill(&r, mo, O_Hnd(mo));
    cur_fn(&r, cur_ctx);
    return true;
}

static void Corpse (corpse_t* c, unsigned h, void* found)
{
    rthing_t	r;

    if (CORPSE_SEC(c) != cur_sec)
	return;
    *(boolean *) found = true;
    FillCorpse(&r, c, h);
    cur_fn(&r, cur_ctx);
}

void rv_sector_things (int sec, rthing_fn fn, void* ctx)
{
    boolean	found = false;

    cur_fn = fn;
    cur_ctx = ctx;
    cur_sec = sec;
    O_SectorThings(sec, Thing);
    if (secstat[sec] & SS_CORPSE)
    {
	// SS_CORPSE is a hint: evicted corpse pages leave it set, the
	// first scan that finds no record clears it
	O_ForCorpses(Corpse, &found);
	if (!found)
	    secstat[sec] &= ~SS_CORPSE;
    }
}

int rv_thing (uint16_t handle, rthing_t* out)
{
    mobj_t	v;
    mobj_t*	mo;
    corpse_t*	c;

    if (H_KIND(handle) == HK_CORPSE)
    {
	c = O_Corpse(handle);
	if (!c)
	    return 0;
	FillCorpse(out, c, handle);
	return 1;
    }
    mo = O_Get(handle, &v);
    if (!mo || (mo->flags2 & MF2_REMOVED))
	return 0;
    Fill(out, mo, handle);
    return 1;
}
