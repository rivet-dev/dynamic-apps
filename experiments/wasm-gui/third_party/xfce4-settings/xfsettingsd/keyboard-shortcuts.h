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

#ifndef __XFCE_KEYBOARD_SHORTCUTS_HELPER_H__
#define __XFCE_KEYBOARD_SHORTCUTS_HELPER_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _XfceKeyboardShortcutsHelperClass   XfceKeyboardShortcutsHelperClass;
typedef struct _XfceKeyboardShortcutsHelper        XfceKeyboardShortcutsHelper;

#define XFCE_TYPE_KEYBOARD_SHORTCUTS_HELPER            (xfce_keyboard_shortcuts_helper_get_type ())
#define XFCE_KEYBOARD_SHORTCUTS_HELPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_KEYBOARD_SHORTCUTS_HELPER, XfceKeyboardShortcutsHelper))
#define XFCE_KEYBOARD_SHORTCUTS_HELPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_KEYBOARD_SHORTCUTS_HELPER, XfceKeyboardShortcutsHelperClass))
#define XFCE_IS_KEYBOARD_SHORTCUTS_HELPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_KEYBOARD_SHORTCUTS_HELPER))
#define XFCE_IS_KEYBOARD_SHORTCUTS_HELPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_KEYBOARD_SHORTCUTS_HELPER)
#define XFCE_KEYBOARD_SHORTCUTS_HELPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_KEYBOARD_SHORTCUTS_HELPER, XfceKeyboardShortcutsHelperClass))

GType xfce_keyboard_shortcuts_helper_get_type (void) G_GNUC_CONST;

G_END_DECLS;

#endif /* !__XFCE_KEYBOARD_SHORTCUTS_HELPER_H__ */
