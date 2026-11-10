// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Play mode and the results loop of the meter (resident, meter only). The results screen
// (REPORT overlay) shows the demo results saved last: SAVE goes to play mode, MODE plays the
// demos (dmc_demo.c) and saves new results, HOLD opens the data drive on USB. The menu's USB
// drive and Benchmark end the game in the play loop (fe.action): the data drive on USB, or
// the demos and the results screen, then the title loop again. The game pad driver
// (controller/rc-n1/dmc_dji.c) runs in play mode only, on the meter build.

#include "doomdef.h"
#include "doomstat.h"
#include "d_loop.h"
#include "d_main.h"
#include "g_game.h"
#include "i_video.h"
#include "m_menu.h"
#include "s_sound.h"

#include "dmc_dji.h"
#include "dmc_fw.h"
#include "fe.h"
#include "ovl.h"
#include "r_strip.h"

void D_Display(void);
extern boolean sendpause;

void bench(int run)
{
    if (run)
        dmc_run_demos();
    for (;;) {
        int b = OVL_CALL(OVL_REPORT, REPORT_E_MENU, report_menu_fn);
        if (b == 0)
            dmc_run_demos();
        if (b == 2)
            data_drive(USB_ASIS);
        if (b == 1)
            return;
    }
}

// D_DoomLoop in real time, with input and sound, from the title loop on. The tics go on from
// the gametic the demo passes left, the first frame sends the whole screen. A crash record
// says play mode, also after a boot straight into it.
static void fe_begin(void)
{
    res_cur_state = RS_PLAY;
    res_cur_demo = 0;
    dmc_render = 1;
    singletics = false;
    fe.exit_k = 0;
    M_Init();
    snd_volume(dmc_set.sound);
    showMessages = !dmc_set.msg_off;
    D_ResetTics();
    R_PaletteChanged();
    /* Vanilla intro: title, DEMO1, then the credits, until a key opens
       the menu. New Game from that menu starts the level. */
    D_StartTitle();
    if ((uintptr_t)__dmc_variant == 1)
        dji_start();
}

// What a game leaves behind and the demo passes do not set up: they start as after a boot
static void fe_end(void)
{
    paused = sendpause = false;
    fe.slot = 0;  // a save the menu still sends (vanilla's sendsave)
    gameaction = ga_nothing;
    fe.flags &= ~FE_HASHED;
    ovl_invalidate();
}

void ble_poll(void);
void gauge_poll(void);

void play(void)
{
    fe_begin();
    for (;;) {
        ble_poll();
        gauge_poll();
        I_StartFrame();
        TryRunTics();
        S_UpdateSounds(players[consoleplayer].mo);
        D_Display();
        if (fe.action) {
            int a = fe.action;
            fe.action = 0;
            dji_stop();
            if (a == FE_ACT_USB)
                data_drive(USB_ASIS);
            fe_end();
            bench(1);
            fe_begin();
        }
    }
}
