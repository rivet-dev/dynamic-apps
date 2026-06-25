/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <thunarx/thunarx.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-dnd.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-private.h>



static void
dnd_action_selected (GtkWidget     *item,
                     GdkDragAction *dnd_action_return)
{
  *dnd_action_return = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (item), "dnd-action"));
}



/**
 * thunar_dnd_ask:
 * @widget    : the widget on which the drop was performed.
 * @folder    : the #ThunarFile to which the @path_list is being dropped.
 * @path_list : the #GFile<!---->s of the files being dropped to @file.
 * @actions   : the list of actions supported for the drop.
 *
 * Pops up a menu that asks the user to choose one of the
 * @actions or to cancel the drop. If the user chooses a
 * valid #GdkDragAction from @actions, then this action is
 * returned. Else if the user cancels the drop, 0 will be
 * returned.
 *
 * This method can be used to implement a response to the
 * #GDK_ACTION_ASK action on drops.
 *
 * Return value: the selected #GdkDragAction or 0 to cancel.
 **/
GdkDragAction
thunar_dnd_ask (GtkWidget    *widget,
                ThunarFile   *folder,
                GList        *path_list,
                GdkDragAction dnd_actions)
{
  static const GdkDragAction dnd_action_items[] = { GDK_ACTION_COPY, GDK_ACTION_MOVE, GDK_ACTION_LINK };
  static const gchar        *dnd_action_names[] = { N_ ("_Copy here"), N_ ("_Move here"), N_ ("_Link here") };
  static const gchar        *dnd_action_icons[] = { "stock_folder-copy", "stock_folder-move", NULL };

  ThunarxProviderFactory *factory;
  GdkDragAction           dnd_action = 0;
  ThunarFile             *file;
  GtkWidget              *window;
  GtkWidget              *image;
  GtkWidget              *menu;
  GtkWidget              *item;
  GList                  *file_list = NULL;
  GList                  *providers = NULL;
  GList                  *items = NULL;
  GList                  *lp;
  guint                   n;

  _thunar_return_val_if_fail (thunar_file_is_directory (folder), 0);
  _thunar_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  /* connect to the provider factory */
  factory = thunarx_provider_factory_get_default ();

  /* prepare the popup menu */
  menu = gtk_menu_new ();

  /* append the various items */
  for (n = 0; n < G_N_ELEMENTS (dnd_action_items); ++n)
    if (G_LIKELY ((dnd_actions & dnd_action_items[n]) != 0))
      {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        item = gtk_image_menu_item_new_with_mnemonic (_(dnd_action_names[n]));
G_GNUC_END_IGNORE_DEPRECATIONS
        g_object_set_data (G_OBJECT (item), I_("dnd-action"), GUINT_TO_POINTER (dnd_action_items[n]));
        g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (dnd_action_selected), &dnd_action);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        gtk_widget_show (item);

        /* add image to the menu item */
        if (G_LIKELY (dnd_action_icons[n] != NULL))
          {
            image = gtk_image_new_from_icon_name (dnd_action_icons[n], GTK_ICON_SIZE_MENU);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
G_GNUC_END_IGNORE_DEPRECATIONS
          }
      }

  /* append the separator */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* determine the toplevel window the widget belongs to */
  window = gtk_widget_get_toplevel (widget);
  if (G_LIKELY (window != NULL && gtk_widget_get_toplevel (window)))
    {
      /* check if we can resolve all paths */
      for (lp = path_list; lp != NULL; lp = lp->next)
        {
          /* try to resolve this path */
          file = thunar_file_cache_lookup (lp->data);
          if (G_LIKELY (file != NULL))
            file_list = g_list_prepend (file_list, file);
          else
            break;
        }

      /* check if we resolved all paths (and have atleast one file) */
      if (G_LIKELY (file_list != NULL && lp == NULL))
        {
          /* load the menu providers from the provider factory */
          providers = thunarx_provider_factory_list_providers (factory, THUNARX_TYPE_MENU_PROVIDER);

          /* load the dnd menu items offered by the menu providers */
          for (lp = providers; lp != NULL; lp = lp->next)
            {
              /* merge the menu items from this provider */
              items = g_list_concat (items, thunarx_menu_provider_get_dnd_menu_items (lp->data, window, THUNARX_FILE_INFO (folder), file_list));
              g_object_unref (G_OBJECT (lp->data));
            }
          g_list_free (providers);

          /* check if we have at least one item */
          if (G_UNLIKELY (items != NULL))
            {
              /* add menu items for all items */
              for (lp = items; lp != NULL; lp = lp->next)
                thunar_gtk_menu_thunarx_menu_item_new (lp->data, GTK_MENU_SHELL (menu));
              g_list_free (items);

              /* append another separator */
              item = gtk_separator_menu_item_new ();
              gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
              gtk_widget_show (item);
            }
        }
    }

  /* append the cancel item */
  item = gtk_menu_item_new_with_mnemonic (_("_Cancel"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* run the menu (takes over the floating of menu) */
  thunar_gtk_menu_run (GTK_MENU (menu));

  /* cleanup */
  g_object_unref (G_OBJECT (factory));
  g_list_free_full (file_list, g_object_unref);

  return dnd_action;
}



/**
 * thunar_dnd_perform:
 * @widget            : the #GtkWidget on which the drop was done.
 * @file              : the #ThunarFile on which the @file_list was dropped.
 * @file_list         : the list of #GFile<!---->s that was dropped.
 * @action            : the #GdkDragAction that was performed.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Performs the drop of @file_list on @file in @widget, as given in
 * @action and returns %TRUE if the drop was started successfully
 * (or even completed successfully), else %FALSE.
 *
 * Return value: %TRUE if the DnD operation was started
 *               successfully, else %FALSE.
 **/
gboolean
thunar_dnd_perform (GtkWidget    *widget,
                    ThunarFile   *file,
                    GList        *file_list,
                    GdkDragAction action,
                    GClosure     *new_files_closure)
{
  ThunarApplication *application;
  gboolean           succeed = TRUE;
  GError            *error = NULL;

  _thunar_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (gtk_widget_get_realized (widget), FALSE);

  /* query a reference on the application object */
  application = thunar_application_get ();

  /* check if the file is a directory */
  if (thunar_file_is_directory (file))
    {
      /* perform the given directory operation */
      switch (action)
        {
        case GDK_ACTION_COPY:
          thunar_application_copy_into (application, widget, file_list, thunar_file_get_file (file), THUNAR_OPERATION_LOG_OPERATIONS,  new_files_closure);
          break;

        case GDK_ACTION_MOVE:
          thunar_application_move_into (application, widget, file_list, thunar_file_get_file (file), THUNAR_OPERATION_LOG_OPERATIONS, new_files_closure);
          break;

        case GDK_ACTION_LINK:
          thunar_application_link_into (application, widget, file_list, thunar_file_get_file (file), THUNAR_OPERATION_LOG_OPERATIONS, new_files_closure);
          break;

        default:
          succeed = FALSE;
        }
    }
  else if (thunar_file_can_execute (file))
    {
      /* TODO any chance to determine the working dir here? */
      succeed = thunar_file_execute (file, NULL, widget, file_list, NULL, &error);
      if (G_UNLIKELY (!succeed))
        {
          /* display an error to the user */
          thunar_dialogs_show_error (widget, error, _("Failed to execute file \"%s\""), thunar_file_get_display_name (file));

          /* release the error */
          g_error_free (error);
        }
    }
  else
    {
      succeed = FALSE;
    }

  /* release the application reference */
  g_object_unref (G_OBJECT (application));

  return succeed;
}



