/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2012 Nick Schermer <nick@xfce.org>
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

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <thunar/thunar-deep-count-job.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-size-label.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_LABEL_TYPE,
  PROP_FILES,
  PROP_FILE_SIZE_BINARY
};



static void     thunar_size_label_finalize              (GObject              *object);
static void     thunar_size_label_get_property          (GObject              *object,
                                                         guint                 prop_id,
                                                         GValue               *value,
                                                         GParamSpec           *pspec);
static void     thunar_size_label_set_property          (GObject              *object,
                                                         guint                 prop_id,
                                                         const GValue         *value,
                                                         GParamSpec           *pspec);
static gboolean thunar_size_label_button_press_event    (GtkWidget            *ebox,
                                                         GdkEventButton       *event,
                                                         ThunarSizeLabel      *size_label);
static void     thunar_size_label_files_changed         (ThunarSizeLabel      *size_label);
static void     thunar_size_label_error                 (ExoJob               *job,
                                                         const GError         *error,
                                                         ThunarSizeLabel      *size_label);
static void     thunar_size_label_finished              (ExoJob               *job,
                                                         ThunarSizeLabel      *size_label);
static void     thunar_size_label_status_update         (ThunarDeepCountJob   *job,
                                                         guint64               total_size,
                                                         guint                 file_count,
                                                         guint                 directory_count,
                                                         guint                 unreadable_directory_count,
                                                         ThunarSizeLabel      *size_label);
static GList   *thunar_size_label_get_files             (ThunarSizeLabel      *size_label);
static void     thunar_size_label_set_files             (ThunarSizeLabel      *size_label,
                                                         GList                *files);



struct _ThunarSizeLabelClass
{
  GtkHBoxClass __parent__;
};

struct _ThunarSizeLabel
{
  GtkHBox             __parent__;

  ThunarDeepCountJob *job;
  ThunarPreferences  *preferences;

  GList              *files;
  gboolean            file_size_binary;

  GtkWidget          *label;
  GtkWidget          *spinner;

  /* detailed type of the thunar size label */
  ThunarSizeLabelType type;
};



G_DEFINE_TYPE (ThunarSizeLabel, thunar_size_label, GTK_TYPE_BOX)



static void
thunar_size_label_class_init (ThunarSizeLabelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_size_label_finalize;
  gobject_class->get_property = thunar_size_label_get_property;
  gobject_class->set_property = thunar_size_label_set_property;

  /**
   * ThunarSizeLabel:label-type:
   *
   * The #ThunarSizeLabelType of this #ThunarSizeLabel.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LABEL_TYPE,
                                   g_param_spec_int ("label-type",
                                                     "label-type",
                                                     "label-type",
                                                     0, N_THUNAR_SIZE_LABEL - 1, 0, // min, max, default
                                                     G_PARAM_WRITABLE
                                                     | G_PARAM_CONSTRUCT_ONLY));

  /**
   * ThunarSizeLabel:file:
   *
   * The #ThunarFile whose size should be displayed
   * by this #ThunarSizeLabel.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILES,
                                   g_param_spec_boxed ("files", "files", "files",
                                                       THUNARX_TYPE_FILE_INFO_LIST,
                                                       EXO_PARAM_READWRITE));

  /**
   * ThunarPropertiesDialog:file_size_binary:
   *
   * Whether the file size should be shown in binary or decimal.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE_SIZE_BINARY,
                                   g_param_spec_boolean ("file-size-binary",
                                                         "FileSizeBinary",
                                                         NULL,
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_size_label_init (ThunarSizeLabel *size_label)
{
  GtkWidget *ebox;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (size_label), GTK_ORIENTATION_HORIZONTAL);

  /* binary file size */
  size_label->preferences = thunar_preferences_get ();
  g_object_bind_property (G_OBJECT (size_label->preferences),
                          "misc-file-size-binary",
                          G_OBJECT (size_label),
                          "file-size-binary",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (G_OBJECT (size_label->preferences), "notify::misc-file-size-binary",
                            G_CALLBACK (thunar_size_label_files_changed), size_label);

  /* configure the box */
  gtk_box_set_spacing (GTK_BOX (size_label), 6);

  /* add an evenbox for the spinner */
  ebox = gtk_event_box_new ();
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);
  g_signal_connect (G_OBJECT (ebox), "button-press-event", G_CALLBACK (thunar_size_label_button_press_event), size_label);
  gtk_widget_set_tooltip_text (ebox, _("Click here to stop calculating the total size of the folder."));
  gtk_box_pack_start (GTK_BOX (size_label), ebox, FALSE, FALSE, 0);

  /* add the spinner widget */
  size_label->spinner = gtk_spinner_new ();
  g_object_bind_property (G_OBJECT (size_label->spinner), "visible", G_OBJECT (ebox), "visible", G_BINDING_SYNC_CREATE);
  gtk_container_add (GTK_CONTAINER (ebox), size_label->spinner);
  gtk_widget_show (size_label->spinner);

  /* add the label widget */
  size_label->label = gtk_label_new (_("Calculating..."));
  gtk_label_set_xalign (GTK_LABEL (size_label->label), 0.0f);
  gtk_label_set_selectable (GTK_LABEL (size_label->label), TRUE);
  gtk_label_set_line_wrap (GTK_LABEL (size_label->label), TRUE);
  gtk_box_pack_start (GTK_BOX (size_label), size_label->label, TRUE, TRUE, 0);
  gtk_widget_show (size_label->label);
}



static void
thunar_size_label_finalize (GObject *object)
{
  ThunarSizeLabel *size_label = THUNAR_SIZE_LABEL (object);

  /* cancel the pending job (if any) */
  if (G_UNLIKELY (size_label->job != NULL))
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (size_label->job), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, size_label);
      exo_job_cancel (EXO_JOB (size_label->job));
      g_object_unref (size_label->job);
    }

  /* reset the file property */
  thunar_size_label_set_files (size_label, NULL);

  /* disconnect from the preferences */
  g_signal_handlers_disconnect_by_func (size_label->preferences, thunar_size_label_files_changed, size_label);
  g_object_unref (size_label->preferences);

  (*G_OBJECT_CLASS (thunar_size_label_parent_class)->finalize) (object);
}



static void
thunar_size_label_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ThunarSizeLabel *size_label = THUNAR_SIZE_LABEL (object);

  switch (prop_id)
    {
    case PROP_FILES:
      g_value_set_boxed (value, thunar_size_label_get_files (size_label));
      break;

    case PROP_FILE_SIZE_BINARY:
      g_value_set_boolean (value, size_label->file_size_binary);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_size_label_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ThunarSizeLabel *size_label = THUNAR_SIZE_LABEL (object);

  switch (prop_id)
    {
    case PROP_LABEL_TYPE:
      size_label->type = g_value_get_int (value);
      break;

    case PROP_FILES:
      thunar_size_label_set_files (size_label, g_value_get_boxed (value));
      break;

    case PROP_FILE_SIZE_BINARY:
      size_label->file_size_binary = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_size_label_button_press_event (GtkWidget       *ebox,
                                      GdkEventButton  *event,
                                      ThunarSizeLabel *size_label)
{
  _thunar_return_val_if_fail (GTK_IS_EVENT_BOX (ebox), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_SIZE_LABEL (size_label), FALSE);

  /* left button press on the spinner cancels the calculation */
  if (G_LIKELY (event->button == 1))
    {
      /* cancel the pending job (if any) */
      if (G_UNLIKELY (size_label->job != NULL))
        {
          g_signal_handlers_disconnect_matched (size_label->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, size_label);
          exo_job_cancel (EXO_JOB (size_label->job));
          g_object_unref (size_label->job);
          size_label->job = NULL;
        }

      /* be sure to stop and hide the spinner */
      gtk_spinner_stop (GTK_SPINNER (size_label->spinner));
      gtk_widget_hide (size_label->spinner);

      /* tell the user that the operation was canceled */
      gtk_label_set_text (GTK_LABEL (size_label->label), _("Calculation aborted"));

      /* we handled the event */
      return TRUE;
    }

  return FALSE;
}



static void
thunar_size_label_files_changed (ThunarSizeLabel *size_label)
{
  gchar             *size_string;
  guint64            size;

  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  _thunar_return_if_fail (size_label->files != NULL);
  _thunar_return_if_fail (THUNAR_IS_FILE (size_label->files->data));

  /* cancel the pending job (if any) */
  if (G_UNLIKELY (size_label->job != NULL))
    {
      g_signal_handlers_disconnect_matched (size_label->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, size_label);
      exo_job_cancel (EXO_JOB (size_label->job));
      g_object_unref (size_label->job);
      size_label->job = NULL;
    }

  /* check if there are multiple files or the single file is a directory */
  if (size_label->files->next != NULL
      || thunar_file_is_directory (THUNAR_FILE (size_label->files->data)))
    {
      /* schedule a new job to determine the total size of the directory (not following symlinks) */
      size_label->job = thunar_deep_count_job_new (size_label->files, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS);
      g_signal_connect (size_label->job, "error", G_CALLBACK (thunar_size_label_error), size_label);
      g_signal_connect (size_label->job, "finished", G_CALLBACK (thunar_size_label_finished), size_label);
      g_signal_connect (size_label->job, "status-update", G_CALLBACK (thunar_size_label_status_update), size_label);

      /* tell the user that we started calculation */
      gtk_label_set_text (GTK_LABEL (size_label->label), _("Calculating..."));
      gtk_spinner_start (GTK_SPINNER (size_label->spinner));
      gtk_widget_show (size_label->spinner);

      /* launch the job */
      exo_job_launch (EXO_JOB (size_label->job));
    }
  else
    {
      /* this is going to be quick, stop and hide the spinner */
      gtk_spinner_stop (GTK_SPINNER (size_label->spinner));
      gtk_widget_hide (size_label->spinner);

      /* determine the size of the file */
      size = thunar_file_get_size (THUNAR_FILE (size_label->files->data));

      /* setup the new label */
      size_string = g_format_size_full (size, size_label->file_size_binary ? G_FORMAT_SIZE_LONG_FORMAT | G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_LONG_FORMAT);
      gtk_label_set_text (GTK_LABEL (size_label->label), size_string);
      g_free (size_string);
    }
}



static void
thunar_size_label_error (ExoJob          *job,
                         const GError    *error,
                         ThunarSizeLabel *size_label)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  _thunar_return_if_fail (size_label->job == THUNAR_DEEP_COUNT_JOB (job));

  /* setup the error text as label */
  gtk_label_set_text (GTK_LABEL (size_label->label), error->message);
}



static void
thunar_size_label_finished (ExoJob          *job,
                            ThunarSizeLabel *size_label)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  _thunar_return_if_fail (size_label->job == THUNAR_DEEP_COUNT_JOB (job));

  /* stop and hide the spinner */
  gtk_spinner_stop (GTK_SPINNER (size_label->spinner));
  gtk_widget_hide (size_label->spinner);

  /* disconnect from the job */
  g_signal_handlers_disconnect_matched (size_label->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, size_label);
  g_object_unref (size_label->job);
  size_label->job = NULL;
}



static void
thunar_size_label_status_update (ThunarDeepCountJob *job,
                                 guint64             total_size,
                                 guint               file_count,
                                 guint               directory_count,
                                 guint               unreadable_directory_count,
                                 ThunarSizeLabel    *size_label)
{
  gchar             *size_string;
  gchar             *folder_size_string;
  gchar             *file_size_string;
  gchar             *text;
  guint              n;
  gchar             *unreable_text;

  _thunar_return_if_fail (THUNAR_IS_DEEP_COUNT_JOB (job));
  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  _thunar_return_if_fail (size_label->job == job);

  /* determine the total number of items */
  n = file_count + directory_count + unreadable_directory_count;

  if (G_LIKELY (n > unreadable_directory_count))
    {
      if (size_label->type == THUNAR_SIZE_LABEL_SIZE)
        {
          size_string = g_format_size_full (total_size, G_FORMAT_SIZE_LONG_FORMAT | (size_label->file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT));
          text = g_strdup_printf ("%s", size_string);

          gtk_label_set_text (GTK_LABEL (size_label->label), text);
          g_free (size_string);
        }
      else /* if (size_label->type == THUNAR_SIZE_LABEL_CONTENT) */
        {
          folder_size_string = g_strdup_printf (ngettext ("%d folder", "%d folders", directory_count ), directory_count);
          file_size_string = g_strdup_printf (ngettext ("%d file", "%d files", file_count), file_count);
          text = g_strdup_printf (ngettext ("%u item (%s, %s)", "%u items (%s, %s)", n), n, file_size_string, folder_size_string);

          g_free (folder_size_string);
          g_free (file_size_string);
        }

      if (unreadable_directory_count > 0)
        {
          /* TRANSLATORS: this is shows if during the deep count size
           * directories were not accessible */
          unreable_text = g_strconcat (text, "\n", _("(some contents unreadable)"), NULL);
          g_free (text);
          text = unreable_text;
        }

      gtk_label_set_text (GTK_LABEL (size_label->label), text);
      g_free (text);
    }
  else
    {
      /* nothing was readable, so permission was denied */
      gtk_label_set_text (GTK_LABEL (size_label->label), _("Permission denied"));
    }
}



/**
 * thunar_size_label_get_files:
 * @size_label : a #ThunarSizeLabel.
 *
 * Get the files displayed by the @size_label.
 **/
static GList*
thunar_size_label_get_files (ThunarSizeLabel *size_label)
{
  _thunar_return_val_if_fail (THUNAR_IS_SIZE_LABEL (size_label), NULL);
  return size_label->files;
}



/**
 * thunar_size_label_set_files:
 * @size_label : a #ThunarSizeLabel.
 * @files      : a list of #ThunarFile's or %NULL.
 *
 * Sets @file as the #ThunarFile displayed by the @size_label.
 **/
static void
thunar_size_label_set_files (ThunarSizeLabel *size_label,
                             GList           *files)
{
  GList *lp;

  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  _thunar_return_if_fail (files == NULL || THUNAR_IS_FILE (files->data));

  /* disconnect from the previous files */
  for (lp = size_label->files; lp != NULL; lp = lp->next)
    {
      _thunar_assert (THUNAR_IS_FILE (lp->data));

      g_signal_handlers_disconnect_by_func (G_OBJECT (lp->data), thunar_size_label_files_changed, size_label);
      g_object_unref (G_OBJECT (lp->data));
    }
  g_list_free (size_label->files);

  size_label->files = g_list_copy (files);

  /* connect to the new file */
  for (lp = size_label->files; lp != NULL; lp = lp->next)
    {
      _thunar_assert (THUNAR_IS_FILE (lp->data));

      g_object_ref (G_OBJECT (lp->data));
      g_signal_connect_swapped (G_OBJECT (lp->data), "changed", G_CALLBACK (thunar_size_label_files_changed), size_label);
    }

  if (size_label->files != NULL)
    thunar_size_label_files_changed (size_label);

  /* notify listeners */
  g_object_notify (G_OBJECT (size_label), "files");
}



/**
 * thunar_size_label_new:
 *
 * Allocates a new #ThunarSizeLabel instance.
 *
 * Return value: the newly allocated #ThunarSizeLabel.
 **/
GtkWidget*
thunar_size_label_new (void)
{
  return g_object_new (THUNAR_TYPE_SIZE_LABEL, "label-type", THUNAR_SIZE_LABEL_SIZE, NULL);
}



/**
 * thunar_content_label_new:
 *
 * Allocates a new #ThunarSizeLabel instance.
 *
 * Return value: the newly allocated #ThunarSizeLabel.
 **/
GtkWidget*
thunar_content_label_new (void)
{
  return g_object_new (THUNAR_TYPE_SIZE_LABEL, "label-type", THUNAR_SIZE_LABEL_CONTENT, NULL);
}