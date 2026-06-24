/* $Id$ */
/*-
 * vi:set et ai sts=2 sw=2 cindent:
 *
 * Copyright (c) 2007-2011 Jannis Pohlmann <jannis@xfce.org>
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
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <glib/gprintf.h>

#include <garcon/garcon.h>



static void
print_menu (GarconMenu  *menu,
            const gchar *path)
{
  GarconMenuDirectory *directory;
  GFile               *file;
  GList               *menus;
  GList               *items;
  GList               *iter;
  gchar               *name;
  gchar               *file_path;

  if (!garcon_menu_element_get_visible (GARCON_MENU_ELEMENT (menu)))
    return;

  /* Determine menu name */
  directory = garcon_menu_get_directory (menu);

  if (G_UNLIKELY (path == NULL))
    name = g_strdup ("");
  else
    {
      name = g_strdup_printf ("%s%s/", path,
                              (directory == NULL
                               ? garcon_menu_element_get_name (GARCON_MENU_ELEMENT (menu))
                               : garcon_menu_directory_get_name (directory)));
    }

  /* Fetch submenus */
  menus = garcon_menu_get_menus (menu);

  /* Print child menus */
  for (iter = menus; iter != NULL; iter = g_list_next (iter))
    {
      /* Only display menus which are not hidden or excluded from this environment */
      if (G_LIKELY (garcon_menu_element_get_visible (iter->data)))
        print_menu (GARCON_MENU (iter->data), name);
    }

  /* Free submenu list */
  g_list_free (menus);

  /* Fetch menu items */
  items = garcon_menu_get_elements (menu);

  /* Print menu items */
  for (iter = items; iter != NULL; iter = g_list_next (iter))
    {
      if (GARCON_IS_MENU_ITEM (iter->data)
          && garcon_menu_element_get_visible (iter->data))
        {
          file = garcon_menu_item_get_file (iter->data);
          file_path = g_file_get_path (file);

          g_printf ("%s\t%s\t%s\n", name, garcon_menu_item_get_desktop_id (iter->data),
                    file_path);

          g_free (file_path);
          g_object_unref (file);
        }
    }

  /* Free menu item list */
  g_list_free (items);

  /* Free name */
  g_free (name);
}



int
main (int    argc,
      char **argv)
{
  GarconMenu *menu;
  GError     *error = NULL;
#ifdef HAVE_STDLIB_H
  int         exit_code = EXIT_SUCCESS;
#else
  int         exit_code = 0;
#endif

  g_set_prgname ("test-menu-spec");

  /* Try to get the root menu */
  menu = garcon_menu_new_applications ();

  if (G_LIKELY (garcon_menu_load (menu, NULL, &error)))
    {
      /* Print menu contents according to the test suite criteria */
      print_menu (menu, NULL);
    }
  else
    {
      gchar *uri;

      uri = g_file_get_uri (garcon_menu_get_file (menu));
      g_error ("Could not load menu from %s: %s", uri, error->message);
      g_free (uri);

      g_error_free (error);
#ifdef HAVE_STDLIB_H
      exit_code = EXIT_FAILURE;
#else
      exit_code = -1;
#endif
    }

  return exit_code;
}
