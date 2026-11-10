// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The SAVEG overlay (fe.h, its entry table in port/game/l_save.c): a slot of SAVE.DAT holds
// the game state of a level at the end of a tic, after its display and before the next tic,
// where vanilla takes a save from the menu and no wipe is pending. Its body:
//   globals block  the game state outside the level arena (SAVE_VARS), the player record with
//                  its pointers as handles and state indices, then gametic and the demo
//                  cursor, which only the test loads take back (DMC_SF_EXACT, DMC_SF_DEMO)
//   order block    with DMC_SF_ORDER: the creation order of the pages and drop slots that the
//                  lockstep hash needs (the host, the verification pass of the meter) as u16
//                  ranks, where only the order counts
//   level arena    as it is in RAM
// A write erases the four sectors of the slot, programs the body, reads it back against its
// CRC and programs the header last, so a cut save leaves an empty slot. A restore runs after
// the level setup of the saved map and skill and puts the body over it, with the pointers and
// the arena at the same places. Work buffer: the tic scratch behind the front-end scratch
// (compose buffer 1), free between tics and frames.
//
// The menu (MENU overlay) links this file for the slot headers and the check of a save.

#include <stddef.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_loop.h"
#include "d_main.h"
#include "p_local.h"
#include "p_mapdata.h"

#include "crc32.h"
#include "extents.h"
#include "fe.h"
#include "img.h"
#include "nor.h"
#include "ovl.h"
#include "platform.h"
#include "r_strip.h"
#include "things.h"

extern int prndindex, rndindex;
extern unsigned short seg1, seg2, seg2tail;
extern unsigned o_moverused;
extern gamestate_t oldgamestate;
extern boolean onground;
extern uint32_t demo_p, demoend, demobufoff;

#define SAVE_BUF ((byte *)&dmc_tic + 256)
#define SAVE_BUFBYTES (TICSCRATCH - 256)

// The game state outside the level arena, the player record first. onground (p_user.c) is
// that of the last move, which P_CalcHeight reads while a teleport keeps the player still.
#define SAVE_VARS(X)                                                                          \
    X(players[0]) X(leveltime) X(prndindex) X(rndindex) X(usergame) X(demoplayback) X(o_pool) \
    X(o_ndrops) X(o_r0) X(o_cursor) X(seg1) X(seg2) X(seg2tail) X(o_moverused) X(numlights)   \
    X(onground)

typedef struct {
    void *p;
    unsigned n;
} savevar_t;

#define SAVE_VAR(v) {&(v), sizeof(v)},
#define SAVE_SIZE(v) +sizeof(v)

static const savevar_t savevars[] = {SAVE_VARS(SAVE_VAR)};

// SAVE_VARS, gametic and the demo cursor
#define GLOBALS_BYTES ((0 SAVE_VARS(SAVE_SIZE) + 2 * sizeof(uint32_t) + 3) & ~3u)

_Static_assert(GLOBALS_BYTES <= DMC_SAVE_GLOBALS_MAX, "the globals block of a save");
_Static_assert(GLOBALS_BYTES <= SAVE_BUFBYTES, "the globals block fits the work buffer");

// ---- SAVE.DAT

int G_SaveUsable(void)
{
    const dmc_file_t *f = &dmc_files[DMC_F_SAVE];
    // a slot is whole NOR sectors
    if (f->size != DMC_SAVE_SIZE || f->cshift != 12 || (f->data & 0xFFF))
        return SAVE_ENOFILE;
    return SAVE_OK;
}

int G_SaveHeader(int slot, dmc_save_hdr_t *h)
{
    uint8_t uuid[8];
    if (G_SaveUsable())
        return SAVE_ENOFILE;
    ext_read(DMC_F_SAVE, slot * SAVE_SLOT_BYTES, h, sizeof(*h));
    img_read(offsetof(dmc_header_t, image_uuid), uuid, sizeof(uuid));
    if (memcmp(h->magic, DMC_MAGIC_SAVE, 8) || memcmp(h->image_uuid, uuid, 8)
        || h->size > SAVE_SLOT_BYTES - sizeof(*h))
        return SAVE_EEMPTY;
    for (int i = 0; i < 8; i++)
        if (h->build_id[i] != dmc_build_id[i])
            return SAVE_EEMPTY;
    return SAVE_OK;
}

// CRC of n bytes of SAVE.DAT from off
static uint32_t file_crc(uint32_t off, uint32_t n)
{
    uint32_t reg = CRC32_INIT;
    for (uint32_t k; n; off += k, n -= k) {
        k = n < SAVE_BUFBYTES ? n : SAVE_BUFBYTES;
        ext_read(DMC_F_SAVE, off, SAVE_BUF, k);
        reg = crc32_update(reg, SAVE_BUF, k);
    }
    return crc32_final(reg);
}

int G_SaveCheck(int slot, dmc_save_hdr_t *h)
{
    int e = G_SaveHeader(slot, h);
    if (e)
        return e;
    if (file_crc(slot * SAVE_SLOT_BYTES + sizeof(*h), h->size) != h->crc)
        return SAVE_ECRC;
    return SAVE_OK;
}

// n bytes of src to SAVE.DAT at off
static int program(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    for (uint32_t run; n; off += run, s += run, n -= run) {
        uint32_t a = ext_phys(&dmc_files[DMC_F_SAVE], off, &run);
        if (run > n)
            run = n;
        if (nor_program(a, s, run))
            return SAVE_ENOR;
    }
    return SAVE_OK;
}

// ---- the game

// Bytes of the level arena, which ends at O_ARENA_END
static uint32_t arena_bytes(void)
{
#ifdef DMC_STATS
    if (o_promote_all)
        return O_ARENA_MAX;
#endif
    return M.plan.arena_bytes;
}

// The options of the host's object store, which a load must run with too
static uint32_t store(void)
{
#ifdef DMC_STATS
    return (uint32_t)(o_promote_all | o_keep_corpses << 1 | (o_reserve & 0xFF) << 2
                      | (o_mover_cap & 0xFF) << 10) | (uint32_t)o_page_cap << 18;
#else
    return 0;
#endif
}

// Object pointers of the player record as handles and back. The attacker of the previous
// level is O_DETACHED (port/obj/things.h).
static uintptr_t ref(mobj_t *mo)
{
    return !mo ? H_NONE : mo == O_DETACHED ? H_DANGLE : H_OBJ(O_PAGEIDX(mo));
}

static mobj_t *deref(uintptr_t h)
{
    return h == H_NONE ? NULL : h == H_DANGLE ? O_DETACHED : O_PAGE(H_IDX(h));
}

static void gather(uint8_t *b)
{
    player_t *p = (player_t *)(void *)b;
    for (unsigned i = 0; i < arrlen(savevars); i++) {
        memcpy(b, savevars[i].p, savevars[i].n);
        b += savevars[i].n;
    }
    memcpy(b, &gametic, sizeof(uint32_t));
    memcpy(b + sizeof(uint32_t), &demo_p, sizeof(uint32_t));
    p->mo = (mobj_t *)ref(p->mo);
    p->attacker = (mobj_t *)ref(p->attacker);
    p->message = NULL;
    for (int i = 0; i < NUMPSPRITES; i++)
        if (p->psprites[i].state)
            p->psprites[i].state =
                (const state_t *)(uintptr_t)(p->psprites[i].state - dmc_states + 1);
}

static void scatter(const uint8_t *b, int flags)
{
    player_t *p = &players[0];
    for (unsigned i = 0; i < arrlen(savevars); i++) {
        memcpy(savevars[i].p, b, savevars[i].n);
        b += savevars[i].n;
    }
    if (flags & DMC_SF_EXACT)
        memcpy(&gametic, b, sizeof(uint32_t));
    if (flags & DMC_SF_DEMO)
        memcpy(&demo_p, b + sizeof(uint32_t), sizeof(uint32_t));
    p->mo = deref((uintptr_t)p->mo);
    p->attacker = deref((uintptr_t)p->attacker);
    for (int i = 0; i < NUMPSPRITES; i++)
        if (p->psprites[i].state)
            p->psprites[i].state = &dmc_states[(uintptr_t)p->psprites[i].state - 1];
}

// The creation order: entry i of the pages, then of the drop slots
static uint32_t order_count(void)
{
    return o_pool.npages + M.plan.drop_slots + 1u;
}

static unsigned *order_entry(uint32_t i)
{
    return i < o_pool.npages ? &o_seq_page[i] : &o_seq_drop[i - o_pool.npages];
}

// Rank of creation number v among all of them, 0 for none. Numbers are given once each, so
// the ranks keep the order.
static unsigned rank(unsigned v)
{
    unsigned r = 1;
    if (!v)
        return 0;
    for (uint32_t i = 0, n = order_count(); i < n; i++) {
        unsigned u = *order_entry(i);
        r += u && u < v;
    }
    return r;
}

// ---- SAVE_E_WRITE

int G_SaveWrite(int slot, int flags)
{
    dmc_save_hdr_t h;
    uint32_t base = (uint32_t)slot * SAVE_SLOT_BYTES, off = base + sizeof(h), order = 0, run;
    uint32_t arena = arena_bytes(), reg = CRC32_INIT;
    uint16_t *r = (uint16_t *)(void *)SAVE_BUF;
    int secs = leveltime / TICRATE, e;

    if (G_SaveUsable())
        return SAVE_ENOFILE;
    // a level whose wipe the display has taken (the first display after a start follows the
    // tic after the start) and no game action waiting, such as the end of the level after an
    // exit in the tic before. A save from the menu clears its own action first.
    if (gamestate != GS_LEVEL || wipegamestate != gamestate || gameaction != ga_nothing)
        return SAVE_EGAME;
    if (o_seq_page) {
        flags |= DMC_SF_ORDER;
        order = 2 * order_count();
    }
    if (demoplayback)
        flags |= DMC_SF_DEMO;
    if (sizeof(h) + GLOBALS_BYTES + order + arena > SAVE_SLOT_BYTES)
        return SAVE_EBIG;

    for (uint32_t i = 0; i < DMC_SAVE_SLOT_SECTORS; i++)
        if (nor_erase4k(ext_phys(&dmc_files[DMC_F_SAVE], base + 4096 * i, &run)))
            return SAVE_ENOR;

    memset(SAVE_BUF, 0, GLOBALS_BYTES);
    gather(SAVE_BUF);
    reg = crc32_update(reg, SAVE_BUF, GLOBALS_BYTES);
    if ((e = program(off, SAVE_BUF, GLOBALS_BYTES)))
        return e;
    off += GLOBALS_BYTES;

    for (uint32_t i = 0, k; i < order / 2; i += k) {
        for (k = 0; k < SAVE_BUFBYTES / 2 && i + k < order / 2; k++)
            r[k] = (uint16_t)rank(*order_entry(i + k));
        reg = crc32_update(reg, r, 2 * k);
        if ((e = program(off, r, 2 * k)))
            return e;
        off += 2 * k;
    }

    reg = crc32_update(reg, O_ARENA_END - arena, arena);
    if ((e = program(off, O_ARENA_END - arena, arena)))
        return e;
    off += arena;

    memset(&h, 0, sizeof(h));
    h.size = off - base - sizeof(h);
    h.crc = crc32_final(reg);
    if (file_crc(base + sizeof(h), h.size) != h.crc)
        return SAVE_ENOR;
    memcpy(h.magic, DMC_MAGIC_SAVE, 8);
    h.map = (uint8_t)gamemap;
    h.skill = (uint8_t)gameskill;
    h.plan = M.plan.plan;
    h.flags = (uint8_t)flags;
    h.leveltime = (uint32_t)leveltime;
    plat_fmt(h.desc, sizeof(h.desc), "E%dM%d %d:%02d", gameepisode, gamemap, secs / 60, secs % 60);
    for (int i = 0; i < 8; i++)
        h.build_id[i] = dmc_build_id[i];
    img_read(offsetof(dmc_header_t, image_uuid), h.image_uuid, sizeof(h.image_uuid));
    h.npages = o_pool.npages;
    h.arena_tail = (uint16_t)o_arena_tail;
    h.store = store();
    if ((e = program(base, &h, sizeof(h))))
        return e;
    // as vanilla's G_DoSaveGame, a test save leaves the game as it is
    if (!(flags & DMC_SF_EXACT))
        players[consoleplayer].message = P_String(DMC_STR_GGSAVED);
    return SAVE_OK;
}

// ---- SAVE_E_RESTORE: after the level setup of the saved map and skill, the arena, the
// globals and the creation order. No wipe follows (the save was taken after a display), the
// status band is sent whole.

int G_SaveRestore(int slot, int flags)
{
    dmc_save_hdr_t h;
    uint32_t off = (uint32_t)slot * SAVE_SLOT_BYTES + sizeof(h), order, arena = arena_bytes();
    uint16_t *r = (uint16_t *)(void *)SAVE_BUF;
    int e = G_SaveHeader(slot, &h);

    if (e)
        return e;
    order = h.flags & DMC_SF_ORDER ? 2 * order_count() : 0;
    if (gamestate != GS_LEVEL || h.map != gamemap || h.skill != gameskill
        || h.npages != o_pool.npages || h.arena_tail != o_arena_tail || h.store != store()
        || h.size != GLOBALS_BYTES + order + arena)
        return SAVE_EGAME;

    ext_read(DMC_F_SAVE, off, SAVE_BUF, GLOBALS_BYTES);
    scatter(SAVE_BUF, flags & h.flags);
    off += GLOBALS_BYTES;

    if (o_seq_page) {
        uint32_t n = order_count();
        o_seqnext = 0;
        for (uint32_t i = 0, k; i < n; i += k) {
            k = n - i < SAVE_BUFBYTES / 2 ? n - i : SAVE_BUFBYTES / 2;
            // a save without the order gives every object the same rank
            if (order)
                ext_read(DMC_F_SAVE, off + 2 * i, r, 2 * k);
            else
                memset(r, 0, 2 * k);
            for (uint32_t j = 0; j < k; j++) {
                *order_entry(i + j) = r[j];
                if (r[j] > o_seqnext)
                    o_seqnext = r[j];
            }
        }
    }
    off += order;

    ext_read(DMC_F_SAVE, off, O_ARENA_END - arena, arena);

    wipegamestate = gamestate;
    oldgamestate = GS_LEVEL;
    if (flags & h.flags & DMC_SF_EXACT)
        D_ResetTics();
    if (flags & h.flags & DMC_SF_DEMO)
        demobufoff = demoend;
    R_PaletteChanged();
    return SAVE_OK;
}
