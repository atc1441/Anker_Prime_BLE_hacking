// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Bulk-Only Transport and SCSI commands of the drive (usb.h).
//
// A command: the CBW (31 B) into the block buffer, the data phase in pieces of at most 4 KB,
// then the CSW. The data phase follows the cases of the transport (host expects none, IN or
// OUT data of length H against what the command moves):
//   - what the command moves in the host's direction, but at most H bytes (more is a phase
//     error), in place of data in the wrong direction nothing (a phase error)
//   - IN short of H: the data (a short packet ends it), then a STALL of the IN pipe, then
//     the CSW after ClearFeature
//   - OUT: all H bytes are taken, those the command does not use are dropped
// A CBW of the wrong size or signature, or not meaningful (reserved bits, LUN, command
// length), stalls both pipes until a Bulk-Only Mass Storage Reset.
// Blocks are written as they arrive (fat_sector_write, port/data/fat_write.h), no cache:
// SYNCHRONIZE CACHE has nothing to do.

#include <string.h>

#include "fat_write.h"
#include "nor.h"
#include "usb.h"

#define R usb_ram

enum { BOT_CBW = 1, BOT_DATA, BOT_HALT, BOT_CSW, BOT_DEAD };
enum { D_NONE, D_IN, D_OUT };
enum { S_BUF, S_NOR };  // IN data: the reply in the buffer or blocks of the NOR

// Sense keys and additional sense codes
#define NOT_READY       2
#define MEDIUM_ERROR    3
#define ILLEGAL_REQUEST 5
#define ASC_WRITE_ERROR 0x0C
#define ASC_BAD_OPCODE  0x20
#define ASC_BAD_LBA     0x21
#define ASC_BAD_FIELD   0x24
#define ASC_NO_MEDIUM   0x3A

// Direct access, removable, SPC-2, response format 2
static const uint8_t inquiry[36] = {0, 0x80, 4, 2, 31, 0, 0, 0, 'D', 'M', 'C', '-', '1', '0', '0', ' ',
                                    'D', 'O', 'O', 'M', ' ', 'd', 'a', 't', 'a', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
                                    '1', '.', '0', ' '};

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static void put_le32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++, v >>= 8)
        p[i] = (uint8_t)v;
}

static void put_be32(uint8_t *p, uint32_t v)
{
    for (int i = 3; i >= 0; i--, v >>= 8)
        p[i] = (uint8_t)v;
}

static void fail(uint8_t key, uint8_t asc)
{
    R.status = 1;
    R.key = key;
    R.asc = asc;
}

static void cbw_wait(void)
{
    R.st = BOT_CBW;
    usbd_xfer(USB_EP_OUT, R.buf, 64);
}

void msc_reset(void)
{
    usbd_abort_in();
    cbw_wait();
}

static void csw(void)
{
    uint8_t *p = R.csw;
    if (R.stall_in) {
        usbd_stall(USB_EP_IN, 1);
        R.st = BOT_HALT;
        return;
    }
    put_le32(p, 0x53425355u);  // "USBS"
    put_le32(p + 4, R.tag);
    put_le32(p + 8, R.resid);
    p[12] = R.status;
    R.st = BOT_CSW;
    usbd_xfer(USB_EP_IN, p, 13);
}

void msc_clear_halt(uint8_t ep)
{
    if (R.st == BOT_DEAD)
        return;
    usbd_stall(ep, 0);
    if (ep == USB_EP_IN && R.st == BOT_HALT) {
        R.stall_in = 0;
        csw();
    }
}

int usb_released(void) { return R.gone || (R.eject && R.st == BOT_CBW); }

// The next piece of the data phase, the CSW after the last
static void data(void)
{
    uint32_t k = R.left < USB_BLOCK ? R.left : USB_BLOCK;
    R.chunk = k;
    if (!k) {
        csw();
    } else if (R.dir == D_IN) {
        if (R.src == S_NOR) {
            nor_read(R.addr, R.buf, k);
            R.addr += k;
            R.nread++;
        }
        usbd_xfer(USB_EP_IN, R.buf, k);
    } else {
        usbd_xfer(USB_EP_OUT, R.buf, k);
    }
}

// The command in R.cb: R.ddir and R.dlen say what it moves, an IN reply is in R.buf (S_BUF)
// or on the NOR at R.addr (S_NOR), OUT data goes to the NOR at R.addr. R.status and the
// sense say how it ended.
static void scsi(void)
{
    const uint8_t *c = R.cb;
    uint8_t *b = R.buf, op = c[0];
    uint32_t n = 0, alloc = c[4], lba = c[2] << 24 | c[3] << 16 | c[4] << 8 | c[5], cnt = c[7] << 8 | c[8];

    R.ddir = D_IN;
    R.src = S_BUF;
    R.status = 0;
    if (op != 0x03)
        R.key = R.asc = 0;
    memset(b, 0, 36);
    switch (R.eject && op != 0x12 && op != 0x03 && op != 0x1B && op != 0x1E ? -1 : op) {
    case -1:  // stopped or ejected: no medium for the rest
        fail(NOT_READY, ASC_NO_MEDIUM);
        break;
    case 0x00:  // TEST UNIT READY
    case 0x1E:  // PREVENT ALLOW MEDIUM REMOVAL
    case 0x35:  // SYNCHRONIZE CACHE(10)
        break;
    case 0x03:  // REQUEST SENSE: fixed format, then the sense is cleared
        b[0] = 0x70;
        b[2] = R.key;
        b[7] = 10;
        b[12] = R.asc;
        n = 18;
        R.key = R.asc = 0;
        break;
    case 0x12:  // INQUIRY, no vital product data pages
        if (c[1] & 1) {
            fail(ILLEGAL_REQUEST, ASC_BAD_FIELD);
            break;
        }
        memcpy(b, inquiry, sizeof(inquiry));
        n = sizeof(inquiry);
        alloc = c[3] << 8 | c[4];
        break;
    case 0x1A:  // MODE SENSE(6): the header, no pages, not write protected
        b[0] = 3;
        n = 4;
        break;
    case 0x5A:  // MODE SENSE(10)
        b[1] = 6;
        n = 8;
        alloc = cnt;
        break;
    case 0x1B:  // START STOP UNIT: a stop (with or without LoEj) ends the session, a start
                // undoes it, power conditions are taken and ignored
        if (!(c[4] & 0xF0))
            R.eject = !(c[4] & 1);
        break;
    case 0x23:  // READ FORMAT CAPACITIES: formatted medium of 768 blocks of 4 KB
        b[3] = 8;
        put_be32(b + 4, USB_BLOCKS);
        b[8] = 2;
        b[10] = USB_BLOCK >> 8;
        n = 12;
        alloc = cnt;
        break;
    case 0x25:  // READ CAPACITY(10)
        put_be32(b, USB_BLOCKS - 1);
        put_be32(b + 4, USB_BLOCK);
        n = alloc = 8;
        break;
    case 0x28:  // READ(10)
    case 0x2A:  // WRITE(10)
    case 0x2F:  // VERIFY(10), without a byte compare
        if (lba >= USB_BLOCKS || cnt > USB_BLOCKS - lba) {
            fail(ILLEGAL_REQUEST, ASC_BAD_LBA);
        } else if (op == 0x2F) {
            if (c[1] & 2)
                fail(ILLEGAL_REQUEST, ASC_BAD_FIELD);
        } else {
            R.addr = USB_VOL + lba * USB_BLOCK;
            R.src = S_NOR;
            R.ddir = op == 0x2A ? D_OUT : D_IN;
            n = alloc = cnt * USB_BLOCK;
        }
        break;
    default:
        fail(ILLEGAL_REQUEST, ASC_BAD_OPCODE);
    }
    R.dlen = n < alloc ? n : alloc;
}

static void cbw(uint32_t n)
{
    const uint8_t *c = R.buf;
    if (n != 31 || le32(c) != 0x43425355u || (c[12] & 0x7F) || c[13] || !c[14] || c[14] > 16) {
        usbd_stall(USB_EP_IN, 1);
        usbd_stall(USB_EP_OUT, 1);
        R.st = BOT_DEAD;
        return;
    }
    uint32_t h = le32(c + 8), d;
    R.tag = le32(c + 4);
    R.dir = !h ? D_NONE : c[12] & 0x80 ? D_IN : D_OUT;
    memcpy(R.cb, c + 15, 16);
    scsi();
    d = R.dlen;
    if ((d && R.ddir != R.dir) || d > h) {
        R.status = 2;
        d = R.dir == D_IN && R.ddir == D_IN ? h : 0;
    }
    R.resid = h - d;
    R.dev = d;
    R.left = R.dir == D_OUT ? h : d;
    R.stall_in = R.dir == D_IN && R.resid;
    R.st = BOT_DATA;
    data();
}

void msc_done(uint8_t ep, uint32_t n)
{
    if (R.st == BOT_CBW && ep == USB_EP_OUT) {
        cbw(n);
    } else if (R.st == BOT_CSW && ep == USB_EP_IN) {
        cbw_wait();
    } else if (R.st == BOT_DATA && ep == (R.dir == D_IN ? USB_EP_IN : USB_EP_OUT)) {
        if (R.dir == D_OUT) {
            // the residue counts what was not written: the host ended the data early, or the
            // NOR failed (the rest of the data is dropped)
            if (n < R.chunk) {
                R.status = 2;
                R.left = R.chunk;
                R.resid += R.dev;
                R.dev = 0;
            }
            if (R.dev) {
                if (fat_sector_write(R.addr, R.buf) && fat_sector_write(R.addr, R.buf)) {
                    fail(MEDIUM_ERROR, ASC_WRITE_ERROR);
                    R.resid += R.dev;
                    R.dev = 0;
                } else {
                    R.addr += USB_BLOCK;
                    R.dev -= USB_BLOCK;
                    R.nwrite++;
                }
            }
        }
        R.left -= R.chunk;
        data();
    }
}
