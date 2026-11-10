#include "gd32.h"
#include "lcd.h"
#include "nor.h"
#include "adc.h"
#include "power.h"
#include "platform.h"

void systick_init(void);
void snd_init(void);

static int dwt_ok;

void board_gpio_restore(void)
{
    /* Output latch first, then the mode word, so a pin that stock holds
       high never dips low while it becomes an output. */
    static const struct {
        uint32_t base, ctl0, ctl1, octl;
    } port[] = {
        {GPIOA_BASE, STOCK_GPIOA_CTL0, STOCK_GPIOA_CTL1, STOCK_GPIOA_OCTL},
        {GPIOB_BASE, STOCK_GPIOB_CTL0, STOCK_GPIOB_CTL1, STOCK_GPIOB_OCTL},
        {GPIOC_BASE, STOCK_GPIOC_CTL0, STOCK_GPIOC_CTL1, STOCK_GPIOC_OCTL},
        {GPIOD_BASE, STOCK_GPIOD_CTL0, STOCK_GPIOD_CTL1, STOCK_GPIOD_OCTL},
        {GPIOE_BASE, STOCK_GPIOE_CTL0, STOCK_GPIOE_CTL1, STOCK_GPIOE_OCTL},
    };
    for (unsigned i = 0; i < 5u; i++) {
        REG32(port[i].base + GPIO_OCTL) = port[i].octl;
        REG32(port[i].base + GPIO_CTL0) = port[i].ctl0;
        REG32(port[i].base + GPIO_CTL1) = port[i].ctl1;
    }
    REG32(HOLD_PORT + GPIO_BOP) = HOLD_MASK;
}

static void clock_init(void)
{
    RCU_CTL |= 1u; /* IRC8M on */
    while ((RCU_CTL & 2u) == 0u)
        ;
    /* Stock CFG0 has PLLSEL set, so the PLL input is HXTAL, not IRC8M/2. */
    RCU_CTL |= 1u << 16;
    while ((RCU_CTL & (1u << 17)) == 0u)
        ;
    FMC_WS = 3u; /* covers the 90..120 MHz band; extra wait is safe below that */
    RCU_CFG0 = STOCK_RCU_CFG0 & ~3u;
    RCU_CTL |= 1u << 24; /* PLL on */
    while ((RCU_CTL & (1u << 25)) == 0u)
        ;
    RCU_CFG0 = (RCU_CFG0 & ~3u) | 2u;
    while (((RCU_CFG0 >> 2) & 3u) != 2u)
        ;

    RCU_AHBEN = 0x00000015u;
    RCU_APB2EN = 0x00005A7Du;
    RCU_APB1EN = 0x10144007u;
    /* Halted debug must not let the watchdog reset the core under the probe. */
    DBG_CTL |= 0x300u;
}

static void dwt_init(void)
{
    DEMCR |= 1u << 24;
    DWT_CYCCNT = 0;
    DWT_CTRL |= 1u;
    dwt_ok = (DWT_CTRL & 1u) != 0u;
}

static void wdt_init(void)
{
    /* The free watchdog counts on IRC40K. A system reset can leave that
       oscillator off, and the prescaler update then never finishes. */
    RCU_RSTSCK |= 1u;
    while ((RCU_RSTSCK & 2u) == 0u)
        ;
    /* About two seconds. SysTick reloads it every millisecond. */
    FWDGT_CTL = 0x5555u;
    while (FWDGT_STAT & 1u)
        ;
    FWDGT_PSC = 4u;
    while (FWDGT_STAT & 1u)
        ;
    FWDGT_RLD = 1250u;
    while (FWDGT_STAT & 2u)
        ;
    FWDGT_CTL = 0xAAAAu;
    FWDGT_CTL = 0xCCCCu;
}

void wdt_kick(void) { FWDGT_CTL = 0xAAAAu; }

uint32_t plat_cycles(void) { return dwt_ok ? DWT_CYCCNT : 0u; }

void plat_idle(void) { __asm volatile("wfi"); }

void plat_delay_ms(uint32_t ms)
{
    uint32_t t0 = plat_ms();
    while ((plat_ms() - t0) < ms)
        plat_idle();
}

__attribute__((noreturn)) void plat_power_off(void)
{
    __asm volatile("cpsid i");
    lcd_backlight(0);
    lcd_recover();
    REG32(HOLD_PORT + GPIO_BOP) = HOLD_MASK;
    for (;;) {
        wdt_kick();
        __asm volatile("wfi");
    }
}

void plat_init(void)
{
    clock_init();
    /* The converter rail is read on ADC rank 1, and power_ports_start()
       decides from it whether this is a cold start, so the converter has
       to be up before anything touches the power path. */
    adc_init();
    board_gpio_restore();
    dbg_init();
    dbg_str("doom gd32 title");
    dbg_nl();
    dbg_mark(1);
    dwt_init();
    systick_init();
    wdt_init();
    buttons_init();
    snd_init();
    if (nor_init())
        dbg_str("nor none");
    else {
        dbg_str("nor ");
        dbg_hex(nor_jedec(), 6);
    }
    dbg_nl();
    dbg_mark(2);
    ble_init();
    gauge_init();
    /* The USB ports stay dark until the controller is configured. */
    timers_init();
    power_ports_start();
    lcd_init(LCD_INIT_DEFAULT);
    dbg_str("lcd up");
    dbg_nl();
    dbg_mark(3);
}
