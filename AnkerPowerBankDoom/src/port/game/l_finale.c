// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Entry table of the FINALE overlay (fe.h), linked at FRAME_BASE (mk/fw.mk). It also draws the
// pages of the title loop.

#include "doomdef.h"
#include "f_finale.h"

#include "fe.h"
#include "ovl.h"

OVL_TABLE(FINALE, OVL_ENTRY(FIN_E_START, fe_fn, F_OvlStart),
          OVL_ENTRY(FIN_E_DRAW, fe_fn, F_OvlDrawer),
          OVL_ENTRY(FIN_E_PAGE, fe_fn, F_OvlPage));
