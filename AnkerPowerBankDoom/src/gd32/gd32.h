/* GD32F303 registers and the hooks the board files share.
   Pin numbers and the captured stock GPIO words live in board/pins.h. */

#ifndef GD32_H
#define GD32_H

#include <stdint.h>

#include "pins.h"

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))

#define RCU_BASE   0x40021000u
#define RCU_CTL    REG32(RCU_BASE + 0x00u)
#define RCU_CFG0   REG32(RCU_BASE + 0x04u)
#define RCU_AHBEN  REG32(RCU_BASE + 0x14u)
#define RCU_APB2EN REG32(RCU_BASE + 0x18u)
#define RCU_APB1EN REG32(RCU_BASE + 0x1Cu)
#define RCU_RSTSCK REG32(RCU_BASE + 0x24u)

#define FMC_WS REG32(0x40022000u)
#define DBG_CTL REG32(0xE0042004u)

#define SYST_CSR REG32(0xE000E010u)
#define SYST_RVR REG32(0xE000E014u)
#define SYST_CVR REG32(0xE000E018u)
#define SCB_SHPR3 REG32(0xE000ED20u)
#define SCB_VTOR REG32(0xE000ED08u)
#define DWT_CTRL REG32(0xE0001000u)
#define DWT_CYCCNT REG32(0xE0001004u)
#define DEMCR REG32(0xE000EDFCu)
#define FWDGT_CTL REG32(0x40003000u)
#define FWDGT_PSC REG32(0x40003004u)
#define FWDGT_RLD REG32(0x40003008u)
#define FWDGT_STAT REG32(0x4000300Cu)

#define CPU_HZ 120000000u

/* Screen columns 40..279 of the 320-wide engine land on the 240-wide panel. */
#define PANEL_W 240
#define PANEL_H 240
#define VIEW_X0 40
#define VIEW_X1 279

void spi_init(uint32_t base, uint32_t ctl);
uint8_t spi_xfer(uint32_t base, uint8_t v);
void spi_idle(uint32_t base);

void wdt_kick(void);
/* Puts every pin back to the state the stock firmware idles in. */
void board_gpio_restore(void);
/* The three timers and the two sense interrupt lines stock runs. */
void timers_init(void);
void snd_tick(void);
void buttons_init(void);
void buttons_sample(void);
void buttons_splash_begin(void);
int buttons_splash_pressed(void);
int buttons_splash_action(void);
void buttons_splash_end(void);

struct gauge_reading {
    int ok;
    uint16_t mv;
    uint16_t soc100;
    int temp_c;
    uint16_t i_raw;
    int charging;
    int i_stock;
    uint16_t cycles;
    uint8_t status;
    uint16_t v_stock;
    /* raw regs 0x00, 0x04, 0x05, 0x06, 0x08, 0x09, 0x0A, 0x0B */
    uint8_t raw[8];
};

void gauge_init(void);
void gauge_poll(void);
const struct gauge_reading *gauge_now(void);

void ble_init(void);
void ble_poll(void);
uint8_t ble_buttons(void);
uint8_t ble_pressed(void);
uint32_t ble_bytes(void);
uint16_t ble_last(void);
uint32_t ble_replies(void);

void dbg_init(void);
void dbg_putc(char c);
void dbg_str(const char *s);
void dbg_nl(void);
void dbg_hex(uint32_t v, unsigned digits);
void dbg_dec(uint32_t v);
void dbg_mark(uint32_t stage);
void dbg_tick(uint32_t ms, uint32_t buttons);
void dbg_frame(void);
void dbg_nor(uint32_t cyc, uint32_t bytes);
void dbg_pump(uint32_t cyc, uint32_t px);

void pump_close(void);
void pump_reset(void);
unsigned lcd_row_reps(int y);
void lcd_begin_raw(int x0, int y0, int x1, int y1);
unsigned lcd_write_column(int sx, int y0, int y1, const uint8_t *idx,
                          const uint8_t *rep, const uint16_t *lut, unsigned h);

#endif
