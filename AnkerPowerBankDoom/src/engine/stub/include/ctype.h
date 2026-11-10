// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Minimal <ctype.h> for the freestanding ARM builds (the firmware and the size build).

#ifndef DMC_STUB_CTYPE_H
#define DMC_STUB_CTYPE_H

int toupper(int c);
int tolower(int c);
int isspace(int c);
int isdigit(int c);
int isalpha(int c);
int isprint(int c);

#endif
