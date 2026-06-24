/*-
 * Copyright (c) 2004-2007 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <exo/exo-string.h>
#include <exo/exo-alias.h>

/**
 * SECTION: exo-string
 * @title: String Utility Functions
 * @short_description: Various string-related functions
 * @include: exo/exo.h
 *
 * This section describes a number of utility functions for
 * manipulating strings.
 **/



/**
 * exo_str_elide_underscores:
 * @text : A zero terminated string.
 *
 * Returns a copy of @text with all mnemonic underscores
 * stripped off.
 *
 * Deprecated: xfce 4.18: Unused within xfce project
 *
 * Returns: A copy of @text without underscores. The returned string
 *          must be freed when no longer required.
 **/
gchar*
exo_str_elide_underscores (const gchar *text)
{
  const gchar *s;
  gboolean     last_underscore = FALSE;
  gchar       *result;
  gchar       *t;

  g_return_val_if_fail (text != NULL, NULL);

  result = g_malloc (strlen (text) + 1);

  for (s = text, t = result; *s != '\0'; ++s)
    if (!last_underscore && *s == '_')
      {
        last_underscore = TRUE;
      }
    else
      {
        last_underscore = FALSE;
        *t++ = *s;
      }

  *t = '\0';

  return result;
}



/**
 * exo_str_is_equal:
 * @a : A pointer to first string or %NULL.
 * @b : A pointer to second string or %NULL.
 *
 * %NULL-safe string comparison. Returns %TRUE if both @a and @b are
 * %NULL or if @a and @b refer to valid strings which are equal.
 *
 * You should always prefer this function over strcmp().
 *
 * Deprecated: xfce 4.18: In favor of g_strcmp0()
 *
 * Returns: %TRUE if @a equals @b, else %FALSE.
 **/
gboolean
exo_str_is_equal (const gchar *a,
                  const gchar *b)
{
  if (a == NULL || b == NULL)
    return (a == b);

  while (*a == *b++)
    if (*a++ == '\0')
      return TRUE;

  return FALSE;
}



/**
 * exo_str_replace:
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
 * Deprecated: xfce 4.18: Replaced by xfce_str_replace()
 *
 * Returns: a newly allocated copy of @str where all occurances of
 *          @pattern are replaced with @replacement. Or %NULL if
 *          @str and/or @pattern is %NULL.
 *
 * Since: 0.3.1.1
 **/
gchar*
exo_str_replace (const gchar *str,
                 const gchar *pattern,
                 const gchar *replacement)
{
  const gchar *s, *p;
  GString     *result;

  /* an empty string or pattern is useless, so just
   * return a copy of str */
  if (G_UNLIKELY (exo_str_is_empty (str)
      || exo_str_is_empty (pattern)))
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
              if (G_LIKELY (!exo_str_is_empty (replacement)))
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
 * exo_strdup_strftime:
 * @format : format string to pass to strftime(3). See the strftime(3) documentation
 *           for details.
 * @tm     : date/time, in struct tm format.
 *
 * Cover for standard date-and-time-formatting routine strftime that returns
 * a newly-allocated string of the correct size. The caller is responsible
 * to free the returned string using g_free() when no longer needed.
 *
 * Besides the buffer management, there are two differences between this
 * and the library strftime:
 *
 * The modifiers "-" and "_" between a "%" and a numeric directive
 * are defined as for the GNU version of strftime. "-" means "do not
 * pad the field" and "_" means "pad with spaces instead of zeroes".
 *
 * Non-ANSI extensions to strftime are flagged at runtime with a
 * warning, so it's easy to notice use of the extensions without
 * testing with multiple versions of the library.
 *
 * Do NOT pass in the value returned by localtime(3) as the parameter @tm, as
 * this is a pointer to a shared static struct which can be changed by subsequent
 * function calls, including any calls to g_warning() made by exo_strdup_strftime()
 * itself. Use e.g. localtime_r(3) or make a copy of the struct to pass in instead.
 *
 * Deprecated: xfce 4.18: In favor of g_date_time_format()
 *
 * Returns: a newly allocated string containing the formatted date/time.
 *
 * Since: 0.3.3
 **/
gchar*
exo_strdup_strftime (const gchar     *format,
                     const struct tm *tm)
{
  static const gchar C_STANDARD_STRFTIME_CHARACTERS[] = "aAbBcCdeFgGhHIjklmMnprRsStTuUVwWxXyYzZ";
  static const gchar C_STANDARD_NUMERIC_STRFTIME_CHARACTERS[] = "CdegGHIjklmMsSuUVwWyY";
  static const gchar SUS_EXTENDED_STRFTIME_MODIFIERS[] = "EO";
  const gchar       *remainder;
  const gchar       *percent;
  gboolean           strip_leading_zeros;
  gboolean           turn_leading_zeros_to_spaces;
  GString           *string;
  gsize              string_length;
  gchar              code[4];
  gchar              buffer[512];
  gchar             *piece;
  gchar             *result;
  gchar             *converted;
  gchar              modifier = 0;
  gint               i;

  /* Format could be translated, and contain UTF-8 chars,
   * so convert to locale encoding which strftime uses.
   */
  converted = g_locale_from_utf8 (format, -1, NULL, NULL, NULL);
  if (G_UNLIKELY (converted == NULL))
    return NULL;

  /* start processing the format */
  string = g_string_new ("");
  remainder = converted;

  /* walk from % character to % character */
  for (;;)
    {
      /* look out for the next % character */
      percent = strchr (remainder, '%');
      if (percent == NULL)
        {
          /* we're done with the format */
          g_string_append (string, remainder);
          break;
        }

      /* append the characters in between */
      g_string_append_len (string, remainder, percent - remainder);

      /* handle the "%" character */
      remainder = percent + 1;
      switch (*remainder)
        {
        case '-':
          strip_leading_zeros = TRUE;
          turn_leading_zeros_to_spaces = FALSE;
          remainder++;
          break;

        case '_':
          strip_leading_zeros = FALSE;
          turn_leading_zeros_to_spaces = TRUE;
          remainder++;
          break;

        case '%':
          g_string_append_c (string, '%');
          remainder++;
          continue;

        case '\0':
          g_warning ("Trailing %% passed to exo_strdup_strftime");
          g_string_append_c (string, '%');
          continue;

        default:
          strip_leading_zeros = FALSE;
          turn_leading_zeros_to_spaces = FALSE;
          break;
        }

      if (strchr (SUS_EXTENDED_STRFTIME_MODIFIERS, *remainder) != NULL)
        {
          modifier = *remainder++;
          if (*remainder == 0)
            {
              g_warning ("Unfinished %%%c modifier passed to exo_strdup_strftime", modifier);
              break;
            }
        }

      if (strchr (C_STANDARD_STRFTIME_CHARACTERS, *remainder) == NULL)
        g_warning ("exo_strdup_strftime does not support non-standard escape code %%%c", *remainder);

      /* convert code to strftime format. We have a fixed
       * limit here that each code can expand to a maximum
       * of 512 bytes, which is probably OK. There's no
       * limit on the total size of the result string.
       */
      i = 0;
      code[i++] = '%';
#ifdef HAVE_STRFTIME_EXTENSION
      if (modifier != 0)
        code[i++] = modifier;
#endif
      code[i++] = *remainder;
      code[i++] = '\0';
      string_length = strftime (buffer, sizeof (buffer), code, tm);
      if (string_length == 0)
        {
          /* we could put a warning here, but there's no
           * way to tell a successful conversion to
           * empty string from a failure
           */
          buffer[0] = '\0';
        }

      /* strip leading zeros if requested */
      piece = buffer;
      if (strip_leading_zeros || turn_leading_zeros_to_spaces)
        {
          if (strchr (C_STANDARD_NUMERIC_STRFTIME_CHARACTERS, *remainder) == NULL)
            g_warning ("exo_strdup_strftime does not support modifier for non-numeric escape code %%%c%c", remainder[-1], *remainder);

          if (*piece == '0')
            {
              while (*++piece == '\0');
              if (!g_ascii_isdigit (*piece))
                --piece;
            }

          if (turn_leading_zeros_to_spaces)
            {
              memset (buffer, ' ', piece - buffer);
              piece = buffer;
            }
        }

      /* advance */
      ++remainder;

      /* add this piece */
      g_string_append (string, piece);
    }

  /* Convert the string back into UTF-8 */
  result = g_locale_to_utf8 (string->str, -1, NULL, NULL, NULL);

  /* cleanup */
  g_string_free (string, TRUE);
  g_free (converted);

  return result;
}



/**
 * exo_strndupv:
 * @strv  : String vector to duplicate.
 * @num   : Number of strings in @strv to duplicate.
 *
 * Creates a new string vector containing the first @n elements
 * of @strv. If called on a %NULL value or @num is 0, exo_strndupv()
 * simply returns %NULL.
 *
 * Deprecated: xfce 4.18: Unused within xfce project
 *
 * Returns: A new NULL-terminated array of strings or %NULL.
 *          Should be freed using g_strfreev() when no longer needed.
 **/
gchar**
exo_strndupv (gchar **strv,
              guint   num)
{
  gchar **result;
  guint   i;

  /* return null when there is nothing to copy */
  if (G_UNLIKELY (strv == NULL || num == 0))
    return NULL;

  /* duplicate the first @num string */
  result = g_new (gchar *, num + 1);
  for (i = 0; i < num && strv[i] != NULL; i++)
    result[i] = g_strdup (strv[i]);
  result[i] = NULL;

  /* resize the string if we allocated too much space */
  if (G_UNLIKELY (num > i))
    result = g_renew (gchar *, result, i + 1);

  return result;
}



/**
 * exo_str_looks_like_an_uri:
 * @str : an input string.
 *
 * Check if @str looks like an uri. This function is no guarantee that
 * the uri exists, or is supported by the system.
 *
 * Deprecated: xfce 4.18: In favor of g_uri_is_valid()
 *
 * Returns: %TRUE if the @str looks like an URI
 *          according to RFC 2396, %FALSE otherwise.
 *
 * Since: 0.5.0
 **/
gboolean
exo_str_looks_like_an_uri (const gchar *str)
{
  const gchar *s = str;

  if (G_UNLIKELY (str == NULL))
    return FALSE;

  /* <scheme> starts with an alpha character */
  if (g_ascii_isalpha (*s))
    {
      /* <scheme> continues with (alpha | digit | "+" | "-" | ".")* */
      for (++s; g_ascii_isalnum (*s) || *s == '+' || *s == '-' || *s == '.'; ++s);

      /* <scheme> must be followed by ":" */
      return (*s == ':' && *(s+1) != '\0');
    }

  return FALSE;
}



/**
 * exo_str_is_flag:
 * @str : an input string.
 *
 * Check if @str looks like a commandline flag. This function simply
 * checks if the string begins with a single dash.
 *
 * Deprecated: xfce 4.18: In favor of g_str_has_prefix()
 *
 * Returns: %TRUE if the @str looks like a flag, %FALSE otherwise.
 *
 * Since: 0.11.5
 **/
gboolean
exo_str_is_flag (const gchar *str)
{
  if (G_UNLIKELY (str == NULL))
    return FALSE;

  return g_str_has_prefix (str, "-");
}



#define __EXO_STRING_C__
#include <exo/exo-aliasdef.c>
