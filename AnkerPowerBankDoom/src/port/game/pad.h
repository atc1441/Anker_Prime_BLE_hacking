// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Game pad state. The USB driver fills it, the game reads it once per built ticcmd.
//
// Driver API: pad_set(axis, buttons) with every report received, the five axes in PAD_LX ..
// PAD_DIAL order and the PAD_* button bits (pad_set adds PAD_PRESENT). pad_lost() when the pad
// goes away (unplugged, or no report for a while). Both may run in an interrupt. The game
// reads the buttons first and acts only while PAD_PRESENT is set, a report that lands between
// its reads mixes two reports for one tic, which is harmless. Buttons held when the pad
// comes (back) are not new presses.
//
// Axes are signed and 0 at the centre. The RC-N1 sends 364..1684 with the centre at 1024 on
// every axis, the driver stores raw - 1024 (about -660..660). X is positive to the right, Y
// positive pushed away from the player (up), the dial positive turned to the right. A pad
// without some of the buttons leaves their bits 0.
//
// Mapping (the meter's buttons keep working at the same time):
//   level, menu closed, no demo playing (G_BuildTiccmd, engine/g_game.c):
//     left Y        forward and back, analog
//     left X        strafe, analog
//     right X       turn, analog, full deflection turns at the vanilla fast turn rate
//     right Y       unused
//     FIRE, USE     fire and use while held
//     RUN           run while held (the same as without it while always-run is on)
//   keys posted through the chord resolver (port/game/i_input.c):
//     MENU          Escape: opens the menu, closes it, answers no to a prompt
//     ENTER         Enter (y in a prompt), on the title it opens the menu. Nothing in a
//                   level (no demo), where Enter would show the last message again.
//     left stick    flicks past PAD_FLICK are arrow keys while the menu is open, at once
//                   and again every PAD_REPEAT tics while held
//     MAP           automap toggle (a level, no demo)
//     NEXTW, PREVW  next and previous weapon, as the meter's buttons (a level, no demo)
//     dial          past PAD_DIAL_STEP next (right) or previous (left) weapon, once per turn,
//                   re-armed when it is back under PAD_DIAL_REARM
// Analog axes have a dead zone of PAD_DEAD and reach full output at PAD_DEAD + PAD_SPAN, a bit
// before the end of the travel. In between the output grows as a * (a + PAD_SPAN) for a past
// the dead zone, fine near the centre and steep towards the edge.

#pragma once

#include <stdint.h>

enum { PAD_LX, PAD_LY, PAD_RX, PAD_RY, PAD_DIAL, PAD_NAXES };

// Logical buttons. FIRE and USE equal BT_ATTACK and BT_USE of the ticcmd.
#define PAD_FIRE 0x01
#define PAD_USE 0x02
#define PAD_MENU 0x04
#define PAD_MAP 0x08
#define PAD_RUN 0x10
#define PAD_NEXTW 0x20
#define PAD_PREVW 0x40
#define PAD_ENTER 0x80
#define PAD_PRESENT 0x100

#define PAD_DEAD 60
#define PAD_SPAN 512  // a power of two, the curve divides by 2 * PAD_SPAN^2
#define PAD_FLICK 400
#define PAD_FLICK_OUT 250  // a held flick ends under this
#define PAD_REPEAT 8
#define PAD_DIAL_STEP 330
#define PAD_DIAL_REARM 120

typedef struct {
    int16_t axis[PAD_NAXES];
    uint16_t buttons;  // PAD_* bits, 0 without a pad
} pad_t;

extern volatile pad_t dmc_pad;

static inline void pad_set(const int16_t axis[PAD_NAXES], unsigned buttons)
{
    for (int i = 0; i < PAD_NAXES; i++)
        dmc_pad.axis[i] = axis[i];
    dmc_pad.buttons = (uint16_t)(buttons | PAD_PRESENT);
}

static inline void pad_lost(void)
{
    dmc_pad.buttons = 0;
}

// Axis value v past the dead zone, scaled to -max..max along the curve above
int II_PadAxis(int v, int max);
