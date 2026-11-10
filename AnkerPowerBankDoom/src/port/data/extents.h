// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Files on the two FAT12 volumes of the NOR: the 3 MB
// data drive at 0x100000 holds DOOM.DAT and SAVE.DAT, the firmware drive at 0 holds
// DOOMX.DAT and DOOMCODE.DMC. The firmware also opens a demo (DEMOn.LMP on the
// data drive) and its results file (DOOMRES.DAT on the firmware drive). Each
// file is a list of cluster runs (extents).

#ifndef DMC_EXTENTS_H
#define DMC_EXTENTS_H

#include <stdint.h>

#define DMC_VOL_MAIN 0x100000u  // data drive
#define DMC_VOL_UPDATE 0u       // firmware drive

enum { DMC_F_DOOM, DMC_F_DOOMX, DMC_F_SAVE, DMC_F_CODE, DMC_F_DEMO, DMC_F_RES, DMC_NFILES };

// Extents per file: DOOM.DAT 16, DOOMX.DAT 8, SAVE.DAT 8, DOOMCODE.DMC 4,
// demo 4, results 1 (one 4 KB cluster).
#define DMC_EXT_TOTAL 41

typedef struct {
    uint16_t lcl;  // first file cluster of the run
    uint16_t pcl;  // its FAT cluster number (2 = first data cluster)
    uint16_t n;    // clusters in the run
} dmc_extent_t;

typedef struct {
    uint32_t data;  // NOR address of cluster 2 of the volume
    uint32_t size;  // file size in bytes, 0 when the file is absent
    uint8_t cshift; // log2 of the cluster size
    uint8_t next;   // extents in use
    uint8_t max;    // extents available
    uint8_t first;  // index of the first extent in dmc_extents
} dmc_file_t;

extern dmc_file_t dmc_files[DMC_NFILES];
extern dmc_extent_t dmc_extents[DMC_EXT_TOTAL];

// Sets the extent capacity of every file and marks all files absent.
void ext_reset(void);

// Appends cluster pcl as file cluster lcl. Returns -1 when the file needs
// more extents than it has.
int ext_add(dmc_file_t *f, uint16_t lcl, uint16_t pcl);

// NOR address of byte off of the file. *run receives the bytes from there to
// the end of the extent (never past the end of the file).
uint32_t ext_phys(const dmc_file_t *f, uint32_t off, uint32_t *run);

// Reads n bytes at file offset off, split at extent boundaries. Returns -1
// when the range is outside the file.
int ext_read(int file, uint32_t off, void *dst, uint32_t n);

#endif
