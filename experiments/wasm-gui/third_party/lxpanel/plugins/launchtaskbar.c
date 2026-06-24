/*
 * Copyright (C) 2006-2009 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2006-2008 Jim Huang <jserv.tw@gmail.com>
 *               2008 Fred Chien <fred@lxde.org>
 *               2009 Andrew Lee <ajqlee@debian.org>
 *               2009 Jürgen Hötzel <juergen@archlinux.org>
 *               2009 Ying-Chun Liu (PaulLiu) <grandpaul@gmail.com>
 *               2009-2010 Marty Jack <martyj19@comcast.net>
 *               2010 Julien Lavergne <julien.lavergne@gmail.com>
 *               2011-2014 Henry Gebhardt <hsggebhardt@gmail.com>
 *               2012 Piotr Sipika <Piotr.Sipika@gmail.com>
 *               2012-2014 Giuseppe Penone <giuspen@gmail.com>
 *               2013 Vincenzo di Cicco <enzodicicco@gmail.com>
 *               2013 Rouslan <rouslan-k@users.sourceforge.net>
 *               2014-2016 Andriy Grytsenko <andrej@rep.kiev.ua>
 *               2014 Vladimír Pýcha <vpycha@gmail.com>
 *               2014 Raimar Bühmann <raimar@buehmann.de>
 *               2014 Andy Balaam <axis3x3@users.sf.net>
 *               2015 Balló György <ballogyor@gmail.com>
 *               2015 Rafał Mużyło <galtgendo@gmail.com>
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

/*
 * Started by Giuseppe Penone <giuspen@gmail.com> merging launchbar and taskbar
 * and adding interoperability between them.
*/

/*
 * Taskbar plugin:
 * 2006.09.10 modified by Hong Jen Yee (PCMan) pcman.tw (AT) gmail.com
 * Following features are added:
 * 1. Add XUrgencyHint support. (Flashing task bar buttons, can be disabled)
 * 2. Raise window when files get dragged over taskbar buttons.
 * 3. Add Restore & Maximize menu items to popup menu of task bar buttons.
 */

//#define DEBUG // killall lxpanel && lxpanel --profile Lubuntu &

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>

#include <libfm/fm-gtk.h>

#include "misc.h"
#include "ev.h"
#include "plugin.h"
#include "task-button.h"
#include "launch-button.h"
#include "icon-grid.h"
#ifndef DISABLE_MENU
# include "menu-policy.h"
#endif
#include "gtk-compat.h"

#define PANEL_ICON_SIZE 24 /* see the private.h */

/* Column definitions for configuration dialogs. */
enum {
    COL_ICON,
    COL_TITLE,
    COL_ICON_NAME,
    COL_BTN,
    N_COLS
};

typedef enum {
    LAUNCHBAR = 0,  /* GtkComboBox is 0-indexed. */
    TASKBAR,
    LAUNCHTASKBAR
} LtbMode;

typedef struct LaunchTaskBarPlugin LaunchTaskBarPlugin;

/* Private context for taskbar plugin. */
struct LaunchTaskBarPlugin {
    /* LAUNCHBAR */
    GtkWidget *lb_icon_grid;         /* Icon grid managing the container */
    GtkWidget     *p_button_add, *p_button_remove, *p_label_menu_app_exec, *p_label_def_app_exec;
    FmDndDest *dd;                 /* Drag & drop on launchbar */
    GtkWidget *last_lb_drag_dest;  /* Last launch button near drag on launchbar */
    PanelIconGridDropPosition last_lb_drag_pos;
    gboolean in_drag;              /* TRUE if some launcher is being dragged */
    FmPath *dragged_launcher;      /* Path of launcher currently being dragged */
    int drag_start_x, drag_start_y; /* Drag start coordinates */
    /* TASKBAR */
    GtkWidget * tb_icon_grid;      /* Manager for taskbar buttons */
    int number_of_desktops;        /* Number of desktops, from NET_WM_NUMBER_OF_DESKTOPS */
    int current_desktop;           /* Current desktop, from NET_WM_CURRENT_DESKTOP */
    guint dnd_delay_timer;         /* Timer for drag and drop delay */
    gboolean dnd_task_moving;      /* User is currently moving a task button */
    GtkWidget *dnd_delay_task;     /* Task button to raise on drag timeout */
    int icon_size;                 /* Size of task icons */
    gboolean grouped_tasks;        /* User preference: windows from same task are grouped onto a single button */
    TaskShowFlags flags;        /* User preferences flags */
    int task_width_max;            /* Maximum width of a taskbar button in horizontal orientation */
    int spacing;                   /* Spacing between taskbar buttons */
    guint flash_timeout;        /* Timer for urgency notification */
    gboolean flash_state;       /* One-bit counter to flash taskbar */
    /* COMMON */
#ifndef DISABLE_MENU
    FmPath * path;              /* Current menu item path */
    GtkWidget       *p_menuitem_lock_tbp;
    GtkWidget       *p_menuitem_unlock_tbp;
    GtkWidget       *p_menuitem_new_instance;
    GtkWidget       *p_menuitem_separator;
#endif
    GtkWidget * plugin;                 /* Back pointer to Plugin */
    LXPanel * panel;                    /* Back pointer to panel */
    config_setting_t * settings;
    GdkScreen       *screen;
    GtkWidget       *config_dlg;        /* Configuration dialog */
    GtkNotebook     *p_notebook;
    GtkWidget       *p_notebook_page_launch;
    GtkWidget       *p_notebook_page_task;
    GKeyFile        *p_key_file_special_cases;
    int              mode;
    gboolean         lb_built;
    gboolean         tb_built;
    gboolean         fixed_mode;        /* if mode cannot be changed */
    int              w, h;              /* override GtkBox bug with allocation */
};

static gchar *launchtaskbar_rc = "style 'launchtaskbar-style' = 'theme-panel'\n"
        "{\n"
        "GtkWidget::focus-padding=0\n"
        "GtkButton::default-border={0,0,0,0}\n"
        "GtkButton::default-outside-border={0,0,0,0}\n"
        "GtkButton::inner-border={0,0,0,0}\n"
        "}\n"
        "widget '*launchbar.*' style 'launchtaskbar-style'\n"
        "widget '*taskbar.*' style 'launchtaskbar-style'";

#define DRAG_ACTIVE_DELAY    1000
#define TASK_WIDTH_MAX       200
#define ALL_WORKSPACES       -1
#define ICON_ONLY_EXTRA      6      /* Amount needed to have button lay out symmetrically */
#define ICON_BUTTON_TRIM 4      /* Amount needed to have button remain on panel */

static void launchtaskbar_destructor(gpointer user_data);

static LaunchButton *launchbar_exec_bin_exists(LaunchTaskBarPlugin *lb, FmPath *path);
static void launchbar_remove_button(LaunchTaskBarPlugin *ltbp, LaunchButton *btn);

static void taskbar_redraw(LaunchTaskBarPlugin * tb);
static void taskbar_net_client_list(GtkWidget * widget, LaunchTaskBarPlugin * tb);
static void taskbar_net_current_desktop(GtkWidget * widget, LaunchTaskBarPlugin * tb);
static void taskbar_net_number_of_desktops(GtkWidget * widget, LaunchTaskBarPlugin * tb);
static void taskbar_net_active_window(GtkWidget * widget, LaunchTaskBarPlugin * tb);
static GdkFilterReturn taskbar_event_filter(XEvent * xev, GdkEvent * event, LaunchTaskBarPlugin * tb);
static void taskbar_window_manager_changed(GdkScreen * screen, LaunchTaskBarPlugin * tb);
static void taskbar_apply_configuration(LaunchTaskBarPlugin * ltbp);
static void taskbar_add_task_button(LaunchTaskBarPlugin * tb, TaskButton * task);

#define taskbar_reset_menu(tb) if (tb->tb_built) task_button_reset_menu(tb->tb_icon_grid)


#ifndef DISABLE_MENU
static char *task_get_cmdline(Window win, LaunchTaskBarPlugin *ltbp)
{
    GPid pid = get_net_wm_pid(win);
    char proc_path[64];
    gchar *cmdline = NULL;
    gchar *p_char = NULL;

    snprintf(proc_path, sizeof(proc_path),
             G_DIR_SEPARATOR_S "proc" G_DIR_SEPARATOR_S "%lu" G_DIR_SEPARATOR_S "cmdline",
             (gulong)pid);
    g_file_get_contents(proc_path, &cmdline, NULL, NULL);
    if (cmdline)
    {
        p_char = strchr(cmdline, '\n');
        if(p_char != NULL) *p_char = '\0';
        p_char = strrchr(cmdline, G_DIR_SEPARATOR);
        if (p_char != NULL) p_char++;
        else p_char = cmdline;
        if(strcmp(p_char, "python") == 0)
        {
            snprintf(proc_path, sizeof(proc_path),
                     G_DIR_SEPARATOR_S "proc" G_DIR_SEPARATOR_S "%lu" G_DIR_SEPARATOR_S "comm",
                     (gulong)pid);
            g_free(cmdline);
            cmdline = NULL;
            g_file_get_contents(proc_path, &cmdline, NULL, NULL);
            if (cmdline)
            {
                p_char = strchr(cmdline, '\n');
                if(p_char != NULL) *p_char = '\0';
            }
        }
        else
        {
            p_char = g_key_file_get_string(ltbp->p_key_file_special_cases,
                                           "special_cases", p_char, NULL);
            if (p_char != NULL) /* found this key */
            {
                g_free(cmdline);
                cmdline = p_char;
            }
        }
    }
    return cmdline;
}

static FmPath *f_find_menu_launchbutton_recursive(Window win, LaunchTaskBarPlugin *ltbp)
{
    MenuCache *mc;
    guint32 flags;
    GSList *apps, *l;
    size_t len;
    char *exec_bin = task_get_cmdline(win, ltbp);
    const char *exec, *short_exec;
    char *str_path;
    FmPath *path = NULL;

    /* FIXME: cache it in Task object */
    mc = panel_menu_cache_new(&flags);
    /* FIXME: if menu plugin wasn't loaded yet we'll get NULL list here */
    apps = menu_cache_list_all_apps(mc);
    short_exec = strrchr(exec_bin, '/');
    if (short_exec != NULL)
        short_exec++;
    else
        short_exec = exec_bin;
    len = strlen(short_exec);
    /* the same executable may be used in numerous applications so wild guess
       estimation check for desktop id equal to short_exec+".desktop" first */
    for (l = apps; l; l = l->next)
    {
        exec = menu_cache_item_get_id(MENU_CACHE_ITEM(l->data));
        /* we don't check flags here because user always can manually
           start any app that isn't visible in the desktop menu */
        if (strncmp(exec, short_exec, len) == 0 && exec[len] == '.')
            break;
    }
    /* if not found then check for non-absolute exec name in application
       since it usually is expanded by application starting functions */
    if (l == NULL) for (l = apps; l; l = l->next)
    {
        exec = menu_cache_app_get_exec(MENU_CACHE_APP(l->data));
        if (exec && exec[0] != '/' && strncmp(exec, short_exec, len) == 0 &&
            (exec[len] == ' ' || exec[len] == 0))
            break;
    }
    /* well, not matched, let try full path, we assume here if application
       starts executable by full path then process cannot have short name */
    if (l == NULL && exec_bin[0] == '/')
    {
        len = strlen(exec_bin);
        for (l = apps; l; l = l->next)
        {
            exec = menu_cache_app_get_exec(MENU_CACHE_APP(l->data));
            if (exec && exec[0] == '/' && strncmp(exec, exec_bin, len) == 0 &&
                (exec[len] == ' ' || exec[len] == 0))
                break;
        }
    }
    if (l)
    {
        str_path = menu_cache_dir_make_path(MENU_CACHE_DIR(l->data));
        path = fm_path_new_relative(fm_path_get_apps_menu(), str_path+13); /* skip /Applications */
        g_free(str_path);
    }
    g_slist_foreach(apps, (GFunc)menu_cache_item_unref, NULL);
    g_slist_free(apps);
    menu_cache_unref(mc);
    g_debug("f_find_menu_launchbutton_recursive: search '%s' found=%d", exec_bin, (path != NULL));
    g_free(exec_bin);
    return path;
}
#endif

static void launchbar_remove_bootstrap(LaunchTaskBarPlugin *ltbp)
{
    GList *btns = gtk_container_get_children(GTK_CONTAINER(ltbp->lb_icon_grid));
    GList *l;

    for (l = btns; l; l = l->next)
        if (launch_button_get_settings(l->data) == NULL)
        {
            gtk_widget_destroy(l->data);
            g_debug("launchtaskbar: removed bootstrap button %p", l->data);
        }
    g_list_free(btns);
}

static void launchbar_check_bootstrap(LaunchTaskBarPlugin *lb)
{
    LaunchButton *btn;

    if (panel_icon_grid_get_n_children(PANEL_ICON_GRID(lb->lb_icon_grid)) > 0)
        return;
    btn = launch_button_new(lb->panel, lb->plugin, NULL, NULL);
    gtk_container_add(GTK_CONTAINER(lb->lb_icon_grid), GTK_WIDGET(btn));
    g_debug("launchtaskbar: added bootstrap button %p", btn);
}

static void launchbar_remove_launcher(LaunchTaskBarPlugin *ltbp, FmPath *path)
{
    LaunchButton *btn = launchbar_exec_bin_exists(ltbp, path);

    if (btn != NULL)
        launchbar_remove_button(ltbp, btn);
}


/* Process drag & drop launchers between panels and places */
/* targets are added to FmDndDest, also only targets for GtkTreeDragSource */
enum {
    LAUNCHBUTTON_DND_TARGET = N_FM_DND_DEST_DEFAULT_TARGETS
};

/* Target types for dragging items of list */
static const GtkTargetEntry dnd_targets[] = {
    { "application/x-lxpanel-launcher", GTK_TARGET_SAME_APP, LAUNCHBUTTON_DND_TARGET }
};

static GdkAtom launch_button_dnd_atom;
static GtkTargetList *drag_src_target_list = NULL;


/* source side -- form and send path */
static gboolean on_launchbar_drag_source(GtkWidget *widget, GdkEvent *event,
                                         LaunchTaskBarPlugin *lb)
{
    PanelIconGrid *ig;
    GtkWidget *btn;
    PanelIconGridDropPosition pos;
    FmFileInfo *fi;
    GdkWindow *win;
    gdouble x, y;
#if !GTK_CHECK_VERSION(2, 22, 0)
    gint px, py;
#endif

    switch (event->type)
    {
    case GDK_BUTTON_PRESS:
        if (event->button.button != 1)
            break;
        if (lb->in_drag)
            break;
        if (lb->dragged_launcher)
            fm_path_unref(lb->dragged_launcher);
        lb->dragged_launcher = NULL;
        /* check if it is a launcher where the drag begins */
        ig = PANEL_ICON_GRID(widget);
        win = event->button.window;
        x = event->button.x;
        y = event->button.y;
        while (win != NULL && win != gtk_widget_get_window(widget))
        {
#if GTK_CHECK_VERSION(2, 22, 0)
            gdk_window_coords_to_parent(win, x, y, &x, &y);
            win = gdk_window_get_effective_parent(win);
#else
            gdk_window_get_position(win, &px, &py);
            x += px;
            y += py;
            win = gdk_window_get_parent(win);
#endif
        }
        if (win == NULL)
            /* this should never happen */
            break;
        if (!panel_icon_grid_get_dest_at_pos(ig, (int)x, (int)y, &btn, &pos) ||
            !PANEL_IS_LAUNCH_BUTTON(btn))
            break;
        /* remember the current coordinates and the launcher */
        fi = launch_button_get_file_info((LaunchButton *)btn);
        if (fi == NULL)
            break;
        lb->dragged_launcher = fm_path_ref(fm_file_info_get_path(fi));
        lb->drag_start_x = event->button.x;
        lb->drag_start_y = event->button.y;
        if (pos != PANEL_ICON_GRID_DROP_INTO)
            break;
        /* g_debug("started dragging button #%d at %d,%d", panel_icon_grid_get_child_position(ig, btn), (int)x, (int)y); */
        lb->in_drag = TRUE;
        break;
    case GDK_BUTTON_RELEASE:
        if (event->button.button != 1)
            break;
        /* forget the drag state */
        lb->in_drag = FALSE;
        /* if user clicked somewhere on border, he/she expects nearest
           application to be launched, see LP bug #824071. If click was
           delivered here, it already missed the button, so handle it */
        if (lb->dragged_launcher &&
            /* if it was clicked outside button and didn't moved */
            !gtk_drag_check_threshold(widget, lb->drag_start_x, lb->drag_start_y,
                                      event->button.x, event->button.y))
        {
            /* g_debug("outside click detected"); */
            lxpanel_launch_path(lb->panel, lb->dragged_launcher);
        }
        break;
    case GDK_MOTION_NOTIFY:
        if ((event->motion.state & GDK_BUTTON1_MASK) == 0)
            break;
        /* if in drag state then check for threshold */
        if (lb->in_drag &&
            gtk_drag_check_threshold(widget, lb->drag_start_x, lb->drag_start_y,
                                     event->motion.x, event->motion.y))
        {
            lb->in_drag = FALSE;
            if (G_UNLIKELY(drag_src_target_list == NULL))
                drag_src_target_list = gtk_target_list_new(dnd_targets,
                                                           G_N_ELEMENTS(dnd_targets));
#if GTK_CHECK_VERSION(3, 10, 0)
            gtk_drag_begin_with_coordinates(widget, drag_src_target_list,
                                            GDK_ACTION_MOVE, 1, event,
                                            event->motion.x, event->motion.y);
#else
            gtk_drag_begin(widget, drag_src_target_list, GDK_ACTION_MOVE,
                           1, event);
#endif
            return TRUE;
        }
        break;
    default: ;
    }
    return FALSE;
}

static void on_launchbar_drag_begin(GtkWidget *widget, GdkDragContext *context,
                                    LaunchTaskBarPlugin *lb)
{
    /* setup the drag icon from the launcher */
    LaunchButton *btn = launchbar_exec_bin_exists(lb, lb->dragged_launcher);

    if (btn)
    {
        FmIcon *icon = launch_button_get_icon(btn);

        if (icon)
#if GTK_CHECK_VERSION(3, 2, 0)
            gtk_drag_set_icon_gicon(context, fm_icon_get_gicon(icon), 0, 0);
#else
        {
            gint w;
            GdkPixbuf *pix;
            gtk_icon_size_lookup(GTK_ICON_SIZE_DND, &w, NULL);
            pix = fm_pixbuf_from_icon(icon, w);
            if (pix)
            {
                gtk_drag_set_icon_pixbuf(context, pix, 0, 0);
                g_object_unref(pix);
            }
        }
#endif
    }
}

static void on_launchbar_drag_data_get(GtkWidget *widget, GdkDragContext *context,
                                       GtkSelectionData *sel_data,
                                       guint info, guint time,
                                       LaunchTaskBarPlugin *lb)
{
    char *path_str;

    switch(info)
    {
    case LAUNCHBUTTON_DND_TARGET:
        if (lb->dragged_launcher == NULL)
            break;
        /* create a path_str for currently dragged launcher */
        path_str = fm_path_to_str(lb->dragged_launcher);
        /* save it into selection */
        gtk_selection_data_set(sel_data, launch_button_dnd_atom, 8,
                               (guchar *)path_str, strlen(path_str));
        g_free(path_str);
        break;
    default: ;
    }
}

static void on_launchbar_drag_data_delete(GtkWidget *widget,
                                          GdkDragContext *context,
                                          LaunchTaskBarPlugin *lb)
{
    /* check if remembered data are still valid and remove appropriate child */
    if (lb->dragged_launcher == NULL)
        return;
    launchbar_remove_launcher(lb, lb->dragged_launcher);
    fm_path_unref(lb->dragged_launcher);
    lb->dragged_launcher = NULL;
    if (lb->mode == LAUNCHBAR)
    {
        /* destroy empty plugin if it is in LAUNCHBAR mode */
        if (panel_icon_grid_get_n_children(PANEL_ICON_GRID(lb->lb_icon_grid)) == 0)
            lxpanel_remove_plugin(lb->panel, lb->plugin);
    }
    else
        launchbar_check_bootstrap(lb);
}

static void on_launchbar_drag_end(GtkWidget *widget, GdkDragContext *context,
                                  LaunchTaskBarPlugin *lb)
{
    /* forget the currently dragged launcher */
    if (lb->dragged_launcher)
        fm_path_unref(lb->dragged_launcher);
    lb->dragged_launcher = NULL;
}


/* destination side -- process data */
static gboolean on_launchbar_drag_drop(GtkWidget *widget, GdkDragContext *context,
                                       gint x, gint y, guint time,
                                       LaunchTaskBarPlugin *lb)
{
    GdkAtom target;

    target = gtk_drag_dest_find_target(widget, context, NULL);
    if (target == launch_button_dnd_atom)
    {
        /* request for data, it will be processed on "drag-data-received" signal */
        gtk_drag_get_data(widget, context, launch_button_dnd_atom, time);
        return TRUE;
    }
    target = fm_dnd_dest_find_target(lb->dd, context);
    if (G_LIKELY(target != GDK_NONE))
        return fm_dnd_dest_drag_drop(lb->dd, context, target, x, y, time);
    return FALSE;
}

static void on_launchbar_drag_data_received(GtkWidget *widget,
                                            GdkDragContext *context, gint x,
                                            gint y, GtkSelectionData *sel_data,
                                            guint info, guint time,
                                            LaunchTaskBarPlugin *lb)
{
    PanelIconGrid *ig = PANEL_ICON_GRID(widget);
    GtkWidget *btn;
    PanelIconGridDropPosition pos;
    int i;
    const char *path_str;
    FmPath *path;
    config_setting_t *s;

    switch(info)
    {
    case LAUNCHBUTTON_DND_TARGET:
        if (panel_icon_grid_get_dest_at_pos(ig, x, y, &btn, &pos) &&
            PANEL_IS_LAUNCH_BUTTON(btn) && (pos != PANEL_ICON_GRID_DROP_INTO ||
            launch_button_get_settings((LaunchButton *)btn) == NULL))
        {
            /* it is destined to some position, calculate it */
            i = panel_icon_grid_get_child_position(ig, btn);
            g_return_if_fail(i >= 0);
            if (pos == PANEL_ICON_GRID_DROP_LEFT_AFTER ||
                pos == PANEL_ICON_GRID_DROP_RIGHT_AFTER)
                i++;
            /* get data from selection */
            path_str = (char *)gtk_selection_data_get_data(sel_data);
            if (!path_str)
                break;
            /* g_debug("dropping dragged button to %d position", i); */
            path = fm_path_new_for_str(path_str);
            /* create new LaunchButton */
            s = config_group_add_subgroup(lb->settings, "Button");
            if (fm_path_equal(fm_path_get_scheme_path(path), fm_path_get_apps_menu()))
                config_group_set_string(s, "id", fm_path_get_basename(path));
            else
                config_group_set_string(s, "id", path_str);
            btn = GTK_WIDGET(launch_button_new(lb->panel, lb->plugin, path, s));
            fm_path_unref(path);
            if (btn)
            {
                /* position it and send confirmation to remove it from former place */
                gtk_container_add(GTK_CONTAINER(lb->lb_icon_grid), btn);
                panel_icon_grid_reorder_child(PANEL_ICON_GRID(lb->lb_icon_grid),
                                              btn, i);
                config_setting_move_elem(s, config_setting_get_parent(s), i);
                lxpanel_config_save(lb->panel);
                launchbar_remove_bootstrap(lb);
                gtk_drag_finish(context, TRUE, TRUE, time);
                break;
            }
            /* failed to create button - should be impossible though */
            config_setting_destroy(s);
        }
        gtk_drag_finish(context, FALSE, FALSE, time);
        break;
    default:
        fm_dnd_dest_drag_data_received(lb->dd, context, x, y, sel_data, info, time);
    }
}

/* Handler for "drag-motion" event from launchtaskbar button. */
static gboolean on_launchbar_drag_motion(
    GtkWidget * widget,
    GdkDragContext * context,
    gint x,
    gint y,
    guint time,
    LaunchTaskBarPlugin * b)
{
    GdkAtom target;
    GdkDragAction action = 0, lb_atom_action = 0;
    GtkWidget *btn;
    FmFileInfo *fi = NULL;
    PanelIconGridDropPosition pos;

    if (!panel_icon_grid_get_dest_at_pos(PANEL_ICON_GRID(b->lb_icon_grid), x, y,
                                         &btn, &pos) || !btn)
    {
        panel_icon_grid_set_drag_dest(PANEL_ICON_GRID(b->lb_icon_grid), NULL, 0);
        fm_dnd_dest_set_dest_file(b->dd, NULL);
        gdk_drag_status(context, 0, time);
        return TRUE;
    }
    b->last_lb_drag_dest = btn;
    b->last_lb_drag_pos = pos;
    panel_icon_grid_set_drag_dest(PANEL_ICON_GRID(b->lb_icon_grid), btn, pos);
    if (PANEL_IS_LAUNCH_BUTTON(btn))
    {
        if (launch_button_get_settings((LaunchButton *)btn) == NULL)
        {
            /* bootstrap button */
            b->last_lb_drag_pos = PANEL_ICON_GRID_DROP_LEFT_BEFORE;
            lb_atom_action = GDK_ACTION_MOVE;
        }
        else if (pos == PANEL_ICON_GRID_DROP_INTO)
            fi = launch_button_get_file_info((LaunchButton *)btn);
        else
            lb_atom_action = GDK_ACTION_MOVE;
    }
    fm_dnd_dest_set_dest_file(b->dd, fi);
    target = fm_dnd_dest_find_target(b->dd, context);
    if (target == GDK_NONE)
    {
        target = gtk_drag_dest_find_target(widget, context, NULL);
        if (target == launch_button_dnd_atom)
            action = lb_atom_action; /* action was set above */
    }
    else if (fm_dnd_dest_is_target_supported(b->dd, target))
    {
        action = fm_dnd_dest_get_default_action(b->dd, context, target);
        if (fi == NULL && PANEL_IS_LAUNCH_BUTTON(btn))
            /* dropping on free place */
            action = GDK_ACTION_COPY;
    }
    gdk_drag_status(context, action, time);
    /* g_debug("launchbutton_drag_motion_event: act=%u",action); */
    return TRUE;
}

static void on_launchbar_drag_leave(GtkWidget * widget, GdkDragContext * drag_context,
                                    guint time, LaunchTaskBarPlugin * lb)
{
    fm_dnd_dest_drag_leave(lb->dd, drag_context, time);
    panel_icon_grid_set_drag_dest(PANEL_ICON_GRID(lb->lb_icon_grid), NULL, 0);
    fm_dnd_dest_set_dest_file(lb->dd, NULL);
}

static gboolean on_launchbar_files_dropped(FmDndDest *dd, int x, int y, GdkDragAction action,
                                           FmDndDestTargetType info_type,
                                           FmPathList *files, LaunchTaskBarPlugin *lb)
{
    FmPath *path;
    LaunchButton *btn;
    char *path_str;
    config_setting_t *s;
    int i;

    if (action != GDK_ACTION_COPY)
        return FALSE;
    path = fm_path_list_peek_head(files);
    if (!path)
        return FALSE;
    if (!lb->last_lb_drag_dest)
        return FALSE;
    i = panel_icon_grid_get_child_position(PANEL_ICON_GRID(lb->lb_icon_grid),
                                           lb->last_lb_drag_dest);
    lb->last_lb_drag_dest = NULL;
    if (i < 0)
        return FALSE;
    switch (lb->last_lb_drag_pos)
    {
    case PANEL_ICON_GRID_DROP_LEFT_AFTER:
    case PANEL_ICON_GRID_DROP_RIGHT_AFTER:
        i++;
        break;
    case PANEL_ICON_GRID_DROP_LEFT_BEFORE:
    case PANEL_ICON_GRID_DROP_RIGHT_BEFORE:
        break;
    default:
        return FALSE;
    }
    s = config_group_add_subgroup(lb->settings, "Button");
    if (fm_path_equal(fm_path_get_scheme_path(path), fm_path_get_apps_menu()))
        config_group_set_string(s, "id", fm_path_get_basename(path));
    else
    {
        path_str = fm_path_to_str(path);
        /* g_debug("*** path '%s' pos %d", path_str, i); */
        config_group_set_string(s, "id", path_str);
        g_free(path_str);
    }
    btn = launch_button_new(lb->panel, lb->plugin, path, s);
    if (btn)
    {
        gtk_container_add(GTK_CONTAINER(lb->lb_icon_grid), GTK_WIDGET(btn));
        panel_icon_grid_reorder_child(PANEL_ICON_GRID(lb->lb_icon_grid),
                                      GTK_WIDGET(btn), i);
        config_setting_move_elem(s, config_setting_get_parent(s), i);
        lxpanel_config_save(lb->panel);
        launchbar_remove_bootstrap(lb);
    }
    else
        config_setting_destroy(s);
    return TRUE;
}

static LaunchButton *launchbar_exec_bin_exists(LaunchTaskBarPlugin *lb, FmPath *path)
{
    LaunchButton *ret_val = NULL;
    GList *children, *l;
    FmFileInfo *fi;

    if (!path)
        return NULL;
    children = gtk_container_get_children(GTK_CONTAINER(lb->lb_icon_grid));
    for (l = children; l != NULL; l = l->next)
    {
        fi = launch_button_get_file_info(PANEL_LAUNCH_BUTTON(l->data));
        if (fi && fm_path_equal(path, fm_file_info_get_path(fi)))
        {
            ret_val = l->data;
            break;
        }
    }
    g_list_free(children);
    return ret_val;
}

/* Read the configuration file entry for a launchtaskbar button and create it. */
static gboolean launchbutton_constructor(LaunchTaskBarPlugin * lb, config_setting_t * s)
{
    LaunchButton *btn = NULL;
    const char *str;
    char *str_path = NULL;
    FmPath *path;

    /* Read parameters from the configuration file and validate. */
    if (!config_setting_lookup_string(s, "id", &str) || str[0] == '\0')
        return FALSE;

    /* Build the structures and return. */
    if (str[0] == '~')
    {
        str_path = expand_tilda(str);
        path = fm_path_new_for_path(str_path);
    }
    else if (strchr(str, '/') != NULL)
    {
        path = fm_path_new_for_str(str);
        /* FIXME: check if str contains invalid path */
    }
    else
    {
        str_path = g_strdup_printf("search://menu://applications/?recursive=1&show_hidden=1&name=%s", str);
        path = fm_path_new_for_uri(str_path);
    }
    btn = launch_button_new(lb->panel, lb->plugin, path, s);
    g_free(str_path);
    fm_path_unref(path);
    if (btn)
        gtk_container_add(GTK_CONTAINER(lb->lb_icon_grid), GTK_WIDGET(btn));
    return (btn != NULL);
}

/* prototype of this is app_info_create_from_commandline() in libfm */
static gboolean _launchbutton_create_id(LaunchTaskBarPlugin * lb, config_setting_t * s)
{
    const char *icon = NULL, *name, *exec, *path = NULL;
    char *dirname, *filename;
    int fd, terminal = 0;
    gboolean ret = FALSE;

    if (!config_setting_lookup_string(s, "action", &exec) || exec[0] == '\0')
        return FALSE;
    if (!config_setting_lookup_string(s, "tooltip", &name) || name[0] == '\0')
        name = "Launcher"; /* placeholder, XDG requires a non-empty name */
    config_setting_lookup_string(s, "image", &icon);
    config_setting_lookup_string(s, "path", &path);
    config_setting_lookup_int(s, "terminal", &terminal);

    dirname = g_build_filename(g_get_user_data_dir(), "applications", NULL);
    if (g_mkdir_with_parents(dirname, 0700) == 0)
    {
        filename = g_strdup_printf("%s/lxpanel-launcher-XXXXXX.desktop", dirname);
        fd = g_mkstemp (filename);
        if (fd != -1)
        {
            GString* content = g_string_sized_new(256);

            g_string_printf(content,
                "[" G_KEY_FILE_DESKTOP_GROUP "]\n"
                G_KEY_FILE_DESKTOP_KEY_TYPE "=" G_KEY_FILE_DESKTOP_TYPE_APPLICATION "\n"
                G_KEY_FILE_DESKTOP_KEY_NAME "=%s\n"
                G_KEY_FILE_DESKTOP_KEY_EXEC "=%s\n"
                G_KEY_FILE_DESKTOP_KEY_CATEGORIES "=X-LXPanel;\n",
                name, exec);
            if (icon)
                g_string_append_printf(content, "Icon=%s\n", icon);
            if (terminal)
                g_string_append(content, G_KEY_FILE_DESKTOP_KEY_TERMINAL "=true\n");
            if (path && path[0] == '/')
                g_string_append_printf(content, "Path=%s\n", path);
            close(fd);
            ret = g_file_set_contents(filename, content->str, content->len, NULL);
            if (ret) {
                config_group_set_string(s, "id", filename);
                /* FIXME: is it reasonable to remove obsolete keys too? */
                lxpanel_config_save(lb->panel);
            } else
                g_unlink(filename);
            g_string_free(content, TRUE);
        }
        g_free(filename);
    }
    g_free(dirname);
    if (ret) /* we created it, let use it */
        return launchbutton_constructor(lb, s);
    return FALSE;
}

static void launchtaskbar_constructor_add_default_special_case(LaunchTaskBarPlugin *ltbp, const gchar *tk_exec, const gchar *mb_exec)
{
    g_key_file_set_value(ltbp->p_key_file_special_cases, "special_cases", tk_exec, mb_exec);
}

static void launchtaskbar_constructor_launch(LaunchTaskBarPlugin *ltbp)
{
    config_setting_t *settings;
    guint i = 0;

    if(!ltbp->lb_built)
    {
        ltbp->lb_built = TRUE;
        /* Read parameters from the configuration file. */
        settings = config_setting_get_member(ltbp->settings, "");
        if (settings && config_setting_is_list(settings))
        {
            config_setting_t *s;

            while ((s = config_setting_get_elem(settings, i)) != NULL)
            {
                if (strcmp(config_setting_get_name(s), "Button") != 0)
                {
                    g_warning("launchtaskbar: illegal token %s\n", config_setting_get_name(s));
                    config_setting_destroy(s);
                }
                else if (!launchbutton_constructor(ltbp, s) &&
                         /* try to create desktop id from old-style manual setup */
                         !_launchbutton_create_id(ltbp, s))
                {
                    g_warning( "launchtaskbar: can't init button\n");
                    /* FIXME: show failed id to the user instead */
                    config_setting_destroy(s);
                }
                else /* success, accept the setting */
                    i++;
            }
        }
        if (i == 0)
        {
            /* build bootstrap button */
            launchbar_check_bootstrap(ltbp);
        }
        /* Drag and drop support. */
        g_signal_connect(ltbp->lb_icon_grid, "button-press-event",
                         G_CALLBACK(on_launchbar_drag_source), ltbp);
        g_signal_connect(ltbp->lb_icon_grid, "button-release-event",
                         G_CALLBACK(on_launchbar_drag_source), ltbp);
        g_signal_connect(ltbp->lb_icon_grid, "motion-notify-event",
                         G_CALLBACK(on_launchbar_drag_source), ltbp);
        g_signal_connect(ltbp->lb_icon_grid, "drag-begin",
                         G_CALLBACK(on_launchbar_drag_begin), ltbp);
        g_signal_connect(ltbp->lb_icon_grid, "drag-data-get",
                         G_CALLBACK(on_launchbar_drag_data_get), ltbp);
        g_signal_connect(ltbp->lb_icon_grid, "drag-data-delete",
                         G_CALLBACK(on_launchbar_drag_data_delete), ltbp);
        g_signal_connect(ltbp->lb_icon_grid, "drag-end",
                         G_CALLBACK(on_launchbar_drag_end), ltbp);
        ltbp->dd = fm_dnd_dest_new(ltbp->lb_icon_grid);
        fm_dnd_dest_add_targets(ltbp->lb_icon_grid, dnd_targets,
                                G_N_ELEMENTS(dnd_targets));
        g_signal_connect(ltbp->lb_icon_grid, "drag-motion",
                         G_CALLBACK(on_launchbar_drag_motion), ltbp);
        g_signal_connect(ltbp->lb_icon_grid, "drag-drop",
                         G_CALLBACK(on_launchbar_drag_drop), ltbp);
        g_signal_connect(ltbp->lb_icon_grid, "drag-data-received",
                         G_CALLBACK(on_launchbar_drag_data_received), ltbp);
        g_signal_connect(ltbp->lb_icon_grid, "drag-leave",
                         G_CALLBACK(on_launchbar_drag_leave), ltbp);
        g_signal_connect(ltbp->dd, "files-dropped",
                         G_CALLBACK(on_launchbar_files_dropped), ltbp);
        launch_button_dnd_atom = gdk_atom_intern("application/x-lxpanel-launcher", FALSE);
    }
    gtk_widget_set_visible(ltbp->lb_icon_grid, TRUE);
}

static void flash_window_update_iter(GtkWidget *widget, gpointer data)
{
    task_button_set_flash_state(PANEL_TASK_BUTTON(widget), GPOINTER_TO_INT(data));
}

static gboolean flash_window_timeout(gpointer user_data)
{
    LaunchTaskBarPlugin *tb;

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    tb = user_data;
    tb->flash_state = !tb->flash_state;
    gtk_container_foreach(GTK_CONTAINER(tb->tb_icon_grid),
                          flash_window_update_iter, GINT_TO_POINTER(tb->flash_state));
    return TRUE;
}

static void on_gtk_cursor_blink_time_changed(GObject *gsettings, GParamSpec *pspec,
                                             LaunchTaskBarPlugin *tb)
{
    gint interval;

    if (tb->flash_timeout == 0) /* nothing to do? */
        return;
    g_source_remove(tb->flash_timeout);
    g_object_get(gtk_widget_get_settings(GTK_WIDGET(tb)), "gtk-cursor-blink-time",
                 &interval, NULL);
    tb->flash_timeout = g_timeout_add(interval / 2, flash_window_timeout, tb);
}

/* Set an urgency timer on a task. */
static void set_timer_on_task(LaunchTaskBarPlugin *tb)
{
    gint interval;

    if (tb->flash_timeout != 0)
        return;
    g_object_get(gtk_widget_get_settings(GTK_WIDGET(tb->plugin)),
                 "gtk-cursor-blink-time", &interval, NULL);
    g_signal_connect(gtk_widget_get_settings(GTK_WIDGET(tb->plugin)),
                     "notify::gtk-cursor-blink-time",
                     G_CALLBACK(on_gtk_cursor_blink_time_changed), tb);
    tb->flash_timeout = g_timeout_add(interval / 2, flash_window_timeout, tb);
}

static void reset_timer_on_task(LaunchTaskBarPlugin *tb)
{
    if (tb->flash_timeout == 0)
        return;
    g_source_remove(tb->flash_timeout);
    tb->flash_timeout = 0;
    g_signal_handlers_disconnect_by_func(gtk_widget_get_settings(GTK_WIDGET(tb->plugin)),
                                         on_gtk_cursor_blink_time_changed, tb);
}


static void launchtaskbar_constructor_task(LaunchTaskBarPlugin *ltbp)
{
    if(!ltbp->tb_built)
    {
        config_setting_t *s = ltbp->settings;
        gint tmp_int;

        ltbp->tb_built = TRUE;

        /* Parse configuration now */
        if (config_setting_lookup_int(s, "tooltips", &tmp_int))
            ltbp->flags.tooltips = (tmp_int != 0);
        if (config_setting_lookup_int(s, "IconsOnly", &tmp_int))
            ltbp->flags.icons_only = (tmp_int != 0);
        if (config_setting_lookup_int(s, "ShowAllDesks", &tmp_int))
            ltbp->flags.show_all_desks = (tmp_int != 0);
        if (config_setting_lookup_int(s, "SameMonitorOnly", &tmp_int))
            ltbp->flags.same_monitor_only = (tmp_int != 0);
        if (config_setting_lookup_int(s, "DisableUpscale", &tmp_int))
            ltbp->flags.disable_taskbar_upscale = (tmp_int != 0);
        config_setting_lookup_int(s, "MaxTaskWidth", &ltbp->task_width_max);
        config_setting_lookup_int(s, "spacing", &ltbp->spacing);
        if (config_setting_lookup_int(s, "UseMouseWheel", &tmp_int))
            ltbp->flags.use_mouse_wheel = (tmp_int != 0);
        if (config_setting_lookup_int(s, "UseUrgencyHint", &tmp_int))
            ltbp->flags.use_urgency_hint = (tmp_int != 0);
        if (config_setting_lookup_int(s, "FlatButton", &tmp_int))
            ltbp->flags.flat_button = (tmp_int != 0);
        if (config_setting_lookup_int(s, "GroupedTasks", &tmp_int))
            ltbp->grouped_tasks = (tmp_int != 0);
        if (config_setting_lookup_int(s, "UseSmallerIcons", &tmp_int))
            ltbp->flags.use_smaller_icons = (tmp_int != 0);

        /* Make container for task buttons as a child of top level widget. */
        ltbp->tb_icon_grid = panel_icon_grid_new(panel_get_orientation(ltbp->panel),
                                                 ((ltbp->flags.icons_only) ? ltbp->icon_size + ICON_ONLY_EXTRA : ltbp->task_width_max),
                                                 ((ltbp->flags.icons_only) ? ltbp->icon_size + ICON_ONLY_EXTRA : ltbp->icon_size + ICON_BUTTON_TRIM),
                                                 ltbp->spacing, 0,
                                                 panel_get_height(ltbp->panel));
        panel_icon_grid_set_constrain_width(PANEL_ICON_GRID(ltbp->tb_icon_grid), TRUE);
        gtk_box_pack_start(GTK_BOX(ltbp->plugin), ltbp->tb_icon_grid, TRUE, TRUE, 0);
        /* taskbar_update_style(ltbp); */

        /* Add GDK event filter. */
        gdk_window_add_filter(NULL, (GdkFilterFunc) taskbar_event_filter, ltbp);

        /* Connect signals to receive root window events and initialize root window properties. */
        ltbp->number_of_desktops = get_net_number_of_desktops();
        ltbp->current_desktop = get_net_current_desktop();
        g_signal_connect(G_OBJECT(fbev), "current-desktop", G_CALLBACK(taskbar_net_current_desktop), (gpointer) ltbp);
        g_signal_connect(G_OBJECT(fbev), "active-window", G_CALLBACK(taskbar_net_active_window), (gpointer) ltbp);
        g_signal_connect(G_OBJECT(fbev), "number-of-desktops", G_CALLBACK(taskbar_net_number_of_desktops), (gpointer) ltbp);
        g_signal_connect(G_OBJECT(fbev), "client-list", G_CALLBACK(taskbar_net_client_list), (gpointer) ltbp);

        /* Connect a signal to be notified when the window manager changes.  This causes re-evaluation of the "use_net_active" status. */
        g_signal_connect(ltbp->screen, "window-manager-changed", G_CALLBACK(taskbar_window_manager_changed), ltbp);

        /* Start blinking timeout if configured */
        if (ltbp->flags.use_urgency_hint)
            set_timer_on_task(ltbp);

        /* Fetch the client list and redraw the taskbar.  Then determine what window has focus. */
        taskbar_net_client_list(NULL, ltbp);
        taskbar_net_active_window(NULL, ltbp);
    }
    gtk_widget_set_visible(ltbp->tb_icon_grid, TRUE);
}

/* Override GtkBox bug - it does not always propagate allocation to children */
static void on_size_allocation(GtkWidget *widget, GtkAllocation *a, LaunchTaskBarPlugin *ltbp)
{
    if (ltbp->w != a->width || ltbp->h != a->height)
    {
        ltbp->w = a->width;
        ltbp->h = a->height;
        if (ltbp->lb_built && gtk_widget_get_visible(ltbp->lb_icon_grid))
            panel_icon_grid_set_geometry(PANEL_ICON_GRID(ltbp->lb_icon_grid),
                                         panel_get_orientation(ltbp->panel),
                                         ltbp->icon_size, ltbp->icon_size,
                                         3, 0, panel_get_height(ltbp->panel));
        if (ltbp->tb_built && gtk_widget_get_visible(ltbp->tb_icon_grid))
            panel_icon_grid_set_geometry(PANEL_ICON_GRID(ltbp->tb_icon_grid),
                                         panel_get_orientation(ltbp->panel),
                                         ((ltbp->flags.icons_only) ? ltbp->icon_size + ICON_ONLY_EXTRA : ltbp->task_width_max),
                                         ((ltbp->flags.icons_only) ? ltbp->icon_size + ICON_ONLY_EXTRA : ltbp->icon_size + ICON_BUTTON_TRIM),
                                         ltbp->spacing, 0,
                                         panel_get_height(ltbp->panel));
    }
}

/* Plugin constructor. */
static GtkWidget *_launchtaskbar_constructor(LXPanel *panel, config_setting_t *settings,
                                             LtbMode mode)
{
    GtkWidget *p;
    LaunchTaskBarPlugin *ltbp;
    int height;

    gtk_rc_parse_string(launchtaskbar_rc);

    /* Allocate plugin context and set into Plugin private data pointer. */
    ltbp = g_new0(LaunchTaskBarPlugin, 1);
    ltbp->panel = panel;
    ltbp->settings = settings;
    ltbp->mode = mode;
    ltbp->screen = gtk_widget_get_screen((GtkWidget*)panel);

    /* Initialize to defaults. */
    ltbp->icon_size         = panel_get_icon_size(panel);
    ltbp->flags.tooltips    = TRUE;
    ltbp->flags.icons_only  = FALSE;
    ltbp->flags.show_all_desks = TRUE;
    ltbp->task_width_max    = TASK_WIDTH_MAX;
    ltbp->spacing           = 1;
    ltbp->flags.use_mouse_wheel = TRUE;
    ltbp->flags.use_urgency_hint = TRUE;
    ltbp->flags.use_smaller_icons = TRUE;
    ltbp->grouped_tasks     = FALSE;
    ltbp->fixed_mode        = (mode == LAUNCHBAR) || (mode == TASKBAR);

    /* Special cases key file */
    ltbp->p_key_file_special_cases = g_key_file_new();
    gchar *special_cases_filepath = g_build_filename(g_get_user_config_dir(),
                                                     "lxpanel", "launchtaskbar.cfg", NULL);
    if (!g_key_file_load_from_file(ltbp->p_key_file_special_cases,
                                   special_cases_filepath,
                                   G_KEY_FILE_KEEP_COMMENTS, NULL))
    {
        launchtaskbar_constructor_add_default_special_case(ltbp, "synaptic", "synaptic-pkexec");
        launchtaskbar_constructor_add_default_special_case(ltbp, "soffice.bin", "libreoffice");
        launchtaskbar_constructor_add_default_special_case(ltbp, "x-terminal-emulator", "lxterminal");
        gchar *key_file_data = g_key_file_to_data(ltbp->p_key_file_special_cases, NULL, NULL);
        g_file_set_contents(special_cases_filepath, key_file_data, -1, NULL);
        g_free(key_file_data);
    }
    g_free(special_cases_filepath);

    /* Allocate top level widget and set into Plugin widget pointer. */
    ltbp->plugin = p = panel_box_new(panel, FALSE, 5);
    lxpanel_plugin_set_data(p, ltbp, launchtaskbar_destructor);
    /* Allocate an icon grid manager to manage the container. */
    height = panel_get_height(panel);
    ltbp->lb_icon_grid = panel_icon_grid_new(panel_get_orientation(panel),
                                             ltbp->icon_size, ltbp->icon_size,
                                             3, 0, height);
    gtk_box_pack_start(GTK_BOX(p), ltbp->lb_icon_grid, FALSE, TRUE, 0);

    /* Setup override on GtkBox bug */
    g_signal_connect(p, "size-allocate", G_CALLBACK(on_size_allocation), ltbp);

    /* Read parameters from the configuration file. */
    config_setting_lookup_int(settings, "LaunchTaskBarMode", &ltbp->mode);
    switch (ltbp->mode) {
    case LAUNCHBAR:
        launchtaskbar_constructor_launch(ltbp);
        gtk_widget_set_name(p, "launchbar");
        break;
    default:
        ltbp->mode = LAUNCHTASKBAR; /* reset invalid value */
    case LAUNCHTASKBAR:
        launchtaskbar_constructor_launch(ltbp);
        gtk_widget_set_name(p, "launchtaskbar");
    case TASKBAR:
        launchtaskbar_constructor_task(ltbp);
        if (ltbp->mode == TASKBAR)
            gtk_widget_set_name(p, "taskbar");
    }

    return p;
}

static GtkWidget *launchtaskbar_constructor(LXPanel *panel, config_setting_t *settings)
{
    return _launchtaskbar_constructor(panel, settings, LAUNCHTASKBAR);
}

static void launchtaskbar_destructor_launch(LaunchTaskBarPlugin *ltbp)
{
    if (ltbp->dd)
    {
        g_signal_handlers_disconnect_by_func(ltbp->dd, on_launchbar_files_dropped, ltbp);
        g_object_unref(ltbp->dd);
    }
    /* do not disconnect handler on child widget - it is already destroyed */
    if (ltbp->dragged_launcher)
        fm_path_unref(ltbp->dragged_launcher);
}

static void launchtaskbar_destructor_task(LaunchTaskBarPlugin *ltbp)
{
    /* Remove GDK event filter. */
    gdk_window_remove_filter(NULL, (GdkFilterFunc) taskbar_event_filter, ltbp);

    /* Remove root window signal handlers. */
    g_signal_handlers_disconnect_by_func(fbev, taskbar_net_current_desktop, ltbp);
    g_signal_handlers_disconnect_by_func(fbev, taskbar_net_active_window, ltbp);
    g_signal_handlers_disconnect_by_func(fbev, taskbar_net_number_of_desktops, ltbp);
    g_signal_handlers_disconnect_by_func(fbev, taskbar_net_client_list, ltbp);

    /* Remove "window-manager-changed" handler. */
    g_signal_handlers_disconnect_by_func(ltbp->screen, taskbar_window_manager_changed, ltbp);

    /* Stop blinking timeout */
    reset_timer_on_task(ltbp);
#ifndef DISABLE_MENU
    if (ltbp->path)
        fm_path_unref(ltbp->path);
#endif

    /* DND delay handler */
    if (ltbp->dnd_delay_timer != 0)
    {
        g_source_remove(ltbp->dnd_delay_timer);
        ltbp->dnd_delay_timer = 0;
    }
    if (ltbp->dnd_delay_task)
        g_object_remove_weak_pointer(G_OBJECT(ltbp->dnd_delay_task), (gpointer *)&ltbp->dnd_delay_task);
}

/* Plugin destructor. */
static void launchtaskbar_destructor(gpointer user_data)
{
    LaunchTaskBarPlugin *ltbp = (LaunchTaskBarPlugin *)user_data;

    // TASKBAR
    if(ltbp->tb_built) launchtaskbar_destructor_task(ltbp);

    // LAUNCHBAR
    if(ltbp->lb_built) launchtaskbar_destructor_launch(ltbp);

    // LAUNCHTASKBAR

    /* Deallocate all memory. */
    if (ltbp->p_key_file_special_cases != NULL)
        g_key_file_free(ltbp->p_key_file_special_cases);
    g_free(ltbp);
}

static void _launchbar_configure_add(GtkTreeView *menu_view, LaunchTaskBarPlugin *ltbp)
{
    GtkTreeView * defined_view = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(ltbp->config_dlg), "defined_view"));
    FmPath * sel_path = fm_app_menu_view_dup_selected_app_desktop_path(menu_view);
    LaunchButton * btn;

    if (sel_path != NULL &&
        (btn = launch_button_new(ltbp->panel, ltbp->plugin, sel_path, NULL)) != NULL &&
        launch_button_wait_load(btn))
    {
        GtkListStore * list = GTK_LIST_STORE(gtk_tree_view_get_model(defined_view));
        GtkTreeIter it;
        GdkPixbuf* pix;
        char *path;
        config_setting_t *settings;
        gtk_container_add(GTK_CONTAINER(ltbp->lb_icon_grid), GTK_WIDGET(btn));
        gtk_list_store_append(list, &it);
        pix = fm_pixbuf_from_icon(launch_button_get_icon(btn), PANEL_ICON_SIZE);
        gtk_list_store_set(list, &it,
            COL_ICON, pix,
            COL_TITLE, launch_button_get_disp_name(btn),
            COL_BTN, btn,
            -1);
        g_object_unref(pix);
        settings = config_group_add_subgroup(ltbp->settings, "Button");
        if (fm_path_equal(fm_path_get_scheme_path(sel_path), fm_path_get_apps_menu()))
            config_group_set_string(settings, "id", fm_path_get_basename(sel_path));
        else
        {
            path = fm_path_to_str(sel_path);
            /* g_debug("*** path '%s'",path); */
            config_group_set_string(settings, "id", path);
            g_free(path);
        }
        launch_button_set_settings(btn, settings);
        lxpanel_config_save(ltbp->panel);
        fm_path_unref(sel_path);
        launchbar_remove_bootstrap(ltbp);
    }
}

/* Handler for "clicked" action on launchtaskbar configuration dialog "Add" button. */
static void launchbar_configure_add_button(GtkButton * widget, LaunchTaskBarPlugin *ltbp)
{
    GtkTreeView * menu_view = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(ltbp->config_dlg), "menu_view"));

    _launchbar_configure_add(menu_view, ltbp);
}

static void  launchbar_remove_button(LaunchTaskBarPlugin *ltbp, LaunchButton *btn)
{
    config_setting_destroy(launch_button_get_settings(btn));
    lxpanel_config_save(ltbp->panel);
    gtk_widget_destroy(GTK_WIDGET(btn));
}

/* Handler for "clicked" action on launchtaskbar configuration dialog "Remove" button. */
static void launchbar_configure_remove_button(GtkButton * widget, LaunchTaskBarPlugin *ltbp)
{
    GtkTreeView * defined_view = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(ltbp->config_dlg), "defined_view"));
    GtkTreeModel * list;
    GtkTreeIter it;
    if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(defined_view), &list, &it))
    {
        LaunchButton * btn;
        gtk_tree_model_get(list, &it, COL_BTN, &btn, -1);

        /* We have found a selected button.
         * Remove it from the icon grid, the data structure, and the view. */
        gtk_list_store_remove(GTK_LIST_STORE(list), &it);
        gtk_widget_set_visible(ltbp->p_label_def_app_exec, FALSE);

        launchbar_remove_button(ltbp, btn);
        launchbar_check_bootstrap(ltbp);
    }
}

/* Handler for "clicked" action on launchtaskbar configuration dialog "Move Up" button. */
static void launchbar_configure_move_up_button(GtkButton * widget, LaunchTaskBarPlugin *ltbp)
{
    GtkTreeView * defined_view = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(ltbp->config_dlg), "defined_view"));
    GtkTreeModel * list;
    GtkTreeIter it;
    if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(defined_view), &list, &it))
    {
        LaunchButton *btn;
        gtk_tree_model_get(GTK_TREE_MODEL(list), &it, COL_BTN, &btn, -1);
        GtkTreePath * path = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &it);
        if ((gtk_tree_path_get_indices(path)[0] > 0)
        && (gtk_tree_path_prev(path)))
        {
            GtkTreeIter it2;
            if (gtk_tree_model_get_iter(list, &it2, path))
            {
                /* We have found a selected button that can be moved.
                 * Reorder it in the icon grid, the data structure, and the view. */
                int i = gtk_tree_path_get_indices(path)[0];
                config_setting_t *settings = launch_button_get_settings(btn);
                gtk_list_store_move_before(GTK_LIST_STORE(list), &it, &it2);
                panel_icon_grid_reorder_child(PANEL_ICON_GRID(ltbp->lb_icon_grid),
                                              GTK_WIDGET(btn), i);
                config_setting_move_elem(settings,
                                         config_setting_get_parent(settings), i);
            }
        }
        gtk_tree_path_free(path);
    }
}

/* Handler for "clicked" action on launchtaskbar configuration dialog "Move Down" button. */
static void launchbar_configure_move_down_button(GtkButton * widget, LaunchTaskBarPlugin *ltbp)
{
    GtkTreeView * defined_view = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(ltbp->config_dlg), "defined_view"));
    GtkTreeModel * list;
    GtkTreeIter it;
    if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(defined_view), &list, &it))
    {
        LaunchButton *btn;
        gtk_tree_model_get(GTK_TREE_MODEL(list), &it, COL_BTN, &btn, -1);
        GtkTreePath * path = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &it);
        int n = gtk_tree_model_iter_n_children(list, NULL);
        if (gtk_tree_path_get_indices(path)[0] < (n - 1))
        {
            gtk_tree_path_next(path);
            GtkTreeIter it2;
            if (gtk_tree_model_get_iter( list, &it2, path))
            {
                /* We have found a selected button that can be moved.
                 * Reorder it in the icon grid, the data structure, and the view. */
                int i = gtk_tree_path_get_indices(path)[0];
                config_setting_t *settings = launch_button_get_settings(btn);
                gtk_list_store_move_after(GTK_LIST_STORE(list), &it, &it2);
                panel_icon_grid_reorder_child(PANEL_ICON_GRID(ltbp->lb_icon_grid),
                                              GTK_WIDGET(btn), i);
                config_setting_move_elem(settings,
                                         config_setting_get_parent(settings), i);
            }
        }
        gtk_tree_path_free(path);
    }
}

/* Initialize the list of existing launchtaskbar buttons when the configuration dialog is shown. */
static void launchbar_configure_initialize_list(LaunchTaskBarPlugin *ltbp, GtkWidget * dlg, GtkTreeView * view)
{
    /* Set the selection mode. */
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(view), GTK_SELECTION_BROWSE);

    /* Define a column. */
    GtkTreeViewColumn* col = gtk_tree_view_get_column(view, 0);

    /* Establish the pixbuf column cell renderer. */
    GtkCellRenderer * render = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, render, FALSE);
    gtk_tree_view_column_set_attributes(col, render, "pixbuf", COL_ICON, NULL);

    /* Establish the text column cell renderer. */
    render = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, render, TRUE);
    gtk_tree_view_column_add_attribute(col, render, "text", COL_TITLE);

    /* Establish the column data types. */
    GtkListStore* list = GTK_LIST_STORE(gtk_tree_view_get_model(view));

    /* Initialize from defined launchtaskbar buttons. */
    GList *children = gtk_container_get_children(GTK_CONTAINER(ltbp->lb_icon_grid));
    GList *l;
    for (l = children; l != NULL; l = l->next)
    {
        LaunchButton * btn = (LaunchButton *) l->data;
        GdkPixbuf * pix;
        GtkTreeIter it;
        if (launch_button_get_settings(btn) == NULL) /* bootstrap button */
            continue;
        gtk_list_store_append(list, &it);
        pix = fm_pixbuf_from_icon(launch_button_get_icon(btn), PANEL_ICON_SIZE);
        gtk_list_store_set(list, &it,
                           COL_ICON, pix,
                           COL_TITLE, launch_button_get_disp_name(btn),
                           COL_BTN, btn,
                           -1);
        g_object_unref(pix);
    }
    g_list_free(children);
    g_object_set_data(G_OBJECT(dlg), "defined_view", view);
}

static void  plugin_set_expand_status(LaunchTaskBarPlugin *ltbp, gboolean expand_new)
{
    gboolean     old_expand, fill;
    guint        padding;
    GtkPackType  pack_type;
    GtkWidget   *box = gtk_widget_get_parent(ltbp->plugin);
    g_return_if_fail(box);
    gtk_box_query_child_packing(GTK_BOX(box), ltbp->plugin, &old_expand, &fill, &padding, &pack_type);
    gtk_box_set_child_packing(GTK_BOX(box), ltbp->plugin, expand_new, fill, padding, pack_type);
}

static void set_config_visibility(LaunchTaskBarPlugin *ltbp)
{
    switch (ltbp->mode) {
    default:
    case LAUNCHTASKBAR:
        gtk_widget_set_visible(ltbp->p_notebook_page_launch, TRUE);
        gtk_widget_set_visible(ltbp->p_notebook_page_task, TRUE);
        gtk_notebook_set_show_tabs(ltbp->p_notebook, TRUE);
        break;
    case TASKBAR:
        gtk_widget_set_visible(ltbp->p_notebook_page_launch, FALSE);
        gtk_widget_set_visible(ltbp->p_notebook_page_task, TRUE);
        gtk_notebook_set_show_tabs(ltbp->p_notebook, FALSE);
        break;
    case LAUNCHBAR:
        gtk_widget_set_visible(ltbp->p_notebook_page_launch, TRUE);
        gtk_widget_set_visible(ltbp->p_notebook_page_task, FALSE);
        gtk_notebook_set_show_tabs(ltbp->p_notebook, FALSE);
    }
}

static void on_combobox_mode_changed(GtkComboBox *p_combobox, gpointer p_data)
{
    LaunchTaskBarPlugin *ltbp = p_data;
    int new_mode = gtk_combo_box_get_active(GTK_COMBO_BOX(p_combobox));

    if (new_mode < 0 || new_mode == ltbp->mode) /* no change was made */
        return;

    ltbp->mode = new_mode;

    set_config_visibility(ltbp);

    switch (ltbp->mode) {
    case LAUNCHBAR:
        if (ltbp->tb_icon_grid)
            gtk_widget_set_visible(ltbp->tb_icon_grid, FALSE);
        launchtaskbar_constructor_launch(ltbp);
        plugin_set_expand_status(ltbp, FALSE);
        gtk_widget_set_name(ltbp->plugin, "launchbar");
        break;
    case TASKBAR:
        gtk_widget_set_visible(ltbp->lb_icon_grid, FALSE);
        launchtaskbar_constructor_task(ltbp);
        plugin_set_expand_status(ltbp, TRUE);
        gtk_widget_set_name(ltbp->plugin, "taskbar");
        break;
    default:
        ltbp->mode = LAUNCHTASKBAR;
    case LAUNCHTASKBAR:
        launchtaskbar_constructor_launch(ltbp);
        launchtaskbar_constructor_task(ltbp);
        plugin_set_expand_status(ltbp, TRUE);
        gtk_widget_set_name(ltbp->plugin, "launchtaskbar");
        break;
    }

    config_group_set_int(ltbp->settings, "LaunchTaskBarMode", ltbp->mode);
}

static void on_checkbutton_show_tooltips_toggled(GtkToggleButton *p_togglebutton, gpointer p_data)
{
    LaunchTaskBarPlugin *ltbp = (LaunchTaskBarPlugin *)p_data;
    ltbp->flags.tooltips = gtk_toggle_button_get_active(p_togglebutton);
    //g_print("\nltbp->flags.tooltips upd\n");
    config_group_set_int(ltbp->settings, "tooltips", ltbp->flags.tooltips);
    taskbar_apply_configuration(ltbp);
}

static void on_checkbutton_icons_only_toggled(GtkToggleButton *p_togglebutton, gpointer p_data)
{
    LaunchTaskBarPlugin *ltbp = (LaunchTaskBarPlugin *)p_data;
    ltbp->flags.icons_only = gtk_toggle_button_get_active(p_togglebutton);
    //g_print("\ntb->flags.icons_only upd\n");
    config_group_set_int(ltbp->settings, "IconsOnly", ltbp->flags.icons_only);
    taskbar_apply_configuration(ltbp);
}

static void on_checkbutton_flat_buttons_toggled(GtkToggleButton *p_togglebutton, gpointer p_data)
{
    LaunchTaskBarPlugin *ltbp = (LaunchTaskBarPlugin *)p_data;
    ltbp->flags.flat_button = gtk_toggle_button_get_active(p_togglebutton);
    //g_print("\ntb->flags.flat_button upd\n");
    config_group_set_int(ltbp->settings, "FlatButton", ltbp->flags.flat_button);
    taskbar_apply_configuration(ltbp);
}

static void on_checkbutton_show_all_desks_toggled(GtkToggleButton *p_togglebutton, gpointer p_data)
{
    LaunchTaskBarPlugin *ltbp = (LaunchTaskBarPlugin *)p_data;
    ltbp->flags.show_all_desks = gtk_toggle_button_get_active(p_togglebutton);
    //g_print("\ntb->flags.show_all_desks upd\n");
    config_group_set_int(ltbp->settings, "ShowAllDesks", ltbp->flags.show_all_desks);
    taskbar_apply_configuration(ltbp);
}

static void on_checkbutton_same_monitor_only_toggled(GtkToggleButton *p_togglebutton, gpointer p_data)
{
    LaunchTaskBarPlugin *ltbp = (LaunchTaskBarPlugin *)p_data;
    ltbp->flags.same_monitor_only = gtk_toggle_button_get_active(p_togglebutton);
    //g_print("\ntb->flags.same_monitor_only upd\n");
    config_group_set_int(ltbp->settings, "SameMonitorOnly", ltbp->flags.same_monitor_only);
    taskbar_apply_configuration(ltbp);
}

static void on_checkbutton_disable_taskbar_upscale_toggled(GtkToggleButton *p_togglebutton, gpointer p_data)
{
    LaunchTaskBarPlugin *ltbp = (LaunchTaskBarPlugin *)p_data;
    ltbp->flags.disable_taskbar_upscale = gtk_toggle_button_get_active(p_togglebutton);
    //g_print("\ntb->flags.disable_taskbar_upscale upd\n");
    config_group_set_int(ltbp->settings, "DisableUpscale", ltbp->flags.disable_taskbar_upscale);
    taskbar_apply_configuration(ltbp);
}

static void on_checkbutton_use_smaller_icons_toggled(GtkToggleButton *p_togglebutton, gpointer p_data)
{
    LaunchTaskBarPlugin *ltbp = (LaunchTaskBarPlugin *)p_data;
    ltbp->flags.use_smaller_icons = gtk_toggle_button_get_active(p_togglebutton);
    config_group_set_int(ltbp->settings, "UseSmallerIcons", ltbp->flags.use_smaller_icons);
    taskbar_apply_configuration(ltbp);
}

static void on_checkbutton_mouse_wheel_toggled(GtkToggleButton *p_togglebutton, gpointer p_data)
{
    LaunchTaskBarPlugin *ltbp = (LaunchTaskBarPlugin *)p_data;
    ltbp->flags.use_mouse_wheel = gtk_toggle_button_get_active(p_togglebutton);
    //g_print("\ntb->flags.use_mouse_wheel upd\n");
    config_group_set_int(ltbp->settings, "UseMouseWheel", ltbp->flags.use_mouse_wheel);
    taskbar_apply_configuration(ltbp);
}

static void on_checkbutton_urgency_hint_toggled(GtkToggleButton *p_togglebutton, gpointer p_data)
{
    LaunchTaskBarPlugin *ltbp = (LaunchTaskBarPlugin *)p_data;
    ltbp->flags.use_urgency_hint = gtk_toggle_button_get_active(p_togglebutton);
    //g_print("\ntb->flags.use_urgency_hint upd\n");
    config_group_set_int(ltbp->settings, "UseUrgencyHint", ltbp->flags.use_urgency_hint);
    taskbar_apply_configuration(ltbp);
    /* Start/stop blinking timeout if configured */
    if (ltbp->flags.use_urgency_hint)
        set_timer_on_task(ltbp);
    else
        reset_timer_on_task(ltbp);
}

static void on_checkbutton_grouped_tasks_toggled(GtkToggleButton *p_togglebutton, gpointer p_data)
{
    LaunchTaskBarPlugin *ltbp = (LaunchTaskBarPlugin *)p_data;
    ltbp->grouped_tasks = gtk_toggle_button_get_active(p_togglebutton);
    //g_print("\ntb->grouped_tasks upd\n");
    config_group_set_int(ltbp->settings, "GroupedTasks", ltbp->grouped_tasks);
    taskbar_apply_configuration(ltbp);
    if (ltbp->grouped_tasks)
    {
        gboolean changed = FALSE;
        GList *children, *this, *l;
        int i = 0;

        children = gtk_container_get_children(GTK_CONTAINER(ltbp->tb_icon_grid));
        /* merge buttons with the same class into first of that class */
        while ((this = g_list_nth(children, i++)))
        {
            for (l = this->next; l; l = l->next)
                if (task_button_merge(this->data, l->data))
                    changed = TRUE;
            if (changed)
            {
                /* some button was consumed, need to reload buttons list */
                g_list_free(children);
                children = gtk_container_get_children(GTK_CONTAINER(ltbp->tb_icon_grid));
                changed = FALSE;
            }
        }
        g_list_free(children);
    }
    else
    {
        TaskButton *old_btn, *new_btn;
        GList *children, *this;
        int i;

        children = gtk_container_get_children(GTK_CONTAINER(ltbp->tb_icon_grid));
        /* split each button starting from last one */
        for (this = g_list_last(children); this; this = this->prev)
        {
            old_btn = this->data;
            i = panel_icon_grid_get_child_position(PANEL_ICON_GRID(ltbp->tb_icon_grid),
                                                   GTK_WIDGET(old_btn));
            while ((new_btn = task_button_split(old_btn)))
            {
                /* insert rest before old_btn */
                taskbar_add_task_button(ltbp, new_btn);
                panel_icon_grid_reorder_child(PANEL_ICON_GRID(ltbp->tb_icon_grid),
                                              GTK_WIDGET(new_btn), i);
                /* continue split with the rest */
                old_btn = new_btn;
            }
        }
        g_list_free(children);
    }
}

static void on_spinbutton_max_width_value_changed(GtkSpinButton *p_spinbutton, gpointer p_data)
{
    LaunchTaskBarPlugin *ltbp = (LaunchTaskBarPlugin *)p_data;
    ltbp->task_width_max = gtk_spin_button_get_value(p_spinbutton);
    //g_print("\ntb->task_width_max upd\n");
    config_group_set_int(ltbp->settings, "MaxTaskWidth", ltbp->task_width_max);
    taskbar_apply_configuration(ltbp);
}

static void on_spinbutton_spacing_value_changed(GtkSpinButton *p_spinbutton, gpointer p_data)
{
    LaunchTaskBarPlugin *ltbp = (LaunchTaskBarPlugin *)p_data;
    ltbp->spacing = gtk_spin_button_get_value(p_spinbutton);
    //g_print("\ntb->spacing upd\n");
    config_group_set_int(ltbp->settings, "spacing", ltbp->spacing);
    taskbar_apply_configuration(ltbp);
}

static gboolean on_defined_view_button_press_event(GtkWidget *p_widget, GdkEventButton *p_event, gpointer p_data)
{
    LaunchTaskBarPlugin *lb = (LaunchTaskBarPlugin *)p_data;
    if(p_event->button == 1)
    {
        if(p_event->type == GDK_2BUTTON_PRESS)
        {
            gtk_button_clicked(GTK_BUTTON(lb->p_button_remove));
        }
    }
    return FALSE;
}

static void on_defined_view_cursor_changed(GtkTreeView *p_treeview, gpointer p_data)
{
    gboolean  label_set = FALSE;
    LaunchTaskBarPlugin *lb = (LaunchTaskBarPlugin *)p_data;
    GtkTreeIter  tree_iter_sel;
    GtkTreeModel* p_treemodel = gtk_tree_view_get_model(p_treeview);
    GtkTreeSelection *p_treeselection = gtk_tree_view_get_selection(p_treeview);
    if(gtk_tree_selection_get_selected(p_treeselection,
                                       (GtkTreeModel **)(&p_treemodel),
                                       &tree_iter_sel))
    {
        LaunchButton * p_btn;
        FmFileInfo *fi;
        gtk_tree_model_get(p_treemodel, &tree_iter_sel, COL_BTN, &p_btn, -1);
        if ((p_btn != NULL) && ((fi = launch_button_get_file_info(p_btn)) != NULL))
        {
            GString *p_gstring = g_string_new("");
            g_string_printf(p_gstring, "<i>%s</i>", fm_file_info_get_disp_name(fi));
            gtk_label_set_markup(GTK_LABEL(lb->p_label_def_app_exec), p_gstring->str);
            g_string_free(p_gstring, TRUE/*free also gstring->str*/);
            label_set = TRUE;
        }
    }
    gtk_widget_set_visible(lb->p_label_def_app_exec, label_set);
    gtk_widget_set_sensitive(lb->p_button_remove, label_set);
}

static void on_menu_view_cursor_changed(GtkTreeView *p_treeview, gpointer p_data)
{
    gboolean  label_set = FALSE;
    LaunchTaskBarPlugin *lb = (LaunchTaskBarPlugin *)p_data;
    GAppInfo *app = fm_app_menu_view_dup_selected_app(p_treeview);

    if (app)
    {
        GString *p_gstring = g_string_new("");
        if (g_app_info_get_description(app))
            g_string_printf(p_gstring, "<i>%s</i>", g_app_info_get_description(app));
        else
            g_string_printf(p_gstring, "<i>%s</i>", g_app_info_get_name(app));
        gtk_label_set_markup(GTK_LABEL(lb->p_label_menu_app_exec), p_gstring->str);
        g_string_free(p_gstring, TRUE/*free also gstring->str*/);
        label_set = TRUE;
    }
    gtk_widget_set_visible(lb->p_label_menu_app_exec, label_set);
    gtk_widget_set_sensitive(lb->p_button_add, label_set);
}

static void on_menu_view_row_activated(GtkTreeView *tree_view, GtkTreePath *path,
                                       GtkTreeViewColumn *column,
                                       LaunchTaskBarPlugin *ltbp)
{
    _launchbar_configure_add(tree_view, ltbp);
}

/* FIXME: add support for global hotkeys for launchers */

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *launchtaskbar_configure(LXPanel *panel, GtkWidget *p)
{
    LaunchTaskBarPlugin *ltbp = lxpanel_plugin_get_data(p);

    {
        GtkWidget *dlg, *btn, *defined_view, *menu_view, *menu_view_window;
        GtkBuilder *builder = gtk_builder_new();
        GObject *object;

        gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/launchtaskbar.ui", NULL);
        dlg = (GtkWidget *)gtk_builder_get_object(builder, "dlg");
        panel_apply_icon(GTK_WINDOW(dlg));

        defined_view = (GtkWidget *)gtk_builder_get_object(builder, "defined_view");
        menu_view_window = (GtkWidget*)gtk_builder_get_object(builder, "menu_view_window");
        if (menu_view_window == NULL) /* fallback for old glade file */
        {
            menu_view_window = (GtkWidget*)gtk_builder_get_object(builder, "scroll2");
            gtk_widget_destroy(gtk_bin_get_child(GTK_BIN(menu_view_window)));
        }
        menu_view = GTK_WIDGET(fm_app_menu_view_new());
        gtk_container_add(GTK_CONTAINER(menu_view_window), menu_view);
        gtk_widget_show(menu_view);
        ltbp->p_label_def_app_exec = (GtkWidget*)gtk_builder_get_object(builder, "label_def_app_exec");
        ltbp->p_label_menu_app_exec = (GtkWidget*)gtk_builder_get_object(builder, "label_menu_app_exec");

        /* Connect signals. */
        ltbp->p_button_add = (GtkWidget *)gtk_builder_get_object(builder, "button_add");
        g_signal_connect(ltbp->p_button_add, "clicked", G_CALLBACK(launchbar_configure_add_button), ltbp);

        ltbp->p_button_remove = (GtkWidget *)gtk_builder_get_object(builder, "button_remove");
        g_signal_connect(ltbp->p_button_remove, "clicked", G_CALLBACK(launchbar_configure_remove_button), ltbp);

        btn = (GtkWidget *)gtk_builder_get_object(builder, "button_up");
        g_signal_connect(btn, "clicked", G_CALLBACK(launchbar_configure_move_up_button), ltbp);

        btn = (GtkWidget *)gtk_builder_get_object(builder, "button_down");
        g_signal_connect(btn, "clicked", G_CALLBACK(launchbar_configure_move_down_button), ltbp);

        /* FIXME: add a button 'New' with launcher creation dialog */

        g_signal_connect(defined_view, "button-press-event", G_CALLBACK(on_defined_view_button_press_event), ltbp);
        g_signal_connect(defined_view, "cursor-changed", G_CALLBACK(on_defined_view_cursor_changed), ltbp);
        g_signal_connect(menu_view, "cursor-changed", G_CALLBACK(on_menu_view_cursor_changed), ltbp);
        g_signal_connect(menu_view, "row-activated", G_CALLBACK(on_menu_view_row_activated), ltbp);

        ltbp->p_notebook = GTK_NOTEBOOK(gtk_builder_get_object(builder, "notebook"));
        ltbp->p_notebook_page_launch = gtk_notebook_get_nth_page(ltbp->p_notebook, 0);
        ltbp->p_notebook_page_task = gtk_notebook_get_nth_page(ltbp->p_notebook, 1);
        set_config_visibility(ltbp);
	object = gtk_builder_get_object(builder, "combobox_mode");
        gtk_combo_box_set_active(GTK_COMBO_BOX(object), ltbp->mode);
	g_signal_connect(object, "changed",
			G_CALLBACK(on_combobox_mode_changed), ltbp);

#define SETUP_TOGGLE_BUTTON(button,member) \
        object = gtk_builder_get_object(builder, #button); \
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(object), ltbp->flags.member); \
        g_signal_connect(object, "toggled", G_CALLBACK(on_##button##_toggled), ltbp)

        SETUP_TOGGLE_BUTTON(checkbutton_show_tooltips, tooltips);
        SETUP_TOGGLE_BUTTON(checkbutton_icons_only, icons_only);
        SETUP_TOGGLE_BUTTON(checkbutton_flat_buttons, flat_button);
        SETUP_TOGGLE_BUTTON(checkbutton_show_all_desks, show_all_desks);
        SETUP_TOGGLE_BUTTON(checkbutton_same_monitor_only, same_monitor_only);
        SETUP_TOGGLE_BUTTON(checkbutton_mouse_wheel, use_mouse_wheel);
        SETUP_TOGGLE_BUTTON(checkbutton_urgency_hint, use_urgency_hint);
        SETUP_TOGGLE_BUTTON(checkbutton_disable_taskbar_upscale, disable_taskbar_upscale);
        //SETUP_TOGGLE_BUTTON(checkbutton_use_smaller_icons, use_smaller_icons);
#undef SETUP_TOGGLE_BUTTON
        object = gtk_builder_get_object(builder, "checkbutton_grouped_tasks");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(object), ltbp->grouped_tasks);
        g_signal_connect(object, "toggled", G_CALLBACK(on_checkbutton_grouped_tasks_toggled), ltbp);
        /* FIXME: for transitional period, turn into SETUP_TOGGLE_BUTTON later */
        object = gtk_builder_get_object(builder, "checkbutton_use_smaller_icons");
        if (object)
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(object), ltbp->flags.use_smaller_icons);
            g_signal_connect(object, "toggled", G_CALLBACK(on_checkbutton_use_smaller_icons_toggled), ltbp);
        }

#define SETUP_SPIN_BUTTON(button,member) \
        object = gtk_builder_get_object(builder, #button); \
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(object), ltbp->member); \
        g_signal_connect(object, "value-changed", \
                         G_CALLBACK(on_##button##_value_changed), ltbp)

        SETUP_SPIN_BUTTON(spinbutton_max_width, task_width_max);
        SETUP_SPIN_BUTTON(spinbutton_spacing, spacing);
#undef SETUP_SPIN_BUTTON

        ltbp->config_dlg = dlg;

        /* Initialize the tree view contents. */
        launchbar_configure_initialize_list(ltbp, dlg, GTK_TREE_VIEW(defined_view));
        g_object_set_data(G_OBJECT(dlg), "menu_view", menu_view);

        gtk_widget_set_visible(ltbp->p_label_menu_app_exec, FALSE);
        gtk_widget_set_visible(ltbp->p_label_def_app_exec, FALSE);
        gtk_widget_set_sensitive(ltbp->p_button_add, FALSE);
        gtk_widget_set_sensitive(ltbp->p_button_remove, FALSE);
        if (ltbp->fixed_mode)
        {
            object = gtk_builder_get_object(builder, "hbox_mode");
            if (object)
                gtk_widget_destroy(GTK_WIDGET(object));
            if (ltbp->mode == LAUNCHBAR)
                gtk_window_set_title(GTK_WINDOW(ltbp->config_dlg),
                                     _("Application Launch Bar"));
            else
                gtk_window_set_title(GTK_WINDOW(ltbp->config_dlg),
                                     _("Task Bar (Window List)"));
        }

        g_object_unref(builder);
    }
    return ltbp->config_dlg;
}

/* Callback when panel configuration changes. */
static void launchtaskbar_panel_configuration_changed(LXPanel *panel, GtkWidget *p)
{
    /* Set orientation into the icon grid. */
    LaunchTaskBarPlugin *ltbp = lxpanel_plugin_get_data(p);
    int new_icon_size = panel_get_icon_size(panel);
    int height = panel_get_height(panel);

    if (ltbp->lb_built)
        panel_icon_grid_set_geometry(PANEL_ICON_GRID(ltbp->lb_icon_grid),
                                     panel_get_orientation(panel),
                                     new_icon_size, new_icon_size,
                                     3, 0, height);

    /* Redraw all the labels.  Icon size or font color may have changed. */
    if (ltbp->tb_built)
    {
        ltbp->icon_size = new_icon_size;
        panel_icon_grid_set_geometry(PANEL_ICON_GRID(ltbp->tb_icon_grid),
            panel_get_orientation(panel),
            ((ltbp->flags.icons_only) ? ltbp->icon_size + ICON_ONLY_EXTRA : ltbp->task_width_max),
            ((ltbp->flags.icons_only) ? ltbp->icon_size + ICON_ONLY_EXTRA : ltbp->icon_size + ICON_BUTTON_TRIM),
            ltbp->spacing, 0, height);
        taskbar_reset_menu(ltbp);
        taskbar_redraw(ltbp);
    }
}

static gboolean launchtaskbar_control(GtkWidget *p, const char *cmd)
{
    LaunchTaskBarPlugin *ltbp = lxpanel_plugin_get_data(p);

    if (ltbp->mode == LAUNCHBAR || ltbp->mode == LAUNCHTASKBAR)
    {
        if (strncmp(cmd, "add ", 4) == 0)
        {
            config_setting_t *s;

            s = config_group_add_subgroup(ltbp->settings, "Button");
            config_group_set_string(s, "id", &cmd[4]);
            if (launchbutton_constructor(ltbp, s))
            {
                launchbar_remove_bootstrap(ltbp);
                lxpanel_config_save(ltbp->panel);
                return TRUE;
            }
            else
                config_setting_destroy(s);
        }
    }
    return FALSE;
}

/* Redraw all tasks in the taskbar. */
static void taskbar_redraw(LaunchTaskBarPlugin * tb)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(tb->tb_icon_grid));
    GList *l;
    guint mon = panel_get_monitor(tb->panel);
    guint icon_size = panel_get_icon_size(tb->panel);

    if (tb->flags.use_smaller_icons)
        icon_size -= 4;
    for (l = children; l; l = l->next)
        task_button_update(l->data, tb->current_desktop, tb->number_of_desktops,
                           mon, icon_size, tb->flags);
    g_list_free(children);
}

/* Determine if a task should be visible given its NET_WM_STATE. */
static gboolean accept_net_wm_state(NetWMState * nws)
{
    return ( ! (nws->skip_taskbar));
}

/* Determine if a task should be visible given its NET_WM_WINDOW_TYPE. */
static gboolean accept_net_wm_window_type(NetWMWindowType * nwwt)
{
    return ( ! ((nwwt->desktop) || (nwwt->dock) || (nwwt->splash)));
}

/* Set the class associated with a task. */
static char *task_get_class(Window win)
{
    /* Read the WM_CLASS property. */
    XClassHint ch;
    ch.res_name = NULL;
    ch.res_class = NULL;
    char *res_class = NULL;
    XGetClassHint(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), win, &ch);

    /* If the res_name was returned, free it.  We make no use of it at this time. */
    if (ch.res_name != NULL)
    {
        XFree(ch.res_name);
    }

    /* If the res_class was returned, process it.
     * This identifies the application that created the window and is the basis for taskbar grouping. */
    if (ch.res_class != NULL)
    {
        /* Convert the class to UTF-8 and enter it in the class table. */
        res_class = g_locale_to_utf8(ch.res_class, -1, NULL, NULL, NULL);
        XFree(ch.res_class);
    }
    return res_class;
}

/* Look up a task in the task list. */
static TaskButton *task_lookup(LaunchTaskBarPlugin * tb, Window win)
{
    TaskButton *task = NULL;
    GList *children = gtk_container_get_children(GTK_CONTAINER(tb->tb_icon_grid));
    GList *l;

    for (l = children; l; l = l->next)
        if (task_button_has_window(l->data, win))
        {
            task = l->data;
            break;
        }
    g_list_free(children);
    return task;
}


#ifndef DISABLE_MENU
static void  on_menuitem_lock_tbp_clicked(GtkWidget * widget, LaunchTaskBarPlugin * tb);
static void  on_menuitem_unlock_tbp_clicked(GtkWidget * widget, LaunchTaskBarPlugin * tb);
static void  on_menuitem_new_instance_clicked(GtkWidget * widget, LaunchTaskBarPlugin * tb);
#endif

static void on_task_menu_built(GtkWidget *unused, GtkMenu *menu, LaunchTaskBarPlugin *tb)
{
#ifndef DISABLE_MENU
    /* add callbacks for task-to-launcher items with weak pointers */
    void (*_m_add)(GtkMenuShell *self, GtkWidget* child);

    if (panel_is_at_bottom(tb->panel))
        _m_add = gtk_menu_shell_append;
    else
        _m_add = gtk_menu_shell_prepend;

    tb->p_menuitem_lock_tbp = gtk_menu_item_new_with_mnemonic(_("A_dd to Launcher"));
    g_object_add_weak_pointer(G_OBJECT(menu), (void **)&tb->p_menuitem_lock_tbp);
    tb->p_menuitem_unlock_tbp = gtk_menu_item_new_with_mnemonic(_("Rem_ove from Launcher"));
    g_object_add_weak_pointer(G_OBJECT(menu), (void **)&tb->p_menuitem_lock_tbp);
    tb->p_menuitem_new_instance = gtk_menu_item_new_with_mnemonic(_("_New Instance"));
    g_object_add_weak_pointer(G_OBJECT(menu), (void **)&tb->p_menuitem_lock_tbp);
    tb->p_menuitem_separator = gtk_separator_menu_item_new();
    _m_add(GTK_MENU_SHELL(menu), tb->p_menuitem_separator);
    _m_add(GTK_MENU_SHELL(menu), tb->p_menuitem_lock_tbp);
    _m_add(GTK_MENU_SHELL(menu), tb->p_menuitem_unlock_tbp);
    _m_add(GTK_MENU_SHELL(menu), tb->p_menuitem_new_instance);
    g_signal_connect(G_OBJECT(tb->p_menuitem_lock_tbp), "activate", (GCallback)on_menuitem_lock_tbp_clicked, tb);
    g_signal_connect(G_OBJECT(tb->p_menuitem_unlock_tbp), "activate", (GCallback)on_menuitem_unlock_tbp_clicked, tb);
    g_signal_connect(G_OBJECT(tb->p_menuitem_new_instance), "activate", (GCallback)on_menuitem_new_instance_clicked, tb);
#endif
}

static void on_task_menu_target_set(TaskButton *btn, gulong win, LaunchTaskBarPlugin *ltbp)
{
#ifndef DISABLE_MENU
            if(ltbp->mode == LAUNCHTASKBAR)
            {
                FmPath *path = f_find_menu_launchbutton_recursive(win, ltbp);
                LaunchButton *btn = launchbar_exec_bin_exists(ltbp, path);
                /* FIXME: shouldn't we make file info at task button creation? */
#ifdef DEBUG
                g_print("\nTB '%s' right-click, in LB: %c\n", tk->exec_bin, btn != NULL ? 'Y':'N');
#endif
                if(btn != NULL)
                {
                    gtk_widget_set_visible(ltbp->p_menuitem_lock_tbp, FALSE);
                    gtk_widget_set_visible(ltbp->p_menuitem_unlock_tbp, TRUE);
                    gtk_widget_set_visible(ltbp->p_menuitem_new_instance, TRUE);
                }
                else
                {
                    gtk_widget_set_visible(ltbp->p_menuitem_lock_tbp, path != NULL);
                    gtk_widget_set_visible(ltbp->p_menuitem_unlock_tbp, FALSE);
                    gtk_widget_set_visible(ltbp->p_menuitem_new_instance, path != NULL);
                }
                gtk_widget_set_visible(ltbp->p_menuitem_separator, TRUE);
                if (ltbp->path)
                    fm_path_unref(ltbp->path);
                ltbp->path = path;
            }
            else
            {
                gtk_widget_set_visible(ltbp->p_menuitem_lock_tbp, FALSE);
                gtk_widget_set_visible(ltbp->p_menuitem_unlock_tbp, FALSE);
                gtk_widget_set_visible(ltbp->p_menuitem_new_instance, FALSE);
                gtk_widget_set_visible(ltbp->p_menuitem_separator, FALSE);
            }
#endif
}

/* Handler for "drag-motion" timeout. */
static gboolean taskbar_button_drag_motion_timeout(LaunchTaskBarPlugin * tb)
{
    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;

    task_button_raise_window(PANEL_TASK_BUTTON(tb->dnd_delay_task), CurrentTime);
    tb->dnd_delay_timer = 0;
    return FALSE;
}

/* Handler for "drag-motion" event from taskbar button. */
static gboolean taskbar_button_drag_motion(GtkWidget * widget, GdkDragContext * drag_context, gint x, gint y, guint time, LaunchTaskBarPlugin * tb)
{
    GtkWidget * drag_source = gtk_drag_get_source_widget(drag_context);
    if (drag_source != NULL && gtk_widget_get_parent(drag_source) == gtk_widget_get_parent(widget))
    {
        tb->dnd_task_moving = TRUE;
        gdk_drag_status(drag_context, GDK_ACTION_MOVE, time);
    }
    else
    {
        /* Prevent excessive motion notification. */
        if (tb->dnd_delay_timer == 0)
            tb->dnd_delay_timer = g_timeout_add(DRAG_ACTIVE_DELAY, (GSourceFunc) taskbar_button_drag_motion_timeout, tb);

        if (tb->dnd_delay_task != widget)
        {
            if (tb->dnd_delay_task)
                g_object_remove_weak_pointer(G_OBJECT(tb->dnd_delay_task), (gpointer *)&tb->dnd_delay_task);
            tb->dnd_delay_task = widget;
            g_object_add_weak_pointer(G_OBJECT(widget), (gpointer *)&tb->dnd_delay_task);
        }

        gdk_drag_status(drag_context, 0, time);
    }
    return TRUE;
}

/* Handler for "drag-drop" event from taskbar button. */
static gboolean taskbar_button_drag_drop(GtkWidget * widget, GdkDragContext * drag_context, gint x, gint y, guint time, LaunchTaskBarPlugin * tb)
{
    GtkWidget * drag_source = gtk_drag_get_source_widget(drag_context);

    tb->dnd_task_moving = FALSE;
    /* Cancel the timer if set. */
    if (tb->dnd_delay_timer != 0)
    {
        g_source_remove(tb->dnd_delay_timer);
        tb->dnd_delay_timer = 0;
    }
    if (drag_source != NULL && gtk_widget_get_parent(drag_source) == gtk_widget_get_parent(widget))
    {
        if (drag_source != widget)
        {
            PanelIconGrid *ig = PANEL_ICON_GRID(tb->tb_icon_grid);
            gint i = panel_icon_grid_get_child_position(ig, widget);
            panel_icon_grid_reorder_child(ig, drag_source, i);
        }
        gtk_drag_finish(drag_context, TRUE, TRUE, time);
        return TRUE;
    }

    return FALSE;
}

/* Handler for "drag-leave" event from taskbar button. */
static void taskbar_button_drag_leave(GtkWidget * widget, GdkDragContext * drag_context, guint time, LaunchTaskBarPlugin * tb)
{
    /* Cancel the timer if set. */
    if (tb->dnd_delay_timer != 0)
    {
        g_source_remove(tb->dnd_delay_timer);
        tb->dnd_delay_timer = 0;
    }
    return;
}

/* Handler for "enter" event from taskbar button.  This indicates that the cursor position has entered the button. */
static void taskbar_button_enter(GtkWidget * widget, GdkEvent *event, LaunchTaskBarPlugin * tb)
{
    tb->dnd_task_moving = FALSE;
}

/* Handler for "button-release-event" event from taskbar button. */
static gboolean taskbar_button_release_event(GtkWidget * widget, GdkEventButton * event, LaunchTaskBarPlugin * tb)
{
    if (tb->dnd_task_moving)
        /* SF bug#731: don't process button release with DND. Also if button was
           released outside of widget but DND wasn't activated: this might happen
           if drag started at edge of button so drag treshold wasn't reached. */
        return TRUE;
    return FALSE;
}

enum {
    TARGET_TASK_BUTTON
};

static GtkTargetEntry task_button_target_list[] = {
    { "task_button", GTK_TARGET_SAME_APP, TARGET_TASK_BUTTON }
};

static guint task_button_n_targets = G_N_ELEMENTS(task_button_target_list);

/* Build graphic elements needed for a task button. */
static void taskbar_add_task_button(LaunchTaskBarPlugin * tb, TaskButton * task)
{
    /* Allocate a toggle button as the top level widget. */
    gtk_container_add(GTK_CONTAINER(tb->tb_icon_grid), GTK_WIDGET(task));

    /* Connect signals to the button. */
    /* handle menu callbacks */
    g_signal_connect(G_OBJECT(task), "menu-built",
                     (GCallback)on_task_menu_built, tb);
    g_signal_connect(G_OBJECT(task), "menu-target-set",
                     (GCallback)on_task_menu_target_set, tb);
    /* handle drag & drop on task buttons */
    gtk_drag_dest_set(GTK_WIDGET(task), 0, NULL, 0, 0);
    gtk_drag_source_set(GTK_WIDGET(task), GDK_BUTTON1_MASK,
                        task_button_target_list, task_button_n_targets,
                        GDK_ACTION_MOVE);
    g_signal_connect(G_OBJECT(task), "drag-motion",
                     G_CALLBACK(taskbar_button_drag_motion), tb);
    g_signal_connect(G_OBJECT(task), "drag-leave",
                     G_CALLBACK(taskbar_button_drag_leave), tb);
    g_signal_connect(G_OBJECT(task), "drag-drop",
                     G_CALLBACK(taskbar_button_drag_drop), tb);
    g_signal_connect(task, "button-release-event",
                     G_CALLBACK(taskbar_button_release_event), tb);
    g_signal_connect_after(G_OBJECT(task), "enter-notify-event",
                           G_CALLBACK(taskbar_button_enter), tb);
}

/* add win to tb, using list of task buttons */
static void taskbar_add_new_window(LaunchTaskBarPlugin * tb, Window win, GList *list)
{
    gchar *res_class = task_get_class(win);
    TaskButton *task;

    if (!tb->grouped_tasks || res_class == NULL)
        list = NULL;
    else for (; list; list = list->next)
        if (task_button_add_window(list->data, win, res_class))
            break;
    if (list != NULL)
        return; /* some button accepted it, done */

    task = task_button_new(win, tb->current_desktop, tb->number_of_desktops,
                           tb->panel, res_class, tb->flags);
    taskbar_add_task_button(tb, task);
}

/*****************************************************
 * handlers for NET actions                          *
 *****************************************************/

/* Handler for "client-list" event from root window listener. */
static void taskbar_net_client_list(GtkWidget * widget, LaunchTaskBarPlugin * tb)
{
    LaunchTaskBarPlugin *ltbp = tb;
    if(ltbp->mode == LAUNCHBAR) return;

    /* Get the NET_CLIENT_LIST property. */
    int client_count;
    Window * client_list = get_xaproperty(GDK_ROOT_WINDOW(), a_NET_CLIENT_LIST, XA_WINDOW, &client_count);
    if (client_list != NULL)
    {
        GList *children = gtk_container_get_children(GTK_CONTAINER(tb->tb_icon_grid)), *l;
        /* Remove windows from the task list that are not present in the NET_CLIENT_LIST. */
        for (l = children; l; l = l->next)
            task_button_update_windows_list(l->data, client_list, client_count);
        g_list_free(children);
        children = gtk_container_get_children(GTK_CONTAINER(tb->tb_icon_grid));
        /* Loop over client list, correlating it with task list. */
        int i;
        for (i = 0; i < client_count; i++)
        {
            /* Search for the window in the task list.  Set up context to do an insert right away if needed. */
            for (l = children; l; l = l->next)
            {
                if (task_button_has_window(l->data, client_list[i]))
                    break;
            }

            /* Task is not in task list. */
            if (l == NULL)
            {
                /* Evaluate window state and window type to see if it should be in task list. */
                NetWMWindowType nwwt;
                NetWMState nws;
                get_net_wm_state(client_list[i], &nws);
                get_net_wm_window_type(client_list[i], &nwwt);
                if ((accept_net_wm_state(&nws))
                && (accept_net_wm_window_type(&nwwt)))
                {
                    /* Allocate and initialize new task structure. */
                    taskbar_add_new_window(tb, client_list[i], children);
                    g_list_free(children);
                    children = gtk_container_get_children(GTK_CONTAINER(tb->tb_icon_grid));
                }
            }
        }
        g_list_free(children);
        XFree(client_list);
    }

    else /* clear taskbar */
        gtk_container_foreach(GTK_CONTAINER(tb->tb_icon_grid),
                              (GtkCallback)gtk_widget_destroy, NULL);
}

/* Handler for "current-desktop" event from root window listener. */
static void taskbar_net_current_desktop(GtkWidget * widget, LaunchTaskBarPlugin * tb)
{
    LaunchTaskBarPlugin *ltbp = tb;
    if(ltbp->mode == LAUNCHBAR) return;

    /* Store the local copy of current desktops.  Redisplay the taskbar. */
    tb->current_desktop = get_net_current_desktop();
    taskbar_redraw(tb);
}

/* Handler for "number-of-desktops" event from root window listener. */
static void taskbar_net_number_of_desktops(GtkWidget * widget, LaunchTaskBarPlugin * tb)
{
    LaunchTaskBarPlugin *ltbp = tb;
    if(ltbp->mode == LAUNCHBAR) return;

    /* Store the local copy of number of desktops.  Recompute the popup menu and redisplay the taskbar. */
    tb->number_of_desktops = get_net_number_of_desktops();
    taskbar_reset_menu(tb);
    taskbar_redraw(tb);
}

/* Handler for "active-window" event from root window listener. */
static void taskbar_net_active_window(GtkWidget * widget, LaunchTaskBarPlugin * tb)
{
    LaunchTaskBarPlugin *ltbp = tb;
    if(ltbp->mode == LAUNCHBAR) return;

    /* Get the window that has focus. */
    Window * f = get_xaproperty(GDK_ROOT_WINDOW(), a_NET_ACTIVE_WINDOW, XA_WINDOW, 0);

    gtk_container_foreach(GTK_CONTAINER(tb->tb_icon_grid),
                          (GtkCallback)task_button_window_focus_changed, f);
    if (f != NULL)
        XFree(f);
}

/* Handle PropertyNotify event.
 * http://tronche.com/gui/x/icccm/
 * http://standards.freedesktop.org/wm-spec/wm-spec-1.4.html */
static void taskbar_property_notify_event(LaunchTaskBarPlugin *tb, XEvent *ev)
{
    /* State may be PropertyNewValue, PropertyDeleted. */
    if (((XPropertyEvent*) ev)->state == PropertyNewValue)
    {
        Atom at = ev->xproperty.atom;
        Window win = ev->xproperty.window;
        if (win != GDK_ROOT_WINDOW())
        {
            /* Look up task structure by X window handle. */
            TaskButton * tk = task_lookup(tb, win);
            if (tk != NULL)
            {
                /* Install an error handler that ignores BadWindow.
                 * We frequently get a PropertyNotify event on deleted windows. */
                XErrorHandler previous_error_handler = XSetErrorHandler(panel_handle_x_error_swallow_BadWindow_BadDrawable);

                /* Dispatch on atom. */
                if (at == a_NET_WM_STATE)
                {
                    /* Window changed EWMH state. */
                    NetWMState nws;
                    get_net_wm_state(win, &nws);
                    if ( ! accept_net_wm_state(&nws))
                        task_button_drop_window(tk, win, FALSE);
                    /* else
                        task_button_window_state_changed(tk, win, nws); */
                }
                else if (at == a_NET_WM_WINDOW_TYPE)
                {
                    /* Window changed EWMH window type. */
                    NetWMWindowType nwwt;
                    get_net_wm_window_type(win, &nwwt);
                    if ( ! accept_net_wm_window_type(&nwwt))
                        task_button_drop_window(tk, win, FALSE);
                }
                else if (at == XA_WM_CLASS && tb->grouped_tasks
                         && task_button_drop_window(tk, win, TRUE))
                {
                    GList *children = gtk_container_get_children(GTK_CONTAINER(tb->tb_icon_grid));
                    /* if Window was not single window of that class then
                       add it to another class or make another button */
                    taskbar_add_new_window(tb, win, children);
                    g_list_free(children);
                }
                else
                {
                    /* simply notify button, it will handle the event */
                    task_button_window_xprop_changed(tk, win, at);
                }

                XSetErrorHandler(previous_error_handler);
            }
        }
    }
}

/* Handle ConfigureNotify events */
static void taskbar_configure_notify_event(LaunchTaskBarPlugin * tb, XConfigureEvent * ev)
{
    /* If the same_monitor_only option is set and the window is on a different
       monitor than before, redraw the taskbar */
    TaskButton *task;

    if (ev->window != GDK_ROOT_WINDOW())
    {
        task = task_lookup(tb, ev->window);
        if (task)
        {
            /* Deleted windows seem to get ConfigureNotify events too. */
            XErrorHandler previous_error_handler = XSetErrorHandler(panel_handle_x_error_swallow_BadWindow_BadDrawable);

            /* Monitor might be changed so button might need update */
            task_button_window_reconfigured(task, ev->window);

            XSetErrorHandler(previous_error_handler);
        }
    }
}

/* GDK event filter. */
static GdkFilterReturn taskbar_event_filter(XEvent * xev, GdkEvent * event, LaunchTaskBarPlugin * tb)
{
    if (tb->mode == LAUNCHBAR)
        return GDK_FILTER_CONTINUE;

    if (xev->type == PropertyNotify)
        taskbar_property_notify_event(tb, xev);
    else if (xev->type == ConfigureNotify)
        taskbar_configure_notify_event(tb, &xev->xconfigure);

    return GDK_FILTER_CONTINUE;
}

#ifndef DISABLE_MENU
static void  on_menuitem_lock_tbp_clicked(GtkWidget * widget, LaunchTaskBarPlugin * tb)
{
    LaunchButton *btn;
    char *path;
    config_setting_t *settings;

    if (tb->path)
    {
        /* Create a button and add settings for it */
        path = fm_path_to_str(tb->path);
        /* g_debug("*** path '%s'",path); */
        settings = config_group_add_subgroup(tb->settings, "Button");
        config_group_set_string(settings, "id", path);
        g_free(path);
        btn = launch_button_new(tb->panel, tb->plugin, tb->path, settings);
        if (btn)
            gtk_container_add(GTK_CONTAINER(tb->lb_icon_grid), GTK_WIDGET(btn));
        else
            config_setting_destroy(settings);
        lxpanel_config_save(tb->panel);
    }
}

static void  on_menuitem_unlock_tbp_clicked(GtkWidget * widget, LaunchTaskBarPlugin * tb)
{
    launchbar_remove_launcher(tb, tb->path);
    launchbar_check_bootstrap(tb);
}

static void  on_menuitem_new_instance_clicked(GtkWidget * widget, LaunchTaskBarPlugin * tb)
{
    if (tb->path)
    {
        lxpanel_launch_path(tb->panel, tb->path);
    }
}
#endif

/* Handler for "window-manager-changed" event. */
static void taskbar_window_manager_changed(GdkScreen * screen, LaunchTaskBarPlugin * tb)
{
    /* Force re-evaluation of use_net_active. */
    GdkAtom net_active_atom = gdk_x11_xatom_to_atom(a_NET_ACTIVE_WINDOW);
    tb->flags.use_net_active = gdk_x11_screen_supports_net_wm_hint(tb->screen, net_active_atom);
    taskbar_redraw(tb);
}

/* Callback from configuration dialog mechanism to apply the configuration. */
static void taskbar_apply_configuration(LaunchTaskBarPlugin *ltbp)
{
    taskbar_redraw(ltbp);
}

static GtkWidget *launchbar_constructor(LXPanel *panel, config_setting_t *settings)
{
    return _launchtaskbar_constructor(panel, settings, LAUNCHBAR);
}

static GtkWidget *taskbar_constructor(LXPanel *panel, config_setting_t *settings)
{
    return _launchtaskbar_constructor(panel, settings, TASKBAR);
}

static LXPanelPluginInit _launchbar_init = {
    .name = N_("Application Launch Bar"),
    .description = N_("Bar with buttons to launch application"),

    .new_instance = launchbar_constructor,
    .config = launchtaskbar_configure,
    .reconfigure = launchtaskbar_panel_configuration_changed,
    .control = launchtaskbar_control
};

static LXPanelPluginInit _taskbar_init = {
    .name = N_("Task Bar (Window List)"),
    .description = N_("Taskbar shows all opened windows and allows to iconify them, shade or get focus"),

    .expand_available = TRUE,
    .expand_default = TRUE,

    .new_instance = taskbar_constructor,
    .config = launchtaskbar_configure,
    .reconfigure = launchtaskbar_panel_configuration_changed,
    .control = launchtaskbar_control
};

static void launchtaskbar_init(void)
{
    lxpanel_register_plugin_type("launchbar", &_launchbar_init);
    lxpanel_register_plugin_type("taskbar", &_taskbar_init);
}

/* Plugin descriptor. */
LXPanelPluginInit lxpanel_static_plugin_launchtaskbar = {
    .name = N_("Application Launch and Task Bar"),
    .description = N_("Bar with buttons to launch application and/or show all opened windows"),

    .expand_available = TRUE,
    .expand_default = TRUE,

    .init = launchtaskbar_init,
    .new_instance = launchtaskbar_constructor,
    .config = launchtaskbar_configure,
    /* .update_context_menu = launchtaskbar_update_context_menu, */
    .reconfigure = launchtaskbar_panel_configuration_changed,
    .control = launchtaskbar_control
};


/* vim: set sw=4 sts=4 et : */
