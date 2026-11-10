// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek

#include "crc32.h"

static const uint32_t crc_nib[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
};

uint32_t crc32_update(uint32_t reg, const void *p, uint32_t n)
{
    const uint8_t *b = p;
    while (n--) {
        reg ^= *b++;
        reg = (reg >> 4) ^ crc_nib[reg & 15];
        reg = (reg >> 4) ^ crc_nib[reg & 15];
    }
    return reg;
}

uint32_t crc32_buf(const void *p, uint32_t n)
{
    return crc32_final(crc32_update(CRC32_INIT, p, n));
}
