/* vi:set sw=2 sts=2 ts=2 et ai: */
/*-
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __COMMAND_DIALOG_H__
#define __COMMAND_DIALOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _CommandDialogClass CommandDialogClass;
typedef struct _CommandDialog      CommandDialog;

#define TYPE_COMMAND_DIALOG            (command_dialog_get_type ())
#define COMMAND_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_COMMAND_DIALOG, CommandDialog))
#define COMMAND_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_COMMAND_DIALOG, CommandDialogClass))
#define IS_COMMAND_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_COMMAND_DIALOG))
#define IS_COMMAND_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_COMMAND_DIALOG))
#define COMMAND_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_COMMAND_DIALOG, CommandDialogClass))

GType       command_dialog_get_type     (void) G_GNUC_CONST;

GtkWidget  *command_dialog_new          (const gchar   *shortcut,
                                         const gchar   *action,
                                         gboolean       snotify);
const char *command_dialog_get_command  (CommandDialog *dialog);
gboolean    command_dialog_get_snotify  (CommandDialog *dialog);
gint        command_dialog_run          (CommandDialog *dialog,
                                         GtkWidget     *parent);

G_END_DECLS

#endif /* !__COMMAND_DIALOG_H__ */
