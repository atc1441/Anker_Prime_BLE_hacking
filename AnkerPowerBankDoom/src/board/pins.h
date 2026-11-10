/* Board pins of the Anker Prime GD32F303VET6.
   Taken from ref/backup/gd32_internal_flash.bin and a read-only SWD halt on
   J-Link 601000179. See notes/button-gpio.md. Do not copy the PY32 pinout. */

#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include <stdint.h>

#define GPIOA_BASE 0x40010800u
#define GPIOB_BASE 0x40010C00u
#define GPIOC_BASE 0x40011000u
#define GPIOD_BASE 0x40011400u
#define GPIOE_BASE 0x40011800u

#define GPIO_CTL0  0x00u
#define GPIO_CTL1  0x04u
#define GPIO_ISTAT 0x08u
#define GPIO_OCTL  0x0Cu
#define GPIO_BOP   0x10u
#define GPIO_BC    0x14u

/* The one physical button. Stock menu_button_pd0 at 0x0800F6E8 reads PD0
   active low and cycles ui_page. PD3 is the C2 charge port. PC15 is the
   USB charge cable. */
#define BTN_PORT GPIOD_BASE
#define BTN_MASK 0x0001u /* PD0, input pull-up */
#define BTN_ACTIVE_LOW 1

/* Stock idle sets this through GPIOD BOP when it leaves 0x0801EE9C.
   Hold it high from the first instructions so a reset does not drop the bank. */
#define HOLD_PORT GPIOD_BASE
#define HOLD_MASK 0x8000u /* PD15 */

/* LCD is SPI0. CS is PC5, D/C is PA6. The backlight is TIMER0 channel 0
   on PA8. PC4 is the panel reset: stock holds it high, then pulses it
   low before the first SLPOUT. */
#define LCD_SPI_BASE 0x40013000u
/* Stock SPI0 init at 0x08025188 ORs mode 3, PCLK/4, master, soft NSS, 8-bit.
   Bit 6 is SPIEN, set once the other bits are written. */
#define SPI_CTL_LCD 0x0000034Fu
#define LCD_SCK_PORT GPIOA_BASE
#define LCD_SCK_PIN  5u
#define LCD_MOSI_PORT GPIOA_BASE
#define LCD_MOSI_PIN  7u
#define LCD_DC_PORT  GPIOA_BASE
#define LCD_DC_MASK  0x0040u /* PA6 */
#define LCD_CS_PORT  GPIOC_BASE
#define LCD_CS_MASK  0x0020u /* PC5, active low */
#define LCD_RST_MASK 0x0010u /* PC4, active low */

/* External NOR is SPI1, a separate bus from the panel. CS is PC7. */
#define NOR_SPI_BASE 0x40003800u
/* Stock SPI1 CTL0 was mode 0, PCLK/4 (0x034C). Doom reads at PCLK/2,
   30 MHz. A GD25Q64 normal read (0x03) is in spec at that clock. */
#define SPI_CTL_NOR 0x00000344u
#define NOR_CS_PORT  GPIOC_BASE
#define NOR_CS_MASK  0x0080u /* PC7, active low */
#define NOR_JEDEC_GD25Q64 0xC84017u

/* Captured while stock was running, after the button read. Restore these
   before any pin is retasked. ISTAT is not a configuration word. */
#define STOCK_GPIOA_CTL0 0xB3B00000u
#define STOCK_GPIOA_CTL1 0x888448BBu
#define STOCK_GPIOA_OCTL 0x0000A400u
#define STOCK_GPIOB_CTL0 0x4348440Bu
#define STOCK_GPIOB_CTL1 0xB4B47777u
#define STOCK_GPIOB_OCTL 0x00000F10u
#define STOCK_GPIOC_CTL0 0x34338434u
#define STOCK_GPIOC_CTL1 0x848B7743u
#define STOCK_GPIOC_OCTL 0x0000ADB8u
#define STOCK_GPIOD_CTL0 0x44488448u
#define STOCK_GPIOD_CTL1 0x3843334Bu
#define STOCK_GPIOD_OCTL 0x00008419u
/* GPIOE read back from the running stock firmware with a phone on USB-C
   (SWD halt, J-Link 601000179). Stock drives PE0, PE1, PE3, PE6, PE7 and
   PE15 as push-pull outputs and holds them low; PE9 is high. This port
   was missing from the capture, so those lines floated under Doom. */
#define STOCK_GPIOE_CTL0 0x33843433u
#define STOCK_GPIOE_CTL1 0x34443334u
#define STOCK_GPIOE_OCTL 0x00000E20u

/* RCU while stock idle. PLL is HSI/2 times 20, SYSCLK about 80 MHz. */
#define STOCK_RCU_CTL  0x03037783u
#define STOCK_RCU_CFG0 0x080F840Au

static inline int board_button_down(uint32_t istat)
{
    uint32_t high = (istat & BTN_MASK) != 0u;
    return BTN_ACTIVE_LOW ? !high : high;
}

#endif
