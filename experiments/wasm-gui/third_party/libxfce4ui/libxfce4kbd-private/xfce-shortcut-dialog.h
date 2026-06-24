/* vi:set expandtab sw=2 sts=2: */
/*
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __XFCE_SHORTCUT_DIALOG_H__
#define __XFCE_SHORTCUT_DIALOG_H__

#include <gtk/gtk.h>

#include <libxfce4kbd-private/xfce-shortcuts.h>

G_BEGIN_DECLS

typedef struct _XfceShortcutDialogClass XfceShortcutDialogClass;
typedef struct _XfceShortcutDialog      XfceShortcutDialog;

#define XFCE_TYPE_SHORTCUT_DIALOG            (xfce_shortcut_dialog_get_type ())
#define XFCE_SHORTCUT_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_SHORTCUT_DIALOG, XfceShortcutDialog))
#define XFCE_SHORTCUT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_SHORTCUT_DIALOG, XfceShortcutDialogClass))
#define XFCE_IS_SHORTCUT_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_SHORTCUT_DIALOG))
#define XFCE_IS_SHORTCUT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_SHORTCUT_DIALOG))
#define XFCE_SHORTCUT_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_SHORTCUT_DIALOG, XfceShortcutDialogClass))

GType        xfce_shortcut_dialog_get_type        (void) G_GNUC_CONST;

GtkWidget   *xfce_shortcut_dialog_new             (const gchar        *provider,
                                                   const gchar        *action_name,
                                                   const gchar        *action) G_GNUC_MALLOC;
gint         xfce_shortcut_dialog_run             (XfceShortcutDialog *dialog,
                                                   GtkWidget          *parent);
const gchar *xfce_shortcut_dialog_get_shortcut    (XfceShortcutDialog *dialog);
const gchar *xfce_shortcut_dialog_get_action      (XfceShortcutDialog *action);
const gchar *xfce_shortcut_dialog_get_action_name (XfceShortcutDialog *action);

G_END_DECLS

#endif /* !__XFCE_SHORTCUT_DIALOG_H__ */
