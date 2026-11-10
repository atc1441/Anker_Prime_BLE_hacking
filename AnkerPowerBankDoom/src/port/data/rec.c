// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek

#include <string.h>

#include "rec.h"

dmc_map_t M;

int map_find(int episode, int map)
{
    for (int k = 0; k < img.nmaps; k++) {
        dmc_mapent_t e;
        img_mapent(k, &e);
        if (e.episode == episode && e.map == map)
            return k;
    }
    return -1;
}

void map_load(int k, int g)
{
    dmc_mapent_t e;
    // dmc_maphdr_t from nlines to root
    uint16_t cnt[(offsetof(dmc_maphdr_t, root) + 2 - offsetof(dmc_maphdr_t, nlines)) / 2];
    uint32_t off[DMC_NSTREAMS], size[DMC_NSTREAMS];

    img_mapent(k, &e);
    char magic[4];
    img_read(e.off, magic, 4);
    if (memcmp(magic, DMC_MAGIC_MAP, 4))
        dmc_fatal("bad map block");
    img_read(e.off + offsetof(dmc_maphdr_t, nlines), cnt, sizeof(cnt));
    img_read(e.off + offsetof(dmc_maphdr_t, stream_off), off, sizeof(off));
    img_read(e.off + offsetof(dmc_maphdr_t, stream_size), size, sizeof(size));
    memset(&M, 0, sizeof(M));
    img_read(e.off + offsetof(dmc_maphdr_t, plans) + sizeof(dmc_plan_t) * (unsigned)g, &M.plan,
             sizeof(M.plan));

#define CNT(f) cnt[(offsetof(dmc_maphdr_t, f) - offsetof(dmc_maphdr_t, nlines)) / 2]
    M.nlines = CNT(nlines);
    M.nsides = CNT(nsides);
    M.nsegs = CNT(nsegs);
    M.nssectors = CNT(nssectors);
    M.nnodes = CNT(nnodes);
    M.nsectors = CNT(nsectors);
    M.nthings = CNT(nthings);
    M.ntags = CNT(ntags);
    M.nseclines = CNT(nseclines);
    M.nteledest = CNT(nteledest);
    M.nscrollers = CNT(nscrollers);
    M.nmutable = CNT(nmutable);
    M.nswitches = CNT(nswitches);
    M.nspeclines = CNT(nspeclines);
    M.bmap_ox = (int16_t)CNT(bmap_ox);
    M.bmap_oy = (int16_t)CNT(bmap_oy);
    M.bmap_cols = CNT(bmap_cols);
    M.bmap_rows = CNT(bmap_rows);
    M.root = CNT(root);
#undef CNT
    uint8_t tf[2];
    img_read(e.off + offsetof(dmc_maphdr_t, ntex), tf, 2);
    M.ntex = tf[0];
    M.nflat = tf[1];
    for (int s = 0; s < DMC_NSTREAMS; s++)
        M.st[s] = e.off + off[s];
    // TAGS: ranges[ntags + 1], sectors[], values[ntags + 1]
    M.tagval = M.st[DMC_ST_TAGS] + size[DMC_ST_TAGS] - 2u * (M.ntags + 1u);
    M.episode = e.episode;
    M.map = e.map;
    M.next = e.next;
    M.secret_next = e.secret_next;
    M.sky = e.sky_texture;
    M.name_string = e.name_string;
    M.par_seconds = e.par_seconds;
    rc_reset();
}

unsigned map_tagindex(int value)
{
    for (unsigned t = 1; t <= M.ntags; t++)
        if (map_tagval(t) == value)
            return t;
    return 0;
}

int map_seg_run(ssecidx_t s, dmc_seg_t *buf, int max, segidx_t *first, secidx_t *sector)
{
    dmc_ssector_t ss;
    map_ssector(s, &ss);
    if (ss.numsegs > max)
        dmc_fatal("seg run too long");
    img_read(MAP_REC(DMC_ST_SEGS, ss.firstseg, 16), buf, 16u * ss.numsegs);
    *first = ss.firstseg;
    *sector = ss.sector;
    return ss.numsegs;
}
