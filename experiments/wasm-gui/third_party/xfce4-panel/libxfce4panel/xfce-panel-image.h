/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#if !defined(_LIBXFCE4PANEL_INSIDE_LIBXFCE4PANEL_H) && !defined(LIBXFCE4PANEL_COMPILATION)
#error "Only <libxfce4panel/libxfce4panel.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __XFCE_PANEL_IMAGE_H__
#define __XFCE_PANEL_IMAGE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _XfcePanelImageClass   XfcePanelImageClass;
typedef struct _XfcePanelImage        XfcePanelImage;
typedef struct _XfcePanelImagePrivate XfcePanelImagePrivate;

struct _XfcePanelImageClass
{
  /*< private >*/
  GtkWidgetClass __parent__;

  /*< private >*/
  void (*reserved1) (void);
  void (*reserved2) (void);
  void (*reserved3) (void);
  void (*reserved4) (void);
};

/**
 * XfcePanelImage:
 *
 * This struct contain private data only and should be accessed by
 * the functions below.
 **/
struct _XfcePanelImage
{
  /*< private >*/
  GtkWidget __parent__;

  /*< private >*/
  XfcePanelImagePrivate *priv;
};

#define XFCE_TYPE_PANEL_IMAGE            (xfce_panel_image_get_type ())
#define XFCE_PANEL_IMAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_PANEL_IMAGE, XfcePanelImage))
#define XFCE_PANEL_IMAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_PANEL_IMAGE, XfcePanelImageClass))
#define XFCE_IS_PANEL_IMAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_PANEL_IMAGE))
#define XFCE_IS_PANEL_IMAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_PANEL_IMAGE))
#define XFCE_PANEL_IMAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_PANEL_IMAGE, XfcePanelImageClass))

GType      xfce_panel_image_get_type        (void)
G_GNUC_CONST G_GNUC_DEPRECATED;

GtkWidget *xfce_panel_image_new             (void)
G_GNUC_MALLOC G_GNUC_DEPRECATED_FOR (gtk_image_new());

GtkWidget *xfce_panel_image_new_from_pixbuf (GdkPixbuf      *pixbuf)
G_GNUC_MALLOC G_GNUC_DEPRECATED_FOR (gtk_image_new_from_pixbuf());

GtkWidget *xfce_panel_image_new_from_source (const gchar    *source)
G_GNUC_MALLOC G_GNUC_DEPRECATED_FOR (gtk_image_new() and xfce_panel_set_image_from_source());

void       xfce_panel_image_set_from_pixbuf (XfcePanelImage *image,
                                             GdkPixbuf      *pixbuf)
G_GNUC_DEPRECATED_FOR (gtk_image_set_from_pixbuf());

void       xfce_panel_image_set_from_source (XfcePanelImage *image,
                                             const gchar    *source)
G_GNUC_DEPRECATED_FOR (xfce_panel_set_image_from_source());

void       xfce_panel_image_set_size        (XfcePanelImage *image,
                                             gint            size)
G_GNUC_DEPRECATED_FOR (gtk_image_set_pixel_size());

gint       xfce_panel_image_get_size        (XfcePanelImage *image)
G_GNUC_DEPRECATED_FOR (gtk_image_get_pixel_size());

void       xfce_panel_image_clear           (XfcePanelImage *image)
G_GNUC_DEPRECATED_FOR (gtk_image_clear());

G_END_DECLS

#endif /* !__XFCE_PANEL_IMAGE_H__ */
