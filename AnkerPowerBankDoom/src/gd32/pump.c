/* Polled palette pump. The panel and the NOR are on different SPI buses,
   so a column is just written out. Columns outside 40..279 are dropped:
   the engine still renders 320 of them, and the glass is 240 wide. */

#include "gd32.h"
#include "lcd.h"
#include "platform.h"

uint16_t pump_lut[256];
volatile struct pump_stats pump_stats;

static uint8_t is_open;
static uint8_t reps_dbl;
static uint8_t saw_column;
static int16_t win_x1, win_y0, win_y1, cur_x, win_h;
static uint8_t rowrep[LCD_H];

static void check_window(int x0, int x1, int y0, int y1)
{
    int h = y1 - y0 + 1;
    if (h <= 0 || h > LCD_H || x0 < 0 || x1 < x0 || x1 >= LCD_W || y0 < 0 || y1 >= LCD_H)
        plat_panic("pump: bad window");
}

void pump_end(void)
{
    if (!is_open)
        return;
    is_open = 0;
    if (win_h >= LCD_VIEW_H)
        dbg_frame();
    lcd_end();
}

void pump_close(void) { pump_end(); }
void pump_reset(void) { is_open = 0; }
void pump_sync(int max) { (void)max; }
int pump_pending(void) { return 0; }

void pump_begin(int x0, int x1, int y0, int y1, int flags)
{
    int i;
    check_window(x0, x1, y0, y1);
    pump_close();
    win_x1 = (int16_t)x1;
    win_y0 = (int16_t)y0;
    win_y1 = (int16_t)y1;
    cur_x = (int16_t)x0;
    win_h = (int16_t)(y1 - y0 + 1);
    reps_dbl = (flags & PUMP_DOUBLE) ? 2u : 1u;
    for (i = 0; i < win_h; i++)
        rowrep[i] = (uint8_t)lcd_row_reps(y0 + i);
    is_open = 1;
}

void pump_submit(const uint8_t *cols, int ncols)
{
    uint32_t t0 = DWT_CYCCNT;
    uint32_t px = 0;
    const uint16_t *lut = pump_lut;
    unsigned h = (unsigned)win_h;

    if (!is_open || ncols <= 0)
        return;
    if (!saw_column) {
        saw_column = 1;
        dbg_str("pump columns");
        dbg_nl();
        dbg_mark(6);
    }

    for (; ncols > 0 && cur_x <= win_x1; ncols--, cols += h) {
        unsigned r;
        for (r = 0; r < reps_dbl && cur_x <= win_x1; r++, cur_x++) {
            if (cur_x < VIEW_X0 || cur_x > VIEW_X1)
                continue;
            px += lcd_write_column(cur_x, win_y0, win_y1, cols, rowrep, lut, h);
        }
    }
    dbg_pump(DWT_CYCCNT - t0, px);
}
