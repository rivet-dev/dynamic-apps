/*
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <gtk/gtk.h>

/**
 * Legend colors
 */
#define XTM_LEGEND_COLOR_NONE "#000000"
#define XTM_LEGEND_COLOR_STARTING "#aed581"
#define XTM_LEGEND_COLOR_CHANGING "#fff176"
#define XTM_LEGEND_COLOR_TERMINATING "#e57373"

/**
 * Task struct used as elements of a task list GArray.
 */

typedef struct _Task Task;
struct _Task
{
	guint		uid;
	GPid		pid;
	GPid		ppid;
	gchar		name[256];
	gchar		cmdline[1024];
	gchar		state[16];
	gfloat		cpu_user;
	gfloat		cpu_system;
	guint64		vsz;
	guint64		rss;
	gint		prio;
};

/**
 * OS specific implementation.
 *
 * memory_available = free + cache + buffers + an-OS-specific-value
 */

gboolean	get_memory_usage	(guint64 *memory_total, guint64 *memory_available, guint64 *memory_free, guint64 *memory_cache, guint64 *memory_buffers, guint64 *swap_total, guint64 *swap_free);
gboolean	get_cpu_usage		(gushort *cpu_count, gfloat *cpu_user, gfloat *cpu_system);
gboolean	get_task_list		(GArray *task_list);
gboolean	pid_is_sleeping		(GPid pid);

/**
 * GObject class used to update the graphical widgets.
 */

#define XTM_TYPE_TASK_MANAGER			(xtm_task_manager_get_type ())
#define XTM_TASK_MANAGER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), XTM_TYPE_TASK_MANAGER, XtmTaskManager))
#define XTM_TASK_MANAGER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), XTM_TYPE_TASK_MANAGER, XtmTaskManagerClass))
#define XTM_IS_TASK_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), XTM_TYPE_TASK_MANAGER))
#define XTM_IS_TASK_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), XTM_TYPE_TASK_MANAGER))
#define XTM_TASK_MANAGER_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), XTM_TYPE_TASK_MANAGER, XtmTaskManagerClass))

typedef struct _XtmTaskManager XtmTaskManager;

GType			xtm_task_manager_get_type			(void);
XtmTaskManager *	xtm_task_manager_new				(GtkTreeModel *model);
void			xtm_task_manager_get_system_info		(XtmTaskManager *manager, guint *num_processes, gfloat *cpu,
									 guint64 *memory_used, guint64 *memory_total,
									 guint64 *swap_used, guint64 *swap_total);
void			xtm_task_manager_get_swap_usage			(XtmTaskManager *manager, guint64 *swap_free, guint64 *swap_total);
const GArray *		xtm_task_manager_get_task_list			(XtmTaskManager *manager);
void			xtm_task_manager_update_model			(XtmTaskManager *manager);

/**
 * Helper functions.
 */

enum
{
	XTM_SIGNAL_TERMINATE = 0,
	XTM_SIGNAL_STOP,
	XTM_SIGNAL_CONTINUE,
	XTM_SIGNAL_KILL,
};

enum
{
	XTM_PRIORITY_VERY_LOW = 0,
	XTM_PRIORITY_LOW,
	XTM_PRIORITY_NORMAL,
	XTM_PRIORITY_HIGH,
	XTM_PRIORITY_VERY_HIGH,
};

gchar *		get_uid_name		(guint uid);
gboolean	send_signal_to_pid	(GPid pid, gint xtm_signal);
gint		task_pid_compare_fn	(gconstpointer a, gconstpointer b);
gboolean	set_priority_to_pid	(GPid pid, gint priority);


#ifndef __unused
#	define __unused				__attribute__((__unused__))
#endif

#ifdef DEBUG
#	define G_DEBUG_FMT(fmt, args...)	g_debug((fmt), ##args)
#else
#	define G_DEBUG_FMT(fmt, args...)
#endif

gboolean	xtm_gtk_widget_is_dark_mode (GtkWidget *widget);

#endif /* !TASK_MANAGER_H */
