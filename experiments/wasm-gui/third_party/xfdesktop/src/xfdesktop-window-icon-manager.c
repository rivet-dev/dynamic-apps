/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <bjt23@cornell.edu>
 *  Copyright (c) 2010 Jannis Pohlmann, <jannis@xfce.org>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib-object.h>

#include <libwnck/libwnck.h>

#include <libxfce4util/libxfce4util.h>

#include <libxfce4ui/libxfce4ui.h>

#include "xfdesktop-icon-view.h"
#include "xfdesktop-window-icon.h"
#include "xfdesktop-window-icon-manager.h"
#include "xfce-desktop.h"
#include "xfdesktop-common.h"

static void xfdesktop_window_icon_manager_set_property(GObject *object,
                                                       guint property_id,
                                                       const GValue *value,
                                                       GParamSpec *pspec);
static void xfdesktop_window_icon_manager_get_property(GObject *object,
                                                       guint property_id,
                                                       GValue *value,
                                                       GParamSpec *pspec);
static void xfdesktop_window_icon_manager_finalize(GObject *obj);
static void xfdesktop_window_icon_manager_icon_view_manager_init(XfdesktopIconViewManagerIface *iface);

static gboolean xfdesktop_window_icon_manager_real_init(XfdesktopIconViewManager *manager,
                                                        XfdesktopIconView *icon_view);
static void xfdesktop_window_icon_manager_fini(XfdesktopIconViewManager *manager);

static void xfdesktop_window_icon_manager_populate_context_menu(XfdesktopIconViewManager *manager,
                                                                GtkMenuShell *menu);
enum
{
    PROP0 = 0,
    PROP_SCREEN,
};

typedef struct
{
    GHashTable *icons;
    XfdesktopWindowIcon *selected_icon;
} XfdesktopWindowIconWorkspace;

struct _XfdesktopWindowIconManagerPrivate
{
    gboolean inited;

    GtkWidget *desktop;
    XfdesktopIconView *icon_view;

    GdkScreen *gscreen;
    WnckScreen *wnck_screen;

    gint nworkspaces;
    gint active_ws_num;
    XfdesktopWindowIconWorkspace **icon_workspaces;
};


G_DEFINE_TYPE_EXTENDED(XfdesktopWindowIconManager,
                       xfdesktop_window_icon_manager,
                       G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON_VIEW_MANAGER,
                                             xfdesktop_window_icon_manager_icon_view_manager_init)
                       G_ADD_PRIVATE(XfdesktopWindowIconManager))

static void
xfdesktop_window_icon_manager_class_init(XfdesktopWindowIconManagerClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;

    gobject_class->set_property = xfdesktop_window_icon_manager_set_property;
    gobject_class->get_property = xfdesktop_window_icon_manager_get_property;
    gobject_class->finalize = xfdesktop_window_icon_manager_finalize;

    g_object_class_install_property(gobject_class, PROP_SCREEN,
                                    g_param_spec_object("screen", "GDK Screen",
                                                        "GDK Screen this icon manager manages",
                                                        GDK_TYPE_SCREEN,
                                                        G_PARAM_READWRITE
                                                        | G_PARAM_CONSTRUCT_ONLY));
}

static void
xfdesktop_window_icon_manager_init(XfdesktopWindowIconManager *wmanager)
{
    wmanager->priv = xfdesktop_window_icon_manager_get_instance_private(wmanager);
}

static void
xfdesktop_window_icon_manager_set_property(GObject *object,
                                           guint property_id,
                                           const GValue *value,
                                           GParamSpec *pspec)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(object);

    switch(property_id) {
        case PROP_SCREEN:
            wmanager->priv->gscreen = g_value_peek_pointer(value);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            wmanager->priv->wnck_screen = wnck_screen_get(gdk_screen_get_number(wmanager->priv->gscreen));
G_GNUC_END_IGNORE_DEPRECATIONS
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
xfdesktop_window_icon_manager_get_property(GObject *object,
                                           guint property_id,
                                           GValue *value,
                                           GParamSpec *pspec)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(object);

    switch(property_id) {
        case PROP_SCREEN:
            g_value_set_object(value, wmanager->priv->gscreen);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
xfdesktop_window_icon_manager_finalize(GObject *obj)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(obj);

    TRACE("entering");

    if(wmanager->priv->inited)
        xfdesktop_window_icon_manager_fini(XFDESKTOP_ICON_VIEW_MANAGER(wmanager));

    G_OBJECT_CLASS(xfdesktop_window_icon_manager_parent_class)->finalize(obj);
}

static void
xfdesktop_window_icon_manager_icon_view_manager_init(XfdesktopIconViewManagerIface *iface)
{
    iface->manager_init = xfdesktop_window_icon_manager_real_init;
    iface->manager_fini = xfdesktop_window_icon_manager_fini;
    iface->populate_context_menu = xfdesktop_window_icon_manager_populate_context_menu;
}


static void
xfdesktop_window_icon_manager_icon_selection_changed_cb(XfdesktopIconView *icon_view,
                                                        gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;
    GList *selected;

    selected = xfdesktop_icon_view_get_selected_items(icon_view);
    if(selected) {
        XfdesktopWindowIcon *window_icon = XFDESKTOP_WINDOW_ICON(selected->data);
        gint ws = xfdesktop_window_icon_get_workspace(window_icon);
        wmanager->priv->icon_workspaces[ws]->selected_icon = window_icon;
        g_list_free(selected);
    } else
        wmanager->priv->icon_workspaces[wmanager->priv->active_ws_num]->selected_icon = NULL;
}

static XfdesktopWindowIcon *
xfdesktop_window_icon_manager_add_icon(XfdesktopWindowIconManager *wmanager,
                                       WnckWindow *window,
                                       gint ws_num)
{
    XfdesktopWindowIcon *icon = xfdesktop_window_icon_new(window, ws_num);

    g_hash_table_insert(wmanager->priv->icon_workspaces[ws_num]->icons,
                        window, icon);

    if(ws_num == wmanager->priv->active_ws_num)
        xfdesktop_icon_view_add_item(wmanager->priv->icon_view,
                                     XFDESKTOP_ICON(icon));

    return icon;
}

static void
xfdesktop_add_window_icons_foreach(gpointer key,
                                   gpointer value,
                                   gpointer user_data)
{
    XfdesktopIcon *icon = value;
    XfdesktopWindowIconManager *wmanager = user_data;
    xfdesktop_icon_view_add_item(wmanager->priv->icon_view, icon);
}

static void
workspace_changed_cb(WnckScreen *wnck_screen,
                     WnckWorkspace *previously_active_space,
                     gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(user_data);
    gint n;
    WnckWorkspace *ws;

    ws = wnck_screen_get_active_workspace(wmanager->priv->wnck_screen);
    if(!WNCK_IS_WORKSPACE(ws)) {
        XF_DEBUG("got weird failure of wnck_screen_get_active_workspace(), bailing");
        return;
    }

    xfdesktop_icon_view_remove_all(wmanager->priv->icon_view);

    wmanager->priv->active_ws_num = n = wnck_workspace_get_number(ws);

    if(!wmanager->priv->icon_workspaces[n]->icons) {
        GList *windows, *l;

        wmanager->priv->icon_workspaces[n]->icons =
            g_hash_table_new_full(g_direct_hash,
                                  g_direct_equal,
                                  NULL,
                                  (GDestroyNotify)g_object_unref);

        windows = wnck_screen_get_windows(wmanager->priv->wnck_screen);
        for(l = windows; l; l = l->next) {
            WnckWindow *window = l->data;

            if((ws == wnck_window_get_workspace(window)
                || wnck_window_is_pinned(window))
               && wnck_window_is_minimized(window)
               && !wnck_window_is_skip_tasklist(window))
            {
                xfdesktop_window_icon_manager_add_icon(wmanager,
                                                       window, n);
            }
        }
    } else
        g_hash_table_foreach(wmanager->priv->icon_workspaces[n]->icons,
                             xfdesktop_add_window_icons_foreach, wmanager);

    if(wmanager->priv->icon_workspaces[n]->selected_icon) {
        xfdesktop_icon_view_select_item(wmanager->priv->icon_view,
                                        XFDESKTOP_ICON(wmanager->priv->icon_workspaces[n]->selected_icon));
    }
}

static void
workspace_created_cb(WnckScreen *wnck_screen,
                     WnckWorkspace *workspace,
                     gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;
    gint ws_num, n_ws;

    n_ws = wnck_screen_get_workspace_count(wnck_screen);
    wmanager->priv->nworkspaces = n_ws;
    ws_num = wnck_workspace_get_number(workspace);

    wmanager->priv->icon_workspaces = g_realloc(wmanager->priv->icon_workspaces,
                                                sizeof(gpointer) * n_ws);

    if(ws_num != n_ws - 1) {
        memmove(wmanager->priv->icon_workspaces + ws_num + 1,
                wmanager->priv->icon_workspaces + ws_num,
                sizeof(gpointer) * (n_ws - ws_num - 1));
    }

    wmanager->priv->icon_workspaces[ws_num] = g_new0(XfdesktopWindowIconWorkspace,
                                                     1);
}

static void
workspace_destroyed_cb(WnckScreen *wnck_screen,
                       WnckWorkspace *workspace,
                       gpointer user_data)
{
    /* TODO: check if we get workspace-destroyed before or after all the
     * windows on that workspace were moved and we got workspace-changed
     * for each one.  preferably that is the case. */
    XfdesktopWindowIconManager *wmanager = user_data;
    gint ws_num, n_ws;

    n_ws = wnck_screen_get_workspace_count(wnck_screen);
    wmanager->priv->nworkspaces = n_ws;
    ws_num = wnck_workspace_get_number(workspace);

    if(wmanager->priv->icon_workspaces[ws_num]->icons)
        g_hash_table_destroy(wmanager->priv->icon_workspaces[ws_num]->icons);
    g_free(wmanager->priv->icon_workspaces[ws_num]);

    if(ws_num != n_ws) {
        memmove(wmanager->priv->icon_workspaces + ws_num,
                wmanager->priv->icon_workspaces + ws_num + 1,
                sizeof(gpointer) * (n_ws - ws_num));
    }

    wmanager->priv->icon_workspaces = g_realloc(wmanager->priv->icon_workspaces,
                                                sizeof(gpointer) * n_ws);
}

static void
window_state_changed_cb(WnckWindow *window,
                        WnckWindowState changed_mask,
                        WnckWindowState new_state,
                        gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;
    WnckWorkspace *ws;
    gint ws_num = -1, i, max_i;
    gboolean is_add = FALSE;
    XfdesktopWindowIcon *icon;

    TRACE("entering");

    if(!(changed_mask & (WNCK_WINDOW_STATE_MINIMIZED |
                         WNCK_WINDOW_STATE_SKIP_TASKLIST)))
    {
        return;
    }

    XF_DEBUG("changed_mask indicates an action");

    ws = wnck_window_get_workspace(window);
    if(ws)
        ws_num = wnck_workspace_get_number(ws);

    if(   (changed_mask & WNCK_WINDOW_STATE_MINIMIZED
           && new_state & WNCK_WINDOW_STATE_MINIMIZED)
       || (changed_mask & WNCK_WINDOW_STATE_SKIP_TASKLIST
           && !(new_state & WNCK_WINDOW_STATE_SKIP_TASKLIST)))
    {
        is_add = TRUE;
    }

    XF_DEBUG("is_add == %s", is_add?"TRUE":"FALSE");

    /* this is a cute way of handling adding/removing from *all* workspaces
     * when we're dealing with a sticky windows, and just adding/removing
     * from a single workspace otherwise, without duplicating code */
    if(wnck_window_is_pinned(window)) {
        i = 0;
        max_i = wmanager->priv->nworkspaces;
    } else {
        if(ws_num == -1)
            return;
        i = ws_num;
        max_i = i + 1;
    }

    if(is_add) {
        for(; i < max_i; i++) {
            TRACE("loop: %d", i);
            if(!wmanager->priv->icon_workspaces[i]->icons
               || g_hash_table_lookup(wmanager->priv->icon_workspaces[i]->icons,
                                      window))
            {
                continue;
            }

            XF_DEBUG("adding to WS %d", i);
            xfdesktop_window_icon_manager_add_icon(wmanager, window, i);
        }
    } else {
        for(; i < max_i; i++) {
            TRACE("loop: %d", i);
            if(!wmanager->priv->icon_workspaces[i]->icons)
                continue;

            icon = g_hash_table_lookup(wmanager->priv->icon_workspaces[i]->icons,
                                       window);
            if(icon) {
                if(wmanager->priv->icon_workspaces[i]->selected_icon == icon)
                    wmanager->priv->icon_workspaces[i]->selected_icon = NULL;
                if(i == wmanager->priv->active_ws_num) {
                    XF_DEBUG("removing from WS %d", i);
                    xfdesktop_icon_view_remove_item(wmanager->priv->icon_view,
                                                    XFDESKTOP_ICON(icon));
                    g_hash_table_remove(wmanager->priv->icon_workspaces[i]->icons,
                                        window);
                }
            }
        }
    }
}

static void
window_workspace_changed_cb(WnckWindow *window,
                            gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;
    WnckWorkspace *new_ws;
    gint i, new_ws_num = -1, n_ws;
    XfdesktopIcon *icon;

    TRACE("entering");

    if(!wnck_window_is_minimized(window))
        return;

    n_ws = wmanager->priv->nworkspaces;

    new_ws = wnck_window_get_workspace(window);
    if(new_ws)
        new_ws_num = wnck_workspace_get_number(new_ws);

    for(i = 0; i < n_ws; i++) {
        if(!wmanager->priv->icon_workspaces[i]->icons)
            continue;

        icon = g_hash_table_lookup(wmanager->priv->icon_workspaces[i]->icons,
                                   window);

        if(new_ws) {
            /* window is not sticky */
            if(i != new_ws_num && icon) {
                if(i == wmanager->priv->active_ws_num) {
                    if(wmanager->priv->icon_workspaces[i]->selected_icon == XFDESKTOP_WINDOW_ICON(icon)) {
                        wmanager->priv->icon_workspaces[i]->selected_icon = NULL;
                    }
                    xfdesktop_icon_view_remove_item(wmanager->priv->icon_view,
                                                    icon);
                }
                g_hash_table_remove(wmanager->priv->icon_workspaces[i]->icons,
                                    window);
            } else if(i == new_ws_num && !icon)
                xfdesktop_window_icon_manager_add_icon(wmanager, window, i);
        } else {
            /* window is sticky */
            if(!icon)
                xfdesktop_window_icon_manager_add_icon(wmanager, window, i);
        }
    }
}

static void
window_destroyed_cb(gpointer data,
                    GObject *where_the_object_was)
{
    XfdesktopWindowIconManager *wmanager = data;
    WnckWindow *window = (WnckWindow *)where_the_object_was;
    gint i;
    XfdesktopIcon *icon;

    for(i = 0; i < wmanager->priv->nworkspaces; i++) {
        if(!wmanager->priv->icon_workspaces[i]->icons)
            continue;

        icon = g_hash_table_lookup(wmanager->priv->icon_workspaces[i]->icons,
                                   window);
        if(icon) {
            if(wmanager->priv->icon_workspaces[i]->selected_icon)
                wmanager->priv->icon_workspaces[i]->selected_icon = NULL;
            if(i == wmanager->priv->active_ws_num) {
                xfdesktop_icon_view_remove_item(wmanager->priv->icon_view,
                                                icon);
            }
            g_hash_table_remove(wmanager->priv->icon_workspaces[i]->icons,
                                window);
        }
    }
}

static void
window_created_cb(WnckScreen *wnck_screen,
                  WnckWindow *window,
                  gpointer user_data)
{
    XfdesktopWindowIconManager *wmanager = user_data;

    g_signal_connect(G_OBJECT(window), "state-changed",
                     G_CALLBACK(window_state_changed_cb), wmanager);
    g_signal_connect(G_OBJECT(window), "workspace-changed",
                     G_CALLBACK(window_workspace_changed_cb), wmanager);
    g_object_weak_ref(G_OBJECT(window), window_destroyed_cb, wmanager);
}

static void
xfdesktop_window_icon_manager_populate_context_menu(XfdesktopIconViewManager *manager,
                                                    GtkMenuShell *menu)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    XfdesktopWindowIconWorkspace *wiws = wmanager->priv->icon_workspaces[wmanager->priv->active_ws_num];

    if (wiws->selected_icon != NULL) {
        xfdesktop_icon_populate_context_menu(XFDESKTOP_ICON(wiws->selected_icon),
                                             GTK_WIDGET(menu));
    }
}


/* public api */

XfdesktopIconViewManager *
xfdesktop_window_icon_manager_new(GdkScreen *gscreen)
{
    g_return_val_if_fail(GDK_IS_SCREEN(gscreen), NULL);
    return g_object_new(XFDESKTOP_TYPE_WINDOW_ICON_MANAGER,
                        "screen", gscreen,
                        NULL);
}


/* XfdesktopIconViewManager virtual functions */

static gboolean
xfdesktop_window_icon_manager_real_init(XfdesktopIconViewManager *manager,
                                        XfdesktopIconView *icon_view)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    GList *windows, *l;
    gint i;

    wmanager->priv->icon_view = icon_view;
    xfdesktop_icon_view_set_selection_mode(icon_view, GTK_SELECTION_SINGLE);
    g_signal_connect(G_OBJECT(icon_view), "icon-selection-changed",
                     G_CALLBACK(xfdesktop_window_icon_manager_icon_selection_changed_cb),
                     wmanager);

    wmanager->priv->desktop = gtk_widget_get_toplevel(GTK_WIDGET(icon_view));

    wnck_screen_force_update(wmanager->priv->wnck_screen);
    g_signal_connect(G_OBJECT(wmanager->priv->wnck_screen),
                     "active-workspace-changed",
                     G_CALLBACK(workspace_changed_cb), wmanager);
    g_signal_connect(G_OBJECT(wmanager->priv->wnck_screen), "window-opened",
                     G_CALLBACK(window_created_cb), wmanager);
    g_signal_connect(G_OBJECT(wmanager->priv->wnck_screen), "workspace-created",
                     G_CALLBACK(workspace_created_cb), wmanager);
    g_signal_connect(G_OBJECT(wmanager->priv->wnck_screen),
                     "workspace-destroyed",
                     G_CALLBACK(workspace_destroyed_cb), wmanager);

    wmanager->priv->nworkspaces = wnck_screen_get_workspace_count(wmanager->priv->wnck_screen);
    wmanager->priv->active_ws_num = wnck_workspace_get_number(wnck_screen_get_active_workspace(wmanager->priv->wnck_screen));
    wmanager->priv->icon_workspaces = g_malloc0(wmanager->priv->nworkspaces
                                                * sizeof(gpointer));
    for(i = 0; i < wmanager->priv->nworkspaces; ++i) {
        wmanager->priv->icon_workspaces[i] = g_new0(XfdesktopWindowIconWorkspace,
                                                    1);
    }

    windows = wnck_screen_get_windows(wmanager->priv->wnck_screen);
    for(l = windows; l; l = l->next) {
        WnckWindow *window = l->data;

        g_signal_connect(G_OBJECT(window), "state-changed",
                         G_CALLBACK(window_state_changed_cb), wmanager);
        g_signal_connect(G_OBJECT(window), "workspace-changed",
                         G_CALLBACK(window_workspace_changed_cb), wmanager);
        g_object_weak_ref(G_OBJECT(window), window_destroyed_cb, wmanager);
    }

    workspace_changed_cb(wmanager->priv->wnck_screen, NULL, wmanager);

    wmanager->priv->inited = TRUE;

    return TRUE;
}

static void
xfdesktop_window_icon_manager_fini(XfdesktopIconViewManager *manager)
{
    XfdesktopWindowIconManager *wmanager = XFDESKTOP_WINDOW_ICON_MANAGER(manager);
    gint i;
    GList *windows, *l;

    TRACE("entering");

    wmanager->priv->inited = FALSE;

    g_signal_handlers_disconnect_by_func(G_OBJECT(wmanager->priv->wnck_screen),
                                         G_CALLBACK(workspace_changed_cb),
                                         wmanager);
    g_signal_handlers_disconnect_by_func(G_OBJECT(wmanager->priv->wnck_screen),
                                         G_CALLBACK(window_created_cb),
                                         wmanager);
    g_signal_handlers_disconnect_by_func(G_OBJECT(wmanager->priv->wnck_screen),
                                         G_CALLBACK(workspace_created_cb),
                                         wmanager);
    g_signal_handlers_disconnect_by_func(G_OBJECT(wmanager->priv->wnck_screen),
                                         G_CALLBACK(workspace_destroyed_cb),
                                         wmanager);

    windows = wnck_screen_get_windows(wmanager->priv->wnck_screen);
    for(l = windows; l; l = l->next) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(l->data),
                                             G_CALLBACK(window_state_changed_cb),
                                             wmanager);
        g_signal_handlers_disconnect_by_func(G_OBJECT(l->data),
                                             G_CALLBACK(window_workspace_changed_cb),
                                             wmanager);
        g_object_weak_unref(G_OBJECT(l->data), window_destroyed_cb, wmanager);
    }

    xfdesktop_icon_view_remove_all(wmanager->priv->icon_view);
    g_signal_handlers_disconnect_by_func(G_OBJECT(wmanager->priv->icon_view),
                                         G_CALLBACK(xfdesktop_window_icon_manager_icon_selection_changed_cb),
                                         wmanager);

    for(i = 0; i < wmanager->priv->nworkspaces; ++i) {
        if(wmanager->priv->icon_workspaces[i]->icons)
            g_hash_table_destroy(wmanager->priv->icon_workspaces[i]->icons);
        g_free(wmanager->priv->icon_workspaces[i]);
    }
    g_free(wmanager->priv->icon_workspaces);
    wmanager->priv->icon_workspaces = NULL;
}

