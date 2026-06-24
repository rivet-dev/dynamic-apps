/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include <gio/gio.h>

#include <exo/exo-config.h>
#include <exo/exo-job.h>
#include <exo/exo-private.h>
#include <exo/exo-alias.h>
#include <libxfce4util/libxfce4util.h>

/**
 * SECTION: exo-job
 * @title: ExoJob
 * @short_description: Base class for threaded/asynchronous jobs
 * @include: exo/exo.h
 * @see_also: <link linkend="ExoSimpleJob">ExoSimpleJob</link>
 *
 * <link linkend="ExoJob">ExoJob</link> is an abstract base class
 * intended to wrap threaded/asynchronous operations (called jobs here).
 * It was written because the ways of dealing with threads provided by
 * GLib are not exactly object-oriented.
 *
 * It can be used to wrap any kind of long-running or possibly-blocking
 * operation like file operations or communication with web services.
 * The benefit of using <link linkend="ExoJob">ExoJob</link> is that one
 * gets an object associated with each operation. After creating the job
 * the caller can connect to signals like <link linkend="ExoJob::error">"error"
 * </link> or <link linkend="ExoJob::percent">"percent"</link>. This
 * design integrates very well with the usual object-oriented design of
 * applications based on GObject.
 **/



/* Signal identifiers */
enum
{
  ERROR,
  FINISHED,
  INFO_MESSAGE,
  PERCENT,
  LAST_SIGNAL,
};



typedef struct _ExoJobSignalData ExoJobSignalData;



static void exo_job_finalize   (GObject      *object);
static void exo_job_error      (ExoJob       *job,
                                const GError *error);
static void exo_job_finished   (ExoJob       *job);



struct _ExoJobPrivate
{
  GIOSchedulerJob *scheduler_job;
  GCancellable    *cancellable;
  guint            running : 1;
  GError          *error;
  gboolean         failed;
  GMainContext    *context;
};

struct _ExoJobSignalData
{
  gpointer instance;
  GQuark   signal_detail;
  guint    signal_id;
  va_list  var_args;
};



static guint job_signals[LAST_SIGNAL];



G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ExoJob, exo_job, G_TYPE_OBJECT)



static void
exo_job_class_init (ExoJobClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = exo_job_finalize;

  klass->execute = NULL;
  klass->error = NULL;
  klass->finished = NULL;
  klass->info_message = NULL;
  klass->percent = NULL;

  /**
   * ExoJob::error:
   * @job   : an #ExoJob.
   * @error : a #GError describing the cause.
   *
   * Emitted whenever an error occurs while executing the @job. This signal
   * may not be emitted from within #ExoJob subclasses. If a subclass wants
   * to emit an "error" signal (and thereby terminate the operation), it has
   * to fill the #GError structure and abort from its execute() method.
   * #ExoJob will automatically emit the "error" signal when the #GError is
   * filled after the execute() method has finished.
   *
   * Callers interested in whether the @job was cancelled can connect to
   * the "cancelled" signal of the #GCancellable returned from
   * exo_job_get_cancellable().
   **/
  job_signals[ERROR] =
    g_signal_new (I_("error"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS,
                  G_STRUCT_OFFSET (ExoJobClass, error),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ExoJob::finished:
   * @job : an #ExoJob.
   *
   * This signal will be automatically emitted once the @job finishes
   * its execution, no matter whether @job completed successfully or
   * was cancelled by the user. It may not be emitted by subclasses of
   * #ExoJob as it is automatically emitted by #ExoJob after the execute()
   * method has finished.
   **/
  job_signals[FINISHED] =
    g_signal_new (I_("finished"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS,
                  G_STRUCT_OFFSET (ExoJobClass, finished),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ExoJob::info-message:
   * @job     : an #ExoJob.
   * @message : information to be displayed about @job.
   *
   * This signal is emitted to display information about the status of
   * the @job. Examples of messages are "Preparing..." or "Cleaning up...".
   *
   * The @message is garanteed to contain valid UTF-8, so it can be
   * displayed by #GtkWidget<!---->s out of the box.
   **/
  job_signals[INFO_MESSAGE] =
    g_signal_new (I_("info-message"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS,
                  G_STRUCT_OFFSET (ExoJobClass, info_message),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  /**
   * ExoJob::percent:
   * @job     : an #ExoJob.
   * @percent : the percentage of completeness.
   *
   * This signal is emitted to present the overall progress of the
   * operation. The @percent value is garantied to be a value between
   * 0.0 and 100.0.
   **/
  job_signals[PERCENT] =
    g_signal_new (I_("percent"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS,
                  G_STRUCT_OFFSET (ExoJobClass, percent),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__DOUBLE,
                  G_TYPE_NONE, 1, G_TYPE_DOUBLE);
}



static void
exo_job_init (ExoJob *job)
{
  job->priv = exo_job_get_instance_private (job);
  job->priv->cancellable = g_cancellable_new ();
  job->priv->running = FALSE;
  job->priv->scheduler_job = NULL;
  job->priv->error = NULL;
  job->priv->failed = FALSE;
  job->priv->context = NULL;
}



static void
exo_job_finalize (GObject *object)
{
  ExoJob *job = EXO_JOB (object);

  if (job->priv->running)
    exo_job_cancel (job);

  if (job->priv->error != NULL)
    g_error_free (job->priv->error);

  g_object_unref (job->priv->cancellable);

  if (job->priv->context != NULL)
    g_main_context_unref (job->priv->context);

  (*G_OBJECT_CLASS (exo_job_parent_class)->finalize) (object);
}



/**
 * exo_job_async_ready:
 * @object : an #ExoJob.
 * @result : the #GAsyncResult of the job.
 *
 * This function is called by the #GIOScheduler at the end of the
 * operation. It checks if there were errors during the operation
 * and emits "error" and "finished" signals.
 **/
static gboolean
exo_job_async_ready (gpointer user_data)
{
  ExoJob *job = EXO_JOB (user_data);

  _exo_return_val_if_fail (EXO_IS_JOB (job), FALSE);

  if (job->priv->failed)
    {
      g_assert (job->priv->error != NULL);

      /* don't treat cancellation as an error */
      if (!g_error_matches (job->priv->error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        exo_job_error (job, job->priv->error);

      /* cleanup */
      g_error_free (job->priv->error);
      job->priv->error = NULL;
    }

  exo_job_finished (job);

  job->priv->running = FALSE;

  return FALSE;
}



/**
 * exo_job_scheduler_job_func:
 * @scheduler_job : the #GIOSchedulerJob running the operation.
 * @cancellable   : the #GCancellable associated with the job.
 * @user_data     : an #ExoJob.
 *
 * This function is called by the #GIOScheduler to execute the
 * operation associated with the job. It basically calls the
 * execute() function of #ExoJobClass.
 *
 * Returns: %FALSE, to stop the thread at the end of the operation.
 **/
static gboolean
exo_job_scheduler_job_func (GIOSchedulerJob *scheduler_job,
                            GCancellable    *cancellable,
                            gpointer         user_data)
{
  ExoJob   *job = EXO_JOB (user_data);
  GError   *error = NULL;
  gboolean  success;
  GSource  *source;

  _exo_return_val_if_fail (EXO_IS_JOB (job), FALSE);
  _exo_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);

  job->priv->scheduler_job = scheduler_job;

  success = (*EXO_JOB_GET_CLASS (job)->execute) (job, &error);

  if (!success)
    {
      /* clear existing error */
      if (G_UNLIKELY (job->priv->error != NULL))
        g_error_free (job->priv->error);

      /* take the error */
      job->priv->error = error;
    }

  job->priv->failed = !success;

  /* idle completion in the thread exo job was started from */
  source = g_idle_source_new ();
  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_callback (source, exo_job_async_ready, g_object_ref (job),
                         g_object_unref);
  g_source_attach (source, job->priv->context);
  g_source_unref (source);

  return FALSE;
}



/**
 * exo_job_emit_valist_in_mainloop:
 * @user_data : an #ExoJobSignalData.
 *
 * Called from the main loop of the application to emit the signal
 * specified by the @user_data.
 *
 * Returns: %FALSE, to keep the function from being called
 *          multiple times in a row.
 **/
static gboolean
exo_job_emit_valist_in_mainloop (gpointer user_data)
{
  ExoJobSignalData *data = user_data;

  g_signal_emit_valist (data->instance, data->signal_id, data->signal_detail,
                        data->var_args);

  return FALSE;
}


/**
 * exo_job_emit_valist:
 * @job           : an #ExoJob.
 * @signal_id     : the signal id.
 * @signal_detail : the signal detail.
 * @var_args      : a list of parameters to be passed to the signal,
 *                  followed by a location for the return value. If the
 *                  return type of the signal is G_TYPE_NONE, the return
 *                  value location can be omitted.
 *
 * Sends a the signal with the given @signal_id and @signal_detail to the
 * main loop of the application and waits for listeners to handle it.
 **/
static void
exo_job_emit_valist (ExoJob *job,
                     guint   signal_id,
                     GQuark  signal_detail,
                     va_list var_args)
{
  ExoJobSignalData data;

  _exo_return_if_fail (EXO_IS_JOB (job));
  _exo_return_if_fail (job->priv->scheduler_job != NULL);

  data.instance = job;
  data.signal_id = signal_id;
  data.signal_detail = signal_detail;

  /* copy the variable argument list */
  G_VA_COPY (data.var_args, var_args);

  /* emit the signal in the main loop */
  exo_job_send_to_mainloop (job,
                            exo_job_emit_valist_in_mainloop,
                            &data, NULL);

  va_end (data.var_args);
}



/**
 * exo_job_error:
 * @job   : an #ExoJob.
 * @error : a #GError.
 *
 * Emits the "error" signal and passes the @error to it so that the
 * application can handle it (e.g. by displaying an error dialog).
 *
 * This function should never be called from outside the application's
 * main loop.
 **/
static void
exo_job_error (ExoJob       *job,
               const GError *error)
{
  _exo_return_if_fail (EXO_IS_JOB (job));
  _exo_return_if_fail (error != NULL);

  g_signal_emit (job, job_signals[ERROR], 0, error);
}



/**
 * exo_job_finished:
 * @job : an #ExoJob.
 *
 * Emits the "finished" signal to notify listeners of the end of the
 * operation.
 *
 * This function should never be called from outside the application's
 * main loop.
 **/
static void
exo_job_finished (ExoJob *job)
{
  _exo_return_if_fail (EXO_IS_JOB (job));
  g_signal_emit (job, job_signals[FINISHED], 0);
}



/**
 * exo_job_launch:
 * @job : an #ExoJob.
 *
 * This functions schedules the @job to be run as soon as possible, in
 * a separate thread. The caller can connect to signals of the @job prior
 * or after this call in order to be notified on errors, progress updates
 * and the end of the operation.
 *
 * Returns: the @job itself.
 **/
ExoJob *
exo_job_launch (ExoJob *job)
{
  _exo_return_val_if_fail (EXO_IS_JOB (job), NULL);
  _exo_return_val_if_fail (!job->priv->running, NULL);
  _exo_return_val_if_fail (EXO_JOB_GET_CLASS (job)->execute != NULL, NULL);

  /* mark the job as running */
  job->priv->running = TRUE;

  job->priv->context = g_main_context_ref_thread_default ();

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_io_scheduler_push_job (exo_job_scheduler_job_func, g_object_ref (job),
                           (GDestroyNotify) g_object_unref,
                           G_PRIORITY_HIGH, job->priv->cancellable);
G_GNUC_END_IGNORE_DEPRECATIONS

  return job;
}



/**
 * exo_job_cancel:
 * @job : a #ExoJob.
 *
 * Attempts to cancel the operation currently performed by @job. Even
 * after the cancellation of @job, it may still emit signals, so you
 * must take care of disconnecting all handlers appropriately if you
 * cannot handle signals after cancellation.
 *
 * Calling this function when the @job has not been launched yet or
 * when it has already finished will have no effect.
 **/
void
exo_job_cancel (ExoJob *job)
{
  _exo_return_if_fail (EXO_IS_JOB (job));

  if (job->priv->running)
    g_cancellable_cancel (job->priv->cancellable);
}



/**
 * exo_job_is_cancelled:
 * @job : a #ExoJob.
 *
 * Checks whether @job was previously cancelled
 * by a call to exo_job_cancel().
 *
 * Returns: %TRUE if @job is cancelled.
 **/
gboolean
exo_job_is_cancelled (const ExoJob *job)
{
  _exo_return_val_if_fail (EXO_IS_JOB (job), FALSE);
  return g_cancellable_is_cancelled (job->priv->cancellable);
}



/**
 * exo_job_get_cancellable:
 * @job : an #ExoJob.
 *
 * Returns the #GCancellable that can be used to cancel the @job.
 *
 * Returns: the #GCancellable associated with the @job. It
 *          is owned by the @job and must not be released.
 **/
GCancellable *
exo_job_get_cancellable (const ExoJob *job)
{
  _exo_return_val_if_fail (EXO_IS_JOB (job), NULL);
  return job->priv->cancellable;
}



/**
 * exo_job_set_error_if_cancelled:
 * @job   : an #ExoJob.
 * @error : error to be set if the @job was cancelled.
 *
 * Sets the @error if the @job was cancelled. This is a convenience
 * function that is equivalent to
 * <informalexample><programlisting>
 * GCancellable *cancellable;
 * cancellable = exo_job_get_cancllable (job);
 * g_cancellable_set_error_if_cancelled (cancellable, error);
 * </programlisting></informalexample>
 *
 * Returns: %TRUE if the job was cancelled and @error is now set,
 *          %FALSE otherwise.
 **/
gboolean
exo_job_set_error_if_cancelled (ExoJob  *job,
                                GError **error)
{
  _exo_return_val_if_fail (EXO_IS_JOB (job), FALSE);
  return g_cancellable_set_error_if_cancelled (job->priv->cancellable, error);
}



/**
 * exo_job_emit:
 * @job           : an #ExoJob.
 * @signal_id     : the signal id.
 * @signal_detail : the signal detail.
 * @...           : a list of parameters to be passed to the signal,
 *                  followed by a location for the return value. If the
 *                  return type of the signal is G_TYPE_NONE, the return
 *                  value location can be omitted.
 *
 * Sends the signal with @signal_id and @signal_detail to the application's
 * main loop and waits for listeners to handle it.
 **/
void
exo_job_emit (ExoJob *job,
              guint   signal_id,
              GQuark  signal_detail,
              ...)
{
  va_list var_args;

  _exo_return_if_fail (EXO_IS_JOB (job));

  va_start (var_args, signal_detail);
  exo_job_emit_valist (job, signal_id, signal_detail, var_args);
  va_end (var_args);
}



/**
 * exo_job_info_message:
 * @job     : an #ExoJob.
 * @format  : a format string.
 * @...     : parameters for the format string.
 *
 * Generates and emits an "info-message" signal and sends it to the
 * application's main loop.
 **/
void
exo_job_info_message (ExoJob      *job,
                      const gchar *format,
                      ...)
{
  va_list var_args;
  gchar  *message;

  _exo_return_if_fail (EXO_IS_JOB (job));
  _exo_return_if_fail (format != NULL);

  va_start (var_args, format);
  message = g_strdup_vprintf (format, var_args);

  exo_job_emit (job, job_signals[INFO_MESSAGE], 0, message);

  g_free (message);
  va_end (var_args);
}



/**
 * exo_job_percent:
 * @job     : an #ExoJob.
 * @percent : percentage of completeness of the operation.
 *
 * Emits a "percent" signal and sends it to the application's main
 * loop. Also makes sure that @percent is between 0.0 and 100.0.
 **/
void
exo_job_percent (ExoJob *job,
                 gdouble percent)
{
  _exo_return_if_fail (EXO_IS_JOB (job));

  percent = CLAMP (percent, 0.0, 100.0);
  exo_job_emit (job, job_signals[PERCENT], 0, percent);
}



/**
 * exo_job_send_to_mainloop:
 * @job            : an #ExoJob.
 * @func           : a #GSourceFunc callback that will be called in the main thread.
 * @user_data      : data to pass to @func.
 * @destroy_notify : a #GDestroyNotify for @user_data, or %NULL.
 *
 * This functions schedules @func to be run in the main loop (main thread),
 * waiting for the result (and blocking the job in the meantime).
 *
 * Returns: The return value of @func.
 **/
gboolean
exo_job_send_to_mainloop (ExoJob        *job,
                          GSourceFunc    func,
                          gpointer       user_data,
                          GDestroyNotify destroy_notify)
{
  _exo_return_val_if_fail (EXO_IS_JOB (job), FALSE);
  _exo_return_val_if_fail (job->priv->scheduler_job != NULL, FALSE);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return g_io_scheduler_job_send_to_mainloop (job->priv->scheduler_job, func, user_data,
                                              destroy_notify);
G_GNUC_END_IGNORE_DEPRECATIONS
}




#define __EXO_JOB_C__
#include <exo/exo-aliasdef.c>
