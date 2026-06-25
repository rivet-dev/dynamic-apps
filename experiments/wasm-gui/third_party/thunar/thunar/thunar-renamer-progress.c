/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-private.h>
#include <thunar/thunar-renamer-progress.h>
#include <thunar/thunar-util.h>



/* Signal identifiers */
enum
{
  FINISHED,
  LAST_SIGNAL,
};



static void     thunar_renamer_progress_finalize          (GObject               *object);
static void     thunar_renamer_progress_destroy           (GtkWidget             *object);
static gboolean thunar_renamer_progress_next_idle         (gpointer               user_data);
static void     thunar_renamer_progress_next_idle_destroy (gpointer               user_data);
static void     thunar_renamer_progress_run_helper        (ThunarRenamerProgress *renamer_progress,
                                                           GList                 *pairs);
static void     thunar_renamer_progress_run_error_dialog  (ThunarRenamerProgress *renamer_progress,
                                                           ThunarRenamerPair     *pair,
                                                           GError                *error);



struct _ThunarRenamerProgressClass
{
  GtkAlignmentClass __parent__;
};

struct _ThunarRenamerProgress
{
  GtkAlignment __parent__;
  GtkWidget   *bar;

  GList       *pairs_renamed_current_run; /* list of pairs renamed in a given run */
  GList       *pairs_renamed_all_runs;    /* list of pairs renamed across all runs */
  GList       *pairs_failed_current_run;
  GList       *pairs_pending_current_run;
  gboolean     show_dialog_on_error;
  gboolean     pairs_undo;                /* whether we're undoing previous changes */

  /* the user may cancel the renaming operation while there are still failed pairs and runs remaining */
  gboolean     cancel_all_remaining_runs;

  /* internal main loop for the _rename() method */
  guint        next_idle_id;
  GMainLoop   *next_idle_loop;
};



G_DEFINE_TYPE (ThunarRenamerProgress, thunar_renamer_progress, GTK_TYPE_BOX)



static void
thunar_renamer_progress_class_init (ThunarRenamerProgressClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_renamer_progress_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->destroy = thunar_renamer_progress_destroy;
}



static void
thunar_renamer_progress_init (ThunarRenamerProgress *renamer_progress)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (renamer_progress), GTK_ORIENTATION_HORIZONTAL);

  renamer_progress->bar = gtk_progress_bar_new ();
  gtk_widget_set_hexpand (renamer_progress->bar, TRUE);
  gtk_container_add (GTK_CONTAINER (renamer_progress), renamer_progress->bar);
  gtk_widget_show (renamer_progress->bar);
}



static void
thunar_renamer_progress_finalize (GObject *object)
{
  ThunarRenamerProgress *renamer_progress = THUNAR_RENAMER_PROGRESS (object);

  /* make sure we're not finalized while the main loop is active */
  _thunar_assert (renamer_progress->next_idle_id == 0);
  _thunar_assert (renamer_progress->next_idle_loop == NULL);

  /* release the pairs */
  thunar_renamer_pair_list_free (renamer_progress->pairs_renamed_current_run);
  thunar_renamer_pair_list_free (renamer_progress->pairs_pending_current_run);

  (*G_OBJECT_CLASS (thunar_renamer_progress_parent_class)->finalize) (object);
}



static void
thunar_renamer_progress_destroy (GtkWidget *object)
{
  ThunarRenamerProgress *renamer_progress = THUNAR_RENAMER_PROGRESS (object);

  /* exit the internal main loop on destroy */
  thunar_renamer_progress_cancel (renamer_progress);

  (*GTK_WIDGET_CLASS (thunar_renamer_progress_parent_class)->destroy) (object);
}



static void
thunar_renamer_progress_run_error_dialog (ThunarRenamerProgress *renamer_progress,
                                          ThunarRenamerPair     *pair,
                                          GError                *error)
{
  gchar     *oldname;
  GtkWindow *toplevel;
  GtkWidget *message;
  gint       response;

  oldname = g_strdup (thunar_file_get_display_name (pair->file));

  /* determine the toplevel widget */
  toplevel = (GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (renamer_progress));

  /* tell the user that we failed */
  message = gtk_message_dialog_new (toplevel,
                                    GTK_DIALOG_DESTROY_WITH_PARENT
                                    | GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_NONE,
                                    _("Failed to rename \"%s\" to \"%s\"."),
                                    oldname, pair->name);

  gtk_window_set_title (GTK_WINDOW (message), _("Error"));

  /* release old name */
  g_free (oldname);

  /* check if we should provide undo */
  if (!renamer_progress->pairs_undo && renamer_progress->pairs_renamed_all_runs != NULL)
    {
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
                                                _("You can either choose to skip this file and continue to rename the "
                                                  "remaining files, or revert the previously renamed files to their "
                                                  "previous names, or cancel the operation without reverting previous "
                                                  "changes."));
      gtk_dialog_add_button (GTK_DIALOG (message), _("_Cancel"), GTK_RESPONSE_CANCEL);
      gtk_dialog_add_button (GTK_DIALOG (message), _("_Revert Changes"), GTK_RESPONSE_REJECT);
      gtk_dialog_add_button (GTK_DIALOG (message), _("_Skip This File"), GTK_RESPONSE_ACCEPT);
      gtk_dialog_set_default_response (GTK_DIALOG (message), GTK_RESPONSE_ACCEPT);
    }
  else if (renamer_progress->pairs_pending_current_run != NULL)
    {
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
                                                _("Do you want to skip this file and continue to rename the "
                                                  "remaining files?"));
      gtk_dialog_add_button (GTK_DIALOG (message), _("_Cancel"), GTK_RESPONSE_CANCEL);
      gtk_dialog_add_button (GTK_DIALOG (message), _("_Skip This File"), GTK_RESPONSE_ACCEPT);
      gtk_dialog_set_default_response (GTK_DIALOG (message), GTK_RESPONSE_ACCEPT);
    }
  else
    {
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s.", error->message);
      gtk_dialog_add_button (GTK_DIALOG (message), _("_Close"), GTK_RESPONSE_CANCEL);
    }

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (message));
  if (response == GTK_RESPONSE_REJECT)
    {
      /* undo previous changes */
      renamer_progress->pairs_undo = TRUE;

      /* release the todo pairs and use the done as todo */
      thunar_renamer_pair_list_free (renamer_progress->pairs_pending_current_run);
      renamer_progress->pairs_pending_current_run = renamer_progress->pairs_renamed_all_runs;
      renamer_progress->pairs_renamed_all_runs = NULL;
    }
  else if (response != GTK_RESPONSE_ACCEPT)
    {
      /* canceled, just exit the main loop */
      g_main_loop_quit (renamer_progress->next_idle_loop);
    }

  /* destroy the dialog */
  gtk_widget_destroy (message);
}



static gboolean
thunar_renamer_progress_next_idle (gpointer user_data)
{
  ThunarRenamerProgress *renamer_progress = THUNAR_RENAMER_PROGRESS (user_data);
  ThunarRenamerPair     *pair;
  GError                *error = NULL;
  gchar                 *oldname;
  gchar                  text[128];
  guint                  n_pairs_processed;
  guint                  n_total;
  GList                 *first;

THUNAR_THREADS_ENTER

  /* check if we have still pairs to go */
  if (G_LIKELY (renamer_progress->pairs_pending_current_run != NULL))
    {
      /* pop the first pair from the todo list */
      first = g_list_first (renamer_progress->pairs_pending_current_run);
      pair = first->data;
      renamer_progress->pairs_pending_current_run = g_list_delete_link (renamer_progress->pairs_pending_current_run, first);

      /* determine the done/todo items */
      n_pairs_processed = g_list_length (renamer_progress->pairs_renamed_current_run) + g_list_length (renamer_progress->pairs_failed_current_run) + 1;
      n_total = n_pairs_processed + g_list_length (renamer_progress->pairs_pending_current_run);

      /* update the progress bar text */
      g_snprintf (text, sizeof (text), "%d/%d", n_pairs_processed, n_total);
      gtk_progress_bar_set_text (GTK_PROGRESS_BAR (renamer_progress->bar), text);

      /* update the progress bar fraction */
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (renamer_progress->bar), CLAMP ((gdouble) n_pairs_processed / MAX (n_total, 1), 0.0, 1.0));

      /* try to rename the file */
      if (!thunar_file_rename (pair->file, pair->name, NULL, FALSE, &error))
        {
          /* add pair to the list of failed pairs */
          renamer_progress->pairs_failed_current_run = g_list_prepend (renamer_progress->pairs_failed_current_run, pair);

          /* check if error dialog box is to de displayed*/
          if (renamer_progress->show_dialog_on_error)
            thunar_renamer_progress_run_error_dialog (renamer_progress, pair, error);

          /* clear the error */
          g_clear_error (&error);
        }
      else
        {
          /* remember the old file name (for undo) */
          oldname = g_strdup (thunar_file_get_display_name (pair->file));

          /* replace the newname with the oldname for the pair (-> undo) */
          g_free (pair->name);
          pair->name = oldname;

          /* move the pair to the list of completed pairs */
          renamer_progress->pairs_renamed_current_run = g_list_prepend (renamer_progress->pairs_renamed_current_run, pair);
          renamer_progress->pairs_renamed_all_runs = g_list_prepend (renamer_progress->pairs_renamed_all_runs, thunar_renamer_pair_copy (pair));
        }
    }

  /* be sure to cancel the internal loop once we're done */
  if (G_UNLIKELY (renamer_progress->pairs_pending_current_run == NULL))
    g_main_loop_quit (renamer_progress->next_idle_loop);

THUNAR_THREADS_LEAVE

  /* keep the idle source alive as long as we have anything to do */
  return (renamer_progress->pairs_pending_current_run != NULL);
}



static void
thunar_renamer_progress_next_idle_destroy (gpointer user_data)
{
  THUNAR_RENAMER_PROGRESS (user_data)->next_idle_id = 0;
}



/**
 * thunar_renamer_progress_new:
 *
 * Allocates a new #ThunarRenamerProgress instance.
 *
 * Return value: the newly allocated #ThunarRenamerProgress.
 **/
GtkWidget*
thunar_renamer_progress_new (void)
{
  return g_object_new (THUNAR_TYPE_RENAMER_PROGRESS, NULL);
}



/**
 * thunar_renamer_progress_cancel:
 * @renamer_progress : a #ThunarRenamerProgress.
 *
 * Cancels any pending rename operation for @renamer_progress.
 **/
void
thunar_renamer_progress_cancel (ThunarRenamerProgress *renamer_progress)
{
  _thunar_return_if_fail (THUNAR_IS_RENAMER_PROGRESS (renamer_progress));

  /* exit the internal main loop (if any) */
  if (G_UNLIKELY (renamer_progress->next_idle_loop != NULL))
    g_main_loop_quit (renamer_progress->next_idle_loop);

  renamer_progress->cancel_all_remaining_runs = TRUE;
}



/**
 * thunar_renamer_progress_running:
 * @renamer_progress : a #ThunarRenamerProgress.
 *
 * Returns %TRUE if @renamer_progress is running.
 *
 * Return value: %TRUE if @renamer_progress is running.
 **/
gboolean
thunar_renamer_progress_running (ThunarRenamerProgress *renamer_progress)
{
  _thunar_return_val_if_fail (THUNAR_IS_RENAMER_PROGRESS (renamer_progress), FALSE);
  return (renamer_progress->next_idle_loop != NULL);
}



/**
 * thunar_renamer_progress_run_helper:
 * @renamer_progress : a #ThunarRenamerProgress.
 * @pair_list        : a #GList of #ThunarRenamePair<!---->s.
 *
 * Renames all #ThunarRenamePair<!---->s in the specified @pair_list
 * using the @renamer_progress.
 *
 * A helper function to be used by thunar_renamer_progress_run function.
 * This method starts a new main loop, and returns only after the
 * rename operation is done (or cancelled by a "destroy" signal).
 **/
static void
thunar_renamer_progress_run_helper (ThunarRenamerProgress *renamer_progress,
                                    GList                 *pairs)
{
  _thunar_return_if_fail (THUNAR_IS_RENAMER_PROGRESS (renamer_progress));

  /* make sure we're not already renaming */
  if (G_UNLIKELY (renamer_progress->next_idle_id != 0
      || renamer_progress->next_idle_loop != NULL))
    return;

  /* make sure to release the list of completed items */
  thunar_renamer_pair_list_free (renamer_progress->pairs_renamed_current_run);
  renamer_progress->pairs_renamed_current_run = NULL;

  /* make sure to release the list of failed items */
  thunar_renamer_pair_list_free (renamer_progress->pairs_failed_current_run);
  renamer_progress->pairs_failed_current_run = NULL;

  /* set the pairs on the todo list */
  thunar_renamer_pair_list_free (renamer_progress->pairs_pending_current_run);
  renamer_progress->pairs_pending_current_run = thunar_renamer_pair_list_copy (pairs);

  /* schedule the idle source */
  renamer_progress->next_idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_renamer_progress_next_idle,
                                                    renamer_progress, thunar_renamer_progress_next_idle_destroy);

  /* run the inner main loop */
  renamer_progress->next_idle_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (renamer_progress->next_idle_loop);
  g_main_loop_unref (renamer_progress->next_idle_loop);
  renamer_progress->next_idle_loop = NULL;

  /* be sure to cancel any pending idle source */
  if (G_UNLIKELY (renamer_progress->next_idle_id != 0))
    g_source_remove (renamer_progress->next_idle_id);

  /* release the list of completed items */
  thunar_renamer_pair_list_free (renamer_progress->pairs_renamed_current_run);
  renamer_progress->pairs_renamed_current_run = NULL;

  /* release the list of todo items */
  thunar_renamer_pair_list_free (renamer_progress->pairs_pending_current_run);
  renamer_progress->pairs_pending_current_run = NULL;
}



/**
 * thunar_renamer_progress_run:
 * @renamer_progress : a #ThunarRenamerProgress.
 * @pair_list        : a #GList of #ThunarRenamePair<!---->s.
 *
 * Renames all #ThunarRenamePair<!---->s in the specified @pair_list
 * using the @renamer_progress.
 *
 * This method uses thunar_renamer_progress_run_helper function to
 * rename all the given pairs. It first tries to rename all the pairs
 * and stores all the failed pairs. Then it sorts the failed pairs in
 * ascending order and again tries to rename them. If still some pairs
 * are left then it sorts them in descending order and then tries to
 * rename them.
 **/
void
thunar_renamer_progress_run (ThunarRenamerProgress *renamer_progress,
                             GList                 *pairs)
{
  _thunar_return_if_fail (THUNAR_IS_RENAMER_PROGRESS (renamer_progress));

  /* make sure we're not already renaming */
  if (G_UNLIKELY (renamer_progress->next_idle_id != 0
      || renamer_progress->next_idle_loop != NULL))
    return;

  /* take an additional reference on the progress */
  g_object_ref (G_OBJECT (renamer_progress));

  /* initialize all the boolean variables to false */
  renamer_progress->show_dialog_on_error = FALSE;
  renamer_progress->cancel_all_remaining_runs = FALSE;
  renamer_progress->pairs_undo = FALSE;

  /* make sure to release the list of total renamed items first */
  thunar_renamer_pair_list_free (renamer_progress->pairs_renamed_all_runs);
  renamer_progress->pairs_renamed_all_runs = NULL;

  /* try to rename all the files for the first time */
  thunar_renamer_progress_run_helper (renamer_progress, pairs);
  pairs = NULL;

  /**
   * while renaming a file, the new name can match with an existing file name, which is yet to be renamed.
   * by sorting them in ascending and descending sort, files causing such conflicts will be renamed first, and thus renamer can work.
   * try to rename all the failed files after sorting them in ascending order.
   **/
  if (!renamer_progress->cancel_all_remaining_runs && renamer_progress->pairs_failed_current_run != NULL)
    {
      GList *temp_pairs;
      temp_pairs = thunar_renamer_pair_list_copy (renamer_progress->pairs_failed_current_run);
      temp_pairs = g_list_sort (temp_pairs, thunar_renamer_pair_compare_ascending);
      thunar_renamer_progress_run_helper (renamer_progress, temp_pairs);
    }

  /* try to rename all the failed files after sorting them in descending order */
  if (!renamer_progress->cancel_all_remaining_runs && renamer_progress->pairs_failed_current_run != NULL)
    {
      GList *temp_pairs;

      /* make sure to show the error dialog box */
      renamer_progress->show_dialog_on_error = TRUE;

      temp_pairs = thunar_renamer_pair_list_copy (renamer_progress->pairs_failed_current_run);
      temp_pairs = g_list_sort (temp_pairs, thunar_renamer_pair_compare_descending);
      thunar_renamer_progress_run_helper (renamer_progress, temp_pairs);
    }

  /* release the list of completed items */
  thunar_renamer_pair_list_free (renamer_progress->pairs_failed_current_run);
  renamer_progress->pairs_failed_current_run = NULL;

  /* release the additional reference on the progress */
  g_object_unref (G_OBJECT (renamer_progress));
}


