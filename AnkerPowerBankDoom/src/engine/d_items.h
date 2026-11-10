//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// Modified 2026 for the DMC-100 port.
//
// DESCRIPTION:
//	Items: key cards, artifacts, weapon, ammunition.
//


#ifndef __D_ITEMS__
#define __D_ITEMS__

#include "doomdef.h"

#include "info.h"


// Weapon info: sprite frames, ammunition use. The table is generated
// (gen/info.c) with the fields of vanilla weaponinfo_t: ammo, upstate,
// downstate, readystate, atkstate and flashstate.
typedef dmc_weaponinfo_t	weaponinfo_t;

#define weaponinfo	dmc_weaponinfo

#endif
