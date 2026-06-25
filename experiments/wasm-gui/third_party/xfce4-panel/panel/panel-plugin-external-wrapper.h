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

#ifndef __PANEL_PLUGIN_EXTERNAL_WRAPPER_H__
#define __PANEL_PLUGIN_EXTERNAL_WRAPPER_H__

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin-provider.h>
#include <panel/panel-module.h>

G_BEGIN_DECLS

typedef struct _PanelPluginExternalWrapperClass PanelPluginExternalWrapperClass;
typedef struct _PanelPluginExternalWrapper      PanelPluginExternalWrapper;

#define PANEL_TYPE_PLUGIN_EXTERNAL_WRAPPER            (panel_plugin_external_wrapper_get_type ())
#define PANEL_PLUGIN_EXTERNAL_WRAPPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_PLUGIN_EXTERNAL_WRAPPER, PanelPluginExternalWrapper))
#define PANEL_PLUGIN_EXTERNAL_WRAPPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_PLUGIN_EXTERNAL_WRAPPER, PanelPluginExternalWrapperClass))
#define PANEL_IS_PLUGIN_EXTERNAL_WRAPPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_PLUGIN_EXTERNAL_WRAPPER))
#define PANEL_IS_PLUGIN_EXTERNAL_WRAPPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_PLUGIN_EXTERNAL_WRAPPER))
#define PANEL_PLUGIN_EXTERNAL_WRAPPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PANEL_TYPE_PLUGIN_EXTERNAL_WRAPPER, PanelPluginExternalWrapperClass))

GType      panel_plugin_external_wrapper_get_type (void) G_GNUC_CONST;

GtkWidget *panel_plugin_external_wrapper_new      (PanelModule  *module,
                                                   gint          unique_id,
                                                   gchar       **arguments) G_GNUC_MALLOC;

G_END_DECLS

#endif /* !__PANEL_PLUGIN_EXTERNAL_WRAPPER_H__ */
