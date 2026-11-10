// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The C library functions the engine calls on the meter, beyond the mem* and strlen of
// platform/rt.c. vsnprintf is the platform's formatter (%s %c %d %i %u %x).

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "platform.h"

int abs(int v)
{
    return v < 0 ? -v : v;
}

int atoi(const char *s)
{
    int v = 0, neg = *s == '-';
    if (neg || *s == '+')
        s++;
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

int toupper(int c)
{
    return c >= 'a' && c <= 'z' ? c - 32 : c;
}

int strncasecmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        int d = toupper((unsigned char)*a) - toupper((unsigned char)*b);
        if (d || !*a)
            return d;
    }
    return 0;
}

int strcasecmp(const char *a, const char *b)
{
    return strncasecmp(a, b, (size_t)-1);
}

char *strncpy(char *d, const char *s, size_t n)
{
    char *p = d;
    for (; n && *s; n--)
        *p++ = *s++;
    while (n--)
        *p++ = 0;
    return d;
}

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap)
{
    return plat_vfmt(buf, (int)n, fmt, ap);
}
