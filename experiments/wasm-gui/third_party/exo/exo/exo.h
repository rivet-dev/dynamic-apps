/*-
 * Copyright (c) 2004-2007 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef __EXO_H__
#define __EXO_H__

/* be sure to have i18n macros available and libintl.h included! */
#if defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include <gtk/gtk.h>

#define EXO_INSIDE_EXO_H

#include <exo/exo-config.h>
#include <exo/exo-binding.h>
#include <exo/exo-enum-types.h>
#include <exo/exo-cell-renderer-icon.h>
#include <exo/exo-gdk-pixbuf-extensions.h>
#include <exo/exo-gobject-extensions.h>
#include <exo/exo-execute.h>
#include <exo/exo-job.h>
#include <exo/exo-simple-job.h>
#include <exo/exo-string.h>
#include <exo/exo-utils.h>
#include <exo/exo-gtk-extensions.h>
#include <exo/exo-icon-chooser-dialog.h>
#include <exo/exo-icon-view.h>
#include <exo/exo-tree-view.h>

#undef EXO_INSIDE_EXO_H

#endif /* !__EXO_H__ */
