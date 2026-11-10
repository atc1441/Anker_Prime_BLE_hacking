// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The HASH overlay (dmc_fw.h): the lockstep hash of every tic of the verification pass
// (tests/lockstep/hash.c with LS_SMALL, linked into this overlay), chained into DOOMRES.DAT,
// and the progress on the screen. Its scratch is the frame region behind the overlay's window
// up to the creation order of the object store (dmc_level_end).

#include "doomdef.h"
#include "doomstat.h"

#include "dmc_fw.h"
#include "hash.h"
#include "nor.h"
#include "ovl.h"
#include "ui.h"

static void progress(const dmc_run_t *r)
{
    if (r->v.tics == 1) {
        dmc_screen("Demo verification");
        ui_row(1, C_WHITE, "%s: lockstep hash of every tic", dmc_demo_names[r->k]);
        ui_row(2, C_GREY, "no 3D view, sound muted, MODE: stop");
    }
    ui_row(4, C_WHITE, "%s: tic %u of %u", dmc_demo_names[r->k], (unsigned)r->v.tics,
           (unsigned)r->tics[r->k]);
    ui_row(5, C_GREY, "chain %08X%08X", (unsigned)(r->v.c >> 32), (unsigned)r->v.c);
    ui_row(6, C_GREY, "demo %d, E%dM%d skill %d", (int)r->k + 1, gameepisode, gamemap,
           gameskill + 1);
}

// Chain slot n of DOOMRES.DAT gets the low word of c.
static void chain(uint32_t n, uint64_t c)
{
    uint32_t w = (uint32_t)c;
    if (res_addr && n < RES_CHAIN_MAX)
        nor_program(res_addr + RES_CHAIN_OFF + 4u * n, &w, 4);
}

static void hash_tic(void)
{
    dmc_run_t *r = &dmc_run;
    uint8_t *mem = DMC_FRAME_BASE + OVL_WIN_HASH;
    uint64_t h = LS_HashScratch(mem, dmc_hash_top > mem ? (uint32_t)(dmc_hash_top - mem) : 0);
    r->v.h = h;
    r->v.c = LS_Feed(LS_Feed(r->v.c, (uint32_t)h), (uint32_t)(h >> 32));
    r->v.tics++;
    if ((gametic + 1) % RES_CHAIN_TICS == 0)
        chain(r->v.first + r->v.nchain++, r->v.c);
    if (r->v.tics % RES_CHAIN_TICS == 1)
        progress(r);
}

OVL_TABLE(HASH, OVL_ENTRY(HASH_E_TIC, hash_tic_fn, hash_tic));
