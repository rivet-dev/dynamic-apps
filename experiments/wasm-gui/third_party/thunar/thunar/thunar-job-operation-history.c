/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2022 Alexander Schwinn <alexxcons@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-job-operation-history.h>
#include <thunar/thunar-notify.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <libxfce4ui/libxfce4ui.h>

/**
 * SECTION:thunar-job-operation-history
 * @Short_description: Manages the logging of job operations (copy, move etc.) and undoing and redoing them
 * @Title: ThunarJobOperationHistory
 *
 * The single #ThunarJobOperationHistory instance stores all job operations in a #GList
 * and manages tools to manage the list and the next/previous operations which can be undone/redone */

/* property identifiers */
enum
{
  PROP_0,
  PROP_CAN_UNDO,
  PROP_CAN_REDO,
};


static void thunar_job_operation_history_finalize     (GObject    *object);
static void thunar_job_operation_history_get_property (GObject    *object,
                                                       guint       prop_id,
                                                       GValue     *value,
                                                       GParamSpec *pspec);



struct _ThunarJobOperationHistory
{
  GObject  __parent__;

  /* List of job operations which were logged */
  GList   *job_operation_list;
  gint     job_operation_list_max_size;

  /* since the job operation list, lp_undo and lp_redo all refer to the same memory locations,
   * which may be accessed by different threads, we need to protect this memory with a mutex */
  GMutex   job_operation_list_mutex;

  /* List pointer to the operation which can be undone */
  GList   *lp_undo;

  /* List pointer to the operation which can be redone */
  GList   *lp_redo;
};

static ThunarJobOperationHistory *job_operation_history;

G_DEFINE_TYPE (ThunarJobOperationHistory, thunar_job_operation_history, G_TYPE_OBJECT)



static void
thunar_job_operation_history_class_init (ThunarJobOperationHistoryClass *klass)
{
  GObjectClass      *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = thunar_job_operation_history_get_property;
  gobject_class->finalize = thunar_job_operation_history_finalize;

  g_object_class_install_property (gobject_class,
                                   PROP_CAN_UNDO,
                                   g_param_spec_boolean ("can-undo",
                                                         "can-undo",
                                                         "can-undo",
                                                         FALSE,
                                                         EXO_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_CAN_REDO,
                                   g_param_spec_boolean ("can-redo",
                                                         "can-redo",
                                                         "can-redo",
                                                         FALSE,
                                                         EXO_PARAM_READABLE));
}



static void
thunar_job_operation_history_init (ThunarJobOperationHistory *self)
{
  ThunarPreferences *preferences;

  self->job_operation_list = NULL;
  self->lp_undo = NULL;
  self->lp_redo = NULL;

  preferences = thunar_preferences_get ();
  g_object_get (G_OBJECT (preferences), "misc-undo-redo-history-size", &(self->job_operation_list_max_size), NULL);
  g_object_unref (preferences);

  g_mutex_init (&self->job_operation_list_mutex);
}



static void
thunar_job_operation_history_finalize (GObject *object)
{
  ThunarJobOperationHistory *history = THUNAR_JOB_OPERATION_HISTORY (object);

  _thunar_return_if_fail (THUNAR_IS_JOB_OPERATION_HISTORY (history));

  g_list_free_full (history->job_operation_list, g_object_unref);

  g_mutex_clear (&history->job_operation_list_mutex);

  (*G_OBJECT_CLASS (thunar_job_operation_history_parent_class)->finalize) (object);
}



static void
thunar_job_operation_history_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_CAN_UNDO:
      g_value_set_boolean (value, thunar_job_operation_history_can_undo ());
      break;

    case PROP_CAN_REDO:
      g_value_set_boolean (value, thunar_job_operation_history_can_redo ());
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/**
 * thunar_job_operation_history_get_default:
 *
 * Returns a reference to the default #ThunarJobOperationHistory
 * instance.
 *
 * The caller is responsible to free the returned instance
 * using g_object_unref() when no longer needed.
 *
 * Return value: the default #ThunarJobOperationHistory instance.
 **/
ThunarJobOperationHistory*
thunar_job_operation_history_get_default (void)
{
  if (G_UNLIKELY (job_operation_history == NULL))
    {
      /* allocate the default monitor */
      job_operation_history = g_object_new (THUNAR_TYPE_JOB_OPERATION_HISTORY, NULL);
      g_object_add_weak_pointer (G_OBJECT (job_operation_history),
                                 (gpointer) &job_operation_history);
    }
  else
    {
      /* take a reference for the caller */
      g_object_ref (G_OBJECT (job_operation_history));
    }

  return job_operation_history;
}



/**
 * thunar_job_operation_history_commit:
 * @job_operation: a #ThunarJobOperation
 *
 * Commits, or registers, the given thunar_job_operation, adding the job operation
 * to the job operation list.
 **/
void
thunar_job_operation_history_commit (ThunarJobOperation *job_operation)
{
  GList* new_list = NULL;

  _thunar_return_if_fail (THUNAR_IS_JOB_OPERATION (job_operation));

  if (thunar_job_operation_get_kind (job_operation) == THUNAR_JOB_OPERATION_KIND_TRASH)
    {
      /* set the timestamp for the operation, in seconds. g_get_real_time gives
       * us the time in microseconds, so we need to divide by 1e6. */
      thunar_job_operation_set_end_timestamp (job_operation, g_get_real_time () / (gint64) 1e6);
    }

  g_mutex_lock (&job_operation_history->job_operation_list_mutex);

  /* When a new operation is added, drop all previous operations which were undone from the list */
  if (job_operation_history->lp_redo != NULL)
    {
      for (GList* lp = job_operation_history->job_operation_list;
           lp != NULL && lp != job_operation_history->lp_redo;
           lp = lp->next)
        new_list = g_list_append (new_list, g_object_ref (lp->data));
      g_list_free_full (job_operation_history->job_operation_list, g_object_unref);
      job_operation_history->job_operation_list = new_list;
    }

  /* Add the new operation to our list */
  job_operation_history->job_operation_list = g_list_append (job_operation_history->job_operation_list, g_object_ref (job_operation));

  /* reset the undo pointer to latest operation and clear the redo pointer */
  job_operation_history->lp_undo = g_list_last (job_operation_history->job_operation_list);
  job_operation_history->lp_redo = NULL;

  /* Limit the size of the list */
  if (job_operation_history->job_operation_list_max_size != -1 && g_list_length (job_operation_history->job_operation_list) > (guint)(job_operation_history->job_operation_list_max_size))
    {
      GList* first = g_list_first (job_operation_history->job_operation_list);
      job_operation_history->job_operation_list = g_list_remove_link (job_operation_history->job_operation_list, first);
      g_list_free_full (first, g_object_unref);
    }

  g_mutex_unlock (&job_operation_history->job_operation_list_mutex);

  /* Notify all subscribers of our properties */
  g_object_notify (G_OBJECT (job_operation_history), "can-undo");
  g_object_notify (G_OBJECT (job_operation_history), "can-redo");
}



/**
 * thunar_job_operation_history_update_trash_timestamps:
 * @job_operation: a #ThunarJobOperation
 *
 * Only updates the timestamps of the latest trash operation
 * That is needed after 'redo' of a 'trash' operation,
 * since it requires to set new timestamps (otherwise 'undo' of that operation wont work afterwards)
 **/
void
thunar_job_operation_history_update_trash_timestamps (ThunarJobOperation *job_operation)
{
  _thunar_return_if_fail (THUNAR_IS_JOB_OPERATION (job_operation));

  if (thunar_job_operation_get_kind (job_operation) != THUNAR_JOB_OPERATION_KIND_TRASH)
    return;

  g_mutex_lock (&job_operation_history->job_operation_list_mutex);

  if (job_operation_history->lp_undo == NULL)
    {
      g_mutex_unlock (&job_operation_history->job_operation_list_mutex);
      return;
    }

  if (thunar_job_operation_compare ( THUNAR_JOB_OPERATION (job_operation_history->lp_undo->data), job_operation) == 0)
    {
      gint64 start_timestamp, end_timestamp;

      thunar_job_operation_get_timestamps (job_operation, &start_timestamp, &end_timestamp);

      /* set the timestamp for the operation, in seconds. g_get_real_time gives
       * us the time in microseconds, so we need to divide by 1e6. */
      end_timestamp = g_get_real_time () / (gint64) 1e6;

      thunar_job_operation_set_start_timestamp (THUNAR_JOB_OPERATION (job_operation_history->lp_undo->data), start_timestamp);
      thunar_job_operation_set_end_timestamp (THUNAR_JOB_OPERATION (job_operation_history->lp_undo->data), end_timestamp);
    }

  g_mutex_unlock (&job_operation_history->job_operation_list_mutex);
}



/**
 * thunar_job_operation_history_undo:
 *
 * Undoes the latest job operation, by executing its inverse
 **/
void
thunar_job_operation_history_undo (void)
{
  ThunarJobOperation *operation_marker;
  ThunarJobOperation *inverted_operation;
  GString            *warning_body;
  gchar              *file_uri;
  GError             *err = NULL;
  const GList        *overwritten_files;
  gboolean            operation_canceled;

  g_mutex_lock (&job_operation_history->job_operation_list_mutex);

  /* Show a warning in case there is no operation to undo */
  if (job_operation_history->lp_undo == NULL)
    {
      xfce_dialog_show_warning (NULL,
                                _("No operation which can be undone has been performed yet.\n"
                                  "(For some operations undo is not supported)"),
                                _("There is no operation to undo"));
      g_mutex_unlock (&job_operation_history->job_operation_list_mutex);
      return;
    }

  /* the 'marked' operation */
  operation_marker = job_operation_history->lp_undo->data;

  /* fix position undo/redo pointers */
  job_operation_history->lp_redo = job_operation_history->lp_undo;
  job_operation_history->lp_undo = g_list_previous (job_operation_history->lp_undo);

  /* warn the user if the previous operation is empty, since then there is nothing to undo */
  if (thunar_job_operation_empty (operation_marker))
    {

      xfce_dialog_show_warning (NULL,
                                _("The operation you are trying to undo does not have any files "
                                  "associated with it, and thus cannot be undone. "),
                                _("%s operation cannot be undone"), thunar_job_operation_get_kind_nick (operation_marker));
      g_mutex_unlock (&job_operation_history->job_operation_list_mutex);
      return;
    }

    /* if there were files overwritten in the operation, warn about them */
    overwritten_files = thunar_job_operation_get_overwritten_files (operation_marker);
    if (overwritten_files != NULL)
      {
        gint index;

        index = 1; /* one indexed for the dialog */
        warning_body = g_string_new (_("The following files were overwritten in the operation "
                                       "you are trying to undo and cannot be restored:\n"));

        for (const GList *lp = overwritten_files; lp != NULL; lp = lp->next, index++)
          {
            file_uri = g_file_get_uri (lp->data);
            g_string_append_printf (warning_body, "%d. %s\n", index, file_uri);
            g_free (file_uri);
          }

        xfce_dialog_show_warning (NULL,
                                  warning_body->str,
                                  _("%s operation can only be partially undone"),
                                  thunar_job_operation_get_kind_nick (operation_marker));

        g_string_free (warning_body, TRUE);
      }

    inverted_operation = thunar_job_operation_new_invert (operation_marker);
    operation_canceled = thunar_job_operation_execute (inverted_operation, &err);
    g_object_unref (inverted_operation);

    if (err == NULL && !operation_canceled)
      thunar_notify_undo (operation_marker);

    if (err != NULL)
      {
        xfce_dialog_show_warning (NULL,
                                  err->message,
                                  _("Failed to undo operation '%s'"),
                                  thunar_job_operation_get_kind_nick (operation_marker));
        g_clear_error (&err);
      }

    g_mutex_unlock (&job_operation_history->job_operation_list_mutex);

    /* Notify all subscribers of our properties */
    g_object_notify (G_OBJECT (job_operation_history), "can-undo");
    g_object_notify (G_OBJECT (job_operation_history), "can-redo");
}



/**
 * thunar_job_operation_history_redo:
 *
 * Redoes the last job operation which had been undone (if any)
 **/
void
thunar_job_operation_history_redo (void)
{
  ThunarJobOperation *operation_marker;
  GString            *warning_body;
  gchar              *file_uri;
  GError             *err = NULL;
  const GList        *overwritten_files;
  gboolean            operation_canceled;

  g_mutex_lock (&job_operation_history->job_operation_list_mutex);

  /* Show a warning in case there is no operation to undo */
  if (job_operation_history->lp_redo == NULL)
    {
      xfce_dialog_show_warning (NULL,
                                _("No operation which can be redone available.\n"),
                                _("There is no operation to redo"));
      g_mutex_unlock (&job_operation_history->job_operation_list_mutex);
      return;
    }

  /* the 'marked' operation */
  operation_marker = job_operation_history->lp_redo->data;

  /* fix position undo/redo pointers */
  job_operation_history->lp_undo = job_operation_history->lp_redo;
  job_operation_history->lp_redo = g_list_next (job_operation_history->lp_redo);

  /* warn the user if the previous operation is empty, since then there is nothing to undo */
  if (thunar_job_operation_empty (operation_marker))
    {

      xfce_dialog_show_warning (NULL,
                                _("The operation you are trying to redo does not have any files "
                                  "associated with it, and thus cannot be redone. "),
                                _("%s operation cannot be redone"), thunar_job_operation_get_kind_nick (operation_marker));
      g_mutex_unlock (&job_operation_history->job_operation_list_mutex);
      return;
    }

    /* if there were files overwritten in the operation, warn about them */
    overwritten_files = thunar_job_operation_get_overwritten_files (operation_marker);
    if (overwritten_files != NULL)
      {
        gint index;

        index = 1; /* one indexed for the dialog */
        warning_body = g_string_new (_("The following files were overwritten in the operation "
                                       "you are trying to redo and cannot be restored:\n"));

        for (const GList *lp = overwritten_files; lp != NULL; lp = lp->next, index++)
          {
            file_uri = g_file_get_uri (lp->data);
            g_string_append_printf (warning_body, "%d. %s\n", index, file_uri);
            g_free (file_uri);
          }

        xfce_dialog_show_warning (NULL,
                                  warning_body->str,
                                  _("%s operation can only be partially redone"),
                                  thunar_job_operation_get_kind_nick (operation_marker));

        g_string_free (warning_body, TRUE);
      }

    operation_canceled = thunar_job_operation_execute (operation_marker, &err);

    if (err == NULL && !operation_canceled)
      thunar_notify_undo (operation_marker);

    if (err != NULL)
      {
        xfce_dialog_show_warning (NULL,
                                  err->message,
                                  _("Failed to redo operation '%s'"),
                                  thunar_job_operation_get_kind_nick (operation_marker));
        g_clear_error (&err);
      }

    g_mutex_unlock (&job_operation_history->job_operation_list_mutex);

    /* Notify all subscribers of our properties */
    g_object_notify (G_OBJECT (job_operation_history), "can-undo");
    g_object_notify (G_OBJECT (job_operation_history), "can-redo");
}



/* thunar_job_operation_history_can_undo:
 *
 * Returns whether or not there is an operation on the job operation list that can be undone.
 *
 * Return value: A gboolean representing whether or not there is an undoable operation
 **/
gboolean
thunar_job_operation_history_can_undo (void)
{
  g_mutex_lock (&job_operation_history->job_operation_list_mutex);

  if (job_operation_history->lp_undo == NULL)
    {
      g_mutex_unlock (&job_operation_history->job_operation_list_mutex);
      return FALSE;
    }

  if (thunar_job_operation_empty (job_operation_history->lp_undo->data))
    {
      g_mutex_unlock (&job_operation_history->job_operation_list_mutex);
      return FALSE;
    }

  g_mutex_unlock (&job_operation_history->job_operation_list_mutex);
  return TRUE;
}



/* thunar_job_operation_history_can_redo:
 *
 * Returns whether or not there is an operation on the job operation list that can be redone.
 *
 * Return value: A gboolean representing whether or not there is an redoable operation
 **/
gboolean
thunar_job_operation_history_can_redo (void)
{
  g_mutex_lock (&job_operation_history->job_operation_list_mutex);

  if (job_operation_history->lp_redo == NULL)
    {
      g_mutex_unlock (&job_operation_history->job_operation_list_mutex);
      return FALSE;
    }

  if (thunar_job_operation_empty (job_operation_history->lp_redo->data))
    {
      g_mutex_unlock (&job_operation_history->job_operation_list_mutex);
      return FALSE;
    }

  g_mutex_unlock (&job_operation_history->job_operation_list_mutex);
  return TRUE;
}

