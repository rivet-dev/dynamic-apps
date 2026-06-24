/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/util.h for the Openbox window manager
   Copyright (c) 2007        Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#ifndef __obt_util_h
#define __obt_util_h

#include <glib.h>

#ifdef HAVE_STRING_H
#  include <string.h> /* for memset() */
#endif

G_BEGIN_DECLS

/* Util funcs */
#define obt_free g_free
#define obt_free0(p, type, num) memset((p), 0, sizeof(type) * (num)), g_free(p)

G_END_DECLS


#endif /*__obt_util_h*/
