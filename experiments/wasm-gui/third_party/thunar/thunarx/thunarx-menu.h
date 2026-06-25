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

#if !defined(THUNARX_INSIDE_THUNARX_H) && !defined(THUNARX_COMPILATION)
#error "Only <thunarx/thunarx.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __THUNARX_MENU_H__
#define __THUNARX_MENU_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* ThunarxMenu types */
typedef struct _ThunarxMenuPrivate ThunarxMenuPrivate;
typedef struct _ThunarxMenuClass   ThunarxMenuClass;
typedef struct _ThunarxMenu        ThunarxMenu;

/* ThunarxMenuItem types */
typedef struct _ThunarxMenuItemPrivate ThunarxMenuItemPrivate;
typedef struct _ThunarxMenuItemClass   ThunarxMenuItemClass;
typedef struct _ThunarxMenuItem        ThunarxMenuItem;




/* ThunarxMenu defines */
#define THUNARX_TYPE_MENU            (thunarx_menu_get_type ())
#define THUNARX_MENU(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNARX_TYPE_MENU, ThunarxMenu))
#define THUNARX_MENU_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNARX_TYPE_MENU, ThunarxMenuClass))
#define THUNARX_IS_MENU(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNARX_TYPE_MENU))
#define THUNARX_IS_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), THUNARX_TYPE_MENU))
#define THUNARX_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), THUNARX_TYPE_MENU))

/* ThunarxMenuItem defines */
#define THUNARX_TYPE_MENU_ITEM            (thunarx_menu_item_get_type ())
#define THUNARX_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNARX_TYPE_MENU_ITEM, ThunarxMenuItem))
#define THUNARX_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNARX_TYPE_MENU_ITEM, ThunarxMenuItemClass))
#define THUNARX_IS_MENU_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNARX_TYPE_MENU_ITEM))
#define THUNARX_IS_MENU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), THUNARX_TYPE_MENU_ITEM))
#define THUNARX_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), THUNARX_TYPE_MENU_ITEM))



/* ThunarxMenu structs */
struct _ThunarxMenuClass {
  GObjectClass __parent__;
};

struct _ThunarxMenu {
  GObject __parent__;
  ThunarxMenuPrivate *priv;
};

/* ThunarxMenuItem structs */
struct _ThunarxMenuItemClass {
  GObjectClass __parent__;
  void (*activate) (ThunarxMenuItem *item);
};

struct _ThunarxMenuItem {
  GObject __parent__;
  ThunarxMenuItemPrivate *priv;
};



/* ThunarxMenu methods */
GType         thunarx_menu_get_type       (void) G_GNUC_CONST;

ThunarxMenu  *thunarx_menu_new            (void) G_GNUC_MALLOC;

void          thunarx_menu_append_item    (ThunarxMenu     *menu,
                                           ThunarxMenuItem *item);

void          thunarx_menu_prepend_item   (ThunarxMenu     *menu,
                                           ThunarxMenuItem *item);

GList*        thunarx_menu_get_items      (ThunarxMenu     *menu);

/* ThunarxMenuItem methods */
GType             thunarx_menu_item_get_type      (void) G_GNUC_CONST;

ThunarxMenuItem  *thunarx_menu_item_new           (const gchar     *name,
                                                   const gchar     *label,
                                                   const gchar     *tooltip,
                                                   const gchar     *icon) G_GNUC_MALLOC;

gboolean          thunarx_menu_item_activate      (ThunarxMenuItem *item);

gboolean          thunarx_menu_item_get_sensitive (ThunarxMenuItem *item);
void              thunarx_menu_item_set_sensitive (ThunarxMenuItem *item,
                                                   gboolean         sensitive);

void              thunarx_menu_item_list_free     (GList           *items);

void              thunarx_menu_item_set_menu      (ThunarxMenuItem *item,
                                                   ThunarxMenu     *menu);

G_END_DECLS

#endif /* !__THUNARX_MENU_H__ */
