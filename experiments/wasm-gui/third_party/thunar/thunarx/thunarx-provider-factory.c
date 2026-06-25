/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-206 Benedikt Meurer <benny@xfce.org>
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

#include <gdk/gdk.h>

#include <thunarx/thunarx-private.h>
#include <thunarx/thunarx-provider-factory.h>
#include <thunarx/thunarx-provider-module.h>
#include <thunarx/thunarx-provider-plugin.h>



/* "provider cache" cleanup interval (in seconds) */
#define THUNARX_PROVIDER_FACTORY_INTERVAL (45)



static void     thunarx_provider_factory_finalize       (GObject                     *object);
static void     thunarx_provider_factory_add            (ThunarxProviderFactory      *factory,
                                                         ThunarxProviderModule       *module);
static void     thunarx_provider_factory_create_modules (ThunarxProviderFactory      *factory);
static gboolean thunarx_provider_factory_timer          (gpointer                     user_data);
static void     thunarx_provider_factory_timer_destroy  (gpointer                     user_data);

/**
 * SECTION: thunarx-provider-factory
 * @short_description: The provider factory support for applications
 * @title: ThunarxProviderFactory
 * @include: thunarx/thunarx.h
 *
 * The #ThunarxProviderFactory class allows applications to use Thunar plugins. It handles
 * the loading of the installed extensions and instantiates providers for the application.
 * For example, Thunar uses this class to access the installed extensions.
 */

typedef struct
{
  GObject *provider;  /* cached provider reference or %NULL */
  GType    type;      /* provider GType */
} ThunarxProviderInfo;

struct _ThunarxProviderFactoryClass
{
  GObjectClass __parent__;
};

struct _ThunarxProviderFactory
{
  GObject __parent__;

  ThunarxProviderInfo *infos;     /* provider types and cached provider references */
  gint                 n_infos;   /* number of items in the infos array */

  guint                timer_id;  /* GSource timer to cleanup cached providers */

  gint                 initialized;

};

static gboolean thunarx_provider_modules_created = FALSE;
static GList *thunarx_provider_modules            = NULL; /* list of all active provider modules */
static GList *thunarx_persistent_provider_modules = NULL; /* list of active persistent provider modules */
static GList *thunarx_volatile_provider_modules   = NULL; /* list of active volatile provider modules */


G_DEFINE_TYPE (ThunarxProviderFactory, thunarx_provider_factory, G_TYPE_OBJECT)



static void
thunarx_provider_factory_class_init (ThunarxProviderFactoryClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunarx_provider_factory_finalize;
}



static void
thunarx_provider_factory_init (ThunarxProviderFactory *factory)
{
  factory->initialized = FALSE;
}



static void
thunarx_provider_factory_finalize (GObject *object)
{
  ThunarxProviderFactory *factory = THUNARX_PROVIDER_FACTORY (object);
  gint                    n;

  /* stop the "provider cache" cleanup timer */
  if (G_LIKELY (factory->timer_id != 0))
    g_source_remove (factory->timer_id);

  /* release provider infos */
  for (n = 0; n < factory->n_infos; ++n)
    if (factory->infos[n].provider != NULL)
      g_object_unref (factory->infos[n].provider);
  g_free (factory->infos);

  (*G_OBJECT_CLASS (thunarx_provider_factory_parent_class)->finalize) (object);
}



static void
thunarx_provider_factory_add (ThunarxProviderFactory *factory,
                              ThunarxProviderModule  *module)
{
  const GType *types;
  gint         n_types;

  /* determines the types provided by the module */
  thunarx_provider_module_list_types (module, &types, &n_types);

  /* add the types provided by the extension */
  factory->infos = g_renew (ThunarxProviderInfo, factory->infos, factory->n_infos + n_types);
  for (; n_types-- > 0; ++types)
    {
      factory->infos[factory->n_infos].provider = NULL;
      factory->infos[factory->n_infos].type = *types;
      ++factory->n_infos;
    }
}



static void
thunarx_provider_factory_create_modules (ThunarxProviderFactory *factory)
{
  ThunarxProviderModule *module;
  const gchar           *name;
  GList                 *lp;
  GDir                  *dp;
  gchar                 *dirs_string = NULL;
  gchar                **dirs;

  if (g_strcmp0 (THUNARX_ENABLE_CUSTOM_DIRS, "TRUE") == 0)
    dirs_string = (gchar *) g_getenv ("THUNARX_DIRS");

  if (dirs_string == NULL)
    dirs_string = THUNARX_DIRECTORY;

  dirs = g_strsplit (dirs_string, G_SEARCHPATH_SEPARATOR_S, 0);

  for (int i = 0; dirs[i] != NULL; i++)
    {

      dp = g_dir_open (dirs[i], 0, NULL);

      if (G_LIKELY (dp != NULL))
        {
          /* determine the types for all existing plugins */
          for (name = g_dir_read_name (dp); name != NULL; name = g_dir_read_name (dp))
            {
              /* check if this is a valid plugin file */
              if (g_str_has_suffix (name, "." G_MODULE_SUFFIX))
                {
                  gboolean module_already_loaded = FALSE;

                  /* check if we already have that module */
                  for (lp = thunarx_provider_modules; lp != NULL; lp = lp->next)
                    {
                      if (g_str_equal (G_TYPE_MODULE (lp->data)->name, name))
                        {
                          module_already_loaded = TRUE;
                          break;
                        }
                    }

                  if (G_UNLIKELY (module_already_loaded == TRUE))
                    continue;

                  /* allocate the new module and add it to our lists */
                  module = thunarx_provider_module_new (name);
                  thunarx_provider_modules = g_list_prepend (thunarx_provider_modules, module);
                }
            }

          g_dir_close (dp);
        }
    }

  g_strfreev (dirs);
}



static gboolean
thunarx_provider_factory_timer (gpointer user_data)
{
  ThunarxProviderFactory *factory = THUNARX_PROVIDER_FACTORY (user_data);
  ThunarxProviderInfo    *info;
  gint                    n;

THUNAR_THREADS_ENTER

  /* drop all providers for which only we keep a reference */
  for (n = factory->n_infos; --n >= 0; )
    {
      info = factory->infos + n;
      if (info->provider != NULL && info->provider->ref_count == 1)
        {
          g_object_unref (info->provider);
          info->provider = NULL;
        }
    }

THUNAR_THREADS_LEAVE

  return TRUE;
}



static void
thunarx_provider_factory_timer_destroy (gpointer user_data)
{
  THUNARX_PROVIDER_FACTORY (user_data)->timer_id = 0;
}



/**
 * thunarx_provider_factory_get_default:
 *
 * Returns a reference to the default #ThunarxProviderFactory
 * instance.
 *
 * The caller is responsible to free the returned object
 * using g_object_unref() when no longer needed.
 *
 * Returns: (transfer full): a reference to the default #ThunarxProviderFactory
 *          instance.
 **/
ThunarxProviderFactory*
thunarx_provider_factory_get_default (void)
{
  static ThunarxProviderFactory *factory = NULL;

  /* allocate the default factory instance on-demand */
  if (G_UNLIKELY (factory == NULL))
    {
      factory = g_object_new (THUNARX_TYPE_PROVIDER_FACTORY, NULL);
      g_object_add_weak_pointer (G_OBJECT (factory), (gpointer) &factory);
    }
  else
    {
      /* take a reference on the default factory for the caller */
      g_object_ref (G_OBJECT (factory));
    }

  return factory;
}



/**
 * thunarx_provider_factory_list_providers:
 * @factory : a #ThunarxProviderFactory instance.
 * @type    : the provider #GType.
 *
 * Returns all providers of the given @type.
 *
 * The caller is responsible to release the returned
 * list of providers using code like this:
 * <informalexample><programlisting>
 * g_list_free_full (list, g_object_unref);
 * </programlisting></informalexample>
 *
 * Returns: (transfer full) (element-type GObject): the of providers for @type.
 **/
GList*
thunarx_provider_factory_list_providers (ThunarxProviderFactory *factory,
                                         GType                   type)
{
  ThunarxProviderInfo *info;
  GList               *providers = NULL;
  GList               *lp;
  gint                 n;

  /* create all available modules */
  if (thunarx_provider_modules_created == FALSE)
    {
      thunarx_provider_factory_create_modules (factory);

      /* On the first call, we need to load all modules once, since only when loaded, we can tell if they are persistent or volatile */
      for (lp = thunarx_provider_modules; lp != NULL; lp = lp->next)
        {
          g_type_module_use (G_TYPE_MODULE (lp->data));
          if (thunarx_provider_plugin_get_resident (THUNARX_PROVIDER_PLUGIN (lp->data)))
              thunarx_persistent_provider_modules = g_list_prepend (thunarx_persistent_provider_modules, lp->data);
          else
              thunarx_volatile_provider_modules = g_list_prepend (thunarx_volatile_provider_modules, lp->data);
        }

      thunarx_provider_modules_created = TRUE;
    }
  else
    {
      /* volatile modules are reloaded on each call */
      for (lp = thunarx_volatile_provider_modules; lp != NULL; lp = lp->next)
        g_type_module_use (G_TYPE_MODULE (lp->data));
    }

  if (factory->initialized == FALSE)
    {
      for (lp = thunarx_provider_modules; lp != NULL; lp = lp->next)
        thunarx_provider_factory_add (factory, THUNARX_PROVIDER_MODULE (lp->data));

      if (G_UNLIKELY (factory->timer_id == 0))
        {
          /* start the "provider cache" cleanup timer */
          factory->timer_id = g_timeout_add_seconds_full (G_PRIORITY_LOW, THUNARX_PROVIDER_FACTORY_INTERVAL,
                                                          thunarx_provider_factory_timer, factory,
                                                          thunarx_provider_factory_timer_destroy);
        }
      factory->initialized = TRUE;
    }

  /* determine all available providers for the type */
  for (info = factory->infos, n = factory->n_infos; --n >= 0; ++info)
    if (G_LIKELY (g_type_is_a (info->type, type)))
      {
        /* allocate the provider on-demand */
        if (G_UNLIKELY (info->provider == NULL))
          {
            info->provider = g_object_new (info->type, NULL);
            if (G_UNLIKELY (info->provider == NULL))
              continue;
          }

        /* take a reference for the caller */
        g_object_ref (info->provider);

        /* add the provider to the result list */
        providers = g_list_append (providers, info->provider);
      }

  /* unload all volatile modules */
  for (lp = thunarx_volatile_provider_modules; lp != NULL; lp = lp->next)
    g_type_module_unuse (G_TYPE_MODULE (lp->data));

  return providers;
}
