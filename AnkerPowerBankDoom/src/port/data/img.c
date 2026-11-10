// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek

#include <string.h>

#include "crc32.h"
#include "fat12.h"
#include "img.h"
#include "nor.h"
#ifdef DMC_HOST
#include "nor_file.h"
#endif

img_dir_t img;
void (*img_verify_hook)(uint32_t done, uint32_t total);

void img_mapent(int k, dmc_mapent_t *e)
{
    img_read(DMC_MAP_TABLE_OFF + 32u * (unsigned)k, e, sizeof(*e));
}

uint32_t img_phys_run(uint32_t loff, uint32_t *run)
{
    uint32_t phys;
    if (loff < img.split) {
        phys = ext_phys(&dmc_files[DMC_F_DOOM], loff, run);
        if (*run > img.split - loff)
            *run = img.split - loff;
    } else {
        phys = ext_phys(&dmc_files[DMC_F_DOOMX], loff - img.split + DMC_DOOMX_HDR, run);
    }
    return phys;
}

void img_read(uint32_t loff, void *dst, uint32_t n)
{
    uint8_t *d = dst;
    if (loff > img.size || n > img.size - loff)
        dmc_fatal("image read out of range");
    while (n) {
        uint32_t run;
        uint32_t phys = img_phys_run(loff, &run);
        if (run > n)
            run = n;
#ifdef DMC_STATS
        img_stat_hook(loff, run, 1);
#endif
        nor_read(phys, d, run);
        d += run;
        loff += run;
        n -= run;
    }
}

#ifdef DMC_HOST
void img_peek(uint32_t loff, void *dst, uint32_t n)
{
    uint8_t *d = dst;
    if (loff > img.size || n > img.size - loff)
        dmc_fatal("image read out of range");
    while (n) {
        uint32_t run;
        uint32_t phys = img_phys_run(loff, &run);
        if (run > n)
            run = n;
        nor_file_peek(phys, d, run);
        d += run;
        loff += run;
        n -= run;
    }
}
#endif

int img_read_async(uint32_t loff, void *dst, uint32_t n, int prio)
{
    uint8_t *d = dst;
    int t = 0;
    if (loff > img.size || n > img.size - loff)
        dmc_fatal("image read out of range");
    while (n) {
        uint32_t run;
        uint32_t phys = img_phys_run(loff, &run);
        if (run > n)
            run = n;
#ifdef DMC_STATS
        img_stat_hook(loff, run, (run + 255) / 256);
#endif
        t = nor_read_async(phys, d, run, prio);
        d += run;
        loff += run;
        n -= run;
    }
    return t;
}
