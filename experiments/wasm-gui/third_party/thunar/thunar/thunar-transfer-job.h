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

#ifndef __THUNAR_TRANSFER_JOB_H__
#define __THUNAR_TRANSFER_JOB_H__

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * ThunarTransferJobFlags:
 *
 * Flags to control the behavior of the transfer job.
 **/
typedef enum /*< enum >*/
{
  THUNAR_TRANSFER_JOB_COPY,
  THUNAR_TRANSFER_JOB_LINK,
  THUNAR_TRANSFER_JOB_MOVE,
  THUNAR_TRANSFER_JOB_TRASH,
} ThunarTransferJobType;

typedef struct _ThunarTransferJobPrivate ThunarTransferJobPrivate;
typedef struct _ThunarTransferJobClass   ThunarTransferJobClass;
typedef struct _ThunarTransferJob        ThunarTransferJob;

#define THUNAR_TYPE_TRANSFER_JOB            (thunar_transfer_job_get_type ())
#define THUNAR_TRANSFER_JOB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_TRANSFER_JOB, ThunarTransferJob))
#define THUNAR_TRANSFER_JOB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_TRANSFER_JOB, ThunarTransferJobClass))
#define THUNAR_IS_TRANSFER_JOB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_TRANSFER_JOB))
#define THUNAR_IS_TRANSFER_JOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_TRANSFER_JOB)
#define THUNAR_TRANSFER_JOB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_TRANSFER_JOB, ThunarTransferJobClass))

GType      thunar_transfer_job_get_type (void) G_GNUC_CONST;

ThunarJob *thunar_transfer_job_new        (GList                *source_file_list,
                                           GList                *target_file_list,
                                           ThunarTransferJobType type) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

gchar     *thunar_transfer_job_get_status (ThunarTransferJob    *job);

gboolean   thunar_transfer_job_can_start  (ThunarTransferJob *transfer_job,
                                           GList             *running_job_list);

G_END_DECLS

#endif /* !__THUNAR_TRANSFER_JOB_H__ */
