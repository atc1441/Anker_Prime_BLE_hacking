// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// USB device controller of the AT32F415 (OTGFS, RM chapter 20), full speed, polled, for the
// mass storage drive of the DOOM firmware (port/usb). No interrupts: usbd_poll() runs the
// controller and returns one event at a time.
//
// - Clock: 48 MHz from the 144 MHz PLL through the USB divider (PLL / 3). Pins PA11 (D-) and
//   PA12 (D+) belong to the controller once it is clocked and its transceiver is on. VBUS
//   sensing is on with PA9 pulled up, so the session is always valid, as the bootloader
//   does it. With VBUSIG instead the controller never connects its D+ pull-up.
// - Endpoint 0 (64 B): the driver moves the stages of a control transfer, the application
//   answers each SETUP with usbd_ctl_send() or usbd_ctl_stall(). Replies are at most 64 B
//   (one packet). The OUT data stage of a request is received with usbd_ctl_recv().
// - Endpoint 1 IN and OUT, bulk, 64 B packets, one transfer at a time per direction.
// - FIFO RAM (1,280 B): receive FIFO 96 words, endpoint 0 transmit 16, endpoint 1 transmit 128
//   (8 packets, the most the controller keeps in a transmit FIFO), 80 words unused.
// - The interrupt status bits are polled. The endpoint masks are set as the RM's reset
//   handling has them, the global interrupt mask (GAHBCFG) stays off: no interrupt is raised,
//   unless usbd_irq_on() asks for one (the game pad of the DOOM firmware calls usbd_poll() in
//   its OTGFS interrupt handler).
// The state lives in usbd_ram, which the application places (the DOOM firmware has it in its
// fixed core, controller/rc-n1/dmc_dji.c).

#pragma once

#include <stdint.h>

enum { USBD_NONE, USBD_RESET, USBD_SETUP, USBD_DONE };

typedef struct {
    uint8_t setup[8];  // USBD_SETUP: the request
    uint8_t ep;        // USBD_DONE: the endpoint address (0x81 IN, 0x01 OUT, 0 OUT data stage)
    uint32_t n;        // USBD_DONE on OUT: bytes received
} usbd_ev_t;

typedef struct {
    uint8_t *in, *out;               // endpoint 1 transfers
    uint32_t in_len, in_pos, out_len, out_pos;
    uint8_t *ep0_out;                // OUT data stage under way
    uint32_t ep0_len, ep0_pos;
    uint8_t setup[8];
    uint8_t ep0_in;                  // the control transfer has an IN data stage
    uint8_t out_new;                 // endpoint 1 OUT closed: the next transfer is programmed
} usbd_state_t;

#define USBD_RAM_MAX 64u
_Static_assert(sizeof(usbd_state_t) <= USBD_RAM_MAX, "usbd_ram is larger than its place");
extern usbd_state_t usbd_ram;

// Clock, core reset, device mode, FIFOs. The device stays disconnected (no pull-up on D+).
void usbd_init(void);
// Pull-up on: the host sees the device and resets it.
void usbd_connect(void);
// Interrupt mode, after usbd_init: the controller raises its interrupt (IRQ 67, the NVIC is
// the application's) on a bus reset, the end of the enumeration, a suspend, a receive FIFO
// entry and the endpoint events usbd_poll() reports. The handler calls usbd_poll() until
// USBD_NONE, then usbd_irq_done(), which clears what usbd_poll() leaves set and returns
// nonzero when the bus was suspended.
void usbd_irq_on(void);
int usbd_irq_done(void);
// Soft disconnect, transceiver and controller clock off: the host sees the device leave.
void usbd_off(void);
// usbd_off, then a system reset: the bootloader starts the app again.
__attribute__((noreturn)) void usbd_reboot(void);
// The chip's 96-bit unique ID.
void usbd_uid(uint32_t id[3]);

// Runs the controller: bus reset, receive FIFO, transmit FIFO refill, endpoint events.
// Returns the next event (USBD_*), USBD_NONE when there is nothing to do.
int usbd_poll(usbd_ev_t *e);
// The bus is suspended: no traffic for 3 ms (host asleep, port disabled, cable pulled).
int usbd_suspended(void);

// Answer to the SETUP of the last USBD_SETUP: n bytes of data (at most 64, the host gets the
// first wLength of them) and the status stage, or the status stage alone for n = 0.
void usbd_ctl_send(const void *data, uint32_t n);
void usbd_ctl_stall(void);
// OUT data stage of the last USBD_SETUP: receives up to n bytes (wLength) into buf. It ends
// after n bytes or a short packet with USBD_DONE of ep 0, then the application answers with
// usbd_ctl_send(0, 0) (the status stage) or usbd_ctl_stall().
void usbd_ctl_recv(void *buf, uint32_t n);
void usbd_set_address(uint8_t addr);

// Endpoint 1 (ep 0x81 or 0x01): activation as a bulk endpoint with DATA0, deactivation (a
// transfer under way is stopped, OUT through the global OUT NAK).
void usbd_open(uint8_t ep);
void usbd_close(uint8_t ep);
// Starts a transfer: IN sends n bytes of buf (n > 0), OUT receives up to n bytes into buf and
// ends at n bytes or a short packet. buf stays in use until the USBD_DONE of ep. An OUT
// endpoint that still waits for a transfer keeps it and only takes the new buf and n.
void usbd_xfer(uint8_t ep, void *buf, uint32_t n);
// Drops the IN transfer that is under way and flushes its FIFO (Bulk-Only Mass Storage
// Reset).
void usbd_abort_in(void);
// STALL on or off. Off also resets the data toggle to DATA0 (ClearFeature ENDPOINT_HALT).
void usbd_stall(uint8_t ep, int on);
int usbd_stalled(uint8_t ep);
