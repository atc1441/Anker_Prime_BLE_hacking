// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Decoders for the tables in gen/trig_tables.c. The arithmetic matches
// tools/conv/trig.py exactly.

#include "trig.h"

#define ANG90 0x40000000u

#ifdef DMC_RSTATS
uint32_t dmc_trig_calls[DMC_TRIG_KINDS];
#endif

#ifdef DMC_SINE_L1
// Compact sine table: dmc_sine_q[j] interpolated between every 16th entry, plus its
// 2-bit residual
static int sine_quarter(unsigned j)
{
    const uint16_t *c = &dmc_sine_coarse[j >> DMC_SINE_SHIFT];
    int f = (int)(j & ((1u << DMC_SINE_SHIFT) - 1u));
    int r = (dmc_sine_resid[j >> 2] >> ((j & 3u) * 2u)) & 3;
    return c[0] + (((c[1] - c[0]) * f) >> DMC_SINE_SHIFT) + r;
}
#else
#define sine_quarter(j) ((int)dmc_sine_q[j])
#endif

// Quadrants 1 and 3 mirror the quarter wave, 2 and 3 negate it
static int sine_rule(unsigned i)
{
    unsigned a = i & 8191u, j = a & 2047u;
    if (a & 2048u)
        j = 2047u - j;
    int v = sine_quarter(j);
    return a & 4096u ? -v : v;
}

int dmc_finesine(unsigned i)
{
    unsigned b = i / DMC_SINE_FIX_BLOCK;
    DMC_TRIG_COUNT(DMC_TRIG_SINE);
    if (dmc_sine_fix_map[b >> 5] & (1u << (b & 31u))) {
        unsigned lo = 0, hi = DMC_SINE_NFIX;
        while (lo < hi) {
            unsigned mid = (lo + hi) >> 1;
            if (dmc_sine_fix_idx[mid] < i)
                lo = mid + 1;
            else
                hi = mid;
        }
        if (lo < DMC_SINE_NFIX && dmc_sine_fix_idx[lo] == i)
            return dmc_sine_fix_val[lo];
    }
    return sine_rule(i);
}

int dmc_finecosine(unsigned i)
{
    return dmc_finesine(i + 2048u);
}

int dmc_finetangent(unsigned i)
{
    return dmc_finetangent_inline(i);
}

uint32_t dmc_tantoangle(unsigned i)
{
    unsigned b = i / DMC_ATAN_BLOCK;
    int j = (int)(i % DMC_ATAN_BLOCK);
    DMC_TRIG_COUNT(DMC_TRIG_ATAN);
    int64_t t = (int64_t)dmc_atan_a[b] * j + (int64_t)dmc_atan_c[b] * (j * (j - (DMC_ATAN_BLOCK - 1)));
    return dmc_atan_base[b] + (uint32_t)(t >> DMC_ATAN_SHIFT) + (uint32_t)(int32_t)dmc_atan_resid[i];
}

static unsigned popcount32(uint32_t v)
{
    v = v - ((v >> 1) & 0x55555555u);
    v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
    v = (v + (v >> 4)) & 0x0F0F0F0Fu;
    return (v * 0x01010101u) >> 24;
}

int dmc_viewangletox(int detail, unsigned i)
{
    unsigned lo, hi, width;
    const uint32_t *words;
    const uint16_t *prefix;
    if (detail) {
        lo = DMC_VATX_LO_LOW, hi = DMC_VATX_HI_LOW, width = 160;
        words = dmc_vatx_words_low, prefix = dmc_vatx_prefix_low;
    } else {
        lo = DMC_VATX_LO_HIGH, hi = DMC_VATX_HI_HIGH, width = 320;
        words = dmc_vatx_words_high, prefix = dmc_vatx_prefix_high;
    }
    if (i < lo)
        return (int)width;
    if (i > hi)
        return 0;
    unsigned k = i - lo;
    uint32_t w = words[k >> 5] & ((2u << (k & 31u)) - 1u);
    return (int)(width - prefix[k >> 5] - popcount32(w));
}

uint32_t dmc_xtoviewangle(int detail, unsigned x)
{
    unsigned fine = detail ? dmc_xtva_low[x] : dmc_xtva_high[x];
    return ((uint32_t)fine << 19) - ANG90;
}

int dmc_yslope(unsigned y)
{
    return dmc_yslope_tab[y];
}

static int clamp_level(int v)
{
    return v < 0 ? 0 : v > 31 ? 31 : v;
}

int dmc_light_startmap(unsigned lightnum)
{
    return dmc_light_startmap_tab[lightnum];
}

int dmc_scalelight(unsigned lightnum, unsigned scale)
{
    return clamp_level(dmc_light_startmap_tab[lightnum] - (int)(scale >> 1));
}

int dmc_zlight(unsigned lightnum, unsigned z)
{
    return clamp_level(dmc_light_startmap_tab[lightnum] - dmc_light_dist[z]);
}

const uint8_t *dmc_colormap_row(unsigned row)
{
    return dmc_colormap + row * 256u;
}
