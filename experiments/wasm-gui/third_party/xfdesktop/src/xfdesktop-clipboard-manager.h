/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2010 Jannis Pohlmann <jannis@xfce.org>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * Copied, renamed, and hacked to pieces by Brian Tarricone <bjt23@cornell.edu>.
 * Original code from Thunar.
 */

#ifndef __XFDESKTOP_CLIPBOARD_MANAGER_H__
#define __XFDESKTOP_CLIPBOARD_MANAGER_H__

#include <gio/gio.h>

G_BEGIN_DECLS;

/* fwd decl */
struct _XfdesktopFileIcon;

typedef struct _XfdesktopClipboardManagerClass XfdesktopClipboardManagerClass;
typedef struct _XfdesktopClipboardManager      XfdesktopClipboardManager;

#define XFDESKTOP_TYPE_CLIPBOARD_MANAGER             (xfdesktop_clipboard_manager_get_type ())
#define XFDESKTOP_CLIPBOARD_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFDESKTOP_TYPE_CLIPBOARD_MANAGER, XfdesktopClipboardManager))
#define XFDESKTOP_CLIPBOARD_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((obj), XFDESKTOP_TYPE_CLIPBOARD_MANAGER, XfdesktopClipboardManagerClass))
#define XFDESKTOP_IS_CLIPBOARD_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFDESKTOP_TYPE_CLIPBOARD_MANAGER))
#define XFDESKTOP_IS_CLIPBAORD_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), XFDESKTOP_TYPE_CLIPBOARD_MANAGER))
#define XFDESKTOP_CLIPBOARD_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), XFDESKTOP_TYPE_CLIPBAORD_MANAGER, XfdesktopClipboardManagerClass))

GType                      xfdesktop_clipboard_manager_get_type        (void) G_GNUC_CONST;

XfdesktopClipboardManager *xfdesktop_clipboard_manager_get_for_display (GdkDisplay                      *display);

gboolean                   xfdesktop_clipboard_manager_get_can_paste   (XfdesktopClipboardManager       *manager);

gboolean                   xfdesktop_clipboard_manager_has_cutted_file (XfdesktopClipboardManager       *manager,
                                                                        const struct _XfdesktopFileIcon *file);

void                       xfdesktop_clipboard_manager_copy_files      (XfdesktopClipboardManager       *manager,
                                                                        GList                           *files);
void                       xfdesktop_clipboard_manager_cut_files       (XfdesktopClipboardManager       *manager,
                                                                        GList                           *files);
void                       xfdesktop_clipboard_manager_paste_files     (XfdesktopClipboardManager       *manager,
                                                                        GFile                           *target_file,
                                                                        GtkWidget                       *widget,
                                                                        GClosure                        *new_files_closure);

G_END_DECLS;

#endif /* !__XFDESKTOP_CLIPBOARD_MANAGER_H__ */
