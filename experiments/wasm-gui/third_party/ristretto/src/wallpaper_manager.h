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

#ifndef __RISTRETTO_WALLPAPER_MANAGER_IFACE__
#define __RISTRETTO_WALLPAPER_MANAGER_IFACE__

#include <gtk/gtk.h>

#include "file.h"

G_BEGIN_DECLS

#define RSTTO_WALLPAPER_MANAGER_TYPE  rstto_wallpaper_manager_get_type ()
G_DECLARE_INTERFACE (RsttoWallpaperManager, rstto_wallpaper_manager, RSTTO, WALLPAPER_MANAGER, GObject)

typedef struct _RsttoWallpaperManagerInterface RsttoWallpaperManagerInterface;

struct _RsttoWallpaperManagerInterface
{
    GTypeInterface parent;

    gint (*configure_dialog_run) (RsttoWallpaperManager *self,
                                  RsttoFile *file,
                                  GtkWindow *parent);
    gboolean (*set) (RsttoWallpaperManager *self,
                     RsttoFile *file);
    gboolean (*check_running) (RsttoWallpaperManager *self);
};



gboolean
rstto_wallpaper_manager_check_running (RsttoWallpaperManager *self);

gint
rstto_wallpaper_manager_configure_dialog_run (RsttoWallpaperManager *self,
                                              RsttoFile *file,
                                              GtkWindow *parent);

gboolean
rstto_wallpaper_manager_set (RsttoWallpaperManager *self,
                             RsttoFile *file);

G_END_DECLS

#endif /* __RISTRETTO_WALLPAPER_MANAGER_IFACE__ */
