// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// FAT12 locator, read only (fat_write.h writes): BPB, root directory 8.3
// names and FAT chains to extents.

#ifndef DMC_FAT12_H
#define DMC_FAT12_H

#include <stdint.h>

#include "extents.h"

enum {
    FAT_OK = 0,
    FAT_E_BPB = -1,     // no FAT12 volume at that address
    FAT_E_NOFILE = -2,  // name not in the root directory
    FAT_E_FRAG = -3,    // more extents than the file has room for
    FAT_E_CHAIN = -4,   // chain does not match the file size
};

typedef struct {
    uint32_t fat;    // NOR address of the first FAT
    uint32_t root;   // NOR address of the root directory
    uint32_t data;   // NOR address of cluster 2
    uint16_t nroot;  // root directory entries
    uint16_t nclus;  // data clusters
    uint8_t cshift;  // log2 of the cluster size
} fat_vol_t;

int fat_mount(fat_vol_t *v, uint32_t base);

// name83 is the 11 character directory form, for example "DOOM    DAT". LFN,
// deleted and directory entries are skipped.
int fat_open(const fat_vol_t *v, const char *name83, dmc_file_t *f);

// Directories of the root (UI and UI_OFF, dmc_boot.c and the USB overlay):
// NOR address of the entry of directory name83, FAT_OK or FAT_E_NOFILE.
int fat_dir(const fat_vol_t *v, const char *name83, uint32_t *addr);

#endif
