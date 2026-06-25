/* vi:set sw=2 sts=2 ts=2 et ai: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 */

#ifndef __THUNAR_DEEP_COUNT_JOB_H__
#define __THUNAR_DEEP_COUNT_JOB_H__

#include <gio/gio.h>

#include <thunar/thunar-job.h>

G_BEGIN_DECLS;

typedef struct _ThunarDeepCountJobPrivate ThunarDeepCountJobPrivate;
typedef struct _ThunarDeepCountJobClass   ThunarDeepCountJobClass;
typedef struct _ThunarDeepCountJob        ThunarDeepCountJob;

#define THUNAR_TYPE_DEEP_COUNT_JOB            (thunar_deep_count_job_get_type ())
#define THUNAR_DEEP_COUNT_JOB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_DEEP_COUNT_JOB, ThunarDeepCountJob))
#define THUNAR_DEEP_COUNT_JOB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_DEEP_COUNT_JOB, ThunarDeepCountJobClass))
#define THUNAR_IS_DEEP_COUNT_JOB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_DEEP_COUNT_JOB))
#define THUNAR_IS_DEEP_COUNT_JOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_DEEP_COUNT_JOB)
#define THUNAR_DEEP_COUNT_JOB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_DEEP_COUNT_JOB, ThunarDeepCountJobClass))

GType               thunar_deep_count_job_get_type (void) G_GNUC_CONST;

ThunarDeepCountJob *thunar_deep_count_job_new      (GList              *files,
                                                    GFileQueryInfoFlags flags) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS;

#endif /* !__THUNAR_DEEP_COUNT_JOB_H__ */
