/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <gtk/gtk.h>
#include <colord.h>

G_BEGIN_DECLS

#define TYPE_COLOR_DEVICE (color_device_get_type ())
G_DECLARE_FINAL_TYPE (ColorDevice, color_device, SETTINGS, COLOR_DEVICE, GtkListBoxRow)

gchar       *color_device_get_kind          (CdDevice     *device);
gchar       *color_device_get_sortable_base (CdDevice     *device);
gchar       *color_device_get_title         (CdDevice     *device);
GtkWidget   *color_device_new               (CdDevice     *device);
CdDevice    *color_device_get_device        (ColorDevice  *color_device);
const gchar *color_device_get_type_icon     (CdDevice     *device);
const gchar *color_device_get_sortable      (ColorDevice  *color_device);
void         color_device_set_enabled       (ColorDevice  *color_device,
                                             gboolean      enabled);

G_END_DECLS
