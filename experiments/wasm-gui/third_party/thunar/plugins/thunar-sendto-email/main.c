/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
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

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <gtk/gtk.h>

#include <exo/exo.h>

#include <libxfce4util/libxfce4util.h>



typedef struct _TseData TseData;

struct _TseData
{
  GFileInfo *info;
  GFile     *file;
};



/* well known archive types */
static const char *TSE_MIME_TYPES[] = {
  "application/x-7z-compressed",
  "application/x-7z-compressed-tar",
  "application/x-ar",
  "application/x-arj",
  "application/x-brotli",
  "application/x-brotli-compressed-tar",
  "application/x-bzip",
  "application/x-bzip-compressed-tar",
  "application/x-compress",
  "application/x-compressed-tar",
  "application/x-deb",
  "application/x-gtar",
  "application/x-gzip",
  "application/x-lha",
  "application/x-lhz",
  "application/x-lzma",
  "application/x-lzma-compressed-tar",
  "application/x-rar",
  "application/x-rar-compressed",
  "application/x-tar",
  "application/x-xz",
  "application/x-xz-compressed-tar",
  "application/x-zip",
  "application/x-zip-compressed",
  "application/zip",
  "multipart/x-zip",
  "application/x-rpm",
  "application/x-jar",
  "application/x-java-archive",
  "application/x-lzop",
  "application/x-zoo",
  "application/x-cd-image",
  "application/zstd",
  "application/x-zstd-compressed-tar",
};



/* compress response ids */
enum
{
  TSE_RESPONSE_COMPRESS,
  TSE_RESPONSE_PLAIN,
};



static void
tse_error (GError      *error,
           const gchar *format,
           ...)
{
  GtkWidget *dialog;
  va_list    args;
  gchar     *primary_text;

  /* determine the primary error text */
  va_start (args, format);
  primary_text = g_strdup_vprintf (format, args);
  va_end (args);

  /* allocate the error dialog */
  dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s.", primary_text);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Error"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s.", error->message);

  /* display the dialog */
  gtk_dialog_run (GTK_DIALOG (dialog));

  /* cleanup */
  gtk_widget_destroy (dialog);
  g_free (primary_text);
}

/**
 * Check if the single file is already an archive
 **/
static gboolean
tse_file_is_archive (GFileInfo *file_info)
{
  const gchar *content_type;
  guint        n;
  /* determine the content type */
  content_type = g_file_info_get_attribute_string (file_info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
  if (content_type == NULL)
  {
    return FALSE;
  }
  for (n = 0; n < G_N_ELEMENTS (TSE_MIME_TYPES); ++n)
  {
    /* check if this mime type matches */
    if (g_content_type_is_a (content_type, TSE_MIME_TYPES[n]))
    {
      /* yep, that's a match then */
      return TRUE;
    }
  }
  return FALSE;
}


static gint
tse_ask_compress (GList *infos)
{
  TseData     *tse_data;
  GtkWidget   *message;
  guint64      total_size = 0;
  GList       *lp;
  gint         response = TSE_RESPONSE_PLAIN;
  gint         n_archives = 0;
  gint         n_infos = 0;

  /* check the file infos */
  for (lp = infos; lp != NULL; lp = lp->next, ++n_infos)
    {
      /* need to compress if we have any directories */
      tse_data = (TseData *) lp->data;

      if (g_file_info_get_file_type (tse_data->info) == G_FILE_TYPE_DIRECTORY)
        return TSE_RESPONSE_COMPRESS;

      if (tse_file_is_archive (tse_data->info))
        ++n_archives;

      /* add file size to total */
      total_size += g_file_info_get_size (tse_data->info);
    }

  /* check if the total size is larger than 200KiB, or we have more than
   * one file, and at least one of the files is not already an archive.
   */
  if ((n_infos > 1 || total_size > 200 * 1024) && n_infos != n_archives)
    {
      /* check if we have more than one file */
      if (G_LIKELY (n_infos == 1))
        {
          /* ask the user whether to compress the file */
          tse_data = (TseData *) infos->data;
          message = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                            _("Send \"%s\" as compressed archive?"),
                                            g_file_info_get_display_name (tse_data->info));
          gtk_window_set_title (GTK_WINDOW (message), _("Send email"));
          gtk_dialog_add_button (GTK_DIALOG (message), _("_Cancel"), GTK_RESPONSE_CANCEL);
          gtk_dialog_add_button (GTK_DIALOG (message), _("Send _directly"), TSE_RESPONSE_PLAIN);
          gtk_dialog_add_button (GTK_DIALOG (message), _("Send com_pressed"), TSE_RESPONSE_COMPRESS);
          gtk_dialog_set_default_response (GTK_DIALOG (message), TSE_RESPONSE_COMPRESS);
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), _("When sending a file via email, you can either choose to "
                                                                                    "send the file directly, as is, or compress the file before "
                                                                                    "attaching it to an email. It is highly recommended to "
                                                                                    "compress large files before sending them."));
          response = gtk_dialog_run (GTK_DIALOG (message));
          gtk_widget_destroy (message);
        }
      else
        {
          /* ask the user whether to send files as archive */
          message = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                            ngettext ("Send %d file as compressed archive?",
                                                      "Send %d files as compressed archive?",
                                                      n_infos),
                                            n_infos);
          gtk_window_set_title (GTK_WINDOW (message), _("Send email"));
          gtk_dialog_add_button (GTK_DIALOG (message), _("_Cancel"), GTK_RESPONSE_CANCEL);
          gtk_dialog_add_button (GTK_DIALOG (message), _("Send _directly"), TSE_RESPONSE_PLAIN);
          gtk_dialog_add_button (GTK_DIALOG (message), _("Send as _archive"), TSE_RESPONSE_COMPRESS);
          gtk_dialog_set_default_response (GTK_DIALOG (message), TSE_RESPONSE_COMPRESS);
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), _("When sending multiple files via email, you can either choose "
                                                                                    "to send the files directly, attaching multiple files to an "
                                                                                    "email, or send all files compressed into a single archive "
                                                                                    "file and attach the archive. It is highly recommended to "
                                                                                    "send multiple large files as archive."));
          response = gtk_dialog_run (GTK_DIALOG (message));
          gtk_widget_destroy (message);
        }
    }

  return response;
}



static void
tse_child_watch (GPid     pid,
                 gint     status,
                 gpointer user_data)
{
  g_object_set_data (G_OBJECT (user_data), I_("tse-child-status"), GINT_TO_POINTER (status));
  gtk_dialog_response (GTK_DIALOG (user_data), GTK_RESPONSE_YES);
}



static gboolean
tse_progress_cb(gpointer user_data)
{
  GtkWidget *pulse = user_data;

  gtk_progress_bar_pulse(GTK_PROGRESS_BAR(pulse));
  return TRUE;
}

static gboolean
tse_progress (const gchar *working_directory,
              gchar      **argv,
              GError     **error)
{
  GtkWidget *progress;
  GtkWidget *dialog;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *vbox;
  gboolean   succeed = FALSE;
  GPid       pid;
  guint      pulse_timer_id;
  gint       watch_id;
  gint       response;
  gint       status;

  /* try to run the command */
  if (!g_spawn_async (working_directory, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL, NULL, &pid, error))
    return FALSE;

  /* allocate the progress dialog */
  dialog = gtk_dialog_new_with_buttons (_("Compressing files..."),
                                        NULL, 0,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 300, -1);

  /* setup the hbox */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  /* setup the image */
  image = gtk_image_new_from_icon_name ("gnome-package", GTK_ICON_SIZE_DIALOG);
  gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (image, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  /* setup the vbox */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  /* setup the label */
  label = gtk_label_new (_("Compressing files..."));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* setup the progress bar */
  progress = gtk_progress_bar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), progress, FALSE, FALSE, 0);
  gtk_widget_show (progress);

  /* start the child watch */
  watch_id = g_child_watch_add (pid, tse_child_watch, dialog);

  /* start the pulse timer */
  pulse_timer_id = g_timeout_add (125, tse_progress_cb, progress);

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_YES)
    {
      /* check if the command failed */
      status = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog), I_("tse-child-status")));
      if (!WIFEXITED (status) || WEXITSTATUS (status) != 0)
        {
          /* tell the user that the command failed */
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("ZIP command terminated with error %d"), status);
        }
      else
        {
          /* the command succeed */
          succeed = TRUE;
        }
    }
  else
    {
      /* terminate the ZIP command */
      kill (pid, SIGQUIT);
    }

  /* cleanup */
  g_source_remove (pulse_timer_id);
  gtk_widget_destroy (dialog);
  g_source_remove (watch_id);

  return succeed;
}



#ifndef HAVE_MKDTEMP
static gchar*
mkdtemp (gchar *tmpl)
{
  static const gchar LETTERS[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static guint64     value;
  guint64            v;
  gint               len;
  gint               i, j;

  len = strlen (tmpl);
  if (len < 6 || strcmp (&tmpl[len - 6], "XXXXXX") != 0)
    {
      errno = EINVAL;
      return NULL;
    }

  value += ((guint64) time (NULL)) ^ getpid ();

  for (i = 0; i < TMP_MAX; ++i, value += 7777)
    {
      /* fill in the random bits */
      for (j = 0, v = value; j < 6; ++j)
        tmpl[(len - 6) + j] = LETTERS[v % 62]; v /= 62;

      /* try to create the directory */
      if (g_mkdir (tmpl, 0700) == 0)
        return tmpl;
      else if (errno != EEXIST)
        return NULL;
    }

  errno = EEXIST;
  return NULL;
}
#endif



static gboolean
tse_compress (GList  *infos,
              gchar **zipfile_return)
{
  TseData       *tse_data;
  gboolean       succeed = TRUE;
  GError        *error = NULL;
  GFile         *parent;
  GFile         *parent_parent;
  gchar        **argv;
  gchar         *base_name;
  gchar         *zipfile;
  gchar         *tmppath;
  gchar         *tmpdir;
  gchar         *path;
  gchar         *dot;
  GList         *lp;
  gint           n;

  /* create a temporary directory */
  tmpdir = g_strdup ("/tmp/thunar-sendto-email.XXXXXX");
  if (G_UNLIKELY (mkdtemp (tmpdir) == NULL))
    {
      /* tell the user that we failed to create a temporary directory */
      error = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
      tse_error (error, _("Failed to create temporary directory"));
      g_error_free (error);
      return FALSE;
    }

  /* check if we have only a single file/folder to compress */
  if (G_LIKELY (infos != NULL && infos->next == NULL))
    {
      /* determine the name for the ZIP file */
      tse_data = (TseData *) infos->data;
      path = g_strdup (g_file_info_get_display_name (tse_data->info));
      dot = strrchr (path, '.');
      if (G_LIKELY (dot != NULL))
        *dot = '\0';
      zipfile = g_strconcat (tmpdir, G_DIR_SEPARATOR_S, path, ".zip", NULL);
      g_free (path);
    }
  else
    {
      /* determine the name for the ZIP file */
      tse_data = (TseData *) infos->data;
      parent = g_file_get_parent (tse_data->file);

      /* we assume the parent exists because we only allow non-root
       * files as TseData in main () */
      g_assert (parent != NULL);

      parent_parent = g_file_get_parent (parent);

      if (parent_parent != NULL)
        {
          /* use the parent directory's name */
          base_name = g_file_get_basename (parent);
          zipfile = g_strconcat (tmpdir, G_DIR_SEPARATOR_S, base_name, ".zip", NULL);
          g_free (base_name);

          g_object_unref (parent_parent);
        }
      else
        {
          /* use the first file's name */
          zipfile = g_strconcat (tmpdir, G_DIR_SEPARATOR_S, g_file_info_get_display_name (tse_data->info), ".zip", NULL);
        }

      g_object_unref (parent);
    }

  /* generate the argument list for the ZIP command */
  argv = g_new0 (gchar *, g_list_length (infos) + 5);
  argv[0] = g_strdup ("zip");
  argv[1] = g_strdup ("-q");
  argv[2] = g_strdup ("-r");
  argv[3] = g_strdup (zipfile);
  for (lp = infos, n = 4; lp != NULL && succeed; lp = lp->next, ++n)
    {
      /* create a symlink for the file to the tmp dir */
      tse_data = (TseData *) lp->data;
      path = g_file_get_path (tse_data->file);

      if (path == NULL)
        {
          error = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (ENOTSUP), g_strerror (errno));
          tse_error (error, _("Failed to create symbolic link for \"%s\""), g_file_info_get_display_name (tse_data->info));
          g_error_free (error);
        }
      else
        {
          tmppath = g_build_filename (tmpdir, g_file_get_basename (tse_data->file), NULL);
          succeed = (symlink (path, tmppath) == 0);
          if (G_UNLIKELY (!succeed))
            {
              /* tell the user that we failed to create a symlink for this file */
              error = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
              tse_error (error, _("Failed to create symbolic link for \"%s\""), g_file_info_get_display_name (tse_data->info));
              g_error_free (error);
            }
          else
            {
              /* add the file to the ZIP command line */
              argv[n] = g_path_get_basename (tmppath);
            }
          g_free (tmppath);
          g_free (path);
        }
    }

  /* check if we succeed up to this point */
  if (G_LIKELY (succeed))
    {
      /* try to run the ZIP command */
      succeed = tse_progress (tmpdir, argv, &error);
      if (G_UNLIKELY (!succeed))
        {
          /* check if we failed or the user cancelled */
          if (G_LIKELY (error != NULL))
            {
              /* tell the user that we failed to compress the file(s) */
              tse_error (error, ngettext ("Failed to compress %d file",
                                          "Failed to compress %d files",
                                          g_list_length (infos)),
                         g_list_length (infos));
              g_error_free (error);
            }

          /* delete the temporary directory */
          dot = g_strdup_printf ("rm -rf '%s'", tmpdir);
          g_spawn_command_line_sync (dot, NULL, NULL, NULL, NULL);
          g_free (dot);
        }
      else
        {
          /* return the path to the compressed file */
          *zipfile_return = zipfile;
          zipfile = NULL;
        }
    }

  /* cleanup */
  g_strfreev (argv);
  g_free (zipfile);
  g_free (tmpdir);

  return succeed;
}



int
main (int argc, char **argv)
{
  GFileInfo     *info;
  TseData       *tse_data;
  GFile         *file;
  GFile         *parent;
  GString       *mailto;
  GError        *error = NULL;
  GList         *infos = NULL;
  gchar        **attachments = NULL;
  gchar         *zipfile = NULL;
  GList         *lp;
  gint           response;
  gint           n;

  /* setup translation domain */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

#ifdef G_ENABLE_DEBUG
  /* Do NOT remove this line for now, If something doesn't work,
   * fix your code instead!
   */
  g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

  /* initialize Gtk+ */
  gtk_init (&argc, &argv);

  /* set default icon for dialogs */
  gtk_window_set_default_icon_name ("internet-mail");

  /* verify that atleast one file was supplied */
  if (G_UNLIKELY (argc == 1))
    {
      g_fprintf (stderr, "Usage: thunar-sendto-email [FILES...]\n");
      return EXIT_FAILURE;
    }

  /* determine the GFiles and GFileInfos (bundled as TseData) for the files */
  for (n = 1; n < argc; ++n)
    {
      /* try to transform to a GFile */
      file = g_file_new_for_commandline_arg (argv[n]);

      /* verify that we're not trying to send root */
      parent = g_file_get_parent (file);
      if (parent == NULL)
        {
          error = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (EFBIG), g_strerror (EFBIG));
          g_fprintf (stderr, "thunar-sendto-email: Invalid argument \"%s\": %s\n", argv[n], error->message);
          g_error_free (error);
          g_object_unref (file);
          return EXIT_FAILURE;
        }
      else
        {
          g_object_unref (parent);
        }

      /* try to determine the info */
      info = g_file_query_info (file,
                                G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
                                G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                G_FILE_QUERY_INFO_NONE, NULL, &error);

      /* check if we failed */
      if (G_UNLIKELY (info == NULL))
        {
          g_fprintf (stderr, "thunar-sendto-email: Invalid argument \"%s\": %s\n", argv[n], error->message);
          g_error_free (error);
          g_object_unref (file);
          return EXIT_FAILURE;
        }

      tse_data = g_slice_new0 (TseData);
      tse_data->info = info;
      tse_data->file = file;

      /* add to our list of infos */
      infos = g_list_append (infos, tse_data);
    }

  /* check whether to compress the files */
  response = tse_ask_compress (infos);
  if (response == TSE_RESPONSE_COMPRESS)
    {
      /* try to compress the files */
      if (tse_compress (infos, &zipfile))
        {
          /* we have only a single attachment now */
          attachments = g_new0 (gchar *, 2);
          attachments[0] = g_filename_to_uri (zipfile, NULL, NULL);
          g_free (zipfile);
        }
    }
  else if (response == TSE_RESPONSE_PLAIN)
    {
      /* attach the files one by one */
      attachments = g_new0 (gchar *, g_list_length (infos) + 1);
      for (lp = infos, n = 0; lp != NULL; lp = lp->next, ++n)
        {
          tse_data = (TseData *) lp->data;
          attachments[n] = g_file_get_uri (tse_data->file);
        }
    }

  /* check if we have anything to attach */
  if (G_LIKELY (attachments != NULL))
    {
      /* no specific email address */
      mailto = g_string_new ("?");
      for (n = 0; attachments[n] != NULL; ++n)
        g_string_append_printf (mailto, "attach=%s&", attachments[n]);
      g_strfreev (attachments);

      /* open the mail composer */
      if (!exo_execute_preferred_application ("MailReader", mailto->str, NULL, NULL, &error))
        {
          /* tell the user that we failed */
          tse_error (error, _("Failed to compose new email"));
          g_error_free (error);
        }

      /* cleanup */
      g_string_free (mailto, TRUE);
    }

  /* cleanup */
  for (lp = infos; lp != NULL; lp = lp->next)
    {
      tse_data = (TseData *) lp->data;
      g_object_unref (tse_data->file);
      g_object_unref (tse_data->info);
      g_slice_free (TseData, tse_data);
    }
  g_list_free (infos);

  return EXIT_SUCCESS;
}

