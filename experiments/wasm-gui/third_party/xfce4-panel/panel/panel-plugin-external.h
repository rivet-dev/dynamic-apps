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

#ifndef __PANEL_PLUGIN_EXTERNAL_H__
#define __PANEL_PLUGIN_EXTERNAL_H__

#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin-provider.h>
#include <panel/panel-module.h>

G_BEGIN_DECLS

typedef struct _PanelPluginExternalClass   PanelPluginExternalClass;
typedef struct _PanelPluginExternal        PanelPluginExternal;
typedef struct _PanelPluginExternalPrivate PanelPluginExternalPrivate;

#define PANEL_TYPE_PLUGIN_EXTERNAL            (panel_plugin_external_get_type ())
#define PANEL_PLUGIN_EXTERNAL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PANEL_TYPE_PLUGIN_EXTERNAL, PanelPluginExternal))
#define PANEL_PLUGIN_EXTERNAL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_PLUGIN_EXTERNAL, PanelPluginExternalClass))
#define PANEL_IS_PLUGIN_EXTERNAL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PANEL_TYPE_PLUGIN_EXTERNAL))
#define PANEL_IS_PLUGIN_EXTERNAL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PANEL_TYPE_PLUGIN_EXTERNAL))
#define PANEL_PLUGIN_EXTERNAL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PANEL_TYPE_PLUGIN_EXTERNAL, PanelPluginExternalClass))

struct _PanelPluginExternalClass
{
  GtkSocketClass __parent__;

  /* send panel values to the plugin or wrapper */
  void       (*set_properties) (PanelPluginExternal *external,
                                GSList              *properties);

  /* complete startup array for the plugin */
  gchar    **(*get_argv)       (PanelPluginExternal  *external,
                                gchar               **arguments);

  /* handling of remote events */
  gboolean   (*remote_event)   (PanelPluginExternal  *external,
                                const gchar          *name,
                                const GValue         *value,
                                guint                *handle);
};

struct _PanelPluginExternal
{
  GtkSocket __parent__;

  PanelPluginExternalPrivate *priv;

  PanelModule                *module;

  gint                        unique_id;

  /* some info received on plugin startup by the
   * implementations of the abstract object */
  guint                       show_configure : 1;
  guint                       show_about : 1;
};

typedef struct
{
  XfcePanelPluginProviderPropType type;
  GValue                          value;
}
PluginProperty;



GType        panel_plugin_external_get_type             (void) G_GNUC_CONST;

void         panel_plugin_external_restart              (PanelPluginExternal  *external);

void         panel_plugin_external_set_opacity          (PanelPluginExternal *external,
                                                         gdouble              opacity);

void         panel_plugin_external_set_background_color (PanelPluginExternal  *external,
                                                         const GdkRGBA        *color);

void         panel_plugin_external_set_background_image (PanelPluginExternal  *external,
                                                         const gchar          *image);

GPid         panel_plugin_external_get_pid              (PanelPluginExternal  *external);

G_END_DECLS

#endif /* !__PANEL_PLUGIN_EXTERNAL_H__ */
