/*
 * Copyright (C) 2009-2010 Nick Schermer <nick@xfce.org>
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

#ifndef __PANEL_BUILDER_H__
#define __PANEL_BUILDER_H__

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>

/* Hook to make sure GtkBuilder knows are the XfceTitledDialog object */
#define PANEL_UTILS_LINK_4UI \
  if (xfce_titled_dialog_get_type () == 0) \
    return;

void        _panel_utils_weak_notify   (gpointer          data,
                                        GObject          *where_the_object_was);

GtkBuilder *panel_utils_builder_new    (XfcePanelPlugin  *panel_plugin,
                                        const gchar      *buffer,
                                        gsize             length,
                                        GObject         **dialog_return) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

void        panel_utils_show_help      (GtkWindow        *parent,
                                        const gchar      *page,
                                        const gchar      *offset);

gboolean    panel_utils_device_grab    (GtkWidget        *widget);

void        panel_utils_set_atk_info   (GtkWidget        *widget,
                                        const gchar      *name,
                                        const gchar      *description);

void        panel_utils_destroy_later  (GtkWidget        *widget);

#endif /* !__PANEL_BUILDER_H__ */
