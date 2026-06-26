/*  $Id$
 *
 *  Copyright © 2024 André Miranda <andreldm@xfce.org>
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

#ifndef __HAVE_CAPTURE_WAYLAND_H__
#define __HAVE_CAPTURE_WAYLAND_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk/gdk.h>



GdkPixbuf
*screenshooter_capture_screenshot_wayland (gint     region,
                                           gint     delay,
                                           gboolean show_mouse,
                                           gboolean show_border);

#endif
