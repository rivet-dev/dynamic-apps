/*
 * Copyright (C) 2019 Xfce Development Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <gtk/gtk.h>

#if G_GNUC_CHECK_VERSION (4, 6)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#endif
#include <glibtop/mem.h>
#if G_GNUC_CHECK_VERSION (4, 6)
#pragma GCC diagnostic pop
#endif

#include <glibtop/sysinfo.h>



enum
{
  OS_NAME,
  DEVICE_NAME,
  KERNEL
};

char *get_os_type (void);
char *get_system_info (guint infotype);
char *get_cpu_info (const glibtop_sysinfo *info);
char *get_gpu_info (guint *num_gpus);
char *get_os_name (void);
char *get_renderer (void);
