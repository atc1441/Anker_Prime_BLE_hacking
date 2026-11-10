// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// DOOM.BIN, the firmware of the meter: boot shell.
//
// Boot: the platform, then DOOMCODE.DMC (the firmware drive first, then the data drive), then
// the BOOT overlay checks the data on the NOR and runs the install flow (dmc_boot.c). Without
// a usable DOOMCODE.DMC of this build the loader's fault screen says so ("Copy DOOMCODE.DMC
// from the same release"). When the install flow wants the 3 MB data drive on USB, the USB
// overlay (dmc_usb.c) shows it to the PC and resets the meter afterwards. Else the engine
// starts in play mode (fe_play.c): the title loop, where a button opens the menu, with the
// chord resolver and sound. The test setting dmc_test.boot_bench shows the results screen
// (REPORT overlay) first, with the demo results saved last: SAVE goes on to play mode, MODE
// plays the demos (dmc_demo.c) and saves new results, HOLD opens the data drive on USB.

#include "doomdef.h"
#include "doomstat.h"
#include "d_loop.h"
#include "d_main.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "i_video.h"
#include "p_setup.h"
#include "r_main.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_video.h"

#include "dmc_fw.h"
#include "extents.h"
#include "fat12.h"
#include "fetch.h"
#include "i_input.h"
#include "img.h"
#include "lcd.h"
#include "ovl.h"
#include "r_strip.h"
#include "s_pcspk.h"
#include "ui.h"

dmc_set_t dmc_set;

// ---- screens

void dmc_screen(const char *title)
{
    R_PumpIdle();
    ui_clear();
    ui_text_row(0, C_BLACK, C_YELLOW, title);
}

void dmc_wait_release(void)
{
    while (plat_buttons())
        plat_idle();
    while (plat_event() >= 0) {
    }
}

int dmc_wait_press(void)
{
    for (;;) {
        int ev = plat_event();
        if (ev >= 0 && (ev & EV_PRESS))
            return ev & 7;
        if (ui_power_held(2000))
            plat_power_off();
        plat_idle();
    }
}

void dmc_wait_off(void)
{
    ui_footer("hold POWER: off");
    for (;;)
        dmc_wait_press();
}

// ---- DOOMCODE.DMC

// Opens DOOMCODE.DMC as img_init does, so the BOOT overlay can load before it.
static void code_find(void)
{
    fat_vol_t v;
    ext_reset();
    if (!fat_mount(&v, DMC_VOL_UPDATE) && !fat_open(&v, "DOOMCODEDMC", &dmc_files[DMC_F_CODE]))
        return;
    if (!fat_mount(&v, DMC_VOL_MAIN))
        fat_open(&v, "DOOMCODEDMC", &dmc_files[DMC_F_CODE]);
}

// The data drive on USB, then a reset. DOOMCODE.DMC is opened again, the failed data check
// may have left it closed.
void data_drive(int how)
{
    code_find();
    OVL_CALL(OVL_USB, USB_E_RUN, usb_run_fn, how);
}

void bank_splash(void);

void app_main(void)
{
    plat_init();
    bank_splash();
    plat_fault_hook(res_crash);
    code_find();
    int how = OVL_CALL(OVL_BOOT, BOOT_E_DATA, boot_data_fn);
    if (how)
        data_drive(how);
    OVL_CALL(OVL_BOOT, BOOT_E_ENGINE, boot_fn);
    ovl_invalidate();
    R_Init();
    P_Init();
    R_ExecuteSetViewSize();
    if (dmc_test.boot_bench)
        bench(0);
    play();
}
