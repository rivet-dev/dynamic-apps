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
 * SECTION: thunarx-menu-item
 * @short_description: The base class for menu items added to the context menus
 * @title: ThunarxMenuItem
 * @include: thunarx/thunarx.h
 *
 * The class for menu items that can be added to Thunar's context menus
 * by extensions implementing the #ThunarxMenuProvider, #ThunarxPreferencesProvider
 * or #ThunarxRenamerProvider interfaces. The items returned by extensions from
 * *_get_menu_items() methods are instances of this class or a derived class.
 */

/* Signal identifiers */
enum
{
  ACTIVATE,
  LAST_SIGNAL
};

/* Property identifiers */
enum
{
  PROP_0,
  PROP_NAME,
  PROP_LABEL,
  PROP_TOOLTIP,
  PROP_ICON,
  PROP_SENSITIVE,
  PROP_PRIORITY,
  PROP_MENU,
  LAST_PROP
};



static void thunarx_menu_item_get_property (GObject      *object,
                                            guint         param_id,
                                            GValue       *value,
                                            GParamSpec   *pspec);
static void thunarx_menu_item_set_property (GObject      *object,
                                            guint         param_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);
static void thunarx_menu_item_finalize     (GObject      *object);



struct _ThunarxMenuItemPrivate
{
  gchar       *name;
  gchar       *label;
  gchar       *tooltip;
  gchar       *icon;
  gboolean     sensitive;
  gboolean     priority;
  ThunarxMenu *menu;
};



static guint signals[LAST_SIGNAL];



G_DEFINE_TYPE_WITH_PRIVATE (ThunarxMenuItem, thunarx_menu_item, G_TYPE_OBJECT)



static void
thunarx_menu_item_class_init (ThunarxMenuItemClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunarx_menu_item_finalize;
  gobject_class->get_property = thunarx_menu_item_get_property;
  gobject_class->set_property = thunarx_menu_item_set_property;

  signals[ACTIVATE] =
      g_signal_new ("activate",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (ThunarxMenuItemClass,
                                     activate),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Name of the item",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        "Label",
                                                        "Label to display to the user",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_TOOLTIP,
                                   g_param_spec_string ("tooltip",
                                                        "Tooltip",
                                                        "Tooltip for the menu item",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_ICON,
                                   g_param_spec_string ("icon",
                                                        "Icon",
                                                        "Textual representation of the icon (as returned "
                                                        "by g_icon_to_string()) to display in the menu item",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_SENSITIVE,
                                   g_param_spec_boolean ("sensitive",
                                                         "Sensitive",
                                                         "Whether the menu item is sensitive",
                                                         TRUE,
                                                         G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_PRIORITY,
                                   g_param_spec_boolean ("priority",
                                                         "Priority",
                                                         "Show priority text in toolbars",
                                                         TRUE,
                                                         G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_MENU,
                                   g_param_spec_object ("menu",
                                                        "Menu",
                                                        "The menu belonging to this item. May be null.",
                                                        THUNARX_TYPE_MENU,
                                                        G_PARAM_READWRITE));
}



static void
thunarx_menu_item_init (ThunarxMenuItem *item)
{
  item->priv = thunarx_menu_item_get_instance_private (item);
  item->priv->sensitive = TRUE;
  item->priv->priority = FALSE;
  item->priv->menu = NULL;
}



static void
thunarx_menu_item_get_property (GObject    *object,
                                guint       param_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ThunarxMenuItem *item = THUNARX_MENU_ITEM (object);

  switch (param_id)
    {
      case PROP_NAME:
        g_value_set_string (value, item->priv->name);
        break;

      case PROP_LABEL:
        g_value_set_string (value, item->priv->label);
        break;

      case PROP_TOOLTIP:
        g_value_set_string (value, item->priv->tooltip);
        break;

      case PROP_ICON:
        g_value_set_string (value, item->priv->icon);
        break;

      case PROP_SENSITIVE:
        g_value_set_boolean (value, item->priv->sensitive);
        break;

      case PROP_PRIORITY:
        g_value_set_boolean (value, item->priv->priority);
        break;

      case PROP_MENU:
        g_value_set_object (value, item->priv->menu);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
     }
}



static void
thunarx_menu_item_set_property (GObject      *object,
                                guint         param_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ThunarxMenuItem *item = THUNARX_MENU_ITEM (object);

  switch (param_id)
    {
      case PROP_NAME:
        g_free (item->priv->name);
        item->priv->name = g_strdup (g_value_get_string (value));
        g_object_notify (object, "name");
        break;

      case PROP_LABEL:
        g_free (item->priv->label);
        item->priv->label = g_strdup (g_value_get_string (value));
        g_object_notify (object, "label");
        break;

      case PROP_TOOLTIP:
        g_free (item->priv->tooltip);
        item->priv->tooltip = g_strdup (g_value_get_string (value));
        g_object_notify (object, "tooltip");
        break;

      case PROP_ICON:
        g_free (item->priv->icon);
        item->priv->icon = g_strdup (g_value_get_string (value));
        g_object_notify (object, "icon");
        break;

      case PROP_SENSITIVE:
        item->priv->sensitive = g_value_get_boolean (value);
        g_object_notify (object, "sensitive");
        break;

      case PROP_PRIORITY:
        item->priv->priority = g_value_get_boolean (value);
        g_object_notify (object, "priority");
        break;

      case PROP_MENU:
        if (item->priv->menu)
          g_object_unref (item->priv->menu);
        item->priv->menu = g_object_ref (g_value_get_object (value));
        g_object_notify (object, "menu");
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}



static void
thunarx_menu_item_finalize (GObject *object)
{
  ThunarxMenuItem *item = THUNARX_MENU_ITEM (object);

  g_free (item->priv->name);
  g_free (item->priv->label);
  g_free (item->priv->tooltip);
  g_free (item->priv->icon);

  if (item->priv->menu)
    g_object_unref (item->priv->menu);

  (*G_OBJECT_CLASS (thunarx_menu_item_parent_class)->finalize) (object);
}



/**
 * thunarx_menu_item_new:
 * @name: identifier for the menu item
 * @label: user-visible label of the menu item
 * @tooltip: tooltip of the menu item
 * @icon: textual representation of the icon to display in the menu
 *        item, as returned by g_icon_to_string(). A path or icon name
 *        are valid representations too.
 *
 * Creates a new menu item that can be added to the toolbar or to a contextual menu.
 *
 * Returns: a newly created #ThunarxMenuItem
 */
ThunarxMenuItem *
thunarx_menu_item_new (const gchar *name,
                       const gchar *label,
                       const gchar *tooltip,
                       const gchar *icon)
{
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (label != NULL, NULL);

  return g_object_new (THUNARX_TYPE_MENU_ITEM,
                       "name", name,
                       "label", label,
                       "tooltip", tooltip,
                       "icon", icon,
                       NULL);
}



/**
 * thunarx_menu_item_activate:
 * @item: pointer to a #ThunarxMenuItem instance
 *
 * Emits the activate signal.
 */
gboolean
thunarx_menu_item_activate (ThunarxMenuItem *item)
{
  g_signal_emit (item, signals[ACTIVATE], 0);
  return TRUE;
}



/**
 * thunarx_menu_item_get_sensitive:
 * @item: pointer to a #ThunarxMenuItem instance
 *
 * Returns whether the menu item is sensitive.
 */
gboolean
thunarx_menu_item_get_sensitive (ThunarxMenuItem *item)
{
  g_return_val_if_fail (THUNARX_IS_MENU_ITEM (item), FALSE);
  return item->priv->sensitive;
}



/**
 * thunarx_menu_item_set_sensitive:
 * @item: pointer to a #ThunarxMenuItem instance
 * @sensitive: %TRUE to make the menu item sensitive
 *
 * Sets the ::sensitive property of the menu item to @sensitive.
 */
void
thunarx_menu_item_set_sensitive (ThunarxMenuItem *item,
                                 gboolean         sensitive)
{
  g_return_if_fail (THUNARX_IS_MENU_ITEM (item));
  item->priv->sensitive = sensitive;
}



/**
 * thunarx_menu_item_set_menu:
 * @item: pointer to a #ThunarxMenuItem instance
 * @menu: pointer to a #ThunarxMenu instance
 *
 * Attaches @menu to menu item.
 */
void
thunarx_menu_item_set_menu (ThunarxMenuItem *item,
                            ThunarxMenu     *menu)
{
  g_return_if_fail (THUNARX_IS_MENU_ITEM (item));
  g_return_if_fail (THUNARX_IS_MENU (menu));

  g_object_set (item, "menu", THUNARX_MENU (menu), NULL);
}



/**
 * thunarx_menu_item_list_free:
 * @items: (element-type ThunarxMenuItem): a list of #ThunarxMenuItem
 */
void
thunarx_menu_item_list_free (GList *items)
{
  g_return_if_fail (items != NULL);

  g_list_foreach (items, (GFunc) (void (*)(void)) g_object_unref, NULL);
  g_list_free (items);
}
