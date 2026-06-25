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

#ifndef __RISTRETTO_APP_MENU_ITEM_H__
#define __RISTRETTO_APP_MENU_ITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define RSTTO_TYPE_APP_MENU_ITEM (rstto_app_menu_item_get_type ())
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtkImageMenuItem, g_object_unref)
G_DECLARE_FINAL_TYPE (RsttoAppMenuItem, rstto_app_menu_item, RSTTO, APP_MENU_ITEM, GtkImageMenuItem)

typedef struct _RsttoAppMenuItemPrivate RsttoAppMenuItemPrivate;

struct _RsttoAppMenuItem
{
    GtkImageMenuItem parent;
    RsttoAppMenuItemPrivate *priv;
};



GtkWidget *
rstto_app_menu_item_new (GAppInfo *app_info,
                         GFile *file);

G_END_DECLS

#endif /* __RISTRETTO_APP_MENU_ITEM_H__ */
