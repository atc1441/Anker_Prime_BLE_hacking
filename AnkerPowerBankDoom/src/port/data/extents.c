// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek

#include "extents.h"
#include "nor.h"

dmc_file_t dmc_files[DMC_NFILES];
dmc_extent_t dmc_extents[DMC_EXT_TOTAL];

static const uint8_t ext_cap[DMC_NFILES] = { 16, 8, 8, 4, 4, 1 };

void ext_reset(void)
{
    unsigned first = 0;
    for (int i = 0; i < DMC_NFILES; i++) {
        dmc_file_t *f = &dmc_files[i];
        f->data = 0;
        f->size = 0;
        f->cshift = 12;
        f->next = 0;
        f->max = ext_cap[i];
        f->first = (uint8_t)first;
        first += ext_cap[i];
    }
}

int ext_add(dmc_file_t *f, uint16_t lcl, uint16_t pcl)
{
    dmc_extent_t *e = &dmc_extents[f->first];
    if (f->next) {
        dmc_extent_t *last = &e[f->next - 1];
        if (last->pcl + last->n == pcl && last->lcl + last->n == lcl) {
            last->n++;
            return 0;
        }
    }
    if (f->next == f->max)
        return -1;
    e += f->next++;
    e->lcl = lcl;
    e->pcl = pcl;
    e->n = 1;
    return 0;
}

uint32_t ext_phys(const dmc_file_t *f, uint32_t off, uint32_t *run)
{
    const dmc_extent_t *e = &dmc_extents[f->first];
    uint32_t cl = off >> f->cshift;
    int lo = 0, hi = f->next - 1;
    // last extent with lcl <= cl
    while (lo < hi) {
        int mid = (lo + hi + 1) >> 1;
        if (e[mid].lcl <= cl)
            lo = mid;
        else
            hi = mid - 1;
    }
    e += lo;
    uint32_t in = off - ((uint32_t)e->lcl << f->cshift);
    uint32_t left = ((uint32_t)e->n << f->cshift) - in;
    if (left > f->size - off)
        left = f->size - off;
    *run = left;
    return f->data + ((uint32_t)(e->pcl - 2) << f->cshift) + in;
}

int ext_read(int file, uint32_t off, void *dst, uint32_t n)
{
    const dmc_file_t *f = &dmc_files[file];
    uint8_t *d = dst;
    if (off > f->size || n > f->size - off)
        return -1;
    while (n) {
        uint32_t run;
        uint32_t phys = ext_phys(f, off, &run);
        if (run > n)
            run = n;
        nor_read(phys, d, run);
        d += run;
        off += run;
        n -= run;
    }
    return 0;
}
