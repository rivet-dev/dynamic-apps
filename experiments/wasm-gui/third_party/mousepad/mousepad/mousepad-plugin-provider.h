/*
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

#ifndef __MOUSEPAD_PLUGIN_PROVIDER_H__
#define __MOUSEPAD_PLUGIN_PROVIDER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_PLUGIN_PROVIDER (mousepad_plugin_provider_get_type ())
G_DECLARE_FINAL_TYPE (MousepadPluginProvider, mousepad_plugin_provider, MOUSEPAD, PLUGIN_PROVIDER, GTypeModule)

/* minimal set of pre-instantiation data that each plugin must provide */
typedef struct _MousepadPluginData
{
  /* G_TYPE_INVALID-terminated array of registered types */
  const GType *types;

  /* whether the plugin is destroyed when no longer used */
  gboolean destroyable;

  /* for the plugin checkbox in the prefs dialog */
  const gchar *label, *tooltip, *category;

  /* keybinding for enabling the plugin, also shown in the prefs dialog */
  const gchar *accel;
} MousepadPluginData;

MousepadPluginProvider *mousepad_plugin_provider_new                (const gchar             *name);

void                    mousepad_plugin_provider_get_types          (MousepadPluginProvider  *plugin,
                                                                     const GType            **types,
                                                                     gint                    *n_types);

gboolean                mousepad_plugin_provider_is_destroyable     (MousepadPluginProvider  *provider);

gboolean                mousepad_plugin_provider_is_instantiated    (MousepadPluginProvider  *provider);

const gchar            *mousepad_plugin_provider_get_label          (MousepadPluginProvider  *provider);

const gchar            *mousepad_plugin_provider_get_tooltip        (MousepadPluginProvider  *provider);

const gchar            *mousepad_plugin_provider_get_category       (MousepadPluginProvider  *provider);

const gchar            *mousepad_plugin_provider_get_accel          (MousepadPluginProvider  *provider);

GtkWidget              *mousepad_plugin_provider_create_setting_box (MousepadPluginProvider  *provider);

GtkWidget              *mousepad_plugin_provider_get_setting_box    (MousepadPluginProvider  *provider);

void                    mousepad_plugin_provider_new_plugin         (MousepadPluginProvider  *provider);

void                    mousepad_plugin_provider_unuse              (gpointer                 data);

G_END_DECLS

#endif /* !__MOUSEPAD_PLUGIN_PROVIDER_H__ */
