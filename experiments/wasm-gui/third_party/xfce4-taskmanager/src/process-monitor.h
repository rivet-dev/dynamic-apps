/*
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PROCESS_MONITOR_H
#define PROCESS_MONITOR_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>

#define XTM_TYPE_PROCESS_MONITOR		(xtm_process_monitor_get_type ())
#define XTM_PROCESS_MONITOR(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), XTM_TYPE_PROCESS_MONITOR, XtmProcessMonitor))
#define XTM_PROCESS_MONITOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), XTM_TYPE_PROCESS_MONITOR, XtmProcessMonitorClass))
#define XTM_IS_PROCESS_MONITOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), XTM_TYPE_PROCESS_MONITOR))
#define XTM_IS_PROCESS_MONITOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), XTM_TYPE_PROCESS_MONITOR))
#define XTM_PROCESS_MONITOR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), XTM_TYPE_PROCESS_MONITOR, XtmProcessMonitorClass))

typedef struct _XtmProcessMonitor XtmProcessMonitor;

GType		xtm_process_monitor_get_type			(void);
GtkWidget *	xtm_process_monitor_new				(void);
void		xtm_process_monitor_add_peak			(XtmProcessMonitor *monitor, gfloat peak, gfloat peak_swap);
void		xtm_process_monitor_set_step_size		(XtmProcessMonitor *monitor, gfloat step_size);
void		xtm_process_monitor_set_type			(XtmProcessMonitor *monitor, gint type);
void		xtm_process_monitor_clear			(XtmProcessMonitor *monitor);

#endif /* !PROCESS_MONITOR_H */
