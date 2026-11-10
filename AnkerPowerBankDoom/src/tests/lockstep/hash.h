// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Per-tic game state hash of the lockstep tests, computed the same way by the
// oracle and by the engine.
//
// The hash is taken after each G_Ticker call, before gametic is incremented.
// Every value is fed as a 32-bit two's complement integer, little-endian
// byte order, into 64-bit FNV-1a (offset basis 0xcbf29ce484222325,
// prime 0x100000001b3). Booleans are 0 or 1. Pointers are never hashed:
// an object reference is the 1-based ordinal of the referenced mobj in the
// hashed mobj list (part 4), 0 for NULL and -1 for an object that is not in
// that list (removed or dangling).
//
// Order of values:
//
// 1. Globals: gametic, leveltime, gamestate, gameskill, gameepisode,
//    gamemap, prndindex, rndindex.
//
// 2. For each player i in 0..MAXPLAYERS-1 with playeringame[i]: i, mo (ref),
//    playerstate, cmd.forwardmove, cmd.sidemove, cmd.angleturn,
//    cmd.buttons, viewz, viewheight, deltaviewheight, bob, health,
//    armorpoints, armortype, powers[0..5], cards[0..5], backpack,
//    readyweapon, pendingweapon, weaponowned[0..8], ammo[0..3],
//    maxammo[0..3], attackdown, usedown, cheats, refire, killcount,
//    itemcount, secretcount, damagecount, bonuscount, attacker (ref),
//    extralight, fixedcolormap, then for psprites 0..1: state index (-1 for
//    NULL), tics, sx, sy, and finally didsecret.
//
// 3. numsectors, then for each sector in index order: floorheight,
//    ceilingheight, lightlevel, special, mover presence
//    (specialdata != NULL).
//
// 4. The hashed mobjs: every live mobj thinker (function P_MobjThinker,
//    removed thinkers skipped) in thinker-list order, without the resting
//    corpses below when LS_SKIP_RESTING_CORPSES is 1. For each: type, x, y,
//    z, momx, momy, momz, angle, flags, health, state index (state - states),
//    tics, target (ref), movedir, movecount, reactiontime, threshold,
//    lastlook. Then the number of hashed mobjs.
//
// Before the first level is set up (the title page at startup) the thinker
// list does not exist yet: part 4 is empty and the number of hashed mobjs
// is 0. Part 3 is then numsectors 0 and no sectors.
//
// Resting corpse: MF_CORPSE set, player == NULL, tics == -1, momx, momy and
// momz zero and z == floorz, and not pinned. A mobj is pinned when it is the
// target or tracer of a mobj that is not itself a resting corpse candidate,
// or the mo or attacker of a player in game.
//
// Mobj types, state indices, weapons and sector specials use the vanilla
// numbering of doomgeneric dcb7a8d (info.h, d_items.h). An engine with
// renumbered tables maps them back before hashing.
//
// Output line per tic: decimal gametic, one space, 16 lowercase hex digits.

#ifndef LS_HASH_H
#define LS_HASH_H

#include <stdint.h>

#define LS_HASH_VERSION 1

#ifndef LS_SKIP_RESTING_CORPSES
#define LS_SKIP_RESTING_CORPSES 0
#endif

#define LS_FNV_OFFSET 0xcbf29ce484222325ull
#define LS_FNV_PRIME 0x100000001b3ull

static inline uint64_t LS_Feed(uint64_t h, uint32_t v)
{
    for (int i = 0; i < 4; i++) {
        h ^= (v >> (8 * i)) & 0xffu;
        h *= LS_FNV_PRIME;
    }
    return h;
}

// Hash of the current game state.
uint64_t LS_Hash(void);

// LS_SMALL builds (hash.c): the hash with a scratch of 2 B per handle slot
// (map things, pages and drops of the level). LS_Hash of host builds uses a
// static one.
uint64_t LS_HashScratch(void *mem, uint32_t bytes);

// Per-tic output: LS_Open starts a hash stream file, LS_Tic appends the line
// for the tic that just ran, LS_DumpAt writes a text dump of every hashed
// value of one tic to a second file. LS_Close closes the stream.
int LS_Open(const char *path);
void LS_DumpAt(int tic, const char *path);
void LS_Tic(void);
void LS_Close(void);

// When set, gametic is hashed (and dumped) as 0, so the streams of two runs
// whose tics are numbered apart can be compared (--hash-no-gametic).
extern int LS_NoGametic;

// Scripted input stream shared by both runners: 8-byte records, record n is
// the ticcmd built for maketic n. Layout: forwardmove (int8), sidemove
// (int8), angleturn (int16 little-endian), buttons (uint8), 3 zero bytes.
// Past the end of the stream the ticcmd is all zero. LS_FillCmd takes the
// ticcmd the game built for maketic and replaces it by record maketic. When
// the built one sends a special (BT_SPECIAL, a pause or a save from the
// menu), its buttons are kept, so the special reaches G_Ticker.
#define LS_CMD_SIZE 8
int LS_LoadCmds(const char *path);
int LS_HaveCmds(void);
void LS_FillCmd(void *ticcmd, int maketic);

#endif
