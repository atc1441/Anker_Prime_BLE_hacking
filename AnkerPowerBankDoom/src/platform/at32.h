// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// AT32F415 registers and board pins used by the platform layer. Internal to platform/.

#pragma once

#include <stdint.h>

// The host model test (platform/tests/model.h) maps these to an emulated register file
#ifndef REG32
#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define REG8(a)  (*(volatile uint8_t *)(uintptr_t)(a))
#endif

// Clock and reset
#define CRM_CFG    REG32(0x40021004u)
#define CRM_AHBEN  REG32(0x40021014u)
#define CRM_APB2EN REG32(0x40021018u)
#define CRM_APB1EN REG32(0x4002101Cu)

// IOMUX: SWJTAG_MUX (REMAP bits 26:24) and SWJTAG_GMUX (REMAP7 bits 18:16), 010 keeps SWD
// and frees PA15, PB3 and PB4
#define IOMUX_REMAP  REG32(0x40010004u)
#define IOMUX_REMAP7 REG32(0x40010030u)

// GPIO (F1 style CFGLR/CFGHR)
#define GPIOA 0x40010800u
#define GPIOB 0x40010C00u
#define GPIOF 0x40011C00u
#define GPIO_IDT(p) REG32((p) + 0x08u)
#define GPIO_SCR(p) REG32((p) + 0x10u)
#define GPIO_CLR(p) REG32((p) + 0x14u)
#define PIN_OUT_PP 0x3u
#define PIN_AF_PP  0xBu
#define PIN_IN     0x4u
#define PIN_IN_PU  0x8u

// SPI
#define SPI1 0x40013000u
#define SPI2 0x40003800u
#define SPI_CTRL1(s) REG32((s) + 0x00u)
#define SPI_CTRL2(s) REG32((s) + 0x04u)
#define SPI_STS(s)   REG32((s) + 0x08u)
#define SPI_DT(s)    REG32((s) + 0x0Cu)
#define SPI_DT_ADDR(s) ((s) + 0x0Cu)
#define SPI_CPHA   (1u << 0)
#define SPI_CPOL   (1u << 1)
#define SPI_MSTEN  (1u << 2)
#define SPI_MDIV4  (1u << 3)  // MDIV 001: APB / 4 = 36 MHz instead of / 2 = 72 MHz
#define SPI_EN     (1u << 6)
#define SPI_SWCSIL (1u << 8)
#define SPI_SWCSEN (1u << 9)
#define SPI_FBN16  (1u << 11)
#define SPI_SLBTD  (1u << 14)
#define SPI_SLBEN  (1u << 15)
#define SPI_DMAREN (1u << 0)
#define SPI_DMATEN (1u << 1)
#define SPI_RDBF   (1u << 0)
#define SPI_TDBE   (1u << 1)
#define SPI_ROERR  (1u << 6)
#define SPI_BF     (1u << 7)

// DMA1 with the fixed request map: ch2 SPI1 RX, ch3 SPI1 TX, ch5 SPI2 TX
#define DMA1 0x40020000u
#define DMA_STS        REG32(DMA1 + 0x00u)
#define DMA_CLR        REG32(DMA1 + 0x04u)
#define DMA_CCTRL(ch)  REG32(DMA1 + 0x08u + 20u * ((ch) - 1u))
#define DMA_CDTCNT(ch) REG32(DMA1 + 0x0Cu + 20u * ((ch) - 1u))
#define DMA_CPADDR(ch) REG32(DMA1 + 0x10u + 20u * ((ch) - 1u))
#define DMA_CMADDR(ch) REG32(DMA1 + 0x14u + 20u * ((ch) - 1u))
#define DMA_SRC_SEL1   REG32(DMA1 + 0xA4u)
#define DMA_FLAGS(ch)  (0xFu << (4u * ((ch) - 1u)))
#define DMA_FDTF(ch)   (2u << (4u * ((ch) - 1u)))
#define DMA_ERRF(ch)   (8u << (4u * ((ch) - 1u)))
#define DMA_CHEN    (1u << 0)
#define DMA_FDTIEN  (1u << 1)
#define DMA_DTD     (1u << 4)
#define DMA_MINCM   (1u << 7)
#define DMA_P16     (1u << 8)
#define DMA_M16     (1u << 10)
#define DMA_CHPL(p) ((uint32_t)(p) << 12)

// TMR4
#define TMR4 0x40000800u
#define TMR_CTRL1(t) REG32((t) + 0x00u)
#define TMR_SWEVT(t) REG32((t) + 0x14u)
#define TMR_CM2(t)   REG32((t) + 0x1Cu)
#define TMR_CCTRL(t) REG32((t) + 0x20u)
#define TMR_DIV(t)   REG32((t) + 0x28u)
#define TMR_PR(t)    REG32((t) + 0x2Cu)
#define TMR_C3DT(t)  REG32((t) + 0x3Cu)

// Flash controller
#define FLASH_UNLOCK REG32(0x40022004u)
#define FLASH_STS    REG32(0x4002200Cu)
#define FLASH_CTRL   REG32(0x40022010u)
#define FLASH_ADDR   REG32(0x40022014u)

// Core
#define SYST_CSR   REG32(0xE000E010u)
#define SYST_RVR   REG32(0xE000E014u)
#define SYST_CVR   REG32(0xE000E018u)
#define NVIC_ISER0 REG32(0xE000E100u)
#define NVIC_ICER0 REG32(0xE000E180u)
#define NVIC_IPR(n) REG8(0xE000E400u + (n))
#define SCB_VTOR   REG32(0xE000ED08u)
#define SCB_AIRCR  REG32(0xE000ED0Cu)
#define SCB_SHPR3  REG32(0xE000ED20u)
#define SCB_CFSR   REG32(0xE000ED28u)
#define SCB_HFSR   REG32(0xE000ED2Cu)
#define SCB_MMFAR  REG32(0xE000ED34u)
#define SCB_BFAR   REG32(0xE000ED38u)
#define SCB_DEMCR  REG32(0xE000EDFCu)
#define DWT_CTRL   REG32(0xE0001000u)
#define DWT_CYCCNT REG32(0xE0001004u)

// IRQ numbers and the one shared preemption priority
#define IRQ_DMA1_CH2 12
#define IRQ_DMA1_CH5 15
#define IRQ_OTGFS    67
#define IRQ_PRIO     0x80u

// Board pins
#define PIN_LCD_BL   3   // PA3, high = backlight on
#define PIN_NOR_CS   4   // PA4
#define PIN_LCD_RST  8   // PA8
#define PIN_BUZZER   8   // PB8, TMR4 CH3
#define PIN_LCD_DC   12  // PB12
#define PIN_LCD_CS   14  // PB14
#define PIN_PWR_HOLD 6   // PF6, high keeps the board powered

static inline void pin_cfg(uint32_t port, unsigned pin, uint32_t cfg)
{
    uint32_t a = port + (pin < 8 ? 0x00u : 0x04u);
    unsigned sh = (pin & 7u) * 4u;
    REG32(a) = (REG32(a) & ~(0xFu << sh)) | (cfg << sh);
}

static inline void pin_high(uint32_t port, unsigned pin) { GPIO_SCR(port) = 1u << pin; }
static inline void pin_low(uint32_t port, unsigned pin) { GPIO_CLR(port) = 1u << pin; }

#ifdef PLAT_MODEL
uint32_t irq_save(void);
void irq_restore(uint32_t m);
static inline void irq_end(void) {}
#else
static inline uint32_t irq_save(void)
{
    uint32_t m;
    __asm volatile("mrs %0, primask\ncpsid i" : "=r"(m)::"memory");
    return m;
}

static inline void irq_restore(uint32_t m) { __asm volatile("msr primask, %0" ::"r"(m) : "memory"); }

// The last statement of an interrupt handler, for ARM erratum 838869 of the Cortex-M4 r0p1
static inline void irq_end(void) { __asm volatile("dsb" ::: "memory"); }
#endif

// SPI clocks: NOR_SPI_SLOW=1 (the default) runs SPI1 (NOR) at 36 MHz, 0 at 72 MHz.
// LCD_SPI_SLOW=1 runs SPI2 (LCD) at 36 MHz instead of 72 MHz.
#ifndef NOR_SPI_SLOW
#define NOR_SPI_SLOW 1
#endif
#ifndef LCD_SPI_SLOW
#define LCD_SPI_SLOW 0
#endif

// Internal hooks between platform files
void buttons_init(void);
void buttons_sample(void);
void snd_init(void);
void snd_tick(void);
void systick_init(void);
void spin_cycles(uint32_t n);
void lcd_window(int x0, int y0, int x1, int y1);  // CASET/RASET/RAMWR, leaves the data phase open
void lcd_data_end(void);                          // waits for SPI2 to drain, raises CS
void pump_close(void);                            // closes an open pump window (no-op if none)
void pump_reset(void);                            // forgets pump state, DMA already stopped
extern uint8_t lcd_ready;                         // set at the end of lcd_init
// Latch low, then waits until POWER has been released for 50 ms and resets (USB power)
__attribute__((noreturn)) void power_cut(void);
