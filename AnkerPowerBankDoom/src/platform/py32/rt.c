// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// The few C library functions the compiler and the platform need. There is no libc and no
// compiler-rt on the target. Built with -fno-builtin so these loops stay loops. All are weak,
// so another C library linked into the same image (for example engine/stub/stub_libc.c)
// takes precedence without a duplicate symbol error.

#include <stddef.h>
#include <stdint.h>

typedef uint32_t __attribute__((may_alias)) word_t;

#define WEAK __attribute__((weak))

WEAK void *memcpy(void *restrict d, const void *restrict s, size_t n)
{
    uint8_t *p = d;
    const uint8_t *q = s;
    if (!(((uintptr_t)p | (uintptr_t)q) & 3u)) {
        for (; n >= 4; n -= 4, p += 4, q += 4)
            *(word_t *)(void *)p = *(const word_t *)(const void *)q;
    }
    while (n--)
        *p++ = *q++;
    return d;
}

WEAK void *memmove(void *d, const void *s, size_t n)
{
    uint8_t *p = d;
    const uint8_t *q = s;
    if (p <= q || p >= q + n)
        return memcpy(d, s, n);
    while (n--)
        p[n] = q[n];
    return d;
}

WEAK void *memset(void *d, int c, size_t n)
{
    uint8_t *p = d;
    if (!((uintptr_t)p & 3u)) {
        uint32_t v = (uint8_t)c * 0x01010101u;
        for (; n >= 4; n -= 4, p += 4)
            *(word_t *)(void *)p = v;
    }
    while (n--)
        *p++ = (uint8_t)c;
    return d;
}

WEAK int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *p = a, *q = b;
    for (; n; n--, p++, q++) {
        if (*p != *q)
            return *p - *q;
    }
    return 0;
}

WEAK size_t strlen(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return (size_t)(p - s);
}

// ARM EABI entry points the compiler uses for struct copies and clears
WEAK void __aeabi_memcpy(void *d, const void *s, size_t n) { memcpy(d, s, n); }
void __aeabi_memcpy4(void *d, const void *s, size_t n) __attribute__((weak, alias("__aeabi_memcpy")));
void __aeabi_memcpy8(void *d, const void *s, size_t n) __attribute__((weak, alias("__aeabi_memcpy")));
WEAK void __aeabi_memmove(void *d, const void *s, size_t n) { memmove(d, s, n); }
void __aeabi_memmove4(void *d, const void *s, size_t n) __attribute__((weak, alias("__aeabi_memmove")));
void __aeabi_memmove8(void *d, const void *s, size_t n) __attribute__((weak, alias("__aeabi_memmove")));
WEAK void __aeabi_memset(void *d, size_t n, int c) { memset(d, c, n); }
void __aeabi_memset4(void *d, size_t n, int c) __attribute__((weak, alias("__aeabi_memset")));
void __aeabi_memset8(void *d, size_t n, int c) __attribute__((weak, alias("__aeabi_memset")));
WEAK void __aeabi_memclr(void *d, size_t n) { memset(d, 0, n); }
void __aeabi_memclr4(void *d, size_t n) __attribute__((weak, alias("__aeabi_memclr")));
void __aeabi_memclr8(void *d, size_t n) __attribute__((weak, alias("__aeabi_memclr")));
