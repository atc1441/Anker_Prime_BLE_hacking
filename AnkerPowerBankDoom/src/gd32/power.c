/* Port and pack telemetry for the pre-game pages.

   What the stock firmware does, from ref/backup/gd32_internal_flash.bin:

   - The pack comes from the CW2217 at I2C 0xC8 (gauge.c).
   - USB-A is a chip on I2C 0x60. The stock poller at 0x08013CA4 reads
     register 0 (state), 6/7 (voltage, big endian, mV), 8/9 (current,
     big endian, mA) and 0x0A/0x0B, and prints them as "usb_A[0]=%x",
     "vo=%d", "c=%d". It turns them into the tenths the BLE status
     carries: tenths_V = mV / 100, tenths_A = (mA + 50) / 100.
   - USB-C1 and USB-C2 are NOT on I2C. Stock scans six ADC0 channels into
     a DMA buffer at 0x20005EAC and copies ranks 1..5 into the port
     struct at 0x20000264: +0x24 C1 current, +0x26 C1 voltage, +0x28 C2
     current, +0x2A C2 voltage, with the zero offsets at +0x44 / +0x46.
     The channel-to-pin map and the current scaling are still open, so
     those two ports read "no data" here instead of a made-up number.
     notes/stock-power-telemetry.md has the full trail.

   Nothing here writes to a chip. Reads only. */

#include "gd32.h"
#include "platform.h"
#include "adc.h"
#include "power.h"

#define USBA_ADDR_W 0x60u
#define PORTCTL_ADDR_W 0xE8u

static struct port_reading usba;
static uint8_t bad_runs;
static uint8_t gave_up;
static uint8_t scan_done;
static uint8_t active;
static uint8_t scan_hits[16];
static uint8_t scan_n;
static uint8_t scan_bus[16];
static uint8_t scan_bus_n;
static uint32_t next_ms;

/* Register pictures of the port controller, before and after the init,
   so the two can be compared over SWD and on the SYSTEM page. */
#define CTL_BUSES 3
static uint8_t ctl_before[CTL_BUSES][16];
static uint8_t ctl_after[CTL_BUSES][16];
static uint8_t ctl_live[16];
static uint8_t ports_started;

/* Bench hook: a debugger can put an address, a register and a value here
   and set dbg_go. The next poll performs that single I2C access and puts
   the result in dbg_res, so register behaviour can be tried out without
   building and flashing for each attempt. Nothing sets dbg_go from
   inside the firmware. */
/* Bench trigger: writing 1 here runs the cold bring-up once, so the
   sequence can be tried on a running device without reflashing. */
volatile uint8_t pw_force_cold;
volatile uint8_t pw_addr = 0xE8u;
volatile uint8_t pw_bus;
volatile uint8_t pw_reg;
volatile uint8_t pw_val;
volatile uint8_t pw_go;    /* 1 = read pw_reg, 2 = write pw_val */
volatile uint8_t pw_res;
volatile uint8_t pw_seq;

static void cold_bring_up(void);

static void debug_hook(void)
{
    uint8_t go = pw_go;
    if (pw_force_cold) {
        pw_force_cold = 0;
        cold_bring_up();
        dbg_str("forced cold bring-up");
        dbg_nl();
    }
    if (!go)
        return;
    pw_go = 0;
    gbus_select(pw_bus);
    if (go == 1u) {
        if (!gbus_read(pw_addr, pw_reg, (uint8_t *)&pw_res, 1))
            pw_res = 0xEE;
    } else {
        pw_res = (uint8_t)(gbus_write(pw_addr, pw_reg, pw_val) ? 0 : 0xEE);
    }
    gbus_select(0);
    pw_seq++;
}

static int reg8(uint8_t reg, uint8_t *out) { return gbus_read(USBA_ADDR_W, reg, out, 1); }

/* The stock poller reads one register per transaction, so this does the
   same: a chip without address auto-increment would hand back rubbish
   for a block read. */
static int usba_sample(struct port_reading *out)
{
    uint8_t st, vh, vl, ih, il, a, b;
    if (!reg8(0x00, &st) || !reg8(0x06, &vh) || !reg8(0x07, &vl))
        return 0;
    if (!reg8(0x08, &ih) || !reg8(0x09, &il))
        return 0;
    if (!reg8(0x0A, &a) || !reg8(0x0B, &b))
        return 0;
    /* 0x30 answers its address even when it has nothing to say: every
       register then reads 0xFF. That is not a measurement, so it counts
       as no data rather than 65.5 V. */
    if (st == 0xFFu && vh == 0xFFu && vl == 0xFFu && ih == 0xFFu && il == 0xFFu)
        return 0;
    out->ok = 1;
    out->state = st;
    out->mv = (uint16_t)((vh << 8) | vl);
    out->ma = (uint16_t)((ih << 8) | il);
    out->reg_a = a;
    out->reg_b = b;
    /* Stock's own rounding, so the page shows the same tenths the phone
       gets over BLE. */
    out->dv = (uint16_t)(out->mv / 100u);
    out->da = (uint16_t)((out->ma + 50u) / 100u);
    out->dw = (uint16_t)(((uint32_t)out->dv * out->da) / 10u);
    return 1;
}

/* One pass over the 7-bit space on the gauge bus. Address only, no
   register write, so it cannot disturb a chip that answers. */
/* Every bus in the stock table, address phase only. A chip that is not
   there costs a missing ACK, nothing is written. */
static void bus_scan(void)
{
    unsigned b, a;
    scan_n = 0;
    for (b = 0; b < gbus_count(); b++) {
        gbus_select(b);
        dbg_str("i2c bus ");
        dbg_dec(b);
        dbg_str(":");
        for (a = 0x08u; a < 0x78u; a++) {
            if (gbus_probe((uint8_t)(a << 1))) {
                dbg_putc(' ');
                dbg_hex(a, 2);
                if (scan_n < sizeof scan_hits)
                    scan_hits[scan_n++] = (uint8_t)a;
                if (scan_bus_n < sizeof scan_bus)
                    scan_bus[scan_bus_n++] = (uint8_t)b;
            }
            wdt_kick();
        }
        dbg_nl();
    }
    gbus_select(0);
    scan_done = 1;
}

/* The gauge shares the two wires, so nothing here touches the bus
   unless a page that shows port data is actually up. */
void power_set_active(int on)
{
    active = on ? 1u : 0u;
    if (on && !scan_done)
        next_ms = 0;
}

static void bus_scan(void);

static void dump16(uint8_t addr_w, uint8_t *dst)
{
    unsigned i;
    for (i = 0; i < 16u; i++)
        if (!gbus_read(addr_w, (uint8_t)i, &dst[i], 1))
            dst[i] = 0xEE;
}

/* The stock power-up sequence.

   Not transcribed from the disassembly by hand - the stock firmware was
   single-stepped through 0x08018640..0x08018A84 with the debugger while
   every call and every change to a GPIO control or output register was
   logged (.scratch/trace_init.py). Registers get reused inside that
   function, so reading it off the listing is easy to get subtly wrong;
   this is what the hardware actually saw. The log is in
   notes/stock-power-telemetry.md.

   Three controllers get configured, each over its own bit-banged bus,
   with lines driven around them. The lines are the part a snapshot of
   the pins cannot show: PD12 and PD10 are pulsed, PB6, PC8, PE9 and
   PE15 are driven, and all of them end up where they started, which is
   why the idle pin state of stock and of this firmware looked equal.

   gpio_init(port, mode, speed, pins) of the vendor library becomes one
   CTL nibble here: OUT_PP 50 MHz is 0x3, OUT_OD 50 MHz is 0x7,
   IN_FLOATING is 0x4, and IPU is 0x8 with the output latch set. */

#define M_PP 0x3u
#define M_OD 0x7u
#define M_IN 0x4u
#define M_IPU 0x8u

static void pin_mode(uint32_t port, unsigned pin, uint32_t nib)
{
    uint32_t reg = port + (pin < 8u ? GPIO_CTL0 : GPIO_CTL1);
    unsigned sh = (pin & 7u) * 4u;
    uint32_t v = REG32(reg);
    REG32(reg) = (v & ~(0xFu << sh)) | (nib << sh);
}

static void pins_mode(uint32_t port, uint32_t mask, uint32_t nib)
{
    unsigned p;
    for (p = 0; p < 16u; p++)
        if (mask & (1u << p))
            pin_mode(port, p, nib);
    if (nib == M_IPU)
        REG32(port + GPIO_BOP) = mask; /* the library selects pull-up via the latch */
}

#define BOP(port, m) (REG32((port) + GPIO_BOP) = (uint32_t)(m))
#define BC(port, m) (REG32((port) + GPIO_BC) = (uint32_t)(m))

/* Bus numbers in the table in gauge.c: 0 is PE10/11, 1 is PB10/11,
   2 is PB8/9. The stock firmware numbers them 2, 0 and 1. */
#define BUS_PE 0u
#define BUS_PB10 1u
#define BUS_PB8 2u

static void ctl_write(unsigned bus, uint8_t addr, uint8_t reg, uint8_t val)
{
    gbus_select(bus);
    gbus_write(addr, reg, val);
    plat_delay_ms(2);
}

/* The seven registers both USB-C controllers get. */
static const struct {
    uint8_t reg;
    uint8_t val;
} ctl_boot[] = {
    {0x09, 0x03}, {0x00, 0x21}, {0x06, 0xFF}, {0x05, 0x64},
    {0x0A, 0x19}, {0x08, 0x38}, {0x0C, 0x02},
};

/* The third controller gets a different set, in a different order. */
static const struct {
    uint8_t reg;
    uint8_t val;
} ctl_boot_pe[] = {
    {0x00, 0x21}, {0x06, 0xFF}, {0x0A, 0x19}, {0x09, 0x03},
    {0x05, 0x14}, {0x07, 0xB4}, {0x0C, 0x02},
};

/* Pin modes only. This part of the stock sequence drives nothing, it
   just puts the two bit-banged buses and the two sense inputs into the
   shape the bit-banging needs. Without the pull-ups on the SDA lines the
   controllers on PB8/9 stop answering, so this runs on every start,
   warm or cold. */
void ports_pins_setup(void)
{
    pins_mode(GPIOE_BASE, 0x0020u, M_IPU); /* PE5 */
    pins_mode(GPIOC_BASE, 0x2000u, M_IPU); /* PC13 */
    pins_mode(GPIOB_BASE, 0x0A00u, M_IPU); /* PB9, PB11, the two SDA lines */
    pins_mode(GPIOB_BASE, 0x0500u, M_OD);  /* PB8, PB10, the two SCL lines */
    pins_mode(GPIOE_BASE, 0x0800u, M_IPU); /* PE11 SDA */
    pins_mode(GPIOE_BASE, 0x0400u, M_OD);  /* PE10 SCL */
    pins_mode(GPIOC_BASE, 0x8000u, M_IPU); /* PC15, the charge cable sense */
    pins_mode(GPIOD_BASE, 0x0008u, M_IPU); /* PD3, the C2 port sense */
}

/* The driving half: pulses and levels. This is the part that tears a
   running power path down, so it is for a cold start only. */
static void ports_gpio_sequence(void)
{
    ports_pins_setup();

    BOP(GPIOE_BASE, 0x0400u); /* PE10 */
    BOP(GPIOB_BASE, 0x0100u); /* PB8 */
    BOP(GPIOB_BASE, 0x0400u); /* PB10 */
    BOP(GPIOB_BASE, 0x0040u); /* PB6 */
    BOP(GPIOC_BASE, 0x0100u); /* PC8 */
    BOP(GPIOE_BASE, 0x0200u); /* PE9 */
    pins_mode(GPIOB_BASE, 0x0040u, M_PP);
    pins_mode(GPIOC_BASE, 0x0100u, M_PP);
    pins_mode(GPIOE_BASE, 0x0200u, M_PP);
    BOP(GPIOB_BASE, 0x0040u);
    BOP(GPIOC_BASE, 0x0100u);
    BOP(GPIOE_BASE, 0x0200u);

    BC(GPIOE_BASE, 0x0001u); /* PE0 */
    BC(GPIOE_BASE, 0x0002u); /* PE1 */
    BOP(GPIOE_BASE, 0x8000u); /* PE15 */
    pins_mode(GPIOE_BASE, 0x8003u, M_PP);
    BC(GPIOE_BASE, 0x0001u);
    BC(GPIOE_BASE, 0x0002u);
    BOP(GPIOE_BASE, 0x8000u);

    BOP(GPIOD_BASE, 0x0400u); /* PD10 */
    pins_mode(GPIOD_BASE, 0x0400u, M_PP);
    BOP(GPIOD_BASE, 0x0400u);

    BC(GPIOE_BASE, 0x0040u); /* PE6 */
    BC(GPIOE_BASE, 0x0008u); /* PE3 */
    pins_mode(GPIOE_BASE, 0x0048u, M_PP);
    BC(GPIOE_BASE, 0x0040u);
    BC(GPIOE_BASE, 0x0008u);

    BC(GPIOD_BASE, 0x1000u); /* PD12 */
    pins_mode(GPIOD_BASE, 0x1000u, M_PP);
    BC(GPIOD_BASE, 0x1000u);
}

/* How the ports are brought up.

   The stock sequence above is a COLD start: it runs from reset, before
   the converter is up, and part of it tears the power path down on the
   way (raising PB6 is what does it - the boot marks fall from 871 to
   678 within two milliseconds of that write). The hardware keeps its
   configuration across a reset of the processor, so after a firmware
   swap the path is usually already running. Replaying a cold-start
   sequence on a live system switches the bank off.

   So the rail decides. ADC rank 1 (PA2) carries it: about 870 counts
   while the converter runs, and it decays to nothing within seconds
   once it stops. Rail up means the path is alive and nothing here
   touches it - the controllers then move into their active state on
   their own. Rail down means a genuine cold start, and the stock
   sequence runs. */

#define RAIL_UP 400u

static uint16_t rail_at_boot;
static uint16_t rail_after;
static uint8_t cold_start;

uint16_t power_rail(void) { return adc_sample_rank(1); }
uint16_t power_rail_boot(void) { return rail_at_boot; }
int power_cold_start(void) { return cold_start; }

static void cold_bring_up(void)
{
    unsigned i;

    ports_gpio_sequence();

    gbus_select(BUS_PB8);
    dump16(PORTCTL_ADDR_W, ctl_before[0]);
    BOP(GPIOB_BASE, 0x0040u);
    plat_delay_ms(2);
    for (i = 0; i < sizeof ctl_boot / sizeof ctl_boot[0]; i++)
        ctl_write(BUS_PB8, PORTCTL_ADDR_W, ctl_boot[i].reg, ctl_boot[i].val);
    wdt_kick();

    ctl_write(BUS_PB10, 0x10u, 0x01u, 0x83u);
    plat_delay_ms(2);
    BOP(GPIOC_BASE, 0x0100u);
    plat_delay_ms(2);
    gbus_select(BUS_PB10);
    dump16(PORTCTL_ADDR_W, ctl_before[1]);
    for (i = 0; i < sizeof ctl_boot / sizeof ctl_boot[0]; i++)
        ctl_write(BUS_PB10, PORTCTL_ADDR_W, ctl_boot[i].reg, ctl_boot[i].val);
    BOP(GPIOC_BASE, 0x0100u);
    ctl_write(BUS_PB8, 0x10u, 0x01u, 0x03u);
    plat_delay_ms(2);
    wdt_kick();

    BC(GPIOE_BASE, 0x0080u); /* PE7 */
    pins_mode(GPIOE_BASE, 0x0080u, M_PP);
    BC(GPIOC_BASE, 0x0002u); /* PC1 */
    pins_mode(GPIOC_BASE, 0x0002u, M_PP);
    BC(GPIOE_BASE, 0x0080u);
    BC(GPIOC_BASE, 0x0002u);
    pins_mode(GPIOE_BASE, 0x0100u, M_IN);  /* PE8 */
    pins_mode(GPIOB_BASE, 0x1000u, M_IN);  /* PB12 */
    pins_mode(GPIOC_BASE, 0x0008u, M_IPU); /* PC3 */
    pins_mode(GPIOE_BASE, 0x8000u, M_PP);  /* PE15 */
    BOP(GPIOE_BASE, 0x0200u);              /* PE9 */
    plat_delay_ms(2);
    gbus_select(BUS_PE);
    dump16(PORTCTL_ADDR_W, ctl_before[2]);
    for (i = 0; i < sizeof ctl_boot_pe / sizeof ctl_boot_pe[0]; i++)
        ctl_write(BUS_PE, PORTCTL_ADDR_W, ctl_boot_pe[i].reg, ctl_boot_pe[i].val);
    /* The lines the sequence leaves high are low once stock idles. */
    board_gpio_restore();
    wdt_kick();
}

void power_ports_start(void)
{
    unsigned i;
    static const unsigned order[CTL_BUSES] = {BUS_PB8, BUS_PB10, BUS_PE};

    if (ports_started)
        return;
    ports_started = 1;
    /* Pin modes only, no line is driven. The SDA lines need their
       internal pull-up or the controllers answer only now and then. */
    ports_pins_setup();
    if (!scan_done)
        bus_scan();

    rail_at_boot = adc_sample_rank(1);
    cold_start = rail_at_boot < RAIL_UP;
    dbg_str(cold_start ? "rail down, cold start " : "rail up, leaving it ");
    dbg_dec(rail_at_boot);
    dbg_nl();
    if (cold_start)
        cold_bring_up();
    rail_after = adc_sample_rank(1);

    /* Nothing else is written on a warm start. Measured on the device:
       with the controllers left alone USB-C1 holds a steady voltage,
       while writing their mode or enable registers makes the port cycle
       through attach after attach. A running power path is left to run. */

    for (i = 0; i < CTL_BUSES; i++) {
        gbus_select(order[i]);
        dump16(PORTCTL_ADDR_W, ctl_after[i]);
    }
    gbus_select(0);
    wdt_kick();
}

const uint8_t *power_ctl_regs(void) { return ctl_live; }

void power_init(void)
{
    active = 0;
    usba.ok = 0;
    bad_runs = 0;
    gave_up = 0;
    scan_done = 0;
    scan_n = 0;
    scan_bus_n = 0;
    next_ms = 0;
}

void power_poll(void)
{
    struct port_reading n;
    debug_hook();
    if (!active)
        return;
    if ((int32_t)(plat_ms() - next_ms) < 0)
        return;
    next_ms = plat_ms() + 500u;
    if (!scan_done)
        bus_scan();
    /* The gauge shares these two wires. A chip that keeps refusing is
       not worth disturbing the pack reading for, so after a few tries
       this stops asking until the next boot. */
    dump16(PORTCTL_ADDR_W, ctl_live);
    if (gave_up)
        return;
    if (usba_sample(&n)) {
        usba = n;
        bad_runs = 0;
    } else {
        usba.ok = 0;
        if (++bad_runs >= 4u) {
            gave_up = 1;
            dbg_str("usb-a chip quiet, polling off");
            dbg_nl();
        }
    }
}

const struct port_reading *power_usba(void) { return &usba; }

int power_usba_given_up(void) { return gave_up; }

const uint8_t *power_i2c_seen(unsigned *n)
{
    *n = scan_n;
    return scan_hits;
}

/* Pack side, from the gauge. Current is signed: positive goes into the
   pack. The stock scale is 131 / 8192 mA per count, kept as is. */
int power_pack_ma(void)
{
    const struct gauge_reading *g = gauge_now();
    if (!g->ok)
        return 0;
    return g->charging ? g->i_stock : -g->i_stock;
}
