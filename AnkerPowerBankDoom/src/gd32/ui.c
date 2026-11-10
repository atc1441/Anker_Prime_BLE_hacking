/* The screen helpers and the vendor ui_* API, for this board.

   This file replaces the port's platform/apps/ui.c in the build. The
   port's version writes text from x = 0 of the 320-wide engine screen,
   but the glass only shows columns VIEW_X0..VIEW_X1, so those first 40
   columns fall off the left bezel: the boot and loading pages lost their
   first five characters and everything else sat hard against the edge.
   Here every row starts at the text margin instead, and the bars that
   want the whole glass say so. */

#include <stdarg.h>

#include "pagedraw.h"
#include "ui.h"

#include "lcd.h"
#include "platform.h"

#define ROW_H 10

static char buf[64];

static void vfmt(const char *f, va_list ap) { plat_vfmt(buf, (int)sizeof buf, f, ap); }

void pg_clear(uint16_t bg) { lcd_fill(0, 0, LCD_W - 1, LCD_H - 1, bg); }

void pg_fill(int x, int y, int w, int h, uint16_t c)
{
    if (w <= 0 || h <= 0)
        return;
    lcd_fill(PG_X0 + x, y, PG_X0 + x + w - 1, y + h - 1, c);
}

void pg_edge_fill(int y, int h, uint16_t c)
{
    if (h <= 0)
        return;
    lcd_fill(PG_EDGE_X0, y, PG_EDGE_X0 + PG_EDGE_W - 1, y + h - 1, c);
}

void pg_frame(int x, int y, int w, int h, uint16_t c)
{
    if (w <= 1 || h <= 1)
        return;
    pg_fill(x, y, w, 1, c);
    pg_fill(x, y + h - 1, w, 1, c);
    pg_fill(x, y, 1, h, c);
    pg_fill(x + w - 1, y, 1, h, c);
}

int pg_text(int x, int y, uint16_t fg, uint16_t bg, int scale, const char *s)
{
    return lcd_text(PG_X0 + x, y, s, fg, bg, scale) - PG_X0;
}

int pg_textf(int x, int y, uint16_t fg, uint16_t bg, int scale, const char *f, ...)
{
    va_list ap;
    va_start(ap, f);
    vfmt(f, ap);
    va_end(ap);
    return pg_text(x, y, fg, bg, scale, buf);
}

static int text_px(const char *s, int scale)
{
    int n = 0;
    while (s[n])
        n++;
    return n * 8 * scale;
}

void pg_center(int y, uint16_t fg, uint16_t bg, int scale, const char *s)
{
    int w = text_px(s, scale);
    int x = (PG_W - w) / 2;
    if (x < 0)
        x = 0;
    pg_text(x, y, fg, bg, scale, s);
}

void pg_centerf(int y, uint16_t fg, uint16_t bg, int scale, const char *f, ...)
{
    va_list ap;
    va_start(ap, f);
    vfmt(f, ap);
    va_end(ap);
    pg_center(y, fg, bg, scale, buf);
}

void pg_line(int y, uint16_t fg, uint16_t bg, const char *f, ...)
{
    va_list ap;
    int x;
    va_start(ap, f);
    vfmt(f, ap);
    va_end(ap);
    x = pg_text(0, y, fg, bg, 1, buf);
    if (x < PG_W)
        pg_fill(x, y, PG_W - x, 8, bg);
}

void pg_pair(int y, uint16_t lfg, const char *label, uint16_t vfg, const char *f, ...)
{
    va_list ap;
    int w, x;
    pg_text(0, y, lfg, C_BLACK, 1, label);
    va_start(ap, f);
    vfmt(f, ap);
    va_end(ap);
    w = text_px(buf, 1);
    x = PG_W - w;
    if (x < 0)
        x = 0;
    /* Clear between the label and the right-aligned value so a value
       that got shorter does not leave its old tail behind. */
    {
        int lw = text_px(label, 1);
        if (x > lw)
            pg_fill(lw, y, x - lw, 8, C_BLACK);
    }
    pg_text(x, y, vfg, C_BLACK, 1, buf);
}

void pg_bar(int x, int y, int w, int h, uint16_t pct100, uint16_t fill, uint16_t rest,
             uint16_t frame)
{
    int inner = w - 4;
    int done;
    if (pct100 > 10000u)
        pct100 = 10000u;
    if (inner <= 0 || h <= 4)
        return;
    pg_frame(x, y, w, h, frame);
    done = (int)(((uint32_t)inner * pct100) / 10000u);
    pg_fill(x + 2, y + 2, done, h - 4, fill);
    pg_fill(x + 2 + done, y + 2, inner - done, h - 4, rest);
}

/* ---- the vendor API, now inset ---- */

void ui_clear(void) { pg_clear(C_BLACK); }

void ui_text_row(int y, uint16_t fg, uint16_t bg, const char *s)
{
    int x;
    /* The band runs the whole glass so a header or footer colour reaches
       both bezels; the text itself keeps the margin. */
    pg_edge_fill(y, ROW_H, bg);
    x = pg_text(0, y + 1, fg, bg, 1, s);
    if (x < PG_W)
        pg_fill(x, y + 1, PG_W - x, 8, bg);
}

void ui_header(const char *title, int page, int npages)
{
    char s[44];
    plat_fmt(s, sizeof(s), "%d/%d %s", page + 1, npages, title);
    ui_text_row(0, C_BLACK, C_YELLOW, s);
}

void ui_footer(const char *s) { ui_text_row(LCD_H - ROW_H, C_BLACK, C_GREY, s); }

void ui_row(int r, uint16_t fg, const char *f, ...)
{
    char s[44];
    va_list ap;
    va_start(ap, f);
    plat_vfmt(s, sizeof(s), f, ap);
    va_end(ap);
    if (r >= 0 && r < UI_ROWS)
        ui_text_row(UI_ROW_Y(r), fg, C_BLACK, s);
}

int ui_power_held(uint32_t ms)
{
    static uint32_t since;
    static int armed, released;
    uint32_t b = plat_buttons();
    if (!(b & BTN_BIT(BTN_POWER)))
        released = 1;
    if (b != BTN_BIT(BTN_POWER) || !released) {
        armed = 0;
        return 0;
    }
    if (!armed) {
        armed = 1;
        since = plat_ms();
    }
    return plat_ms() - since >= ms;
}

void ui_fix2(char *out, int cap, uint32_t v100)
{
    plat_fmt(out, cap, "%u.%02u", (unsigned)(v100 / 100u), (unsigned)(v100 % 100u));
}

uint32_t ui_cyc_to_us100(uint32_t cycles)
{
    uint32_t mhz = CPU_HZ / 1000000u;
    return cycles / mhz * 100u + (cycles % mhz) * 100u / mhz;
}
