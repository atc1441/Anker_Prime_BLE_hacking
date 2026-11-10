// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Platform interface of the FNIRSI DMC-100 port (AT32F415, Cortex-M4 without FPU, 144 MHz,
// 32 KB SRAM) and of its host stand-in.
//
// Rules:
// - Functions are for thread context unless noted. The three interrupts (SysTick, DMA1 ch2
//   for NOR, DMA1 ch5 for the LCD pump) share one preemption priority and never nest.
// - The application provides app_main(). Startup calls it after .data/.bss init and
//   plat_power_off() runs if it ever returns.
// - Display, NOR and LCD pump interfaces are in lcd.h and nor.h.

#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

void app_main(void);

// ---- Init and time ----

#define PLAT_CPU_HZ 144000000u

// Power latch, SysTick, DWT, buttons, buzzer, NOR (nor_init) and LCD (lcd_init with the
// build's default profile).
void plat_init(void);
// Milliseconds since plat_init, counted by the 1 kHz SysTick interrupt. I_GetTime() is
// ms / 1000 * 35 + ms % 1000 * 35 / 1000 (plat_ms() * 35 overflows after 34 hours).
uint32_t plat_ms(void);
void plat_delay_ms(uint32_t ms);
// Sleeps until the next interrupt (at most 1 ms, the SysTick period).
void plat_idle(void);
// DWT cycle counter (144 per microsecond). Reads 0 where the counter does not exist.
uint32_t plat_cycles(void);

// ---- Buttons ----
// Index of each button. The meter is held sideways (clamp left), so HOLD is the left arrow
// and V-A the right arrow under the right thumb, MODE and SAVE/MENU are the other two keys of
// that cluster, LIGHT is on the side.

#define BTN_HOLD  0  // PB4, left arrow
#define BTN_SAVE  1  // PB5, SAVE/MENU
#define BTN_MODE  2  // PB6
#define BTN_VA    3  // PB7, right arrow
#define BTN_LIGHT 4  // PA15
#define BTN_POWER 5  // PF7
#define BTN_COUNT 6
#define BTN_BIT(b) (1u << (b))

// Debounced state, bit BTN_BIT(b) set while button b is held. Sampled at 1 kHz, a change
// needs 3 equal samples.
uint32_t plat_buttons(void);
// Next debounced edge from an 8-entry ring, or -1 when empty. Bits 0-2 are the button
// index, EV_PRESS is set for a press and clear for a release. When the ring is full new
// edges are dropped, so callers that care can resync from plat_buttons().
#define EV_PRESS 0x80
int plat_event(void);

// ---- PC speaker ----
// One channel of DMX PC-speaker tone bytes (DP lump bodies without their 4-byte header),
// stepped at 140 Hz by the SysTick interrupt. Tone 0 is silence.

#define SND_MAX 192
enum { SND_OFF, SND_LOW, SND_HIGH };  // volume: off, 12.5 % duty, 50 % duty
void snd_volume(int vol);
// Copies up to SND_MAX tone bytes and starts them, replacing any sound that is playing.
void snd_play(const uint8_t *tones, uint32_t n);
void snd_stop(void);
bool snd_busy(void);
// Plain tone in Hz for alerts and tests (0 = off). Stops the stepper.
void snd_tone(uint32_t hz);
// Number of 140 Hz steps taken since boot (diagnostics).
uint32_t snd_steps(void);

// ---- Power ----

// Backlight off, then drops the power latch (PF6). POWER held down keeps the board powered,
// so it waits for 50 ms of released POWER. If the board is still powered then (USB), it
// resets.
__attribute__((noreturn)) void plat_power_off(void);

// ---- Settings page (internal flash 0x0801FC00, 1 KB) ----
// Stored as magic, version, size, payload, CRC-32. A firmware update may erase the page.

#define SETTINGS_MAX 1012u
// Copies the payload to dst and returns 0 when magic, version, size and CRC match.
// Otherwise returns -1 and leaves dst untouched.
int settings_load(void *dst, uint32_t size, uint16_t version);
// Erases and programs the page. About 20 ms with interrupts off. Returns 0 on success.
int settings_save(const void *src, uint32_t size, uint16_t version);

// ---- Faults and stack ----
// The stack is the bottom 2 KB of SRAM (0x20000000-0x200007FF, 1.5 KB in the DOOM firmware),
// painted at reset.
// CPU faults show a screen with PC, LR, CFSR, HFSR, BFAR, the stack high-water mark and the
// lines added by the fault hook. POWER then powers off.

// I_Error: resets MSP and shows msg (up to 63 characters) on the fault screen.
__attribute__((noreturn)) void plat_panic(const char *msg);
// The fault on the screen, for the fault hook: exception number (0 for plat_panic), PC
// (0xFFFFFFFF when the exception frame was not in SRAM, 0 for plat_panic), LR, the fault
// status registers and the plat_panic message ("" for CPU faults).
struct plat_fault {
    uint32_t exc, pc, lr, cfsr, hfsr, bfar, mmfar;
    const char *msg;
};
void plat_fault_get(struct plat_fault *f);
// fn runs at the end of the fault screen and may call plat_fault_line() for each extra line,
// for example pool high-water marks. It runs on a fresh stack with interrupts masked.
void plat_fault_hook(void (*fn)(void));
void plat_fault_line(const char *label, uint32_t value);
// Deepest stack use since reset in bytes, from the painted stack.
uint32_t plat_stack_used(void);

// ---- RAM regions (platform/link/app.ld) ----
// 0x20000000 stack 2,048 B | fixed core (.data, .bss) 5,216 B (the DOOM firmware: 1,536 and
// 5,728 B, platform/link/regions.ld) | FRAME_BASE: frame region,
// caches and level arena up to 0x20008000, split at run time by the map's plan.
// Overlays link at FRAME_BASE and may use FRAME_MIN bytes.
// The plan sizes are defined once, in platform/link/regions.ld, and reach C as link-time
// symbols whose address is the value (target links only): PLAT_FRAME_MIN, PLAT_FRAME_MAX.

extern uint8_t __frame_min[], __frame_max[];
#define PLAT_FRAME_MIN ((uint32_t)(uintptr_t)__frame_min)  // plan D frame region
#define PLAT_FRAME_MAX ((uint32_t)(uintptr_t)__frame_max)  // plan A frame region
extern uint8_t __frame_start[];  // FRAME_BASE, 16 B aligned
extern uint8_t __dyn_free[];     // first byte after statically placed PLAT_DYN data
extern uint8_t __ram_end[];      // 0x20008000

// Places a static buffer at FRAME_BASE and up, for apps without a plan (test apps). These
// buffers are zeroed at reset like .bss.
#ifdef PLAT_HOST
#define PLAT_DYN __attribute__((aligned(4)))
#else
#define PLAT_DYN __attribute__((section(".dynram"), aligned(4)))
#endif

// ---- Formatting ----
// snprintf subset for status text: %s %c %d %i %u %x (%X, both upper case) with optional
// '-', '0' and width.
// Always NUL-terminates, returns the untruncated length.
int plat_fmt(char *out, int cap, const char *f, ...) __attribute__((format(printf, 3, 4)));
int plat_vfmt(char *out, int cap, const char *f, va_list ap) __attribute__((format(printf, 3, 0)));
