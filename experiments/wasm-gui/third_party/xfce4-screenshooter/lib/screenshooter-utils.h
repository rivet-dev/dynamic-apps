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

#ifndef __HAVE_UTILS_H__
#define __HAVE_UTILS_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "screenshooter-global.h"



void       screenshooter_copy_to_clipboard        (GdkPixbuf      *screenshot);
void       screenshooter_read_rc_file             (const gchar    *file,
                                                   ScreenshotData *sd);
void       screenshooter_write_rc_file            (const gchar    *file,
                                                   ScreenshotData *sd);
void       screenshooter_open_screenshot          (const gchar    *screenshot_path,
                                                   const gchar    *application,
                                                   GAppInfo       *app_info);
gchar     *screenshooter_get_home_uri             (void);
gchar     *screenshooter_get_xdg_image_dir_uri    (void);
gboolean   screenshooter_is_remote_uri            (const gchar    *uri);
void       screenshooter_error                    (const gchar    *format,
                                                   ...);
gchar     *screenshooter_get_filename_for_uri     (const gchar    *uri,
                                                   const gchar    *title,
                                                   const gchar    *extension,
                                                   gboolean        timestamp);
void       screenshooter_open_help                (GtkWindow      *parent);
gboolean   screenshooter_f1_key                   (GtkWidget      *widget,
                                                   GdkEventKey    *event,
                                                   gpointer        user_data);
void       screenshooter_show_file_in_folder      (const gchar    *save_location);
gboolean   screenshooter_is_format_supported      (const gchar    *format);
void       screenshooter_restrict_file_permission (GFile          *file);

#endif
