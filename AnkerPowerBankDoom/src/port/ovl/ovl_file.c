// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// DOOMCODE.DMC through the data layer: img_init() finds it on the FAT12 volumes and opens it
// as dmc_files[DMC_F_CODE] (fat_open), reads follow its FAT extents. The firmware links this
// file, the tests that have no FAT volume link ovl_fixed.c instead.

#include "extents.h"
#include "ovl.h"

int32_t ovl_file_size(void)
{
    uint32_t size = dmc_files[DMC_F_CODE].size;
    return size ? (int32_t)size : -1;
}

void ovl_file_read(uint32_t off, void *dst, uint32_t n) { ext_read(DMC_F_CODE, off, dst, n); }
