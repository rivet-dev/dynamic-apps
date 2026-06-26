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
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "process-statusbar.h"
#include "settings.h"
#include "task-manager.h"



enum
{
	PROP_CPU = 1,
	PROP_MEMORY,
	PROP_SWAP,
	PROP_SHOW_SWAP,
	PROP_NUM_PROCESSES,
};
typedef struct _XtmProcessStatusbarClass XtmProcessStatusbarClass;
struct _XtmProcessStatusbarClass
{
	GtkStatusbarClass	parent_class;
};
struct _XtmProcessStatusbar
{
	GtkHBox			parent;
	/*<private>*/
	XtmSettings *		settings;

	GtkWidget *		label_num_processes;
	GtkWidget *		label_cpu;
	GtkWidget *		label_memory;
	GtkWidget *		label_swap;

	gfloat			cpu;
	gchar			memory[64];
	gchar			swap[64];
	guint			num_processes;

	gboolean		dark_mode;
};
G_DEFINE_TYPE (XtmProcessStatusbar, xtm_process_statusbar, GTK_TYPE_BOX)

static void	xtm_process_statusbar_finalize			(GObject *object);
static void	xtm_process_statusbar_set_property		(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);



static void
xtm_process_statusbar_class_init (XtmProcessStatusbarClass *klass)
{
	GObjectClass *class = G_OBJECT_CLASS (klass);
	xtm_process_statusbar_parent_class = g_type_class_peek_parent (klass);
	class->finalize = xtm_process_statusbar_finalize;
	class->set_property = xtm_process_statusbar_set_property;
	g_object_class_install_property (class, PROP_CPU,
		g_param_spec_float ("cpu", "CPU", "CPU usage", 0, 100, 0, G_PARAM_CONSTRUCT|G_PARAM_WRITABLE));
	g_object_class_install_property (class, PROP_MEMORY,
		g_param_spec_string ("memory", "Memory", "Memory usage", "", G_PARAM_CONSTRUCT|G_PARAM_WRITABLE));
	g_object_class_install_property (class, PROP_SWAP,
		g_param_spec_string ("swap", "Swap", "Swap usage", "", G_PARAM_CONSTRUCT|G_PARAM_WRITABLE));
	g_object_class_install_property (class, PROP_SHOW_SWAP,
		g_param_spec_boolean ("show-swap", "ShowSwap", "Show or hide swap usage", TRUE, G_PARAM_WRITABLE));
	g_object_class_install_property (class, PROP_NUM_PROCESSES,
		g_param_spec_uint ("num-processes", "NumProcesses", "Number of processes", 0, G_MAXUINT, 0, G_PARAM_CONSTRUCT|G_PARAM_WRITABLE));
}

static void
xtm_process_statusbar_set_label_style (XtmProcessStatusbar *statusbar, GtkWidget *label)
{
	GtkStyleContext *context;
	gboolean has_dark_mode;

	context = gtk_widget_get_style_context (label);
	has_dark_mode = gtk_style_context_has_class (context, "dark");
	if (has_dark_mode && !statusbar->dark_mode)
		gtk_style_context_remove_class (context, "dark");
	if (!has_dark_mode && statusbar->dark_mode)
		gtk_style_context_add_class (context, "dark");
}

static void
xtm_process_statusbar_on_notify_theme_name (XtmProcessStatusbar *statusbar)
{
	statusbar->dark_mode = xtm_gtk_widget_is_dark_mode (GTK_WIDGET (statusbar));

	xtm_process_statusbar_set_label_style (statusbar, statusbar->label_cpu);
	xtm_process_statusbar_set_label_style (statusbar, statusbar->label_num_processes);
	xtm_process_statusbar_set_label_style (statusbar, statusbar->label_memory);
	xtm_process_statusbar_set_label_style (statusbar, statusbar->label_swap);
}

static void
xtm_process_statusbar_init (XtmProcessStatusbar *statusbar)
{
	GtkSettings *settings = gtk_settings_get_default ();
	GtkWidget *hbox, *hbox_cpu, *hbox_mem;
	GtkStyleContext *context;
	GtkCssProvider *provider;
	statusbar->settings = xtm_settings_get_default ();
	statusbar->dark_mode = xtm_gtk_widget_is_dark_mode (GTK_WIDGET (statusbar));

	g_signal_connect_swapped (settings, "notify::gtk-theme-name", G_CALLBACK (xtm_process_statusbar_on_notify_theme_name), statusbar);
	g_signal_connect (statusbar, "realize", G_CALLBACK (xtm_process_statusbar_on_notify_theme_name), NULL);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 16);
	hbox_cpu = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 16);
	hbox_mem = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 16);

	statusbar->label_cpu = gtk_label_new (NULL);
	gtk_box_pack_start (GTK_BOX (hbox_cpu), statusbar->label_cpu, TRUE, FALSE, 0);
	context  = gtk_widget_get_style_context (statusbar->label_cpu);
	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (provider, "* { color: #ff6e00; } .dark { color: #0091ff; }", -1, NULL);
	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (provider);

	statusbar->label_num_processes = gtk_label_new (NULL);
	gtk_box_pack_start (GTK_BOX (hbox_cpu), statusbar->label_num_processes, TRUE, FALSE, 0);

	statusbar->label_memory = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (statusbar->label_memory), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start (GTK_BOX (hbox_mem), statusbar->label_memory, TRUE, FALSE, 0);
	context  = gtk_widget_get_style_context (statusbar->label_memory);
	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (provider, "* { color: #cb386c; } .dark { color: #34c793; }", -1, NULL);
	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (provider);

	statusbar->label_swap = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (statusbar->label_swap), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start (GTK_BOX (hbox_mem), statusbar->label_swap, TRUE, FALSE, 0);
	context  = gtk_widget_get_style_context (statusbar->label_swap);
	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (provider, "* { color: #75324d; } .dark { color: #8acdb2; }", -1, NULL);
	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (provider);

	gtk_box_pack_start (GTK_BOX (hbox), hbox_cpu, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_mem, TRUE, TRUE, 0);
	gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);

	gtk_box_pack_start (GTK_BOX (statusbar), hbox, TRUE, TRUE, 0);

	xtm_process_statusbar_set_label_style (statusbar, statusbar->label_cpu);
	xtm_process_statusbar_set_label_style (statusbar, statusbar->label_num_processes);
	xtm_process_statusbar_set_label_style (statusbar, statusbar->label_memory);
	xtm_process_statusbar_set_label_style (statusbar, statusbar->label_swap);

	gtk_widget_show_all (hbox);
}

static void
xtm_process_statusbar_finalize (GObject *object)
{
	g_object_unref (XTM_PROCESS_STATUSBAR (object)->settings);
	G_OBJECT_CLASS (xtm_process_statusbar_parent_class)->finalize (object);
}

static gchar *
rounded_float_value (gfloat value, XtmSettings *settings)
{
	gboolean precision;
	g_object_get (settings, "more-precision", &precision, NULL);
	return g_strdup_printf ((precision) ? "%.2f" : "%.0f", value);
}

static void
xtm_process_statusbar_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	XtmProcessStatusbar *statusbar = XTM_PROCESS_STATUSBAR (object);
	gchar *text;
	gchar *float_value;

	switch (property_id)
	{
		case PROP_CPU:
		statusbar->cpu = g_value_get_float (value);
		float_value = rounded_float_value (statusbar->cpu, statusbar->settings);
		text = g_strdup_printf (_("CPU: %s%%"), float_value);
		gtk_label_set_text (GTK_LABEL (statusbar->label_cpu), text);
		g_free (float_value);
		g_free (text);
		break;

		case PROP_MEMORY:
		g_strlcpy(statusbar->memory, g_value_get_string (value), sizeof(statusbar->memory));
		text = g_strdup_printf (_("Memory: %s"), statusbar->memory);
		gtk_label_set_text (GTK_LABEL (statusbar->label_memory), text);
		gtk_widget_set_tooltip_text (statusbar->label_memory, text);
		g_free (text);
		break;

		case PROP_SWAP:
		g_strlcpy(statusbar->swap, g_value_get_string (value), sizeof(statusbar->swap));
		text = g_strdup_printf (_("Swap: %s"), statusbar->swap);
		gtk_label_set_text (GTK_LABEL (statusbar->label_swap), text);
		gtk_widget_set_tooltip_text (statusbar->label_swap, text);
		g_free (text);
		break;

		case PROP_SHOW_SWAP:
		if (g_value_get_boolean (value))
			gtk_widget_show (statusbar->label_swap);
		else
			gtk_widget_hide (statusbar->label_swap);
		break;

		case PROP_NUM_PROCESSES:
		statusbar->num_processes = g_value_get_uint (value);
		text = g_strdup_printf (_("Processes: %d"), statusbar->num_processes);
		gtk_label_set_text (GTK_LABEL (statusbar->label_num_processes), text);
		g_free (text);
		break;

		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}



GtkWidget *
xtm_process_statusbar_new (void)
{
	return g_object_new (XTM_TYPE_PROCESS_STATUSBAR, NULL);
}
