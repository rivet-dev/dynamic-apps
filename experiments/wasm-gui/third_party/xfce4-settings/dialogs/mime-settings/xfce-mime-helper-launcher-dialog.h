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

#ifndef __XFCE_MIME_HELPER_LAUNCHER_DIALOG_H__
#define __XFCE_MIME_HELPER_LAUNCHER_DIALOG_H__

#include <xfce-mime-helper.h>

G_BEGIN_DECLS

typedef struct _XfceMimeHelperLauncherDialogClass XfceMimeHelperLauncherDialogClass;
typedef struct _XfceMimeHelperLauncherDialog      XfceMimeHelperLauncherDialog;

#define XFCE_MIME_TYPE_HELPER_LAUNCHER_DIALOG             (xfce_mime_helper_launcher_dialog_get_type ())
#define XFCE_MIME_HELPER_LAUNCHER_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_MIME_TYPE_HELPER_LAUNCHER_DIALOG, XfceMimeHelperLauncherDialog))
#define XFCE_MIME_HELPER_LAUNCHER_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_MIME_TYPE_HELPER_LAUNCHER_DIALOG, XfceMimeHelperLauncherDialogClass))
#define XFCE_MIME_IS_HELPER_LAUNCHER_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_MIME_TYPE_HELPER_LAUNCHER_DIALOG))
#define XFCE_MIME_IS_HELPER_LAUNCHER_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_MIME_TYPE_HELPER_LAUNCHER_DIALOG))
#define XFCE_MIME_HELPER_LAUNCHER_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_MIME_TYPE_HELPER_LAUNCHER_DIALOG, XfceMimeHelperLauncherDialogClass))

GType                   xfce_mime_helper_launcher_dialog_get_type     (void) G_GNUC_CONST;

GtkWidget              *xfce_mime_helper_launcher_dialog_new          (XfceMimeHelperCategory              category) G_GNUC_MALLOC;

XfceMimeHelperCategory  xfce_mime_helper_launcher_dialog_get_category (const XfceMimeHelperLauncherDialog *launcher_dialog);
void                    xfce_mime_helper_launcher_dialog_set_category (XfceMimeHelperLauncherDialog       *launcher_dialog,
                                                                       XfceMimeHelperCategory              category);

G_END_DECLS

#endif /* !__XFCE_MIME_HELPER_LAUNCHER_DIALOG_H__ */
