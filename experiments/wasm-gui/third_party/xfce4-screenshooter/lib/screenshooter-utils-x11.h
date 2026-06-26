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

#ifndef __HAVE_UTILS_X11_H__
#define __HAVE_UTILS_X11_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "screenshooter-global.h"



void       screenshooter_get_screen_geometry      (GdkRectangle   *geometry);
GdkWindow *screenshooter_get_active_window        (GdkScreen      *screen,
                                                   gboolean       *needs_unref,
                                                   gboolean       *border);
GdkPixbuf *screenshooter_pixbuf_get_from_window   (GdkWindow      *window,
                                                   gint            src_x,
                                                   gint            src_y,
                                                   gint            width,
                                                   gint            height);

#endif
