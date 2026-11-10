// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Writes to the FAT12 volumes of the NOR, next to the reader of fat12.h: 4 KB NOR sectors
// through a 4 KB buffer, directory and FAT12 entries, and a new file in free clusters
// (SAVE.DAT, which the BOOT overlay makes). The USB overlay links it for its quick format and
// the folder UI (port/usb/usb_vol.c), the host engine to make SAVE.DAT as the meter does.

#ifndef DMC_FAT_WRITE_H
#define DMC_FAT_WRITE_H

#include <stdint.h>

#include "extents.h"
#include "fat12.h"

#define FAT_SECTOR 4096u  // NOR erase sector

enum {
    FATW_OK = 0,
    FATW_E_NOR = -1,     // a NOR error, or the sector did not read back
    FATW_E_FS = -2,      // no FAT12 volume, or clusters that are not NOR sectors
    FATW_E_FULL = -3,    // not enough free clusters, runs or root entries
    FATW_E_EXISTS = -4,  // the root directory has an entry of that name
};

// Writes the 4 KB NOR sector at a: nothing when it already holds buf, no erase when it is
// erased, then a read back. FATW_OK, or FATW_E_NOR after a NOR error or a difference.
int fat_sector_write(uint32_t a, const uint8_t *buf);

// n bytes of src (zeros for NULL) to NOR address a, sector by sector through the 4 KB buf.
int fat_put(uint8_t *buf, uint32_t a, const void *src, uint32_t n);

// Directory entry e of name83 with attribute attr at cluster cl, size bytes, dated 2026-01-01.
void fat_dirent(uint8_t *e, const char *name83, int attr, uint32_t cl, uint32_t size);

// The boot sector of the volume v at base up to the volume label into h (54 B). Returns the
// bytes of one FAT, h[16] is the number of FATs.
uint32_t fat_fats(uint32_t base, const fat_vol_t *v, uint8_t *h);

// FAT12 entry c of the volume v at base set to value in every FAT.
int fat_set(uint8_t *buf, uint32_t base, const fat_vol_t *v, unsigned c, unsigned value);

// A new file name83 of n clusters in the root of the volume at base, whose clusters must be
// 4 KB NOR sectors: the first free run of n clusters, else the longest free runs, at most
// maxext (8 or less). The file clusters k with bit k of erase set are erased first (so a file
// of records reads as empty ones), then the chain goes into every FAT and the root entry
// (attribute 0x20, n clusters of bytes) is written last: a power loss before it leaves lost
// clusters only. Then the file is opened into f (NULL for none). FATW_OK or FATW_E_*.
int fat_create(uint32_t base, const char *name83, unsigned n, unsigned maxext, uint32_t erase,
               uint8_t *buf, dmc_file_t *f);

#endif
