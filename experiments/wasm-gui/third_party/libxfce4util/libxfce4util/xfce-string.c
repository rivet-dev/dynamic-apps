/*
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

/**
 * SECTION:xfce-string
 * @title: Xfce Cstring/GString Functions
 * @short_description: various functions to handle string
 * @include: libxfce4util/libxfce4util.h
 *
 * Since: 4.17
 **/



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4util/libxfce4util-alias.h>



/**
 * xfce_str_replace:
 * @str         : the input string.
 * @pattern     : a search pattern in @str.
 * @replacement : replacement string for @pattern.
 *
 * Searches @str for occurances of @pattern and replaces each
 * such occurance with @replacement. Returns a newly allocated
 * copy of @str on which the given replacement were performed.
 * The caller is responsible to free the returned string using
 * g_free() when no longer needed.
 *
 * Note that @pattern and @replacement don't need to be of the
 * same size. If @replacement is %NULL, the pattern will be
 * removed from the string.
 *
 * Note for future Xfce developers: Deprecate this function when
 * g_string_replace() is available. (Added since Glib >= 2.68)
 *
 * Returns: (nullable) (transfer full):
 *          a newly allocated copy of @str where all occurrences of
 *          @pattern are replaced with @replacement. Or %NULL if
 *          @str is %NULL.
 *
 * Since: 4.17
 **/
gchar*
xfce_str_replace (const gchar *str,
                  const gchar *pattern,
                  const gchar *replacement)
{
  const gchar *s, *p;
  GString     *result;

  /* an empty string or pattern is useless, so just
   * return a copy of str */
  if (G_UNLIKELY (xfce_str_is_empty (str)
               || xfce_str_is_empty (pattern)))
    return g_strdup (str);

  /* allocate the result string */
  result = g_string_sized_new (strlen (str));

  /* process the input string */
  while (*str != '\0')
    {
      if (G_UNLIKELY (*str == *pattern))
        {
          /* compare the pattern to the current string */
          for (p = pattern + 1, s = str + 1; *p == *s; ++s, ++p)
            if (*p == '\0' || *s == '\0')
              break;

          /* check if the pattern fully matched */
          if (G_LIKELY (*p == '\0'))
            {
              if (G_LIKELY (!xfce_str_is_empty (replacement)))
                g_string_append (result, replacement);
              str = s;
              continue;
            }
        }

      g_string_append_c (result, *str++);
    }

  return g_string_free (result, FALSE);
}


/**
 * xfce_g_string_append_quoted:
 * @string: A #GString.
 * @unquoted: A literal string.
 *
 * Quotes a string @unquoted and appends to an existing
 * #GString @string. The shell will interpret the quoted string
 * to mean @unquoted. The quoting style used is undefined
 * (check g_shell_quote ()).
 *
 * Since: 4.17
 **/
void
xfce_g_string_append_quoted (GString     *string,
                             const gchar *unquoted)
{
  gchar *quoted;

  quoted = g_shell_quote (unquoted);
  g_string_append (string, quoted);
  g_free (quoted);
}

#define __XFCE_STRING_C__
#include <libxfce4util/libxfce4util-aliasdef.c>
