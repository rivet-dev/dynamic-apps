/**
 * Copyright (C) 2016 Andriy Grytsenko <andrej@rep.kiev.ua>
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

#ifndef __LAUNCH_BUTTON_H__
#define __LAUNCH_BUTTON_H__ 1

#include "plugin.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PANEL_TYPE_LAUNCH_BUTTON           (launch_button_get_type())
#define PANEL_LAUNCH_BUTTON(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                            PANEL_TYPE_LAUNCH_BUTTON, LaunchButton))
#define PANEL_LAUNCH_BUTTON_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass), \
                                            PANEL_TYPE_LAUNCH_BUTTON, LaunchButtonClass))
#define PANEL_IS_LAUNCH_BUTTON(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                            PANEL_TYPE_LAUNCH_BUTTON))
#define PANEL_IS_LAUNCH_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                             PANEL_TYPE_LAUNCH_BUTTON))

extern GType launch_button_get_type        (void) G_GNUC_CONST;

typedef struct _LaunchButton                LaunchButton;
typedef struct _LaunchButtonClass           LaunchButtonClass;

struct _LaunchButtonClass
{
    GtkEventBoxClass parent_class;
};

/* creates new button */
LaunchButton *launch_button_new(LXPanel *panel, GtkWidget *plugin, FmPath *path, config_setting_t *settings);
FmFileInfo *launch_button_get_file_info(LaunchButton *btn);
const char *launch_button_get_disp_name(LaunchButton *btn);
FmIcon *launch_button_get_icon(LaunchButton *btn);
config_setting_t *launch_button_get_settings(LaunchButton *btn);
void launch_button_set_settings(LaunchButton *btn, config_setting_t *settings);
gboolean launch_button_wait_load(LaunchButton *btn);

G_END_DECLS

#endif /* __LAUNCH_BUTTON_H__ */
