/* The pages that come up before Doom: battery, ports, system.

   One tap turns the page, a quick double tap starts the game, a triple
   goes back one page and four taps still reset the bank. The numbers
   are read live while the pages are up: the gauge every second, the
   port chip twice a second. */

#include <stdarg.h>

#include "adc.h"
#include "gd32.h"
#include "gestures.h"
#include "lcd.h"
#include "nor.h"
#include "platform.h"
#include "power.h"
#include "pagedraw.h"
#include "ui.h"

#define PAGES 3
#define HDR_H 14
#define FTR_H 12
#define BODY_Y (HDR_H + 4)
#define FTR_Y (PANEL_H - FTR_H)

#define C_RULE 0x2965u  /* slate, for the hair lines */
#define C_NOTE 0x6B6Du  /* readable grey for the caption lines */
#define C_PANEL 0x10A2u /* very dark blue, the card background */

extern char __image_end;

static const char *const page_name[PAGES] = {"BATTERY", "PORTS", "SYSTEM"};

static uint16_t soc_colour(uint16_t soc100)
{
    if (soc100 >= 5000u)
        return C_GREEN;
    if (soc100 >= 2000u)
        return C_YELLOW;
    return C_RED;
}

static void header(int page)
{
    char s[40];
    plat_fmt(s, sizeof s, "%d/%d %s", page + 1, PAGES, page_name[page]);
    pg_edge_fill(0, HDR_H, C_YELLOW);
    pg_text(0, 3, C_BLACK, C_YELLOW, 1, s);
    pg_fill(0, HDR_H - 1, PG_W, 1, C_ORANGE);
}

static void footer(void)
{
    pg_edge_fill(FTR_Y, FTR_H, C_PANEL);
    pg_fill(0, FTR_Y, PG_W, 1, C_RULE);
    pg_text(0, FTR_Y + 2, C_GREY, C_PANEL, 1, "1x PAGE  2x DOOM  4x RESET");
}

static void body_clear(void)
{
    pg_edge_fill(HDR_H, FTR_Y - HDR_H, C_BLACK);
}

/* ---- page 1: battery ---- */

static void page_battery(void)
{
    const struct gauge_reading *g = gauge_now();
    int ma = power_pack_ma();
    unsigned abs_ma = (unsigned)(ma < 0 ? -ma : ma);
    uint16_t soc = g->ok ? g->soc100 : 0;
    uint16_t col = soc_colour(soc);
    int y;

    if (!g->ok) {
        pg_center(70, C_RED, C_BLACK, 2, "NO GAUGE");
        pg_line(100, C_WHITE, C_BLACK, "CW2217 on PE10/PE11 is silent.");
        pg_line(112, C_GREY, C_BLACK, "Doom still starts with 2 taps.");
        return;
    }

    pg_centerf(22, col, C_BLACK, 3, "%3u.%02u%%", soc / 100u, soc % 100u);
    pg_bar(0, 60, PG_W, 22, soc, col, C_PANEL, C_RULE);

    y = 94;
    pg_pair(y, C_GREY, "CELL", C_WHITE, "%u.%03u V", g->mv / 1000u, g->mv % 1000u);
    y += 13;
    pg_pair(y, C_GREY, "CURRENT", g->charging ? C_GREEN : C_CYAN, "%c%u.%03u A",
             ma < 0 ? '-' : '+', abs_ma / 1000u, abs_ma % 1000u);
    y += 13;
    pg_pair(y, C_GREY, "FLOW", g->charging ? C_GREEN : C_CYAN,
             abs_ma < 15u ? "IDLE" : (g->charging ? "CHARGING IN" : "OUTPUT"));
    y += 13;
    pg_pair(y, C_GREY, "TEMP", g->temp_c > 45 ? C_ORANGE : C_WHITE, "%d C", g->temp_c);
    y += 13;
    pg_pair(y, C_GREY, "CYCLES", C_WHITE, "%u", g->cycles);
    y += 13;
    pg_pair(y, C_GREY, "GAUGE ST", C_WHITE, "0x%02X", g->status);
    y += 17;
    pg_fill(0, y, PG_W, 1, C_RULE);
    y += 5;
    pg_line(y, C_NOTE, C_BLACK, "cell gauge CW2217, I2C 0x64");
}

/* ---- page 2: ports ---- */

#define COL_V 72
#define COL_A 128
#define COL_W 184

static void port_row(int y, const char *name, const struct port_reading *p)
{
    pg_text(0, y, C_WHITE, C_BLACK, 1, name);
    if (p && p->ok) {
        uint16_t c = p->da ? C_GREEN : C_GREY;
        pg_textf(COL_V, y, c, C_BLACK, 1, "%2u.%u", p->dv / 10u, p->dv % 10u);
        pg_textf(COL_A, y, c, C_BLACK, 1, "%2u.%u", p->da / 10u, p->da % 10u);
        pg_textf(COL_W, y, c, C_BLACK, 1, "%3u.%u", p->dw / 10u, p->dw % 10u);
    } else {
        pg_text(COL_V, y, C_DGREY, C_BLACK, 1, "  --");
        pg_text(COL_A, y, C_DGREY, C_BLACK, 1, "  --");
        pg_text(COL_W, y, C_DGREY, C_BLACK, 1, "   --");
    }
}

/* The current channels have an offset that is not calibrated yet, so the
   raw count is shown instead of an amp figure that would be wrong. */
static void adc_row(int y, const char *name, int ok, uint16_t dv, uint16_t da, uint16_t dw)
{
    pg_text(0, y, C_WHITE, C_BLACK, 1, name);
    if (ok) {
        uint16_t c = dv ? C_GREEN : C_GREY;
        pg_textf(COL_V, y, c, C_BLACK, 1, "%2u.%u", dv / 10u, dv % 10u);
        pg_textf(COL_A, y, c, C_BLACK, 1, "%2u.%u", da / 10u, da % 10u);
        pg_textf(COL_W, y, c, C_BLACK, 1, "%3u.%u", dw / 10u, dw % 10u);
    } else {
        pg_text(COL_V, y, C_DGREY, C_BLACK, 1, "  --");
        pg_text(COL_A, y, C_DGREY, C_BLACK, 1, "  --");
        pg_text(COL_W, y, C_DGREY, C_BLACK, 1, "   --");
    }
}

static void page_ports(void)
{
    const struct gauge_reading *g = gauge_now();
    const struct port_reading *a = power_usba();
    int ma = power_pack_ma();
    unsigned abs_ma = (unsigned)(ma < 0 ? -ma : ma);
    int y = 22;

    pg_text(0, y, C_GREY, C_BLACK, 1, "PORT");
    pg_text(COL_V, y, C_GREY, C_BLACK, 1, "V");
    pg_text(COL_A, y, C_GREY, C_BLACK, 1, "A");
    pg_text(COL_W, y, C_GREY, C_BLACK, 1, "W");
    y += 10;
    pg_fill(0, y, PG_W, 1, C_RULE);

    y += 6;
    {
        const struct adc_reading *ad = adc_now();
        adc_row(y, "USB-C1", ad->ok, ad->c1_dv, ad->c1_da, ad->c1_dw);
        y += 13;
        adc_row(y, "USB-C2", ad->ok, ad->c2_dv, ad->c2_da, ad->c2_dw);
    }
    y += 13;
    port_row(y, "USB-A", a);

    y += 20;
    pg_fill(0, y, PG_W, 1, C_RULE);
    y += 6;
    if (g->ok) {
        pg_pair(y, C_GREY, "PACK", C_WHITE, "%u.%03u V", g->mv / 1000u, g->mv % 1000u);
        y += 13;
        pg_pair(y, C_GREY, "PACK I", ma < 0 ? C_CYAN : C_GREEN, "%c%u.%03u A",
                 ma < 0 ? '-' : '+', abs_ma / 1000u, abs_ma % 1000u);
        y += 13;
    }
    {
        uint16_t r = power_rail();
        pg_pair(y, C_GREY, "RAIL", r >= 400u ? C_GREEN : C_RED,
                r >= 400u ? "up %u" : "down %u", r);
    }
    y += 13;
    pg_pair(y, C_GREY, "USB-A CHIP", a->ok ? C_GREEN : C_ORANGE,
            a->ok ? "0x30 live" : (power_usba_given_up() ? "0x30 idle" : "0x30 ..."));
    y += 13;
    if (a->ok)
        pg_pair(y, C_GREY, "A RAW", C_WHITE, "%02X %5u %5u", a->state, a->mv, a->ma);
    else
        pg_pair(y, C_GREY, "A RAW", C_DGREY, "-");
    y += 17;
    pg_line(y, C_NOTE, C_BLACK, "C1/C2 from ADC0, USB-A from");
    y += 10;
    pg_line(y, C_NOTE, C_BLACK, "its own chip on I2C 0x30.");
}

/* ---- page 3: system ---- */

static void page_system(void)
{
    const struct gauge_reading *g = gauge_now();
    const uint8_t *seen;
    unsigned n, i;
    uint32_t up = plat_ms() / 1000u;
    char list[28];
    unsigned at = 0;
    int y = 22;

    pg_pair(y, C_GREY, "FIRMWARE", C_WHITE, "DOOM GD32");
    y += 13;
    pg_pair(y, C_GREY, "BUILD", C_WHITE, "%s", __DATE__);
    y += 13;
    pg_pair(y, C_GREY, "UPTIME", C_WHITE, "%u:%02u:%02u", (unsigned)(up / 3600u),
            (unsigned)((up / 60u) % 60u), (unsigned)(up % 60u));
    y += 13;
    pg_pair(y, C_GREY, "NOR", C_WHITE, "%06X", (unsigned)nor_jedec());
    y += 13;
    pg_pair(y, C_GREY, "PANEL", C_WHITE, "ST7789 %dx%d", PANEL_W, PANEL_H);
    y += 13;
    pg_pair(y, C_GREY, "IMAGE", C_WHITE, "%u B", (unsigned)(&__image_end - (char *)0x08000000u));
    y += 13;
    pg_pair(y, C_GREY, "CPU", C_WHITE, "%u MHz", (unsigned)(CPU_HZ / 1000000u));
    y += 13;
    pg_pair(y, C_GREY, "BLE BYTES", C_WHITE, "%u", (unsigned)ble_bytes());
    y += 13;
    pg_pair(y, C_GREY, "BLE REPLY", C_WHITE, "%u", (unsigned)ble_replies());
    y += 13;
    pg_pair(y, C_GREY, "BLE CMD", C_WHITE, "%04X", ble_last());
    y += 13;

    seen = power_i2c_seen(&n);
    for (i = 0; i < n && at + 3u < sizeof list; i++) {
        static const char hex[] = "0123456789ABCDEF";
        list[at++] = hex[(seen[i] >> 4) & 0xFu];
        list[at++] = hex[seen[i] & 0xFu];
        list[at++] = ' ';
    }
    list[at] = 0;
    pg_pair(y, C_GREY, "I2C", C_WHITE, "%s", at ? list : "scanning");
    y += 13;
    pg_pair(y, C_GREY, "GAUGE", g->ok ? C_GREEN : C_RED, g->ok ? "ok" : "silent");
    y += 13;
    {
        uint16_t r = power_rail();
        pg_pair(y, C_GREY, "RAIL", r >= 400u ? C_GREEN : C_RED, "%u %s", r,
                power_cold_start() ? "cold" : "kept");
    }
    y += 17;
    pg_fill(0, y, PG_W, 1, C_RULE);
    y += 5;
    pg_line(y, C_NOTE, C_BLACK, "github.com/atc1441");
}

static void draw(int page)
{
    switch (page) {
    case 0:
        page_battery();
        break;
    case 1:
        page_ports();
        break;
    default:
        page_system();
        break;
    }
}

void bank_splash(void)
{
    int page = 0;
    int redraw = 1;
    uint32_t drawn = 0;
    uint32_t acted = 0;

    buttons_splash_begin();
    power_init();
    power_set_active(0);
    ui_clear();
    footer();
    for (;;) {
        int act;
        ble_poll();
        gauge_poll();
        power_poll();
        adc_poll();

        act = buttons_splash_action();
        if (!act && ble_pressed())
            act = GEST_FIRE;
        /* A held double or triple repeats while the finger is down, so
           one action per 400 ms is enough to walk the pages. */
        if (act && (uint32_t)(plat_ms() - acted) >= 400u) {
            acted = plat_ms();
            if (act == GEST_LEFT)
                break;
            if (act == GEST_FIRE)
                page = (page + 1) % PAGES;
            else if (act == GEST_RIGHT)
                page = (page + PAGES - 1) % PAGES;
            redraw = 1;
        }

        if (redraw) {
            redraw = 0;
            power_set_active(page != 0);
            body_clear();
            header(page);
            footer();
            draw(page);
            drawn = plat_ms();
        } else if ((uint32_t)(plat_ms() - drawn) >= 400u) {
            draw(page);
            drawn = plat_ms();
        }
        wdt_kick();
        plat_idle();
    }
    buttons_splash_end();
}
