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

#ifndef __SN_PLUGIN_H__
#define __SN_PLUGIN_H__

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>

#include "systray.h"
#include "systray-manager.h"

#include "sn-backend.h"
#include "sn-config.h"

G_BEGIN_DECLS

typedef struct _SnPluginClass SnPluginClass;
typedef struct _SnPlugin      SnPlugin;

#define XFCE_TYPE_SN_PLUGIN            (sn_plugin_get_type ())
#define XFCE_SN_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_SN_PLUGIN, SnPlugin))
#define XFCE_SN_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_SN_PLUGIN, SnPluginClass))
#define XFCE_IS_SN_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_SN_PLUGIN))
#define XFCE_IS_SN_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_SN_PLUGIN))
#define XFCE_SN_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_SN_PLUGIN, SnPluginClass))

struct _SnPluginClass
{
  XfcePanelPluginClass __parent__;
};

struct _SnPlugin
{
  XfcePanelPlugin      __parent__;

  /* Systray manager */
  SystrayManager *manager;

  guint           idle_startup;
  gboolean        has_hidden_systray_items;
  gboolean        has_hidden_sn_items;

  /* Widgets */
  GtkWidget           *box;
  GtkWidget           *systray_box;
  GtkWidget           *button;
  GtkWidget           *item;
  GtkWidget           *sn_box;

  /* Systray settings */
  GSList         *names_ordered;
  GHashTable     *names_hidden;

  GtkBuilder     *configure_builder;

  /* Statusnotifier settings */
#ifdef HAVE_DBUSMENU
  SnBackend           *backend;
#endif
  SnConfig            *config;
};

GType                  sn_plugin_get_type                      (void) G_GNUC_CONST;

void                   sn_plugin_register_type                 (XfcePanelTypeModule     *panel_type_module);

gboolean               sn_plugin_legacy_item_added             (SnPlugin                *plugin,
                                                                const gchar             *name);

G_END_DECLS

#endif /* !__SN_PLUGIN_H__ */
