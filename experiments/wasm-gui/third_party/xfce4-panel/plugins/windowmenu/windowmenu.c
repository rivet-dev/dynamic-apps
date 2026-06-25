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

#include <exo/exo.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libwnck/libwnck.h>
#include <common/panel-xfconf.h>
#include <common/panel-utils.h>
#include <gdk/gdkkeysyms.h>
#include <common/panel-private.h>

#include "windowmenu.h"
#include "windowmenu-dialog_ui.h"

#define ARROW_BUTTON_SIZE       (12)
#define DEFAULT_ICON_LUCENCY    (50)
#define DEFAULT_MAX_WIDTH_CHARS (24)
#define DEFAULT_ELLIPSIZE_MODE  (PANGO_ELLIPSIZE_MIDDLE)
#define URGENT_FLAGS            (WNCK_WINDOW_STATE_DEMANDS_ATTENTION | \
                                 WNCK_WINDOW_STATE_URGENT)

struct _WindowMenuPluginClass
{
  XfcePanelPluginClass __parent__;
};

struct _WindowMenuPlugin
{
  XfcePanelPlugin __parent__;

  /* the screen we're showing */
  WnckScreen         *screen;

  /* panel widgets */
  GtkWidget          *button;
  GtkWidget          *icon;

  /* settings */
  guint               button_style : 1;
  guint               workspace_actions : 1;
  guint               workspace_names : 1;
  guint               urgentcy_notification : 1;
  guint               all_workspaces : 1;

  /* urgent window counter */
  gint                urgent_windows;

  /* gtk style properties */
  gint                minimized_icon_lucency;
  PangoEllipsizeMode  ellipsize_mode;
  gint                max_width_chars;
};

enum
{
  PROP_0,
  PROP_STYLE,
  PROP_WORKSPACE_ACTIONS,
  PROP_WORKSPACE_NAMES,
  PROP_URGENTCY_NOTIFICATION,
  PROP_ALL_WORKSPACES
};

enum
{
  BUTTON_STYLE_ICON = 0,
  BUTTON_STYLE_ARROW
};



static void      window_menu_plugin_get_property            (GObject            *object,
                                                             guint               prop_id,
                                                             GValue             *value,
                                                             GParamSpec         *pspec);
static void      window_menu_plugin_set_property            (GObject            *object,
                                                             guint               prop_id,
                                                             const GValue       *value,
                                                             GParamSpec         *pspec);
static void      window_menu_plugin_style_updated           (GtkWidget          *widget);
static void      window_menu_plugin_screen_changed          (GtkWidget          *widget,
                                                             GdkScreen          *previous_screen);
static void      window_menu_plugin_construct               (XfcePanelPlugin    *panel_plugin);
static void      window_menu_plugin_free_data               (XfcePanelPlugin    *panel_plugin);
static void      window_menu_plugin_screen_position_changed (XfcePanelPlugin    *panel_plugin,
                                                             XfceScreenPosition  screen_position);
static gboolean  window_menu_plugin_size_changed            (XfcePanelPlugin    *panel_plugin,
                                                             gint                size);
static void      window_menu_plugin_configure_plugin        (XfcePanelPlugin    *panel_plugin);
static gboolean  window_menu_plugin_remote_event            (XfcePanelPlugin    *panel_plugin,
                                                             const gchar        *name,
                                                             const GValue       *value);
static void      window_menu_plugin_active_window_changed   (WnckScreen         *screen,
                                                             WnckWindow         *previous_window,
                                                             WindowMenuPlugin   *plugin);
static void      window_menu_plugin_window_state_changed    (WnckWindow         *window,
                                                             WnckWindowState     changed_mask,
                                                             WnckWindowState     new_state,
                                                             WindowMenuPlugin   *plugin);
static void      window_menu_plugin_window_opened           (WnckScreen         *screen,
                                                             WnckWindow         *window,
                                                             WindowMenuPlugin   *plugin);
static void      window_menu_plugin_window_closed           (WnckScreen         *screen,
                                                             WnckWindow         *window,
                                                             WindowMenuPlugin   *plugin);
static void      window_menu_plugin_windows_disconnect      (WindowMenuPlugin   *plugin);
static void      window_menu_plugin_windows_connect         (WindowMenuPlugin   *plugin,
                                                             gboolean            traverse_windows);
static void      window_menu_plugin_menu                    (GtkWidget          *button,
                                                             WindowMenuPlugin   *plugin);



/* define the plugin */
XFCE_PANEL_DEFINE_PLUGIN_RESIDENT (WindowMenuPlugin, window_menu_plugin)



static GQuark window_quark = 0;



static void
window_menu_plugin_class_init (WindowMenuPluginClass *klass)
{
  XfcePanelPluginClass *plugin_class;
  GObjectClass         *gobject_class;
  GtkWidgetClass       *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = window_menu_plugin_get_property;
  gobject_class->set_property = window_menu_plugin_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->style_updated = window_menu_plugin_style_updated;

  plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
  plugin_class->construct = window_menu_plugin_construct;
  plugin_class->free_data = window_menu_plugin_free_data;
  plugin_class->screen_position_changed = window_menu_plugin_screen_position_changed;
  plugin_class->size_changed = window_menu_plugin_size_changed;
  plugin_class->configure_plugin = window_menu_plugin_configure_plugin;
  plugin_class->remote_event = window_menu_plugin_remote_event;

  g_object_class_install_property (gobject_class,
                                   PROP_STYLE,
                                   g_param_spec_uint ("style",
                                                      NULL, NULL,
                                                      BUTTON_STYLE_ICON,
                                                      BUTTON_STYLE_ARROW,
                                                      BUTTON_STYLE_ICON,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_WORKSPACE_ACTIONS,
                                   g_param_spec_boolean ("workspace-actions",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_WORKSPACE_NAMES,
                                   g_param_spec_boolean ("workspace-names",
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_URGENTCY_NOTIFICATION,
                                   g_param_spec_boolean ("urgentcy-notification",
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_ALL_WORKSPACES,
                                   g_param_spec_boolean ("all-workspaces",
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("minimized-icon-lucency",
                                                             NULL,
                                                             "Lucent percentage of minimized icons",
                                                             0, 100,
                                                             DEFAULT_ICON_LUCENCY,
                                                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_enum ("ellipsize-mode",
                                                              NULL,
                                                              "The ellipsize mode used for the menu label",
                                                              PANGO_TYPE_ELLIPSIZE_MODE,
                                                              DEFAULT_ELLIPSIZE_MODE,
                                                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("max-width-chars",
                                                             NULL,
                                                             "Maximum length of window/workspace name",
                                                             1, G_MAXINT,
                                                             DEFAULT_MAX_WIDTH_CHARS,
                                                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  window_quark = g_quark_from_static_string ("window-list-window-quark");
}



static void
window_menu_plugin_scale_factor (WindowMenuPlugin *plugin)
{
  gint scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (plugin));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  wnck_set_default_icon_size (WNCK_DEFAULT_ICON_SIZE * scale_factor);
  wnck_set_default_mini_icon_size (WNCK_DEFAULT_MINI_ICON_SIZE * scale_factor);
G_GNUC_END_IGNORE_DEPRECATIONS
}



static void
window_menu_plugin_init (WindowMenuPlugin *plugin)
{
  plugin->button_style = BUTTON_STYLE_ICON;
  plugin->workspace_actions = FALSE;
  plugin->workspace_names = TRUE;
  plugin->urgentcy_notification = TRUE;
  plugin->all_workspaces = TRUE;
  plugin->urgent_windows = 0;
  plugin->minimized_icon_lucency = DEFAULT_ICON_LUCENCY;
  plugin->ellipsize_mode = DEFAULT_ELLIPSIZE_MODE;
  plugin->max_width_chars = DEFAULT_MAX_WIDTH_CHARS;

  /* create the widgets */
  plugin->button = xfce_arrow_button_new (GTK_ARROW_NONE);
  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), plugin->button);
  gtk_container_add (GTK_CONTAINER (plugin), plugin->button);
  gtk_button_set_relief (GTK_BUTTON (plugin->button), GTK_RELIEF_NONE);
  gtk_widget_set_name (plugin->button, "windowmenu-button");
  g_signal_connect (G_OBJECT (plugin->button), "toggled",
      G_CALLBACK (window_menu_plugin_menu), plugin);

  plugin->icon = gtk_image_new_from_icon_name ("user-desktop", GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (plugin->button), plugin->icon);
  gtk_widget_show (plugin->icon);

  window_menu_plugin_scale_factor (plugin);
  g_signal_connect (plugin, "notify::scale-factor", G_CALLBACK (window_menu_plugin_scale_factor), NULL);
}



static void
window_menu_plugin_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  WindowMenuPlugin *plugin = XFCE_WINDOW_MENU_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_STYLE:
      g_value_set_uint (value, plugin->button_style);
      break;

    case PROP_WORKSPACE_ACTIONS:
      g_value_set_boolean (value, plugin->workspace_actions);
      break;

    case PROP_WORKSPACE_NAMES:
      g_value_set_boolean (value, plugin->workspace_names);
      break;

    case PROP_URGENTCY_NOTIFICATION:
      g_value_set_boolean (value, plugin->urgentcy_notification);
      break;

    case PROP_ALL_WORKSPACES:
      g_value_set_boolean (value, plugin->all_workspaces);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
window_menu_plugin_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  WindowMenuPlugin *plugin = XFCE_WINDOW_MENU_PLUGIN (object);
  XfcePanelPlugin  *panel_plugin = XFCE_PANEL_PLUGIN (object);
  guint             button_style;
  gboolean          urgentcy_notification;

  panel_return_if_fail (XFCE_IS_WINDOW_MENU_PLUGIN (plugin));

  switch (prop_id)
    {
    case PROP_STYLE:
      button_style = g_value_get_uint (value);
      if (plugin->button_style != button_style)
        {
          plugin->button_style = button_style;

          /* show or hide the icon */
          if (button_style == BUTTON_STYLE_ICON)
            gtk_widget_show (plugin->icon);
          else
            gtk_widget_hide (plugin->icon);

          /* update the plugin */
          xfce_panel_plugin_set_small (panel_plugin, plugin->button_style == BUTTON_STYLE_ICON);
          window_menu_plugin_size_changed (panel_plugin,
              xfce_panel_plugin_get_size (panel_plugin));
          window_menu_plugin_screen_position_changed (panel_plugin,
              xfce_panel_plugin_get_screen_position (panel_plugin));
          if (plugin->screen != NULL)
            window_menu_plugin_active_window_changed (plugin->screen, NULL, plugin);
        }
      break;

    case PROP_WORKSPACE_ACTIONS:
      plugin->workspace_actions = g_value_get_boolean (value);
      break;

    case PROP_WORKSPACE_NAMES:
      plugin->workspace_names = g_value_get_boolean (value);
      break;

    case PROP_URGENTCY_NOTIFICATION:
      urgentcy_notification = g_value_get_boolean (value);
      if (plugin->urgentcy_notification != urgentcy_notification)
        {
          plugin->urgentcy_notification = urgentcy_notification;

          if (plugin->screen != NULL)
            {
              /* (dis)connect window signals */
              if (plugin->urgentcy_notification)
                window_menu_plugin_windows_connect (plugin, TRUE);
              else
                window_menu_plugin_windows_disconnect (plugin);
            }
        }
      break;

    case PROP_ALL_WORKSPACES:
      plugin->all_workspaces = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
window_menu_plugin_style_updated (GtkWidget *widget)
{
  WindowMenuPlugin *plugin = XFCE_WINDOW_MENU_PLUGIN (widget);

  /* let gtk update the widget style */
  (*GTK_WIDGET_CLASS (window_menu_plugin_parent_class)->style_updated) (widget);

  /* read the style properties */
  gtk_widget_style_get (GTK_WIDGET (plugin),
                        "minimized-icon-lucency", &plugin->minimized_icon_lucency,
                        "ellipsize-mode", &plugin->ellipsize_mode,
                        "max-width-chars", &plugin->max_width_chars,
                        NULL);
}



static void
window_menu_plugin_screen_changed (GtkWidget *widget,
                                   GdkScreen *previous_screen)
{
  WindowMenuPlugin *plugin = XFCE_WINDOW_MENU_PLUGIN (widget);
  GdkScreen        *screen;
  WnckScreen       *wnck_screen;

  /* get the wnck screen */
  screen = gtk_widget_get_screen (widget);
  panel_return_if_fail (GDK_IS_SCREEN (screen));
  wnck_screen = panel_wnck_screen_get (panel_screen_get_number (screen));
  panel_return_if_fail (WNCK_IS_SCREEN (wnck_screen));

  /* leave when we same wnck screen was picked */
  if (plugin->screen == wnck_screen)
    return;

  if (G_UNLIKELY (plugin->screen != NULL))
    {
      /* disconnect from all windows on the old screen */
      window_menu_plugin_windows_disconnect (plugin);

      /* disconnect from the previous screen */
      g_signal_handlers_disconnect_by_func (G_OBJECT (plugin->screen),
          window_menu_plugin_active_window_changed, plugin);
    }

  /* set the new screen */
  plugin->screen = wnck_screen;

  /* connect signal to monitor this screen */
  g_signal_connect (G_OBJECT (plugin->screen), "active-window-changed",
      G_CALLBACK (window_menu_plugin_active_window_changed), plugin);

  if (plugin->urgentcy_notification)
    window_menu_plugin_windows_connect (plugin, TRUE);
}



static void
window_menu_plugin_construct (XfcePanelPlugin *panel_plugin)
{
  WindowMenuPlugin    *plugin = XFCE_WINDOW_MENU_PLUGIN (panel_plugin);
  const PanelProperty  properties[] =
  {
    { "style", G_TYPE_UINT },
    { "workspace-actions", G_TYPE_BOOLEAN },
    { "workspace-names", G_TYPE_BOOLEAN },
    { "urgentcy-notification", G_TYPE_BOOLEAN },
    { "all-workspaces", G_TYPE_BOOLEAN },
    { NULL }
  };

  /* show configure */
  xfce_panel_plugin_menu_show_configure (XFCE_PANEL_PLUGIN (plugin));
  xfce_panel_plugin_set_small (panel_plugin, TRUE);

  /* bind all properties */
  panel_properties_bind (NULL, G_OBJECT (plugin),
                         xfce_panel_plugin_get_property_base (panel_plugin),
                         properties, FALSE);

  /* monitor screen changes */
  g_signal_connect (G_OBJECT (plugin), "screen-changed",
      G_CALLBACK (window_menu_plugin_screen_changed), NULL);

  /* initialize the screen */
  window_menu_plugin_screen_changed (GTK_WIDGET (plugin), NULL);

  gtk_widget_show (plugin->button);
}



static void
window_menu_plugin_free_data (XfcePanelPlugin *panel_plugin)
{
  WindowMenuPlugin *plugin = XFCE_WINDOW_MENU_PLUGIN (panel_plugin);

  /* disconnect screen changed signal */
  g_signal_handlers_disconnect_by_func (G_OBJECT (plugin),
          window_menu_plugin_screen_changed, NULL);

  /* disconnect from the screen */
  if (G_LIKELY (plugin->screen != NULL))
    {
      /* disconnect from all windows */
      window_menu_plugin_windows_disconnect (plugin);

      /* disconnect from the screen */
      g_signal_handlers_disconnect_by_func (G_OBJECT (plugin->screen),
          window_menu_plugin_active_window_changed, plugin);

      plugin->screen = NULL;
    }
}



static void
window_menu_plugin_screen_position_changed (XfcePanelPlugin    *panel_plugin,
                                            XfceScreenPosition  screen_position)
{
  WindowMenuPlugin *plugin = XFCE_WINDOW_MENU_PLUGIN (panel_plugin);
  GtkArrowType      arrow_type = GTK_ARROW_NONE;

  /* set the arrow direction if the arrow is visible */
  if (plugin->button_style == BUTTON_STYLE_ARROW)
    arrow_type = xfce_panel_plugin_arrow_type (panel_plugin);

  xfce_arrow_button_set_arrow_type (XFCE_ARROW_BUTTON (plugin->button),
                                    arrow_type);
}



static gboolean
window_menu_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                                 gint             size)
{
  WindowMenuPlugin *plugin = XFCE_WINDOW_MENU_PLUGIN (panel_plugin);
  gint              button_size;

  if (plugin->button_style == BUTTON_STYLE_ICON)
    {
      /* square the plugin */
      size /= xfce_panel_plugin_get_nrows (panel_plugin);
      gtk_widget_set_size_request (GTK_WIDGET (plugin), size, size);
    }
  else
    {
      /* set the size of the arrow button */
      if (xfce_panel_plugin_get_orientation (panel_plugin) ==
              GTK_ORIENTATION_HORIZONTAL)
        {
          gtk_widget_get_preferred_width (plugin->button, NULL, &button_size);
          gtk_widget_set_size_request (GTK_WIDGET (plugin), button_size, -1);
        }
      else
        {
          gtk_widget_get_preferred_height (plugin->button, NULL, &button_size);
          gtk_widget_set_size_request (GTK_WIDGET (plugin), -1, button_size);
        }
    }
  /* Update the plugin's pixbuf size too */
  if (plugin->screen != NULL)
    window_menu_plugin_active_window_changed (plugin->screen, NULL, plugin);

  return TRUE;
}



static void
window_menu_plugin_configure_plugin (XfcePanelPlugin *panel_plugin)
{
  WindowMenuPlugin *plugin = XFCE_WINDOW_MENU_PLUGIN (panel_plugin);
  GtkBuilder       *builder;
  GObject          *dialog, *object;
  guint             i;
  const gchar      *names[] = { "workspace-actions", "workspace-names",
                                "urgentcy-notification", "all-workspaces",
                                "style" };

  /* setup the dialog */
  PANEL_UTILS_LINK_4UI
  builder = panel_utils_builder_new (panel_plugin, windowmenu_dialog_ui,
                                     windowmenu_dialog_ui_length, &dialog);
  if (G_UNLIKELY (builder == NULL))
    return;

  /* connect bindings */
  for (i = 0; i < G_N_ELEMENTS (names); i++)
    {
      object = gtk_builder_get_object (builder, names[i]);
      panel_return_if_fail (GTK_IS_WIDGET (object));
      g_object_bind_property (G_OBJECT (plugin), names[i],
                              G_OBJECT (object), "active",
                              G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    }

  gtk_widget_show (GTK_WIDGET (dialog));
}



static gboolean
window_menu_plugin_remote_event (XfcePanelPlugin *panel_plugin,
                                 const gchar     *name,
                                 const GValue    *value)
{
  WindowMenuPlugin *plugin = XFCE_WINDOW_MENU_PLUGIN (panel_plugin);
  GtkWidget *invisible;

  panel_return_val_if_fail (value == NULL || G_IS_VALUE (value), FALSE);

  /* try next plugin or indicate that it failed */
  if (strcmp (name, "popup") != 0
      || ! gtk_widget_get_visible (GTK_WIDGET (panel_plugin)))
    return FALSE;

  invisible = gtk_invisible_new ();
  gtk_widget_show (invisible);

  /* a menu is already shown, don't popup another one */
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (plugin->button))
      || ! panel_utils_device_grab (invisible))
    {
      gtk_widget_destroy (invisible);
      return TRUE;
    }

  /*
   * The menu will take over the grab when it is shown or it will be lost when destroying
   * invisible below. This way we are sure that other invocations of the command by
   * keyboard shortcut will not interfere.
   */
  if (value != NULL
      && G_VALUE_HOLDS_BOOLEAN (value)
      && g_value_get_boolean (value))
    {
      /* popup menu at pointer */
      window_menu_plugin_menu (NULL, plugin);
    }
  else
    {
      /* popup menu at button */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->button), TRUE);
    }

  gtk_widget_destroy (invisible);

  /* don't popup another menu */
  return TRUE;
}



static void
window_menu_plugin_set_icon (WindowMenuPlugin *plugin,
                             WnckWindow       *window)
{
  GdkPixbuf *pixbuf;
  gint       icon_size, scale_factor;

  panel_return_if_fail (XFCE_IS_WINDOW_MENU_PLUGIN (plugin));
  panel_return_if_fail (WNCK_IS_WINDOW (window));

  if (! wnck_window_is_active (window))
    return;

  gtk_widget_set_tooltip_text (plugin->icon, wnck_window_get_name (window));

  icon_size = xfce_panel_plugin_get_icon_size (XFCE_PANEL_PLUGIN (plugin));
  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (plugin));
  if (icon_size < WNCK_DEFAULT_ICON_SIZE)
    pixbuf = wnck_window_get_mini_icon (window);
  else
    pixbuf = wnck_window_get_icon (window);

  if (G_LIKELY (pixbuf != NULL))
    {
      cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale_factor, NULL);
      gtk_image_set_from_surface (GTK_IMAGE (plugin->icon), surface);
      cairo_surface_destroy (surface);
    }
  else
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (plugin->icon), "image-missing", icon_size);
      gtk_image_set_pixel_size (GTK_IMAGE (plugin->icon), icon_size);
    }
}



static void
window_menu_plugin_active_window_changed (WnckScreen       *screen,
                                          WnckWindow       *previous_window,
                                          WindowMenuPlugin *plugin)
{
  WnckWindow     *window;
  gint            icon_size;
  GtkWidget      *icon = GTK_WIDGET (plugin->icon);
  WnckWindowType  type;

  panel_return_if_fail (XFCE_IS_WINDOW_MENU_PLUGIN (plugin));
  panel_return_if_fail (GTK_IMAGE (icon));
  panel_return_if_fail (WNCK_IS_SCREEN (screen));
  panel_return_if_fail (plugin->screen == screen);

  /* only do this when the icon is visible */
  if (plugin->button_style == BUTTON_STYLE_ICON)
    {
      window = wnck_screen_get_active_window (screen);
      if (G_LIKELY (window != NULL))
        {
          /* skip 'fake' windows */
          type = wnck_window_get_window_type (window);
          if (type == WNCK_WINDOW_DESKTOP || type == WNCK_WINDOW_DOCK)
            goto show_desktop_icon;

          window_menu_plugin_set_icon (plugin, window);
        }
      else
        {
          show_desktop_icon:

          /* desktop is shown right now */
          icon_size = xfce_panel_plugin_get_icon_size (XFCE_PANEL_PLUGIN (plugin));
          gtk_image_set_from_icon_name (GTK_IMAGE (icon), "user-desktop", icon_size);
          gtk_image_set_pixel_size (GTK_IMAGE (icon), icon_size);
          gtk_widget_set_tooltip_text (GTK_WIDGET (icon), _("Desktop"));
        }
    }
}



static void
window_menu_plugin_window_state_changed (WnckWindow       *window,
                                         WnckWindowState   changed_mask,
                                         WnckWindowState   new_state,
                                         WindowMenuPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_WINDOW_MENU_PLUGIN (plugin));
  panel_return_if_fail (WNCK_IS_WINDOW (window));
  panel_return_if_fail (plugin->urgentcy_notification);
  panel_return_if_fail (plugin->urgentcy_notification);

  /* only response to urgency changes and urgency notify is enabled */
  if (!PANEL_HAS_FLAG (changed_mask, URGENT_FLAGS))
    return;

  /* update the blinking state */
  if (PANEL_HAS_FLAG (new_state, URGENT_FLAGS))
    plugin->urgent_windows++;
  else
    plugin->urgent_windows--;

  /* check if we need to change the button */
  if (plugin->urgent_windows == 1)
    xfce_arrow_button_set_blinking (XFCE_ARROW_BUTTON (plugin->button), TRUE);
  else if (plugin->urgent_windows == 0)
    xfce_arrow_button_set_blinking (XFCE_ARROW_BUTTON (plugin->button), FALSE);
}



static void
window_menu_plugin_window_opened (WnckScreen       *screen,
                                  WnckWindow       *window,
                                  WindowMenuPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_WINDOW_MENU_PLUGIN (plugin));
  panel_return_if_fail (WNCK_IS_WINDOW (window));
  panel_return_if_fail (WNCK_IS_SCREEN (screen));
  panel_return_if_fail (plugin->screen == screen);
  panel_return_if_fail (plugin->urgentcy_notification);

  /* monitor some window properties */
  g_signal_connect (G_OBJECT (window), "state-changed",
      G_CALLBACK (window_menu_plugin_window_state_changed), plugin);
  g_signal_connect_swapped (G_OBJECT (window), "icon-changed",
      G_CALLBACK (window_menu_plugin_set_icon), plugin);

  /* check if the window needs attention */
  if (wnck_window_needs_attention (window))
    window_menu_plugin_window_state_changed (window, URGENT_FLAGS,
                                             URGENT_FLAGS, plugin);
}



static void
window_menu_plugin_window_closed (WnckScreen       *screen,
                                  WnckWindow       *window,
                                  WindowMenuPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_WINDOW_MENU_PLUGIN (plugin));
  panel_return_if_fail (WNCK_IS_WINDOW (window));
  panel_return_if_fail (WNCK_IS_SCREEN (screen));
  panel_return_if_fail (plugin->screen == screen);
  panel_return_if_fail (plugin->urgentcy_notification);

  /* check if we need to update the urgency counter */
  if (wnck_window_needs_attention (window))
    window_menu_plugin_window_state_changed (window, URGENT_FLAGS,
                                             0, plugin);
}



static void
window_menu_plugin_windows_disconnect (WindowMenuPlugin *plugin)
{
  GList *windows, *li;

  panel_return_if_fail (XFCE_IS_WINDOW_MENU_PLUGIN (plugin));
  panel_return_if_fail (WNCK_IS_SCREEN (plugin->screen));

  /* disconnect screen signals */
  g_signal_handlers_disconnect_by_func (G_OBJECT (plugin->screen),
     window_menu_plugin_window_closed, plugin);
  g_signal_handlers_disconnect_by_func (G_OBJECT (plugin->screen),
     window_menu_plugin_window_opened, plugin);

  /* disconnect from all window signals */
  windows = wnck_screen_get_windows (plugin->screen);
  for (li = windows; li != NULL; li = li->next)
    {
      panel_return_if_fail (WNCK_IS_WINDOW (li->data));
      g_signal_handlers_disconnect_by_func (G_OBJECT (li->data),
          window_menu_plugin_window_state_changed, plugin);
      g_signal_handlers_disconnect_by_func (G_OBJECT (li->data),
          window_menu_plugin_set_icon, plugin);
    }

  /* stop blinking */
  plugin->urgent_windows = 0;
  xfce_arrow_button_set_blinking (XFCE_ARROW_BUTTON (plugin->button), FALSE);
}



static void
window_menu_plugin_windows_connect (WindowMenuPlugin *plugin,
                                    gboolean          traverse_windows)
{
  GList *windows, *li;

  panel_return_if_fail (XFCE_IS_WINDOW_MENU_PLUGIN (plugin));
  panel_return_if_fail (WNCK_IS_SCREEN (plugin->screen));
  panel_return_if_fail (plugin->urgentcy_notification);

  g_signal_connect (G_OBJECT (plugin->screen), "window-opened",
      G_CALLBACK (window_menu_plugin_window_opened), plugin);
  g_signal_connect (G_OBJECT (plugin->screen), "window-closed",
      G_CALLBACK (window_menu_plugin_window_closed), plugin);

  if (!traverse_windows)
    return;

  /* connect the state changed signal to all windows */
  windows = wnck_screen_get_windows (plugin->screen);
  for (li = windows; li != NULL; li = li->next)
    {
      panel_return_if_fail (WNCK_IS_WINDOW (li->data));
      window_menu_plugin_window_opened (plugin->screen,
                                        WNCK_WINDOW (li->data),
                                        plugin);
    }
}



static void
window_menu_plugin_workspace_add (GtkWidget        *mi,
                                  WindowMenuPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_WINDOW_MENU_PLUGIN (plugin));
  panel_return_if_fail (WNCK_IS_SCREEN (plugin->screen));

  /* increase the number of workspaces */
  wnck_screen_change_workspace_count (plugin->screen,
      wnck_screen_get_workspace_count (plugin->screen) + 1);
}



static void
window_menu_plugin_workspace_remove (GtkWidget        *mi,
                                     WindowMenuPlugin *plugin)
{
  gint n_workspaces;

  panel_return_if_fail (XFCE_IS_WINDOW_MENU_PLUGIN (plugin));
  panel_return_if_fail (WNCK_IS_SCREEN (plugin->screen));

  /* decrease the number of workspaces */
  n_workspaces = wnck_screen_get_workspace_count (plugin->screen);
  if (G_LIKELY (n_workspaces > 1))
    wnck_screen_change_workspace_count (plugin->screen, n_workspaces - 1);
}



static void
window_menu_plugin_menu_workspace_item_active (GtkWidget     *mi,
                                               WnckWorkspace *workspace)
{
  panel_return_if_fail (WNCK_IS_WORKSPACE (workspace));

  /* activate the workspace */
  wnck_workspace_activate (workspace, gtk_get_current_event_time ());
}



static GtkWidget *
window_menu_plugin_menu_workspace_item_new (WnckWorkspace        *workspace,
                                            WindowMenuPlugin     *plugin,
                                            gboolean              bold)
{
  const gchar *name;
  gchar       *label_text = NULL;
  gchar       *utf8 = NULL, *name_num = NULL;
  GtkWidget   *mi, *label;

  panel_return_val_if_fail (WNCK_IS_WORKSPACE (workspace), NULL);
  panel_return_val_if_fail (XFCE_IS_WINDOW_MENU_PLUGIN (plugin), NULL);

  /* try to get an utf-8 valid name */
  name = wnck_workspace_get_name (workspace);
  if (!panel_str_is_empty (name)
      && !g_utf8_validate (name, -1, NULL))
    name = utf8 = g_locale_to_utf8 (name, -1, NULL, NULL, NULL);

  if (panel_str_is_empty (name))
    name = name_num = g_strdup_printf (_("Workspace %d"),
        wnck_workspace_get_number (workspace) + 1);

  mi = gtk_menu_item_new_with_label (name);
  g_signal_connect (G_OBJECT (mi), "activate",
      G_CALLBACK (window_menu_plugin_menu_workspace_item_active), workspace);

  /* make the label pretty on long workspace names */
  label = gtk_bin_get_child (GTK_BIN (mi));
  panel_return_val_if_fail (GTK_IS_LABEL (label), NULL);
  gtk_label_set_ellipsize (GTK_LABEL (label), plugin->ellipsize_mode);
  gtk_label_set_max_width_chars (GTK_LABEL (label), plugin->max_width_chars);
  gtk_label_set_xalign (GTK_LABEL (label), 0.5);

  /* modify the label font if needed */
  if (bold)
    label_text = g_strdup_printf ("<b>%s</b>", name);
  else
    label_text = g_strdup_printf ("<i>%s</i>", name);
  if (label_text)
  {
    gtk_label_set_markup (GTK_LABEL (label), label_text);
    g_free (label_text);
  }

  g_free (utf8);
  g_free (name_num);

  return mi;
}



static void
window_menu_plugin_menu_actions_selection_done (GtkWidget    *action_menu,
                                                GtkMenuShell *menu)
{
  panel_return_if_fail (GTK_IS_MENU_SHELL (menu));
  panel_return_if_fail (WNCK_IS_ACTION_MENU (action_menu));

  gtk_widget_destroy (action_menu);

  /* deactive the window list menu */
  gtk_menu_shell_cancel (menu);
}



static gboolean
window_menu_plugin_menu_window_item_activate (GtkWidget        *mi,
                                              GdkEventButton   *event,
                                              WindowMenuPlugin *plugin)
{
  WnckWindow    *window;
  WnckWorkspace *workspace;
  GtkWidget     *menu;

  panel_return_val_if_fail (GTK_IS_MENU_ITEM (mi), FALSE);
  panel_return_val_if_fail (GTK_IS_MENU_SHELL (gtk_widget_get_parent (mi)), FALSE);

  /* only respond to a button releases */
  if (event->type != GDK_BUTTON_RELEASE)
    return FALSE;

  window = g_object_get_qdata (G_OBJECT (mi), window_quark);
  if (event->button == 1)
    {
      /* go to workspace and activate window */
      workspace = wnck_window_get_workspace (window);
      if (workspace != NULL)
        wnck_workspace_activate (workspace, event->time - 1);
      wnck_window_activate (window, event->time);
    }
  else if (event->button == 2)
    {
      /* active the window (bring it to this workspace) */
      wnck_window_activate (window, event->time);
    }
  else if (event->button == 3)
    {
      /* popup the window action menu */
      menu = wnck_action_menu_new (window);
      g_signal_connect (G_OBJECT (menu), "selection-done",
          G_CALLBACK (window_menu_plugin_menu_actions_selection_done),
          gtk_widget_get_parent (mi));
      xfce_panel_plugin_popup_menu (XFCE_PANEL_PLUGIN (plugin), GTK_MENU (menu),
                                    NULL, (GdkEvent *) event);

      return TRUE;
    }

  return FALSE;
}



static GtkWidget *
window_menu_plugin_menu_window_item_new (WnckWindow           *window,
                                         WindowMenuPlugin     *plugin,
                                         PangoFontDescription *italic,
                                         PangoFontDescription *bold,
                                         gint                  icon_w,
                                         gint                  icon_h)
{
  const gchar *name, *tooltip;
  gchar       *label_text = NULL;
  gchar       *utf8 = NULL;
  gchar       *decorated = NULL;
  GtkWidget   *mi, *label, *image;
  GdkPixbuf   *pixbuf, *lucent = NULL, *scaled = NULL;
  gint         scale_factor;

  panel_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);

  /* try to get an utf-8 valid name */
  name = wnck_window_get_name (window);
  if (!panel_str_is_empty (name) && !g_utf8_validate (name, -1, NULL))
    name = utf8 = g_locale_to_utf8 (name, -1, NULL, NULL, NULL);

  if (panel_str_is_empty (name))
    name = "?";

  /* store the tooltip text */
  tooltip = name;

  /* create a decorated name for the label */
  if (wnck_window_is_shaded (window))
    name = decorated = g_strdup_printf ("=%s=", name);
  else if (wnck_window_is_minimized (window))
    name = decorated = g_strdup_printf ("[%s]", name);

  /* create the menu item */
  mi = panel_image_menu_item_new_with_label (name);
  gtk_widget_set_tooltip_text (mi, tooltip);
  g_object_set_qdata (G_OBJECT (mi), window_quark, window);
  g_signal_connect (G_OBJECT (mi), "button-release-event",
      G_CALLBACK (window_menu_plugin_menu_window_item_activate), plugin);


  /* make the label pretty on long window names */
  label = gtk_bin_get_child (GTK_BIN (mi));
  panel_return_val_if_fail (GTK_IS_LABEL (label), NULL);
  /* modify the label font if needed */
  if (wnck_window_is_active (window))
    label_text = g_strdup_printf ("<b><i>%s</i></b>", name);
  else if (wnck_window_or_transient_needs_attention (window))
    label_text = g_strdup_printf ("<b>%s</b>", name);
  if (label_text)
    {
      gtk_label_set_markup (GTK_LABEL (label), label_text);
      g_free (label_text);
    }

  g_free (decorated);
  g_free (utf8);

  gtk_label_set_ellipsize (GTK_LABEL (label), plugin->ellipsize_mode);
  gtk_label_set_max_width_chars (GTK_LABEL (label), plugin->max_width_chars);

  if (plugin->minimized_icon_lucency > 0)
    {
      /* get the window icon */
      pixbuf = wnck_window_get_mini_icon (window);
      scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (plugin));
      icon_w *= scale_factor;
      icon_h *= scale_factor;
      if (pixbuf != NULL
          && (gdk_pixbuf_get_width (pixbuf) < icon_w
              || gdk_pixbuf_get_height (pixbuf) < icon_h))
        pixbuf = wnck_window_get_icon (window);

      if (pixbuf != NULL)
        {
          cairo_surface_t *surface;

          /* scale the icon if needed */
          if (gdk_pixbuf_get_width (pixbuf) > icon_w
              || gdk_pixbuf_get_height (pixbuf) > icon_h)
            {
              scaled = gdk_pixbuf_scale_simple (pixbuf, icon_w, icon_h, GDK_INTERP_BILINEAR);
              if (G_LIKELY (scaled != NULL))
                pixbuf = scaled;
            }

          /* dimm the icon if the window is minimized */
          if (wnck_window_is_minimized (window)
              && plugin->minimized_icon_lucency < 100)
            {
              lucent = exo_gdk_pixbuf_lucent (pixbuf, plugin->minimized_icon_lucency);
              if (G_LIKELY (lucent != NULL))
                pixbuf = lucent;
            }

          /* set the menu item label */
          surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale_factor, NULL);
          image = gtk_image_new_from_surface (surface);
          cairo_surface_destroy (surface);
          panel_image_menu_item_set_image (mi, image);
          gtk_widget_show (image);

          if (lucent != NULL)
            g_object_unref (G_OBJECT (lucent));
          if (scaled != NULL)
            g_object_unref (G_OBJECT (scaled));
        }
    }

  return mi;
}



static void
window_menu_plugin_menu_deactivate (GtkWidget        *menu,
                                    WindowMenuPlugin *plugin)
{
  panel_return_if_fail (plugin->button == NULL || GTK_IS_TOGGLE_BUTTON (plugin->button));
  panel_return_if_fail (GTK_IS_MENU (menu));

  if (plugin->button != NULL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->button), FALSE);

  /* delay destruction so we can handle the activate event first */
  panel_utils_destroy_later (GTK_WIDGET (menu));
}



static gboolean
window_menu_plugin_menu_key_press_event (GtkWidget        *menu,
                                         GdkEventKey      *event,
                                         WindowMenuPlugin *plugin)
{
  GtkWidget      *mi = NULL;
  GdkEventButton  fake_event = { 0, };
  guint           modifiers;
  WnckWindow     *window;

  panel_return_val_if_fail (GTK_IS_MENU (menu), FALSE);

  /* construct an event */
  switch (event->keyval)
    {
    case GDK_KEY_space:
    case GDK_KEY_Return:
    case GDK_KEY_KP_Space:
    case GDK_KEY_KP_Enter:
      /* active the menu item */
      fake_event.button = 1;
      break;

    case GDK_KEY_Menu:
      /* popup the window actions menu */
      fake_event.button = 3;
      break;

    default:
      return FALSE;
    }

  /* popdown the menu, this will also emit the "deactivate" signal */
  gtk_menu_shell_deactivate (GTK_MENU_SHELL (menu));

  /* get the active menu item leave when no item if found */
  mi = gtk_menu_get_active (GTK_MENU (menu));
  panel_return_val_if_fail (mi == NULL || GTK_IS_MENU_ITEM (mi), FALSE);
  if (mi == NULL)
    return FALSE;

  if (fake_event.button == 1)
    {
      /* get the modifiers */
      modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

      if (modifiers == GDK_SHIFT_MASK)
        fake_event.button = 2;
      else if (modifiers == GDK_CONTROL_MASK)
        fake_event.button = 3;
    }

  /* complete the event */
  fake_event.type = GDK_BUTTON_RELEASE;
  fake_event.time = event->time;

  /* try the get the window and active an item */
  window = g_object_get_qdata (G_OBJECT (mi), window_quark);
  if (window != NULL)
    window_menu_plugin_menu_window_item_activate (mi, &fake_event, plugin);
  else
    gtk_menu_item_activate (GTK_MENU_ITEM (mi));

  return FALSE;
}



static GtkWidget *
window_menu_plugin_menu_new (WindowMenuPlugin *plugin)
{
  GtkWidget            *menu, *mi = NULL, *image;
  GList                *workspaces, *lp, fake;
  GList                *windows, *li;
  WnckWorkspace        *workspace = NULL;
  WnckWorkspace        *active_workspace, *window_workspace;
  WnckWindow           *window;
  PangoFontDescription *italic, *bold;
  gint                  urgent_windows = 0;
  gboolean              is_empty = TRUE;
  guint                 n_workspaces = 0;
  const gchar          *name = NULL;
  gchar                *utf8 = NULL, *label;
  gint                  w, h;

  panel_return_val_if_fail (XFCE_IS_WINDOW_MENU_PLUGIN (plugin), NULL);
  panel_return_val_if_fail (WNCK_IS_SCREEN (plugin->screen), NULL);

  italic = pango_font_description_from_string ("italic");
  bold = pango_font_description_from_string ("bold");

  if (!gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h))
    w = h = 16;

  menu = gtk_menu_new ();
  g_signal_connect (G_OBJECT (menu), "key-press-event",
      G_CALLBACK (window_menu_plugin_menu_key_press_event), plugin);

  /* get all the windows and the active workspace */
  windows = wnck_screen_get_windows_stacked (plugin->screen);
  active_workspace = wnck_screen_get_active_workspace (plugin->screen);

  if (plugin->all_workspaces)
    {
      /* get all the workspaces */
      workspaces = wnck_screen_get_workspaces (plugin->screen);
    }
  else
    {
      /* create a fake list with only the active workspace */
      fake.next = fake.prev = NULL;
      fake.data = active_workspace;
      workspaces = &fake;
    }

  for (lp = workspaces; lp != NULL; lp = lp->next, n_workspaces++)
    {
      workspace = WNCK_WORKSPACE (lp->data);

      if (plugin->workspace_names)
        {
          /* create the workspace menu item */
          mi = window_menu_plugin_menu_workspace_item_new (workspace, plugin,
              workspace == active_workspace);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
          gtk_widget_show (mi);

          /* not empty anymore */
          is_empty = FALSE;
        }

      for (li = windows; li != NULL; li = li->next)
        {
          window = WNCK_WINDOW (li->data);

          /* windows we always want to skip */
          if (wnck_window_is_skip_pager (window)
              || wnck_window_is_skip_tasklist (window))
            continue;

          /* get the window's workspace */
          window_workspace = wnck_window_get_workspace (window);

          /* show only windows from this workspace or pinned
           * windows on the active workspace */
          if (window_workspace != workspace
              && !(window_workspace == NULL
                   && workspace == active_workspace))
            continue;

          /* create the menu item */
          mi = window_menu_plugin_menu_window_item_new (window, plugin, italic, bold, w, h);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
          gtk_widget_show (mi);

          /* menu is not empty anymore */
          is_empty = FALSE;

          /* count the urgent windows */
          if (wnck_window_needs_attention (window))
            urgent_windows++;
        }

      mi = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_show (mi);
    }

  /* destroy the last menu item if it's a separator */
  if (mi != NULL && GTK_IS_SEPARATOR_MENU_ITEM (mi))
    gtk_widget_destroy (mi);

  /* add a menu item if there are not windows found */
  if (is_empty)
    {
      mi = gtk_menu_item_new_with_label (_("No Windows"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_set_sensitive (mi, FALSE);
      gtk_widget_show (mi);
    }

  /* check if we need to append the urgent windows on other workspaces */
  if (!plugin->all_workspaces && plugin->urgent_windows > urgent_windows)
    {
      if (plugin->workspace_names)
        {
          mi = gtk_separator_menu_item_new ();
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
          gtk_widget_show (mi);

          mi = gtk_menu_item_new_with_label (_("Urgent Windows"));
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
          gtk_widget_set_sensitive (mi, FALSE);
          gtk_widget_show (mi);
        }

      mi = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_show (mi);

      for (li = windows; li != NULL; li = li->next)
        {
          window = WNCK_WINDOW (li->data);

          /* always skip these windows */
          if (wnck_window_is_skip_pager (window)
              || wnck_window_is_skip_tasklist (window))
            continue;

          /* get the window's workspace */
          window_workspace = wnck_window_get_workspace (window);

          /* only acept windows that are not on the active workspace,
           * not sticky and urgent */
          if (window_workspace == active_workspace
              || window_workspace == NULL
              || !wnck_window_needs_attention (window))
            continue;

          /* create the menu item */
          mi = window_menu_plugin_menu_window_item_new (window, plugin, italic, bold, w, h);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
          gtk_widget_show (mi);
        }
    }

  if (plugin->workspace_actions)
    {
      mi = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_show (mi);

      mi = panel_image_menu_item_new_with_label (_("Add Workspace"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      g_signal_connect (G_OBJECT (mi), "activate",
          G_CALLBACK (window_menu_plugin_workspace_add), plugin);
      gtk_widget_show (mi);

      image = gtk_image_new_from_icon_name ("list-add", GTK_ICON_SIZE_MENU);
      panel_image_menu_item_set_image (mi, image);
      gtk_widget_show (mi);

      if (G_LIKELY (workspace != NULL))
        {
          /* try to get an utf-8 valid name */
          name = wnck_workspace_get_name (workspace);
          if (!panel_str_is_empty (name) && !g_utf8_validate (name, -1, NULL))
            name = utf8 = g_locale_to_utf8 (name, -1, NULL, NULL, NULL);
        }

      /* create label */
      if (!panel_str_is_empty (name))
        label = g_strdup_printf (_("Remove Workspace \"%s\""), name);
      else
        label = g_strdup_printf (_("Remove Workspace %d"), n_workspaces);

      mi = panel_image_menu_item_new_with_label (label);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_set_sensitive (mi, !!(n_workspaces > 1));
      g_signal_connect (G_OBJECT (mi), "activate",
          G_CALLBACK (window_menu_plugin_workspace_remove), plugin);
      gtk_widget_show (mi);

      g_free (label);
      g_free (utf8);

      image = gtk_image_new_from_icon_name ("list-remove", GTK_ICON_SIZE_MENU);
      panel_image_menu_item_set_image (mi, image);
      gtk_widget_show (mi);
    }

  pango_font_description_free (italic);
  pango_font_description_free (bold);

  return menu;
}



static void
window_menu_plugin_menu (GtkWidget        *button,
                         WindowMenuPlugin *plugin)
{
  GtkWidget *menu;
  GdkEvent  *event = NULL;

  panel_return_if_fail (XFCE_IS_WINDOW_MENU_PLUGIN (plugin));
  panel_return_if_fail (button == NULL || plugin->button == button);

  if (button != NULL
      && !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    return;

  /* Panel plugin remote events don't send actual GdkEvents, so construct a minimal one so that
   * gtk_menu_popup_at_pointer/rect can extract a location correctly from a GdkWindow */
  event = gtk_get_current_event ();
  if (event == NULL)
    {
      GdkSeat *seat = gdk_display_get_default_seat (gdk_display_get_default ());
      event = gdk_event_new (GDK_BUTTON_PRESS);
      event->button.window = g_object_ref (gdk_get_default_root_window ());
      gdk_event_set_device (event, gdk_seat_get_pointer (seat));
    }

  /* popup the menu */
  menu = window_menu_plugin_menu_new (plugin);
  g_signal_connect (G_OBJECT (menu), "deactivate",
      G_CALLBACK (window_menu_plugin_menu_deactivate), plugin);

  /* do not block panel autohide if popup-command at pointer */
  if (button == NULL)
    gtk_menu_popup_at_pointer (GTK_MENU (menu), event);
  else
    xfce_panel_plugin_popup_menu (XFCE_PANEL_PLUGIN (plugin), GTK_MENU (menu), button, event);

  gdk_event_free (event);
}
