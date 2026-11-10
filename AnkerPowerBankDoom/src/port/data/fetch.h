// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Graphics, sound and text data of the image.
// Directories go through the record cache, column and flat data are read
// straight into caller buffers with nor_read_async, so the device can queue
// them behind record misses. The ticket a request returns must be waited for
// before the buffer is read: when the buffer already holds the data it is
// the ticket of the read still filling it, or 0 when that read is done.
// Directory ids beyond their section end in dmc_fatal.

#ifndef DMC_FETCH_H
#define DMC_FETCH_H

#include <stdint.h>

#include "dmcdat.h"

#define FETCH_COLBUF 256  // a stored column or post list never exceeds 255 B

// ---- textures (global ids)

int tex_count(void);
void tex_entry(int tex, dmc_texent_t *e);

// Base level L of the stored LOD.
static inline int tex_base(const dmc_texent_t *e)
{
    return (e->flags & DMC_TF_BASE_MASK) >> DMC_TF_BASE_SHIFT;
}

// Offset of level `level` from the base level in the texture's data: the
// stored levels follow each other from the base level on.
static inline uint32_t tex_level_off(unsigned log2_w, unsigned log2_sh, int base, int level)
{
    uint32_t off = 0;
    for (int k = base; k < level; k++)
        off += 1u << (log2_w - k + log2_sh - k);
    return off;
}

// Rows y0..y1 of opaque column col at level `level` (clamped to the stored
// levels) into buf[y0..y1]. Row and column numbers are in level units
// (column col of the texture is stored column col >> level). y0 > y1 wraps
// around the stored height. The previous request into the same buffer is
// reused when it covers the rows.
int tex_col_request(int tex, int col, int level, int y0, int y1, uint8_t *buf);
// The same with the TEXDIR entry the caller already read.
int tex_col_fetch(int tex, const dmc_texent_t *e, int col, int level, int y0, int y1, uint8_t *buf);

// Posts of masked column col (texture units) into buf. Returns the ticket,
// *len receives the byte count.
int tex_masked_request(int tex, int col, uint8_t *buf, int *len);

// ---- patches: sprites first, then UI pictures

// Logical offset of a patch (its header).
uint32_t patch_loff(int patch);
void patch_header(int patch, dmc_patch_t *h);

// Posts of original column col of a patch into buf (stored column
// col >> xshift). *len receives the byte count.
int spr_col_request(int patch, int col, uint8_t *buf, int *len);

// Number of sprites (dense ids) in SPRDEF.
int spr_count(void);

// Sprite frame record, -1 when the frame is beyond the sprite's frames.
int spr_frame(int sprite, int frame, dmc_sprframe_t *f);

// UIDIR: patch id of a dmc_ui picture, -1 when the pack does not have it.
int ui_patch(int ui);

// ---- flats (global ids), K slots of 32x32

#define FLAT_SLOTS_MAX 4

int flat_count(void);
// Firmware builds (DMC_FW) keep the slots in the RAM after the record cache
// of the map's plan: flat_place moves them to base, which holds
// k x DMC_FLAT_SIZE bytes for the k of flat_set_slots (NULL only with k 0),
// and empties them. Other builds use a static array.
void flat_place(void *base);
void flat_set_slots(int k);
// Texels of a flat, loaded into the least recently used slot when absent
// (prefetched slots still loading are kept). The texels stay valid until
// the next flat_get. NULL with K = 0: draw flat_avg instead.
const uint8_t *flat_get(int flat);
// Queues a load without waiting, into the least recently used slot that is
// neither loading nor the one flat_get returned last. Does nothing with
// fewer than 2 slots or when no slot qualifies.
void flat_prefetch(int flat);
// Average colour of a flat (solid floors).
uint8_t flat_avg(int flat);
// Bytes off..off+n-1 of the 64x64 finale flat (row by row) into buf.
void flat_finale(uint32_t off, uint8_t *buf, uint32_t n);
// Forgets every slot (level load).
void flat_reset(void);

// ---- other sections

void pal_read(int pal, uint16_t *rgb565);
const char *str_get(int id, char *buf, int max);
// Up to n bytes of string id from byte off on into buf, without a NUL. Returns how many
// there are before the string ends, n when it goes on.
int str_part(int id, uint32_t off, char *buf, int n);
int snd_get(int sfx, uint32_t *loff, uint16_t *len);
int anim_count(void);
void anim_get(int k, dmc_anim_t *a);
// Texture and flat cycles of ANIMS for the renderer (the image directory's animation ranges).
// anim_ranges() keeps the frames of each one but the switches, in ANIMS
// order: -1 when one is empty, too long or too many, else the count. anim_offset(i, o) sets
// the frame offset of the cycle i of those (P_UpdateSpecials), anim_frame(n, flat) gives the
// frame shown for texture n (flat 0) or flat n (flat 1): vanilla's texturetranslation[n] and
// flattranslation[n].
int anim_ranges(void);
void anim_offset(int i, int o);
int anim_frame(int n, int flat);
int demo_get(int k, uint32_t *loff, uint32_t *size);

// Demos outside the image (.lmp files), provided by the application: a file
// of the data drive on the meter, a host file. dmc_demo_open returns 0
// and the size, or -1. Reads stay within the size.
int dmc_demo_open(const char *name, uint32_t *size);
void dmc_demo_read(uint32_t off, void *dst, uint32_t n);

void fetch_wait(int ticket);

// Forgets the previous-column state of every buffer.
void fetch_reset(void);

#ifdef DMC_HOST
// Host self check of the renderer's column contexts (--column-check): the
// logical offset tex_col_fetch reads column col of level `level` from, and
// the offset and byte count of the posts tex_masked_request or
// spr_col_request read for column col. The records are read with img_peek,
// so neither the record cache nor the counters see them.
uint32_t tex_col_peek(int tex, int col, int level);
uint32_t tex_masked_peek(int tex, int col, uint32_t *len);
uint32_t spr_col_peek(int patch, int col, uint32_t *len);
#endif

#endif
