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

#include <libxfce4util/libxfce4util.h>

#include <thunarx/thunarx-preferences-provider.h>
#include <thunarx/thunarx-private.h>

/**
 * SECTION: thunarx-preferences-provider
 * @short_description: The interface to extensions that provide preferences
 * @title: ThunarxPreferencesProvider
 * @include: thunarx/thunarx.h
 *
 * The ThunarxPreferencesProvider interface is implemented by extensions that
 * want to register additional items in the preferences menu of the file
 * manager. In general this should only be done by extensions that are closely
 * tied to the file manager (for example, the <literal>thunar-uca</literal> is
 * such an extension, while an extension that just adds <guimenuitem>Compress
 * file</guimenuitem> and <guimenuitem>Uncompress file</guimenuitem> to the
 * context menu of compressed files should not add their own preferences to
 * the file manager menu, because it should use desktop-wide settings for
 * archive managers instead).
 *
 * The name of <link linkend="ThunarxMenuItem"><type>ThunarxMenuItem</type></link>s
 * returned from the thunarx_preferences_provider_get_menu items() method must be
 * namespaced with the model to avoid collision with internal file manager menu items
 * and menu items provided by other extensions. For example, the preferences menu item
 * provided by the <literal>thunar-uca</literal> extension is called
 * <literal>ThunarUca::manage-menu-items</literal>.
 */

GType
thunarx_preferences_provider_get_type (void)
{
  static gsize type__static = 0;
  GType        type;

  if (g_once_init_enter (&type__static))
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            I_("ThunarxPreferencesProvider"),
                                            sizeof (ThunarxPreferencesProviderIface),
                                            NULL,
                                            0,
                                            NULL,
                                            0);

      g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);

      g_once_init_leave (&type__static, type);
    }

  return type__static;
}



/**
 * thunarx_preferences_provider_get_menu_items: (skip)
 * @provider : a #ThunarxPreferencesProvider.
 * @window   : the #GtkWindow within which the menu items will be used.
 *
 * Returns the list of #ThunarxMenuItem<!---->s that @provider has to offer
 * as preferences within @window. These menu items will usually be added
 * to the builtin list of preferences in the "Edit" menu of the file
 * manager's @window.
 *
 * Plugin writers that implement this interface should make sure to
 * choose descriptive names and tooltips, and not to crowd the
 * "Edit" menu too much. That said, think twice before implementing
 * this interface, as too many preference menu items will render the
 * file manager useless over time!
 *
 * The caller is responsible to free the returned list of menu items using
 * something like this when no longer needed:
 * <informalexample><programlisting>
 * g_list_free_full (list, g_object_unref);
 * </programlisting></informalexample>
 *
 * Returns: (transfer full) (element-type ThunarxMenuItem): the list of
 *          #ThunarxMenuItem<!---->s that @provider has to offer as preferences
 *          within @window.
 **/
GList*
thunarx_preferences_provider_get_menu_items (ThunarxPreferencesProvider *provider,
                                             GtkWidget                  *window)
{
  GList *items;

  g_return_val_if_fail (THUNARX_IS_PREFERENCES_PROVIDER (provider), NULL);
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  if (THUNARX_PREFERENCES_PROVIDER_GET_IFACE (provider)->get_menu_items != NULL)
    {
      /* query the menu items from the implementation */
      items = (*THUNARX_PREFERENCES_PROVIDER_GET_IFACE (provider)->get_menu_items) (provider, window);

      /* take a reference on the provider for each menu item */
      thunarx_object_list_take_reference (items, provider);
    }
  else
    {
      items = NULL;
    }

  return items;
}
