/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunarx/thunarx-menu-provider.h>
#include <thunarx/thunarx-private.h>


/**
 * SECTION: thunarx-menu-provider
 * @short_description: The interface to extensions that provide additional menu items
 * @title: ThunarxMenuProvider
 * @include: thunarx/thunarx.h
 *
 * The <interface>ThunarxMenuProvider</interface> interface is implemented by
 * extensions which provide additional menu items that should be displayed in
 * the file or folder context menus for certain types of files.
 *
 * Extensions that implement this interface should do its best to respond as
 * fast as possible to method invokations by the file manager. That said, when
 * the file manager calls the thunarx_menu_provider_get_file_menu_items() or the
 * thunarx_menu_provider_get_folder_menu_items() method, the implementation in the
 * extension should use only the thunarx_file_info_has_mime_type() and
 * thunarx_file_info_is_directory() methods to determine the menu items that should
 * be added to the file manager's context menu. Don't perform any complicated I/O
 * to determine the menu items list, as that would block the whole file manager process.
 *
 * The name of <link linkend="ThunarxMenuItem">ThunarxMenuItem</link>s returned from the
 * thunarx_menu_provider_get_file_menu_items() and thunarx_menu_provider_get_folder_menu_items()
 * methods must be namespaced with the module to avoid collision with internal file manager
 * menu items and menu items provided by other extensions. For example, the menu item provided
 * by the <systemitem class="library">ThunarOpenTerminal</systemitem> extension should be
 * called <literal>ThunarOpenTerminal::open-terminal</literal>.
 */

GType
thunarx_menu_provider_get_type (void)
{
  static gsize type__static = 0;
  GType        type;

  if (g_once_init_enter (&type__static))
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            I_("ThunarxMenuProvider"),
                                            sizeof (ThunarxMenuProviderIface),
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
 * thunarx_menu_provider_get_file_menu_items: (skip)
 * @provider: a #ThunarxMenuProvider.
 * @window: the #GtkWindow within which the menu items will be used.
 * @files: (element-type ThunarxFileInfo): the list of #ThunarxFileInfo<!---->s
 *         to which the menu items will be applied.
 *
 * Returns the list of #ThunarxMenuItem<!---->s that @provider has to offer for
 * @files.
 *
 * As a special note, this method automatically takes a reference on the
 * @provider for every #ThunarxMenuItem object returned from the real implementation
 * of this method in @provider. This is to make sure that the extension stays
 * in memory for at least the time that the menu items are used. If the extension
 * wants to stay in memory for a longer time, it'll need to take care of this
 * itself (e.g. by taking an additional reference on the @provider itself,
 * that's released at a later time).
 *
 * The caller is responsible to free the returned list of menu items using
 * something like this when no longer needed:
 * <informalexample><programlisting>
 * g_list_free_full (list, g_object_unref);
 * </programlisting></informalexample>
 *
 * Returns: (transfer full) (element-type ThunarxMenuItem): the list of #ThunarxMenuItem<!---->s
 *          that @provider has to offer for @files.
 **/
GList*
thunarx_menu_provider_get_file_menu_items (ThunarxMenuProvider *provider,
                                           GtkWidget           *window,
                                           GList               *files)
{
  GList *items;

  g_return_val_if_fail (THUNARX_IS_MENU_PROVIDER (provider), NULL);
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (files != NULL, NULL);

  if (THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_file_menu_items != NULL)
    {
      /* query the menu items from the implementation */
      items = (*THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_file_menu_items) (provider, window, files);

      /* take a reference on the provider for each menu item */
      thunarx_object_list_take_reference (items, provider);
    }
  else
    {
      items = NULL;
    }

  return items;
}



/**
 * thunarx_menu_provider_get_folder_menu_items: (skip)
 * @provider : a #ThunarxMenuProvider.
 * @window   : the #GtkWindow within which the menu items will be used.
 * @folder   : the folder to which the menu items should will be applied.
 *
 * Returns the list of #ThunarxMenuItem<!---->s that @provider has to offer for
 * @folder.
 *
 * As a special note, this method automatically takes a reference on the
 * @provider for every #ThunarxMenuItem object returned from the real implementation
 * of this method in @provider. This is to make sure that the extension stays
 * in memory for at least the time that the menu items are used. If the extension
 * wants to stay in memory for a longer time, it'll need to take care of this
 * itself (e.g. by taking an additional reference on the @provider itself,
 * that's released at a later time).
 *
 * The caller is responsible to free the returned list of menu items using
 * something like this when no longer needed:
 * <informalexample><programlisting>
 * g_list_free_full (list, g_object_unref);
 * </programlisting></informalexample>
 *
 * Returns: (transfer full) (element-type ThunarxMenuItem): the list of #ThunarxMenuItem<!---->s
 *          that @provider has to offer for @folder.
 **/
GList*
thunarx_menu_provider_get_folder_menu_items (ThunarxMenuProvider *provider,
                                             GtkWidget           *window,
                                             ThunarxFileInfo     *folder)
{
  GList *items;

  g_return_val_if_fail (THUNARX_IS_MENU_PROVIDER (provider), NULL);
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (THUNARX_IS_FILE_INFO (folder), NULL);
  g_return_val_if_fail (thunarx_file_info_is_directory (folder), NULL);

  if (THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_folder_menu_items != NULL)
    {
      /* query the menu items from the implementation */
      items = (*THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_folder_menu_items) (provider, window, folder);

      /* take a reference on the provider for each menu item */
      thunarx_object_list_take_reference (items, provider);
    }
  else
    {
      items = NULL;
    }

  return items;
}



/**
 * thunarx_menu_provider_get_dnd_menu_items: (skip)
 * @provider: a #ThunarxMenuProvider.
 * @window: the #GtkWindow within which the menu items will be used.
 * @folder: the folder into which the @files are being dropped
 * @files: (element-type ThunarxFileInfo): the list of #ThunarxFileInfo<!---->s
 *         for the files that are being dropped to @folder in @window.
 *
 * Returns the list of #ThunarxMenuItem<!---->s that @provider has to offer for
 * dropping the @files into the @folder. For example, the thunar-archive-plugin
 * provides <guilabel>Extract Here</guilabel> menu items when dropping archive
 * files into a folder that is writable by the user.
 *
 * As a special note, this method automatically takes a reference on the
 * @provider for every #ThunarxMenuItem object returned from the real implementation
 * of this method in @provider. This is to make sure that the extension stays
 * in memory for at least the time that the menu items are used. If the extension
 * wants to stay in memory for a longer time, it'll need to take care of this
 * itself (e.g. by taking an additional reference on the @provider itself,
 * that's released at a later time).
 *
 * The caller is responsible to free the returned list of menu items using
 * something like this when no longer needed:
 * <informalexample><programlisting>
 * g_list_free_full (list, g_object_unref);
 * </programlisting></informalexample>
 *
 * Returns: (transfer full) (element-type ThunarxMenuItem): the list of #ThunarxMenuItem<!---->s
 *          that @provider has to offer for dropping @files to @folder.
 *
 * Since: 0.4.1
 **/
GList*
thunarx_menu_provider_get_dnd_menu_items (ThunarxMenuProvider *provider,
                                          GtkWidget           *window,
                                          ThunarxFileInfo     *folder,
                                          GList               *files)
{
  GList *items;

  g_return_val_if_fail (THUNARX_IS_MENU_PROVIDER (provider), NULL);
  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (THUNARX_IS_FILE_INFO (folder), NULL);
  g_return_val_if_fail (thunarx_file_info_is_directory (folder), NULL);
  g_return_val_if_fail (files != NULL, NULL);

  if (THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_dnd_menu_items != NULL)
    {
      /* query the menu items from the implementation */
      items = (*THUNARX_MENU_PROVIDER_GET_IFACE (provider)->get_dnd_menu_items) (provider, window, folder, files);

      /* take a reference on the provider for each menu item */
      thunarx_object_list_take_reference (items, provider);
    }
  else
    {
      items = NULL;
    }

  return items;
}
