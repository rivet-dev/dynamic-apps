/*-
 * Copyright (c) 2017 Andre Miranda <andreldm@xfce.org>
 *
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <thunarx/thunarx-private.h>
#include <thunarx/thunarx-menu-item.h>
#include <thunarx/thunarx-menu.h>



/**
 * SECTION: thunarx-menu
 * @short_description: The base class for submenus added to the context menus
 * @title: ThunarxMenu
 * @include: thunarx/thunarx.h
 *
 * The class for submenus that can be added to Thunar's context menus. Extensions
 * can provide ThunarxMenu objects by attaching them to ThunarxMenuItem objects,
 * using thunarx_menu_item_set_menu().
 */



static void thunarx_menu_finalize (GObject      *object);



struct _ThunarxMenuPrivate
{
  GList *items;
};



G_DEFINE_TYPE_WITH_PRIVATE (ThunarxMenu, thunarx_menu, G_TYPE_OBJECT)



static void
thunarx_menu_class_init (ThunarxMenuClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunarx_menu_finalize;
}



static void
thunarx_menu_init (ThunarxMenu *menu)
{
  menu->priv = thunarx_menu_get_instance_private (menu);
  menu->priv->items = NULL;
}



static void
thunarx_menu_finalize (GObject *object)
{
  ThunarxMenu *menu = THUNARX_MENU (object);

  if (menu->priv->items)
    g_list_free (menu->priv->items);

  (*G_OBJECT_CLASS (thunarx_menu_parent_class)->finalize) (object);
}



/**
 * thunarx_menu_new:
 *
 * Creates a new menu that can be added to the toolbar or to a contextual menu.
 *
 * Returns: a newly created #ThunarxMenu
 */
ThunarxMenu *
thunarx_menu_new (void)
{
  return g_object_new (THUNARX_TYPE_MENU, NULL);
}



/**
 * thunarx_menu_append_item:
 * @menu: a #ThunarxMenu
 * @item: a #ThunarxMenuItem
 */
void
thunarx_menu_append_item (ThunarxMenu *menu, ThunarxMenuItem *item)
{
  g_return_if_fail (menu != NULL);
  g_return_if_fail (item != NULL);

  menu->priv->items = g_list_append (menu->priv->items, g_object_ref (item));
}



/**
 * thunarx_menu_prepend_item:
 * @menu: a #ThunarxMenu
 * @item: a #ThunarxMenuItem
 */
void
thunarx_menu_prepend_item (ThunarxMenu *menu, ThunarxMenuItem *item)
{
  g_return_if_fail (menu != NULL);
  g_return_if_fail (item != NULL);

  menu->priv->items = g_list_prepend (menu->priv->items, g_object_ref (item));
}



/**
 * thunarx_menu_get_items:
 * @menu: a #ThunarxMenu
 *
 * Returns: (element-type ThunarxMenuItem) (transfer full): the provided #ThunarxMenuItem list
 * Must be freed with thunarx_menu_item_list_free() after usage
 */
GList *
thunarx_menu_get_items (ThunarxMenu *menu)
{
  GList *items;

  g_return_val_if_fail (menu != NULL, NULL);

  items = g_list_copy (menu->priv->items);
  g_list_foreach (items, (GFunc) (void (*)(void)) g_object_ref, NULL);

  return items;
}
