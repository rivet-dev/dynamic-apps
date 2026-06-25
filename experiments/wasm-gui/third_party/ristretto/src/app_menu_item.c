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

#include "util.h"
#include "app_menu_item.h"



static void
rstto_app_menu_item_finalize (GObject *object);

static void
rstto_app_menu_item_activate (GtkMenuItem *object);



struct _RsttoAppMenuItemPrivate
{
    GAppInfo *app_info;
    GFile *file;
};



G_GNUC_BEGIN_IGNORE_DEPRECATIONS
G_DEFINE_TYPE_WITH_PRIVATE (RsttoAppMenuItem, rstto_app_menu_item, GTK_TYPE_IMAGE_MENU_ITEM)
G_GNUC_END_IGNORE_DEPRECATIONS



static void
rstto_app_menu_item_init (RsttoAppMenuItem *menu_item)
{
    menu_item->priv = rstto_app_menu_item_get_instance_private (menu_item);
}

static void
rstto_app_menu_item_class_init (RsttoAppMenuItemClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkMenuItemClass *menu_item_class = GTK_MENU_ITEM_CLASS (klass);

    object_class->finalize = rstto_app_menu_item_finalize;

    menu_item_class->activate = rstto_app_menu_item_activate;
}

/**
 * rstto_app_menu_item_finalize:
 * @object: application-menu-item
 *
 * Cleanup the app-menu-item internals
 *
 */
static void
rstto_app_menu_item_finalize (GObject *object)
{
    RsttoAppMenuItem *menu_item = RSTTO_APP_MENU_ITEM (object);

    if (menu_item->priv->app_info)
    {
        g_object_unref (menu_item->priv->app_info);
        menu_item->priv->app_info = NULL;
    }
    if (menu_item->priv->file)
    {
        g_object_unref (menu_item->priv->file);
        menu_item->priv->file = NULL;
    }

    G_OBJECT_CLASS (rstto_app_menu_item_parent_class)->finalize (object);
}

/**
 * rstto_app_menu_item_activate:
 * @object: GtkMenuItem that is activated
 *
 * Launch the associated application
 */
static void
rstto_app_menu_item_activate (GtkMenuItem *object)
{
    RsttoAppMenuItem *app_menu_item = RSTTO_APP_MENU_ITEM (object);
    GList *files = g_list_append (NULL, app_menu_item->priv->file);

    g_app_info_launch (app_menu_item->priv->app_info, files, NULL, NULL);

    GTK_MENU_ITEM_CLASS (rstto_app_menu_item_parent_class)->activate (object);
}

/**
 * rstto_app_menu_item_new:
 * @app_info: Application info
 * @file: File
 *
 * Creates new RsttoAppMenuItem
 *
 * Returns: RsttoAppMenuItem that launches application @app_info with @file
 */
GtkWidget *
rstto_app_menu_item_new (GAppInfo *app_info, GFile *file)
{
    RsttoAppMenuItem *menu_item;
    GtkWidget *image = NULL;
    GIcon *icon = NULL;

    g_return_val_if_fail (app_info != NULL, NULL);

    menu_item = g_object_new (RSTTO_TYPE_APP_MENU_ITEM, NULL);

    menu_item->priv->app_info = app_info;
    g_object_ref (app_info);

    menu_item->priv->file = file;
    g_object_ref (file);

    icon = g_app_info_get_icon (app_info);
    if (icon)
    {
        image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
    }

    gtk_menu_item_set_label (GTK_MENU_ITEM (menu_item), g_app_info_get_name (app_info));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
G_GNUC_END_IGNORE_DEPRECATIONS

    return GTK_WIDGET (menu_item);
}
