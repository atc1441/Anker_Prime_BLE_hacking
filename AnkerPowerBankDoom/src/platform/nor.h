// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// W25Q32JV SPI NOR (4 MB, no XIP) on SPI1 at 36 MHz (72 MHz with NOR_SPI_SLOW=0).
//
// Reads use FAST READ (0x0B): the 5-byte header is written polled, the data moves with
// DMA1 ch2 (RX) and ch3 (TX, constant 0xFF). Cost model at 72 MHz: t + n / 7.87 MB/s per
// transaction, about half that rate at 36 MHz.
//
// - nor_read() is synchronous. It waits for the async chunk on the bus (at most 256 B, about
//   34 us at 72 MHz), runs before every queued request and polls its own completion.
// - nor_read_async() queues up to 4 requests. Each is split into chunks of at most 256 B, the
//   DMA1 ch2 transfer-complete interrupt starts the next chunk of the best request: lowest
//   prio value first, then the oldest. A full queue blocks until a slot frees.
// - nor_col_start() puts one read of at most 256 B on a free bus without the queue and its
//   interrupt, nor_col_wait() polls its end. The queue stays empty while it runs.
// - Erase and program wait for the bus like a sync read. None of these may run in an
//   interrupt handler or with interrupts masked. A wait during which no async chunk starts
//   for 100 ms ends in plat_panic.
// The NOR_POLLED=1 build (Renode) has the same API with polled transfers, async requests
// and column reads complete before nor_read_async() and nor_col_start() return.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define NOR_SIZE 0x400000u
#define NOR_JEDEC_W25Q32 0xEF4016u

enum { NOR_PRIO_REC = 0, NOR_PRIO_COL = 1, NOR_PRIO_FLAT = 2 };

// Sets up SPI1 and DMA, wakes the chip and reads the JEDEC ID. Returns 0 when the ID looks
// like a real chip (not 000000 or FFFFFF). Called by plat_init.
int nor_init(void);
uint32_t nor_jedec(void);
uint8_t nor_status(uint8_t cmd);  // 0x05, 0x35 or 0x15

void nor_read(uint32_t phys, void *dst, uint32_t n);
// Returns a ticket (> 0). dst must stay valid until the ticket is done.
int nor_read_async(uint32_t phys, void *dst, uint32_t n, int prio);
bool nor_done(int ticket);
void nor_wait(int ticket);
void nor_wait_all(void);

// Column read when the bus is free: the header goes out now, the DMA moves the data while
// the caller works. A column read still on the bus is ended first. Returns 0 when started,
// -1 when queued requests own the bus or n is 0 or over 256 (the caller queues the read
// instead). Sync reads, queued requests, nor_wait_all, erase and program end it too.
int nor_col_start(uint32_t phys, void *dst, uint32_t n);
// Ends the column read on the bus, if there is one, and returns at once otherwise.
void nor_col_wait(void);

// Erase of the 4 KB sector (cmd NOR_ERASE_4K) or the 64 KB block (NOR_ERASE_64K) at phys,
// aligned to its size, and page program (any length, split at 256 B pages). 0 on success, -1
// on timeout or when the write enable latch did not set.
#define NOR_ERASE_4K  0x20u
#define NOR_ERASE_64K 0xD8u
int nor_erase(uint32_t phys, unsigned cmd);
static inline int nor_erase4k(uint32_t phys) { return nor_erase(phys, NOR_ERASE_4K); }
int nor_program(uint32_t phys, const void *src, uint32_t n);

// Fault path: stops any DMA transfer on SPI1, raises CS and forgets the queued requests, so
// the blocking calls above work again with interrupts masked (the fault hook). Their
// timeouts then count loop turns instead of milliseconds.
void nor_recover(void);

// Alternative sync read in 16-bit SPI frames, not used by the firmware. phys, n and dst must be
// even and phys > 0. dst gets the bytes swapped in pairs (dst[0] = byte phys + 1, dst[1] =
// byte phys), so an image stored pair-swapped reads back in order. Returns 0, or -1 on a DMA
// error or in the NOR_POLLED build.
int nor_read16(uint32_t phys, void *dst, uint32_t n);

struct nor_stats {
    uint32_t sync, async_chunks, errors;
    uint32_t isr_cycles;  // DWT cycles in the DMA1 ch2 interrupt, summed
    union {
        // NOR_PROFILE=1 builds, sync DMA reads: cycles in the header, DMA arming,
        // transfer wait and finish phases
        struct {
            uint32_t prof[4], prof_n;
        };
        // Other builds: nor_read_async and nor_col_start requests, bytes requested by
        // nor_read, nor_read_async and nor_col_start, DWT cycles inside nor_read, nor_wait,
        // nor_wait_all, nor_col_wait and nor_col_start's end of the column read before it (0
        // in the NOR_POLLED build)
        struct {
            uint32_t async, bytes, wait;
        };
    };
};

extern volatile struct nor_stats nor_stats;
