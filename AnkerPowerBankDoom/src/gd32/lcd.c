/* ST7789 on SPI0. CS is PC5, D/C is PA6, reset is PC4.
   The command list, the reset pulse and the pixel byte order are the
   stock function at 0x0801C3F4. Pixels go out high byte, then low byte. */

#include "gd32.h"
#include "lcd.h"
#include "platform.h"

static uint8_t profile;
static uint8_t flipped;
static uint8_t win_open;
static uint8_t col_open;
static int16_t lx0, ly0, lx1, ly1, cx, cy;

static void cs_hi(void) { REG32(LCD_CS_PORT + GPIO_BOP) = LCD_CS_MASK; }
static void cs_lo(void) { REG32(LCD_CS_PORT + GPIO_BC) = LCD_CS_MASK; }
static void dc_cmd(void) { REG32(LCD_DC_PORT + GPIO_BC) = LCD_DC_MASK; }
static void dc_dat(void) { REG32(LCD_DC_PORT + GPIO_BOP) = LCD_DC_MASK; }

/* Bit is tested first, so a ready FIFO does not pay for the timeout count. */
static void tx8(uint8_t v)
{
    uint32_t n = 20000u;
    while ((REG32(LCD_SPI_BASE + 0x08u) & 2u) == 0u && --n) {}
    REG32(LCD_SPI_BASE + 0x0Cu) = v;
    n = 20000u;
    while ((REG32(LCD_SPI_BASE + 0x08u) & 1u) == 0u && --n) {}
    (void)REG32(LCD_SPI_BASE + 0x0Cu);
}

static void wait_idle(void)
{
    uint32_t n = 20000u;
    while ((REG32(LCD_SPI_BASE + 0x08u) & 0x80u) != 0u && --n) {}
}

static void cmd(uint8_t c, const uint8_t *data, unsigned n)
{
    cs_hi();
    wait_idle();
    dc_cmd();
    cs_lo();
    tx8(c);
    if (n) {
        wait_idle();
        dc_dat();
        while (n--)
            tx8(*data++);
    }
    wait_idle();
    cs_hi();
}

static int panel_x(int sx) { return sx - VIEW_X0; }

static int column_visible(int sx) { return sx >= VIEW_X0 && sx <= VIEW_X1; }

static void set_window(int x0, int y0, int x1, int y1)
{
    uint8_t b[4];
    b[0] = (uint8_t)(x0 >> 8);
    b[1] = (uint8_t)x0;
    b[2] = (uint8_t)(x1 >> 8);
    b[3] = (uint8_t)x1;
    cmd(0x2A, b, 4);
    b[0] = (uint8_t)(y0 >> 8);
    b[1] = (uint8_t)y0;
    b[2] = (uint8_t)(y1 >> 8);
    b[3] = (uint8_t)y1;
    cmd(0x2B, b, 4);
    cmd(0x2C, 0, 0);
}

/* MADCTL is 0x00, so the panel's address counter runs along the screen x
   axis: a window wider than one pixel takes its pixels row by row. Every
   caller of lcd_push streams column-major instead (lcd.h), because that is
   what the pump and lcd_write_column need. So the polled stream opens one
   panel window per screen column: a one-pixel-wide window wraps on every
   pixel and walks down the column, which is the order the callers write.
   With a single wide window an 8x8 glyph came out transposed - the text of
   the pre-game pages looked turned by 90 degrees while the picture, which
   goes out column by column anyway, was right. */
static void open_column(void)
{
    int vy0 = ly0 < 0 ? 0 : ly0;
    int vy1 = ly1 > (PANEL_H - 1) ? (PANEL_H - 1) : ly1;

    col_open = 0;
    if (!win_open || !column_visible(cx) || vy1 < vy0)
        return;
    set_window(panel_x(cx), vy0, panel_x(cx), vy1);
    dc_dat();
    cs_lo();
    col_open = 1;
}

static void close_column(void)
{
    if (!col_open)
        return;
    spi_idle(LCD_SPI_BASE);
    cs_hi();
    col_open = 0;
}

static void begin_window(int x0, int y0, int x1, int y1)
{
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 >= LCD_W)
        x1 = LCD_W - 1;
    if (y1 >= LCD_H)
        y1 = LCD_H - 1;
    if (x1 < x0 || y1 < y0) {
        win_open = 0;
        col_open = 0;
        return;
    }
    lx0 = (int16_t)x0;
    ly0 = (int16_t)y0;
    lx1 = (int16_t)x1;
    ly1 = (int16_t)y1;
    cx = (int16_t)x0;
    cy = (int16_t)y0;
    win_open = 1;
    open_column();
}

void lcd_begin(int x0, int y0, int x1, int y1)
{
    pump_close();
    begin_window(x0, y0, x1, y1);
}

void lcd_begin_raw(int x0, int y0, int x1, int y1) { begin_window(x0, y0, x1, y1); }

void lcd_push(uint16_t c)
{
    if (!win_open)
        return;
    if (col_open && cy >= 0 && cy < PANEL_H)
        tx8((uint8_t)(c >> 8)), tx8((uint8_t)c);
    if (++cy > ly1) {
        cy = ly0;
        cx++;
        close_column();
        if (cx <= lx1)
            open_column();
    }
    (void)lx0;
}

void lcd_push_n(const uint16_t *px, uint32_t n)
{
    while (n--)
        lcd_push(*px++);
}

/* One screen column, top to bottom, as RGB565. The window is one pixel
   wide because MADCTL does not swap axes, so the address counter walks
   down the column. Stock sends each pixel as two 8-bit writes. */
unsigned lcd_write_column(int sx, int y0, int y1, const uint8_t *idx,
                          const uint8_t *rep, const uint16_t *lut, unsigned h)
{
    uint8_t b[4];
    unsigned i, px = 0;
    int x = sx - VIEW_X0;

    if (x < 0 || x >= PANEL_W || y1 < y0 || y0 < 0 || y1 >= PANEL_H)
        return 0;
    if (win_open)
        lcd_end();
    b[0] = 0;
    b[1] = (uint8_t)x;
    b[2] = 0;
    b[3] = (uint8_t)x;
    cmd(0x2A, b, 4);
    b[0] = (uint8_t)(y0 >> 8);
    b[1] = (uint8_t)y0;
    b[2] = (uint8_t)(y1 >> 8);
    b[3] = (uint8_t)y1;
    cmd(0x2B, b, 4);
    cmd(0x2C, 0, 0);
    dc_dat();
    cs_lo();
    for (i = 0; i < h; i++) {
        unsigned n = rep[i];
        uint16_t c;
        if (!n)
            continue;
        c = lut[idx[i]];
        px += n;
        do {
            tx8((uint8_t)(c >> 8));
            tx8((uint8_t)c);
        } while (--n);
    }
    wait_idle();
    cs_hi();
    return px;
}

void lcd_end(void)
{
    if (!win_open)
        return;
    close_column();
    win_open = 0;
}

/* One colour, so the order inside the window does not matter: this takes
   the whole rectangle in one window instead of one window per column. */
void lcd_fill(int x0, int y0, int x1, int y1, uint16_t c)
{
    uint32_t n;
    int vx0, vx1, vy0, vy1;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 >= LCD_W)
        x1 = LCD_W - 1;
    if (y1 >= LCD_H)
        y1 = LCD_H - 1;
    if (x1 < x0 || y1 < y0)
        return;
    vx0 = x0 < VIEW_X0 ? VIEW_X0 : x0;
    vx1 = x1 > VIEW_X1 ? VIEW_X1 : x1;
    vy0 = y0;
    vy1 = y1 > (PANEL_H - 1) ? (PANEL_H - 1) : y1;
    if (vx1 < vx0 || vy1 < vy0)
        return;
    pump_close();
    if (win_open)
        lcd_end();
    set_window(panel_x(vx0), vy0, panel_x(vx1), vy1);
    dc_dat();
    cs_lo();
    n = (uint32_t)(vx1 - vx0 + 1) * (uint32_t)(vy1 - vy0 + 1);
    while (n--) {
        tx8((uint8_t)(c >> 8));
        tx8((uint8_t)c);
    }
    spi_idle(LCD_SPI_BASE);
    cs_hi();
}

/* Stock backlight at 0x0800CD48: TIMER0 CH0, PA8, PSC 29, period 199,
   PWM mode 0. A compare of 200 stays high for the whole period. */
#define TIM0_BASE 0x40012C00u

void lcd_backlight(int on)
{
    REG32(TIM0_BASE + 0x28u) = 29u;
    REG32(TIM0_BASE + 0x2Cu) = 199u;
    REG32(TIM0_BASE + 0x18u) = 0x60u;
    /* Stock's full-brightness path writes compare 200. PWM mode 0 is
       high while the counter is below that, so 200 of 199 is 100%. */
    REG32(TIM0_BASE + 0x34u) = on ? 200u : 0u;
    REG32(TIM0_BASE + 0x14u) = 1u;
    REG32(TIM0_BASE + 0x20u) = 1u;
    REG32(TIM0_BASE + 0x44u) = 0x8000u;
    REG32(TIM0_BASE + 0x00u) = 0x81u;
}

void lcd_flip(int flip)
{
    uint8_t mad;
    pump_close();
    flipped = flip ? 1u : 0u;
    mad = flipped ? 0xC0u : 0x00u;
    cmd(0x36, &mad, 1);
}

int lcd_flipped(void) { return flipped; }
int lcd_profile(void) { return profile; }

unsigned lcd_row_reps(int y) { return (y >= 0 && y < PANEL_H) ? 1u : 0u; }

void lcd_recover(void)
{
    cs_hi();
    win_open = 0;
    col_open = 0;
    spi_init(LCD_SPI_BASE, SPI_CTL_LCD);
}

/* Command stream from the stock function at 0x0801C3F4.
   MADCTL data is 0x00. COLMOD data is 0x55. */
static const uint8_t lcd_panel_init[] = {
    0x36, 1, 0x00,
    0x3A, 1, 0x55,
    0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33,
    0xB7, 1, 0x35,
    0xBB, 1, 0x24,
    0xC0, 1, 0x2C,
    0xC2, 1, 0x01,
    0xC3, 1, 0x21,
    0xC4, 1, 0x0C,
    0xC6, 1, 0x0F,
    0xD0, 2, 0xA4, 0xA1,
    0xD6, 1, 0xA1,
    0xE0, 14, 0xD0, 0x06, 0x10, 0x09, 0x0A, 0x07, 0x3A, 0x43, 0x50, 0x17, 0x13, 0x13, 0x31, 0x33,
    0xE1, 14, 0xD0, 0x12, 0x17, 0x0D, 0x0B, 0x25, 0x3A, 0x43, 0x4F, 0x38, 0x15, 0x15, 0x2F, 0x35,
    0
};

static void send_table(const uint8_t *p)
{
    while (*p) {
        uint8_t n = p[1];
        cmd(p[0], p + 2, n);
        p += 2u + n;
    }
}

static void panel_reset(void)
{
    /* Stock 0x0801C3F4: high 20 ms, SPI setup, low 50 ms, high 120 ms. */
    REG32(LCD_CS_PORT + GPIO_BOP) = LCD_RST_MASK;
    plat_delay_ms(20);
    spi_init(LCD_SPI_BASE, SPI_CTL_LCD);
    cs_hi();
    REG32(LCD_CS_PORT + GPIO_BC) = LCD_RST_MASK;
    plat_delay_ms(50);
    REG32(LCD_CS_PORT + GPIO_BOP) = LCD_RST_MASK;
    plat_delay_ms(120);
}

void lcd_init(int prof)
{
    profile = (uint8_t)prof;
    panel_reset();
    cmd(0x11, 0, 0);
    plat_delay_ms(120);
    send_table(lcd_panel_init);
    cmd(0x21, 0, 0);
    lcd_fill(0, 0, LCD_W - 1, LCD_H - 1, 0);
    cmd(0x29, 0, 0);
    plat_delay_ms(20);
    cmd(0x11, 0, 0);
    plat_delay_ms(10);
    cmd(0x11, 0, 0);
    plat_delay_ms(80);
    lcd_fill(0, 0, LCD_W - 1, LCD_H - 1, 0);
    cmd(0x29, 0, 0);
    lcd_backlight(1);
}
