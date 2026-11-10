// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// DOOMRES.DAT, the results file of the DOOM firmware on the firmware drive. The user creates it
// on the PC (RES_SIZE bytes, any content, make fw-package writes a zeroed one) and the
// firmware rewrites its cluster in place, never the directory entry or the FAT. The update
// drive must have 4 KB clusters. tests/device/doomres.py decodes it: it parses the defines
// below, so keep one "#define NAME value" per line. All fields are little-endian u32 words.
//
// One 4 KB sector, rewritten (erase, program, read back) after each pass with what it held
// before, so nothing is lost but the header, the record and the crash record being written:
//   RES_HDR_OFF    header (H_*)
//   RES_DEMO_OFF   one record per demo (D_*)
//   RES_CRASH_OFF  crash record (C_*), written by the fault screen
//   RES_CHAIN_OFF  chained hashes of the verification passes, RES_CHAIN_MAX u32 slots shared
//                  by the demos in turn (demo k from slot D_V_FIRST on), erased when a run
//                  starts and programmed one slot at a time while a pass runs
//
// Chained hash: c = LS_FNV_OFFSET, then for every tic of the pass the 64-bit lockstep hash
// of that tic (tests/lockstep/hash.h, resting corpses skipped) is fed with LS_Feed, low word
// first. After every RES_CHAIN_TICS tics (gametic + 1 a multiple of it) the low word of c is
// stored. The record keeps the last tic's hash and c after it, 64 bits each.

#pragma once

#define RES_MAGIC        0x32524D44u  // "DMR2"
#define RES_VERSION      2
#define RES_SIZE         4096
#define RES_SECTOR       4096
#define RES_DEMOS        4     // DEMO1 of the image, then DEMO2.LMP .. DEMO4.LMP
#define RES_HDR_OFF      0
#define RES_DEMO_OFF     128   // byte offset of the record of demo 0
#define RES_DEMO_BYTES   512
#define RES_CRASH_OFF    2176
#define RES_CRASH_MAGIC  0x43524D44u  // "DMRC"
#define RES_CHAIN_OFF    2304
#define RES_CHAIN_MAX    448
#define RES_CHAIN_TICS   35

// Header words
#define H_MAGIC          0
#define H_VERSION        1
#define H_SIZE           2
#define H_SAVES          3   // saves since the run started
#define H_UPTIME         4   // ms at the last save
#define H_STATE          5   // RS_*
#define H_DEMO           6   // demo of the state
#define H_NDEMOS         7   // demos found (records used)
#define H_BUILD          8   // 5 words: firmware build ID
#define H_UUID           13  // 4 words: image uuid
#define H_TEST_TICS      17  // tics per verification pass in the Renode test, 0 = whole demos
#define H_CORE           18  // fixed core (.data + .bss) in bytes
#define H_FRAME_BASE     19
#define H_RAM_END        20
#define H_STACK          21  // stack high-water mark so far, bytes
#define H_VARIANT        22  // link variant of mk/fw.mk: 1 meter, 2 renode
#define H_RUNS           23  // runs of the demos since boot
#define H_TEST_FRAMES    24  // frames per timed pass in the Renode test, 0 = whole demos
#define H_BOOT           25  // boots of the firmware that found this file, this one included
#define H_WORDS          32

#define RS_RUN           1   // demos running
#define RS_DONE          2   // all demos done
#define RS_PLAY          3   // play mode

// Demo record words, at RES_DEMO_OFF + k * RES_DEMO_BYTES
#define D_NAME           0   // 3 words: "DEMO1", "DEMO2.LMP", NUL padded
#define D_STATUS         3   // DS_* bits
#define D_LEVEL          4   // episode | map << 8 | skill << 16 | plan << 24 (0 = A .. 3 = D)
                             // of the first tic, bit 28 + plan for each plan the pass set up
#define D_V_TICS         5   // tics hashed in the verification pass
#define D_V_END          6   // DE_*
#define D_V_HASH         7   // 2 words: hash of the last tic
#define D_V_CHAIN        9   // 2 words: c after the last tic
#define D_V_NCHAIN       11  // chained values stored
#define D_V_MS           12  // pass time
#define D_V_EVENTS       13  // 5 words: counted events of the object store (EV_*) in the pass
#define D_V_PAGES        18  // pages of the plan of D_LEVEL | peak pages in use << 16
#define D_V_RAM          19  // frame region bytes of that plan | caches bytes << 16
#define D_V_ARENA        20  // level arena bytes of that plan
#define D_V_FIRST        21  // first chain slot of the demo
#define D_T_FRAMES       22  // frames timed (the first DMC_VIEW_FRAMES are hashed, not timed)
#define D_T_MS           23  // pass time
#define D_T_NOR          24  // 4 words: sync reads, queued reads, bytes, DMA chunks
#define D_T_STACK        28  // stack high-water mark after the pass, bytes
#define D_T_PAGES        29  // peak pages in use
#define D_T_LCD          30  // 2 words: pump interrupts, pump restarts over the pass
#define D_T_PHASE        32  // PH_COUNT x 4 words: mean, p50, p95, max in microseconds
#define D_T_HIST         60  // 51 words, 102 u16: frame time in 1 ms bins 0..99, 100 more
#define D_T_VIEW         111 // DMC_VIEW_FRAMES x 3 words: gametic, view hash low, high
#define D_WORDS          123

#define DS_VERIFIED      1   // verification pass ran to its end
#define DS_TIMED         2   // timed pass ran to its end
#define DS_TRUNC         4   // more chained values than free slots
#define DS_NODEMO        8   // not a demo this firmware plays (version, players, map)
#define DS_STOPPED       16  // a button stopped the passes

#define DE_MARKER        1   // the demo's end marker
#define DE_EXIT          2   // a level exit, never written by this firmware
#define DE_LIMIT         3   // H_TEST_TICS reached
#define DE_STOP          4   // a button press

// Phases of a timed frame (D_T_PHASE). NOR and LCD are the blocking waits of the whole frame,
// strips is the 3D view without pass 1. Mean and max are exact. p50 and p95 of the total come
// from the 1 ms histogram (the upper edge of the bin), those of the other phases from bins of
// a factor 2^(1/2) above 128 us, interpolated inside the bin: within about 20 %.
#define PH_TIC           0
#define PH_PASS1         1
#define PH_STRIPS        2
#define PH_BAND          3
#define PH_NOR           4
#define PH_LCD           5
#define PH_TOTAL         6
#define PH_COUNT         7

#define DMC_VIEW_FRAMES  4

// Crash record words, at RES_CRASH_OFF
#define C_MAGIC          0
#define C_UPTIME         1
#define C_EXC            2   // exception number, 0 for a panic (I_Error)
#define C_PC             3
#define C_LR             4
#define C_CFSR           5
#define C_HFSR           6
#define C_BFAR           7
#define C_MMFAR          8
#define C_STATE          9
#define C_DEMO           10
#define C_GAMETIC        11
#define C_STACK          12
#define C_MSG            13  // 16 words: the panic message, NUL padded
#define C_BOOT           29  // H_BOOT of the boot that crashed
#define C_WORDS          30
