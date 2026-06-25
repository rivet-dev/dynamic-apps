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

#ifndef __THUNARX_PROVIDER_PLUGIN_H__
#define __THUNARX_PROVIDER_PLUGIN_H__

#include <glib-object.h>

typedef struct _ThunarxProviderPluginIface ThunarxProviderPluginIface;
typedef struct _ThunarxProviderPlugin      ThunarxProviderPlugin;

#define THUNARX_TYPE_PROVIDER_PLUGIN           (thunarx_provider_plugin_get_type ())
#define THUNARX_PROVIDER_PLUGIN(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNARX_TYPE_PROVIDER_PLUGIN, ThunarxProviderPlugin))
#define THUNARX_IS_PROVIDER_PLUGIN(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNARX_TYPE_PROVIDER_PLUGIN))
#define THUNARX_PROVIDER_PLUGIN_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), THUNARX_TYPE_PROVIDER_PLUGIN, ThunarxProviderPluginIface))

/**
 * ThunarxProviderPluginIface:
 * @get_resident: see thunarx_provider_plugin_get_resident().
 * @set_resident: see thunarx_provider_plugin_set_resident().
 * @register_type: see thunarx_provider_plugin_register_type().
 * @add_interface: see thunarx_provider_plugin_add_interface().
 * @register_enum: see thunarx_provider_plugin_register_enum().
 * @register_flags: see thunarx_provider_plugin_register_flags().
 *
 * Interface with virtual methods implemented by the file manager and
 * accessible from the extensions.
 */

struct _ThunarxProviderPluginIface
{
  /*< private >*/
  GTypeInterface __parent__;

  /*< public >*/
  gboolean (*get_resident)    (const ThunarxProviderPlugin *plugin);
  void     (*set_resident)    (ThunarxProviderPlugin       *plugin,
                               gboolean                     resident);

  GType    (*register_type)   (ThunarxProviderPlugin       *plugin,
                               GType                        type_parent,
                               const gchar                 *type_name,
                               const GTypeInfo             *type_info,
                               GTypeFlags                   type_flags);
  void     (*add_interface)   (ThunarxProviderPlugin       *plugin,
                               GType                        instance_type,
                               GType                        interface_type,
                               const GInterfaceInfo        *interface_info);
  GType    (*register_enum)   (ThunarxProviderPlugin       *plugin,
                               const gchar                 *name,
                               const GEnumValue            *const_static_values);
  GType    (*register_flags)  (ThunarxProviderPlugin       *plugin,
                               const gchar                 *name,
                               const GFlagsValue           *const_static_values);

  /*< private >*/
  void (*reserved1) (void);
  void (*reserved2) (void);
  void (*reserved3) (void);
  void (*reserved4) (void);
};

GType     thunarx_provider_plugin_get_type       (void) G_GNUC_CONST;

gboolean  thunarx_provider_plugin_get_resident   (const ThunarxProviderPlugin *plugin);
void      thunarx_provider_plugin_set_resident   (ThunarxProviderPlugin       *plugin,
                                                  gboolean                     resident);

GType     thunarx_provider_plugin_register_type  (ThunarxProviderPlugin *plugin,
                                                  GType                  type_parent,
                                                  const gchar           *type_name,
                                                  const GTypeInfo       *type_info,
                                                  GTypeFlags             type_flags);
void      thunarx_provider_plugin_add_interface  (ThunarxProviderPlugin *plugin,
                                                  GType                  instance_type,
                                                  GType                  interface_type,
                                                  const GInterfaceInfo  *interface_info);
GType     thunarx_provider_plugin_register_enum  (ThunarxProviderPlugin *plugin,
                                                  const gchar           *name,
                                                  const GEnumValue      *const_static_values);
GType     thunarx_provider_plugin_register_flags (ThunarxProviderPlugin *plugin,
                                                  const gchar           *name,
                                                  const GFlagsValue     *const_static_values);

#endif /* !__THUNARX_PROVIDER_PLUGIN_H__ */
