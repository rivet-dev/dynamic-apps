/*
 *  Copyright (c) Gaël Bonithon 2023 <gael@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#ifndef __RSTTO_PRINT_H__
#define __RSTTO_PRINT_H__

#include <gtk/gtk.h>

#include "image_viewer.h"

G_BEGIN_DECLS

#define RSTTO_TYPE_PRINT rstto_print_get_type ()
G_DECLARE_FINAL_TYPE (RsttoPrint, rstto_print, RSTTO, PRINT, GtkPrintOperation)

RsttoPrint *
rstto_print_new (RsttoImageViewer *viewer);

gboolean
rstto_print_image_interactive (RsttoPrint *print,
                               GtkWindow *parent,
                               GError **error);

G_END_DECLS

#endif /* !__RSTTO_PRINT_H__ */
