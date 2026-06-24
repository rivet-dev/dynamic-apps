/*
 * Copyright (C) 2012 Nick Schermer <nick@xfce.org>
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

#ifndef __XFCE_MIME_CHOOSER_H__
#define __XFCE_MIME_CHOOSER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _XfceMimeChooserClass XfceMimeChooserClass;
typedef struct _XfceMimeChooser      XfceMimeChooser;

#define XFCE_TYPE_MIME_CHOOSER            (xfce_mime_chooser_get_type ())
#define XFCE_MIME_CHOOSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_MIME_CHOOSER, XfceMimeChooser))
#define XFCE_MIME_CHOOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_MIME_CHOOSER, XfceMimeChooserClass))
#define XFCE_IS_MIME_CHOOSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_MIME_CHOOSER))
#define XFCE_IS_MIME_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_MIME_CHOOSER))
#define XFCE_MIME_CHOOSER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_MIME_CHOOSER, XfceMimeChooserClass))

GType      xfce_mime_chooser_get_type      (void) G_GNUC_CONST;

void       xfce_mime_chooser_set_mime_type (XfceMimeChooser *chooser,
                                            const gchar     *mime_type);

GAppInfo  *xfce_mime_chooser_get_app_info  (XfceMimeChooser *chooser);

G_END_DECLS

#endif /* !__XFCE_MIME_CHOOSER_H__ */
