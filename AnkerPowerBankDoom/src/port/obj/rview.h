// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The renderer's view of sectors and things (port/obj/rview.c).

#ifndef DMC_RVIEW_H
#define DMC_RVIEW_H

#include "doomtype.h"
#include "m_fixed.h"
#include "tables.h"

fixed_t rv_floorh(int sec);
fixed_t rv_ceilh(int sec);
int rv_floorpic(int sec);
int rv_ceilpic(int sec);
int rv_light(int sec);

// Defined as rsector_t in r_bsp.h.
struct rsector_s;

// Heights, flats and light of a sector from one record read.
void rv_sector(int sec, struct rsector_s *s);

typedef struct
{
    fixed_t x, y, z;
    angle_t angle;
    uint32_t flags;
    int sprite, frame;
    uint16_t handle;
} rthing_t;

typedef void (*rthing_fn)(const rthing_t *t, void *ctx);

// Things in sector sec in vanilla thinglist order (most recently linked
// first), including static items and corpses.
void rv_sector_things(int sec, rthing_fn fn, void *ctx);

// The same view again during the same frame, 0 if the handle is no
// longer valid.
int rv_thing(uint16_t handle, rthing_t *out);

#endif
