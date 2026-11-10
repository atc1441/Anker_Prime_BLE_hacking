// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Writes to the data drive (usb.h): the quick format and the folder UI. Every write goes
// through the sector writer of the data layer (port/data/fat_write.c) to a sector of the
// volume, located with its FAT12 reader (port/data/fat12.c), whose checks keep the volume
// inside the NOR. The quick format also erases the clusters with the NOR driver.

#include <string.h>

#include "fat12.h"
#include "fat_write.h"
#include "nor.h"
#include "usb.h"

#define UI_ON  "UI         "
#define UI_OFF "UI_OFF     "

// Boot sector with the geometry of the meter's data drive, as tests/host/mknor.py writes it:
// 4 KB sectors, one per cluster, 1 reserved sector, 2 FATs of 1 sector, 512 root entries, 768
// sectors, media 0xF8, then an extended boot record (serial "DMC1", no label)
static const uint8_t bpb[62] = {
    0xEB, 0x3C, 0x90, 'D', 'M', 'C', 'D', 'O', 'O', 'M', ' ', 0x00, 0x10, 1, 1, 0, 2, 0x00, 0x02,
    0x00, 0x03, 0xF8, 1, 0, 32, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x80, 0, 0x29, 0x31, 0x43, 0x4D,
    0x44, 'N', 'O', ' ', 'N', 'A', 'M', 'E', ' ', ' ', ' ', ' ', 'F', 'A', 'T', '1', '2', ' ', ' ', ' ',
};

// The cluster of a new folder at cl: "." and "..", the rest zero
static int folder(uint8_t *buf, const fat_vol_t *v, uint32_t cl)
{
    uint8_t e[64];
    uint32_t a = v->data + ((cl - 2) << v->cshift);
    fat_dirent(e, ".          ", 0x10, cl, 0);
    fat_dirent(e + 32, "..         ", 0x10, 0, 0);
    if (fat_put(buf, a, NULL, 1u << v->cshift))
        return VOL_E_NOR;
    return fat_put(buf, a, e, 64);
}

int vol_format(uint8_t *buf, void (*progress)(uint32_t done, uint32_t total))
{
    fat_vol_t v;
    uint32_t fb, a, d, end, total;
    uint8_t e[64], h[54];
    if (fat_mount(&v, USB_VOL) || !v.nclus) {
        memset(buf, 0, USB_BLOCK);
        memcpy(buf, bpb, sizeof(bpb));
        buf[510] = 0x55;
        buf[511] = 0xAA;
        if (fat_sector_write(USB_VOL, buf) || fat_mount(&v, USB_VOL))
            return VOL_E_NOR;
    }
    fb = fat_fats(USB_VOL, &v, h);
    // FATs and root directory cleared sector by sector, then the whole 4 KB sectors of the
    // clusters erased, in 64 KB blocks where a block is whole, so that the copy to the drive
    // programs them without an erase. fat_mount keeps the clusters inside the volume. The
    // progress counts the bytes of both.
    d = (v.data + USB_BLOCK - 1) & ~(USB_BLOCK - 1);
    end = (v.data + ((uint32_t)v.nclus << v.cshift)) & ~(USB_BLOCK - 1);
    if (d > end)
        d = end;
    total = v.data - v.fat + end - d;
    for (a = v.fat; a < v.data;) {
        uint32_t k = USB_BLOCK - (a & (USB_BLOCK - 1));
        if (k > v.data - a)
            k = v.data - a;
        if (fat_put(buf, a, NULL, k))
            return VOL_E_NOR;
        a += k;
        if (progress)
            progress(a - v.fat, total);
    }
    for (a = d; a < end;) {
        int big = !(a & 0xFFFFu) && end - a >= 0x10000u;
        if (nor_erase(a, big ? NOR_ERASE_64K : NOR_ERASE_4K))
            return VOL_E_NOR;
        a += big ? 0x10000u : USB_BLOCK;
        if (progress)
            progress(v.data - v.fat + a - d, total);
    }
    // clusters 0 and 1 reserved (the first with the media byte), cluster 2 the folder UI
    e[0] = h[21];
    e[1] = e[2] = e[3] = 0xFF;
    e[4] = 0x0F;
    for (uint32_t f = 0; f < h[16]; f++) {
        if (fat_put(buf, v.fat + f * fb, e, 5))
            return VOL_E_NOR;
    }
    // the volume label of the extended boot record stays in the root directory too
    int lab = h[38] == 0x29 && memcmp(h + 43, "NO NAME    ", 11);
    memset(e, 0, sizeof(e));
    if (lab) {
        memcpy(e, h + 43, 11);
        e[11] = 0x08;
    }
    fat_dirent(e + 32 * lab, UI_ON, 0x10, 2, 0);
    if (fat_put(buf, v.root, e, sizeof(e)))
        return VOL_E_NOR;
    return folder(buf, &v, 2);
}

int vol_ui(uint8_t *buf)
{
    fat_vol_t v;
    uint32_t a, i, c;
    uint8_t e[32];
    if (fat_mount(&v, USB_VOL))
        return VOL_E_FS;
    if (!fat_dir(&v, UI_ON, &a))
        return VOL_OK;
    if (!fat_dir(&v, UI_OFF, &a))
        return fat_put(buf, a, UI_ON, 11);
    // a free root entry and a free cluster
    for (i = 0; i < v.nroot; i++) {
        nor_read(v.root + 32 * i, e, 1);
        if (!e[0] || e[0] == 0xE5)
            break;
    }
    for (c = 2; c < v.nclus + 2u; c++) {
        nor_read(v.fat + c + c / 2, e, 2);
        if (!((c & 1 ? e[0] >> 4 | e[1] << 4 : e[0] | e[1] << 8) & 0xFFF))
            break;
    }
    if (i == v.nroot || c == v.nclus + 2u)
        return VOL_E_FULL;
    // its FAT entry (free until now): end of chain
    if (folder(buf, &v, c) || fat_set(buf, USB_VOL, &v, c, 0xFFF))
        return VOL_E_NOR;
    fat_dirent(e, UI_ON, 0x10, c, 0);
    return fat_put(buf, v.root + 32 * i, e, 32);
}
