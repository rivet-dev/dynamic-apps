/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <exo/exo.h>

#include <libxfce4util/libxfce4util.h>

#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-job.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-private.h>



/* Signal identifiers */
enum
{
  ASK,
  ASK_REPLACE,
  FILES_READY,
  NEW_FILES,
  FROZEN,
  UNFROZEN,
  LAST_SIGNAL,
};



static void              thunar_job_finalize            (GObject            *object);
static ThunarJobResponse thunar_job_real_ask            (ThunarJob          *job,
                                                         const gchar        *message,
                                                         ThunarJobResponse   choices);
static ThunarJobResponse thunar_job_real_ask_replace    (ThunarJob          *job,
                                                         ThunarFile         *source_file,
                                                         ThunarFile         *target_file);



struct _ThunarJobPrivate
{
  ThunarJobResponse         earlier_ask_create_response;
  ThunarJobResponse         earlier_ask_overwrite_response;
  ThunarJobResponse         earlier_ask_delete_response;
  ThunarJobResponse         earlier_ask_skip_response;
  GList                    *total_files;
  guint                     n_total_files;
  gboolean                  pausable;
  gboolean                  paused; /* the job has been manually paused using the UI */
  gboolean                  frozen; /* the job has been automaticaly paused regarding some parallel copy behavior */
  ThunarOperationLogMode    log_mode;
};



static guint job_signals[LAST_SIGNAL];



G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ThunarJob, thunar_job, EXO_TYPE_JOB)



static gboolean
_thunar_job_ask_accumulator (GSignalInvocationHint *ihint,
                             GValue                *return_accu,
                             const GValue          *handler_return,
                             gpointer               data)
{
  g_value_copy (handler_return, return_accu);
  return FALSE;
}



static void
thunar_job_class_init (ThunarJobClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_job_finalize;

  klass->ask = thunar_job_real_ask;
  klass->ask_replace = thunar_job_real_ask_replace;

  /**
   * ThunarJob::ask:
   * @job     : a #ThunarJob.
   * @message : question to display to the user.
   * @choices : a combination of #ThunarJobResponse<!---->s.
   *
   * The @message is garantied to contain valid UTF-8.
   *
   * Return value: the selected choice.
   **/
  job_signals[ASK] =
    g_signal_new (I_("ask"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS | G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarJobClass, ask),
                  _thunar_job_ask_accumulator, NULL,
                  _thunar_marshal_FLAGS__STRING_FLAGS,
                  THUNAR_TYPE_JOB_RESPONSE,
                  2, G_TYPE_STRING,
                  THUNAR_TYPE_JOB_RESPONSE);

  /**
   * ThunarJob::ask-replace:
   * @job      : a #ThunarJob.
   * @src_file : the #ThunarFile of the source file.
   * @dst_file : the #ThunarFile of the destination file, that
   *             may be replaced with the source file.
   *
   * Emitted to ask the user whether the destination file should
   * be replaced by the source file.
   *
   * Return value: the selected choice.
   **/
  job_signals[ASK_REPLACE] =
    g_signal_new (I_("ask-replace"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS | G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarJobClass, ask_replace),
                  _thunar_job_ask_accumulator, NULL,
                  _thunar_marshal_FLAGS__OBJECT_OBJECT,
                  THUNAR_TYPE_JOB_RESPONSE,
                  2, THUNAR_TYPE_FILE, THUNAR_TYPE_FILE);

  /**
   * ThunarJob::files-ready:
   * @job       : a #ThunarJob.
   * @file_list : a list of #ThunarFile<!---->s.
   *
   * This signal is used by #ThunarJob<!---->s returned by
   * the thunar_io_jobs_list_directory() function whenever
   * there's a bunch of #ThunarFile<!---->s ready. This signal
   * is garantied to be never emitted with an @file_list
   * parameter of %NULL.
   *
   * To allow some further optimizations on the handler-side,
   * the handler is allowed to take over ownership of the
   * @file_list, i.e. it can reuse the @infos list and just replace
   * the data elements with it's own objects based on the
   * #ThunarFile<!---->s contained within the @file_list (and
   * of course properly unreffing the previously contained infos).
   * If a handler takes over ownership of @file_list it must return
   * %TRUE here, and no further handlers will be run. Else, if
   * the handler doesn't want to take over ownership of @infos,
   * it must return %FALSE, and other handlers will be run. Use
   * this feature with care, and only if you can be sure that
   * you are the only handler connected to this signal for a
   * given job!
   *
   * Return value: %TRUE if the handler took over ownership of
   *               @file_list, else %FALSE.
   **/
  job_signals[FILES_READY] =
    g_signal_new (I_("files-ready"),
                  G_TYPE_FROM_CLASS (klass), G_SIGNAL_NO_HOOKS,
                  0, g_signal_accumulator_true_handled, NULL,
                  _thunar_marshal_BOOLEAN__POINTER,
                  G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);

  /**
   * ThunarJob::new-files:
   * @job       : a #ThunarJob.
   * @file_list : a list of #GFile<!---->s that were created by @job.
   *
   * This signal is emitted by the @job right before the @job is terminated
   * and informs the application about the list of created files in @file_list.
   * @file_list contains only the toplevel file items, that were specified by
   * the application on creation of the @job.
   **/
  job_signals[NEW_FILES] =
    g_signal_new (I_("new-files"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarJob::frozen:
   * @job       : a #ThunarJob.
   *
   * This signal is emitted by the @job right after the @job is being frozen.
   **/
  job_signals[FROZEN] =
    g_signal_new (I_("frozen"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0,
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 0);

  /**
   * ThunarJob::unfrozen:
   * @job       : a #ThunarJob.
   *
   * This signal is emitted by the @job right after the @job is being unfrozen.
   **/
  job_signals[UNFROZEN] =
    g_signal_new (I_("unfrozen"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS, 0,
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 0);
}



static void
thunar_job_init (ThunarJob *job)
{
  job->priv = thunar_job_get_instance_private (job);
  job->priv->earlier_ask_create_response = 0;
  job->priv->earlier_ask_overwrite_response = 0;
  job->priv->earlier_ask_delete_response = 0;
  job->priv->earlier_ask_skip_response = 0;
  job->priv->n_total_files = 0;
  job->priv->pausable = FALSE;
  job->priv->paused = FALSE;
  job->priv->frozen = FALSE;
}



static void
thunar_job_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (thunar_job_parent_class)->finalize) (object);
}



static ThunarJobResponse
thunar_job_real_ask (ThunarJob        *job,
                     const gchar      *message,
                     ThunarJobResponse choices)
{
  ThunarJobResponse response;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  g_signal_emit (job, job_signals[ASK], 0, message, choices, &response);
  return response;
}



static ThunarJobResponse
thunar_job_real_ask_replace (ThunarJob  *job,
                             ThunarFile *source_file,
                             ThunarFile *target_file)
{
  ThunarJobResponse response;
  gchar            *message;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (source_file), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (target_file), THUNAR_JOB_RESPONSE_CANCEL);

  message = g_strdup_printf (_("The file \"%s\" already exists. Would you like to replace it?\n\n"
                               "If you replace an existing file, its contents will be overwritten."),
                             thunar_file_get_display_name (source_file));

  g_signal_emit (job, job_signals[ASK], 0, message,
                 THUNAR_JOB_RESPONSE_REPLACE
                 | THUNAR_JOB_RESPONSE_REPLACE_ALL
                 | THUNAR_JOB_RESPONSE_RENAME
                 | THUNAR_JOB_RESPONSE_RENAME_ALL
                 | THUNAR_JOB_RESPONSE_SKIP
                 | THUNAR_JOB_RESPONSE_SKIP_ALL
                 | THUNAR_JOB_RESPONSE_CANCEL,
                 &response);

  /* clean up */
  g_free (message);

  return response;
}



static ThunarJobResponse
_thunar_job_ask_valist (ThunarJob        *job,
                        const gchar      *format,
                        va_list           var_args,
                        const gchar      *question,
                        ThunarJobResponse choices)
{
  ThunarJobResponse response;
  gchar            *text;
  gchar            *message;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (g_utf8_validate (format, -1, NULL), THUNAR_JOB_RESPONSE_CANCEL);

  /* generate the dialog message */
  text = g_strdup_vprintf (format, var_args);
  message = (question != NULL)
            ? g_strconcat (text, ".\n\n", question, NULL)
            : g_strconcat (text, ".", NULL);
  g_free (text);

  /* send the question and wait for the answer */
  exo_job_emit (EXO_JOB (job), job_signals[ASK], 0, message, choices, &response);
  g_free (message);

  /* cancel the job as per users request */
  if (G_UNLIKELY (response == THUNAR_JOB_RESPONSE_CANCEL))
    exo_job_cancel (EXO_JOB (job));

  return response;
}



ThunarJobResponse
thunar_job_ask_overwrite (ThunarJob   *job,
                          const gchar *format,
                          ...)
{
  ThunarJobResponse response;
  va_list           var_args;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (format != NULL, THUNAR_JOB_RESPONSE_CANCEL);

  /* check if the user already cancelled the job */
  if (G_UNLIKELY (exo_job_is_cancelled (EXO_JOB (job))))
    return THUNAR_JOB_RESPONSE_CANCEL;

  /* check if the user said "Overwrite All" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_overwrite_response == THUNAR_JOB_RESPONSE_REPLACE_ALL))
    return THUNAR_JOB_RESPONSE_REPLACE;

  /* check if the user said "Overwrite None" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_overwrite_response == THUNAR_JOB_RESPONSE_SKIP_ALL))
    return THUNAR_JOB_RESPONSE_SKIP;

  /* ask the user what he wants to do */
  va_start (var_args, format);
  response = _thunar_job_ask_valist (job, format, var_args,
                                     _("Do you want to overwrite it?"),
                                     THUNAR_JOB_RESPONSE_REPLACE
                                     | THUNAR_JOB_RESPONSE_REPLACE_ALL
                                     | THUNAR_JOB_RESPONSE_SKIP
                                     | THUNAR_JOB_RESPONSE_SKIP_ALL
                                     | THUNAR_JOB_RESPONSE_CANCEL);
  va_end (var_args);

  /* remember response for later */
  job->priv->earlier_ask_overwrite_response = response;

  /* translate response */
  switch (response)
    {
    case THUNAR_JOB_RESPONSE_REPLACE_ALL:
      response = THUNAR_JOB_RESPONSE_REPLACE;
      break;

    case THUNAR_JOB_RESPONSE_SKIP_ALL:
      response = THUNAR_JOB_RESPONSE_SKIP;
      break;

    default:
      break;
    }

  return response;
}



ThunarJobResponse
thunar_job_ask_delete (ThunarJob   *job,
                       const gchar *format,
                       ...)
{
  ThunarJobResponse response;
  va_list           var_args;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (format != NULL, THUNAR_JOB_RESPONSE_CANCEL);

  /* check if the user already cancelled the job */
  if (G_UNLIKELY (exo_job_is_cancelled (EXO_JOB (job))))
    return THUNAR_JOB_RESPONSE_CANCEL;

  /* check if the user said "Delete All" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_delete_response == THUNAR_JOB_RESPONSE_YES_ALL))
    return THUNAR_JOB_RESPONSE_YES;

  /* check if the user said "Delete None" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_delete_response == THUNAR_JOB_RESPONSE_NO_ALL))
    return THUNAR_JOB_RESPONSE_NO;

  /* ask the user what he wants to do */
  va_start (var_args, format);
  response = _thunar_job_ask_valist (job, format, var_args,
                                     _("Do you want to permanently delete it?"),
                                     THUNAR_JOB_RESPONSE_YES
                                     | THUNAR_JOB_RESPONSE_YES_ALL
                                     | THUNAR_JOB_RESPONSE_NO
                                     | THUNAR_JOB_RESPONSE_NO_ALL
                                     | THUNAR_JOB_RESPONSE_CANCEL);
  va_end (var_args);

  /* remember response for later */
  job->priv->earlier_ask_delete_response = response;

  /* translate response */
  switch (response)
    {
    case THUNAR_JOB_RESPONSE_YES_ALL:
      response = THUNAR_JOB_RESPONSE_YES;
      break;

    case THUNAR_JOB_RESPONSE_NO_ALL:
      response = THUNAR_JOB_RESPONSE_NO;
      break;

    default:
      break;
    }

  return response;
}



ThunarJobResponse
thunar_job_ask_create (ThunarJob   *job,
                       const gchar *format,
                       ...)
{
  ThunarJobResponse response;
  va_list           var_args;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);

  if (G_UNLIKELY (exo_job_is_cancelled (EXO_JOB (job))))
    return THUNAR_JOB_RESPONSE_CANCEL;

  /* check if the user said "Create All" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_create_response == THUNAR_JOB_RESPONSE_YES_ALL))
    return THUNAR_JOB_RESPONSE_YES;

  /* check if the user said "Create None" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_create_response == THUNAR_JOB_RESPONSE_NO_ALL))
    return THUNAR_JOB_RESPONSE_NO;

  va_start (var_args, format);
  response = _thunar_job_ask_valist (job, format, var_args,
                                     _("Do you want to create it?"),
                                     THUNAR_JOB_RESPONSE_YES
                                     | THUNAR_JOB_RESPONSE_CANCEL);
  va_end (var_args);

  job->priv->earlier_ask_create_response = response;

  /* translate the response */
  if (response == THUNAR_JOB_RESPONSE_YES_ALL)
    response = THUNAR_JOB_RESPONSE_YES;
  else if (response == THUNAR_JOB_RESPONSE_NO_ALL)
    response = THUNAR_JOB_RESPONSE_NO;
  else if (response == THUNAR_JOB_RESPONSE_CANCEL)
    exo_job_cancel (EXO_JOB (job));

  return response;
}



ThunarJobResponse
thunar_job_ask_replace (ThunarJob *job,
                        GFile     *source_path,
                        GFile     *target_path,
                        GError   **error)
{
  ThunarJobResponse response;
  ThunarFile       *source_file;
  ThunarFile       *target_file;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (G_IS_FILE (source_path), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (G_IS_FILE (target_path), THUNAR_JOB_RESPONSE_CANCEL);

  if (G_UNLIKELY (exo_job_set_error_if_cancelled (EXO_JOB (job), error)))
    return THUNAR_JOB_RESPONSE_CANCEL;

  /* check if the user said "Overwrite All" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_overwrite_response == THUNAR_JOB_RESPONSE_REPLACE_ALL))
    return THUNAR_JOB_RESPONSE_REPLACE;

  /* check if the user said "Rename All" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_overwrite_response == THUNAR_JOB_RESPONSE_RENAME_ALL))
    return THUNAR_JOB_RESPONSE_RENAME;

  /* check if the user said "Overwrite None" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_overwrite_response == THUNAR_JOB_RESPONSE_SKIP_ALL))
    return THUNAR_JOB_RESPONSE_SKIP;

  source_file = thunar_file_get (source_path, error);

  if (G_UNLIKELY (source_file == NULL))
    return THUNAR_JOB_RESPONSE_SKIP;

  target_file = thunar_file_get (target_path, error);

  if (G_UNLIKELY (target_file == NULL))
    {
      g_object_unref (source_file);
      return THUNAR_JOB_RESPONSE_SKIP;
    }

  exo_job_emit (EXO_JOB (job), job_signals[ASK_REPLACE], 0,
                source_file, target_file, &response);

  g_object_unref (source_file);
  g_object_unref (target_file);

  /* remember the response for later */
  job->priv->earlier_ask_overwrite_response = response;

  /* translate the response */
  if (response == THUNAR_JOB_RESPONSE_REPLACE_ALL)
    response = THUNAR_JOB_RESPONSE_REPLACE;
  else if (response == THUNAR_JOB_RESPONSE_RENAME_ALL)
    response = THUNAR_JOB_RESPONSE_RENAME;
  else if (response == THUNAR_JOB_RESPONSE_SKIP_ALL)
    response = THUNAR_JOB_RESPONSE_SKIP;
  else if (response == THUNAR_JOB_RESPONSE_CANCEL)
    exo_job_cancel (EXO_JOB (job));

  return response;
}



ThunarJobResponse
thunar_job_ask_skip (ThunarJob   *job,
                     const gchar *format,
                     ...)
{
  ThunarJobResponse response;
  va_list           var_args;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (format != NULL, THUNAR_JOB_RESPONSE_CANCEL);

  /* check if the user already cancelled the job */
  if (G_UNLIKELY (exo_job_is_cancelled (EXO_JOB (job))))
    return THUNAR_JOB_RESPONSE_CANCEL;

  /* check if the user said "Skip All" earlier */
  if (G_UNLIKELY (job->priv->earlier_ask_skip_response == THUNAR_JOB_RESPONSE_SKIP_ALL))
    return THUNAR_JOB_RESPONSE_SKIP;

  /* ask the user what he wants to do */
  va_start (var_args, format);
  response = _thunar_job_ask_valist (job, format, var_args,
                                     _("Do you want to skip it?"),
                                     THUNAR_JOB_RESPONSE_SKIP
                                     | THUNAR_JOB_RESPONSE_SKIP_ALL
                                     | THUNAR_JOB_RESPONSE_CANCEL
                                     | THUNAR_JOB_RESPONSE_RETRY);
  va_end (var_args);

  /* remember the response */
  job->priv->earlier_ask_skip_response = response;

  /* translate the response */
  switch (response)
    {
    case THUNAR_JOB_RESPONSE_SKIP_ALL:
      response = THUNAR_JOB_RESPONSE_SKIP;
      break;

    case THUNAR_JOB_RESPONSE_SKIP:
    case THUNAR_JOB_RESPONSE_CANCEL:
    case THUNAR_JOB_RESPONSE_RETRY:
      break;

    default:
      _thunar_assert_not_reached ();
    }

  return response;
}



gboolean
thunar_job_ask_no_size (ThunarJob   *job,
                        const gchar *format,
                        ...)
{
  ThunarJobResponse response;
  va_list           var_args;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (format != NULL, THUNAR_JOB_RESPONSE_CANCEL);

  /* check if the user already cancelled the job */
  if (G_UNLIKELY (exo_job_is_cancelled (EXO_JOB (job))))
    return THUNAR_JOB_RESPONSE_CANCEL;

  /* ask the user what he wants to do */
  va_start (var_args, format);
  response = _thunar_job_ask_valist (job, format, var_args,
                                     _("There is not enough space on the destination. Try to remove files to make space."),
                                     THUNAR_JOB_RESPONSE_FORCE
                                     | THUNAR_JOB_RESPONSE_CANCEL);
  va_end (var_args);

  return (response == THUNAR_JOB_RESPONSE_FORCE);
}



gboolean
thunar_job_files_ready (ThunarJob *job,
                        GList     *file_list)
{
  gboolean handled = FALSE;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);

  exo_job_emit (EXO_JOB (job), job_signals[FILES_READY], 0, file_list, &handled);
  return handled;
}



void
thunar_job_new_files (ThunarJob   *job,
                      const GList *file_list)
{
  ThunarFile  *file;
  const GList *lp;

  _thunar_return_if_fail (THUNAR_IS_JOB (job));

  /* check if we have any files */
  if (G_LIKELY (file_list != NULL))
    {
      /* schedule a reload of cached files when idle */
      for (lp = file_list; lp != NULL; lp = lp->next)
        {
          file = thunar_file_cache_lookup (lp->data);
          if (file != NULL)
            {
              thunar_file_reload_idle_unref (file);
            }
        }

      /* emit the "new-files" signal */
      exo_job_emit (EXO_JOB (job), job_signals[NEW_FILES], 0, file_list);
    }
}



void
thunar_job_set_total_files (ThunarJob *job,
                            GList     *total_files)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (job->priv->total_files == NULL);
  _thunar_return_if_fail (total_files != NULL);

  job->priv->total_files = total_files;
  job->priv->n_total_files = g_list_length (total_files);
}



void
thunar_job_set_pausable (ThunarJob *job,
                         gboolean   pausable)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  job->priv->pausable = pausable;
}



gboolean
thunar_job_is_pausable (ThunarJob *job)
{
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  return job->priv->pausable;
}



void
thunar_job_pause (ThunarJob *job)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  job->priv->paused = TRUE;
}



void
thunar_job_resume (ThunarJob *job)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  job->priv->paused = FALSE;
}



void
thunar_job_freeze (ThunarJob *job)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  job->priv->frozen = TRUE;
  g_signal_emit (EXO_JOB (job), job_signals[FROZEN], 0);
}



void
thunar_job_unfreeze (ThunarJob *job)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  job->priv->frozen = FALSE;
  g_signal_emit (EXO_JOB (job), job_signals[UNFROZEN], 0);
}



gboolean
thunar_job_is_paused (ThunarJob *job)
{
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  return job->priv->paused;
}



gboolean
thunar_job_is_frozen (ThunarJob *job)
{
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  return job->priv->frozen;
}



void
thunar_job_processing_file (ThunarJob *job,
                            GList     *current_file,
                            guint      n_processed)
{
  gchar *base_name;
  gchar *display_name;

  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (current_file != NULL);

  /* emit only if n_processed is a multiple of 8 */
  if ((n_processed % 8) != 0)
    return;

  base_name = g_file_get_basename (current_file->data);
  display_name = g_filename_display_name (base_name);
  g_free (base_name);

  exo_job_info_message (EXO_JOB (job), "%s", display_name);
  g_free (display_name);

  /* verify that we have total files set */
  if (G_LIKELY (job->priv->n_total_files > 0))
    exo_job_percent (EXO_JOB (job), (n_processed * 100.0) / job->priv->n_total_files);
}



void
thunar_job_set_log_mode (ThunarJob             *job,
                         ThunarOperationLogMode log_mode)
{
  job->priv->log_mode = log_mode;
}



ThunarOperationLogMode
thunar_job_get_log_mode (ThunarJob *job)
{
  return job->priv->log_mode;
}
