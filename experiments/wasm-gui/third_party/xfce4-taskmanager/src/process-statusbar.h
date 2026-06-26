/*
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PROCESS_STATUSBAR_H
#define PROCESS_STATUSBAR_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <gtk/gtk.h>

#define XTM_TYPE_PROCESS_STATUSBAR		(xtm_process_statusbar_get_type ())
#define XTM_PROCESS_STATUSBAR(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), XTM_TYPE_PROCESS_STATUSBAR, XtmProcessStatusbar))
#define XTM_PROCESS_STATUSBAR_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), XTM_TYPE_PROCESS_STATUSBAR, XtmProcessStatusbarClass))
#define XTM_IS_PROCESS_STATUSBAR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), XTM_TYPE_PROCESS_STATUSBAR))
#define XTM_IS_PROCESS_STATUSBAR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), XTM_TYPE_PROCESS_STATUSBAR))
#define XTM_PROCESS_STATUSBAR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), XTM_TYPE_PROCESS_STATUSBAR, XtmProcessStatusbarClass))

typedef struct _XtmProcessStatusbar XtmProcessStatusbar;

GType		xtm_process_statusbar_get_type			(void);
GtkWidget *	xtm_process_statusbar_new			(void);

#endif /* !PROCESS_STATUSBAR_H */
