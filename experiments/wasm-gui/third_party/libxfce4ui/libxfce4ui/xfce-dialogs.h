/*
 * Copyright (c) 2007 The Xfce Development Team
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

#ifndef __XFCE_DIALOGS_H__
#define __XFCE_DIALOGS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * XFCE_BUTTON_TYPE_MIXED:
 *
 * This allows you to easily create mixed buttons in a dialog.
 * param1 is used for the stock_id, param2 for the label and
 * param3 for the response_id. See also xfce_gtk_button_new_mixed().
 *
 **/
#define XFCE_BUTTON_TYPE_MIXED  "button-mixed"

/**
 * XFCE_BUTTON_TYPE_PIXBUF:
 *
 * Creates a button with the #GdkPixbuf as button icon.
 * param1 is the #GdkPixbuf, param2 for the label and
 * param3 for the response_id.
 *
 **/
#define XFCE_BUTTON_TYPE_PIXBUF "button-pixbuf"

void       xfce_dialog_show_help              (GtkWindow    *parent,
                                               const gchar  *component,
                                               const gchar  *page,
                                               const gchar  *offset);

void       xfce_dialog_show_help_with_version (GtkWindow    *parent,
                                               const gchar  *component,
                                               const gchar  *page,
                                               const gchar  *offset,
                                               const gchar  *version);

void       xfce_dialog_show_info              (GtkWindow    *parent,
                                               const gchar  *secondary_text,
                                               const gchar  *primary_format,
                                               ...) G_GNUC_PRINTF (3, 4);

void       xfce_dialog_show_warning           (GtkWindow    *parent,
                                               const gchar  *secondary_text,
                                               const gchar  *primary_format,
                                               ...) G_GNUC_PRINTF (3, 4);

void       xfce_dialog_show_error             (GtkWindow    *parent,
                                               const GError *error,
                                               const gchar  *primary_format,
                                               ...) G_GNUC_PRINTF (3, 4);

gboolean   xfce_dialog_confirm                (GtkWindow    *parent,
                                               const gchar  *stock_id,
                                               const gchar  *confirm_label,
                                               const gchar  *secondary_text,
                                               const gchar  *primary_format,
                                               ...) G_GNUC_PRINTF (5, 6);

gint       xfce_dialog_confirm_close_tabs     (GtkWindow   *parent,
                                               gint         num_tabs,
                                               gboolean     show_confirm_box,
                                               gboolean    *confirm_box_checked);

GtkWidget *xfce_message_dialog_new_valist     (GtkWindow   *parent,
                                               const gchar *title,
                                               const gchar *icon_stock_id,
                                               const gchar *primary_text,
                                               const gchar *secondary_text,
                                               const gchar *first_button_text,
                                               va_list      args) G_GNUC_MALLOC;

GtkWidget *xfce_message_dialog_new            (GtkWindow    *parent,
                                               const gchar  *title,
                                               const gchar  *stock_id,
                                               const gchar  *primary_text,
                                               const gchar  *secondary_text,
                                               const gchar  *first_button_text,
                                               ...) G_GNUC_NULL_TERMINATED G_GNUC_MALLOC;

gint       xfce_message_dialog                (GtkWindow    *parent,
                                               const gchar  *title,
                                               const gchar  *stock_id,
                                               const gchar  *primary_text,
                                               const gchar  *secondary_text,
                                               const gchar  *first_button_text,
                                               ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /* !__XFCE_DIALOGS_H__ */
