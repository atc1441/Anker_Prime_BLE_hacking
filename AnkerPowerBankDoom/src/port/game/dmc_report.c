// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The REPORT overlay (dmc_fw.h): which demos run, the header and the records of DOOMRES.DAT
// (dmc_res.h) and the screens between passes. Every save rewrites the file's sector through
// DMC_SECTOR_BUF with what it held before.

#include <string.h>

#include "doomdef.h"
#include "doomstat.h"

#include "dmc_fw.h"
#include "cache.h"
#include "extents.h"
#include "fetch.h"
#include "img.h"
#include "lcd.h"
#include "nor.h"
#include "ovl.h"
#include "rec.h"
#include "things.h"
#include "ui.h"

// The sector of DOOMRES.DAT in DMC_SECTOR_BUF: as it is, or cleared for a new run but its
// crash record.
static uint8_t *sector(int fresh)
{
    uint8_t *buf = DMC_SECTOR_BUF;
    nor_read(res_addr, buf, RES_SECTOR);
    if (fresh) {
        memset(buf, 0, RES_CRASH_OFF);
        memset(buf + RES_CHAIN_OFF, 0xFF, RES_SECTOR - RES_CHAIN_OFF);
    }
    return buf;
}

static uint32_t *record(uint8_t *buf, int k)
{
    return (uint32_t *)(void *)(buf + RES_DEMO_OFF + (uint32_t)k * RES_DEMO_BYTES);
}

// The header into buf, the sector back to the NOR (a second try after a difference), then
// the record cache and the flat slots emptied: the buffer may have been over them.
static void save(uint8_t *buf)
{
    uint32_t *w = (uint32_t *)(void *)(buf + RES_HDR_OFF);
    w[H_MAGIC] = RES_MAGIC;
    w[H_VERSION] = RES_VERSION;
    w[H_SIZE] = RES_SIZE;
    w[H_SAVES] = ++res_saves;
    w[H_UPTIME] = plat_ms();
    w[H_STATE] = res_cur_state;
    w[H_DEMO] = res_cur_demo;
    w[H_NDEMOS] = res_ndemos;
    for (unsigned i = 0; i < 20; i++)
        ((uint8_t *)&w[H_BUILD])[i] = dmc_build_id[i];
    // the image uuid from the header of DOOM.DAT
    if (ext_read(DMC_F_DOOM, offsetof(dmc_header_t, image_uuid), &w[H_UUID], 16))
        memset(&w[H_UUID], 0, 16);
    w[H_TEST_TICS] = dmc_test.tics;
    w[H_CORE] = (uint32_t)(uintptr_t)__core_size;
    w[H_FRAME_BASE] = (uint32_t)(uintptr_t)DMC_FRAME_BASE;
    w[H_RAM_END] = (uint32_t)(uintptr_t)__ram_end;
    w[H_STACK] = plat_stack_used();
    w[H_VARIANT] = (uint32_t)(uintptr_t)__dmc_variant;
    w[H_RUNS] = res_runs;
    w[H_TEST_FRAMES] = dmc_test.frames;
    w[H_BOOT] = res_boot;
    if (dmc_sector_write(res_addr, buf, RES_SECTOR))
        dmc_sector_write(res_addr, buf, RES_SECTOR);
    rc_reset();
    flat_reset();
}

// ---- the demos of a run

// Demo k: its tics, 0 when it is not there, -1 when this firmware does not play it (it plays
// v1.9 single player demos up to Ultra-Violence, without -fast and -respawn, of a map of the
// pack).
static int demo_tics(int k)
{
    uint8_t h[13];
    uint32_t off, size;
    if (!k) {
        if (demo_get(0, &off, &size))
            return 0;
        img_read(off, h, sizeof(h));
    } else {
        if (dmc_demo_open(dmc_demo_names[k], &size))
            return 0;
        if (size < 14)
            return -1;
        dmc_demo_read(0, h, sizeof(h));
    }
    if (h[0] != 109 || h[1] > sk_hard || h[4] || h[5] || h[6] || h[8] || h[9] != 1 || h[10]
        || h[11] || h[12] || map_find(h[2], h[3]) < 0)
        return -1;
    // 13 B header, 4 B per tic, the end marker
    return (int)((size - 14) / 4);
}

// The demos of a run: those the test settings pick (dmc_test.demos), else every one found
static int report_begin(void)
{
    int found[RES_DEMOS], n = 0;
    for (int k = 0; k < RES_DEMOS; k++) {
        found[k] = dmc_test.demos && !(dmc_test.demos >> k & 1) ? 0 : demo_tics(k);
        dmc_run.tics[k] = found[k] > 0 ? (uint32_t)found[k] : 0;
        n += found[k] != 0;
    }
    res_runs++;
    res_saves = 0;
    res_ndemos = (uint32_t)n;
    res_next_slot = 0;
    res_cur_state = RS_RUN;
    res_cur_demo = 0;
    if (!res_addr)
        return n;
    uint8_t *buf = sector(1);
    for (int k = 0; k < RES_DEMOS; k++) {
        uint32_t *r = record(buf, k);
        if (!found[k])
            continue;
        memcpy(&r[D_NAME], dmc_demo_names[k], sizeof(dmc_demo_names[k]));
        if (found[k] < 0)
            r[D_STATUS] = DS_NODEMO;
    }
    save(buf);
    return n;
}

// ---- records

static void report_verified(void)
{
    const dmc_run_t *d = &dmc_run;
    if (!res_addr)
        return;
    uint8_t *buf = sector(0);
    uint32_t *r = record(buf, (int)d->k);
    uint32_t slots = d->v.first < RES_CHAIN_MAX ? RES_CHAIN_MAX - d->v.first : 0;
    r[D_STATUS] = !d->tics[d->k] ? DS_NODEMO
        : (d->v.end == DE_STOP ? DS_STOPPED : DS_VERIFIED) | (d->v.nchain > slots ? DS_TRUNC : 0);
    r[D_LEVEL] = d->v.level;
    r[D_V_TICS] = d->v.tics;
    r[D_V_END] = d->v.end;
    r[D_V_HASH] = (uint32_t)d->v.h;
    r[D_V_HASH + 1] = (uint32_t)(d->v.h >> 32);
    r[D_V_CHAIN] = (uint32_t)d->v.c;
    r[D_V_CHAIN + 1] = (uint32_t)(d->v.c >> 32);
    r[D_V_NCHAIN] = d->v.nchain < slots ? d->v.nchain : slots;
    r[D_V_FIRST] = d->v.first;
    r[D_V_MS] = d->ms;
    for (int e = 0; e < 5; e++)
        r[D_V_EVENTS + e] = o_stats.events[e];
    r[D_V_PAGES] = d->v.pages | d->pages_peak << 16;
    r[D_V_RAM] = d->v.frame | (uint32_t)d->v.caches << 16;
    r[D_V_ARENA] = d->v.arena;
    save(buf);
}

// Percentile pct of a phase: p50 and p95 of the total from the 1 ms histogram (upper edge of
// the bin), those of the other phases interpolated in their bins (dmc_res.h).
static uint32_t pct(const dmc_stats_t *st, int p, uint32_t pct)
{
    uint32_t total = 0, n = 0, v = st->max[p];
    if (p == PH_TOTAL) {
        uint32_t want = (st->frames * pct + 99u) / 100u;
        for (unsigned b = 0; b < 101; b++) {
            n += st->hist[b];
            if (n >= want && n) {
                v = b < 100 ? (b + 1) * 1000u : v;
                break;
            }
        }
    } else {
        for (unsigned b = 0; b < ST_BINS; b++)
            total += st->bin[p][b];
        uint32_t want = total * pct;  // in hundredths of a frame
        for (unsigned b = 0; b < ST_BINS && total; b++) {
            uint32_t c = st->bin[p][b];
            if ((n + c) * 100u >= want && c) {
                uint32_t lo = b ? ST_EDGE(b) : 0, hi = b + 1 < ST_BINS ? ST_EDGE(b + 1) : v;
                uint32_t f = (want - n * 100u) * 256u / (c * 100u), w = hi > lo ? hi - lo : 0;
                v = lo + w / 256u * f + w % 256u * f / 256u;
                break;
            }
            n += c;
        }
    }
    return v < st->max[p] ? v : st->max[p];
}

static void report_timed(void)
{
    const dmc_run_t *d = &dmc_run;
    const dmc_stats_t *st = &dmc_st;
    uint32_t mean = st->frames ? st->sum[PH_TOTAL] / st->frames : 0;

    if (res_addr) {
        uint8_t *buf = sector(0);
        uint32_t *r = record(buf, (int)d->k);
        if (dmc_timed.done)
            r[D_STATUS] |= DS_TIMED;
        if (d->stopped)
            r[D_STATUS] |= DS_STOPPED;
        r[D_T_FRAMES] = dmc_timed.frames;
        r[D_T_MS] = d->ms;
        memcpy(&r[D_T_NOR], dmc_timed.nor, sizeof(dmc_timed.nor));
        r[D_T_STACK] = plat_stack_used();
        r[D_T_PAGES] = d->pages_peak;
        memcpy(&r[D_T_LCD], dmc_timed.lcd, sizeof(dmc_timed.lcd));
        for (int p = 0; p < PH_COUNT; p++) {
            uint32_t *w = r + D_T_PHASE + 4 * p;
            w[0] = st->frames ? st->sum[p] / st->frames : 0;
            w[1] = pct(st, p, 50);
            w[2] = pct(st, p, 95);
            w[3] = st->max[p];
        }
        memcpy(r + D_T_HIST, st->hist, sizeof(st->hist));
        memcpy(r + D_T_VIEW, dmc_timed.view, sizeof(dmc_timed.view));
        save(buf);
    }
    dmc_screen("Timed demo");
    ui_row(1, C_WHITE, "%s: %u frames in %u ms", dmc_demo_names[d->k], (unsigned)dmc_timed.frames,
           (unsigned)d->ms);
    if (mean)
        ui_row(2, C_WHITE, "frame mean %u us, %u fps, max %u us", (unsigned)mean,
               (unsigned)(1000000u / mean), (unsigned)st->max[PH_TOTAL]);
}

static void report_state(int state, int demo)
{
    res_cur_state = (uint32_t)state;
    res_cur_demo = (uint32_t)demo;
    if (res_addr && res_runs)
        save(sector(0));
}

// ---- results screen

static int report_menu(void)
{
    static const char *const end[] = {"", "end", "level exit", "limit", "stopped"};
    res_menus++;
    dmc_screen("DOOM for the DMC-100");
    if (!res_addr) {
        ui_row(1, C_ORANGE, "No DOOMRES.DAT of 4 KB on the firmware");
        ui_row(2, C_ORANGE, "drive: the results are not saved.");
    } else {
        uint32_t r[D_WORDS];
        ui_row(1, C_WHITE, "Demo results (DOOMRES.DAT):");
        for (int k = 0, row = 2; k < RES_DEMOS; k++) {
            nor_read(res_addr + RES_DEMO_OFF + (uint32_t)k * RES_DEMO_BYTES, r, sizeof(r));
            if (!r[D_STATUS])
                continue;
            uint32_t mean = r[D_T_PHASE + 4 * PH_TOTAL];
            if (r[D_STATUS] & DS_NODEMO) {
                ui_row(row++, C_ORANGE, "%s: not a demo this DOOM plays", dmc_demo_names[k]);
                continue;
            }
            ui_row(row++, C_WHITE, "%s %u tics (%s) %u fps", dmc_demo_names[k],
                   (unsigned)r[D_V_TICS], end[r[D_V_END] % 5], (unsigned)(mean ? 1000000u / mean : 0));
            ui_row(row++, C_GREY, " hash %08X%08X", (unsigned)r[D_V_CHAIN + 1], (unsigned)r[D_V_CHAIN]);
        }
    }
    ui_row(12, C_YELLOW, "SAVE: play");
    ui_row(13, C_YELLOW, "MODE: run the demos, save the results");
    ui_row(14, C_YELLOW, "HOLD: open the data drive on USB");
    ui_row(15, C_YELLOW, "      (add DEMO*.LMP)");
    ui_footer("hold POWER: off");
    dmc_wait_release();
    for (;;) {
        int b = dmc_wait_press();
        if (b == BTN_SAVE) {
            report_state(RS_PLAY, 0);
            lcd_fill(0, 0, LCD_W - 1, LCD_H - 1, C_BLACK);
            dmc_wait_release();
            snd_volume(dmc_set.sound);
            return 1;
        }
        if (b == BTN_MODE)
            return 0;
        if (b == BTN_HOLD)
            return 2;
    }
}

OVL_TABLE(REPORT, OVL_ENTRY(REPORT_E_BEGIN, report_begin_fn, report_begin),
          OVL_ENTRY(REPORT_E_VERIFIED, report_pass_fn, report_verified),
          OVL_ENTRY(REPORT_E_TIMED, report_pass_fn, report_timed),
          OVL_ENTRY(REPORT_E_STATE, report_state_fn, report_state),
          OVL_ENTRY(REPORT_E_MENU, report_menu_fn, report_menu));
