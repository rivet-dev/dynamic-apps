/*
 * Copyright (c) 2010  Mike Massonnet <mmassonnet@xfce.org>
 * Copyright (c) 2009  Peter Tribble <peter.tribble@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <kstat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <procfs.h>
#include <sys/procfs.h>
#include <sys/swap.h>

#include <glib.h>

#include "task-manager.h"

static kstat_ctl_t *kc;
static gushort _cpu_count = 0;
static gulong ticks_total_delta = 0;

static void
init_stats (void)
{
	kc = kstat_open ();
}

gboolean
get_memory_usage (guint64 *memory_total, guint64 *memory_available, guint64 *memory_free, guint64 *memory_cache, guint64 *memory_buffers, guint64 *swap_total, guint64 *swap_free)
{
	kstat_t *ksp;
	kstat_named_t *knp;
	gint n;

	if (!kc)
		init_stats();

	if (!(ksp = kstat_lookup (kc, "unix", 0, "system_pages")))
		return FALSE;
	kstat_read (kc, ksp, NULL);
	knp = kstat_data_lookup (ksp, "physmem");
	*memory_total = getpagesize () * knp->value.ui64;
	knp = kstat_data_lookup (ksp, "freemem");
	*memory_free = getpagesize () * knp->value.ui64;
	*memory_cache = 0;
	*memory_buffers = 0;
	*memory_available = *memory_free + *memory_cache + *memory_buffers;

	*swap_total = *swap_free = 0;
	if ((n = swapctl (SC_GETNSWP, NULL)) > 0)
	{
		struct swaptable *st;
		struct swapent *swapent;
		gchar path[MAXPATHLEN];
		gint i;

		if ((st = malloc (sizeof (int) + n * sizeof (swapent_t))) == NULL)
			return FALSE;
		st->swt_n = n;

		swapent = st->swt_ent;
		for (i = 0; i < n; i++, swapent++)
			swapent->ste_path = path;

		if ((swapctl (SC_LIST, st)) == -1)
		{
			free (st);
			return FALSE;
		}

		swapent = st->swt_ent;
		for (i = 0; i < n; i++, swapent++)
		{
			*swap_total += swapent->ste_pages * getpagesize ();
			*swap_free += swapent->ste_free * getpagesize ();
		}

		free (st);
	}

	return TRUE;
}

static void
get_cpu_percent (GPid pid, gulong ticks_user, gfloat *cpu_user, gulong ticks_system, gfloat *cpu_system)
{
	static GHashTable *hash_cpu_user = NULL;
	static GHashTable *hash_cpu_system = NULL;
	gulong ticks_user_old, ticks_system_old;

	if (hash_cpu_user == NULL)
	{
		hash_cpu_user = g_hash_table_new (NULL, NULL);
		hash_cpu_system = g_hash_table_new (NULL, NULL);
	}

	ticks_user_old = GPOINTER_TO_UINT (g_hash_table_lookup (hash_cpu_user, GINT_TO_POINTER (pid)));
	ticks_system_old = GPOINTER_TO_UINT (g_hash_table_lookup (hash_cpu_system, GINT_TO_POINTER (pid)));
	g_hash_table_insert (hash_cpu_user, GINT_TO_POINTER (pid), GUINT_TO_POINTER (ticks_user));
	g_hash_table_insert (hash_cpu_system, GINT_TO_POINTER (pid), GUINT_TO_POINTER (ticks_system));

	if (ticks_user < ticks_user_old || ticks_system < ticks_system_old)
		return;

	if (_cpu_count > 0 && ticks_total_delta > 0)
	{
		*cpu_user = (((ticks_user - ticks_user_old) * 100.0f) / (float)ticks_total_delta);
		*cpu_system = (((ticks_system - ticks_system_old) * 100.0f) / (float)ticks_total_delta);
	}
	else
	{
		*cpu_user = *cpu_system = 0.0f;
	}
}

gboolean
get_cpu_usage (gushort *cpu_count, gfloat *cpu_user, gfloat *cpu_system)
{
	kstat_t *ksp;
	kstat_named_t *knp;
	static gulong ticks_total = 0, ticks_total_old = 0;
	gulong ticks_user = 0, ticks_system = 0;

	if (!kc)
		init_stats ();

	_cpu_count = 0;
	kstat_chain_update (kc);

	ticks_total_old = ticks_total;
	ticks_total = 0;

	for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next)
	{
		if (!g_strcmp0 (ksp->ks_module, "cpu_info"))
		{
			_cpu_count += 1;
		}
		else if (!g_strcmp0 (ksp->ks_module, "cpu") && !g_strcmp0 (ksp->ks_name, "sys"))
		{
			kstat_read (kc, ksp, NULL);
			knp = kstat_data_lookup (ksp, "cpu_nsec_user");
			ticks_user += knp->value.ul / 100000;
			knp = kstat_data_lookup (ksp, "cpu_nsec_kernel");
			ticks_system += knp->value.ul / 100000;
			knp = kstat_data_lookup (ksp, "cpu_nsec_intr");
			ticks_system += knp->value.ul / 100000;
			knp = kstat_data_lookup (ksp, "cpu_nsec_idle");
			ticks_total += knp->value.ul / 100000;
			ticks_total += ticks_user + ticks_system;
		}
	}

	if (ticks_total > ticks_total_old)
		ticks_total_delta = ticks_total - ticks_total_old;

	get_cpu_percent (0, ticks_user, cpu_user, ticks_system, cpu_system);
	*cpu_count = _cpu_count;

        return TRUE;
}

static gboolean
get_task_details (GPid pid, Task *task)
{
	FILE *file;
	gchar filename[96];
	psinfo_t process;

	snprintf (filename, sizeof(filename), "/proc/%d/psinfo", pid);
	if ((file = fopen (filename, "r")) == NULL)
		return FALSE;

	if (fread (&process, sizeof (psinfo_t), 1, file) != 1)
	{
		fclose (file);
		return FALSE;
	}

	memset(task, 0, sizeof(Task));
	task->pid = process.pr_pid;
	task->ppid = process.pr_ppid;
	g_strlcpy (task->name, process.pr_fname, sizeof(task->name));
	snprintf (task->cmdline, sizeof(task->cmdline), "%s", process.pr_psargs);
	snprintf (task->state, sizeof(task->state), "%c", process.pr_lwp.pr_sname);
	task->vsz = (guint64)process.pr_size * 1024;
	task->rss = (guint64)process.pr_rssize * 1024;
	task->prio = process.pr_lwp.pr_pri;
	task->uid = (guint)process.pr_uid;
	get_cpu_percent (task->pid, (process.pr_time.tv_sec * 1000 + process.pr_time.tv_nsec / 100000), &task->cpu_user, 0, &task->cpu_system);

	fclose (file);

	return TRUE;
}

gboolean
get_task_list (GArray *task_list)
{
	GDir *dir;
	const gchar *name;
	GPid pid;
	Task task;

	if ((dir = g_dir_open ("/proc", 0, NULL)) == NULL)
		return FALSE;

	while ((name = g_dir_read_name(dir)) != NULL)
	{
		if ((pid = (GPid)g_ascii_strtoull (name, NULL, 0)) > 0)
		{
			if (get_task_details (pid, &task))
				g_array_append_val(task_list, task);
		}
	}

	g_dir_close (dir);

	g_array_sort (task_list, task_pid_compare_fn);

	return FALSE;
}

gboolean
pid_is_sleeping (GPid pid)
{
	FILE *file;
	gchar filename[96];
	gchar state[2];
	psinfo_t process;

	snprintf (filename, sizeof(filename), "/proc/%d/psinfo", pid);
	if ((file = fopen (filename, "r")) == NULL)
		return FALSE;

	if (fread (&process, sizeof (psinfo_t), 1, file) != 1)
	{
		fclose (file);
		return FALSE;
	}

	snprintf (state, sizeof(state), "%c", process.pr_lwp.pr_sname);
	fclose (file);

	return (state[0] == 'T') ? TRUE : FALSE;
}

