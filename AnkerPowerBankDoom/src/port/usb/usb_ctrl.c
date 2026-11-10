// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Endpoint 0 of the drive (usb.h): descriptors, the standard requests and the two class
// requests of the Bulk-Only Transport (Get Max LUN, Bulk-Only Mass Storage Reset).

#include <string.h>

#include "usb.h"

#define R usb_ram

// Device: USB 2.0, class in the interface, 64 B on endpoint 0. VID 0x2E3C and PID 0x5720 are
// the chip vendor's IDs, the ones the meter's own bootloader drive uses. No manufacturer
// string, product 1, serial number 2, one configuration.
static const uint8_t dev_desc[18] = {18, 1, 0x00, 0x02, 0, 0, 0, 64, 0x3C, 0x2E, 0x20, 0x57,
                                     0x00, 0x01, 0, 1, 2, 1};

// Configuration 1, self-powered (the meter's battery) with up to 100 mA from the bus:
// interface 0 of mass storage class (8), SCSI transparent command set (6), Bulk-Only
// Transport (0x50), bulk endpoints 0x81 and 0x01
static const uint8_t cfg_desc[32] = {
    9, 2, 32, 0, 1, 1, 0, 0xC0, 50,
    9, 4, 0, 0, 2, 8, 6, 0x50, 0,
    7, 5, USB_EP_IN, 2, 64, 0, 0,
    7, 5, USB_EP_OUT, 2, 64, 0, 0,
};

static const char product[] = "DMC-100 DOOM data";

// String descriptor of the n ASCII characters of s into R.ep0, returns its length
static uint32_t str_desc(const char *s, uint32_t n)
{
    uint8_t *o = R.ep0;
    o[0] = (uint8_t)(2 + 2 * n);
    o[1] = 3;
    for (uint32_t i = 0; i < n; i++) {
        o[2 + 2 * i] = (uint8_t)s[i];
        o[3 + 2 * i] = 0;
    }
    return o[0];
}

// The serial number: the chip's unique ID in 24 upper case hex digits
static uint32_t serial(void)
{
    uint32_t id[3];
    char s[24];
    usbd_uid(id);
    for (unsigned i = 0; i < 24; i++) {
        unsigned d = id[i / 8] >> (28 - 4 * (i % 8)) & 15u;
        s[i] = (char)(d < 10 ? '0' + d : 'A' - 10 + d);
    }
    return str_desc(s, 24);
}

static void configure(uint8_t c)
{
    R.gone = !c && R.config;
    R.config = c;
    usbd_close(USB_EP_IN);
    usbd_close(USB_EP_OUT);
    if (c) {
        usbd_open(USB_EP_IN);
        usbd_open(USB_EP_OUT);
        msc_reset();
    }
}

static void setup(const uint8_t *s)
{
    const uint8_t *d = R.ep0;
    uint8_t *r = R.ep0;
    uint32_t val = s[2] | s[3] << 8, idx = s[4] | s[5] << 8, len = s[6] | s[7] << 8;
    // a bulk endpoint of the configuration, endpoint 0
    int bulk = R.config && (idx == USB_EP_IN || idx == USB_EP_OUT), ep0 = !(idx & 0x7F);
    int32_t n = -1;  // reply length, -1 stalls the request

    r[0] = r[1] = 0;
    switch (s[0] << 8 | s[1]) {
    case 0x8006:  // GET_DESCRIPTOR
        if (val == 0x100) {
            d = dev_desc;
            n = sizeof(dev_desc);
        } else if (val == 0x200) {
            d = cfg_desc;
            n = sizeof(cfg_desc);
        } else if (val == 0x300) {
            r[0] = 4;
            r[1] = 3;
            r[2] = 0x09;  // English (United States)
            r[3] = 0x04;
            n = 4;
        } else if (val == 0x301) {
            n = (int32_t)str_desc(product, sizeof(product) - 1);
        } else if (val == 0x302) {
            n = (int32_t)serial();
        }
        break;
    case 0x0005:  // SET_ADDRESS
        usbd_set_address((uint8_t)(val & 0x7F));
        n = 0;
        break;
    case 0x8008:  // GET_CONFIGURATION
        r[0] = R.config;
        n = 1;
        break;
    case 0x0009:  // SET_CONFIGURATION
        if (val <= 1) {
            configure((uint8_t)val);
            n = 0;
        }
        break;
    case 0x8000:  // GET_STATUS of the device: self-powered, no remote wakeup
        r[0] = 1;
        n = 2;
        break;
    case 0x8100:  // of interface 0, once configured
        n = R.config && !idx ? 2 : -1;
        break;
    case 0x8200:  // of an endpoint: its halt
        if (bulk || ep0) {
            r[0] = (uint8_t)(bulk && usbd_stalled((uint8_t)idx));
            n = 2;
        }
        break;
    case 0x0201:  // CLEAR_FEATURE(ENDPOINT_HALT)
        if (!val && (bulk || ep0)) {
            if (bulk)
                msc_clear_halt((uint8_t)idx);
            n = 0;
        }
        break;
    case 0x0203:  // SET_FEATURE(ENDPOINT_HALT)
        if (!val && bulk) {
            usbd_stall((uint8_t)idx, 1);
            n = 0;
        }
        break;
    case 0x810A:  // GET_INTERFACE: alternate setting 0
        n = R.config && !idx ? 1 : -1;
        break;
    case 0x010B:  // SET_INTERFACE
        n = R.config && !idx && !val ? 0 : -1;
        break;
    case 0xA1FE:  // Get Max LUN: one LUN
        if (R.config && !val && !idx && len == 1)
            n = 1;
        break;
    case 0x21FF:  // Bulk-Only Mass Storage Reset
        if (R.config && !val && !idx && !len) {
            msc_reset();
            n = 0;
        }
        break;
    }
    if (n < 0)
        usbd_ctl_stall();
    else
        usbd_ctl_send(d, (uint32_t)n < len ? (uint32_t)n : len);
}

void usb_start(void)
{
    memset((uint8_t *)&R + USB_BLOCK, 0, sizeof(R) - USB_BLOCK);
    usbd_init();
    usbd_connect();
}

int usb_task(void)
{
    usbd_ev_t e;
    int t = usbd_poll(&e);
    if (t == USBD_RESET)
        R.config = R.gone = 0;
    else if (t == USBD_SETUP)
        setup(e.setup);
    else if (t == USBD_DONE)
        msc_done(e.ep, e.n);
    return t;
}

