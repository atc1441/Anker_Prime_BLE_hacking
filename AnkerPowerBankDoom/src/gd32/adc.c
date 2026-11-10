/* ADC0 with DMA0, the way the stock firmware runs it.

   Register values read back from the running stock firmware over SWD
   while a phone was charging (see notes/stock-power-telemetry.md):

     ADC0 CTL0  0x00000100   scan mode
     ADC0 CTL1  0x001E0101   ADC on, DMA, external trigger SWRSTART
     ADC0 SAMPT1 0x38007FFF  sample times for channels 0..9
     ADC0 RSQ0  0x00500000   six conversions
     ADC0 RSQ2  0x12120043   sequence SQ1..SQ6 = 3, 2, 0, 4, 1, 9
     DMA0 CH0   CTL 0x2AA1, CNT 6, PADDR ADC0_RDATA, MADDR a 6-word buffer

   Channels to pins on this board, from the analog nibbles in the stock
   GPIO words (PA0..PA4 and PB1 are analog):

     rank 0  channel 3  PA3   not used by the stock port struct
     rank 1  channel 2  PA2   USB-C1 current
     rank 2  channel 0  PA0   USB-C1 voltage
     rank 3  channel 4  PA4   USB-C2 current
     rank 4  channel 1  PA1   USB-C2 voltage
     rank 5  channel 9  PB1   fifth value, meaning still open

   Stock averages ten scans and throws away the highest and the lowest
   sample: (sum - max - min) >> 3. Same here.

   The voltage scale is stock's own: (raw - zero) * 30 / 819 volts, with
   the zero taken while the port is dark. The current scale is not known
   yet, so the raw count is what the pages show. */

#include "adc.h"
#include "gd32.h"
#include "platform.h"

#define ADC0_BASE 0x40012400u
#define ADC_STAT (ADC0_BASE + 0x00u)
#define ADC_CTL0 (ADC0_BASE + 0x04u)
#define ADC_CTL1 (ADC0_BASE + 0x08u)
#define ADC_SAMPT0 (ADC0_BASE + 0x0Cu)
#define ADC_SAMPT1 (ADC0_BASE + 0x10u)
#define ADC_WDHT (ADC0_BASE + 0x24u)
#define ADC_WDLT (ADC0_BASE + 0x28u)
#define ADC_RSQ0 (ADC0_BASE + 0x2Cu)
#define ADC_RSQ1 (ADC0_BASE + 0x30u)
#define ADC_RSQ2 (ADC0_BASE + 0x34u)
#define ADC_RDATA (ADC0_BASE + 0x4Cu)

#define DMA0_BASE 0x40020000u
#define DMA_INTC (DMA0_BASE + 0x04u)
#define DMA_CH0CTL (DMA0_BASE + 0x08u)
#define DMA_CH0CNT (DMA0_BASE + 0x0Cu)
#define DMA_CH0PADDR (DMA0_BASE + 0x10u)
#define DMA_CH0MADDR (DMA0_BASE + 0x14u)

#define RANKS 6u
#define RUNS 10u

static volatile uint32_t adc_raw[RANKS];
static uint32_t acc[RANKS];
static uint16_t hi[RANKS], lo[RANKS];
static uint8_t runs;
static uint8_t armed;
static struct adc_reading adc_live;
/* The zero point both stock and this port use: the stock firmware keeps
   it at 0x20000264+0x44 and +0x46 and it reads 1638 on this board. It is
   NOT measured at start-up here - the port often already carries voltage
   by then, and calibrating against that reads the voltage away, which is
   exactly what made the pages show nothing. */
#define ADC_ZERO 1638u
#define ZERO_LO 1500u
#define ZERO_HI 1800u
/* Seeded with what the two voltage channels read on this board with
   their port dark. The tracker below only corrects downwards, so a seed
   that is slightly high would read a port as lower than it is until it
   goes dark once; these are the measured values. */
static uint16_t zero_v[2] = {1648u, 1652u};

/* A dark port sits at the channel's own zero, and that is the lowest it
   ever reads, so the smallest plausible sample is kept as the zero. The
   stock constant 1638 is the seed, which is what a port that already
   carries voltage at start-up falls back to. */
/* The current channels have their own idle level: about 870 counts on
   C1 and about 17 on C2. The lowest sample seen is that level.

   The scale comes from the stock firmware's own limit arithmetic at
   0x08010ED6, where a value becomes counts as value * 449 / 43. Reading
   that back the other way puts the count in whole amps, not tenths:
   checked against a USB meter on the port, 11 W at 11.9 V means 0.92 A
   while the channel stood about 13 counts above its idle level. So
   tenths of an amp are counts * 430 / 449.

   This is one calibration point against one meter, and the idle level
   it is measured from is itself tracked, so treat the amp and watt
   columns as good to about ten percent, not better. */
/* Seeded with the idle levels measured on this board while the stock
   firmware ran with both ports dark, because a port can already be
   loaded when this firmware starts and a plain running minimum would
   then take the loaded value for the idle one. The minimum still
   corrects downwards if the real idle turns out lower. */
static uint16_t zero_i[2] = {870u, 17u};

static void izero_track(unsigned i, uint16_t raw)
{
    if (raw < zero_i[i])
        zero_i[i] = raw;
}

static uint16_t amps10(uint16_t raw, uint16_t zero)
{
    uint32_t d = raw > zero ? (uint32_t)(raw - zero) : 0u;
    return (uint16_t)((d * 430u) / 449u);
}

static void zero_track(unsigned i, uint16_t raw)
{
    if (raw < ZERO_LO || raw > ZERO_HI)
        return;      /* out of the window: a live port, or nonsense */
    if (raw < zero_v[i])
        zero_v[i] = raw;
}
static uint32_t next_ms;

static void start_scan(void)
{
    /* SWRSTART, the trigger the stock CTL1 selects. */
    REG32(ADC_CTL1) |= 1u << 22;
}

void adc_init(void)
{
    unsigned i;

    for (i = 0; i < RANKS; i++) {
        acc[i] = 0;
        hi[i] = 0;
        lo[i] = 0xFFFFu;
    }
    runs = 0;
    adc_live.ok = 0;

    REG32(DMA_CH0CTL) = 0;
    REG32(DMA_INTC) = 0x0Fu;
    REG32(DMA_CH0PADDR) = ADC_RDATA;
    REG32(DMA_CH0MADDR) = (uint32_t)(uintptr_t)adc_raw;
    REG32(DMA_CH0CNT) = RANKS;
    /* Circular, 32 bit both sides, memory increment, high priority. */
    REG32(DMA_CH0CTL) = 0x2AA1u;

    REG32(ADC_SAMPT0) = 0x00000000u;
    REG32(ADC_SAMPT1) = 0x38007FFFu;
    REG32(ADC_RSQ0) = 0x00500000u;
    REG32(ADC_RSQ1) = 0x00000000u;
    REG32(ADC_RSQ2) = 0x12120043u;
    REG32(ADC_WDHT) = 0x00000FFFu;
    REG32(ADC_WDLT) = 0x00000000u;
    REG32(ADC_CTL0) = 0x00000100u;

    REG32(ADC_CTL1) = 1u; /* power the converter and let it settle */
    /* A plain spin, not plat_delay_ms: this runs before the tick does. */
    for (i = 0; i < 200000u; i++)
        __asm volatile("nop");
    REG32(ADC_CTL1) |= 1u << 3; /* reset calibration */
    while (REG32(ADC_CTL1) & (1u << 3)) {
    }
    REG32(ADC_CTL1) |= 1u << 2; /* calibrate */
    while (REG32(ADC_CTL1) & (1u << 2)) {
    }
    REG32(ADC_CTL1) = 0x001E0101u;
    armed = 1;
    start_scan();
    next_ms = 0;
}

static int volts10(uint16_t raw_v, uint16_t zero)
{
    int d = (int)raw_v - (int)zero;
    if (d < 0)
        d = 0;
    /* Stock uses (raw - zero) * 30 / 819 for whole volts. Ten times that
       keeps one decimal without leaving integer arithmetic. */
    return (d * 300) / 819;
}

void adc_poll(void)
{
    unsigned i;
    uint16_t avg[RANKS];

    if (!armed)
        return;
    if ((int32_t)(plat_ms() - next_ms) < 0)
        return;
    next_ms = plat_ms() + 10u;

    for (i = 0; i < RANKS; i++) {
        uint16_t v = (uint16_t)(adc_raw[i] & 0xFFFFu);
        acc[i] += v;
        if (v > hi[i])
            hi[i] = v;
        if (v < lo[i])
            lo[i] = v;
    }
    start_scan();
    if (++runs < RUNS)
        return;
    runs = 0;
    for (i = 0; i < RANKS; i++) {
        uint32_t s = acc[i];
        s -= hi[i];
        s -= lo[i];
        avg[i] = (uint16_t)(s >> 3);
        acc[i] = 0;
        hi[i] = 0;
        lo[i] = 0xFFFFu;
    }

    adc_live.rank0 = avg[0];
    adc_live.c1_i_raw = avg[1];
    adc_live.c1_v_raw = avg[2];
    adc_live.c2_i_raw = avg[3];
    adc_live.c2_v_raw = avg[4];
    adc_live.rank5 = avg[5];
    zero_track(0, adc_live.c1_v_raw);
    zero_track(1, adc_live.c2_v_raw);
    izero_track(0, adc_live.c1_i_raw);
    izero_track(1, adc_live.c2_i_raw);
    adc_live.c1_zero = zero_v[0];
    adc_live.c2_zero = zero_v[1];
    adc_live.c1_dv = (uint16_t)volts10(adc_live.c1_v_raw, zero_v[0]);
    adc_live.c2_dv = (uint16_t)volts10(adc_live.c2_v_raw, zero_v[1]);
    adc_live.c1_da = amps10(adc_live.c1_i_raw, zero_i[0]);
    adc_live.c2_da = amps10(adc_live.c2_i_raw, zero_i[1]);
    adc_live.c1_dw = (uint16_t)(((uint32_t)adc_live.c1_dv * adc_live.c1_da) / 10u);
    adc_live.c2_dw = (uint16_t)(((uint32_t)adc_live.c2_dv * adc_live.c2_da) / 10u);
    adc_live.ok = 1;
}

const struct adc_reading *adc_now(void) { return &adc_live; }

/* One scan, polled, without needing the millisecond tick. Used while the
   board is still coming up to see at which step a rail goes away. */
uint16_t adc_sample_rank(unsigned rank)
{
    uint32_t t;
    if (rank >= RANKS)
        return 0;
    start_scan();
    for (t = 0; t < 200000u; t++)
        __asm volatile("nop");
    return (uint16_t)(adc_raw[rank] & 0xFFFFu);
}

/* Boot trace: rank 1 (PA2) after each step of the start-up. */
uint16_t adc_boot_marks[24];
static unsigned mark_n;

void adc_mark(void)
{
    if (mark_n < sizeof adc_boot_marks / sizeof adc_boot_marks[0])
        adc_boot_marks[mark_n++] = adc_sample_rank(1);
}
