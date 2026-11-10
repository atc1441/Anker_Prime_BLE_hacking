// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Code overlays. Cold code lives in DOOMCODE.DMC on the NOR and
// runs from the frame region. Each overlay is linked on its own at FRAME_BASE against the
// resident image (main.elf, ld.lld --just-symbols), so it calls resident functions and uses
// resident data directly. Resident code enters an overlay only through the entry table at the
// start of its blob:
//
//   typedef boolean menu_resp_fn(event_t *ev)
//   r = OVL_CALL(OVL_MENU, MENU_E_RESPONDER, menu_resp_fn, ev)
//
// OVL_CALL loads the overlay when it is not resident. A failed load calls the hook set with
// ovl_set_fail_hook (the build-id mismatch screen), then plat_panic.
//
// Rules:
// - The frame region is shared with the renderer and the tic scratch (FRAME_BASE + 6,912 and
//   up in plan D). Code that writes into an overlay window calls ovl_invalidate() first. So
//   no tic may run while an overlay with an 8,368 B window is cached unless the tic scratch
//   owner calls ovl_invalidate() before using it. A load waits for queued NOR reads and for
//   the LCD pump, then overwrites FRAME_BASE up to the overlay's copy + bss size.
// - Overlays have no .data or .bss of their own (pack_overlays.py refuses them), so they
//   behave the same on the target and on the host. State lives in resident structs. Resident
//   code keeps no pointer into an overlay after the call returns.
// - Overlays never reach the loader (ovl_load, ovl_entry, ovl_invalidate), directly or
//   through resident code, so no overlay loads another one over itself and nothing marks the
//   window free while it runs. The same holds for resident code that writes the frame region
//   (pack_overlays.py --forbid names it). Calls through function pointers read from data
//   tables or set at run time are not checked, the author keeps them clean.
// - Overlays use no resident object inside the frame region. They get frame region buffers
//   (the automap compose strip, WI and FINALE buffers) from linker symbols such as
//   __frame_start, or from resident pointers, and keep them above their own window.
// - OVL_CALL arguments must not load an overlay: C does not order the entry lookup and the
//   arguments, so a nested load could replace the overlay whose entry was already fetched.
//   Call each entry from a resident wrapper function, e.g. M_Responder(ev) returning
//   OVL_CALL(OVL_MENU, MENU_E_RESPONDER, menu_resp_fn, ev): the wrapper's arguments are evaluated
//   before it runs.
// - Host builds link the overlays into the binary (OVL_TABLE registers the native table) and
//   OVL_CALL calls through it. With ovl_host_copy = 1 a load also reads and checks
//   DOOMCODE.DMC like the target and copies the blob into ovl_host_frame (format test).

#pragma once

#include <stdint.h>

// Overlay ids (u8 in DOOMCODE.DMC). Plain numbers: OVL_TABLE stringifies them. The ids of
// the tests/ovl demo overlays exist only in builds with -DOVL_DEMO, those of the firmware's
// own overlays (port/game/dmc_fw.h) only in its builds (-DDMC_FW). The front end's overlays
// (MENU, WI, FINALE, SAVEG) have their ids in every build.
#define OVL_LEVEL   1
#define OVL_MENU    2
#define OVL_WI      3
#define OVL_FINALE  4
#define OVL_AUTOMAP 5
#if defined(DMC_FW)
#define OVL_BOOT    6
#define OVL_REPORT  7
#define OVL_HASH    8
#define OVL_LINES   9
#define OVL_USB     10
#elif defined(OVL_DEMO)
#define OVL_TEST_A  6
#define OVL_TEST_B  7
#endif
#define OVL_SAVEG   11
#define OVL_IDS     12

// Windows from FRAME_BASE: the plan D frame region (FRAME_MIN in platform/link/regions.ld),
// and for the automap that minus the tic scratch (1,456 B) and one compose strip (1,600 B),
// which follow it while tics run. Overlays of the firmware that load while tics run stay
// below the tic scratch and the creation order of the object store that its verification
// pass keeps under it (at most 1,204 B on the E1 maps), the hash leaves its scratch behind.
// The front end's overlays (OVL_WIN_FE) stay below it too, the intermission and the finale
// run inside that pass.
#define OVL_WIN_FRAME   8368u
#define OVL_WIN_AUTOMAP 5312u
#define OVL_WIN_TIC     5696u
#define OVL_WIN_HASH    3072u
#define OVL_WIN_FE      OVL_WIN_TIC
// The firmware's level setup stays below the tic scratch (FRAME_BASE + 6,912)
#ifdef DMC_FW
#define OVL_WIN_LEVEL   6912u
#else
#define OVL_WIN_LEVEL   OVL_WIN_FRAME
#endif

#define OVL_LIST(X)                \
    X(LEVEL, OVL_WIN_LEVEL)        \
    X(MENU, OVL_WIN_FE)            \
    X(WI, OVL_WIN_FE)              \
    X(FINALE, OVL_WIN_FE)          \
    X(AUTOMAP, OVL_WIN_AUTOMAP)    \
    X(SAVEG, OVL_WIN_FE)           \
    OVL_LIST_EXTRA(X)

#if defined(DMC_FW)
#define OVL_LIST_EXTRA(X) X(BOOT, OVL_WIN_FRAME) X(REPORT, OVL_WIN_FRAME) X(HASH, OVL_WIN_HASH) \
    X(LINES, OVL_WIN_TIC) X(USB, OVL_WIN_FRAME)
#elif defined(OVL_DEMO)
#define OVL_LIST_EXTRA(X) X(TEST_A, OVL_WIN_FRAME) X(TEST_B, OVL_WIN_FRAME)
#else
#define OVL_LIST_EXTRA(X)
#endif

// Window per id, 0 for ids the firmware does not know. pack_overlays.py reads it from
// main.elf.
extern const uint16_t ovl_win[OVL_IDS];

// Load results
enum {
    OVL_OK = 0,
    OVL_ENOFILE = -1,  // no DOOMCODE.DMC
    OVL_EHEADER = -2,  // bad magic, format, sizes or header CRC
    OVL_EBUILD = -3,   // made for another firmware build
    OVL_ENOENT = -4,   // overlay not in the file (or unknown id)
    OVL_EBLOB = -5,    // blob header wrong or too big for its window
    OVL_ECRC = -6,     // blob CRC error
    OVL_EENTRY = -7,   // entry index out of range (OVL_CALL only)
};

typedef void (*ovl_fn)(void);

// Makes overlay id resident. Returns OVL_OK at once when it already is. On an error after
// the copy started no overlay is resident.
int ovl_load(int id);
// Entry idx of overlay id, loading it first if needed. Does not return on errors.
ovl_fn ovl_entry(int id, int idx);
// Resident overlay id, 0 for none.
int ovl_resident(void);
// Marks the frame region as overwritten.
void ovl_invalidate(void);

// Called with the id and the error when OVL_CALL cannot load its overlay. It may draw its own
// screen and wait for POWER, if it returns the fault screen shows the error.
typedef void ovl_fail_fn(int id, int err);
void ovl_set_fail_hook(ovl_fail_fn *fn);

// Completed loads since boot
extern uint16_t ovl_loads;

// type is a function type name, as menu_resp_fn above
#define OVL_FN(id, idx, type) ((type *)ovl_entry((id), (idx)))
#define OVL_CALL(id, idx, type, ...) (OVL_FN(id, idx, type)(__VA_ARGS__))

// Firmware build ID: SHA-1 of the resident image with these 20 bytes zeroed, written into
// main.elf by "pack_overlays.py stamp". DOOMCODE.DMC carries the same bytes, SAVE.DAT the
// first 8.
#ifdef PLAT_HOST
extern uint8_t dmc_build_id[20];
#else
extern const volatile uint8_t dmc_build_id[20];
#endif

// ---- Overlay side ----
// Every overlay defines its entry table once, in one of its files. On the target it is the
// start of the blob and also defines __ovl_id for the packer. OVL_ENTRY fails to compile when
// fn does not have the type the resident callers use.
//
//   OVL_TABLE(MENU, OVL_ENTRY(MENU_E_RESPONDER, menu_resp_fn, M_OvlResponder), ...)

#define OVL_ENTRY(idx, type, fn) [idx] = (ovl_fn)_Generic((fn), type *: (fn))

#define OVL_STR_(x) #x
#define OVL_STR(x) OVL_STR_(x)
#if defined(PLAT_HOST) || defined(OVL_RESIDENT)
#define OVL_TABLE(name, ...)                              \
    const ovl_fn ovl_tab_##name[] = {__VA_ARGS__};        \
    const uint16_t ovl_cnt_##name = (uint16_t)(sizeof(ovl_tab_##name) / sizeof(ovl_fn))
#else
#define OVL_TABLE(name, ...)                                              \
    __asm__(".globl __ovl_id\n.set __ovl_id, " OVL_STR(OVL_##name));      \
    __attribute__((section(".ovl_entries"), used)) const ovl_fn ovl_tab_##name[] = {__VA_ARGS__}
#endif

// ---- DOOMCODE.DMC access ----
// port/ovl/ovl_file.c reads the file through the data layer's FAT extents (the firmware),
// port/ovl/ovl_fixed.c from the NOR offset ovl_fixed_base (tests without a FAT volume).

// Size of DOOMCODE.DMC (an upper bound is enough), or -1 when there is none.
int32_t ovl_file_size(void);
// Reads n bytes at file offset off, splitting at extent boundaries.
void ovl_file_read(uint32_t off, void *dst, uint32_t n);
extern uint32_t ovl_fixed_base;

#ifdef PLAT_HOST
// Format test switch (see above). The blob's vma must equal ovl_host_vma, which is the
// target's FRAME_BASE (__frame_start of main.elf).
extern int ovl_host_copy;
extern uint32_t ovl_host_vma;
extern uint8_t ovl_host_frame[OVL_WIN_FRAME];
#endif
