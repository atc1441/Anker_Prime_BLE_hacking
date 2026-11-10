// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Stub C library for the freestanding ARM size build of the engine. The
// string functions are real and small. The formatted output and the heap
// are empty placeholders: the firmware formats with the platform's
// plat_vfmt (port/game/libc_dmc.c) and has no heap.

#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

FILE *stderr;
FILE *stdout;

void *memcpy(void *d, const void *s, size_t n)
{
    uint8_t *p = d;
    const uint8_t *q = s;
    while (n--)
        *p++ = *q++;
    return d;
}

void *memmove(void *d, const void *s, size_t n)
{
    uint8_t *p = d;
    const uint8_t *q = s;
    if (p < q) {
        while (n--)
            *p++ = *q++;
    } else {
        while (n--)
            p[n] = q[n];
    }
    return d;
}

void *memset(void *d, int c, size_t n)
{
    uint8_t *p = d;
    while (n--)
        *p++ = (uint8_t)c;
    return d;
}

void __aeabi_memcpy(void *d, const void *s, size_t n)
{
    memcpy(d, s, n);
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    for (; n; n--, x++, y++)
        if (*x != *y)
            return *x - *y;
    return 0;
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

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return (size_t)(p - s);
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        int d = (unsigned char)*a - (unsigned char)*b;
        if (d || !*a)
            return d;
    }
    return 0;
}

int strcmp(const char *a, const char *b)
{
    return strncmp(a, b, (size_t)-1);
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

int abs(int v)
{
    return v < 0 ? -v : v;
}

void *calloc(size_t n, size_t m)
{
    (void)n;
    (void)m;
    return NULL;
}

void free(void *p)
{
    (void)p;
}

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap)
{
    (void)fmt;
    (void)ap;
    if (n)
        buf[0] = 0;
    return 0;
}

int snprintf(char *buf, size_t n, const char *fmt, ...)
{
    (void)fmt;
    if (n)
        buf[0] = 0;
    return 0;
}

int printf(const char *fmt, ...)
{
    (void)fmt;
    return 0;
}

int fprintf(FILE *f, const char *fmt, ...)
{
    (void)f;
    (void)fmt;
    return 0;
}

int sscanf(const char *s, const char *fmt, ...)
{
    (void)s;
    (void)fmt;
    return 0;
}
