// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The front end: the menu (MENU overlay, engine/m_menu.c), the intermission (WI overlay,
// engine/wi_stuff.c), the finale and the title pages (FINALE overlay, engine/f_finale.c) and the
// glue that keeps the engine's names for them resident (port/game/fe_glue.c, on the host and on
// the meter). The engine calls the vanilla names, the glue forwards to the overlay entries
// below.
//
// RAM: the state of a front-end screen lives in the front-end scratch fe_scr, the first 256 B
// of the tic scratch (dmc_tic, FRAME_BASE + 6,912 on the meter). It is valid only while no
// P_Ticker runs: through the intermission, the finale, the title pages and a paused game under
// the menu. Overlays have no data of their own, so the persistent state is the resident
// fe_state_t fe.
//
// Saves: SAVE.DAT on the data drive (made by the BOOT overlay, by the host engine at its start)
// holds DMC_SAVE_SLOTS slots, each a header (dmc_save_hdr_t, gen/dmcdat.h) programmed last and
// its body: the globals block, the creation order block (DMC_SF_ORDER) and the level arena. The
// SAVEG overlay (port/game/g_save.c) writes and restores them. The vanilla save path is kept: a
// save from the menu reaches G_Ticker in the ticcmd (BTS_SAVEGAME), a load is a game action.
//
// Drawing: the screens have no framebuffer. Their drawers compose them with R_ScreenDraw
// (port/render/r_screen.h) in the compose buffers of the frame region, which frames without a
// 3D view own (port/render/r_strip.h). The menu is drawn on a black panel over the picture of
// the frame before it opened, which stays on the LCD.

#pragma once

#include <stdint.h>

#include "d_event.h"
#include "dmcdat.h"
#include "hu_stuff.h"
#include "p_local.h"

typedef struct {                // intermission state, single player
    int32_t bcnt;               // background animation clock
    int32_t cnt_kills, cnt_items, cnt_secret, cnt_time, cnt_par;
    int32_t nexttic[10];        // next tic of each background animation of episode 0
    int16_t cnt, cnt_pause;
    int8_t ctr[10];             // frame of each background animation, -1 for none
    int8_t state;               // stateenum_t, NoState is -1
    uint8_t sp_state, acceleratestage;
} wi_state_t;

typedef struct {                // finale state
    uint32_t count;             // finalecount
    uint16_t endcount;          // strlen(E1TEXT) * TEXTSPEED + TEXTWAIT, set once
    uint8_t stage;              // F_STAGE_TEXT, F_STAGE_ARTSCREEN
    uint8_t width[HU_FONTSIZE]; // STCFN glyph widths
} fin_state_t;

enum { F_STAGE_TEXT, F_STAGE_ARTSCREEN };

typedef struct {
    uint32_t hash[40];          // per 8-column group: draw calls of the last composed frame
    union {
        wi_state_t wi;
        fin_state_t fin;
    } u;
} fe_scr_t;

_Static_assert(sizeof(fe_scr_t) <= 256, "the front-end scratch is 256 B");

#define fe_scr (*(fe_scr_t *)(void *)&dmc_tic)

// The resident state of the front end (fe_glue.c)
typedef struct {
    uint8_t menu, item;         // currentMenu (index), itemOn
    uint8_t skull, skullcnt;    // whichSkull, skullAnimCounter
    uint8_t last[5];            // lastOn of main, skill, options, load, save
    uint8_t msg, msgfn, msgarg; // message text id, its routine, the quit message index
    uint8_t msglast;            // messageLastMenuActive
    uint8_t px0, px1, py0, py1; // panel union: x0/2, x1/2, y0, y1
    uint8_t flags;              // screen and settings flags
    uint8_t action;             // what the play loop does next (FE_ACT_*)
    uint8_t slot;               // load or save slot, FE_SENDSAVE
    uint8_t saves;              // bit per slot with a save of this game (Load and Save pages)
    uint8_t exit_k;             // test: next forced exit (dmc_test.exits, the meter)
} fe_state_t;

extern fe_state_t fe;

// fe.flags
#define FE_HASHED 0x01  // fe_scr.hash describes the screen on the LCD (cleared by P_Ticker and
                        // by palette changes)
#define FE_VIEW   0x02  // this frame draws the 3D view, else the compose buffers are free
#define FE_SNL    0x04  // the intermission's "you are here" pointer, which vanilla keeps from
                        // one intermission to the next (snl_pointeron)
#define FE_SETTINGS 0x08  // the menu changed a setting, saved when it closes
#define FE_PANEL  0x10  // the panel of the menu closed last is still on the LCD: no frame without
                        // the menu has drawn the picture under it since (cleared by fe_frame)

// fe.slot: G_BuildTiccmd sends the save of the slot (vanilla's sendsave)
#define FE_SENDSAVE 0x80

// fe.action: the play loop of the meter opens the data drive on USB or runs the benchmark
enum { FE_ACT_USB = 1, FE_ACT_BENCH };

// Screens of R_ScreenDraw: the intermission adds its state (-1..1), the finale its stage
enum { FE_SCR_PAGE = 1, FE_SCR_FINALE = 2, FE_SCR_WI = 5, FE_SCR_MENU = 7 };

// Called by D_Display before it draws: view is 1 when the frame draws the 3D view. In a frame
// without one it waits for the strips still on their way to the LCD and drops a cached overlay
// that reaches the compose buffers.
void fe_frame(int view);
// The title page of D_PageDrawer (pagepic), drawn by the FINALE overlay
void fe_page(void);

// Drawing on (1) or off (0, the verification pass of the meter): dmc_demo.c on the meter,
// host/main_host.c on the host
extern int dmc_render;

// ---- overlay entries

typedef void fe_fn(void);

enum {
    MENU_E_RESPONDER,  // M_OvlResponder: an event while the menu is up, or Escape or F10
    MENU_E_DRAW,       // M_OvlDrawer
    MENU_E_OPEN,       // M_OvlOpen: the main menu (M_StartControlPanel)
};

typedef boolean menu_resp_fn(event_t *ev);

enum {
    WI_E_START,      // WI_OvlStart: a new intermission of wminfo
    WI_E_TICKER,     // WI_OvlTicker
    WI_E_DRAW,       // WI_OvlDrawer
    WI_E_COMPLETED,  // G_DoCompleted, the meter's end of a level
};

enum {
    FIN_E_START,     // F_OvlStart
    FIN_E_DRAW,      // F_OvlDrawer
    FIN_E_PAGE,      // F_OvlPage: the title page
};

// ---- saves (SAVEG overlay, port/game/g_save.c)

#define SAVE_SLOT_BYTES (DMC_SAVE_SIZE / DMC_SAVE_SLOTS)
// The clusters of SAVE.DAT that are erased when it is made (fat_create): the first of each
// slot, its header
#define SAVE_ERASE \
    (((1u << DMC_SAVE_SLOTS * DMC_SAVE_SLOT_SECTORS) - 1) / ((1u << DMC_SAVE_SLOT_SECTORS) - 1))

enum {
    SAVE_OK,
    SAVE_ENOFILE,  // SAVE.DAT is missing or not usable
    SAVE_EEMPTY,   // no save of this build and image in the slot
    SAVE_ECRC,     // the body does not match its CRC
    SAVE_EGAME,    // not in a level or a game action waits, or a save of another map, skill or
                   // object store
    SAVE_EBIG,     // the body does not fit the slot
    SAVE_ENOR,     // a NOR erase or program failed, or it did not read back
};

enum {
    SAVE_E_WRITE,    // G_SaveWrite: the level into a slot (GS_LEVEL), DMC_SF_EXACT: a test save
    SAVE_E_CHECK,    // G_SaveCheck: the header of a slot and the CRC of its body
    SAVE_E_RESTORE,  // G_SaveRestore: a slot over the level set up for its map and skill,
                     // DMC_SF_EXACT gametic too, DMC_SF_DEMO the demo cursor too
    SAVE_E_HEADER,   // G_SaveHeader: the header of a slot alone
};

typedef int save_fn(int slot, int flags);
typedef int save_check_fn(int slot, dmc_save_hdr_t *h);

// g_save.c, also linked into MENU: SAVE.DAT can hold saves, a slot holds a save of this build
// and image (its header into h), and that save is whole. SAVE_OK or SAVE_E*.
int G_SaveUsable(void);
int G_SaveHeader(int slot, dmc_save_hdr_t *h);
int G_SaveCheck(int slot, dmc_save_hdr_t *h);

// g_save.c: the entries SAVE_E_WRITE and SAVE_E_RESTORE (SAVE_E_CHECK is G_SaveCheck,
// SAVE_E_HEADER G_SaveHeader)
int G_SaveWrite(int slot, int flags);
int G_SaveRestore(int slot, int flags);

// fe_glue.c: a load of slot as G_DoLoadGame does it, flags of SAVE_E_RESTORE. SAVE_OK, else
// nothing changed when the slot did not check out. The CRC is checked by the test loads
// (DMC_SF_EXACT), the menu checks it before a load of its own (M_LoadSelect).
int fe_load(int slot, int flags);
