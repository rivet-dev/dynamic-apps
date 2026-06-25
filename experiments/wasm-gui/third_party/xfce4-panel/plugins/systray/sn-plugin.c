/*
 *  Copyright (c) 2017 Viktor Odintsev <ninetls@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <common/panel-private.h>
#include <common/panel-xfconf.h>
#include <common/panel-utils.h>
#include <common/panel-debug.h>

#include "systray.h"
#include "systray-box.h"
#include "systray-socket.h"
#include "systray-manager.h"

#include "sn-backend.h"
#include "sn-box.h"
#include "sn-button.h"
#include "sn-dialog.h"
#include "sn-item.h"
#include "sn-plugin.h"

#define BUTTON_SIZE   (16)


static void                  sn_plugin_construct                     (XfcePanelPlugin         *panel_plugin);

static void                  sn_plugin_free                          (XfcePanelPlugin         *panel_plugin);

static gboolean              sn_plugin_size_changed                  (XfcePanelPlugin         *panel_plugin,
                                                                      gint                     size);

static void                  sn_plugin_mode_changed                  (XfcePanelPlugin         *panel_plugin,
                                                                      XfcePanelPluginMode      mode);

static void                  sn_plugin_configure_plugin              (XfcePanelPlugin         *panel_plugin);





XFCE_PANEL_DEFINE_PLUGIN (SnPlugin, sn_plugin,
                          systray_box_register_type,
                          systray_manager_register_type,
                          systray_socket_register_type)



static void
sn_plugin_class_init (SnPluginClass *klass)
{
  XfcePanelPluginClass *panel_plugin_class;

  panel_plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
  panel_plugin_class->construct = sn_plugin_construct;
  panel_plugin_class->free_data = sn_plugin_free;
  panel_plugin_class->size_changed = sn_plugin_size_changed;
  panel_plugin_class->mode_changed = sn_plugin_mode_changed;
  panel_plugin_class->configure_plugin = sn_plugin_configure_plugin;
}



static void
sn_plugin_init (SnPlugin *plugin)
{
  /* Systray init */
  plugin->manager = NULL;
  plugin->idle_startup = 0;
  plugin->names_ordered = NULL;
  plugin->names_hidden = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  plugin->systray_box = NULL;

  /* Statusnotifier init */
  plugin->item = NULL;
#ifdef HAVE_DBUSMENU
  plugin->backend = NULL;
#endif
  plugin->config = NULL;
}



static void
sn_plugin_free (XfcePanelPlugin *panel_plugin)
{
  SnPlugin *plugin = XFCE_SN_PLUGIN (panel_plugin);

  /* Systray */
  /* stop pending idle startup */
  if (plugin->idle_startup != 0)
    g_source_remove (plugin->idle_startup);

  /* disconnect screen changed signal */
  g_signal_handlers_disconnect_by_func (G_OBJECT (plugin),
      systray_plugin_screen_changed, NULL);

  g_slist_free_full (plugin->names_ordered, g_free);
  g_hash_table_destroy (plugin->names_hidden);

  if (G_LIKELY (plugin->manager != NULL))
    {
      systray_manager_unregister (plugin->manager);
      g_object_unref (G_OBJECT (plugin->manager));
    }

  gtk_container_remove (GTK_CONTAINER (plugin->box), plugin->systray_box);

  /* Statusnotifier */
  /* remove children so they won't use unrefed SnItems and SnConfig */
  gtk_container_remove (GTK_CONTAINER (plugin->box), plugin->sn_box);
  gtk_container_remove (GTK_CONTAINER (panel_plugin), plugin->box);

#ifdef HAVE_DBUSMENU
  g_object_unref (plugin->backend);
#endif
  g_object_unref (plugin->config);
}



static gboolean
sn_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                        gint             size)
{
  SnPlugin *plugin = XFCE_SN_PLUGIN (panel_plugin);

  sn_config_set_size (plugin->config,
                      size,
                      xfce_panel_plugin_get_nrows (panel_plugin),
                      xfce_panel_plugin_get_icon_size (panel_plugin));
  systray_plugin_size_changed (panel_plugin,
                               xfce_panel_plugin_get_size (panel_plugin));

  return TRUE;
}



static void
sn_plugin_mode_changed (XfcePanelPlugin     *panel_plugin,
                        XfcePanelPluginMode  mode)
{
  SnPlugin       *plugin = XFCE_SN_PLUGIN (panel_plugin);
  GtkOrientation  orientation;
  GtkOrientation  panel_orientation;

  panel_orientation = xfce_panel_plugin_get_orientation (panel_plugin);
  orientation = mode == XFCE_PANEL_PLUGIN_MODE_VERTICAL
                ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;

  sn_config_set_orientation (plugin->config, panel_orientation, orientation);
  systray_plugin_orientation_changed (panel_plugin, panel_orientation);

  sn_plugin_size_changed (panel_plugin, xfce_panel_plugin_get_size (panel_plugin));
}



static void
sn_plugin_unblock_autohide (gpointer  data,
                            GObject  *where_the_object_was)
{
  XfcePanelPlugin *panel_plugin = data;

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN (panel_plugin));

  xfce_panel_plugin_block_autohide (panel_plugin, FALSE);
}



static void
sn_plugin_configure_plugin (XfcePanelPlugin *panel_plugin)
{
  SnPlugin *plugin = XFCE_SN_PLUGIN (panel_plugin);
  SnDialog *dialog;

  dialog = sn_dialog_new (plugin->config, gtk_widget_get_screen (GTK_WIDGET (plugin)));
  if (dialog != NULL)
    {
      xfce_panel_plugin_block_menu (panel_plugin);
      xfce_panel_plugin_block_autohide (panel_plugin, TRUE);
      g_object_weak_ref (G_OBJECT (dialog), _panel_utils_weak_notify, panel_plugin);
      g_object_weak_ref (G_OBJECT (dialog), sn_plugin_unblock_autohide, panel_plugin);
    }
}

#ifdef HAVE_DBUSMENU
static void
sn_plugin_item_added (SnPlugin *plugin,
                      SnItem   *item)
{
  GtkWidget *button;

  button = sn_button_new (item, plugin, plugin->config);

  sn_config_add_known_item (plugin->config, sn_item_get_name (item));

  gtk_container_add (GTK_CONTAINER (plugin->sn_box), button);
  gtk_widget_show (button);
}
#endif



gboolean
sn_plugin_legacy_item_added (SnPlugin    *plugin,
                             const gchar *name)
{
  return sn_config_add_known_legacy_item (plugin->config, name);
}

#ifdef HAVE_DBUSMENU
static void
sn_plugin_item_removed (SnPlugin *plugin,
                        SnItem   *item)
{
  sn_box_remove_item (XFCE_SN_BOX (plugin->sn_box), item);
}
#endif


static void
update_button_visibility (SnPlugin *plugin)
{
  gboolean visible = plugin->has_hidden_systray_items || plugin->has_hidden_sn_items;
  gtk_widget_set_visible(GTK_WIDGET(plugin->button), visible);
}


static void
systray_has_hidden_cb (SystrayBox *box,
                       GParamSpec *pspec,
                       SnPlugin   *plugin)
{
  plugin->has_hidden_systray_items = systray_box_has_hidden_items (box);
  update_button_visibility (plugin);
}


static void
snbox_has_hidden_cb (SnBox      *box,
                     GParamSpec *pspec,
                     SnPlugin   *plugin)
{
  plugin->has_hidden_sn_items = sn_box_has_hidden_items (box);
  update_button_visibility (plugin);
}



static void
sn_plugin_button_set_arrow(SnPlugin *plugin)
{
  GtkArrowType arrow_type;
  gboolean show_hidden;
  GtkOrientation orientation;

  panel_return_if_fail(XFCE_IS_SN_PLUGIN(plugin));

  show_hidden = systray_box_get_show_hidden(XFCE_SYSTRAY_BOX(plugin->systray_box));
  orientation = xfce_panel_plugin_get_orientation(XFCE_PANEL_PLUGIN(plugin));
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    arrow_type = show_hidden ? GTK_ARROW_LEFT : GTK_ARROW_RIGHT;
  else
    arrow_type = show_hidden ? GTK_ARROW_UP : GTK_ARROW_DOWN;

  xfce_arrow_button_set_arrow_type(XFCE_ARROW_BUTTON(plugin->button), arrow_type);
}



static void
sn_plugin_button_toggled (GtkWidget     *button,
                          SnPlugin      *plugin)
{
  panel_return_if_fail (XFCE_IS_SN_PLUGIN (plugin));
  panel_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));
  panel_return_if_fail (plugin->button == button);

  systray_box_set_show_hidden (XFCE_SYSTRAY_BOX (plugin->systray_box),
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
  sn_box_set_show_hidden (XFCE_SN_BOX (plugin->sn_box),
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
  sn_plugin_button_set_arrow (plugin);
}



static void
sn_plugin_construct (XfcePanelPlugin *panel_plugin)
{
  SnPlugin *plugin = XFCE_SN_PLUGIN (panel_plugin);

  plugin->manager = NULL;
  plugin->idle_startup = 0;
  plugin->names_ordered = NULL;
  plugin->names_hidden = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  xfce_panel_plugin_menu_show_configure (panel_plugin);

  plugin->config = sn_config_new (xfce_panel_plugin_get_property_base (panel_plugin));

  /* Container for both plugins */
  plugin->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_container_add (GTK_CONTAINER (plugin), plugin->box);
  gtk_widget_show (plugin->box);

  /* Add systray box */
  plugin->systray_box = systray_box_new ();
  gtk_box_pack_start (GTK_BOX (plugin->box), plugin->systray_box, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (plugin->systray_box), "draw",
      G_CALLBACK (systray_plugin_box_draw), plugin);
  gtk_container_set_border_width (GTK_CONTAINER (plugin->systray_box), 0);
  gtk_widget_show (plugin->systray_box);

  /* monitor screen changes */
  g_signal_connect (G_OBJECT (plugin), "screen-changed",
      G_CALLBACK (systray_plugin_screen_changed), NULL);
  systray_plugin_screen_changed (GTK_WIDGET (plugin), NULL);

  /* restart internally if compositing changed */
  g_signal_connect_object (gdk_screen_get_default (), "composited-changed",
     G_CALLBACK (systray_plugin_composited_changed), plugin, G_CONNECT_SWAPPED);

  /* Add statusnotifier box */
  plugin->sn_box = sn_box_new (plugin->config);
  gtk_box_pack_start (GTK_BOX (plugin->box), plugin->sn_box, TRUE, TRUE, 0);
  gtk_widget_show (GTK_WIDGET (plugin->sn_box));

  g_signal_connect_swapped (plugin->config, "configuration-changed",
                            G_CALLBACK (gtk_widget_queue_resize), plugin->systray_box);
  g_signal_connect_swapped (plugin->config, "configuration-changed",
                            G_CALLBACK (gtk_widget_queue_resize), plugin->sn_box);
  g_signal_connect (plugin->config, "configuration-changed",
                            G_CALLBACK (systray_plugin_configuration_changed), plugin);
  g_signal_connect (plugin->config, "legacy-items-list-changed",
                            G_CALLBACK (systray_plugin_configuration_changed), plugin);

#ifdef HAVE_DBUSMENU
  plugin->backend = sn_backend_new ();
  g_signal_connect_swapped (plugin->backend, "item-added",
                            G_CALLBACK (sn_plugin_item_added), plugin);
  g_signal_connect_swapped (plugin->backend, "item-removed",
                            G_CALLBACK (sn_plugin_item_removed), plugin);
  sn_backend_start (plugin->backend);
#endif

  /* Systray arrow button */
  plugin->button = xfce_arrow_button_new(GTK_ARROW_RIGHT);
  gtk_box_pack_start(GTK_BOX(plugin->box), plugin->button, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(plugin->button), "toggled",
                   G_CALLBACK(sn_plugin_button_toggled), plugin);
  gtk_button_set_relief(GTK_BUTTON(plugin->button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT(plugin->systray_box), "notify::has-hidden",
                    G_CALLBACK(systray_has_hidden_cb), plugin);
  g_signal_connect (G_OBJECT(plugin->sn_box), "notify::has-hidden",
                    G_CALLBACK(snbox_has_hidden_cb), plugin);
  xfce_panel_plugin_add_action_widget(XFCE_PANEL_PLUGIN(plugin), plugin->button);
}
