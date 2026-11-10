/* One-button gestures for the power bank.
   A short press fires. Holding the first press walks forward.
   Two quick presses turn left once, three turn right once.
   Holding the second press turns left until release, holding the third
   turns right until release. A fourth press is GEST_USE. The board
   treats that as a full reset back to the battery screen.
   The firmware and the host test both call gest_feed. */

#ifndef GESTURES_H
#define GESTURES_H

#include <stdint.h>

enum gest_action {
    GEST_NONE = 0,
    GEST_FIRE,
    GEST_FORWARD,
    GEST_LEFT,
    GEST_RIGHT,
    GEST_USE
};

typedef struct gest {
    uint32_t long_ms;
    uint32_t gap_ms;
    uint32_t t_down;
    uint32_t t_up;
    uint8_t down;
    uint8_t holding;
    uint8_t taps;
    uint8_t hold; /* GEST_FORWARD, GEST_LEFT, or GEST_RIGHT while held */
} gest_t;

void gest_init(gest_t *g, uint32_t long_ms, uint32_t gap_ms);

/* now_ms is monotonic. down is 1 while the button is pressed.
   GEST_FORWARD, GEST_LEFT, and GEST_RIGHT repeat on every call while that
   hold continues. A released double reports GEST_LEFT once, a released
   triple reports GEST_RIGHT once. FIRE and USE are reported once, when
   the gap after the last release expires. */
int gest_feed(gest_t *g, uint32_t now_ms, int down);

#endif
