/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006-2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __GARCON_MENU_H__
#define __GARCON_MENU_H__

#include <gio/gio.h>
#include <garcon/garcon-menu-item-pool.h>

G_BEGIN_DECLS

#define GARCON_TYPE_MENU            (garcon_menu_get_type ())
#define GARCON_MENU(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GARCON_TYPE_MENU, GarconMenu))
#define GARCON_MENU_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GARCON_TYPE_MENU, GarconMenuClass))
#define GARCON_IS_MENU(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GARCON_TYPE_MENU))
#define GARCON_IS_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GARCON_TYPE_MENU))
#define GARCON_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GARCON_TYPE_MENU, GarconMenuClass))

typedef struct _GarconMenuPrivate GarconMenuPrivate;
typedef struct _GarconMenuClass   GarconMenuClass;
typedef struct _GarconMenu        GarconMenu;

GType                garcon_menu_get_type           (void) G_GNUC_CONST;

GarconMenu          *garcon_menu_new                (GFile        *file) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
GarconMenu          *garcon_menu_new_for_path       (const gchar  *filename) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
GarconMenu          *garcon_menu_new_applications   (void) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gboolean             garcon_menu_load               (GarconMenu   *menu,
                                                     GCancellable *cancellable,
                                                     GError      **error);
GFile               *garcon_menu_get_file           (GarconMenu   *menu);
GarconMenuDirectory *garcon_menu_get_directory      (GarconMenu   *menu);
GList               *garcon_menu_get_menus          (GarconMenu   *menu);
void                 garcon_menu_add_menu           (GarconMenu   *menu,
                                                     GarconMenu   *submenu);
GarconMenu          *garcon_menu_get_menu_with_name (GarconMenu   *menu,
                                                     const gchar  *name);
GarconMenu          *garcon_menu_get_parent         (GarconMenu   *menu);
GarconMenuItemPool  *garcon_menu_get_item_pool      (GarconMenu   *menu);
GList               *garcon_menu_get_items          (GarconMenu   *menu);
GList               *garcon_menu_get_elements       (GarconMenu   *menu);

struct _GarconMenuClass
{
  GObjectClass __parent__;
};

struct _GarconMenu
{
  GObject              __parent__;

  /* < private > */
  GarconMenuPrivate *priv;
};

G_END_DECLS

#endif /* !__GARCON_MENU_H__ */
