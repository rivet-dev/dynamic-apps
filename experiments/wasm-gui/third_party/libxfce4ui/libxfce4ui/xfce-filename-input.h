/*
 * Copyright (c) 2020 The Xfce Development Team
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#if !defined (_LIBXFCE4UI_INSIDE_LIBXFCE4UI_H) && !defined (LIBXFCE4UI_COMPILATION)
#error "Only <libxfce4ui/libxfce4ui.h> can be included directly, this file is not part of the public API."
#endif

#ifndef __XFCE_FILENAME_INPUT_H__
#define __XFCE_FILENAME_INPUT_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _XfceFilenameInputClass XfceFilenameInputClass;
typedef struct _XfceFilenameInput      XfceFilenameInput;

#define XFCE_TYPE_FILENAME_INPUT             (xfce_filename_input_get_type())
#define XFCE_FILENAME_INPUT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), XFCE_TYPE_FILENAME_INPUT, XfceFilenameInput))
#define XFCE_IS_FILENAME_INPUT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFCE_TYPE_FILENAME_INPUT))
#define XFCE_FILENAME_INPUT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),  XFCE_TYPE_FILENAME_INPUT, XfceFilenameInputClass))
#define XFCE_IS_FILENAME_INPUT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),  XFCE_TYPE_FILENAME_INPUT))
#define XFCE_FILENAME_INPUT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj),  XFCE_TYPE_FILENAME_INPUT, XfceFilenameInputClass))

GType        xfce_filename_input_get_type              (void) G_GNUC_CONST;

const gchar *xfce_filename_input_get_text              (XfceFilenameInput *filename_input);

void         xfce_filename_input_check                 (XfceFilenameInput *filename_input);

GtkEntry    *xfce_filename_input_get_entry             (XfceFilenameInput *filename_input);

void         xfce_filename_input_sensitise_widget      (GtkWidget         *widget);

void         xfce_filename_input_desensitise_widget    (GtkWidget         *widget);


G_END_DECLS

#endif /* !__XFCE_FILENAME_INPUT_H__ */
