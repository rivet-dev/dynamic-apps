/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>

#include <exo/exo.h>

#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-job.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-io-scan-directory.h>


/**
 * thunar_io_scan_directory:
 * @job                 : a #ThunarJob instance
 * @file                : The folder to scan 
 * @flags               : @GFileQueryInfoFlags to consider during scan
 * @recursively         : Wheather as well subfolders should be scanned
 * @unlinking           : ???
 * @return_thunar_files : TRUE in order to return the result as a list of #ThunarFile's, FALSE to return a list of #GFile's  
 * @n_files_max         : Maximum number of files to scan, NULL for unlimited
 * @error               : Will be se on any error
 *
 * Scans the passed folder for files and returns them as a #GList
 *
 * Return value: (transfer full): the #GLIst of #GFiles or #ThunarFiles, to be released with e.g. 'g_list_free_full' 
 **/
GList *
thunar_io_scan_directory (ThunarJob          *job,
                          GFile              *file,
                          GFileQueryInfoFlags flags,
                          gboolean            recursively,
                          gboolean            unlinking,
                          gboolean            return_thunar_files,
                          guint              *n_files_max,
                          GError            **error)
{
  GFileEnumerator *enumerator;
  GFileInfo       *info;
  GFileInfo       *recent_info;
  GFileType        type;
  GError          *err = NULL;
  GFile           *child_file;
  GList           *child_files = NULL;
  GList           *files = NULL;
  const gchar     *namespace;
  ThunarFile      *thunar_file;
  gboolean         is_mounted;
  GCancellable    *cancellable = NULL;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* abort if the job was cancelled */
  if (job != NULL && exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return NULL;

  /* don't recurse when we are scanning prior to unlinking and the current
   * file/dir is in the trash. In GVfs, only the top-level directories in
   * the trash can be modified and deleted directly. See
   * https://bugzilla.xfce.org/show_bug.cgi?id=7147
   * for more information */
  if (unlinking
      && thunar_g_file_is_trashed (file)
      && !thunar_g_file_is_root (file))
    {
      return NULL;
    }

  if (job != NULL)
    cancellable = exo_job_get_cancellable (EXO_JOB (job));

  /* query the file type */
  type = g_file_query_file_type (file, flags, cancellable);

  /* abort if the job was cancelled */
  if (job != NULL && exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return NULL;

  /* ignore non-directory nodes */
  if (type != G_FILE_TYPE_DIRECTORY)
    return NULL;

  /* determine the namespace */
  if (return_thunar_files)
    namespace = THUNARX_FILE_INFO_NAMESPACE;
  else
    namespace = G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                G_FILE_ATTRIBUTE_STANDARD_NAME ", recent::*";

  /* try to read from the direectory */
  enumerator = g_file_enumerate_children (file, namespace,
                                          flags, cancellable, &err);

  /* abort if there was an error or the job was cancelled */
  if (err != NULL)
    {
      g_propagate_error (error, err);
      return NULL;
    }

  /* iterate over children one by one */
  while (job == NULL || !exo_job_is_cancelled (EXO_JOB (job)))
    {
      /* query info of the child */
      info = g_file_enumerator_next_file (enumerator, cancellable, &err);

      /* break when end of enumerator is reached */
      if (G_UNLIKELY (info == NULL && err == NULL))
        break;

      if (G_UNLIKELY (n_files_max != NULL))
        {
          if (*n_files_max == 0)
            break;
          else
            (*n_files_max)--;
        }

      is_mounted = TRUE;
      if (err != NULL)
        {
          if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_NOT_MOUNTED))
            {
              is_mounted = FALSE;
              g_clear_error (&err);
            }
          else
            {
              if (info != NULL)
                g_warning ("Error while scanning file: %s : %s", g_file_info_get_display_name (info), err->message);
              else
                g_warning ("Error while scanning directory: %s : %s", g_file_get_uri (file), err->message);

              if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_FAILED))
                {
                  /* ignore any other IO error and continue processing the
                   * remaining files */
                  g_clear_error(&err);
                  continue;
                }
              else
                {
                  /* break on other errors */
                  break;
                }
            }
        }

      /* check if we are scanning `recent:///` */
      if (g_file_has_uri_scheme (file, "recent"))
        {
          /* create Gfile using the target URI */
          child_file = g_file_new_for_uri (g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI));

          /* create new file info using Gfile*/
          recent_info = info;
          info = g_file_query_info (child_file, namespace, flags, cancellable, &err);

          if (G_UNLIKELY (info == NULL))
            {
              g_object_unref (recent_info);
              break;
            }
        }
      else
        {
          /* create GFile for the child */
          child_file = g_file_get_child (file, g_file_info_get_name (info));
          recent_info = NULL;
        }

      if (return_thunar_files)
        {
          /* Prepend the ThunarFile */
          thunar_file = thunar_file_get_with_info (child_file, info, recent_info, !is_mounted);
          files = thunar_g_list_prepend_deep (files, thunar_file);
          g_object_unref (G_OBJECT (thunar_file));
        }
      else
        {
          /* Prepend the GFile */
          files = thunar_g_list_prepend_deep (files, child_file);
        }

      /* if the child is a directory and we need to recurse ... just do so */
      if (recursively
          && is_mounted
          && g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        {
          child_files = thunar_io_scan_directory (job, child_file, flags, recursively,
                                                  unlinking, return_thunar_files, n_files_max, &err);

          /* prepend children to the file list to make sure they're
           * processed first (required for unlinking) */
          files = g_list_concat (child_files, files);
        }

      g_object_unref (child_file);
      g_object_unref (info);
    }

  /* release the enumerator */
  g_object_unref (enumerator);

  if (G_UNLIKELY (err != NULL))
    {
      g_propagate_error (error, err);
      thunar_g_list_free_full (files);
      return NULL;
    }
  else if (job != NULL && exo_job_set_error_if_cancelled (EXO_JOB (job), &err))
    {
      g_propagate_error (error, err);
      thunar_g_list_free_full (files);
      return NULL;
    }

  return files;
}
