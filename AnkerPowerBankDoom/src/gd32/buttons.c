/* PD0 is the only control. Stock menu_button_pd0 reads it active low.
   PD3 is the C2 charge port and PC15 follows the USB charge cable, so
   neither is read. A tap fires and uses. Holding the first press walks.
   Two quick taps turn left for a short step, three turn right.
   Holding the second press turns left until release, holding the third
   turns right until release. A fourth tap resets the whole bank. */

#include "gd32.h"
#include "gestures.h"
#include "platform.h"

#include "d_event.h"
#include "d_ticcmd.h"

#define RING 8

#define TURN_MS 350u
/* Vanilla fast turn is 1280 per tic. A held turn uses the slow step so
   the heading can be stopped on a door. A quick double or triple stays fast. */
#define TURN_FAST 1280
#define TURN_HELD 320

static volatile uint32_t state;
static uint32_t raw_n;
static gest_t gest;
static uint8_t ring[RING];
static volatile uint8_t rd_i, wr_i;
static int turn_btn;
static volatile int turn_held;
static uint32_t turn_until;
static volatile uint8_t fire_latch;
static volatile uint8_t use_latch;
static volatile uint8_t splash_mode;
static volatile uint8_t splash_got;
static volatile uint8_t splash_act;

static void board_reset(void)
{
    __asm volatile("cpsid i");
    REG32(HOLD_PORT + GPIO_BOP) = HOLD_MASK;
    REG32(0xE000ED0Cu) = 0x05FA0004u;
    for (;;)
        ;
}

void buttons_splash_begin(void)
{
    splash_mode = 1;
    splash_got = 0;
    splash_act = 0;
    state = 0;
    fire_latch = 0;
    use_latch = 0;
    turn_held = 0;
    turn_btn = 0;
    rd_i = wr_i = 0;
}

int buttons_splash_pressed(void) { return splash_got != 0; }

/* The pre-game pages want to tell a tap from a double or a triple, so
   the gesture itself is handed over instead of a single flag. */
int buttons_splash_action(void)
{
    int a = splash_act;
    splash_act = 0;
    return a;
}

void buttons_splash_end(void)
{
    splash_mode = 0;
    splash_got = 0;
    splash_act = 0;
    state = 0;
    fire_latch = 0;
    use_latch = 0;
    turn_held = 0;
    turn_btn = 0;
    rd_i = wr_i = 0;
}

uint32_t plat_buttons(void) { return state; }

int plat_event(void)
{
    if (rd_i == wr_i)
        return -1;
    int e = ring[rd_i % RING];
    rd_i++;
    return e;
}

static void push_edge(int b, int press)
{
    if ((uint8_t)(wr_i - rd_i) >= RING)
        return;
    ring[wr_i % RING] = (uint8_t)(b | (press ? EV_PRESS : 0));
    wr_i++;
}

static void level(int b, int on)
{
    uint32_t bit = BTN_BIT(b);
    int have = (state & bit) != 0u;
    if (on == have)
        return;
    if (on)
        state |= bit;
    else
        state &= ~bit;
    push_edge(b, on);
}

static void pulse(int b)
{
    push_edge(b, 1);
    push_edge(b, 0);
}

/* A held turn stays down until the finger comes up. A one-shot turn from
   a quick double or triple keeps the same bit for TURN_MS. */
static void set_turn(int b)
{
    if (turn_btn == b)
        return;
    if (turn_btn)
        level(turn_btn, 0);
    turn_btn = b;
    if (b)
        level(b, 1);
}

void buttons_init(void)
{
    /* Stock leaves PD0 as an input with the pull-up on. Refresh that
       pull-up. PD15 is the power hold on this same port; BOP only sets
       the button bit. */
    REG32(BTN_PORT + GPIO_BOP) = BTN_MASK;
    gest_init(&gest, 450, 300);
    state = 0;
}

void buttons_sample(void)
{
    uint32_t istat;
    uint32_t raw;
    int down;
    int action;

    REG32(BTN_PORT + GPIO_BOP) = BTN_MASK;
    istat = REG32(BTN_PORT + GPIO_ISTAT);
    raw = board_button_down(istat) ? 1u : 0u;
    if (raw) {
        if (raw_n < 8u)
            raw_n++;
    } else if (raw_n) {
        raw_n--;
    }
    down = raw_n >= 4u;

    {
        uint32_t now = plat_ms();
        action = gest_feed(&gest, now, down);
        if (action == GEST_USE)
            board_reset();
        if (splash_mode) {
            if (action == GEST_FIRE)
                splash_got = 1;
            if (action == GEST_FIRE || action == GEST_LEFT || action == GEST_RIGHT)
                splash_act = (uint8_t)action;
        } else if (action == GEST_LEFT || action == GEST_RIGHT) {
            int b = action == GEST_LEFT ? BTN_MODE : BTN_SAVE;
            if (turn_btn == b && !turn_held)
                turn_held = 1;
            else if (turn_btn != b) {
                turn_held = 0;
                turn_until = now + TURN_MS;
            }
            set_turn(b);
        } else {
            if (turn_held || (int32_t)(now - turn_until) >= 0) {
                turn_held = 0;
                set_turn(0);
            }
            level(BTN_VA, action == GEST_FORWARD);
            if (action == GEST_FIRE) {
                fire_latch = 1;
                use_latch = 1;
                pulse(BTN_LIGHT);
            }
        }
    }
}

/* Runs after the keyboard ticcmd. The button bits win, so a press still
   moves the player when the event queue does not. */
void gd32_tic_hook(ticcmd_t *cmd)
{
    uint32_t b = state;
    if (b & BTN_BIT(BTN_VA))
        cmd->forwardmove = 0x32;
    if (b & BTN_BIT(BTN_MODE))
        cmd->angleturn = turn_held ? TURN_HELD : TURN_FAST;
    else if (b & BTN_BIT(BTN_SAVE))
        cmd->angleturn = turn_held ? -TURN_HELD : -TURN_FAST;
    if (fire_latch) {
        fire_latch = 0;
        cmd->buttons |= BT_ATTACK;
    }
    if (use_latch) {
        use_latch = 0;
        cmd->buttons |= BT_USE;
    }
    {
        uint8_t extra = ble_buttons();
        if (extra & 0x02u)
            cmd->forwardmove = 0x32;
        if (!(b & (BTN_BIT(BTN_MODE) | BTN_BIT(BTN_SAVE)))) {
            if (extra & 0x04u)
                cmd->angleturn = TURN_FAST;
            else if (extra & 0x08u)
                cmd->angleturn = -TURN_FAST;
        }
        if (extra & 0x01u)
            cmd->buttons |= BT_ATTACK | BT_USE;
    }
}
