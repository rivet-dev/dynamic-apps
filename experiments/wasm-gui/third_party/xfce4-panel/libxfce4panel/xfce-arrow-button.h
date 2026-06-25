/*
 * Copyright (C) 2006-2007 Jasper Huijsmans <jasper@xfce.org>
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



#ifndef __XFCE_ARROW_BUTTON_H__
#define __XFCE_ARROW_BUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _XfceArrowButtonPrivate XfceArrowButtonPrivate;
typedef struct _XfceArrowButtonClass   XfceArrowButtonClass;
typedef struct _XfceArrowButton        XfceArrowButton;

#define XFCE_TYPE_ARROW_BUTTON            (xfce_arrow_button_get_type ())
#define XFCE_ARROW_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_ARROW_BUTTON, XfceArrowButton))
#define XFCE_ARROW_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_ARROW_BUTTON, XfceArrowButtonClass))
#define XFCE_IS_ARROW_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_ARROW_BUTTON))
#define XFCE_IS_ARROW_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_ARROW_BUTTON))
#define XFCE_ARROW_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_ARROW_BUTTON, XfceArrowButtonClass))

struct _XfceArrowButtonClass
{
  /*< private >*/
  GtkToggleButtonClass __parent__;

  /*< signals >*/
  void (*arrow_type_changed) (XfceArrowButton *button,
                              GtkArrowType     type);
};

/**
 * XfceArrowButton:
 *
 * This struct contain private data only and should be accessed by
 * the functions below.
 **/
struct _XfceArrowButton
{
  /*< private >*/
  GtkToggleButton __parent__;

  /*< private >*/
  XfceArrowButtonPrivate *priv;
};

GType         xfce_arrow_button_get_type       (void) G_GNUC_CONST;

GtkWidget    *xfce_arrow_button_new            (GtkArrowType     arrow_type) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GtkArrowType  xfce_arrow_button_get_arrow_type (XfceArrowButton *button);
void          xfce_arrow_button_set_arrow_type (XfceArrowButton *button,
                                                GtkArrowType     arrow_type);

gboolean      xfce_arrow_button_get_blinking   (XfceArrowButton *button);
void          xfce_arrow_button_set_blinking   (XfceArrowButton *button,
                                                gboolean         blinking);

G_END_DECLS

#endif /* !__XFCE_ARROW_BUTTON_H__ */
