// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
// Copyright (C) 2026 VapeDoom port
//
// Register map of the Puya PY32F403xC as far as this port needs it, plus the
// pin assignment of the Aspire PIXO.
//
// The values of the clock tree and of the pin configuration were not derived
// from the reference manual but read back over SWD from a running firmware
// that is known to drive this board correctly (tools/dumpregs.py of the
// Vape_DOOM_ScreenShare tree). Where a register is written here as a whole
// word, that word is the measured one.

#ifndef PY32_HW_H
#define PY32_HW_H

#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))

// ---- Cortex-M4 core ----
#define SCB_ICSR   REG32(0xE000ED04u)
#define SCB_VTOR   REG32(0xE000ED08u)
#define SCB_AIRCR  REG32(0xE000ED0Cu)
#define SCB_SHPR3  REG32(0xE000ED20u)
#define SCB_SHCSR  REG32(0xE000ED24u)
#define SCB_CFSR   REG32(0xE000ED28u)
#define SCB_HFSR   REG32(0xE000ED2Cu)
#define SCB_MMFAR  REG32(0xE000ED34u)
#define SCB_BFAR   REG32(0xE000ED38u)
#define SCB_CPACR  REG32(0xE000ED88u)
#define SCB_DEMCR  REG32(0xE000EDFCu)

#define SYST_CSR   REG32(0xE000E010u)
#define SYST_RVR   REG32(0xE000E014u)
#define SYST_CVR   REG32(0xE000E018u)

#define DWT_CTRL   REG32(0xE0001000u)
#define DWT_CYCCNT REG32(0xE0001004u)

#define NVIC_ISER(n) REG32(0xE000E100u + 4u * (n))
#define NVIC_ICER(n) REG32(0xE000E180u + 4u * (n))
#define NVIC_IPR(n)  (*(volatile uint8_t *)(uintptr_t)(0xE000E400u + (n)))

// ---- Peripherals ----
#define RCC_BASE   0x40021000u
#define RCC_CR     REG32(RCC_BASE + 0x00u)
#define RCC_CFGR   REG32(RCC_BASE + 0x04u)
#define RCC_AHB1EN REG32(RCC_BASE + 0x14u)
#define RCC_APB2EN REG32(RCC_BASE + 0x18u)
#define RCC_APB1EN REG32(RCC_BASE + 0x1Cu)
#define RCC_CSR    REG32(RCC_BASE + 0x24u)
#define RCC_CFGR1  REG32(RCC_BASE + 0x28u)
#define RCC_AHB2EN REG32(RCC_BASE + 0x34u)
#define RCC_CFGR2  REG32(RCC_BASE + 0x38u)

#define FLASH_BASE 0x40022000u
#define FLASH_ACR  REG32(FLASH_BASE + 0x00u)
#define FLASH_KEYR REG32(FLASH_BASE + 0x08u)
#define FLASH_SR   REG32(FLASH_BASE + 0x10u)
#define FLASH_CR   REG32(FLASH_BASE + 0x14u)
#define FLASH_OPTR REG32(FLASH_BASE + 0x20u)

#define IWDG_BASE  0x40003000u
#define IWDG_KR    REG32(IWDG_BASE + 0x00u)
#define IWDG_PR    REG32(IWDG_BASE + 0x04u)
#define IWDG_RLR   REG32(IWDG_BASE + 0x08u)
#define IWDG_SR    REG32(IWDG_BASE + 0x0Cu)

#define SPI1_BASE  0x40013000u
#define SPI_CR1    REG32(SPI1_BASE + 0x00u)
#define SPI_CR2    REG32(SPI1_BASE + 0x04u)
#define SPI_SR     REG32(SPI1_BASE + 0x08u)
#define SPI_DR8    (*(volatile uint8_t *)(uintptr_t)(SPI1_BASE + 0x0Cu))
#define SPI_DR32   REG32(SPI1_BASE + 0x0Cu)

#define SPI_CR1_CPHA     (1u << 0)
#define SPI_CR1_CPOL     (1u << 1)
#define SPI_CR1_MSTR     (1u << 2)
#define SPI_CR1_BR_SHIFT 3
#define SPI_CR1_SPE      (1u << 6)
#define SPI_CR1_SSI      (1u << 8)
#define SPI_CR1_SSM      (1u << 9)
#define SPI_CR2_FRXTH    (1u << 12)
#define SPI_SR_RXNE      (1u << 0)
#define SPI_SR_TXE       (1u << 1)
#define SPI_SR_BSY       (1u << 7)
#define SPI_SR_FRLVL     (3u << 9)
#define SPI_SR_FTLVL     (3u << 11)

// GPIO sits on AHB2 at 0x48000000, not where the APB peripherals are.
#define GPIOA 0x48000000u
#define GPIOB 0x48000400u
#define GPIOC 0x48000800u
#define GPIOD 0x48000C00u
#define GPIOE 0x48001000u

#define GPIO_MODER(p)   REG32((p) + 0x00u)
#define GPIO_OTYPER(p)  REG32((p) + 0x04u)
#define GPIO_OSPEEDR(p) REG32((p) + 0x08u)
#define GPIO_PUPDR(p)   REG32((p) + 0x0Cu)
#define GPIO_IDR(p)     REG32((p) + 0x10u)
#define GPIO_ODR(p)     REG32((p) + 0x14u)
#define GPIO_BSRR(p)    REG32((p) + 0x18u)
#define GPIO_BRR(p)     REG32((p) + 0x28u)
#define GPIO_AFRL(p)    REG32((p) + 0x20u)
#define GPIO_AFRH(p)    REG32((p) + 0x24u)

static inline void pin_high(uint32_t port, unsigned pin) { GPIO_BSRR(port) = 1u << pin; }
static inline void pin_low(uint32_t port, unsigned pin) { GPIO_BRR(port) = 1u << pin; }
static inline uint32_t pin_read(uint32_t port, unsigned pin)
{
    return (GPIO_IDR(port) >> pin) & 1u;
}

// MODER field of one pin: 0 input, 1 output, 2 alternate, 3 analogue
static inline void pin_mode(uint32_t port, unsigned pin, unsigned m)
{
    GPIO_MODER(port) = (GPIO_MODER(port) & ~(3u << (2u * pin))) | (m << (2u * pin));
}

// ---- Board pins of the Aspire PIXO (Pinout.txt of the screen-share tree) ----
#define PIN_COIL     13  // PB13, load path. LOW is the safe state and the one
                         // the stock firmware holds, so nothing here ever
                         // drives it high.
#define PIN_EXTWDT    6  // PC6, external watchdog, needs edges
#define PIN_BL       14  // PE14, backlight
#define PIN_LCD_CS    6  // PB6
#define PIN_LCD_RST   7  // PB7
#define PIN_LCD_DC    5  // PD5
#define PIN_NOR_CS    6  // PD6
#define PIN_SCL       8  // PB8, bit-banged I2C to the touch controller
#define PIN_SDA       9  // PB9
#define PIN_TC_INT   15  // PB15, touch interrupt (measured: toggles with a touch)
#define PIN_TC_RST   12  // PE12, held low by the stock firmware

#define LCD_CS_LO()  pin_low(GPIOB, PIN_LCD_CS)
#define LCD_CS_HI()  pin_high(GPIOB, PIN_LCD_CS)
#define LCD_DC_CMD() pin_low(GPIOD, PIN_LCD_DC)
#define LCD_DC_DAT() pin_high(GPIOD, PIN_LCD_DC)
#define NOR_CS_LO()  pin_low(GPIOD, PIN_NOR_CS)
#define NOR_CS_HI()  pin_high(GPIOD, PIN_NOR_CS)

// ---- Panel geometry ----
// Native portrait: 173 columns with a 34 pixel offset, 323 rows. The screen
// coordinates of the engine (320 x 240) are mapped onto it in lcd.c.
#define PANEL_W 173u
#define PANEL_H 323u
#define PANEL_XOFF 34u

#define PLAT_IRQ_PRIO 0xC0u

void py32_clock_init(void);
void py32_pins_init(void);
void wdt_kick(void);
void spin_cycles(uint32_t n);

// Internal platform calls, the counterparts of the DMC-100 at32.h ones.
void systick_init(void);
void buttons_init(void);
void buttons_sample(void);
void input_legend(void);
void snd_init(void);
void snd_tick(void);
void pump_close(void);
void pump_reset(void);
void lcd_window(int x0, int y0, int x1, int y1);

// SPI1. The transmit path is inline because the pump writes two bytes per
// panel pixel and a call per byte would cost more than the transfer.
void py32_spi_init(void);
void py32_spi_idle(void);
void py32_spi_drain(void);

static inline void py32_spi_tx(uint8_t b)
{
    uint32_t guard = 100000u;
    while (!(SPI_SR & SPI_SR_TXE) && --guard) {
    }
    SPI_DR8 = b;
}

// Panel columns one screen row turns into: 0, 1 or 2. See lcd.c.
unsigned lcd_row_reps(int y);

// Bus arbiter. The panel and the flash share SPI1 and its data line, so a
// NOR access in the middle of a frame has to lift the panel's chip select
// and put it back. The panel keeps its address counter across that, and
// 0x3C (write memory continue) resumes the pixel stream where it stopped -
// 0x2C would start the window over.
int lcd_bus_release(void);
void lcd_bus_resume(int was_open);
void lcd_stream_arm(void);
void lcd_begin_raw(int x0, int y0, int x1, int y1);

// The write that clears an interrupt must land before the handler returns,
// otherwise it can be taken again.
static inline void irq_end(void) { __asm volatile("dsb" ::: "memory"); }

// ---- Debug log over SWD ----
// There is no serial port on this board and USB is the vape's own connector,
// so progress and panics go into a ring in RAM that the host reads with the
// debugger while the CPU runs. tools/vapelog.py prints it.
void dbg_init(void);
void dbg_putc(char c);
void dbg_str(const char *s);
void dbg_hex(uint32_t v, unsigned digits);
void dbg_dec(uint32_t v);
void dbg_nl(void);
void dbg_mark(uint32_t stage);
void dbg_tick(uint32_t ms, uint32_t buttons);
void dbg_frame(void);
void dbg_touch(int ok, int down);
void dbg_touch_pos(uint32_t rx, uint32_t ry, uint32_t sx, uint32_t sy);
void dbg_nor(uint32_t cyc, uint32_t bytes);
void dbg_pump(uint32_t cyc, uint32_t px);

#endif
