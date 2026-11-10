#include "gd32.h"

void spi_init(uint32_t base, uint32_t ctl)
{
    /* Program the frame while the peripheral is off, then enable it.
       The LCD and the NOR do not use the same clock phase. */
    REG32(base + 0x00u) = ctl & ~0x40u;
    REG32(base + 0x00u) = ctl | 0x40u;
}

static int wait_bit(uint32_t stat, uint32_t mask, uint32_t want)
{
    for (uint32_t n = 0; n < 200000u; n++) {
        if ((REG32(stat) & mask) == want)
            return 1;
    }
    return 0;
}

uint8_t spi_xfer(uint32_t base, uint8_t v)
{
    if (!wait_bit(base + 0x08u, 2u, 2u))
        return 0xEEu;
    REG32(base + 0x0Cu) = v;
    if (!wait_bit(base + 0x08u, 1u, 1u))
        return 0xEFu;
    return (uint8_t)REG32(base + 0x0Cu);
}

void spi_idle(uint32_t base)
{
    wait_bit(base + 0x08u, 2u, 2u);
    /* TRANS (busy) is bit 7. Wait until it drops. */
    for (uint32_t n = 0; n < 200000u; n++) {
        if ((REG32(base + 0x08u) & 0x80u) == 0u)
            return;
    }
}
