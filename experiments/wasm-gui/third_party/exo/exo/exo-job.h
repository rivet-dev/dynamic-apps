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

#if !defined (EXO_INSIDE_EXO_H) && !defined (EXO_COMPILATION)
#error "Only <exo/exo.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef __EXO_JOB_H__
#define __EXO_JOB_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define EXO_TYPE_JOB            (exo_job_get_type ())
#define EXO_JOB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXO_TYPE_JOB, ExoJob))
#define EXO_JOB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EXO_TYPE_JOB, ExoJobClass))
#define EXO_IS_JOB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXO_TYPE_JOB))
#define EXO_IS_JOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EXO_TYPE_JOB)
#define EXO_JOB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EXO_TYPE_JOB, ExoJobClass))

typedef struct _ExoJobPrivate ExoJobPrivate;
typedef struct _ExoJobClass   ExoJobClass;
typedef struct _ExoJob        ExoJob;

struct _ExoJobClass
{
  GObjectClass __parent__;

  /* virtual methods */
  gboolean   (*execute)      (ExoJob      *job,
                              GError     **error);

  /* signals */
  void       (*error)        (ExoJob      *job,
                              GError      *error);
  void       (*finished)     (ExoJob      *job);
  void       (*info_message) (ExoJob      *job,
                              const gchar *message);
  void       (*percent)      (ExoJob      *job,
                              gdouble      percent);
};

/**
 * ExoJob:
 *
 * The #ExoJob struct contains only private fields and should not be
 * directly accessed.
 **/
struct _ExoJob
{
  GObject __parent__;

  /*< private >*/
  ExoJobPrivate *priv;
};

GType           exo_job_get_type               (void) G_GNUC_CONST;

ExoJob         *exo_job_launch                 (ExoJob        *job);
void            exo_job_cancel                 (ExoJob        *job);
gboolean        exo_job_is_cancelled           (const ExoJob  *job);
GCancellable   *exo_job_get_cancellable        (const ExoJob  *job);
gboolean        exo_job_set_error_if_cancelled (ExoJob        *job,
                                                GError       **error);
void            exo_job_emit                   (ExoJob        *job,
                                                guint          signal_id,
                                                GQuark         signal_detail,
                                                ...);
void            exo_job_info_message           (ExoJob        *job,
                                                const gchar   *format,
                                                ...);
void            exo_job_percent                (ExoJob        *job,
                                                gdouble        percent);
gboolean        exo_job_send_to_mainloop       (ExoJob        *job,
                                                GSourceFunc    func,
                                                gpointer       user_data,
                                                GDestroyNotify destroy_notify);

G_END_DECLS

#endif /* !__EXO_JOB_H__ */
