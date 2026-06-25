/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __THUNAR_IO_SCAN_DIRECTORY_H__
#define __THUNAR_IO_SCAN_DIRECTORY_H__

#include <exo/exo.h>

#include <thunar/thunar-job.h>
#include <thunar/thunar-private.h>

G_BEGIN_DECLS

GList *thunar_io_scan_directory (ThunarJob          *job,
                                 GFile              *file,
                                 GFileQueryInfoFlags flags,
                                 gboolean            recursively,
                                 gboolean            unlinking,
                                 gboolean            return_thunar_files,
                                 guint              *n_files_max,
                                 GError            **error);

G_END_DECLS

#endif /* !__THUNAR_IO_SCAN_DIRECTORY_H__ */
