// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright(C) 2005-2014 Simon Howard
// Copyright (C) 2026 Artur Bieniek
//
// PC-speaker sound effects, see s_pcspk.h. Implements the engine's I_* sound interface.

#include "s_pcspk.h"

#include "platform.h"

#include "dmcdat.h"
#include "i_sound.h"
#include "sounds.h"

static pcs_read_t rd;
static uint32_t base, size;
static uint16_t count;
static uint8_t cur = 0xFF;  // handle of the current sound, 0xFF none (refused starts are -1)
static int16_t pend = -1;   // sfx started since the last update

static const uint8_t disabled[] = {sfx_posact, sfx_bgact, sfx_dmact, sfx_dmpain, sfx_popain, sfx_sawidl};

void S_PcsInit(pcs_read_t read, uint32_t sounds, uint32_t n)
{
    dmc_sounds_hdr_t h = {0};
    if (n >= sizeof(h))
        read(sounds, &h, sizeof(h));
    rd = read;
    base = sounds;
    size = n;
    count = sizeof(h) + h.count * sizeof(dmc_sound_t) <= n ? h.count : 0;
    cur = 0xFF;
    pend = -1;
}

int S_PcsStart(int sfx, int channel)
{
    if ((unsigned)sfx >= count)  // also before S_PcsInit, count is 0 then
        return -1;
    for (unsigned i = 0; i < sizeof(disabled); i++)
        if (sfx == disabled[i])
            return -1;
    cur = (uint8_t)channel;
    pend = (int16_t)sfx;
    return channel;
}

void S_PcsStop(int handle)
{
    if (handle != cur)
        return;
    pend = -1;
    snd_stop();
}

int S_PcsPlaying(int handle) { return handle == cur && (pend >= 0 || snd_busy()); }

void S_PcsUpdate(void)
{
    if (pend < 0)
        return;
    dmc_sound_t e;
    rd(base + sizeof(dmc_sounds_hdr_t) + (uint32_t)pend * sizeof(e), &e, sizeof(e));
    pend = -1;
    uint32_t n = e.len < SND_MAX ? e.len : SND_MAX;
    if (!n || e.off > size || e.len > size - e.off) {
        snd_stop();
        return;
    }
    uint8_t buf[SND_MAX];
    rd(base + e.off, buf, n);
    snd_play(buf, n);
}

// ---- Engine interface (i_sound.h) ----

void I_InitSound(boolean use_sfx_prefix) { (void)use_sfx_prefix; }

void I_UpdateSound(void) { S_PcsUpdate(); }

int I_GetSfxLumpNum(sfxinfo_t *sfxinfo) { return (int)(sfxinfo - S_sfx); }

int I_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep)
{
    (void)vol;
    (void)sep;
    return S_PcsStart((int)(sfxinfo - S_sfx), channel);
}

void I_StopSound(int handle) { S_PcsStop(handle); }

boolean I_SoundIsPlaying(int handle) { return S_PcsPlaying(handle); }

void I_UpdateSoundParams(int handle, int vol, int sep)
{
    (void)handle;
    (void)vol;
    (void)sep;
}
