// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The firmware of the meter (DOOM.BIN). Resident: the boot shell (dmc_main.c), play mode
// and the results loop (fe_play.c), the demo passes (dmc_demo.c), the results file's state and
// crash record (dmc_res.c), the engine's system interface (i_dmc.c) and the front end's glue
// (fe_glue.c, fe.h). Overlays in DOOMCODE.DMC (ids in port/ovl/ovl.h), each reached only
// through the resident code here, in i_dmc.c and in fe_glue.c:
//   BOOT    dmc_boot.c: the data on the NOR, the install flow, the settings stamp, the start
//           of the engine
//   REPORT  dmc_report.c: which demos run, the records and saves of DOOMRES.DAT, the screens
//           between passes
//   HASH    dmc_hash.c with tests/lockstep/hash.c: the hash of every verified tic
//   LEVEL   l_level.c: level setup and the level transitions of the engine
//   LINES   l_lines.c: the line specials, in the middle of a tic
//   MENU    l_menu.c: the menu, its options and settings (fe.h)
//   WI      l_wi.c: the intermission and the end of a level (fe.h)
//   FINALE  l_finale.c: the finale (fe.h)
//   USB     dmc_usb.c with port/usb and platform/usbd.c: the 3 MB data volume as a USB drive,
//           then a reset
//
// RAM outside the fixed core: the frame region at FRAME_BASE is free between frames, while no
// level is set up and between demo passes. BOOT and REPORT run there then, HASH runs between
// the tics of the verification pass (no 3D view) with its scratch behind its window and the
// creation order of the object store under the tic scratch (dmc_demo.c), LINES, LEVEL, WI and
// FINALE may load inside a tic. The front end keeps the state of its screens in the tic
// scratch while no level tics run (fe.h). Every 3D view marks the overlay window lost
// (ovl_invalidate in R_DrawFrame), a frame without one keeps the front end's overlays below
// its compose buffers (fe_frame). DMC_SECTOR_BUF, the
// 4 KB buffer of the DOOMRES.DAT sector and of the directory rename, is behind the smallest
// frame region, where the record cache sits before the first level and the caches of a plan
// may be: its users empty them afterwards (rc_reset, flat_reset). USB uses it as its block
// buffer, with its state behind it (usb_ram), and ends in a reset. The state of the USB
// controller driver (usbd_ram) is in the fixed core, the game pad driver
// (controller/rc-n1/dmc_dji.c) uses the controller in play mode.

#pragma once

#include <stdint.h>

#include "dmc_res.h"
#include "platform.h"

// Link symbols: doom.ld, and the variant of mk/fw.mk (1 meter, 2 renode)
extern uint8_t __frame_start[], __ram_end[], __core_size[], __dmc_variant[];
#define DMC_FRAME_BASE ((uint8_t *)__frame_start)
#define DMC_SECTOR_BUF (DMC_FRAME_BASE + PLAT_FRAME_MIN)

// Test settings in flash, all 0 in DOOM.BIN. The Renode test (tests/renode/test_fw.py) writes
// them when app_main starts.
typedef struct {
    uint16_t exits[12];   // forced exits by leveltime of the k-th level, bit 15 secret, 0 ends
    uint32_t save_tic;    // verification pass: save into slot 0 after D_Display of this tic
    uint8_t load;         // verification pass: slot + 1 to restore right after G_DoPlayDemo
    uint8_t boot_bench;   // boot to the results screen first
    uint8_t demos;        // bit k: run demo k of dmc_demo_names, 0 = all (REPORT_E_BEGIN)
    uint8_t pad;
    uint32_t tics;        // tics per verification pass, 0 for whole demos
    uint32_t frames;      // frames per timed pass, 0 for whole demos
} dmc_test_t;

_Static_assert(sizeof(dmc_test_t) == 40, "tests/renode/test_fw.py writes dmc_test by offset");

extern const volatile dmc_test_t dmc_test;

// ---- settings page: dmc_main.c keeps the mirror, BOOT loads and stamps it,
// the menu (MENU overlay) saves the sound and the messages

#define SET_VERSION 1

typedef struct {
    // install stamp: DOOM.DAT and DOOMX.DAT as they were when the CRCs were checked
    uint32_t doom_size, doom_first, header_crc, doomx_size, doomx_first;
    uint8_t verified;
    uint8_t sound;  // SND_OFF, SND_LOW, SND_HIGH
    uint8_t preset, flip, always_run;
    uint8_t msg_off;  // messages off (the menu's Messages)
    uint8_t pad[2];
} dmc_set_t;

extern dmc_set_t dmc_set;

// ---- dmc_main.c: text screens on the polled LCD

// Clears the screen and shows a title.
void dmc_screen(const char *title);
void dmc_wait_release(void);
// Next button press (BTN_*), keeping the POWER long press (off) alive.
int dmc_wait_press(void);
// Waits for POWER held 2 s and powers off.
__attribute__((noreturn)) void dmc_wait_off(void);
// The data drive on USB (USB_ASIS or USB_ERASE), then a reset.
void data_drive(int how);

// ---- fe_play.c

// The results screen and its choices until SAVE (play), MODE plays the demos, HOLD opens the
// data drive on USB. run: the demos play first (the menu's Benchmark).
void bench(int run);
// Play mode: the title loop and the game in real time, with input and sound.
__attribute__((noreturn)) void play(void);

// ---- dmc_demo.c

// Plays every demo twice (verification and timed pass) and saves the results.
void dmc_run_demos(void);
// Called around the LEVEL overlay (i_dmc.c): the creation order kept for the hash, and the
// plan of each level the verification pass sets up.
void dmc_level_begin(void);
void dmc_level_end(void);
// Rendering on (timed pass, play) or off (verification pass)
extern int dmc_render;

// The demos of a run and the results of its passes: the resident loops fill them, REPORT
// turns them into records. Demo k is dmc_demo_names[k], its record is record k.
extern const char dmc_demo_names[RES_DEMOS][12];

typedef struct {
    uint32_t tics[RES_DEMOS];  // tics of demo k by its size, 0 when it does not run
    uint32_t k;                // demo of the pass
    uint32_t stopped;          // a button stopped the run
    uint32_t ms, pages_peak;
    struct {                   // verification pass
        uint64_t h, c;         // last hash, chain
        uint32_t tics, nchain, first, end, level, frame;
        uint16_t pages, caches, arena;  // of the level of the first tic, as level and frame
    } v;
} dmc_run_t;

extern dmc_run_t dmc_run;
// The verification pass: the hash scratch ends here, below the creation order of the
// object store (dmc_demo.c).
extern uint8_t *dmc_hash_top;

// Statistics of the timed pass (D_T_PHASE): microseconds per phase, the exact 1 ms histogram
// of the frame and bins of a factor 2^(1/2) for the other phases (dmc_res.h), u8 counts that
// are halved together when one would overflow.
#define ST_BINS 19
typedef struct {
    uint32_t sum[PH_COUNT];
    uint32_t max[PH_COUNT];
    uint32_t frames;
    uint16_t hist[102];
    uint8_t bin[PH_TOTAL][ST_BINS];
} dmc_stats_t;

// The results of the timed pass. They take no fixed core: the timed pass makes the object
// store leave their bytes free at the end of the level arena (o_arena_tail,
// port/obj/things.h), 7 or 8 of its pages, from its first level load through the level
// changes of the demo until REPORT has written them.
// Demo hashes do not depend on the page count while the pages last (the host gives the same
// hashes for DEMO1 with 100 of its 148 pages).
typedef struct {
    uint32_t frames, done, nor[4], lcd[2];
    uint32_t view[DMC_VIEW_FRAMES][3];
    dmc_stats_t st;
} dmc_timed_t;

#define DMC_TIMED_BYTES ((sizeof(dmc_timed_t) + 3u) & ~3u)
#define dmc_timed (*(dmc_timed_t *)(void *)(__ram_end - DMC_TIMED_BYTES))
#define dmc_st (dmc_timed.st)
// Lower edge of bin b (b >= 1) of dmc_stats_t.bin in microseconds, bin 0 starts at 0
#define ST_EDGE(b) ((b) & 1 ? 128u << ((b) / 2) : 181u << ((b) / 2 - 1))

// ---- dmc_res.c: DOOMRES.DAT

// NOR address of the file's cluster, 0 when there is none of RES_SIZE bytes
extern uint32_t res_addr;
extern uint32_t res_cur_state, res_cur_demo, res_boot, res_runs, res_saves, res_ndemos;
extern uint32_t res_next_slot;
// Results screens shown since boot
extern uint32_t res_menus;
// Erases the 4 KB NOR sector at addr and programs n bytes of buf into it, then reads them
// back. Returns 0, or -1 on a NOR error or a difference.
int dmc_sector_write(uint32_t addr, const uint8_t *buf, uint32_t n);
// Fault screen hook: the crash record, polled.
void res_crash(void);

// ---- i_dmc.c

// Places the record cache and the flat slots after the frame region of the loaded map's
// plan, checks that the level arena fits behind them.
void dmc_place_caches(void);

// ---- overlay entries

enum { BOOT_E_DATA, BOOT_E_ENGINE };
// How the USB overlay opens the 3 MB data drive
enum { USB_ASIS = 1, USB_ERASE = 2 };
// BOOT_E_DATA: the data on the NOR and the settings. Returns 0 when the engine can start,
// USB_ASIS or USB_ERASE when the user chose to open the data drive on USB (the installer, a
// problem of DOOM.DAT), else shows the problem until the meter is switched off.
typedef int boot_data_fn(void);
// BOOT_E_ENGINE: the start-up of the engine but R_Init and P_Init, which use the frame region
// and the LEVEL overlay.
typedef void boot_fn(void);

enum { USB_E_RUN };
// The 3 MB data drive on USB (USB_ASIS, or USB_ERASE after a quick format) until the PC
// is done with it (stop, eject, configuration 0) or SAVE is pressed, then the folder UI and a
// reset. Does not return.
typedef void usb_run_fn(int how);

enum { REPORT_E_BEGIN, REPORT_E_VERIFIED, REPORT_E_TIMED, REPORT_E_STATE, REPORT_E_MENU };
// The demos of a new run into dmc_run.tics, the header of the run. Returns how many it found.
typedef int report_begin_fn(void);
// Builds and saves the record of the pass that just ended (dmc_run, dmc_st).
typedef void report_pass_fn(void);
// Header state (RS_*) and its demo, saved.
typedef void report_state_fn(int state, int demo);
// The results of the run and the choice: 1 SAVE play (the state saved, the screen cleared,
// the volume set), 0 MODE the demos again, 2 HOLD the data drive on USB.
typedef int report_menu_fn(void);

enum { HASH_E_TIC };
// Hash of the tic that just ran, chained into dmc_run, the progress every RES_CHAIN_TICS.
typedef void hash_tic_fn(void);
