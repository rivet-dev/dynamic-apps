/* GdkPixbuf library - Compatibility Math Header for
 *                     pre-Visual Studio 2013 Headers
 *                     to Build GDK-Pixbuf
 *
 * Copyright (C) 2014 Chun-wei Fan
 *
 * Authors: Chun-wei Fan <fanc999@yahoo.com.tw>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MSC_VER
#error "This Header is intended for Visual Studio Builds only"
#endif

/* Include the stock Visual Studio math.h first */
#include <../include/math.h>

/* Visual Studio 2013 and later do not need this compatibility item */
#if (_MSC_VER < 1800)

#ifndef __MSVC_MATH_COMPAT_H__
#define __MSVC_MATH_COMPAT_H__

/* Note: This is a rather generic fallback implementation for round(), which is
 *       okay for the purposes for GTK+, but please note the following if intending
 *       use this implementation:
 *
 *       -The largest floating point value strictly less than 0.5.  The problem is
 *        that the addition produces 1 due to rounding.
 *
 *       -A set of large integers near 2^52 for which adding 0.5 is the same as
 *        adding 1, again due to rounding.
 */

static __inline double
round (double x)
{
  if (x >= 0)
    return floor (x + 0.5);
  else
    return ceil (x - 0.5);
}

#endif /* __MSVC_MATH_COMPAT_H__ */

#endif /* _MSC_VER < 1800 */