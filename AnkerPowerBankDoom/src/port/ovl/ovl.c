// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Overlay loader. DOOMCODE.DMC layout (little-endian, written by tools/pack_overlays.py):
//
//   header, 64 B: char[8] "DMCCODE\0", u16 fmt = 1, u16 count, u8 build_id[20],
//                 u32 total size, u32 crc, 24 B zero
//   directory:    count x 16 B: u8 id, u8 0, u16 entry count, u32 off, u32 size, u32 crc
//   blobs:        u32 magic "OVL1", u32 vma (FRAME_BASE), u32 copy size (= size),
//                 u32 bss size (cleared from the end of the blob), u32 entry[n], then code,
//                 rodata and data
//
// The header crc is the CRC-32 (port/data/crc32.c) of the header (crc field 0) and the
// directory, each blob has its own. A load reads the header and the directory, checks the blob
// header against the window, then copies the blob to FRAME_BASE, checks its CRC and clears
// the bss.

#include <stddef.h>

#include "crc32.h"
#include "lcd.h"
#include "nor.h"
#include "ovl.h"
#include "platform.h"

// From the C library on the host, from platform/rt.c on the target
int memcmp(const void *a, const void *b, size_t n);
void *memset(void *d, int c, size_t n);

#define CODE_FMT 1u
#define BLOB_MAGIC 0x314C564Fu  // "OVL1"
#define HDR_SIZE 64u
#define ENT_SIZE 16u
#define MAX_COUNT 32u

struct code_hdr {
    char magic[8];
    uint16_t fmt, count;
    uint8_t build_id[20];
    uint32_t total, crc;
    uint8_t zero[24];
};

struct code_ent {
    uint8_t id, pad;
    uint16_t nent;
    uint32_t off, size, crc;
};

struct blob_hdr {
    uint32_t magic, vma, copy, bss;
};

_Static_assert(sizeof(struct code_hdr) == HDR_SIZE, "DOOMCODE.DMC header");
_Static_assert(sizeof(struct code_ent) == ENT_SIZE, "DOOMCODE.DMC entry");

const uint16_t ovl_win[OVL_IDS] = {
#define X(name, win) [OVL_##name] = win,
    OVL_LIST(X)
#undef X
};

uint16_t ovl_loads;
static uint8_t cur;
static uint16_t nent;
static ovl_fail_fn *fail_hook;

#ifdef PLAT_HOST

uint8_t dmc_build_id[20];
int ovl_host_copy;
uint32_t ovl_host_vma;
// The frame model of the host engine places it at the start of its frame region (mk/engine.mk)
#ifndef DMC_HOST_FRAME
uint8_t ovl_host_frame[OVL_WIN_FRAME] __attribute__((aligned(16)));
#endif
#define BASE ovl_host_frame
#define VMA ovl_host_vma

// Native entry tables of the overlays linked into this binary (OVL_TABLE)
#define X(name, win)                                                   \
    extern const ovl_fn ovl_tab_##name[] __attribute__((weak));        \
    extern const uint16_t ovl_cnt_##name __attribute__((weak));
OVL_LIST(X)
#undef X

static const ovl_fn *host_tab(int id, unsigned *n)
{
    *n = 0;
    switch (id) {
#define X(name, win)                                                   \
    case OVL_##name:                                                   \
        if (!ovl_tab_##name)                                           \
            return 0;                                                  \
        *n = ovl_cnt_##name;                                           \
        return ovl_tab_##name;
        OVL_LIST(X)
#undef X
    }
    return 0;
}

#else

// Placeholder until stamped, 19 characters and a NUL
const volatile uint8_t dmc_build_id[20] = "DMCDOOM NOT STAMPED";
#define BASE ((uint8_t *)__frame_start)
#define VMA ((uint32_t)(uintptr_t)__frame_start)

#endif

int ovl_load(int id)
{
    if (id == cur && id)
        return OVL_OK;
    if (id <= 0 || id >= OVL_IDS || !ovl_win[id])
        return OVL_ENOENT;
#ifdef PLAT_HOST
    unsigned hn;
    const ovl_fn *ht = host_tab(id, &hn);
    if (!ht)
        return OVL_ENOENT;
    if (!ovl_host_copy) {
#ifdef DMC_HOST_FRAME
        // what the copy of the target overwrites, after the same waits
        nor_wait_all();
        pump_sync(0);
        memset(BASE, 0xA5, ovl_win[id]);
#endif
        cur = (uint8_t)id;
        nent = (uint16_t)hn;
        ovl_loads++;
        return OVL_OK;
    }
#endif
    int32_t fsize = ovl_file_size();
    if (fsize < 0)
        return OVL_ENOFILE;
    if (fsize < (int32_t)HDR_SIZE)
        return OVL_EHEADER;
    struct code_hdr h;
    ovl_file_read(0, &h, HDR_SIZE);
    if (memcmp(h.magic, "DMCCODE", 8) || h.fmt != CODE_FMT || h.count > MAX_COUNT ||
        h.total > (uint32_t)fsize || h.total < HDR_SIZE + ENT_SIZE * h.count)
        return OVL_EHEADER;
    uint32_t want = h.crc;
    h.crc = 0;
    uint32_t c = crc32_update(CRC32_INIT, &h, HDR_SIZE);
    struct code_ent e, f = {0};
    for (unsigned i = 0; i < h.count; i++) {
        ovl_file_read(HDR_SIZE + ENT_SIZE * i, &e, ENT_SIZE);
        c = crc32_update(c, &e, ENT_SIZE);
        if (e.id == id)
            f = e;
    }
    if (crc32_final(c) != want)
        return OVL_EHEADER;
    for (unsigned i = 0; i < 20; i++) {
        if (h.build_id[i] != dmc_build_id[i])
            return OVL_EBUILD;
    }
    if (f.id != id)
        return OVL_ENOENT;
    struct blob_hdr b;
    uint32_t win = ovl_win[id];
    if (f.off > h.total || f.size > h.total - f.off || f.size > win || f.size < 16u + 4u * f.nent)
        return OVL_EBLOB;
    ovl_file_read(f.off, &b, sizeof(b));
    if (b.magic != BLOB_MAGIC || b.vma != VMA || b.copy != f.size || b.bss > win - f.size ||
        !f.nent)
        return OVL_EBLOB;
#ifdef PLAT_HOST
    if (f.nent != hn)
        return OVL_EBLOB;
#endif
    nor_wait_all();
    pump_sync(0);
    cur = 0;
    ovl_file_read(f.off, BASE, f.size);
    if (crc32_buf(BASE, f.size) != f.crc)
        return OVL_ECRC;
    memset(BASE + f.size, 0, b.bss);
#ifndef PLAT_HOST
    __asm volatile("dsb\n\tisb" ::: "memory");
#endif
    cur = (uint8_t)id;
    nent = f.nent;
    ovl_loads++;
    return OVL_OK;
}

static const char *const why[] = {
    "DOOMCODE.DMC not found",                   // OVL_ENOFILE
    "DOOMCODE.DMC is damaged, copy it again",   // OVL_EHEADER
    "Copy DOOMCODE.DMC from the same release",  // OVL_EBUILD
    "DOOMCODE.DMC lacks an overlay",            // OVL_ENOENT
    "DOOMCODE.DMC has a bad overlay",           // OVL_EBLOB
    "DOOMCODE.DMC CRC error, copy it again",    // OVL_ECRC
    "Overlay entry out of range",               // OVL_EENTRY
};

__attribute__((noreturn)) static void ovl_fail(int id, int err)
{
    char m[64];
    if (fail_hook)
        fail_hook(id, err);
    // The first fault screen line holds 39 characters
    plat_fmt(m, sizeof(m), "%-39s overlay %d", why[-err - 1], id);
    plat_panic(m);
}

ovl_fn ovl_entry(int id, int idx)
{
    if (id != cur || !id) {
        int err = ovl_load(id);
        if (err)
            ovl_fail(id, err);
    }
    if ((unsigned)idx >= nent)
        ovl_fail(id, OVL_EENTRY);
#ifdef PLAT_HOST
    unsigned n;
    return host_tab(id, &n)[idx];
#else
    return ((const ovl_fn *)(BASE + sizeof(struct blob_hdr)))[idx];
#endif
}

int ovl_resident(void) { return cur; }
void ovl_invalidate(void) { cur = 0; }
void ovl_set_fail_hook(ovl_fail_fn *fn) { fail_hook = fn; }
