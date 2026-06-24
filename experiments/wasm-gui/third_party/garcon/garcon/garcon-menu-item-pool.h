/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006-2010 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __GARCON_MENU_ITEM_POOL_H__
#define __GARCON_MENU_ITEM_POOL_H__

#include <glib-object.h>
#include <garcon/garcon-menu-item.h>

G_BEGIN_DECLS

#define GARCON_TYPE_MENU_ITEM_POOL            (garcon_menu_item_pool_get_type ())
#define GARCON_MENU_ITEM_POOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GARCON_TYPE_MENU_ITEM_POOL, GarconMenuItemPool))
#define GARCON_MENU_ITEM_POOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GARCON_TYPE_MENU_ITEM_POOL, GarconMenuItemPoolClass))
#define GARCON_IS_MENU_ITEM_POOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GARCON_TYPE_MENU_ITEM_POOL))
#define GARCON_IS_MENU_ITEM_POOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GARCON_TYPE_MENU_ITEM_POOL))
#define GARCON_MENU_ITEM_POOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GARCON_TYPE_MENU_ITEM_POOL, GarconMenuItemPoolClass))

typedef struct _GarconMenuItemPoolPrivate GarconMenuItemPoolPrivate;
typedef struct _GarconMenuItemPoolClass   GarconMenuItemPoolClass;
typedef struct _GarconMenuItemPool        GarconMenuItemPool;

GType               garcon_menu_item_pool_get_type           (void) G_GNUC_CONST;

GarconMenuItemPool *garcon_menu_item_pool_new                (void) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

void                garcon_menu_item_pool_insert             (GarconMenuItemPool *pool,
                                                              GarconMenuItem     *item);
GarconMenuItem     *garcon_menu_item_pool_lookup             (GarconMenuItemPool *pool,
                                                              const gchar        *desktop_id);
GarconMenuItem     *garcon_menu_item_pool_lookup_file        (GarconMenuItemPool *pool,
                                                              GFile              *file);
void                garcon_menu_item_pool_foreach            (GarconMenuItemPool *pool,
                                                              GHFunc              func,
                                                              gpointer            user_data);
void                garcon_menu_item_pool_apply_exclude_rule (GarconMenuItemPool *pool,
                                                              GNode              *node);
gboolean            garcon_menu_item_pool_get_empty          (GarconMenuItemPool *pool);
void                garcon_menu_item_pool_clear              (GarconMenuItemPool *pool);

G_END_DECLS

#endif /* !__GARCON_MENU_ITEM_POOL_H__ */
