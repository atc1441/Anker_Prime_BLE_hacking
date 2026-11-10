// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The BOOT overlay (dmc_fw.h): the settings page and the data on the NOR, before the engine
// starts.
//   no DOOM.DAT       installer: SAVE erases the 3 MB data drive for DOOM, MODE keeps it as
//                     it is, then the USB overlay opens it on USB for DOOM.DAT (dmc_usb.c).
//                     Also when that drive has no FAT12 file system.
//   DOOM.DAT fine     a folder UI_OFF (MODE on a data error screen made it) is renamed back
//                     to UI, so upgrade mode shows the firmware drive again
//   any other error   a screen in plain words. A problem of DOOM.DAT offers SAVE to open the
//                     data drive on USB, one of the firmware drive's files says how to reach it
//                     in upgrade mode.
// After a change of DOOM.DAT or DOOMX.DAT (size, first cluster, header CRC) every section and
// map CRC is checked once with a progress screen, before any rename, and the result is
// stamped in the settings page. Then SAVE.DAT is made on the data drive when it has none and
// room for it, and DOOMRES.DAT is looked up. A second entry starts the engine.

#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_loop.h"
#include "hu_stuff.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_video.h"

#include "dmc_fw.h"
#include "cache.h"
#include "extents.h"
#include "fat12.h"
#include "fat_write.h"
#include "fe.h"
#include "fetch.h"
#include "i_input.h"
#include "img.h"
#include "lcd.h"
#include "nor.h"
#include "ovl.h"
#include "s_pcspk.h"
#include "ui.h"

static void set_defaults(void)
{
    memset(&dmc_set, 0, sizeof(dmc_set));
    dmc_set.sound = SND_LOW;
    dmc_set.always_run = 1;
}

// ---- the folder UI of the data drive: without it upgrade mode shows that volume in place
// of the firmware drive

enum { DIR_DONE, DIR_ALREADY, DIR_NOFOLDER, DIR_BOTH, DIR_FAIL };

#define UI_ON "UI         "
#define UI_OFF "UI_OFF     "

// Renames the root folder from to to on the data drive: the 4 KB sector of its directory
// entry is changed in DMC_SECTOR_BUF, then erased, programmed and read back, up to 3 times.
static int ui_rename(const char *from, const char *to)
{
    fat_vol_t v;
    uint32_t a, b;
    if (fat_mount(&v, DMC_VOL_MAIN))
        return DIR_FAIL;
    int has_from = !fat_dir(&v, from, &a), has_to = !fat_dir(&v, to, &b);
    if (has_from && has_to)
        return DIR_BOTH;
    if (has_to)
        return DIR_ALREADY;
    if (!has_from)
        return DIR_NOFOLDER;
    uint8_t *buf = DMC_SECTOR_BUF;
    uint32_t sec = a & ~(RES_SECTOR - 1u);
    nor_read(sec, buf, RES_SECTOR);
    memcpy(buf + (a - sec), to, 11);
    for (int tries = 0; tries < 3; tries++)
        if (!dmc_sector_write(sec, buf, RES_SECTOR))
            return DIR_DONE;
    return DIR_FAIL;
}

static int ui_back(void) { return ui_rename(UI_OFF, UI_ON); }

static void dir_rows(int r, int res)
{
    switch (res) {
    case DIR_DONE:
    case DIR_ALREADY:
        ui_row(r, C_GREEN, "Upgrade mode shows the firmware drive.");
        break;
    case DIR_NOFOLDER:
        ui_row(r, C_ORANGE, "The data drive has no folder UI: upgrade");
        ui_row(r + 1, C_ORANGE, "mode shows it, not the firmware drive.");
        ui_row(r + 2, C_ORANGE, "Opening it on USB once makes the folder.");
        break;
    case DIR_BOTH:
        ui_row(r, C_RED, "The data drive has UI and UI_OFF: open");
        ui_row(r + 1, C_RED, "it on USB and delete UI_OFF.");
        break;
    default:
        ui_row(r, C_RED, "Renaming the folder failed (NOR).");
        break;
    }
}

// SAVE: the data drive on USB. MODE: the folder UI becomes UI_OFF, so upgrade mode shows the
// data drive instead of the firmware drive, and the next good boot names it UI again.
static int to_usb(int row)
{
    ui_row(row, C_YELLOW, "SAVE: open the data drive on USB");
    ui_row(row + 1, C_YELLOW, "MODE: show it in upgrade mode instead");
    ui_footer("hold POWER: off");
    dmc_wait_release();
    for (;;) {
        int b = dmc_wait_press();
        if (b == BTN_SAVE)
            return USB_ASIS;
        if (b != BTN_MODE)
            continue;
        int r = ui_rename(UI_ON, UI_OFF);
        dmc_screen("DOOM: the data drive in upgrade mode");
        if (r == DIR_DONE || r == DIR_ALREADY) {
            ui_row(2, C_GREEN, "Upgrade mode now shows the data drive.");
            ui_row(3, C_WHITE, "Copy DOOM.DAT there, then start DOOM.");
        } else {
            dir_rows(2, r);
        }
        ui_row(10, C_WHITE, "Hold POWER to switch off. Then hold HOLD");
        ui_row(11, C_WHITE, "and SAVE and press POWER: upgrade mode.");
        dmc_wait_off();
    }
}

// No DOOM.DAT: the installer
static int installer(void)
{
    dmc_screen("DOOM: install the game data");
    ui_row(2, C_WHITE, "DOOM.DAT is not on the 3 MB data drive.");
    ui_row(4, C_WHITE, "The drive holds the meter's own pictures");
    ui_row(5, C_WHITE, "(folder UI, about 1.2 MB). The original");
    ui_row(6, C_WHITE, "firmware needs them: to keep them, use");
    ui_row(7, C_WHITE, "MODE and copy UI to the computer first.");
    ui_row(9, C_YELLOW, "SAVE: erase the data drive for DOOM,");
    ui_row(10, C_YELLOW, "      then open it on USB");
    ui_row(12, C_YELLOW, "MODE: open the data drive on USB as it");
    ui_row(13, C_YELLOW, "      is (delete files, keep folder UI)");
    ui_footer("hold POWER: off");
    dmc_wait_release();
    for (;;) {
        int b = dmc_wait_press();
        if (b == BTN_SAVE)
            return USB_ERASE;
        if (b == BTN_MODE)
            return USB_ASIS;
    }
}

static const char *const img_why[][2] = {
    // IMG_E_FS
    {"A drive's file system is damaged: copy", "the files again, or format the drive."},
    // IMG_E_NODATA
    {"DOOM.DAT is missing.", ""},
    // IMG_E_FRAG
    {"A DOOM file is in too many pieces:", "delete it, then copy it again."},
    // IMG_E_HEADER
    {"DOOM.DAT is not DMC DOOM data of this", "release: copy the DOOM.DAT of DOOM.BIN."},
    // IMG_E_CRC
    {"DOOM.DAT or DOOMX.DAT is damaged", "(checksum): copy both files again."},
    // IMG_E_DOOMX
    {"DOOMX.DAT is missing on the firmware", "drive or of another DOOM.DAT."},
};

// A data error. upd: the problem is a file of the firmware drive, which upgrade mode shows
// after the rename r (DIR_*). Else SAVE opens the data drive on USB. A checksum error may be
// DOOMX.DAT's: the way to upgrade mode is shown too. DOOMX.DAT of another DOOM.DAT may also
// mean an old DOOM.DAT, as after an upgrade, so SAVE opens the data drive then as well.
static int data_error(int e, int upd, int r)
{
    dmc_screen("DOOM: the game data has a problem");
    for (int i = 0; i < 2; i++)
        ui_row(2 + i, C_WHITE, "%s", img_why[-e - 1][i]);
    ui_row(5, C_GREY, "(error %d)", e);
    if (upd)
        dir_rows(7, r);
    if (upd || e == IMG_E_CRC) {
        ui_row(10, C_WHITE, "Hold POWER to switch off. Then hold HOLD");
        ui_row(11, C_WHITE, "and SAVE and press POWER: upgrade mode.");
    }
    if (!upd || e == IMG_E_DOOMX)
        return to_usb(upd ? 13 : 7);
    dmc_wait_off();
}

// ---- data check

static void verify_progress(uint32_t done, uint32_t total)
{
    int w = (int)((done >> 10) * (LCD_W - 40) / ((total >> 10) + 1));
    lcd_fill(20, 120, 20 + w, 135, C_GREEN);
    ui_row(14, C_WHITE, "%u of %u KB", (unsigned)(done >> 10), (unsigned)(total >> 10));
}

// DOOM.DAT and DOOMX.DAT as the settings page has them checked
static int stamped(void)
{
    const dmc_file_t *fd = &dmc_files[DMC_F_DOOM], *fx = &dmc_files[DMC_F_DOOMX];
    uint32_t dfirst = dmc_extents[fd->first].pcl, xfirst = fx->size ? dmc_extents[fx->first].pcl : 0;
    if (dmc_set.verified && dmc_set.doom_size == fd->size && dmc_set.doom_first == dfirst
        && dmc_set.header_crc == img.header_crc && dmc_set.doomx_size == fx->size
        && dmc_set.doomx_first == xfirst)
        return 1;
    dmc_set.doom_size = fd->size;
    dmc_set.doom_first = dfirst;
    dmc_set.header_crc = img.header_crc;
    dmc_set.doomx_size = fx->size;
    dmc_set.doomx_first = xfirst;
    dmc_set.verified = 0;
    return 0;
}

// ---- DOOMRES.DAT

// Its cluster on the firmware drive, and the boot count it holds
static void res_open(void)
{
    fat_vol_t v;
    dmc_file_t *f = &dmc_files[DMC_F_RES];
    uint32_t run, h[H_WORDS];

    res_addr = 0;
    if (fat_mount(&v, DMC_VOL_UPDATE) || fat_open(&v, "DOOMRES DAT", f) || f->size < RES_SIZE
        || f->cshift < 12)
        return;
    uint32_t a = ext_phys(f, 0, &run);
    if (run < RES_SECTOR || (a & (RES_SECTOR - 1)) || a + RES_SECTOR > DMC_VOL_MAIN)
        return;
    res_addr = a;
    nor_read(a + RES_HDR_OFF, h, sizeof(h));
    res_boot = (h[H_MAGIC] == RES_MAGIC && h[H_VERSION] == RES_VERSION ? h[H_BOOT] : 0) + 1;
}

static int boot_data(void)
{
    fat_vol_t v;
    if (settings_load(&dmc_set, sizeof(dmc_set), SET_VERSION))
        set_defaults();
    dmc_screen("DOOM for the DMC-100");
    ui_row(2, C_WHITE, "Looking for the game data...");
    // the notice of GPL 2 section 2(c)
    ui_row(19, C_GREY, "GPL 2.0+, NO WARRANTY.");
    ui_row(20, C_GREY, "github.com/ArturBieniek4/DMC100-Doom");
    int e = img_init();
    if (e == IMG_E_NODATA || (e == IMG_E_FS && fat_mount(&v, DMC_VOL_MAIN)))
        return installer();
    if (e == IMG_OK && !stamped()) {
        dmc_screen("DOOM: checking the new game data");
        ui_row(2, C_WHITE, "Once after every copy, about a second.");
        img_verify_hook = verify_progress;
        e = img_verify();
        img_verify_hook = NULL;
        if (e == IMG_OK) {
            dmc_set.verified = 1;
            settings_save(&dmc_set, sizeof(dmc_set), SET_VERSION);
        }
    }
    // DOOM.DAT is fine: upgrade mode should show the firmware drive again
    int upd = e == IMG_E_DOOMX
        || ((e == IMG_E_FS || e == IMG_E_FRAG) && dmc_files[DMC_F_DOOM].size), r = DIR_ALREADY;
    if (e == IMG_OK || upd) {
        r = ui_back();
        if (r != DIR_ALREADY) {
            dir_rows(4, r);
            plat_delay_ms(1500);
        }
    }
    if (e != IMG_OK)
        return data_error(e, upd, r);
    // without it, or with one that is not usable, the menu's Load and Save pages say so
    if (!dmc_files[DMC_F_SAVE].size)
        fat_create(DMC_VOL_MAIN, "SAVE    DAT", DMC_SAVE_SIZE / FAT_SECTOR, 8, SAVE_ERASE,
                   DMC_SECTOR_BUF, &dmc_files[DMC_F_SAVE]);
    res_open();
    return 0;
}

// ---- the engine

static void boot_engine(void)
{
    // the record cache before the first level: behind the smallest frame region, where no
    // overlay reaches
    rc_place(DMC_FRAME_BASE + PLAT_FRAME_MIN);
    flat_place(NULL);
    flat_set_slots(0);
    V_Init();
    S_PcsInit(img_read, img.sect[DMC_SECT_SOUNDS].off, img.sect[DMC_SECT_SOUNDS].size);
    S_Init(sfxVolume * 8);
    D_CheckNetGame();
    HU_Init();
    ST_Init();
    II_Configure(ii_presets[dmc_set.preset < II_NPRESETS ? dmc_set.preset : 0], dmc_set.flip,
                 dmc_set.always_run);
}

OVL_TABLE(BOOT, OVL_ENTRY(BOOT_E_DATA, boot_data_fn, boot_data),
          OVL_ENTRY(BOOT_E_ENGINE, boot_fn, boot_engine));
