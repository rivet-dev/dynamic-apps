/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __PANEL_WINDOW_H__
#define __PANEL_WINDOW_H__

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

#include <common/panel-xfconf.h>

G_BEGIN_DECLS

typedef struct _PanelWindowClass PanelWindowClass;
typedef struct _PanelWindow      PanelWindow;

#define PANEL_TYPE_WINDOW            (panel_window_get_type ())
#define PANEL_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_WINDOW, PanelWindow))
#define PANEL_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_WINDOW, PanelWindowClass))
#define PANEL_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_WINDOW))
#define PANEL_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_WINDOW))
#define PANEL_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PANEL_TYPE_WINDOW, PanelWindowClass))

GType          panel_window_get_type                  (void) G_GNUC_CONST;

GtkWidget     *panel_window_new                       (GdkScreen              *screen,
                                                       gint                    id,
                                                       gint                    autohide_block) G_GNUC_MALLOC;

gint           panel_window_get_id                    (PanelWindow            *window);

gboolean       panel_window_has_position              (PanelWindow            *window);

void           panel_window_set_provider_info         (PanelWindow            *window,
                                                       GtkWidget              *provider,
                                                       gboolean                moving_to_other_panel);

void           panel_window_freeze_autohide           (PanelWindow            *window);

void           panel_window_thaw_autohide             (PanelWindow            *window);

void           panel_window_set_locked                (PanelWindow            *window,
                                                       gboolean                locked);

gboolean       panel_window_get_locked                (PanelWindow            *window);

void           panel_window_focus                     (PanelWindow            *window);

void           panel_window_migrate_old_properties    (PanelWindow            *window,
                                                       XfconfChannel          *xfconf,
                                                       const gchar            *property_base,
                                                       const PanelProperty    *old_properties,
                                                       const PanelProperty    *new_properties);

G_END_DECLS

#endif /* !__PANEL_WINDOW_H__ */
