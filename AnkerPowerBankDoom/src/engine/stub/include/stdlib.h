// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Minimal <stdlib.h> for the freestanding ARM builds (the firmware and the size build).

#ifndef DMC_STUB_STDLIB_H
#define DMC_STUB_STDLIB_H

#include <stddef.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

int abs(int v);
int atoi(const char *s);
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void *realloc(void *p, size_t n);
void free(void *p);
void exit(int code) __attribute__((noreturn));

#endif
