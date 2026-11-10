/* Reset for the GD32F303VET6. The first writes drive PD15 high. That is the
   line the stock idle loop sets through GPIOD BOP, and it has to stay high
   or the bank can drop its own power. */

#include <stdint.h>

#include "gd32.h"
#include "platform.h"

#define STACK_PAINT 0x5AC3A55Cu

extern uint32_t __stack_bottom[], __stack_top[];
extern uint32_t __data_start[], __data_end[], __data_load[];
extern uint32_t __bss_start[], __bss_end[];
extern uint32_t __dyn_start[];

void Reset_Handler(void);
void Fault_Handler(void);
void SysTick_Handler(void);
void USART2_IRQHandler(void);

__attribute__((naked, noreturn, used)) void Reset_Handler(void)
{
    __asm volatile(
        "cpsid i\n"
        "ldr r0, =0x40021018\n" /* RCU_APB2EN: clock GPIOD before the pin write */
        "ldr r1, [r0]\n"
        "orr r1, r1, #0x20\n"
        "str r1, [r0]\n"
        "ldr r0, =0x40011400\n" /* GPIOD */
        "ldr r1, [r0, #4]\n"
        "bic r1, r1, #0xF0000000\n"
        "orr r1, r1, #0x30000000\n" /* PD15 output, push-pull, 50 MHz */
        "str r1, [r0, #4]\n"
        "ldr r1, =0x8000\n"
        "str r1, [r0, #0x10]\n" /* BOP set PD15 */
        "ldr r0, =__stack_top\n"
        "mov sp, r0\n"
        "ldr r1, =__stack_bottom\n"
        "ldr r2, =0x5AC3A55C\n"
        "1: str r2, [r1], #4\n"
        "cmp r1, r0\n"
        "blo 1b\n"
        "b reset_c\n");
}

__attribute__((used, noreturn)) void reset_c(void)
{
    SYST_CSR = 0;
    for (unsigned i = 0; i < 4u; i++) {
        REG32(0xE000E180u + 4u * i) = 0xFFFFFFFFu;
        REG32(0xE000E280u + 4u * i) = 0xFFFFFFFFu;
    }
    SCB_VTOR = 0x08000000u;
    __asm volatile("dsb\nisb");
    for (uint32_t *s = __data_load, *d = __data_start; d < __data_end;)
        *d++ = *s++;
    for (uint32_t *p = __bss_start; p < __bss_end; p++)
        *p = 0;
    for (uint32_t *p = __dyn_start; p < (uint32_t *)(void *)__dyn_free; p++)
        *p = 0;
    __asm volatile("cpsie i");
    app_main();
    plat_power_off();
}

uint32_t plat_stack_used(void)
{
    const uint32_t *p = __stack_bottom;
    while (p < __stack_top && *p == STACK_PAINT)
        p++;
    return (uint32_t)((uintptr_t)__stack_top - (uintptr_t)p);
}

#define VEC_COUNT 96

__attribute__((section(".vectors"), used)) const uintptr_t vectors[VEC_COUNT] = {
    [0] = (uintptr_t)__stack_top,
    [1] = (uintptr_t)Reset_Handler,
    [2 ... 14] = (uintptr_t)Fault_Handler,
    [15] = (uintptr_t)SysTick_Handler,
    [16 ... VEC_COUNT - 1] = (uintptr_t)Fault_Handler,
    /* IRQ 39, the stock BLE link on PD8/PD9. */
    [55] = (uintptr_t)USART2_IRQHandler,
};
