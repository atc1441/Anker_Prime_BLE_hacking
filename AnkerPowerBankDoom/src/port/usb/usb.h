// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The 3 MB data volume of the NOR (0x100000-0x3FFFFF) as a USB drive: a mass storage device,
// Bulk-Only Transport, SCSI transparent command set, one LUN of 768 logical blocks of 4 KB.
// The block is the volume's FAT12 sector and the NOR's erase sector. Runs in the USB overlay
// of the DOOM firmware (port/game/dmc_usb.c) on the controller driver platform/usbd.h, and on
// the host against a fake driver (tests/usb).
//   usb_ctrl.c  descriptors, standard requests, the class requests of the transport
//   usb_msc.c   Bulk-Only Transport and the SCSI commands
//   usb_vol.c   the quick format and the folder UI of the volume
// Nothing here reaches the NOR outside the volume.

#pragma once

#include <stdint.h>

#include "usbd.h"

#define USB_VOL    0x100000u  // NOR address of the volume (DMC_VOL_MAIN)
#define USB_BLOCK  4096u
#define USB_BLOCKS 768u
#define USB_EP_IN  0x81u
#define USB_EP_OUT 0x01u

// The state and the block buffer. Overlays have no static data: the DOOM firmware places it
// behind the overlay's window (platform/link/doom.ld), the host tests define it.
typedef struct {
    uint8_t buf[USB_BLOCK];  // the block buffer (CBW, replies, block data)
    uint8_t ep0[64];         // control replies built at run time
    uint8_t cb[16];          // command block of the command under way
    uint8_t csw[16];
    uint32_t tag, left, chunk, dev, resid, addr;
    uint32_t nread, nwrite;  // blocks moved, for the screen
    uint8_t config;          // configuration value, 0 when not configured
    uint8_t st;              // BOT_* (usb_msc.c)
    uint8_t dir, ddir, src;  // data phase: host direction, device direction, source
    uint32_t dlen;           // bytes the command moves
    uint8_t status, key, asc;  // CSW status, sense key and additional sense code
    uint8_t stall_in;        // the IN pipe stalls before the CSW
    uint8_t eject;           // START STOP UNIT stopped or ejected the medium
    uint8_t gone;            // SET_CONFIGURATION 0 after a configuration, until the next one
} usb_ram_t;

#define USB_RAM_MAX 4608u
_Static_assert(sizeof(usb_ram_t) <= USB_RAM_MAX, "usb_ram is larger than its place");
extern usb_ram_t usb_ram;

// ---- usb_ctrl.c

// Clears the state, starts the controller and connects.
void usb_start(void);
// One event of the controller (usbd_poll) handled. Returns its type (USBD_*).
int usb_task(void);

// ---- usb_msc.c

// Configuration set, USB reset or Bulk-Only Mass Storage Reset: the transfer under way is
// dropped and the next CBW awaited. The halts stay.
void msc_reset(void);
// ClearFeature(ENDPOINT_HALT) of a bulk endpoint. After an invalid CBW the halt stays until
// a Bulk-Only Mass Storage Reset.
void msc_clear_halt(uint8_t ep);
// A transfer of a bulk endpoint is through, n bytes received on OUT.
void msc_done(uint8_t ep, uint32_t n);
// The host is done with the drive: it stopped or ejected the medium and the CSW of that
// command has gone out, or it took back the configuration (Linux does that when it powers
// the drive off).
int usb_released(void);

// ---- usb_vol.c

enum { VOL_OK = 0, VOL_E_NOR = -1, VOL_E_FS = -2, VOL_E_FULL = -3 };

// Quick format of the volume with its geometry, or that of the meter's data drive when it
// has no FAT12 boot sector: both FATs and the root directory cleared, the clusters erased, an
// empty folder UI. progress (or NULL) gets the bytes cleared and erased so far and their
// total. buf is a 4 KB buffer.
int vol_format(uint8_t *buf, void (*progress)(uint32_t done, uint32_t total));
// Gives the root directory a folder UI, which the bootloader needs to show the firmware
// drive in upgrade mode: UI_OFF renamed back, or a new empty folder. VOL_OK when there is
// one, else VOL_E_*.
int vol_ui(uint8_t *buf);
