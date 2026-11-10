#include <stdint.h>

#define DBG_MAGIC 0x564C4F47u
#define DBG_SIZE 1024u

struct dbg_block {
    volatile uint32_t magic;
    volatile uint32_t size;
    volatile uint32_t wr;
    volatile uint32_t stage;
    volatile uint32_t ms;
    volatile uint32_t frames;
    volatile uint32_t buttons;
    volatile uint32_t fps100;
    volatile uint32_t gamestate;
    volatile uint32_t episode_map;
    volatile uint32_t nor_cyc, nor_calls, nor_bytes;
    volatile uint32_t pump_cyc, pump_px;
    volatile uint8_t buf[DBG_SIZE];
};

__attribute__((used)) struct dbg_block dbg;

/* The arm-none-eabi build uses short enums, so gamestate is one byte.
   A 32-bit load would copy oldgamestate and the following BSS bytes. */
extern unsigned char gamestate;
extern int gameepisode;
extern int gamemap;

void dbg_init(void)
{
    dbg.size = DBG_SIZE;
    dbg.wr = 0;
    dbg.stage = 0;
    dbg.magic = DBG_MAGIC;
}

void dbg_putc(char c)
{
    uint32_t w = dbg.wr;
    dbg.buf[w % DBG_SIZE] = (uint8_t)c;
    dbg.wr = w + 1u;
}

void dbg_str(const char *s)
{
    while (*s)
        dbg_putc(*s++);
}

void dbg_hex(uint32_t v, unsigned digits)
{
    static const char hx[] = "0123456789ABCDEF";
    unsigned i;
    for (i = digits; i; i--)
        dbg_putc(hx[(v >> ((i - 1u) * 4u)) & 0xFu]);
}

void dbg_dec(uint32_t v)
{
    char b[11];
    int n = 0;
    if (!v) {
        dbg_putc('0');
        return;
    }
    while (v && n < 10) {
        b[n++] = (char)('0' + v % 10u);
        v /= 10u;
    }
    while (n)
        dbg_putc(b[--n]);
}

void dbg_nl(void)
{
    dbg_putc('\r');
    dbg_putc('\n');
}

void dbg_mark(uint32_t stage) { dbg.stage = stage; }

void dbg_tick(uint32_t ms, uint32_t buttons)
{
    static uint32_t last_ms, last_frames;
    dbg.ms = ms;
    dbg.buttons = buttons;
    dbg.gamestate = (uint32_t)gamestate;
    dbg.episode_map = ((uint32_t)gameepisode << 16) | (uint32_t)gamemap;
    if (ms - last_ms >= 1000u) {
        dbg.fps100 = (dbg.frames - last_frames) * 100000u / (ms - last_ms);
        last_ms = ms;
        last_frames = dbg.frames;
    }
}

void dbg_frame(void) { dbg.frames++; }

void dbg_nor(uint32_t cyc, uint32_t bytes)
{
    (void)bytes;
    dbg.nor_cyc += cyc;
    dbg.nor_calls++;
}

void dbg_pump(uint32_t cyc, uint32_t px)
{
    dbg.pump_cyc += cyc;
    dbg.pump_px += px;
}
