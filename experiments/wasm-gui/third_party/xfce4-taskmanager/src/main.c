/*
 * Copyright (c) 2005-2006 Johannes Zellner, <webmaster@nebulon.de>
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>

#include "settings.h"
#include "process-window.h"
#include "task-manager.h"

static XtmSettings *settings;
static GtkWidget *window;
static GtkStatusIcon *status_icon_or_null = NULL;
static XtmTaskManager *task_manager;
static guint timer_id;
static gboolean start_hidden = FALSE;
static gboolean standalone = FALSE;

static GOptionEntry main_entries[] = {
	{ "start-hidden", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &start_hidden, "Don't open a task manager window", NULL },
	{ "standalone", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &standalone, "Launch standalone", NULL },
	{ NULL, 0, 0, 0, NULL, NULL, NULL }
};

static void	destroy_window (void);


static void
status_icon_activated (void)
{
	if (!(gtk_widget_get_visible (window)))
		gtk_widget_show (window);
	else
		gtk_widget_hide (window);
}

static void
status_icon_popup_menu (GtkStatusIcon *_status_icon, guint button, guint activate_time)
{
	static GtkWidget *menu = NULL;

	if (menu == NULL)
	{
		GtkWidget *mi;
		menu = gtk_menu_new ();
		mi = gtk_menu_item_new_with_label (_("Quit"));
		g_signal_connect (mi, "activate", G_CALLBACK (destroy_window), NULL);
		gtk_container_add (GTK_CONTAINER (menu), mi);
		gtk_widget_show_all (menu);
	}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, gtk_status_icon_position_menu, _status_icon, button, activate_time);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
create_status_icon (void)
{
	if (!status_icon_or_null)
	{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		GtkStatusIcon *status_icon = gtk_status_icon_new_from_icon_name ("org.xfce.taskmanager");
G_GNUC_END_IGNORE_DEPRECATIONS
		g_signal_connect (status_icon, "activate", G_CALLBACK (status_icon_activated), NULL);
		g_signal_connect (status_icon, "popup-menu", G_CALLBACK (status_icon_popup_menu), NULL);
		status_icon_or_null = status_icon;
	}
}

static gboolean
status_icon_get_visible (void)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	return status_icon_or_null && gtk_status_icon_get_visible (status_icon_or_null);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
show_hide_status_icon (void)
{
	gboolean show_status_icon;
	g_object_get (settings, "show-status-icon", &show_status_icon, NULL);
	if (show_status_icon)
	{
		create_status_icon ();
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		gtk_status_icon_set_visible (status_icon_or_null, TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS
	}
	else if (status_icon_get_visible ())
	{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		gtk_status_icon_set_visible (status_icon_or_null, FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS
	}
}

static void
destroy_window (void)
{
	if (gtk_main_level () > 0) {
		if (timer_id > 0)
			g_source_remove (timer_id);
		gtk_main_quit ();
	}
}

static gboolean
delete_window (void)
{
	if (!status_icon_get_visible ())
	{
		if (timer_id > 0)
			g_source_remove (timer_id);
		gtk_main_quit ();
		return FALSE;
	}
	gtk_widget_hide (window);
	return TRUE;
}

static void
collect_data (void)
{
	guint num_processes;
	gfloat cpu, memory_percent, swap_percent;
	guint64 swap_used, swap_free, swap_total, memory_used, memory_total;
	gchar *used, *total, tooltip[1024], memory_info[64], swap_info[64];

	xtm_task_manager_get_system_info (task_manager, &num_processes, &cpu, &memory_used, &memory_total, &swap_used, &swap_total);

	memory_percent = (memory_total != 0) ? ((memory_used * 100.0f) / (float)memory_total) : 0.0f;
	swap_percent = (swap_total != 0) ? ((swap_used * 100.0f) / (float)swap_total) : 0.0f;

	used = g_format_size_full(memory_used, G_FORMAT_SIZE_IEC_UNITS);
	total = g_format_size_full(memory_total, G_FORMAT_SIZE_IEC_UNITS);
	g_snprintf (memory_info, sizeof(memory_info), "%.0f%% (%s / %s)", memory_percent, used, total);
	g_free(used);
	g_free(total);

	used = g_format_size_full(swap_used, G_FORMAT_SIZE_IEC_UNITS);
	total = g_format_size_full(swap_total, G_FORMAT_SIZE_IEC_UNITS);
	g_snprintf (swap_info, sizeof(swap_info), "%.0f%% (%s / %s)", swap_percent, used, total);
	g_free(used);
	g_free(total);

	xtm_process_window_set_system_info (XTM_PROCESS_WINDOW (window), num_processes, cpu, memory_percent, memory_info, swap_percent, swap_info);

	xtm_task_manager_get_swap_usage (task_manager, &swap_free, &swap_total);
	xtm_process_window_show_swap_usage (XTM_PROCESS_WINDOW (window), (swap_total > 0));

	if (status_icon_get_visible ())
	{
		g_snprintf (tooltip, sizeof(tooltip),
				_("<b>Processes:</b> %u\n"
				"<b>CPU:</b> %.0f%%\n"
				"<b>Memory:</b> %s\n"
				"<b>Swap:</b> %s"),
				num_processes, cpu, memory_info, swap_info);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		gtk_status_icon_set_tooltip_markup (GTK_STATUS_ICON (status_icon_or_null), tooltip);
G_GNUC_END_IGNORE_DEPRECATIONS
	}

	xtm_task_manager_update_model (task_manager);
}

static gboolean
init_timeout_cb (gpointer user_data)
{
	collect_data ();
	return TRUE;
}

static void
init_timeout (void)
{
	guint refresh_rate;

	g_object_get (settings, "refresh-rate", &refresh_rate, NULL);
	timer_id = g_timeout_add (refresh_rate, init_timeout_cb, NULL);
}

static void
refresh_rate_changed (void)
{
	if (!g_source_remove (timer_id))
	{
		g_critical ("Unable to remove source");
		return;
	}
	timer_id = 0;
	init_timeout ();
}

int main (int argc, char *argv[])
{
	XfconfChannel *channel;
	GApplication *app;
	GError *error = NULL;
	GOptionContext *opt_context;

	xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	gtk_init (&argc, &argv);
	g_set_application_name (_("Task Manager"));

	opt_context = g_option_context_new ("");
	g_option_context_add_main_entries (opt_context, main_entries, NULL);
	g_option_context_add_group (opt_context, gtk_get_option_group (TRUE));
	if (!g_option_context_parse (opt_context, &argc, &argv, &error))
	{
		g_print ("Unable to parse arguments: %s\n", error->message);
		g_error_free (error);
		g_option_context_free (opt_context);
		return 1;
	}
	g_option_context_free (opt_context);

	app = g_application_new ("xfce.taskmanager", standalone ? G_APPLICATION_NON_UNIQUE : G_APPLICATION_FLAGS_NONE);
	g_application_register (G_APPLICATION (app), NULL, &error);
	if (error != NULL)
	{
		g_warning ("Unable to register GApplication: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	if (g_application_get_is_remote (G_APPLICATION (app)))
	{
		if (!start_hidden)
			g_application_activate (G_APPLICATION (app));
		g_object_unref (app);
		return 0;
	}

	if (!xfconf_init (&error))
	{
		xfce_message_dialog (NULL, _("Xfce Notify Daemon"),
			"dialog-error",
			_("Settings daemon is unavailable"),
			error->message,
			"application-exit", GTK_RESPONSE_ACCEPT,
			NULL);
		exit (EXIT_FAILURE);
	}

	channel = xfconf_channel_new (CHANNEL);
	settings = xtm_settings_get_default ();
	xtm_settings_bind_xfconf (settings, channel);
	show_hide_status_icon ();

	window = g_object_ref_sink (xtm_process_window_new ());

	if (!start_hidden)
		gtk_widget_show (window);

	g_signal_connect_swapped (app, "activate", G_CALLBACK (xtm_process_window_show), window);

	task_manager = xtm_task_manager_new (xtm_process_window_get_model (XTM_PROCESS_WINDOW (window)));

	collect_data ();
	init_timeout ();
	g_signal_connect (settings, "notify::refresh-rate", G_CALLBACK (refresh_rate_changed), NULL);
	g_signal_connect_after (settings, "notify::more-precision", G_CALLBACK (collect_data), NULL);
	g_signal_connect_after (settings, "notify::full-command-line", G_CALLBACK (collect_data), NULL);
	g_signal_connect (settings, "notify::show-status-icon", G_CALLBACK (show_hide_status_icon), NULL);

	g_signal_connect (window, "destroy", G_CALLBACK (destroy_window), NULL);
	g_signal_connect (window, "delete-event", G_CALLBACK (delete_window), NULL);

	if (gtk_widget_get_visible (window) || status_icon_get_visible ())
		gtk_main ();
	else
		g_warning ("Nothing to do: activate hiding to the notification area when using --start-hidden");

	g_object_unref (task_manager);
	g_object_unref (window);
	g_object_unref (settings);
	if (status_icon_or_null != NULL)
		g_object_unref (status_icon_or_null);
	xfconf_shutdown();

	return 0;
}
