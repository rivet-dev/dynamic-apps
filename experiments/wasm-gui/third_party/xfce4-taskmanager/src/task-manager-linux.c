/*
 * Copyright (c) 2008-2010  Mike Massonnet <mmassonnet@xfce.org>
 * Copyright (c) 2006  Johannes Zellner <webmaster@nebulon.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include "task-manager.h"

static gushort _cpu_count = 0;
static gulong jiffies_total_delta = 0;

gboolean
get_memory_usage (guint64 *memory_total, guint64 *memory_available, guint64 *memory_free, guint64 *memory_cache, guint64 *memory_buffers, guint64 *swap_total, guint64 *swap_free)
{
	FILE *file;
	gchar *filename = "/proc/meminfo";
	guint64 mem_total = 0,
	       mem_free = 0,
	       mem_avail = 0,
	       mem_cached = 0,
	       mem_buffers = 0,
	       swp_total = 0,
	       swp_free = 0;

	if ((file = fopen (filename, "r")) != NULL)
	{
		gint found = 0;
		gchar buffer[256];
		while (found < 7 && fgets (buffer, sizeof(buffer), file) != NULL)
		{
			found += !mem_total ? sscanf (buffer, "MemTotal:\t%lu kB", &mem_total) : 0;
			found += !mem_free ? sscanf (buffer, "MemFree:\t%lu kB", &mem_free) : 0;
			found += !mem_avail ? sscanf (buffer, "MemAvailable:\t%lu kB", &mem_avail) : 0; /* Since Linux 3.14 */
			found += !mem_cached ? sscanf (buffer, "Cached:\t%lu kB", &mem_cached) : 0;
			found += !mem_buffers ? sscanf (buffer, "Buffers:\t%lu kB", &mem_buffers) : 0;
			found += !swp_total ? sscanf (buffer, "SwapTotal:\t%lu kB", &swp_total) : 0;
			found += !swp_free ? sscanf (buffer, "SwapFree:\t%lu kB", &swp_free) : 0;
		}
		fclose (file);
	}

	if (mem_avail == 0)
	{
		mem_avail = mem_free + mem_cached + mem_buffers;
	}

	*memory_total = mem_total * 1024;
	*memory_available = mem_avail * 1024;
	*memory_free = mem_free * 1024;
	*memory_cache = mem_cached * 1024;
	*memory_buffers = mem_buffers * 1024;
	*swap_total = swp_total * 1024;
	*swap_free = swp_free * 1024;

	return file ? TRUE : FALSE;
}

gboolean
get_cpu_usage (gushort *cpu_count, gfloat *cpu_user, gfloat *cpu_system)
{
	FILE *file;
	gchar *filename = "/proc/stat";
	gchar buffer[1024];
	static gulong jiffies_user = 0, jiffies_system = 0, jiffies_total = 0;
	static gulong jiffies_user_old = 0, jiffies_system_old = 0, jiffies_total_old = 0;
	gulong user = 0, user_nice = 0, system = 0, idle = 0;

	if ((file = fopen (filename, "r")) == NULL)
		return FALSE;
	if (fgets (buffer, sizeof(buffer), file) == NULL)
	{
		fclose (file);
		return FALSE;
	}

	sscanf (buffer, "cpu\t%lu %lu %lu %lu", &user, &user_nice, &system, &idle);

	if (_cpu_count == 0)
	{
		while (fgets (buffer, sizeof(buffer), file) != NULL)
		{
			if (buffer[0] != 'c' && buffer[1] != 'p' && buffer[2] != 'u')
				break;
			_cpu_count += 1;
		}
		if (_cpu_count == 0)
			_cpu_count = 1;
	}

	fclose (file);

	jiffies_user_old = jiffies_user;
	jiffies_system_old = jiffies_system;
	jiffies_total_old = jiffies_total;

	jiffies_user = user + user_nice;
	jiffies_system = system;
	jiffies_total = jiffies_user + jiffies_system + idle;

	*cpu_user = *cpu_system = 0.0f;
	if (jiffies_total > jiffies_total_old)
	{
		jiffies_total_delta = jiffies_total - jiffies_total_old;
		*cpu_user = (((jiffies_user - jiffies_user_old) * 100.0f) / (float)jiffies_total_delta);
		*cpu_system = (((jiffies_system - jiffies_system_old) * 100.0f) / (float)jiffies_total_delta);
	}
	*cpu_count = _cpu_count;

	return TRUE;
}

static inline int get_pagesize (void)
{
	static int pagesize = 0;
	if (pagesize == 0)
	{
		pagesize = sysconf (_SC_PAGESIZE);
		if (pagesize == 0)
			pagesize = 4096;
	}
	return pagesize;
}

static gboolean
get_task_cmdline (Task *task)
{
	FILE *file;
	gchar filename[96];
	gint i;
	gint c;

	snprintf (filename, sizeof(filename), "/proc/%i/cmdline", task->pid);
	if ((file = fopen (filename, "r")) == NULL)
		return FALSE;

	/* Read full command byte per byte until EOF */
	for (i = 0; (c = fgetc (file)) != EOF && i < (gint)sizeof (task->cmdline) - 1; i++)
		task->cmdline[i] = (c == '\0') ? ' ' : (gchar)c;
	task->cmdline[i] = '\0';
	if (i > 0 && task->cmdline[i-1] == ' ')
		task->cmdline[i-1] = '\0';
	fclose (file);

	/* Kernel processes don't have a cmdline nor an exec path */
	if (i == 0)
	{
		size_t len = strlen (task->name);
		g_strlcpy (&task->cmdline[1], task->name, len + 1);
		task->cmdline[0] = '[';
		task->cmdline[len+1] = ']';
		task->cmdline[len+2] = '\0';
	}

	return TRUE;
}

static void
get_cpu_percent (GPid pid, gulong jiffies_user, gfloat *cpu_user, gulong jiffies_system, gfloat *cpu_system)
{
	static GHashTable *hash_cpu_user = NULL;
	static GHashTable *hash_cpu_system = NULL;
	gulong jiffies_user_old, jiffies_system_old;

	if (hash_cpu_user == NULL)
	{
		hash_cpu_user = g_hash_table_new (NULL, NULL);
		hash_cpu_system = g_hash_table_new (NULL, NULL);
	}

	jiffies_user_old = GPOINTER_TO_UINT (g_hash_table_lookup (hash_cpu_user, GINT_TO_POINTER (pid)));
	jiffies_system_old = GPOINTER_TO_UINT (g_hash_table_lookup (hash_cpu_system, GINT_TO_POINTER (pid)));
	g_hash_table_insert (hash_cpu_user, GINT_TO_POINTER (pid), GUINT_TO_POINTER (jiffies_user));
	g_hash_table_insert (hash_cpu_system, GINT_TO_POINTER (pid), GUINT_TO_POINTER (jiffies_system));

	if (jiffies_user < jiffies_user_old || jiffies_system < jiffies_system_old)
		return;

	if (_cpu_count > 0 && jiffies_total_delta > 0)
	{
		*cpu_user = (((jiffies_user - jiffies_user_old) * 100.0f) / (float)jiffies_total_delta);
		*cpu_system = (((jiffies_system - jiffies_system_old) * 100.0f) / (float)jiffies_total_delta);
	}
	else
	{
		*cpu_user = *cpu_system = 0.0f;
	}
}

static gboolean
get_task_details (GPid pid, Task *task)
{
	FILE *file;
	gchar filename[96];
	gchar buffer[1024];

	snprintf (filename, sizeof(filename), "/proc/%d/stat", pid);
	if ((file = fopen (filename, "r")) == NULL)
		return FALSE;
	if (fgets (buffer, sizeof(buffer), file) == NULL)
	{
		fclose (file);
		return FALSE;
	}
	fclose (file);

	memset(task, 0, sizeof(Task));

	/* Scanning the short process name is unreliable with scanf when it contains
	 * spaces, retrieve it manually and fill the buffer */
	{
		gchar *p1, *po, *p2;
		guint i = 0;
		p1 = po = g_strstr_len (buffer, -1, "(");
		p2 = g_strrstr (buffer, ")");
		while (po <= p2)
		{
			if (po > p1 && po < p2)
			{
				task->name[i++] = *po;
				task->name[i] = '\0';
			}
			*po = 'x';
			po++;
		}
	}

	/* Parse the stat file */
	{
		gchar dummy[256];
		gint idummy;
		gulong jiffies_user = 0, jiffies_system = 0;

		sscanf(buffer, "%i %255s %1s %i %i %i %i %i %255s %255s %255s %255s %255s %lu %lu %i %i %i %d %i %i %i %llu %llu %255s %255s %255s %i %255s %255s %255s %255s %255s %255s %255s %255s %255s %255s %i %255s %255s",
			&task->pid,	// processid
			 dummy,		// processname
			task->state,	// processstate
			&task->ppid,	// parentid
			 &idummy,	// processs groupid

			 &idummy,	// session id
			 &idummy,	// tty id
			 &idummy,	// tpgid the process group ID of the process running on tty of the process
			 dummy,		// flags
			 dummy,		// minflt minor faults the process has maid

			 dummy,		// cminflt
			 dummy,		// majflt
			 dummy,		// cmajflt
			&jiffies_user,	// utime the number of jiffies that this process has scheduled in user mode
			&jiffies_system,// stime " system mode

			 &idummy,	// cutime " waited for children in user mode
			 &idummy,	// cstime " system mode
			 &idummy,	// priority (nice value + fifteen)
			 &task->prio, // nice range from 19 to -19
			 &idummy,	// hardcoded 0

			 &idummy,	// itrealvalue time in jiffies to next SIGALRM send to this process
			 &idummy,	// starttime jiffies the process startet after system boot
			(unsigned long long*)&task->vsz, // vsize in bytes
			(unsigned long long*)&task->rss, // rss (number of pages in real memory)
			 dummy,		// rlim limit in bytes for rss

			 dummy,		// startcode
			 dummy,		// endcode
			 &idummy,	// startstack
			 dummy,		// kstkesp value of esp (stack pointer)
			 dummy,		// kstkeip value of EIP (instruction pointer)

			 dummy,		// signal. bitmap of pending signals
			 dummy,		// blocked: bitmap of blocked signals
			 dummy,		// sigignore: bitmap of ignored signals
			 dummy,		// sigcatch: bitmap of catched signals
			 dummy,		// wchan

			 dummy,		// nswap
			 dummy,		// cnswap
			 dummy,		// exit_signal
			 &idummy,	// CPU number last executed on
			 dummy,

			 dummy
		);

		task->rss *= get_pagesize ();
		get_cpu_percent (task->pid, jiffies_user, &task->cpu_user, jiffies_system, &task->cpu_system);
	}

	/* Parse the status file: it contains the UIDs */
	{
		guint dummy;

		snprintf(filename, sizeof (filename), "/proc/%d/status", pid);
		if ((file = fopen (filename, "r")) == NULL)
			return FALSE;

		while (fgets (buffer, sizeof(buffer), file) != NULL)
		{
			if (sscanf (buffer, "Uid:\t%u\t%u\t%u\t%u", &dummy, &task->uid, &dummy, &dummy) == 1) // UIDs in order: real, effective, saved set, and filesystem
				break;
		}
		fclose (file);
	}

	/* Read the full command line */
	if (!get_task_cmdline (task))
		return FALSE;

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
			{
				g_array_append_val (task_list, task);
			}
		}
	}

	g_dir_close (dir);

	g_array_sort (task_list, task_pid_compare_fn);

	return TRUE;
}

gboolean
pid_is_sleeping (GPid pid)
{
	FILE *file;
	gchar filename[96];
	gchar buffer[1024];
	gchar state[2];

	snprintf (filename, sizeof(filename), "/proc/%i/status", pid);
	if ((file = fopen (filename, "r")) == NULL)
		return FALSE;

	while (fgets (buffer, sizeof(buffer), file) != NULL)
	{
		if (sscanf (buffer, "State:\t%1s", state) > 0)
			break;
	}
	fclose (file);

	return (state[0] == 'T') ? TRUE : FALSE;
}
