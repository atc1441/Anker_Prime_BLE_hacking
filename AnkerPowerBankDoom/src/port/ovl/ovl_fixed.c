// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// DOOMCODE.DMC at a fixed NOR offset, one extent, for the overlay tests that have no FAT
// volume. The firmware links ovl_file.c instead.

#include "nor.h"
#include "ovl.h"

#ifndef OVL_FIXED_NOR
#define OVL_FIXED_NOR 0x0F0000u
#endif

uint32_t ovl_fixed_base = OVL_FIXED_NOR;

int32_t ovl_file_size(void) { return (int32_t)(NOR_SIZE - ovl_fixed_base); }

void ovl_file_read(uint32_t off, void *dst, uint32_t n) { nor_read(ovl_fixed_base + off, dst, n); }
