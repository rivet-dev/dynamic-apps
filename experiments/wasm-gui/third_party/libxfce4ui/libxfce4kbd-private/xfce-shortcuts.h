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

#ifndef __XFCE_SHORTCUTS_H__
#define __XFCE_SHORTCUTS_H__

#include <X11/Xlib.h>
#include <xfconf/xfconf.h>

G_BEGIN_DECLS

gboolean xfce_shortcut_conflict_dialog (GtkWindow   *parent,
                                        const gchar *owner,
                                        const gchar *other,
                                        const gchar *shortcut,
                                        const gchar *owner_action,
                                        const gchar *other_action,
                                        gboolean     ignore_same_provider);

G_END_DECLS

#endif /* !__XFCE_SHORTCUTS_H__ */
