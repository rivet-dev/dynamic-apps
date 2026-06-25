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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <libxfce4util/libxfce4util.h>

#include <thunarx/thunarx-private.h>
#include <thunarx/thunarx-provider-plugin.h>



static void thunarx_provider_plugin_class_init (gpointer klass);

/**
 * SECTION: thunarx-provider-plugin
 * @short_description: The interface to the plugin type registration
 * @title: ThunarxProviderPlugin
 * @include: thunarx/thunarx.h
 *
 * #ThunarxProviderPlugin is the interface to the file manager's type plugin loader implementation. It
 * provides methods to extensions - also called <emphasis>provider plugins</emphasis> - to register dynamic
 * types and control certain aspects of the plugin (see thunarx_provider_plugin_set_resident()).
 */

GType
thunarx_provider_plugin_get_type (void)
{
  static gsize type__static = 0;
  GType        type;

  if (g_once_init_enter (&type__static))
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            I_("ThunarxProviderPlugin"),
                                            sizeof (ThunarxProviderPluginIface),
                                            (GClassInitFunc) (void (*)(void)) thunarx_provider_plugin_class_init,
                                            0,
                                            NULL,
                                            0);

      g_once_init_leave (&type__static, type);
    }

  return type__static;
}



static void
thunarx_provider_plugin_class_init (gpointer klass)
{
  /**
   * ThunarxProviderPlugin:resident:
   *
   * Tells whether a plugin must reside in memory once loaded for
   * the first time. See thunarx_provider_plugin_get_resident() and
   * thunarx_provider_plugin_set_resident() for more details.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_boolean ("resident",
                                                             _("Resident"),
                                                             _("Don't unload the plugin from memory"),
                                                             FALSE,
                                                             G_PARAM_READWRITE));
}



/**
 * thunarx_provider_plugin_get_resident:
 * @plugin : a #ThunarxProviderPlugin.
 *
 * Determines whether the application is allowed to unload @plugin
 * from memory when no longer needed and reload it on demand. If
 * %FALSE is returned, then the application may unload @plugin, else
 * if %TRUE is returned the application will take care that @plugin
 * is never unloaded from memory during the lifetime of the application.
 *
 * Return value: %TRUE if @plugin will be kept in memory once loaded
 *               for the first time.
 **/
gboolean
thunarx_provider_plugin_get_resident (const ThunarxProviderPlugin *plugin)
{
  g_return_val_if_fail (THUNARX_IS_PROVIDER_PLUGIN (plugin), FALSE);

  return (*THUNARX_PROVIDER_PLUGIN_GET_IFACE (plugin)->get_resident) (plugin);
}



/**
 * thunarx_provider_plugin_set_resident:
 * @plugin   : a #ThunarxProviderPlugin.
 * @resident : %TRUE to make @plugin resident in memory.
 *
 * This method is used to instruct the application that @plugin must be
 * kept in memory during the lifetime of the application. The default
 * is to allow the application to unload @plugin from the memory when
 * no longer needed. If this method is invoked with a @resident value
 * of %TRUE then the application will never try to unload @plugin.
 *
 * This method has no effect unless called from the
 * %thunar_extension_initialize method of the @plugin.
 **/
void
thunarx_provider_plugin_set_resident (ThunarxProviderPlugin *plugin,
                                      gboolean               resident)
{
  g_return_if_fail (THUNARX_IS_PROVIDER_PLUGIN (plugin));

  (*THUNARX_PROVIDER_PLUGIN_GET_IFACE (plugin)->set_resident) (plugin, resident);
}



/**
 * thunarx_provider_plugin_register_type:
 * @plugin      : a #ThunarxProviderPlugin.
 * @type_parent : the type for the parent class.
 * @type_name   : name for the type.
 * @type_info   : type information structure.
 * @type_flags  : flags field providing details about the type.
 *
 * Looks up or registers a type that is implemented with a particular type @plugin. If a type with name @type_name
 * was previously registered, the #GType identifier for the type is returned, otherwise the type is newly registered,
 * and the resulting #GType identifier returned.
 *
 * When reregistering a type (typically because a module is unloaded then reloaded, and reinitialized), module and
 * @type_parent must be the same as they were previously.
 *
 * Return value: the new or existing type id.
 **/
GType
thunarx_provider_plugin_register_type (ThunarxProviderPlugin *plugin,
                                       GType                  type_parent,
                                       const gchar           *type_name,
                                       const GTypeInfo       *type_info,
                                       GTypeFlags             type_flags)
{
  g_return_val_if_fail (THUNARX_IS_PROVIDER_PLUGIN (plugin), G_TYPE_INVALID);
  g_return_val_if_fail (G_TYPE_IS_DERIVABLE (type_parent), G_TYPE_INVALID);
  g_return_val_if_fail (type_name != NULL && *type_name != '\0', G_TYPE_INVALID);
  g_return_val_if_fail (type_info != NULL, G_TYPE_INVALID);

  return (*THUNARX_PROVIDER_PLUGIN_GET_IFACE (plugin)->register_type) (plugin, type_parent, type_name, type_info, type_flags);
}



/**
 * thunarx_provider_plugin_add_interface:
 * @plugin         : a #ThunarxProviderPlugin.
 * @instance_type  : type to which to add the interface.
 * @interface_type : interface type to add.
 * @interface_info : type information structure.
 *
 * Registers an additional interface for a type, whose interface lives in the given type @plugin.
 * If the interface was already registered for the type in this @plugin, nothing will be done.
 *
 * As long as any instances of the type exist, the type @plugin will not be unloaded.
 **/
void
thunarx_provider_plugin_add_interface (ThunarxProviderPlugin *plugin,
                                       GType                  instance_type,
                                       GType                  interface_type,
                                       const GInterfaceInfo  *interface_info)
{
  g_return_if_fail (THUNARX_IS_PROVIDER_PLUGIN (plugin));
  g_return_if_fail (G_TYPE_IS_INTERFACE (interface_type));
  g_return_if_fail (interface_info != NULL);

  (*THUNARX_PROVIDER_PLUGIN_GET_IFACE (plugin)->add_interface) (plugin, instance_type, interface_type, interface_info);
}



/**
 * thunarx_provider_plugin_register_enum:
 * @plugin              : a #ThunarxProviderPlugin.
 * @name                : the name for the type.
 * @const_static_values : an array of #GEnumValue structs for the possible enumeration values.
 *                        The array is terminated by a struct with all members being %0.
 *
 * Looks up or registers an enumeration that is implemented with a particular type @plugin. If a type
 * with name @name was previously registered, the #GType identifier for the type is returned,
 * otherwise the type is newly registered, and the resulting #GType identifier returned.
 *
 * As long as any instances of the type exist, the type @plugin will not be unloaded.
 *
 * Return value: the new or existing type id.
 **/
GType
thunarx_provider_plugin_register_enum (ThunarxProviderPlugin *plugin,
                                       const gchar           *name,
                                       const GEnumValue      *const_static_values)
{
  g_return_val_if_fail (THUNARX_IS_PROVIDER_PLUGIN (plugin), G_TYPE_INVALID);
  g_return_val_if_fail (name != NULL && *name != '\0', G_TYPE_INVALID);
  g_return_val_if_fail (const_static_values != NULL, G_TYPE_INVALID);

  return (*THUNARX_PROVIDER_PLUGIN_GET_IFACE (plugin)->register_enum) (plugin, name, const_static_values);
}



/**
 * thunarx_provider_plugin_register_flags:
 * @plugin              : a #ThunarxProviderPlugin.
 * @name                : name for the type.
 * @const_static_values : an array of #GFlagsValue structs for the possible flags values.
 *                        The array is terminated by a struct with all members being %0.
 *
 * Looks up or registers a flags type that is implemented with a particular type @plugin. If a type with name
 * qname was previously registered, the #GType identifier for the type is returned, otherwise the type is newly
 * registered, and the resulting #GType identifier returned.
 *
 * As long as any instances of the type exist, the type @plugin will not be unloaded.
 *
 * Return value: the new or existing type id.
 **/
GType
thunarx_provider_plugin_register_flags (ThunarxProviderPlugin *plugin,
                                        const gchar           *name,
                                        const GFlagsValue     *const_static_values)
{
  g_return_val_if_fail (THUNARX_IS_PROVIDER_PLUGIN (plugin), G_TYPE_INVALID);
  g_return_val_if_fail (name != NULL && *name != '\0', G_TYPE_INVALID);
  g_return_val_if_fail (const_static_values != NULL, G_TYPE_INVALID);

  return (*THUNARX_PROVIDER_PLUGIN_GET_IFACE (plugin)->register_flags) (plugin, name, const_static_values);
}
