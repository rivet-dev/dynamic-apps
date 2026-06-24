/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2004      Jasper Huijsman <jasper@xfce.org>
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
 * SECTION: xfce-i18n
 * @title: Internationalization
 * @short_description: Internationalization and Localization Support Functions
 * @see_also: https://developer.gnome.org/glib/stable/glib-I18N.html
 *
 * Provides functions to aid application developers making their software
 * localizable. It extends the basic internationalization support provided
 * by GLib 2.4 (and newer).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4util/libxfce4util-alias.h>



/* fallback to g_strlcpy() if strlcpy() is not found */
#ifndef HAVE_STRLCPY
#define strlcpy(x,y,z)  g_strlcpy(x,y,z)
#endif

/* some platforms don't define PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* fallback locale */
#define DEFAULT_LOCALE  "C"



static gchar *xfce_localize_path_internal (gchar       *buffer,
                                           gsize        len,
                                           const gchar *path,
                                           GFileTest    test);



static gchar*
xfce_localize_path_internal (gchar       *buffer,
                             gsize        len,
                             const gchar *path,
                             GFileTest    test)
{
  static const gchar delim[] = { '.', '@', '_' };
  const gchar       *lang;
  gchar             *langext;
  gchar             *p;
  guint              n;

#ifdef HAVE_SETLOCALE
  lang = setlocale (LC_MESSAGES, NULL);
#else
  lang = NULL;
#endif

  if (G_UNLIKELY (lang == NULL))
    lang = getenv ("LANG");

  if (G_LIKELY (lang != NULL && strchr (path, G_DIR_SEPARATOR) != NULL))
    {
      /* ok, we will try four things here
       * - full locale name:         ll_LL@qualifier.encoding
       * - locale without encoding:  ll_LL@qualifier
       * - locale without qualifier: ll_LL
       * - base locale:              ll
       */
      g_snprintf (buffer, len, "%s.%s", path, lang);

      if (g_file_test (buffer, test))
        goto found;

      for (n = 0; n < G_N_ELEMENTS (delim); ++n)
        {
          p = strchr (lang, delim[n]);
          if (p != NULL)
            {
              langext = g_strndup (lang, p - lang);
              g_snprintf (buffer, len, "%s.%s", path, langext);
              g_free (langext);

              if (g_file_test (buffer, test))
                goto found;
            }
        }
    }

  strlcpy (buffer, path, len);

found:
  return buffer;
}



/**
 * xfce_textdomain:
 * @package   : the package name.
 * @localedir : the @package<!---->s locale directory.
 * @encoding  : the encoding to use the @package<!---->s translations
 *              or %NULL to use "UTF-8".
 *
 * Sets up the translations for @package.
 **/
void
xfce_textdomain (const gchar *package,
                 const gchar *localedir,
                 const gchar *encoding)
{
  g_return_if_fail (package != NULL);
  g_return_if_fail (localedir != NULL);

  /* make sure to set locale according to the environment variables */
#ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif

  /* bind the text domain for the package to the given directory */
  bindtextdomain (package, localedir);

  /* setup the encoding for the package (default to UTF-8) */
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  if (G_UNLIKELY (encoding == NULL))
    encoding = "UTF-8";
  bind_textdomain_codeset (package, encoding);
#endif

  /* set the package's domain as default */
  textdomain (package);
}



/**
 * xfce_get_file_localized:
 * @filename : name of a file to look for a localized version.
 *
 * Checks if theres a version of @filename which is localized to the current
 * locale. This is done by appending the full locale name to @filename, separated
 * by a '.'. If theres no file of that name, it retries using the full locale
 * name without the encoding (if any), then without the qualifier (if any) and
 * at last the base locale is tried. If all of those fails, a copy of @filename
 * is returned.
 *
 * Return value: path of the localized file or copy of @filename if no such
 *               file exists. Returned string should be freed using g_free().
 **/
gchar*
xfce_get_file_localized (const gchar *filename)
{
  gchar buffer[PATH_MAX + 1];

  g_return_val_if_fail (filename != NULL, NULL);

  return g_strdup (xfce_localize_path_internal (buffer, sizeof (buffer), filename, G_FILE_TEST_IS_REGULAR));
}



/**
 * xfce_get_file_localized_r:
 * @buffer   : destination buffer to store the localized filename to.
 * @length   : size of @buffer in bytes.
 * @filename : name of a file to look for a localized version.
 *
 * Similar in functionality to xfce_get_file_localized(), but stores the
 * result in @buffer instead of allocating a new buffer.
 *
 * Return value: pointer to @buffer or %NULL on error.
 **/
gchar*
xfce_get_file_localized_r (gchar *buffer, gsize length, const gchar *filename)
{
  g_return_val_if_fail (buffer != NULL, NULL);
  g_return_val_if_fail (filename != NULL, NULL);

  return xfce_localize_path_internal (buffer, length, filename, G_FILE_TEST_IS_REGULAR);
}



/**
 * xfce_get_dir_localized:
 * @directory : directory name to check for a localized variant.
 *
 * Similar to xfce_get_file_localized(), but works on directory instead of
 * a file.
 *
 * Return value: path of the localized directory name or copy of @directory if
 *               no such directory exists. Returned string should be freed using
 *               g_free().
 **/
gchar*
xfce_get_dir_localized (const gchar *directory)
{
  gchar buffer[PATH_MAX + 1];

  g_return_val_if_fail (directory != NULL, NULL);

  return g_strdup (xfce_localize_path_internal (buffer, sizeof (buffer), directory, G_FILE_TEST_IS_DIR));
}



/**
 * xfce_get_dir_localized_r:
 * @buffer    : destination buffer to store the localized filename to.
 * @length    : size of @buffer in bytes.
 * @directory : name of directory to check for localized variant of.
 *
 * Similar to #xfce_get_file_localized_r, but works on directory instead
 * of regular file.
 *
 * Return value: pointer to @buffer or %NULL on error.
 **/
gchar*
xfce_get_dir_localized_r (gchar *buffer, gsize length, const gchar *directory)
{
  g_return_val_if_fail (buffer != NULL, NULL);
  g_return_val_if_fail (directory != NULL, NULL);

  return xfce_localize_path_internal (buffer, length, directory, G_FILE_TEST_IS_DIR);
}



/**
 * xfce_get_path_localized:
 * @dst      : destination buffer.
 * @size     : size of @dst in bytes.
 * @paths    : the path
 * @filename : the filename
 * @test     : test
 *
 * @paths is a ':'-separated list of pathnames, with:
 *
 * - `%F`: the @filename
 * - `%L`: the language string, as returned by `setlocale(LC_MESSAGES, NULL)`
 * - `%l`: the language component of the language string
 * - `%N`: application name
 *
 * Example paths: `/usr/local/lib/%L/%F:/usr/local/share/%N/%l/%F`
 *
 */
gchar*
xfce_get_path_localized (gchar       *dst,
                         gsize        size,
                         const gchar *paths,
                         const gchar *filename,
                         GFileTest    test)
{
  static const gchar delim[] = { '.', '@', '_' };
  const gchar       *locale;
  const gchar       *f;
  const gchar       *p;
  gboolean           need_lang = FALSE;
  gchar             *dstlast = dst + (size - 1);
  gchar             *d = dst;
  gchar             *langext;
  gchar             *buffer;
  guint              n;

  g_return_val_if_fail (paths != NULL, NULL);
  g_return_val_if_fail (dst != NULL, NULL);
  g_return_val_if_fail (size > 2, NULL);

#ifdef HAVE_SETLOCALE
  locale = setlocale (LC_MESSAGES, NULL);
#else
  locale = NULL;
#endif

  /* allocate buffer space */
  buffer = g_newa (gchar, size);

  /* determine locale fallbacks */
  if (locale == NULL)
    {
      locale = g_getenv ("LANGUAGE");
      if (locale == NULL)
        {
          locale = g_getenv ("LANG");
          if (locale == NULL)
            locale = DEFAULT_LOCALE;
        }
    }

  while (d < dstlast)
    {
      if (*paths == ':' || *paths == '\0')
        {
          *d = '\0';

          if (need_lang)
            {
              /* ok, we will try four things here:
                 - ll_LL@qualifier.encoding
                 - ll_LL@qualifier
                 - ll_LL
                 - ll
               */
              g_snprintf (buffer, size, dst, locale);

              if (g_file_test(buffer, test))
                {
                  strncpy (dst, buffer, size);
                  return dst;
                }

              for (n = 0; n < G_N_ELEMENTS (delim); ++n)
                {
                  p = strchr (locale, delim[n]);
                  if (G_LIKELY (p != NULL))
                    {
                      langext = g_strndup (locale, p - locale);
                      g_snprintf (buffer, size, dst, langext);
                      g_free (langext);

                      if (g_file_test (buffer, test))
                        {
                          strncpy (dst, buffer, size);
                          return dst;
                        }
                    }
                }
            }
          else if (g_file_test (dst, test))
            {
              return dst;
            }

          if (*paths == ':')
            {
              d = dst;
              paths++;
              need_lang = FALSE;
              continue;
            }

          break;
        }

      if (paths[0] == '%')
        {
          if (paths[1] == 'F')
            {
              /*
               * if "filename" is NULL, then simply skip
               * the %F.
               */
              if (G_LIKELY (filename != NULL))
                {
                  for (f = filename; *f && d < dstlast; )
                    *d++ = *f++;
                }

              paths += 2;
              continue;
            }
          else if (paths[1] == 'L')
            {
              for (f = locale; *f && d < dstlast; )
                *d++ = *f++;

              paths += 2;
              continue;
            }
          else if (paths[1] == 'l')
            {
              if (d + 2 < dstlast)
                {
                  /* Ok if someone has a path with '%s' in it this will break.
                   * That should be against the law anyway IMO ;-)
                   */
                  *d++ = '%';
                  *d++ = 's';
                  need_lang = TRUE;
                }

              paths += 2;
              continue;
            }
          else if (paths[1] == 'N')
            {
              f = g_get_prgname ();
              if (G_LIKELY (f != NULL))
                {
                  while (*f && d < dstlast)
                    *d++ = *f++;
                }

              paths += 2;
              continue;
            }
        }

      *d++ = *paths++;
    }

  return NULL;
}



/**
 * xfce_locale_match:
 * @locale1 : the current locale value as returned by setlocale(LC_MESSAGES,%NULL).
 * @locale2 : the locale value to match against.
 *
 * The locale is of the general form LANG_COUNTRY.ENCODING @ MODIFIER, where
 * each of COUNTRY, ENCODING and MODIFIER can be absent.
 *
 * The match is done by actually removing the rightmost element one by one. This
 * is not entirely according to the freedesktop.org specification, but much easier.
 * Will probably be fixed in the future.
 *
 * Return value: an integer value indicating the level of matching, where
 *               the constant #XFCE_LOCALE_FULL_MATCH indicates a full match
 *               and #XFCE_LOCALE_NO_MATCH means no match. Every other value
 *               indicates a partial match, the higher the value, the better
 *               the match. You should not rely on any specific value besides
 *               the constants #XFCE_LOCALE_FULL_MATCH and #XFCE_LOCALE_NO_MATCH,
 *               since the range of returned values may change in the future.
 *
 * Since: 4.2
 **/
guint
xfce_locale_match (const gchar *locale1,
                   const gchar *locale2)
{
  g_return_val_if_fail (locale1 != NULL, XFCE_LOCALE_NO_MATCH);
  g_return_val_if_fail (locale2 != NULL, XFCE_LOCALE_NO_MATCH);

  while (*locale1 == *locale2 && *locale1 != '\0')
    {
      ++locale1;
      ++locale2;
    }

  if (*locale1 == '\0')
    {
      if (*locale2 == '\0')
        return XFCE_LOCALE_FULL_MATCH;

      /* FALL-THROUGH */
    }
  else if (*locale2 == '\0')
    {
      switch (*locale1)
        {
          case '@': return XFCE_LOCALE_NO_MATCH + 3;
          case '.': return XFCE_LOCALE_NO_MATCH + 2;
          case '_': return XFCE_LOCALE_NO_MATCH + 1;
        }

      /* FALL-THROUGH */
    }

  return XFCE_LOCALE_NO_MATCH;
}



#define __XFCE_I18N_C__
#include <libxfce4util/libxfce4util-aliasdef.c>
