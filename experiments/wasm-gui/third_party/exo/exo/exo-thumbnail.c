/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
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
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <exo/exo-gdk-pixbuf-extensions.h>
#include <exo/exo-private.h>
#include <exo/exo-thumbnail.h>
#include <exo/exo-alias.h>

/* use g_rename() and g_unlink() on win32 */
#if defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_rename(oldfilename, newfilename) (rename ((oldfilename), (newfilename)))
#define g_unlink(filename) (unlink ((filename)))
#endif



static GdkPixbuf *exo_thumbnail_load (const gchar *thumbnail_path,
                                      const gchar *uri,
                                      time_t       mtime,
                                      GError     **error) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;;
static gboolean   exo_thumbnail_save (GdkPixbuf   *thumbnail,
                                      const gchar *thumbnail_path,
                                      const gchar *uri,
                                      time_t       mtime,
                                      GError     **error);



static GdkPixbuf*
exo_thumbnail_load (const gchar *thumbnail_path,
                    const gchar *uri,
                    time_t       mtime,
                    GError     **error)
{
  const gchar *thumbnail_mtime;
  const gchar *thumbnail_uri;
  GdkPixbuf   *thumbnail;

  /* try to load the thumbnail */
  thumbnail = gdk_pixbuf_new_from_file (thumbnail_path, error);
  if (G_LIKELY (thumbnail != NULL))
    {
      /* determine the URI and the mtime from the thumbnail */
      thumbnail_uri = gdk_pixbuf_get_option (thumbnail, "tEXt::Thumb::URI");
      thumbnail_mtime = gdk_pixbuf_get_option (thumbnail, "tEXt::Thumb::MTime");

      /* verify both the URI and the mtime for the thumbnail */
      if (G_UNLIKELY (thumbnail_uri == NULL || thumbnail_mtime == NULL || strcmp (thumbnail_uri, uri) != 0
                   || (mtime != (time_t) -1 && strtoul (thumbnail_mtime, NULL, 10) != (gulong) mtime)))
        {
          /* the thumbnail is invalid */
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT, "%s", g_strerror (ENOENT));
          g_object_unref (G_OBJECT (thumbnail));
          thumbnail = NULL;
        }
    }

  /* return the thumbnail */
  return thumbnail;
}



static gboolean
exo_thumbnail_save (GdkPixbuf   *thumbnail,
                    const gchar *thumbnail_path,
                    const gchar *uri,
                    time_t       mtime,
                    GError     **error)
{
  gboolean succeed;
  gchar   *tmp_path;
  gchar   *dirname;
  gchar    smtime[32];
  gint     tmp_fd;

  /* verify that the thumbnail directory exists */
  dirname = g_path_get_dirname (thumbnail_path);
  succeed = xfce_mkdirhier (dirname, 0700, error);
  g_free (dirname);

  /* check if we succeed so far */
  if (G_LIKELY (succeed))
    {
      /* try to create a temporary file to write the thumbnail to */
      tmp_path = g_strconcat (thumbnail_path, ".XXXXXX", NULL);
      tmp_fd = g_mkstemp (tmp_path);
      if (G_UNLIKELY (tmp_fd < 0))
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), "%s", g_strerror (errno));
          g_free (tmp_path);
          return FALSE;
        }
      close (tmp_fd);

      /* determine the string representation of the mtime */
      g_snprintf (smtime, sizeof (smtime), "%lu", (gulong) mtime);

      /* write the thumbnail to the temporary location */
      succeed = gdk_pixbuf_save (thumbnail, tmp_path, "png", error,
                                 "tEXt::Thumb::URI", uri,
                                 "tEXt::Thumb::MTime", smtime,
                                 "tEXt::Software", PACKAGE_STRING,
                                 NULL);

      /* rename the file to the final location */
      if (succeed && g_rename (tmp_path, thumbnail_path) < 0)
        {
          /* set an error and unlink the temporary file */
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), "%s", g_strerror (errno));
          g_unlink (tmp_path);
          succeed = FALSE;
        }

      /* cleanup */
      g_free (tmp_path);
    }

  return succeed;
}



/**
 * _exo_thumbnail_get_for_file:
 * @filename : the absolute path to the file for which to load or generate a thumbnail.
 * @size     : the desired thumbnail size, either %EXO_THUMBNAIL_SIZE_NORMAL or %EXO_THUMBNAIL_SIZE_LARGE.
 * @error    : return location for errors or %NULL.
 *
 * Loads the thumbnail stored for @filename in the thumbnail database if such a thumbnail exists. If no
 * such thumbnail exists, the function tries to generate a store a thumbnail for the @filename.
 *
 * The caller is responsible to free the returned pixbuf using g_object_unref() when no longer needed.
 *
 * Returns: the #GdkPixbuf for the thumbnail of @filename or %NULL in case of an error.
 **/
GdkPixbuf*
_exo_thumbnail_get_for_file (const gchar     *filename,
                             ExoThumbnailSize size,
                             GError         **error)
{
  struct stat statb;
  GdkPixbuf  *thumbnail = NULL;
  GError     *err = NULL;
  gchar      *name;
  gchar      *path;
  gchar      *md5;
  gchar      *uri;

  _exo_return_val_if_fail (size == EXO_THUMBNAIL_SIZE_NORMAL || size == EXO_THUMBNAIL_SIZE_LARGE, NULL);
  _exo_return_val_if_fail (error == NULL || *error == NULL, NULL);
  _exo_return_val_if_fail (g_path_is_absolute (filename), NULL);

  /* stat the file first */
  if (stat (filename, &statb) < 0)
    {
      /* we cannot recover from here */
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), "%s", g_strerror (errno));
    }
  else
    {
      /* determine the URI of the file */
      uri = g_filename_to_uri (filename, NULL, error);
      if (G_LIKELY (uri != NULL))
        {
          /* determine the filename of the thumbnail */
          md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
          name = g_strconcat (md5, ".png", NULL);
          g_free (md5);

          /* determine the path of the thumbnail */
          path = g_build_path ("/", g_get_user_cache_dir(), "thumbnails", (size == EXO_THUMBNAIL_SIZE_NORMAL) ? "normal" : "large", name, NULL);
          g_free (name);

          /* try to load the thumbnail */
          thumbnail = exo_thumbnail_load (path, uri, statb.st_mtime, NULL);
          if (G_UNLIKELY (thumbnail == NULL))
            {
              /* try to generate a thumbnail for the file using the available GdkPixbufLoaders */
              thumbnail = exo_gdk_pixbuf_new_from_file_at_max_size (filename, size, size, TRUE, error);
              if (G_LIKELY (thumbnail != NULL))
                {
                  /* save the generated thumbnail into the thumbnail database */
                  if (!exo_thumbnail_save (thumbnail, path, uri, statb.st_mtime, &err))
                    {
                      /* better let the user know whats going on, but no need to fail here */
                      g_warning ("Failed to save generated thumbnail for \"%s\" to \"%s\": %s", filename, path, err->message);
                      g_error_free (err);
                    }
                }
            }

          /* cleanup */
          g_free (path);
          g_free (uri);
        }
    }

  return thumbnail;
}



/**
 * _exo_thumbnail_get_for_uri:
 * @uri   : the URI for which to load the thumbnail.
 * @size  : the desired thumbnail size.
 * @error : return location for errors or %NULL.
 *
 * Similar to _exo_thumbnail_get_for_file(), but does not try to generate
 * a thumbnail if no valid thumbnail is found.
 *
 * Returns: the thumbnail for the @uri or %NULL.
 **/
GdkPixbuf*
_exo_thumbnail_get_for_uri (const gchar     *uri,
                            ExoThumbnailSize size,
                            GError         **error)
{
  GdkPixbuf *thumbnail;
  gchar     *name;
  gchar     *path;
  gchar     *md5;

  _exo_return_val_if_fail (size == EXO_THUMBNAIL_SIZE_NORMAL || size == EXO_THUMBNAIL_SIZE_LARGE, NULL);
  _exo_return_val_if_fail (error == NULL || *error == NULL, NULL);
  _exo_return_val_if_fail (uri != NULL, NULL);

  /* determine the filename of the thumbnail */
  md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
  name = g_strconcat (md5, ".png", NULL);
  g_free (md5);

  /* determine the path of the thumbnail */
  path = g_build_path ("/", g_get_user_cache_dir(), "thumbnails", (size == EXO_THUMBNAIL_SIZE_NORMAL) ? "normal" : "large", name, NULL);
  g_free (name);

  /* try to load the thumbnail */
  thumbnail = exo_thumbnail_load (path, uri, (time_t) -1, error);
  g_free (path);

  return thumbnail;
}



#define __EXO_THUMBNAIL_C__
#include <exo/exo-aliasdef.c>
