// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek

#include <string.h>

#include "fat12.h"
#include "nor.h"

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)rd16(p) | (uint32_t)rd16(p + 2) << 16;
}

static int log2_exact(uint32_t v)
{
    int s = 0;
    if (!v || (v & (v - 1)))
        return -1;
    while ((1u << s) != v)
        s++;
    return s;
}

int fat_mount(fat_vol_t *v, uint32_t base)
{
    uint8_t b[36];
    nor_read(base, b, sizeof(b));
    int bshift = log2_exact(rd16(b + 11));
    int sshift = log2_exact(b[13]);
    uint32_t rsv = rd16(b + 14), nfat = b[16], nroot = rd16(b + 17);
    uint32_t total = rd16(b + 19), fatsz = rd16(b + 22);
    if (!total)
        total = rd32(b + 32);
    if (bshift < 9 || bshift > 12 || sshift < 0 || bshift + sshift > 15 || !rsv || !nfat
        || nfat > 2 || !nroot || !fatsz)
        return FAT_E_BPB;
    // the volume ends inside its NOR region: the firmware drive before the
    // data drive, the data drive before the end of the chip
    uint32_t limit = base < DMC_VOL_MAIN ? DMC_VOL_MAIN : NOR_SIZE;
    if (base >= limit || total > (limit - base) >> bshift)
        return FAT_E_BPB;
    uint32_t rootsec = rsv + nfat * fatsz;
    uint32_t datasec = rootsec + ((nroot * 32 + (1u << bshift) - 1) >> bshift);
    if (total <= datasec)
        return FAT_E_BPB;
    uint32_t nclus = (total - datasec) >> sshift;
    // FAT12 only, and the FAT must hold an entry for every cluster
    if (nclus >= 4085 || (nclus + 2) * 3 / 2 > fatsz << bshift)
        return FAT_E_BPB;
    v->fat = base + (rsv << bshift);
    v->root = base + (rootsec << bshift);
    v->data = base + (datasec << bshift);
    v->nroot = (uint16_t)nroot;
    v->nclus = (uint16_t)nclus;
    v->cshift = (uint8_t)(bshift + sshift);
    return FAT_OK;
}

// FAT12 entry of cluster c through a small window on the stack.
typedef struct {
    uint32_t off;
    uint8_t b[48];
} fat_win_t;

static unsigned fat_next(const fat_vol_t *v, fat_win_t *w, unsigned c)
{
    uint32_t o = c + (c >> 1);
    if (o < w->off || o + 1 >= w->off + sizeof(w->b)) {
        w->off = o;
        nor_read(v->fat + o, w->b, sizeof(w->b));
    }
    unsigned e = rd16(w->b + (o - w->off));
    return c & 1 ? e >> 4 : e & 0xFFF;
}

// Root directory entry named name83 into e and its NOR address into *addr.
// Directories only with dirs set, LFN, deleted and volume label entries
// never.
static int find(const fat_vol_t *v, const char *name83, int dirs, uint8_t *e, uint32_t *addr)
{
    uint8_t d[32 * 4];

    for (unsigned i = 0; i < v->nroot; i += 4) {
        nor_read(v->root + i * 32, d, sizeof(d));
        for (unsigned k = 0; k < 4; k++) {
            const uint8_t *p = d + 32 * k;
            if (i + k >= v->nroot || !p[0])
                return FAT_E_NOFILE;
            if (p[0] == 0xE5 || p[11] == 0x0F || (p[11] & 0x08) || !(p[11] & 0x10) != !dirs)
                continue;
            if (!memcmp(p, name83, 11)) {
                memcpy(e, p, 32);
                *addr = v->root + (i + k) * 32;
                return FAT_OK;
            }
        }
    }
    return FAT_E_NOFILE;
}

int fat_dir(const fat_vol_t *v, const char *name83, uint32_t *addr)
{
    uint8_t e[32];
    return find(v, name83, 1, e, addr);
}

int fat_open(const fat_vol_t *v, const char *name83, dmc_file_t *f)
{
    uint8_t e[32];
    uint32_t addr;

    if (find(v, name83, 0, e, &addr))
        return FAT_E_NOFILE;
    uint32_t size = rd32(e + 28);
    unsigned first = rd16(e + 26);

    f->data = v->data;
    f->cshift = v->cshift;
    f->next = 0;
    f->size = 0;
    uint32_t count = (size + (1u << v->cshift) - 1) >> v->cshift;
    if (!count)
        return first ? FAT_E_CHAIN : FAT_OK;

    fat_win_t w;
    w.off = 0xFFFFFFFFu;
    unsigned c = first;
    for (uint32_t k = 0; k < count; k++) {
        if (c < 2 || c >= v->nclus + 2u)
            return FAT_E_CHAIN;
        if (ext_add(f, (uint16_t)k, (uint16_t)c))
            return FAT_E_FRAG;
        c = fat_next(v, &w, c);
    }
    if (c < 0xFF8)
        return FAT_E_CHAIN;
    f->size = size;
    return FAT_OK;
}
