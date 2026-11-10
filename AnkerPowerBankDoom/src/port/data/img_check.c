// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The checks of the logical image (img.h): img_init finds and checks the files, img_verify
// their CRCs. Apart from img.c because only the start runs them: the firmware keeps this file
// in its BOOT overlay, so it has no data of its own.

#include <string.h>

#include "crc32.h"
#include "fat12.h"
#include "img.h"
#include "info.h"

// Progress of img_verify
typedef struct {
    uint32_t done, total;
} progress_t;

// k more bytes checked
static void verified(progress_t *p, uint32_t k)
{
    uint32_t was = p->done;
    p->done += k;
    if (img_verify_hook && (p->done >> 16) != (was >> 16))
        img_verify_hook(p->done, p->total);
}

static int fat_err(int e)
{
    return e == FAT_E_NOFILE ? IMG_E_NODATA : e == FAT_E_FRAG ? IMG_E_FRAG : IMG_E_FS;
}

// Optional file: absent is fine, a damaged or fragmented one is not.
static int open_opt(const fat_vol_t *v, const char *name, int file)
{
    int e = fat_open(v, name, &dmc_files[file]);
    return e == FAT_E_NOFILE ? IMG_OK : e ? fat_err(e) : IMG_OK;
}

// CRC of n bytes of a file through 256 byte chunks.
static uint32_t file_crc(progress_t *p, int file, uint32_t off, uint32_t n)
{
    uint8_t buf[256];
    uint32_t reg = CRC32_INIT;
    while (n) {
        uint32_t k = n < sizeof(buf) ? n : sizeof(buf);
        ext_read(file, off, buf, k);
        reg = crc32_update(reg, buf, k);
        verified(p, k);
        off += k;
        n -= k;
    }
    return crc32_final(reg);
}

static uint32_t img_crc(progress_t *p, uint32_t loff, uint32_t n)
{
    uint8_t buf[256];
    uint32_t reg = CRC32_INIT;
    while (n) {
        uint32_t k = n < sizeof(buf) ? n : sizeof(buf);
        img_read(loff, buf, k);
        reg = crc32_update(reg, buf, k);
        verified(p, k);
        loff += k;
        n -= k;
    }
    return crc32_final(reg);
}

int img_init(void)
{
    fat_vol_t vm, vu;
    dmc_header_t h;
    int e;

    memset(&img, 0, sizeof(img));
    ext_reset();
    if (fat_mount(&vm, DMC_VOL_MAIN))
        return IMG_E_FS;
    if ((e = fat_open(&vm, "DOOM    DAT", &dmc_files[DMC_F_DOOM])))
        return fat_err(e);
    // SAVE.DAT, made by the meter: one in too many pieces or damaged only turns the saves off
    // (its size stays 0)
    fat_open(&vm, "SAVE    DAT", &dmc_files[DMC_F_SAVE]);
    // The firmware drive is optional for an image without DOOMX.DAT.
    // DOOMCODE.DMC is copied there with DOOM.BIN, a copy
    // on the data drive is only the fallback.
    if (!fat_mount(&vu, DMC_VOL_UPDATE)) {
        if ((e = open_opt(&vu, "DOOMX   DAT", DMC_F_DOOMX)))
            return e;
        if ((e = open_opt(&vu, "DOOMCODEDMC", DMC_F_CODE)))
            return e;
    }
    if (!dmc_files[DMC_F_CODE].size && (e = open_opt(&vm, "DOOMCODEDMC", DMC_F_CODE)))
        return e;

    const dmc_file_t *fd = &dmc_files[DMC_F_DOOM];
    if (fd->size < DMC_HEADER_SIZE)
        return IMG_E_HEADER;
    ext_read(DMC_F_DOOM, 0, &h, sizeof(h));
    if (memcmp(h.magic, DMC_MAGIC, 8) || h.fmt_major != DMC_FMT_MAJOR || h.split > h.image_size
        || h.split > fd->size || h.split < DMC_HEADER_SIZE || h.nsect > DMC_MAX_SECTS
        || h.nmaps > DMC_MAX_MAPS)
        return IMG_E_HEADER;

    // header CRC with the CRC field read as zero
    uint8_t buf[256];
    uint32_t reg = CRC32_INIT;
    for (uint32_t o = 0; o < DMC_HEADER_SIZE; o += sizeof(buf)) {
        ext_read(DMC_F_DOOM, o, buf, sizeof(buf));
        if (!o)
            memset(buf + offsetof(dmc_header_t, header_crc), 0, 4);
        reg = crc32_update(reg, buf, sizeof(buf));
    }
    if (crc32_final(reg) != h.header_crc)
        return IMG_E_CRC;
    // a DOOM.DAT of another WAD: the firmware's tables (gen/info.h) are made from this one
    if ((h.wad_sha256[0] | h.wad_sha256[1] << 8 | h.wad_sha256[2] << 16 | (uint32_t)h.wad_sha256[3] << 24)
        != DMC_WAD_ID)
        return IMG_E_HEADER;

    img.size = h.image_size;
    img.split = h.split;
    img.header_crc = h.header_crc;
    img.nmaps = h.nmaps;
    img.nsect = h.nsect;

    if (h.split < h.image_size) {
        dmc_doomx_t x;
        const dmc_file_t *fx = &dmc_files[DMC_F_DOOMX];
        if (fx->size < DMC_DOOMX_HDR)
            return IMG_E_DOOMX;
        ext_read(DMC_F_DOOMX, 0, &x, sizeof(x));
        if (memcmp(x.magic, DMC_MAGIC_X, 8) || x.fmt_major != DMC_FMT_MAJOR
            || memcmp(x.image_uuid, h.image_uuid, 16) || x.logical_start != h.split
            || x.length != h.image_size - h.split || fx->size - DMC_DOOMX_HDR < x.length)
            return IMG_E_DOOMX;
    }

    // every global section is required, a missing one would leave the
    // directory reads at offset 0 (the header)
    uint32_t seen = 0;
    for (unsigned k = 0; k < h.nsect; k++) {
        dmc_sect_t s;
        img_read(DMC_SECT_TABLE_OFF + 16 * k, &s, sizeof(s));
        if (s.off > img.size || s.size > img.size - s.off)
            return IMG_E_HEADER;
        if (s.id && s.id < DMC_SECT_MAX) {
            img.sect[s.id].off = s.off;
            img.sect[s.id].size = s.size;
            img.sect[s.id].flags = s.flags;
            seen |= 1u << s.id;
        }
    }
    if (seen != (1u << DMC_SECT_MAX) - 2)
        return IMG_E_HEADER;
    return IMG_OK;
}

int img_verify(void)
{
    progress_t p = {0, img.size - img.split};
    for (unsigned k = 0; k < img.nsect; k++) {
        dmc_sect_t s;
        img_read(DMC_SECT_TABLE_OFF + 16 * k, &s, sizeof(s));
        p.total += s.size;
    }
    for (int k = 0; k < img.nmaps; k++) {
        dmc_mapent_t e;
        img_mapent(k, &e);
        p.total += e.size;
    }
    for (unsigned k = 0; k < img.nsect; k++) {
        dmc_sect_t s;
        img_read(DMC_SECT_TABLE_OFF + 16 * k, &s, sizeof(s));
        if (img_crc(&p, s.off, s.size) != s.crc)
            return IMG_E_CRC;
    }
    for (int k = 0; k < img.nmaps; k++) {
        dmc_mapent_t e;
        img_mapent(k, &e);
        if (e.off > img.size || e.size > img.size - e.off || img_crc(&p, e.off, e.size) != e.crc)
            return IMG_E_CRC;
    }
    if (img.split < img.size) {
        dmc_doomx_t x;
        ext_read(DMC_F_DOOMX, 0, &x, sizeof(x));
        if (file_crc(&p, DMC_F_DOOMX, DMC_DOOMX_HDR, x.length) != x.payload_crc)
            return IMG_E_CRC;
    }
    return IMG_OK;
}

