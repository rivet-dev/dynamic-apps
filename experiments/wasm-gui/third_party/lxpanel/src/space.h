/*
 * Copyright (C) 2014-2016 Andriy Grytsenko <andrej@rep.kiev.ua>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __SPACE_H__
#define __SPACE_H__ 1

#include "plugin.h"

#define PANEL_TYPE_SPACE               (panel_space_get_type())
#define PANEL_SPACE(obj)               (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                        PANEL_TYPE_SPACE, PanelSpace))
#define PANEL_IS_SPACE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                        PANEL_TYPE_SPACE))

extern GType panel_space_get_type   (void) G_GNUC_CONST;

typedef struct _PanelSpace           PanelSpace;
typedef struct _PanelSpaceClass      PanelSpaceClass;

GtkWidget *_panel_space_new(LXPanel *panel, config_setting_t *settings);
void _panel_space_resize(GtkWidget *spacer, gint size);
gint _panel_space_get_size(GtkWidget *spacer);

extern LXPanelPluginInit _lxpanel_static_plugin_space;

#endif
