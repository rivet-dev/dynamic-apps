/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#if !defined (_GARCON_INSIDE_GARCON_H) && !defined (GARCON_COMPILATION)
#error "Only <garcon/garcon.h> can be included directly. This file may disappear or change contents."
#endif

#ifndef __GARCON_MENU_TREE_PROVIDER_H__
#define __GARCON_MENU_TREE_PROVIDER_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define GARCON_TYPE_MENU_TREE_PROVIDER            (garcon_menu_tree_provider_get_type ())
#define GARCON_MENU_TREE_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GARCON_TYPE_MENU_TREE_PROVIDER, GarconMenuTreeProvider))
#define GARCON_IS_MENU_TREE_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GARCON_TYPE_MENU_TREE_PROVIDER))
#define GARCON_MENU_TREE_PROVIDER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GARCON_TYPE_MENU_TREE_PROVIDER, GarconMenuTreeProviderIface))

typedef struct _GarconMenuTreeProviderIface GarconMenuTreeProviderIface;
typedef struct _GarconMenuTreeProvider      GarconMenuTreeProvider;

GType  garcon_menu_tree_provider_get_type (void) G_GNUC_CONST;

GNode *garcon_menu_tree_provider_get_tree (GarconMenuTreeProvider *provider) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
GFile *garcon_menu_tree_provider_get_file (GarconMenuTreeProvider *provider) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

struct _GarconMenuTreeProviderIface
{
  GTypeInterface __parent__;

  /* Virtual methods */
  GNode       *(*get_tree) (GarconMenuTreeProvider *provider);
  GFile       *(*get_file) (GarconMenuTreeProvider *provider);
};

G_END_DECLS

#endif /* !__GARCON_MENU_TREE_PROVIDER_H__ */
