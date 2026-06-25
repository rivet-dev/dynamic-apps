/*
 * Copyright (C) 2009 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

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

#include <common/panel-private.h>
#include <wrapper/wrapper-module.h>



static void     wrapper_module_dispose (GObject     *object);
static gboolean wrapper_module_load    (GTypeModule *type_module);
static void     wrapper_module_unload  (GTypeModule *type_module);




struct _WrapperModuleClass
{
  GTypeModuleClass __parent__;
};

struct _WrapperModule
{
  GTypeModule __parent__;

  GModule *library;
  GType plugin_type;
};



G_DEFINE_TYPE (WrapperModule, wrapper_module, G_TYPE_TYPE_MODULE)



static void
wrapper_module_class_init (WrapperModuleClass *klass)
{
  GObjectClass *gobject_class;
  GTypeModuleClass *gtype_module_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = wrapper_module_dispose;

  gtype_module_class = G_TYPE_MODULE_CLASS (klass);
  gtype_module_class->load = wrapper_module_load;
  gtype_module_class->unload = wrapper_module_unload;
}



static void
wrapper_module_init (WrapperModule *module)
{
  module->plugin_type = G_TYPE_NONE;
}



static void
wrapper_module_dispose (GObject *object)
{
  WrapperModule *module = WRAPPER_MODULE (object);

  if (module->plugin_type != G_TYPE_NONE)
    {
      /* a module containing type implementations must exist forever */
      g_object_ref (module);
      return;
    }

  G_OBJECT_CLASS (wrapper_module_parent_class)->dispose (object);
}



static gboolean
wrapper_module_load (GTypeModule *type_module)
{
  return TRUE;
}



static void
wrapper_module_unload (GTypeModule *type_module)
{
  /* foo */
}



WrapperModule *
wrapper_module_new (GModule *library)
{
  WrapperModule *module;

  module = g_object_new (WRAPPER_TYPE_MODULE, NULL);
  module->library = library;

  return module;
}



GtkWidget *
wrapper_module_new_provider (WrapperModule  *module,
                             GdkScreen      *screen,
                             const gchar    *name,
                             gint            unique_id,
                             const gchar    *display_name,
                             const gchar    *comment,
                             gchar         **arguments)
{
  GtkWidget           *plugin = NULL;
  PluginConstructFunc  construct_func;
  PluginInitFunc       init_func;

  panel_return_val_if_fail (WRAPPER_IS_MODULE (module), NULL);
  panel_return_val_if_fail (module->library != NULL, NULL);

  g_type_module_use (G_TYPE_MODULE (module));

  /* try to link the contruct or init function */
  if (g_module_symbol (module->library, "xfce_panel_module_init",
      (gpointer) &init_func) && init_func != NULL)
    {
      /* initialize the plugin */
      module->plugin_type = (init_func) (G_TYPE_MODULE (module), NULL);

      /* create the object */
      plugin = g_object_new (module->plugin_type,
                             "name", name,
                             "unique-id", unique_id,
                             "display-name", display_name,
                             "comment", comment,
                             "arguments", arguments, NULL);
    }
  else if (g_module_symbol (module->library, "xfce_panel_module_construct",
           (gpointer) &construct_func) && construct_func != NULL)
    {
      /* create a new panel plugin */
      plugin = (*construct_func) (name, unique_id,
                                  display_name, comment,
                                  arguments, screen);
    }
  else
    {
      /* print critical warning */
      g_critical ("Plugin \"%s\" lacks a plugin register function.", name);
    }

  return plugin;
}
