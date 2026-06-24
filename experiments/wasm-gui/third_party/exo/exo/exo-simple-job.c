/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
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

/* avoid waring in the gvalue collector, since it will break the api */
#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_30

#include <gobject/gvaluecollector.h>

#include <exo/exo-job.h>
#include <exo/exo-private.h>
#include <exo/exo-simple-job.h>
#include <exo/exo-alias.h>

/**
 * SECTION: exo-simple-job
 * @title: ExoSimpleJob
 * @short_description: Simple interface to execute functions asynchronously
 * @include: exo/exo.h
 * @see_also: <link linkend="ExoJob">ExoJob</link>
 *
 * <link linkend="ExoSimpleJob">ExoSimpleJob</link> can be used to execute
 * functions asynchronously in an #ExoJob wrapper object. It is easier to
 * use than the #GThread system and provides basic signals to follow the
 * progress of an operation.
 **/



static void     exo_simple_job_finalize   (GObject           *object);
static gboolean exo_simple_job_execute    (ExoJob            *job,
                                           GError           **error);



struct _ExoSimpleJobClass
{
  ExoJobClass __parent__;
};

/**
 * ExoSimpleJob:
 *
 * The #ExoSimpleJob struct contains only private fields and should not be
 * directly accessed.
 **/
struct _ExoSimpleJob
{
  ExoSimpleJobFunc func;
  GValueArray     *param_values;
  ExoJob           __parent__;
};



G_DEFINE_TYPE (ExoSimpleJob, exo_simple_job, EXO_TYPE_JOB)



static void
exo_simple_job_class_init (ExoSimpleJobClass *klass)
{
  ExoJobClass  *exojob_class;
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = exo_simple_job_finalize;

  exojob_class = EXO_JOB_CLASS (klass);
  exojob_class->execute = exo_simple_job_execute;
}



static void
exo_simple_job_init (ExoSimpleJob *simple_job)
{
}



static void
exo_simple_job_finalize (GObject *object)
{
  ExoSimpleJob *simple_job = EXO_SIMPLE_JOB (object);

  /* release the param values */
  g_value_array_free (simple_job->param_values);

  (*G_OBJECT_CLASS (exo_simple_job_parent_class)->finalize) (object);
}



static gboolean
exo_simple_job_execute (ExoJob  *job,
                        GError **error)
{
  ExoSimpleJob *simple_job = EXO_SIMPLE_JOB (job);
  gboolean         success = TRUE;
  GError          *err = NULL;

  _exo_return_val_if_fail (EXO_IS_SIMPLE_JOB (job), FALSE);
  _exo_return_val_if_fail (simple_job->func != NULL, FALSE);

  /* try to execute the job using the supplied function */
  success = (*simple_job->func) (job, simple_job->param_values, &err);

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
 * exo_simple_job_launch:
 * @func           : the #ExoSimpleJobFunc to execute the job.
 * @n_param_values : the number of parameters to pass to the @func.
 * @...            : a list of #GType and parameter pairs (exactly
 *                   @n_param_values pairs) that are passed to @func.
 *
 * Allocates a new #ExoJob which executes the specified @func with
 * the specified parameters.
 *
 * An example could be:
 *
 * <informalexample><programlisting>
 * static gboolean
 * list_directory (ExoJob      *job,
 *                 GValueArray *param_values,
 *                 GError     **error)
 * {
 *   GFileEnumerator *enumerator;
 *   GFileInfo       *info;
 *   GError          *err = NULL;
 *   GFile           *directory;
 *
 *   if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
 *     return FALSE;
 *
 *   directory = g_value_get_object (g_value_array_get_nth (param_values, 0));
 *
 *   enumerator = g_file_enumerate_children (directory,
 *                                           "standard::display-name",
 *                                           G_FILE_QUERY_INFO_NONE,
 *                                           exo_job_get_cancellable (job),
 *                                           &err);
 *
 *   if (err != NULL)
 *     {
 *       g_propagate_error (error, err);
 *       return FALSE;
 *     }
 *
 *   while (TRUE)
 *     {
 *       info = g_file_enumerator_next_file (enumerator,
 *                                           exo_job_get_cancellable (job),
 *                                           &err);
 *
 *       if (info == NULL)
 *         break;
 *
 *       exo_job_info_message (job, _("Child: %s"),
 *                             g_file_info_get_display_name (info));
 *
 *       g_object_unref (info);
 *     }
 *
 *   g_object_unref (enumerator);
 *
 *   if (err != NULL)
 *     {
 *       g_propagate_error (error, err);
 *       return FALSE;
 *     }
 *   else
 *     {
 *       return TRUE;
 *     }
 * }
 *
 * ...
 *
 * GFile *file = g_file_new_for_path ("/home/user");
 * ExoJob *job = exo_simple_job_launch (list_directory, 1, G_TYPE_FILE, file);
 * g_signal_connect (job, "info-message", G_CALLBACK (update_some_widget), widget);
 * g_signal_connect (job, "finished", G_CALLBACK (unref_the_job_object), NULL);
 * </programlisting></informalexample>
 *
 * The caller is responsible to release the returned #ExoJob object
 * using g_object_unref() when no longer needed.
 *
 * Returns: the launched #ExoJob.
 **/
ExoJob*
exo_simple_job_launch (ExoSimpleJobFunc func,
                       guint            n_param_values,
                       ...)
{
  ExoSimpleJob *simple_job;
  va_list       var_args;
  GValue        value = { 0, };
  gchar        *error_message;
  guint         n;

  /* allocate and initialize the simple job */
  simple_job = g_object_new (EXO_TYPE_SIMPLE_JOB, NULL);
  simple_job->func = func;
  simple_job->param_values = g_value_array_new (n_param_values);

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

      g_value_array_insert (simple_job->param_values, n, &value);
      g_value_unset (&value);
    }
  va_end (var_args);

  /* launch the job */
  return exo_job_launch (EXO_JOB (simple_job));
}


#define __EXO_SIMPLE_JOB_C__
#include <exo/exo-aliasdef.c>
