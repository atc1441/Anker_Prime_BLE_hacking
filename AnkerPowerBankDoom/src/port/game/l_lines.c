// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Artur Bieniek
//
// Entry table of the LINES overlay (l_lines.h), linked with the engine's line special
// objects at FRAME_BASE (mk/fw.mk).

#include "l_lines.h"
#include "ovl.h"
#include "p_spec.h"

OVL_TABLE(LINES, OVL_ENTRY(LINES_E_CROSS, lines_cross_fn, P_CrossSpecialLine),
          OVL_ENTRY(LINES_E_SHOOT, lines_shoot_fn, P_ShootSpecialLine),
          OVL_ENTRY(LINES_E_USE, lines_use_fn, P_UseSpecialLine));
