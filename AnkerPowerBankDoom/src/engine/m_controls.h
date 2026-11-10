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

#ifndef __M_CONTROLS_H__
#define __M_CONTROLS_H__
 
extern const int key_right;
extern const int key_left;

extern const int key_up;
extern const int key_down;
extern const int key_strafeleft;
extern const int key_straferight;
extern const int key_fire;
extern const int key_use;
extern const int key_strafe;
extern int key_speed;	// the input layer sets it (always run)

extern const int key_message_refresh;
extern const int key_pause;

extern const int key_weapon1;
extern const int key_weapon2;
extern const int key_weapon3;
extern const int key_weapon4;
extern const int key_weapon5;
extern const int key_weapon6;
extern const int key_weapon7;
extern const int key_weapon8;

extern const int key_map_north;
extern const int key_map_south;
extern const int key_map_east;
extern const int key_map_west;
extern const int key_map_zoomin;
extern const int key_map_zoomout;
extern const int key_map_toggle;
extern const int key_map_maxzoom;
extern const int key_map_follow;
extern const int key_map_grid;
extern const int key_map_mark;
extern const int key_map_clearmark;

// menu keys:

extern const int key_menu_activate;
extern const int key_menu_up;
extern const int key_menu_down;
extern const int key_menu_left;
extern const int key_menu_right;
extern const int key_menu_back;
extern const int key_menu_forward;
extern const int key_menu_confirm;
extern const int key_menu_abort;

extern const int key_menu_help;
extern const int key_menu_volume;
extern const int key_menu_detail;
extern const int key_menu_endgame;
extern const int key_menu_messages;
extern const int key_menu_quit;

extern const int key_menu_incscreen;
extern const int key_menu_decscreen;

#endif /* #ifndef __M_CONTROLS_H__ */

