// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright(C) 2005-2014 Simon Howard
// Copyright (C) 2026 Artur Bieniek
//
// Chord resolver for the six buttons, see i_input.h.

#include "i_input.h"

#include "pad.h"
#include "platform.h"

#include "d_event.h"
#include "doomdef.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "m_controls.h"
#include "m_menu.h"

// d_loop.c: a ticcmd is built after this I_StartTic
boolean D_WillBuildTic(void);

#define PWR BTN_BIT(BTN_POWER)
#define MENU_TICS (1 * TICRATE)
#define QUIT_TICS (4 * TICRATE)  // also stops the POWER timer

const uint8_t ii_presets[II_NPRESETS][II_MAPLEN] = {
    {II_BACK, II_TURNR, II_TURNL, II_FWD, II_FIRE, II_PREVW, II_STRAFER, II_STRAFEL, II_NEXTW, II_USE},
    {II_BACK, II_STRAFER, II_STRAFEL, II_FWD, II_FIRE, II_PREVW, II_TURNR, II_TURNL, II_NEXTW, II_USE},
};

// Default keys of m_controls.c per action
static const uint8_t act_key[II_NACT] = {
    [II_FWD] = KEY_UPARROW,     [II_BACK] = KEY_DOWNARROW,  [II_TURNL] = KEY_LEFTARROW,
    [II_TURNR] = KEY_RIGHTARROW, [II_STRAFEL] = KEY_STRAFE_L, [II_STRAFER] = KEY_STRAFE_R,
    [II_FIRE] = KEY_FIRE,       [II_USE] = KEY_USE,         [II_RUN] = KEY_RSHIFT,
    [II_STRAFE] = KEY_RALT,     [II_MAP] = KEY_TAB,
};
// Keys of HOLD, SAVE, MODE, V-A, LIGHT and of the POWER tap per context. The game takes
// the first five from the map.
static const uint8_t ctx_key[3][BTN_COUNT] = {
    {0, 0, 0, 0, 0, KEY_ESCAPE},
    {KEY_DOWNARROW, KEY_RIGHTARROW, KEY_LEFTARROW, KEY_UPARROW, KEY_ENTER, KEY_ESCAPE},
    {0, 0, 0, 0, 'y', 'n'},
};
// Chocolate Doom's weapon_order_table without the super shotgun
static const uint8_t weapon_order[8] = {wp_fist,     wp_chainsaw, wp_pistol, wp_shotgun,
                                        wp_chaingun, wp_missile,  wp_plasma, wp_bfg};

static const uint8_t *map;
static uint8_t flip;   // BTN_HOLD ^ BTN_VA when flipped
static uint8_t held;   // buttons treated as down
static uint8_t fresh;  // pressed during this tic
static uint8_t late;   // released in the tic of their press, handled at the next tic
static uint8_t pw_tics;
static uint8_t pw_notap;  // no tap on the POWER release (chord or menu)
// Key held down by each button, then keys tapped in this tic (up at the next), 0 none
static uint8_t own[BTN_COUNT + 8];

volatile pad_t dmc_pad;
static uint16_t pad_old;  // pad buttons of the tic before
static uint8_t pad_dir;   // arrow key of the held flick, 0 none
static uint8_t pad_rep;   // tics to its next tap, 0 waits for a new flick
static uint8_t pad_dial;  // the dial is past PAD_DIAL_STEP and has not come back yet

_Static_assert(PAD_FIRE == BT_ATTACK && PAD_USE == BT_USE, "pad bits are ticcmd buttons");

static void post(evtype_t type, int key)
{
    event_t ev = {.type = type, .data1 = key};
    D_PostEvent(&ev);
}

// Key up unless a held button or a tap still holds it
static void release_key(int k)
{
    for (unsigned i = 0; i < sizeof(own); i++)
        if (own[i] == k)
            return;
    post(ev_keyup, k);
}

static void tap(int k)
{
    post(ev_keydown, k);
    for (unsigned i = BTN_COUNT; i < sizeof(own); i++)
        if (!own[i]) {
            own[i] = (uint8_t)k;
            return;
        }
    post(ev_keyup, k);
}

// Number key of the next selectable weapon in direction dir, as G_NextWeapon
static int next_weapon(int dir)
{
    const player_t *p = &players[consoleplayer];
    int w = p->pendingweapon == wp_nochange ? p->readyweapon : p->pendingweapon;
    int i = 0;
    while (i < 7 && weapon_order[i] != w)
        i++;
    int start = i;
    do {
        i = (i + dir) & 7;
        w = weapon_order[i];
    } while (i != start &&
             !(p->weaponowned[w] &&
               !(w == wp_fist && p->weaponowned[wp_chainsaw] && !p->powers[pw_strength])));
    return '1' + (w == wp_chainsaw ? wp_fist : w);
}

static void edge(int b, int down, int ctx)
{
    unsigned bit = 1u << b;
    if (!down) {
        if (!(held & bit))
            return;
        if (fresh & bit) {  // keep it down for one tic
            late |= (uint8_t)bit;
            return;
        }
        held &= (uint8_t)~bit;
        if (b == BTN_POWER) {
            if (!pw_notap)
                tap(ctx_key[ctx][BTN_POWER]);
            return;
        }
        int k = own[b];
        own[b] = 0;
        if (k)
            release_key(k);
        return;
    }
    if (held & bit) {  // repeated edge, or pressed again before a late release
        late &= (uint8_t)~bit;
        return;
    }
    unsigned others = held & ~late;
    held |= (uint8_t)bit;
    fresh |= (uint8_t)bit;
    if (b == BTN_POWER) {
        pw_tics = others ? QUIT_TICS : 0;
        pw_notap = 0;
        return;
    }
    int shift = others & PWR;
    if (shift) {
        pw_tics = QUIT_TICS;
        pw_notap = 1;
    }
    int lb = b == BTN_HOLD || b == BTN_VA ? b ^ flip : b;
    int k = ctx_key[ctx][lb], hold = 0;
    if (ctx == II_GAME) {
        int a = map[lb + (shift ? II_SHIFT : 0)];
        if (a == II_NEXTW || a == II_PREVW) {
            k = next_weapon(a == II_NEXTW ? 1 : -1);
        } else {
            k = a < II_NACT ? act_key[a] : 0;
            hold = a != II_MAP;
        }
    }
    if (!k)
        return;
    if (!hold) {
        tap(k);
        return;
    }
    post(ev_keydown, k);
    own[b] = (uint8_t)k;
}

int II_PadAxis(int v, int max)
{
    int a = (v < 0 ? -v : v) - PAD_DEAD;
    if (a <= 0)
        return 0;
    if (a > PAD_SPAN)
        a = PAD_SPAN;
    a = (int)((unsigned)(max * a * (a + PAD_SPAN)) / (2u * PAD_SPAN * PAD_SPAN));
    return v < 0 ? -a : a;
}

// The keys of the game pad in one tic, see pad.h
static void pad_tic(int ctx)
{
    // The first tic with the pad present takes its buttons as held, not as new presses
    unsigned b = dmc_pad.buttons, down = pad_old & PAD_PRESENT ? b & ~pad_old : 0;
    pad_old = (uint16_t)b;
    if (!(b & PAD_PRESENT))
        return;
    if (down & PAD_MENU)
        tap(KEY_ESCAPE);
    // Enter in a level would show the last message again (key_message_refresh)
    if ((down & PAD_ENTER) && (ctx != II_GAME || gamestate != GS_LEVEL || demoplayback))
        tap(ctx == II_PROMPT ? 'y' : KEY_ENTER);
    // The flick of the left stick, followed in every context, so a stick held while the menu
    // opens waits for a new flick
    int x = dmc_pad.axis[PAD_LX], y = dmc_pad.axis[PAD_LY];
    int t = pad_dir ? PAD_FLICK_OUT : PAD_FLICK;
    int k = y > t ? KEY_UPARROW : -y > t ? KEY_DOWNARROW : x > t ? KEY_RIGHTARROW
          : -x > t ? KEY_LEFTARROW : 0;
    if (k != pad_dir) {
        pad_dir = (uint8_t)k;
        pad_rep = 1;
    }
    // Weapon steps: the buttons, and the dial once per turn past PAD_DIAL_STEP
    int d = dmc_pad.axis[PAD_DIAL], w = down & PAD_NEXTW ? 1 : down & PAD_PREVW ? -1 : 0;
    if (d < PAD_DIAL_REARM && d > -PAD_DIAL_REARM) {
        pad_dial = 0;
    } else if (!pad_dial && (d >= PAD_DIAL_STEP || -d >= PAD_DIAL_STEP)) {
        pad_dial = 1;
        w = d > 0 ? 1 : -1;
    }
    if (ctx != II_GAME) {
        if (k && pad_rep && !--pad_rep) {
            tap(k);
            pad_rep = PAD_REPEAT;
        }
        return;
    }
    pad_rep = 0;
    if (demoplayback)
        return;
    if (down & PAD_MAP)
        tap(KEY_TAB);
    if (w)
        tap(next_weapon(w));
}

// Edges of the buttons in mask to the state in now
static void sync(unsigned mask, uint32_t now, int ctx)
{
    for (int b = 0; b < BTN_COUNT; b++)
        if (mask >> b & 1)
            edge(b, now >> b & 1, ctx);
}

void II_Configure(const uint8_t *m, int fl, int always_run)
{
    map = m;
    flip = fl ? BTN_HOLD ^ BTN_VA : 0;
    key_speed = always_run ? 256 : KEY_RSHIFT;
}

void II_Tic(int ctx)
{
    if (!map)
        II_Configure(ii_presets[0], 0, 1);
    for (unsigned i = BTN_COUNT; i < sizeof(own); i++) {
        int k = own[i];
        own[i] = 0;
        if (k)
            release_key(k);
    }
    fresh = 0;
    unsigned l = late;
    late = 0;
    sync(l, 0, ctx);
    for (int ev; (ev = plat_event()) >= 0;)
        edge(ev & 7, ev & EV_PRESS, ctx);
    // Edges dropped by a full ring: follow the debounced state
    uint32_t now = plat_buttons();
    sync((held & ~late) ^ now, now, ctx);
    // POWER held alone. A chord or a press during another press stops the count.
    if ((held & PWR) && pw_tics < QUIT_TICS) {
        if (++pw_tics == MENU_TICS) {
            pw_notap = 1;
            tap(KEY_ESCAPE);
        } else if (pw_tics == QUIT_TICS) {
            if (ctx != II_GAME)
                tap(KEY_ESCAPE);
            tap(KEY_F10);
        }
    }
    pad_tic(ctx);
}

void I_InputTic(void)
{
    if (D_WillBuildTic())
        II_Tic(messageToPrint ? (messageNeedsInput ? II_PROMPT : II_MENU)
                              : menuactive ? II_MENU : II_GAME);
}
