// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// CRC-32/IEEE (reflected, poly 0xEDB88320, init and xorout 0xFFFFFFFF) with a
// 16 entry nibble table.

#ifndef DMC_CRC32_H
#define DMC_CRC32_H

#include <stdint.h>

#define CRC32_INIT 0xFFFFFFFFu

// Updates the running register (start with CRC32_INIT).
uint32_t crc32_update(uint32_t reg, const void *p, uint32_t n);

static inline uint32_t crc32_final(uint32_t reg)
{
    return reg ^ 0xFFFFFFFFu;
}

uint32_t crc32_buf(const void *p, uint32_t n);

#endif
