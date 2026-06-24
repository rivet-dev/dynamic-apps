/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2009 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>

#include <garcon/garcon-private.h>



static gboolean
garcon_looks_like_an_uri (const gchar *string)
{
  const gchar *s = string;

  if (G_UNLIKELY (string == NULL))
    return FALSE;

  /* <scheme> starts with an alpha character */
  if (g_ascii_isalpha (*s))
    {
      /* <scheme> continues with (alpha | digit | "+" | "-" | ".")* */
      for (++s; g_ascii_isalnum (*s) || *s == '+' || *s == '-' || *s == '.'; ++s);

      /* <scheme> must be followed by ":" */
      return (*s == ':');
    }

  return FALSE;
}



GFile *
_garcon_file_new_for_unknown_input (const gchar *path,
                                    GFile       *parent)
{
  g_return_val_if_fail (path != NULL, NULL);

  if (g_path_is_absolute (path))
    return g_file_new_for_path (path);

  if (garcon_looks_like_an_uri (path))
    return g_file_new_for_uri (path);

  if (G_LIKELY (parent != NULL))
    return g_file_resolve_relative_path (parent, path);
  else
    return g_file_new_for_path (path);
}



GFile *
_garcon_file_new_relative_to_file (const gchar *path,
                                   GFile       *file)
{
  GFileType type;
  GFile    *result;
  GFile    *dir;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  type = g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL);

  if (G_UNLIKELY (type == G_FILE_TYPE_DIRECTORY))
    dir = g_object_ref (file);
  else
    dir = g_file_get_parent (file);

  result = _garcon_file_new_for_unknown_input (path, dir);
  g_object_unref (dir);

  return result;
}



gchar *
_garcon_file_get_uri_relative_to_file (const gchar *path,
                                       GFile       *file)
{
  GFile *absolute_file;
  gchar *uri;

  absolute_file = _garcon_file_new_relative_to_file (path, file);
  uri = g_file_get_uri (absolute_file);
  g_object_unref (absolute_file);

  return uri;
}
