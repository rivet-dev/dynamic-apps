/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2015 Danila Poyarkov <dannotemail@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <libxfce4util/libxfce4util.h>

#include <garcon/garcon-environment.h>
#include <garcon/garcon-menu-item-action.h>
#include <garcon/garcon-private.h>

/* Property identifiers */
enum
{
  PROP_0,
  PROP_NAME,
  PROP_COMMAND,
  PROP_ICON_NAME,
};

static void garcon_menu_item_action_finalize     (GObject       *object);
static void garcon_menu_item_action_get_property (GObject       *object,
                                                  guint          prop_id,
                                                  GValue        *value,
                                                  GParamSpec    *pspec);
static void garcon_menu_item_action_set_property (GObject       *object,
                                                  guint          prop_id,
                                                  const GValue  *value,
                                                  GParamSpec    *pspec);

struct _GarconMenuItemActionPrivate
{
  /* Name to be displayed for the action */
  gchar *name;

  /* Command to be executed when the action is clicked */
  gchar *command;

  /* Name of the icon associated with the action */
  gchar *icon_name;
};

G_DEFINE_TYPE_WITH_PRIVATE (GarconMenuItemAction, garcon_menu_item_action, G_TYPE_OBJECT)

static void
garcon_menu_item_action_class_init (GarconMenuItemActionClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = garcon_menu_item_action_finalize;
  gobject_class->get_property = garcon_menu_item_action_get_property;
  gobject_class->set_property = garcon_menu_item_action_set_property;

  /**
   * GarconMenuItemAction:name:
   *
   * Name of the application action (will be displayed in menus etc.).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Name of the action",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItemAction:command:
   *
   * Command to be executed when the application action is clicked.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COMMAND,
                                   g_param_spec_string ("command",
                                                        "Command",
                                                        "Application command",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItemAction:icon-name:
   *
   * Name of the custom icon associated with this action.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name",
                                                        "icon-name",
                                                        "Custom icon name",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
garcon_menu_item_action_init (GarconMenuItemAction *action)
{
  action->priv = garcon_menu_item_action_get_instance_private (action);
}



static void
garcon_menu_item_action_finalize (GObject *object)
{
  GarconMenuItemAction *action = GARCON_MENU_ITEM_ACTION (object);

  g_free (action->priv->name);
  g_free (action->priv->command);
  g_free (action->priv->icon_name);
  
  (*G_OBJECT_CLASS (garcon_menu_item_action_parent_class)->finalize) (object);
}



static void
garcon_menu_item_action_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GarconMenuItemAction *action = GARCON_MENU_ITEM_ACTION (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, garcon_menu_item_action_get_name (action));
      break;

    case PROP_COMMAND:
      g_value_set_string (value, garcon_menu_item_action_get_command (action));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, garcon_menu_item_action_get_icon_name (action));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
garcon_menu_item_action_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GarconMenuItemAction *action = GARCON_MENU_ITEM_ACTION (object);

  switch (prop_id)
    {
    case PROP_NAME:
      garcon_menu_item_action_set_name (action, g_value_get_string (value));
      break;

    case PROP_COMMAND:
      garcon_menu_item_action_set_command (action, g_value_get_string (value));
      break;

    case PROP_ICON_NAME:
      garcon_menu_item_action_set_icon_name (action, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


const gchar*
garcon_menu_item_action_get_name (GarconMenuItemAction *action)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM_ACTION (action), NULL);
  return action->priv->name;
}



void
garcon_menu_item_action_set_name (GarconMenuItemAction *action,
                                  const gchar          *name)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM_ACTION (action));
  g_return_if_fail (g_utf8_validate (name, -1, NULL));

  /* Abort if old and new name are equal */
  if (g_strcmp0 (action->priv->name, name) == 0)
    return;

  /* Assign new name */
  g_free (action->priv->name);
  action->priv->name = g_strdup (name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (action), "name");
}



const gchar*
garcon_menu_item_action_get_icon_name (GarconMenuItemAction  *action)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM_ACTION (action), NULL);
  return action->priv->icon_name;
}



void
garcon_menu_item_action_set_icon_name (GarconMenuItemAction  *action,
                                       const gchar           *icon_name)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM_ACTION (action));

  /* Abort if old and new name are equal */
  if (g_strcmp0 (action->priv->icon_name, icon_name) == 0)
    return;

  /* Assign new name */
  g_free (action->priv->icon_name);
  if (icon_name != NULL)
    {
      action->priv->icon_name = g_strdup (icon_name);
    }
  else
    {
      action->priv->icon_name = NULL;
    }

  /* Notify listeners */
  g_object_notify (G_OBJECT (action), "icon-name");
}



const gchar*
garcon_menu_item_action_get_command (GarconMenuItemAction *action)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM_ACTION (action), NULL);
  return action->priv->command;
}



void
garcon_menu_item_action_set_command (GarconMenuItemAction *action,
                                     const gchar          *command)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM_ACTION (action));
  g_return_if_fail (command != NULL);

  /* Abort if old and new command are equal */
  if (g_strcmp0 (action->priv->command, command) == 0)
    return;

  /* Assign new command */
  g_free (action->priv->command);
  action->priv->command = g_strdup (command);

  /* Notify listeners */
  g_object_notify (G_OBJECT (action), "command");
}

void
garcon_menu_item_action_ref (GarconMenuItemAction *action)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM_ACTION (action));

  /* Grab a reference on the object */
  g_object_ref (G_OBJECT (action));
}



void
garcon_menu_item_action_unref (GarconMenuItemAction *action)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM_ACTION (action));

  /* Decrement the reference counter */
  g_object_unref (G_OBJECT (action));
}
