/*
 * Copyright (C) 2008 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2008 Fred Chien <fred@lxde.org>
 *               2009 Marty Jack <martyj19@comcast.net>
 *               2012 Michael Rawson <michaelrawson76@gmail.com>
 *               2012 Julien Lavergne <julien.lavergne@gmail.com>
 *               2014 Andriy Grytsenko <andrej@rep.kiev.ua>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "plugin.h"
#include "misc.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>

#include <X11/XKBlib.h>

#include "icon-grid.h"

static const char * on_icons_theme[] = {
    "capslock-on",
    "numlock-on",
    "scrllock-on"
};

static const char * off_icons_theme[] = {
    "capslock-off",
    "numlock-off",
    "scrllock-off"
};

static int xkb_event_base = 0;
static int xkb_error_base = 0;

/* Private context for keyboard LED plugin. */
typedef struct {
    config_setting_t *settings;
    GtkWidget *indicator_image[3];		/* Image for each indicator */
    unsigned int current_state;			/* Current LED state, bit encoded */
    gboolean visible[3];			/* True if control is visible (per user configuration) */
} KeyboardLEDPlugin;

static void kbled_update_image(KeyboardLEDPlugin * kl, int i, unsigned int state);
static void kbled_update_display(KeyboardLEDPlugin * kl, unsigned int state);
static void kbled_destructor(gpointer user_data);

/* Update image to correspond to current state. */
static void kbled_update_image(KeyboardLEDPlugin * kl, int i, unsigned int state)
{
    lxpanel_image_change_icon(kl->indicator_image[i],
                              (state ? on_icons_theme[i] : off_icons_theme[i]),
                              NULL);
}

/* Redraw after Xkb event or initialization. */
static void kbled_update_display(KeyboardLEDPlugin * kl, unsigned int new_state)
{
    int i;
    for (i = 0; i < 3; i++)
    {
        /* If the control changed state, redraw it. */
        int current_is_lit = kl->current_state & (1 << i);
        int new_is_lit = new_state & (1 << i);
        if (current_is_lit != new_is_lit)
            kbled_update_image(kl, i, new_is_lit);
    }

    /* Save new state. */
    kl->current_state = new_state;
}

/* GDK event filter. */
static GdkFilterReturn kbled_event_filter(GdkXEvent * gdkxevent, GdkEvent * event, KeyboardLEDPlugin * kl)
{
    /* Look for XkbIndicatorStateNotify events and update the display. */
    XEvent * xev = (XEvent *) gdkxevent;
    if (xev->xany.type == xkb_event_base + XkbEventCode)
    {
        XkbEvent * xkbev = (XkbEvent *) xev;
        if (xkbev->any.xkb_type == XkbIndicatorStateNotify)
            kbled_update_display(kl, xkbev->indicators.state);
    }
    return GDK_FILTER_CONTINUE;
}

/* Plugin constructor. */
static GtkWidget *kbled_constructor(LXPanel *panel, config_setting_t *settings)
{
    /* Allocate and initialize plugin context and set into Plugin private data pointer. */
    KeyboardLEDPlugin * kl = g_new0(KeyboardLEDPlugin, 1);
    GtkWidget *p;
    int i;
    unsigned int current_state;
    Display *xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());

    kl->settings = settings;
    kl->visible[0] = FALSE;
    kl->visible[1] = TRUE;
    kl->visible[2] = TRUE;

    /* Load parameters from the configuration file. */
    if (config_setting_lookup_int(settings, "ShowCapsLock", &i))
        kl->visible[0] = i != 0;
    if (config_setting_lookup_int(settings, "ShowNumLock", &i))
        kl->visible[1] = i != 0;
    if (config_setting_lookup_int(settings, "ShowScrollLock", &i))
        kl->visible[2] = i != 0;

    /* Allocate top level widget and set into Plugin widget pointer. */
    p = panel_icon_grid_new(panel_get_orientation(panel),
                            panel_get_icon_size(panel),
                            panel_get_icon_size(panel),
                            1, 0, panel_get_height(panel));
    lxpanel_plugin_set_data(p, kl, kbled_destructor);

    /* Then allocate three images for the three indications, but make them visible only when the configuration requests. */
    for (i = 0; i < 3; i++)
    {
        kl->indicator_image[i] = lxpanel_image_new_for_icon(panel, off_icons_theme[i], -1, NULL);
        gtk_container_add(GTK_CONTAINER(p), kl->indicator_image[i]);
        gtk_widget_set_visible(kl->indicator_image[i], kl->visible[i]);
    }

    /* Initialize Xkb extension if not yet done. */
    if (xkb_event_base == 0)
    {
        int opcode;
        int maj = XkbMajorVersion;
        int min = XkbMinorVersion;
        if ( ! XkbLibraryVersion(&maj, &min))
            return 0;
        if ( ! XkbQueryExtension(xdisplay, &opcode, &xkb_event_base, &xkb_error_base, &maj, &min))
            return 0;
    }

    /* Add GDK event filter and enable XkbIndicatorStateNotify events. */
    gdk_window_add_filter(NULL, (GdkFilterFunc) kbled_event_filter, kl);
    if ( ! XkbSelectEvents(xdisplay, XkbUseCoreKbd, XkbIndicatorStateNotifyMask, XkbIndicatorStateNotifyMask))
        return 0;

    /* Get current indicator state and update display.
     * Force current state to differ in all bits so a full redraw will occur. */
    XkbGetIndicatorState(xdisplay, XkbUseCoreKbd, &current_state);
    kl->current_state = ~ current_state;
    kbled_update_display(kl, current_state);

    /* Show the widget. */
    return p;
}

/* Plugin destructor. */
static void kbled_destructor(gpointer user_data)
{
    KeyboardLEDPlugin * kl = (KeyboardLEDPlugin *) user_data;

    /* Remove GDK event filter. */
    gdk_window_remove_filter(NULL, (GdkFilterFunc) kbled_event_filter, kl);
    g_free(kl);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static gboolean kbled_apply_configuration(gpointer user_data)
{
    KeyboardLEDPlugin * kl = lxpanel_plugin_get_data(user_data);
    int i;

    for (i = 0; i < 3; i++)
        gtk_widget_set_visible(kl->indicator_image[i], kl->visible[i]);
    config_group_set_int(kl->settings, "ShowCapsLock", kl->visible[0]);
    config_group_set_int(kl->settings, "ShowNumLock", kl->visible[1]);
    config_group_set_int(kl->settings, "ShowScrollLock", kl->visible[2]);
    return FALSE;
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *kbled_configure(LXPanel *panel, GtkWidget *p)
{
    KeyboardLEDPlugin * kl = lxpanel_plugin_get_data(p);
    GtkWidget * dlg = lxpanel_generic_config_dlg(_("Keyboard LED"),
        panel, kbled_apply_configuration, p,
        _("Show CapsLock"), &kl->visible[0], CONF_TYPE_BOOL,
        _("Show NumLock"), &kl->visible[1], CONF_TYPE_BOOL,
        _("Show ScrollLock"), &kl->visible[2], CONF_TYPE_BOOL,
        NULL);
    gtk_widget_set_size_request(GTK_WIDGET(dlg), 200, -1);	/* Improve geometry */
    return dlg;
}

/* Callback when panel configuration changes. */
static void kbled_panel_configuration_changed(LXPanel *panel, GtkWidget *p)
{
    /* Set orientation into the icon grid. */
    KeyboardLEDPlugin * kl = lxpanel_plugin_get_data(p);

    panel_icon_grid_set_geometry(PANEL_ICON_GRID(p), panel_get_orientation(panel),
                                 panel_get_icon_size(panel),
                                 panel_get_icon_size(panel),
                                 1, 0, panel_get_height(panel));

    /* Do a full redraw. */
    int current_state = kl->current_state;
    kl->current_state = ~ kl->current_state;
    kbled_update_display(kl, current_state);
}

FM_DEFINE_MODULE(lxpanel_gtk, kbled)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Keyboard LED"),
    .description = N_("Indicators for CapsLock, NumLock, and ScrollLock keys"),

    .new_instance = kbled_constructor,
    .config = kbled_configure,
    .reconfigure = kbled_panel_configuration_changed
};
