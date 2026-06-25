/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4ui/libxfce4ui.h>
#include <common/panel-xfconf.h>
#include <common/panel-utils.h>
#include <common/panel-private.h>
#include <libxfce4panel/libxfce4panel.h>

#include "tasklist-widget.h"
#include "tasklist-dialog_ui.h"


#define HANDLE_OFFSET (0.15)
#define HANDLE_SIZE      (4)


/* TODO move to header */
GType tasklist_plugin_get_type (void) G_GNUC_CONST;
void tasklist_plugin_register_type (XfcePanelTypeModule *type_module);
#define XFCE_TYPE_TASKLIST_PLUGIN            (tasklist_plugin_get_type ())
#define XFCE_TASKLIST_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_TASKLIST_PLUGIN, TasklistPlugin))
#define XFCE_TASKLIST_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_TASKLIST_PLUGIN, TasklistPluginClass))
#define XFCE_IS_TASKLIST_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_TASKLIST_PLUGIN))
#define XFCE_IS_TASKLIST_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_TASKLIST_PLUGIN))
#define XFCE_TASKLIST_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_TASKLIST_PLUGIN, TasklistPluginClass))


typedef struct _TasklistPluginClass TasklistPluginClass;
struct _TasklistPluginClass
{
  XfcePanelPluginClass __parent__;
};

typedef struct _TasklistPlugin TasklistPlugin;
struct _TasklistPlugin
{
  XfcePanelPlugin __parent__;

  /* the tasklist widget */
  GtkWidget     *tasklist;
  GtkWidget     *handle;
};



static void     tasklist_plugin_construct               (XfcePanelPlugin    *panel_plugin);
static void     tasklist_plugin_mode_changed            (XfcePanelPlugin    *panel_plugin,
                                                         XfcePanelPluginMode mode);
static gboolean tasklist_plugin_size_changed            (XfcePanelPlugin    *panel_plugin,
                                                         gint                size);
static void     tasklist_plugin_nrows_changed           (XfcePanelPlugin    *panel_plugin,
                                                         guint               nrows);
static void     tasklist_plugin_screen_position_changed (XfcePanelPlugin    *panel_plugin,
                                                         XfceScreenPosition  position);
static void     tasklist_plugin_configure_plugin        (XfcePanelPlugin    *panel_plugin);
static gboolean tasklist_plugin_handle_draw             (GtkWidget          *widget,
                                                         cairo_t            *cr,
                                                         TasklistPlugin     *plugin);



/* define and register the plugin */
XFCE_PANEL_DEFINE_PLUGIN_RESIDENT (TasklistPlugin, tasklist_plugin)



static void
tasklist_plugin_class_init (TasklistPluginClass *klass)
{
  XfcePanelPluginClass *plugin_class;

  plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
  plugin_class->construct = tasklist_plugin_construct;
  plugin_class->mode_changed = tasklist_plugin_mode_changed;
  plugin_class->size_changed = tasklist_plugin_size_changed;
  plugin_class->nrows_changed = tasklist_plugin_nrows_changed;
  plugin_class->screen_position_changed = tasklist_plugin_screen_position_changed;
  plugin_class->configure_plugin = tasklist_plugin_configure_plugin;
}



static void
tasklist_plugin_init (TasklistPlugin *plugin)
{
  GtkWidget *box;

  /* create widgets */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (plugin), box);
  g_object_bind_property (G_OBJECT (plugin), "orientation",
                          G_OBJECT (box), "orientation",
                          G_BINDING_SYNC_CREATE);
  gtk_widget_show (box);

  plugin->handle = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (box), plugin->handle, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (plugin->handle), "draw",
      G_CALLBACK (tasklist_plugin_handle_draw), plugin);
  gtk_widget_set_size_request (plugin->handle, 8, 8);
  gtk_widget_show (plugin->handle);

  plugin->tasklist = g_object_new (XFCE_TYPE_TASKLIST, NULL);
  gtk_box_pack_start (GTK_BOX (box), plugin->tasklist, TRUE, TRUE, 0);

  g_object_bind_property (G_OBJECT (plugin->tasklist), "show-handle",
                          G_OBJECT (plugin->handle), "visible",
                          G_BINDING_SYNC_CREATE);
}



static void
tasklist_plugin_construct (XfcePanelPlugin *panel_plugin)
{
  TasklistPlugin      *plugin = XFCE_TASKLIST_PLUGIN (panel_plugin);
  const PanelProperty  properties[] =
  {
    { "show-labels", G_TYPE_BOOLEAN },
    { "grouping", G_TYPE_BOOLEAN },
    { "include-all-workspaces", G_TYPE_BOOLEAN },
    { "include-all-monitors", G_TYPE_BOOLEAN },
    { "flat-buttons", G_TYPE_BOOLEAN },
    { "switch-workspace-on-unminimize", G_TYPE_BOOLEAN },
    { "show-only-minimized", G_TYPE_BOOLEAN },
    { "show-wireframes", G_TYPE_BOOLEAN },
    { "show-handle", G_TYPE_BOOLEAN },
    { "show-tooltips", G_TYPE_BOOLEAN },
    { "sort-order", G_TYPE_UINT },
    { "window-scrolling", G_TYPE_BOOLEAN },
    { "wrap-windows", G_TYPE_BOOLEAN },
    { "include-all-blinking", G_TYPE_BOOLEAN },
    { "middle-click", G_TYPE_UINT },
    { "label-decorations", G_TYPE_BOOLEAN },
    { NULL }
  };

  /* show configure */
  xfce_panel_plugin_menu_show_configure (XFCE_PANEL_PLUGIN (plugin));

  /* bind all properties */
  panel_properties_bind (NULL, G_OBJECT (plugin->tasklist),
                         xfce_panel_plugin_get_property_base (panel_plugin),
                         properties, FALSE);

  /* show the tasklist */
  gtk_widget_show (plugin->tasklist);
}



static void
tasklist_plugin_mode_changed (XfcePanelPlugin     *panel_plugin,
                              XfcePanelPluginMode  mode)
{
  TasklistPlugin *plugin = XFCE_TASKLIST_PLUGIN (panel_plugin);

  /* set the new tasklist mode */
  xfce_tasklist_set_mode (XFCE_TASKLIST (plugin->tasklist), mode);
}



static gboolean
tasklist_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                              gint             size)
{
  TasklistPlugin *plugin = XFCE_TASKLIST_PLUGIN (panel_plugin);

  /* set the tasklist size */
  xfce_tasklist_set_size (XFCE_TASKLIST (plugin->tasklist), size);

  return TRUE;
}



static void
tasklist_plugin_nrows_changed (XfcePanelPlugin *panel_plugin,
                               guint            nrows)
{
  TasklistPlugin *plugin = XFCE_TASKLIST_PLUGIN (panel_plugin);

  /* set the tasklist nrows */
  xfce_tasklist_set_nrows (XFCE_TASKLIST (plugin->tasklist), nrows);
}



static void
tasklist_plugin_screen_position_changed (XfcePanelPlugin    *panel_plugin,
                                         XfceScreenPosition  position)
{
  TasklistPlugin *plugin = XFCE_TASKLIST_PLUGIN (panel_plugin);

  /* update monitor geometry; this function is also triggered when
   * the panel is moved to another monitor during runtime */
  xfce_tasklist_update_monitor_geometry (XFCE_TASKLIST (plugin->tasklist));
}



static void
tasklist_plugin_configure_plugin (XfcePanelPlugin *panel_plugin)
{
  TasklistPlugin *plugin = XFCE_TASKLIST_PLUGIN (panel_plugin);
  GtkBuilder     *builder;
  GObject        *dialog;
  GObject        *object;

  /* setup the dialog */
  PANEL_UTILS_LINK_4UI
  builder = panel_utils_builder_new (panel_plugin, tasklist_dialog_ui,
                                     tasklist_dialog_ui_length, &dialog);
  if (G_UNLIKELY (builder == NULL))
    return;

#define TASKLIST_DIALOG_BIND(name, property) \
  object = gtk_builder_get_object (builder, (name)); \
  panel_return_if_fail (G_IS_OBJECT (object)); \
  g_object_bind_property (G_OBJECT (plugin->tasklist), (name), \
                          G_OBJECT (object), (property), \
                          G_BINDING_BIDIRECTIONAL \
                          | G_BINDING_SYNC_CREATE);

#define TASKLIST_DIALOG_BIND_INV(name, property) \
  object = gtk_builder_get_object (builder, (name)); \
  panel_return_if_fail (G_IS_OBJECT (object)); \
  g_object_bind_property (G_OBJECT (plugin->tasklist), \
                          name,  G_OBJECT (object), \
                          property, \
                          G_BINDING_BIDIRECTIONAL \
                          | G_BINDING_SYNC_CREATE \
                          | G_BINDING_INVERT_BOOLEAN);

  TASKLIST_DIALOG_BIND ("show-labels", "active")
  TASKLIST_DIALOG_BIND ("grouping", "active")
  TASKLIST_DIALOG_BIND ("include-all-workspaces", "active")
  TASKLIST_DIALOG_BIND ("include-all-monitors", "active")
  TASKLIST_DIALOG_BIND ("flat-buttons", "active")
  TASKLIST_DIALOG_BIND_INV ("switch-workspace-on-unminimize", "active")
  TASKLIST_DIALOG_BIND ("show-only-minimized", "active")
  TASKLIST_DIALOG_BIND ("show-wireframes", "active")
  TASKLIST_DIALOG_BIND ("show-handle", "active")
  TASKLIST_DIALOG_BIND ("show-tooltips", "active")
  TASKLIST_DIALOG_BIND ("sort-order", "active")
  TASKLIST_DIALOG_BIND ("window-scrolling", "active")
  TASKLIST_DIALOG_BIND ("middle-click", "active")

#ifndef GDK_WINDOWING_X11
  /* not functional in x11, so avoid confusion */
  object = gtk_builder_get_object (builder, "show-wireframes");
  gtk_widget_hide (GTK_WIDGET (object));
#endif

  gtk_widget_show (GTK_WIDGET (dialog));
}



static gboolean
tasklist_plugin_handle_draw (GtkWidget      *widget,
                             cairo_t        *cr,
                             TasklistPlugin *plugin)
{
  GtkAllocation     allocation;
  GtkStyleContext  *ctx;
  gdouble           x, y;
  guint             i;
  GdkRGBA           fg_rgba;

  panel_return_val_if_fail (XFCE_IS_TASKLIST_PLUGIN (plugin), FALSE);
  panel_return_val_if_fail (plugin->handle == widget, FALSE);

  if (!gtk_widget_is_drawable (widget))
    return FALSE;

  gtk_widget_get_allocation (widget, &allocation);
  ctx = gtk_widget_get_style_context (widget);

  gtk_style_context_get_color (ctx, gtk_widget_get_state_flags (widget), &fg_rgba);
  /* Tone down the foreground color a bit for the separators */
  fg_rgba.alpha = 0.5;
  gdk_cairo_set_source_rgba (cr, &fg_rgba);
  cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);

  x = (allocation.width - HANDLE_SIZE) / 2;
  y = (allocation.height - HANDLE_SIZE) / 2;
  cairo_set_line_width (cr, 1.0);
  /* draw the handle */
  for (i = 0; i < 3; i++)
    {
      if (xfce_panel_plugin_get_orientation (XFCE_PANEL_PLUGIN (plugin)) ==
          GTK_ORIENTATION_HORIZONTAL)
        {
          cairo_move_to (cr, x, y + (i * HANDLE_SIZE) - (HANDLE_SIZE / 2));
          cairo_line_to (cr, x + HANDLE_SIZE, y + (i * HANDLE_SIZE) - (HANDLE_SIZE / 2));
        }
      else
        {
          cairo_move_to (cr, x + (i * HANDLE_SIZE) - (HANDLE_SIZE / 2), y);
          cairo_line_to (cr, x + (i * HANDLE_SIZE) - (HANDLE_SIZE / 2), y + HANDLE_SIZE);
        }
      cairo_stroke (cr);
    }

  return TRUE;
}
