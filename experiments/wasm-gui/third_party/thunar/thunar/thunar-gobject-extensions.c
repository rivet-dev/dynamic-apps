/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-gobject-extensions.h>

#include <gio/gio.h>



static void transform_string_to_boolean (const GValue *src, GValue *dst);
static void transform_string_to_enum    (const GValue *src, GValue *dst);
static void transform_string_to_int     (const GValue *src, GValue *dst);
static void transform_string_to_uint    (const GValue *src, GValue *dst);



static void
transform_string_to_boolean (const GValue *src,
                             GValue       *dst)
{
  g_value_set_boolean (dst, strcmp (g_value_get_string (src), "FALSE") != 0);
}



static void
transform_string_to_enum (const GValue *src,
                          GValue       *dst)
{
  GEnumClass *klass;
  gint        value = 0;
  guint       n;

  /* determine the enum value matching the src... */
  klass = g_type_class_ref (G_VALUE_TYPE (dst));
  for (n = 0; n < klass->n_values; ++n)
    {
      value = klass->values[n].value;
      if (g_strcmp0 (klass->values[n].value_name, g_value_get_string (src)) == 0)
        break;
    }
  g_type_class_unref (klass);

  /* ...and return that value */
  g_value_set_enum (dst, value);
}



static void
transform_string_to_int (const GValue *src,
                         GValue       *dst)
{
  g_value_set_int (dst, (gint) strtol (g_value_get_string (src), NULL, 10));
}



static void
transform_string_to_uint (const GValue *src,
                          GValue       *dst)
{
  g_value_set_uint (dst, (guint) strtoul (g_value_get_string (src), NULL, 10));
}



/**
 * thunar_g_initialize_transformations:
 *
 * Registers various transformation functions to the
 * GLib Type System, which are used by g_value_transform()
 * to transform between different kinds of values.
 **/
void
thunar_g_initialize_transformations (void)
{
  if (!g_value_type_transformable (G_TYPE_STRING, G_TYPE_BOOLEAN))
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_BOOLEAN, transform_string_to_boolean);
  if (!g_value_type_transformable (G_TYPE_STRING, G_TYPE_INT))
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_INT, transform_string_to_int);
  if (!g_value_type_transformable (G_TYPE_STRING, G_TYPE_UINT))
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UINT, transform_string_to_uint);

  /* register a transformation function string->enum unconditionally */
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_ENUM, transform_string_to_enum);
}



/**
 * thunar_g_strescape
 * @source  : The string to escape
 *
 * Similar to g_strescape, but as well escapes SPACE
 *
 * Escapes the special characters '\b', '\f', '\n', '\r', '\t', '\v', '\' ' ' and '"' in the string source by inserting a '\' before them.
 * Additionally all characters in the range 0x01-0x1F (SPACE and everything below)
 * and in the range 0x7F-0xFF (all non-ASCII chars) are replaced with a '\' followed by their octal representation.
 *
 * Return value: (transfer full): The new string. Has to be freed with g_free after usage.
 **/
gchar*
thunar_g_strescape (const gchar *source)
{
  gchar*       g_escaped;
  gchar*       result;
  unsigned int j = 0;
  unsigned int new_size = 0;

  /* First apply the default escaping .. will escape everything, expect SPACE */
  g_escaped = g_strescape (source, NULL);

  /* calc required new size */
  for (unsigned int i = 0; i < strlen (g_escaped); i++)
    {
      if (g_escaped[i] == ' ')
        new_size++;
      new_size++;
    }

  /* strlen() does not include the \0 character, add an extra slot for it */
  new_size++;
  result = malloc (new_size * sizeof (gchar));

  for (unsigned int i = 0; i < strlen (g_escaped); i++)
    {
      if (g_escaped[i] == ' ')
        {
          result[j] = '\\';
          j++;
        }
      result[j] = g_escaped[i];
      j++;
    }
  result[j] = '\0';
  g_free (g_escaped);
  return result;
}



/**
 * thunar_g_utf8_normalize_for_search
 * @str               : The string to normalize, zero-terminated.
 * @strip_diacritics  : Remove diacritics, leaving only base characters.
 * @casefold          : Fold case, to ignore letter case distinctions.
 *
 * Canonicalize a UTF-8 string into a form suitable for substring
 * matching against other strings passed through this function with the
 * same options. The strings produced by this function cannot be
 * transformed back into the original string. They can however be
 * matched against each other with functions like g_strrstr() that
 * aren't Unicode-aware.
 *
 * The implementation is currently not locale-aware, and relies only on
 * what GLib can do. It may change, so these strings should not be
 * persisted to disk and reused later.
 *
 * Do not use these strings for sorting. Use g_utf8_collate_key()
 * instead.
 *
 * Like g_utf8_normalize(), this returns NULL if the string is not valid
 * UTF-8.
 *
 * Return value: (transfer full): The normalized string, or NULL. Free non-NULL values with g_free() after use.
 **/

gchar*
thunar_g_utf8_normalize_for_search (const gchar *str,
                                    gboolean     strip_diacritics,
                                    gboolean     casefold)
{
  gchar *normalized;
  gchar *folded;

  /* g_utf8_normalize() and g_utf8_next_char() both require valid UTF-8 */
  if (g_utf8_validate (str, -1, NULL) == FALSE)
    return NULL;

  /* Optionally remove combining characters as a crude way of doing diacritic stripping */
  if (strip_diacritics == TRUE)
    {
      /* decompose composed characters to <base-char + combining-marks> */
      gchar *tmp = g_utf8_normalize (str, -1, G_NORMALIZE_DEFAULT);

      /* Discard the combining characters/marks/accents */
      /* GStrings keep track of where their ends are, so it's efficient */
      GString *stripped = g_string_sized_new (strlen (tmp));
      gunichar c;
      for (gchar *i = tmp; *i != '\0'; i = g_utf8_next_char (i))
        {
          c = g_utf8_get_char (i);
          if (G_LIKELY (g_unichar_combining_class (c) == 0))
            g_string_append_unichar (stripped, c);
          /* We can limit it by Unicode block here if this turns out to be
           * too aggressive and makes searches work badly for particular
           * scripts */
        }
      g_free (tmp);

      tmp = g_string_free (stripped, FALSE); /* transfers buffer ownership */
      normalized = g_utf8_normalize (tmp, -1, G_NORMALIZE_ALL_COMPOSE);
      g_free (tmp);
    }
  else  /* strip_diacritics == FALSE */
    {
      normalized = g_utf8_normalize (str, -1, G_NORMALIZE_ALL_COMPOSE);
    }

  /* String representation is now normalized the way we want it:
   *
   * 1. Combining chars have been stripped off if requested.
   *
   * 2. Compatibility chars like SUPERSCRIPT THREE are collapsed to
   *    their standard representations (DIGIT THREE, in this case).
   *
   * 3. If any combining chars are left, they are now composed back onto
   *    their base char. Otherwise a single LATIN LETTER A in a pattern
   *    wpould match LATIN LETTER A followed by COMBINING GRAVE ACCENT.
   */

  /* permit case-insensitive matching, if required */
  if (casefold == FALSE)
    return normalized;

  /* remove case distinctions (not locale aware) */
  folded = g_utf8_casefold (normalized, strlen (normalized));
  g_free (normalized);
  return folded;
}



/**
 * thunar_g_app_info_equal
 * @appinfo1  : The first g_app_info object
 * @appinfo2  : The second g_app_info object
 *
 * For unknown reason "g_app_info_equal" does weird stuff / crashes thunar in some cases.
 * (select two files of the same type + Sent to --> mail recipient )
 * So we use this trivial method to compare applications for now.
 *
 * Return value: : TRUE if appinfo1 is equal to appinfo2. FALSE otherwise.
 **/
gboolean
thunar_g_app_info_equal (gpointer appinfo1,
                         gpointer appinfo2)
{
  return g_utf8_collate (g_app_info_get_name (appinfo1),
                         g_app_info_get_name (appinfo2)) == 0;
}

/**
 * thunar_g_object_set_guint_data
 * @object  : The #GObject to set
 * @key     : key for which the data should be set
 * @data    : guint value to set as data
 *
 * Since it is not possible to set a plain uint to a G_OBJECT, we need to use a pointer
 * This helper method encapsulates the process of doing so
 **/
void
thunar_g_object_set_guint_data (GObject     *object,
                                const gchar *key,
                                guint        data)
{
  guint *data_ptr;

  data_ptr = g_malloc (sizeof (gint));
  *data_ptr = data;
  g_object_set_data_full (object, key, data_ptr, g_free);
}
