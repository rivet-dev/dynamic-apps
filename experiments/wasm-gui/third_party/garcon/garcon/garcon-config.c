/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
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

#include <garcon/garcon-config.h>



/**
 * SECTION: garcon-config
 * @title: Version Information
 * @short_description: Information about the garcon version in use.
 * @include: garcon/garcon.h
 *
 * The garcon library provides version information, which could be used
 * by developers to handle new API.
 **/



/**
 * garcon_major_version:
 *
 * The major version number of the garcon library (e.g. in
 * version 4.8.0 this is 4).
 *
 * This variable is in the library, so represents the
 * garcon library you have linked against. Contrast with the
 * #GARCON_MAJOR_VERSION macro, which represents the major
 * version of the garcon headers you have included.
 **/
const guint garcon_major_version = GARCON_MAJOR_VERSION;

/**
 * garcon_minor_version:
 *
 * The minor version number of the garcon library (e.g. in
 * version 4.8.0 this is 8).
 *
 * This variable is in the library, so represents the
 * garcon library you have linked against. Contrast with the
 * #GARCON_MINOR_VERSION macro, which represents the minor
 * version of the garcon headers you have included.
 **/
const guint garcon_minor_version = GARCON_MINOR_VERSION;

/**
 * garcon_micro_version:
 *
 * The micro version number of the garcon library (e.g. in
 * version 4.8.0 this is 0).
 *
 * This variable is in the library, so represents the
 * garcon library you have linked against. Contrast with the
 * #GARCON_MICRO_VERSION macro, which represents the micro
 * version of the garcon headers you have included.
 **/
const guint garcon_micro_version = GARCON_MICRO_VERSION;




/**
 * garcon_check_version:
 * @required_major : the required major version.
 * @required_minor : the required minor version.
 * @required_micro : the required micro version.
 *
 * Checks that the <systemitem class="library">garcon</systemitem>
 * library in use is compatible with the given version. Generally you
 * would pass in the constants #GARCON_MAJOR_VERSION,
 * #GARCON_MINOR_VERSION and #GARCON_MICRO_VERSION as the three
 * arguments to this function; that produces a check that the library
 * in use is compatible with the version of
 * <systemitem class="library">garcon</systemitem> the application was
 * compiled against.
 *
 * <example>
 * <title>Checking the runtime version of the garcon library</title>
 * <programlisting>
 * const gchar *mismatch;
 * mismatch = garcon_check_version (GARCON_VERSION_MAJOR,
 *                                  GARCON_VERSION_MINOR,
 *                                  GARCON_VERSION_MICRO);
 * if (G_UNLIKELY (mismatch != NULL))
 *   g_error ("Version mismatch: %<!---->s", mismatch);
 * </programlisting>
 * </example>
 *
 * Returns: %NULL if the library is compatible with the given version,
 *          or a string describing the version mismatch. The returned
 *          string is owned by the library and must not be freed or
 *          modified by the caller.
 **/
const gchar*
garcon_check_version (guint required_major,
                      guint required_minor,
                      guint required_micro)
{
  return NULL;
}



/**
 * garcon_config_build_paths:
 * @filename: name of .desktop file
 *
 * Returns: (transfer full):
 */
gchar **
garcon_config_build_paths (const gchar *filename)
{
  const gchar * const *dirs;
  gchar              **paths;
  guint                n;

  g_return_val_if_fail (filename != NULL && *filename != '\0', NULL);

  dirs = g_get_system_config_dirs ();

  paths = g_new0 (gchar *, 1 + g_strv_length ((gchar **)dirs) + 1);
  
  paths[0] = g_build_filename (g_get_user_config_dir (), filename, NULL);
  for (n = 1; dirs[n-1] != NULL; ++n)
    paths[n] = g_build_filename (dirs[n-1], filename, NULL);
  paths[n] = NULL;

  return paths;
}



/**
 * garcon_config_lookup:
 * @filename : relative filename of the config resource.
 *
 * Looks for the filename in the users' config directory and then
 * the system config directories.
 *
 * Returns: the absolute path to the first file in the search path,
 *          that matches @filename or %NULL if no such
 *          file or directory could be found.
 **/
gchar *
garcon_config_lookup (const gchar *filename)
{
  const gchar * const *dirs;
  gchar               *path;
  guint                i;

  g_return_val_if_fail (filename != NULL && *filename != '\0', NULL);

  /* Look for the file in the user's config directory */
  path = g_build_filename (g_get_user_config_dir (), filename, NULL);
  if (g_path_is_absolute (path) && g_file_test (path, G_FILE_TEST_IS_REGULAR))
    return path;

  g_free (path);
  path = NULL;

  /* Look for the file in the system config directories */
  dirs = g_get_system_config_dirs ();
  for (i = 0; path == NULL && dirs[i] != NULL; ++i)
    {
      /* Build the filename, if the file exists return the path */
      path = g_build_filename (dirs[i], filename, NULL);
      if (g_path_is_absolute (path) && g_file_test (path, G_FILE_TEST_IS_REGULAR))
        return path;

      g_free (path);
      path = NULL;
    }

  /* Also try the install prefix of garcon in case XDG_CONFIG_DIRS is not
   * properly set (startxfce4 for exaple should take care of that) */
  path = g_build_filename (SYSCONFIGDIR, filename, NULL);
  if (g_path_is_absolute (path) && g_file_test (path, G_FILE_TEST_IS_REGULAR))
    return path;

  g_free (path);
  path = NULL;

  /* Return the path or NULL if the file could not be found */
  return path;
}
