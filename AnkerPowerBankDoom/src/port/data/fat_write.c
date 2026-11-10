// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek

#include <string.h>

#include "fat_write.h"
#include "nor.h"

int fat_sector_write(uint32_t a, const uint8_t *buf)
{
    uint8_t t[256];
    int same = 1, blank = 1;
    for (uint32_t o = 0; o < FAT_SECTOR; o += sizeof(t)) {
        nor_read(a + o, t, sizeof(t));
        for (unsigned i = 0; i < sizeof(t); i++) {
            same &= t[i] == buf[o + i];
            blank &= t[i] == 0xFF;
        }
    }
    if (same)
        return FATW_OK;
    if ((!blank && nor_erase4k(a)) || nor_program(a, buf, FAT_SECTOR))
        return FATW_E_NOR;
    for (uint32_t o = 0; o < FAT_SECTOR; o += sizeof(t)) {
        nor_read(a + o, t, sizeof(t));
        if (memcmp(t, buf + o, sizeof(t)))
            return FATW_E_NOR;
    }
    return FATW_OK;
}

int fat_put(uint8_t *buf, uint32_t a, const void *src, uint32_t n)
{
    const uint8_t *p = src;
    while (n) {
        uint32_t s = a & ~(FAT_SECTOR - 1), o = a - s, k = FAT_SECTOR - o;
        if (k > n)
            k = n;
        nor_read(s, buf, FAT_SECTOR);
        if (p) {
            memcpy(buf + o, p, k);
            p += k;
        } else {
            memset(buf + o, 0, k);
        }
        if (fat_sector_write(s, buf))
            return FATW_E_NOR;
        a += k;
        n -= k;
    }
    return FATW_OK;
}

void fat_dirent(uint8_t *e, const char *name83, int attr, uint32_t cl, uint32_t size)
{
    memset(e, 0, 32);
    memcpy(e, name83, 11);
    e[11] = (uint8_t)attr;
    e[24] = 0x21;
    e[25] = 0x5C;
    e[26] = (uint8_t)cl;
    e[27] = (uint8_t)(cl >> 8);
    for (int i = 0; i < 4; i++)
        e[28 + i] = (uint8_t)(size >> 8 * i);
}

uint32_t fat_fats(uint32_t base, const fat_vol_t *v, uint8_t *h)
{
    nor_read(base, h, 54);
    return (v->root - v->fat) / h[16];
}

// The bytes of FAT12 entry c (at NOR address a) that lie in the sector of buf at s, set to
// value
static void set12(uint8_t *buf, uint32_t s, uint32_t a, unsigned c, unsigned value)
{
    uint32_t i = a - s;
    if (i < FAT_SECTOR)
        buf[i] = (uint8_t)(c & 1 ? (buf[i] & 0x0F) | (value << 4 & 0xF0) : value);
    if (++i < FAT_SECTOR)
        buf[i] = (uint8_t)(c & 1 ? value >> 4 : (buf[i] & 0xF0) | (value >> 8 & 0x0F));
}

int fat_set(uint8_t *buf, uint32_t base, const fat_vol_t *v, unsigned c, unsigned value)
{
    uint8_t e[2], h[54];
    uint32_t fb = fat_fats(base, v, h), a = v->fat + c + c / 2;
    nor_read(a, e, 2);
    set12(e, a, a, c, value);
    for (uint32_t f = 0; f < h[16]; f++) {
        if (fat_put(buf, a + f * fb, e, 2))
            return FATW_E_NOR;
    }
    return FATW_OK;
}

// ---- a new file

typedef struct {
    uint16_t c, n;  // first cluster, clusters
} frun_t;

static unsigned fat_get(const fat_vol_t *v, unsigned c)
{
    uint8_t e[2];
    nor_read(v->fat + c + c / 2, e, 2);
    return (c & 1 ? e[0] >> 4 | e[1] << 4 : e[0] | e[1] << 8) & 0xFFF;
}

// The run of len clusters at c among the max longest runs so far in r, longest first (the
// earlier of two of the same length first)
static void keep(frun_t *r, unsigned *nr, unsigned max, unsigned c, unsigned len)
{
    unsigned k = *nr < max ? (*nr)++ : max;
    for (; k && r[k - 1].n < len; k--) {
        if (k < max)
            r[k] = r[k - 1];
    }
    if (k < max) {
        r[k].c = (uint16_t)c;
        r[k].n = (uint16_t)len;
    }
}

int fat_create(uint32_t base, const char *name83, unsigned n, unsigned maxext, uint32_t erase,
               uint8_t *buf, dmc_file_t *f)
{
    fat_vol_t v;
    frun_t r[8];
    uint8_t e[32], h[54];
    uint32_t at = 0;
    unsigned nr = 0, got = 0, start = 0, len = 0, i, j, k;

    if (fat_mount(&v, base))
        return FATW_E_FS;
    if ((1u << v.cshift) != FAT_SECTOR || (v.data & (FAT_SECTOR - 1)) || !maxext || maxext > 8)
        return FATW_E_FS;
    // no file of that name, the first free root entry
    for (i = 0; i < v.nroot; i++) {
        nor_read(v.root + 32 * i, e, 12);
        if (e[0] && e[0] != 0xE5 && !(e[11] & 0x08) && !memcmp(e, name83, 11))
            return FATW_E_EXISTS;
        if (!at && (!e[0] || e[0] == 0xE5))
            at = v.root + 32 * i;
        if (!e[0])
            break;
    }
    if (!at)
        return FATW_E_FULL;
    // the first free run that holds the file, else the longest runs
    for (i = 2; i <= v.nclus + 2u; i++) {
        if (i < v.nclus + 2u && !fat_get(&v, i)) {
            if (!len++)
                start = i;
            continue;
        }
        if (len >= n) {
            r[0].c = (uint16_t)start;
            r[0].n = (uint16_t)n;
            nr = 1;
            got = n;
            break;
        }
        if (len)
            keep(r, &nr, maxext, start, len);
        len = 0;
    }
    if (got < n) {
        for (k = 0; k < nr && got < n; k++)
            got += r[k].n;
        if (got < n)
            return FATW_E_FULL;
        nr = k;
        r[nr - 1].n = (uint16_t)(r[nr - 1].n - (got - n));
        // in the order of the volume
        for (i = 1; i < nr; i++) {
            frun_t t = r[i];
            for (j = i; j && r[j - 1].c > t.c; j--)
                r[j] = r[j - 1];
            r[j] = t;
        }
    }
    // the clusters are not referenced yet
    for (i = k = 0; i < nr; i++) {
        for (j = 0; j < r[i].n; j++, k++) {
            if ((erase >> k & 1) && nor_erase4k(v.data + ((uint32_t)(r[i].c + j - 2) << 12)))
                return FATW_E_NOR;
        }
    }
    // the chain in every FAT, sector by sector
    uint32_t fb = fat_fats(base, &v, h);
    for (uint32_t s = v.fat & ~(FAT_SECTOR - 1); s < v.fat + h[16] * fb; s += FAT_SECTOR) {
        nor_read(s, buf, FAT_SECTOR);
        for (i = 0; i < nr; i++) {
            for (j = 0; j < r[i].n; j++) {
                unsigned c = r[i].c + j;
                unsigned next = j + 1 < r[i].n ? c + 1 : i + 1 < nr ? r[i + 1].c : 0xFFF;
                for (uint32_t t = 0; t < h[16]; t++)
                    set12(buf, s, v.fat + t * fb + c + c / 2, c, next);
            }
        }
        if (fat_sector_write(s, buf))
            return FATW_E_NOR;
    }
    fat_dirent(e, name83, 0x20, r[0].c, n * FAT_SECTOR);
    if (fat_put(buf, at, e, 32))
        return FATW_E_NOR;
    if (f && fat_open(&v, name83, f))
        return FATW_E_FS;
    return FATW_OK;
}
