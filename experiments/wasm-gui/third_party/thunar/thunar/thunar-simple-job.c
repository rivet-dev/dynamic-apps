/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gobject/gvaluecollector.h>

#include <thunar/thunar-job.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-simple-job.h>



static void     thunar_simple_job_finalize   (GObject              *object);
static gboolean thunar_simple_job_execute    (ExoJob               *job,
                                              GError              **error);



struct _ThunarSimpleJobClass
{
  ThunarJobClass __parent__;
};

struct _ThunarSimpleJob
{
  ThunarJob __parent__;

  ThunarSimpleJobFunc  func;
  GArray              *param_values;
};



G_DEFINE_TYPE (ThunarSimpleJob, thunar_simple_job, THUNAR_TYPE_JOB)



static void
thunar_simple_job_class_init (ThunarSimpleJobClass *klass)
{
  GObjectClass *gobject_class;
  ExoJobClass  *exojob_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_simple_job_finalize;

  exojob_class = EXO_JOB_CLASS (klass);
  exojob_class->execute = thunar_simple_job_execute;
}



static void
thunar_simple_job_init (ThunarSimpleJob *simple_job)
{
}



static void
thunar_simple_job_finalize (GObject *object)
{
  ThunarSimpleJob *simple_job = THUNAR_SIMPLE_JOB (object);
  guint            i;

  /* release the param values */
  for (i = 0; i < simple_job->param_values->len; i++)
    g_value_unset (&g_array_index (simple_job->param_values, GValue, i));
  g_array_free (simple_job->param_values, TRUE);

  (*G_OBJECT_CLASS (thunar_simple_job_parent_class)->finalize) (object);
}



static gboolean
thunar_simple_job_execute (ExoJob  *job,
                           GError **error)
{
  ThunarSimpleJob *simple_job = THUNAR_SIMPLE_JOB (job);
  gboolean         success = TRUE;
  GError          *err = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_SIMPLE_JOB (job), FALSE);
  _thunar_return_val_if_fail (simple_job->func != NULL, FALSE);

  /* try to execute the job using the supplied function */
  success = (*simple_job->func) (THUNAR_JOB (job), simple_job->param_values, &err);

  if (!success)
    {
      g_assert (err != NULL || exo_job_is_cancelled (job));

      /* set error if the job was cancelled. otherwise just propagate
       * the results of the processing function */
      if (exo_job_set_error_if_cancelled (job, error))
        {
          g_clear_error (&err);
        }
      else
        {
          if (err != NULL)
            g_propagate_error (error, err);
        }

      return FALSE;
    }
  else
    return TRUE;
}



/**
 * thunar_simple_job_new:
 * @func           : the #ThunarSimpleJobFunc to execute the job.
 * @n_param_values : the number of parameters to pass to the @func.
 * @...            : a list of #GType and parameter pairs (exactly
 *                   @n_param_values pairs) that are passed to @func.
 *
 * Allocates a new #ThunarSimpleJob, which executes the specified
 * @func with the specified parameters.
 *
 * Use exo_job_launch() to launch the returned job..
 *
 * For example the listdir @func expects a #ThunarPath for the
 * folder to list, so the call to thunar_simple_job_new()
 * would look like this:
 *
 * <informalexample><programlisting>
 * job = thunar_simple_job_new (_thunar_io_jobs_listdir, 1,
 *                              THUNAR_TYPE_PATH, path);
 * exo_job_launch (EXO_JOB (job));
 * </programlisting></informalexample>
 *
 * The caller is responsible to release the returned object using
 * thunar_job_unref() when no longer needed.
 *
 * Return value: a #ThunarJob.
 **/
ThunarJob *
thunar_simple_job_new (ThunarSimpleJobFunc func,
                       guint               n_param_values,
                       ...)
{
  ThunarSimpleJob *simple_job;
  va_list          var_args;
  GValue           value = { 0, };
  gchar           *error_message;
  guint            n;

  /* allocate and initialize the simple job */
  simple_job = g_object_new (THUNAR_TYPE_SIMPLE_JOB, NULL);
  simple_job->func = func;
  simple_job->param_values = g_array_sized_new (FALSE, TRUE, sizeof (GValue), n_param_values);

  /* collect the parameters */
  va_start (var_args, n_param_values);
  for (n = 0; n < n_param_values; ++n)
    {
      /* initialize the value to hold the next parameter */
      g_value_init (&value, va_arg (var_args, GType));

      /* collect the value from the stack */
      G_VALUE_COLLECT (&value, var_args, 0, &error_message);

      /* check if an error occurred */
      if (G_UNLIKELY (error_message != NULL))
        {
          g_error ("%s: %s", G_STRLOC, error_message);
          g_free (error_message);
        }

      g_array_insert_val (simple_job->param_values, n, value);

      /* manually unset the value, g_value_unset doesn't work
       * because we don't want to free the data */
      memset (&value, 0, sizeof (GValue));
    }
  va_end (var_args);

  return THUNAR_JOB (simple_job);
}



GArray *
thunar_simple_job_get_param_values (ThunarSimpleJob *job)
{
  _thunar_return_val_if_fail (THUNAR_IS_SIMPLE_JOB (job), NULL);
  return job->param_values;
}
