/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __THUNAR_CLIPBOARD_MANAGER_H__
#define __THUNAR_CLIPBOARD_MANAGER_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarClipboardManagerClass ThunarClipboardManagerClass;
typedef struct _ThunarClipboardManager      ThunarClipboardManager;

#define THUNAR_TYPE_CLIPBOARD_MANAGER             (thunar_clipboard_manager_get_type ())
#define THUNAR_CLIPBOARD_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_CLIPBOARD_MANAGER, ThunarClipboardManager))
#define THUNAR_CLIPBOARD_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((obj), THUNAR_TYPE_CLIPBOARD_MANAGER, ThunarClipboardManagerClass))
#define THUNAR_IS_CLIPBOARD_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_CLIPBOARD_MANAGER))
#define THUNAR_IS_CLIPBOARD_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_CLIPBOARD_MANAGER))
#define THUNAR_CLIPBOARD_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_CLIPBOARD_MANAGER, ThunarClipboardManagerClass))

GType                   thunar_clipboard_manager_get_type        (void) G_GNUC_CONST;

ThunarClipboardManager *thunar_clipboard_manager_get_for_display (GdkDisplay             *display);

gboolean                thunar_clipboard_manager_get_can_paste   (ThunarClipboardManager *manager);

gboolean                thunar_clipboard_manager_has_cutted_file (ThunarClipboardManager *manager,
                                                                  const ThunarFile       *file);

void                    thunar_clipboard_manager_copy_files      (ThunarClipboardManager *manager,
                                                                  GList                  *files);
void                    thunar_clipboard_manager_cut_files       (ThunarClipboardManager *manager,
                                                                  GList                  *files);
void                    thunar_clipboard_manager_paste_files     (ThunarClipboardManager *manager,
                                                                  GFile                  *target_file,
                                                                  GtkWidget              *widget,
                                                                  GClosure               *new_files_closure);

G_END_DECLS;

#endif /* !__THUNAR_CLIPBOARD_MANAGER_H__ */
