/*  $Id$
 *
 *  Copyright © 2008-2009 Jérôme Guelfucci <jeromeg@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __HAVE_DIALOGS_H__
#define __HAVE_DIALOGS_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "screenshooter-utils.h"
#include "screenshooter-global.h"
#include "screenshooter-actions.h"

#ifdef HAVE_GIO
#include <gio/gio.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>



GtkWidget *screenshooter_actions_dialog_new    (ScreenshotData         *sd);
void       screenshooter_region_dialog_show    (ScreenshotData         *sd,
                                                gboolean                plugin);
GtkWidget *screenshooter_region_dialog_new     (ScreenshotData         *sd,
                                                gboolean                plugin);
gchar     *screenshooter_save_screenshot       (GdkPixbuf              *screenshot,
                                                const gchar            *directory,
                                                const gchar            *filename,
                                                const gchar            *extension,
                                                gboolean                save_dialog,
                                                gboolean                show_preview);
gchar     *screenshooter_save_screenshot_to    (GdkPixbuf              *screenshot,
                                                const gchar            *save_uri);
void       screenshooter_preference_dialog_run (GtkWidget              *parent);

#endif
