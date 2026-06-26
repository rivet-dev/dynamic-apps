/*  $Id$
 *
 *  Copyright © 2008-2010 Jérôme Guelfucci <jeromeg@xfce.org>
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

#include "screenshooter-utils-x11.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <cairo-xlib.h>
#include <libxfce4ui/libxfce4ui.h>



/* Internals */



/* Replacement for gdk_screen_get_active_window */
static Window
get_active_window_from_xlib (void)
{
  GdkDisplay *display;
  Display *dsp;
  Atom active_win, type;
  int status, format;
  unsigned long n_items, bytes_after;
  unsigned char *prop;
  Window window;

  display = gdk_display_get_default ();
  dsp = gdk_x11_display_get_xdisplay (display);

  active_win = XInternAtom (dsp, "_NET_ACTIVE_WINDOW", True);
  if (active_win == None)
    return None;

  gdk_x11_display_error_trap_push (display);

  status = XGetWindowProperty (dsp, DefaultRootWindow (dsp),
                               active_win, 0, G_MAXLONG, False,
                               XA_WINDOW, &type, &format, &n_items,
                               &bytes_after, &prop);

  if (status != Success || type != XA_WINDOW)
    {
      if (prop)
        XFree (prop);

      gdk_x11_display_error_trap_pop_ignored (display);
      return None;
    }

  if (gdk_x11_display_error_trap_pop (display) != Success)
    {
      if (prop)
        XFree (prop);

      return None;
    }

  window = *(Window *)(void*) prop;
  XFree (prop);
  return window;
}



/* Public */



/* Replacement for gdk_screen_width/gdk_screen_height */
void
screenshooter_get_screen_geometry (GdkRectangle *geometry)
{
  GdkRectangle *tmp = xfce_gdk_screen_get_geometry ();
  geometry->width = tmp->width;
  geometry->height = tmp->height;
  g_free (tmp);
}



GdkWindow*
screenshooter_get_active_window (GdkScreen *screen,
                                 gboolean  *needs_unref,
                                 gboolean  *border)
{
  GdkWindow *window, *window2;
  Window xwindow;
  GdkDisplay *display;

  TRACE ("Get the active window");

  display = gdk_display_get_default ();
  xwindow = get_active_window_from_xlib ();
  if (xwindow != None)
    window = gdk_x11_window_foreign_new_for_display (display, xwindow);
  else
    window = NULL;

  /* If there is no active window, we fallback to the whole screen. */
  if (G_UNLIKELY (window == NULL))
    {
      TRACE ("No active window, fallback to the root window");

      window = gdk_get_default_root_window ();
      *needs_unref = FALSE;
      *border = FALSE;
    }
  else if (G_UNLIKELY (gdk_window_is_destroyed (window)))
    {
      TRACE ("The active window is destroyed, fallback to the root window.");

      g_object_unref (window);
      window = gdk_get_default_root_window ();
      *needs_unref = FALSE;
      *border = FALSE;
    }
  else if (gdk_window_get_type_hint (window) == GDK_WINDOW_TYPE_HINT_DESKTOP)
    {
      /* If the active window is the desktop, grab the whole screen */
      TRACE ("The active window is the desktop, fallback to the root window");

      g_object_unref (window);

      window = gdk_get_default_root_window ();
      *needs_unref = FALSE;
      *border = FALSE;
    }
  else
    {
      /* Else we find the toplevel window to grab the decorations. */
      TRACE ("Active window is a normal window, grab the toplevel window");

      window2 = gdk_window_get_toplevel (window);

      g_object_unref (window);

      window = window2;
      *border = TRUE;
    }

  return window;
}



/*
 * Replacement for gdk_pixbuf_get_from_window which only takes
 * one (first) correct screenshot per execution.
 * For more details see:
 * https://gitlab.xfce.org/apps/xfce4-screenshooter/-/issues/89
 */
GdkPixbuf*
screenshooter_pixbuf_get_from_window (GdkWindow *window,
                                      gint       src_x,
                                      gint       src_y,
                                      gint       width,
                                      gint       height)
{
  gint             scale_factor;
  cairo_surface_t *surface;
  GdkPixbuf       *pixbuf;

  scale_factor = gdk_window_get_scale_factor (window);
  surface = cairo_xlib_surface_create (gdk_x11_display_get_xdisplay (gdk_window_get_display (window)),
                                       gdk_x11_window_get_xid (window),
                                       gdk_x11_visual_get_xvisual (gdk_window_get_visual (window)),
                                       gdk_window_get_width (window) * scale_factor,
                                       gdk_window_get_height (window) * scale_factor);
  pixbuf = gdk_pixbuf_get_from_surface (surface,
                                        src_x * scale_factor,
                                        src_y * scale_factor,
                                        width * scale_factor,
                                        height * scale_factor);

  cairo_surface_destroy (surface);

  return pixbuf;
}
