/* GD25Q64 on SPI1, chip select PC7. Reads are polled and finish before
   they return, which is the NOR_POLLED shape of the platform API. */

#include <stddef.h>

#include "gd32.h"
#include "nor.h"
#include "platform.h"

volatile struct nor_stats nor_stats;

static uint32_t jedec_id;
static int ticket_n;

static void cs_hi(void) { REG32(NOR_CS_PORT + GPIO_BOP) = NOR_CS_MASK; }
static void cs_lo(void) { REG32(NOR_CS_PORT + GPIO_BC) = NOR_CS_MASK; }

static uint8_t nor_tx(uint8_t v)
{
    uint32_t n = 20000u;
    while ((REG32(NOR_SPI_BASE + 0x08u) & 2u) == 0u && --n) {}
    if (!n)
        return 0xEEu;
    REG32(NOR_SPI_BASE + 0x0Cu) = v;
    n = 20000u;
    while ((REG32(NOR_SPI_BASE + 0x08u) & 1u) == 0u && --n) {}
    if (!n)
        return 0xEFu;
    return (uint8_t)REG32(NOR_SPI_BASE + 0x0Cu);
}

static void begin(void)
{
    cs_hi();
    spi_idle(NOR_SPI_BASE);
    cs_lo();
}

static void end(void)
{
    spi_idle(NOR_SPI_BASE);
    cs_hi();
}

static int wait_ready(void)
{
    uint32_t t0 = plat_ms();
    for (;;) {
        uint8_t st;
        begin();
        nor_tx(0x05u);
        st = nor_tx(0xFFu);
        end();
        if ((st & 1u) == 0u)
            return 0;
        if ((plat_ms() - t0) > 4000u)
            return -1;
    }
}

static void wren(void)
{
    begin();
    nor_tx(0x06u);
    end();
}

static void read_raw(uint32_t phys, uint8_t *dst, uint32_t n)
{
    uint32_t t0 = DWT_CYCCNT;
    begin();
    nor_tx(0x03u);
    nor_tx((uint8_t)(phys >> 16));
    nor_tx((uint8_t)(phys >> 8));
    nor_tx((uint8_t)phys);
    while (n--)
        *dst++ = nor_tx(0xA5u);
    end();
    dbg_nor(DWT_CYCCNT - t0, 0);
}

void nor_read(uint32_t phys, void *dst, uint32_t n)
{
    if (!n)
        return;
    read_raw(phys, dst, n);
    nor_stats.sync++;
    nor_stats.bytes += n;
}

int nor_read_async(uint32_t phys, void *dst, uint32_t n, int prio)
{
    (void)prio;
    nor_read(phys, dst, n);
    nor_stats.async++;
    nor_stats.async_chunks++;
    return ++ticket_n;
}

bool nor_done(int ticket)
{
    (void)ticket;
    return true;
}

void nor_wait(int ticket) { (void)ticket; }
void nor_wait_all(void) {}

int nor_col_start(uint32_t phys, void *dst, uint32_t n)
{
    if (!n || n > 256u)
        return -1;
    nor_read(phys, dst, n);
    return 0;
}

void nor_col_wait(void) {}

uint8_t nor_status(uint8_t cmd)
{
    uint8_t st;
    begin();
    nor_tx(cmd);
    st = nor_tx(0xFFu);
    end();
    return st;
}

int nor_erase(uint32_t phys, unsigned cmd)
{
    if (wait_ready())
        return -1;
    wren();
    begin();
    nor_tx((uint8_t)cmd);
    nor_tx((uint8_t)(phys >> 16));
    nor_tx((uint8_t)(phys >> 8));
    nor_tx((uint8_t)phys);
    end();
    return wait_ready();
}

int nor_program(uint32_t phys, const void *src, uint32_t n)
{
    const uint8_t *p = src;
    while (n) {
        uint32_t page = 256u - (phys & 255u);
        uint32_t chunk = n < page ? n : page;
        uint32_t i;
        if (wait_ready())
            return -1;
        wren();
        begin();
        nor_tx(0x02u);
        nor_tx((uint8_t)(phys >> 16));
        nor_tx((uint8_t)(phys >> 8));
        nor_tx((uint8_t)phys);
        for (i = 0; i < chunk; i++)
            nor_tx(p[i]);
        end();
        phys += chunk;
        p += chunk;
        n -= chunk;
    }
    return wait_ready();
}

void nor_recover(void)
{
    cs_hi();
    spi_init(NOR_SPI_BASE, SPI_CTL_NOR);
}

int nor_read16(uint32_t phys, void *dst, uint32_t n)
{
    (void)phys;
    (void)dst;
    (void)n;
    return -1;
}

uint32_t nor_jedec(void) { return jedec_id; }

int nor_init(void)
{
    uint8_t a, b, c;
    spi_init(NOR_SPI_BASE, SPI_CTL_NOR);
    cs_hi();
    begin();
    nor_tx(0xABu);
    end();
    plat_delay_ms(1);
    begin();
    nor_tx(0x9Fu);
    a = nor_tx(0x00u);
    b = nor_tx(0x00u);
    c = nor_tx(0x00u);
    end();
    jedec_id = ((uint32_t)a << 16) | ((uint32_t)b << 8) | c;
    if (jedec_id == 0u || jedec_id == 0xFFFFFFu)
        return -1;
    return 0;
}
