/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(THUNARX_INSIDE_THUNARX_H) && !defined(THUNARX_COMPILATION)
#error "Only <thunarx/thunarx.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __THUNARX_PREFERENCES_PROVIDER_H__
#define __THUNARX_PREFERENCES_PROVIDER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _ThunarxPreferencesProviderIface ThunarxPreferencesProviderIface;
typedef struct _ThunarxPreferencesProvider      ThunarxPreferencesProvider;

#define THUNARX_TYPE_PREFERENCES_PROVIDER           (thunarx_preferences_provider_get_type ())
#define THUNARX_PREFERENCES_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNARX_TYPE_PREFERENCES_PROVIDER, ThunarxPreferencesProvider))
#define THUNARX_IS_PREFERENCES_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNARX_TYPE_PREFERENCES_PROVIDER))
#define THUNARX_PREFERENCES_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), THUNARX_TYPE_PREFERENCES_PROVIDER, ThunarxPreferencesProviderIface))

/**
 * ThunarxPreferencesProviderIface:
 * @get_menu_items: See thunarx_preferences_provider_get_menu_items().
 *
 * Interface with virtual methods implementation by extensions that
 * want to install preferences items in the file managers menu.
 * Providers don't need to implement all of the virtual methods listed
 * in the interface.
 */

struct _ThunarxPreferencesProviderIface
{
  /*< private >*/
  GTypeInterface __parent__;

  /*< public >*/
  GList *(*get_menu_items) (ThunarxPreferencesProvider *provider,
                            GtkWidget                  *window);

  /*< private >*/
  void (*reserved1) (void);
  void (*reserved2) (void);
  void (*reserved3) (void);
  void (*reserved4) (void);
  void (*reserved5) (void);
};

GType  thunarx_preferences_provider_get_type    (void) G_GNUC_CONST;

GList *thunarx_preferences_provider_get_menu_items (ThunarxPreferencesProvider *provider,
                                                    GtkWidget                  *window);

G_END_DECLS

#endif /* !__THUNARX_PREFERENCES_PROVIDER_H__ */
