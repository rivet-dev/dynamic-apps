/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __THUNAR_PROGRESS_DIALOG_H__
#define __THUNAR_PROGRESS_DIALOG_H__

#include <gtk/gtk.h>
#include <thunar/thunar-job.h>

G_BEGIN_DECLS;

typedef struct _ThunarProgressDialogClass ThunarProgressDialogClass;
typedef struct _ThunarProgressDialog      ThunarProgressDialog;

#define THUNAR_TYPE_PROGRESS_DIALOG            (thunar_progress_dialog_get_type ())
#define THUNAR_PROGRESS_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_PROGRESS_DIALOG, ThunarProgressDialog))
#define THUNAR_PROGRESS_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_PROGRESS_DIALOG, ThunarProgressDialogClass))
#define THUNAR_IS_PROGRESS_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_PROGRESS_DIALOG))
#define THUNAR_IS_PROGRESS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_PROGRESS_DIALOG))
#define THUNAR_PROGRESS_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_PROGRESS_DIALOG, ThunarProgressDialogClass))

GType      thunar_progress_dialog_get_type  (void) G_GNUC_CONST;

GtkWidget *thunar_progress_dialog_new       (void);
GList     *thunar_progress_dialog_list_jobs (ThunarProgressDialog *dialog);
void       thunar_progress_dialog_add_job   (ThunarProgressDialog *dialog,
                                             ThunarJob            *job,
                                             const gchar          *icon_name,
                                             const gchar          *title);
gboolean   thunar_progress_dialog_has_jobs  (ThunarProgressDialog *dialog);

G_END_DECLS;

#endif /* !__THUNAR_PROGRESS_DIALOG_H__ */
