/* CW2217 fuel gauge. Stock talks to it as I2C address 0x64 on bus 5.
   The pin table at 0x08026FBC puts that bus on GPIOE: SCL is PE10
   (mask 0x400) and SDA is PE11 (mask 0x800). PD10 and PD11 are a
   different stock output and are put back. Voltage uses the CW2217
   datasheet, 5/16 mV per count. The stock display word (7 * raw) >> 5
   and the stock state of charge are kept beside it. */

#include "gd32.h"
#include "platform.h"
#include "power.h"

#define ADDR_W 0xC8u

/* The stock pin table at 0x08026FBC lists six bit-banged buses on four
   distinct pin pairs. All of them sit on pins 8..11, so one CTL1 nibble
   per line is enough. gpio_restore() already leaves PB8..PB11 and
   PC10/PC11 as released open-drain outputs, the way stock holds them. */
static const struct i2c_bus {
    uint32_t port;
    uint8_t scl, sda;
} buses[] = {
    {GPIOE_BASE, 10u, 11u}, /* 0: gauge, USB-A chip, port controller */
    {GPIOB_BASE, 10u, 11u}, /* 1 */
    {GPIOB_BASE, 8u, 9u},   /* 2 */
    {GPIOC_BASE, 10u, 11u}, /* 3 */
};
#define BUS_COUNT (sizeof buses / sizeof buses[0])

static const struct i2c_bus *bus = &buses[0];

#define SCL_PIN (bus->scl)
#define SDA_PIN (bus->sda)
#define SCL_MASK (1u << bus->scl)
#define SDA_MASK (1u << bus->sda)

unsigned gbus_count(void) { return BUS_COUNT; }

/* Leaves the previous bus released. */
void gbus_select(unsigned n)
{
    if (n >= BUS_COUNT)
        return;
    bus = &buses[n];
}

static struct gauge_reading live;
static uint32_t next_ms;
static uint32_t wake_ms;
static uint8_t announced;

static void delay_us(unsigned us)
{
    uint32_t n = us * (CPU_HZ / 1000000u);
    uint32_t t = DWT_CYCCNT;
    while ((DWT_CYCCNT - t) < n)
        ;
}

static void mode_pin(unsigned pin, uint32_t nibble)
{
    uint32_t shift = (pin - 8u) * 4u;
    uint32_t ctl = REG32(bus->port + GPIO_CTL1);
    ctl = (ctl & ~(0xFu << shift)) | (nibble << shift);
    REG32(bus->port + GPIO_CTL1) = ctl;
}

static void line_release(unsigned pin, uint32_t mask)
{
    REG32(bus->port + GPIO_BOP) = mask;
    mode_pin(pin, 0x8u);
}

static void line_low(unsigned pin, uint32_t mask)
{
    mode_pin(pin, 0x2u);
    REG32(bus->port + GPIO_BC) = mask;
}

static int sda_high(void)
{
    return (REG32(bus->port + GPIO_ISTAT) & SDA_MASK) != 0u;
}

static int scl_rise(void)
{
    line_release(SCL_PIN, SCL_MASK);
    for (int i = 0; i < 40; i++) {
        if (REG32(GPIOE_BASE + GPIO_ISTAT) & SCL_MASK)
            return 1;
        delay_us(2);
    }
    return 0;
}

static void i2c_start(void)
{
    line_release(SDA_PIN, SDA_MASK);
    line_release(SCL_PIN, SCL_MASK);
    delay_us(5);
    line_low(SDA_PIN, SDA_MASK);
    delay_us(5);
    line_low(SCL_PIN, SCL_MASK);
    delay_us(4);
}

static void i2c_stop(void)
{
    line_low(SDA_PIN, SDA_MASK);
    delay_us(4);
    line_release(SCL_PIN, SCL_MASK);
    delay_us(5);
    line_release(SDA_PIN, SDA_MASK);
    delay_us(5);
}

static int i2c_write(uint8_t byte)
{
    int ack;
    for (int i = 7; i >= 0; i--) {
        if (byte & (1u << i))
            line_release(SDA_PIN, SDA_MASK);
        else
            line_low(SDA_PIN, SDA_MASK);
        delay_us(2);
        if (!scl_rise())
            return 0;
        delay_us(4);
        line_low(SCL_PIN, SCL_MASK);
        delay_us(2);
    }
    line_release(SDA_PIN, SDA_MASK);
    delay_us(2);
    if (!scl_rise())
        return 0;
    delay_us(3);
    ack = !sda_high();
    line_low(SCL_PIN, SCL_MASK);
    delay_us(2);
    return ack;
}

static int i2c_read(int ack, uint8_t *out)
{
    uint8_t v = 0;
    line_release(SDA_PIN, SDA_MASK);
    for (int i = 7; i >= 0; i--) {
        if (!scl_rise())
            return 0;
        delay_us(3);
        if (sda_high())
            v |= (uint8_t)(1u << i);
        line_low(SCL_PIN, SCL_MASK);
        delay_us(2);
    }
    if (ack)
        line_low(SDA_PIN, SDA_MASK);
    else
        line_release(SDA_PIN, SDA_MASK);
    delay_us(2);
    if (!scl_rise())
        return 0;
    delay_us(3);
    line_low(SCL_PIN, SCL_MASK);
    line_release(SDA_PIN, SDA_MASK);
    *out = v;
    return 1;
}

/* A slave that was interrupted mid-byte can keep SDA low and block
   every transfer after it. Nine clocks with SDA released walk it out of
   that byte, then a STOP puts the bus back at idle. */
static void i2c_recover(void)
{
    int i;
    line_release(SDA_PIN, SDA_MASK);
    for (i = 0; i < 9; i++) {
        line_low(SCL_PIN, SCL_MASK);
        delay_us(5);
        line_release(SCL_PIN, SCL_MASK);
        delay_us(5);
        if (sda_high())
            break;
    }
    i2c_stop();
    delay_us(20);
}

/* The gauge sits on the PE10/PE11 bus. The stock pin table lists three
   entries on those two pins, so more than one chip shares them: the
   address is a parameter and gbus_read is what the port telemetry uses
   to talk to the other chip on the same wires. */
int gbus_read(uint8_t addr_w, uint8_t reg, uint8_t *dst, unsigned n)
{
    unsigned i;
    i2c_start();
    if (!i2c_write(addr_w) || !i2c_write(reg)) {
        i2c_stop();
        i2c_recover();
        return 0;
    }
    i2c_start();
    if (!i2c_write(addr_w | 1u)) {
        i2c_stop();
        i2c_recover();
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (!i2c_read(i + 1u < n, dst + i)) {
            i2c_stop();
            i2c_recover();
            return 0;
        }
    }
    i2c_stop();
    delay_us(30);
    return 1;
}

int gbus_write(uint8_t addr_w, uint8_t reg, uint8_t val)
{
    int ok;
    i2c_start();
    ok = i2c_write(addr_w) && i2c_write(reg) && i2c_write(val);
    i2c_stop();
    if (!ok)
        i2c_recover();
    delay_us(30);
    return ok;
}

/* START, address, STOP. No register is touched, so a chip that is not
   there only costs a missing ACK. */
int gbus_probe(uint8_t addr_w)
{
    int ack;
    i2c_start();
    ack = i2c_write(addr_w);
    i2c_stop();
    if (!ack)
        i2c_recover();
    delay_us(30);
    return ack;
}

static int read_at(uint8_t reg, uint8_t *dst, unsigned n)
{
    return gbus_read(ADDR_W, reg, dst, n);
}

/* One register per transaction. A five-byte burst from 0x02 hands back
   the two voltage bytes and then rubbish on this chip - that is what
   made the state of charge read 0 and the temperature 87 C while single
   reads of the same registers gave 100 % and 25.5 C. */
static int reg(uint8_t r, uint8_t *v) { return read_at(r, v, 1); }

static int gauge_sample(struct gauge_reading *out)
{
    uint8_t v_h, v_l, soc_i, soc_f, temp;
    uint8_t cur_h, cur_l, cyc_h, cyc_l, status, mode;
    uint16_t vraw, iraw, mag;
    struct gauge_reading n;

    if (!reg(0x02, &v_h) || !reg(0x03, &v_l))
        return 0;
    if (!reg(0x04, &soc_i) || !reg(0x05, &soc_f) || !reg(0x06, &temp))
        return 0;
    if (!reg(0x0E, &cur_h) || !reg(0x0F, &cur_l))
        return 0;
    if (!reg(0xA4, &cyc_h) || !reg(0xA5, &cyc_l) || !reg(0xA7, &status))
        return 0;
    if (!reg(0x08, &mode))
        return 0;

    vraw = (uint16_t)((v_h << 8) | v_l);
    iraw = (uint16_t)((cur_h << 8) | cur_l);
    n.ok = 1;
    n.mv = (uint16_t)((vraw * 5u) / 16u);
    n.v_stock = (uint16_t)((7u * vraw) >> 5);
    n.soc100 = 0;
    if (soc_i <= 100u) {
        unsigned soc = soc_i * 100u + (20u * soc_f) / 51u;
        if (soc > 10000u)
            soc = 10000u;
        n.soc100 = (uint16_t)soc;
    }
    n.temp_c = (int)(temp / 2u) - 40;
    n.i_raw = iraw;
    if (cur_h & 0x80u) {
        n.charging = 0;
        mag = (uint16_t)(iraw & 0x7FFFu);
        if (mag > 1u)
            mag--;
        n.i_stock = (int)((131u * (0x8000u - mag)) >> 13);
    } else {
        n.charging = 1;
        n.i_stock = (int)((131u * iraw) >> 13);
    }
    n.cycles = (uint16_t)((cyc_h << 8) | cyc_l);
    n.status = status;
    n.raw[0] = 0;
    (void)reg(0x00, &n.raw[0]);
    n.raw[1] = soc_i;
    n.raw[2] = soc_f;
    n.raw[3] = temp;
    n.raw[4] = mode;
    n.raw[5] = cur_h;
    n.raw[6] = cur_l;
    n.raw[7] = status;
    *out = n;
    return 1;
}

static void restore_pd(void)
{
    /* Stock output word: PD10 high, PD11 low, both push-pull. */
    REG32(GPIOD_BASE + GPIO_BOP) = 1u << 10;
    REG32(GPIOD_BASE + GPIO_BC) = 1u << 11;
    uint32_t ctl = REG32(GPIOD_BASE + GPIO_CTL1);
    ctl = (ctl & ~(0xFFu << 8)) | (0x33u << 8);
    REG32(GPIOD_BASE + GPIO_CTL1) = ctl;
}

/* The CW2217 comes up asleep: it still answers and still reports a cell
   voltage, but the state of charge stays 0 and the temperature register
   reads 0xFE. The stock firmware wakes it at 0x0801AE3C by writing the
   mode register 0x08 with 0x30, waiting, then writing 0x00. This is
   that same sequence, and it is the only register this port writes. */
static void gauge_wake(void)
{
    gbus_write(ADDR_W, 0x08, 0x30);
    plat_delay_ms(20);
    gbus_write(ADDR_W, 0x08, 0x00);
    plat_delay_ms(10);
}

void gauge_init(void)
{
    restore_pd();
    line_release(SCL_PIN, SCL_MASK);
    line_release(SDA_PIN, SDA_MASK);
    i2c_recover();
    gauge_wake();
    next_ms = 0;
}

void gauge_poll(void)
{
    struct gauge_reading n;
    if ((int32_t)(plat_ms() - next_ms) < 0)
        return;
    next_ms = plat_ms() + 1000u;
    if (gauge_sample(&n)) {
        /* Mode register back off normal: wake it, at most once a minute. */
        if (n.raw[4] != 0x00u && (int32_t)(plat_ms() - wake_ms) >= 0) {
            wake_ms = plat_ms() + 60000u;
            gauge_wake();
        }
        live = n;
        if (announced != 1u) {
            announced = 1u;
            dbg_str("soc ");
            dbg_dec(n.soc100);
            dbg_str(" mV ");
            dbg_dec(n.mv);
            dbg_str(" st ");
            dbg_dec(n.status);
            dbg_nl();
        }
    } else if (announced == 0u) {
        announced = 2u;
        dbg_str("gauge nack");
        dbg_nl();
    }
}

const struct gauge_reading *gauge_now(void) { return &live; }
