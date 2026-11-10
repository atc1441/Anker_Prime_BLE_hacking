#include "gd32.h"
#include "platform.h"

static volatile uint32_t ms_now;

uint32_t plat_ms(void) { return ms_now; }

void SysTick_Handler(void)
{
    ms_now++;
    wdt_kick();
    buttons_sample();
    /* Parse during the LCD draw. Transmit only after the line is quiet,
       so this does not answer inside the TLSR's send routine. */
    ble_poll();
    snd_tick();
    dbg_tick(ms_now, plat_buttons());
}

void systick_init(void)
{
    SYST_CSR = 0;
    SYST_RVR = CPU_HZ / 1000u - 1u;
    SYST_CVR = 0;
    SCB_SHPR3 = (SCB_SHPR3 & 0x00FFFFFFu) | (0xF0u << 24);
    SYST_CSR = 7u;
}
