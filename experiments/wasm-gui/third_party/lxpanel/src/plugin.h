/*
 * Copyright (C) 2006-2008 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2006-2008 Jim Huang <jserv.tw@gmail.com>
 *               2009-2010 Marty Jack <martyj19@comcast.net>
 *               2014-2016 Andriy Grytsenko <andrej@rep.kiev.ua>
 *               2015 Hanno Zulla <hhz@users.sf.net>
 *
 * This file is a part of LXPanel project.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __PLUGIN_H__
#define __PLUGIN_H__ 1

#include <libfm/fm.h>

#include "panel.h"
#include "conf.h"

G_BEGIN_DECLS

/* New plugins style which uses FmModule loader, our module type is "lxpanel_gtk" */

#define FM_MODULE_lxpanel_gtk_VERSION 1 /* version of this API */

/**
 * LXPanelPluginInit:
 * @init: (allow-none): callback on lxpanel start
 * @finalize: (allow-none): callback on lxpanel exit
 * @name: name to represent plugin in lists
 * @description: tooltip on the plugin in lists
 * @new_instance: callback to create new plugin instance in panel
 * @config: (allow-none): callback to show configuration dialog
 * @reconfigure: (allow-none): callback to apply panel configuration change
 * @button_press_event: (allow-none): callback on "button-press-event" signal
 * @show_system_menu: (allow-none): callback to queue show system menu
 * @update_context_menu: (allow-none): callback to update context menu
 * @control: (allow-none): callback to pass messages from lxpanelctl
 * @gettext_package: (allow-none): additional catalog to read translations
 *
 * Callback @init is called on module loading, only once per application
 * lifetime.
 *
 * Callback @new_instance is called when panel tries to add some plugin.
 * It should initialize instance data, prepare widget, and return the
 * widget or %NULL if something went wrong. The @panel and @settings data
 * are guaranteed to be valid until gtk_widget_destroy() is called on the
 * instance. Instance own data should be attached to the instance using
 * lxpanel_plugin_set_data().
 *
 * Callback @config is called when user tries to configure the instance
 * (either via context menu or from plugins selection dialog). It should
 * create dialog window with instance configuration options. Returned
 * dialog will be destroyed on responce or on instance destroy and any
 * changed settings will be saved to the panel configuration file. See
 * also lxpanel_generic_config_dlg().
 *
 * Callback @reconfigure is called when panel configuration was changed
 * in the panel configuration dialog so the instance may change layout of
 * own subwidgets appropriately to new geometry.
 *
 * Callback @button_press_event is a handler for "button-press-event"
 * signal on the plugin instance. This callback would never receive any
 * right-clicks without modifier keys because panel itself will handle it
 * showing context menu. This callback should never return %TRUE for the
 * button 2 (middle-click) because that will disrupt the plugins movement
 * feature of panel (drag & drop) for that plugin.
 *
 * Callback @show_system_menu is called when lxpanel received a message
 * by 'lxpanelctl menu' command. It will be sent to each instance if more
 * than one with this callback available exists.
 *
 * Callback @update_context_menu is called when panel context menu being
 * composed. The @menu contains only item for plugin instance config. The
 * callback can append or prepend own items to the menu. The callback
 * should return %TRUE if panel's common menu should be moved into the
 * submenu 'Panel' (therefore context menu will contain 'Settings' item,
 * any added ones, and 'Panel') and %FALSE if panel's common menu items
 * should be in this menu after separator.
 *
 * Callback @control is called when command was sent via the lxpanelctl.
 * The message will be sent to only one instance of plugin. Some messages
 * are handled by lxpanel: "DEL" will remove plugin from panel, "ADD"
 * will create new instance if there is no instance yet. Due to design
 * limitations of XClientMessageEvent the size of plugin type and command
 * cannot exceed 18 characters in total.
 *
 * If @gettext_package is not %NULL then it will be used for translation
 * of @name and @description. (Since: 0.9.0)
 */
typedef struct {
    /*< public >*/
    void (*init)(void);         /* optional startup */
    void (*finalize)(void);     /* optional finalize */
    char *name;                 /* name to represent in lists */
    char *description;          /* tooltip text */
    GtkWidget *(*new_instance)(LXPanel *panel, config_setting_t *settings);
    GtkWidget *(*config)(LXPanel *panel, GtkWidget *instance);
    void (*reconfigure)(LXPanel *panel, GtkWidget *instance);
    gboolean (*button_press_event)(GtkWidget *widget, GdkEventButton *event, LXPanel *panel);
    void (*show_system_menu)(GtkWidget *widget);
    gboolean (*update_context_menu)(GtkWidget *plugin, GtkMenu *menu);
    gboolean (*control)(GtkWidget *plugin, const char *cmd);
    char *gettext_package;      /* optional: gettext package used to translate name and description */
    /*< private >*/
    gpointer _reserved1;
    gpointer _reserved2;
    /*< public >*/
    int one_per_system : 1;     /* True to disable more than one instance */
    int expand_available : 1;   /* True if "stretch" option is available */
    int expand_default : 1;     /* True if "stretch" option is default */
    int superseded : 1;         /* True if plugin was superseded by another */
} LXPanelPluginInit; /* constant data */

/*
 * This descriptor instance should be defined in each plugin code as main
 * entry point for plugin creation. Primitive plugin example follows:
 *
 * #include <lxpanel/plugin.h>
 *
 * GtkWidget *test_new_instance(LXPanel *panel, config_setting_t *settings)
 * {
 *      return gtk_image_new_from_stock(GTK_STOCK_OK, panel_get_icon_size(panel));
 * }
 *
 * FM_DEFINE_MODULE(lxpanel_gtk, test)
 *
 * LXPanelPluginInit fm_module_init_lxpanel_gtk = {
 *      .name = "Test plugin",
 *      .description = "An image with OK icon",
 *      .new_instance = test_new_instance
 * }
 */
extern LXPanelPluginInit fm_module_init_lxpanel_gtk;

extern GQuark lxpanel_plugin_qdata; /* access to plugin private data */
/**
 * lxpanel_plugin_get_data
 * @_i: instance widget
 *
 * Retrieves instance data attached to the widget.
 *
 * Returns: (transfer none): pointer to the data.
 */
#define lxpanel_plugin_get_data(_i) g_object_get_qdata(G_OBJECT(_i),lxpanel_plugin_qdata)

/**
 * lxpanel_plugin_set_data
 * @_i: instance widget
 * @_data: instance data
 * @_destructor: (allow-none): destructor for @_data
 *
 * Attaches data to the widget instance. The @_destructor callback will
 * be called on @_data when @_i is destroyed and therefore it should free
 * any allocated data. The instance widget at that moment is already not
 * available to use in any way so not rely on it or its children.
 */
#define lxpanel_plugin_set_data(_i,_data,_destructor) g_object_set_qdata_full(G_OBJECT(_i),lxpanel_plugin_qdata,_data,_destructor)

/**
 * lxpanel_register_plugin_type
 * @name: name of new plugin type
 * @init: plugin-specific type descriptor
 *
 * Register new plugin type. Can be called from plugins init() callback too.
 *
 * Returns: %TRUE in case of success.
 */
extern gboolean lxpanel_register_plugin_type(const char *name, const LXPanelPluginInit *init);

/**
 * lxpanel_get_plugin_menu
 * @panel: panel instance pointer
 * @plugin: plugin instance pointer
 * @use_sub_menu: %TRUE if panel management options should be in separated submenu
 *
 * Creates context menu for given @panel and @plugin.
 *
 * Returns: (transfer full): new menu widget.
 */
extern GtkMenu* lxpanel_get_plugin_menu(LXPanel* panel, GtkWidget* plugin, gboolean use_sub_menu);

/**
 * lxpanel_plugin_adjust_popup_position
 * @popup: a widget to adjust position
 * @plugin: a plugin instance
 *
 * Adjusts the position of a @popup window to ensure that it is not hidden
 * by the panel and moves @popup near @plugin if possible.
 */
extern void lxpanel_plugin_adjust_popup_position(GtkWidget * popup, GtkWidget * plugin);

/**
 * lxpanel_plugin_popup_set_position_helper
 * @p: a panel instance
 * @near: a widget to position the popup
 * @popup: a widget to calculate position
 * @px: (out): pointer to receive X coordinate
 * @py: (out): pointer to receive Y coordinate
 *
 * Calculates desired position of @popup to be placed right to the widget
 * @near accordingly to position of panel @p and returns its coordinates.
 * Can be used in position-calculation callback for popup menus.
 */
extern void lxpanel_plugin_popup_set_position_helper(LXPanel * p, GtkWidget * near, GtkWidget * popup, gint * px, gint * py);

/**
 * plugin_widget_set_background
 * @widget: a widget
 * @p: a panel instance
 *
 * Recursively set the background of @widget and its children. Can be
 * used on a panel background configuration change.
 */
extern void plugin_widget_set_background(GtkWidget * widget, LXPanel * p);

/**
 * lxpanel_launch_path
 * @panel: a panel instance
 * @path: a path to launch
 *
 * Launches the @path either itself, or using default application.
 *
 * Returns: %TRUE if launch was successful.
 */
extern gboolean lxpanel_launch_path(LXPanel *panel, FmPath *path);

/**
 * lxpanel_plugin_show_config_dialog
 * @plugin: a plugin instance
 *
 * Calls config() callback and shows configuration window.
 */
extern void lxpanel_plugin_show_config_dialog(GtkWidget* plugin);

/**
 * PluginConfType:
 * @CONF_TYPE_STR: string entry, pointer is char **
 * @CONF_TYPE_INT: spin entry (range 0...1000), pointer is gint *
 * @CONF_TYPE_BOOL: check button, pointer is gboolean *
 * @CONF_TYPE_FILE: file chooser, pointer is char **
 * @CONF_TYPE_FILE_ENTRY: file path entry, pointer is char **
 * @CONF_TYPE_DIRECTORY_ENTRY: directory path entry, pointer is char **
 * @CONF_TYPE_TRIM: just a text in italic, pointer is ignored
 * @CONF_TYPE_EXTERNAL: (since 0.8) provided by caller, pointer is GtkWidget *
 *
 * Type of variable passed to lxpanel_generic_config_dlg().
 */
typedef enum {
    CONF_TYPE_STR,
    CONF_TYPE_INT,
    CONF_TYPE_BOOL,
    CONF_TYPE_FILE,
    CONF_TYPE_FILE_ENTRY,
    CONF_TYPE_DIRECTORY_ENTRY,
    CONF_TYPE_TRIM,
    CONF_TYPE_EXTERNAL
} PluginConfType;

/**
 * lxpanel_generic_config_dlg
 * @title: (allow-none): optional title of dialog window
 * @panel: a panel instance
 * @apply_func: (allow-none): function to apply changes to the plugin
 * @plugin: (allow-none): an argument for @apply_func
 * @name: variable-size list of options to configure, terminated with %NULL
 *
 * Creates a generic dialog widget to configure the plugin parameters.
 * The dialog widget may be used for plugin's callback config() then.
 * Variable-size list of options consists of three arguments for each
 * option:
 *   - const char* name: text representing the option in dialog
 *   - gpointer ret_value: (in out): pointer to the option value
 *   - PluginConfType type: type of the option
 * Note that for type CONF_TYPE_EXTERNAL the name argument is ignored and
 * therefore empty string ("") have to be passed.
 *
 * Returns: (tranfer full): new created dialog widget.
 */
/* Parameters: const char* name, gpointer ret_value, PluginConfType type, ....NULL */
extern GtkWidget *lxpanel_generic_config_dlg(const char *title, LXPanel *panel,
                                             GSourceFunc apply_func,
                                             GtkWidget *plugin,
                                             const char *name, ...);

/**
 * panel_config_int_button_new
 * @name: text representing the option in dialog
 * @min: minimal spin button value
 * @max: maximal spin button value
 * @val: (in out): pointer to the option value
 *
 * Widget to use as CONF_TYPE_EXTERNAL instead of CONF_TYPE_INT if plugin
 * wants to customize range for lxpanel_generic_config_dlg(). The default
 * widget uses range 0...1000 and here you can set custom range. This API
 * should be never used outside of lxpanel_generic_config_dlg() arguments
 * since it uses callbacks specific to that API.
 *
 * See also: lxpanel_generic_config_dlg().
 *
 * Returns: (transfer full): new widget.
 *
 * Since: 0.8.0
 */
extern GtkWidget *panel_config_int_button_new(const char *name, gint *val,
                                              gint min, gint max);

/*
 * panel_config_hotkey_button_new
 * @label: text representing the option in dialog
 * @hotkey: (allow-none): current hotkey
 *
 * Creates a #GtkFrame derived widget which can select hotkey binding.
 * The widget emits "changed" signal when some new combination selected
 *      gboolean callback(PanelCfgInputButton *, char *, gpointer);
 * Caller should test if keybinding can be used in the callback and then
 * return test result as %TRUE or %FALSE.
 * Widget can be used for lxpanel_generic_config_dlg as CONF_TYPE_EXTERNAL
 *
 * Returns: (transfer full): a created widget.
 *
 * Since: 0.8.0
 */
extern GtkWidget *panel_config_hotkey_button_new(const char *label, const char *hotkey);
extern GtkWidget *panel_config_click_button_new(const char *label, const char *click);

/**
 * lxpanel_apply_hotkey
 * @hkptr: (in out) (transfer full): pointer to hotkey string
 * @keystring: (allow-none): new hotkey
 * @handler: callback to assign to hotkey
 * @user_data: data to provide for @handler
 * @show_error: %TRUE to show error window if assignment failed
 *
 * Function designed to use in callback on "changed" signal from widget
 * created by panel_config_hotkey_button_new(). Should be also used on
 * initial binding and on unbinding when module unloaded (in latter case
 * @keystring should be %NULL). In case of success returns %TRUE, unbinds
 * previous hotkey from @hkptr, and updates @hkptr. The @hkptr contains
 * allocated string.
 *
 * Returns: %TRUE on success.
 *
 * Since: 0.8.0
 */
extern gboolean lxpanel_apply_hotkey(char **hkptr, const char *keystring,
                                     void (*handler)(const char *keystring, gpointer user_data),
                                     gpointer user_data, gboolean show_error);

/**
 * panel_config_click_parse
 * @keystring: string to parse
 * @mods: (out): return location for modifier
 *
 * Parses click string received in "changed" signal emission of widget
 * created with panel_config_click_button_new() and returns button and
 * modifier that can be compared with GdkEventButton data when required.
 *
 * Returns: button number or 0 if @keystring is invalid.
 *
 * Since: 0.8.0
 */
extern guint panel_config_click_parse(const char *keystring, GdkModifierType *mods);

/* Add/remove plugin to/from panel */
GtkWidget *lxpanel_add_plugin(LXPanel *p, const char *name, config_setting_t *cfg, gint at);
void lxpanel_remove_plugin(LXPanel *p, GtkWidget *plugin);

G_END_DECLS

#endif /* __PLUGIN_H__ */
