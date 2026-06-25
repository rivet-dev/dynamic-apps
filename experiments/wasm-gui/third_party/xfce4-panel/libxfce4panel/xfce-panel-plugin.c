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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <glib.h>
#include <libxfce4util/libxfce4util.h>

#include <common/panel-private.h>
#include <libxfce4panel/xfce-panel-macros.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4panel/xfce-panel-plugin-provider.h>
#include <libxfce4panel/libxfce4panel-marshal.h>
#include <libxfce4panel/libxfce4panel-alias.h>



/**
 * SECTION: xfce-panel-plugin
 * @title: XfcePanelPlugin
 * @short_description: Interface for panel plugins
 * @include: libxfce4panel/libxfce4panel.h
 *
 * The interface plugin developers used to interact with the plugin and
 * the panel.
 **/



#define XFCE_PANEL_PLUGIN_CONSTRUCTED(plugin) \
  PANEL_HAS_FLAG (XFCE_PANEL_PLUGIN (plugin)->priv->flags, \
                  PLUGIN_FLAG_CONSTRUCTED)



typedef const gchar *(*ProviderToPluginChar) (XfcePanelPluginProvider *provider);
typedef gint         (*ProviderToPluginInt)  (XfcePanelPluginProvider *provider);



static void          xfce_panel_plugin_provider_init          (XfcePanelPluginProviderInterface *iface);
static GObject      *xfce_panel_plugin_constructor            (GType                             type,
                                                               guint                             n_props,
                                                               GObjectConstructParam            *props);
static void          xfce_panel_plugin_get_property           (GObject                          *object,
                                                               guint                             prop_id,
                                                               GValue                           *value,
                                                               GParamSpec                       *pspec);
static void          xfce_panel_plugin_set_property           (GObject                          *object,
                                                               guint                             prop_id,
                                                               const GValue                     *value,
                                                               GParamSpec                       *pspec);
static void          xfce_panel_plugin_dispose                (GObject                          *object);
static void          xfce_panel_plugin_finalize               (GObject                          *object);
static void          xfce_panel_plugin_realize                (GtkWidget                        *widget);
static gboolean      xfce_panel_plugin_button_press_event     (GtkWidget                        *widget,
                                                               GdkEventButton                   *event);
static void          xfce_panel_plugin_menu_move              (XfcePanelPlugin                  *plugin);
static void          xfce_panel_plugin_menu_remove            (XfcePanelPlugin                  *plugin);
static void          xfce_panel_plugin_menu_add_items         (XfcePanelPlugin                  *plugin);
static void          xfce_panel_plugin_menu_panel_preferences (XfcePanelPlugin                  *plugin);
static GtkMenu      *xfce_panel_plugin_menu_get               (XfcePanelPlugin                  *plugin);
static inline gchar *xfce_panel_plugin_relative_filename      (XfcePanelPlugin                  *plugin);
static void          xfce_panel_plugin_unregister_menu        (GtkMenu                          *menu,
                                                               XfcePanelPlugin                  *plugin);
static void          xfce_panel_plugin_set_size               (XfcePanelPluginProvider          *provider,
                                                               gint                              size);
static void          xfce_panel_plugin_set_icon_size          (XfcePanelPluginProvider          *provider,
                                                               gint                              icon_size);
static void          xfce_panel_plugin_set_dark_mode          (XfcePanelPluginProvider          *provider,
                                                               gboolean                          dark_mode);
static void          xfce_panel_plugin_set_mode               (XfcePanelPluginProvider          *provider,
                                                               XfcePanelPluginMode               mode);
static void          xfce_panel_plugin_set_nrows              (XfcePanelPluginProvider          *provider,
                                                               guint                             nrows);
static void          xfce_panel_plugin_set_screen_position    (XfcePanelPluginProvider          *provider,
                                                               XfceScreenPosition                screen_position);
static void          xfce_panel_plugin_save                   (XfcePanelPluginProvider          *provider);
static gboolean      xfce_panel_plugin_get_show_configure     (XfcePanelPluginProvider          *provider);
static void          xfce_panel_plugin_show_configure         (XfcePanelPluginProvider          *provider);
static gboolean      xfce_panel_plugin_get_show_about         (XfcePanelPluginProvider          *provider);
static void          xfce_panel_plugin_show_about             (XfcePanelPluginProvider          *provider);
static void          xfce_panel_plugin_removed                (XfcePanelPluginProvider          *provider);
static gboolean      xfce_panel_plugin_remote_event           (XfcePanelPluginProvider          *provider,
                                                               const gchar                      *name,
                                                               const GValue                     *value,
                                                               guint                            *handle);
static void          xfce_panel_plugin_set_locked             (XfcePanelPluginProvider          *provider,
                                                               gboolean                          locked);
static void          xfce_panel_plugin_ask_remove             (XfcePanelPluginProvider          *provider);



enum
{
  PROP_0,
  PROP_NAME,
  PROP_DISPLAY_NAME,
  PROP_COMMENT,
  PROP_ARGUMENTS,
  PROP_UNIQUE_ID,
  PROP_ORIENTATION,
  PROP_SIZE,
  PROP_ICON_SIZE,
  PROP_DARK_MODE,
  PROP_SMALL,
  PROP_SCREEN_POSITION,
  PROP_EXPAND,
  PROP_MODE,
  PROP_NROWS,
  PROP_SHRINK,
  N_PROPERTIES
};

enum
{
  ABOUT,
  CONFIGURE_PLUGIN,
  FREE_DATA,
  ORIENTATION_CHANGED,
  REMOTE_EVENT,
  REMOVED,
  SAVE,
  SIZE_CHANGED,
  SCREEN_POSITION_CHANGED,
  MODE_CHANGED,
  NROWS_CHANGED,
  LAST_SIGNAL
};

typedef enum
{
  PLUGIN_FLAG_DISPOSED       = 1 << 0,
  PLUGIN_FLAG_CONSTRUCTED    = 1 << 1,
  PLUGIN_FLAG_REALIZED       = 1 << 2,
  PLUGIN_FLAG_SHOW_CONFIGURE = 1 << 3,
  PLUGIN_FLAG_SHOW_ABOUT     = 1 << 4,
}
PluginFlags;

struct _XfcePanelPluginPrivate
{
  /* plugin information */
  gchar               *name;
  gchar               *display_name;
  gchar               *comment;
  gint                 unique_id;
  gchar               *property_base;
  gchar              **arguments;
  gint                 size; /* single row size */
  gint                 icon_size;
  gboolean             dark_mode;
  guint                expand : 1;
  guint                shrink : 1;
  guint                nrows;
  XfcePanelPluginMode  mode;
  guint                small : 1;
  XfceScreenPosition   screen_position;
  guint                locked : 1;
  GSList              *menu_items;

  /* flags for rembering states */
  PluginFlags          flags;

  /* plugin right-click menu */
  GtkMenu             *menu;

  /* menu block counter (configure insensitive) */
  gint                 menu_blocked;

  /* autohide block counter */
  gint                 panel_lock;
};



static guint       plugin_signals[LAST_SIGNAL];
static GQuark      item_properties = 0;
static GQuark      item_about = 0;
static GParamSpec *plugin_props[N_PROPERTIES] = { NULL, };



G_DEFINE_TYPE_WITH_CODE (XfcePanelPlugin, xfce_panel_plugin, GTK_TYPE_EVENT_BOX,
                         G_ADD_PRIVATE (XfcePanelPlugin)
                         G_IMPLEMENT_INTERFACE (XFCE_TYPE_PANEL_PLUGIN_PROVIDER,
                                                xfce_panel_plugin_provider_init));



static void
xfce_panel_plugin_class_init (XfcePanelPluginClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  klass->construct = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructor = xfce_panel_plugin_constructor;
  gobject_class->get_property = xfce_panel_plugin_get_property;
  gobject_class->set_property = xfce_panel_plugin_set_property;
  gobject_class->dispose = xfce_panel_plugin_dispose;
  gobject_class->finalize = xfce_panel_plugin_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = xfce_panel_plugin_realize;
  gtkwidget_class->button_press_event = xfce_panel_plugin_button_press_event;

  /**
   * XfcePanelPlugin::about
   * @plugin : an #XfcePanelPlugin.
   *
   * This signal is emmitted when the About entry in the right-click
   * menu is clicked. Plugin writers can use it to show information
   * about the plugin and display credits of the developers, translators
   * and other contributors.
   *
   * See also: xfce_panel_plugin_menu_show_about().
   **/
  plugin_signals[ABOUT] =
    g_signal_new (g_intern_static_string ("about"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfcePanelPluginClass, about),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * XfcePanelPlugin::configure-plugin
   * @plugin : an #XfcePanelPlugin.
   *
   * This signal is emmitted when the Properties entry in the right-click
   * menu is clicked. Plugin writers can use this signal to open a
   * plugin settings dialog. It is their responsibility to block/unblock panel
   * autohide when the dialog is shown/hidden.
   *
   * See also: xfce_panel_plugin_menu_show_configure() and
   *           xfce_titled_dialog_new ().
   **/
  plugin_signals[CONFIGURE_PLUGIN] =
    g_signal_new (g_intern_static_string ("configure-plugin"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfcePanelPluginClass, configure_plugin),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * XfcePanelPlugin::free-data
   * @plugin : an #XfcePanelPlugin.
   *
   * This signal is emmitted when the plugin is closing. Plugin
   * writers should use this signal to free any allocated resources.
   **/
  plugin_signals[FREE_DATA] =
    g_signal_new (g_intern_static_string ("free-data"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfcePanelPluginClass, free_data),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * XfcePanelPlugin::orientation-changed
   * @plugin      : an #XfcePanelPlugin.
   * @orientation : new #GtkOrientation of the panel.
   *
   * This signal is emmitted whenever the orientation of the panel
   * the @plugin is on changes. Plugins writers can for example use
   * this signal to change the order of widgets in the plugin.
   **/
  plugin_signals[ORIENTATION_CHANGED] =
    g_signal_new (g_intern_static_string ("orientation-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfcePanelPluginClass, orientation_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1, GTK_TYPE_ORIENTATION);

  /**
   * XfcePanelPlugin::mode-changed
   * @plugin : an #XfcePanelPlugin.
   * @mode   : new #XfcePanelPluginMode of the panel.
   *
   * This signal is emmitted whenever the mode of the panel
   * the @plugin is on changes.
   *
   * Since: 4.10
   **/
  plugin_signals[MODE_CHANGED] =
    g_signal_new (g_intern_static_string ("mode-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfcePanelPluginClass, mode_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1, XFCE_TYPE_PANEL_PLUGIN_MODE);

  /**
   * XfcePanelPlugin::nrows-changed
   * @plugin : an #XfcePanelPlugin.
   * @rows   : new number of rows of the panel
   *
   * This signal is emmitted whenever the nrows of the panel
   * the @plugin is on changes.
   *
   * Since: 4.10
   **/
  plugin_signals[NROWS_CHANGED] =
    g_signal_new (g_intern_static_string ("nrows-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfcePanelPluginClass, nrows_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE, 1, G_TYPE_UINT);

  /**
   * XfcePanelPlugin::remote-event
   * @plugin : an #XfcePanelPlugin.
   * @name   : name of the signal.
   * @value  : value of the signal.
   *
   * This signal is emmitted by the user by running
   * xfce4-panel --plugin-event=plugin-name:name:type:value. It can be
   * used for remote communication, like for example to popup a menu.
   *
   * Returns: %TRUE to stop signal emission to other plugins, %FALSE
   *          to send the signal also to other plugins with the same
   *          name.
   **/
  plugin_signals[REMOTE_EVENT] =
    g_signal_new (g_intern_static_string ("remote-event"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfcePanelPluginClass, remote_event),
                  NULL, NULL,
                  _libxfce4panel_marshal_BOOLEAN__STRING_BOXED,
                  G_TYPE_BOOLEAN, 2, G_TYPE_STRING, G_TYPE_VALUE);

  /**
   * XfcePanelPlugin::removed
   * @plugin : an #XfcePanelPlugin.
   *
   * This signal is emmitted when the plugin is permanently removed from
   * the panel configuration by the user. Developers can use this signal
   * to cleanup custom setting locations that for example store passwords.
   *
   * The free-data signal is emitted after this signal!
   *
   * Note that if you use the xfconf channel and base property provided
   * by xfce_panel_plugin_get_property_base() or the rc file location
   * returned by xfce_panel_plugin_save_location(), the panel will take
   * care of removing those settings.
   *
   * Since: 4.8
   **/
  plugin_signals[REMOVED] =
    g_signal_new (g_intern_static_string ("removed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfcePanelPluginClass, removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * XfcePanelPlugin::save
   * @plugin : an #XfcePanelPlugin.
   *
   * This signal is emitted when the plugin should save it's
   * configuration. The signal is always emmitted before the plugin
   * closes (before the "free-data" signal) and also once in 10
   * minutes or so.
   *
   * See also: xfce_panel_plugin_save_location().
   **/
  plugin_signals[SAVE] =
    g_signal_new (g_intern_static_string ("save"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfcePanelPluginClass, save),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * XfcePanelPlugin::size-changed
   * @plugin : an #XfcePanelPlugin.
   * @size   : the new size of the panel.
   *
   * This signal is emmitted whenever the size of the panel
   * the @plugin is on changes. Plugins writers can for example use
   * this signal to update their icon size.
   *
   * If the function returns %FALSE or is not used, the panel will force
   * a square size to the plugin. If you want non-square plugins and you
   * don't need this signal you can use something like this:
   *
   * g_signal_connect (plugin, "size-changed", G_CALLBACK (gtk_true), NULL);
   **/
  plugin_signals[SIZE_CHANGED] =
    g_signal_new (g_intern_static_string ("size-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfcePanelPluginClass, size_changed),
                  g_signal_accumulator_true_handled, NULL,
                  _libxfce4panel_marshal_BOOLEAN__INT,
                  G_TYPE_BOOLEAN, 1, G_TYPE_INT);

  /**
   * XfcePanelPlugin::screen-position-changed
   * @plugin   : an #XfcePanelPlugin.
   * @position : the new #XfceScreenPosition of the panel.
   *
   * This signal is emmitted whenever the screen position of the panel
   * the @plugin is on changes. Plugins writers can for example use
   * this signal to change the arrow direction of buttons.
   **/
  plugin_signals[SCREEN_POSITION_CHANGED] =
    g_signal_new (g_intern_static_string ("screen-position-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfcePanelPluginClass, screen_position_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1, XFCE_TYPE_SCREEN_POSITION);

  /**
   * XfcePanelPlugin:name:
   *
   * The internal, unstranslated, name of the #XfcePanelPlugin. Plugin
   * writer can use it to read the plugin name, but
   * xfce_panel_plugin_get_name() is recommended since that returns
   * a const string.
   **/
  plugin_props[PROP_NAME] =
      g_param_spec_string ("name",
                           "Name",
                           "Plugin internal name",
                           NULL,
                           G_PARAM_READWRITE
                           | G_PARAM_STATIC_STRINGS
                           | G_PARAM_CONSTRUCT_ONLY);

  /**
   * XfcePanelPlugin:display-name:
   *
   * The translated display name of the #XfcePanelPlugin. This property is set
   * during plugin construction and can't be set twice. Plugin writer can use
   * it to read the plugin display name, but xfce_panel_plugin_get_display_name()
   * is recommended.
   **/
  plugin_props[PROP_DISPLAY_NAME] =
      g_param_spec_string ("display-name",
                           "Display Name",
                           "Plugin display name",
                           NULL,
                           G_PARAM_READWRITE
                           | G_PARAM_STATIC_STRINGS
                           | G_PARAM_CONSTRUCT_ONLY);

  /**
   * XfcePanelPlugin:comment:
   *
   * The translated description of the #XfcePanelPlugin. This property is set
   * during plugin construction and can't be set twice. Plugin writer can use
   * it to read the plugin description, but xfce_panel_plugin_get_comment()
   * is recommended.
   *
   * Since: 4.8
   **/
  plugin_props[PROP_COMMENT] =
      g_param_spec_string ("comment",
                           "Comment",
                           "Plugin comment",
                           NULL,
                           G_PARAM_READWRITE
                           | G_PARAM_STATIC_STRINGS
                           | G_PARAM_CONSTRUCT_ONLY);

  /**
   * XfcePanelPlugin:id:
   *
   * The unique id of the #XfcePanelPlugin. This property is set during plugin
   * construction and can't be set twice. Plugin writer can use it to read the
   * plugin display name, but xfce_panel_plugin_get_unique_id() is recommended.
   *
   * Since: 4.8
   **/
  plugin_props[PROP_UNIQUE_ID] =
      g_param_spec_int ("unique-id",
                        "Unique ID",
                        "Unique plugin ID",
                        -1, G_MAXINT, -1,
                        G_PARAM_READWRITE
                        | G_PARAM_STATIC_STRINGS
                        | G_PARAM_CONSTRUCT_ONLY);

  /**
   * XfcePanelPlugin:arguments:
   *
   * The arguments the plugin was started with. If the plugin was not
   * started with any arguments this value is %NULL. Plugin writer can
   * use it to read the arguments array, but
   * xfce_panel_plugin_get_arguments() is recommended.
   **/
  plugin_props[PROP_ARGUMENTS] =
      g_param_spec_boxed ("arguments",
                          "Arguments",
                          "Startup arguments for the plugin",
                          G_TYPE_STRV,
                          G_PARAM_READWRITE
                          | G_PARAM_STATIC_STRINGS
                          | G_PARAM_CONSTRUCT_ONLY);

  /**
   * XfcePanelPlugin:orientation:
   *
   * The #GtkOrientation of the #XfcePanelPlugin. Plugin writer can use it to read the
   * plugin orientation, but xfce_panel_plugin_get_orientation() is recommended.
   **/
  plugin_props[PROP_ORIENTATION] =
      g_param_spec_enum ("orientation",
                         "Orientation",
                         "Orientation of the plugin's panel",
                         GTK_TYPE_ORIENTATION,
                         GTK_ORIENTATION_HORIZONTAL,
                         G_PARAM_READABLE
                         | G_PARAM_STATIC_STRINGS);

  /**
   * XfcePanelPlugin:size:
   *
   * The size in pixels of the #XfcePanelPlugin. Plugin writer can use it to read the
   * plugin size, but xfce_panel_plugin_get_size() is recommended.
   **/
  plugin_props[PROP_SIZE] =
      g_param_spec_int ("size",
                        "Size",
                        "Size of the plugin's panel",
                        0, (128 * 6), 0,
                        G_PARAM_READABLE
                        | G_PARAM_STATIC_STRINGS);

  /**
   * XfcePanelPlugin:icon-size:
   *
   * The icon-size in pixels of the #XfcePanelPlugin. Plugin writers can use it to read the
   * plugin's icon size, but xfce_panel_plugin_get_icon_size() is recommended.
   *
   * Since: 4.14
   **/
  plugin_props[PROP_ICON_SIZE] =
      g_param_spec_int ("icon-size",
                        "Icon Size",
                        "Size of the plugin's icon",
                        0, (256 * 6), 0,
                        G_PARAM_READABLE
                        | G_PARAM_STATIC_STRINGS);

  /**
   * XfcePanelPlugin:dark-mode:
   *
   * Whether the #XfcePanelPlugin shall request the Gtk dark theme variant (based on the panel
   * setting).
   *
   * Since: 4.14
   **/
  plugin_props[PROP_DARK_MODE] =
      g_param_spec_boolean ("dark-mode",
                            "Dark Mode",
                            "Whether or not to request the Gtk dark theme variant",
                            FALSE,
                            G_PARAM_READWRITE
                            | G_PARAM_STATIC_STRINGS);

  /**
   * XfcePanelPlugin:screen-position:
   *
   * The #XfceScreenPosition of the #XfcePanelPlugin. Plugin writer can use it
   * to read the plugin's screen position, but xfce_panel_plugin_get_screen_position()
   * is recommended.
   **/
  plugin_props[PROP_SCREEN_POSITION] =
      g_param_spec_enum  ("screen-position",
                          "Screen Position",
                          "Screen position of the plugin's panel",
                          XFCE_TYPE_SCREEN_POSITION,
                          XFCE_SCREEN_POSITION_NONE,
                          G_PARAM_READABLE
                          | G_PARAM_STATIC_STRINGS);

  /**
   * XfcePanelPlugin:small:
   *
   * Whether the #XfcePanelPlugin is small enough to fit a single row of a multi-row panel.
   * Plugin writers can use it to read or set this property, but xfce_panel_plugin_set_small()
   * is recommended.
   *
   * Since: 4.10
   **/
  plugin_props[PROP_SMALL] =
      g_param_spec_boolean ("small",
                            "Small",
                            "Is this plugin small, e.g. a single button?",
                            FALSE,
                            G_PARAM_READWRITE
                            | G_PARAM_STATIC_STRINGS);

  /**
   * XfcePanelPlugin:expand:
   *
   * Whether the #XfcePanelPlugin expands on the panel. Plugin writers can use it
   * to read or set this property, but xfce_panel_plugin_set_expand()
   * is recommended.
   **/
  plugin_props[PROP_EXPAND] =
      g_param_spec_boolean ("expand",
                            "Expand",
                            "Whether this plugin is expanded",
                            FALSE,
                            G_PARAM_READWRITE
                            | G_PARAM_STATIC_STRINGS);

  /**
   * XfcePanelPlugin:shrink:
   *
   * Whether the #XfcePanelPlugin can shrink when there is no space left on the panel.
   * Plugin writers can use it to read or set this property, but xfce_panel_plugin_set_shrink()
   * is recommended.
   *
   * Since: 4.10
   **/
  plugin_props[PROP_SHRINK] =
      g_param_spec_boolean ("shrink",
                            "Shrink",
                            "Whether this plugin can shrink",
                            FALSE,
                            G_PARAM_READWRITE
                            | G_PARAM_STATIC_STRINGS);

  /**
   * XfcePanelPlugin:mode:
   *
   * Display mode of the plugin.
   *
   * Since: 4.10
   **/
  plugin_props[PROP_MODE] =
      g_param_spec_enum ("mode",
                         "Mode",
                         "Disply mode of the plugin",
                         XFCE_TYPE_PANEL_PLUGIN_MODE,
                         XFCE_PANEL_PLUGIN_MODE_HORIZONTAL,
                         G_PARAM_READABLE
                         | G_PARAM_STATIC_STRINGS);

  /**
   * XfcePanelPlugin:nrows:
   *
   * Number of rows the plugin is embedded on.
   *
   * Since: 4.10
   **/
  plugin_props[PROP_NROWS] =
      g_param_spec_uint ("nrows",
                         "Nrows",
                         "Number of rows of the panel",
                         1, 6, 1,
                         G_PARAM_READABLE
                         | G_PARAM_STATIC_STRINGS);

  /* install all properties */
  g_object_class_install_properties (gobject_class, N_PROPERTIES, plugin_props);

  item_properties = g_quark_from_static_string ("item-properties");
  item_about = g_quark_from_static_string ("item-about");
}



static void
xfce_panel_plugin_init (XfcePanelPlugin *plugin)
{
  plugin->priv = xfce_panel_plugin_get_instance_private (plugin);

  plugin->priv->name = NULL;
  plugin->priv->display_name = NULL;
  plugin->priv->comment = NULL;
  plugin->priv->unique_id = -1;
  plugin->priv->property_base = NULL;
  plugin->priv->arguments = NULL;
  plugin->priv->size = 0;
  plugin->priv->icon_size = 0;
  plugin->priv->dark_mode = FALSE;
  plugin->priv->small = FALSE;
  plugin->priv->expand = FALSE;
  plugin->priv->shrink = FALSE;
  plugin->priv->mode = XFCE_PANEL_PLUGIN_MODE_HORIZONTAL;
  plugin->priv->screen_position = XFCE_SCREEN_POSITION_NONE;
  plugin->priv->menu = NULL;
  plugin->priv->menu_blocked = 0;
  plugin->priv->panel_lock = 0;
  plugin->priv->flags = 0;
  plugin->priv->locked = TRUE;
  plugin->priv->menu_items = NULL;
  plugin->priv->nrows = 1;

  /* bind the text domain of the panel so our strings
   * are properly translated in the old 4.6 panel plugins */
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  if (g_getenv ("PANEL_GDK_CORE_DEVICE_EVENTS"))
    g_unsetenv ("GDK_CORE_DEVICE_EVENTS");

  /* hide the event box window to make the plugin transparent */
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (plugin), FALSE);
}



static void
xfce_panel_plugin_provider_init (XfcePanelPluginProviderInterface *iface)
{
  iface->get_name = (ProviderToPluginChar) xfce_panel_plugin_get_name;
  iface->get_unique_id = (ProviderToPluginInt) xfce_panel_plugin_get_unique_id;
  iface->set_size = xfce_panel_plugin_set_size;
  iface->set_icon_size = xfce_panel_plugin_set_icon_size;
  iface->set_dark_mode = xfce_panel_plugin_set_dark_mode;
  iface->set_mode = xfce_panel_plugin_set_mode;
  iface->set_nrows = xfce_panel_plugin_set_nrows;
  iface->set_screen_position = xfce_panel_plugin_set_screen_position;
  iface->save = xfce_panel_plugin_save;
  iface->get_show_configure = xfce_panel_plugin_get_show_configure;
  iface->show_configure = xfce_panel_plugin_show_configure;
  iface->get_show_about = xfce_panel_plugin_get_show_about;
  iface->show_about = xfce_panel_plugin_show_about;
  iface->removed = xfce_panel_plugin_removed;
  iface->remote_event = xfce_panel_plugin_remote_event;
  iface->set_locked = xfce_panel_plugin_set_locked;
  iface->ask_remove = xfce_panel_plugin_ask_remove;
}



static GObject *
xfce_panel_plugin_constructor (GType                  type,
                               guint                  n_props,
                               GObjectConstructParam *props)
{
  GObject *plugin;

  plugin = G_OBJECT_CLASS (xfce_panel_plugin_parent_class)->constructor (type, n_props, props);

  /* all the properties are set and can be used in public */
  PANEL_SET_FLAG (XFCE_PANEL_PLUGIN (plugin)->priv->flags, PLUGIN_FLAG_CONSTRUCTED);

  return plugin;
}



static void
xfce_panel_plugin_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  XfcePanelPluginPrivate *private = XFCE_PANEL_PLUGIN (object)->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_static_string (value, private->name);
      break;

    case PROP_DISPLAY_NAME:
      g_value_set_static_string (value, private->display_name);
      break;

    case PROP_COMMENT:
      g_value_set_static_string (value, private->comment);
      break;

    case PROP_UNIQUE_ID:
      g_value_set_int (value, private->unique_id);
      break;

    case PROP_ARGUMENTS:
      g_value_set_boxed (value, private->arguments);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, xfce_panel_plugin_get_orientation (XFCE_PANEL_PLUGIN (object)));
      break;

    case PROP_SIZE:
      g_value_set_int (value, private->size * private->nrows);
      break;

    case PROP_ICON_SIZE:
      g_value_set_uint (value, private->icon_size);
      break;

    case PROP_DARK_MODE:
      g_value_set_boolean (value, private->dark_mode);
      break;

    case PROP_NROWS:
      g_value_set_uint (value, private->nrows);
      break;

    case PROP_MODE:
      g_value_set_enum (value, private->mode);
      break;

    case PROP_SMALL:
      g_value_set_boolean (value, private->small);
      break;

    case PROP_SCREEN_POSITION:
      g_value_set_enum (value, private->screen_position);
      break;

    case PROP_EXPAND:
      g_value_set_boolean (value, private->expand);
      break;

    case PROP_SHRINK:
      g_value_set_boolean (value, private->shrink);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_panel_plugin_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  XfcePanelPluginPrivate *private = XFCE_PANEL_PLUGIN (object)->priv;
  gchar                  *name;

  switch (prop_id)
    {
    case PROP_NAME:
    case PROP_UNIQUE_ID:
      if (prop_id == PROP_NAME)
        private->name = g_value_dup_string (value);
      else
        private->unique_id = g_value_get_int (value);

      if (private->unique_id != -1 && private->name != NULL)
        {
          /* give the widget a unique name for theming */
          name = g_strdup_printf ("%s-%d", private->name, private->unique_id);
          gtk_widget_set_name (GTK_WIDGET (object), name);
          g_free (name);
        }
      break;

    case PROP_DISPLAY_NAME:
      private->display_name = g_value_dup_string (value);
      break;

    case PROP_COMMENT:
      private->comment = g_value_dup_string (value);
      break;

    case PROP_ARGUMENTS:
      private->arguments = g_value_dup_boxed (value);
      break;

    case PROP_DARK_MODE:
      xfce_panel_plugin_set_dark_mode (XFCE_PANEL_PLUGIN_PROVIDER (object),
                                       g_value_get_boolean (value));
      break;

    case PROP_SMALL:
      xfce_panel_plugin_set_small (XFCE_PANEL_PLUGIN (object),
                                   g_value_get_boolean (value));
      break;

    case PROP_EXPAND:
      xfce_panel_plugin_set_expand (XFCE_PANEL_PLUGIN (object),
                                    g_value_get_boolean (value));
      break;

    case PROP_SHRINK:
      xfce_panel_plugin_set_shrink (XFCE_PANEL_PLUGIN (object),
                                    g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_panel_plugin_dispose (GObject *object)
{
  XfcePanelPlugin *plugin = XFCE_PANEL_PLUGIN (object);

  if (!PANEL_HAS_FLAG (plugin->priv->flags, PLUGIN_FLAG_DISPOSED))
    {
      /* allow the plugin to cleanup */
      g_signal_emit (G_OBJECT (object), plugin_signals[FREE_DATA], 0);

      /* plugin disposed, don't try this again */
      PANEL_SET_FLAG (plugin->priv->flags, PLUGIN_FLAG_DISPOSED);
    }

  (*G_OBJECT_CLASS (xfce_panel_plugin_parent_class)->dispose) (object);
}



static void
xfce_panel_plugin_finalize (GObject *object)
{
  XfcePanelPlugin *plugin = XFCE_PANEL_PLUGIN (object);

  g_slist_free_full (plugin->priv->menu_items, g_object_unref);
  g_free (plugin->priv->name);
  g_free (plugin->priv->display_name);
  g_free (plugin->priv->comment);
  g_free (plugin->priv->property_base);
  g_strfreev (plugin->priv->arguments);

  (*G_OBJECT_CLASS (xfce_panel_plugin_parent_class)->finalize) (object);
}



static void
xfce_panel_plugin_realize (GtkWidget *widget)
{
  XfcePanelPluginClass *klass;
  XfcePanelPlugin      *plugin = XFCE_PANEL_PLUGIN (widget);

  /* let gtk realize the plugin */
  (*GTK_WIDGET_CLASS (xfce_panel_plugin_parent_class)->realize) (widget);

  /* launch the construct function for object oriented plugins, but
   * do this only once */
  if (!PANEL_HAS_FLAG (plugin->priv->flags, PLUGIN_FLAG_REALIZED))
    {
      PANEL_SET_FLAG (plugin->priv->flags, PLUGIN_FLAG_REALIZED);

      /* whether this is an object plugin */
      klass = XFCE_PANEL_PLUGIN_GET_CLASS (widget);
      if (klass->construct != NULL)
        (*klass->construct) (XFCE_PANEL_PLUGIN (widget));
    }
}



static gboolean
xfce_panel_plugin_button_press_event (GtkWidget      *widget,
                                      GdkEventButton *event)
{
  XfcePanelPlugin *plugin = XFCE_PANEL_PLUGIN (widget);
  guint            modifiers;
  GtkMenu         *menu;
  GtkWidget       *item;

  panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (widget), FALSE);

  /* get the default accelerator modifier mask */
  modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

  if (event->button == 3
      || (event->button == 1 && modifiers == GDK_CONTROL_MASK))
    {
      /* get the panel menu */
      menu = xfce_panel_plugin_menu_get (plugin);

      /* if the menu is block, some items are insensitive */
      item = g_object_get_qdata (G_OBJECT (menu), item_properties);
      if (item != NULL)
        gtk_widget_set_sensitive (item, plugin->priv->menu_blocked == 0);

      /* popup the menu */
      gtk_menu_popup_at_pointer (menu, (GdkEvent *) event);
      return TRUE;
    }

  return FALSE;
}



static gboolean
xfce_panel_plugin_idle_move (gpointer user_data)
{
  XfcePanelPlugin *plugin = user_data;

  panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), FALSE);
  panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (plugin), FALSE);

  /* move the plugin */
  xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                          PROVIDER_SIGNAL_MOVE_PLUGIN);

  return FALSE;
}



static void
xfce_panel_plugin_menu_move (XfcePanelPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (plugin));

  /* wait for the popup to go down */
  g_idle_add (xfce_panel_plugin_idle_move, plugin);
}



static void
xfce_panel_plugin_menu_remove (XfcePanelPlugin *plugin)
{
  GtkWidget *dialog;

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));

  xfce_panel_plugin_block_autohide (plugin, TRUE);

  /* create question dialog (same code is also in panel-preferences-dialog.c) */
  dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
      GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
      /* I18N: %s is the name of the plugin */
      _("Are you sure that you want to remove \"%s\"?"),
      xfce_panel_plugin_get_display_name (plugin));
  gtk_window_set_screen (GTK_WINDOW (dialog),
      gtk_widget_get_screen (GTK_WIDGET (plugin)));
  gtk_window_set_title (GTK_WINDOW (dialog),
      _("Remove Item"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
      _("Removing the item from the panel also means its configuration will be lost."));
  gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Cancel"),
      GTK_RESPONSE_NO, _("_Remove"), GTK_RESPONSE_YES, NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_NO);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES)
    {
      gtk_widget_hide (dialog);

      /* send signal to unlock the panel before removing the plugin */
      xfce_panel_plugin_block_autohide (plugin, FALSE);

      /* ask the panel or wrapper to remove the plugin */
      xfce_panel_plugin_remove (plugin);
    }
  else
    xfce_panel_plugin_block_autohide (plugin, FALSE);

  gtk_widget_destroy (dialog);
}



static void
xfce_panel_plugin_menu_add_items (XfcePanelPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (plugin));
  panel_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  /* open items dialog */
  if (!xfce_panel_plugin_get_locked (plugin))
    xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                            PROVIDER_SIGNAL_ADD_NEW_ITEMS);
}



static void
xfce_panel_plugin_menu_panel_preferences (XfcePanelPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (plugin));
  panel_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  /* open preferences dialog */
  if (!xfce_panel_plugin_get_locked (plugin))
    xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                            PROVIDER_SIGNAL_PANEL_PREFERENCES);
}



static void
xfce_panel_plugin_menu_panel_logout (XfcePanelPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (plugin));
  panel_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  /* logout the session */
  xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                          PROVIDER_SIGNAL_PANEL_LOGOUT);
}



static void
xfce_panel_plugin_menu_panel_about (XfcePanelPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (plugin));
  panel_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  /* open the about dialog of the panel */
  xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                          PROVIDER_SIGNAL_PANEL_ABOUT);
}



static void
xfce_panel_plugin_menu_panel_help (XfcePanelPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (plugin));
  panel_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  /* open the manual of the panel */
  xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                          PROVIDER_SIGNAL_PANEL_HELP);
}



static GtkMenu *
xfce_panel_plugin_menu_get (XfcePanelPlugin *plugin)
{
  GtkWidget *menu, *submenu;
  GtkWidget *item;
  GtkWidget *image;
  gboolean   locked;
  GSList    *li;

  panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), NULL);

  if (G_UNLIKELY (plugin->priv->menu == NULL))
    {
      locked = xfce_panel_plugin_get_locked (plugin);

      menu = gtk_menu_new ();
      gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (plugin), NULL);

      /* item with plugin name */
      item = gtk_menu_item_new_with_label (xfce_panel_plugin_get_display_name (plugin));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_set_sensitive (item, FALSE);
      gtk_widget_show (item);

      /* add custom menu items */
      for (li = plugin->priv->menu_items; li != NULL; li = li->next)
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), GTK_WIDGET (li->data));

      /* separator */
      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      if (!locked)
        {
          /* properties item */
          item = panel_image_menu_item_new_with_mnemonic (_("_Properties"));
          g_signal_connect_swapped (G_OBJECT (item), "activate",
              G_CALLBACK (xfce_panel_plugin_show_configure), plugin);
          g_object_set_qdata (G_OBJECT (menu), item_properties, item);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          image = gtk_image_new_from_icon_name ("document-properties", GTK_ICON_SIZE_MENU);
          panel_image_menu_item_set_image (item, image);
          if (PANEL_HAS_FLAG (plugin->priv->flags, PLUGIN_FLAG_SHOW_CONFIGURE))
            gtk_widget_show (item);

          /* about item */
          item = panel_image_menu_item_new_with_mnemonic (_("_About"));
          g_signal_connect_swapped (G_OBJECT (item), "activate",
              G_CALLBACK (xfce_panel_plugin_show_about), plugin);
          g_object_set_qdata (G_OBJECT (menu), item_about, item);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          image = gtk_image_new_from_icon_name ("help-about", GTK_ICON_SIZE_MENU);
          panel_image_menu_item_set_image (item, image);
          if (PANEL_HAS_FLAG (plugin->priv->flags, PLUGIN_FLAG_SHOW_ABOUT))
            gtk_widget_show (item);

          /* move item */
          item = panel_image_menu_item_new_with_mnemonic (_("_Move"));
          g_signal_connect_swapped (G_OBJECT (item), "activate",
              G_CALLBACK (xfce_panel_plugin_menu_move), plugin);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);

          image = gtk_image_new_from_icon_name ("go-next", GTK_ICON_SIZE_MENU);
          panel_image_menu_item_set_image (item, image);
          gtk_widget_show (image);

          /* separator */
          item = gtk_separator_menu_item_new ();
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);

          /* remove */
          item = panel_image_menu_item_new_with_mnemonic (_("_Remove"));
          g_signal_connect_object (G_OBJECT (item), "activate",
              G_CALLBACK (xfce_panel_plugin_menu_remove), plugin, G_CONNECT_SWAPPED);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);

          image = gtk_image_new_from_icon_name ("list-remove", GTK_ICON_SIZE_MENU);
          panel_image_menu_item_set_image (item, image);
          gtk_widget_show (image);

          /* separator */
          item = gtk_separator_menu_item_new ();
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);
        }

      /* create a panel submenu item */
      submenu = gtk_menu_new ();
      item = gtk_menu_item_new_with_mnemonic (_("Pane_l"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
      gtk_widget_show (item);

      if (!locked)
        {
          /* add new items */
          item = panel_image_menu_item_new_with_mnemonic (_("Add _New Items..."));
          g_signal_connect_swapped (G_OBJECT (item), "activate",
              G_CALLBACK (xfce_panel_plugin_menu_add_items), plugin);
          gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
          gtk_widget_show (item);

          image = gtk_image_new_from_icon_name ("list-add", GTK_ICON_SIZE_MENU);
          panel_image_menu_item_set_image (item, image);
          gtk_widget_show (image);

          /* customize panel */
          item = panel_image_menu_item_new_with_mnemonic (_("Panel Pr_eferences..."));
          g_signal_connect_swapped (G_OBJECT (item), "activate",
              G_CALLBACK (xfce_panel_plugin_menu_panel_preferences), plugin);
          gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
          gtk_widget_show (item);

          image = gtk_image_new_from_icon_name ("preferences-system", GTK_ICON_SIZE_MENU);
          panel_image_menu_item_set_image (item, image);
          gtk_widget_show (image);

          /* separator */
          item = gtk_separator_menu_item_new ();
          gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
          gtk_widget_show (item);
        }

      /* logout item */
      item = panel_image_menu_item_new_with_mnemonic (_("Log _Out"));
      g_signal_connect_swapped (G_OBJECT (item), "activate",
          G_CALLBACK (xfce_panel_plugin_menu_panel_logout), plugin);
      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
      gtk_widget_show (item);

      image = gtk_image_new_from_icon_name ("system-log-out", GTK_ICON_SIZE_MENU);
      panel_image_menu_item_set_image (item, image);
      gtk_widget_show (image);

      /* separator */
      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
      gtk_widget_show (item);

      /* help item */
      item = panel_image_menu_item_new_with_mnemonic (_("_Help"));
      g_signal_connect_swapped (G_OBJECT (item), "activate",
          G_CALLBACK (xfce_panel_plugin_menu_panel_help), plugin);
      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
      gtk_widget_show (item);

      image = gtk_image_new_from_icon_name ("help-browser", GTK_ICON_SIZE_MENU);
      panel_image_menu_item_set_image (item, image);
      gtk_widget_show (image);

      /* about item */
      item = panel_image_menu_item_new_with_mnemonic (_("About"));
      g_signal_connect_swapped (G_OBJECT (item), "activate",
          G_CALLBACK (xfce_panel_plugin_menu_panel_about), plugin);
      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
      gtk_widget_show (item);

      image = gtk_image_new_from_icon_name ("help-about", GTK_ICON_SIZE_MENU);
      panel_image_menu_item_set_image (item, image);
      gtk_widget_show (image);

      /* set panel menu */
      plugin->priv->menu = GTK_MENU (menu);
    }

  /* block autohide when this menu is shown */
  xfce_panel_plugin_register_menu (plugin, GTK_MENU (plugin->priv->menu));

  return plugin->priv->menu;
}



static inline gchar *
xfce_panel_plugin_relative_filename (XfcePanelPlugin *plugin)
{
  panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), NULL);
  panel_return_val_if_fail (xfce_panel_plugin_get_name (plugin) != NULL, NULL);
  panel_return_val_if_fail (xfce_panel_plugin_get_unique_id (plugin) != -1, NULL);

  /* return the relative configuration filename */
  return g_strdup_printf (PANEL_PLUGIN_RC_RELATIVE_PATH,
                          plugin->priv->name, plugin->priv->unique_id);
}



static void
xfce_panel_plugin_unregister_menu (GtkMenu         *menu,
                                   XfcePanelPlugin *plugin)
{
  guint id;

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  panel_return_if_fail (GTK_IS_MENU (menu));

  /* disconnect this signal */
  g_signal_handlers_disconnect_by_func (G_OBJECT (menu),
      G_CALLBACK (xfce_panel_plugin_unregister_menu), plugin);

  /* remove pending source */
  id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (menu), "menu-reposition-id"));
  if (id != 0)
    {
      g_source_remove (id);
      g_object_set_data (G_OBJECT (menu), "menu-reposition-id", GUINT_TO_POINTER (0));
    }

  /* tell panel it needs to unlock */
  xfce_panel_plugin_block_autohide (plugin, FALSE);
}



static void
xfce_panel_plugin_set_size (XfcePanelPluginProvider *provider,
                            gint                     size)
{
  XfcePanelPlugin *plugin = XFCE_PANEL_PLUGIN (provider);
  gboolean         handled = FALSE;
  gint             real_size;

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (provider));

  /* check if update is required, -1 for forced property emit
   * by xfce_panel_plugin_set_nrows */
  if (G_LIKELY (plugin->priv->size != size))
    {
      if (size != -1)
        plugin->priv->size = size;

      real_size = plugin->priv->size * plugin->priv->nrows;

      g_signal_emit (G_OBJECT (plugin),
                     plugin_signals[SIZE_CHANGED], 0, real_size, &handled);

      /* handle the size when not done by the plugin */
      if (!handled)
        gtk_widget_set_size_request (GTK_WIDGET (plugin), real_size, real_size);

      g_object_notify_by_pspec (G_OBJECT (plugin), plugin_props[PROP_SIZE]);
    }
}



static void
xfce_panel_plugin_set_icon_size (XfcePanelPluginProvider *provider,
                                 gint                     icon_size)
{
  XfcePanelPlugin *plugin = XFCE_PANEL_PLUGIN (provider);

  if (G_LIKELY (plugin->priv->icon_size != icon_size))
    {
      plugin->priv->icon_size = icon_size;
      g_object_notify_by_pspec (G_OBJECT (plugin), plugin_props[PROP_ICON_SIZE]);

      /* also update the size so the icon gets re-rendered */
      xfce_panel_plugin_set_size (provider, -1);
    }
}



static void
xfce_panel_plugin_set_dark_mode (XfcePanelPluginProvider *provider,
                                 gboolean                 dark_mode)
{
  XfcePanelPlugin *plugin = XFCE_PANEL_PLUGIN (provider);
  GtkSettings *gtk_settings;

  if (G_LIKELY (plugin->priv->dark_mode != dark_mode))
    {
      plugin->priv->dark_mode = dark_mode;
      g_object_notify_by_pspec (G_OBJECT (plugin), plugin_props[PROP_DARK_MODE]);

      gtk_settings = gtk_widget_get_settings (GTK_WIDGET (plugin));

      if (!dark_mode)
        gtk_settings_reset_property (gtk_settings,
                                     "gtk-application-prefer-dark-theme");

      g_object_set (gtk_settings,
                    "gtk-application-prefer-dark-theme",
                    dark_mode,
                    NULL);
    }
}



static void
xfce_panel_plugin_set_mode (XfcePanelPluginProvider *provider,
                            XfcePanelPluginMode      mode)
{
  XfcePanelPlugin *plugin = XFCE_PANEL_PLUGIN (provider);
  GtkOrientation   old_orientation;
  GtkOrientation   new_orientation;

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (provider));

  /* check if update is required */
  if (G_LIKELY (plugin->priv->mode != mode))
    {
      old_orientation = xfce_panel_plugin_get_orientation (plugin);

      plugin->priv->mode = mode;

      g_signal_emit (G_OBJECT (plugin),
                     plugin_signals[MODE_CHANGED], 0, mode);

      g_object_notify_by_pspec (G_OBJECT (plugin), plugin_props[PROP_MODE]);

      /* emit old orientation property for compatibility */
      new_orientation = xfce_panel_plugin_get_orientation (plugin);
      if (old_orientation != new_orientation)
        {
          g_signal_emit (G_OBJECT (plugin),
                         plugin_signals[ORIENTATION_CHANGED], 0, new_orientation);

          g_object_notify_by_pspec (G_OBJECT (plugin), plugin_props[PROP_ORIENTATION]);
        }
    }
}



static void
xfce_panel_plugin_set_nrows (XfcePanelPluginProvider *provider,
                             guint                    nrows)
{
  XfcePanelPlugin *plugin = XFCE_PANEL_PLUGIN (provider);

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (provider));

  nrows = MAX (nrows, 1);

  /* check if update is required */
  if (G_LIKELY (plugin->priv->nrows != nrows))
    {
      plugin->priv->nrows = nrows;

      g_signal_emit (G_OBJECT (plugin),
                     plugin_signals[NROWS_CHANGED], 0, nrows);

      g_object_notify_by_pspec (G_OBJECT (plugin), plugin_props[PROP_NROWS]);

      /* also the size changed */
      xfce_panel_plugin_set_size (provider, -1);
    }
}



static void
xfce_panel_plugin_set_screen_position (XfcePanelPluginProvider *provider,
                                       XfceScreenPosition       screen_position)
{
  XfcePanelPlugin *plugin = XFCE_PANEL_PLUGIN (provider);

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (provider));

  /* check if update is required */
  if (G_LIKELY (plugin->priv->screen_position != screen_position
      || xfce_screen_position_is_floating (screen_position)))
    {
      plugin->priv->screen_position = screen_position;

      g_signal_emit (G_OBJECT (plugin),
                     plugin_signals[SCREEN_POSITION_CHANGED], 0,
                     screen_position);

      g_object_notify_by_pspec (G_OBJECT (plugin), plugin_props[PROP_SCREEN_POSITION]);
    }
}



static void
xfce_panel_plugin_save (XfcePanelPluginProvider *provider)
{
  XfcePanelPlugin *plugin = XFCE_PANEL_PLUGIN (provider);

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (provider));

  /* only send the save signal if the plugin is not locked */
  if (XFCE_PANEL_PLUGIN (provider)->priv->menu_blocked == 0
      && !xfce_panel_plugin_get_locked (plugin))
    g_signal_emit (G_OBJECT (provider), plugin_signals[SAVE], 0);
}



static gboolean
xfce_panel_plugin_get_show_configure (XfcePanelPluginProvider *provider)
{
  panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (provider), FALSE);

  return PANEL_HAS_FLAG (XFCE_PANEL_PLUGIN (provider)->priv->flags,
                         PLUGIN_FLAG_SHOW_CONFIGURE);
}



static void
xfce_panel_plugin_show_configure (XfcePanelPluginProvider *provider)
{
  XfcePanelPlugin *plugin = XFCE_PANEL_PLUGIN (provider);

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (provider));

  if (plugin->priv->menu_blocked == 0
      && !xfce_panel_plugin_get_locked (plugin))
    g_signal_emit (G_OBJECT (plugin), plugin_signals[CONFIGURE_PLUGIN], 0);
}



static gboolean
xfce_panel_plugin_get_show_about (XfcePanelPluginProvider *provider)
{
  panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (provider), FALSE);

  return PANEL_HAS_FLAG (XFCE_PANEL_PLUGIN (provider)->priv->flags,
                         PLUGIN_FLAG_SHOW_ABOUT);
}



static void
xfce_panel_plugin_show_about (XfcePanelPluginProvider *provider)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (provider));

  g_signal_emit (G_OBJECT (provider), plugin_signals[ABOUT], 0);
}



static void
xfce_panel_plugin_removed (XfcePanelPluginProvider *provider)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (provider));

  if (!xfce_panel_plugin_get_locked (XFCE_PANEL_PLUGIN (provider)))
    g_signal_emit (G_OBJECT (provider), plugin_signals[REMOVED], 0);
}



static gboolean
xfce_panel_plugin_remote_event (XfcePanelPluginProvider *provider,
                                const gchar             *name,
                                const GValue            *value,
                                guint                   *handle)
{
  gboolean stop_emission;

  panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (provider), TRUE);
  panel_return_val_if_fail (name != NULL, TRUE);
  panel_return_val_if_fail (value == NULL || G_IS_VALUE (value), TRUE);

  g_signal_emit (G_OBJECT (provider), plugin_signals[REMOTE_EVENT], 0,
                 name, value, &stop_emission);

  return stop_emission;
}



static void
xfce_panel_plugin_set_locked (XfcePanelPluginProvider *provider,
                              gboolean                 locked)
{
  XfcePanelPlugin *plugin = XFCE_PANEL_PLUGIN (provider);

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (provider));

  if (G_LIKELY (plugin->priv->locked != locked))
    {
      plugin->priv->locked = locked;

      /* destroy the menu if it exists */
      if (plugin->priv->locked)
        xfce_panel_plugin_menu_destroy (plugin);
    }
}



static void
xfce_panel_plugin_ask_remove (XfcePanelPluginProvider *provider)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (provider));

  xfce_panel_plugin_menu_remove (XFCE_PANEL_PLUGIN (provider));
}



/**
 * xfce_panel_plugin_get_name:
 * @plugin : an #XfcePanelPlugin.
 *
 * The internal name of the panel plugin.
 *
 * Returns: the name of the panel plugin.
 **/
const gchar *
xfce_panel_plugin_get_name (XfcePanelPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), NULL);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), NULL);

  return plugin->priv->name;
}



/**
 * xfce_panel_plugin_get_display_name:
 * @plugin : an #XfcePanelPlugin.
 *
 * This returns the translated name of the plugin set in the .desktop
 * file of the plugin.
 *
 * Returns: the (translated) display name of the plugin.
 **/
const gchar *
xfce_panel_plugin_get_display_name (XfcePanelPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), NULL);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), NULL);

  if (G_LIKELY (plugin->priv->display_name))
    return plugin->priv->display_name;

  return plugin->priv->name;
}



/**
 * xfce_panel_plugin_get_comment:
 * @plugin : an #XfcePanelPlugin.
 *
 * This returns the translated comment of the plugin set in
 * the .desktop file of the plugin.
 *
 * Returns: the (translated) comment of the plugin.
 *
 * Since: 4.8
 **/
const gchar *
xfce_panel_plugin_get_comment (XfcePanelPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), NULL);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), NULL);

  return plugin->priv->comment;
}



/**
 * xfce_panel_plugin_get_unique_id:
 * @plugin : an #XfcePanelPlugin.
 *
 * The internal unique id of the plugin. Each plugin in the panel has
 * a unique number that is for example used for the config file name
 * or property base in the xfconf channel.
 *
 * Returns: the unique id of the plugin.
 *
 * Since 4.8
 **/
gint
xfce_panel_plugin_get_unique_id (XfcePanelPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), -1);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), -1);

  return plugin->priv->unique_id;
}



/**
 * xfce_panel_plugin_get_property_base:
 * @plugin : an #XfcePanelPlugin.
 *
 * The property base for this plugin in the xfce4-panel XfconfChannel,
 * this name is something like /plugins/plugin-1.
 *
 * Returns: the property base for the xfconf channel userd by a plugin.
 *
 * See also: xfconf_channel_new_with_property_base.
 *           XFCE_PANEL_PLUGIN_CHANNEL_NAME and
 *           xfce_panel_get_channel_name
 **/
const gchar *
xfce_panel_plugin_get_property_base (XfcePanelPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), NULL);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), NULL);

  /* create the propert if needed */
  if (plugin->priv->property_base == NULL)
    plugin->priv->property_base = g_strdup_printf (PLUGINS_PROPERTY_BASE,
                                                   plugin->priv->unique_id);

  return plugin->priv->property_base;
}



/**
 * xfce_panel_plugin_get_arguments:
 * @plugin : an #XfcePanelPlugin.
 *
 * Argument vector passed to the plugin when it was added. Most of the
 * time the return value will be %NULL, but if could for example contain
 * a list of filenames when the user added the plugin with
 *
 * xfce4-panel --add=launcher *.desktop
 *
 * see the code of the launcher plugin how to use this.
 *
 * Returns: the argument vector. The vector is owned by the plugin and
 *          should not be freed.
 *
 * Since: 4.8
 **/
const gchar * const *
xfce_panel_plugin_get_arguments (XfcePanelPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), NULL);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), NULL);

  return (const gchar * const *) plugin->priv->arguments;
}



/**
 * xfce_panel_plugin_get_size:
 * @plugin : an #XfcePanelPlugin.
 *
 * The size of the panel in which the plugin is embedded.
 *
 * Returns: the current size of the panel.
 **/
gint
xfce_panel_plugin_get_size (XfcePanelPlugin *plugin)
{
  gint real_size;

  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), -1);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), -1);

  /* always return a 'positive' size that makes sence */
  real_size = plugin->priv->size * plugin->priv->nrows;
  return MAX (16, real_size);
}



/**
 * xfce_panel_plugin_get_expand:
 * @plugin : an #XfcePanelPlugin.
 *
 * Whether the plugin is expanded or not. This set by the plugin using
 * xfce_panel_plugin_set_expand().
 *
 * Returns: %TRUE when the plugin should expand,
 *          %FALSE otherwise.
 **/
gboolean
xfce_panel_plugin_get_expand (XfcePanelPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), FALSE);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), FALSE);

  return plugin->priv->expand;
}



/**
 * xfce_panel_plugin_set_expand:
 * @plugin : an #XfcePanelPlugin.
 * @expand : whether to expand the plugin.
 *
 * Whether the plugin should expand of not
 **/
void
xfce_panel_plugin_set_expand (XfcePanelPlugin *plugin,
                              gboolean         expand)
{
  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  /* normalize the value */
  expand = !!expand;

  /* check if update is required */
  if (G_LIKELY (plugin->priv->expand != expand))
    {
      plugin->priv->expand = expand;

      /* emit signal (in provider) */
      xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                              expand ? PROVIDER_SIGNAL_EXPAND_PLUGIN :
                                                  PROVIDER_SIGNAL_COLLAPSE_PLUGIN);

      g_object_notify_by_pspec (G_OBJECT (plugin), plugin_props[PROP_EXPAND]);
    }
}



/**
 * xfce_panel_plugin_get_shrink:
 * @plugin : an #XfcePanelPlugin.
 *
 * Whether the plugin can shrink if the size on the panel is limited. This
 * is effective with plugins that do not have expand set, but can accept
 * a smaller size when needed.
 *
 * Returns: %TRUE when the plugin can shrink,
 *          %FALSE otherwise.
 *
 * Since: 4.10
 **/
gboolean
xfce_panel_plugin_get_shrink (XfcePanelPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), FALSE);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), FALSE);

  return plugin->priv->shrink;
}



/**
 * xfce_panel_plugin_set_shrink:
 * @plugin : an #XfcePanelPlugin.
 * @shrink : whether the plugin can shrink.
 *
 * Whether the plugin can shrink if the size on the panel
 * is limited. This does not work if the plugin is expanded.
 **/
void
xfce_panel_plugin_set_shrink (XfcePanelPlugin *plugin,
                              gboolean         shrink)
{
  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  /* normalize the value */
  shrink = !!shrink;

  /* check if update is required */
  if (G_LIKELY (plugin->priv->shrink != shrink))
    {
      plugin->priv->shrink = shrink;

      /* emit signal (in provider) */
      xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                              shrink ? PROVIDER_SIGNAL_SHRINK_PLUGIN :
                                                  PROVIDER_SIGNAL_UNSHRINK_PLUGIN);

      g_object_notify_by_pspec (G_OBJECT (plugin), plugin_props[PROP_SHRINK]);
    }
}


/**
 * xfce_panel_plugin_get_small:
 * @plugin : an #XfcePanelPlugin.
 *
 * Whether the plugin is small enough to fit in a single row of
 * a multi-row panel. E.g. if it is a button-like applet.
 *
 * Returns: %TRUE when the plugin is small,
 *          %FALSE otherwise.
 *
 * Since: 4.10
 **/
gboolean
xfce_panel_plugin_get_small (XfcePanelPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), FALSE);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), FALSE);

  return plugin->priv->small;
}



/**
 * xfce_panel_plugin_set_small:
 * @plugin : an #XfcePanelPlugin.
 * @small : whether the plugin is a small button-like applet.
 *
 * Whether the plugin is small enough to fit in a single row of
 * a multi-row panel. E.g. if it is a button-like applet.
 **/
void
xfce_panel_plugin_set_small (XfcePanelPlugin *plugin,
                             gboolean         small)
{
  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  /* normalize the value */
  small = !!small;

  /* check if update is required */
  if (G_LIKELY (plugin->priv->small != small))
    {
      plugin->priv->small = small;

      /* emit signal (in provider) */
      xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                              small ? PROVIDER_SIGNAL_SMALL_PLUGIN :
                                              PROVIDER_SIGNAL_UNSMALL_PLUGIN);

      g_object_notify_by_pspec (G_OBJECT (plugin), plugin_props[PROP_SMALL]);
    }
}



/**
 * xfce_panel_plugin_get_icon_size:
 * @plugin : an #XfcePanelPlugin,
 *
 * Returns either the icon size defined in the panel's settings or
 * a preferred icon size.
 *
 * Since: 4.14
 **/
gint
xfce_panel_plugin_get_icon_size (XfcePanelPlugin *plugin)
{
  gint width;

  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), FALSE);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), FALSE);


  /* 0 is handled as 'automatic sizing' */
  if (plugin->priv->icon_size == 0)
    {
      width = xfce_panel_plugin_get_size (plugin) / xfce_panel_plugin_get_nrows (plugin);

      /* Since symbolic icons are usually only provided in 16px we
      *  try to be clever and use size steps.
         Some assumptions: We set 0px padding on panel buttons in the panel's internal
         css, we expect that each button still has a 1px border, so we deduct 4px from
         the panel width for the size steps to avoid clipping. */
      if (width <= 19)
        return 12;
      else if (width <= 27)
        return 16;
      else if (width <= 35)
        return 24;
      else if (width <= 41)
        return 32;
      else
        return width - 4;
    }
  else
    {
      return plugin->priv->icon_size;
    }
}


/**
 * xfce_panel_plugin_get_orientation:
 * @plugin : an #XfcePanelPlugin.
 *
 * The orientation of the panel in which the plugin is embedded.
 *
 * Returns: the current #GtkOrientation of the panel.
 **/
GtkOrientation
xfce_panel_plugin_get_orientation (XfcePanelPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), GTK_ORIENTATION_HORIZONTAL);

  if (plugin->priv->mode == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL)
    return GTK_ORIENTATION_HORIZONTAL;
  else
    return GTK_ORIENTATION_VERTICAL;
}



/**
 * xfce_panel_plugin_get_mode:
 * @plugin : an #XfcePanelPlugin.
 *
 * The mode of the panel in which the plugin is embedded.
 *
 * Returns: the current #XfcePanelPluginMode of the panel.
 *
 * Since: 4.10
 **/
XfcePanelPluginMode
xfce_panel_plugin_get_mode (XfcePanelPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), XFCE_PANEL_PLUGIN_MODE_HORIZONTAL);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), XFCE_PANEL_PLUGIN_MODE_HORIZONTAL);

  return plugin->priv->mode;
}



/**
 * xfce_panel_plugin_get_nrows:
 * @plugin : an #XfcePanelPlugin.
 *
 * The number of rows of the panel in which the plugin is embedded.
 *
 * Returns: the current number of rows of the panel.
 *
 * Since: 4.10
 **/
guint
xfce_panel_plugin_get_nrows (XfcePanelPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), 1);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), 1);

  return plugin->priv->nrows;
}



/**
 * xfce_panel_plugin_get_screen_position:
 * @plugin : an #XfcePanelPlugin.
 *
 * The screen position of the panel in which the plugin is embedded.
 *
 * Returns: the current #XfceScreenPosition of the panel.
 **/
XfceScreenPosition
xfce_panel_plugin_get_screen_position (XfcePanelPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), XFCE_SCREEN_POSITION_NONE);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), XFCE_SCREEN_POSITION_NONE);

  return plugin->priv->screen_position;
}



/**
 * xfce_panel_plugin_take_window:
 * @plugin : an #XfcePanelPlugin.
 * @window : a #GtkWindow.
 *
 * Connect a dialog to a plugin. When the @plugin is closed, it will
 * destroy the @window.
 *
 * Since: 4.8
 **/
void
xfce_panel_plugin_take_window (XfcePanelPlugin *plugin,
                               GtkWindow       *window)
{
  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (GTK_IS_WINDOW (window));

  gtk_window_set_screen (window, gtk_widget_get_screen (GTK_WIDGET (plugin)));
  g_signal_connect_object (plugin, "destroy", G_CALLBACK (gtk_widget_destroy),
                           window, G_CONNECT_SWAPPED);
}



/**
 * xfce_panel_plugin_add_action_widget:
 * @plugin : an #XfcePanelPlugin.
 * @widget : a #GtkWidget that receives mouse events.
 *
 * Attach the plugin menu to this widget. Plugin writers should call this
 * for every widget that can receive mouse events. If you forget to call this
 * the plugin will not have a right-click menu and the user won't be able to
 * remove it.
 **/
void
xfce_panel_plugin_add_action_widget (XfcePanelPlugin *plugin,
                                     GtkWidget       *widget)
{
  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_signal_connect_swapped (G_OBJECT (widget), "button-press-event",
      G_CALLBACK (xfce_panel_plugin_button_press_event), plugin);
}



/**
 * xfce_panel_plugin_menu_insert_item:
 * @plugin : an #XfcePanelPlugin.
 * @item   : a #GtkMenuItem.
 *
 * Insert a custom menu item to the plugin's right click menu. This item
 * is packed below the first item in the menu, which displays the plugin's
 * name.
 **/
void
xfce_panel_plugin_menu_insert_item (XfcePanelPlugin *plugin,
                                    GtkMenuItem     *item)
{
  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (GTK_IS_MENU_ITEM (item));

  /* take the item and add to internal list */
  plugin->priv->menu_items = g_slist_append (plugin->priv->menu_items,
                                             g_object_ref_sink (item));
}



/**
 * xfce_panel_plugin_menu_show_configure:
 * @plugin : an #XfcePanelPlugin.
 *
 * Show the "Properties" item in the menu. Clicking on the menu item
 * will emit the "configure-plugin" signal.
 **/
void
xfce_panel_plugin_menu_show_configure (XfcePanelPlugin *plugin)
{
  GtkMenu   *menu;
  GtkWidget *item;

  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  PANEL_SET_FLAG (plugin->priv->flags, PLUGIN_FLAG_SHOW_CONFIGURE);

  /* show the menu item if the menu is already generated */
  if (G_UNLIKELY (plugin->priv->menu != NULL))
    {
       /* get and show the properties item */
       menu = xfce_panel_plugin_menu_get (plugin);
       item = g_object_get_qdata (G_OBJECT (menu), item_properties);
       if (G_LIKELY (item != NULL))
         gtk_widget_show (item);
    }

  /* emit signal, used by the external plugin */
  xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                          PROVIDER_SIGNAL_SHOW_CONFIGURE);
}



/**
 * xfce_panel_plugin_menu_show_about:
 * @plugin : an #XfcePanelPlugin.
 *
 * Show the "About" item in the menu. Clicking on the menu item
 * will emit the "about" signal.
 **/
void
xfce_panel_plugin_menu_show_about (XfcePanelPlugin *plugin)
{
  GtkMenu   *menu;
  GtkWidget *item;

  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  PANEL_SET_FLAG (plugin->priv->flags, PLUGIN_FLAG_SHOW_ABOUT);

  /* show the menu item if the menu is already generated */
  if (G_UNLIKELY (plugin->priv->menu != NULL))
    {
       /* get and show the about item */
       menu = xfce_panel_plugin_menu_get (plugin);
       item = g_object_get_qdata (G_OBJECT (menu), item_about);
       if (G_LIKELY (item != NULL))
         gtk_widget_show (item);
    }

  /* emit signal, used by the external plugin */
  xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                          PROVIDER_SIGNAL_SHOW_ABOUT);
}



/**
 * xfce_panel_plugin_menu_destroy:
 * @plugin : an #XfcePanelPlugin.
 *
 * Remove all custom menu items added through #xfce_panel_plugin_menu_insert_item
 * from the menu.
 **/
void
xfce_panel_plugin_menu_destroy (XfcePanelPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  panel_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  g_slist_free_full (plugin->priv->menu_items, g_object_unref);
  plugin->priv->menu_items = NULL;

  /* ignore the request for destruction if the menu is popped up */
  if (plugin->priv->menu != NULL && !gtk_widget_get_visible (GTK_WIDGET (plugin->priv->menu)))
    {
      gtk_menu_detach (GTK_MENU (plugin->priv->menu));
      plugin->priv->menu = NULL;
    }
}




/**
 * xfce_panel_plugin_get_locked:
 * @plugin : an #XfcePanelPlugin.
 *
 * Whether the plugin is locked (not allowing customization). This
 * is emitted through the panel based on the Xfconf locking of the
 * panel window the plugin is embedded on.
 *
 * It is however possible to send a fake signal to the plugin to
 * override this propery, so you should only use this for interface
 * elements and (if you use Xfconf) check the locking yourself
 * before you write any values or query the kiosk mode using the
 * api in libxfce4util.
 *
 * Returns: %TRUE if the user is not allowed to modify the plugin,
 *          %FALSE is customization is allowed.
 *
 * Since: 4.8
 **/
gboolean
xfce_panel_plugin_get_locked (XfcePanelPlugin *plugin)
{
  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), TRUE);

  return plugin->priv->locked;
}



/**
 * xfce_panel_plugin_remove:
 * @plugin : an #XfcePanelPlugin.
 *
 * Remove this plugin from the panel and remove all its configuration.
 *
 * Plugins should not use this function to implement their own
 * menu item or button to remove theirselfs from the panel, but only
 * in case the there are problems with the plugin in the panel. Always
 * try to inform the user why this occured.
 *
 * Since: 4.8
 **/
void
xfce_panel_plugin_remove (XfcePanelPlugin *plugin)
{
  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));

  /* ask the panel or wrapper to remove the plugin */
  xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                          PROVIDER_SIGNAL_REMOVE_PLUGIN);
}



/**
 * xfce_panel_plugin_block_menu:
 * @plugin : an #XfcePanelPlugin.
 *
 * Block configuring the plugin. This will make the "Properties" menu
 * item insensitive.
 **/
void
xfce_panel_plugin_block_menu (XfcePanelPlugin *plugin)
{
  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));

  /* increase block counter */
  plugin->priv->menu_blocked++;
}



/**
 * xfce_panel_plugin_unblock_menu:
 * @plugin : an #XfcePanelPlugin.
 *
 * Unblock configuring the plugin. This will make the "Properties" menu
 * item sensitive.
 **/
void
xfce_panel_plugin_unblock_menu (XfcePanelPlugin *plugin)
{
  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (plugin->priv->menu_blocked > 0);

  /* decrease block counter */
  if (G_LIKELY (plugin->priv->menu_blocked > 0))
    plugin->priv->menu_blocked--;
}



/**
 * xfce_panel_plugin_register_menu:
 * @plugin : an #XfcePanelPlugin.
 * @menu   : a #GtkMenu that will be opened
 *
 * Register a menu that is about to popup. This will make sure the panel
 * will properly handle its autohide behaviour. You have to call this
 * function every time the menu is opened (e.g. using gtk_menu_popup_at_widget()).
 *
 * If you want to open the menu aligned to the side of the panel (and the
 * plugin), you should use xfce_panel_plugin_popup_menu(). This function
 * will take care of calling xfce_panel_plugin_register_menu() as well.
 *
 * See also: xfce_panel_plugin_popup_menu() and xfce_panel_plugin_block_autohide().
 **/
void
xfce_panel_plugin_register_menu (XfcePanelPlugin *plugin,
                                 GtkMenu         *menu)
{
  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  /* connect signal to menu to decrease counter */
  g_signal_connect (G_OBJECT (menu), "deactivate",
      G_CALLBACK (xfce_panel_plugin_unregister_menu), plugin);
  g_signal_connect (G_OBJECT (menu), "selection-done",
      G_CALLBACK (xfce_panel_plugin_unregister_menu), plugin);
  g_signal_connect (G_OBJECT (menu), "destroy",
      G_CALLBACK (xfce_panel_plugin_unregister_menu), plugin);
  g_signal_connect (G_OBJECT (menu), "hide",
      G_CALLBACK (xfce_panel_plugin_unregister_menu), plugin);

  /* tell panel it needs to lock */
  xfce_panel_plugin_block_autohide (plugin, TRUE);
}



/**
 * xfce_panel_plugin_arrow_type:
 * @plugin : an #XfcePanelPlugin.
 *
 * Determine the #GtkArrowType for a widget that opens a menu.
 *
 * Returns: the #GtkArrowType to use.
 **/
GtkArrowType
xfce_panel_plugin_arrow_type (XfcePanelPlugin *plugin)
{
  XfceScreenPosition  screen_position;
  GdkScreen          *screen;
  GdkDisplay         *display;
  GdkMonitor         *monitor;
  GdkRectangle        geometry;
  gint                x, y;
  GdkWindow          *window;

  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), GTK_ARROW_NONE);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), GTK_ARROW_NONE);

  /* get the plugin screen position */
  screen_position = xfce_panel_plugin_get_screen_position (plugin);

  /* detect the arrow type */
  if (xfce_screen_position_is_top (screen_position))
    return GTK_ARROW_DOWN;
  else if (xfce_screen_position_is_bottom (screen_position))
    return GTK_ARROW_UP;
  else if (xfce_screen_position_is_left (screen_position))
    return GTK_ARROW_RIGHT;
  else if (xfce_screen_position_is_right (screen_position))
    return GTK_ARROW_LEFT;
  else /* floating */
    {
      window = gtk_widget_get_window (GTK_WIDGET (plugin));
      if (G_UNLIKELY (window == NULL))
        return GTK_ARROW_NONE;

      /* get the monitor geometry */
      screen = gtk_widget_get_screen (GTK_WIDGET (plugin));
      display = gdk_screen_get_display (screen);
      monitor = gdk_display_get_monitor_at_window (display, window);
      gdk_monitor_get_geometry (monitor, &geometry);

      /* get the plugin root origin */
      gdk_window_get_root_origin (window, &x, &y);

      /* detect arrow type */
      if (screen_position == XFCE_SCREEN_POSITION_FLOATING_H)
        return (y < (geometry.y + geometry.height / 2)) ? GTK_ARROW_DOWN : GTK_ARROW_UP;
      else
        return (x < (geometry.x + geometry.width / 2)) ? GTK_ARROW_RIGHT : GTK_ARROW_LEFT;
    }
}



/**
 * xfce_panel_plugin_position_widget:
 * @plugin        : an #XfcePanelPlugin.
 * @menu_widget   : a #GtkWidget that will be used as popup menu.
 * @attach_widget : (allow-none): a #GtkWidget relative to which the menu should be positioned.
 * @x             : (out): return location for the x coordinate.
 * @y             : (out): return location for the y coordinate.
 *
 * Computes the x and y coordinates to position the @menu_widget
 * relative to @attach_widget. If @attach_widget is NULL, the computed
 * position will be relative to @plugin.
 *
 * Note that if the panel is hidden (autohide), you should delay calling this
 * function until the panel is shown, so that it returns the correct coordinates.
 *
 * This function is intended for custom menu widgets.
 * For a regular #GtkMenu you should use xfce_panel_plugin_popup_menu() instead.
 *
 * See also: xfce_panel_plugin_popup_menu().
 **/
void
xfce_panel_plugin_position_widget (XfcePanelPlugin *plugin,
                                   GtkWidget       *menu_widget,
                                   GtkWidget       *attach_widget,
                                   gint            *x,
                                   gint            *y)
{
  GtkRequisition  requisition;
  GdkScreen      *screen;
  GdkRectangle    geometry;
  GdkDisplay     *display;
  GdkMonitor     *monitor;
  GtkWidget      *toplevel, *plug;
  gint            px, py;
  GtkAllocation   alloc;

  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (GTK_IS_WIDGET (menu_widget));
  g_return_if_fail (attach_widget == NULL || GTK_IS_WIDGET (attach_widget));
  g_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  /* if the attach widget is null, use the panel plugin */
  if (attach_widget == NULL)
    attach_widget = GTK_WIDGET (plugin);

  /* make sure the menu is realized to get valid rectangle sizes */
  if (!gtk_widget_get_realized (menu_widget))
    gtk_widget_realize (menu_widget);

  /* make sure the attach widget is realized for the gdkwindow */
  if (!gtk_widget_get_realized (attach_widget))
    gtk_widget_realize (attach_widget);

  /* get the menu/widget size request */
  gtk_widget_get_preferred_size (menu_widget, &requisition, NULL);

  /* get the root position of the attach widget */
  toplevel = gtk_widget_get_toplevel (attach_widget);
  gtk_window_get_position (GTK_WINDOW (toplevel), x, y);

  /* correct position for external plugins */
  plug = gtk_widget_get_ancestor (attach_widget, GTK_TYPE_PLUG);
  if (plug != NULL)
    {
       gdk_window_get_geometry (gtk_plug_get_socket_window (GTK_PLUG (plug)),
                                &px, &py, NULL, NULL);
       *x += px;
       *y += py;
    }

  /* add the widgets allocation */
  gtk_widget_get_allocation (attach_widget, &alloc);
  *x += alloc.x;
  *y += alloc.y;

  switch (xfce_panel_plugin_arrow_type (plugin))
    {
    case GTK_ARROW_UP:
      *y -= requisition.height;
      break;

    case GTK_ARROW_DOWN:
      *y += alloc.height;
      break;

    case GTK_ARROW_LEFT:
      *x -= requisition.width;
      break;

    default: /* GTK_ARROW_RIGHT and GTK_ARROW_NONE */
      *x += alloc.width;
      break;
    }

  /* get the monitor geometry */
  screen = gtk_widget_get_screen (attach_widget);
  display = gdk_screen_get_display (screen);
  monitor = gdk_display_get_monitor_at_window (display, gtk_widget_get_window (attach_widget));
  gdk_monitor_get_geometry (monitor, &geometry);

  /* keep the menu inside the screen */
  if (*x > geometry.x + geometry.width - requisition.width)
    *x = geometry.x + geometry.width - requisition.width;
  if (*x < geometry.x)
    *x = geometry.x;
  if (*y > geometry.y + geometry.height - requisition.height)
    *y = geometry.y + geometry.height - requisition.height;
  if (*y < geometry.y)
    *y = geometry.y;

  /* popup on the correct screen */
  if (G_LIKELY (GTK_IS_MENU (menu_widget)))
    gtk_menu_set_screen (GTK_MENU (menu_widget), screen);
  else if (GTK_IS_WINDOW (menu_widget))
    gtk_window_set_screen (GTK_WINDOW (menu_widget), screen);
}



/**
 * xfce_panel_plugin_position_menu:
 * @menu         : a #GtkMenu.
 * @x            : (out): return location for the x coordinate.
 * @y            : (out): return location for the y coordinate.
 * @push_in      : keep inside the screen (see #GtkMenuPositionFunc)
 * @panel_plugin : an #XfcePanelPlugin.
 *
 * Function to be used as #GtkMenuPositionFunc in a call to gtk_menu_popup().
 * As data argument it needs an #XfcePanelPlugin.
 *
 * The menu is normally positioned relative to @panel_plugin. If you want the
 * menu to be positioned relative to another widget, you can use
 * gtk_menu_attach_to_widget() to explicitly set a 'parent' widget.
 *
 * As a convenience, xfce_panel_plugin_position_menu() calls
 * xfce_panel_plugin_register_menu() for the menu.
 *
 * <example>
 * void
 * myplugin_popup_menu (XfcePanelPlugin *plugin,
 *                      GtkMenu         *menu,
 *                      GdkEventButton  *ev)
 * {
 *     gtk_menu_popup (menu, NULL, NULL,
 *                     xfce_panel_plugin_position_menu, plugin,
 *                     ev->button, ev->time );
 * }
 * </example>
 *
 * For a custom widget that will be used as a popup menu, use
 * xfce_panel_plugin_position_widget() instead.
 *
 * See also: gtk_menu_popup().
 *
 * Deprecated: 4.17.2: Use xfce_panel_plugin_popup_menu() instead.
 **/
void
xfce_panel_plugin_position_menu (GtkMenu  *menu,
                                 gint     *x,
                                 gint     *y,
                                 gboolean *push_in,
                                 gpointer  panel_plugin)
{
  GtkWidget *attach_widget;

  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (panel_plugin));
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (panel_plugin));

  /* register the menu */
  xfce_panel_plugin_register_menu (XFCE_PANEL_PLUGIN (panel_plugin), menu);

  /* calculate the coordinates */
  attach_widget = gtk_menu_get_attach_widget (menu);
  xfce_panel_plugin_position_widget (XFCE_PANEL_PLUGIN (panel_plugin),
                                     GTK_WIDGET (menu), attach_widget, x, y);

  /* FIXME */
  /* A workaround for Gtk3 popup menus with scroll buttons */
  /* Menus are "pushed in" anyway */
  *push_in = FALSE;
}



static gboolean
xfce_panel_plugin_popup_menu_reposition (gpointer data)
{
  gtk_menu_reposition (data);
  g_object_set_data (data, "menu-reposition-id", GUINT_TO_POINTER (0));

  return FALSE;
}



/**
 * xfce_panel_plugin_popup_menu:
 * @plugin        : an #XfcePanelPlugin.
 * @menu          : a #GtkMenu.
 * @widget        : (allow-none): the #GtkWidget to align @menu with or %NULL
 *                  to pop up @menu at pointer.
 * @trigger_event : (allow-none): the #GdkEvent that initiated this request or
 *                  %NULL if it's the current event.
 *
 * Pops up @menu at @widget if @widget is non-%NULL and if appropriate given
 * the panel position, otherwise pops up @menu at pointer.
 *
 * As a convenience, xfce_panel_plugin_popup_menu() calls
 * xfce_panel_plugin_register_menu() for the @menu.
 *
 * For a custom widget that will be used as a popup menu, use
 * xfce_panel_plugin_position_widget() instead.
 *
 * See also: gtk_menu_popup_at_widget() and gtk_menu_popup_at_pointer().
 *
 * Since: 4.17.2
 **/
void
xfce_panel_plugin_popup_menu (XfcePanelPlugin *plugin,
                              GtkMenu         *menu,
                              GtkWidget       *widget,
                              const GdkEvent  *trigger_event)
{
  GdkGravity widget_anchor, menu_anchor;
  gboolean   popup_at_widget = TRUE;
  guint      id;

  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (GTK_IS_MENU (menu));

  /* check if conditions are met to pop up menu at widget */
  if (widget != NULL)
    {
      switch (xfce_panel_plugin_arrow_type (plugin))
        {
          case GTK_ARROW_DOWN:
            widget_anchor = GDK_GRAVITY_SOUTH_WEST;
            menu_anchor = GDK_GRAVITY_NORTH_WEST;
            break;

          case GTK_ARROW_RIGHT:
            widget_anchor = GDK_GRAVITY_NORTH_EAST;
            menu_anchor = GDK_GRAVITY_NORTH_WEST;
            break;

          case GTK_ARROW_LEFT:
            widget_anchor = GDK_GRAVITY_NORTH_WEST;
            menu_anchor = GDK_GRAVITY_NORTH_EAST;
            break;

          case GTK_ARROW_UP:
            widget_anchor = GDK_GRAVITY_NORTH_WEST;
            menu_anchor = GDK_GRAVITY_SOUTH_WEST;
            break;

          default:
            popup_at_widget = FALSE;
            break;
        }
    }
  else
    popup_at_widget = FALSE;

  /* register the menu */
  xfce_panel_plugin_register_menu (plugin, menu);

  /* since we requested a panel lock, queue a menu repositioning in case the panel is hidden */
  id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (menu), "menu-reposition-id"));
  if (id != 0)
    g_source_remove (id);

  id = g_idle_add (xfce_panel_plugin_popup_menu_reposition, menu);
  g_object_set_data (G_OBJECT (menu), "menu-reposition-id", GUINT_TO_POINTER (id));

  /* pop up the menu */
  if (popup_at_widget)
    gtk_menu_popup_at_widget (menu, widget, widget_anchor, menu_anchor, trigger_event);
  else
    gtk_menu_popup_at_pointer (menu, trigger_event);
}



/**
 * xfce_panel_plugin_focus_widget:
 * @plugin : an #XfcePanelPlugin.
 * @widget : a #GtkWidget inside the plugins that should be focussed.
 *
 * Grab the focus on @widget. Asks the panel to allow focus on its items
 * and set the focus to the requested widget.
 **/
void
xfce_panel_plugin_focus_widget (XfcePanelPlugin *plugin,
                                GtkWidget       *widget)
{
  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  /* focus the panel window */
  xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                          PROVIDER_SIGNAL_FOCUS_PLUGIN);

  /* let the widget grab focus */
  gtk_widget_grab_focus (widget);
}



/**
 * xfce_panel_plugin_block_autohide:
 * @plugin  : an #XfcePanelPlugin.
 * @blocked : new blocking state of this plugin.
 *
 * Whether this plugin blocks the autohide functionality of the panel. Use
 * this when you 'popup' something that is visually attached to the
 * plugin at it will look weird for a user if the panel will hide while
 * he/she is working in the popup.
 *
 * Be sure to use this function as lock/unlock pairs, as a counter is
 * incremented/decremented under the hood. For menus, you can use
 * xfce_panel_plugin_register_menu() which will take care of this.
 **/
void
xfce_panel_plugin_block_autohide (XfcePanelPlugin *plugin,
                                  gboolean         blocked)
{
  g_return_if_fail (XFCE_IS_PANEL_PLUGIN (plugin));
  g_return_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin));

  if (blocked)
    {
      /* increase the counter */
      plugin->priv->panel_lock++;

      /* tell panel it needs to lock */
      if (plugin->priv->panel_lock == 1)
        xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                                PROVIDER_SIGNAL_LOCK_PANEL);
    }
  else
    {
      /* decrease the counter */
      panel_return_if_fail (plugin->priv->panel_lock > 0);
      plugin->priv->panel_lock--;

      /* tell panel it needs to unlock */
      if (plugin->priv->panel_lock == 0)
        xfce_panel_plugin_provider_emit_signal (XFCE_PANEL_PLUGIN_PROVIDER (plugin),
                                                PROVIDER_SIGNAL_UNLOCK_PANEL);
    }
}



/**
 * xfce_panel_plugin_lookup_rc_file:
 * @plugin : an #XfcePanelPlugin.
 *
 * Looks for the plugin resource file. This should be used to get the
 * plugin read location of the config file. You should only use the
 * returned path to read information from, since it might point to a
 * not-writable file (in kiosk mode for example).
 *
 * See also: xfce_panel_plugin_save_location() and xfce_resource_lookup()
 *
 * Returns: (transfer full): The path to a config file or %NULL if no file was found.
 *          The returned string must be freed using g_free()
 **/
gchar *
xfce_panel_plugin_lookup_rc_file (XfcePanelPlugin *plugin)
{
  gchar *filename, *path;

  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), NULL);
  g_return_val_if_fail (XFCE_PANEL_PLUGIN_CONSTRUCTED (plugin), NULL);

  filename = xfce_panel_plugin_relative_filename (plugin);
  path = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, filename);
  g_free (filename);

  return path;
}



/**
 * xfce_panel_plugin_save_location:
 * @plugin : an #XfcePanelPlugin.
 * @create : whether to create missing directories.
 *
 * Returns the path that can be used to store configuration information.
 * Don't use this function if you want to read from the config file, but
 * use xfce_panel_plugin_lookup_rc_file() instead.
 *
 * See also: xfce_panel_plugin_lookup_rc_file() and xfce_resource_save_location()
 *
 * Returns: (transfer full): The path to a config file or %NULL if no file was found.
 *          The returned string must be freed u sing g_free().
 **/
gchar *
xfce_panel_plugin_save_location (XfcePanelPlugin *plugin,
                                 gboolean         create)
{
  gchar *filename, *path;

  g_return_val_if_fail (XFCE_IS_PANEL_PLUGIN (plugin), NULL);

  filename = xfce_panel_plugin_relative_filename (plugin);
  path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, filename, create);
  g_free (filename);

  return path;
}



#define __XFCE_PANEL_PLUGIN_C__
#include <libxfce4panel/libxfce4panel-aliasdef.c>
