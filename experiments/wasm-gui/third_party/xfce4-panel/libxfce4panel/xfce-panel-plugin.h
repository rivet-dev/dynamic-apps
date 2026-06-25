/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */



#ifndef __XFCE_PANEL_PLUGIN_H__
#define __XFCE_PANEL_PLUGIN_H__

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel-enums.h>

G_BEGIN_DECLS

typedef struct _XfcePanelPluginPrivate XfcePanelPluginPrivate;
typedef struct _XfcePanelPluginClass   XfcePanelPluginClass;
typedef struct _XfcePanelPlugin        XfcePanelPlugin;

/**
 * XfcePanelPluginFunc:
 * @plugin : an #XfcePanelPlugin
 *
 * Callback function to create the plugin contents. It should be given as
 * the argument to the registration macros.
 **/
typedef void (*XfcePanelPluginFunc) (XfcePanelPlugin *plugin);

/**
 * XfcePanelPluginPreInit:
 * @argc: number of arguments to the plugin
 * @argv: argument array
 *
 * Callback function that is run in an external plugin before gtk_init(). It
 * should return %FALSE if the plugin is not available for whatever reason.
 * The function can be given as argument to one of the registration macros.
 *
 * The main purpose of this callback is to allow multithreaded plugins to call
 * g_thread_init().
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 *
 * Since: 4.6
 **/
typedef gboolean (*XfcePanelPluginPreInit) (gint    argc,
                                            gchar **argv);

/**
 * XfcePanelPluginCheck:
 * @screen : the #GdkScreen the panel is running on
 *
 * Callback function that is run before creating a plugin. It should return
 * %FALSE if the plugin is not available for whatever reason. The function
 * can be given as argument to one of the registration macros.
 *
 * Returns: %TRUE if the plugin can be started, %FALSE otherwise.
 **/
typedef gboolean (*XfcePanelPluginCheck) (GdkScreen *screen);

#define XFCE_TYPE_PANEL_PLUGIN            (xfce_panel_plugin_get_type ())
#define XFCE_PANEL_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_PANEL_PLUGIN, XfcePanelPlugin))
#define XFCE_PANEL_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_PANEL_PLUGIN, XfcePanelPluginClass))
#define XFCE_IS_PANEL_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_PANEL_PLUGIN))
#define XFCE_IS_PANEL_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_PANEL_PLUGIN))
#define XFCE_PANEL_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_PANEL_PLUGIN, XfcePanelPluginClass))

/**
 * XfcePanelPluginClass:
 * @construct :               This function is for object orientated plugins and
 *                            triggered after the init function of the object.
 *                            When this function is triggered, the plugin
 *                            information like name, display name, comment and unique
 *                            id are available. This is also the place where you would
 *                            call functions like xfce_panel_plugin_menu_show_configure().
 *                            You can see this as the replacement of #XfcePanelPluginFunc
 *                            for object based plugins. Since 4.8.
 * @screen_position_changed : See #XfcePanelPlugin::screen-position-changed for more information.
 * @size_changed :            See #XfcePanelPlugin::size-changed for more information.
 * @orientation_changed :     See #XfcePanelPlugin::orientation-changed for more information.
 * @free_data :               See #XfcePanelPlugin::free-data for more information.
 * @save :                    See #XfcePanelPlugin::save for more information.
 * @about :                   See #XfcePanelPlugin::about for more information.
 * @configure_plugin :        See #XfcePanelPlugin::configure-plugin for more information.
 * @removed :                 See #XfcePanelPlugin::removed for more information.
 * @remote_event :            See #XfcePanelPlugin::remote-event for more information.
 * @mode_changed :            See #XfcePanelPlugin::mode-changed for more information.
 * @nrows_changed :           See #XfcePanelPlugin::nrows-changed for more information.
 *
 * Class of an #XfcePanelPlugin. The interface can be used to create GObject based plugin.
 **/
struct _XfcePanelPluginClass
{
  /*< private >*/
  GtkEventBoxClass __parent__;

  /*< public >*/
  /* for object oriented plugins only */
  void     (*construct)               (XfcePanelPlugin    *plugin);

  /* signals */
  void     (*screen_position_changed) (XfcePanelPlugin    *plugin,
                                       XfceScreenPosition  position);
  gboolean (*size_changed)            (XfcePanelPlugin    *plugin,
                                       gint                size);
  void     (*orientation_changed)     (XfcePanelPlugin    *plugin,
                                       GtkOrientation      orientation);
  void     (*free_data)               (XfcePanelPlugin    *plugin);
  void     (*save)                    (XfcePanelPlugin    *plugin);
  void     (*about)                   (XfcePanelPlugin    *plugin);
  void     (*configure_plugin)        (XfcePanelPlugin    *plugin);
  void     (*removed)                 (XfcePanelPlugin    *plugin);
  gboolean (*remote_event)            (XfcePanelPlugin    *plugin,
                                       const gchar        *name,
                                       const GValue       *value);

  /* new in 4.10 */
  void     (*mode_changed)            (XfcePanelPlugin    *plugin,
                                       XfcePanelPluginMode mode);
  void     (*nrows_changed)           (XfcePanelPlugin    *plugin,
                                       guint               rows);

  /*< private >*/
  void (*reserved1) (void);
  void (*reserved2) (void);
};


/**
 * XfcePanelPlugin:
 *
 * This struct contain private data only and should be accessed by
 * the functions below.
 **/
struct _XfcePanelPlugin
{
  /*< private >*/
  GtkEventBox __parent__;

  /*< private >*/
  XfcePanelPluginPrivate *priv;
};



GType                 xfce_panel_plugin_get_type            (void) G_GNUC_CONST;

const gchar          *xfce_panel_plugin_get_name            (XfcePanelPlugin   *plugin) G_GNUC_PURE;

const gchar          *xfce_panel_plugin_get_display_name    (XfcePanelPlugin   *plugin) G_GNUC_PURE;

const gchar          *xfce_panel_plugin_get_comment         (XfcePanelPlugin   *plugin) G_GNUC_PURE;

gint                  xfce_panel_plugin_get_unique_id       (XfcePanelPlugin   *plugin) G_GNUC_PURE;

const gchar          *xfce_panel_plugin_get_property_base   (XfcePanelPlugin   *plugin) G_GNUC_PURE;

const gchar * const  *xfce_panel_plugin_get_arguments       (XfcePanelPlugin   *plugin) G_GNUC_PURE;

gint                  xfce_panel_plugin_get_size            (XfcePanelPlugin   *plugin) G_GNUC_PURE;

gboolean              xfce_panel_plugin_get_expand          (XfcePanelPlugin   *plugin) G_GNUC_PURE;

void                  xfce_panel_plugin_set_expand          (XfcePanelPlugin   *plugin,
                                                             gboolean           expand);

gboolean              xfce_panel_plugin_get_shrink          (XfcePanelPlugin   *plugin) G_GNUC_PURE;

void                  xfce_panel_plugin_set_shrink          (XfcePanelPlugin   *plugin,
                                                             gboolean           shrink);

gboolean              xfce_panel_plugin_get_small           (XfcePanelPlugin   *plugin) G_GNUC_PURE;

void                  xfce_panel_plugin_set_small           (XfcePanelPlugin   *plugin,
                                                             gboolean           small);

gint                  xfce_panel_plugin_get_icon_size       (XfcePanelPlugin   *plugin) G_GNUC_PURE;

GtkOrientation        xfce_panel_plugin_get_orientation     (XfcePanelPlugin   *plugin) G_GNUC_PURE;

XfcePanelPluginMode   xfce_panel_plugin_get_mode            (XfcePanelPlugin   *plugin) G_GNUC_PURE;

guint                 xfce_panel_plugin_get_nrows           (XfcePanelPlugin   *plugin) G_GNUC_PURE;

XfceScreenPosition    xfce_panel_plugin_get_screen_position (XfcePanelPlugin   *plugin) G_GNUC_PURE;

void                  xfce_panel_plugin_take_window         (XfcePanelPlugin   *plugin,
                                                             GtkWindow         *window);

void                  xfce_panel_plugin_add_action_widget   (XfcePanelPlugin   *plugin,
                                                             GtkWidget         *widget);

void                  xfce_panel_plugin_menu_insert_item    (XfcePanelPlugin   *plugin,
                                                             GtkMenuItem       *item);

void                  xfce_panel_plugin_menu_show_configure (XfcePanelPlugin   *plugin);

void                  xfce_panel_plugin_menu_show_about     (XfcePanelPlugin   *plugin);

void                  xfce_panel_plugin_menu_destroy        (XfcePanelPlugin   *plugin);

gboolean              xfce_panel_plugin_get_locked          (XfcePanelPlugin   *plugin);

void                  xfce_panel_plugin_remove              (XfcePanelPlugin   *plugin);

void                  xfce_panel_plugin_block_menu          (XfcePanelPlugin   *plugin);

void                  xfce_panel_plugin_unblock_menu        (XfcePanelPlugin   *plugin);

void                  xfce_panel_plugin_register_menu       (XfcePanelPlugin   *plugin,
                                                             GtkMenu           *menu);

GtkArrowType          xfce_panel_plugin_arrow_type          (XfcePanelPlugin   *plugin);

void                  xfce_panel_plugin_position_widget     (XfcePanelPlugin   *plugin,
                                                             GtkWidget         *menu_widget,
                                                             GtkWidget         *attach_widget,
                                                             gint              *x,
                                                             gint              *y);

void                  xfce_panel_plugin_position_menu       (GtkMenu           *menu,
                                                             gint              *x,
                                                             gint              *y,
                                                             gboolean          *push_in,
                                                             gpointer           panel_plugin)
G_GNUC_DEPRECATED_FOR (xfce_panel_plugin_popup_menu());

void                  xfce_panel_plugin_popup_menu          (XfcePanelPlugin   *plugin,
                                                             GtkMenu           *menu,
                                                             GtkWidget         *widget,
                                                             const GdkEvent    *trigger_event);

void                  xfce_panel_plugin_focus_widget        (XfcePanelPlugin   *plugin,
                                                             GtkWidget         *widget);

void                  xfce_panel_plugin_block_autohide      (XfcePanelPlugin   *plugin,
                                                             gboolean           blocked);

gchar                *xfce_panel_plugin_lookup_rc_file      (XfcePanelPlugin   *plugin) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

gchar                *xfce_panel_plugin_save_location       (XfcePanelPlugin   *plugin,
                                                             gboolean           create) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* !__XFCE_PANEL_PLUGIN_H__ */
