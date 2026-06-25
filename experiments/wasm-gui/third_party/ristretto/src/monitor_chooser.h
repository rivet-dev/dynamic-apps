/*
 *  Copyright (c) Stephan Arts 2006-2012 <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#ifndef __RISTRETTO_MONITOR_CHOOSER_H__
#define __RISTRETTO_MONITOR_CHOOSER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define RSTTO_TYPE_MONITOR_CHOOSER rstto_monitor_chooser_get_type ()
G_DECLARE_FINAL_TYPE (RsttoMonitorChooser, rstto_monitor_chooser, RSTTO, MONITOR_CHOOSER, GtkWidget)

typedef struct _RsttoMonitorChooserPrivate RsttoMonitorChooserPrivate;

struct _RsttoMonitorChooser
{
    GtkWidget parent;
    RsttoMonitorChooserPrivate *priv;
};



GtkWidget *
rstto_monitor_chooser_new (void);

gint
rstto_monitor_chooser_add (RsttoMonitorChooser *chooser,
                           gint width,
                           gint height);

gint
rstto_monitor_chooser_set_image_surface (RsttoMonitorChooser *chooser,
                                         gint monitor_id,
                                         cairo_surface_t *surface,
                                         GError **error);

gint
rstto_monitor_chooser_get_selected (RsttoMonitorChooser *chooser);

void
rstto_monitor_chooser_get_dimensions (RsttoMonitorChooser *chooser,
                                      gint nr,
                                      gint *width,
                                      gint *height);

G_END_DECLS

#endif /* __RISTRETTO_MONITOR_CHOOSER_H__ */
