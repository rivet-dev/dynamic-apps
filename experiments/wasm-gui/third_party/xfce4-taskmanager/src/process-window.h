/*
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PROCESS_WINDOW_H
#define PROCESS_WINDOW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <gtk/gtk.h>

#define XTM_TYPE_PROCESS_WINDOW			(xtm_process_window_get_type ())
#define XTM_PROCESS_WINDOW(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), XTM_TYPE_PROCESS_WINDOW, XtmProcessWindow))
#define XTM_PROCESS_WINDOW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), XTM_TYPE_PROCESS_WINDOW, XtmProcessWindowClass))
#define XTM_IS_PROCESS_WINDOW(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), XTM_TYPE_PROCESS_WINDOW))
#define XTM_IS_PROCESS_WINDOW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), XTM_TYPE_PROCESS_WINDOW))
#define XTM_PROCESS_WINDOW_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), XTM_TYPE_PROCESS_WINDOW, XtmProcessWindowClass))
#define XTM_SHOW_MESSAGE(type, title, message, ...) { \
	GtkWidget *dialog = gtk_message_dialog_new (NULL, 0, type, GTK_BUTTONS_OK, title); \
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), message , ## __VA_ARGS__ ); \
	gtk_window_set_title (GTK_WINDOW (dialog), _("Task Manager")); \
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE); \
	gtk_dialog_run (GTK_DIALOG (dialog)); \
	gtk_widget_destroy (dialog); \
}

typedef struct _XtmProcessWindow XtmProcessWindow;

GType		xtm_process_window_get_type			(void);
GtkWidget *	xtm_process_window_new				(void);
void		xtm_process_window_settings_init		(XtmProcessWindow *window, XfconfChannel *channel);
void		xtm_process_window_show				(GtkWidget *widget);
GtkTreeModel *	xtm_process_window_get_model			(XtmProcessWindow *window);
void		xtm_process_window_set_system_info		(XtmProcessWindow *window, guint num_processes, gfloat cpu, gfloat memory, gchar* memory_str, gfloat swap, gchar* swap_str);
void		xtm_process_window_show_swap_usage		(XtmProcessWindow *window, gboolean show_swap_usage);

#endif /* !PROCESS_WINDOW_H */
