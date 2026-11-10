// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The USB overlay (dmc_fw.h): the 3 MB data volume as a USB drive (port/usb), after a quick
// format when the installer asked for one. The drive stays open until the PC is done with it
// (it stops or ejects the medium, or takes back the configuration) or SAVE is pressed. A bus
// that stays suspended (cable pulled, the PC asleep or its port disabled) only brings back
// "Connect the meter", the meter cannot tell those apart. Then the volume gets its folder UI
// back (without it the bootloader shows this volume in upgrade mode instead of the firmware
// drive) and the meter resets, so the next boot checks the data from a clean state. The block
// buffer and the state are usb_ram, behind the overlay's window (platform/link/doom.ld). The
// controller driver (platform/usbd.c) and its state usbd_ram are resident: the play loop
// stops the game pad driver (controller/rc-n1/dmc_dji.c) before this overlay runs.

#include "dmc_fw.h"
#include "lcd.h"
#include "ovl.h"
#include "ui.h"
#include "usb.h"

static void erase_progress(uint32_t done, uint32_t total)
{
    lcd_fill(20, 60, 20 + (int)(done * (LCD_W - 40) / total), 75, C_GREEN);
}

static void usb_run(int how)
{
    uint32_t t = 0, done = 0, idle = 0, link = 2, r = ~0u, w = ~0u;
    if (how == USB_ERASE) {
        dmc_screen("DOOM: erasing the data drive");
        if (vol_format(usb_ram.buf, erase_progress)) {
            ui_row(8, C_RED, "Erasing failed (NOR error).");
            plat_delay_ms(3000);
        }
    }
    dmc_screen("DOOM: the data drive on USB");
    ui_row(4, C_WHITE, "Copy DOOM.DAT to the drive. Keep the");
    ui_row(5, C_WHITE, "folder UI, the meter needs it.");
    ui_row(10, C_WHITE, "Done? Eject the drive on the computer,");
    ui_row(11, C_WHITE, "or press SAVE here. Then DOOM starts");
    ui_row(12, C_WHITE, "(without USB power the meter turns off).");
    ui_footer("hold POWER: off");
    // the host gets answers from the moment the pull-up is on
    dmc_wait_release();
    usb_start();
    for (;;) {
        usb_task();
        if (plat_event() == (EV_PRESS | BTN_SAVE))
            break;
        if (ui_power_held(2000))
            plat_power_off();
        // the PC is done: it gets a moment for what it still sends, unless it configures the
        // drive again in that time
        if (!usb_released())
            done = 0;
        else if (!done)
            done = plat_ms() | 1u;
        else if (plat_ms() - done > 500)
            break;
        if (plat_ms() - t < 200)
            continue;
        t = plat_ms();
        // connected: configured, and the bus not suspended for a second
        idle = usbd_suspended() ? idle + 1 : 0;
        uint32_t l = usb_ram.config && idle <= 5;
        if (link != l) {
            link = l;
            ui_row(2, l ? C_GREEN : C_YELLOW, l ? "Connected to the computer." : "Connect the meter to a computer.");
        }
        if (r != usb_ram.nread || w != usb_ram.nwrite) {
            r = usb_ram.nread;
            w = usb_ram.nwrite;
            ui_row(8, C_GREY, "Read %u KB, written %u KB", (unsigned)r * 4u, (unsigned)w * 4u);
        }
    }
    usbd_off();
    dmc_screen("DOOM: closing the data drive");
    int e = vol_ui(usb_ram.buf);
    if (e) {
        ui_row(2, C_ORANGE, e == VOL_E_FS ? "The drive has no FAT12 file system." : "Could not make the folder UI.");
        plat_delay_ms(3000);
    }
    usbd_reboot();
}

OVL_TABLE(USB, OVL_ENTRY(USB_E_RUN, usb_run_fn, usb_run));
