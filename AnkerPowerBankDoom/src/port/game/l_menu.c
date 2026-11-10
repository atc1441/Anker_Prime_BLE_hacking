// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Entry table of the MENU overlay (fe.h), linked at FRAME_BASE (mk/fw.mk).

#include "doomdef.h"
#include "m_menu.h"

#include "fe.h"
#include "ovl.h"

OVL_TABLE(MENU, OVL_ENTRY(MENU_E_RESPONDER, menu_resp_fn, M_OvlResponder),
          OVL_ENTRY(MENU_E_DRAW, fe_fn, M_OvlDrawer),
          OVL_ENTRY(MENU_E_OPEN, fe_fn, M_OvlOpen));
