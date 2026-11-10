// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Stub platform for the ARM size build of the engine: every I_* function
// the engine calls, doing nothing, and an entry point that keeps the whole
// game reachable for the linker.

#include <stddef.h>

#include "doomtype.h"
#include "doomstat.h"
#include "d_main.h"
#include "f_finale.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_menu.h"
#include "nor.h"
#include "wi_stuff.h"

#include "dmc_fw.h"
#include "fe.h"
#include "fetch.h"
#include "lcd.h"
#include "ovl.h"
#include "pad.h"
#include "r_strip.h"

byte *I_VideoBuffer;

__attribute__((used, noreturn)) void dmc_size_entry(void)
{
    D_DoomMain();
    for (;;)
        ;
}

void I_Error(char *error, ...)
{
    (void)error;
    for (;;)
        ;
}

void I_Quit(void)
{
    for (;;)
        ;
}

byte *I_ZoneBase(int *size)
{
    *size = 0;
    return NULL;
}

void I_Tactile(int on, int off, int total)
{
    (void)on;
    (void)off;
    (void)total;
}

int I_GetTime(void)
{
    return 0;
}

int I_GetTimeMS(void)
{
    return 0;
}

void I_Sleep(int ms)
{
    (void)ms;
}

void I_InitTimer(void)
{
}

void I_WaitVBL(int count)
{
    (void)count;
}

void I_InitGraphics(void)
{
}

void I_StartFrame(void)
{
}

void I_StartTic(void)
{
}

void I_UpdateNoBlit(void)
{
}

// The renderer's palette hook, then the palette into the pump LUT, as the
// device does it.
void I_SetPalette(int palette)
{
    R_PaletteChanged();
    pal_read(palette, pump_lut);
}

void I_FinishUpdate(void)
{
}

void I_InitSound(boolean use_sfx_prefix)
{
    (void)use_sfx_prefix;
}

void I_UpdateSound(void)
{
}

int I_GetSfxLumpNum(sfxinfo_t *sfxinfo)
{
    (void)sfxinfo;
    return 0;
}

int I_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep)
{
    (void)sfxinfo;
    (void)vol;
    (void)sep;
    return channel;
}

void I_StopSound(int handle)
{
    (void)handle;
}

boolean I_SoundIsPlaying(int handle)
{
    (void)handle;
    return false;
}

void I_UpdateSoundParams(int handle, int vol, int sep)
{
    (void)handle;
    (void)vol;
    (void)sep;
}

// The front end: the firmware's resident glue (port/game/fe_glue.c) calls the
// overlay entries through the loader, here they are called directly.
fe_state_t fe;
boolean menuactive;
boolean inhelpscreens;
int messageToPrint;
boolean messageNeedsInput;
int showMessages = 1;
int detailLevel;
int screenblocks = 10;
dmc_set_t dmc_set;

void M_Init(void)
{
}

void M_Ticker(void)
{
}

boolean M_Responder(event_t *ev)
{
    return M_OvlResponder(ev);
}

void M_StartControlPanel(void)
{
    M_OvlOpen();
}

void M_Drawer(void)
{
    M_OvlDrawer();
}

// The game pad: its state and curve are part of the input module (port/game/i_input.c),
// which the resident flash total (tools/gates/sizegate.py) adds on its own
volatile pad_t dmc_pad;

int II_PadAxis(int v, int max)
{
    (void)max;
    return v;
}

void snd_volume(int vol)
{
    (void)vol;
}

int settings_save(const void *src, uint32_t size, uint16_t version)
{
    (void)src;
    (void)size;
    (void)version;
    return 0;
}

void fe_frame(int view)
{
    (void)view;
}

void fe_page(void)
{
    F_OvlPage();
}

void ovl_invalidate(void)
{
}

void WI_Start(wbstartstruct_t *wb)
{
    (void)wb;
    WI_OvlStart();
}

void WI_Ticker(void)
{
    WI_OvlTicker();
}

void WI_Drawer(void)
{
    WI_OvlDrawer();
}

void WI_End(void)
{
}

void F_StartFinale(void)
{
    F_OvlStart();
}

boolean F_Responder(event_t *ev)
{
    (void)ev;
    return false;
}

void F_Ticker(void)
{
}

void F_Drawer(void)
{
    F_OvlDrawer();
}

// The saves of the SAVEG overlay (port/game/g_save.c) are not part of this build
void G_DoSaveGame(void)
{
}

void G_DoLoadGame(void)
{
}

int G_SaveUsable(void)
{
    return SAVE_ENOFILE;
}

int G_SaveHeader(int slot, dmc_save_hdr_t *h)
{
    (void)slot;
    (void)h;
    return SAVE_ENOFILE;
}

int G_SaveCheck(int slot, dmc_save_hdr_t *h)
{
    (void)slot;
    (void)h;
    return SAVE_ENOFILE;
}

void dmc_fatal(const char *msg)
{
    I_Error((char *)msg);
}

int dmc_demo_open(const char *name, uint32_t *size)
{
    (void)name;
    (void)size;
    return -1;
}

void dmc_demo_read(uint32_t off, void *dst, uint32_t n)
{
    (void)off;
    (void)dst;
    (void)n;
}

// NOR driver stand-in (platform/nor.c is counted in the platform footprint)
volatile struct nor_stats nor_stats;

void nor_read(uint32_t phys, void *dst, uint32_t n)
{
    (void)phys;
    (void)dst;
    (void)n;
}

int nor_read_async(uint32_t phys, void *dst, uint32_t n, int prio)
{
    (void)phys;
    (void)dst;
    (void)n;
    (void)prio;
    return 1;
}

bool nor_done(int ticket)
{
    (void)ticket;
    return true;
}

void nor_wait(int ticket)
{
    (void)ticket;
}

int nor_col_start(uint32_t phys, void *dst, uint32_t n)
{
    (void)phys;
    (void)dst;
    (void)n;
    return 0;
}

void nor_col_wait(void)
{
}
