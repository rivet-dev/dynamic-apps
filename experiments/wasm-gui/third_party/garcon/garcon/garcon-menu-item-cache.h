/* $Id$ */
/* vi:set expandtab sw=2 sts=2: */
/*-
 * Copyright (c) 2006-2007 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __GARCON_MENU_ITEM_CACHE_H__
#define __GARCON_MENU_ITEM_CACHE_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GarconMenuItemCachePrivate GarconMenuItemCachePrivate;
typedef struct _GarconMenuItemCacheClass   GarconMenuItemCacheClass;
typedef struct _GarconMenuItemCache        GarconMenuItemCache;

#define GARCON_TYPE_MENU_ITEM_CACHE            (garcon_menu_item_cache_get_type ())
#define GARCON_MENU_ITEM_CACHE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GARCON_TYPE_MENU_ITEM_CACHE, GarconMenuItemCache))
#define GARCON_MENU_ITEM_CACHE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GARCON_TYPE_MENU_ITEM_CACHE, GarconMenuItemCacheClass))
#define GARCON_IS_MENU_ITEM_CACHE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GARCON_TYPE_MENU_ITEM_CACHE))
#define GARCON_IS_MENU_ITEM_CACHE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GARCON_TYPE_MENU_ITEM_CACHE))
#define GARCON_MENU_ITEM_CACHE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GARCON_TYPE_MENU_ITEM_CACHE, GarconMenuItemCacheClass))

struct _GarconMenuItemCacheClass
{
  GObjectClass __parent__;
};

struct _GarconMenuItemCache
{
  GObject                     __parent__;

  /* Private data */
  GarconMenuItemCachePrivate *priv;
};



GType                garcon_menu_item_cache_get_type        (void) G_GNUC_CONST;

GarconMenuItemCache *garcon_menu_item_cache_get_default     (void);

GarconMenuItem      *garcon_menu_item_cache_lookup          (GarconMenuItemCache *cache,
                                                             const gchar         *uri,
                                                             const gchar         *desktop_id);
void                 garcon_menu_item_cache_foreach         (GarconMenuItemCache *cache,
                                                             GHFunc               func,
                                                             gpointer             user_data);
void                 garcon_menu_item_cache_invalidate      (GarconMenuItemCache *cache);
void                 garcon_menu_item_cache_invalidate_file (GarconMenuItemCache *cache,
                                                             GFile               *file);

G_END_DECLS

#endif /* !__GARCON_MENU_ITEM_CACHE_H__ */
