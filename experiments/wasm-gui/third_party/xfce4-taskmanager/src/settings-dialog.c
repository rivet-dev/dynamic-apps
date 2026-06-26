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

#include <glib-object.h>
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include <libxfce4ui/libxfce4ui.h>

#include "settings.h"
#include "settings-dialog.h"
#include "settings-dialog_ui.h"

static void	show_about_dialog				(GtkWidget *widget, gpointer user_data);
static GtkWidget *xtm_settings_dialog_new (GtkBuilder *builder, GtkWidget *parent_window);


typedef struct
{
	GtkWidget		*combobox;
	guint		rate;
} XtmRefreshRate;

static void
button_toggled (GtkToggleButton *button, XtmSettings *settings)
{
	gboolean active = gtk_toggle_button_get_active (button);
	gchar *setting_name = g_object_get_data (G_OBJECT (button), "setting-name");
	g_object_set (settings, setting_name, active, NULL);
}

static void
builder_bind_toggle_button (GtkBuilder *builder, gchar *widget_name, XtmSettings *settings, gchar *setting_name)
{
	gboolean active;
	GtkWidget *button;

	g_object_get (settings, setting_name, &active, NULL);

	button = GTK_WIDGET (gtk_builder_get_object (builder, widget_name));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), active);
	g_object_set_data (G_OBJECT (button), "setting-name", setting_name);
	g_signal_connect (button, "toggled", G_CALLBACK (button_toggled), settings);
}

static void
combobox_changed (GtkComboBox *combobox, XtmSettings *settings)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GValue prop = { 0, };
	gint rate;

	gtk_combo_box_get_active_iter (combobox, &iter);
	model = gtk_combo_box_get_model (combobox);
	gtk_tree_model_get_value (model, &iter, 0, &prop);
	rate = g_value_get_int (&prop);
	g_object_set (settings, "refresh-rate", GUINT_TO_POINTER (rate), NULL);
}

static gboolean
combobox_foreach (GtkTreeModel *model,
                                      GtkTreePath  *path,
                                      GtkTreeIter  *iter,
                                      gpointer      user_data)
{
	XtmRefreshRate *refresh_rate = user_data;
	GValue prop = { 0, };

	gtk_tree_model_get_value (model, iter, 0, &prop);

	if ((guint) g_value_get_int (&prop) == refresh_rate->rate)
	{
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (refresh_rate->combobox), iter);
		return TRUE;
	}

	return FALSE;
}

static void
builder_bind_combobox (GtkBuilder *builder, XtmSettings *settings)
{
	XtmRefreshRate *refresh_rate;
	GtkTreeModel *model;

	refresh_rate = g_new0 (XtmRefreshRate, 1);
	g_object_get (settings, "refresh-rate", &refresh_rate->rate, NULL);

	refresh_rate->combobox = GTK_WIDGET (gtk_builder_get_object (builder, "combobox-refresh-rate"));
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (refresh_rate->combobox));
	gtk_tree_model_foreach (model, combobox_foreach, refresh_rate);
	g_object_set_data (G_OBJECT (refresh_rate->combobox), "setting-name", "refresh-rate");
	g_signal_connect (refresh_rate->combobox, "changed", G_CALLBACK (combobox_changed), settings);
	g_free (refresh_rate);
}

static void
show_about_dialog (GtkWidget *widget, gpointer user_data)
{
	GtkDialog *dialog = GTK_DIALOG (user_data);

	const gchar *authors[] = {
		"(c) 2014-2021 Simon Steinbeiss",
		"(c) 2018-2019 Rozhuk Ivan",
		"(c) 2014 Landry Breuil",
		"(c) 2014 Harald Judt",
		"(c) 2014 Peter de Ridder",
		"(c) 2008-2010 Mike Massonnet",
		"(c) 2005-2008 Johannes Zellner",
		"",
		"FreeBSD",
		"  \342\200\242 Rozhuk Ivan",
		"  \342\200\242 Mike Massonnet",
		"  \342\200\242 Oliver Lehmann",
		"",
		"OpenBSD",
		"  \342\200\242 Landry Breuil",
		"",
		"Linux",
		"  \342\200\242 Johannes Zellner",
		"  \342\200\242 Mike Massonnet",
		"",
		"OpenSolaris",
		"  \342\200\242 Mike Massonnet",
		"  \342\200\242 Peter Tribble",
		NULL };
	const gchar *license =
		"This program is free software; you can redistribute it and/or modify\n"
		"it under the terms of the GNU General Public License as published by\n"
		"the Free Software Foundation; either version 2 of the License, or\n"
		"(at your option) any later version.\n";

	gtk_show_about_dialog (GTK_WINDOW (dialog),
		"program-name", _("Task Manager"),
		"version", PACKAGE_VERSION,
		"copyright", "Copyright \302\251 2005-2024 The Xfce development team",
		"logo-icon-name", "org.xfce.taskmanager",
		"comments", _("Easy to use task manager"),
		"license", license,
		"authors", authors,
		"translator-credits", _("translator-credits"),
		"website", "https://docs.xfce.org/apps/xfce4-taskmanager/start",
		NULL);
}

static void
show_help (GtkWidget *widget, gpointer user_data)
{
	GtkDialog *dialog = GTK_DIALOG (user_data);

	xfce_dialog_show_help_with_version (GTK_WINDOW (dialog), "xfce4-taskmanager", "start", NULL, NULL);
}

static void
dialog_close (GtkWidget *widget, gpointer user_data)
{
	GtkDialog *dialog = GTK_DIALOG (user_data);

	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);
}

static GtkWidget *
xtm_settings_dialog_new (GtkBuilder *builder, GtkWidget *parent_window)
{
	GtkWidget *dialog;
	GtkWidget *button;
	XtmSettings *settings;

	settings = xtm_settings_get_default ();
	dialog = GTK_WIDGET (gtk_builder_get_object (builder, "settings-dialog"));
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent_window));

	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "button-show-application-icons")));
#ifdef HAVE_WNCK
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
		gtk_widget_show (GTK_WIDGET (gtk_builder_get_object (builder, "button-show-application-icons")));
#endif

	// Interface
	builder_bind_toggle_button (builder, "button-show-all-processes", settings, "show-all-processes");
	builder_bind_toggle_button (builder, "button-show-application-icons", settings, "show-application-icons");
	builder_bind_toggle_button (builder, "button-full-command-line", settings, "full-command-line");
	builder_bind_toggle_button (builder, "button-more-precision", settings, "more-precision");
	builder_bind_toggle_button (builder, "button-process-tree", settings, "process-tree");
	builder_bind_toggle_button (builder, "button-show-legend", settings, "show-legend");
	builder_bind_combobox (builder, settings);

	// Miscellaneous
	builder_bind_toggle_button (builder, "button-prompt-terminate-task", settings, "prompt-terminate-task");
	builder_bind_toggle_button (builder, "button-show-status-icon", settings, "show-status-icon");
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "button-show-status-icon")));
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
		gtk_widget_show (GTK_WIDGET (gtk_builder_get_object (builder, "button-show-status-icon")));
#endif

	// Columns
	builder_bind_toggle_button (builder, "pid", settings, "column-pid");
	builder_bind_toggle_button (builder, "ppid", settings, "column-ppid");
	builder_bind_toggle_button (builder, "state", settings, "column-state");
	builder_bind_toggle_button (builder, "vbytes", settings, "column-vsz");
	builder_bind_toggle_button (builder, "rbytes", settings, "column-rss");
	builder_bind_toggle_button (builder, "uid", settings, "column-uid");
	builder_bind_toggle_button (builder, "cpu", settings, "column-cpu");
	builder_bind_toggle_button (builder, "priority", settings, "column-priority");


	button = GTK_WIDGET (gtk_builder_get_object (builder, "button-about"));
	g_signal_connect (button, "clicked", G_CALLBACK (show_about_dialog), dialog);

	button = GTK_WIDGET (gtk_builder_get_object (builder, "button-help"));
	g_signal_connect (button, "clicked", G_CALLBACK (show_help), dialog);

	button = GTK_WIDGET (gtk_builder_get_object (builder, "button-close"));
	g_signal_connect (button, "clicked", G_CALLBACK (dialog_close), dialog);

	return dialog;
}

void
xtm_settings_dialog_run (GtkWidget *parent_window)
{
	GtkBuilder *builder;
	GtkWidget *dialog;

	builder = gtk_builder_new ();
	gtk_builder_add_from_string (builder, settings_dialog_ui, settings_dialog_ui_length, NULL);
	g_return_if_fail (GTK_IS_BUILDER (builder));

	dialog = xtm_settings_dialog_new (builder, parent_window);

	g_object_unref (builder);
	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
}
