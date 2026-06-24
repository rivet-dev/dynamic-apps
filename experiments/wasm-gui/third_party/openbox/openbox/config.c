/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   config.c for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
   Copyright (c) 2003-2007   Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include "config.h"
#include "keyboard.h"
#include "mouse.h"
#include "actions.h"
#include "translate.h"
#include "client.h"
#include "screen.h"
#include "openbox.h"
#include "gettext.h"
#include "obt/paths.h"

gboolean config_focus_new;
gboolean config_focus_follow;
guint    config_focus_delay;
gboolean config_focus_raise;
gboolean config_focus_last;
gboolean config_focus_under_mouse;
gboolean config_unfocus_leave;

ObPlacePolicy  config_place_policy;
gboolean       config_place_center;
ObPlaceMonitor config_place_monitor;

guint          config_primary_monitor_index;
ObPlaceMonitor config_primary_monitor;

StrutPartial config_margins;

gchar   *config_theme;
gboolean config_theme_keepborder;
guint    config_theme_window_list_icon_size;

gchar   *config_title_layout;

gboolean config_animate_iconify;

RrFont *config_font_activewindow;
RrFont *config_font_inactivewindow;
RrFont *config_font_menuitem;
RrFont *config_font_menutitle;
RrFont *config_font_activeosd;
RrFont *config_font_inactiveosd;

guint   config_desktops_num;
GSList *config_desktops_names;
guint   config_screen_firstdesk;
guint   config_desktop_popup_time;

gboolean         config_resize_redraw;
gint             config_resize_popup_show;
ObResizePopupPos config_resize_popup_pos;
GravityPoint     config_resize_popup_fixed;

ObStackingLayer config_dock_layer;
gboolean        config_dock_floating;
gboolean        config_dock_nostrut;
ObDirection     config_dock_pos;
gint            config_dock_x;
gint            config_dock_y;
ObOrientation   config_dock_orient;
gboolean        config_dock_hide;
guint           config_dock_hide_delay;
guint           config_dock_show_delay;
guint           config_dock_app_move_button;
guint           config_dock_app_move_modifiers;

guint    config_keyboard_reset_keycode;
guint    config_keyboard_reset_state;
gboolean config_keyboard_rebind_on_mapping_notify;

gint     config_mouse_threshold;
gint     config_mouse_dclicktime;
gint     config_mouse_screenedgetime;
gboolean config_mouse_screenedgewarp;

guint    config_menu_hide_delay;
gboolean config_menu_middle;
guint    config_submenu_show_delay;
guint    config_submenu_hide_delay;
gboolean config_menu_manage_desktops;
gboolean config_menu_show_icons;

GSList *config_menu_files;

gint     config_resist_win;
gint     config_resist_edge;

GSList *config_per_app_settings;

ObAppSettings* config_create_app_settings(void)
{
    ObAppSettings *settings = g_slice_new0(ObAppSettings);
    settings->type = -1;
    settings->decor = -1;
    settings->shade = -1;
    settings->monitor_type = OB_PLACE_MONITOR_ANY;
    settings->monitor = -1;
    settings->focus = -1;
    settings->desktop = 0;
    settings->layer = -2;
    settings->iconic = -1;
    settings->skip_pager = -1;
    settings->skip_taskbar = -1;
    settings->fullscreen = -1;
    settings->max_horz = -1;
    settings->max_vert = -1;
    return settings;
}

#define copy_if(setting, default) \
  if (src->setting != default) dst->setting = src->setting
void config_app_settings_copy_non_defaults(const ObAppSettings *src,
                                           ObAppSettings *dst)
{
    g_assert(src != NULL);
    g_assert(dst != NULL);

    copy_if(type, (ObClientType)-1);
    copy_if(decor, -1);
    copy_if(shade, -1);
    copy_if(monitor_type, OB_PLACE_MONITOR_ANY);
    copy_if(monitor, -1);
    copy_if(focus, -1);
    copy_if(desktop, 0);
    copy_if(layer, -2);
    copy_if(iconic, -1);
    copy_if(skip_pager, -1);
    copy_if(skip_taskbar, -1);
    copy_if(fullscreen, -1);
    copy_if(max_horz, -1);
    copy_if(max_vert, -1);

    if (src->pos_given) {
        dst->pos_given = TRUE;
        dst->pos_force = src->pos_force;
        dst->position = src->position;
        /* monitor is copied above */
    }

    dst->width_num = src->width_num;
    dst->width_denom = src->width_denom;
    dst->height_num = src->height_num;
    dst->height_denom = src->height_denom;
}

void config_parse_relative_number(gchar *s, gint *num, gint *denom)
{
    *num = strtol(s, &s, 10);

    if (*s == '%') {
        *denom = 100;
    } else if (*s == '/') {
        *denom = atoi(s+1);
    }
}

void config_parse_gravity_coord(xmlNodePtr node, GravityCoord *c)
{
    gchar *s = obt_xml_node_string(node);
    if (!g_ascii_strcasecmp(s, "center"))
        c->center = TRUE;
    else {
        gchar *ps = s;
        if (s[0] == '-')
            c->opposite = TRUE;
        if (s[0] == '-' || s[0] == '+')
            ps++;
        config_parse_relative_number(ps, &c->pos, &c->denom);
    }
    g_free(s);
}

/*
  <applications>
    <application name="aterm">
      <decor>false</decor>
    </application>
    <application name="Rhythmbox">
      <layer>above</layer>
      <position>
        <x>700</x>
        <y>0</y>
        <monitor>1</monitor>
      </position>
      .. there is a lot more settings available
    </application>
  </applications>
*/

static void parse_single_per_app_settings(xmlNodePtr app,
                                          ObAppSettings *settings)
{
    xmlNodePtr n, c;
    gboolean x_pos_given = FALSE;

    if ((n = obt_xml_find_node(app->children, "decor")))
        if (!obt_xml_node_contains(n, "default"))
            settings->decor = obt_xml_node_bool(n);

    if ((n = obt_xml_find_node(app->children, "shade")))
        if (!obt_xml_node_contains(n, "default"))
            settings->shade = obt_xml_node_bool(n);

    if ((n = obt_xml_find_node(app->children, "position"))) {
        if ((c = obt_xml_find_node(n->children, "x"))) {
            if (!obt_xml_node_contains(c, "default")) {
                config_parse_gravity_coord(c, &settings->position.x);
                x_pos_given = TRUE;
            }
        }

        if (x_pos_given && (c = obt_xml_find_node(n->children, "y"))) {
            if (!obt_xml_node_contains(c, "default")) {
                config_parse_gravity_coord(c, &settings->position.y);
                settings->pos_given = TRUE;
            }
        }

        /* monitor can be set without setting x or y */
        if ((c = obt_xml_find_node(n->children, "monitor"))) {
            if (!obt_xml_node_contains(c, "default")) {
                gchar *s = obt_xml_node_string(c);
                if (!g_ascii_strcasecmp(s, "mouse"))
                    settings->monitor_type = OB_PLACE_MONITOR_MOUSE;
                else if (!g_ascii_strcasecmp(s, "active"))
                    settings->monitor_type = OB_PLACE_MONITOR_ACTIVE;
                else if (!g_ascii_strcasecmp(s, "primary"))
                    settings->monitor_type = OB_PLACE_MONITOR_PRIMARY;
                else
                    settings->monitor = obt_xml_node_int(c);
                g_free(s);
            }
        }

        obt_xml_attr_bool(n, "force", &settings->pos_force);
    }

    if ((n = obt_xml_find_node(app->children, "size"))) {
        if ((c = obt_xml_find_node(n->children, "width"))) {
            if (!obt_xml_node_contains(c, "default")) {
                gchar *s = obt_xml_node_string(c);
                config_parse_relative_number(s,
                                             &settings->width_num,
                                             &settings->width_denom);
                if (settings->width_num <= 0 || settings->width_denom < 0)
                    settings->width_num = settings->width_denom = 0;
                g_free(s);
            }
        }

        if ((c = obt_xml_find_node(n->children, "height"))) {
            if (!obt_xml_node_contains(c, "default")) {
                gchar *s = obt_xml_node_string(c);
                config_parse_relative_number(s,
                                             &settings->height_num,
                                             &settings->height_denom);
                if (settings->height_num <= 0 || settings->height_denom < 0)
                    settings->height_num = settings->height_denom = 0;
                g_free(s);
            }
        }
    }

    if ((n = obt_xml_find_node(app->children, "focus"))) {
        if (!obt_xml_node_contains(n, "default"))
            settings->focus = obt_xml_node_bool(n);
    }

    if ((n = obt_xml_find_node(app->children, "desktop"))) {
        if (!obt_xml_node_contains(n, "default")) {
            gchar *s = obt_xml_node_string(n);
            if (!g_ascii_strcasecmp(s, "all"))
                settings->desktop = DESKTOP_ALL;
            else {
                gint i = obt_xml_node_int(n);
                if (i > 0)
                    settings->desktop = i;
            }
            g_free(s);
        }
    }

    if ((n = obt_xml_find_node(app->children, "layer"))) {
        if (!obt_xml_node_contains(n, "default")) {
            gchar *s = obt_xml_node_string(n);
            if (!g_ascii_strcasecmp(s, "above"))
                settings->layer = 1;
            else if (!g_ascii_strcasecmp(s, "below"))
                settings->layer = -1;
            else
                settings->layer = 0;
            g_free(s);
        }
    }

    if ((n = obt_xml_find_node(app->children, "iconic")))
        if (!obt_xml_node_contains(n, "default"))
            settings->iconic = obt_xml_node_bool(n);

    if ((n = obt_xml_find_node(app->children, "skip_pager")))
        if (!obt_xml_node_contains(n, "default"))
            settings->skip_pager = obt_xml_node_bool(n);

    if ((n = obt_xml_find_node(app->children, "skip_taskbar")))
        if (!obt_xml_node_contains(n, "default"))
            settings->skip_taskbar = obt_xml_node_bool(n);

    if ((n = obt_xml_find_node(app->children, "fullscreen")))
        if (!obt_xml_node_contains(n, "default"))
            settings->fullscreen = obt_xml_node_bool(n);

    if ((n = obt_xml_find_node(app->children, "maximized"))) {
        if (!obt_xml_node_contains(n, "default")) {
            gchar *s = obt_xml_node_string(n);
            if (!g_ascii_strcasecmp(s, "horizontal")) {
                settings->max_horz = TRUE;
                settings->max_vert = FALSE;
            } else if (!g_ascii_strcasecmp(s, "vertical")) {
                settings->max_horz = FALSE;
                settings->max_vert = TRUE;
            } else
                settings->max_horz = settings->max_vert =
                    obt_xml_node_bool(n);
            g_free(s);
        }
    }
}

/* Manages settings for individual applications.
   Some notes: monitor is the screen number in a multi monitor
   (Xinerama) setup (starting from 0), or mouse: the monitor the pointer
   is on, active: the active monitor, primary: the primary monitor.
   Layer can be three values, above (Always on top), below
   (Always on bottom) and everything else (normal behaviour).
   Positions can be an integer value or center, which will
   center the window in the specified axis. Position is within
   the monitor, so <position><x>center</x></position><monitor>2</monitor>
   will center the window on the second monitor.
*/
static void parse_per_app_settings(xmlNodePtr node, gpointer d)
{
    xmlNodePtr app = obt_xml_find_node(node->children, "application");
    for (; app; app = obt_xml_find_node(app->next, "application")) {
        ObAppSettings *settings;

        gboolean name_set, class_set, role_set, title_set,
            type_set, group_name_set, group_class_set;
        gchar *name = NULL, *class = NULL, *role = NULL, *title = NULL,
            *type_str = NULL, *group_name = NULL, *group_class = NULL;
        ObClientType type;

        class_set = obt_xml_attr_string(app, "class", &class);
        name_set = obt_xml_attr_string(app, "name", &name);
        group_class_set = obt_xml_attr_string(app, "groupclass", &group_class);
        group_name_set = obt_xml_attr_string(app, "groupname", &group_name);
        type_set = obt_xml_attr_string(app, "type", &type_str);
        role_set = obt_xml_attr_string(app, "role", &role);
        title_set = obt_xml_attr_string(app, "title", &title);

        /* validate the type tho */
        if (type_set) {
            if (!g_ascii_strcasecmp(type_str, "normal"))
                type = OB_CLIENT_TYPE_NORMAL;
            else if (!g_ascii_strcasecmp(type_str, "dialog"))
                type = OB_CLIENT_TYPE_DIALOG;
            else if (!g_ascii_strcasecmp(type_str, "splash"))
                type = OB_CLIENT_TYPE_SPLASH;
            else if (!g_ascii_strcasecmp(type_str, "utility"))
                type = OB_CLIENT_TYPE_UTILITY;
            else if (!g_ascii_strcasecmp(type_str, "menu"))
                type = OB_CLIENT_TYPE_MENU;
            else if (!g_ascii_strcasecmp(type_str, "toolbar"))
                type = OB_CLIENT_TYPE_TOOLBAR;
            else if (!g_ascii_strcasecmp(type_str, "dock"))
                type = OB_CLIENT_TYPE_DOCK;
            else if (!g_ascii_strcasecmp(type_str, "desktop"))
                type = OB_CLIENT_TYPE_DESKTOP;
            else
                type_set = FALSE; /* not valid! */
        }

        if (!(class_set || name_set || role_set || title_set ||
              type_set || group_class_set || group_name_set))
            continue;

        settings = config_create_app_settings();

        if (name_set)
            settings->name = g_pattern_spec_new(name);
        if (class_set)
            settings->class = g_pattern_spec_new(class);
        if (group_name_set)
            settings->group_name = g_pattern_spec_new(group_name);
        if (group_class_set)
            settings->group_class = g_pattern_spec_new(group_class);
        if (role_set)
            settings->role = g_pattern_spec_new(role);
        if (title_set)
            settings->title = g_pattern_spec_new(title);
        if (type_set)
            settings->type = type;

        g_free(name);
        g_free(class);
        g_free(group_name);
        g_free(group_class);
        g_free(role);
        g_free(title);
        g_free(type_str);

        parse_single_per_app_settings(app, settings);
        config_per_app_settings = g_slist_append(config_per_app_settings,
                                                 (gpointer)settings);
    }
}

/*

<keybind key="C-x">
  <action name="ChangeDesktop">
    <desktop>3</desktop>
  </action>
</keybind>

*/

static void parse_key(xmlNodePtr node, GList *keylist)
{
    gchar *keystring, **keys, **key;
    xmlNodePtr n;
    gboolean is_chroot = FALSE;

    if (!obt_xml_attr_string(node, "key", &keystring))
        return;

    obt_xml_attr_bool(node, "chroot", &is_chroot);

    keys = g_strsplit(keystring, " ", 0);
    for (key = keys; *key; ++key) {
        keylist = g_list_append(keylist, *key);

        if ((n = obt_xml_find_node(node->children, "keybind"))) {
            while (n) {
                parse_key(n, keylist);
                n = obt_xml_find_node(n->next, "keybind");
            }
        }
        else if ((n = obt_xml_find_node(node->children, "action"))) {
            while (n) {
                ObActionsAct *action;

                action = actions_parse(n);
                if (action)
                    keyboard_bind(keylist, action);
                n = obt_xml_find_node(n->next, "action");
            }
        }


        if (is_chroot)
            keyboard_chroot(keylist);
        keylist = g_list_delete_link(keylist, g_list_last(keylist));
    }

    g_strfreev(keys);
    g_free(keystring);
}

static void parse_keyboard(xmlNodePtr node, gpointer d)
{
    xmlNodePtr n;
    gchar *key;

    keyboard_unbind_all();

    if ((n = obt_xml_find_node(node->children, "chainQuitKey"))) {
        key = obt_xml_node_string(n);
        translate_key(key, &config_keyboard_reset_state,
                      &config_keyboard_reset_keycode);
        g_free(key);
    }

    if ((n = obt_xml_find_node(node->children, "keybind")))
        while (n) {
            parse_key(n, NULL);
            n = obt_xml_find_node(n->next, "keybind");
        }

    if ((n = obt_xml_find_node(node->children, "rebindOnMappingNotify")))
        config_keyboard_rebind_on_mapping_notify = obt_xml_node_bool(n);
}

/*

<context name="Titlebar">
  <mousebind button="Left" action="Press">
    <action name="Raise"></action>
  </mousebind>
</context>

*/

static void parse_mouse(xmlNodePtr node, gpointer d)
{
    xmlNodePtr n, nbut, nact;
    gchar *buttonstr;
    gchar *cxstr;
    ObMouseAction mact;

    mouse_unbind_all();

    node = node->children;

    if ((n = obt_xml_find_node(node, "dragThreshold")))
        config_mouse_threshold = obt_xml_node_int(n);
    if ((n = obt_xml_find_node(node, "doubleClickTime")))
        config_mouse_dclicktime = obt_xml_node_int(n);
    if ((n = obt_xml_find_node(node, "screenEdgeWarpTime"))) {
        config_mouse_screenedgetime = obt_xml_node_int(n);
        /* minimum value of 25 for this property, when it is 1 and you hit the
           edge it basically never stops */
        if (config_mouse_screenedgetime && config_mouse_screenedgetime < 25)
            config_mouse_screenedgetime = 25;
    }
    if ((n = obt_xml_find_node(node, "screenEdgeWarpMouse")))
        config_mouse_screenedgewarp = obt_xml_node_bool(n);

    for (n = obt_xml_find_node(node, "context");
         n;
         n = obt_xml_find_node(n->next, "context"))
    {
        gchar *modcxstr;
        ObFrameContext cx;

        if (!obt_xml_attr_string(n, "name", &cxstr))
            continue;

        modcxstr = g_strdup(cxstr); /* make a copy to mutilate */
        while (frame_next_context_from_string(modcxstr, &cx)) {
            if (!cx) {
                gchar *s = strchr(modcxstr, ' ');
                if (s) {
                    *s = '\0';
                    g_message(_("Invalid context \"%s\" in mouse binding"),
                              modcxstr);
                    *s = ' ';
                }
                continue;
            }

            for (nbut = obt_xml_find_node(n->children, "mousebind");
                 nbut;
                 nbut = obt_xml_find_node(nbut->next, "mousebind"))
            {

                gchar **button, **buttons;

                if (!obt_xml_attr_string(nbut, "button", &buttonstr))
                    continue;
                if (obt_xml_attr_contains(nbut, "action", "press"))
                    mact = OB_MOUSE_ACTION_PRESS;
                else if (obt_xml_attr_contains(nbut, "action", "release"))
                    mact = OB_MOUSE_ACTION_RELEASE;
                else if (obt_xml_attr_contains(nbut, "action", "click"))
                    mact = OB_MOUSE_ACTION_CLICK;
                else if (obt_xml_attr_contains(nbut, "action","doubleclick"))
                    mact = OB_MOUSE_ACTION_DOUBLE_CLICK;
                else if (obt_xml_attr_contains(nbut, "action", "drag"))
                    mact = OB_MOUSE_ACTION_MOTION;
                else
                    continue;

                buttons = g_strsplit(buttonstr, " ", 0);
                for (nact = obt_xml_find_node(nbut->children, "action");
                     nact;
                     nact = obt_xml_find_node(nact->next, "action"))
                {
                    ObActionsAct *action;

                    /* actions_parse() creates one ref to the action, but we need
                     * exactly one ref per binding we use it for. */
                    if ((action = actions_parse(nact))) {
                        for (button = buttons; *button; ++button) {
                            actions_act_ref(action);
                            mouse_bind(*button, cx, mact, action);
                        }
                        actions_act_unref(action);
                    }
                }
                g_strfreev(buttons);
                g_free(buttonstr);
            }
        }
        g_free(modcxstr);
        g_free(cxstr);
    }
}

static void parse_focus(xmlNodePtr node, gpointer d)
{
    xmlNodePtr n;

    node = node->children;

    if ((n = obt_xml_find_node(node, "focusNew")))
        config_focus_new = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "followMouse")))
        config_focus_follow = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "focusDelay")))
        config_focus_delay = obt_xml_node_int(n);
    if ((n = obt_xml_find_node(node, "raiseOnFocus")))
        config_focus_raise = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "focusLast")))
        config_focus_last = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "underMouse")))
        config_focus_under_mouse = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "unfocusOnLeave")))
        config_unfocus_leave = obt_xml_node_bool(n);
}

static void parse_placement(xmlNodePtr node, gpointer d)
{
    xmlNodePtr n;

    node = node->children;

    if ((n = obt_xml_find_node(node, "policy"))) {
        if (obt_xml_node_contains(n, "UnderMouse"))
            config_place_policy = OB_PLACE_POLICY_MOUSE;
    }
    if ((n = obt_xml_find_node(node, "center"))) {
        config_place_center = obt_xml_node_bool(n);
    }
    if ((n = obt_xml_find_node(node, "monitor"))) {
        if (obt_xml_node_contains(n, "active"))
            config_place_monitor = OB_PLACE_MONITOR_ACTIVE;
        else if (obt_xml_node_contains(n, "mouse"))
            config_place_monitor = OB_PLACE_MONITOR_MOUSE;
        else if (obt_xml_node_contains(n, "any"))
            config_place_monitor = OB_PLACE_MONITOR_ANY;
    }
    if ((n = obt_xml_find_node(node, "primaryMonitor"))) {
        config_primary_monitor_index = obt_xml_node_int(n);
        if (!config_primary_monitor_index) {
            if (obt_xml_node_contains(n, "mouse"))
                config_primary_monitor = OB_PLACE_MONITOR_MOUSE;
        }
    }
}

static void parse_margins(xmlNodePtr node, gpointer d)
{
    xmlNodePtr n;

    node = node->children;

    if ((n = obt_xml_find_node(node, "top")))
        config_margins.top = MAX(0, obt_xml_node_int(n));
    if ((n = obt_xml_find_node(node, "left")))
        config_margins.left = MAX(0, obt_xml_node_int(n));
    if ((n = obt_xml_find_node(node, "right")))
        config_margins.right = MAX(0, obt_xml_node_int(n));
    if ((n = obt_xml_find_node(node, "bottom")))
        config_margins.bottom = MAX(0, obt_xml_node_int(n));
}

static void parse_theme(xmlNodePtr node, gpointer d)
{
    xmlNodePtr n;

    node = node->children;

    if ((n = obt_xml_find_node(node, "name"))) {
        gchar *c;

        g_free(config_theme);
        c = obt_xml_node_string(n);
        config_theme = obt_paths_expand_tilde(c);
        g_free(c);
    }
    if ((n = obt_xml_find_node(node, "titleLayout"))) {
        gchar *c, *d;

        g_free(config_title_layout);
        config_title_layout = obt_xml_node_string(n);

        /* replace duplicates with spaces */
        for (c = config_title_layout; *c != '\0'; ++c)
            for (d = c+1; *d != '\0'; ++d)
                if (*c == *d) *d = ' ';
    }
    if ((n = obt_xml_find_node(node, "keepBorder")))
        config_theme_keepborder = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "animateIconify")))
        config_animate_iconify = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "windowListIconSize"))) {
        config_theme_window_list_icon_size = obt_xml_node_int(n);
        if (config_theme_window_list_icon_size < 16)
            config_theme_window_list_icon_size = 16;
        else if (config_theme_window_list_icon_size > 96)
            config_theme_window_list_icon_size = 96;
    }

    for (n = obt_xml_find_node(node, "font");
         n;
         n = obt_xml_find_node(n->next, "font"))
    {
        xmlNodePtr   fnode;
        RrFont     **font;
        gchar       *name = g_strdup(RrDefaultFontFamily);
        gint         size = RrDefaultFontSize;
        RrFontWeight weight = RrDefaultFontWeight;
        RrFontSlant  slant = RrDefaultFontSlant;

        if (obt_xml_attr_contains(n, "place", "ActiveWindow"))
            font = &config_font_activewindow;
        else if (obt_xml_attr_contains(n, "place", "InactiveWindow"))
            font = &config_font_inactivewindow;
        else if (obt_xml_attr_contains(n, "place", "MenuHeader"))
            font = &config_font_menutitle;
        else if (obt_xml_attr_contains(n, "place", "MenuItem"))
            font = &config_font_menuitem;
        else if (obt_xml_attr_contains(n, "place", "ActiveOnScreenDisplay"))
            font = &config_font_activeosd;
        else if (obt_xml_attr_contains(n, "place", "OnScreenDisplay"))
            font = &config_font_activeosd;
        else if (obt_xml_attr_contains(n, "place","InactiveOnScreenDisplay"))
            font = &config_font_inactiveosd;
        else
            continue;

        if ((fnode = obt_xml_find_node(n->children, "name"))) {
            g_free(name);
            name = obt_xml_node_string(fnode);
        }
        if ((fnode = obt_xml_find_node(n->children, "size"))) {
            int s = obt_xml_node_int(fnode);
            if (s > 0) size = s;
        }
        if ((fnode = obt_xml_find_node(n->children, "weight"))) {
            gchar *w = obt_xml_node_string(fnode);
            if (!g_ascii_strcasecmp(w, "Bold"))
                weight = RR_FONTWEIGHT_BOLD;
            g_free(w);
        }
        if ((fnode = obt_xml_find_node(n->children, "slant"))) {
            gchar *s = obt_xml_node_string(fnode);
            if (!g_ascii_strcasecmp(s, "Italic"))
                slant = RR_FONTSLANT_ITALIC;
            if (!g_ascii_strcasecmp(s, "Oblique"))
                slant = RR_FONTSLANT_OBLIQUE;
            g_free(s);
        }

        *font = RrFontOpen(ob_rr_inst, name, size, weight, slant);
        g_free(name);
    }
}

static void parse_desktops(xmlNodePtr node, gpointer d)
{
    xmlNodePtr n;

    node = node->children;

    if ((n = obt_xml_find_node(node, "number"))) {
        gint d = obt_xml_node_int(n);
        if (d > 0)
            config_desktops_num = (unsigned) d;
    }
    if ((n = obt_xml_find_node(node, "firstdesk"))) {
        gint d = obt_xml_node_int(n);
        if (d > 0)
            config_screen_firstdesk = (unsigned) d;
    }
    if ((n = obt_xml_find_node(node, "names"))) {
        GSList *it;
        xmlNodePtr nname;

        for (it = config_desktops_names; it; it = it->next)
            g_free(it->data);
        g_slist_free(config_desktops_names);
        config_desktops_names = NULL;

        for (nname = obt_xml_find_node(n->children, "name");
             nname;
             nname = obt_xml_find_node(nname->next, "name"))
        {
            config_desktops_names =
                g_slist_append(config_desktops_names,
                               obt_xml_node_string(nname));
        }
    }
    if ((n = obt_xml_find_node(node, "popupTime")))
        config_desktop_popup_time = obt_xml_node_int(n);
}

static void parse_resize(xmlNodePtr node, gpointer d)
{
    xmlNodePtr n;

    node = node->children;

    if ((n = obt_xml_find_node(node, "drawContents")))
        config_resize_redraw = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "popupShow"))) {
        config_resize_popup_show = obt_xml_node_int(n);
        if (obt_xml_node_contains(n, "Always"))
            config_resize_popup_show = 2;
        else if (obt_xml_node_contains(n, "Never"))
            config_resize_popup_show = 0;
        else if (obt_xml_node_contains(n, "Nonpixel"))
            config_resize_popup_show = 1;
    }
    if ((n = obt_xml_find_node(node, "popupPosition"))) {
        if (obt_xml_node_contains(n, "Top"))
            config_resize_popup_pos = OB_RESIZE_POS_TOP;
        else if (obt_xml_node_contains(n, "Center"))
            config_resize_popup_pos = OB_RESIZE_POS_CENTER;
        else if (obt_xml_node_contains(n, "Fixed")) {
            config_resize_popup_pos = OB_RESIZE_POS_FIXED;

            if ((n = obt_xml_find_node(node, "popupFixedPosition"))) {
                xmlNodePtr n2;

                if ((n2 = obt_xml_find_node(n->children, "x")))
                    config_parse_gravity_coord(n2,
                                               &config_resize_popup_fixed.x);
                if ((n2 = obt_xml_find_node(n->children, "y")))
                    config_parse_gravity_coord(n2,
                                               &config_resize_popup_fixed.y);

                config_resize_popup_fixed.x.pos =
                    MAX(config_resize_popup_fixed.x.pos, 0);
                config_resize_popup_fixed.y.pos =
                    MAX(config_resize_popup_fixed.y.pos, 0);
            }
        }
    }
}

static void parse_dock(xmlNodePtr node, gpointer d)
{
    xmlNodePtr n;

    node = node->children;

    if ((n = obt_xml_find_node(node, "position"))) {
        if (obt_xml_node_contains(n, "TopLeft"))
            config_dock_floating = FALSE,
            config_dock_pos = OB_DIRECTION_NORTHWEST;
        else if (obt_xml_node_contains(n, "Top"))
            config_dock_floating = FALSE,
            config_dock_pos = OB_DIRECTION_NORTH;
        else if (obt_xml_node_contains(n, "TopRight"))
            config_dock_floating = FALSE,
            config_dock_pos = OB_DIRECTION_NORTHEAST;
        else if (obt_xml_node_contains(n, "Right"))
            config_dock_floating = FALSE,
            config_dock_pos = OB_DIRECTION_EAST;
        else if (obt_xml_node_contains(n, "BottomRight"))
            config_dock_floating = FALSE,
            config_dock_pos = OB_DIRECTION_SOUTHEAST;
        else if (obt_xml_node_contains(n, "Bottom"))
            config_dock_floating = FALSE,
            config_dock_pos = OB_DIRECTION_SOUTH;
        else if (obt_xml_node_contains(n, "BottomLeft"))
            config_dock_floating = FALSE,
            config_dock_pos = OB_DIRECTION_SOUTHWEST;
        else if (obt_xml_node_contains(n, "Left"))
            config_dock_floating = FALSE,
            config_dock_pos = OB_DIRECTION_WEST;
        else if (obt_xml_node_contains(n, "Floating"))
            config_dock_floating = TRUE;
    }
    if (config_dock_floating) {
        if ((n = obt_xml_find_node(node, "floatingX")))
            config_dock_x = obt_xml_node_int(n);
        if ((n = obt_xml_find_node(node, "floatingY")))
            config_dock_y = obt_xml_node_int(n);
    } else {
        if ((n = obt_xml_find_node(node, "noStrut")))
            config_dock_nostrut = obt_xml_node_bool(n);
    }
    if ((n = obt_xml_find_node(node, "stacking"))) {
        if (obt_xml_node_contains(n, "normal"))
            config_dock_layer = OB_STACKING_LAYER_NORMAL;
        else if (obt_xml_node_contains(n, "below"))
            config_dock_layer = OB_STACKING_LAYER_BELOW;
        else if (obt_xml_node_contains(n, "above"))
            config_dock_layer = OB_STACKING_LAYER_ABOVE;
    }
    if ((n = obt_xml_find_node(node, "direction"))) {
        if (obt_xml_node_contains(n, "horizontal"))
            config_dock_orient = OB_ORIENTATION_HORZ;
        else if (obt_xml_node_contains(n, "vertical"))
            config_dock_orient = OB_ORIENTATION_VERT;
    }
    if ((n = obt_xml_find_node(node, "autoHide")))
        config_dock_hide = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "hideDelay")))
        config_dock_hide_delay = obt_xml_node_int(n);
    if ((n = obt_xml_find_node(node, "showDelay")))
        config_dock_show_delay = obt_xml_node_int(n);
    if ((n = obt_xml_find_node(node, "moveButton"))) {
        gchar *str = obt_xml_node_string(n);
        guint b = 0, s = 0;
        if (translate_button(str, &s, &b)) {
            config_dock_app_move_button = b;
            config_dock_app_move_modifiers = s;
        } else {
            g_message(_("Invalid button \"%s\" specified in config file"), str);
        }
        g_free(str);
    }
}

static void parse_menu(xmlNodePtr node, gpointer d)
{
    xmlNodePtr n;
    node = node->children;

    if ((n = obt_xml_find_node(node, "hideDelay")))
        config_menu_hide_delay = obt_xml_node_int(n);
    if ((n = obt_xml_find_node(node, "middle")))
        config_menu_middle = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "submenuShowDelay")))
        config_submenu_show_delay = obt_xml_node_int(n);
    if ((n = obt_xml_find_node(node, "submenuHideDelay")))
        config_submenu_hide_delay = obt_xml_node_int(n);
    if ((n = obt_xml_find_node(node, "manageDesktops")))
        config_menu_manage_desktops = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "showIcons"))) {
        config_menu_show_icons = obt_xml_node_bool(n);
#if !defined(USE_IMLIB2) && !defined(USE_LIBRSVG)
        if (config_menu_show_icons)
            g_message(_("Openbox was compiled without image loading support. Icons in menus will not be loaded."));
#endif
    }

    for (node = obt_xml_find_node(node, "file");
         node;
         node = obt_xml_find_node(node->next, "file"))
    {
            gchar *c = obt_xml_node_string(node);
            config_menu_files = g_slist_append(config_menu_files,
                                               obt_paths_expand_tilde(c));
            g_free(c);
    }
}

static void parse_resistance(xmlNodePtr node, gpointer d)
{
    xmlNodePtr n;

    node = node->children;
    if ((n = obt_xml_find_node(node, "strength")))
        config_resist_win = obt_xml_node_int(n);
    if ((n = obt_xml_find_node(node, "screen_edge_strength")))
        config_resist_edge = obt_xml_node_int(n);
}

typedef struct
{
    const gchar *key;
    const gchar *actname;
} ObDefKeyBind;

static void bind_default_keyboard(void)
{
    ObDefKeyBind *it;
    ObDefKeyBind binds[] = {
        { "A-Tab", "NextWindow" },
        { "S-A-Tab", "PreviousWindow" },
        { "A-F4", "Close" },
        { NULL, NULL }
    };
    for (it = binds; it->key; ++it) {
        GList *l = g_list_append(NULL, g_strdup(it->key));
        keyboard_bind(l, actions_parse_string(it->actname));
    }
}

typedef struct
{
    const gchar *button;
    const gchar *context;
    const ObMouseAction mact;
    const gchar *actname;
} ObDefMouseBind;

static void bind_default_mouse(void)
{
    ObDefMouseBind *it;
    ObDefMouseBind binds[] = {
        { "Left", "Client", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Middle", "Client", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Right", "Client", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Left", "Desktop", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Middle", "Desktop", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Right", "Desktop", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Left", "Titlebar", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Left", "Bottom", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Left", "BLCorner", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Left", "BRCorner", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Left", "TLCorner", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Left", "TRCorner", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Left", "Close", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Left", "Maximize", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Left", "Iconify", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Left", "Icon", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Left", "AllDesktops", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Left", "Shade", OB_MOUSE_ACTION_PRESS, "Focus" },
        { "Left", "Client", OB_MOUSE_ACTION_CLICK, "Raise" },
        { "Left", "Titlebar", OB_MOUSE_ACTION_CLICK, "Raise" },
        { "Middle", "Titlebar", OB_MOUSE_ACTION_CLICK, "Lower" },
        { "Left", "BLCorner", OB_MOUSE_ACTION_CLICK, "Raise" },
        { "Left", "BRCorner", OB_MOUSE_ACTION_CLICK, "Raise" },
        { "Left", "TLCorner", OB_MOUSE_ACTION_CLICK, "Raise" },
        { "Left", "TRCorner", OB_MOUSE_ACTION_CLICK, "Raise" },
        { "Left", "Close", OB_MOUSE_ACTION_CLICK, "Raise" },
        { "Left", "Maximize", OB_MOUSE_ACTION_CLICK, "Raise" },
        { "Left", "Iconify", OB_MOUSE_ACTION_CLICK, "Raise" },
        { "Left", "Icon", OB_MOUSE_ACTION_CLICK, "Raise" },
        { "Left", "AllDesktops", OB_MOUSE_ACTION_CLICK, "Raise" },
        { "Left", "Shade", OB_MOUSE_ACTION_CLICK, "Raise" },
        { "Left", "Close", OB_MOUSE_ACTION_CLICK, "Close" },
        { "Left", "Maximize", OB_MOUSE_ACTION_CLICK, "ToggleMaximize" },
        { "Left", "Iconify", OB_MOUSE_ACTION_CLICK, "Iconify" },
        { "Left", "AllDesktops", OB_MOUSE_ACTION_CLICK, "ToggleOmnipresent" },
        { "Left", "Shade", OB_MOUSE_ACTION_CLICK, "ToggleShade" },
        { "Left", "TLCorner", OB_MOUSE_ACTION_MOTION, "Resize" },
        { "Left", "TRCorner", OB_MOUSE_ACTION_MOTION, "Resize" },
        { "Left", "BLCorner", OB_MOUSE_ACTION_MOTION, "Resize" },
        { "Left", "BRCorner", OB_MOUSE_ACTION_MOTION, "Resize" },
        { "Left", "Top", OB_MOUSE_ACTION_MOTION, "Resize" },
        { "Left", "Bottom", OB_MOUSE_ACTION_MOTION, "Resize" },
        { "Left", "Left", OB_MOUSE_ACTION_MOTION, "Resize" },
        { "Left", "Right", OB_MOUSE_ACTION_MOTION, "Resize" },
        { "Left", "Titlebar", OB_MOUSE_ACTION_MOTION, "Move" },
        { "A-Left", "Frame", OB_MOUSE_ACTION_MOTION, "Move" },
        { "A-Middle", "Frame", OB_MOUSE_ACTION_MOTION, "Resize" },
        { NULL, NULL, 0, NULL }
    };

    for (it = binds; it->button; ++it)
        mouse_bind(it->button, frame_context_from_string(it->context),
                   it->mact, actions_parse_string(it->actname));
}

void config_startup(ObtXmlInst *i)
{
    config_focus_new = TRUE;
    config_focus_follow = FALSE;
    config_focus_delay = 0;
    config_focus_raise = FALSE;
    config_focus_last = TRUE;
    config_focus_under_mouse = FALSE;
    config_unfocus_leave = FALSE;

    obt_xml_register(i, "focus", parse_focus, NULL);

    config_place_policy = OB_PLACE_POLICY_SMART;
    config_place_center = TRUE;
    config_place_monitor = OB_PLACE_MONITOR_PRIMARY;

    config_primary_monitor_index = 1;
    config_primary_monitor = OB_PLACE_MONITOR_ACTIVE;

    obt_xml_register(i, "placement", parse_placement, NULL);

    STRUT_PARTIAL_SET(config_margins, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    obt_xml_register(i, "margins", parse_margins, NULL);

    config_theme = NULL;

    config_animate_iconify = TRUE;
    config_title_layout = g_strdup("NLIMC");
    config_theme_keepborder = TRUE;
    config_theme_window_list_icon_size = 36;

    config_font_activewindow = NULL;
    config_font_inactivewindow = NULL;
    config_font_menuitem = NULL;
    config_font_menutitle = NULL;
    config_font_activeosd = NULL;
    config_font_inactiveosd = NULL;

    obt_xml_register(i, "theme", parse_theme, NULL);

    config_desktops_num = 4;
    config_screen_firstdesk = 1;
    config_desktops_names = NULL;
    config_desktop_popup_time = 875;

    obt_xml_register(i, "desktops", parse_desktops, NULL);

    config_resize_redraw = TRUE;
    config_resize_popup_show = 1; /* nonpixel increments */
    config_resize_popup_pos = OB_RESIZE_POS_CENTER;
    GRAVITY_COORD_SET(config_resize_popup_fixed.x, 0, FALSE, FALSE);
    GRAVITY_COORD_SET(config_resize_popup_fixed.y, 0, FALSE, FALSE);

    obt_xml_register(i, "resize", parse_resize, NULL);

    config_dock_layer = OB_STACKING_LAYER_ABOVE;
    config_dock_pos = OB_DIRECTION_NORTHEAST;
    config_dock_floating = FALSE;
    config_dock_nostrut = FALSE;
    config_dock_x = 0;
    config_dock_y = 0;
    config_dock_orient = OB_ORIENTATION_VERT;
    config_dock_hide = FALSE;
    config_dock_hide_delay = 300;
    config_dock_show_delay = 300;
    config_dock_app_move_button = 2; /* middle */
    config_dock_app_move_modifiers = 0;

    obt_xml_register(i, "dock", parse_dock, NULL);

    translate_key("C-g", &config_keyboard_reset_state,
                  &config_keyboard_reset_keycode);
    config_keyboard_rebind_on_mapping_notify = TRUE;

    bind_default_keyboard();

    obt_xml_register(i, "keyboard", parse_keyboard, NULL);

    config_mouse_threshold = 8;
    config_mouse_dclicktime = 500;
    config_mouse_screenedgetime = 400;
    config_mouse_screenedgewarp = FALSE;

    bind_default_mouse();

    obt_xml_register(i, "mouse", parse_mouse, NULL);

    config_resist_win = 10;
    config_resist_edge = 20;

    obt_xml_register(i, "resistance", parse_resistance, NULL);

    config_menu_hide_delay = 250;
    config_menu_middle = FALSE;
    config_submenu_show_delay = 100;
    config_submenu_hide_delay = 400;
    config_menu_manage_desktops = TRUE;
    config_menu_files = NULL;
    config_menu_show_icons = TRUE;

    obt_xml_register(i, "menu", parse_menu, NULL);

    config_per_app_settings = NULL;

    obt_xml_register(i, "applications", parse_per_app_settings, NULL);
}

void config_shutdown(void)
{
    GSList *it;

    g_free(config_theme);

    g_free(config_title_layout);

    RrFontClose(config_font_activewindow);
    RrFontClose(config_font_inactivewindow);
    RrFontClose(config_font_menuitem);
    RrFontClose(config_font_menutitle);
    RrFontClose(config_font_activeosd);
    RrFontClose(config_font_inactiveosd);

    for (it = config_desktops_names; it; it = g_slist_next(it))
        g_free(it->data);
    g_slist_free(config_desktops_names);

    for (it = config_menu_files; it; it = g_slist_next(it))
        g_free(it->data);
    g_slist_free(config_menu_files);

    for (it = config_per_app_settings; it; it = g_slist_next(it)) {
        ObAppSettings *itd = (ObAppSettings *)it->data;
        if (itd->name) g_pattern_spec_free(itd->name);
        if (itd->role) g_pattern_spec_free(itd->role);
        if (itd->title) g_pattern_spec_free(itd->title);
        if (itd->class) g_pattern_spec_free(itd->class);
        if (itd->group_name) g_pattern_spec_free(itd->group_name);
        if (itd->group_class) g_pattern_spec_free(itd->group_class);
        g_slice_free(ObAppSettings, it->data);
    }
    g_slist_free(config_per_app_settings);
}
