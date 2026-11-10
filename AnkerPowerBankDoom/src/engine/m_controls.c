//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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

#include "doomtype.h"
#include "doomkeys.h"

//
// Keyboard controls
//

const int key_right = KEY_RIGHTARROW;
const int key_left = KEY_LEFTARROW;
const int key_up = KEY_UPARROW;
const int key_down = KEY_DOWNARROW; 
const int key_strafeleft = KEY_STRAFE_L;
const int key_straferight = KEY_STRAFE_R;
const int key_fire = KEY_FIRE;
const int key_use = KEY_USE;
const int key_strafe = KEY_RALT;
int key_speed = KEY_RSHIFT; 

const int key_message_refresh = KEY_ENTER;
const int key_pause = KEY_PAUSE;

// Weapon selection keys:

const int key_weapon1 = '1';
const int key_weapon2 = '2';
const int key_weapon3 = '3';
const int key_weapon4 = '4';
const int key_weapon5 = '5';
const int key_weapon6 = '6';
const int key_weapon7 = '7';
const int key_weapon8 = '8';

// Map control keys:

const int key_map_north     = KEY_UPARROW;
const int key_map_south     = KEY_DOWNARROW;
const int key_map_east      = KEY_RIGHTARROW;
const int key_map_west      = KEY_LEFTARROW;
const int key_map_zoomin    = '=';
const int key_map_zoomout   = '-';
const int key_map_toggle    = KEY_TAB;
const int key_map_maxzoom   = '0';
const int key_map_follow    = 'f';
const int key_map_grid      = 'g';
const int key_map_mark      = 'm';
const int key_map_clearmark = 'c';

// menu keys:

const int key_menu_activate  = KEY_ESCAPE;
const int key_menu_up        = KEY_UPARROW;
const int key_menu_down      = KEY_DOWNARROW;
const int key_menu_left      = KEY_LEFTARROW;
const int key_menu_right     = KEY_RIGHTARROW;
const int key_menu_back      = KEY_BACKSPACE;
const int key_menu_forward   = KEY_ENTER;
const int key_menu_confirm   = 'y';
const int key_menu_abort     = 'n';

const int key_menu_help      = KEY_F1;
const int key_menu_volume    = KEY_F4;
const int key_menu_detail    = KEY_F5;
const int key_menu_endgame   = KEY_F7;
const int key_menu_messages  = KEY_F8;
const int key_menu_quit      = KEY_F10;

const int key_menu_incscreen = KEY_EQUALS;
const int key_menu_decscreen = KEY_MINUS;
