/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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
 *
 */

#ifndef __GSD_CLIPBOARD_MANAGER_H
#define __GSD_CLIPBOARD_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GsdClipboardManager        GsdClipboardManager;
typedef struct _GsdClipboardManagerClass   GsdClipboardManagerClass;
typedef struct _GsdClipboardManagerPrivate GsdClipboardManagerPrivate;

#define GSD_TYPE_CLIPBOARD_MANAGER         (gsd_clipboard_manager_get_type ())
#define GSD_CLIPBOARD_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_CLIPBOARD_MANAGER, GsdClipboardManager))
#define GSD_CLIPBOARD_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GSD_TYPE_CLIPBOARD_MANAGER, GsdClipboardManagerClass))
#define GSD_IS_CLIPBOARD_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_CLIPBOARD_MANAGER))
#define GSD_IS_CLIPBOARD_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_CLIPBOARD_MANAGER))
#define GSD_CLIPBOARD_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_CLIPBOARD_MANAGER, GsdClipboardManagerClass))

struct _GsdClipboardManager
{
    GObject                     parent;
    GsdClipboardManagerPrivate *priv;
};

struct _GsdClipboardManagerClass
{
    GObjectClass parent_class;
};

GType gsd_clipboard_manager_get_type (void);

gboolean gsd_clipboard_manager_start (GsdClipboardManager *manager,
                                      gboolean             replace);

void     gsd_clipboard_manager_stop  (GsdClipboardManager *manager);

G_END_DECLS

#endif /* __GSD_CLIPBOARD_MANAGER_H */
