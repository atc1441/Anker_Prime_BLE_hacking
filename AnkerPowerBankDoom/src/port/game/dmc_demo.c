// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Demo verification and timed demos, the resident loops. Every demo (DEMO1 of the image, then
// DEMO2.LMP .. DEMO4.LMP from the root of the data drive, streamed by g_game.c) is played
// twice from gametic 0, the way the host engine plays it with -playdemo: d_loop.c's RunTic in
// singletics mode for the first tic, then every tic followed by S_UpdateSounds and D_Display
// (D_DoomLoop).
//   verification  the 3D view and the status band are not drawn (the rest of D_Display runs,
//                 it moves the menu random index), the HASH overlay takes the lockstep hash of
//                 tests/lockstep/hash.c (resting corpses skipped) after every tic and chains
//                 it into DOOMRES.DAT (dmc_res.h)
//   timed         every tic is drawn, as vanilla -timedemo does, and DWT times the phases of
//                 each frame (D_T_PHASE). The first DMC_VIEW_FRAMES frames are hashed instead
//                 (FNV-1a 64 over the 3D view, column by column from the left, rows 0..199 of
//                 each physical column, palette indices) and not timed.
// The buzzer is muted during both, the sound code runs. Between tics POWER held 2 s switches
// the meter off and MODE stops the run. A pass ends at the demo's end marker, a level exit
// goes on into the intermission as in vanilla. The REPORT overlay picks the demos and writes
// the records. The probes below wrap the engine's and the platform's calls (ld.lld --wrap,
// mk/fw.mk) and run in play mode too, as do the forced exits of the test settings
// (dmc_test). The NOR driver counts and times its own calls (nor_stats). The test save and
// load of those settings run in the verification pass: the save into slot 0 after the display
// of a tic, the load right after the demo has set up its level (port/game/i_dmc.c), both as on
// the host (--save-at, --load).

#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_loop.h"
#include "d_main.h"
#include "g_game.h"
#include "i_system.h"
#include "p_local.h"
#include "r_local.h"
#include "s_sound.h"

#include "dmc_fw.h"
#include "cache.h"
#include "extents.h"
#include "fat12.h"
#include "fe.h"
#include "hash.h"
#include "lcd.h"
#include "nor.h"
#include "ovl.h"
#include "r_strip.h"
#include "rec.h"
#include "things.h"
#include "ui.h"

extern boolean advancedemo;
void D_Display(void);
extern ticcmd_t *netcmds;

const volatile dmc_test_t dmc_test;
int dmc_render = 1;
dmc_run_t dmc_run;
uint8_t *dmc_hash_top;
const char dmc_demo_names[RES_DEMOS][12] = {"DEMO1", "DEMO2.LMP", "DEMO3.LMP", "DEMO4.LMP"};
static int verifying;

// ---- probes

static uint32_t pr_lcd, pr_pass1, pr_view, pr_band;
static uint8_t in_lcd, in_view;
static int view_frame;  // 1.. while the view hash of a frame is taken
static uint64_t view_hash;

void __real_pump_sync(int max);
void __real_pump_submit(const uint8_t *cols, int ncols);
void __real_pump_end(void);
void __real_R_RenderPlayerView(player_t *player);
void __real_R_RenderBSPPass(void);
void __real_R_BandFlush(int hud, int bar);
void __real_G_Ticker(void);

#define LCD_IN() uint32_t t0 = plat_cycles(); in_lcd++
#define LCD_OUT() if (!--in_lcd) pr_lcd += plat_cycles() - t0

void __wrap_pump_sync(int max)
{
    LCD_IN();
    __real_pump_sync(max);
    LCD_OUT();
}

void __wrap_pump_submit(const uint8_t *cols, int ncols)
{
    if (in_view && view_frame)
        for (int i = 0; i < ncols; i++)
            for (int r = 0; r <= detailshift; r++)
                for (int y = 0; y < R_VIEWH; y++)
                    view_hash = (view_hash ^ cols[i * R_VIEWH + y]) * LS_FNV_PRIME;
    LCD_IN();
    __real_pump_submit(cols, ncols);
    LCD_OUT();
}

void __wrap_pump_end(void)
{
    LCD_IN();
    __real_pump_end();
    LCD_OUT();
}

void __wrap_R_RenderPlayerView(player_t *player)
{
    if (!dmc_render)
        return;
    uint32_t t0 = plat_cycles();
    in_view = 1;
    __real_R_RenderPlayerView(player);
    in_view = 0;
    pr_view += plat_cycles() - t0;
}

void __wrap_R_RenderBSPPass(void)
{
    uint32_t t0 = plat_cycles();
    __real_R_RenderBSPPass();
    pr_pass1 += plat_cycles() - t0;
}

void __wrap_R_BandFlush(int hud, int bar)
{
    if (!dmc_render)
        return;
    uint32_t t0 = plat_cycles();
    __real_R_BandFlush(hud, bar);
    pr_band += plat_cycles() - t0;
}

// The forced exit of the k-th level by its leveltime, before the tic as the oracle's
// --exit-list does it (tests/lockstep/play.h)
void __wrap_G_Ticker(void)
{
    uint32_t e = fe.exit_k < arrlen(dmc_test.exits) ? dmc_test.exits[fe.exit_k] : 0;
    if (e && gamestate == GS_LEVEL && leveltime == (int)(e & 0x7FFFu)) {
        fe.exit_k++;
        if (e & 0x8000u)
            G_SecretExitLevel();
        else
            G_ExitLevel();
    }
    __real_G_Ticker();
}

// ---- timed statistics (dmc_stats_t)

static void st_add(const uint32_t *us)
{
    dmc_stats_t *st = &dmc_st;
    for (int p = 0; p < PH_COUNT; p++) {
        st->sum[p] += us[p];
        if (us[p] > st->max[p])
            st->max[p] = us[p];
        if (p == PH_TOTAL)
            break;
        unsigned b = 0;
        while (b + 1 < ST_BINS && us[p] >= ST_EDGE(b + 1))
            b++;
        uint8_t *bin = st->bin[p];
        if (bin[b] == 0xFF)
            for (int i = 0; i < ST_BINS; i++)
                bin[i] >>= 1;
        bin[b]++;
    }
    uint32_t ms = us[PH_TOTAL] / 1000u;
    uint16_t *h = &st->hist[ms > 100u ? 100u : ms];
    if (*h != 0xFFFF)
        (*h)++;
    st->frames++;
}

static uint32_t us_of(uint32_t cycles)
{
    return cycles / (PLAT_CPU_HZ / 1000000u);
}

// ---- demos

// 8.3 directory form of a file name
static void name83(const char *s, char *o)
{
    memset(o, ' ', 11);
    for (int i = 0, k = 0; s[i] && k < 11; i++) {
        if (s[i] == '.')
            k = 8;
        else
            o[k++] = s[i] >= 'a' && s[i] <= 'z' ? (char)(s[i] - 32) : s[i];
    }
}

int dmc_demo_open(const char *name, uint32_t *size)
{
    fat_vol_t v;
    char n[11];
    name83(name, n);
    if (fat_mount(&v, DMC_VOL_MAIN) || fat_open(&v, n, &dmc_files[DMC_F_DEMO]))
        return -1;
    *size = dmc_files[DMC_F_DEMO].size;
    return 0;
}

void dmc_demo_read(uint32_t off, void *dst, uint32_t n)
{
    if (ext_read(DMC_F_DEMO, off, dst, n))
        I_Error("demo read at %u + %u", (unsigned)off, (unsigned)n);
}

static int demo_started;

// One tic as RunTic of d_loop.c runs it with singletics, no input. Returns 0 when the demo
// ended in it: that tic is not part of the demo (the host's engine quits inside it).
static int demo_tic(void)
{
    ticcmd_t cmd;
    G_BuildTiccmd(&cmd, gametic);
    netcmds = &cmd;
    if (advancedemo)
        D_DoAdvanceDemo();
    G_Ticker();
    if (!demoplayback)
        return 0;
    demo_started = 1;
    return 1;
}

// Starts demo k at gametic 0.
static void demo_start(int k)
{
    gametic = 0;
    demo_started = 0;
    fe.exit_k = 0;
    advancedemo = false;
    singletics = true;
    dmc_run.k = res_cur_demo = (uint32_t)k;
    dmc_run.pages_peak = 0;
    dmc_run.ms = plat_ms();
    G_DeferedPlayDemo((char *)dmc_demo_names[k]);
}

static void demo_stop(void)
{
    advancedemo = false;
    demoplayback = false;
    dmc_run.ms = plat_ms() - dmc_run.ms;
    if (dmc_render)
        R_PumpIdle();
}

// Between tics: POWER held 2 s powers off, MODE stops the run. Returns 1 to stop.
static int poll_stop(void)
{
    int ev;
    if (ui_power_held(2000))
        plat_power_off();
    while ((ev = plat_event()) >= 0)
        if ((ev & EV_PRESS) && (ev & 7) == BTN_MODE)
            dmc_run.stopped = 1;
    return dmc_run.stopped;
}

static void pages_note(void)
{
    uint32_t used = (uint32_t)o_pool.npages - o_pool.nfree;
    if (used > dmc_run.pages_peak)
        dmc_run.pages_peak = used;
}

void dmc_level_begin(void)
{
    o_seq_page = NULL;
    o_seq_drop = NULL;
}

// The verification pass keeps the creation order of the level's objects and drops for the
// hash under the tic scratch, the hash scratch ends below it. Bits 28-31 of the record's
// level word note the plans of the levels the pass set up.
void dmc_level_end(void)
{
    if (!verifying)
        return;
    dmc_run.v.level |= 1u << (28 + M.plan.plan);
    uint32_t n = (uint32_t)o_pool.npages + M.plan.drop_slots + 1u;
    unsigned *seq = (unsigned *)(void *)((uint8_t *)&dmc_tic - 4u * n);
    // the overlays that load while tics run end below it
    if ((uint8_t *)seq < DMC_FRAME_BASE + OVL_WIN_TIC)
        I_Error("hash order: %u slots", (unsigned)n);
    memset(seq, 0, 4u * n);
    o_seq_page = seq;
    o_seq_drop = seq + o_pool.npages;
    dmc_hash_top = (uint8_t *)seq;
}

static void verify_pass(int k)
{
    dmc_run_t *r = &dmc_run;

    snd_volume(SND_OFF);
    verifying = 1;
    dmc_render = 0;
    o_arena_tail = 0;
    memset(&r->v, 0, sizeof(r->v));
    r->v.c = LS_FNV_OFFSET;
    r->v.first = res_next_slot;
    // the object store counts on through the levels of the pass (O_PagesInit)
    memset(o_stats.events, 0, sizeof(o_stats.events));
    demo_start(k);
    for (;;) {
        if (dmc_test.tics && r->v.tics >= dmc_test.tics) {
            r->v.end = DE_LIMIT;
            break;
        }
        if (poll_stop()) {
            r->v.end = DE_STOP;
            break;
        }
        if (!demo_tic()) {
            r->v.end = DE_MARKER;
            break;
        }
        if (!r->v.tics) {
            r->v.level = (r->v.level & 0xF0000000u) | (uint32_t)gameepisode
                | (uint32_t)gamemap << 8 | (uint32_t)gameskill << 16
                | (uint32_t)M.plan.plan << 24;
            R_FrameRegion(&r->v.frame);
            r->v.pages = (uint16_t)o_pool.npages;
            r->v.caches = (uint16_t)(RC_BYTES + M.plan.k * DMC_FLAT_SIZE);
            r->v.arena = M.plan.arena_bytes;
        }
        OVL_CALL(OVL_HASH, HASH_E_TIC, hash_tic_fn);
        pages_note();
        gametic++;
        // no 3D view or band: D_Display leaves the frame region to the overlays. As on the
        // host, every tic but the demo's first is followed by one, the first tic after a test
        // load too.
        if (gametic > 1) {
            S_UpdateSounds(players[consoleplayer].mo);
            D_Display();
        }
        if (dmc_test.save_tic && gametic == (int)dmc_test.save_tic + 1)
            OVL_CALL(OVL_SAVEG, SAVE_E_WRITE, save_fn, 0, DMC_SF_EXACT);
    }
    demo_stop();
    verifying = 0;
    dmc_level_begin();
    res_next_slot = r->v.first + r->v.nchain;
    if (!demo_started)
        r->tics[k] = 0;
    OVL_CALL(OVL_REPORT, REPORT_E_VERIFIED, report_pass_fn);
}

// The view hash of the frame. The first DMC_VIEW_FRAMES frames have a slot each.
static void view_note(void)
{
    uint32_t f = dmc_timed.frames;
    uint32_t *v = dmc_timed.view[f];
    v[0] = (uint32_t)gametic;
    v[1] = (uint32_t)view_hash;
    v[2] = (uint32_t)(view_hash >> 32);
}

static void timed_pass(int k)
{
    uint32_t us[PH_COUNT];
    uint32_t sync0 = nor_stats.sync, async0 = nor_stats.async, bytes0 = nor_stats.bytes;
    uint32_t chunks0 = nor_stats.async_chunks;
    uint32_t irq0 = pump_stats.irqs, restart0 = pump_stats.restarts;

    snd_volume(SND_OFF);
    dmc_render = 1;
    o_arena_tail = DMC_TIMED_BYTES;
    demo_start(k);
    lcd_fill(0, 0, LCD_W - 1, LCD_H - 1, 0);
    // the whole status band is sent again on the first frame
    R_PaletteChanged();
    // the first tic loads the level, the results are behind its pages from then on
    int started = demo_tic();
    memset(&dmc_timed, 0, sizeof(dmc_timed));
    if (started) {
        gametic++;
        for (;;) {
            if ((dmc_test.frames && dmc_timed.frames >= dmc_test.frames) || poll_stop())
                break;
            pr_lcd = pr_pass1 = pr_view = pr_band = 0;
            uint32_t wait0 = nor_stats.wait;
            view_frame = dmc_timed.frames < DMC_VIEW_FRAMES ? (int)dmc_timed.frames + 1 : 0;
            view_hash = LS_FNV_OFFSET;
            uint32_t c0 = plat_cycles();
            if (!demo_tic()) {
                dmc_timed.done = 1;
                break;
            }
            uint32_t c1 = plat_cycles();
            gametic++;
            S_UpdateSounds(players[consoleplayer].mo);
            D_Display();
            uint32_t c2 = plat_cycles();
            pages_note();
            if (view_frame) {
                view_note();
            } else {
                us[PH_TIC] = us_of(c1 - c0);
                us[PH_PASS1] = us_of(pr_pass1);
                us[PH_STRIPS] = us_of(pr_view - pr_pass1);
                us[PH_BAND] = us_of(pr_band);
                us[PH_NOR] = us_of(nor_stats.wait - wait0);
                us[PH_LCD] = us_of(pr_lcd);
                us[PH_TOTAL] = us_of(c2 - c0);
                st_add(us);
            }
            dmc_timed.frames++;
        }
    }
    view_frame = 0;
    demo_stop();
    dmc_timed.nor[0] = nor_stats.sync - sync0;
    dmc_timed.nor[1] = nor_stats.async - async0;
    dmc_timed.nor[2] = nor_stats.bytes - bytes0;
    dmc_timed.nor[3] = nor_stats.async_chunks - chunks0;
    dmc_timed.lcd[0] = pump_stats.irqs - irq0;
    dmc_timed.lcd[1] = pump_stats.restarts - restart0;
    OVL_CALL(OVL_REPORT, REPORT_E_TIMED, report_pass_fn);
    o_arena_tail = 0;
}

void dmc_run_demos(void)
{
    dmc_run.stopped = 0;
    int n = OVL_CALL(OVL_REPORT, REPORT_E_BEGIN, report_begin_fn);
    for (int k = 0; k < RES_DEMOS && !dmc_run.stopped; k++) {
        if (!dmc_run.tics[k])
            continue;
        verify_pass(k);
        if (dmc_run.tics[k] && !dmc_run.stopped)
            timed_pass(k);
    }
    OVL_CALL(OVL_REPORT, REPORT_E_STATE, report_state_fn, RS_DONE, n);
}
