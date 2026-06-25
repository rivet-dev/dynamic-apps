/*
 * Copyright (c) 2002      Anders Carlsson <andersca@gnu.org>
 * Copyright (c) 2003-2006 Vincent Untz
 * Copyright (c) 2008      Red Hat, Inc.
 * Copyright (c) 2009-2010 Nick Schermer <nick@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4panel/libxfce4panel.h>

#include <common/panel-private.h>
#include <common/panel-debug.h>

#include "systray-socket.h"



struct _SystraySocketClass
{
  GtkSocketClass __parent__;
};

struct _SystraySocket
{
  GtkSocket __parent__;

  /* plug window */
  Window           window;

  gchar           *name;

  guint            is_composited : 1;
  guint            parent_relative_bg : 1;
  guint            hidden : 1;
};



static void     systray_socket_finalize      (GObject        *object);
static void     systray_socket_realize       (GtkWidget      *widget);
static void     systray_socket_size_allocate (GtkWidget      *widget,
                                              GtkAllocation  *allocation);
static gboolean systray_socket_draw          (GtkWidget      *widget,
                                              cairo_t        *cr);



XFCE_PANEL_DEFINE_TYPE (SystraySocket, systray_socket, GTK_TYPE_SOCKET)



static void
systray_socket_class_init (SystraySocketClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = systray_socket_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = systray_socket_realize;
  gtkwidget_class->size_allocate = systray_socket_size_allocate;
  gtkwidget_class->draw = systray_socket_draw;
}



static void
systray_socket_init (SystraySocket *socket)
{
  socket->hidden = FALSE;
  socket->name = NULL;
}



static void
systray_socket_finalize (GObject *object)
{
  SystraySocket *socket = XFCE_SYSTRAY_SOCKET (object);

  g_free (socket->name);

  G_OBJECT_CLASS (systray_socket_parent_class)->finalize (object);
}



static void
systray_socket_realize (GtkWidget *widget)
{
  SystraySocket *socket = XFCE_SYSTRAY_SOCKET (widget);
  GdkRGBA        transparent = { 0.0, 0.0, 0.0, 0.0 };
  GdkWindow     *window;

  GTK_WIDGET_CLASS (systray_socket_parent_class)->realize (widget);

  window = gtk_widget_get_window (widget);

  if (socket->is_composited)
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gdk_window_set_background_rgba (window, &transparent);
      gdk_window_set_composited (window, TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS

      socket->parent_relative_bg = FALSE;
    }
  else if (gtk_widget_get_visual (widget) ==
           gdk_window_get_visual (gdk_window_get_parent (window)))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gdk_window_set_background_pattern (window, NULL);
G_GNUC_END_IGNORE_DEPRECATIONS

      socket->parent_relative_bg = TRUE;
    }
  else
    {
      socket->parent_relative_bg = FALSE;
    }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_window_set_composited (window, socket->is_composited);
G_GNUC_END_IGNORE_DEPRECATIONS

  gtk_widget_set_app_paintable (widget,
      socket->parent_relative_bg || socket->is_composited);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_widget_set_double_buffered (widget, socket->parent_relative_bg);
G_GNUC_END_IGNORE_DEPRECATIONS

  panel_debug_filtered (PANEL_DEBUG_SYSTRAY,
      "socket %s[%p] (composited=%s, relative-bg=%s",
      systray_socket_get_name (socket), socket,
      PANEL_DEBUG_BOOL (socket->is_composited),
      PANEL_DEBUG_BOOL (socket->parent_relative_bg));
}



static void
systray_socket_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  SystraySocket *socket = XFCE_SYSTRAY_SOCKET (widget);
  GtkAllocation  widget_allocation;
  gboolean       moved;
  gboolean       resized;

  gtk_widget_get_allocation (widget, &widget_allocation);
  moved = allocation->x != widget_allocation.x
       || allocation->y != widget_allocation.y;
  resized = allocation->width != widget_allocation.width
          ||allocation->height != widget_allocation.height;

  if ((moved || resized)
      && gtk_widget_get_mapped (widget))
    {
      if (socket->is_composited)
        gdk_window_invalidate_rect (gdk_window_get_parent (gtk_widget_get_window (widget)),
                                    &widget_allocation, FALSE);
    }

  GTK_WIDGET_CLASS (systray_socket_parent_class)->size_allocate (widget, allocation);

  if ((moved || resized)
      && gtk_widget_get_mapped (widget))
    {
      if (socket->is_composited)
        gdk_window_invalidate_rect (gdk_window_get_parent (gtk_widget_get_window (widget)),
                                    &widget_allocation, FALSE);
      else if (moved && socket->parent_relative_bg)
        systray_socket_force_redraw (socket);
    }
}



static gboolean
systray_socket_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  SystraySocket *socket = XFCE_SYSTRAY_SOCKET (widget);

  if (socket->is_composited)
    {
      /* clear to transparent */
      cairo_set_source_rgba (cr, 0, 0, 0, 0);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_fill (cr);
    }
  else if (socket->parent_relative_bg)
    {
      /* clear to parent-relative pixmap */
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_fill (cr);
    }

  return FALSE;
}



GtkWidget *
systray_socket_new (GdkScreen       *screen,
                    Window           window)
{
  SystraySocket     *socket;
  GdkDisplay        *display;
  XWindowAttributes  attr;
  gint               result;
  GdkVisual         *visual;
  gint               red_prec, green_prec, blue_prec;
  gboolean           supports_composite = FALSE;

  panel_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  /* get the window attributes */
  display = gdk_screen_get_display (screen);
  gdk_x11_display_error_trap_push (display);
  result = XGetWindowAttributes (GDK_DISPLAY_XDISPLAY (display),
                                 window, &attr);

  /* leave on an error or if the window does not exist */
  if (gdk_x11_display_error_trap_pop (display) != 0 || result == 0)
    return NULL;

  /* get the windows visual */
  visual = gdk_x11_screen_lookup_visual (screen, attr.visual->visualid);
  panel_return_val_if_fail (visual == NULL || GDK_IS_VISUAL (visual), NULL);
  if (G_UNLIKELY (visual == NULL))
    return NULL;

  /* create a new socket */
  socket = g_object_new (XFCE_TYPE_SYSTRAY_SOCKET, NULL);
  socket->window = window;
  socket->is_composited = FALSE;
  gtk_widget_set_visual (GTK_WIDGET (socket), visual);

  /* check if there is an alpha channel in the visual */
  gdk_visual_get_red_pixel_details (visual, NULL, NULL, &red_prec);
  gdk_visual_get_green_pixel_details (visual, NULL, NULL, &green_prec);
  gdk_visual_get_blue_pixel_details (visual, NULL, NULL, &blue_prec);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  supports_composite = gdk_display_supports_composite (gdk_screen_get_display (screen));
G_GNUC_END_IGNORE_DEPRECATIONS
  if (red_prec + blue_prec + green_prec < gdk_visual_get_depth (visual)
      && supports_composite)
    socket->is_composited = TRUE;

  return GTK_WIDGET (socket);
}



void
systray_socket_force_redraw (SystraySocket *socket)
{
  GtkWidget     *widget = GTK_WIDGET (socket);
  XEvent         xev;
  GdkDisplay    *display;
  GtkAllocation  allocation;

  panel_return_if_fail (XFCE_IS_SYSTRAY_SOCKET (socket));

  if (gtk_widget_get_mapped (widget) && socket->parent_relative_bg)
    {
      display = gtk_widget_get_display (widget);

      gtk_widget_get_allocation (widget, &allocation);

      xev.xexpose.type = Expose;
      xev.xexpose.window = GDK_WINDOW_XID (gtk_socket_get_plug_window (GTK_SOCKET (socket)));
      xev.xexpose.x = 0;
      xev.xexpose.y = 0;
      xev.xexpose.width = allocation.width;
      xev.xexpose.height = allocation.height;
      xev.xexpose.count = 0;

      gdk_x11_display_error_trap_push (display);
      XSendEvent (GDK_DISPLAY_XDISPLAY (display),
                  xev.xexpose.window,
                  False, ExposureMask,
                  &xev);
      /* We have to sync to reliably catch errors from the XSendEvent(),
       * since that is asynchronous.
       */
      XSync (GDK_DISPLAY_XDISPLAY (display), False);
      gdk_x11_display_error_trap_pop_ignored (display);
    }
}



gboolean
systray_socket_is_composited (SystraySocket *socket)
{
  panel_return_val_if_fail (XFCE_IS_SYSTRAY_SOCKET (socket), FALSE);

  return socket->is_composited;
}



static gchar *
systray_socket_get_name_prop (SystraySocket *socket,
                              const gchar   *prop_name,
                              const gchar   *type_name)
{
  GdkDisplay *display;
  Atom        req_type, type;
  gint        result;
  gchar      *val;
  gint        format;
  gulong      nitems;
  gulong      bytes_after;
  gchar      *name = NULL;

  panel_return_val_if_fail (XFCE_IS_SYSTRAY_SOCKET (socket), NULL);
  panel_return_val_if_fail (type_name != NULL && prop_name != NULL, NULL);

  display = gtk_widget_get_display (GTK_WIDGET (socket));

  req_type = gdk_x11_get_xatom_by_name_for_display (display, type_name);

  gdk_x11_display_error_trap_push (display);

  result = XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
                               socket->window,
                               gdk_x11_get_xatom_by_name_for_display (display, prop_name),
                               0, G_MAXLONG, False,
                               req_type,
                               &type, &format, &nitems,
                               &bytes_after,
                               (guchar **) &val);

  /* check if everything went fine */
  if (gdk_x11_display_error_trap_pop (display) != 0
      || result != Success
      || val == NULL)
    return NULL;

  /* check the returned data */
  if (type == req_type
      && format == 8
      && nitems > 0
      && g_utf8_validate (val, nitems, NULL))
   {
     /* lowercase the result */
     name = g_utf8_strdown (val, nitems);
   }

  XFree (val);

  return name;
}



const gchar *
systray_socket_get_name (SystraySocket *socket)
{
  panel_return_val_if_fail (XFCE_IS_SYSTRAY_SOCKET (socket), NULL);

  if (G_LIKELY (socket->name != NULL))
    return socket->name;

  /* try _NET_WM_NAME first, for gtk icon implementations, fall back to
   * WM_NAME for qt icons */
  socket->name = systray_socket_get_name_prop (socket, "_NET_WM_NAME", "UTF8_STRING");
  if (G_UNLIKELY (socket->name == NULL))
    socket->name = systray_socket_get_name_prop (socket, "WM_NAME", "STRING");

  return socket->name;
}



Window *
systray_socket_get_window (SystraySocket *socket)
{
  panel_return_val_if_fail (XFCE_IS_SYSTRAY_SOCKET (socket), NULL);

  return &socket->window;
}



gboolean
systray_socket_get_hidden (SystraySocket *socket)
{
  panel_return_val_if_fail (XFCE_IS_SYSTRAY_SOCKET (socket), FALSE);

  return socket->hidden;
}



void
systray_socket_set_hidden (SystraySocket *socket,
                           gboolean       hidden)
{
  panel_return_if_fail (XFCE_IS_SYSTRAY_SOCKET (socket));

  socket->hidden = hidden;
}
