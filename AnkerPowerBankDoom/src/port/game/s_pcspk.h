// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// PC-speaker sound effects with the semantics of Chocolate Doom's i_pcsound.c. One sound
// plays at a time: every sound that s_sound.c starts (its channel and priority rules
// decide) replaces the current one, a handle is the channel number, stopping or asking for
// another handle does nothing. The DP posact, bgact, dmact, dmpain, popain and sawidl are
// never played, as in Chocolate Doom.
//
// The tone bytes come from the SOUNDS section of the image (gen/dmcdat.h) through a read
// callback. A start only records the sound. S_PcsUpdate (I_UpdateSound, called by
// S_UpdateSounds in the main loop right after the tics) reads the bytes into a 192 B stack
// buffer and hands them to the platform's 140 Hz stepper (snd_play, platform/buzzer.c),
// which copies them, so the SysTick interrupt never touches NOR. S_PcsUpdate's frame is
// 216 B at -Oz. It runs outside the deepest tic path, so it does not add to the high-water
// mark of the 1,536 B stack. A platform call that fills the stepper's buffer in place would
// remove the buffer.
// Volume is the platform's snd_volume(SND_OFF / SND_LOW / SND_HIGH).
//
// Engine integration: link this file instead of the I_* sound stubs and call
// S_PcsInit(img_read, img.sect[DMC_SECT_SOUNDS].off, img.sect[DMC_SECT_SOUNDS].size) once
// the image is located. I_WaitVBL should call I_UpdateSound first, so a sound started
// right before a wait (the quit sound) plays.

#pragma once

#include <stdint.h>

// Reads n bytes at logical image offset off
typedef void (*pcs_read_t)(uint32_t off, void *dst, uint32_t n);

// sounds, size = logical offset and size of the SOUNDS section (size 0 when absent). Reads
// its header. A table that does not fit the section plays nothing, an entry outside it
// plays silence.
void S_PcsInit(pcs_read_t read, uint32_t sounds, uint32_t size);
// Returns the handle (channel) or -1 when the sound is not played
int S_PcsStart(int sfx, int channel);
void S_PcsStop(int handle);
int S_PcsPlaying(int handle);
void S_PcsUpdate(void);
