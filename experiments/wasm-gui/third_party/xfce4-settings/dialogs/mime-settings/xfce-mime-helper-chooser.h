/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __XFCE_MIME_HELPER_CHOOSER_H__
#define __XFCE_MIME_HELPER_CHOOSER_H__

#include "xfce-mime-helper.h"

G_BEGIN_DECLS

#define XFCE_MIME_TYPE_HELPER_CHOOSER            (xfce_mime_helper_chooser_get_type ())
#define XFCE_MIME_HELPER_CHOOSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_MIME_TYPE_HELPER_CHOOSER, XfceMimeHelperChooser))
#define XFCE_MIME_HELPER_CHOOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_MIME_TYPE_HELPER_CHOOSER, XfceMimeHelperChooserClass))
#define XFCE_MIME_IS_HELPER_CHOOSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_MIME_TYPE_HELPER_CHOOSER))
#define XFCE_MIME_IS_HELPER_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_MIME_TYPE_HELPER_CHOOSER))
#define XFCE_MIME_HELPER_CHOOSER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_MIME_TYPE_HELPER_CHOOSER, XfceMimeHelperChooserClass))

typedef struct _XfceMimeHelperChooserClass XfceMimeHelperChooserClass;
typedef struct _XfceMimeHelperChooser      XfceMimeHelperChooser;

GType                   xfce_mime_helper_chooser_get_type      (void) G_GNUC_CONST;

GtkWidget              *xfce_mime_helper_chooser_new           (XfceMimeHelperCategory       category) G_GNUC_MALLOC;

XfceMimeHelperCategory  xfce_mime_helper_chooser_get_category  (const XfceMimeHelperChooser *chooser);
void                    xfce_mime_helper_chooser_set_category  (XfceMimeHelperChooser       *chooser,
                                                                XfceMimeHelperCategory       category);

gboolean                xfce_mime_helper_chooser_get_is_valid  (const XfceMimeHelperChooser *chooser);

G_END_DECLS

#endif /* !__XFCE_MIME_HELPER_CHOOSER_H__ */
