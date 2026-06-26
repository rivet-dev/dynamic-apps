/*
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 * Copyright (c) 2018 Rozhuk Ivan <rozhuk.im@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#include <sys/resource.h>

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gmodule.h>

#include "task-manager.h"
#ifdef HAVE_WNCK
#include <gdk/gdkx.h>
#include "app-manager.h"
#endif
#include "process-tree-view.h" /* for the columns of the model */
#include "settings.h"

#define TIMESTAMP_DELTA 4



static XtmSettings *settings = NULL;
static gboolean model_update_forced = FALSE;
static gboolean more_precision;
static gboolean full_cmdline;



typedef struct _XtmTaskManagerClass XtmTaskManagerClass;
struct _XtmTaskManagerClass
{
	GObjectClass		parent_class;
};
struct _XtmTaskManager
{
	GObject			parent;
	/*<private>*/
#ifdef HAVE_WNCK
	XtmAppManager *		app_manager;
#endif
	GtkTreeModel *		model;
	GArray *		tasks;
	gushort			cpu_count;
	gfloat			cpu_user;
	gfloat			cpu_system;
	guint64			memory_total;
	guint64			memory_available; /* free + cache + buffers + an-OS-specific-value */
	guint64			memory_free;
	guint64			memory_cache;
	guint64			memory_buffers;
	guint64			swap_total;
	guint64			swap_free;
};
G_DEFINE_TYPE (XtmTaskManager, xtm_task_manager, G_TYPE_OBJECT)

static void	xtm_task_manager_finalize			(GObject *object);

static void	setting_changed					(GObject *object, GParamSpec *pspec, XtmTaskManager *manager);
static void	model_add_task					(XtmTaskManager *manager, Task *task, glong timestamp);
static void	model_update_tree_iter				(XtmTaskManager *manager, GtkTreeIter *iter, glong timestamp, gboolean update_cmd_line, Task *task);
static void	model_mark_tree_iter_as_removed			(GtkTreeModel *model, GtkTreeIter *iter, glong timestamp);
static void	model_remove_tree_iter				(GtkTreeModel *model, GtkTreeIter *iter);
static gboolean	task_list_find_for_pid				(GArray *task_list, GPid pid, Task **task, guint *idx);
static glong	__current_timestamp				(void);



static void
xtm_task_manager_class_init (XtmTaskManagerClass *klass)
{
	GObjectClass *class = G_OBJECT_CLASS (klass);
	xtm_task_manager_parent_class = g_type_class_peek_parent (klass);
	class->finalize = xtm_task_manager_finalize;
}

static void
xtm_task_manager_init (XtmTaskManager *manager)
{
#ifdef HAVE_WNCK
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
		manager->app_manager = xtm_app_manager_new ();
	}
#endif
	manager->tasks = g_array_new (FALSE, FALSE, sizeof (Task));

	/* Listen to settings changes and force an update on the whole model */
	settings = xtm_settings_get_default ();
	g_object_get (settings, "more-precision", &more_precision, NULL);
	g_object_get (settings, "full-command-line", &full_cmdline, NULL);
	g_signal_connect (settings, "notify::more-precision", G_CALLBACK (setting_changed), manager);
	g_signal_connect (settings, "notify::full-command-line", G_CALLBACK (setting_changed), manager);
}

static void
xtm_task_manager_finalize (GObject *object)
{
	XtmTaskManager *manager = XTM_TASK_MANAGER (object);
	g_array_free (manager->tasks, TRUE);
#ifdef HAVE_WNCK
	if (manager->app_manager != NULL) {
		g_object_unref (manager->app_manager);
	}
#endif
	g_object_unref (settings);

	G_OBJECT_CLASS (xtm_task_manager_parent_class)->finalize (object);
}

static void
setting_changed (GObject *object, GParamSpec *pspec __unused, XtmTaskManager *manager __unused)
{
	g_object_get (object, "more-precision", &more_precision, NULL);
	g_object_get (object, "full-command-line", &full_cmdline, NULL);
	model_update_forced = TRUE;
}

static gchar *
pretty_cmdline (gchar *cmdline, gchar *comm)
{
	gunichar c;
	gchar *ch, *text_max, *text = g_strstrip (g_strdup (cmdline));
	gsize csize, text_size = (gsize)strlen (text);

	/* UTF-8 normalize. */
	do {
		for (ch = text, text_max = (text + text_size);
		     text_max > ch;
		     text_max = (text + text_size), ch = g_utf8_next_char(ch)) {
			c = g_utf8_get_char_validated(ch, -1); /* If use (text_max - ch) - result is worse. */
			if ((gunichar)-2 == c) {
				text_size = (gsize)(ch - text);
				(*ch) = 0;
				break;
			}
			if ((gunichar)-1 == c) {
				(*ch) = ' ';
				continue;
			}
			csize = (gsize)g_unichar_to_utf8(c, NULL);

			if (!g_unichar_isdefined(c) ||
			    !g_unichar_isprint(c) ||
			    (g_unichar_isspace(c) && (1 != csize || (' ' != (*ch) && '	' != (*ch)))) ||
			    g_unichar_ismark(c) ||
			    g_unichar_istitle(c) ||
			    g_unichar_iswide(c) ||
			    g_unichar_iszerowidth(c) ||
			    g_unichar_iscntrl(c)) {
				if (text_max < (ch + csize))
					break;
				memmove(ch, (ch + csize), (gsize)(text_max - (ch + csize)));
				text_size -= csize;
			}
		}
		text[text_size] = 0;
	} while (!g_utf8_validate(text, (gssize)text_size, NULL));

	if (!full_cmdline && text_size > 3)
	{
		/* Shorten full path to commands and wine applications */
		if (text[0] == '/' || (g_ascii_isupper (text[0]) && text[1] == ':' && text[2] == '\\'))
		{
			gchar *p = g_strstr_len (text, (gssize)text_size, comm);
			if (p != NULL)
			{
				memmove (text, p, (gsize)(text_size - (gsize)(p - text) + 1));
			}
		}
	}
	return text;
}

static void
_xtm_task_manager_set_model (XtmTaskManager *manager, GtkTreeModel *model)
{
	g_return_if_fail (XTM_IS_TASK_MANAGER (manager));
	g_return_if_fail (GTK_IS_TREE_MODEL (model));
	manager->model = model;
}

static void
model_add_task (XtmTaskManager *manager, Task *task, glong timestamp)
{
	GtkTreeIter iter;
	GtkTreeModel *model = manager->model;
	gchar *uid_name = get_uid_name (task->uid);

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		XTM_PTV_COLUMN_PID, task->pid,
		XTM_PTV_COLUMN_STATE, task->state,
		XTM_PTV_COLUMN_UID, task->uid,
		XTM_PTV_COLUMN_UID_STR, uid_name,
		XTM_PTV_COLUMN_TIMESTAMP, timestamp,
		-1);
	g_free(uid_name);
	model_update_tree_iter (manager, &iter, timestamp, TRUE, task);
}

static void
model_mark_tree_iter_as_removed (GtkTreeModel *model, GtkTreeIter *iter, glong timestamp)
{
	gtk_list_store_set (GTK_LIST_STORE (model), iter,
		XTM_PTV_COLUMN_CPU, 0.0,
		XTM_PTV_COLUMN_CPU_STR, "-",
		XTM_PTV_COLUMN_BACKGROUND, XTM_LEGEND_COLOR_TERMINATING,
		XTM_PTV_COLUMN_FOREGROUND, XTM_LEGEND_COLOR_NONE,
		XTM_PTV_COLUMN_TIMESTAMP, timestamp,
		-1);
}

static void
model_remove_tree_iter (GtkTreeModel *model, GtkTreeIter *iter)
{
	gtk_list_store_remove (GTK_LIST_STORE (model), iter);
}

static void
model_update_tree_iter (XtmTaskManager *manager, GtkTreeIter *iter, glong timestamp, gboolean update_cmd_line, Task *task)
{
	GtkTreeModel *model = manager->model;
	gchar *vsz, *rss, cpu[16];
	gchar value[14];
	glong old_timestamp;
	gchar *old_state;
	gchar *background, *foreground;
#ifdef HAVE_WNCK
	App *app = manager->app_manager != NULL ? xtm_app_manager_get_app_from_pid (manager->app_manager, task->pid) : NULL;
	cairo_surface_t *surface = NULL;
#endif

	vsz = g_format_size_full (task->vsz, G_FORMAT_SIZE_IEC_UNITS);
	rss = g_format_size_full (task->rss, G_FORMAT_SIZE_IEC_UNITS);

	g_snprintf (value, sizeof(value), (more_precision) ? "%.2f" : "%.0f", (task->cpu_user + task->cpu_system));
	g_snprintf (cpu, sizeof(cpu), _("%s%%"), value);

	/* Retrieve values for tweaking background/foreground color and updating content as needed */
	gtk_tree_model_get (model, iter, XTM_PTV_COLUMN_TIMESTAMP, &old_timestamp, XTM_PTV_COLUMN_STATE, &old_state,
			XTM_PTV_COLUMN_BACKGROUND, &background, XTM_PTV_COLUMN_FOREGROUND, &foreground,
#ifdef HAVE_WNCK
			XTM_PTV_COLUMN_ICON, &surface,
#endif
			-1);

#ifdef HAVE_WNCK
	if (surface != NULL)
		cairo_surface_destroy (surface);
	else if (app != NULL)
		gtk_list_store_set (GTK_LIST_STORE (model), iter, XTM_PTV_COLUMN_ICON, app->surface, -1);

	if (app != NULL && full_cmdline == FALSE)
	{
		gchar *cmdline = g_strdup (app->name);
		gtk_list_store_set (GTK_LIST_STORE (model), iter, XTM_PTV_COLUMN_COMMAND, cmdline, -1);
		g_free (cmdline);
	}
	else if (update_cmd_line)
#else
	if (update_cmd_line)
#endif
	{
		gchar *cmdline = pretty_cmdline (task->cmdline, task->name);
		gtk_list_store_set (GTK_LIST_STORE (model), iter, XTM_PTV_COLUMN_COMMAND, cmdline, -1);
		g_free (cmdline);
	}

	if (g_strcmp0 (task->state, old_state) != 0 && background == NULL)
	{
		/* Set yellow color for changing state */
		g_free (background);
		g_free (foreground);
		background = g_strdup (XTM_LEGEND_COLOR_CHANGING);
		foreground = g_strdup (XTM_LEGEND_COLOR_NONE);
		old_timestamp = timestamp - TIMESTAMP_DELTA + 3;
	}

	if (timestamp != 0 && (timestamp - old_timestamp) <= TIMESTAMP_DELTA)
	{
		/* Set green color for started task */
		background = (background == NULL) ? g_strdup (XTM_LEGEND_COLOR_STARTING) : background;
		foreground = (foreground == NULL) ? g_strdup (XTM_LEGEND_COLOR_NONE) : foreground;
	}
	else
	{
		/* Reset color */
		g_free (background);
		g_free (foreground);
		background = NULL;
		foreground = NULL;
	}

	gtk_list_store_set (GTK_LIST_STORE (model), iter,
		XTM_PTV_COLUMN_COMMAND_RAW, task->cmdline,
		XTM_PTV_COLUMN_PPID, task->ppid,
		XTM_PTV_COLUMN_STATE, task->state,
		XTM_PTV_COLUMN_VSZ, task->vsz,
		XTM_PTV_COLUMN_VSZ_STR, vsz,
		XTM_PTV_COLUMN_RSS, task->rss,
		XTM_PTV_COLUMN_RSS_STR, rss,
		XTM_PTV_COLUMN_CPU, (task->cpu_user + task->cpu_system),
		XTM_PTV_COLUMN_CPU_STR, cpu,
		XTM_PTV_COLUMN_PRIORITY, task->prio,
		XTM_PTV_COLUMN_BACKGROUND, background,
		XTM_PTV_COLUMN_FOREGROUND, foreground,
		XTM_PTV_COLUMN_TIMESTAMP, old_timestamp,
		-1);

	g_free (background);
	g_free (foreground);
	g_free (old_state);
	g_free (vsz);
	g_free (rss);
}

static gboolean
task_list_find_for_pid (GArray *task_list, GPid pid, Task **task, guint *idx)
{
	Task *task_tmp = NULL, tkey;

	if (task_list->data != NULL) {
		tkey.pid = pid;
		task_tmp = bsearch(&tkey, task_list->data, task_list->len, sizeof(Task), task_pid_compare_fn);
	}

	if (NULL != task)
	{
		(*task) = task_tmp;
	}
	if (NULL != idx)
	{
		if (NULL != task_tmp)
		{
			(*idx) = (guint)(((size_t)task_tmp - (size_t)task_list->data) / sizeof(Task));
		}
		else
		{
			(*idx) = (~((guint)0));
		}
	}
	return ((NULL != task_tmp));
}

static glong
__current_timestamp (void)
{
	gint64 tv = g_get_real_time ();
	return tv / G_USEC_PER_SEC;
}

XtmTaskManager *
xtm_task_manager_new (GtkTreeModel *model)
{
	XtmTaskManager *manager = g_object_new (XTM_TYPE_TASK_MANAGER, NULL);
	_xtm_task_manager_set_model (manager, model);
	return manager;
}

void
xtm_task_manager_get_system_info (XtmTaskManager *manager, guint *num_processes, gfloat *cpu,
				  guint64 *memory_used, guint64 *memory_total,
				  guint64 *swap_used, guint64 *swap_total)

{
	g_return_if_fail (XTM_IS_TASK_MANAGER (manager));

	/* Set number of processes */
	*num_processes = manager->tasks->len;

	/* Set memory and swap usage */
	get_memory_usage (&manager->memory_total, &manager->memory_available, &manager->memory_free, &manager->memory_cache, &manager->memory_buffers,
			&manager->swap_total, &manager->swap_free);

	*memory_used = manager->memory_total - manager->memory_available;
	*memory_total = manager->memory_total;
	*swap_used = manager->swap_total - manager->swap_free;
	*swap_total = manager->swap_total;

	/* Set CPU usage */
	get_cpu_usage (&manager->cpu_count, &manager->cpu_user, &manager->cpu_system);
	*cpu = manager->cpu_user + manager->cpu_system;
}

void
xtm_task_manager_get_swap_usage (XtmTaskManager *manager, guint64 *swap_free, guint64 *swap_total)
{
	g_return_if_fail (XTM_IS_TASK_MANAGER (manager));
	*swap_free = manager->swap_free;
	*swap_total = manager->swap_total;
}

const GArray *
xtm_task_manager_get_task_list (XtmTaskManager *manager)
{
	g_return_val_if_fail (XTM_IS_TASK_MANAGER (manager), NULL);
	xtm_task_manager_update_model (manager);
	return manager->tasks;
}

void
xtm_task_manager_update_model (XtmTaskManager *manager)
{
	GArray *array;
	guint i;
	GtkTreeIter iter;
	gboolean valid, need_update, update_cmd_line;
	glong timestamp;

	g_return_if_fail (XTM_IS_TASK_MANAGER (manager));

	/* First time fast refresh. */
	timestamp = ((manager->tasks->len == 0) ? 0 : __current_timestamp ());

	/* Retrieve new task list */
	array = g_array_new (FALSE, FALSE, sizeof (Task));
	get_task_list (array);

	/* Remove terminated tasks, mark to remove, update existing. */
	valid = gtk_tree_model_get_iter_first (manager->model, &iter);
	while (valid)
	{
		GPid pid;
		gchar *cpu_str;
		glong old_timestamp;
		gboolean found;
		Task *task, *task_new;
		GtkTreeIter cur_iter = iter;

		valid = gtk_tree_model_iter_next (manager->model, &iter);
		gtk_tree_model_get (manager->model, &cur_iter, XTM_PTV_COLUMN_CPU_STR, &cpu_str, XTM_PTV_COLUMN_TIMESTAMP, &old_timestamp, XTM_PTV_COLUMN_PID, &pid, -1);
		found = (g_strcmp0 (cpu_str, "-") == 0);
		g_free (cpu_str);
		if (found)
		{
			if ((timestamp - old_timestamp) > TIMESTAMP_DELTA) {
				G_DEBUG_FMT ("Remove old task %d", pid);
				model_remove_tree_iter (manager->model, &cur_iter);
			}
			continue;
		}

		/* Looking for new terminated tasks and check updates for existing. */
		found = task_list_find_for_pid (manager->tasks, pid, &task, &i);
		if (FALSE == found)
		{
			G_DEBUG_FMT ("Cached task not found for pid: %d", pid);
			continue;
		}
		found = task_list_find_for_pid (array, pid, &task_new, NULL);
		if (FALSE == found) /* Mark as removed. */
		{
			G_DEBUG_FMT ("Task %d '%s' terminated, marking as removed", pid, task->name);
			model_mark_tree_iter_as_removed (manager->model, &cur_iter, timestamp);
			g_array_remove_index (manager->tasks, i);
			continue;
		}

		/* Task alive, check for update. */
		need_update = FALSE;
		update_cmd_line = FALSE;

		/* Update the model (with the rest) only if needed, this keeps the CPU cool */
		if (model_update_forced || 0 != memcmp(task, task_new, sizeof(Task)))
		{
			need_update = TRUE;
			/* Update command name if needed (can happen) */
			update_cmd_line = (model_update_forced || g_strcmp0 (task->cmdline, task_new->cmdline));
			memcpy(task, task_new, sizeof(Task));
		}
		else /* Update color if needed */
		{
			gchar *color;

			gtk_tree_model_get (manager->model, &cur_iter, XTM_PTV_COLUMN_BACKGROUND, &color, -1);
			if (color != NULL && (timestamp - old_timestamp) > TIMESTAMP_DELTA)
			{
				need_update = TRUE;
				G_DEBUG_FMT ("Remove color from running PID %d", pid);
			}
			g_free (color);
		}

		if (need_update)
		{
			model_update_tree_iter (manager, &cur_iter, timestamp, update_cmd_line, task);
		}
	}

	/* Append started tasks and update existing ones */
	for (i = 0; i < array->len; i++)
	{
		Task *tasktmp = &g_array_index (array, Task, i);

		if (task_list_find_for_pid (manager->tasks, tasktmp->pid, NULL, NULL))
			continue;
		model_add_task (manager, tasktmp, timestamp);
		/* XXX: add bininsert() here. */
		g_array_append_val (manager->tasks, *tasktmp);
		g_array_sort (manager->tasks, task_pid_compare_fn);
		G_DEBUG_FMT ("Add new task %d %s", tasktmp->pid, tasktmp->name);
	}

	g_array_free (array, TRUE);
	model_update_forced = FALSE;

	return;
}


gchar *
get_uid_name (guint uid)
{
	int error;
	struct passwd *pw = NULL, pwd_buf;
	char buf[4096];

	memset(buf, 0, sizeof(buf));
	error = getpwuid_r(uid, &pwd_buf, buf, sizeof(buf), &pw);

	return (g_strdup ((0 == error && pw != NULL) ? pw->pw_name : "nobody"));
}

gboolean
send_signal_to_pid (GPid pid, gint xtm_signal)
{
	gint sig;
	gint res;
	switch (xtm_signal)
	{
		case XTM_SIGNAL_TERMINATE:
			sig = SIGTERM;
			break;
		case XTM_SIGNAL_STOP:
			sig = SIGSTOP;
			break;
		case XTM_SIGNAL_CONTINUE:
			sig = SIGCONT;
			break;
		case XTM_SIGNAL_KILL:
			sig = SIGKILL;
			break;
		default:
			return TRUE;
	}
	res = kill (pid, sig);
	return (res == 0) ? TRUE : FALSE;
}

gboolean
set_priority_to_pid (GPid pid, gint priority)
{
	gint prio;
	gint res;
	switch (priority)
	{
		case XTM_PRIORITY_VERY_LOW:
			prio = 15;
			break;
		case XTM_PRIORITY_LOW:
			prio = 5;
			break;
		case XTM_PRIORITY_NORMAL:
			prio = 0;
			break;
		case XTM_PRIORITY_HIGH:
			prio = -5;
			break;
		case XTM_PRIORITY_VERY_HIGH:
			prio = -15;
			break;
		default:
			return TRUE;
	}
	res = setpriority (PRIO_PROCESS, pid, prio);
	return (res == 0) ? TRUE : FALSE;
}

gint
task_pid_compare_fn(gconstpointer a, gconstpointer b)
{
	return (((const Task*)a)->pid - ((const Task*)b)->pid);
}

/* About the constants used below, see for example: https://stackoverflow.com/a/596243 */
gboolean
xtm_gtk_widget_is_dark_mode (GtkWidget *widget)
{
	GtkStyleContext *context;
	GdkRGBA *color;
	double luminosity;
	gboolean dark_mode;

	context = gtk_widget_get_style_context (gtk_widget_get_toplevel (widget));
	gtk_style_context_get (context, GTK_STATE_FLAG_NORMAL, GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &color, NULL);
	luminosity = (0.2126 * color->red + 0.7152 * color->green + 0.0722 * color->blue);
	gdk_rgba_free (color);
	dark_mode = (luminosity < 0.5);

	return dark_mode;
}
