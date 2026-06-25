/*-
 * Copyright (c) 2004 os-cillation e.K.
 * Copyright (c) 2012 Stephan Arts <stephan@xfce.org>
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __RSTTO_ICON_BAR_H__
#define __RSTTO_ICON_BAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define RSTTO_TYPE_ICON_BAR (rstto_icon_bar_get_type ())
G_DECLARE_FINAL_TYPE (RsttoIconBar, rstto_icon_bar, RSTTO, ICON_BAR, GtkContainer)

typedef struct _RsttoIconBarPrivate RsttoIconBarPrivate;

struct _RsttoIconBar
{
    GtkContainer parent;
    RsttoIconBarPrivate *priv;
};



GtkWidget *
rstto_icon_bar_new (GtkWidget *s_window);
GtkWidget *
rstto_icon_bar_new_with_model (GtkTreeModel *model);

GtkTreeModel *
rstto_icon_bar_get_model (RsttoIconBar *icon_bar);
void
rstto_icon_bar_set_model (RsttoIconBar *icon_bar,
                          GtkTreeModel *model);

GtkOrientation
rstto_icon_bar_get_orientation (RsttoIconBar *icon_bar);
void
rstto_icon_bar_set_orientation (RsttoIconBar *icon_bar,
                                GtkOrientation orientation);

gint
rstto_icon_bar_get_active (RsttoIconBar *icon_bar);
void
rstto_icon_bar_set_active (RsttoIconBar *icon_bar,
                           gint idx);

gboolean
rstto_icon_bar_get_active_iter (RsttoIconBar *icon_bar,
                                GtkTreeIter *iter);
void
rstto_icon_bar_set_active_iter (RsttoIconBar *icon_bar,
                                GtkTreeIter *iter);

gint
rstto_icon_bar_get_n_visible_items (RsttoIconBar *icon_bar);

gboolean
rstto_icon_bar_get_show_text (RsttoIconBar *icon_bar);
void
rstto_icon_bar_set_show_text (RsttoIconBar *icon_bar,
                              gboolean show_text);

void
rstto_icon_bar_show_active (RsttoIconBar *icon_bar);

G_END_DECLS

#endif /* !__RSTTO_ICON_BAR_H__ */

