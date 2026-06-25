/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>

#ifdef HAVE_GIO_UNIX
#include <gio/gunixmounts.h>
#include <gio/gdesktopappinfo.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <thunar/thunar-file.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>



/* See : https://freedesktop.org/wiki/Specifications/icon-naming-spec/ */
static struct
{
  const gchar *icon_name;
  const gchar *type;
}
device_icon_name [] =
{
  /* Implementation specific */
  { "multimedia-player-apple-ipod-touch" , N_("iPod touch") },
  { "computer-apple-ipad"                , N_("iPad") },
  { "phone-apple-iphone"                 , N_("iPhone") },
  { "drive-harddisk-solidstate"          , N_("Solid State Drive") },
  { "drive-harddisk-system"              , N_("System Drive") },
  { "drive-harddisk-usb"                 , N_("USB Drive") },
  { "drive-removable-media-usb"          , N_("USB Drive") },

  /* Freedesktop icon-naming-spec */
  { "camera*"                , N_("Camera") },
  { "drive-harddisk*"        , N_("Harddisk") },
  { "drive-optical*"         , N_("Optical Drive") },
  { "drive-removable-media*" , N_("Removable Drive") },
  { "media-flash*"           , N_("Flash Drive") },
  { "media-floppy*"          , N_("Floppy") },
  { "media-optical*"         , N_("Optical Media") },
  { "media-tape*"            , N_("Tape") },
  { "multimedia-player*"     , N_("Multimedia Player") },
  { "pda*"                   , N_("PDA") },
  { "phone*"                 , N_("Phone") },
  { NULL                     , NULL }
};



static const gchar *guess_device_type_from_icon_name (const gchar * icon_name);



GFile *
thunar_g_file_new_for_home (void)
{
  return g_file_new_for_path (xfce_get_homedir ());
}



GFile *
thunar_g_file_new_for_root (void)
{
  return g_file_new_for_uri ("file:///");
}



GFile *
thunar_g_file_new_for_recent (void)
{
  return g_file_new_for_uri ("recent:///");
}



GFile *
thunar_g_file_new_for_trash (void)
{
  return g_file_new_for_uri ("trash:///");
}



GFile *
thunar_g_file_new_for_computer (void)
{
  return g_file_new_for_uri ("computer://");
}



GFile *
thunar_g_file_new_for_network (void)
{
  return g_file_new_for_uri ("network://");
}



GFile *
thunar_g_file_new_for_desktop (void)
{
  return g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));
}



GFile *
thunar_g_file_new_for_bookmarks (void)
{
  gchar *filename;
  GFile *bookmarks;

  filename = g_build_filename (g_get_user_config_dir (), "gtk-3.0", "bookmarks", NULL);
  bookmarks = g_file_new_for_path (filename);
  g_free (filename);

  return bookmarks;
}



/**
 * thunar_g_file_resolve_symlink:
 * @file : a #GFile.
 *
 * Returns the resolved symlink target of @file as a new #GFile.
 * If @file is not a symlink, @file will just be returned
 *
 * Return value: (nullable) (transfer full): A #GFile on success and %NULL on failure.
 **/
GFile *
thunar_g_file_resolve_symlink (GFile *file)
{
  gchar       *basename;
  GFile       *parent = NULL;
  GFile       *target = NULL;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);

  parent = g_file_get_parent (file);
  if (parent == NULL)
    return NULL;
  
  basename = g_file_get_basename (file);
  if (basename == NULL)
    {
      g_object_unref (parent);
      return NULL;
    }

  target = g_file_resolve_relative_path (parent, basename);
  g_object_unref (parent);
  g_free (basename);
  return target;
}



gboolean
thunar_g_file_is_root (GFile *file)
{
  GFile   *parent;
  gboolean is_root = TRUE;

  parent = g_file_get_parent (file);
  if (G_UNLIKELY (parent != NULL))
    {
      is_root = FALSE;
      g_object_unref (parent);
    }

  return is_root;
}



gboolean
thunar_g_file_is_trashed (GFile *file)
{
  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);
  return g_file_has_uri_scheme (file, "trash");
}



gboolean
thunar_g_file_is_in_recent (GFile *file)
{
  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);
  return g_file_has_uri_scheme (file, "recent");
}



gboolean
thunar_g_file_is_home (GFile *file)
{
  GFile   *home;
  gboolean is_home = FALSE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  home = thunar_g_file_new_for_home ();
  is_home = g_file_equal (home, file);
  g_object_unref (home);

  return is_home;
}



gboolean
thunar_g_file_is_trash (GFile *file)
{
  char *uri;
  gboolean is_trash = FALSE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  uri = g_file_get_uri (file);
  is_trash = g_strcmp0 (uri, "trash:///") == 0;
  g_free (uri);

  return is_trash;
}



gboolean
thunar_g_file_is_recent (GFile *file)
{
  char *uri;
  gboolean is_recent = FALSE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  uri = g_file_get_uri (file);
  is_recent = g_strcmp0 (uri, "recent:///") == 0;
  g_free (uri);

  return is_recent;
}



gboolean
thunar_g_file_is_computer (GFile *file)
{
  char *uri;
  gboolean is_computer = FALSE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  uri = g_file_get_uri (file);
  is_computer = g_strcmp0 (uri, "computer:///") == 0;
  g_free (uri);

  return is_computer;
}



gboolean
thunar_g_file_is_network (GFile *file)
{
  char *uri;
  gboolean is_network = FALSE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  uri = g_file_get_uri (file);
  is_network = g_strcmp0 (uri, "network:///") == 0;
  g_free (uri);

  return is_network;
}



GKeyFile *
thunar_g_file_query_key_file (GFile              *file,
                              GCancellable       *cancellable,
                              GError            **error)
{
  GKeyFile *key_file;
  gchar    *contents = NULL;
  gsize     length;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* try to load the entire file into memory */
  if (!g_file_load_contents (file, cancellable, &contents, &length, NULL, error))
    return NULL;

  /* allocate a new key file */
  key_file = g_key_file_new ();

  /* try to parse the key file from the contents of the file */
  if (G_LIKELY (length == 0
      || g_key_file_load_from_data (key_file, contents, length,
                                    G_KEY_FILE_KEEP_COMMENTS
                                    | G_KEY_FILE_KEEP_TRANSLATIONS,
                                    error)))
    {
      g_free (contents);
      return key_file;
    }
  else
    {
      g_free (contents);
      g_key_file_free (key_file);
      return NULL;
    }
}



gboolean
thunar_g_file_write_key_file (GFile        *file,
                              GKeyFile     *key_file,
                              GCancellable *cancellable,
                              GError      **error)
{
  gchar    *contents;
  gsize     length;
  gboolean  result = TRUE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (key_file != NULL, FALSE);
  _thunar_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* write the key file into the contents buffer */
  contents = g_key_file_to_data (key_file, &length, NULL);

  /* try to replace the file contents with the key file data */
  if (contents != NULL)
    {
      result = g_file_replace_contents (file, contents, length, NULL, FALSE,
                                        G_FILE_CREATE_NONE,
                                        NULL, cancellable, error);

      /* cleanup */
      g_free (contents);
    }

  return result;
}



gchar *
thunar_g_file_get_location (GFile *file)
{
  gchar *location;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);

  location = g_file_get_path (file);
  if (location == NULL)
    location = g_file_get_uri (file);

  return location;
}



static const gchar *
guess_device_type_from_icon_name (const gchar * icon_name)
{
  for (int i = 0; device_icon_name[i].type != NULL ; i++)
    {
      if (g_pattern_match_simple (device_icon_name[i].icon_name, icon_name))
        return g_dgettext (NULL, device_icon_name[i].type);
    }
  return NULL;
}



/**
 * thunar_g_file_guess_device_type:
 * @file : a #GFile instance.
 *
 * Returns : (transfer none) (nullable): the string of the device type.
 */
const gchar *
thunar_g_file_guess_device_type (GFile *file)
{
  GFileInfo         *fileinfo    = NULL;
  GIcon             *icon        = NULL;
  const gchar       *icon_name   = NULL;
  const gchar       *device_type = NULL;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);

  fileinfo = g_file_query_info (file,
                                G_FILE_ATTRIBUTE_STANDARD_ICON,
                                G_FILE_QUERY_INFO_NONE, NULL, NULL);
  if (fileinfo == NULL)
    return NULL;

  icon = G_ICON (g_file_info_get_attribute_object (fileinfo, G_FILE_ATTRIBUTE_STANDARD_ICON));
  if (!G_IS_THEMED_ICON (icon))
    {
      g_object_unref (fileinfo);
      return NULL;
    }

  icon_name = g_themed_icon_get_names (G_THEMED_ICON (icon))[0];
  device_type = guess_device_type_from_icon_name (icon_name);
  g_object_unref (fileinfo);

  return device_type;
}



gchar *
thunar_g_file_get_display_name (GFile *file)
{
  gchar *base_name;
  gchar *display_name;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);

  base_name = g_file_get_basename (file);
  if (G_LIKELY (base_name != NULL))
    {
      if (strcmp (base_name, "/") == 0)
        display_name = g_strdup (_("File System"));
      else if (thunar_g_file_is_trash (file))
        display_name = g_strdup (_("Trash"));
      else if (g_utf8_validate (base_name, -1, NULL))
        display_name = g_strdup (base_name);
      else
        display_name = g_uri_escape_string (base_name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);

      g_free (base_name);
   }
 else
   {
     display_name = g_strdup ("?");
   }

  return display_name;
}



gchar *
thunar_g_file_get_display_name_remote (GFile *mount_point)
{
  gchar       *scheme;
  gchar       *parse_name;
  const gchar *p;
  const gchar *path;
  gchar       *unescaped;
  gchar       *hostname;
  gchar       *display_name = NULL;
  const gchar *skip;
  const gchar *firstdot;
  const gchar  skip_chars[] = ":@";
  guint        n;

  _thunar_return_val_if_fail (G_IS_FILE (mount_point), NULL);

  /* not intended for local mounts */
  if (!g_file_is_native (mount_point))
    {
      scheme = g_file_get_uri_scheme (mount_point);
      parse_name = g_file_get_parse_name (mount_point);

      if (scheme != NULL && g_str_has_prefix (parse_name, scheme))
        {
          /* extract the hostname */
          p = parse_name + strlen (scheme);
          while (*p == ':' || *p == '/')
            ++p;

          /* goto path part */
          path = strchr (p, '/');
          firstdot = strchr (p, '.');

          if (firstdot != NULL)
            {
              /* skip password or login names in the hostname */
              for (n = 0; n < G_N_ELEMENTS (skip_chars) - 1; n++)
                {
                  skip = strchr (p, skip_chars[n]);
                  if (skip != NULL
                       && (path == NULL || skip < path)
                       && (skip < firstdot))
                    p = skip + 1;
                }
            }

          /* extract the path and hostname from the string */
          if (G_LIKELY (path != NULL))
            {
              hostname = g_strndup (p, path - p);
            }
          else
            {
              hostname = g_strdup (p);
              path = "/";
            }

          /* unescape the path so that spaces and other characters are shown correctly */
          unescaped = g_uri_unescape_string (path, NULL);

          /* TRANSLATORS: this will result in "<path> on <hostname>" */
          display_name = g_strdup_printf (_("%s on %s"), unescaped, hostname);

          g_free (unescaped);
          g_free (hostname);
        }

      if (scheme != NULL)
        g_free (scheme);
      g_free (parse_name);
    }

  /* never return null */
  if (display_name == NULL)
    display_name = thunar_g_file_get_display_name (mount_point);

  return display_name;
}



gboolean
thunar_g_vfs_is_uri_scheme_supported (const gchar *scheme)
{
  const gchar * const *supported_schemes;
  gboolean             supported = FALSE;
  guint                n;
  GVfs                *gvfs;

  _thunar_return_val_if_fail (scheme != NULL && *scheme != '\0', FALSE);

  gvfs = g_vfs_get_default ();
  supported_schemes = g_vfs_get_supported_uri_schemes (gvfs);

  if (supported_schemes == NULL)
    return FALSE;

  for (n = 0; !supported && supported_schemes[n] != NULL; ++n)
    if (g_strcmp0 (supported_schemes[n], scheme) == 0)
      supported = TRUE;

  return supported;
}



/**
 * thunar_g_file_get_free_space:
 * @file           : a #GFile instance.
 * @fs_free_return : return location for the amount of
 *                   free space or %NULL.
 * @fs_size_return : return location for the total volume size.
 *
 * Determines the amount of free space of the volume on
 * which @file resides. Returns %TRUE if the amount of
 * free space was determined successfully and placed into
 * @free_space_return, else %FALSE will be returned.
 *
 * Return value: %TRUE if successfull, else %FALSE.
 **/
gboolean
thunar_g_file_get_free_space (GFile   *file,
                              guint64 *fs_free_return,
                              guint64 *fs_size_return)
{
  GFileInfo *filesystem_info;
  gboolean   success = FALSE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  filesystem_info = g_file_query_filesystem_info (file,
                                                  THUNARX_FILESYSTEM_INFO_NAMESPACE,
                                                  NULL, NULL);

  if (filesystem_info != NULL)
    {
      if (fs_free_return != NULL)
        {
          *fs_free_return = g_file_info_get_attribute_uint64 (filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
          success = g_file_info_has_attribute (filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
        }

      if (fs_size_return != NULL)
        {
          *fs_size_return = g_file_info_get_attribute_uint64 (filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
          success = g_file_info_has_attribute (filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
        }

      g_object_unref (filesystem_info);
    }

  return success;
}



gchar *
thunar_g_file_get_free_space_string (GFile *file, gboolean file_size_binary)
{
  gchar             *fs_size_free_str;
  gchar             *fs_size_used_str;
  guint64            fs_size_free;
  guint64            fs_size_total;
  gchar             *free_space_string = NULL;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);

  if (thunar_g_file_get_free_space (file, &fs_size_free, &fs_size_total) && fs_size_total > 0)
    {
      fs_size_free_str = g_format_size_full (fs_size_free, file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
      fs_size_used_str = g_format_size_full (fs_size_total - fs_size_free, file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);

      free_space_string = g_strdup_printf (_("%s used (%.0f%%)  |  %s free (%.0f%%)"),
                                   fs_size_used_str, ((fs_size_total - fs_size_free) * 100.0 / fs_size_total),
                                   fs_size_free_str, (fs_size_free * 100.0 / fs_size_total));

      g_free (fs_size_free_str);
      g_free (fs_size_used_str);
    }

  return free_space_string;
}



GType
thunar_g_file_list_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_boxed_type_register_static (I_("ThunarGFileList"),
                                           (GBoxedCopyFunc) thunar_g_list_copy_deep,
                                           (GBoxedFreeFunc) thunar_g_list_free_full);
    }

  return type;
}



/**
 * thunar_g_file_copy:
 * @source                 : input #GFile
 * @destination            : destination #GFile
 * @flags                  : set of #GFileCopyFlags
 * @use_partial            : option to use *.partial~
 * @cancellable            : (nullable): optional #GCancellable object
 * @progress_callback      : (nullable) (scope call): function to callback with progress information
 * @progress_callback_data : (clousure): user data to pass to @progress_callback
 * @error                  : (nullable): #GError to set on error
 *
 * Calls g_file_copy() if @use_partial is not enabled.
 * If enabled, copies files to *.partial~ first and then
 * renames *.partial~ into its original name.
 *
 * Return value: %TRUE on success, %FALSE otherwise.
 **/
gboolean
thunar_g_file_copy (GFile                *source,
                    GFile                *destination,
                    GFileCopyFlags        flags,
                    gboolean              use_partial,
                    GCancellable         *cancellable,
                    GFileProgressCallback progress_callback,
                    gpointer              progress_callback_data,
                    GError              **error)
{
  gboolean            success;
  GFileQueryInfoFlags query_flags;
  GFileInfo          *info = NULL;
  GFile              *parent;
  GFile              *partial;
  gchar              *partial_name;
  gchar              *base_name;

  _thunar_return_val_if_fail (g_file_has_parent (destination, NULL), FALSE);

  if (use_partial)
    {
      query_flags = (flags & G_FILE_COPY_NOFOLLOW_SYMLINKS) ? G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS : G_FILE_QUERY_INFO_NONE;
      info = g_file_query_info (source,
                                G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                query_flags,
                                cancellable,
                                NULL);
    }

  /* directory does not need .partial */
  if (info == NULL)
    {
      use_partial = FALSE;
    }
  else
    {
      use_partial = g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR;
      g_clear_object (&info);
    }

  if (!use_partial)
    {
      success = g_file_copy (source, destination, flags, cancellable, progress_callback, progress_callback_data, error);
      return success;
    }

  /* check destination */
  if (g_file_query_exists (destination, NULL))
    {
      if (flags & G_FILE_COPY_OVERWRITE)
        {
          /* We want to overwrite. Just delete the old file */
          if (error != NULL)
            g_clear_error (error);
          error = NULL;
          g_file_delete (destination, NULL, error);
          if (error != NULL)
            return FALSE;
        }
      else
        {
          /* Try to mimic g_file_copy() error */
          if (error != NULL)
            *error = g_error_new (G_IO_ERROR, G_IO_ERROR_EXISTS,
                                  "Error opening file \"%s\": File exists", g_file_peek_path (destination));
          return FALSE;
        }
    }

  /* generate partial file name */
  base_name    = g_file_get_basename (destination);
  if (base_name == NULL)
    {
      base_name = g_strdup ("UNNAMED");
    }

  /* limit filename length */
  partial_name = g_strdup_printf ("%.100s.partial~", base_name);
  parent       = g_file_get_parent (destination);

  /* parent can't be NULL since destination must be a file */
  partial      = g_file_get_child (parent, partial_name);
  g_clear_object (&parent);
  g_free (partial_name);

  /* check if partial file exists */
  if (g_file_query_exists (partial, NULL))
    g_file_delete (partial, NULL, error);

  /* copy file to .partial */
  success = g_file_copy (source, partial, flags, cancellable, progress_callback, progress_callback_data, error);

  if (success)
    {
      /* rename .partial if done without problem */
      success = (g_file_set_display_name (partial, base_name, NULL, error) != NULL);
    }

  if (!success)
    {
      /* try to remove incomplete file. */
      /* failure is expected so error is ignored */
      /* it must be triggered if cancelled */
      /* thus cancellable is also ignored */
      g_file_delete (partial, NULL, NULL);
    }

  g_clear_object (&partial);
  g_free (base_name);
  return success;
}



/**
 * thunar_g_file_compare_checksum:
 * @file_a      : a #GFile
 * @file_b      : a #GFile
 * @cancellable : (nullalble): optional #GCancellable object
 * @error       : (nullalble): optional #GError
 *
 * Compare @file_a and @file_b with checksum.
 *
 * Return value: %TRUE if a checksum matches, %FALSE if not.
 **/
gboolean
thunar_g_file_compare_checksum (GFile        *file_a,
                                GFile        *file_b,
                                GCancellable *cancellable,
                                GError      **error)
{
  gchar   *str_a;
  gchar   *str_b;
  gboolean is_equal;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  str_a = xfce_g_file_create_checksum (file_a, cancellable, error);
  str_b = xfce_g_file_create_checksum (file_b, cancellable, error);

  is_equal = g_strcmp0 (str_a, str_b) == 0;

  g_free (str_a);
  g_free (str_b);

  return is_equal;
}



/**
 * thunar_g_file_list_new_from_string:
 * @string : a string representation of an URI list.
 *
 * Splits an URI list conforming to the text/uri-list
 * mime type defined in RFC 2483 into individual URIs,
 * discarding any comments and whitespace. The resulting
 * list will hold one #GFile for each URI.
 *
 * If @string contains no URIs, this function
 * will return %NULL.
 *
 * Return value: the list of #GFile<!---->s or %NULL.
 **/
GList *
thunar_g_file_list_new_from_string (const gchar *string)
{
  GList  *list = NULL;
  gchar **uris;
  gsize   n;

  uris = g_uri_list_extract_uris (string);

  for (n = 0; uris != NULL && uris[n] != NULL; ++n)
    list = g_list_append (list, g_file_new_for_uri (uris[n]));

  g_strfreev (uris);

  return list;
}



/**
 * thunar_g_file_list_to_stringv:
 * @list : a list of #GFile<!---->s.
 *
 * Free the returned value using g_strfreev() when you
 * are done with it. Useful for gtk_selection_data_set_uris.
 *
 * Return value: and array of uris.
 **/
gchar **
thunar_g_file_list_to_stringv (GList *list)
{
  gchar **uris;
  guint   n;
  GList  *lp;

  /* allocate initial string */
  uris = g_new0 (gchar *, g_list_length (list) + 1);

  for (lp = list, n = 0; lp != NULL; lp = lp->next)
    {
      /* Prefer native paths for interoperability. */
      gchar *path = g_file_get_path (G_FILE (lp->data));
      if (path == NULL)
        {
          uris[n++] = g_file_get_uri (G_FILE (lp->data));
        }
      else
        {
          uris[n++] = g_filename_to_uri (path, NULL, NULL);
          g_free(path);
        }
    }

  return uris;
}



/**
 * thunar_g_file_list_get_parents:
 * @list : a list of #GFile<!---->s.
 *
 * Collects all parent folders of the passed files
 * If multiple files share the same parent, the parent will only be added once to the returned list.
 * Each list element of the returned list needs to be freed with g_object_unref() after use.
 *
 * Return value: A list of #GFile<!---->s of all parent folders. Free the returned list with calling g_object_unref() on each element
 **/
GList*
thunar_g_file_list_get_parents (GList *file_list)
{
  GList    *lp_file_list;
  GList    *lp_parent_folder_list;
  GFile    *parent_folder;
  GList    *parent_folder_list = NULL;
  gboolean  folder_already_added;

  for (lp_file_list = file_list; lp_file_list != NULL; lp_file_list = lp_file_list->next)
    {
      if (!G_IS_FILE (lp_file_list->data))
        continue;
      parent_folder = g_file_get_parent (lp_file_list->data);
      if (parent_folder == NULL)
        continue;
      folder_already_added = FALSE;
      /* Check if the folder already is in our list */
      for (lp_parent_folder_list = parent_folder_list; lp_parent_folder_list != NULL; lp_parent_folder_list = lp_parent_folder_list->next)
        {
          if (g_file_equal (lp_parent_folder_list->data, parent_folder))
            {
              folder_already_added = TRUE;
              break;
            }
        }
      /* Keep the reference for each folder added to parent_folder_list */
      if (folder_already_added)
        g_object_unref (parent_folder);
      else
        parent_folder_list = g_list_append (parent_folder_list, parent_folder);
    }
  return parent_folder_list;
}



/**
 * thunar_g_file_is_descendant:
 * @descendant : a #GFile that is a potential descendant of @ancestor
 * @ancestor   : a #GFile
 *
 * Check if @descendant is a subdirectory of @ancestor.
 * @descendant == @ancestor also counts.
 *
 * Returns: %TRUE if @descendant is a subdirectory of @ancestor.
 **/
gboolean
thunar_g_file_is_descendant (GFile *descendant,
                             GFile *ancestor)
{
  _thunar_return_val_if_fail (descendant != NULL && G_IS_FILE (descendant), FALSE);
  _thunar_return_val_if_fail (ancestor   != NULL && G_IS_FILE (ancestor),   FALSE);

  for (GFile *parent = g_object_ref (descendant), *temp;
       parent != NULL;
       temp = g_file_get_parent (parent), g_object_unref (parent), parent = temp)
    {
      if (g_file_equal (parent, ancestor))
        {
          g_object_unref (parent);
          return TRUE;
        }
    }

  return FALSE;
}



gboolean
thunar_g_app_info_launch (GAppInfo          *info,
                          GFile             *working_directory,
                          GList             *path_list,
                          GAppLaunchContext *context,
                          GError           **error)
{
  ThunarFile   *file;
  GAppInfo     *default_app_info;
  GList        *recommended_app_infos;
  GList        *lp;
  const gchar  *content_type;
  gboolean      result = FALSE;
  gchar        *new_path = NULL;
  gchar        *old_path = NULL;
  gboolean      skip_app_info_update;

  _thunar_return_val_if_fail (G_IS_APP_INFO (info), FALSE);
  _thunar_return_val_if_fail (working_directory == NULL || G_IS_FILE (working_directory), FALSE);
  _thunar_return_val_if_fail (path_list != NULL, FALSE);
  _thunar_return_val_if_fail (G_IS_APP_LAUNCH_CONTEXT (context), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  skip_app_info_update = (g_object_get_data (G_OBJECT (info), "skip-app-info-update") != NULL);

  /* check if we want to set the working directory of the spawned app */
  if (working_directory != NULL)
    {
      /* determine the working directory path */
      new_path = g_file_get_path (working_directory);
      if (new_path != NULL)
        {
          /* switch to the desired working directory, remember that of Thunar itself */
          old_path = thunar_util_change_working_directory (new_path);

          /* forget about the new working directory path */
          g_free (new_path);
        }
    }

  /* launch the paths with the specified app info */
  result = g_app_info_launch (info, path_list, context, error);

  /* if successful, remember the application as last used for the file types */
  if (result == TRUE)
    {
      for (lp = path_list; lp != NULL; lp = lp->next)
        {
          gboolean update_app_info = !skip_app_info_update;

          file = thunar_file_get (lp->data, NULL);
          if (file == NULL)
            continue;

          content_type = thunar_file_get_content_type (file);

          /* determine default application */
          default_app_info = thunar_file_get_default_handler (file);
          if (default_app_info != NULL)
            {
              /* check if the application is the default one */
              if (g_app_info_equal (info, default_app_info))
                update_app_info = FALSE;
              g_object_unref (default_app_info);
            }

          if (update_app_info)
            {
              /* obtain list of last used applications */
              recommended_app_infos = g_app_info_get_recommended_for_type (content_type);
              if (recommended_app_infos != NULL)
                {
                  /* check if the application is already the last used one
                   * by comparing it with the first entry in the list */
                  if (g_app_info_equal (info, recommended_app_infos->data))
                    update_app_info = FALSE;

                  g_list_free (recommended_app_infos);
                }
            }

          /* emit "changed" on the file if we successfully changed the last used application */
          if (update_app_info && g_app_info_set_as_last_used_for_type (info, content_type, NULL))
            thunar_file_changed (file);

          g_object_unref (file);
        }
    }

  /* check if we need to reset the working directory to the one Thunar was
   * opened from */
  if (old_path != NULL)
    {
      /* switch to Thunar's original working directory */
      new_path = thunar_util_change_working_directory (old_path);

      /* clean up */
      g_free (new_path);
      g_free (old_path);
    }

  return result;
}



gboolean
thunar_g_app_info_should_show (GAppInfo *info)
{
#ifdef HAVE_GIO_UNIX
  _thunar_return_val_if_fail (G_IS_APP_INFO (info), FALSE);

  if (G_IS_DESKTOP_APP_INFO (info))
    {
      /* NoDisplay=true files should be visible in the interface,
       * because this key is intent to hide mime-helpers from the
       * application menu. Hidden=true is never returned by GIO. */
      return g_desktop_app_info_get_show_in (G_DESKTOP_APP_INFO (info), NULL);
    }

  return TRUE;
#else
  /* we cannot exclude custom actions, so show everything */
  return TRUE;
#endif
}



gboolean
thunar_g_vfs_metadata_is_supported (void)
{
  GFile                  *root;
  GFileAttributeInfoList *attr_info_list;
  gint                    n;
  gboolean                metadata_is_supported = FALSE;

  /* get a GFile for the root directory, and obtain the list of writable namespaces for it */
  root = thunar_g_file_new_for_root ();
  attr_info_list = g_file_query_writable_namespaces (root, NULL, NULL);
  g_object_unref (root);

  /* loop through the returned namespace names and check if "metadata" is included */
  for (n = 0; n < attr_info_list->n_infos; n++)
    {
      if (g_strcmp0 (attr_info_list->infos[n].name, "metadata") == 0)
        {
          metadata_is_supported = TRUE;
          break;
        }
    }

  /* release the attribute info list */
  g_file_attribute_info_list_unref (attr_info_list);

  return metadata_is_supported;
}



/**
 * thunar_g_file_is_on_local_device:
 * @file : the source or target #GFile to test.
 *
 * Tries to find if the @file is on a local device or not.
 * Local device if (all conditions should match):
 * - the file has a 'file' uri scheme.
 * - the file is located on devices not handled by the #GVolumeMonitor (GVFS).
 * - the device is handled by #GVolumeMonitor (GVFS) and cannot be unmounted
 *   (USB key/disk, fuse mounts, Samba shares, PTP devices).
 *
 * The target @file may not exist yet when this function is used, so recurse
 * the parent directory, possibly reaching the root mountpoint.
 *
 * This should be enough to determine if a @file is on a local device or not.
 *
 * Return value: %TRUE if #GFile @file is on a so-called local device.
 **/
gboolean
thunar_g_file_is_on_local_device (GFile *file)
{
  gboolean  is_local = FALSE;
  GFile    *target_file;
  GFile    *target_parent;
  GMount   *file_mount;

  _thunar_return_val_if_fail (file != NULL, TRUE);
  _thunar_return_val_if_fail (G_IS_FILE (file), TRUE);

  if (g_file_has_uri_scheme (file, "file") == FALSE)
    return FALSE;
  for (target_file  = g_object_ref (file);
       target_file != NULL;
       target_file  = target_parent)
    {
      if (g_file_query_exists (target_file, NULL))
        break;

      /* file or parent directory does not exist (yet)
       * query the parent directory */
      target_parent = g_file_get_parent (target_file);
      g_object_unref (target_file);
    }

  if (target_file == NULL)
    return FALSE;

  /* file_mount will be NULL for local files on local partitions/devices */
  file_mount = g_file_find_enclosing_mount (target_file, NULL, NULL);
  g_object_unref (target_file);
  if (file_mount == NULL)
    is_local = TRUE;
  else
    {
      /* mountpoints which cannot be unmounted are local devices.
       * attached devices like USB key/disk, fuse mounts, Samba shares,
       * PTP devices can always be unmounted and are considered remote/slow. */
      is_local = ! g_mount_can_unmount (file_mount);
      g_object_unref (file_mount);
    }

  return is_local;
}



/**
 * thunar_g_file_set_executable_flags:
 * @file : the #GFile for which execute flags should be set
 *
 * Tries to set +x flag of the file for user, group and others
 *
 * Return value: %TRUE on sucess, %FALSE on error
 **/
gboolean
thunar_g_file_set_executable_flags (GFile   *file,
                                    GError **error)
{
  ThunarFileMode  old_mode;
  ThunarFileMode  new_mode;
  GFileInfo      *info;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* try to query information about the file */
  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_UNIX_MODE,
                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                            NULL, error);

  if (G_LIKELY (info != NULL))
    {
      if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE))
        {
          /* determine the current mode */
          old_mode = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);

          /* generate the new mode */
          new_mode = old_mode | THUNAR_FILE_MODE_USR_EXEC | THUNAR_FILE_MODE_GRP_EXEC | THUNAR_FILE_MODE_OTH_EXEC;

          if (old_mode != new_mode)
            {
              g_file_set_attribute_uint32 (file,
                                           G_FILE_ATTRIBUTE_UNIX_MODE, new_mode,
                                           G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                           NULL, error);
            }
        }
      else
        {
          g_warning ("No %s attribute found", G_FILE_ATTRIBUTE_UNIX_MODE);
        }

      g_object_unref (info);
    }

  return (error == NULL);
}



/**
 * thunar_g_file_is_in_xdg_data_dir:
 * @file      : a #GFile.
 *
 * Returns %TRUE if @file is located below one of the directories given in XDG_DATA_DIRS
 *
 * Return value: %TRUE if @file is located inside a XDG_DATA_DIR
 **/
gboolean
thunar_g_file_is_in_xdg_data_dir (GFile *file)
{
    const gchar * const *data_dirs;
    guint                i;
    gchar               *path;
    gboolean             found = FALSE;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (g_file_is_native (file))
    {
      data_dirs = g_get_system_data_dirs ();
      if (G_LIKELY (data_dirs != NULL))
        {
          path = g_file_get_path (file);
          for (i = 0; data_dirs[i] != NULL; i++)
            {
              if (g_str_has_prefix (path, data_dirs[i]))
              {
                found = TRUE;
                break;
              }
            }
          g_free (path);
        }
    }
    return found;
}



/**
 * thunar_g_file_is_desktop_file:
 * @file      : a #GFile.
 *
 * Returns %TRUE if @file is a .desktop file.
 *
 * Return value: %TRUE if @file is a .desktop file.
 **/
gboolean
thunar_g_file_is_desktop_file (GFile *file)
{
  gchar     *basename;
  gboolean   is_desktop_file = FALSE;
  GFileInfo *info;

  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  basename = g_file_get_basename (file);

  /* only allow regular files with a .desktop extension */
  if (g_str_has_suffix (basename, ".desktop"))
    {
      info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
      if (G_LIKELY (info != NULL && g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR))
        is_desktop_file = TRUE;
      g_object_unref (info);
    }
  
  g_free (basename);
  return is_desktop_file;
}



/**
 * thunar_g_file_get_link_path_for_symlink:
 * @file_to_link : the #GFile to which @symlink will have to point
 * @symlink      : a #GFile representing the symlink
 *
 * Method to build the link target path in order to link from @symlink to @file_to_link.
 * The caller is responsible for freeing the string using g_free() when done.
 *
 * Return value: The link path, or NULL on failure
 **/
char *
thunar_g_file_get_link_path_for_symlink (GFile *file_to_link,
                                         GFile *symlink)
{
    GFile *root;
    GFile *parent;
    char  *relative_path;
    char  *link_path;

  _thunar_return_val_if_fail (G_IS_FILE (file_to_link), NULL);
  _thunar_return_val_if_fail (G_IS_FILE (symlink), NULL);

    /* */
    if (g_file_is_native (file_to_link) || g_file_is_native (symlink))
    {
        return g_file_get_path (file_to_link);
    }

    /* Search for the filesystem root */
    root = g_object_ref (file_to_link);
    while ((parent = g_file_get_parent (root)) != NULL)
    {
        g_object_unref (root);
        root = parent;
    }

    /* Build a absolute path, using the relative path up to the filesystem root */
    relative_path = g_file_get_relative_path (root, file_to_link);
    g_object_unref (root);
    link_path = g_strconcat ("/", relative_path, NULL);
    g_free (relative_path);
    return link_path;
}