/* The three timers and the two interrupt lines the stock firmware runs.

   Register values read back from the running stock firmware over SWD
   while it was delivering power (.scratch/periph-stock.txt):

     TIMER1  PSC 0x78   CAR 0x3E8    DMAINTEN 1  CTL0 0x81
     TIMER2  PSC 0x77   CAR 0xDE     CHCTL1 0x60  CHCTL2 0x100  CTL0 0x81
     TIMER3  PSC 0x7D0  CAR 0x7530   DMAINTEN 1  CTL0 0x81
     AFIO EXTISS0 0x3000, EXTISS3 0x2000
     EXTI INTEN 0x8008, RTEN 0x8008, FTEN 0x8008

   TIMER2 drives PB0 as its channel 2 output, and its compare value is
   zero in stock as well, so the pin stays low either way. The update
   interrupts are armed in the timers but not in the NVIC, so no handler
   is needed here: this is about matching the state of the hardware, not
   about running stock's state machines.

   EXTI line 3 is PD3, the C2 port sense, and line 15 is PC15, the
   charge cable sense. Both edges, as stock has them. */

#include "gd32.h"

#define TIMER1_BASE 0x40000000u
#define TIMER2_BASE 0x40000400u
#define TIMER3_BASE 0x40000800u

#define TIM_CTL0 0x00u
#define TIM_DMAINTEN 0x0Cu
#define TIM_CHCTL1 0x1Cu
#define TIM_CHCTL2 0x20u
#define TIM_PSC 0x28u
#define TIM_CAR 0x2Cu

#define AFIO_BASE 0x40010000u
#define AFIO_EXTISS0 0x08u
#define AFIO_EXTISS3 0x14u

#define EXTI_BASE 0x40010400u
#define EXTI_INTEN 0x00u
#define EXTI_RTEN 0x08u
#define EXTI_FTEN 0x0Cu

void timers_init(void)
{
    REG32(TIMER1_BASE + TIM_PSC) = 0x78u;
    REG32(TIMER1_BASE + TIM_CAR) = 0x3E8u;
    REG32(TIMER1_BASE + TIM_DMAINTEN) = 1u;
    REG32(TIMER1_BASE + TIM_CTL0) = 0x81u;

    REG32(TIMER2_BASE + TIM_PSC) = 0x77u;
    REG32(TIMER2_BASE + TIM_CAR) = 0xDEu;
    REG32(TIMER2_BASE + TIM_CHCTL1) = 0x60u;
    REG32(TIMER2_BASE + TIM_CHCTL2) = 0x100u;
    REG32(TIMER2_BASE + TIM_CTL0) = 0x81u;

    REG32(TIMER3_BASE + TIM_PSC) = 0x7D0u;
    REG32(TIMER3_BASE + TIM_CAR) = 0x7530u;
    REG32(TIMER3_BASE + TIM_DMAINTEN) = 1u;
    REG32(TIMER3_BASE + TIM_CTL0) = 0x81u;

    REG32(AFIO_BASE + AFIO_EXTISS0) = 0x3000u;
    REG32(AFIO_BASE + AFIO_EXTISS3) = 0x2000u;
    REG32(EXTI_BASE + EXTI_RTEN) = 0x8008u;
    REG32(EXTI_BASE + EXTI_FTEN) = 0x8008u;
    REG32(EXTI_BASE + EXTI_INTEN) = 0x8008u;
}
