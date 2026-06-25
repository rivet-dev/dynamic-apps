/*
 *  xfworkspace - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2007 Brian Tarricone, <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 *  Random portions taken from or inspired by the original xfworkspace for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <ctype.h>
#include <errno.h>

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <xfconf/xfconf.h>

#include "xfdesktop-common.h"
#include "xfce-workspace.h"
#include "xfce-desktop-enum-types.h"

struct _XfceWorkspacePrivate
{
    GdkScreen *gscreen;

    XfconfChannel *channel;
    gchar *property_prefix;

    guint workspace_num;
    guint nbackdrops;
    gboolean xinerama_stretch;
    XfceBackdrop **backdrops;

    gulong *first_color_id;
    gulong *second_color_id;
};

enum
{
    WORKSPACE_BACKDROP_CHANGED,
    N_SIGNALS,
};

static guint signals[N_SIGNALS] = { 0, };

static void xfce_workspace_finalize(GObject *object);
static void xfce_workspace_set_property(GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec);
static void xfce_workspace_get_property(GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec);

static void xfce_workspace_connect_backdrop_settings(XfceWorkspace *workspace,
                                                   XfceBackdrop *backdrop,
                                                   guint monitor);
static void xfce_workspace_disconnect_backdrop_settings(XfceWorkspace *workspace,
                                                        XfceBackdrop *backdrop,
                                                        guint monitor);

static void xfce_workspace_remove_backdrops(XfceWorkspace *workspace);

G_DEFINE_TYPE_WITH_PRIVATE(XfceWorkspace, xfce_workspace, G_TYPE_OBJECT)


/**
 * xfce_workspace_get_xinerama_stretch:
 * @workspace: An #XfceWorkspace.
 *
 * returns whether the first backdrop is set to spanning screens since it's
 * the only backdrop where that setting is applicable.
 **/
gboolean
xfce_workspace_get_xinerama_stretch(XfceWorkspace *workspace)
{
    g_return_val_if_fail(XFCE_IS_WORKSPACE(workspace), FALSE);
    g_return_val_if_fail(workspace->priv->backdrops != NULL, FALSE);
    g_return_val_if_fail(XFCE_IS_BACKDROP(workspace->priv->backdrops[0]), FALSE);

    return xfce_backdrop_get_image_style(workspace->priv->backdrops[0]) == XFCE_BACKDROP_IMAGE_SPANNING_SCREENS;
}

static void
xfce_workspace_set_xfconf_property_string(XfceWorkspace *workspace,
                                          guint monitor_num,
                                          const gchar *property,
                                          const gchar *value)
{
    XfconfChannel *channel = workspace->priv->channel;
    char buf[1024];
    GdkDisplay *display;
    gchar *monitor_name = NULL;

    TRACE("entering");

    display = gdk_display_get_default();
    monitor_name = g_strdup(gdk_monitor_get_model(gdk_display_get_monitor(display, monitor_num)));

    /* Get the backdrop's image property */
    if(monitor_name == NULL) {
        g_snprintf(buf, sizeof(buf), "%smonitor%d/workspace%d/%s",
                   workspace->priv->property_prefix, monitor_num, workspace->priv->workspace_num, property);
    } else {
        g_snprintf(buf, sizeof(buf), "%smonitor%s/workspace%d/%s",
                   workspace->priv->property_prefix, xfdesktop_remove_whitspaces(monitor_name), workspace->priv->workspace_num, property);

        g_free(monitor_name);
    }

    XF_DEBUG("setting %s to %s", buf, value);

    xfconf_channel_set_string(channel, buf, value);
}

static void
xfce_workspace_set_xfconf_property_value(XfceWorkspace *workspace,
                                         guint monitor_num,
                                         const gchar *property,
                                         const GValue *value)
{
    XfconfChannel *channel = workspace->priv->channel;
    char buf[1024];
    gchar *monitor_name = NULL;
    GdkDisplay *display;
#ifdef G_ENABLE_DEBUG
    gchar *contents = NULL;
#endif

    TRACE("entering");

    display = gdk_display_get_default();
    monitor_name = g_strdup(gdk_monitor_get_model(gdk_display_get_monitor(display, monitor_num)));

    /* Get the backdrop's image property */
    if(monitor_name == NULL) {
        g_snprintf(buf, sizeof(buf), "%smonitor%d/workspace%d/%s",
                   workspace->priv->property_prefix, monitor_num, workspace->priv->workspace_num, property);
    } else {
        g_snprintf(buf, sizeof(buf), "%smonitor%s/workspace%d/%s",
                   workspace->priv->property_prefix, xfdesktop_remove_whitspaces(monitor_name), workspace->priv->workspace_num, property);

        g_free(monitor_name);
    }

#ifdef G_ENABLE_DEBUG
    contents = g_strdup_value_contents(value);
    DBG("setting %s to %s", buf, contents);
    g_free(contents);
#endif

    xfconf_channel_set_property(channel, buf, value);
}

static void
xfce_workspace_change_backdrop(XfceWorkspace *workspace,
                               XfceBackdrop *backdrop,
                               const gchar *backdrop_file)
{
    guint i, monitor_num = 0;

    g_return_if_fail(workspace->priv->backdrops);

    TRACE("entering");

    /* Find out which monitor we're on */
    for(i = 0; i < workspace->priv->nbackdrops; ++i) {
        if(backdrop == workspace->priv->backdrops[i]) {
            monitor_num = i;
            XF_DEBUG("monitor_num %d", monitor_num);
            break;
        }
    }


    /* Update the property so that xfdesktop won't show the same image every
     * time it starts up when the user wants it to cycle different images */
    xfce_workspace_set_xfconf_property_string(workspace, monitor_num, "last-image", backdrop_file);
}

static void
backdrop_cycle_cb(XfceBackdrop *backdrop, gpointer user_data)
{
    XfceWorkspace *workspace = XFCE_WORKSPACE(user_data);
    const gchar *new_backdrop;

    TRACE("entering");

    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    new_backdrop = xfce_backdrop_get_image_filename(backdrop);

    /* update the xfconf property */
    if(new_backdrop != NULL)
        xfce_workspace_change_backdrop(workspace, backdrop, new_backdrop);
}

static void
backdrop_changed_cb(XfceBackdrop *backdrop, gpointer user_data)
{
    XfceWorkspace *workspace = XFCE_WORKSPACE(user_data);
    TRACE("entering");

    /* if we were spanning all the screens and we're not doing it anymore
     * we need to update all the backdrops for this workspace */
    if(workspace->priv->xinerama_stretch == TRUE &&
       xfce_workspace_get_xinerama_stretch(workspace) == FALSE) {
        guint i;

        for(i = 0; i < workspace->priv->nbackdrops; ++i) {
            /* skip the current backdrop, we'll get it last */
            if(workspace->priv->backdrops[i] != backdrop) {
                g_signal_emit(G_OBJECT(user_data),
                              signals[WORKSPACE_BACKDROP_CHANGED],
                              0,
                              workspace->priv->backdrops[i]);
            }
        }
    }

    workspace->priv->xinerama_stretch = xfce_workspace_get_xinerama_stretch(workspace);

    /* Propagate it up */
    g_signal_emit(G_OBJECT(user_data), signals[WORKSPACE_BACKDROP_CHANGED], 0, backdrop);
}

/**
 * xfce_workspace_monitors_changed:
 * @workspace: An #XfceWorkspace.
 * @GdkScreen: screen the workspace is on.
 *
 * Updates the backdrops to correctly display the right settings.
 **/
void
xfce_workspace_monitors_changed(XfceWorkspace *workspace,
                                GdkScreen *gscreen)
{
    guint i;
    guint n_monitors;
    GdkVisual *vis = NULL;

    TRACE("entering");

    g_return_if_fail(gscreen);

    vis = gdk_screen_get_rgba_visual(gscreen);
    if(vis == NULL)
        vis = gdk_screen_get_system_visual(gscreen);

    if(workspace->priv->nbackdrops > 0 &&
       xfce_workspace_get_xinerama_stretch(workspace)) {
        /* When spanning screens we only need one backdrop */
        n_monitors = 1;
    } else {
        n_monitors = gdk_display_get_n_monitors(gdk_screen_get_display(workspace->priv->gscreen));
    }

    /* Remove all backdrops so that the correct monitor is added/removed and
     * things stay in the correct order */
    xfce_workspace_remove_backdrops(workspace);

    /* Allocate space for the backdrops and their color properties so they
     * can correctly be removed */
    workspace->priv->backdrops = g_realloc(workspace->priv->backdrops,
                                           sizeof(XfceBackdrop *) * n_monitors);
    workspace->priv->first_color_id = g_realloc(workspace->priv->first_color_id,
                                                sizeof(gulong) * n_monitors);
    workspace->priv->second_color_id = g_realloc(workspace->priv->second_color_id,
                                                 sizeof(gulong) * n_monitors);

    workspace->priv->nbackdrops = n_monitors;

    for(i = 0; i < n_monitors; ++i) {
        XF_DEBUG("Adding workspace %d backdrop %d", workspace->priv->workspace_num, i);

        workspace->priv->backdrops[i] = xfce_backdrop_new(vis);
        xfce_workspace_connect_backdrop_settings(workspace,
                                               workspace->priv->backdrops[i],
                                               i);
        g_signal_connect(G_OBJECT(workspace->priv->backdrops[i]),
                         "changed",
                         G_CALLBACK(backdrop_changed_cb), workspace);
        g_signal_connect(G_OBJECT(workspace->priv->backdrops[i]),
                         "cycle",
                         G_CALLBACK(backdrop_cycle_cb),
                         workspace);
        g_signal_connect(G_OBJECT(workspace->priv->backdrops[i]),
                         "ready",
                         G_CALLBACK(backdrop_changed_cb), workspace);
    }
}

static void
xfce_workspace_class_init(XfceWorkspaceClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;

    gobject_class->finalize = xfce_workspace_finalize;
    gobject_class->set_property = xfce_workspace_set_property;
    gobject_class->get_property = xfce_workspace_get_property;

    signals[WORKSPACE_BACKDROP_CHANGED] = g_signal_new("workspace-backdrop-changed",
                                                   XFCE_TYPE_WORKSPACE,
                                                   G_SIGNAL_RUN_LAST,
                                                   G_STRUCT_OFFSET(XfceWorkspaceClass,
                                                                   changed),
                                                   NULL, NULL,
                                                   g_cclosure_marshal_VOID__POINTER,
                                                   G_TYPE_NONE, 1,
                                                   G_TYPE_POINTER);
}

static void
xfce_workspace_init(XfceWorkspace *workspace)
{
    workspace->priv = xfce_workspace_get_instance_private(workspace);
}

static void
xfce_workspace_finalize(GObject *object)
{
    XfceWorkspace *workspace = XFCE_WORKSPACE(object);

    xfce_workspace_remove_backdrops(workspace);

    g_object_unref(G_OBJECT(workspace->priv->channel));
    g_free(workspace->priv->property_prefix);
    g_free(workspace->priv->backdrops);
    g_free(workspace->priv->first_color_id);
    g_free(workspace->priv->second_color_id);
}

static void
xfce_workspace_set_property(GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
    switch(property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfce_workspace_get_property(GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
    switch(property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

/* Attempts to get the backdrop color style from the xfdesktop pre-4.11 format */
static void
xfce_workspace_migrate_backdrop_color_style(XfceWorkspace *workspace,
                                            XfceBackdrop *backdrop,
                                            guint monitor)
{
    XfconfChannel *channel = workspace->priv->channel;
    char buf[1024];
    GValue value = { 0, };

    TRACE("entering");

    /* Use the old property format */
    g_snprintf(buf, sizeof(buf), "%smonitor%d/",
               workspace->priv->property_prefix, monitor);

    /* Color style */
    g_strlcat(buf, "color-style", sizeof(buf));
    xfconf_channel_get_property(channel, buf, &value);

    if(G_VALUE_HOLDS_INT(&value)) {
        xfce_workspace_set_xfconf_property_value(workspace, monitor, "color-style", &value);
        g_value_unset(&value);
    } else {
        g_value_init(&value, G_TYPE_INT);
        g_value_set_int(&value, XFCE_BACKDROP_COLOR_SOLID);
        xfce_workspace_set_xfconf_property_value(workspace, monitor, "color-style", &value);
        g_value_unset(&value);
    }
}

/* Attempts to get the backdrop color1 from the xfdesktop pre-4.11 format */
static void
xfce_workspace_migrate_backdrop_first_color(XfceWorkspace *workspace,
                                            XfceBackdrop *backdrop,
                                            guint monitor)
{
    XfconfChannel *channel = workspace->priv->channel;
    char buf[1024];
    GValue value = { 0, };

    TRACE("entering");

    /* TODO: migrate from color1 (and older) to the new rgba1 */
    if(TRUE) {
        TRACE("warning: we aren't migrating from GdkColor to GdkRGBA yet");
        return;
    }

    /* Use the old property format */
    g_snprintf(buf, sizeof(buf), "%smonitor%d/",
               workspace->priv->property_prefix, monitor);

    /* first color */
    g_strlcat(buf, "color1", sizeof(buf));
    xfconf_channel_get_property(channel, buf, &value);

    if(G_VALUE_HOLDS_BOXED(&value)) {
        xfce_workspace_set_xfconf_property_value(workspace, monitor, "color1", &value);
        g_value_unset(&value);
    }
}

/* Attempts to get the backdrop color2 from the xfdesktop pre-4.11 format */
static void
xfce_workspace_migrate_backdrop_second_color(XfceWorkspace *workspace,
                                             XfceBackdrop *backdrop,
                                             guint monitor)
{
    XfconfChannel *channel = workspace->priv->channel;
    char buf[1024];
    GValue value = { 0, };

    TRACE("entering");

    /* TODO: migrate from color1 (and older) to the new rgba1 */
    if(TRUE) {
        TRACE("warning: we aren't migrating from GdkColor to GdkRGBA yet");
        return;
    }

    /* Use the old property format */
    g_snprintf(buf, sizeof(buf), "%smonitor%d/",
               workspace->priv->property_prefix, monitor);

    /* second color */
    g_strlcat(buf, "color2", sizeof(buf));
    xfconf_channel_get_property(channel, buf, &value);

    if(G_VALUE_HOLDS_BOXED(&value)) {
        xfce_workspace_set_xfconf_property_value(workspace, monitor, "color2", &value);
        g_value_unset(&value);
    }
}

/* Attempts to get the backdrop image from the xfdesktop pre-4.11 format */
static void
xfce_workspace_migrate_backdrop_image(XfceWorkspace *workspace,
                                      XfceBackdrop *backdrop,
                                      guint monitor)
{
    XfconfChannel *channel = workspace->priv->channel;
    char buf[1024];
    GValue value = { 0, };
    const gchar *filename;

    TRACE("entering");

    /* Use the old property format */
    g_snprintf(buf, sizeof(buf), "%smonitor%d/image-path",
               workspace->priv->property_prefix, monitor);

    /* Try to lookup the old backdrop */
    xfconf_channel_get_property(channel, buf, &value);

    XF_DEBUG("looking at %s", buf);

    /* Either there was a backdrop to migrate from or we use the backdrop
     * we provide as a default */
    if(G_VALUE_HOLDS_STRING(&value))
        filename = g_value_get_string(&value);
    else
        filename = DEFAULT_BACKDROP;

    /* update the new xfconf property */
    xfce_workspace_change_backdrop(workspace, backdrop, filename);

    if(G_VALUE_HOLDS_STRING(&value))
        g_value_unset(&value);
}

/* Attempts to get the image style from the xfdesktop pre-4.11 format */
static void
xfce_workspace_migrate_backdrop_image_style(XfceWorkspace *workspace,
                                            XfceBackdrop *backdrop,
                                            guint monitor)
{
    XfconfChannel *channel = workspace->priv->channel;
    char buf[1024];
    gint pp_len;
    GValue value = { 0, };

    TRACE("entering");

    /* Use the old property format */
    g_snprintf(buf, sizeof(buf), "%smonitor%d/",
               workspace->priv->property_prefix, monitor);
    pp_len = strlen(buf);

    /* show image */
    buf[pp_len] = 0;
    g_strlcat(buf, "image-show", sizeof(buf));
    xfconf_channel_get_property(channel, buf, &value);

    if(G_VALUE_HOLDS_BOOLEAN(&value)) {
        gboolean show_image = g_value_get_boolean(&value);

        /* if we aren't showing the image, set the style and exit the function
         * so we don't set the style to something else */
        if(!show_image) {
            g_value_unset(&value);
            g_value_init(&value, G_TYPE_INT);
            g_value_set_int(&value, XFCE_BACKDROP_IMAGE_NONE);
            xfce_workspace_set_xfconf_property_value(workspace, monitor, "image-style", &value);
            g_value_unset(&value);
            return;
        }

        g_value_unset(&value);
    }

    /* image style */
    buf[pp_len] = 0;
    g_strlcat(buf, "image-style", sizeof(buf));
    xfconf_channel_get_property(channel, buf, &value);

    if(G_VALUE_HOLDS_INT(&value)) {
        gint image_style = xfce_translate_image_styles(g_value_get_int(&value));
        g_value_set_int(&value, image_style);
        xfce_workspace_set_xfconf_property_value(workspace, monitor, "image-style", &value);
        g_value_unset(&value);
    } else {
        /* If no value was ever set default to zoomed */
        g_value_init(&value, G_TYPE_INT);
        g_value_set_int(&value, XFCE_BACKDROP_IMAGE_ZOOMED);
        xfce_workspace_set_xfconf_property_value(workspace, monitor, "image-style", &value);
        g_value_unset(&value);
    }
}

static void
xfce_workspace_connect_backdrop_settings(XfceWorkspace *workspace,
                                         XfceBackdrop *backdrop,
                                         guint monitor)
{
    XfconfChannel *channel = workspace->priv->channel;
    char buf[1024];
    gint pp_len;
    GdkDisplay *display;
    gchar *monitor_name = NULL;

    TRACE("entering");

    display = gdk_display_get_default();
    monitor_name = g_strdup(gdk_monitor_get_model(gdk_display_get_monitor(display, monitor)));

    if(monitor_name == NULL) {
        g_snprintf(buf, sizeof(buf), "%smonitor%d/workspace%d/",
                   workspace->priv->property_prefix, monitor, workspace->priv->workspace_num);
    } else {
        g_snprintf(buf, sizeof(buf), "%smonitor%s/workspace%d/",
                   workspace->priv->property_prefix, xfdesktop_remove_whitspaces(monitor_name), workspace->priv->workspace_num);
        g_free(monitor_name);
   }
    pp_len = strlen(buf);

    XF_DEBUG("prefix string: %s", buf);

    g_strlcat(buf, "color-style", sizeof(buf));
    if(!xfconf_channel_has_property(channel, buf)) {
        xfce_workspace_migrate_backdrop_color_style(workspace, backdrop, monitor);
    }
    xfconf_g_property_bind(channel, buf, XFCE_TYPE_BACKDROP_COLOR_STYLE,
                           G_OBJECT(backdrop), "color-style");

    buf[pp_len] = 0;
    g_strlcat(buf, "rgba1", sizeof(buf));
    if(!xfconf_channel_has_property(channel, buf)) {
        xfce_workspace_migrate_backdrop_first_color(workspace, backdrop, monitor);
    }
    workspace->priv->first_color_id[monitor] = xfconf_g_property_bind_gdkrgba(channel, buf,
                                                            G_OBJECT(backdrop), "first-color");

    buf[pp_len] = 0;
    g_strlcat(buf, "rgba2", sizeof(buf));
    if(!xfconf_channel_has_property(channel, buf)) {
        xfce_workspace_migrate_backdrop_second_color(workspace, backdrop, monitor);
    }
    workspace->priv->second_color_id[monitor] = xfconf_g_property_bind_gdkrgba(channel, buf,
                                                            G_OBJECT(backdrop), "second-color");

    buf[pp_len] = 0;
    g_strlcat(buf, "image-style", sizeof(buf));
    if(!xfconf_channel_has_property(channel, buf)) {
        xfce_workspace_migrate_backdrop_image_style(workspace, backdrop, monitor);
    }
    xfconf_g_property_bind(channel, buf, XFCE_TYPE_BACKDROP_IMAGE_STYLE,
                           G_OBJECT(backdrop), "image-style");

    buf[pp_len] = 0;
    g_strlcat(buf, "backdrop-cycle-enable", sizeof(buf));
    xfconf_g_property_bind(channel, buf, G_TYPE_BOOLEAN,
                           G_OBJECT(backdrop), "backdrop-cycle-enable");

    buf[pp_len] = 0;
    g_strlcat(buf, "backdrop-cycle-period", sizeof(buf));
    xfconf_g_property_bind(channel, buf, XFCE_TYPE_BACKDROP_CYCLE_PERIOD,
                           G_OBJECT(backdrop), "backdrop-cycle-period");

    buf[pp_len] = 0;
    g_strlcat(buf, "backdrop-cycle-timer", sizeof(buf));
    xfconf_g_property_bind(channel, buf, G_TYPE_UINT,
                           G_OBJECT(backdrop), "backdrop-cycle-timer");

    buf[pp_len] = 0;
    g_strlcat(buf, "backdrop-cycle-random-order", sizeof(buf));
    xfconf_g_property_bind(channel, buf, G_TYPE_BOOLEAN,
                           G_OBJECT(backdrop), "backdrop-cycle-random-order");

    buf[pp_len] = 0;
    g_strlcat(buf, "last-image", sizeof(buf));
    if(!xfconf_channel_has_property(channel, buf)) {
        xfce_workspace_migrate_backdrop_image(workspace, backdrop, monitor);
    }
    xfconf_g_property_bind(channel, buf, G_TYPE_STRING,
                           G_OBJECT(backdrop), "image-filename");

}

static void
xfce_workspace_disconnect_backdrop_settings(XfceWorkspace *workspace,
                                            XfceBackdrop *backdrop,
                                            guint monitor)
{
    TRACE("entering");

    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    xfconf_g_property_unbind_all(G_OBJECT(backdrop));
}

static void
xfce_workspace_remove_backdrops(XfceWorkspace *workspace)
{
    guint i;

    g_return_if_fail(XFCE_IS_WORKSPACE(workspace));

    for(i = 0; i < workspace->priv->nbackdrops; ++i) {
        xfce_workspace_disconnect_backdrop_settings(workspace,
                                                    workspace->priv->backdrops[i],
                                                    i);
        g_object_unref(G_OBJECT(workspace->priv->backdrops[i]));
        workspace->priv->backdrops[i] = NULL;
    }
    workspace->priv->nbackdrops = 0;
}

/* public api */

/**
 * xfce_workspace_new:
 * @gscreen: The current #GdkScreen.
 * @channel: An #XfconfChannel to use for settings.
 * @property_prefix: String prefix for per-screen properties.
 * @number: The workspace number to represent
 *
 * Creates a new #XfceWorkspace for the specified #GdkScreen.  If @gscreen is
 * %NULL, the default screen will be used.
 *
 * Return value: A new #XfceWorkspace.
 **/
XfceWorkspace *
xfce_workspace_new(GdkScreen *gscreen,
                   XfconfChannel *channel,
                   const gchar *property_prefix,
                   gint number)
{
    XfceWorkspace *workspace;

    g_return_val_if_fail(channel && property_prefix, NULL);

    workspace = g_object_new(XFCE_TYPE_WORKSPACE, NULL);

    if(!gscreen)
        gscreen = gdk_display_get_default_screen(gdk_display_get_default());

    workspace->priv->gscreen = gscreen;
    workspace->priv->workspace_num = number;
    workspace->priv->channel = XFCONF_CHANNEL(g_object_ref(G_OBJECT(channel)));
    workspace->priv->property_prefix = g_strdup(property_prefix);

    return workspace;
}

gint
xfce_workspace_get_workspace_num(XfceWorkspace *workspace)
{
    g_return_val_if_fail(XFCE_IS_WORKSPACE(workspace), -1);

    return workspace->priv->workspace_num;
}

/**
 * xfce_workspace_set_workspace_num:
 * @workspace: An #XfceWorkspace.
 * @number: workspace number
 *
 * Identifies which workspace this is. Required for XfceWorkspace to get
 * the correct xfconf settings for its backdrops.
 **/
void
xfce_workspace_set_workspace_num(XfceWorkspace *workspace, gint number)
{
    g_return_if_fail(XFCE_IS_WORKSPACE(workspace));

    workspace->priv->workspace_num = number;
}

/**
 * xfce_workspace_get_backdrop:
 * @workspace: An #XfceWorkspace.
 * @monitor: monitor number
 *
 * Returns the XfceBackdrop on the specified monitor. Returns NULL on an
 * invalid monitor number.
 **/
XfceBackdrop *xfce_workspace_get_backdrop(XfceWorkspace *workspace,
                                          guint monitor)
{
    g_return_val_if_fail(XFCE_IS_WORKSPACE(workspace), NULL);

    if(monitor >= workspace->priv->nbackdrops)
        return NULL;

    return workspace->priv->backdrops[monitor];
}
