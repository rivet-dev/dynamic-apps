/*
 * Copyright (C) 2007-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __CLOCK_H__
#define __CLOCK_H__

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/libxfce4panel.h>

G_BEGIN_DECLS

#define CLOCK_INTERVAL_SECOND (1)
#define CLOCK_INTERVAL_MINUTE (60)

typedef struct _ClockPlugin        ClockPlugin;
typedef struct _ClockPluginClass   ClockPluginClass;
typedef struct _ClockPluginTimeout ClockPluginTimeout;

#define XFCE_TYPE_CLOCK_PLUGIN            (clock_plugin_get_type ())
#define XFCE_CLOCK_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_CLOCK_PLUGIN, ClockPlugin))
#define XFCE_CLOCK_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_CLOCK_PLUGIN, ClockPluginClass))
#define XFCE_IS_CLOCK_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_CLOCK_PLUGIN))
#define XFCE_IS_CLOCK_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_CLOCK_PLUGIN))
#define XFCE_CLOCK_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_CLOCK_PLUGIN, ClockPluginClass))



GType               clock_plugin_get_type             (void) G_GNUC_CONST;

void                clock_plugin_register_type        (XfcePanelTypeModule *type_module);

G_END_DECLS

#endif /* !__CLOCK_H__ */
