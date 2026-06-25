/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_STATUSBAR_H__
#define __THUNAR_STATUSBAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS;

/* avoid including libxfce4ui.h */
typedef struct _XfceGtkActionEntry   XfceGtkActionEntry;

typedef struct _ThunarStatusbarClass ThunarStatusbarClass;
typedef struct _ThunarStatusbar      ThunarStatusbar;

#define THUNAR_TYPE_STATUSBAR             (thunar_statusbar_get_type ())
#define THUNAR_STATUSBAR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_STATUSBAR, ThunarStatusbar))
#define THUNAR_STATUSBAR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_STATUSBAR, ThunarStatusbarClass))
#define THUNAR_IS_STATUSBAR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_STATUSBAR))
#define THUNAR_IS_STATUSBAR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_STATUSBAR))
#define THUNAR_STATUSBAR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_STATUSBAR, ThunarStatusbarClass))

/* #XfceGtkActionEntrys provided by this widget */
typedef enum
{
    THUNAR_STATUS_BAR_ACTION_TOGGLE_SIZE,
    THUNAR_STATUS_BAR_ACTION_TOGGLE_SIZE_IN_BYTES,
    THUNAR_STATUS_BAR_ACTION_TOGGLE_FILETYPE,
    THUNAR_STATUS_BAR_ACTION_TOGGLE_DISPLAY_NAME,
    THUNAR_STATUS_BAR_ACTION_TOGGLE_LAST_MODIFIED,

    THUNAR_STATUS_BAR_N_ACTIONS
} ThunarStatusBarAction;

GType               thunar_statusbar_get_type            (void) G_GNUC_CONST;

GtkWidget          *thunar_statusbar_new                 (void);

void                thunar_statusbar_setup_event         (ThunarStatusbar *statusbar,
                                                          GtkWidget       *event_box);

void                thunar_statusbar_append_accelerators (ThunarStatusbar *statusbar,
                                                          GtkAccelGroup   *accel_group);

XfceGtkActionEntry *thunar_statusbar_get_action_entries  (void);

G_END_DECLS;

#endif /* !__THUNAR_STATUSBAR_H__ */
