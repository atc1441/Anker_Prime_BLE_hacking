// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Frame region, strip loop and LCD output of the strip renderer.
//
// Frame region layout (all offsets from the region start):
//   strip buffers  2 x W x 200
//   pass-1 lists   fragments, solidsegs, sprites, BSP stack
//   pools          drawsegs, vissprites, openings, visplanes (during pass 1
//                  the sector bitset and the seg run use the same bytes)
//   strip arrays   floorclip, ceilingclip, clipbot, cliptop (W shorts each),
//                  spanstart (200, textured floors only), sort index
// The region starts at FRAME_BASE and is as long as the layout of the map's
// plan, the caches and the level arena follow it.

#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "i_video.h"
#include "r_local.h"

#include "fe.h"
#include "fetch.h"
#include "lcd.h"
#include "ovl.h"
#include "r_strip.h"

#ifdef R_FRAME_HOST
// The frame model (DMC_HOST_FRAME) also places the tic scratch and the overlay window here,
// at their offsets on the meter (mk/engine.mk).
#ifndef DMC_HOST_FRAME
static
#endif
uint8_t frame_mem[R_FRAME_HOST] __attribute__((aligned(16)));
#define FRAME_START frame_mem
#define FRAME_MAX ((uint32_t)R_FRAME_HOST)
#else
extern uint8_t __frame_start[], __frame_max[];
#define FRAME_START __frame_start
#define FRAME_MAX ((uint32_t)(uintptr_t)__frame_max)
#endif

int r_s0, r_s1, r_bufx0;
uint8_t *r_stripbuf;
int r_overflow, r_vanilla, r_stripclear;

#ifdef DMC_RSTATS
r_stats_t r_stats;
#endif

#ifdef DMC_ATTR
uint8_t *r_attrbuf;
static uint8_t attr_mem[2][R_MAXW * R_VIEWH];
#endif

static int w_request;   // 0: the map's plan
static int solid_request = -1;  // -1: the map's plan
static int slots_request = -1;  // -1: the map's plan
static uint32_t config_map = 0xFFFFFFFFu;  // map block and plan the region was set for
static uint32_t config_plan;
static uint32_t frame_len;  // region bytes of the loaded map
static r_layout_t lay;  // layout in use, lay.w 0 before the first frame
static uint8_t *bufs[2];
static int cur;
static short *floor_mem, *ceil_mem, *bot_mem, *top_mem;
static int pump_open;

#define PASS1_BYTES (R_MAXFRAGS * 4 + R_MAXSPRS * 6 + R_BSPSTACK * 2 + R_MAXSOLID * 4)
#define PASS1_SCRATCH (R_MAXSECTORS / 8 + MAP_SEGRUN_MAX * 16)
#define ALIGN4(n) (((n) + 3u) & ~3u)
#define ALIGN16(n) (((n) + 15u) & ~15u)
// Pass-1 fragments keep the seg in 14 bits.
#define R_MAXSEGS (1 << 14)

// A one-column strip takes at most three openings per drawseg (masked
// columns, top and bottom clip), so it runs out of drawsegs first.
_Static_assert(R_W8_OPENINGS >= 3 * R_W8_DRAWSEGS && R_W16_OPENINGS >= 3 * R_W16_DRAWSEGS,
               "openings must cover three per drawseg");

r_pools_t R_PoolsFor(int w)
{
    static const r_pools_t w8 = {R_W8_DRAWSEGS, R_W8_VISPLANES, R_W8_VISSPRITES, R_W8_OPENINGS};
    static const r_pools_t w16 = {R_W16_DRAWSEGS, R_W16_VISPLANES, R_W16_VISSPRITES,
                                  R_W16_OPENINGS};
    if (w <= 8)
        return w8;
    if (w <= 16)
        return w16;
    // wider strips (host tests) grow with the width up to the vanilla limits
    r_pools_t p;
    p.drawsegs = (uint16_t)(80 * w / 16 > 256 ? 256 : 80 * w / 16);
    p.visplanes = (uint16_t)(48 * w / 16 > 128 ? 128 : 48 * w / 16);
    p.vissprites = (uint16_t)(24 * w / 16 > 128 ? 128 : 24 * w / 16);
    p.openings = (uint16_t)(480 * w / 16);
    return p;
}

int R_Layout(r_layout_t *l, int w, int textured)
{
    memset(l, 0, sizeof(*l));
    l->w = w;
    l->textured = textured;
    l->pools = R_PoolsFor(w);
    if (!textured)
        l->pools.visplanes = 0;
    l->vp_size = (uint32_t)(sizeof(visplane_t) + 2 * (w + 2) + 1) & ~1u;
    l->strip_bytes = 2u * (uint32_t)w * R_VIEWH;
    l->pass1_bytes = PASS1_BYTES;
    l->pool_bytes = ALIGN4(l->pools.drawsegs * sizeof(drawseg_t) +
                           l->pools.vissprites * sizeof(vissprite_t) + l->pools.openings * 2u +
                           l->pools.visplanes * l->vp_size);
    if (l->pool_bytes < PASS1_SCRATCH)
        l->pool_bytes = PASS1_SCRATCH;
    l->array_bytes = ALIGN4(4u * 2u * (uint32_t)w + (textured ? R_VIEWH : 0) + l->pools.vissprites);
    l->total = l->strip_bytes + l->pass1_bytes + l->pool_bytes + l->array_bytes;
    return l->total <= FRAME_MAX ? 0 : -1;
}

static void R_ApplyLayout(int w, int textured)
{
    r_layout_t l;
    int over = R_Layout(&l, w, textured);
#ifdef R_FRAME_HOST
    // host tests may ask for wider strips or other floors than the plan
    if (!over && l.total > frame_len)
        frame_len = ALIGN16(l.total);
#endif
    if (over || l.total > frame_len)
        I_Error("R_ApplyLayout: W %d needs %u B of frame region, the plan has %u B", w,
                (unsigned)l.total, (unsigned)frame_len);
    lay = l;
    uint8_t *p = FRAME_START;
    bufs[0] = p;
    bufs[1] = p + (uint32_t)w * R_VIEWH;
    p += l.strip_bytes;

    r_frags = (uint32_t *)p;
    p += R_MAXFRAGS * 4;
    solidsegs = (cliprange_t *)p;
    p += R_MAXSOLID * 4;
    r_sprs = p;
    p += R_MAXSPRS * 6;
    r_bspstack = (unsigned short *)p;
    p += R_BSPSTACK * 2;

    uint8_t *pools = p;
    drawsegs = (drawseg_t *)p;
    p += l.pools.drawsegs * sizeof(drawseg_t);
    drawsegs_end = (drawseg_t *)p;
    vissprites = (vissprite_t *)p;
    p += l.pools.vissprites * sizeof(vissprite_t);
    vissprites_end = (vissprite_t *)p;
    openings = (short *)p;
    p += l.pools.openings * 2u;
    openings_end = (short *)p;
    r_planemem = p;
    r_planesize = (int)l.vp_size;
    r_planew = w;
    r_maxplanes = l.pools.visplanes;
    r_solid = !textured;
    // pass-1 scratch over the pools
    r_secbits = pools;
    r_segrun = pools + R_MAXSECTORS / 8;
    p = pools + l.pool_bytes;

    floor_mem = (short *)p;
    ceil_mem = floor_mem + w;
    bot_mem = ceil_mem + w;
    top_mem = bot_mem + w;
    p = (uint8_t *)(top_mem + w);
    spanstart = textured ? p : NULL;
    p += textured ? R_VIEWH : 0;
    r_sortidx = p;
}

void R_SetStripWidth(int w)
{
    w_request = w < 0 ? 0 : w > R_MAXW ? R_MAXW : w;
}

int R_StripWidth(void)
{
    return lay.w;
}

void R_SetSolidFloors(int on)
{
    solid_request = on;
}

void R_SetFlatSlots(int k)
{
    slots_request = k;
    config_map = 0xFFFFFFFFu;
}

// Width, floor mode and flat slots of the map's plan unless the host
// chose them. The region length and the flat cache are set when the map
// or its plan changes.
uint8_t *R_FrameSetup(void)
{
    uint32_t plan = M.plan.plan | (uint32_t)M.plan.w << 8 | (uint32_t)M.plan.k << 16;
    int pw = M.plan.w ? M.plan.w : 8;
    if (M.st[0] != config_map || plan != config_plan) {
        if (M.nsectors > R_MAXSECTORS)
            I_Error("R_FrameSetup: %u sectors, at most %d", (unsigned)M.nsectors, R_MAXSECTORS);
        if (M.nsegs > R_MAXSEGS)
            I_Error("R_FrameSetup: %u segs, at most %d", (unsigned)M.nsegs, R_MAXSEGS);
        R_PumpIdle();
        config_map = M.st[0];
        config_plan = plan;
        flat_set_slots(slots_request >= 0 ? slots_request : M.plan.k);
        r_layout_t l;
        if (R_Layout(&l, pw, M.plan.k != 0))
            I_Error("R_FrameSetup: plan needs %u B of frame region", (unsigned)l.total);
        frame_len = ALIGN16(l.total);
        lay.w = 0;
    }
    int w = w_request ? w_request : pw;
    int textured = solid_request >= 0 ? !solid_request : M.plan.k != 0;
    if (w != lay.w || textured != lay.textured) {
        R_PumpIdle();
        R_ApplyLayout(w, textured);
    }
    return FRAME_START + frame_len;
}

uint8_t *R_FrameRegion(uint32_t *bytes)
{
    R_FrameSetup();
    *bytes = frame_len;
    return FRAME_START;
}

uint8_t *R_TicScratch(uint32_t *bytes)
{
    R_FrameSetup();
    *bytes = lay.pass1_bytes + lay.pool_bytes;
    return FRAME_START + lay.strip_bytes;
}

// ---------------------------------------------------------------- pump

void R_PumpWindow(int x0, int x1, int y0, int y1, int flags)
{
    R_PumpIdle();
    pump_begin(x0, x1, y0, y1, flags);
    pump_open = 1;
}

void R_PumpIdle(void)
{
    if (pump_open) {
        pump_end();
        pump_open = 0;
    }
}

// The compose buffers stay out of the window of the front-end overlays and of the front-end
// scratch at the start of the tic scratch (FRAME_BASE + 6,912, TIC_OFFSET of
// platform/link/doom.ld), inside the smallest frame region.
_Static_assert(R_FE_BUF0 >= OVL_WIN_FE && R_FE_BUF0 + R_FE_BUFBYTES <= 6912
               && R_FE_BUF1 >= 6912 + sizeof(fe_scr_t)
               && R_FE_BUF1 + R_FE_BUFBYTES <= OVL_WIN_FRAME,
               "a compose buffer overlaps a front-end overlay or the front-end scratch");

// A free strip buffer for the band compositor and its size, or a compose buffer in a frame
// without a 3D view.
uint8_t *R_BandBuffer(int *bytes)
{
    cur ^= 1;
    if (pump_open)
        pump_sync(1);
    if (!(fe.flags & FE_VIEW)) {
        *bytes = R_FE_BUFBYTES;
        return FRAME_START + (cur ? R_FE_BUF1 : R_FE_BUF0);
    }
    *bytes = lay.w * R_VIEWH;
    return bufs[cur];
}

// ---------------------------------------------------------------- strips

// One try at the strip [a, b) of the buffer. Returns 0 when a pool
// overflowed and the strip can split.
static int R_TryStrip(int a, int b)
{
    r_s0 = a;
    r_s1 = b;
    r_overflow = 0;
    r_vanilla = b - a == 1;
    floorclip = floor_mem - a;
    ceilingclip = ceil_mem - a;
    clipbot = bot_mem - a;
    cliptop = top_mem - a;
    ds_p = drawsegs;
    R_ClearPlanes();
    if (r_stripclear)
        memset(r_stripbuf + (a - r_bufx0) * R_VIEWH, 0, (size_t)(b - a) * R_VIEWH);
#ifdef DMC_ATTR
    memset(r_attrbuf + (a - r_bufx0) * R_VIEWH, ATTR_NONE, (size_t)(b - a) * R_VIEWH);
#endif
    RSTAT(strips, 1);

    // walls and sky, each column drawn after the next one is requested
    R_ColumnsBegin();
    for (int i = 0; i < r_nfrags && !r_overflow; i++) {
        uint32_t f = r_frags[i];
        if ((int)(f & 511) < a || (int)((f >> 9) & 511) >= b)
            continue;
        R_ReplayWall(f, i);
    }
    R_ColumnsEnd(!r_overflow);
    if (r_overflow)
        return 0;
    if (!r_solid)
        R_DrawPlanes();
    // masked columns over the same column buffers
    R_ColumnsBegin();
    if (!R_DrawMasked())
        return 0;
    RSTAT_MAX(hw_drawsegs, ds_p - drawsegs);
    RSTAT_MAX(hw_openings, lastopening - openings);
#ifdef DMC_RSTATS
    R_StatsStrip((int)(ds_p - drawsegs), r_numplanes, (int)(vissprite_p - vissprites),
                 (int)(lastopening - openings));
#endif
    return 1;
}

// The strip [s0, s1), split in halves until every part fits its pools.
// Parts of one column cannot split and drop as vanilla does.
static void R_DrawStrip(int s0, int s1)
{
    short stack[2 * 10];
    int sp = 0, a = s0, b = s1;
    for (;;) {
        if (R_TryStrip(a, b)) {
            if (!sp)
                return;
            b = stack[--sp];
            a = stack[--sp];
        } else {
            int mid = (a + b) >> 1;
            RSTAT(splits, 1);
            stack[sp++] = (short)mid;
            stack[sp++] = (short)b;
            b = mid;
        }
    }
}

// Temporary screen (host only): the strip goes into the 320x200 buffer
// the pause picture draws over.
static void R_StripToScreen(const uint8_t *buf, int s0, int n)
{
    if (!I_VideoBuffer)
        return;
    for (int i = 0; i < n; i++)
        for (int y = 0; y < R_VIEWH; y++) {
            uint8_t *d = I_VideoBuffer + y * SCREENWIDTH + ((s0 + i) << detailshift);
            d[0] = buf[i * R_VIEWH + y];
            if (detailshift)
                d[1] = d[0];
        }
}

void R_DrawFrame(int tempscreen)
{
    // the strips and the pass-1 lists overwrite the overlay window
    ovl_invalidate();
    R_FrameSetup();
#ifdef DMC_RSTATS
    memset(&r_stats, 0, sizeof(r_stats));
    R_StatsBegin();
#endif
    R_PlanesFrame();
    R_RenderBSPPass();

    if (!tempscreen)
        R_PumpWindow(0, SCREENWIDTH - 1, 0, R_VIEWH - 1, detailshift ? PUMP_DOUBLE : 0);
    for (int s0 = 0; s0 < viewwidth; s0 += lay.w) {
        int s1 = s0 + lay.w < viewwidth ? s0 + lay.w : viewwidth;
        cur ^= 1;
        if (!tempscreen)
            pump_sync(1);  // the buffer sent two strips ago is free
        r_stripbuf = bufs[cur];
        r_bufx0 = s0;
#ifdef DMC_ATTR
        r_attrbuf = attr_mem[cur];
#endif
        R_DrawStrip(s0, s1);
#ifdef DMC_ATTR
        R_AttrStrip(s0, s1 - s0, r_stripbuf, r_attrbuf);
#endif
        if (tempscreen)
            R_StripToScreen(r_stripbuf, s0, s1 - s0);
        else
            pump_submit(r_stripbuf, s1 - s0);
    }
#ifdef DMC_ATTR
    R_AttrViewEnd();
#endif
#ifdef DMC_RSTATS
    R_StatsEnd();
#endif
}
