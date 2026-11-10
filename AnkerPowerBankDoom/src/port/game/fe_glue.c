// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The front end's resident glue (port/game/fe.h), on the host and on the meter: the names the
// engine calls for the menu, the intermission, the finale, the title pages and the saves,
// which call the MENU, WI, FINALE and SAVEG overlays (the host links them natively,
// port/ovl/ovl.c), the menu's skull and the finale's ticker, the frame setup of D_Display and
// the state of the front end.
//
// The automap is a stub on the meter (the host links engine/am_map.c): the engine's entry
// points into it do nothing.

#include <string.h>

#include "doomdef.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "am_map.h"
#include "d_main.h"
#include "f_finale.h"
#include "m_menu.h"
#include "wi_stuff.h"

#include "fe.h"
#include "lcd.h"
#include "ovl.h"

fe_state_t fe;

boolean menuactive;
boolean inhelpscreens;
int messageToPrint;
boolean messageNeedsInput;
int showMessages = 1;
#ifdef DMC_FW
int detailLevel = 1;  // low detail, the meter's view
#else
int detailLevel;
#endif
int screenblocks = 10;

#ifdef DMC_FW

boolean automapactive;

boolean AM_Responder(event_t *ev)
{
    (void)ev;
    return false;
}

void AM_Ticker(void) {}
void AM_Drawer(void) {}
void AM_Stop(void) {}

#endif

// ---- menu

void M_Init(void)
{
    fe.skull = 0;
    fe.skullcnt = 10;
}

// The skull blinks, also while the menu is down, as in vanilla
void M_Ticker(void)
{
    if (!--fe.skullcnt) {
        fe.skull ^= 1;
        fe.skullcnt = 8;
    }
}

// Outside the menu only Escape (open it) and F10 (the quit prompt) do something, the meter
// sends no other menu key there (key_menu_activate and key_menu_quit, m_controls.c)
boolean M_Responder(event_t *ev)
{
    if (!menuactive && !messageToPrint
        && (ev->type != ev_keydown || (ev->data1 != KEY_ESCAPE && ev->data1 != KEY_F10)))
        return false;
    return OVL_CALL(OVL_MENU, MENU_E_RESPONDER, menu_resp_fn, ev);
}

// Any button in the title loop and the attract demo (G_Responder)
void M_StartControlPanel(void)
{
    if (!menuactive)
        OVL_CALL(OVL_MENU, MENU_E_OPEN, fe_fn);
}

void M_Drawer(void)
{
    if (dmc_render && (menuactive || messageToPrint))
        OVL_CALL(OVL_MENU, MENU_E_DRAW, fe_fn);
}

// The host gives the tic scratch to the front end poisoned, so no screen reads what a tic or
// another screen left there (the meter has whatever the frame region holds).
static void fe_poison(void)
{
#ifdef DMC_STATS
    memset(&dmc_tic, 0xa5, sizeof(dmc_tic));
#endif
}

void fe_frame(int view)
{
    // the drawers of this frame draw the picture under the panel of the menu closed last
    if (!menuactive)
        fe.flags &= ~FE_PANEL;
    if (view) {
        fe.flags |= FE_VIEW;
        return;
    }
    fe.flags &= ~FE_VIEW;
    // the last strips of a view (plan A: strip buffer 1 reaches compose buffer 0)
    pump_sync(0);
    if (ovl_win[ovl_resident()] > OVL_WIN_FE)
        ovl_invalidate();
}

// The drawers of the screens: nothing in the verification pass of the meter, and the picture
// stays as it is while the menu is up
static int fe_draws(void)
{
    return dmc_render && !menuactive;
}

void fe_page(void)
{
    if (fe_draws())
        OVL_CALL(OVL_FINALE, FIN_E_PAGE, fe_fn);
}

// ---- intermission

// wb is always &wminfo, which the overlay reads
void WI_Start(wbstartstruct_t *wb)
{
    (void)wb;
    fe_poison();
    OVL_CALL(OVL_WI, WI_E_START, fe_fn);
}

void WI_Ticker(void)
{
    OVL_CALL(OVL_WI, WI_E_TICKER, fe_fn);
}

void WI_Drawer(void)
{
    if (fe_draws())
        OVL_CALL(OVL_WI, WI_E_DRAW, fe_fn);
}

void WI_End(void) {}

// ---- finale

void F_StartFinale(void)
{
    fe_poison();
    OVL_CALL(OVL_FINALE, FIN_E_START, fe_fn);
}

boolean F_Responder(event_t *ev)
{
    (void)ev;
    return false;
}

// Resident, so the menu shown over the finale does not load FINALE every tic
void F_Ticker(void)
{
    fin_state_t *f = &fe_scr.u.fin;
    f->count++;
    if (f->stage == F_STAGE_TEXT && f->count > f->endcount) {
        f->count = 0;
        f->stage = F_STAGE_ARTSCREEN;
        wipegamestate = -1;  // force a wipe
    }
}

void F_Drawer(void)
{
    if (fe_draws())
        OVL_CALL(OVL_FINALE, FIN_E_DRAW, fe_fn);
}

// ---- saves

// g_game.c declares these in the file only
extern skill_t d_skill;
extern int d_episode, d_map;
void G_DoNewGame(void);

void G_DoSaveGame(void)
{
    gameaction = ga_nothing;
    OVL_CALL(OVL_SAVEG, SAVE_E_WRITE, save_fn, fe.slot, 0);
}

// The level of the slot's map and skill (G_DoNewGame), unless the demo of a test load has just
// set it up, then the slot over it. A load from the menu reads the header only, the menu has
// checked the save.
int fe_load(int slot, int flags)
{
    dmc_save_hdr_t h;
    int e = OVL_CALL(OVL_SAVEG, flags & DMC_SF_EXACT ? SAVE_E_CHECK : SAVE_E_HEADER,
                     save_check_fn, slot, &h);
    if (e)
        return e;
    if (!(flags & DMC_SF_DEMO) || h.map != gamemap) {
        d_skill = h.skill;
        d_episode = 1;
        d_map = h.map;
        G_DoNewGame();
    }
    return OVL_CALL(OVL_SAVEG, SAVE_E_RESTORE, save_fn, slot, flags);
}

// The play mode keeps its gametic
void G_DoLoadGame(void)
{
    gameaction = ga_nothing;
    fe_load(fe.slot, 0);
}
