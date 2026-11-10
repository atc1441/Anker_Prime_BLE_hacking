// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Record cache: 128 lines of 16 B over the whole logical
// image, 32 sets of 4 ways, u16 tags, one LRU byte per set. Records are
// copied out, nothing returns a pointer into the cache.

#ifndef DMC_CACHE_H
#define DMC_CACHE_H

#include <stdint.h>

#define RC_LINE 16
#define RC_WAYS 4
#ifndef RC_SETS
#define RC_SETS 32
#endif
#define RC_LINES (RC_WAYS * RC_SETS)
// RAM of the cache: line data, u16 tags and the LRU bytes
#define RC_BYTES (RC_LINES * RC_LINE + RC_LINES * 2 + RC_SETS)

// Firmware builds (DMC_FW) keep the cache in the RAM after the frame region of
// the map's plan: rc_place moves it to RC_BYTES at base (4 B
// aligned) and invalidates every line. Other builds use a static array.
void rc_place(void *base);

// Invalidates every line (level load, or after the cache RAM was borrowed).
void rc_reset(void);

// Copies n bytes at logical offset loff through the cache. Ranges that cross
// a line boundary are read line by line.
void rec_get(uint32_t loff, void *out, uint32_t n);

// Single fields through the cache.
uint16_t rec_u16(uint32_t loff);
uint32_t rec_u32(uint32_t loff);

// Line lookups and line fills since the last reset of the counters, not
// counted in firmware builds (DMC_FW).
#ifndef DMC_FW
extern uint32_t rc_hits, rc_misses;
#endif

#ifdef DMC_STATS
// Called for every line lookup when statistics are enabled.
void rc_stat_hook(uint32_t loff, int hit);
#endif

#ifdef DMC_HOST
// Host model of a second level for the tics (RC2_BYTES at base, 4 B
// aligned): RC2_LINES lines of 16 B, direct mapped by line number, u16 tags.
// While it is on, a line the cache evicts moves there and a miss takes the
// line from there when it holds it, else from the NOR. rc2_begin turns it on
// empty (NULL leaves it off), rc2_end and rc2_drop turn it off.
#define RC2_LINES 128
#define RC2_BYTES (RC2_LINES * RC_LINE + RC2_LINES * 2)
void rc2_begin(void *base);
void rc2_end(void);
void rc2_drop(void);
#ifdef DMC_STATS
// Called with the logical offset of every line the second level gives to a
// miss (hit 1) and of every line it takes from the cache (hit 0) when
// statistics are enabled.
void rc2_stat_hook(uint32_t loff, int hit);
#endif
#endif

#endif
