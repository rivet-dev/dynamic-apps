/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#include <gio/gio.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <thunar-uca/thunar-uca-chooser.h>
#include <thunar-uca/thunar-uca-context.h>
#include <thunar-uca/thunar-uca-model.h>
#include <thunar-uca/thunar-uca-private.h>
#include <thunar-uca/thunar-uca-provider.h>



static void   thunar_uca_provider_menu_provider_init        (ThunarxMenuProviderIface         *iface);
static void   thunar_uca_provider_preferences_provider_init (ThunarxPreferencesProviderIface  *iface);
static void   thunar_uca_provider_finalize                  (GObject                          *object);
static GList *thunar_uca_provider_get_menu_items            (ThunarxPreferencesProvider       *preferences_provider,
                                                             GtkWidget                        *window);
static GList *thunar_uca_provider_get_file_menu_items       (ThunarxMenuProvider              *menu_provider,
                                                             GtkWidget                        *window,
                                                             GList                            *files);
static GList *thunar_uca_provider_get_folder_menu_items     (ThunarxMenuProvider              *menu_provider,
                                                             GtkWidget                        *window,
                                                             ThunarxFileInfo                  *folder);
static void   thunar_uca_provider_activated                 (ThunarUcaProvider                *uca_provider,
                                                             ThunarxMenuItem                  *item);
static void   thunar_uca_provider_child_watch               (ThunarUcaProvider                *uca_provider,
                                                             gint                              exit_status);
static void   thunar_uca_provider_child_watch_destroy       (gpointer                          user_data,
                                                             GClosure                         *closure);



struct _ThunarUcaProviderClass
{
  GObjectClass __parent__;
};

struct _ThunarUcaProvider
{
  GObject __parent__;

  ThunarUcaModel *model;
  gint            last_action_id; /* used to generate unique action names */

  /* child watch support for the last spawned child process
   * to be able to refresh the folder contents after the
   * child process has terminated.
   */
  gchar          *child_watch_path;
  GClosure       *child_watch;
};



static GQuark thunar_uca_context_quark;
static GQuark thunar_uca_folder_quark;
static GQuark thunar_uca_row_quark;



THUNARX_DEFINE_TYPE_WITH_CODE (ThunarUcaProvider,
                               thunar_uca_provider,
                               G_TYPE_OBJECT,
                               THUNARX_IMPLEMENT_INTERFACE (THUNARX_TYPE_MENU_PROVIDER,
                                                            thunar_uca_provider_menu_provider_init)
                               THUNARX_IMPLEMENT_INTERFACE (THUNARX_TYPE_PREFERENCES_PROVIDER,
                                                            thunar_uca_provider_preferences_provider_init));



static void
thunar_uca_provider_class_init (ThunarUcaProviderClass *klass)
{
  GObjectClass *gobject_class;

  /* setup the "thunar-uca-context", "thunar-uca-folder" and "thunar-uca-row" quarks */
  thunar_uca_context_quark = g_quark_from_string ("thunar-uca-context");
  thunar_uca_folder_quark = g_quark_from_string ("thunar-uca-folder");
  thunar_uca_row_quark = g_quark_from_string ("thunar-uca-row");

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_uca_provider_finalize;
}



static void
thunar_uca_provider_menu_provider_init (ThunarxMenuProviderIface *iface)
{
  iface->get_file_menu_items = thunar_uca_provider_get_file_menu_items;
  iface->get_folder_menu_items = thunar_uca_provider_get_folder_menu_items;
}



static void
thunar_uca_provider_preferences_provider_init (ThunarxPreferencesProviderIface *iface)
{
  iface->get_menu_items = thunar_uca_provider_get_menu_items;
}



static void
thunar_uca_provider_init (ThunarUcaProvider *uca_provider)
{
  /* setup the i18n support first */
  thunar_uca_i18n_init ();

  /* grab a reference on the default model */
  uca_provider->model = thunar_uca_model_get_default ();
}



static void
thunar_uca_provider_finalize (GObject *object)
{
  ThunarUcaProvider *uca_provider = THUNAR_UCA_PROVIDER (object);

  /* give up maintaince of any pending child watch */
  thunar_uca_provider_child_watch_destroy (uca_provider, NULL);

  /* drop our reference on the model */
  g_object_unref (G_OBJECT (uca_provider->model));

  (*G_OBJECT_CLASS (thunar_uca_provider_parent_class)->finalize) (object);
}



static void
manage_menu_items (GtkWindow *window)
{
  GtkWidget *dialog;
  gboolean   use_header_bar = FALSE;

  g_object_get (gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (window))),
                "gtk-dialogs-use-header", &use_header_bar, NULL);

  dialog = g_object_new (THUNAR_UCA_TYPE_CHOOSER, "use-header-bar", use_header_bar, NULL);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), window);
  gtk_widget_show (dialog);
}



static GList*
thunar_uca_provider_get_menu_items (ThunarxPreferencesProvider *preferences_provider,
                                    GtkWidget                  *window)
{
  ThunarxMenuItem *item;
  GClosure        *closure;

  item = thunarx_menu_item_new ("ThunarUca::manage-actions", _("Configure c_ustom actions..."),
                                _("Setup custom actions that will appear in the file managers context menus"), NULL);
  closure = g_cclosure_new_object_swap (G_CALLBACK (manage_menu_items), G_OBJECT (window));
  g_signal_connect_closure (G_OBJECT (item), "activate", closure, TRUE);

  return g_list_prepend (NULL, item);
}


/* returned menu must be freed with g_object_unref() */
static ThunarxMenu*
find_submenu_by_name (gchar *name, GList* items)
{
  GList *lp;
  GList *thunarx_menu_items;

  for (lp = g_list_first (items); lp != NULL; lp = lp->next)
    {
      gchar       *item_name = NULL;
      ThunarxMenu *item_menu = NULL;
      g_object_get (G_OBJECT (lp->data), "name", &item_name, "menu", &item_menu, NULL);

      if (item_menu != NULL)
        {
          /* This menu is the correct menu */
          if (g_strcmp0 (item_name, name) == 0)
            {
              g_free (item_name);
              return item_menu;
            }

          /* Some other menu found .. lets check recursively if the menu we search for is inside */
          thunarx_menu_items = thunarx_menu_get_items (item_menu);
          g_object_unref (item_menu);

          if (thunarx_menu_items != NULL)
            {
              ThunarxMenu *submenu = find_submenu_by_name (name, thunarx_menu_items);
              if (submenu != NULL)
                {
                  g_free (item_name);
                  return submenu;
                }
              thunarx_menu_item_list_free (thunarx_menu_items);
            }
        }
      g_free (item_name);
    }

  /* not found */
  return NULL;
}



static GList*
thunar_uca_provider_get_file_menu_items (ThunarxMenuProvider *menu_provider,
                                         GtkWidget           *window,
                                         GList               *files)
{
  GtkTreeRowReference *row;
  ThunarUcaProvider   *uca_provider = THUNAR_UCA_PROVIDER (menu_provider);
  ThunarUcaContext    *uca_context = NULL;
  GtkTreeIter          iter;
  ThunarxMenuItem     *menu_item;
  ThunarxMenuItem     *item;
  GList               *items = NULL;
  GList               *paths;
  GList               *lp;
  ThunarxMenu         *submenu;
  ThunarxMenu         *parent_menu = NULL;

  paths = thunar_uca_model_match (uca_provider->model, files);

  for (lp = g_list_last (paths); lp != NULL; lp = lp->prev)
    {
      gchar  *unique_id = NULL;
      gchar  *name = NULL;
      gchar  *sub_menu_string = NULL;
      gchar **sub_menus_as_array = NULL;
      gchar  *label = NULL;
      gchar  *tooltip = NULL;
      gchar  *icon_name = NULL;
      GIcon  *gicon = NULL;

      /* try to lookup the tree iter for the specified tree path */
      if (gtk_tree_model_get_iter (GTK_TREE_MODEL (uca_provider->model), &iter, lp->data))
        {
          /* determine the label, tooltip and stock-id for the item */
          gtk_tree_model_get (GTK_TREE_MODEL (uca_provider->model), &iter,
                              THUNAR_UCA_MODEL_COLUMN_NAME, &label,
                              THUNAR_UCA_MODEL_COLUMN_SUB_MENU, &sub_menu_string,
                              THUNAR_UCA_MODEL_COLUMN_GICON, &gicon,
                              THUNAR_UCA_MODEL_COLUMN_DESCRIPTION, &tooltip,
                              THUNAR_UCA_MODEL_COLUMN_UNIQUE_ID, &unique_id,
                              -1);

          /* generate a unique action name */
          name = g_strdup_printf ("uca-action-%s", unique_id);

          if (gicon != NULL)
            icon_name = g_icon_to_string (gicon);

          /* Search or build the parent submenus, if required */
          parent_menu = NULL;
          sub_menus_as_array = g_strsplit (sub_menu_string, "/", -1);

          for (int i = 0; sub_menus_as_array[i] != NULL; i++)
           {
              /* get the submenu path up to the iterator  */
              gchar *sub_menu_path = g_strdup (sub_menus_as_array[0]);

              for (int j= 1; j<=i; j++)
                sub_menu_path = g_strconcat (sub_menu_path, "/", sub_menus_as_array[j], NULL);

              /* Check if the full path already exists */
              submenu = find_submenu_by_name (sub_menu_path, items);

              if (submenu != NULL)
                {
                  /* This submenu already exists, we can just use it as new parent */
                  parent_menu = submenu;
                  /* no need to keep the extra reference on it */
                  g_object_unref (submenu);
                }
              else
                {
                  /* Not found, we create a new submenu */
                  menu_item = thunarx_menu_item_new (sub_menu_path, sub_menus_as_array[i], "", "inode-directory");

                  /* Only add base-submenus to the returned list */
                  if (parent_menu == NULL)
                    items = g_list_prepend (items, menu_item);
                  else
                    thunarx_menu_prepend_item (parent_menu, menu_item);

                  /* This sublevel becomes the new parent */
                  parent_menu = thunarx_menu_new ();
                  thunarx_menu_item_set_menu (menu_item, parent_menu);
                }
              g_free (sub_menu_path);
            }
          g_strfreev (sub_menus_as_array);

          /* create the new menu item with the given parameters */
          item = thunarx_menu_item_new (name, label, tooltip, icon_name);

          /* grab a tree row reference on the given path */
          row = gtk_tree_row_reference_new (GTK_TREE_MODEL (uca_provider->model), lp->data);
          g_object_set_qdata_full (G_OBJECT (item), thunar_uca_row_quark, row,
                                   (GDestroyNotify) gtk_tree_row_reference_free);

          /* allocate a new context on-demand */
          if (G_LIKELY (uca_context == NULL))
            uca_context = thunar_uca_context_new (window, files);
          else
            uca_context = thunar_uca_context_ref (uca_context);
          g_object_set_qdata_full (G_OBJECT (item), thunar_uca_context_quark, uca_context, (GDestroyNotify) thunar_uca_context_unref);

          /* connect the "activate" signal */
          g_signal_connect_data (G_OBJECT (item), "activate", G_CALLBACK (thunar_uca_provider_activated),
                                 g_object_ref (G_OBJECT (uca_provider)),
                                 (GClosureNotify) (void (*)(void)) g_object_unref,
                                 G_CONNECT_SWAPPED);

          /* set the action path */
          g_object_set_data (G_OBJECT (item), "action_path",
                             g_strconcat ("<Actions>/ThunarActions/", name, NULL));

          /* add only base menu items to the return list */
          if(parent_menu == NULL)
            items = g_list_prepend (items, item);
          else
            thunarx_menu_prepend_item (parent_menu, item);

          /* cleanup */
          g_free (tooltip);
          g_free (label);
          g_free (name);
          g_free (sub_menu_string);
          g_free (icon_name);
          g_free (unique_id);

          if (gicon != NULL)
            g_object_unref (G_OBJECT (gicon));
        }

      /* release the tree path */
      gtk_tree_path_free (lp->data);
    }
  g_list_free (paths);

  return items;
}



static GList*
thunar_uca_provider_get_folder_menu_items (ThunarxMenuProvider *menu_provider,
                                           GtkWidget           *window,
                                           ThunarxFileInfo     *folder)
{
  GList *items;
  GList  files;
  GList *lp;

  /* fake a file list... */
  files.data = folder;
  files.next = NULL;
  files.prev = NULL;

  /* ...and use the get_file_menu_items() method */
  items = thunarx_menu_provider_get_file_menu_items (menu_provider, window, &files);

  /* mark the menu items, so we can properly detect the working directory */
  for (lp = items; lp != NULL; lp = lp->next)
    g_object_set_qdata (G_OBJECT (lp->data), thunar_uca_folder_quark, GUINT_TO_POINTER (TRUE));

  return items;
}



static void
thunar_uca_provider_activated (ThunarUcaProvider *uca_provider,
                               ThunarxMenuItem   *item)
{
  GtkTreeRowReference *row;
  ThunarUcaContext    *uca_context;
  GtkTreePath         *path;
  GtkTreeIter          iter;
  GtkWidget           *dialog;
  GtkWidget           *window;
  gboolean             succeed;
  GError              *error = NULL;
  GList               *files;
  gchar              **argv;
  gchar               *working_directory = NULL;
  gchar               *filename;
  gchar               *label;
  GFile               *location;
  gint                 argc;
  gchar               *icon_name = NULL;
  gboolean             startup_notify;
  GClosure            *child_watch;

  g_return_if_fail (THUNAR_UCA_IS_PROVIDER (uca_provider));
  g_return_if_fail (THUNARX_IS_MENU_ITEM (item));

  /* check if the row reference is still valid */
  row = g_object_get_qdata (G_OBJECT (item), thunar_uca_row_quark);
  if (G_UNLIKELY (!gtk_tree_row_reference_valid (row)))
    return;

  /* determine the iterator for the item */
  path = gtk_tree_row_reference_get_path (row);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (uca_provider->model), &iter, path);
  gtk_tree_path_free (path);

  /* determine the files and the window for the menu item */
  uca_context = g_object_get_qdata (G_OBJECT (item), thunar_uca_context_quark);
  window = thunar_uca_context_get_window (uca_context);
  files = thunar_uca_context_get_files (uca_context);

  /* determine the argc/argv for the item */
  succeed = thunar_uca_model_parse_argv (uca_provider->model, &iter, files, &argc, &argv, &error);
  if (G_LIKELY (succeed))
    {
      /* get the icon name and whether startup notification is active */
      gtk_tree_model_get (GTK_TREE_MODEL (uca_provider->model), &iter,
                          THUNAR_UCA_MODEL_COLUMN_ICON_NAME, &icon_name,
                          THUNAR_UCA_MODEL_COLUMN_STARTUP_NOTIFY, &startup_notify,
                          -1);

      /* determine the working from the first file */
      if (G_LIKELY (files != NULL))
        {
          /* determine the filename of the first selected file */
          location = thunarx_file_info_get_location (files->data);
          filename = g_file_get_path (location);
          if (G_LIKELY (filename != NULL))
            {
              /* if this is a folder menu item, we just use the filename as working directory */
              if (g_object_get_qdata (G_OBJECT (item), thunar_uca_folder_quark) != NULL)
                {
                  working_directory = filename;
                  filename = NULL;
                }
              else
                {
                  working_directory = g_path_get_dirname (filename);
                }
            }
          g_free (filename);
          g_object_unref (location);
        }

      /* build closre for child watch */
      child_watch = g_cclosure_new_swap (G_CALLBACK (thunar_uca_provider_child_watch),
                                         uca_provider, thunar_uca_provider_child_watch_destroy);
      g_closure_ref (child_watch);
      g_closure_sink (child_watch);

      /* spawn the command on the window's screen */
      succeed = xfce_spawn_on_screen_with_child_watch (gtk_widget_get_screen (GTK_WIDGET (window)),
                                                       working_directory, argv, NULL,
                                                       G_SPAWN_SEARCH_PATH,
                                                       startup_notify,
                                                       gtk_get_current_event_time (),
                                                       icon_name,
                                                       child_watch,
                                                       &error);

      /* check if we succeed */
      if (G_LIKELY (succeed))
        {
          /* release existing child watch */
          thunar_uca_provider_child_watch_destroy (uca_provider, NULL);

          /* set new closure */
          uca_provider->child_watch = child_watch;

          /* take over ownership of the working directory as child watch path */
          uca_provider->child_watch_path = working_directory;
          working_directory = NULL;
        }
      else
        {
          /* spawn failed, release watch */
          g_closure_unref (child_watch);
        }

      /* cleanup */
      g_free (working_directory);
      g_strfreev (argv);
      g_free (icon_name);
    }

  /* present error message to the user */
  if (G_UNLIKELY (!succeed))
    {
      g_object_get (G_OBJECT (item), "label", &label, NULL);
      dialog = gtk_message_dialog_new ((GtkWindow *) window,
                                       GTK_DIALOG_DESTROY_WITH_PARENT
                                       | GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Failed to launch action \"%s\"."), label);
      gtk_window_set_title (GTK_WINDOW (dialog), _("Error"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s.", error->message);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
      g_free (label);
    }
}



static void
thunar_uca_provider_child_watch (ThunarUcaProvider *uca_provider,
                                 gint               exit_status)

{
  GFileMonitor *monitor;
  GFile        *file;

  g_return_if_fail (THUNAR_UCA_IS_PROVIDER (uca_provider));

  /* verify that we still have a valid child_watch_path */
  if (G_LIKELY (uca_provider->child_watch_path != NULL))
    {
      /* determine the corresponding file */
      file = g_file_new_for_path (uca_provider->child_watch_path);

      /* schedule a changed notification on the path */
      monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, NULL);

      if (monitor != NULL)
        {
          g_file_monitor_emit_event (monitor, file, file, G_FILE_MONITOR_EVENT_CHANGED);
          g_object_unref (monitor);
        }

      /* release the file */
      g_object_unref (file);
    }

  thunar_uca_provider_child_watch_destroy (uca_provider, NULL);
}



static void
thunar_uca_provider_child_watch_destroy (gpointer  user_data,
                                         GClosure *closure)
{
  ThunarUcaProvider *uca_provider = THUNAR_UCA_PROVIDER (user_data);
  GClosure          *child_watch;

  /* leave if the closure is not the one we're watching */
  if (uca_provider->child_watch == closure
      || closure == NULL)
    {
      /* reset child watch and path */
      if (G_UNLIKELY (uca_provider->child_watch != NULL))
        {
          child_watch = uca_provider->child_watch;
          uca_provider->child_watch = NULL;

          g_closure_invalidate (child_watch);
          g_closure_unref (child_watch);
        }

      g_free (uca_provider->child_watch_path);
      uca_provider->child_watch_path = NULL;
    }
}
