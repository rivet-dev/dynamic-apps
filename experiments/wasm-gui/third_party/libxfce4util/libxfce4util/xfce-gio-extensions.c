/*
 * Copyright (c) 2021 Yongha Hwang <mshrimp@sogang.ac.kr>
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
 * SECTION:xfce-gio-extensions
 * @title: GIO Extensions
 * @short_description: various extensions to GIO
 * @include: libxfce4util/libxfce4util.h
 *
 * Common used functions for I/O that are not provided by the Glib library
 *
 * Since: 4.17
 **/



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4util/libxfce4util-alias.h>



#define XFCE_ATTRIBUTE_EXECUTABLE_CHECKSUM "metadata::xfce-exe-checksum" /* string */



/**
 * xfce_g_file_metadata_is_supported:
 * @file: a #GFile.
 *
 * Checks if GVFS-metadata is available for
 * the @file provided.
 *
 * Returns: %TRUE is GVFS-metadata is supported, %FALSE otherwise.
 *
 * Since: 4.17
 **/
gboolean
xfce_g_file_metadata_is_supported (GFile *file)
{
  GFileAttributeInfoList *info_list;
  gboolean                is_supported;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  info_list = g_file_query_writable_namespaces (file, NULL, NULL);
  if (info_list == NULL)
    return FALSE;

  is_supported = (g_file_attribute_info_list_lookup (info_list, "metadata") != NULL);
  g_file_attribute_info_list_unref (info_list);

  return is_supported;
}



/**
 * xfce_g_file_create_checksum:
 * @file: a #GFile.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: (nullable): a #GError
 *
 * Generates an SHA-256 hash of the @file.
 *
 * Returns: (transfer full) (nullable): Checksum of the @file.
 * If file read fails, returns %NULL. Free with g_free().
 *
 * Since: 4.17
 **/
gchar *
xfce_g_file_create_checksum (GFile        *file,
                             GCancellable *cancellable,
                             GError      **error)
{
  GFileInputStream *stream;
  GChecksum        *checksum;
  gchar            *checksum_string;
  gssize            read_bytes;
  guint8            buffer[1024];

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (G_IS_FILE (file),                NULL);

  stream = g_file_read (file, cancellable, error);

  /* An error occured */
  if (stream == NULL)
    return NULL;

  checksum = g_checksum_new (G_CHECKSUM_SHA256);

  while ((read_bytes = g_input_stream_read (G_INPUT_STREAM (stream), buffer, sizeof (buffer), cancellable, error)) > 0)
    {
      g_checksum_update (checksum, buffer, read_bytes);
    }

  g_object_unref (stream);

  /* An error occured */
  if (read_bytes == -1)
    {
      g_checksum_free (checksum);
      return NULL;
    }

  checksum_string = g_strdup (g_checksum_get_string (checksum));

  g_checksum_free (checksum);

  return checksum_string;
}


/**
 * xfce_g_file_set_trusted:
 * @file: a #GFile.
 * @is_trusted: #TRUE if trusted, #FALSE if not
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: (nullable): a #GError
 *
 * Sets the "safety flag" on if @is_trusted.
 *
 * Safety flag is a new concept introduced in
 * XFCE 4.17. It is basically an additional
 * execution flag stored in GVFS-metadata.
 * Unlike the execution flag (+x), it is safe
 * to assume that this flag did not come from
 * foreign location (for example, by downloading
 * an tar archive) and is set by user.
 *
 * The checksum of the file is stored, and
 * would be considered "on" only if checksum
 * matches with the file on execution.
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * Since: 4.17
 **/
gboolean
xfce_g_file_set_trusted (GFile        *file,
                         gboolean      is_trusted,
                         GCancellable *cancellable,
                         GError      **error)
{
  GError  *error_local = NULL;
  gchar   *checksum_string;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (G_IS_FILE (file),                FALSE);

  if (!xfce_g_file_metadata_is_supported (file))
    {
      if (error != NULL)
        *error = g_error_new (G_FILE_ERROR,
                              G_FILE_ERROR_NOSYS,
                              "GVFS-metadata is not supported for file '%s'",
                              g_file_peek_path (file));
      return FALSE;
    }

  if (is_trusted)
    {
      checksum_string = xfce_g_file_create_checksum (file, cancellable, &error_local);
      if (error_local != NULL)
        {
          g_propagate_error (error, error_local);
          return FALSE;
        }
    }
  else
    checksum_string = NULL;

  g_file_set_attribute (file,
                        XFCE_ATTRIBUTE_EXECUTABLE_CHECKSUM,
                        is_trusted ? G_FILE_ATTRIBUTE_TYPE_STRING : G_FILE_ATTRIBUTE_TYPE_INVALID,
                        checksum_string,
                        G_FILE_ATTRIBUTE_INFO_COPY_WITH_FILE | G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED,
                        cancellable,
                        &error_local);
  g_free (checksum_string);

  if (error_local != NULL)
    {
      g_propagate_error (error, error_local);
      return FALSE;
    }
  return TRUE;
}



/**
 * xfce_g_file_is_trusted:
 * @file: a #GFile.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: (nullable): a #GError
 *
 * Compares the checksum stored in safety flag
 * with the actual file. If it matches, it
 * is considered safe.
 *
 * Read the documentation of
 * xfce_g_file_set_trusted() for details.
 *
 * Returns: %TRUE if safety flag is verified
 * or not supported. %FALSE otherwise.
 *
 * Since: 4.17
 **/
gboolean
xfce_g_file_is_trusted (GFile        *file,
                        GCancellable *cancellable,
                        GError      **error)
{
  GError      *error_local = NULL;
  GFileInfo   *file_info;
  gboolean     is_trusted;
  const gchar *attribute_string;
  gchar       *checksum_string;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (!xfce_g_file_metadata_is_supported (file))
    {
      if (error != NULL)
        *error = g_error_new (G_FILE_ERROR,
                              G_FILE_ERROR_NOSYS,
                              "GVFS-metadata is not supported for file '%s'",
                              g_file_peek_path (file));
      return TRUE;
    }

  file_info = g_file_query_info (file,
                                 XFCE_ATTRIBUTE_EXECUTABLE_CHECKSUM,
                                 G_FILE_QUERY_INFO_NONE,
                                 cancellable,
                                 &error_local);
  if (error_local != NULL)
    {
      g_propagate_error (error, error_local);
      return FALSE;
    }

  attribute_string = g_file_info_get_attribute_string (file_info,
                                                       XFCE_ATTRIBUTE_EXECUTABLE_CHECKSUM);
  if (attribute_string != NULL)
    {
      checksum_string = xfce_g_file_create_checksum (file, cancellable, &error_local);
      if (error_local != NULL)
        {
          g_object_unref (file_info);
          g_propagate_error (error, error_local);
          return FALSE;
        }
      is_trusted = (g_strcmp0 (attribute_string, checksum_string) == 0);
      g_info ("== Safety flag check ==");
      g_info ("Attribute checksum: %s", attribute_string);
      g_info ("File checksum     : %s", checksum_string);
      g_free (checksum_string);
    }
  else
    is_trusted = FALSE;

  g_object_unref (file_info);

  return is_trusted;
}

#define __XFCE_GIO_EXTENSIONS_C__
#include <libxfce4util/libxfce4util-aliasdef.c>
