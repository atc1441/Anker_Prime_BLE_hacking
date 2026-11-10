// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Minimal <stdio.h> for the freestanding ARM builds (the firmware and the size build).

#ifndef DMC_STUB_STDIO_H
#define DMC_STUB_STDIO_H

#include <stdarg.h>
#include <stddef.h>

typedef struct dmc_file FILE;
extern FILE *stderr;
extern FILE *stdout;

int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int sscanf(const char *s, const char *fmt, ...);

#endif
