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
//	Fixed point implementation.
//



#include "stdlib.h"

#include "doomtype.h"
#include "i_system.h"

#include "m_fixed.h"




// Fixme. __USE_C_FIXED__ or something.

fixed_t
FixedMul
( fixed_t	a,
  fixed_t	b )
{
    return ((int64_t) a * (int64_t) b) >> FRACBITS;
}



//
// FixedDiv, C version.
// The division is done without a 64-bit division helper. The quotient
// of (|a| << 16) / |b| is kept modulo 2^32, as the cast of the 64-bit
// result does. Its top 18 bits are zero when (|a| >> 14) < |b|, which
// holds for every a but INT_MIN, so only 30 steps are needed then.
//

static uint32_t UDivFix(uint32_t ua, uint32_t ub)
{
    uint32_t r, n, q = 0;
    int i;

    if ((ua >> 14) < ub)
    {
	r = ua >> 14;
	n = ua << 18;
	i = 30;
    }
    else
    {
	r = 0;
	n = ua;
	i = 48;
    }

    while (i--)
    {
	r = (r << 1) | (n >> 31);
	n <<= 1;
	q <<= 1;
	if (r >= ub)
	{
	    r -= ub;
	    q |= 1;
	}
    }

    return q;
}

fixed_t FixedDiv(fixed_t a, fixed_t b)
{
    if ((abs(a) >> 14) >= abs(b))
    {
	return (a^b) < 0 ? INT_MIN : INT_MAX;
    }
    else
    {
	uint32_t ua = a < 0 ? 0u - (uint32_t) a : (uint32_t) a;
	uint32_t ub = b < 0 ? 0u - (uint32_t) b : (uint32_t) b;
	uint32_t q = UDivFix(ua, ub);

	return (fixed_t) ((a^b) < 0 ? 0u - q : q);
    }
}

