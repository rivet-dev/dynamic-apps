/* $Id$ */
/*-
 * Copyright (c) 2003      Olivier Fourdan <fourdan@xfce.org>
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

/**
 * SECTION: xfce-utf8
 * @title: Unicode Manipulation
 * @short_description: functions operating on Unicode characters and UTF-8 strings.
 * @see_also: https://developer.gnome.org/glib/stable/glib-Unicode-Manipulation.html
 *
 * Functions operating on Unicode characters and UTF-8 strings.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4util/libxfce4util-alias.h>



/**
 * xfce_utf8_remove_controls:
 * @str     : target string.
 * @max_len : max characters to check or -1 for no character limit.
 * @end     : pointer to the endpoint in @str or %NULL for no endpoint.
 *
 * Removes all control characters from @str up to @end or up to
 * @max_len characters (note that characters does not mean bytes with
 * UTF-8), where both @str and @max_len may not be given.
 *
 * Control characters are replaced in @str by whitespaces, no new string
 * will be allocated. The operation is done in-place.
 *
 * Return value: pointer to @str or %NULL on error.
 *
 * Since: 4.2
 **/
gchar*
xfce_utf8_remove_controls (gchar *str, gssize max_len, const gchar *end)
{
  gchar *p;

  g_return_val_if_fail (str != NULL, NULL);

  for (p = str; p != NULL && *p != '\0' && (!end || p < end) && (max_len < 0 || (p - str) < max_len); )
    {
      if ((*p > 0) && (*p < 32))
          *p = ' ';
      p = g_utf8_find_next_char (p, end);
    }

  return str;
}



/**
 * xfce_utf8_strndup:
 * @src     : target string.
 * @max_len : max characters to duplicate or -1 for no character limit.
 *
 * Duplicates the @src string up to @max_len characters
 * (note that characters does not mean bytes with UTF-8).
 *
 * The caller is responsible to free the returned string
 * using g_free() when no longer needed.
 *
 * Return value: pointer to the newly allocated string.
 *
 * Since: 4.3
 **/
gchar *
xfce_utf8_strndup (const gchar *src,
                   gssize       max_len)
{
  const gchar *s;

  if (max_len <= 0)
    return g_strdup (src);

  for (s = src; max_len > 0 && *s != '\0'; --max_len)
    s = g_utf8_next_char (s);

  return g_strndup (src, s - src);
}



#define __XFCE_UTF8_C__
#include <libxfce4util/libxfce4util-aliasdef.c>
