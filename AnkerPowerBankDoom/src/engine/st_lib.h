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
// 	The status bar widget code.
//

#ifndef __STLIB__
#define __STLIB__


// We are referring to patches.
#include "r_defs.h"

//
// Typedefs of widgets
//
// The widgets are constant: their places, values and pictures do not change
// in a single player game, so they stay in flash. What a widget last drew is
// the st_state_t it points to.

typedef struct
{
    // last number, icon or value drawn
    short	old;

    // multicon: the last draw erased the previous icon first
    // (status band compose)
    short	erased;

} st_state_t;


// Number widget

typedef struct
{
    // upper right-hand corner
    //  of the number (right-justified)
    short	x;
    short	y;

    // max # of digits in number
    short width;    

    // pointer to current value
    int*	num;

    // pointer to boolean stating
    //  whether to update number
    boolean*	on;

    // list of patches for 0-9, as UI ids (DMC_UI_*)
    const unsigned char*	p;

    // last number value
    st_state_t*	s;
    
} st_number_t;



// Percent widget ("child" of number widget,
//  or, more precisely, contains a number widget.)
typedef struct
{
    // number information
    st_number_t		n;

    // percent sign graphic, a UI id
    int		p;
    
} st_percent_t;



// Multiple Icon widget
typedef struct
{
     // center-justified location of icons
    short		x;
    short		y;

    // pointer to current icon
    int*		inum;

    // pointer to boolean stating
    //  whether to update icon
    boolean*		on;

    // list of icons, as UI ids
    const unsigned char*	p;

    // last icon number, and whether its draw erased
    st_state_t*		s;

} st_multicon_t;




// Binary Icon widget

typedef struct
{
    // center-justified location of icon
    short		x;
    short		y;

    // pointer to current icon status
    boolean*		val;

    // pointer to boolean
    //  stating whether to update icon
    boolean*		on;  


    int		p;	// icon, a UI id

    // last icon value
    st_state_t*		s;
    
} st_binicon_t;



//
// Widget access and update routines
//

// The state of a widget before its first draw: number 0, no icon.
void STlib_initNum(const st_number_t* n);
void STlib_initMultIcon(const st_multicon_t* mi);
void STlib_initBinIcon(const st_binicon_t* bi);

// Number widget routines
void
STlib_updateNum
( const st_number_t*	n,
  boolean		refresh );


// Percent widget routines
void
STlib_updatePercent
( const st_percent_t*	per,
  int			refresh );


// Multiple Icon widget routines
void
STlib_updateMultIcon
( const st_multicon_t*	mi,
  boolean		refresh );


// Binary Icon widget routines
void
STlib_updateBinIcon
( const st_binicon_t*	bi,
  boolean		refresh );

#endif
