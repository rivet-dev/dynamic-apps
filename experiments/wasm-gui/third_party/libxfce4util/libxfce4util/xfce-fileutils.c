/* $Id$ */
/*-
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
 * SECTION: xfce-fileutils
 * @title: File Utilities
 * @short_description: miscellaneous file-related utility functions.
 * @see_also: https://developer.gnome.org/glib/stable/glib-File-Utilities.html
 *
 * File Utilities
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4util/libxfce4util-private.h>
#include <libxfce4util/libxfce4util-alias.h>



/**
 * xfce_mkdirhier:
 * @whole_path : path to the directory to create.
 * @mode       : file permissions to use for the newly created directories.
 * @error      : location where to store GError object to, error are returned
 *               in the %G_FILE_ERROR domain.
 *
 * Creates the specified directory @whole_path, but unlike the mkdir()
 * function from the standard C library, if any of the parent directories
 * of the @whole_path do not exists, they are created as well.
 *
 * If the directory specified by @whole_path already exists, this function
 * performs no operation and simply returns %TRUE.
 *
 * Return value: %TRUE on success, else %FALSE.
 *
 * Since: 4.2
 **/
gboolean
xfce_mkdirhier (const gchar *whole_path,
                gulong       mode,
                GError     **error)
{
  /* Stolen from FreeBSD's mkdir(1) with modifications to make it
   * work properly with NFS on Solaris */
  char path[1024];
  struct stat sb;
  mode_t numask, oumask;
  int first, last, sverrno;
  gboolean retval;
  char *p;

  g_return_val_if_fail (whole_path != NULL, FALSE);

  g_strlcpy (path, whole_path, sizeof (path));
  p = path;
  oumask = 0;
  retval = TRUE;

  if (p[0] == G_DIR_SEPARATOR) /* Skip leading '/'. */
    ++p;

  for (first = 1, last = 0; !last ; ++p)
    {
      if (p[0] == '\0')
        last = 1;
      else if (p[0] != G_DIR_SEPARATOR)
        continue;
      else if (p[1] == '\0')
        last = 1;

      *p = '\0';

      if (first)
        {
          /*
           * POSIX 1003.2:
           * For each dir operand that does not name an existing
           * directory, effects equivalent to those cased by the
           * following command shall occcur:
           *
           * mkdir -p -m $(umask -S),u+wx $(dirname dir) &&
           *    mkdir [-m mode] dir
           *
           * We change the user's umask and then restore it,
           * instead of doing chmod's.
           */
          oumask = umask(0);
          numask = oumask & ~(S_IWUSR | S_IXUSR);
          umask(numask);
          first = 0;
        }

      if (last)
        umask(oumask);

      if (mkdir (path, last ? mode : S_IRWXU | S_IRWXG | S_IRWXO) < 0)
        {
          sverrno = errno;

          if (stat (path, &sb) < 0)
            {
              errno = sverrno;
              retval = FALSE;
              break;
            }
          else if (!S_ISDIR (sb.st_mode))
            {
              errno = ENOTDIR;
              retval = FALSE;
              break;
            }
        }

      if (!last)
        *p = G_DIR_SEPARATOR;
    }

  if (!first && !last)
    umask (oumask);

  if (!retval && error != NULL)
    {
      /* be sure to initialize the i18n support */
      _xfce_i18n_init ();

      g_set_error (error, G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   _("Error creating directory '%s': %s"),
                   whole_path, g_strerror (errno));
    }

  return retval;
}



/**
 * xfce_create_shared_thumbnail_path:
 * @uri        : the uri of the file whose shared thumbnail we want to find.
 * @size       : the thumbnail size (e.g. normal, large).
 *
 * Creates the shared thumbnail path for the file that corresponds to the given @uri
 * and @size. No checks are made regarding the existence of the thumbnail.
 *
 * It is the duty of the caller to free the returned string.
 *
 * Return value: a string with the thumbnail path or NULL if the @uri could not be converted to
 * a local filename.
 *
 * Since: 4.17.1
 **/
gchar*
xfce_create_shared_thumbnail_path (const gchar *uri,
                                   const gchar *size)
{
  GChecksum   *checksum;
  gchar       *dir_uri;
  gchar       *dir_path;
  gchar       *name;
  gchar       *filename;
  gchar       *thumbnail_path;

  name = g_path_get_basename (uri);
  dir_uri = g_path_get_dirname (uri);
  dir_path = g_filename_from_uri (dir_uri, NULL, NULL); /* drop file:// */

  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (const guchar *) name, strlen (name));
  filename = g_strconcat (g_checksum_get_string (checksum), ".png", NULL);

  if (dir_path != NULL)
    thumbnail_path = g_build_filename (dir_path, ".sh_thumbnails", size, filename, NULL);
  else
    thumbnail_path = NULL;

  /* free memory */
  g_free (name);
  g_free (filename);
  g_free (dir_uri);
  g_free (dir_path);
  g_checksum_free (checksum);

  return thumbnail_path;
}


#define __XFCE_FILEUTILS_C__
#include <libxfce4util/libxfce4util-aliasdef.c>
