// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// 8x8 text on the LCD and a small snprintf subset. Shared by the target and host builds.

#include <stdarg.h>

#include "lcd.h"
#include "platform.h"

extern const uint8_t font8x8[95][8];

int lcd_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale)
{
    if (scale < 1)
        scale = 1;
    if (scale > 4)
        scale = 4;
    int w = 8 * scale;
    for (; *s && x + w <= LCD_W; s++, x += w) {
        unsigned c = (unsigned char)*s;
        const uint8_t *g = font8x8[(c < 32u || c > 126u ? '?' : c) - 32u];
        if (x < 0 || y < 0 || y + w > LCD_H)
            continue;
        lcd_begin(x, y, x + w - 1, y + w - 1);
        for (int gx = 0; gx < 8; gx++) {
            uint8_t m = (uint8_t)(0x80u >> gx);
            for (int sx = 0; sx < scale; sx++) {
                for (int gy = 0; gy < 8; gy++) {
                    uint16_t c16 = (g[gy] & m) ? fg : bg;
                    for (int sy = 0; sy < scale; sy++)
                        lcd_push(c16);
                }
            }
        }
        lcd_end();
    }
    return x;
}

int plat_vfmt(char *out, int cap, const char *f, va_list ap)
{
    int n = 0;
#define PUT(ch)                  \
    do {                         \
        if (n < cap - 1)         \
            out[n] = (char)(ch); \
        n++;                     \
    } while (0)
    for (; *f; f++) {
        if (*f != '%') {
            PUT(*f);
            continue;
        }
        f++;
        int left = 0, zero = 0, width = 0, len = 0;
        if (*f == '-') {
            left = 1;
            f++;
        }
        if (*f == '0') {
            zero = 1;
            f++;
        }
        while (*f >= '0' && *f <= '9')
            width = width * 10 + (*f++ - '0');
        char tmp[12];
        const char *str = tmp;
        if (*f == 's') {
            str = va_arg(ap, const char *);
            if (!str)
                str = "(null)";
            while (str[len])
                len++;
        } else if (*f == 'c') {
            tmp[len++] = (char)va_arg(ap, int);
        } else if (*f == 'd' || *f == 'i' || *f == 'u' || *f == 'x' || *f == 'X') {
            uint32_t v, base = (*f == 'x' || *f == 'X') ? 16u : 10u;
            int neg = 0;
            if (*f == 'd' || *f == 'i') {
                int32_t sv = va_arg(ap, int32_t);
                neg = sv < 0;
                v = neg ? 0u - (uint32_t)sv : (uint32_t)sv;
            } else {
                v = va_arg(ap, uint32_t);
            }
            char rev[11];
            int r = 0;
            do {
                rev[r++] = "0123456789ABCDEF"[v % base];
                v /= base;
            } while (v);
            if (neg && zero && width > r) {
                PUT('-');
                width--;
            } else if (neg) {
                rev[r++] = '-';
            }
            while (r)
                tmp[len++] = rev[--r];
        } else {
            tmp[len++] = *f ? *f : '%';
            if (!*f)
                f--;
        }
        for (int i = len; !left && i < width; i++)
            PUT(zero ? '0' : ' ');
        for (int i = 0; i < len; i++)
            PUT(str[i]);
        for (int i = len; left && i < width; i++)
            PUT(' ');
    }
    if (cap > 0)
        out[n < cap ? n : cap - 1] = 0;
#undef PUT
    return n;
}

int plat_fmt(char *out, int cap, const char *f, ...)
{
    va_list ap;
    va_start(ap, f);
    int n = plat_vfmt(out, cap, f, ap);
    va_end(ap);
    return n;
}
