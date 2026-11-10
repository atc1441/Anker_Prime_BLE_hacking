// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// DOOMRES.DAT on the firmware drive (dmc_res.h), the resident part: its state, the sector write
// that the overlays use too, and the crash record of the fault screen. The REPORT overlay
// (dmc_report.c) writes the header and the records, BOOT (dmc_boot.c) finds the file.

#include <string.h>

#include "dmc_fw.h"
#include "nor.h"

uint32_t res_addr;
uint32_t res_cur_state, res_cur_demo, res_boot, res_runs, res_saves, res_ndemos;
uint32_t res_menus;
uint32_t res_next_slot;

int dmc_sector_write(uint32_t addr, const uint8_t *buf, uint32_t n)
{
    uint8_t r[64];
    if (nor_erase4k(addr) || nor_program(addr, buf, n))
        return -1;
    for (uint32_t o = 0; o < n; o += sizeof(r)) {
        uint32_t k = n - o < sizeof(r) ? n - o : sizeof(r);
        nor_read(addr + o, r, k);
        if (memcmp(r, buf + o, k))
            return -1;
    }
    return 0;
}

// The crash record goes into the sector with what it held: the frame region is free now.
void res_crash(void)
{
    struct plat_fault f;
    extern int gametic;
    uint8_t *buf = DMC_SECTOR_BUF;
    uint32_t *w = (uint32_t *)(void *)(buf + RES_CRASH_OFF);

    if (!res_addr)
        return;
    nor_recover();
    plat_fault_get(&f);
    nor_read(res_addr, buf, RES_SECTOR);
    memset(w, 0, 4u * C_WORDS);
    w[C_MAGIC] = RES_CRASH_MAGIC;
    w[C_UPTIME] = plat_ms();
    w[C_EXC] = f.exc;
    w[C_PC] = f.pc;
    w[C_LR] = f.lr;
    w[C_CFSR] = f.cfsr;
    w[C_HFSR] = f.hfsr;
    w[C_BFAR] = f.bfar;
    w[C_MMFAR] = f.mmfar;
    w[C_STATE] = res_cur_state;
    w[C_DEMO] = res_cur_demo;
    w[C_GAMETIC] = (uint32_t)gametic;
    w[C_STACK] = plat_stack_used();
    w[C_BOOT] = res_boot;
    for (unsigned i = 0; i < 63 && f.msg[i]; i++)
        ((char *)&w[C_MSG])[i] = f.msg[i];
    plat_fault_line("gametic", w[C_GAMETIC]);
    plat_fault_line("crash record", !dmc_sector_write(res_addr, buf, RES_SECTOR));
}
