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

 #if !defined(_GARCON_INSIDE_GARCON_H) && !defined(GARCON_COMPILATION)
#error "Only <garcon/garcon.h> can be included directly. This file may disappear or change contents."
#endif

#ifndef __GARCON_MENU_ITEM_ACTION_H__
#define __GARCON_MENU_ITEM_ACTION_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _GarconMenuItemActionPrivate GarconMenuItemActionPrivate;
typedef struct _GarconMenuItemActionClass   GarconMenuItemActionClass;
typedef struct _GarconMenuItemAction        GarconMenuItemAction;

#define GARCON_TYPE_MENU_ITEM_ACTION            (garcon_menu_item_action_get_type())
#define GARCON_MENU_ITEM_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GARCON_TYPE_MENU_ITEM_ACTION, GarconMenuItemAction))
#define GARCON_MENU_ITEM_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GARCON_TYPE_MENU_ITEM_ACTION, GarconMenuItemActionClass))
#define GARCON_IS_MENU_ITEM_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GARCON_TYPE_MENU_ITEM_ACTION))
#define GARCON_IS_MENU_ITEM_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GARCON_TYPE_MENU_ITEM_ACTION))
#define GARCON_MENU_ITEM_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GARCON_TYPE_MENU_ITEM_ACTION, GarconMenuItemActionClass))

struct _GarconMenuItemActionClass
{
  GObjectClass __parent__;

  /* signals */
  void (*changed) (GarconMenuItemAction *action);
};

struct _GarconMenuItemAction
{
  GObject                  __parent__;

  /* < private > */
  GarconMenuItemActionPrivate *priv;
};

GType                 garcon_menu_item_action_get_type           (void) G_GNUC_CONST;
GarconMenuItemAction *garcon_menu_item_action_new                (void) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

const gchar          *garcon_menu_item_action_get_command        (GarconMenuItemAction  *action);
void                  garcon_menu_item_action_set_command        (GarconMenuItemAction  *action,
                                                                  const gchar           *command);
const gchar          *garcon_menu_item_action_get_name           (GarconMenuItemAction  *action);
void                  garcon_menu_item_action_set_name           (GarconMenuItemAction  *action,
                                                                  const gchar           *name);
const gchar          *garcon_menu_item_action_get_icon_name      (GarconMenuItemAction  *action);
void                  garcon_menu_item_action_set_icon_name      (GarconMenuItemAction  *action,
                                                                  const gchar           *icon_name);
void                  garcon_menu_item_action_ref                (GarconMenuItemAction  *action);
void                  garcon_menu_item_action_unref              (GarconMenuItemAction  *action);

G_END_DECLS

#endif /* !__GARCON_MENU_ITEM_ACTION_H__ */
