// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright (C) 2026 Artur Bieniek
//
// Inner loops of the drawers, the only renderer code built -O2 (without
// compiler unrolling: the column and span loops are unrolled four times by
// hand, which is where the flash wait states cost most). Strip buffers are
// column-major: a column step is 1 byte, a row step R_VIEWH.

#include "r_strip.h"
#include "trig_tables.h"

void R_ColumnLoop(uint8_t *dest, int n, uint32_t pos, uint32_t step, int sh, const uint8_t *src,
                  const uint8_t *cmap)
{
    for (; n & 3; n--) {
        *dest++ = cmap[src[pos >> sh]];
        pos += step;
    }
    for (; n; n -= 4, dest += 4) {
        dest[0] = cmap[src[pos >> sh]];
        pos += step;
        dest[1] = cmap[src[pos >> sh]];
        pos += step;
        dest[2] = cmap[src[pos >> sh]];
        pos += step;
        dest[3] = cmap[src[pos >> sh]];
        pos += step;
    }
}

// Vanilla R_InitTranslationTables: the green ramp 0x70..0x7f becomes grey,
// brown or red, every other colour stays.
void R_TransColumnLoop(uint8_t *dest, int n, int32_t frac, int32_t step, const uint8_t *src,
                       const uint8_t *cmap, int k)
{
    static const uint8_t base[4] = {0x70, 0x60, 0x40, 0x20};
    unsigned b = base[k & 3];
    do {
        unsigned c = src[(frac >> 16) & 127];
        if ((c & 0xF0u) == 0x70u)
            c = b | (c & 0xFu);
        *dest++ = cmap[c];
        frac += step;
    } while (--n);
}

// Vanilla fuzzoffset with the row step of a column-major strip.
static const int8_t fuzzoff[50] = {
    1, -1, 1, -1, 1, 1, -1, 1, 1, -1, 1, 1, 1, -1, 1, 1, 1, -1, -1, -1, -1, 1, -1, -1, 1,
    1, 1, 1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, -1, -1, -1, 1, 1, 1, 1, -1, 1, 1, -1, 1,
};

void R_FuzzLoop(uint8_t *dest, int n, int pos, const uint8_t *cmap6)
{
    do {
        *dest = cmap6[dest[fuzzoff[pos]]];
        dest++;
        if (++pos == 50)
            pos = 0;
    } while (--n);
}

#define SPAN_TEXEL(p) cmap[src[(((p) >> 6) & 0x3E0u) | ((p) >> 27)]]

void R_SpanLoop(uint8_t *dest, int n, uint32_t pos, uint32_t step, const uint8_t *src,
                const uint8_t *cmap)
{
    for (; n & 3; n--) {
        *dest = SPAN_TEXEL(pos);
        dest += R_VIEWH;
        pos += step;
    }
    for (; n; n -= 4, dest += 4 * R_VIEWH) {
        dest[0] = SPAN_TEXEL(pos);
        pos += step;
        dest[R_VIEWH] = SPAN_TEXEL(pos);
        pos += step;
        dest[2 * R_VIEWH] = SPAN_TEXEL(pos);
        pos += step;
        dest[3 * R_VIEWH] = SPAN_TEXEL(pos);
        pos += step;
    }
}

void R_FillRow(uint8_t *dest, int n, uint8_t c)
{
    do {
        *dest = c;
        dest += R_VIEWH;
    } while (--n);
}

// Vanilla R_MapPlane light: zlight[light][FixedMul(planeheight, yslope[y])
// >> LIGHTZSHIFT], clamped to 127, the row is startmap - dist, clamped to
// 0..31.
static inline int solid_row(int32_t planeheight, int y, int startmap)
{
    uint32_t d = (uint32_t)(int32_t)(((int64_t)planeheight * dmc_yslope_tab[y]) >> 16) >> 20;
    int row = startmap - dmc_light_dist[d > 127 ? 127 : d];
    if (row < 0)
        row = 0;
    if (row > 31)
        row = 31;
    return row;
}

// The same for a product in 0..2^47 - 1: dist is then the high word of the
// product >> 4, which one usat clamps to 0..127.
static inline int solid_row_low(int32_t planeheight, int y, int startmap)
{
    int d = (int32_t)(((int64_t)planeheight * dmc_yslope_tab[y]) >> 32) >> 4;
    int row = startmap - dmc_light_dist[d < 0 ? 0 : d > 127 ? 127 : d];
    if (row < 0)
        row = 0;
    if (row > 31)
        row = 31;
    return row;
}

// yslope is monotone on each side of the horizon, so on a side whose
// products stay in int32 the light row is monotone in y. Its runs of equal
// rows are found by galloping (probe 1, 2, 4, ... rows past the last equal
// one, then halve the last step) and filled with one colormap byte. The
// rows of a side whose products leave int32 (they wrap like vanilla
// FixedMul) are drawn one by one.
void R_SolidColumn(uint8_t *dest, int y, int n, int32_t planeheight, int c, int startmap,
                   const uint8_t *cmaps, const uint8_t *fixedmap)
{
    if (fixedmap) {
        uint8_t v = fixedmap[c];
        RSTAT(solid_runs, 1);
        do
            *dest++ = v;
        while (--n);
        return;
    }
    const uint8_t *cmap = cmaps + c;
    int end = y + n;
#ifdef DMC_RSTATS
    int last = -1;
#endif
    do {
        // The rows up to the horizon or the end. The row nearest the horizon
        // has the largest product: when it is in 0..2^47 - 1 the runs are
        // searched, otherwise row y is drawn alone (a negative planeheight
        // gives a negative product).
        int pe = end, hy = y;
        if (y < R_VIEWH / 2) {
            if (pe > R_VIEWH / 2)
                pe = R_VIEWH / 2;
            hy = pe - 1;
        }
        int search = (((int64_t)planeheight * dmc_yslope_tab[hy]) >> 47) == 0;
        if (!search)
            pe = y + 1;
        int row = solid_row(planeheight, y, startmap);
        for (;;) {
            // rows y..a have the light row, row b another one (or b is pe)
            int a = y, b = pe, step = 1, next;
            while (b - a > 1) {
                int m = a + step;
                if (m >= b)
                    m = (a + b) >> 1;
                int r = solid_row_low(planeheight, m, startmap);
                if (r == row) {
                    a = m;
                    step <<= 1;
                } else {
                    b = m;
                    next = r;
                }
            }
#ifdef DMC_RSTATS
            // the horizon splits runs
            if (row != last || y == R_VIEWH / 2)
                RSTAT(solid_runs, 1);
            last = row;
            if (search)
                RSTAT(solid_fillpx, b - y);
#endif
            uint8_t v = cmap[row << 8];
            do
                *dest++ = v;
            while (++y < b);
            if (y == pe)
                break;
            row = next;
        }
    } while (y < end);
}
