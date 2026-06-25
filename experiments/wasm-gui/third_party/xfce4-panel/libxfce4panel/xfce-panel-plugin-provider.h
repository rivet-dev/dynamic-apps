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

#ifndef __XFCE_PANEL_PLUGIN_PROVIDER_H__
#define __XFCE_PANEL_PLUGIN_PROVIDER_H__

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>

G_BEGIN_DECLS

typedef struct _XfcePanelPluginProviderInterface XfcePanelPluginProviderInterface;
typedef struct _XfcePanelPluginProvider          XfcePanelPluginProvider;

#define XFCE_TYPE_PANEL_PLUGIN_PROVIDER               (xfce_panel_plugin_provider_get_type ())
#define XFCE_PANEL_PLUGIN_PROVIDER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_PANEL_PLUGIN_PROVIDER, XfcePanelPluginProvider))
#define XFCE_IS_PANEL_PLUGIN_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_PANEL_PLUGIN_PROVIDER))
#define XFCE_PANEL_PLUGIN_PROVIDER_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), XFCE_TYPE_PANEL_PLUGIN_PROVIDER, XfcePanelPluginProviderInterface))

/* plugin module functions */
typedef GtkWidget *(*PluginConstructFunc) (const gchar  *name,
                                           gint          unique_id,
                                           const gchar  *display_name,
                                           const gchar  *comment,
                                           gchar       **arguments,
                                           GdkScreen    *screen);
typedef GType      (*PluginInitFunc)      (GTypeModule  *module,
                                           gboolean     *make_resident);

struct _XfcePanelPluginProviderInterface
{
  /*< private >*/
  GTypeInterface __parent__;

  /*<public >*/
  const gchar *(*get_name)            (XfcePanelPluginProvider       *provider);
  gint         (*get_unique_id)       (XfcePanelPluginProvider       *provider);
  void         (*set_size)            (XfcePanelPluginProvider       *provider,
                                       gint                           size);
  void         (*set_icon_size)       (XfcePanelPluginProvider       *provider,
                                       gint                           icon_size);
  void         (*set_dark_mode)       (XfcePanelPluginProvider       *provider,
                                       gboolean                       dark_mode);
  void         (*set_mode)            (XfcePanelPluginProvider       *provider,
                                       XfcePanelPluginMode            mode);
  void         (*set_nrows)           (XfcePanelPluginProvider       *provider,
                                       guint                          rows);
  void         (*set_screen_position) (XfcePanelPluginProvider       *provider,
                                       XfceScreenPosition             screen_position);
  void         (*save)                (XfcePanelPluginProvider       *provider);
  gboolean     (*get_show_configure)  (XfcePanelPluginProvider       *provider);
  void         (*show_configure)      (XfcePanelPluginProvider       *provider);
  gboolean     (*get_show_about)      (XfcePanelPluginProvider       *provider);
  void         (*show_about)          (XfcePanelPluginProvider       *provider);
  void         (*removed)             (XfcePanelPluginProvider       *provider);
  gboolean     (*remote_event)        (XfcePanelPluginProvider       *provider,
                                       const gchar                   *name,
                                       const GValue                  *value,
                                       guint                         *handle);
  void         (*set_locked)          (XfcePanelPluginProvider       *provider,
                                       gboolean                       locked);
  void         (*ask_remove)          (XfcePanelPluginProvider       *provider);
};

/* signals send from the plugin to the panel (possibly through the wrapper) */
typedef enum /*< skip >*/
{
  PROVIDER_SIGNAL_MOVE_PLUGIN = 0,
  PROVIDER_SIGNAL_EXPAND_PLUGIN,
  PROVIDER_SIGNAL_COLLAPSE_PLUGIN,
  PROVIDER_SIGNAL_SMALL_PLUGIN,
  PROVIDER_SIGNAL_UNSMALL_PLUGIN,
  PROVIDER_SIGNAL_LOCK_PANEL,
  PROVIDER_SIGNAL_UNLOCK_PANEL,
  PROVIDER_SIGNAL_REMOVE_PLUGIN,
  PROVIDER_SIGNAL_ADD_NEW_ITEMS,
  PROVIDER_SIGNAL_PANEL_PREFERENCES,
  PROVIDER_SIGNAL_PANEL_LOGOUT,
  PROVIDER_SIGNAL_PANEL_ABOUT,
  PROVIDER_SIGNAL_PANEL_HELP,
  PROVIDER_SIGNAL_SHOW_CONFIGURE,
  PROVIDER_SIGNAL_SHOW_ABOUT,
  PROVIDER_SIGNAL_FOCUS_PLUGIN,
  PROVIDER_SIGNAL_SHRINK_PLUGIN,
  PROVIDER_SIGNAL_UNSHRINK_PLUGIN
}
XfcePanelPluginProviderSignal;

/* properties to the plugin; with a value or as an action */
typedef enum /*< skip >*/
{
  PROVIDER_PROP_TYPE_SET_SIZE,                /* gint */
  PROVIDER_PROP_TYPE_SET_ICON_SIZE,           /* gint */
  PROVIDER_PROP_TYPE_SET_DARK_MODE,           /* gboolean */
  PROVIDER_PROP_TYPE_SET_MODE,                /* XfcePanelPluginMode (as gint) */
  PROVIDER_PROP_TYPE_SET_SCREEN_POSITION,     /* XfceScreenPosition (as gint) */
  PROVIDER_PROP_TYPE_SET_BACKGROUND_ALPHA,    /* gdouble */
  PROVIDER_PROP_TYPE_SET_NROWS,               /* gint */
  PROVIDER_PROP_TYPE_SET_LOCKED,              /* gboolean */
  PROVIDER_PROP_TYPE_SET_SENSITIVE,           /* gboolean */
  PROVIDER_PROP_TYPE_SET_BACKGROUND_COLOR,    /* string, wrapper only */
  PROVIDER_PROP_TYPE_SET_BACKGROUND_IMAGE,    /* string, wrapper only */
  PROVIDER_PROP_TYPE_ACTION_REMOVED,          /* none */
  PROVIDER_PROP_TYPE_ACTION_SAVE,             /* none */
  PROVIDER_PROP_TYPE_ACTION_QUIT,             /* none */
  PROVIDER_PROP_TYPE_ACTION_QUIT_FOR_RESTART, /* none */
  PROVIDER_PROP_TYPE_ACTION_BACKGROUND_UNSET, /* none */
  PROVIDER_PROP_TYPE_ACTION_SHOW_CONFIGURE,   /* none */
  PROVIDER_PROP_TYPE_ACTION_SHOW_ABOUT,       /* none */
  PROVIDER_PROP_TYPE_ACTION_ASK_REMOVE,       /* none */
  PROVIDER_PROP_TYPE_SET_OPACITY              /* gdouble */
}
XfcePanelPluginProviderPropType;

/* plugin exit values */
enum
{
  PLUGIN_EXIT_SUCCESS = 0,
  PLUGIN_EXIT_FAILURE,
  PLUGIN_EXIT_ARGUMENTS_FAILED,
  PLUGIN_EXIT_PREINIT_FAILED,
  PLUGIN_EXIT_CHECK_FAILED,
  PLUGIN_EXIT_NO_PROVIDER,
  PLUGIN_EXIT_SUCCESS_AND_RESTART
};

/* argument handling in plugin and wrapper */
enum
{
  PLUGIN_ARGV_0 = 0,
  PLUGIN_ARGV_FILENAME,
  PLUGIN_ARGV_UNIQUE_ID,
  PLUGIN_ARGV_SOCKET_ID,
  PLUGIN_ARGV_NAME,
  PLUGIN_ARGV_DISPLAY_NAME,
  PLUGIN_ARGV_COMMENT,
  PLUGIN_ARGV_ARGUMENTS
};



GType                 xfce_panel_plugin_provider_get_type            (void) G_GNUC_CONST;

const gchar          *xfce_panel_plugin_provider_get_name            (XfcePanelPluginProvider       *provider);

gint                  xfce_panel_plugin_provider_get_unique_id       (XfcePanelPluginProvider       *provider);

void                  xfce_panel_plugin_provider_set_size            (XfcePanelPluginProvider       *provider,
                                                                      gint                           size);

void                  xfce_panel_plugin_provider_set_icon_size       (XfcePanelPluginProvider       *provider,
                                                                      gint                           icon_size);

void                  xfce_panel_plugin_provider_set_dark_mode       (XfcePanelPluginProvider       *provider,
                                                                      gboolean                       dark_mode);

void                  xfce_panel_plugin_provider_set_mode            (XfcePanelPluginProvider       *provider,
                                                                      XfcePanelPluginMode            mode);

void                  xfce_panel_plugin_provider_set_nrows           (XfcePanelPluginProvider       *provider,
                                                                      guint                          rows);

void                  xfce_panel_plugin_provider_set_screen_position (XfcePanelPluginProvider       *provider,
                                                                      XfceScreenPosition             screen_position);

void                  xfce_panel_plugin_provider_save                (XfcePanelPluginProvider       *provider);

void                  xfce_panel_plugin_provider_emit_signal         (XfcePanelPluginProvider       *provider,
                                                                      XfcePanelPluginProviderSignal  provider_signal);

gboolean              xfce_panel_plugin_provider_get_show_configure  (XfcePanelPluginProvider       *provider);

void                  xfce_panel_plugin_provider_show_configure      (XfcePanelPluginProvider       *provider);

gboolean              xfce_panel_plugin_provider_get_show_about      (XfcePanelPluginProvider       *provider);

void                  xfce_panel_plugin_provider_show_about          (XfcePanelPluginProvider       *provider);

void                  xfce_panel_plugin_provider_removed             (XfcePanelPluginProvider       *provider);

gboolean              xfce_panel_plugin_provider_remote_event        (XfcePanelPluginProvider       *provider,
                                                                      const gchar                   *name,
                                                                      const GValue                  *value,
                                                                      guint                         *handle);

void                  xfce_panel_plugin_provider_set_locked          (XfcePanelPluginProvider       *provider,
                                                                      gboolean                       locked);

void                  xfce_panel_plugin_provider_ask_remove          (XfcePanelPluginProvider       *provider);

G_END_DECLS

#endif /* !__XFCE_PANEL_PLUGIN_PROVIDER_H__ */
