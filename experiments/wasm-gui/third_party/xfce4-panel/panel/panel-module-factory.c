/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <common/panel-private.h>
#include <common/panel-debug.h>

#include <libxfce4panel/libxfce4panel.h>

#include <panel/panel-module.h>
#include <panel/panel-module-factory.h>

#define PANEL_PLUGINS_DATA_DIR     (DATADIR G_DIR_SEPARATOR_S "panel" G_DIR_SEPARATOR_S "plugins")
#define PANEL_PLUGINS_DATA_DIR_OLD (DATADIR G_DIR_SEPARATOR_S "panel-plugins")



static void     panel_module_factory_finalize        (GObject                  *object);
static void     panel_module_factory_load_modules    (PanelModuleFactory       *factory,
                                                      gboolean                  warn_if_known);
static gboolean panel_module_factory_modules_cleanup (gpointer                  key,
                                                      gpointer                  value,
                                                      gpointer                  user_data);
static void     panel_module_factory_remove_plugin   (gpointer                  user_data,
                                                      GObject                  *where_the_object_was);



enum
{
  UNIQUE_CHANGED,
  LAST_SIGNAL
};

struct _PanelModuleFactoryClass
{
  GObjectClass __parent__;
};

struct _PanelModuleFactory
{
  GObject  __parent__;

  /* relation for name -> PanelModule */
  GHashTable *modules;

  /* all plugins in all windows */
  GSList     *plugins;

  /* if the factory contains the launcher plugin */
  guint       has_launcher : 1;
};



static guint    factory_signals[LAST_SIGNAL];
static gboolean force_all_external = FALSE;



G_DEFINE_TYPE (PanelModuleFactory, panel_module_factory, G_TYPE_OBJECT)



static void
panel_module_factory_class_init (PanelModuleFactoryClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = panel_module_factory_finalize;

  /**
   * Emitted when the unique status of one of the modules changed.
   **/
  factory_signals[UNIQUE_CHANGED] =
    g_signal_new (g_intern_static_string ("unique-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, PANEL_TYPE_MODULE);
}



static void
panel_module_factory_init (PanelModuleFactory *factory)
{
  factory->has_launcher = FALSE;
  factory->modules = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            g_free, g_object_unref);

  /* load all the modules */
  panel_module_factory_load_modules (factory, TRUE);
}



static void
panel_module_factory_finalize (GObject *object)
{
  PanelModuleFactory *factory = PANEL_MODULE_FACTORY (object);

  g_hash_table_destroy (factory->modules);
  g_slist_free (factory->plugins);

  (*G_OBJECT_CLASS (panel_module_factory_parent_class)->finalize) (object);
}



static void
panel_module_factory_load_modules_dir (PanelModuleFactory *factory,
                                       const gchar        *path,
                                       gboolean            warn_if_known)
{
  GDir        *dir;
  const gchar *name, *p;
  gchar       *filename;
  PanelModule *module;
  gchar       *internal_name;

  /* try to open the directory */
  dir = g_dir_open (path, 0, NULL);
  if (G_UNLIKELY (dir == NULL))
    return;

  panel_debug (PANEL_DEBUG_MODULE_FACTORY, "reading %s", path);

  /* walk the directory */
  for (;;)
    {
      /* get name of the next file */
      name = g_dir_read_name (dir);
      if (G_UNLIKELY (name == NULL))
        break;

      /* continue if it's not a desktop file */
      if (!g_str_has_suffix (name, ".desktop"))
        continue;

      /* create the full .desktop filename */
      filename = g_build_filename (path, name, NULL);

      /* find the dot in the name, this cannot
       * fail since it passed the .desktop suffix check */
      p = strrchr (name, '.');

      /* get the new module internal name */
      internal_name = g_strndup (name, p - name);

      /* check if the modules name is already loaded */
      if (g_hash_table_lookup (factory->modules, internal_name) != NULL)
        {
          if (warn_if_known)
            panel_debug (PANEL_DEBUG_MODULE_FACTORY, "Another plugin already registered with "
                         "the internal name \"%s\".", internal_name);

          goto exists;
        }

      /* try to load the module */
      module = panel_module_new_from_desktop_file (filename,
                                                   internal_name,
                                                   force_all_external);

      if (G_LIKELY (module != NULL))
        {
          /* add the module to the internal list */
          g_hash_table_insert (factory->modules, internal_name, module);

          /* check if this is the launcher */
          if (!factory->has_launcher)
            factory->has_launcher = g_strcmp0 (LAUNCHER_PLUGIN_NAME, internal_name) == 0;
        }
      else
        {
          exists:
          g_free (internal_name);
        }

      g_free (filename);
    }

  g_dir_close (dir);
}



static void
panel_module_factory_load_modules (PanelModuleFactory *factory,
                                   gboolean            warn_if_known)
{
  panel_return_if_fail (PANEL_IS_MODULE_FACTORY (factory));

  /* load from the new and old location */
  panel_module_factory_load_modules_dir (factory, PANEL_PLUGINS_DATA_DIR, warn_if_known);
  panel_module_factory_load_modules_dir (factory, PANEL_PLUGINS_DATA_DIR_OLD, warn_if_known);
}



static gboolean
panel_module_factory_modules_cleanup (gpointer key,
                                      gpointer value,
                                      gpointer user_data)
{
  PanelModuleFactory *factory = PANEL_MODULE_FACTORY (user_data);
  PanelModule        *module = PANEL_MODULE (value);
  gboolean            remove_from_table;

  panel_return_val_if_fail (PANEL_IS_MODULE (module), TRUE);
  panel_return_val_if_fail (PANEL_IS_MODULE_FACTORY (factory), TRUE);

  /* check if the executable/library still exists */
  remove_from_table = !panel_module_is_valid (module);

  /* if we're going to remove this item, check if it is the launcher */
  if (remove_from_table
      && g_strcmp0 (LAUNCHER_PLUGIN_NAME,
                    panel_module_get_name (module)) == 0)
    factory->has_launcher = FALSE;

  return remove_from_table;
}



static void
panel_module_factory_remove_plugin (gpointer  user_data,
                                    GObject  *where_the_object_was)
{
  PanelModuleFactory *factory = PANEL_MODULE_FACTORY (user_data);

  /* remove the plugin from the internal list */
  factory->plugins = g_slist_remove (factory->plugins, where_the_object_was);
}



static inline gboolean
panel_module_factory_unique_id_exists (PanelModuleFactory *factory,
                                       gint                unique_id)
{
  GSList *li;

  for (li = factory->plugins; li != NULL; li = li->next)
    if (xfce_panel_plugin_provider_get_unique_id (
        XFCE_PANEL_PLUGIN_PROVIDER (li->data)) == unique_id)
      return TRUE;

  return FALSE;
}



PanelModuleFactory *
panel_module_factory_get (void)
{
  static PanelModuleFactory *factory = NULL;

  if (G_LIKELY (factory))
    {
      g_object_ref (G_OBJECT (factory));
    }
  else
    {
      /* create new object */
      factory = g_object_new (PANEL_TYPE_MODULE_FACTORY, NULL);
      g_object_add_weak_pointer (G_OBJECT (factory), (gpointer) &factory);
    }

  return factory;
}



void
panel_module_factory_force_all_external (void)
{
  force_all_external = TRUE;

  panel_debug (PANEL_DEBUG_MODULE_FACTORY,
               "forcing all plugins to run external");

#ifndef NDEBUG
  if (!panel_debug_has_domain (PANEL_DEBUG_YES))
    g_message ("Forcing all plugins to run external.");
#endif
}



gboolean
panel_module_factory_has_launcher (PanelModuleFactory *factory)
{
  panel_return_val_if_fail (PANEL_IS_MODULE_FACTORY (factory), FALSE);

  return factory->has_launcher;
}



void
panel_module_factory_emit_unique_changed (PanelModule *module)
{
  PanelModuleFactory *factory;

  panel_return_if_fail (PANEL_IS_MODULE (module));

  factory = panel_module_factory_get ();
  g_signal_emit (G_OBJECT (factory), factory_signals[UNIQUE_CHANGED], 0, module);
  g_object_unref (G_OBJECT (factory));

}



GList *
panel_module_factory_get_modules (PanelModuleFactory *factory)
{
  panel_return_val_if_fail (PANEL_IS_MODULE_FACTORY (factory), NULL);

  /* add new modules to the hash table */
  panel_module_factory_load_modules (factory, FALSE);

  /* remove modules that are not found on the harddisk */
  g_hash_table_foreach_remove (factory->modules,
      panel_module_factory_modules_cleanup, factory);

  return g_hash_table_get_values (factory->modules);
}



gboolean
panel_module_factory_has_module (PanelModuleFactory *factory,
                                 const gchar        *name)
{
  panel_return_val_if_fail (PANEL_IS_MODULE_FACTORY (factory), FALSE);
  panel_return_val_if_fail (name != NULL, FALSE);

  return !!(g_hash_table_lookup (factory->modules, name) != NULL);
}



GSList *
panel_module_factory_get_plugins (PanelModuleFactory *factory,
                                  const gchar        *plugin_name)
{
  GSList *li, *plugins = NULL;
  gchar  *unique_name;

  panel_return_val_if_fail (PANEL_IS_MODULE_FACTORY (factory), NULL);
  panel_return_val_if_fail (plugin_name != NULL, NULL);

  /* first assume a global plugin name is provided (ie. no name with id) */
  for (li = factory->plugins; li != NULL; li = li->next)
    {
      panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (li->data), NULL);
      if (strcmp (xfce_panel_plugin_provider_get_name (li->data), plugin_name) == 0)
        plugins = g_slist_prepend (plugins, li->data);
    }

  /* try the unique plugin name (with id) if nothing is found */
  for (li = factory->plugins; plugins == NULL && li != NULL; li = li->next)
    {
      panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (li->data), NULL);
      unique_name = g_strdup_printf ("%s-%d", xfce_panel_plugin_provider_get_name (li->data),
                                     xfce_panel_plugin_provider_get_unique_id (li->data));

      if (strcmp (unique_name, plugin_name) == 0)
        plugins = g_slist_prepend (plugins, li->data);

      g_free (unique_name);
    }

  return plugins;
}



GtkWidget *
panel_module_factory_new_plugin (PanelModuleFactory  *factory,
                                 const gchar         *name,
                                 GdkScreen           *screen,
                                 gint                 unique_id,
                                 gchar              **arguments,
                                 gint                *return_unique_id)
{
  PanelModule *module;
  GtkWidget   *provider;
  static gint  unique_id_counter = 0;

  panel_return_val_if_fail (PANEL_IS_MODULE_FACTORY (factory), NULL);
  panel_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  panel_return_val_if_fail (name != NULL, NULL);

  /* find the module in the hash table */
  module = g_hash_table_lookup (factory->modules, name);
  if (G_UNLIKELY (module == NULL))
    {
      panel_debug (PANEL_DEBUG_MODULE_FACTORY, "Module \"%s\" not found in the factory", name);
      return NULL;
    }

  /* make sure this plugin has a unique id */
  while (unique_id == -1
         || panel_module_factory_unique_id_exists (factory, unique_id))
    unique_id = ++unique_id_counter;

  /* set the return value with an always valid unique id */
  if (G_LIKELY (return_unique_id != NULL))
    *return_unique_id = unique_id;

  /* create the new module */
  provider = panel_module_new_plugin (module, screen, unique_id, arguments);

  /* insert plugin in the list */
  if (G_LIKELY (provider))
    {
      factory->plugins = g_slist_prepend (factory->plugins, provider);
      g_object_weak_ref (G_OBJECT (provider), panel_module_factory_remove_plugin, factory);
    }

  /* emit unique-changed if the plugin is unique */
  if (panel_module_is_unique (module))
    panel_module_factory_emit_unique_changed (module);

  return provider;
}
