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

#ifndef __THUNAR_JOB_OPERATION_HISTORY_H__
#define __THUNAR_JOB_OPERATION_HISTORY_H__

#include <glib.h>
#include <thunar-job-operation.h>

G_BEGIN_DECLS

#define THUNAR_TYPE_JOB_OPERATION_HISTORY (thunar_job_operation_history_get_type ())
G_DECLARE_FINAL_TYPE (ThunarJobOperationHistory, thunar_job_operation_history, THUNAR, JOB_OPERATION_HISTORY, GObject)

ThunarJobOperationHistory* thunar_job_operation_history_get_default             (void);
void                       thunar_job_operation_history_commit                  (ThunarJobOperation    *job_operation);
void                       thunar_job_operation_history_update_trash_timestamps (ThunarJobOperation    *job_operation);
void                       thunar_job_operation_history_undo                    (void);
void                       thunar_job_operation_history_redo                    (void);
gboolean                   thunar_job_operation_history_can_undo                (void);
gboolean                   thunar_job_operation_history_can_redo                (void);

G_END_DECLS

#endif /* __THUNAR_JOB_OPERATION_HISTORY_H__ */
