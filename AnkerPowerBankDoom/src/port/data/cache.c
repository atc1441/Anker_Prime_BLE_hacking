// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek

#include <string.h>

#include "cache.h"
#include "img.h"

#define RC_NOTAG 0xFFFFu
// MRU way in bits 0-1 up to the LRU way in bits 6-7
#define RC_LRU_INIT 0xE4u

#ifdef DMC_FW
static uint8_t *rc_mem;
#else
static uint8_t rc_mem[RC_BYTES] __attribute__((aligned(4)));
#endif
#define rc_data ((uint8_t (*)[RC_LINE])rc_mem)
#define rc_tag ((uint16_t *)(rc_mem + RC_LINES * RC_LINE))
#define rc_lru (rc_mem + RC_LINES * RC_LINE + RC_LINES * 2)

#ifndef DMC_FW
uint32_t rc_hits, rc_misses;
#endif

void rc_reset(void)
{
    for (int i = 0; i < RC_LINES; i++)
        rc_tag[i] = RC_NOTAG;
    for (int s = 0; s < RC_SETS; s++)
        rc_lru[s] = RC_LRU_INIT;
}

#ifdef DMC_FW
void rc_place(void *base)
{
    rc_mem = base;
    rc_reset();
}
#endif

#ifdef DMC_HOST
static uint8_t *rc2_mem;
#define rc2_data ((uint8_t (*)[RC_LINE])rc2_mem)
#define rc2_tag ((uint16_t *)(rc2_mem + RC2_LINES * RC_LINE))

void rc2_begin(void *base)
{
    rc2_mem = base;
    for (int i = 0; base && i < RC2_LINES; i++)
        rc2_tag[i] = RC_NOTAG;
}

void rc2_end(void)
{
#ifdef DMC_STATS
    // nothing may rely on what it held
    if (rc2_mem)
        memset(rc2_mem, 0xA5, RC2_BYTES);
#endif
    rc2_mem = NULL;
}

void rc2_drop(void)
{
    rc2_mem = NULL;
}

// Miss of line ln whose set evicts way w: the evicted line
// moves to the second level. Returns 1 when ln came from
// there into the way.
static int rc2_swap(uint32_t ln, unsigned set, unsigned w, uint16_t old)
{
    unsigned s = ln & (RC2_LINES - 1);
    uint8_t want[RC_LINE];
    int hit = rc2_tag[s] == (uint16_t)(ln / RC2_LINES);
    // the wanted line is taken out first, the evicted one
    // may go to the same set
    if (hit)
        memcpy(want, rc2_data[s], RC_LINE);
    if (old != RC_NOTAG) {
        uint32_t vln = (uint32_t)old * RC_SETS + set;
        unsigned v = vln & (RC2_LINES - 1);
        memcpy(rc2_data[v], rc_data[set * RC_WAYS + w], RC_LINE);
        rc2_tag[v] = (uint16_t)(vln / RC2_LINES);
#ifdef DMC_STATS
        rc2_stat_hook(vln * RC_LINE, 0);
#endif
    }
    if (hit) {
        memcpy(rc_data[set * RC_WAYS + w], want, RC_LINE);
#ifdef DMC_STATS
        rc2_stat_hook(ln * RC_LINE, 1);
#endif
    }
    return hit;
}
#endif

// Moves way w to the MRU position.
static uint8_t lru_touch(uint8_t l, unsigned w)
{
    unsigned k = 0;
    while (((l >> (2 * k)) & 3u) != w)
        k++;
    unsigned below = l & ((1u << (2 * k)) - 1u);
    unsigned above = l & ~((1u << (2 * k + 2)) - 1u) & 0xFFu;
    return (uint8_t)(above | below << 2 | w);
}

// Miss of logical line number ln: the LRU way of its set gets the line. Out
// of line, so the hit path of its callers saves fewer registers.
__attribute__((noinline)) static const uint8_t *rc_fill(uint32_t ln)
{
    unsigned set = ln & (RC_SETS - 1);
    uint16_t tag = (uint16_t)(ln / RC_SETS);
    unsigned base = set * RC_WAYS;
#ifndef DMC_FW
    rc_misses++;
#endif
#ifdef DMC_STATS
    rc_stat_hook(ln * RC_LINE, 0);
#endif
    unsigned w = rc_lru[set] >> 6;
#ifdef DMC_HOST
    uint16_t old = rc_tag[base + w];
#endif
    rc_tag[base + w] = RC_NOTAG;
    uint32_t loff = ln * RC_LINE;
    uint32_t n = RC_LINE;
    if (loff + n > img.size)
        n = img.size - loff;
#ifdef DMC_HOST
    if (!rc2_mem || !rc2_swap(ln, set, w, old))
#endif
        img_read(loff, rc_data[base + w], n);
    rc_tag[base + w] = tag;
    rc_lru[set] = lru_touch(rc_lru[set], w);
    return rc_data[base + w];
}

// Byte loff in the cache, its line looked up inline in rec_get.
// The four tags are compared without a loop, and a hit on the MRU way leaves
// the LRU byte as it is (lru_touch returns it unchanged for that way).
static inline __attribute__((always_inline)) const uint8_t *rc_at(uint32_t loff)
{
    uint32_t ln = loff / RC_LINE;
    unsigned set = ln & (RC_SETS - 1);
    uint16_t tag = (uint16_t)(ln / RC_SETS);
    const uint16_t *t = &rc_tag[set * RC_WAYS];
    unsigned w;

    if (t[0] == tag)
        w = 0;
    else if (t[1] == tag)
        w = 1;
    else if (t[2] == tag)
        w = 2;
    else if (t[3] == tag)
        w = 3;
    else
        return rc_fill(ln) + (loff & (RC_LINE - 1));
#ifndef DMC_FW
    rc_hits++;
#endif
#ifdef DMC_STATS
    rc_stat_hook(ln * RC_LINE, 1);
#endif
    const uint8_t *p = rc_data[set * RC_WAYS + w] + (loff & (RC_LINE - 1));
    uint8_t *lru = &rc_lru[set];
    if ((*lru & 3u) != w)
        *lru = lru_touch(*lru, w);
    return p;
}

// The copies of constant size below use __builtin_memcpy: the firmware is
// built freestanding, where memcpy is an ordinary call. Up to 8 B it becomes
// single loads and stores. Lines and destinations need not be word aligned,
// and the M4 does those unaligned (never LDRD, STRD, LDM or STM, which fault).
void rec_get(uint32_t loff, void *out, uint32_t n)
{
    uint8_t *o = out;
    unsigned in = loff & (RC_LINE - 1);

    if (n - 1u < RC_LINE - in) {
        // in one line: every record and stream field
        const uint8_t *p = rc_at(loff);
        switch (n) {
        case 16:
            __builtin_memcpy(o, p, 8);
            __builtin_memcpy(o + 8, p + 8, 8);
            return;
        case 8:
            __builtin_memcpy(o, p, 8);
            return;
        case 4:
            __builtin_memcpy(o, p, 4);
            return;
        case 2:
            __builtin_memcpy(o, p, 2);
            return;
        case 1:
            *o = *p;
            return;
        }
        memcpy(o, p, n);
        return;
    }
    // across lines: the part in each line
    while (n) {
        unsigned k = RC_LINE - in;
        if (k > n)
            k = n;
        rec_get(loff, o, k);
        o += k;
        loff += k;
        n -= k;
        in = 0;
    }
}

uint16_t rec_u16(uint32_t loff)
{
    uint16_t v;
    rec_get(loff, &v, 2);
    return v;
}

uint32_t rec_u32(uint32_t loff)
{
    uint32_t v;
    rec_get(loff, &v, 4);
    return v;
}
