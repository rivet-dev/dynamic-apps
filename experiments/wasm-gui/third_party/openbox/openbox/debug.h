/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   debug.h for the Openbox window manager
   Copyright (c) 2003-2007   Dana Jansens

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

#ifndef __ob__debug_h
#define __ob__debug_h

#include <glib.h>

void ob_debug_startup(void);
void ob_debug_shutdown(void);

void ob_debug(const gchar *a, ...);

typedef enum {
    OB_DEBUG_NORMAL,
    OB_DEBUG_FOCUS,
    OB_DEBUG_APP_BUGS,
    OB_DEBUG_SM,
    OB_DEBUG_TYPE_NUM
} ObDebugType;

void ob_debug_type(ObDebugType type, const gchar *a, ...);

void ob_debug_enable(ObDebugType type, gboolean enable);

void ob_debug_show_prompts(void);

#endif
