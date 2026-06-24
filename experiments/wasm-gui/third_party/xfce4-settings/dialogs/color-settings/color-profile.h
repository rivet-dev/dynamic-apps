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

#define TYPE_COLOR_PROFILE (color_profile_get_type ())
G_DECLARE_FINAL_TYPE (ColorProfile, color_profile, SETTINGS, COLOR_PROFILE, GtkListBoxRow)

GtkWidget   *color_profile_new              (CdDevice        *device,
                                             CdProfile       *profile,
                                             gboolean         is_default);
gboolean     color_profile_get_is_default   (ColorProfile    *color_profile);
void         color_profile_set_is_default   (ColorProfile    *color_profile,
                                             gboolean         profile_is_default);
CdDevice    *color_profile_get_device       (ColorProfile    *color_profile);
CdProfile   *color_profile_get_profile      (ColorProfile    *color_profile);

G_END_DECLS
