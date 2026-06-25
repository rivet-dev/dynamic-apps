/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
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

#ifndef __THUNAR_SIMPLE_JOB_H__
#define __THUNAR_SIMPLE_JOB_H__

#include <thunar/thunar-job.h>

G_BEGIN_DECLS

/**
 * ThunarSimpleJobFunc:
 * @job            : a #ThunarJob.
 * @param_values   : a #GArray of the #GValue<!---->s passed to
 *                   thunar_simple_job_new().
 * @error          : return location for errors.
 *
 * Used by the #ThunarSimpleJob to process the @job. See thunar_simple_job_new()
 * for further details.
 *
 * Return value: %TRUE on success, %FALSE in case of an error.
 **/
typedef gboolean (*ThunarSimpleJobFunc) (ThunarJob  *job,
                                         GArray     *param_values,
                                         GError    **error);


typedef struct _ThunarSimpleJobClass ThunarSimpleJobClass;
typedef struct _ThunarSimpleJob      ThunarSimpleJob;

#define THUNAR_TYPE_SIMPLE_JOB            (thunar_simple_job_get_type ())
#define THUNAR_SIMPLE_JOB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_SIMPLE_JOB, ThunarSimpleJob))
#define THUNAR_SIMPLE_JOB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_SIMPLE_JOB, ThunarSimpleJobClass))
#define THUNAR_IS_SIMPLE_JOB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_SIMPLE_JOB))
#define THUNAR_IS_SIMPLE_JOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_SIMPLE_JOB))
#define THUNAR_SIMPLE_JOB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_SIMPLE_JOB, ThunarSimpleJobClass))

GType      thunar_simple_job_get_type           (void) G_GNUC_CONST;

ThunarJob *thunar_simple_job_new                (ThunarSimpleJobFunc func,
                                                 guint               n_param_values,
                                                 ...) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
GArray    *thunar_simple_job_get_param_values   (ThunarSimpleJob    *job);

G_END_DECLS

#endif /* !__THUNAR_SIMPLE_JOB_H__ */
