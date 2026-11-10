// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The logical image: DOOM.DAT[0, split) followed by
// DOOMX.DAT[64, end). Offsets are translated through the FAT extents of both
// files, reads are split at extent boundaries and at the split point.

#ifndef DMC_IMG_H
#define DMC_IMG_H

#include <stdint.h>

#include "dmcdat.h"

enum {
    IMG_OK = 0,
    IMG_E_FS = -1,      // a volume or a FAT chain is damaged
    IMG_E_NODATA = -2,  // no DOOM.DAT on the data drive
    IMG_E_FRAG = -3,    // a file has more extents than allowed: copy it again
    IMG_E_HEADER = -4,  // bad magic, format version, sizes or a missing section
    IMG_E_CRC = -5,     // header, section, map or DOOMX payload CRC
    IMG_E_DOOMX = -6,   // DOOMX.DAT missing or from another image
};

typedef struct {
    uint32_t off;
    uint32_t size;
    uint16_t flags;
} img_sect_t;

typedef struct {
    uint32_t size;        // logical image size
    uint32_t split;       // DOOM.DAT part of it
    uint32_t header_crc;
    uint16_t nmaps;
    uint16_t nsect;
    img_sect_t sect[DMC_SECT_MAX];  // by section id, size 0 when absent
} img_dir_t;

extern img_dir_t img;

// Locates the files (DOOMCODE.DMC on the firmware drive first, then on the
// data drive), checks both headers and the header CRC, loads the section
// table. Returns IMG_OK or an IMG_E_* code.
int img_init(void);

// Checks every section, map block and the DOOMX payload against their CRCs
// (after an install). Returns IMG_OK or IMG_E_CRC.
int img_verify(void);
// Called by img_verify every 64 KB checked with the bytes done and the total
// (a progress screen), NULL for none.
extern void (*img_verify_hook)(uint32_t done, uint32_t total);

// Blocking read of n bytes at logical offset loff.
void img_read(uint32_t loff, void *dst, uint32_t n);

// Queued read, NOR_PRIO_* priority. Returns the ticket of the last NOR
// request (requests of one priority complete in order), 0 when n is 0.
int img_read_async(uint32_t loff, void *dst, uint32_t n, int prio);

// Map table entry k (0 <= k < img.nmaps).
void img_mapent(int k, dmc_mapent_t *e);

// NOR address of logical offset loff and the bytes readable from there in
// one piece (*run).
uint32_t img_phys_run(uint32_t loff, uint32_t *run);

#ifdef DMC_HOST
// The bytes img_read gives, read from the host NOR file without counting a
// transaction or calling img_stat_hook (host self checks).
void img_peek(uint32_t loff, void *dst, uint32_t n);
#endif

#ifdef DMC_STATS
// Called for every NOR request with its logical offset, length and
// transaction count when statistics are enabled.
void img_stat_hook(uint32_t loff, uint32_t n, uint32_t tx);
#endif

// Provided by the application: unrecoverable data error.
void dmc_fatal(const char *msg);

#endif
