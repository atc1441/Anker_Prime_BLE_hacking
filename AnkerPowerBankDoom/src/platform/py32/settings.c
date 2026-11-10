// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 VapeDoom port
//
// Settings page. The flash of this part is written with a sequence this port
// has not verified on hardware, and a wrong one on the last page of a chip
// that also holds the program is not a mistake worth making for a menu
// preference. So settings live in RAM: they hold for the session and are
// gone after a power cycle. settings_load tells the caller that honestly by
// failing until something has been saved.

#include <stdint.h>

#include "platform.h"

static uint8_t shadow[SETTINGS_MAX];
static uint32_t shadow_size;
static uint16_t shadow_ver;
static uint8_t have;

int settings_load(void *dst, uint32_t size, uint16_t version)
{
    if (!have || size != shadow_size || version != shadow_ver || size > SETTINGS_MAX)
        return -1;
    for (uint32_t i = 0; i < size; i++)
        ((uint8_t *)dst)[i] = shadow[i];
    return 0;
}

int settings_save(const void *src, uint32_t size, uint16_t version)
{
    if (size > SETTINGS_MAX)
        return -1;
    for (uint32_t i = 0; i < size; i++)
        shadow[i] = ((const uint8_t *)src)[i];
    shadow_size = size;
    shadow_ver = version;
    have = 1;
    return 0;
}
