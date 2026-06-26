/*
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_LIBX11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>
#include <X11/Xmu/WinUtil.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>
#include <gdk/gdkx.h>
#endif

#include <glib-object.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>

#include "settings.h"
#include "task-manager.h"
#include "process-window.h"
#include "process-window_ui.h"
#include "process-monitor.h"
#include "process-tree-view.h"
#include "process-statusbar.h"
#include "settings-dialog.h"



typedef struct _XtmProcessWindowClass XtmProcessWindowClass;
struct _XtmProcessWindowClass
{
	GtkWidgetClass		parent_class;
};

struct _XtmProcessWindow
{
	GtkWidget		parent;
	/*<private>*/
	GtkBuilder *		builder;
	GtkWidget *		window;
	GtkWidget *		filter_entry;
	GtkWidget *		filter_searchbar;
	GtkWidget *		cpu_monitor;
	GtkWidget *		mem_monitor;
	GtkWidget *		vpaned;
	GtkWidget *		treeview;
	GtkWidget *		statusbar;
	GtkWidget *		settings_button;
	XtmSettings *		settings;
	XfconfChannel *		channel;
	gint			width;
	gint			height;
	gulong			handler;
	gboolean		view_stuck;
};
G_DEFINE_TYPE (XtmProcessWindow, xtm_process_window, GTK_TYPE_WIDGET)

static void	xtm_process_window_finalize			(GObject *object);
static void	xtm_process_window_hide				(GtkWidget *widget);

static void	emit_destroy_signal				(XtmProcessWindow *window);
static gboolean xtm_process_window_key_pressed	(XtmProcessWindow *window, GdkEventKey *event);
static void	monitor_update_step_size			(XtmProcessWindow *window);


static void
filter_entry_icon_pressed_cb (GtkEntry *entry,
                              gint position,
                              GdkEventButton *event __unused,
                              gpointer data __unused)
{
	if (position == GTK_ENTRY_ICON_SECONDARY) {
		gtk_entry_set_text (entry, "");
		gtk_widget_grab_focus (GTK_WIDGET(entry));
	}
}

#ifdef HAVE_LIBX11
static Window
Select_Window (Display *dpy, int screen)
{
	int status;
	Cursor cursor;
	XEvent event;
	Window target_win = None, root = RootWindow(dpy,screen);
	int buttons = 0;

	/* Make the target cursor */
	cursor = XCreateFontCursor(dpy, XC_crosshair);

	/* Grab the pointer using target cursor, letting it roam all over */
	status = XGrabPointer(dpy, root, False,
	        ButtonPressMask|ButtonReleaseMask, GrabModeSync,
	        GrabModeAsync, root, cursor, CurrentTime);
	if (status != GrabSuccess) {
	    fprintf (stderr, "Can't grab the mouse.\n");
	    return None;
	}

	/* Let the user select a window... */
	while ((target_win == None) || (buttons != 0)) {
	    /* allow one more event */
	    XAllowEvents(dpy, SyncPointer, CurrentTime);
	    XWindowEvent(dpy, root, ButtonPressMask|ButtonReleaseMask, &event);
	    switch (event.type) {
	        case ButtonPress:
	            if (target_win == None) {
	                target_win = event.xbutton.subwindow; /* window selected */
	                if (target_win == None) target_win = root;
	            }
	            buttons++;
	            break;
	        case ButtonRelease:
	            if (buttons > 0) /* there may have been some down before we started */
	                buttons--;
	            break;
	    }
	}

	XUngrabPointer(dpy, CurrentTime);      /* Done with pointer */

	return target_win;
}

static void
xwininfo_clicked_cb (GtkButton *button __unused, gpointer user_data) {
	XtmProcessWindow *window = (XtmProcessWindow *) user_data;
	Window selected_window;
	Display *dpy;
	Atom atom_NET_WM_PID;
	unsigned long _nitems;
	Atom actual_type;
	int actual_format;
	unsigned char *prop;
	int status;
	unsigned long bytes_after;
	GPid pid = 0;

	dpy = XOpenDisplay (NULL);
	selected_window = Select_Window (dpy, 0);
	if (selected_window) {
		selected_window = XmuClientWindow (dpy, selected_window);
	}

	atom_NET_WM_PID = XInternAtom(dpy, "_NET_WM_PID", False);

	status = XGetWindowProperty(dpy, selected_window, atom_NET_WM_PID, 0, (~0L),
	                            False, AnyPropertyType, &actual_type,
	                            &actual_format, &_nitems, &bytes_after,
	                            &prop);
	if (status == BadWindow) {
		XTM_SHOW_MESSAGE(GTK_MESSAGE_INFO,
                                    _("Bad Window"), _("Window id 0x%lx does not exist!"), selected_window);
	} if (status != Success) {
		XTM_SHOW_MESSAGE(GTK_MESSAGE_ERROR,
                                    _("XGetWindowProperty failed"), _("XGetWindowProperty failed!"));
	} else {
		if (_nitems > 0) {
			memcpy(&pid, prop, sizeof(pid));
			xtm_process_tree_view_highlight_pid(XTM_PROCESS_TREE_VIEW (window->treeview), pid);
		} else {
			XTM_SHOW_MESSAGE(GTK_MESSAGE_INFO,
                                            _("No PID found"), _("No PID found for window 0x%lx."), selected_window);
		}
		g_free(prop);
	}

}
#endif

static void
filter_entry_keyrelease_handler(GtkEntry *entry,
                                XtmProcessTreeView *treeview)
{
	const gchar *text;
	gboolean has_text;

	text = gtk_editable_get_chars (GTK_EDITABLE(entry), 0, -1);
	xtm_process_tree_view_set_filter(treeview, text);

	has_text = gtk_entry_get_text_length (GTK_ENTRY(entry)) > 0;
	gtk_entry_set_icon_sensitive (GTK_ENTRY(entry),
	                              GTK_ENTRY_ICON_SECONDARY,
	                              has_text);
}

static void
xtm_process_window_class_init (XtmProcessWindowClass *klass)
{
	GObjectClass *class;
	GtkWidgetClass *widget_class;

	xtm_process_window_parent_class = g_type_class_peek_parent (klass);
	class = G_OBJECT_CLASS (klass);
	class->finalize = xtm_process_window_finalize;
	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->show = xtm_process_window_show;
	widget_class->hide = xtm_process_window_hide;
}

static void
xtm_process_window_size_allocate (GtkWidget *widget, GtkAllocation *allocation, gpointer user_data)
{
	XtmProcessWindow *window = (XtmProcessWindow *) user_data;

	g_return_if_fail (gtk_widget_is_toplevel (widget));

	gtk_window_get_size (GTK_WINDOW (widget), &window->width, &window->height);
}

static void
show_settings_dialog (GtkButton *button, gpointer user_data)
{
	XtmProcessWindow *window = (XtmProcessWindow *) user_data;

	g_signal_handler_block (G_OBJECT (window->window), window->handler);
	xtm_settings_dialog_run (window->window);
	g_signal_handler_unblock (G_OBJECT (window->window), window->handler);
}

static void
xtm_process_window_stick_view (GtkAdjustment *adjustment, XtmProcessWindow *window)
{
	if (window->view_stuck)
		gtk_adjustment_set_value (adjustment, 0);
	else if (gtk_adjustment_get_value (adjustment) == 0)
		window->view_stuck = TRUE;
}

static gboolean
xtm_process_window_unstick_view_event (GtkWidget *widget, GdkEvent *event, XtmProcessWindow *window)
{
	GdkScrollDirection dir;
	gdouble y;

	if (! window->view_stuck)
		return FALSE;

	if (event->type == GDK_SCROLL && (
				(gdk_event_get_scroll_direction (event, &dir) && dir == GDK_SCROLL_UP)
				|| (gdk_event_get_scroll_deltas (event, NULL, &y) && y <= 0)
			))
		return FALSE;

	window->view_stuck = FALSE;

	return FALSE;
}

static void
xtm_process_window_unstick_view_cursor (GtkTreeView *tree_view, XtmProcessWindow *window)
{
	GtkTreePath *cursor, *end;

	if (! window->view_stuck)
		return;

	if (gtk_tree_view_get_visible_range (tree_view, NULL, &end)) {
		gtk_tree_view_get_cursor (tree_view, &cursor, NULL);
		if (cursor != NULL && gtk_tree_path_compare (cursor, end) >= 0)
			window->view_stuck = FALSE;
		gtk_tree_path_free (cursor);
		gtk_tree_path_free (end);
	}
}

static void
xtm_process_window_init (XtmProcessWindow *window)
{
	GtkWidget *button;

	window->settings = xtm_settings_get_default ();
	window->channel = xfconf_channel_new (CHANNEL);

	window->builder = gtk_builder_new ();
	gtk_builder_add_from_string (window->builder, process_window_ui, process_window_ui_length, NULL);

	window->window = GTK_WIDGET (gtk_builder_get_object (window->builder, "process-window"));
	window->width = xfconf_channel_get_int (window->channel, SETTING_WINDOW_WIDTH, DEFAULT_WINDOW_WIDTH);
	window->height = xfconf_channel_get_int (window->channel, SETTING_WINDOW_HEIGHT, DEFAULT_WINDOW_HEIGHT);
	if (window->width >= 1 && window->height >= 1)
		gtk_window_set_default_size (GTK_WINDOW (window->window), window->width, window->height);

	/* If the window was closed maximized, reopen it maximized again */
	if (xfconf_channel_get_bool (window->channel, SETTING_WINDOW_MAXIMIZED, FALSE))
		gtk_window_maximize (GTK_WINDOW (window->window));

	g_signal_connect_swapped (window->window, "destroy", G_CALLBACK (emit_destroy_signal), window);
	window->handler = g_signal_connect (window->window, "size-allocate", G_CALLBACK (xtm_process_window_size_allocate), window);
	g_signal_connect_swapped (window->window, "key-press-event", G_CALLBACK(xtm_process_window_key_pressed), window);

	button = GTK_WIDGET (gtk_builder_get_object (window->builder, "button-settings"));
	g_signal_connect (G_OBJECT (button), "clicked",
										G_CALLBACK (show_settings_dialog), window);

	button = GTK_WIDGET (gtk_builder_get_object (window->builder, "button-identify"));
#ifdef HAVE_LIBX11
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
		g_signal_connect (G_OBJECT (button), "clicked",
						  G_CALLBACK (xwininfo_clicked_cb), window);
	else
#endif
		gtk_widget_hide (button);

	window->filter_searchbar = GTK_WIDGET (gtk_builder_get_object (window->builder, "filter-searchbar"));
	{
		GtkWidget *toolitem;
		guint refresh_rate;

		g_object_get (window->settings,
					  "refresh-rate", &refresh_rate,
					  NULL);

		window->vpaned = GTK_WIDGET (gtk_builder_get_object (window->builder, "mainview-vpaned"));
		xfconf_g_property_bind (window->channel, SETTING_HANDLE_POSITION, G_TYPE_INT,
			G_OBJECT (window->vpaned), "position");

		toolitem = GTK_WIDGET (gtk_builder_get_object (window->builder, "graph-cpu"));
		window->cpu_monitor = xtm_process_monitor_new ();
		xtm_process_monitor_set_step_size (XTM_PROCESS_MONITOR (window->cpu_monitor), refresh_rate / 1000.0f);
		xtm_process_monitor_set_type (XTM_PROCESS_MONITOR (window->cpu_monitor), 0);
		gtk_widget_show (window->cpu_monitor);
		gtk_container_add (GTK_CONTAINER (toolitem), window->cpu_monitor);

		toolitem = GTK_WIDGET (gtk_builder_get_object (window->builder, "graph-mem"));
		window->mem_monitor = xtm_process_monitor_new ();
		xtm_process_monitor_set_step_size (XTM_PROCESS_MONITOR (window->mem_monitor), refresh_rate / 1000.0f);
		xtm_process_monitor_set_type (XTM_PROCESS_MONITOR (window->mem_monitor), 1);
		gtk_widget_show (window->mem_monitor);
		gtk_container_add (GTK_CONTAINER (toolitem), window->mem_monitor);

		g_signal_connect_swapped (window->settings, "notify::refresh-rate", G_CALLBACK (monitor_update_step_size), window);
	}

	window->statusbar = xtm_process_statusbar_new ();
	gtk_widget_show (window->statusbar);
	gtk_box_pack_start (GTK_BOX (gtk_builder_get_object (window->builder, "graph-vbox")), window->statusbar, FALSE, FALSE, 0);

	gtk_widget_set_visible (GTK_WIDGET (gtk_builder_get_object (window->builder, "root-warning-box")), geteuid () == 0);

	window->treeview = xtm_process_tree_view_new ();
	gtk_widget_show (window->treeview);
	{
		GtkScrolledWindow *s_window;
		GtkWidget *bar;
		GtkAdjustment *adjust;

		s_window = GTK_SCROLLED_WINDOW (gtk_builder_get_object (window->builder, "scrolledwindow"));
		bar = gtk_scrolled_window_get_vscrollbar (s_window);
		adjust = gtk_scrolled_window_get_vadjustment (s_window);
		window->view_stuck = TRUE;

		gtk_container_add (GTK_CONTAINER (s_window), window->treeview);
		g_signal_connect (adjust, "value-changed", G_CALLBACK (xtm_process_window_stick_view), window);
		g_signal_connect (bar, "button-press-event", G_CALLBACK (xtm_process_window_unstick_view_event), window);
		g_signal_connect (bar, "scroll-event", G_CALLBACK (xtm_process_window_unstick_view_event), window);
		g_signal_connect (window->treeview, "scroll-event", G_CALLBACK (xtm_process_window_unstick_view_event), window);
		g_signal_connect (window->treeview, "cursor-changed", G_CALLBACK (xtm_process_window_unstick_view_cursor), window);
	}

	window->filter_entry = GTK_WIDGET(gtk_builder_get_object (window->builder, "filter-entry"));
	g_signal_connect (G_OBJECT(window->filter_entry), "icon-press", G_CALLBACK(filter_entry_icon_pressed_cb), NULL);
	g_signal_connect (G_OBJECT(window->filter_entry), "changed", G_CALLBACK(filter_entry_keyrelease_handler), window->treeview);
	gtk_widget_set_tooltip_text (window->filter_entry, _("Filter on process name"));
	gtk_widget_grab_focus (window->filter_entry);

	{
		const gchar *const captions[] = { _("Starting task"), _("Changing task"), _("Terminating task") };
		const gchar *styles =
			".a,.b,.c{border-radius:50%}"
			".a{background-color:" XTM_LEGEND_COLOR_STARTING "}"
			".b{background-color:" XTM_LEGEND_COLOR_CHANGING "}"
			".c{background-color:" XTM_LEGEND_COLOR_TERMINATING "}";
		const gchar *const classes[] = { "a", "b", "c" };
		GtkWidget *hbox_legend = GTK_WIDGET (gtk_builder_get_object (window->builder, "legend"));
		GtkCssProvider *provider = gtk_css_provider_new ();

		gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
			GTK_STYLE_PROVIDER (provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		gtk_css_provider_load_from_data (provider, styles, -1, NULL);
		g_object_unref (provider);

		for (guint i = 0; i < G_N_ELEMENTS (captions); ++i)
		{
			GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

			GtkWidget *label = gtk_label_new (NULL);
			gtk_widget_set_size_request (label, 16, 16);
			gtk_style_context_add_class (gtk_widget_get_style_context (label), classes[i]);
			gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

			label = gtk_label_new (captions[i]);
			gtk_label_set_xalign (GTK_LABEL (label), 0.0);
			gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

			gtk_box_pack_start (GTK_BOX (hbox_legend), hbox, FALSE, FALSE, 0);
		}

		gtk_widget_show_all (hbox_legend);
		g_object_bind_property (window->settings, "show-legend", hbox_legend, "visible", G_BINDING_SYNC_CREATE);
	}
}

static void
xtm_process_window_finalize (GObject *object)
{
	XtmProcessWindow *window = XTM_PROCESS_WINDOW (object);

	g_object_unref (window->settings);
	g_object_unref (window->builder);

	G_OBJECT_CLASS (xtm_process_window_parent_class)->finalize (object);
}

/**
 * Helper functions
 */

static void
emit_destroy_signal (XtmProcessWindow *window)
{
	gboolean maximized = gtk_window_is_maximized (GTK_WINDOW (window->window));

	/* Store whether window is maximized */
	xfconf_channel_set_bool (window->channel, SETTING_WINDOW_MAXIMIZED, maximized);

	if (!maximized)
	{
		/* Store window size */
		xfconf_channel_set_int (window->channel, SETTING_WINDOW_WIDTH, window->width);
		xfconf_channel_set_int (window->channel, SETTING_WINDOW_HEIGHT, window->height);
	}

	g_signal_emit_by_name (window, "destroy", G_TYPE_NONE);
}

static gboolean
xtm_process_window_key_pressed (XtmProcessWindow *window, GdkEventKey *event)
{
	gboolean ret = FALSE;

	if (event->keyval == GDK_KEY_Escape &&
			gtk_widget_is_focus(GTK_WIDGET (window->filter_entry)))
	{
		if (xfconf_channel_get_bool (window->channel, SETTING_SHOW_FILTER, FALSE))
			gtk_entry_set_text (GTK_ENTRY(window->filter_entry), "");
		else
			g_signal_emit_by_name (window, "delete-event", event, &ret, G_TYPE_BOOLEAN);
	}
	else if (event->keyval == GDK_KEY_Escape ||
		(event->keyval == GDK_KEY_q && (event->state & GDK_CONTROL_MASK)))
	{
		g_signal_emit_by_name (window, "delete-event", event, &ret, G_TYPE_BOOLEAN);
		ret = TRUE;
	}
	else if (event->keyval == GDK_KEY_f && (event->state & GDK_CONTROL_MASK))
	{
		gtk_widget_grab_focus (GTK_WIDGET(window->filter_entry));
		xfconf_channel_set_bool (window->channel, SETTING_SHOW_FILTER, TRUE);
		ret = TRUE;
	}

	return ret;
}

static void
monitor_update_step_size (XtmProcessWindow *window)
{
	guint refresh_rate;
	g_object_get (window->settings, "refresh-rate", &refresh_rate, NULL);
	g_object_set (window->cpu_monitor, "step-size", refresh_rate / 1000.0, NULL);
	g_object_set (window->mem_monitor, "step-size", refresh_rate / 1000.0, NULL);
}

/**
 * Class functions
 */

GtkWidget *
xtm_process_window_new (void)
{
	return g_object_new (XTM_TYPE_PROCESS_WINDOW, NULL);
}

void
xtm_process_window_show (GtkWidget *widget)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (GTK_IS_WIDGET (XTM_PROCESS_WINDOW (widget)->window));
	gtk_widget_show (XTM_PROCESS_WINDOW (widget)->window);
	gtk_window_present (GTK_WINDOW (XTM_PROCESS_WINDOW (widget)->window));
	GTK_WIDGET_CLASS (xtm_process_window_parent_class)->show(widget);
}

static void
xtm_process_window_hide (GtkWidget *widget)
{
	gint winx, winy;
	g_return_if_fail (GTK_IS_WIDGET (widget));
	if (!GTK_IS_WIDGET (XTM_PROCESS_WINDOW (widget)->window))
		return;
	gtk_window_get_position (GTK_WINDOW (XTM_PROCESS_WINDOW (widget)->window), &winx, &winy);
	gtk_widget_hide (XTM_PROCESS_WINDOW (widget)->window);
	gtk_window_move (GTK_WINDOW (XTM_PROCESS_WINDOW (widget)->window), winx, winy);
	GTK_WIDGET_CLASS (xtm_process_window_parent_class)->hide(widget);
}

GtkTreeModel *
xtm_process_window_get_model (XtmProcessWindow *window)
{
	g_return_val_if_fail (XTM_IS_PROCESS_WINDOW (window), NULL);
	g_return_val_if_fail (XTM_IS_PROCESS_TREE_VIEW (window->treeview), NULL);
	return xtm_process_tree_view_get_model (XTM_PROCESS_TREE_VIEW (window->treeview));
}

void
xtm_process_window_set_system_info (XtmProcessWindow *window, guint num_processes, gfloat cpu, gfloat memory, gchar* memory_str, gfloat swap, gchar* swap_str)
{
	gchar text[100];
	gchar value[4];

	g_return_if_fail (XTM_IS_PROCESS_WINDOW (window));
	g_return_if_fail (GTK_IS_BOX (window->statusbar));

	g_object_set (window->statusbar, "num-processes", num_processes, "cpu", cpu, "memory", memory_str, "swap", swap_str, NULL);

	xtm_process_monitor_add_peak (XTM_PROCESS_MONITOR (window->cpu_monitor), cpu / 100.0f, -1.0);
	g_snprintf (value, sizeof(value), "%.0f", cpu);
	g_snprintf (text, sizeof(text), _("CPU: %s%%"), value);
	gtk_widget_set_tooltip_text (window->cpu_monitor, text);

	xtm_process_monitor_add_peak (XTM_PROCESS_MONITOR (window->mem_monitor), memory / 100.0f, swap / 100.0f);
	g_snprintf (text, sizeof(text), _("Memory: %s"), memory_str);
	gtk_widget_set_tooltip_text (window->mem_monitor, text);
}

void
xtm_process_window_show_swap_usage (XtmProcessWindow *window, gboolean show_swap_usage)
{
	g_return_if_fail (XTM_IS_PROCESS_WINDOW (window));
	g_return_if_fail (GTK_IS_BOX (window->statusbar));
	g_object_set (window->statusbar, "show-swap", show_swap_usage, NULL);
}
