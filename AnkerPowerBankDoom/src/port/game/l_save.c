// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Entry table of the SAVEG overlay (fe.h, port/game/g_save.c), linked at FRAME_BASE
// (mk/fw.mk).

#include "doomdef.h"

#include "fe.h"
#include "ovl.h"

OVL_TABLE(SAVEG, OVL_ENTRY(SAVE_E_WRITE, save_fn, G_SaveWrite),
          OVL_ENTRY(SAVE_E_CHECK, save_check_fn, G_SaveCheck),
          OVL_ENTRY(SAVE_E_RESTORE, save_fn, G_SaveRestore),
          OVL_ENTRY(SAVE_E_HEADER, save_check_fn, G_SaveHeader));
