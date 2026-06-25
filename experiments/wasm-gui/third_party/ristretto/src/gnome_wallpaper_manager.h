/*
 *  Copyright (c) Stephan Arts 2006-2012 <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#ifndef __RISTRETTO_GNOME_WALLPAPER_MANAGER_H__
#define __RISTRETTO_GNOME_WALLPAPER_MANAGER_H__

#include <gtk/gtk.h>

#include "wallpaper_manager.h"

G_BEGIN_DECLS

#define RSTTO_TYPE_GNOME_WALLPAPER_MANAGER rstto_gnome_wallpaper_manager_get_type ()
G_DECLARE_FINAL_TYPE (RsttoGnomeWallpaperManager, rstto_gnome_wallpaper_manager, RSTTO, GNOME_WALLPAPER_MANAGER, GObject)

typedef struct _RsttoGnomeWallpaperManagerPrivate RsttoGnomeWallpaperManagerPrivate;

struct _RsttoGnomeWallpaperManager
{
    GObject parent;
    RsttoGnomeWallpaperManagerPrivate *priv;
};



RsttoWallpaperManager *rstto_gnome_wallpaper_manager_new (void);

G_END_DECLS

#endif /* __RISTRETTO_GNOME_WALLPAPER_MANAGER_H__ */
