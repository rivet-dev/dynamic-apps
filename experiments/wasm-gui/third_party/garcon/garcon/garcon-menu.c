/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2007-2010 Jannis Pohlmann <jannis@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <garcon/garcon-config.h>
#include <garcon/garcon-environment.h>
#include <garcon/garcon-marshal.h>
#include <garcon/garcon-menu-element.h>
#include <garcon/garcon-menu-item.h>
#include <garcon/garcon-menu-directory.h>
#include <garcon/garcon-menu-item-cache.h>
#include <garcon/garcon-menu-separator.h>
#include <garcon/garcon-menu-node.h>
#include <garcon/garcon-menu-parser.h>
#include <garcon/garcon-menu-merger.h>
#include <garcon/garcon-private.h>



/* Use g_access() on win32 */
#if defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_access(filename, mode) (access ((filename), (mode)))
#endif



/**
 * SECTION: garcon-menu
 * @title: GarconMenu
 * @short_description: Menu element.
 * @include: garcon/garcon.h
 *
 * Main element the contains the parsed menu.
 **/



typedef struct _GarconMenuPair
{
  gpointer first;
  gpointer second;
} GarconMenuPair;



/* Property identifiers */
enum
{
  PROP_0,
  PROP_ENVIRONMENT,
  PROP_FILE,
  PROP_DIRECTORY,
  PROP_PARENT, /* TODO */
};



/* Signal identifiers */
enum
{
  RELOAD_REQUIRED,
  DIRECTORY_CHANGED,
  LAST_SIGNAL
};



static void                 garcon_menu_element_init                    (GarconMenuElementIface  *iface);
static void                 garcon_menu_clear                           (GarconMenu              *menu);
static void                 garcon_menu_finalize                        (GObject                 *object);
static void                 garcon_menu_get_property                    (GObject                 *object,
                                                                         guint                    prop_id,
                                                                         GValue                  *value,
                                                                         GParamSpec              *pspec);
static void                 garcon_menu_set_property                    (GObject                 *object,
                                                                         guint                    prop_id,
                                                                         const GValue            *value,
                                                                         GParamSpec              *pspec);
static void                 garcon_menu_set_directory                   (GarconMenu              *menu,
                                                                         GarconMenuDirectory     *directory);
static void                 garcon_menu_resolve_menus                   (GarconMenu              *menu);
static void                 garcon_menu_resolve_directory               (GarconMenu              *menu,
                                                                         GCancellable            *cancellable,
                                                                         gboolean                 recursive);
static GarconMenuDirectory *garcon_menu_lookup_directory                (GarconMenu              *menu,
                                                                         const gchar             *filename);
static void                 garcon_menu_collect_files                   (GarconMenu              *menu,
                                                                         GHashTable              *desktop_id_table);
static void                 garcon_menu_collect_files_from_path         (GarconMenu              *menu,
                                                                         GHashTable              *desktop_id_table,
                                                                         GFile                   *path,
                                                                         const gchar             *id_prefix);
static void                 garcon_menu_resolve_items                   (GarconMenu              *menu,
                                                                         GHashTable              *desktop_id_table,
                                                                         gboolean                 only_unallocated);
static void                 garcon_menu_resolve_items_by_rule           (GarconMenu              *menu,
                                                                         GHashTable              *desktop_id_table,
                                                                         GNode                   *node);
static void                 garcon_menu_resolve_item_by_rule            (const gchar             *desktop_id,
                                                                         const gchar             *uri,
                                                                         GarconMenuPair          *data);
static void                 garcon_menu_remove_deleted_menus            (GarconMenu              *menu);
static gint                 garcon_menu_compare_items                   (gconstpointer           *a,
                                                                         gconstpointer           *b);
static const gchar         *garcon_menu_get_element_name                (GarconMenuElement       *element);
static const gchar         *garcon_menu_get_element_comment             (GarconMenuElement       *element);
static const gchar         *garcon_menu_get_element_icon_name           (GarconMenuElement       *element);
static gboolean             garcon_menu_get_element_visible             (GarconMenuElement       *element);
static gboolean             garcon_menu_get_element_show_in_environment (GarconMenuElement       *element);
static gboolean             garcon_menu_get_element_no_display          (GarconMenuElement       *element);
static gboolean             garcon_menu_get_element_equal               (GarconMenuElement       *element,
                                                                         GarconMenuElement       *other);
static void                 garcon_menu_start_monitoring                (GarconMenu              *menu);
static void                 garcon_menu_stop_monitoring                 (GarconMenu              *menu);
static void                 garcon_menu_monitor_menu_files              (GarconMenu              *menu);
static void                 garcon_menu_monitor_files                   (GarconMenu              *menu,
                                                                         GList                   *files,
                                                                         gpointer                 callback);
static void                 garcon_menu_monitor_app_dirs                (GarconMenu              *menu);
static void                 garcon_menu_monitor_directory_dirs          (GarconMenu              *menu);
static void                 garcon_menu_file_changed                    (GarconMenu              *menu,
                                                                         GFile                   *file,
                                                                         GFile                   *other_file,
                                                                         GFileMonitorEvent        event_type,
                                                                         GFileMonitor            *monitor);
static void                 garcon_menu_merge_file_changed              (GarconMenu              *menu,
                                                                         GFile                   *file,
                                                                         GFile                   *other_file,
                                                                         GFileMonitorEvent        event_type,
                                                                         GFileMonitor            *monitor);
static void                 garcon_menu_merge_dir_changed               (GarconMenu              *menu,
                                                                         GFile                   *file,
                                                                         GFile                   *other_file,
                                                                         GFileMonitorEvent        event_type,
                                                                         GFileMonitor            *monitor);
static void                 garcon_menu_app_dir_changed                 (GarconMenu              *menu,
                                                                         GFile                   *file,
                                                                         GFile                   *other_file,
                                                                         GFileMonitorEvent        event_type,
                                                                         GFileMonitor            *monitor);
static void                 garcon_menu_directory_file_changed          (GarconMenu              *menu,
                                                                         GFile                   *file,
                                                                         GFile                   *other_file,
                                                                         GFileMonitorEvent        event_type,
                                                                         GFileMonitor            *monitor);
static GarconMenuItem      *garcon_menu_find_file_item                  (GarconMenu              *menu,
                                                                         GFile                   *file);



struct _GarconMenuPrivate
{
  /* Menu file */
  GFile               *file;

  /* DOM tree */
  GNode               *tree;

  /* Merged menu files and merge directories */
  GList               *merge_files;
  GList               *merge_dirs;

  /* File and directory monitors */
  GList               *monitors;

  /* Directory */
  GarconMenuDirectory *directory;

  /* Submenus */
  GList               *submenus;

  /* Parent menu */
  GarconMenu          *parent;

  /* Menu item pool */
  GarconMenuItemPool  *pool;

  /* Shared menu item cache */
  GarconMenuItemCache *cache;

  /* List to merge consecutive file changes into a a single event */
  GSList              *changed_files;
  guint                file_changed_idle;

  /* Flag for marking custom path menus */
  guint                uses_custom_path : 1;

  /* idle reload-required to group events */
  guint                idle_reload_required_id;

  /* asynchronous load from GarconGtkMenu */
  GMutex               load_lock;
};



G_DEFINE_TYPE_WITH_CODE (GarconMenu, garcon_menu, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GarconMenu)
                         G_IMPLEMENT_INTERFACE (GARCON_TYPE_MENU_ELEMENT,
                                                garcon_menu_element_init))



static guint menu_signals[LAST_SIGNAL];
static GQuark garcon_menu_file_quark;



static void
garcon_menu_class_init (GarconMenuClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = garcon_menu_finalize;
  gobject_class->get_property = garcon_menu_get_property;
  gobject_class->set_property = garcon_menu_set_property;

  /**
   * GarconMenu:file:
   *
   * The #GFile from which the %GarconMenu was loaded.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        "file",
                                                        "file",
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS |
                                                        G_PARAM_CONSTRUCT_ONLY));

  /**
   * GarconMenu:directory:
   *
   * The directory entry associated with this menu.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DIRECTORY,
                                   g_param_spec_object ("directory",
                                                        "Directory",
                                                        "Directory entry associated with this menu",
                                                        GARCON_TYPE_MENU_DIRECTORY,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  menu_signals[RELOAD_REQUIRED] =
    g_signal_new ("reload-required",
                  GARCON_TYPE_MENU,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  menu_signals[DIRECTORY_CHANGED] =
    g_signal_new ("directory-changed",
                  GARCON_TYPE_MENU,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL,
                  NULL,
                  garcon_marshal_VOID__OBJECT_OBJECT,
                  G_TYPE_NONE,
                  2,
                  GARCON_TYPE_MENU_DIRECTORY,
                  GARCON_TYPE_MENU_DIRECTORY);

  garcon_menu_file_quark = g_quark_from_string ("garcon-menu-file-quark");
}



static void
garcon_menu_element_init (GarconMenuElementIface *iface)
{
  iface->get_name = garcon_menu_get_element_name;
  iface->get_comment = garcon_menu_get_element_comment;
  iface->get_icon_name = garcon_menu_get_element_icon_name;
  iface->get_visible = garcon_menu_get_element_visible;
  iface->get_show_in_environment = garcon_menu_get_element_show_in_environment;
  iface->get_no_display = garcon_menu_get_element_no_display;
  iface->equal = garcon_menu_get_element_equal;
}



static void
garcon_menu_init (GarconMenu *menu)
{
  menu->priv = garcon_menu_get_instance_private (menu);
  menu->priv->file = NULL;
  menu->priv->tree = NULL;
  menu->priv->merge_files = NULL;
  menu->priv->merge_dirs = NULL;
  menu->priv->monitors = NULL;
  menu->priv->directory = NULL;
  menu->priv->submenus = NULL;
  menu->priv->parent = NULL;
  menu->priv->pool = garcon_menu_item_pool_new ();
  menu->priv->uses_custom_path = TRUE;
  menu->priv->changed_files = NULL;
  menu->priv->file_changed_idle = 0;
  g_mutex_init (&menu->priv->load_lock);
  menu->priv->idle_reload_required_id = 0;

  /* Take reference on the menu item cache */
  menu->priv->cache = garcon_menu_item_cache_get_default ();
}



static void
garcon_menu_clear (GarconMenu *menu)
{
  g_return_if_fail (GARCON_IS_MENU (menu));

  /* Check if the menu is the root menu */
  if (menu->priv->parent == NULL)
    {
      /* Stop monitoring recursively */
      garcon_menu_stop_monitoring (menu);

      /* Destroy the menu tree */
      garcon_menu_node_tree_free (menu->priv->tree);
      menu->priv->tree = NULL;

      /* Release the merge files */
      _garcon_g_list_free_full (menu->priv->merge_files, g_object_unref);
      menu->priv->merge_files = NULL;

      /* Release the merge dirs */
      _garcon_g_list_free_full (menu->priv->merge_dirs, g_object_unref);
      menu->priv->merge_dirs = NULL;
    }

  /* Free submenus */
  _garcon_g_list_free_full (menu->priv->submenus, g_object_unref);
  menu->priv->submenus = NULL;

  /* Free directory */
  if (G_LIKELY (menu->priv->directory != NULL))
    {
      g_object_unref (menu->priv->directory);
      menu->priv->directory = NULL;
    }

  /* Clear the item pool */
  garcon_menu_item_pool_clear (menu->priv->pool);

  /* Stop reload-required emit */
  if (menu->priv->idle_reload_required_id != 0)
    {
      g_source_remove (menu->priv->idle_reload_required_id);
      menu->priv->idle_reload_required_id = 0;
    }
}



static void
garcon_menu_finalize (GObject *object)
{
  GarconMenu *menu = GARCON_MENU (object);

  /* wait for any async operation to finish */
  g_mutex_lock (&menu->priv->load_lock);
  g_mutex_unlock (&menu->priv->load_lock);
  g_mutex_clear (&menu->priv->load_lock);

  /* Clear resources allocated in the load process */
  garcon_menu_clear (menu);

  /* Free file */
  if (menu->priv->file != NULL)
    g_object_unref (menu->priv->file);

  /* Free item pool */
  g_object_unref (menu->priv->pool);

  /* Release item cache reference */
  g_object_unref (menu->priv->cache);

  (*G_OBJECT_CLASS (garcon_menu_parent_class)->finalize) (object);
}



static void
garcon_menu_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GarconMenu *menu = GARCON_MENU (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, menu->priv->file);
      break;

    case PROP_DIRECTORY:
      g_value_set_object (value, menu->priv->directory);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
garcon_menu_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GarconMenu *menu = GARCON_MENU (object);

  switch (prop_id)
    {
    case PROP_FILE:
      menu->priv->file = g_value_dup_object (value);
      break;

    case PROP_DIRECTORY:
      garcon_menu_set_directory (menu, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/**
 * garcon_menu_new: (constructor)
 * @file: #GFile for the .menu file you want to load.
 *
 * Creates a new #GarconMenu for the .menu file referred to by @file.
 * This operation only fails if @file is invalid. To load the menu
 * tree from the file, you need to call garcon_menu_load() with the
 * returned #GarconMenu.
 *
 * <informalexample><programlisting>
 * GarconMenu *menu = garcon_menu_new (filename);
 *
 * if (garcon_menu_load (menu, &error))
 *   ...
 * else
 *   ...
 *
 * g_object_unref (menu);
 * </programlisting></informalexample>
 *
 * The caller is responsible to destroy the returned #GarconMenu
 * using g_object_unref().
 *
 * For more information about the usage @see garcon_menu_new().
 *
 * Returns: (transfer full): a new #GarconMenu.
 **/
GarconMenu *
garcon_menu_new (GFile *file)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  return g_object_new (GARCON_TYPE_MENU, "file", file, NULL);
}



/**
 * garcon_menu_new_for_path: (constructor)
 * @filename : Path/URI of the .menu file you want to load.
 *
 * Creates a new #GarconMenu for the .menu file referred to by @filename.
 * This operation only fails if the filename is %NULL. To load the menu
 * tree from the file, you need to call garcon_menu_load() with the
 * returned #GarconMenu.
 *
 * The caller is responsible to destroy the returned #GarconMenu
 * using g_object_unref().
 *
 * Returns: (transfer full): a new #GarconMenu
 * for @filename.
 **/
GarconMenu *
garcon_menu_new_for_path (const gchar *filename)
{
  GarconMenu *menu;
  GFile      *file;

  g_return_val_if_fail (filename != NULL, NULL);

  /* Create new menu */
  file = _garcon_file_new_for_unknown_input (filename, NULL);
  menu = g_object_new (GARCON_TYPE_MENU, "file", file, NULL);
  g_object_unref (file);

  return menu;
}



/**
 * garcon_menu_new_applications:
 *
 * Creates a new #GarconMenu for the applications.menu file
 * which is being used to display installed applications.
 *
 * For more information about the usage @see garcon_menu_new().
 *
 * Returns: (transfer full): a new #GarconMenu
 * for applications.menu.
 **/
GarconMenu *
garcon_menu_new_applications (void)
{
  GarconMenu *menu = NULL;

  menu = g_object_new (GARCON_TYPE_MENU, NULL);
  menu->priv->uses_custom_path = FALSE;

  return menu;
}



/**
 * garcon_menu_get_file:
 * @menu: a #GarconMenu.
 *
 * Get the file for @menu. It refers to the .menu file from which
 * @menu was or will be loaded.
 *
 * The returned object should be unreffed with g_object_unref()
 * when no longer needed.
 *
 * Returns: (transfer full): a #GFile.
 */
GFile *
garcon_menu_get_file (GarconMenu *menu)
{
  g_return_val_if_fail (GARCON_IS_MENU (menu), NULL);
  return g_object_ref (menu->priv->file);
}




static const gchar *
garcon_menu_get_name (GarconMenu *menu)
{
  g_return_val_if_fail (GARCON_IS_MENU (menu), NULL);
  return garcon_menu_node_tree_get_string_child (menu->priv->tree,
                                                 GARCON_MENU_NODE_TYPE_NAME);
}



/**
 * garcon_menu_get_directory:
 * @menu: a #GarconMenu.
 *
 * Returns the #GarconMenuDirectory of @menu or %NULL if @menu has
 * no valid directory element.
 *
 * The menu directory may contain a lot of useful information about
 * the menu like the display and icon name, desktop environments it
 * should show up in etc.
 *
 * Returns: (transfer none) (nullable): a #GarconMenuDirectory
 */
GarconMenuDirectory*
garcon_menu_get_directory (GarconMenu *menu)
{
  g_return_val_if_fail (GARCON_IS_MENU (menu), NULL);
  return menu->priv->directory;
}



static void
garcon_menu_set_directory (GarconMenu          *menu,
                           GarconMenuDirectory *directory)
{
  g_return_if_fail (GARCON_IS_MENU (menu));
  g_return_if_fail (GARCON_IS_MENU_DIRECTORY (directory));

  /* Abort if directories are equal */
  if (garcon_menu_directory_equal (directory, menu->priv->directory))
    return;

  /* Destroy old directory */
  if (menu->priv->directory != NULL)
    g_object_unref (menu->priv->directory);

  /* Remove the floating reference and acquire a normal one */
  g_object_ref_sink (directory);

  /* Set the new directory */
  menu->priv->directory = directory;

  /* Notify listeners */
  g_object_notify (G_OBJECT (menu), "directory");
}



/**
 * garcon_menu_load:
 * @menu        : a #GarconMenu
 * @cancellable : a #GCancellable
 * @error       : #GError return location
 *
 * This function loads the entire menu tree from the file referred to
 * by @menu. It resolves merges, moves and everything else defined
 * in the menu specification. The resulting tree information is
 * stored within @menu and can be accessed using the public #GarconMenu
 * API afterwards.
 *
 * @cancellable can be used to handle blocking I/O when reading data
 * from files during the loading process.
 *
 * @error should either be NULL or point to a #GError return location
 * where errors should be stored in.
 *
 * Returns: %TRUE if the menu was loaded successfully or
 *          %FALSE if there was an error or the process was
 *          cancelled.
 **/
gboolean
garcon_menu_load (GarconMenu   *menu,
                  GCancellable *cancellable,
                  GError      **error)
{
  GarconMenuParser *parser;
  GarconMenuMerger *merger;
  GHashTable       *desktop_id_table;
  const gchar      *prefix;
  gboolean          success = TRUE;
  gchar            *filename;
  gchar            *relative_filename;

  g_return_val_if_fail (GARCON_IS_MENU (menu), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_mutex_lock (&menu->priv->load_lock);

  /* Make sure to reset the menu to a loadable state */
  garcon_menu_clear (menu);

  /* Check if we need to locate the applications menu file */
  if (!menu->priv->uses_custom_path)
    {
      /* Release the old file if there is one */
      if (menu->priv->file != NULL)
        {
          g_object_unref (menu->priv->file);
          menu->priv->file = NULL;
        }

      /* Build the ${XDG_MENU_PREFIX}applications.menu filename */
      prefix = g_getenv ("XDG_MENU_PREFIX");
      relative_filename = g_strconcat ("menus", G_DIR_SEPARATOR_S,
                                       prefix != NULL ? prefix : _GARCON_DEFAULT_MENU_PREFIX,
                                       "applications.menu", NULL);

      /* Search for the menu file in user and system config dirs */
      filename = garcon_config_lookup (relative_filename);

      /* Use the file if it exists */
      if (filename != NULL)
        menu->priv->file = _garcon_file_new_for_unknown_input (filename, NULL);

      /* Abort with an error if no suitable applications menu file was found */
      if (menu->priv->file == NULL)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                       _("File \"%s\" not found"), relative_filename);
          g_free (relative_filename);
          g_mutex_unlock (&menu->priv->load_lock);
          return FALSE;
        }

      /* Free the filename strings */
      g_free (relative_filename);
      g_free (filename);

    }

  parser = garcon_menu_parser_new (menu->priv->file);

  if (garcon_menu_parser_run (parser, cancellable, error))
    {
      merger = garcon_menu_merger_new (GARCON_MENU_TREE_PROVIDER (parser));

      if (garcon_menu_merger_run (merger,
                                  &menu->priv->merge_files,
                                  &menu->priv->merge_dirs,
                                  cancellable, error))
        {
          menu->priv->tree =
            garcon_menu_tree_provider_get_tree (GARCON_MENU_TREE_PROVIDER (merger));
        }
      else
        {
          success = FALSE;
        }

      g_object_unref (merger);
    }
  else
    success = FALSE;

  g_object_unref (parser);

  if (!success)
    {
      g_mutex_unlock (&menu->priv->load_lock);
      return FALSE;
    }

  /* Generate submenus */
  garcon_menu_resolve_menus (menu);

  /* Resolve the menu directory */
  garcon_menu_resolve_directory (menu, cancellable, TRUE);

  /* Abort if the cancellable was cancelled */
  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    {
      g_mutex_unlock (&menu->priv->load_lock);
      return FALSE;
    }

  desktop_id_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  /* Load menu items */
  garcon_menu_collect_files (menu, desktop_id_table);
  garcon_menu_resolve_items (menu, desktop_id_table, FALSE);
  garcon_menu_resolve_items (menu, desktop_id_table, TRUE);

  /* Remove deleted menus */
  garcon_menu_remove_deleted_menus (menu);

  g_hash_table_unref (desktop_id_table);

  /* Initiate file system monitoring */
  garcon_menu_start_monitoring (menu);

  g_mutex_unlock (&menu->priv->load_lock);

  return TRUE;
}



/**
 * garcon_menu_get_menus:
 * @menu: a #GarconMenu
 *
 * Returns a sorted list of #GarconMenu submenus of @menu. The list
 * should be freed with g_list_free().
 *
 * Returns: (transfer container) (element-type GarconMenu): a sorted list
 * of #GarconMenu.
 */
GList *
garcon_menu_get_menus (GarconMenu *menu)
{
  GList *menus = NULL;

  g_return_val_if_fail (GARCON_IS_MENU (menu), NULL);

  /* Copy submenu list */
  menus = g_list_copy (menu->priv->submenus);

  /* Sort submenus */
  menus = g_list_sort (menus, (GCompareFunc) garcon_menu_compare_items);

  return menus;
}



/**
 * garcon_menu_add_menu:
 * @menu:    a #GarconMenu
 * @submenu: a #GarconMenu
 *
 * Adds @submenu as a sub menu to @menu.
 **/
void
garcon_menu_add_menu (GarconMenu *menu,
                      GarconMenu *submenu)
{
  g_return_if_fail (GARCON_IS_MENU (menu));
  g_return_if_fail (GARCON_IS_MENU (submenu));

  /* Remove floating reference and acquire a 'real' one */
  g_object_ref_sink (G_OBJECT (submenu));

  /* Append menu to the list */
  menu->priv->submenus = g_list_append (menu->priv->submenus, submenu);

  /* TODO: Use property method here */
  submenu->priv->parent = menu;
}



/**
 * garcon_menu_get_menu_with_name:
 * @menu: a #GarconMenu
 * @name: a sub menu name
 *
 * Looks in @menu for a submenu with @name as name.
 *
 * Returns: (transfer none) (nullable): a #GarconMenu or %NULL.
 */
GarconMenu *
garcon_menu_get_menu_with_name (GarconMenu  *menu,
                                const gchar *name)
{
  GarconMenu *result = NULL;
  GList      *iter;

  g_return_val_if_fail (GARCON_IS_MENU (menu), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  /* Iterate over the submenu list */
  for (iter = menu->priv->submenus; result == NULL && iter != NULL; iter = g_list_next (iter))
    {
      /* End loop when a matching submenu is found */
      if (G_UNLIKELY (g_strcmp0 (garcon_menu_get_name (iter->data), name) == 0))
        result = iter->data;
    }

  return result;
}



/**
 * garcon_menu_get_parent:
 * @menu: a #GarconMenu
 *
 * Returns the parent #GarconMenu or @menu.
 *
 * Returns: (transfer none) (nullable): a #GarconMenu or %NULL
 * if @menu is the root menu.
 */
GarconMenu *
garcon_menu_get_parent (GarconMenu *menu)
{
  g_return_val_if_fail (GARCON_IS_MENU (menu), NULL);
  return menu->priv->parent;
}



static void
garcon_menu_resolve_menus (GarconMenu *menu)
{
  GarconMenu *submenu;
  GList      *menus = NULL;
  GList      *iter;

  g_return_if_fail (GARCON_IS_MENU (menu));

  menus = garcon_menu_node_tree_get_child_nodes (menu->priv->tree,
                                                 GARCON_MENU_NODE_TYPE_MENU,
                                                 FALSE);

  for (iter = menus; iter != NULL; iter = g_list_next (iter))
    {
      submenu = g_object_new (GARCON_TYPE_MENU, "file", menu->priv->file, NULL);
      submenu->priv->tree = iter->data;
      garcon_menu_add_menu (menu, submenu);
      g_object_unref (submenu);
    }

  g_list_free (menus);

  for (iter = menu->priv->submenus; iter != NULL; iter = g_list_next (iter))
    garcon_menu_resolve_menus (iter->data);
}



static GList *
garcon_menu_get_directories (GarconMenu *menu)
{
  GList *dirs = NULL;

  /* Fetch all application directories */
  dirs = garcon_menu_node_tree_get_string_children (menu->priv->tree,
                                                    GARCON_MENU_NODE_TYPE_DIRECTORY,
                                                    TRUE);

  if (menu->priv->parent != NULL)
    dirs = g_list_concat (dirs, garcon_menu_get_directories (menu->priv->parent));

  return dirs;
}



static void
garcon_menu_resolve_directory (GarconMenu   *menu,
                               GCancellable *cancellable,
                               gboolean      recursive)
{
  GarconMenuDirectory *directory = NULL;
  GList               *directories = NULL;
  GList               *iter;

  g_return_if_fail (GARCON_IS_MENU (menu));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  /* release the old directory if there is one */
  if (menu->priv->directory != NULL)
    {
      g_object_unref (menu->priv->directory);
      menu->priv->directory = NULL;
    }

  /* Determine all directories for this menu */
  directories = garcon_menu_get_directories (menu);

  /* Try to load one directory name after another */
  for (iter = directories; directory == NULL && iter != NULL; iter = g_list_next (iter))
    {
      /* Try to load the directory with this name */
      directory = garcon_menu_lookup_directory (menu, iter->data);
    }

  if (G_LIKELY (directory != NULL))
    {
      /* Set the directory (assuming that we found at least one valid name) */
      menu->priv->directory = directory;
    }

  /* Free reverse list copy */
  g_list_free (directories);

  if (recursive)
    {
      /* Resolve directories of submenus recursively */
      for (iter = menu->priv->submenus; iter != NULL; iter = g_list_next (iter))
        garcon_menu_resolve_directory (iter->data, cancellable, recursive);
    }
}



static GList *
garcon_menu_get_directory_dirs (GarconMenu *menu)
{
  GList *dirs = NULL;

  /* Fetch all application directories */
  dirs = garcon_menu_node_tree_get_string_children (menu->priv->tree,
                                                    GARCON_MENU_NODE_TYPE_DIRECTORY_DIR,
                                                    TRUE);

  if (menu->priv->parent != NULL)
    dirs = g_list_concat (dirs, garcon_menu_get_directory_dirs (menu->priv->parent));

  return dirs;
}



static GarconMenuDirectory *
garcon_menu_lookup_directory (GarconMenu  *menu,
                              const gchar *filename)
{
  GarconMenuDirectory *directory = NULL;
  GList               *dirs = NULL;
  GList               *iter;
  GFile               *file;
  GFile               *dir;
  gboolean             found = FALSE;

  g_return_val_if_fail (GARCON_IS_MENU (menu), NULL);
  g_return_val_if_fail (filename != NULL, NULL);

  dirs = garcon_menu_get_directory_dirs (menu);

  /* Iterate through all directories */
  for (iter = dirs; !found && iter != NULL; iter = g_list_next (iter))
    {
      dir = _garcon_file_new_relative_to_file (iter->data, menu->priv->file);
      file = _garcon_file_new_relative_to_file (filename, dir);

      /* Check if the file exists and is readable */
      if (G_LIKELY (g_file_query_exists (file, NULL)))
        {
          /* Load menu directory */
          directory = garcon_menu_directory_new (file);

          /* Update search status */
          found = TRUE;
        }

      /* Destroy the file objects */
      g_object_unref (file);
      g_object_unref (dir);
    }

  /* Free reverse copy */
  g_list_free (dirs);

  return directory;
}



static GList *
garcon_menu_get_app_dirs (GarconMenu *menu,
                          gboolean    recursive)
{
  GList *dirs = NULL;
  GList *lp;
  GList *sp;
  GList *submenu_app_dirs;

  /* Fetch all application directories */
  dirs = garcon_menu_node_tree_get_string_children (menu->priv->tree,
                                                    GARCON_MENU_NODE_TYPE_APP_DIR,
                                                    TRUE);

  if (recursive)
    {
      for (lp = menu->priv->submenus; lp != NULL; lp = lp->next)
        {
          submenu_app_dirs = garcon_menu_get_app_dirs (lp->data, recursive);

          for (sp = g_list_last (submenu_app_dirs); sp != NULL; sp = sp->prev)
            if (g_list_find_custom (dirs, sp->data, (GCompareFunc) g_strcmp0) == NULL)
              dirs = g_list_prepend (dirs, sp->data);
        }
    }

  return dirs;
}



static void
garcon_menu_collect_files (GarconMenu *menu,
                           GHashTable *desktop_id_table)
{
  GList *app_dirs = NULL;
  GList *iter;
  GFile *file;

  g_return_if_fail (GARCON_IS_MENU (menu));

  app_dirs = garcon_menu_get_app_dirs (menu, FALSE);

  /* Collect desktop entry filenames */
  for (iter = app_dirs; iter != NULL; iter = g_list_next (iter))
    {
      file = g_file_new_for_uri (iter->data);
      garcon_menu_collect_files_from_path (menu, desktop_id_table, file, NULL);
      g_object_unref (file);
    }

  /* Free directory list */
  g_list_free (app_dirs);

  /* Collect filenames for submenus */
  for (iter = menu->priv->submenus; iter != NULL; iter = g_list_next (iter))
    garcon_menu_collect_files (iter->data, desktop_id_table);
}



static void
garcon_menu_collect_files_from_path (GarconMenu  *menu,
                                     GHashTable  *desktop_id_table,
                                     GFile       *dir,
                                     const gchar *id_prefix)
{
  GFileEnumerator *enumerator;
  GFileInfo       *file_info;
  GFile           *file;
  gchar           *base_name;
  gchar           *new_id_prefix;
  gchar           *desktop_id;

  g_return_if_fail (GARCON_IS_MENU (menu));

  /* Skip directory if it doesn't exist */
  if (G_UNLIKELY (!g_file_query_exists (dir, NULL)))
    return;

  /* Skip directory if it's not a directory */
  if (G_UNLIKELY (g_file_query_file_type (dir, G_FILE_QUERY_INFO_NONE,
                                          NULL) != G_FILE_TYPE_DIRECTORY))
    {
      return;
    }

  /* Open directory for reading */
  enumerator = g_file_enumerate_children (dir, "standard::name,standard::type",
                                          G_FILE_QUERY_INFO_NONE, NULL, NULL);

  /* Abort if directory cannot be opened */
  if (G_UNLIKELY (enumerator == NULL))
    return;

  /* Read file by file */
  while (TRUE)
    {
      file_info = g_file_enumerator_next_file (enumerator, NULL, NULL);

      if (G_UNLIKELY (file_info == NULL))
        break;

      file = g_file_resolve_relative_path (dir, g_file_info_get_name (file_info));
      base_name = g_file_get_basename (file);

      /* Treat files and directories differently */
      if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY)
        {
          /* Create new desktop-file id prefix */
          if (G_LIKELY (id_prefix == NULL))
            new_id_prefix = g_strdup (base_name);
          else
            new_id_prefix = g_strjoin ("-", id_prefix, base_name, NULL);

          /* Collect files in the directory */
          garcon_menu_collect_files_from_path (menu, desktop_id_table, file, new_id_prefix);

          /* Free id prefix */
          g_free (new_id_prefix);
        }
      else if (G_LIKELY (g_str_has_suffix (base_name, ".desktop")))
        {
          /* Create desktop-file id */
          if (G_LIKELY (id_prefix == NULL))
            desktop_id = g_strdup (base_name);
          else
            desktop_id = g_strjoin ("-", id_prefix, base_name, NULL);

          /* Insert into the files hash table if the desktop-file id does not exist there yet */
          if (G_LIKELY (g_hash_table_lookup (desktop_id_table, desktop_id) == NULL))
            g_hash_table_insert (desktop_id_table, desktop_id, g_file_get_uri (file));
          else
            g_free (desktop_id);
        }

      /* Free absolute path */
      g_free (base_name);

      /* Destroy file */
      g_object_unref (file);

      /* Destroy info */
      g_object_unref (file_info);
    }

  g_object_unref (enumerator);
}



static gboolean
collect_rules (GNode   *node,
               GSList **list)
{
  GarconMenuNodeType type;

  type = garcon_menu_node_tree_get_node_type (node);

  if (type == GARCON_MENU_NODE_TYPE_INCLUDE ||
      type == GARCON_MENU_NODE_TYPE_EXCLUDE)
    {
      *list = g_slist_append (*list, node);
    }

  return FALSE;
}



static void
garcon_menu_resolve_items (GarconMenu *menu,
                           GHashTable *desktop_id_table,
                           gboolean    only_unallocated)
{
  GSList  *rules = NULL;
  GSList  *iter;
  GList   *submenu;
  gboolean menu_only_unallocated = FALSE;

  g_return_if_fail (GARCON_IS_MENU (menu));

  menu_only_unallocated =
    garcon_menu_node_tree_get_boolean_child (menu->priv->tree,
                                             GARCON_MENU_NODE_TYPE_ONLY_UNALLOCATED);

  /* Resolve items in this menu (if it matches the only_unallocated argument.
   * This means that in the first pass, all items of menus without
   * <OnlyUnallocated /> are resolved and in the second pass, only items of
   * menus with <OnlyUnallocated /> are resolved */
  if (menu_only_unallocated == only_unallocated)
    {
      g_node_traverse (menu->priv->tree, G_IN_ORDER, G_TRAVERSE_ALL, 2,
                       (GNodeTraverseFunc) collect_rules, &rules);

      /* Iterate over all rules */
      for (iter = rules; iter != NULL; iter = g_slist_next (iter))
        {
          if (G_LIKELY (garcon_menu_node_tree_get_node_type (iter->data) == GARCON_MENU_NODE_TYPE_INCLUDE))
            {
              /* Resolve available items and match them against this rule */
              garcon_menu_resolve_items_by_rule (menu, desktop_id_table, iter->data);
            }
          else
            {
              /* Remove all items matching this exclude rule from the item pool */
              garcon_menu_item_pool_apply_exclude_rule (menu->priv->pool, iter->data);
            }
        }

      /* Cleanup */
      g_slist_free (rules);
    }

  /* Iterate over all submenus */
  for (submenu = menu->priv->submenus; submenu != NULL; submenu = g_list_next (submenu))
    {
      /* Resolve items of the submenu */
      garcon_menu_resolve_items (GARCON_MENU (submenu->data), desktop_id_table,
                                 only_unallocated);
    }
}



static void
garcon_menu_resolve_items_by_rule (GarconMenu *menu,
                                   GHashTable *desktop_id_table,
                                   GNode      *node)
{
  GarconMenuPair pair;

  g_return_if_fail (GARCON_IS_MENU (menu));

  /* Store menu and rule pointer in the pair */
  pair.first = menu;
  pair.second = node;

  /* Try to insert each of the collected desktop entry filenames into the menu */
  g_hash_table_foreach (desktop_id_table, (GHFunc) garcon_menu_resolve_item_by_rule, &pair);
}



static void
garcon_menu_resolve_item_by_rule (const gchar    *desktop_id,
                                  const gchar    *uri,
                                  GarconMenuPair *data)
{
  GarconMenuItem *item = NULL;
  GarconMenu     *menu = NULL;
  GNode          *node = NULL;
  gboolean        only_unallocated = FALSE;

  g_return_if_fail (GARCON_IS_MENU (data->first));
  g_return_if_fail (data->second != NULL);

  /* Restore menu and rule from the data pair */
  menu = data->first;
  node = data->second;

  /* Try to load the menu item from the cache */
  item = garcon_menu_item_cache_lookup (menu->priv->cache, uri, desktop_id);

  if (G_LIKELY (item != NULL))
    {
      only_unallocated = garcon_menu_node_tree_get_boolean_child (menu->priv->tree,
                                                                  GARCON_MENU_NODE_TYPE_ONLY_UNALLOCATED);

      /* Only include item if menu not only includes unallocated items
       * or if the item is not allocated yet */
      if (!only_unallocated || garcon_menu_item_get_allocated (item) == 0)
        {
          /* Add item to the pool if it matches the include rule */
          if (G_LIKELY (garcon_menu_node_tree_rule_matches (node, item)))
            garcon_menu_item_pool_insert (menu->priv->pool, item);
        }
    }
}



static void
garcon_menu_remove_deleted_menus (GarconMenu *menu)
{
  GarconMenu *submenu;
  GList      *iter;
  gboolean    deleted;

  g_return_if_fail (GARCON_IS_MENU (menu));

  /* Note: There's a limitation: if the root menu has a <Deleted/> we
   * can't just free the pointer here. Therefor we only check child menus. */

  for (iter = menu->priv->submenus; iter != NULL; iter = g_list_next (iter))
    {
      submenu = iter->data;

      /* Check whether there is a <Deleted/> element */
      deleted = garcon_menu_node_tree_get_boolean_child (submenu->priv->tree,
                                                         GARCON_MENU_NODE_TYPE_DELETED);

      /* Determine whether this submenu was deleted */
      if (G_LIKELY (submenu->priv->directory != NULL))
        deleted = deleted || garcon_menu_directory_get_hidden (submenu->priv->directory);

      /* Remove submenu if it is deleted, otherwise check submenus of the submenu */
      if (G_UNLIKELY (deleted))
        {
          /* Remove submenu from the list ... */
          menu->priv->submenus = g_list_remove_link (menu->priv->submenus, iter);

          /* ... and destroy it */
          g_object_unref (submenu);
        }
      else
        garcon_menu_remove_deleted_menus (submenu);
    }
}


/**
 * garcon_menu_get_item_pool:
 * @menu: a #GarconMenu.
 *
 * Get the item pool of the menu. This pool contains all items in this
 * menu (for that of its submenus).
 *
 * Returns: (transfer none): a #GarconMenuItemPool.
 */
GarconMenuItemPool*
garcon_menu_get_item_pool (GarconMenu *menu)
{
  g_return_val_if_fail (GARCON_IS_MENU (menu), NULL);

  return menu->priv->pool;
}



static void
items_collect (const gchar    *desktop_id,
               GarconMenuItem *item,
               GList         **listp)
{
  *listp = g_list_prepend (*listp, item);
}



/**
 * garcon_menu_get_items:
 * @menu: a #GarconMenu.
 *
 * Returns all #GarconMenuItem included in @menu. The items are
 * sorted by their display names in ascending order.
 *
 * The caller is responsible to free the returned list using
 * g_list_free() when no longer needed.
 *
 * Returns: (element-type GarconMenuItem) (transfer container): list
 * of #GarconMenuItem included in @menu.
 */
GList *
garcon_menu_get_items (GarconMenu *menu)
{
  GList *items = NULL;

  g_return_val_if_fail (GARCON_IS_MENU (menu), NULL);

  /* Collect the items in the pool */
  garcon_menu_item_pool_foreach (menu->priv->pool, (GHFunc) items_collect, &items);

  /* Sort items */
  items = g_list_sort (items, (GCompareFunc) garcon_menu_compare_items);

  return items;
}



static GNode *
garcon_menu_get_layout (GarconMenu *menu,
                        gboolean    default_only)
{
  GNode *layout = NULL;

  g_return_val_if_fail (GARCON_IS_MENU (menu), NULL);

  if (G_LIKELY (!default_only))
    {
      layout = garcon_menu_node_tree_get_child_node (menu->priv->tree,
                                                     GARCON_MENU_NODE_TYPE_LAYOUT,
                                                     TRUE);
    }

  if (layout == NULL)
    {
      layout = garcon_menu_node_tree_get_child_node (menu->priv->tree,
                                                     GARCON_MENU_NODE_TYPE_DEFAULT_LAYOUT,
                                                     TRUE);

      if (layout == NULL && menu->priv->parent != NULL)
        layout = garcon_menu_get_layout (menu->priv->parent, TRUE);
    }

  return layout;
}



static gboolean
layout_has_menuname (GNode       *layout,
                     const gchar *name)
{
  GList   *nodes;
  GList   *iter;
  gboolean has_menuname = FALSE;

  nodes = garcon_menu_node_tree_get_child_nodes (layout, GARCON_MENU_NODE_TYPE_MENUNAME,
                                                 FALSE);

  for (iter = g_list_first (nodes); !has_menuname && iter != NULL; iter = g_list_next (iter))
    if (g_str_equal (garcon_menu_node_tree_get_string (iter->data), name))
      has_menuname = TRUE;

  g_list_free (nodes);

  return has_menuname;
}



static gboolean
layout_has_filename (GNode       *layout,
                     const gchar *desktop_id)
{
  GList   *nodes;
  GList   *iter;
  gboolean has_filename = FALSE;

  nodes = garcon_menu_node_tree_get_child_nodes (layout, GARCON_MENU_NODE_TYPE_FILENAME,
                                                 FALSE);

  for (iter = g_list_first (nodes); !has_filename && iter != NULL; iter = g_list_next (iter))
    if (g_str_equal (garcon_menu_node_tree_get_string (iter->data), desktop_id))
      has_filename = TRUE;

  g_list_free (nodes);

  return has_filename;
}



static void
layout_elements_collect (GList **dest_list,
                         GList  *src_list,
                         GNode  *layout)
{
  GList *iter;

  for (iter = src_list; iter != NULL; iter = g_list_next (iter))
    {
      if (GARCON_IS_MENU (iter->data))
        {
          if (G_LIKELY (!layout_has_menuname (layout, garcon_menu_get_name (iter->data))))
            *dest_list = g_list_prepend (*dest_list, iter->data);
        }
      else if (GARCON_IS_MENU_ITEM (iter->data))
        {
          if (G_LIKELY (!layout_has_filename (layout, garcon_menu_item_get_desktop_id (iter->data))))
            *dest_list = g_list_prepend (*dest_list, iter->data);
        }
    }
}



/**
 * garcon_menu_get_elements:
 * @menu: a #GarconMenu.
 *
 * Get all the menu element in @menu. This contains sub menus, menu items
 * and separators.
 *
 * Returns a list of #GarconMenuItem or %NULL. Free the list with
 * g_list_free().
 *
 * Returns: (element-type GarconMenuItem) (nullable) (transfer container):
 */
GList *
garcon_menu_get_elements (GarconMenu *menu)
{
  GarconMenuLayoutMergeType merge_type;
  GarconMenuNodeType        type;
  GarconMenuItem           *item;
  GarconMenu               *submenu;
  GList                    *items = NULL;
  GList                    *menu_items;
  GNode                    *layout = NULL;
  GNode                    *node;

  g_return_val_if_fail (GARCON_IS_MENU (menu), NULL);

  /* Determine layout node */
  layout = garcon_menu_get_layout (menu, FALSE);

  /* There should always be a layout, otherwise GarconMenuMerger is broken */
  g_return_val_if_fail (layout != NULL, NULL);

  /* Process layout nodes in order */
  for (node = g_node_first_child (layout); node != NULL; node = g_node_next_sibling (node))
    {
      /* Determine layout node type */
      type = garcon_menu_node_tree_get_node_type (node);

      if (type == GARCON_MENU_NODE_TYPE_FILENAME)
        {
          /* Search for desktop ID in the item pool */
          item = garcon_menu_item_pool_lookup (menu->priv->pool,
                                               garcon_menu_node_tree_get_string (node));

          /* If the item with this desktop ID is included in the menu, append it to the list */
          if (G_LIKELY (item != NULL))
            items = g_list_prepend (items, item);
        }
      else if (type == GARCON_MENU_NODE_TYPE_MENUNAME)
        {
          /* Search submenu with this name */
          submenu = garcon_menu_get_menu_with_name (menu,
                                                    garcon_menu_node_tree_get_string (node));

          /* If there is such a menu, append it to the list */
          if (G_LIKELY (submenu != NULL))
            items = g_list_prepend (items, submenu);
        }
      else if (type == GARCON_MENU_NODE_TYPE_SEPARATOR)
        {
          /* Append separator to the list */
          items = g_list_prepend (items, garcon_menu_separator_get_default ());
        }
      else if (type == GARCON_MENU_NODE_TYPE_MERGE)
        {
          /* Determine merge type */
          merge_type = garcon_menu_node_tree_get_layout_merge_type (node);

          if (merge_type == GARCON_MENU_LAYOUT_MERGE_ALL)
            {
              /* Get all the submenus */
              menu_items = g_list_copy (menu->priv->submenus);

              /* Get all menu items of this menu */
              garcon_menu_item_pool_foreach (menu->priv->pool, (GHFunc) items_collect, &menu_items);

              /* Sort menu items */
              menu_items = g_list_sort (menu_items, (GCompareFunc) garcon_menu_compare_items);

              /* Prepend menu items to the returned item list */
              layout_elements_collect (&items, menu_items, layout);

              /* Cleanup */
              g_list_free (menu_items);
            }
          else if (merge_type == GARCON_MENU_LAYOUT_MERGE_FILES)
            {
              /* Get all menu items of this menu */
              menu_items = garcon_menu_get_items (menu);

              /* Prepend menu items to the returned item list */
              layout_elements_collect (&items, menu_items, layout);

              /* Cleanup */
              g_list_free (menu_items);
            }
          else if (merge_type == GARCON_MENU_LAYOUT_MERGE_MENUS)
            {
              /* Get all submenus */
              menu_items = garcon_menu_get_menus (menu);

              /* Prepend submenus to the returned item list */
              layout_elements_collect (&items, menu_items, layout);

              /* Cleanup */
              g_list_free (menu_items);
            }
        }
    }

  return g_list_reverse (items);
}



static gint
garcon_menu_compare_items (gconstpointer *a,
                           gconstpointer *b)
{
  gchar *casefold_a, *casefold_b;
  gint   result;

  /* do case insensitive sorting, see bug #10594 */
  /* TODO: this function is called often, maybe catch the casefolded name */
  casefold_a = g_utf8_casefold (garcon_menu_element_get_name (GARCON_MENU_ELEMENT (a)), -1);
  casefold_b = g_utf8_casefold (garcon_menu_element_get_name (GARCON_MENU_ELEMENT (b)), -1);

  result = g_utf8_collate (casefold_a, casefold_b);

  g_free (casefold_a);
  g_free (casefold_b);

  return result;
}



static const gchar*
garcon_menu_get_element_name (GarconMenuElement *element)
{
  GarconMenu    *menu;
  const gchar   *name = NULL;

  g_return_val_if_fail (GARCON_IS_MENU (element), NULL);

  menu = GARCON_MENU (element);

  /* Try directory name first */
  if (menu->priv->directory != NULL)
    name = garcon_menu_directory_get_name (menu->priv->directory);

  /* Otherwise use the menu name as a fallback */
  if (name == NULL)
    name = garcon_menu_get_name (menu);

  return name;
}



static const gchar*
garcon_menu_get_element_comment (GarconMenuElement *element)
{
  GarconMenu *menu;

  g_return_val_if_fail (GARCON_IS_MENU (element), NULL);

  menu = GARCON_MENU (element);

  if (menu->priv->directory == NULL)
    return NULL;
  else
    return garcon_menu_directory_get_comment (menu->priv->directory);
}



static const gchar*
garcon_menu_get_element_icon_name (GarconMenuElement *element)
{
  GarconMenu *menu;

  g_return_val_if_fail (GARCON_IS_MENU (element), NULL);

  menu = GARCON_MENU (element);

  if (menu->priv->directory == NULL)
    return NULL;
  else
    return garcon_menu_directory_get_icon_name (menu->priv->directory);
}



static gboolean
garcon_menu_get_element_visible (GarconMenuElement *element)
{
  GarconMenu *menu;
  GList      *items;
  GList      *iter;
  gboolean    visible = FALSE;

  g_return_val_if_fail (GARCON_IS_MENU (element), FALSE);

  menu = GARCON_MENU (element);

  if (menu->priv->directory != NULL)
    {
      if (!garcon_menu_directory_get_visible (menu->priv->directory))
        return FALSE;
    }

  /* if a menu has no visible children it shouldn't be visible */
  items = garcon_menu_get_elements (menu);
  for (iter = items; visible != TRUE && iter != NULL; iter = g_list_next (iter))
    {
      if (garcon_menu_element_get_visible (GARCON_MENU_ELEMENT (iter->data)))
        visible = TRUE;
    }

  g_list_free (items);
  return visible;
}



static gboolean
garcon_menu_get_element_show_in_environment (GarconMenuElement *element)
{
  GarconMenu *menu;

  g_return_val_if_fail (GARCON_IS_MENU (element), FALSE);

  menu = GARCON_MENU (element);

  if (menu->priv->directory == NULL)
    return FALSE;
  else
    return garcon_menu_directory_get_show_in_environment (menu->priv->directory);
}



static gboolean
garcon_menu_get_element_no_display (GarconMenuElement *element)
{
  GarconMenu *menu;

  g_return_val_if_fail (GARCON_IS_MENU (element), FALSE);

  menu = GARCON_MENU (element);

  if (menu->priv->directory == NULL)
    return FALSE;
  else
    return garcon_menu_directory_get_no_display (menu->priv->directory);
}



static gboolean
garcon_menu_get_element_equal (GarconMenuElement *element,
                               GarconMenuElement *other)
{
  g_return_val_if_fail (GARCON_IS_MENU (element), FALSE);
  g_return_val_if_fail (GARCON_IS_MENU (other), FALSE);

  return GARCON_MENU (element) == GARCON_MENU (other);
}



static void
garcon_menu_start_monitoring (GarconMenu *menu)
{
  GList *lp;

  g_return_if_fail (GARCON_IS_MENU (menu));

  /* Let only the root menu monitor menu files, merge files/directories and app dirs */
  if (menu->priv->parent == NULL)
    {
      garcon_menu_monitor_menu_files (menu);

      garcon_menu_monitor_files (menu, menu->priv->merge_files,
                                 garcon_menu_merge_file_changed);

      garcon_menu_monitor_files (menu, menu->priv->merge_dirs,
                                 garcon_menu_merge_dir_changed);

      garcon_menu_monitor_app_dirs (menu);
    }

  garcon_menu_monitor_directory_dirs (menu);

  /* Recurse into submenus */
  for (lp = menu->priv->submenus; lp != NULL; lp = lp->next)
    garcon_menu_start_monitoring (lp->data);

}



static void
garcon_menu_stop_monitoring (GarconMenu *menu)
{
  GList *lp;

  g_return_if_fail (GARCON_IS_MENU (menu));

  /* Recurse into submenus */
  for (lp = menu->priv->submenus; lp != NULL; lp = lp->next)
    garcon_menu_stop_monitoring (lp->data);

  /* Disconnect and destroy all monitors */
  for (lp = menu->priv->monitors; lp != NULL; lp = lp->next)
    {
      g_signal_handlers_disconnect_matched (lp->data, G_SIGNAL_MATCH_DATA,
                                            0, 0, NULL, NULL, menu);
      g_object_unref (lp->data);
    }

  /* Free the monitor list */
  g_list_free (menu->priv->monitors);
  menu->priv->monitors = NULL;

  /* Stop the idle source for handling file changes from being invoked */
  if (menu->priv->file_changed_idle != 0)
    {
      g_source_remove (menu->priv->file_changed_idle);
      menu->priv->file_changed_idle = 0;
    }

  /* Free the hash table for merging consecutive file change events */
  _garcon_g_slist_free_full (menu->priv->changed_files, g_object_unref);
  menu->priv->changed_files = NULL;
}



static void
garcon_menu_monitor_menu_files (GarconMenu *menu)
{
  GFileMonitor *monitor;
  const gchar  *prefix;
  GFile        *file;
  gchar        *relative_filename;
  gchar       **paths;
  gint          n;

  g_return_if_fail (GARCON_IS_MENU (menu));

  if (menu->priv->uses_custom_path)
    {
      /* Monitor the root .menu file */
      monitor = g_file_monitor (menu->priv->file, G_FILE_MONITOR_NONE, NULL, NULL);
      if (monitor != NULL)
        {
          menu->priv->monitors = g_list_prepend (menu->priv->monitors, monitor);
          g_signal_connect_swapped (monitor, "changed",
                                    G_CALLBACK (garcon_menu_file_changed), menu);
        }
    }
  else
    {
      /* Build ${XDG_MENU_PREFIX}applications.menu filename */
      prefix = g_getenv ("XDG_MENU_PREFIX");
      relative_filename = g_strconcat ("menus", G_DIR_SEPARATOR_S,
                                       prefix != NULL ? prefix : _GARCON_DEFAULT_MENU_PREFIX,
                                       "applications.menu", NULL);

      /* Monitor all application menu candidates */
      paths = garcon_config_build_paths (relative_filename);

      for (n = g_strv_length (paths)-1; paths != NULL && n >= 0; --n)
        {
          file = g_file_new_for_path (paths[n]);

          monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, NULL);
          if (monitor != NULL)
            {
              menu->priv->monitors = g_list_prepend (menu->priv->monitors, monitor);
              g_signal_connect_swapped (monitor, "changed",
                                        G_CALLBACK (garcon_menu_file_changed), menu);
            }

          g_object_unref (file);
        }

      /* clean up strings */
      g_strfreev (paths);
      g_free (relative_filename);
    }
}



static gint
find_file_monitor (GFileMonitor *monitor,
                   GFile        *file)
{
  GFile *monitored_file;

  monitored_file = g_object_get_qdata (G_OBJECT (monitor), garcon_menu_file_quark);

  if (monitored_file != NULL && g_file_equal (monitored_file, file))
    return 0;
  else
    return -1;
}



static void
garcon_menu_monitor_files (GarconMenu *menu,
                           GList      *files,
                           gpointer    callback)
{
  GFileMonitor *monitor;
  GList        *lp;

  g_return_if_fail (GARCON_IS_MENU (menu));
  g_return_if_fail (menu->priv->parent == NULL);

  /* Monitor all files from the list */
  for (lp = files; lp != NULL; lp = lp->next)
    {
      /* Monitor files only if they are not being monitored already */
      if (g_list_find_custom (menu->priv->monitors, lp->data,
                              (GCompareFunc) find_file_monitor) == NULL)
        {
          /* Try to monitor the file */
          monitor = g_file_monitor (lp->data, G_FILE_MONITOR_NONE, NULL, NULL);
          if (monitor != NULL)
            {
              /* Associate the monitor with the monitored file */
              g_object_set_qdata_full (G_OBJECT (monitor), garcon_menu_file_quark,
                                       g_object_ref (lp->data), g_object_unref);

              /* Add the monitor to the list of monitors belonging to the menu */
              menu->priv->monitors = g_list_prepend (menu->priv->monitors, monitor);

              /* Make sure we are notified when the file changes */
              g_signal_connect_swapped (monitor, "changed", G_CALLBACK (callback), menu);
            }
        }
    }
}



static void
garcon_menu_monitor_app_dirs (GarconMenu *menu)
{
  GFile *dir;
  GList *app_dirs;
  GList *dirs = NULL;
  GList *lp;

  g_return_if_fail (GARCON_IS_MENU (menu));
  g_return_if_fail (menu->priv->parent == NULL);

  g_return_if_fail (GARCON_IS_MENU (menu));

  /* Determine all application directories we are interested in for this menu */
  app_dirs = garcon_menu_get_app_dirs (menu, TRUE);

  /* Transform app dir filenames into GFile objects, resolving filenames
   * relative to the menu file itself */
  for (lp = app_dirs; lp != NULL; lp = lp->next)
    {
      dir = _garcon_file_new_relative_to_file (lp->data, menu->priv->file);
      dirs = g_list_prepend (dirs, dir);
    }

  /* Monitor the app dirs */
  garcon_menu_monitor_files (menu, dirs, garcon_menu_app_dir_changed);

  /* Release the allocated GFiles and free the list */
  _garcon_g_list_free_full (dirs, g_object_unref);

  /* Free app dir list */
  g_list_free (app_dirs);
}



static void
garcon_menu_monitor_directory_dirs (GarconMenu *menu)
{
  GFileMonitor *monitor;
  GFile        *file;
  GFile        *dir;
  GList        *directory_files;
  GList        *directory_dirs;
  GList        *dp;
  GList        *lp;

  g_return_if_fail (GARCON_IS_MENU (menu));

  /* Determine all .directory files we are interested in for this menu */
  directory_files = garcon_menu_get_directories (menu);

  /* Determine all .directory lookup dirs for this menu */
  directory_dirs = garcon_menu_get_directory_dirs (menu);

  /* Monitor potential .directory files */
  for (lp = directory_files; lp != NULL; lp = lp->next)
    {
      for (dp = directory_dirs; dp != NULL; dp = dp->next)
        {
          dir = _garcon_file_new_relative_to_file (dp->data, menu->priv->file);
          file = _garcon_file_new_relative_to_file (lp->data, dir);

          /* Only try to monitor the .directory file if we don't do that already */
          if (g_list_find_custom (menu->priv->monitors, file,
                                  (GCompareFunc) find_file_monitor) == NULL)
            {
              /* Try to monitor the file */
              monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, NULL);
              if (monitor != NULL)
                {
                  /* Associate the monitor with the monitored file */
                  g_object_set_qdata_full (G_OBJECT (monitor), garcon_menu_file_quark,
                                           g_object_ref (file), g_object_unref);

                  /* Add the monitor to the list of monitors belonging to the menu */
                  menu->priv->monitors = g_list_prepend (menu->priv->monitors, monitor);

                  /* Make sure we are notified when the file changes */
                  g_signal_connect_swapped (monitor, "changed",
                                            G_CALLBACK (garcon_menu_directory_file_changed),
                                            menu);
                }
            }

          g_object_unref (file);
          g_object_unref (dir);
        }
    }

  /* Free lists */
  g_list_free (directory_dirs);
  g_list_free (directory_files);
}



#ifdef DEBUG
static void
garcon_menu_debug (GFile             *file,
                   GFileMonitorEvent  event_type,
                   const gchar       *comment)
{
  gchar *path;
  gchar *msg = NULL;

  if (file != NULL)
    {
      path = g_file_get_path (file);
      msg = g_strdup_printf ("%s (%d, %s)", comment, event_type, path);
      g_free (path);

      comment = msg;
    }

  g_print ("%s\n", comment);
  g_free (msg);
}
#else
#define garcon_menu_debug(...) G_STMT_START{ (void)0; }G_STMT_END
#endif




static gboolean
garcon_menu_file_emit_reload_required_idle (gpointer data)
{
  GarconMenu *menu = GARCON_MENU (data);

  g_return_val_if_fail (GARCON_IS_MENU (menu), FALSE);

  menu->priv->idle_reload_required_id = 0;

  garcon_menu_debug (NULL, 0, "emit reload-required");

  g_signal_emit (menu, menu_signals[RELOAD_REQUIRED], 0);

  return FALSE;
}



static void
garcon_menu_file_emit_reload_required (GarconMenu *menu)
{
  if (menu->priv->idle_reload_required_id == 0)
    {
      garcon_menu_debug (NULL, 0, "schedule idle menu reload");
      menu->priv->idle_reload_required_id =
        g_idle_add (garcon_menu_file_emit_reload_required_idle, menu);
    }
}



static void
garcon_menu_file_changed (GarconMenu       *menu,
                          GFile            *file,
                          GFile            *other_file,
                          GFileMonitorEvent event_type,
                          GFileMonitor     *monitor)
{
  const gchar *prefix;
  gboolean     higher_priority = FALSE;
  gboolean     lower_priority = FALSE;
  GFile       *menu_file;
  gchar      **paths;
  gchar       *relative_filename;
  guint        n;

  g_return_if_fail (GARCON_IS_MENU (menu));
  g_return_if_fail (menu->priv->parent == NULL);

  g_mutex_lock (&menu->priv->load_lock);

  /* Quick check: reloading is needed if the menu file being used has changed */
  if (g_file_equal (menu->priv->file, file))
    {
      garcon_menu_debug (file, event_type, "menu changed");
      garcon_menu_file_emit_reload_required (menu);
      g_mutex_unlock (&menu->priv->load_lock);
      return;
    }

  /* If we receive a delete event here, then abort, because there is no
   * need to refresh if a menu file is removed that we're not using because
   * it is lower in priority (else we'd be using it already) */
  if (event_type == G_FILE_MONITOR_EVENT_DELETED)
    {
      g_mutex_unlock (&menu->priv->load_lock);
      return;
    }

  /* Build the ${XDG_MENU_PREFIX}applications.menu filename */
  prefix = g_getenv ("XDG_MENU_PREFIX");
  relative_filename = g_strconcat ("menus", G_DIR_SEPARATOR_S,
                                   prefix != NULL ? prefix : _GARCON_DEFAULT_MENU_PREFIX,
                                   "applications.menu", NULL);

  /* Get XDG config paths for the root spec (e.g. menus/xfce-applications.menu) */
  paths = garcon_config_build_paths (relative_filename);

  /* Check if the event file has higher priority than the file currently being used */
  for (n = 0;
       !lower_priority && !higher_priority && paths != NULL && paths[n] != NULL;
       ++n)
    {
      menu_file = g_file_new_for_path (paths[n]);

      if (g_file_equal (menu_file, menu->priv->file))
        {
          /* the menu's file comes before the changed file in the load
           * priority order, so the changed file has a lower priority */
          lower_priority = TRUE;
        }
      else if (g_file_equal (menu_file, file))
        {
          /* the changed file comes before the menu's file in the load
           * priority order, so the changed file has a higher priority */
          higher_priority = TRUE;
        }

      g_object_unref (menu_file);
    }

  /* clean up */
  g_free (relative_filename);

  /* If the event file has higher priority, a menu reload is needed */
  if (!lower_priority
      && higher_priority)
    {
      garcon_menu_debug (file, event_type, "new menu has higher prio");
      garcon_menu_file_emit_reload_required (menu);
    }

  g_mutex_unlock (&menu->priv->load_lock);
}



static void
garcon_menu_merge_file_changed (GarconMenu       *menu,
                                GFile            *file,
                                GFile            *other_file,
                                GFileMonitorEvent event_type,
                                GFileMonitor     *monitor)
{
  g_return_if_fail (GARCON_IS_MENU (menu));
  g_return_if_fail (menu->priv->parent == NULL);

  garcon_menu_debug (file, event_type, "merge file changed");
  garcon_menu_file_emit_reload_required (menu);
}



static void
garcon_menu_merge_dir_changed (GarconMenu       *menu,
                               GFile            *file,
                               GFile            *other_file,
                               GFileMonitorEvent event_type,
                               GFileMonitor     *monitor)
{
  g_return_if_fail (GARCON_IS_MENU (menu));
  g_return_if_fail (menu->priv->parent == NULL);

  garcon_menu_debug (file, event_type, "merge dir changed");
  garcon_menu_file_emit_reload_required (menu);
}



static gboolean
garcon_menu_process_file_changes (gpointer user_data)
{
  GarconMenu *menu = user_data;
  GarconMenuItem *item;
  GFileType       file_type;
  gboolean        affects_the_outside = FALSE;
  gboolean        stop_processing = FALSE;
  GFile          *file;
  GSList         *lp;
  gchar          *path;

  g_return_val_if_fail (GARCON_IS_MENU (menu), FALSE);
  g_return_val_if_fail (menu->priv->parent == NULL, FALSE);

  g_mutex_lock (&menu->priv->load_lock);

  for (lp = menu->priv->changed_files; !stop_processing && lp != NULL; lp = lp->next)
    {
      file = G_FILE (lp->data);

      /* query the type of the changed file */
      file_type = g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL);

      if (file_type == G_FILE_TYPE_DIRECTORY)
        {
          /* in this situation, an app dir could have
           * - become unreadable for the current user
           * - been deleted
           * - created (possibly inside an existing one)
           * this is not trivial to handle, so we simply enforce a
           * menu reload to deal with the changes */
          garcon_menu_debug (file, 0, "proccess directory change");
          garcon_menu_file_emit_reload_required (menu);
          stop_processing = TRUE;
        }
      else
        {
          path = g_file_get_path (file);
          if (path != NULL && g_str_has_suffix (path, ".desktop"))
            {
              /* a regular file changed, try to find the corresponding menu item */
              item = garcon_menu_find_file_item (menu, file);
              if (item != NULL)
                {
                  /* try to reload the item */
                  if (garcon_menu_item_reload (item, &affects_the_outside, NULL))
                    {
                      if (affects_the_outside)
                        {
                          /* if the categories changed, the item might have to be
                           * moved around between different menus. this is slightly
                           * more complicated than one would first think, so just
                           * enforce a complete menu reload for now */
                          garcon_menu_file_emit_reload_required (menu);
                          stop_processing = TRUE;
                        }
                      else
                        {
                          /* remove the item from the desktop item cache so we are forced
                           * to reload it from disk the next time */
                          garcon_menu_item_cache_invalidate_file (menu->priv->cache, file);

                          /* nothing else to do here. the item should emit a 'changed'
                           * signal to which users of this library can react */
                        }
                    }
                  else
                    {
                      /* remove the item from the desktop item cache so we are forced
                       * to reload it from disk the next time */
                      garcon_menu_item_cache_invalidate_file (menu->priv->cache, file);

                      /* failed to reload the menu item. this can have many reasons,
                       * one of them being that the file permissions might have changed
                       * or that the file was deleted. handling most situations can be very
                       * tricky, so, again, we just enfore a menu reload until we have
                       * something better */
                      garcon_menu_debug (file, 0, "auto reload failed");
                      garcon_menu_file_emit_reload_required (menu);
                      stop_processing = TRUE;
                    }
                }
              else
                {
                  /* there could be a lot of stuff happening here. seriously, this
                   * stuff is complicated. for now, simply enforce a complete reload
                   * of the menu structure */
                  garcon_menu_debug (file, 0, "unknown file, full reload");
                  garcon_menu_file_emit_reload_required (menu);
                  stop_processing = TRUE;
                }
            }
          g_free (path);
        }
    }

  /* reset the changed files list, all events processed */
  _garcon_g_slist_free_full (menu->priv->changed_files, g_object_unref);
  menu->priv->changed_files = NULL;

  /* reset the idle source ID */
  menu->priv->file_changed_idle = 0;

  g_mutex_unlock (&menu->priv->load_lock);

  /* remove the idle source */
  return FALSE;
}



static gint
compare_files (gconstpointer a,
               gconstpointer b)
{
  return g_file_equal (G_FILE (a), G_FILE (b)) ? 0 : 1;
}



static void
garcon_menu_app_dir_changed (GarconMenu       *menu,
                             GFile            *file,
                             GFile            *other_file,
                             GFileMonitorEvent event_type,
                             GFileMonitor     *monitor)
{
  GarconMenuItem *item;
  GFileType       file_type;

  g_return_if_fail (GARCON_IS_MENU (menu));
  g_return_if_fail (menu->priv->parent == NULL);

  g_mutex_lock (&menu->priv->load_lock);

  if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT
      || event_type == G_FILE_MONITOR_EVENT_CREATED
      || event_type == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED)
    {
      /* add the file to the changed files queue if we have no change event for
       * it queued yet */
      if (g_slist_find_custom (menu->priv->changed_files, file, compare_files) == NULL)
        {
          garcon_menu_debug (file, event_type, "add file to changed-queue");
          menu->priv->changed_files = g_slist_prepend (menu->priv->changed_files,
                                                       g_object_ref (file));

          /* register the idle handler if it is not active yet */
          if (menu->priv->file_changed_idle == 0)
            {
              garcon_menu_debug (NULL, 0, "schedule process file changes");
              menu->priv->file_changed_idle =
                g_idle_add (garcon_menu_process_file_changes, menu);
            }
        }
    }
  else if (event_type == G_FILE_MONITOR_EVENT_DELETED)
    {
      /* query the type of the changed file */
      file_type = g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL);

      if (file_type == G_FILE_TYPE_DIRECTORY)
        {
          /* an existing app dir (or a subdirectory) has been deleted. we
           * could remove all the items that are in use and reside inside
           * this root directory. but for now... enforce a menu reload! */
          garcon_menu_debug (file, event_type, "app dir deleted");
          garcon_menu_file_emit_reload_required (menu);
        }
      else
        {
          /* a regular file was deleted, try to find the corresponding menu item */
          item = garcon_menu_find_file_item (menu, file);
          if (item != NULL)
            {
              /* remove the item from the desktop item cache so we are forced
               * to reload it from disk the next time */
              garcon_menu_item_cache_invalidate_file (menu->priv->cache, file);

              /* ok, so a .desktop file was removed. of course we don't know
               * yet whether there is a replacement in another app dir
               * with lower priority. we could try to find out but for now
               * it's easier to simply enforce a menu reload */
              garcon_menu_debug (file, event_type, "file deleted");
              garcon_menu_file_emit_reload_required (menu);
            }
          else
            {
              /* the deleted file hasn't been in use anyway, so removing it
               * doesn't change anything. so we have nothing to do for a
               * change, no f****ing menu reload! */
            }
        }
    }

  g_mutex_unlock (&menu->priv->load_lock);
}



static void
garcon_menu_directory_file_changed (GarconMenu       *menu,
                                    GFile            *file,
                                    GFile            *other_file,
                                    GFileMonitorEvent event_type,
                                    GFileMonitor     *monitor)
{
  GarconMenuDirectory *old_directory = NULL;

  g_return_if_fail (GARCON_IS_MENU (menu));

  g_mutex_lock (&menu->priv->load_lock);

  if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT
      || event_type == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED
      || event_type == G_FILE_MONITOR_EVENT_DELETED
      || event_type == G_FILE_MONITOR_EVENT_CREATED)
    {
      /* take a reference on the current menu directory */
      if (menu->priv->directory != NULL)
        old_directory = g_object_ref (menu->priv->directory);

      /* reset the menu directory of the menu and load a new one */
      garcon_menu_resolve_directory (menu, NULL, FALSE);

      /* Only emit the event if something changed (see bug #8671) */
      if (event_type != G_FILE_MONITOR_EVENT_DELETED
          || (old_directory == NULL) != (menu->priv->directory == NULL)
          || !garcon_menu_directory_equal (old_directory, menu->priv->directory))
        {
          garcon_menu_debug (file, event_type, "directory changed");

          /* Notify listeners about the old and new menu directories */
          g_signal_emit (menu, menu_signals[DIRECTORY_CHANGED], 0,
                         old_directory, menu->priv->directory);
        }

      /* release the old menu directory we no longer need */
      if (old_directory != NULL)
        g_object_unref (old_directory);
    }

  g_mutex_unlock (&menu->priv->load_lock);
}



static GarconMenuItem *
garcon_menu_find_file_item (GarconMenu *menu,
                            GFile      *file)
{
  GarconMenuItem *item = NULL;
  GList          *lp;

  g_return_val_if_fail (GARCON_IS_MENU (menu), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  item = garcon_menu_item_pool_lookup_file (menu->priv->pool, file);

  if (item == NULL)
    {
      for (lp = menu->priv->submenus; item == NULL && lp != NULL; lp = lp->next)
        item = garcon_menu_find_file_item (lp->data, file);
    }

  return item;

}
