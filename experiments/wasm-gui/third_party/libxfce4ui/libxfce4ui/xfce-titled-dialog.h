/*
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
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

#ifndef __XFCE_TITLED_DIALOG_H__
#define __XFCE_TITLED_DIALOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _XfceTitledDialogPrivate XfceTitledDialogPrivate;
typedef struct _XfceTitledDialogClass   XfceTitledDialogClass;
typedef struct _XfceTitledDialog        XfceTitledDialog;

#define XFCE_TYPE_TITLED_DIALOG             (xfce_titled_dialog_get_type ())
#define XFCE_TITLED_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_TITLED_DIALOG, XfceTitledDialog))
#define XFCE_TITLED_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_TITLED_DIALOG, XfceTitledDialogClass))
#define XFCE_IS_TITLED_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_TITLED_DIALOG))
#define XFCE_IS_TITLED_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_TITLED_DIALOG))
#define XFCE_TITLED_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_TITLED_DIALOG, XfceTitledDialogClass))

struct _XfceTitledDialogClass
{
  /*< private >*/
  GtkDialogClass __parent__;

  /* reserved for future expansion */
  void (*reserved0) (void);
  void (*reserved1) (void);
  void (*reserved2) (void);
  void (*reserved3) (void);
  void (*reserved4) (void);
  void (*reserved5) (void);
};

/**
 * XfceTitledDialog:
 *
 * An opaque struct with only private fields.
 **/
struct _XfceTitledDialog
{
  /*< private >*/
  GtkDialog                __parent__;
  XfceTitledDialogPrivate *priv;
};

GType                 xfce_titled_dialog_get_type         (void) G_GNUC_CONST;

GtkWidget            *xfce_titled_dialog_new              (void) G_GNUC_MALLOC;
GtkWidget            *xfce_titled_dialog_new_with_buttons (const gchar      *title,
                                                           GtkWindow        *parent,
                                                           GtkDialogFlags    flags,
                                                           const gchar      *first_button_text,
                                                           ...)
G_GNUC_MALLOC G_GNUC_DEPRECATED_FOR (xfce_titled_dialog_new_with_mixed_buttons());

GtkWidget            *xfce_titled_dialog_new_with_mixed_buttons (const gchar    *title,
                                                                 GtkWindow      *parent,
                                                                 GtkDialogFlags  flags,
                                                                 const gchar    *first_button_icon_name,
                                                                 const gchar    *first_button_text,
                                                                 ...) G_GNUC_MALLOC;

void                  xfce_titled_dialog_create_action_area   (XfceTitledDialog *titled_dialog);
GtkWidget            *xfce_titled_dialog_add_button           (XfceTitledDialog *titled_dialog,
                                                               const gchar      *button_text,
                                                               gint              response_id);
void                  xfce_titled_dialog_add_action_widget    (XfceTitledDialog *titled_dialog,
                                                               GtkWidget        *child,
                                                               gint              response_id);
void                  xfce_titled_dialog_set_default_response (XfceTitledDialog *titled_dialog,
                                                               gint              response_id);
const gchar          *xfce_titled_dialog_get_subtitle         (XfceTitledDialog *titled_dialog);
void                  xfce_titled_dialog_set_subtitle         (XfceTitledDialog *titled_dialog,
                                                               const gchar      *subtitle);

G_END_DECLS

#endif /* !__XFCE_TITLED_DIALOG_H__ */
