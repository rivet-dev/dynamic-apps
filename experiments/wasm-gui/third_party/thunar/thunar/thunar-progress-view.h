/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __THUNAR_PROGRESS_VIEW_H__
#define __THUNAR_PROGRESS_VIEW_H__

#include <gtk/gtk.h>

#include <thunar/thunar-job.h>

G_BEGIN_DECLS;

typedef struct _ThunarProgressViewClass ThunarProgressViewClass;
typedef struct _ThunarProgressView      ThunarProgressView;

#define THUNAR_TYPE_PROGRESS_VIEW            (thunar_progress_view_get_type ())
#define THUNAR_PROGRESS_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_PROGRESS_VIEW, ThunarProgressView))
#define THUNAR_PROGRESS_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_PROGRESS_VIEW, ThunarProgressViewClass))
#define THUNAR_IS_PROGRESS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_PROGRESS_VIEW))
#define THUNAR_IS_PROGRESS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_PROGRESS_VIEW))
#define THUNAR_PROGRESS_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_PROGRESS_VIEW, ThunarProgressViewClass))

GType      thunar_progress_view_get_type      (void) G_GNUC_CONST;

GtkWidget *thunar_progress_view_new_with_job  (ThunarJob          *job) G_GNUC_MALLOC;

void       thunar_progress_view_set_icon_name (ThunarProgressView *view,
                                               const gchar        *icon_name);
void       thunar_progress_view_set_title     (ThunarProgressView *view,
                                               const gchar        *title);
ThunarJob *thunar_progress_view_get_job       (ThunarProgressView *view);
void       thunar_progress_view_launch_job    (ThunarProgressView *view);

G_END_DECLS;

#endif /* !__THUNAR_PROGRESS_VIEW_H__ */
