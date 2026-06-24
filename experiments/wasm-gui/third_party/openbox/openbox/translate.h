/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   translate.h for the Openbox window manager
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

#ifndef ob__translate_h
#define ob__translate_h

#include <glib.h>

gboolean translate_button(const gchar *str, guint *state, guint *keycode);
gboolean translate_key(const gchar *str, guint *state, guint *keycode);

void RrImageSetDestroyFunc(RrImage *image, RrImageDestroyFunc func);

#endif
