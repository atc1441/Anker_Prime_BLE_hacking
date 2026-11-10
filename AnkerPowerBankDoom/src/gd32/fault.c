#include <stdint.h>

#include "gd32.h"
#include "lcd.h"
#include "nor.h"
#include "platform.h"

static struct plat_fault cur;
static void (*hook)(void);

void plat_fault_get(struct plat_fault *f) { *f = cur; }
void plat_fault_hook(void (*fn)(void)) { hook = fn; }
void plat_fault_line(const char *label, uint32_t value)
{
    (void)label;
    (void)value;
}

static void hold(void)
{
    REG32(HOLD_PORT + GPIO_BOP) = HOLD_MASK;
    for (;;)
        wdt_kick();
}

__attribute__((naked)) void Fault_Handler(void)
{
    __asm volatile(
        "mov r0, lr\n"
        "mov r1, sp\n"
        "b fault_c\n");
}

__attribute__((used, noreturn)) void fault_c(uint32_t lr, uint32_t *sp)
{
    cur.exc = REG32(0xE000ED04u) & 0x1FFu;
    cur.lr = lr;
    cur.cfsr = REG32(0xE000ED28u);
    cur.hfsr = REG32(0xE000ED2Cu);
    cur.bfar = REG32(0xE000ED38u);
    cur.mmfar = REG32(0xE000ED34u);
    cur.msg = "";
    if ((lr & 4u) == 0u && (uintptr_t)sp >= 0x20000000u && (uintptr_t)sp < 0x20010000u)
        cur.pc = sp[6];
    else
        cur.pc = 0xFFFFFFFFu;
    dbg_str("fault");
    dbg_nl();
    nor_recover();
    lcd_recover();
    if (hook)
        hook();
    hold();
}

__attribute__((noreturn)) void plat_panic(const char *msg)
{
    cur.exc = 0;
    cur.pc = 0;
    cur.lr = 0;
    cur.cfsr = REG32(0xE000ED28u);
    cur.hfsr = REG32(0xE000ED2Cu);
    cur.bfar = 0;
    cur.mmfar = 0;
    cur.msg = msg ? msg : "";
    dbg_str("panic ");
    dbg_str(cur.msg);
    dbg_nl();
    __asm volatile("cpsid i");
    nor_recover();
    lcd_recover();
    if (hook)
        hook();
    hold();
}
