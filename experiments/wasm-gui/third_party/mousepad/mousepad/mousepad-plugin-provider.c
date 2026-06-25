/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-plugin-provider.h>
#include <mousepad/mousepad-plugin.h>

#include <gmodule.h>



/* GObject virtual functions */
static void     mousepad_plugin_provider_finalize (GObject     *object);

/* GTypeModule virtual functions */
static gboolean mousepad_plugin_provider_load     (GTypeModule *type_module);
static void     mousepad_plugin_provider_unload   (GTypeModule *type_module);



struct _MousepadPluginProvider
{
  GTypeModule __parent__;

  /* dynamically loaded module */
  GModule *module;

  /* list of instantiated types */
  GList    *instances;
  gboolean  first_instantiation;

  /* plugin data */
  MousepadPluginData *plugin_data;
  GtkWidget          *setting_box;

  /* minimal set of pre-instantiation functions that each plugin must implement */
  void                 (*initialize)      (MousepadPluginProvider *provider);
  MousepadPluginData * (*get_plugin_data) (void);
};



G_DEFINE_TYPE (MousepadPluginProvider, mousepad_plugin_provider, G_TYPE_TYPE_MODULE)



static void
mousepad_plugin_provider_class_init (MousepadPluginProviderClass *klass)
{
  GObjectClass     *gobject_class = G_OBJECT_CLASS (klass);
  GTypeModuleClass *gtype_module_class = G_TYPE_MODULE_CLASS (klass);

  gobject_class->finalize = mousepad_plugin_provider_finalize;

  gtype_module_class->load = mousepad_plugin_provider_load;
  gtype_module_class->unload = mousepad_plugin_provider_unload;
}



static void
mousepad_plugin_provider_init (MousepadPluginProvider *provider)
{
  provider->module = NULL;

  provider->instances = NULL;
  provider->first_instantiation = TRUE;

  provider->plugin_data = NULL;
  provider->setting_box = NULL;

  provider->initialize = NULL;
  provider->get_plugin_data = NULL;
}



static void
mousepad_plugin_provider_finalize (GObject *object)
{
  MousepadPluginProvider *provider = MOUSEPAD_PLUGIN_PROVIDER (object);

  if (provider->module != NULL)
    g_module_close (provider->module);

  G_OBJECT_CLASS (mousepad_plugin_provider_parent_class)->finalize (object);
}



static gboolean
mousepad_plugin_provider_load (GTypeModule *type_module)
{
  MousepadPluginProvider *provider = MOUSEPAD_PLUGIN_PROVIDER (type_module);
  gchar                  *path;

  /* load the module */
  path = g_module_build_path (MOUSEPAD_PLUGIN_DIRECTORY, type_module->name);
  provider->module = g_module_open (path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  g_free (path);

  /* check if the load operation was successful */
  if (G_LIKELY (provider->module != NULL))
    {
      /* initialize the plugin if all required public symbols are present */
      if (g_module_symbol (provider->module, "mousepad_plugin_initialize",
                           (gpointer *) &(provider->initialize))
          && g_module_symbol (provider->module, "mousepad_plugin_get_data",
                              (gpointer *) &(provider->get_plugin_data)))
        {
          provider->initialize (provider);
          provider->plugin_data = provider->get_plugin_data ();

          return TRUE;
        }
      else
        {
          g_warning ("Plugin \"%s\" lacks required symbols", type_module->name);
          g_type_module_unuse (G_TYPE_MODULE (provider));

          return FALSE;
        }
    }
  else
    {
      g_message ("Failed to load plugin \"%s\": %s", type_module->name, g_module_error ());
      return FALSE;
    }
}



static void
mousepad_plugin_provider_unload (GTypeModule *type_module)
{
  MousepadPluginProvider *provider = MOUSEPAD_PLUGIN_PROVIDER (type_module);

  /* destroy the plugin */
  g_list_free_full (provider->instances, g_object_unref);
  provider->instances = NULL;
  if (provider->setting_box != NULL)
    gtk_widget_destroy (provider->setting_box);

  /* reset provider state, except module if ever we go to finalize() */
  provider->initialize = NULL;
  provider->get_plugin_data = NULL;
}



MousepadPluginProvider *
mousepad_plugin_provider_new (const gchar *name)
{
  MousepadPluginProvider *provider;

  provider = g_object_new (MOUSEPAD_TYPE_PLUGIN_PROVIDER, NULL);
  g_type_module_set_name (G_TYPE_MODULE (provider), name);

  return provider;
}



gboolean
mousepad_plugin_provider_is_destroyable (MousepadPluginProvider *provider)
{
  return provider->plugin_data->destroyable;
}



gboolean
mousepad_plugin_provider_is_instantiated (MousepadPluginProvider *provider)
{
  return provider->instances != NULL;
}



const gchar *
mousepad_plugin_provider_get_label (MousepadPluginProvider *provider)
{
  return provider->plugin_data->label;
}



const gchar *
mousepad_plugin_provider_get_tooltip (MousepadPluginProvider *provider)
{
  return provider->plugin_data->tooltip;
}



const gchar *
mousepad_plugin_provider_get_category (MousepadPluginProvider *provider)
{
  return provider->plugin_data->category;
}



const gchar *
mousepad_plugin_provider_get_accel (MousepadPluginProvider *provider)
{
  return provider->plugin_data->accel;
}



GtkWidget *
mousepad_plugin_provider_create_setting_box (MousepadPluginProvider *provider)
{
  /* this should happen only once */
  if (provider->setting_box == NULL)
    {
      provider->setting_box = g_object_ref (gtk_box_new (GTK_ORIENTATION_VERTICAL, 6));
      gtk_widget_set_margin_start (provider->setting_box, 6);
      gtk_widget_set_margin_end (provider->setting_box, 6);
      gtk_widget_set_margin_top (provider->setting_box, 6);
      gtk_widget_set_margin_bottom (provider->setting_box, 6);

      /* if ever the setting box is destroyed from the outside (which should not happen)
       * the pointer will still be reset, and the tests based on it will work correctly */
      g_signal_connect (provider->setting_box, "destroy", G_CALLBACK (gtk_widget_destroyed),
                        &(provider->setting_box));
    }

  return provider->setting_box;
}



GtkWidget *
mousepad_plugin_provider_get_setting_box (MousepadPluginProvider *provider)
{
  return provider->setting_box;
}



void
mousepad_plugin_provider_new_plugin (MousepadPluginProvider *provider)
{
  GTypeModule *module = G_TYPE_MODULE (provider);
  gpointer     instance;
  GType        type;

  /* if ever there is an interest in having multi-instance plugins, this could be changed */
  if (provider->instances != NULL)
    {
      g_warning ("Plugin '%s' is already instantiated", module->name);
      return;
    }

  /* instanciate all types */
  while ((type = *(provider->plugin_data->types++)) != G_TYPE_INVALID)
    {
      if (g_type_is_a (type, MOUSEPAD_TYPE_PLUGIN))
        instance = g_object_new (type, "provider", provider, NULL);
      else if (g_type_is_a (type, G_TYPE_OBJECT))
        instance = g_object_new (type, NULL);
      else
        {
          g_warning ("Type '%s' of plugin '%s' is not a descendant of GObject: ignored",
                     g_type_name (type), module->name);
          continue;
        }

      provider->instances = g_list_prepend (provider->instances, instance);
      if (provider->first_instantiation)
        g_type_module_unuse (G_TYPE_MODULE (provider));
    }

  /* no need to decrement the provider use count next time */
  provider->first_instantiation = FALSE;
}



void
mousepad_plugin_provider_unuse (gpointer data)
{
  MousepadPluginProvider *provider = data;

  if (provider->instances != NULL)
    g_type_module_unuse (data);
}
