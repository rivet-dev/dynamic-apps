
/*-
 * Copyright (c) 2022 Yongha Hwang <mshrimp@sogang.ac.kr>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#if !defined(_LIBXFCE4UTIL_INSIDE_LIBXFCE4UTIL_H) && !defined(LIBXFCE4UTIL_COMPILATION)
#error "Only <libxfce4util/libxfce4util.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __XFCE_STRING_H__
#define __XFCE_STRING_H__

#include <glib.h>
#include <gio/gio.h>



void     xfce_g_string_append_quoted (GString      *string,
                                      const gchar  *unquoted);

gchar*   xfce_str_replace            (const gchar *str,
                                      const gchar *pattern,
                                      const gchar *replacement);



/**
 * xfce_str_is_empty:
 * @string : a string
 *
 * Macro to check if a string is %NULL or empty. You should prefer
 * this function over strlen (str) == 0.
 *
 * Returns: %TRUE if the string is %NULL or empty, %FALSE otherwise.
 *
 * Since : 4.17
 **/
#define xfce_str_is_empty(string) ((string) == NULL || *(string) == '\0')

/**
 * I_:
 * @string : A static string.
 *
 * Shortcut for g_intern_static_string() to return a
 * canonical representation for @string.
 *
 * Returns: a canonical representation for the string.
 *
 * Since : 4.17
 **/
#define I_(string) (g_intern_static_string ((string)))



#endif /* __XFCE_STRING_H__ */
