// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
// Copyright (C) 2026 VapeDoom port
//
// Overlay loader replacement: everything is resident.
//
// The DMC-100 keeps ten parts of the program in a file on its flash and
// copies the one it needs into the frame region, because its microcontroller
// has a 99 KB slot for the whole image. This board has 254 KB, so the entire
// program fits with room to spare and there is nothing to swap.
//
// That is not only simpler, it is better here: the overlay file lives on the
// NOR, and on this board the NOR shares its bus and its data line with the
// panel. Loading an overlay would mean stopping a frame mid-pixel to fetch
// code. Resident code never does that.
//
// The entry tables the port declares with OVL_TABLE are linked in as plain
// arrays (the form the host build uses), so OVL_CALL resolves to a direct
// lookup and the call is an ordinary indirect call.

#include "ovl.h"
#include "platform.h"

const uint16_t ovl_win[OVL_IDS] = {
#define X(name, win) [OVL_##name] = win,
    OVL_LIST(X)
#undef X
};

// The tables are weak: an overlay whose sources this build does not link
// simply has none, and asking for it fails instead of jumping into nothing.
#define X(name, win)                                            \
    extern const ovl_fn ovl_tab_##name[] __attribute__((weak)); \
    extern const uint16_t ovl_cnt_##name __attribute__((weak));
OVL_LIST(X)
#undef X

static const ovl_fn *res_tab(int id, unsigned *n)
{
    *n = 0;
    switch (id) {
#define X(name, win)              \
    case OVL_##name:              \
        if (!ovl_tab_##name)      \
            return 0;             \
        *n = ovl_cnt_##name;      \
        return ovl_tab_##name;
        OVL_LIST(X)
#undef X
    default:
        break;
    }
    return 0;
}

uint16_t ovl_loads;

// Normally stamped into the image by tools/pack_overlays.py so that a loaded
// overlay and the resident part can be checked against each other. Nothing is
// loaded here, so a fixed value is enough; SAVE.DAT still gets a stable tag.
const volatile uint8_t dmc_build_id[20] = {'V', 'A', 'P', 'E', 'D', 'O', 'O', 'M', '1', 0,
                                           0,   0,   0,   0,   0,   0,   0,   0,   0,   0};

static ovl_fail_fn *fail_hook;

int ovl_load(int id)
{
    unsigned n;
    if (id <= 0 || id >= OVL_IDS)
        return OVL_ENOENT;
    return res_tab(id, &n) ? OVL_OK : OVL_ENOENT;
}

ovl_fn ovl_entry(int id, int idx)
{
    unsigned n;
    const ovl_fn *t = res_tab(id, &n);
    if (!t || (unsigned)idx >= n) {
        char m[64];
        if (fail_hook)
            fail_hook(id, t ? OVL_EENTRY : OVL_ENOENT);
        plat_fmt(m, sizeof m, "resident overlay %d entry %d missing", id, idx);
        plat_panic(m);
    }
    return t[idx];
}

int ovl_resident(void) { return 0; }
void ovl_invalidate(void) {}
void ovl_set_fail_hook(ovl_fail_fn *fn) { fail_hook = fn; }

// DOOMCODE.DMC accessors of ovl.h: there is no such file in this build.
int32_t ovl_file_size(void) { return -1; }
void ovl_file_read(uint32_t off, void *dst, uint32_t n)
{
    (void)off;
    (void)dst;
    (void)n;
}
