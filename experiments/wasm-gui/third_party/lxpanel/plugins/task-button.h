/**
 * Copyright (C) 2014-2016 Andriy Grytsenko <andrej@rep.kiev.ua>
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

#ifndef __TASK_BUTTON_H__
#define __TASK_BUTTON_H__ 1

#include "plugin.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct /* bitwise rendering options for taskbar */
{
    gboolean show_all_desks : 1;        /* show windows from all desktops */
    gboolean tooltips : 1;              /* show tooltips */
    gboolean icons_only : 1;            /* show icons only, omit name */
    gboolean use_mouse_wheel : 1;       /* scroll wheel does iconify and raise */
    gboolean use_urgency_hint : 1;      /* windows with urgency will flash */
    gboolean flat_button : 1;           /* taskbar buttons have visible background */
    gboolean same_monitor_only : 1;     /* only show windows that are in the same monitor as the taskbar */
    gboolean disable_taskbar_upscale : 1; /* don't upscale taskbar icons */
    gboolean use_net_active : 1;        /* NET_WM_ACTIVE_WINDOW is supported by the window manager */
    gboolean use_smaller_icons : 1;     /* use icons 4 pixels less than panel default */
} TaskShowFlags;

#define PANEL_TYPE_TASK_BUTTON             (task_button_get_type())
#define PANEL_TASK_BUTTON(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                            PANEL_TYPE_TASK_BUTTON, TaskButton))
#define PANEL_TASK_BUTTON_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), \
                                            PANEL_TYPE_TASK_BUTTON, TaskButtonClass))
#define PANEL_IS_TASK_BUTTON(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                            PANEL_TYPE_TASK_BUTTON))
#define PANEL_IS_TASK_BUTTON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                            PANEL_TYPE_TASK_BUTTON))

extern GType task_button_get_type          (void) G_GNUC_CONST;

typedef struct _TaskButton                  TaskButton;
typedef struct _TaskButtonClass             TaskButtonClass;

struct _TaskButtonClass
{
    GtkToggleButtonClass parent_class;
    void (*menu_built)(TaskButton *button, GtkMenu *menu); /* "menu-built" signal */
    void (*menu_target_set)(TaskButton *button, gulong win); /* "menu-target-set" signal */
};

/* creates new button and sets rendering options */
TaskButton *task_button_new(Window win, gint desk, gint desks, LXPanel *panel,
                            const char *cl, TaskShowFlags flags);

gboolean task_button_has_window(TaskButton *button, Window win);
/* removes windows from button, that are missing in list */
void task_button_update_windows_list(TaskButton *button, Window *list, gint n);
/* returns TRUE if found and updated */
gboolean task_button_window_xprop_changed(TaskButton *button, Window win, Atom atom);
gboolean task_button_window_focus_changed(TaskButton *button, Window *win);
gboolean task_button_window_reconfigured(TaskButton *button, Window win);
/* updates rendering options */
void task_button_update(TaskButton *button, gint desk, gint desks,
                        gint mon, guint icon_size, TaskShowFlags flags);
void task_button_set_flash_state(TaskButton *button, gboolean state);
/* adds task only if it's the same class */
gboolean task_button_add_window(TaskButton *button, Window win, const char *cl);
gboolean task_button_drop_window(TaskButton *button, Window win, gboolean leave_last);
/* leaves only last task in button and returns rest if not empty */
TaskButton *task_button_split(TaskButton *button);
/* merges buttons if they are the same class */
gboolean task_button_merge(TaskButton *button, TaskButton *sibling);
/* single-instance-menu management, should be called on button parent widget */
void task_button_reset_menu(GtkWidget *parent);
/* request for a minimized window to raise */
void task_button_raise_window(TaskButton *button, guint32 time);

G_END_DECLS

#endif /* __TASK_BUTTON_H__ */
