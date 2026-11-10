// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Strip renderer. Pass 1 walks the BSP once per frame at
// full width and records wall fragments and sprites. Pass 2 draws strips of
// W logical columns into a double buffer that the LCD pump sends, replaying
// vanilla R_StoreWallRange for every recorded fragment that overlaps the
// strip. All memory of a frame lives in the frame region:
//
//   strip buffers  2 x W x 200 B, column-major (one logical column is 200 B)
//   pass-1 lists   fragments 256 x 4 B, sprites 128 x 6 B, BSP stack
//                  64 x 2 B, solidsegs 32 x 4 B
//   pools          drawsegs, visplanes (textured floors only), vissprites,
//                  openings, sized per plan, reset for every strip
//   strip arrays   clip arrays, spanstart, sprite sort index
//
// A strip whose pools overflow is drawn again as two halves. A strip of one
// column cannot split and drops what does not fit, as vanilla does.
// The status band (HUD message line and status bar, y 200..239) is composed
// per strip from the vanilla widget state, dirty columns only.

#ifndef DMC_R_STRIP_H
#define DMC_R_STRIP_H

#include <stdint.h>

#define R_VIEWH 200     // rows of the 3D view, also the column stride of a strip buffer
#define R_MAXW 160      // widest strip in logical columns
#define R_MAXFRAGS 256  // pass-1 wall fragments (vanilla MAXDRAWSEGS)
#define R_MAXSPRS 128   // pass-1 sprites (vanilla MAXVISSPRITES)
#define R_BSPSTACK 64   // pass-1 BSP stack entries
#define R_MAXSOLID 32   // pass-1 solidsegs entries (vanilla MAXSEGS)
#define R_MAXSECTORS 1024  // sectors the per-frame sprite bitset covers

// Pools of the W8 plans (strips of up to 8 columns, W = 1 included) and of
// the W16 plan.
#define R_W8_DRAWSEGS 56
#define R_W8_VISPLANES 40
#define R_W8_VISSPRITES 20
#define R_W8_OPENINGS 256
#define R_W16_DRAWSEGS 80
#define R_W16_VISPLANES 48
#define R_W16_VISSPRITES 24
#define R_W16_OPENINGS 480

// The frame region starts at FRAME_BASE of the firmware link (__frame_start,
// at most __frame_max bytes). Host builds define R_FRAME_HOST, the size of
// a static buffer that also holds the layouts of strips wider than 16.

typedef struct {
    uint16_t drawsegs, visplanes, vissprites, openings;
} r_pools_t;

// Frame region layout of a strip width and floor mode.
typedef struct {
    int w, textured;
    r_pools_t pools;
    uint32_t vp_size;        // bytes per visplane
    uint32_t strip_bytes;    // both strip buffers
    uint32_t pass1_bytes;
    uint32_t pool_bytes;
    uint32_t array_bytes;
    uint32_t total;
} r_layout_t;

// Pools of a strip width: the W8 plan up to 8 columns, the W16 plan up to
// 16, wider strips (host tests) grow with the width.
r_pools_t R_PoolsFor(int w);
// Fills l for strip width w and floor mode. Returns 0 when it fits in the
// largest frame region of the platform.
int R_Layout(r_layout_t *l, int w, int textured);

// Frame region of the loaded map: as long as the layout of its plan (M.plan
// w and k, 16 B aligned) and fixed until the map or plan changes (a host
// build grows it for wider test strips). Called at level setup before the
// caches and the level arena are placed, and by every frame and band flush
// (cheap when nothing changed). Returns the end of the region, where the
// caches start.
uint8_t *R_FrameSetup(void);
// Start of the frame region and its length for the loaded map.
uint8_t *R_FrameRegion(uint32_t *bytes);
// Tic scratch: the pass-1 lists and the pools of the current layout, at
// least 5,072 B in every plan. Valid between frames only (the next frame
// overwrites it), never overlaps the strip buffers the pump may still read.
uint8_t *R_TicScratch(uint32_t *bytes);

// Strip width for the next frames: 1..160, 0 = the map's plan (M.plan.w).
void R_SetStripWidth(int w);
int R_StripWidth(void);
// Floors and ceilings: 1 solid colour without visplanes (plan D), 0
// textured, -1 the map's plan (M.plan.k == 0 is solid).
void R_SetSolidFloors(int on);
// Flat cache slots (0..4), -1 the map's plan (M.plan.k), set at a map change.
void R_SetFlatSlots(int k);
// Wall LOD by vertical texel step where mips exist (default on).
extern int r_walllod;
// Strip buffers are cleared to colour index 0 before drawing (tests: pixels
// that nothing draws, vanilla HOM, then read the same for every W).
extern int r_stripclear;

// ---- frame (port/render/r_frame.c)

// The view of one frame: pass 1, then every strip through the pump (or into
// the temporary screen when tempscreen is set).
void R_DrawFrame(int tempscreen);
// Opens an LCD pump window, ending the one left open before.
void R_PumpWindow(int x0, int x1, int y0, int y1, int flags);
// Ends the LCD window the renderer or the band left open.
void R_PumpIdle(void);
// A strip buffer the pump no longer reads, and its size in bytes. In a frame without a 3D view
// (fe.flags FE_VIEW clear, port/game/fe.h) one of the two compose buffers instead.
uint8_t *R_BandBuffer(int *bytes);

// Compose buffers of the frames without a 3D view (front-end screens, the band): frame region
// offsets and size, 6 columns of 200 rows each. Buffer 0 lies between the window of the
// front-end overlays (OVL_WIN_FE, port/ovl/ovl.h) and the front-end scratch, buffer 1 behind
// that scratch in the tic scratch (FRAME_BASE + 6,912 + 256), so the last piece of a frame is
// sent before the next tic runs.
#define R_FE_BUF0 5696
#define R_FE_BUF1 7168
#define R_FE_BUFBYTES 1200

// Current strip and its buffer. The strip covers logical columns
// [r_s0, r_s1), column x is at r_stripbuf + (x - r_bufx0) * R_VIEWH.
extern int r_s0, r_s1, r_bufx0;
extern uint8_t *r_stripbuf;
// Set by a pool allocation that failed in a strip that can still split.
extern int r_overflow;
// The strip is one column: allocations that fail drop as vanilla does.
extern int r_vanilla;

// ---- status band (port/render/r_band.c)

enum { BAND_OFF, BAND_RECORD, BAND_COMPOSE, BAND_HASH };
enum { BAND_HUD, BAND_BAR };
// Drawing target of V_DrawPatch and V_CopyRect: the temporary screen
// (BAND_OFF), dirty marking (BAND_RECORD), the band strip (BAND_COMPOSE) or
// the draw call hashes of a front-end screen (BAND_HASH, V_DrawPatch only,
// port/render/r_screen.h).
extern int r_bandmode;
// Marks screen columns x0..x1 of the status bar dirty (record pass).
void R_BandMark(int x0, int x1);
// Record pass of a front-end screen (BAND_HASH): a draw call identified by
// key covers columns x0..x1 and rows y0..y1. It widens the box of the calls
// in x0, x1, y0 and h (its last row) of r_bandwin, and inside the clip
// window cx0..cy1 it is folded into the hash of every 8-column group it
// touches, ((uint32_t *)r_bandwin.buf)[x >> 3], in call order.
void R_ScreenMark(int x0, int x1, int y0, int y1, uint32_t key);
// Columns x0..x1 of rows y0..y0+h-1 (h a multiple of 4, at most 200) to
// LCD rows lcdy.., one window of the pump: pieces that fit a buffer of
// R_BandBuffer, each cleared to colour 0, drawn by draw in BAND_COMPOSE
// (draw NULL clears) and sent.
void R_BandCompose(int x0, int x1, int lcdy, int y0, int h, void (*draw)(void));
// The HUD message line changed.
void R_BandMarkHud(void);
// Every I_SetPalette calls this before it loads pump_lut: the window the
// pump may still be sending ends, and the whole band is sent again on the
// next flush so the new palette covers the whole screen.
void R_PaletteChanged(void);
// Composes the dirty parts of the band and sends them. hud and bar are 0
// when that part of the band is not shown (it is then cleared once).
void R_BandFlush(int hud, int bar);
// Compose pass window: band strip buf (column-major, h rows per column)
// holding screen columns x0..x1, row 0 is vanilla screen row y0. Drawing
// is clipped to columns cx0..cx1 and vanilla rows cy0..cy1.
typedef struct {
    uint8_t *buf;
    int x0, x1, y0, h;
    int cx0, cx1, cy0, cy1;
} r_bandwin_t;
extern r_bandwin_t r_bandwin;

// ---- inner loops (port/render/r_inner.c, built -O2)

// n >= 1 pixels down a column (dest step 1). The source row of a pixel is
// pos >> sh, pos advances by step (both the texture position shifted left
// so that the row bits are the top ones, see R_DrawColumn).
void R_ColumnLoop(uint8_t *dest, int n, uint32_t pos, uint32_t step, int sh, const uint8_t *src,
                  const uint8_t *cmap);
// Translated column, k = 1..3 is the vanilla player translation table.
void R_TransColumnLoop(uint8_t *dest, int n, int32_t frac, int32_t step, const uint8_t *src,
                       const uint8_t *cmap, int k);
// Fuzz: dest[i] = cmap6[dest[i +- 1]], pos is the fuzz table index of the
// first pixel.
void R_FuzzLoop(uint8_t *dest, int n, int pos, const uint8_t *cmap6);
// n >= 1 pixels along a row (dest step R_VIEWH), 32x32 flat, packed
// position (x in the high, y in the low 16 bits, 6.10 each).
void R_SpanLoop(uint8_t *dest, int n, uint32_t pos, uint32_t step, const uint8_t *src,
                const uint8_t *cmap);
// n >= 1 pixels of colour c along a row.
void R_FillRow(uint8_t *dest, int n, uint8_t c);
// Solid floor or ceiling column, rows y..y+n-1 (n >= 1): colour c through
// the zlight row of each row's distance, or through fixedmap when not NULL.
void R_SolidColumn(uint8_t *dest, int y, int n, int32_t planeheight, int c, int startmap,
                   const uint8_t *cmaps, const uint8_t *fixedmap);

// ---- statistics (host builds with DMC_RSTATS)

typedef struct {
    // pass 1
    uint32_t nodes, subsectors, segs, frags, frags_dropped, sprites, sprites_dropped;
    // pass 2
    uint32_t strips, splits, vanilla_drops, replays, wallcols, wallpx, skycols, skypx;
    uint32_t planes, spans, spanpx, solidcols, solidpx, vissprites, sprcols, maskedcols;
    uint32_t pspcols, colpx, fuzzpx;
    uint32_t nor_tx, nor_bytes;  // filled by the host
    // column requests (walls, sky, masked), columns drawn from data a
    // buffer already held, NOR transactions and bytes of the requests and
    // the drawers' wait for them in cycles: a request is sent before the
    // column queued ahead of it is drawn, which hides that many cycles of
    // its NOR time (measured on the meter: 2.8 us per transaction, 8 MB/s, column setup
    // 260 cycles, 18 cycles per pixel)
    uint32_t colreq, colhit, coltx, colbytes, colwait;
    // status band of the frame: columns composed, pieces sent, NOR traffic
    uint32_t band_cols, band_pieces, band_nor_tx, band_nor_bytes;
    // calls of the table decoders (port/math/trig.h) during the view:
    // finesine and finecosine, the inline finetangent of the wall columns,
    // tantoangle, and those of the game logic since the last view (filled
    // by the host)
    uint32_t trig_sin, trig_tan, trig_atan, logic_sin, logic_atan;
    // per-strip high-water marks of this frame
    uint16_t hw_drawsegs, hw_visplanes, hw_vissprites, hw_openings;
    // speed-ups: column contexts filled for walls and for posts, direct
    // column reads, reads that wrap around the stored height, columns drawn
    // at base level + 2, fragment setups taken from the cache, runs of equal
    // light rows in solid floor columns (split at the horizon) and the
    // pixels drawn by run fills.
    uint32_t ctx_fills, post_fills, col_direct, col_wrap, col_lv2, fragc_hits, solid_runs;
    uint32_t solid_fillpx;
} r_stats_t;

#ifdef DMC_RSTATS
extern r_stats_t r_stats;
#define RSTAT(f, n) (r_stats.f += (n))
#define RSTAT_MAX(f, v) do { if ((v) > r_stats.f) r_stats.f = (uint16_t)(v); } while (0)
// Host hooks around the view of a frame and after every strip drawn
// (host/i_video_host.c): pool use of the strip.
void R_StatsBegin(void);
void R_StatsEnd(void);
void R_StatsStrip(int drawsegs, int visplanes, int vissprites, int openings);
// Queued NOR transactions and bytes so far (host counters).
void R_StatsNor(uint32_t *tx, uint32_t *bytes);
#else
#define RSTAT(f, n) ((void)0)
#define RSTAT_MAX(f, v) ((void)0)
#endif

#ifdef DMC_ATTR
// Host frame test: pixel classes per strip (host/rattr_host.c).
enum { ATTR_NONE, ATTR_COLUMN, ATTR_SPAN, ATTR_FUZZ, ATTR_BEYOND, ATTR_POSTREAD };
extern uint8_t *r_attrbuf;  // parallel to r_stripbuf
void R_AttrStrip(int x0, int n, const uint8_t *pix, const uint8_t *cls);
void R_AttrViewEnd(void);
#endif

#endif
