/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>

#include <libxfce4util/libxfce4util.h>
#include <exo-desktop-item-edit/exo-die-utils.h>



/**
 * exo_die_g_key_file_set_locale_value:
 * @key_file : the #GKeyFile.
 * @group    : the group name.
 * @key      : the key name.
 * @value    : the new value.
 *
 * Stores @value localized for @key in @group if it
 * is already localized.
 **/
void
exo_die_g_key_file_set_locale_value (GKeyFile    *key_file,
                                     const gchar *group,
                                     const gchar *key,
                                     const gchar *value)
{
  const gchar * const *locale;
  gchar               *name;

  g_return_if_fail (key_file != NULL);
  g_return_if_fail (group != NULL);
  g_return_if_fail (key != NULL);
  g_return_if_fail (value != NULL);

  /* try localized first */
  for (locale = g_get_language_names (); *locale != NULL; ++locale)
    {
      name = g_strdup_printf ("%s[%s]", key, *locale);
      if (g_key_file_has_key (key_file, group, name, NULL))
        {
          g_key_file_set_string (key_file, group, name, value);
          g_free (name);
          break;
        }
    }

  /* fallback to unlocalized */
  if (G_UNLIKELY (*locale == NULL))
    g_key_file_set_string (key_file, group, key, value);
}



static void trust_launcher (GFile *gfile)
{
  /* trust the launcher since the user created it */
  guint32 mode = 0111, mask = 0111;
  guint32 old_mode, new_mode;
  GFileInfo *info;

  info = g_file_query_info (gfile,
                            G_FILE_ATTRIBUTE_UNIX_MODE,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            NULL);

  if (info == NULL)
    return;

  old_mode = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);
  new_mode = (old_mode & ~mask) | mode;

  if (old_mode != new_mode) {
    g_file_set_attribute_uint32 (gfile,
                                 G_FILE_ATTRIBUTE_UNIX_MODE, new_mode,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL,
                                 NULL);
  }

  g_object_unref (info);

  if (xfce_g_file_metadata_is_supported (gfile))
    xfce_g_file_set_trusted (gfile, TRUE, NULL, NULL);
}



/**
 * exo_die_g_key_file_save:
 * @key_file : the #GKeyFile.
 * @create   : whether to create.
 * @trust    : whether to trust the launcher.
 * @base     : file or folder (if @create).
 * @mode     : file mode for .directory or .desktop suffix.
 * @error    : return location for errors or %NULL.
 *
 * Writes changes to the @key_file .desktop or .directory file.
 *
 * Return value: %TRUE if successfull, %FALSE otherwise.
 **/
gboolean
exo_die_g_key_file_save (GKeyFile          *key_file,
                         gboolean           create,
                         gboolean           trust,
                         GFile             *base,
                         ExoDieEditorMode   mode,
                         GError           **error)
{
  GFileType    file_type;
  GFile       *file;
  gchar       *name, *s;
  gchar       *filename, *data;
  gsize        length;
  gboolean     result;
  guint        n;
  gboolean     desktop_suffix;
  const gchar *suffix;

  g_return_val_if_fail (G_IS_FILE (base), FALSE);
  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* check if we should create a new file */
  if (G_LIKELY (create))
    {
      if (mode == EXO_DIE_EDITOR_MODE_DIRECTORY)
        suffix = ".directory";
      else
        suffix = ".desktop";

      /* if the filename end with .desktop, then use the base as file */
      name = g_file_get_basename (base);
      desktop_suffix = g_str_has_suffix (name, suffix);
      g_free (name);
      if (desktop_suffix)
        {
          file = g_object_ref (base);
        }
      else
        {
          file_type = g_file_query_file_type (base, G_FILE_QUERY_INFO_NONE, NULL);
          if (file_type == G_FILE_TYPE_REGULAR)
            {
              file = g_object_ref (base);
            }
          else if (file_type == G_FILE_TYPE_DIRECTORY)
            {
              /* determine the desktop entry name */
              name = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                                   G_KEY_FILE_DESKTOP_KEY_NAME, NULL, NULL);
              if (G_UNLIKELY (name == NULL))
                name = g_strdup ("launcher");

              /* replace invalid file system characters */
              for (s = name; *s != '\0'; ++s)
                if (G_IS_DIR_SEPARATOR (*s) || *s == '.')
                  *s = '_';

              /* create a unique filename */
              filename = g_strconcat (name, suffix, NULL);
              file = g_file_get_child_for_display_name (base, filename, error);
              for (n = 0; file != NULL && g_file_query_exists (file, NULL); n++)
                {
                  /* release the previous name */
                  g_free (filename);
                  g_object_unref (file);

                  /* generate a new file name */
                  filename = g_strdup_printf ("%s%d%s", name, n, suffix);
                  file = g_file_get_child_for_display_name (base, filename, error);
                }

              /* cleanup */
              g_free (filename);
              g_free (name);

              if (G_UNLIKELY (file == NULL))
                return FALSE;
            }
          else
            {
              /* base is not a directory, cannot save */
              g_set_error_literal (error, G_FILE_ERROR, g_file_error_from_errno (ENOTDIR),
                                   _("File location is not a regular file or directory"));
              return FALSE;
            }
        }
    }
  else
    {
      /* base is the file */
      file = g_object_ref (base);
    }

  /* determine the data for the key file */
  data = g_key_file_to_data (key_file, &length, error);
  if (G_UNLIKELY (data == NULL))
    {
      g_object_unref (file);
      return FALSE;
    }

  /* need to recalculate checksum */
  trust = trust || xfce_g_file_is_trusted (file, NULL, NULL);

  result = g_file_replace_contents (file, data, length, NULL, FALSE,
                                    G_FILE_CREATE_NONE,
                                    NULL, NULL, error);

  if (trust)
    trust_launcher (file);

  /* cleanup */
  g_free (data);
  g_object_unref (file);

  return result;
}
