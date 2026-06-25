/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */



#ifndef __XFCE_PANEL_MACROS_H__
#define __XFCE_PANEL_MACROS_H__

#include <glib.h>
#include <glib-object.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4panel/xfce-panel-convenience.h>

G_BEGIN_DECLS



/**
 * SECTION: macros
 * @title: Commonly used plugin macros
 * @short_description: Various macros to make life easier for plugin developers.
 * @include: libxfce4panel/libxfce4panel.h
 *
 * Some commonly used macros in panel plugins.
 **/

/**
 * SECTION: register-plugins
 * @title: Panel Plugin Register Macros
 * @short_description: Register panel plugins that are compiled as modules.
 * @include: libxfce4panel/libxfce4panel.h
 *
 * Macros to register panel plugins that are compiled as modules. Internal or
 * external is defined by the boolean key <varname>X-XFCE-Internal</varname>
 * in the plugin's .desktop file.
 **/

/**
 * SECTION: register-gobject-plugins
 * @title: GObject Oriented Panel Plugin Registers Macros
 * @short_description: Macros to register panel plugins, written as GObjects.
 * @include: libxfce4panel/libxfce4panel.h
 *
 * Macros to register panel plugin types and custom types inside panel plugins.
 **/

/**
 * XfcePanelTypeModule:
 *
 * Typedef for GTypeModule for possible future expansion.
 *
 * Since: 4.8
 **/
typedef GTypeModule XfcePanelTypeModule;



/**
 * XFCE_PANEL_CHANNEL_NAME:
 *
 * Macro to return the value of xfce_panel_get_channel_name().
 *
 * See also: xfce_panel_plugin_xfconf_channel_new,
 *           xfce_panel_plugin_get_property_base
 *
 * Since: 4.8
 **/
#define XFCE_PANEL_CHANNEL_NAME (xfce_panel_get_channel_name ())



/**
 * xfce_panel_plugin_xfconf_channel_new:
 * @plugin : An #XfcePanelPlugin.
 *
 * Convienient function for opening an XfconfChannel for a plugin. The
 * channel's property base will be propery returned from
 * xfce_panel_plugin_get_property_base().
 *
 * See also: xfce_panel_plugin_get_property_base,
 *           XFCE_PANEL_PLUGIN_CHANNEL_NAME and
 *           xfce_panel_get_channel_name
 *
 * Since: 4.8
 **/
#define xfce_panel_plugin_xfconf_channel_new(plugin) \
  xfconf_channel_new_with_property_base (XFCE_PANEL_CHANNEL_NAME, \
    xfce_panel_plugin_get_property_base (XFCE_PANEL_PLUGIN (plugin)))



/**
 * XFCE_PANEL_DEFINE_PLUGIN:
 * @TypeName  : The name of the new type, in Camel case.
 * @type_name : The name of the new type, in lowercase, with words separated by '_'.
 * @...       : Optional list of *_register_type() function from other
 *              objects in the plugin created with #XFCE_PANEL_DEFINE_TYPE.
 *
 * Define a new (resident) GObject panel plugin, the parent type of the object
 * should be XFCE_TYPE_PANEL_PLUGIN.
 *
 * Since: 4.8
 **/
#define XFCE_PANEL_DEFINE_PLUGIN(TypeName, type_name, args...) \
  _XPP_DEFINE_PLUGIN (TypeName, type_name, FALSE, args)



/**
 * XFCE_PANEL_DEFINE_PLUGIN_RESIDENT:
 * @TypeName  : The name of the new type, in Camel case.
 * @type_name : The name of the new type, in lowercase, with words separated by '_'.
 * @...       : Optional list of *_register_type() function from other
 *              objects in the plugin created with #XFCE_PANEL_DEFINE_TYPE.
 *
 * Same as #XFCE_PANEL_DEFINE_PLUGIN, but if you use special libraries or objects,
 * it is possible the plugin will give problems when unloading the library,
 * a resident plugin will never be unloaded after the first load, avoiding
 * those issues.
 *
 * Since: 4.8
 **/
#define XFCE_PANEL_DEFINE_PLUGIN_RESIDENT(TypeName, type_name, args...) \
  _XPP_DEFINE_PLUGIN (TypeName, type_name, TRUE, args)



/**
 * XFCE_PANEL_DEFINE_TYPE:
 * @TypeName    : The name of the new type, in Camel case.
 * @type_name   : The name of the new type, in lowercase, with words separated by '_'.
 * @TYPE_PARENT : The GType of the parent type.
 *
 * A convenient macro of #G_DEFINE_DYNAMIC_TYPE for panel plugins. Only
 * difference with #G_DEFINE_DYNAMIC_TYPE is that the type name send to
 * g_type_module_register_type() is prefixed with "Xfce". This allows you
 * use use shorted structure names in the code, while the real name of the
 * object is a full "Xfce" name.
 *
 * The _register_type function should be added to the args in
 * #XFCE_PANEL_DEFINE_PLUGIN.
 *
 * Since: 4.8
 **/
#define XFCE_PANEL_DEFINE_TYPE(TypeName, type_name, TYPE_PARENT) \
  static gpointer type_name##_parent_class = NULL; \
  static GType    type_name##_type = 0; \
  \
  static void     type_name##_init              (TypeName        *self); \
  static void     type_name##_class_init        (TypeName##Class *klass); \
  static void     type_name##_class_intern_init (gpointer klass) \
  { \
    type_name##_parent_class = g_type_class_peek_parent (klass); \
    type_name##_class_init ((TypeName##Class*) klass); \
  } \
  \
  GType \
  type_name##_get_type (void) \
  { \
    return type_name##_type; \
  } \
  \
  void \
  type_name##_register_type (XfcePanelTypeModule *type_module) \
  { \
    GType plugin_define_type_id; \
    static const GTypeInfo plugin_define_type_info = \
    { \
      sizeof (TypeName##Class), \
      NULL, \
      NULL, \
      (GClassInitFunc) (void (*)(void)) type_name##_class_intern_init, \
      NULL, \
      NULL, \
      sizeof (TypeName), \
      0, \
      (GInstanceInitFunc) (void (*)(void)) type_name##_init, \
      NULL, \
    }; \
    \
    plugin_define_type_id = \
        g_type_module_register_type (G_TYPE_MODULE (type_module), TYPE_PARENT, \
                                     "Xfce" #TypeName, &plugin_define_type_info, 0); \
    \
    type_name##_type = plugin_define_type_id; \
  }



/* <private> */
#define _XPP_DEFINE_PLUGIN(TypeName, type_name, resident, args...) \
  GType xfce_panel_module_init (GTypeModule *type_module, gboolean *make_resident); \
  \
  XFCE_PANEL_DEFINE_TYPE (TypeName, type_name, XFCE_TYPE_PANEL_PLUGIN) \
  \
  G_MODULE_EXPORT GType \
  xfce_panel_module_init (GTypeModule *type_module, \
                          gboolean    *make_resident) \
  { \
    typedef void (*XppRegFunc) (XfcePanelTypeModule *module); \
    XppRegFunc reg_funcs[] = { type_name##_register_type, args }; \
    guint      i; \
    \
    /* whether to make this plugin resident */ \
    if (make_resident != NULL) \
      *make_resident = resident; \
    \
    /* register the plugin types */ \
    for (i = 0; i < G_N_ELEMENTS (reg_funcs); i++) \
      (* reg_funcs[i]) (type_module); \
    \
    return type_name##_get_type (); \
  }



/**
 * XFCE_PANEL_DEFINE_PREINIT_FUNC:
 * @preinit_func: name of the function that points to an
 *                #XfcePanelPluginPreInit function.
 *
 * Registers a pre-init function in the plugin module. This function
 * is called before gtk_init() and can be used to initialize
 * special libaries.
 * Downside of this that the plugin cannot run internal. Even if you
 * set X-XFCE-Interal=TRUE in the desktop file, the panel will force
 * the plugin to run inside a wrapper (this because the panel called
 * gtk_init() long before it starts to load the plugins).
 *
 * Note that you can only use this once and it only works in
 * combination with the plugins register/define functions added
 * in 4.8.
 *
 * Since: 4.8
 **/
#define XFCE_PANEL_DEFINE_PREINIT_FUNC(preinit_func) \
  G_MODULE_EXPORT gboolean xfce_panel_module_preinit (gint argc, gchar **argv); \
  \
  G_MODULE_EXPORT gboolean \
  xfce_panel_module_preinit (gint    argc, \
                             gchar **argv) \
  { \
    return (*preinit_func) (argc, argv); \
  }



/**
 * XFCE_PANEL_PLUGIN_REGISTER:
 * @construct_func : name of the function that points to an
 *                   #XfcePanelPluginFunc function.
 *
 * Register a panel plugin using a construct function. This is the
 * simplest way to register a panel plugin.
 * The @construct_func is called everytime a plugin is created.
 *
 * Since: 4.8
 **/
#define XFCE_PANEL_PLUGIN_REGISTER(construct_func) \
  _XFCE_PANEL_PLUGIN_REGISTER_EXTENDED (construct_func, /* foo */, /* foo */)



/**
 * XFCE_PANEL_PLUGIN_REGISTER_WITH_CHECK:
 * @construct_func : name of the function that points to an
 *                   #XfcePanelPluginFunc function.
 * @check_func     : name of the function that points to an
 *                   #XfcePanelPluginCheck function.
 *
 * Register a panel plugin using a construct function. The @check_func
 * will be called before the plugin is created. If this function returns
 * %FALSE, the plugin won't be added to the panel. For proper feedback,
 * you are responsible for showing a dialog why the plugin is not added
 * to the panel.
 *
 * Since: 4.8
 **/
#define XFCE_PANEL_PLUGIN_REGISTER_WITH_CHECK(construct_func, check_func) \
  _XFCE_PANEL_PLUGIN_REGISTER_EXTENDED (construct_func, /* foo */, \
    if (G_LIKELY ((*check_func) (xpp_screen) == TRUE)))



/**
 * XFCE_PANEL_PLUGIN_REGISTER_FULL:
 * @construct_func : name of the function that points to an
 *                   #XfcePanelPluginFunc function.
 * @preinit_func   : name of the function that points to an
 *                   #XfcePanelPluginPreInit function.
 * @check_func     : name of the function that points to an
 *                   #XfcePanelPluginCheck function.
 *
 * Same as calling #XFCE_PANEL_DEFINE_PREINIT_FUNC and
 * #XFCE_PANEL_PLUGIN_REGISTER_WITH_CHECK. See those macros
 * for more information.
 *
 * Since: 4.8
 **/
#define XFCE_PANEL_PLUGIN_REGISTER_FULL(construct_func, preinit_func, check_func) \
  XFCE_PANEL_DEFINE_PREINIT_FUNC (preinit_func) \
  XFCE_PANEL_PLUGIN_REGISTER_WITH_CHECK (construct_func, check_func)



/* <private> */
#define _XFCE_PANEL_PLUGIN_REGISTER_EXTENDED(construct_func, PREINIT_CODE, CHECK_CODE) \
  static void \
  xfce_panel_module_realize (XfcePanelPlugin *xpp) \
  { \
    g_return_if_fail (XFCE_IS_PANEL_PLUGIN (xpp)); \
    \
    g_signal_handlers_disconnect_by_func (G_OBJECT (xpp), \
        G_CALLBACK (xfce_panel_module_realize), NULL); \
    \
    (*construct_func) (xpp); \
  } \
  \
  PREINIT_CODE \
  \
  G_MODULE_EXPORT XfcePanelPlugin * \
  xfce_panel_module_construct (const gchar  *xpp_name, \
                               gint          xpp_unique_id, \
                               const gchar  *xpp_display_name, \
                               const gchar  *xpp_comment, \
                               gchar       **xpp_arguments, \
                               GdkScreen    *xpp_screen); \
  G_MODULE_EXPORT XfcePanelPlugin * \
  xfce_panel_module_construct (const gchar  *xpp_name, \
                               gint          xpp_unique_id, \
                               const gchar  *xpp_display_name, \
                               const gchar  *xpp_comment, \
                               gchar       **xpp_arguments, \
                               GdkScreen    *xpp_screen) \
  { \
    XfcePanelPlugin *xpp = NULL; \
    \
    g_return_val_if_fail (GDK_IS_SCREEN (xpp_screen), NULL); \
    g_return_val_if_fail (xpp_name != NULL && xpp_unique_id != -1, NULL); \
    \
    CHECK_CODE \
      { \
        xpp = g_object_new (XFCE_TYPE_PANEL_PLUGIN, \
                            "name", xpp_name, \
                            "unique-id", xpp_unique_id, \
                            "display-name", xpp_display_name, \
                            "comment", xpp_comment, \
                            "arguments", xpp_arguments, NULL); \
        \
        g_signal_connect_after (G_OBJECT (xpp), "realize", \
            G_CALLBACK (xfce_panel_module_realize), NULL); \
      } \
    \
    return xpp; \
  }

G_END_DECLS

#endif /* !__XFCE_PANEL_MACROS_H__ */
