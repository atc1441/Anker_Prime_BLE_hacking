// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Minimal <strings.h> for the freestanding ARM builds (the firmware and the size build).

#ifndef DMC_STUB_STRINGS_H
#define DMC_STUB_STRINGS_H

#include <stddef.h>

int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, size_t n);

#endif
