// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// FixedMul of m_fixed.c inlined, for the hot paths that cannot afford the
// call (R_MapPlane makes six per span). The engine keeps m_fixed.c for the
// other calls.

#ifndef DMC_FIXED_H
#define DMC_FIXED_H

#include <stdint.h>

static inline __attribute__((always_inline)) int FixedMulI(int a, int b)
{
    return (int)(((int64_t)a * b) >> 16);
}

#endif
