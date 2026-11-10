// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Chord resolver for the six buttons. It turns the platform's
// debounced button edges into vanilla keydown/keyup events (D_PostEvent) with the default
// key codes of m_controls.c (doomkeys.h), so the engine sees a keyboard.
//
// Game mapping, from a 10-byte map (one of the presets below, the settings page says which):
//   button      alone          with POWER held
//   V-A         forward        next weapon
//   HOLD        backward       previous weapon
//   MODE        turn left      strafe left
//   SAVE/MENU   turn right     strafe right
//   LIGHT       fire           use
// POWER is fixed: the menu key (key_menu_activate) on release if no other button was
// pressed while it was held. Held alone for 1 s it toggles the menu, for 4 s it closes the
// menu if it is open and opens the quit prompt (key_menu_quit), whose yes powers the meter
// off. POWER pressed while another button is down never counts as alone, so it only gives
// the menu key on release.
// Menu mapping: V-A up, HOLD down, MODE and SAVE adjust, LIGHT select, POWER tap closes.
// Prompt mapping (a menu message that wants y or n): LIGHT yes, POWER tap no.
// Flip swaps HOLD and V-A everywhere. Always-run sets key_speed to 256, which the engine
// treats as a held run key (g_game.c, key_speed >= NUMKEYS).
//
// A press is resolved once, with the mapping and shift state of that moment, and the key it
// sent goes up when that button is released. Taps (next/previous weapon, automap, menu
// keys, the POWER release) go down for one tic. Every key is down in at least one built
// ticcmd. Next and previous weapon follow Chocolate Doom's G_NextWeapon and press the
// weapon's number key, so G_BuildTiccmd sends the same BT_CHANGE.
//
// Game pad (pad.h): each II_Tic posts the pad's keys after those of the buttons, with the same
// taps. Its sticks, fire and use go into the ticcmd in G_BuildTiccmd.
//
// Engine integration: I_StartTic() calls I_InputTic(). BuildNewTic (d_loop.c) calls
// I_StartTic and D_ProcessEvents on every NetUpdate, but builds no ticcmd while it is 5
// tics ahead, so I_InputTic only works when D_WillBuildTic() says a ticcmd follows and
// otherwise leaves the edges in the platform ring. d_loop.c must export
// boolean D_WillBuildTic(void), true when maketic - gametic < 5 (the condition of
// BuildNewTic). Hold times (1 s, 4 s) count built tics. Code that waits for keys outside
// BuildNewTic calls II_Tic directly. Call II_Configure() after loading the settings.
// Without it the first tic applies preset 0, no flip, always-run on.

#pragma once

#include <stdint.h>

// Actions of a map entry
enum {
    II_NONE,
    II_FWD,      // held keys: key_up
    II_BACK,     // key_down
    II_TURNL,    // key_left
    II_TURNR,    // key_right
    II_STRAFEL,  // key_strafeleft
    II_STRAFER,  // key_straferight
    II_FIRE,     // key_fire
    II_USE,      // key_use
    II_RUN,      // key_speed (does nothing while always-run is on)
    II_STRAFE,   // key_strafe
    II_MAP,      // taps: key_map_toggle
    II_NEXTW,    // weapon number key of the next weapon
    II_PREVW,    // weapon number key of the previous weapon
    II_NACT
};

// Map layout: action of HOLD, SAVE, MODE, V-A, LIGHT (platform BTN_* order) alone, then the
// same five with POWER held.
#define II_MAPLEN 10
#define II_SHIFT 5
#define II_NPRESETS 2  // 0 = the table above, 1 = MODE and SAVE strafe and turn with POWER
extern const uint8_t ii_presets[II_NPRESETS][II_MAPLEN];

// Context of a tic. I_InputTic derives it from the menu state.
enum { II_GAME, II_MENU, II_PROMPT };

// map must stay valid (a preset). Releases nothing: keys already
// down go up with their buttons.
void II_Configure(const uint8_t *map, int flip, int always_run);
// One tic: drains plat_event(), resyncs with plat_buttons() after a full ring and posts
// the events, then the game pad's. Call it once per ticcmd that will be built.
void II_Tic(int ctx);
// II_Tic with the context of the engine's menu (menuactive, messageToPrint,
// messageNeedsInput) when D_WillBuildTic(), else nothing.
void I_InputTic(void);
