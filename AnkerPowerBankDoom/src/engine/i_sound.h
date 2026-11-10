//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// Modified 2026 for the DMC-100 port.
//
// DESCRIPTION:
//	The not so system specific sound interface.
//


#ifndef __I_SOUND__
#define __I_SOUND__

#include "doomtype.h"

#include "info.h"


//
// SoundFX struct.
// The table is generated (gen/info.c) and lives in flash: priority,
// link (sfx number, 0 none), pitch and volume of a link as in vanilla.
// The lump is the sound's number in the image, nothing is cached.
//
typedef const dmc_sfxinfo_t	sfxinfo_t;

// Sound output, provided by the platform. There is no music.

void I_InitSound(boolean use_sfx_prefix);
void I_UpdateSound(void);
int I_GetSfxLumpNum(sfxinfo_t *sfxinfo);
int I_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep);
void I_StopSound(int handle);
boolean I_SoundIsPlaying(int handle);
void I_UpdateSoundParams(int handle, int vol, int sep);

#endif
