#include "gestures.h"

void gest_init(gest_t *g, uint32_t long_ms, uint32_t gap_ms)
{
    g->long_ms = long_ms;
    g->gap_ms = gap_ms;
    g->t_down = 0;
    g->t_up = 0;
    g->down = 0;
    g->holding = 0;
    g->taps = 0;
    g->hold = GEST_NONE;
}

static int finish_taps(gest_t *g)
{
    unsigned n = g->taps;
    g->taps = 0;
    if (n <= 1)
        return GEST_FIRE;
    if (n == 2)
        return GEST_LEFT;
    if (n == 3)
        return GEST_RIGHT;
    return GEST_USE;
}

static int begin_hold(gest_t *g)
{
    unsigned n = g->taps;
    g->holding = 1;
    g->taps = 0;
    if (n == 1)
        g->hold = GEST_LEFT;
    else if (n == 2)
        g->hold = GEST_RIGHT;
    else
        g->hold = GEST_FORWARD;
    return g->hold;
}

int gest_feed(gest_t *g, uint32_t now_ms, int down)
{
    int pressed = down != 0;
    int edge = pressed != (g->down != 0);

    if (edge && pressed) {
        g->t_down = now_ms;
        g->down = 1;
        return GEST_NONE;
    }

    if (edge && !pressed) {
        g->down = 0;
        if (g->holding) {
            g->holding = 0;
            g->taps = 0;
            return GEST_NONE;
        }
        if (g->taps < 255)
            g->taps++;
        g->t_up = now_ms;
        return GEST_NONE;
    }

    if (pressed && !g->holding && (now_ms - g->t_down) >= g->long_ms)
        return begin_hold(g);

    if (pressed && g->holding)
        return g->hold;

    if (!pressed && g->taps && (now_ms - g->t_up) >= g->gap_ms)
        return finish_taps(g);

    return GEST_NONE;
}
