// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The engine's system interface on the meter:
//   video   the 3D view goes through the strip renderer's frame driver and the status band
//           through its compositor (port/render), both into the LCD pump. There is no
//           screen buffer (I_VideoBuffer stays NULL). I_SetPalette tells the renderer, then
//           loads the palette into the pump's LUT.
//   time    35 Hz tics from the 1 kHz SysTick clock
//   input   the chord resolver (i_input.c) on every I_StartTic. Each event it posts is
//           handled at once (D_PostEvent is wrapped), so d_event.c keeps a ring of 2. The
//           game pad driver (controller/rc-n1/dmc_dji.c) runs its timers there first.
//   sound   PC speaker, the I_* sound functions are in s_pcspk.c
//   errors  I_Error formats into 64 B on the stack, plat_panic copies them before it resets
//           the stack for the fault screen
//   level   the level transitions and P_Init run in the LEVEL overlay (l_level.c), the end
//           of a level in WI (l_wi.c), the line specials in LINES (l_lines.c): mk/fw.mk sends
//           the engine's calls to the wrappers here (ld.lld --wrap)
// No zone allocator: the one table the init code asks Z_Malloc for (the sound channels of
// S_Init, in the BOOT overlay with Z_Malloc) is carved once from a static pool, nothing is
// freed.

#include <stdarg.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_main.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "z_zone.h"

#include "cache.h"
#include "dmc_dji.h"
#include "dmc_fw.h"
#include "fe.h"
#include "fetch.h"
#include "i_input.h"
#include "l_level.h"
#include "l_lines.h"
#include "lcd.h"
#include "ovl.h"
#include "platform.h"
#include "r_strip.h"
#include "rec.h"

byte *I_VideoBuffer;

// ---- video

void I_InitGraphics(void) {}
void I_StartFrame(void) {}
void I_UpdateNoBlit(void) {}

void I_StartTic(void)
{
    dji_tic();
    I_InputTic();
}

void __real_D_PostEvent(event_t *ev);

void __wrap_D_PostEvent(event_t *ev)
{
    __real_D_PostEvent(ev);
    D_ProcessEvents();
}

void I_SetPalette(int palette)
{
    R_PaletteChanged();
    pal_read(palette, pump_lut);
}

// The band's pump window stays open, the panel takes its last columns while the next tic
// runs. Any later window or polled drawing ends it.
void I_FinishUpdate(void) {}

// ---- time

int I_GetTimeMS(void)
{
    return (int)plat_ms();
}

int I_GetTime(void)
{
    uint32_t ms = plat_ms();
    return (int)(ms / 1000u * TICRATE + ms % 1000u * TICRATE / 1000u);
}

void I_Sleep(int ms)
{
    plat_delay_ms(ms > 0 ? (uint32_t)ms : 1u);
}

void I_InitTimer(void) {}

void I_WaitVBL(int count)
{
    I_UpdateSound();
    plat_delay_ms((uint32_t)count * 1000u / 70u);
}

// ---- system

void I_Error(char *error, ...)
{
    char msg[64];
    va_list ap;
    va_start(ap, error);
    plat_vfmt(msg, sizeof(msg), error, ap);
    va_end(ap);
    plat_panic(msg);
}

// The return address names the check that failed (the panic's own LR is this function's)
void dmc_fatal(const char *msg)
{
    I_Error("%s at %x", (char *)msg, (unsigned)(uintptr_t)__builtin_return_address(0));
}

void I_Quit(void)
{
    plat_power_off();
}

void I_Tactile(int on, int off, int total)
{
    (void)on;
    (void)off;
    (void)total;
}

// ---- zone

// 4 sound channels of 12 B (snd_channels of s_sound.c)
#define POOL_BYTES 48u

// global: Z_Malloc runs in the BOOT overlay, the pool stays resident (mk/fw.mk)
uint8_t dmc_pool[POOL_BYTES] __attribute__((aligned(4)));
uint32_t dmc_pool_used;

void Z_Init(void) {}
void Z_FreeTags(int lowtag, int hightag)
{
    (void)lowtag;
    (void)hightag;
}
void Z_CheckHeap(void) {}

void *Z_Malloc(int size, int tag, void *user)
{
    uint32_t n = ((uint32_t)size + 3u) & ~3u;
    if (tag != PU_STATIC || user || n > POOL_BYTES - dmc_pool_used)
        I_Error("Z_Malloc %d B, %u B left", size, (unsigned)(POOL_BYTES - dmc_pool_used));
    void *p = dmc_pool + dmc_pool_used;
    dmc_pool_used += n;
    return p;
}

// ---- LEVEL overlay

static int level_set;  // a level was set up (dmc_place_caches)

static void level(int e)
{
    level_set = 0;
    OVL_CALL(OVL_LEVEL, e, level_fn);
    // the renderer and the tic scratch take the frame region from here on
    ovl_invalidate();
    if (level_set)
        dmc_level_end();
}

void __wrap_G_DoLoadLevel(void) { level(LEVEL_E_LOAD); }
void __wrap_G_DoNewGame(void) { level(LEVEL_E_NEWGAME); }
void __wrap_G_DoWorldDone(void) { level(LEVEL_E_WORLDDONE); }

// The test load of the verification pass (dmc_test.load): the demo's ticcmds come from the
// restored demo cursor, so G_Ticker goes on with the tic after the save
void __wrap_G_DoPlayDemo(void)
{
    level(LEVEL_E_PLAYDEMO);
    if (dmc_test.load && !dmc_render && demoplayback)
        fe_load(dmc_test.load - 1, DMC_SF_EXACT | DMC_SF_DEMO);
}

// The end of a level loads only WI, which stays below the creation order of the
// verification pass (dmc_demo.c): the finished level is hashed through the intermission.
void __wrap_G_DoCompleted(void) { OVL_CALL(OVL_WI, WI_E_COMPLETED, fe_fn); }

void __wrap_P_Init(void)
{
    OVL_CALL(OVL_LEVEL, LEVEL_E_INIT, level_fn);
    ovl_invalidate();
}

// ---- LINES overlay: it stays loaded until the next 3D view is drawn (ovl_invalidate in
// R_DrawFrame, port/render/r_frame.c). The early returns of P_UseSpecialLine run here:
// monsters blocked by special lines use them in most tics (P_Move), and seldom open a door.

void __wrap_P_CrossSpecialLine(int linenum, int side, mobj_t *thing)
{
    OVL_CALL(OVL_LINES, LINES_E_CROSS, lines_cross_fn, linenum, side, thing);
}

void __wrap_P_ShootSpecialLine(mobj_t *thing, lineidx_t line)
{
    OVL_CALL(OVL_LINES, LINES_E_SHOOT, lines_shoot_fn, thing, line);
}

boolean __wrap_P_UseSpecialLine(mobj_t *thing, lineidx_t line, int side)
{
    line_t l;
    P_GetLine(line, &l);
    if (side && l.special != 124)
        return false;
    if (!MO_PLAYER(thing) && ((l.flags & ML_SECRET)
                              || (l.special != 1 && (l.special < 32 || l.special > 34))))
        return false;
    return OVL_CALL(OVL_LINES, LINES_E_USE, lines_use_fn, thing, line, side);
}

// Called by P_SetupLevel (in the overlay) once the map header and its plan are loaded.
void dmc_place_caches(void)
{
    level_set = 1;
    dmc_level_begin();
    uint8_t *c = R_FrameSetup();
    uint32_t k = M.plan.k;
    uint8_t *arena = __ram_end - M.plan.arena_bytes;
    uint32_t need = RC_BYTES + k * DMC_FLAT_SIZE;
    if (c + need > arena)
        I_Error("plan %c: caches end %u B into the arena", 'A' + M.plan.plan,
                (unsigned)(c + need - arena));
    rc_place(c);
    flat_place(k ? c + RC_BYTES : NULL);
}
