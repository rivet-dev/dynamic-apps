/*
 * Copyright (c) 2010  Mike Massonnet <mmassonnet@xfce.org>
 * Copyright (c) 2006  Oliver Lehmann <oliver@FreeBSD.org>
 * Copyright (c) 2018 Rozhuk Ivan <rozhuk.im@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <kvm.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <fcntl.h>
#include <paths.h>
#include <unistd.h>
#include <string.h>
#if defined(__FreeBSD_version) && __FreeBSD_version >= 900044
#include <sys/vmmeter.h>
#endif

#include <glib.h>

#include "task-manager.h"

static const gchar ki_stat2state[] = {
	' ', /* - */
	'R', /* SIDL */
	'R', /* SRUN */
	'S', /* SSLEEP */
	'T', /* SSTOP */
	'Z', /* SZOMB */
	'W', /* SWAIT */
	'L' /* SLOCK */
};


static guint64
get_mem_by_bytes (const gchar *name)
{
	guint64 buf = 0;
	gsize len = sizeof (buf);

	if (sysctlbyname (name, &buf, &len, NULL, 0) < 0)
		return 0;

	return buf;
}

static guint64
get_mem_by_pages (const gchar *name)
{
	guint64 res;

	res = get_mem_by_bytes (name);
	if (res > 0)
		res *= (guint64)getpagesize ();

	return res;
}

gboolean
get_memory_usage (guint64 *memory_total, guint64 *memory_available, guint64 *memory_free, guint64 *memory_cache, guint64 *memory_buffers, guint64 *swap_total, guint64 *swap_free)
{
	/* Get memory usage */
	{
		*memory_total = get_mem_by_bytes ("hw.physmem");;
		*memory_free = get_mem_by_pages ("vm.stats.vm.v_free_count");
		*memory_cache = get_mem_by_pages ("vm.stats.vm.v_inactive_count");
		*memory_buffers = get_mem_by_bytes ("vfs.bufspace");
		*memory_available = *memory_free + *memory_cache + *memory_buffers;
	}

	/* Get swap usage */
	{
		kvm_t *kd;
		struct kvm_swap kswap;

		if ((kd = kvm_openfiles (_PATH_DEVNULL, _PATH_DEVNULL, NULL, O_RDONLY, NULL)) == NULL)
			return FALSE;

		kvm_getswapinfo (kd, &kswap, 1, 0);
		*swap_total = (guint64)kswap.ksw_total * (guint64)getpagesize ();
		*swap_free = ((guint64)(kswap.ksw_total - kswap.ksw_used)) * (guint64)getpagesize ();

		kvm_close (kd);
	}

	return TRUE;
}

gboolean
get_cpu_usage (gushort *cpu_count, gfloat *cpu_user, gfloat *cpu_system)
{
	/* Get CPU count */
	{
		size_t len = sizeof (*cpu_count);
		sysctlbyname ("hw.ncpu", cpu_count, &len, NULL, 0);
	}

	/* Get CPU usage */
	{
		static gulong cp_user = 0, cp_system = 0, cp_total = 0;
		static gulong cp_user_old = 0, cp_system_old = 0, cp_total_old = 0;

		gulong cpu_state[CPUSTATES] = { 0 };
		size_t len = sizeof (cpu_state);
		sysctlbyname ("kern.cp_time", &cpu_state, &len, NULL, 0);

		cp_user_old = cp_user;
		cp_system_old = cp_system;
		cp_total_old = cp_total;
		cp_user = cpu_state[CP_USER] + cpu_state[CP_NICE];
		cp_system = cpu_state[CP_SYS] + cpu_state[CP_INTR];
		cp_total = cpu_state[CP_IDLE] + cp_user + cp_system;

		*cpu_user = *cpu_system = 0.0f;
		if (cp_total > cp_total_old)
		{
			*cpu_user = (((cp_user - cp_user_old) * 100.0f) / (float)(cp_total - cp_total_old));
			*cpu_system = (((cp_system - cp_system_old) * 100.0f) / (float)(cp_total - cp_total_old));
		}
	}

	return TRUE;
}

static gboolean
get_task_details (struct kinfo_proc *kp, Task *task)
{
	char buf[1024], *p;
	size_t bufsz;
	int i, oid[4];

	memset(task, 0, sizeof(Task));
	task->pid = kp->ki_pid;
	task->ppid = kp->ki_ppid;
	task->cpu_user = 100.0f * ((float)kp->ki_pctcpu / FSCALE);
	task->cpu_system = 0.0f;
	task->vsz = kp->ki_size;
	task->rss = ((guint64)kp->ki_rssize * (guint64)getpagesize ());
	task->uid = kp->ki_uid;
	task->prio = kp->ki_nice;
	g_strlcpy (task->name, kp->ki_comm, sizeof(task->name));

	oid[0] = CTL_KERN;
	oid[1] = KERN_PROC;
	oid[2] = KERN_PROC_ARGS;
	oid[3] = kp->ki_pid;
	bufsz = sizeof(buf);
	memset(buf, 0, sizeof(buf));
	if (sysctl(oid, 4, buf, &bufsz, 0, 0) == -1) {
		/*
		 * If the supplied buf is too short to hold the requested
		 * value the sysctl returns with ENOMEM. The buf is filled
		 * with the truncated value and the returned bufsz is equal
		 * to the requested len.
		 */
		if (errno != ENOMEM || bufsz != sizeof(buf)) {
			bufsz = 0;
		} else {
			buf[(bufsz - 1)] = 0;
		}
	}

	if (0 != bufsz) {
		p = buf;
		do {
			g_strlcat (task->cmdline, p, sizeof(task->cmdline));
			g_strlcat (task->cmdline, " ", sizeof(task->cmdline));
			p += (strlen(p) + 1);
		} while (p < buf + bufsz);
	}
	else
	{
		g_strlcpy (task->cmdline, kp->ki_comm, sizeof(task->cmdline));
	}

	i = 0;
	switch (kp->ki_stat)
	{
		case SIDL:
		case SRUN:
		case SSTOP:
		case SZOMB:
		case SWAIT:
		case SLOCK:
			task->state[i] = ki_stat2state[(size_t)kp->ki_stat];
			break;

		case SSLEEP:
			if (kp->ki_tdflags & TDF_SINTR)
				task->state[i] = kp->ki_slptime >= MAXSLP ? 'I' : 'S';
			else
				task->state[i] = 'D';
			break;

		default:
			task->state[i] = '?';
	}
	i++;
	if (!(kp->ki_sflag & PS_INMEM))
		task->state[i++] = 'W';
	if (kp->ki_nice < NZERO)
		task->state[i++] = '<';
	else if (kp->ki_nice > NZERO)
		task->state[i++] = 'N';
	if (kp->ki_flag & P_TRACED)
		task->state[i++] = 'X';
	if (kp->ki_flag & P_WEXIT && kp->ki_stat != SZOMB)
		task->state[i++] = 'E';
	if (kp->ki_flag & P_PPWAIT)
		task->state[i++] = 'V';
	if ((kp->ki_flag & P_SYSTEM) || kp->ki_lock > 0)
		task->state[i++] = 'L';
	if (kp->ki_kiflag & KI_SLEADER)
		task->state[i++] = 's';
	if ((kp->ki_flag & P_CONTROLT) && kp->ki_pgid == kp->ki_tpgid)
		task->state[i++] = '+';
	if (kp->ki_flag & P_JAILED)
		task->state[i++] = 'J';

	return TRUE;
}

gboolean
get_task_list (GArray *task_list)
{
	kvm_t *kd;
	struct kinfo_proc *kp;
	int cnt = 0, i;
	Task task;

	if ((kd = kvm_openfiles (_PATH_DEVNULL, _PATH_DEVNULL, NULL, O_RDONLY, NULL)) == NULL)
		return FALSE;

	if ((kp = kvm_getprocs (kd, KERN_PROC_PROC, 0, &cnt)) == NULL)
	{
		kvm_close (kd);
		return FALSE;
	}

	for (i = 0; i < cnt; kp++, i++)
	{
		get_task_details (kp, &task);
		g_array_append_val (task_list, task);
	}

	kvm_close (kd);

	g_array_sort (task_list, task_pid_compare_fn);

	return TRUE;
}

gboolean
pid_is_sleeping (GPid pid)
{
	kvm_t *kd;
	struct kinfo_proc *kp;
	gint cnt;
	gboolean ret;

	if ((kd = kvm_openfiles (_PATH_DEVNULL, _PATH_DEVNULL, NULL, O_RDONLY, NULL)) == NULL)
		return FALSE;

	if ((kp = kvm_getprocs (kd, KERN_PROC_PID, pid, &cnt)) == NULL)
	{
		kvm_close (kd);
		return FALSE;
	}

	ret = (kp->ki_stat == SSTOP);
	kvm_close (kd);

	return ret;
}
