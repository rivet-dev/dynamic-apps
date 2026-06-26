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

#include "screenshooter-capture.h"

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#include "screenshooter-capture-x11.h"
#endif
#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#include "screenshooter-capture-wayland.h"
#endif



GdkPixbuf
*screenshooter_capture_screenshot (gint     region,
                                   gint     delay,
                                   gboolean show_mouse,
                                   gboolean show_border)
{
  GdkPixbuf *screenshot = NULL;

#ifdef ENABLE_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    screenshot = screenshooter_capture_screenshot_x11 (region, delay, show_mouse, show_border);
#endif
#ifdef ENABLE_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    screenshot = screenshooter_capture_screenshot_wayland (region, delay, show_mouse, show_border);
#endif

  return screenshot;
}
