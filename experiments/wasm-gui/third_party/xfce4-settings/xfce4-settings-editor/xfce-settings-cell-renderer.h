/*
 *  xfce4-settings-editor
 *
 *  Copyright (c) 2012      Nick Schermer <nick@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __XFCE_SETTINGS_CELL_RENDERER_H__
#define __XFCE_SETTINGS_CELL_RENDERER_H__

#include <gtk/gtk.h>

#define XFCE_TYPE_SETTINGS_CELL_RENDERER            (xfce_settings_cell_renderer_get_type ())
#define XFCE_SETTINGS_CELL_RENDERER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_SETTINGS_CELL_RENDERER, XfceSettingsCellRenderer))
#define XFCE_SETTINGS_CELL_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_SETTINGS_CELL_RENDERER, XfceSettingsCellRendererClass))
#define XFCE_IS_SETTINGS_CELL_RENDERER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_SETTINGS_CELL_RENDERER))
#define XFCE_IS_SETTINGS_CELL_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_SETTINGS_CELL_RENDERER))
#define XFCE_SETTINGS_CELL_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_SETTINGS_CELL_RENDERER, XfceSettingsCellRendererClass))

G_BEGIN_DECLS

typedef struct _XfceSettingsCellRenderer      XfceSettingsCellRenderer;
typedef struct _XfceSettingsCellRendererClass XfceSettingsCellRendererClass;

GType            xfce_settings_cell_renderer_get_type (void) G_GNUC_CONST;

GtkCellRenderer *xfce_settings_cell_renderer_new      (void);

GType            xfce_settings_array_type             (void);

G_END_DECLS

#endif  /* __XFCE_SETTINGS_CELL_RENDERER_H__ */
