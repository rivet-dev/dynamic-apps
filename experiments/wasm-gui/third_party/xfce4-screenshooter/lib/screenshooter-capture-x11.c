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

#include "screenshooter-capture-x11.h"

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#include <X11/extensions/shape.h>
#include <X11/extensions/XInput2.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <unistd.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "screenshooter-global.h"
#include "screenshooter-utils-x11.h"

#define BACKGROUND_TRANSPARENCY 0.4

enum {
  ANCHOR_UNSET = 0,
  ANCHOR_NONE = 1,
  ANCHOR_TOP = 2,
  ANCHOR_LEFT = 4
};

/* Rubberband data for composited environment */
typedef struct
{
  gboolean left_pressed;
  gboolean rubber_banding;
  gboolean cancelled;
  gboolean move_rectangle;
  gint anchor;
  gint x;
  gint y;
  gint x_root;
  gint y_root;
  cairo_rectangle_int_t rectangle;
  GtkWidget *size_window;
  GtkWidget *size_label;
} RubberBandData;

/* For non-composited environments */
typedef struct
{
  gboolean pressed;
  gboolean cancelled;
  gboolean move_rectangle;
  gint anchor;
  cairo_rectangle_int_t rectangle;
  gint x1, y1; /* holds the position where the mouse was pressed */
  GC *context;
} RbData;


/* Prototypes */

static void             free_pixmap_data                    (guchar *pixels,
                                                             gpointer data);
static GdkPixbuf       *get_cursor_pixbuf                   (GdkDisplay *display,
                                                             GdkWindow *root,
                                                             gint *cursorx,
                                                             gint *cursory,
                                                             gint *xhot,
                                                             gint *yhot);
static void             capture_cursor                      (GdkPixbuf      *screenshot,
                                                             GtkBorder      *window_extents,
                                                             gint            scale,
                                                             gint            x,
                                                             gint            y,
                                                             gint            w,
                                                             gint            h);
static GdkPixbuf       *get_window_screenshot               (GdkWindow      *window,
                                                             gboolean        show_mouse,
                                                             gboolean        border);
static GdkFilterReturn  region_filter_func                  (GdkXEvent      *xevent,
                                                             GdkEvent       *event,
                                                             RbData         *rbdata);
static GdkPixbuf       *get_rectangle_screenshot            (gint            delay,
                                                             gboolean        show_mouse);
static gboolean         cb_key_pressed                      (GtkWidget      *widget,
                                                             GdkEventKey    *event,
                                                             RubberBandData *rbdata);
static gboolean         cb_key_released                     (GtkWidget      *widget,
                                                             GdkEventKey    *event,
                                                             RubberBandData *rbdata);
static gboolean         cb_draw                             (GtkWidget      *widget,
                                                             cairo_t        *cr,
                                                             RubberBandData *rbdata);
static gboolean         cb_button_pressed                   (GtkWidget      *widget,
                                                             GdkEventButton *event,
                                                             RubberBandData *rbdata);
static gboolean         cb_button_released                  (GtkWidget      *widget,
                                                             GdkEventButton *event,
                                                             RubberBandData *rbdata);
static gboolean         cb_motion_notify                    (GtkWidget      *widget,
                                                             GdkEventMotion *event,
                                                             RubberBandData *rbdata);
static GdkPixbuf       *get_rectangle_screenshot_composited (gint            delay,
                                                             gboolean        show_mouse);



/* Internals */


static Window
find_wm_xid (GdkWindow *window)
{
  Window xid, root, parent, *children;
  unsigned int nchildren;

  if (window == gdk_get_default_root_window ())
    return None;

  xid = GDK_WINDOW_XID (window);

  do
    {
      if (XQueryTree (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                      xid, &root, &parent, &children, &nchildren) == 0)
        {
          g_warning ("Couldn't find window manager window");
          return None;
        }

      if (root == parent)
        return xid;

      xid = parent;
    }
  while (TRUE);
}


static void free_pixmap_data (guchar *pixels,  gpointer data)
{
  g_free (pixels);
}


static GdkPixbuf *get_cursor_pixbuf (GdkDisplay *display,
                                     GdkWindow *root,
                                     gint *cursorx,
                                     gint *cursory,
                                     gint *xhot,
                                     gint *yhot)
{
  GdkCursor *cursor = NULL;
  GdkPixbuf *cursor_pixbuf = NULL;
  GdkDevice *pointer = NULL;
  GdkSeat   *seat = NULL;

#ifdef HAVE_XFIXES
  XFixesCursorImage *cursor_image = NULL;
  guint32            tmp;
  guchar            *cursor_pixmap_data = NULL;
  gint               i, j;
  int                event_basep;
  int                error_basep;

  if (!XFixesQueryExtension (GDK_DISPLAY_XDISPLAY (display),
                             &event_basep,
                             &error_basep))
    goto fallback;

  TRACE ("Get the mouse cursor, its image, position and hotspot");

  cursor_image = XFixesGetCursorImage (GDK_DISPLAY_XDISPLAY (display));
  if (cursor_image == NULL)
    goto fallback;

  *cursorx = cursor_image->x;
  *cursory = cursor_image->y;
  *xhot = cursor_image->xhot;
  *yhot = cursor_image->yhot;

  /* cursor_image->pixels contains premultiplied 32-bit ARGB data stored
   * in long (!) */
  cursor_pixmap_data =
    g_new (guchar, cursor_image->width * cursor_image->height * 4);

  for (i = 0, j = 0;
       i < cursor_image->width * cursor_image->height;
       i++, j += 4)
    {
      tmp = ((guint32)cursor_image->pixels[i] << 8) | \
            ((guint32)cursor_image->pixels[i] >> 24);
      cursor_pixmap_data[j] = tmp >> 24;
      cursor_pixmap_data[j + 1] = (tmp >> 16) & 0xff;
      cursor_pixmap_data[j + 2] = (tmp >> 8) & 0xff;
      cursor_pixmap_data[j + 3] = tmp & 0xff;
    }

  cursor_pixbuf = gdk_pixbuf_new_from_data (cursor_pixmap_data,
                                            GDK_COLORSPACE_RGB,
                                            TRUE,
                                            8,
                                            cursor_image->width,
                                            cursor_image->height,
                                            cursor_image->width * 4,
                                            free_pixmap_data,
                                            NULL);

  XFree(cursor_image);

  if (cursor_pixbuf != NULL)
    return cursor_pixbuf;

fallback:
#endif
  TRACE ("Get the mouse cursor and its image through fallback mode");

  /* cursors are not scaled */
  if (gdk_window_get_scale_factor (root) != 1)
    return NULL;

  cursor = gdk_cursor_new_for_display (display, GDK_LEFT_PTR);
  cursor_pixbuf = gdk_cursor_get_image (cursor);

  if (cursor_pixbuf == NULL)
    return NULL;

  TRACE ("Get the coordinates of the cursor");

  seat = gdk_display_get_default_seat (gdk_display_get_default ());
  pointer = gdk_seat_get_pointer (seat);
  gdk_window_get_device_position (root, pointer, cursorx, cursory, NULL);

  TRACE ("Get the cursor hotspot");
  sscanf (gdk_pixbuf_get_option (cursor_pixbuf, "x_hot"), "%d", xhot);
  sscanf (gdk_pixbuf_get_option (cursor_pixbuf, "y_hot"), "%d", yhot);

  g_object_unref (cursor);

  return cursor_pixbuf;
}



static void capture_cursor (GdkPixbuf *screenshot,
                            GtkBorder *window_extents,
                            gint scale,
                            gint x,
                            gint y,
                            gint w,
                            gint h)
{
  gint cursorx, cursory, xhot, yhot;
  GdkPixbuf *cursor_pixbuf;
  GdkRectangle rectangle_window, rectangle_cursor;

  cursor_pixbuf = get_cursor_pixbuf (gdk_display_get_default (),
                                     gdk_get_default_root_window (),
                                     &cursorx, &cursory, &xhot, &yhot);

  if (G_UNLIKELY (cursor_pixbuf == NULL))
    return;

  /* rectangle_window stores the window coordinates */
  rectangle_window.x = x * scale;
  rectangle_window.y = y * scale;
  rectangle_window.width = w * scale;
  rectangle_window.height = h * scale;

  if (window_extents != NULL)
    {
      rectangle_window.x += window_extents->left - 1;
      rectangle_window.y += window_extents->top - 1;
      rectangle_window.width -= window_extents->left + window_extents->right + 2;
      rectangle_window.height -= window_extents->top + window_extents->bottom + 2;
    }

  /* rectangle_cursor stores the cursor coordinates */
  rectangle_cursor.x = cursorx;
  rectangle_cursor.y = cursory;
  rectangle_cursor.width = gdk_pixbuf_get_width (cursor_pixbuf);
  rectangle_cursor.height = gdk_pixbuf_get_height (cursor_pixbuf);

  /* see if the pointer is inside the window */
  if (gdk_rectangle_intersect (&rectangle_window,
                               &rectangle_cursor,
                               &rectangle_cursor))
    {
      int dest_x, dest_y;

      TRACE ("Compose the two pixbufs");

      dest_x = cursorx - rectangle_window.x - xhot;
      dest_y = cursory - rectangle_window.y - yhot;

      gdk_pixbuf_composite (cursor_pixbuf, screenshot,
                            CLAMP (dest_x, 0, dest_x),
                            CLAMP (dest_y, 0, dest_y),
                            rectangle_cursor.width,
                            rectangle_cursor.height,
                            dest_x,
                            dest_y,
                            1.0, 1.0,
                            GDK_INTERP_BILINEAR,
                            255);
    }

  g_object_unref (cursor_pixbuf);
}



static GdkPixbuf
*get_window_screenshot (GdkWindow *window,
                        gboolean show_mouse,
                        gboolean border)
{
  gint x_orig, y_orig;
  gint width, height;
  GdkPixbuf *screenshot;
  GdkWindow *root;

  gint scale;
  GdkRectangle real_coords;
  GdkRectangle screen_geometry;
  GtkBorder extents;
  gboolean has_extents;

  Window wm_xid;

  /* Get the root window */
  TRACE ("Get the root window");

  root = gdk_get_default_root_window ();

  if ((has_extents = xfce_has_gtk_frame_extents (window, &extents)))
    border = FALSE;

  if (border) {
    gdk_window_get_frame_extents (window, &real_coords);
  } else {
    real_coords.width = gdk_window_get_width (window);
    real_coords.height = gdk_window_get_height (window);
    gdk_window_get_origin (window, &real_coords.x, &real_coords.y);
  }

  /* Don't grab things offscreen. */

  TRACE ("Make sure we don't grab things offscreen");

  x_orig = real_coords.x;
  y_orig = real_coords.y;
  width  = real_coords.width;
  height = real_coords.height;

  screenshooter_get_screen_geometry (&screen_geometry);

  if (x_orig < 0)
    {
      width = width + x_orig;
      x_orig = 0;
    }

  if (y_orig < 0)
    {
      height = height + y_orig;
      y_orig = 0;
    }

  if (x_orig + width > screen_geometry.width)
    width = screen_geometry.width - x_orig;

  if (y_orig + height > screen_geometry.height)
    height = screen_geometry.height - y_orig;

  scale = gdk_window_get_scale_factor (window);

  TRACE ("Grab the screenshot");

  if (!has_extents)
    screenshot = screenshooter_pixbuf_get_from_window (root, x_orig, y_orig, width, height);
  else
    {
      GdkRectangle rect;
      gdk_window_get_frame_extents (window, &rect);

      /* Add one pixel to sides so the border is visible */
      rect.x = extents.left / scale - 1;
      rect.y = extents.top / scale - 1;
      rect.width -= (extents.left + extents.right) / scale - 2;
      rect.height -= (extents.top + extents.bottom) / scale - 2;
      screenshot = screenshooter_pixbuf_get_from_window (window, rect.x, rect.y, rect.width, rect.height);
    }

  /* Code adapted from gnome-screenshot:
   * Copyright (C) 2001-2006  Jonathan Blandford <jrb@alum.mit.edu>
   * Copyright (C) 2008 Cosimo Cecchi <cosimoc@gnome.org>
   */
  wm_xid = find_wm_xid (window);

  if (border && wm_xid != None)
    {
      GdkRectangle wm_coords;
      XRectangle *rectangles;
      int rectangle_count, rectangle_order, i;
      GtkBorder frame_offset = { 0, 0, 0, 0 };

      GdkWindow *wm_window = gdk_x11_window_foreign_new_for_display
        (gdk_window_get_display (window), wm_xid);

      gdk_window_get_frame_extents (wm_window, &wm_coords);

       /* Calculate frame_offset */
      frame_offset.left = (gdouble) (real_coords.x - wm_coords.x);
      frame_offset.top = (gdouble) (real_coords.y - wm_coords.y);
      frame_offset.right = (gdouble) (wm_coords.width - real_coords.width - frame_offset.left);
      frame_offset.bottom = (gdouble) (wm_coords.height - real_coords.height - frame_offset.top);

      /* we must use XShape to avoid showing what's under the rounder corners
       * of the WM decoration.
       */
      rectangles = XShapeGetRectangles (GDK_DISPLAY_XDISPLAY (gdk_display_get_default()),
                                        wm_xid,
                                        ShapeBounding,
                                        &rectangle_count,
                                        &rectangle_order);
      if (rectangles && rectangle_count > 0)
        {
          gboolean has_alpha = gdk_pixbuf_get_has_alpha (screenshot);
          GdkPixbuf *tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
                                           gdk_pixbuf_get_width (screenshot),
                                           gdk_pixbuf_get_height (screenshot));

          gdk_pixbuf_fill (tmp, 0);

          for (i = 0; i < rectangle_count; i++)
            {
              gint rec_x, rec_y;
              gint rec_width, rec_height;

              /* If we're using invisible borders, the ShapeBounding might not
               * have the same size as the frame extents, as it would include the
               * areas for the invisible borders themselves.
               * In that case, trim every rectangle we get by the offset between the
               * WM window size and the frame extents.
               *
               * Note that the XShape values are in actual pixels, whereas the GDK
               * ones are in display pixels (i.e. scaled), so we need to apply the
               * scale factor to the former to use display pixels for all our math.
               */
              rec_x = rectangles[i].x / scale;
              rec_y = rectangles[i].y / scale;
              rec_width = rectangles[i].width / scale - (frame_offset.left + frame_offset.right);
              rec_height = rectangles[i].height / scale - (frame_offset.top + frame_offset.bottom);

              if (real_coords.x < 0)
                {
                  rec_x += real_coords.x;
                  rec_x = MAX(rec_x, 0);
                  rec_width += real_coords.x;
                }

              if (real_coords.y < 0)
                {
                  rec_y += real_coords.y;
                  rec_y = MAX(rec_y, 0);
                  rec_height += real_coords.y;
                }

              if (x_orig + rec_x + rec_width > screen_geometry.width)
                rec_width = screen_geometry.width - x_orig - rec_x;

              if (y_orig + rec_y + rec_height > screen_geometry.height)
                rec_height = screen_geometry.height - y_orig - rec_y;

              /* Undo the scale factor in order to copy the pixbuf data pixel-wise */
              for (gint y = rec_y * scale; y < (rec_y + rec_height) * scale; y++)
                {
                  guchar *src_pixels, *dest_pixels;
                  gint x;

                  src_pixels = gdk_pixbuf_get_pixels (screenshot)
                             + y * gdk_pixbuf_get_rowstride(screenshot)
                             + rec_x * scale * (has_alpha ? 4 : 3);
                  dest_pixels = gdk_pixbuf_get_pixels (tmp)
                              + y * gdk_pixbuf_get_rowstride (tmp)
                              + rec_x * scale * 4;

                  for (x = 0; x < rec_width * scale; x++)
                    {
                      *dest_pixels++ = *src_pixels++;
                      *dest_pixels++ = *src_pixels++;
                      *dest_pixels++ = *src_pixels++;

                      if (has_alpha)
                        *dest_pixels++ = *src_pixels++;
                      else
                        *dest_pixels++ = 255;
                    }
                }
            }

          g_set_object (&screenshot, tmp);

          XFree (rectangles);
        }
    }

  if (show_mouse)
    capture_cursor (screenshot, has_extents ? &extents : NULL,
                    scale, x_orig, y_orig, width, height);

  return screenshot;
}



/* Callbacks for the rubber banding function */
static gboolean cb_key_pressed (GtkWidget      *widget,
                                GdkEventKey    *event,
                                RubberBandData *rbdata)
{
  guint key = event->keyval;

  if (key == GDK_KEY_Escape)
    {
      gtk_widget_hide (widget);
      rbdata->cancelled = TRUE;
      return TRUE;
    }

  if (rbdata->left_pressed && (key == GDK_KEY_Control_L || key == GDK_KEY_Control_R))
    {
      rbdata->move_rectangle = TRUE;
      return TRUE;
    }

  return FALSE;
}



static gboolean cb_key_released (GtkWidget      *widget,
                                 GdkEventKey    *event,
                                 RubberBandData *rbdata)
{
  guint key = event->keyval;

  if (rbdata->left_pressed && (key == GDK_KEY_Control_L || key == GDK_KEY_Control_R))
    {
      rbdata->move_rectangle = FALSE;
      rbdata->anchor = ANCHOR_UNSET;
      return TRUE;
    }

  return FALSE;
}



static gboolean cb_draw (GtkWidget *widget,
                         cairo_t *cr,
                         RubberBandData *rbdata)
{
  cairo_rectangle_t *rects = NULL;
  cairo_rectangle_list_t *list = NULL;
  gint n_rects = 0, i;

  TRACE ("Draw event received.");

  list = cairo_copy_clip_rectangle_list (cr);
  n_rects = list->num_rectangles;
  rects = list->rectangles;

  if (rbdata->rubber_banding)
    {
      cairo_rectangle_int_t intersect;
      GdkRectangle rect;

      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

      for (i = 0; i < n_rects; ++i)
        {
          /* Restore the transparent background */
          cairo_set_source_rgba (cr, 0, 0, 0, BACKGROUND_TRANSPARENCY);
          cairo_rectangle(cr, rects[i].x, rects[i].y, rects[i].width, rects[i].height);
          cairo_fill (cr);

          rect.x = (rects[i].x);
          rect.y =  (rects[i].y);
          rect.width = (rects[i].width);
          rect.height = (rects[i].height);

          if (!gdk_rectangle_intersect (&rect, &rbdata->rectangle, &intersect))
            {
              continue;
            }

          /* Paint the rubber banding rectangles */
          cairo_set_source_rgba (cr, 1.0f, 1.0f, 1.0f, 0.0f);
          gdk_cairo_rectangle (cr, &intersect);
          cairo_fill (cr);
        }
    }
  else
    {
      /* Draw the transparent background */
      cairo_set_source_rgba (cr, 0, 0, 0, BACKGROUND_TRANSPARENCY);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

      for (i = 0; i < n_rects; ++i)
        {
          cairo_rectangle(cr, rects[i].x, rects[i].y, rects[i].width, rects[i].height);
          cairo_fill (cr);
        }

    }

  cairo_rectangle_list_destroy (list);

  return FALSE;
}



static gboolean cb_button_pressed (GtkWidget *widget,
                                   GdkEventButton *event,
                                   RubberBandData *rbdata)
{
  if (event->button == 1)
    {
      TRACE ("Left button pressed");

      rbdata->left_pressed = TRUE;
      rbdata->x = event->x;
      rbdata->y = event->y;
      rbdata->x_root = event->x_root;
      rbdata->y_root = event->y_root;

      return TRUE;
    }

  return FALSE;
}



static gboolean cb_button_released (GtkWidget *widget,
                                    GdkEventButton *event,
                                    RubberBandData *rbdata)
{
  if (event->button == 1)
    {
      if (rbdata->rubber_banding)
        {
          gtk_widget_destroy (rbdata->size_window);
          rbdata->size_window = NULL;
          gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_NONE);
          return TRUE;
        }
      else
        rbdata->left_pressed = rbdata->rubber_banding = FALSE;
    }

  return FALSE;
}




static gboolean cb_size_window_draw (GtkWidget *widget,
                                     cairo_t *cr,
                                     gpointer user_data)
{
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
  cairo_paint (cr);

  return FALSE;
}



static void size_window_get_offset (GtkWidget *widget,
                                    gint digits,
                                    gint digit_width,
                                    gint line_height,
                                    gint x_event,
                                    gint y_event,
                                    gint *x_offset,
                                    gint *y_offset)
{
  GdkRectangle geometry;
  gint relative_x, relative_y, text_width;

  GdkDisplay *display = gtk_widget_get_display (widget);
  GdkMonitor *monitor = gdk_display_get_monitor_at_point (display, x_event, y_event);
  gdk_monitor_get_geometry (monitor, &geometry);

  relative_x = x_event - geometry.x;
  relative_y = y_event - geometry.y;

  *x_offset = -2;
  *y_offset = -4;

  /* Add 3/4 of a digit width as right padding */
  text_width = (digits + 0.75) * digit_width;
  /* Add 10% of line height as bottom padding */
  line_height = line_height * 1.1;

  if (relative_x > geometry.width - text_width)
    *x_offset -= text_width;

  if (relative_y > geometry.height - line_height)
    *y_offset -= line_height;
}



static void create_size_window (RubberBandData *rbdata)
{
  GtkWidget *window, *label;
  GtkCssProvider *css_provider;
  GtkStyleContext *context;
  GdkVisual *visual;

  rbdata->size_window = window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (window), 100, 50);
  gtk_widget_set_size_request (GTK_WIDGET (window), 100, 50);
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), FALSE);
  g_signal_connect (G_OBJECT (window), "draw",
                    G_CALLBACK (cb_size_window_draw), NULL);

  visual = gdk_screen_get_rgba_visual (gtk_widget_get_screen (window));
  gtk_widget_set_visual (window, visual);

  rbdata->size_label = label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_valign (label, GTK_ALIGN_START);
  gtk_widget_set_margin_start (label, 6);
  gtk_widget_set_margin_top (label, 6);
  gtk_container_add (GTK_CONTAINER (window), label);

  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css_provider,
    "label { font-family: monospace; color: #fff; text-shadow: 1px 1px 0px black; }", -1, NULL);

  context = gtk_widget_get_style_context (GTK_WIDGET (label));
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css_provider);

  gtk_widget_show_all (GTK_WIDGET (window));
}



static gboolean cb_motion_notify (GtkWidget *widget,
                                  GdkEventMotion *event,
                                  RubberBandData *rbdata)
{
  gchar *coords;
  gint rect_width, rect_height, x_offset, y_offset;
  static gint digit_width = -1, line_height = -1;

  if (rbdata->left_pressed)
    {
      cairo_rectangle_int_t *new_rect;
      cairo_rectangle_int_t old_rect, intersect;
      cairo_region_t *region;

      TRACE ("Mouse is moving with left button pressed");

      new_rect = &rbdata->rectangle;

      if (!rbdata->rubber_banding)
        {
          /* This is the start of a rubber banding */
          rbdata->rubber_banding = TRUE;
          old_rect.x = rbdata->x;
          old_rect.y = rbdata->y;
          old_rect.height = old_rect.width = 1;
        }
      else
        {
          /* Rubber banding has already started, update it */
          old_rect.x = new_rect->x;
          old_rect.y = new_rect->y;
          old_rect.width = new_rect->width;
          old_rect.height = new_rect->height;
        }

      if (rbdata->move_rectangle)
        {
          /* Define the anchor right after the control key is pressed */
          if (rbdata->anchor == ANCHOR_UNSET)
            {
              rbdata->anchor = ANCHOR_NONE;
              rbdata->anchor |= (event->x < rbdata->x) ? ANCHOR_LEFT : 0;
              rbdata->anchor |= (event->y < rbdata->y) ? ANCHOR_TOP : 0;
            }

          /* Do not resize, instead move the rubber banding rectangle around */
          if (rbdata->anchor & ANCHOR_LEFT)
            rbdata->x = (new_rect->x = event->x) + new_rect->width;
          else
            rbdata->x = new_rect->x = event->x - new_rect->width;

          if (rbdata->anchor & ANCHOR_TOP)
            rbdata->y = (new_rect->y = event->y) + new_rect->height;
          else
            rbdata->y = new_rect->y = event->y - new_rect->height;
        }
      else
        {
          /* Get the new rubber banding rectangle */
          new_rect->x = MIN (rbdata->x, event->x);
          new_rect->y = MIN (rbdata->y, event->y);
          new_rect->width = ABS (rbdata->x - event->x) + 1;
          new_rect->height = ABS (rbdata->y - event->y) + 1;
        }

      rect_width = new_rect->width;
      rect_height = new_rect->height;

      /* Take into account when the rectangle is moved out of screen */
      if (new_rect->x < 0) rect_width += new_rect->x;
      if (new_rect->y < 0) rect_height += new_rect->y;

      coords = g_strdup_printf ("%d x %d", rect_width, rect_height);

      if (digit_width == -1)
        {
          PangoLayout *layout;
          PangoContext *context;
          PangoFontMetrics *metrics;

          layout = gtk_label_get_layout (GTK_LABEL (rbdata->size_label));
          context = pango_layout_get_context (layout);
          metrics = pango_context_get_metrics (context,
                                               pango_context_get_font_description (context),
                                               pango_context_get_language (context));

          digit_width = PANGO_PIXELS_CEIL (pango_font_metrics_get_approximate_digit_width (metrics));
          line_height = PANGO_PIXELS_CEIL (pango_font_metrics_get_height (metrics));

          pango_font_metrics_unref (metrics);
        }

      size_window_get_offset (rbdata->size_window, strlen (coords),
                              digit_width, line_height,
                              event->x, event->y,
                              &x_offset, &y_offset);

      gtk_window_move (GTK_WINDOW (rbdata->size_window),
                       event->x + x_offset,
                       event->y + y_offset);

      gtk_label_set_text (GTK_LABEL (rbdata->size_label), coords);
      g_free (coords);

      region = cairo_region_create_rectangle (&old_rect);
      cairo_region_union_rectangle (region, new_rect);

      /* Try to be smart: don't send the expose event for regions which
       * have already been painted */
      if (gdk_rectangle_intersect (&old_rect, new_rect, &intersect)
          && intersect.width > 2 && intersect.height > 2)
        {
          cairo_region_t *region_intersect;

          intersect.x += 1;
          intersect.width -= 2;
          intersect.y += 1;
          intersect.height -= 2;

          region_intersect = cairo_region_create_rectangle(&intersect);
          cairo_region_subtract(region, region_intersect);
          cairo_region_destroy(region_intersect);
        }

      gdk_window_invalidate_region (gtk_widget_get_window (widget), region, TRUE);
      cairo_region_destroy (region);

      return TRUE;
    }

  return FALSE;
}



static GdkPixbuf
*capture_rectangle_screenshot (gint x, gint y, gint w, gint h, gint delay, gboolean show_mouse)
{
  GdkWindow *root;
  int root_width, root_height;
  GdkPixbuf *screenshot;

  root = gdk_get_default_root_window ();
  root_width = gdk_window_get_width (root);
  root_height = gdk_window_get_height (root);

  /* Avoid rectangle parts outside the screen */
  if (x < 0)
    w += x;
  if (y < 0)
    h += y;

  x = MAX(0, x);
  y = MAX(0, y);

  if (x + w > root_width)
    w = root_width - x;
  if (y + h > root_height)
    h = root_height - y;

  /* Await the specified delay, but not less than 200ms */
  if (delay == 0)
    g_usleep (200000);
  else
    sleep (delay);

  screenshot = screenshooter_pixbuf_get_from_window (root, x, y, w, h);

  if (show_mouse)
    capture_cursor (screenshot, NULL, gdk_window_get_scale_factor (root), x, y, w, h);

  return screenshot;
}



static GdkGrabStatus
try_grab (GdkSeat *seat, GdkWindow *window, GdkCursor *cursor)
{
  GdkGrabStatus status;
  gint attempts = 0;

  while (TRUE) {
    status = gdk_seat_grab (seat, window, GDK_SEAT_CAPABILITY_ALL, FALSE,
                            cursor, NULL, NULL, NULL);

    if (++attempts > 5 || status == GDK_GRAB_SUCCESS)
      break;

    /* Wait 100ms before trying again, useful when invoked by global hotkey
     * because xfsettings will grab the key for a moment */
    g_usleep(100000);
  }

  return status;
}



static GdkPixbuf
*get_rectangle_screenshot_composited (gint delay, gboolean show_mouse)
{
  GtkWidget *window;
  RubberBandData rbdata;
  GdkPixbuf *screenshot = NULL;
  GdkGrabStatus res;
  GdkSeat   *seat;
  GdkCursor *xhair_cursor;
  GdkDisplay *display;
  GdkRectangle screen_geometry;

  /* Initialize the rubber band data */
  rbdata.left_pressed = FALSE;
  rbdata.rubber_banding = FALSE;
  rbdata.x = rbdata.y = 0;
  rbdata.cancelled = FALSE;
  rbdata.move_rectangle = FALSE;
  rbdata.anchor = ANCHOR_UNSET;

  /* Create the fullscreen window on which the rubber banding
   * will be drawn. */
  window = gtk_dialog_new ();
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  gtk_window_set_deletable (GTK_WINDOW (window), FALSE);
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_widget_set_app_paintable (window, TRUE);
  gtk_widget_add_events (window,
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_BUTTON_PRESS_MASK |
                         GDK_EXPOSURE_MASK |
                         GDK_POINTER_MOTION_MASK |
                         GDK_KEY_PRESS_MASK);
  gtk_widget_set_visual (window, gdk_screen_get_rgba_visual (gdk_screen_get_default ()));

  /* Connect to the interesting signals */
  g_signal_connect (window, "key-press-event",
                    G_CALLBACK (cb_key_pressed), &rbdata);
  g_signal_connect (window, "key-release-event",
                    G_CALLBACK (cb_key_released), &rbdata);
  g_signal_connect (window, "draw",
                    G_CALLBACK (cb_draw), &rbdata);
  g_signal_connect (window, "button-press-event",
                    G_CALLBACK (cb_button_pressed), &rbdata);
  g_signal_connect (window, "button-release-event",
                    G_CALLBACK (cb_button_released), &rbdata);
  g_signal_connect (window, "motion-notify-event",
                    G_CALLBACK (cb_motion_notify), &rbdata);

  /* This window is not managed by the window manager, we have to set everything
   * ourselves */
  display = gdk_display_get_default ();
  gtk_widget_realize (window);
  xhair_cursor = gdk_cursor_new_for_display (display, GDK_CROSSHAIR);

  screenshooter_get_screen_geometry (&screen_geometry);

  gdk_window_set_override_redirect (gtk_widget_get_window (window), TRUE);
  gtk_widget_set_size_request (window,
                               screen_geometry.width, screen_geometry.height);
  gdk_window_raise (gtk_widget_get_window (window));
  gtk_widget_show_now (window);
  gtk_widget_grab_focus (window);
  gdk_display_flush (display);

  /* Grab the mouse and the keyboard to prevent any interaction with other
   * applications */
  seat = gdk_display_get_default_seat (display);

  res = try_grab (seat, gtk_widget_get_window (window), xhair_cursor);

  if (res != GDK_GRAB_SUCCESS)
    {
      gtk_widget_destroy (window);
      g_object_unref (xhair_cursor);
      g_warning ("Failed to grab seat");
      return NULL;
    }

  /* set up the window showing the screenshot size */
  create_size_window (&rbdata);

  gtk_dialog_run (GTK_DIALOG (window));
  gtk_widget_destroy (window);
  g_object_unref (xhair_cursor);
  gdk_display_flush (display);

  if (rbdata.cancelled)
    goto cleanup;

  /* Grab the screenshot on the main window */
  screenshot = capture_rectangle_screenshot (rbdata.rectangle.x,
                                             rbdata.rectangle.y,
                                             rbdata.rectangle.width,
                                             rbdata.rectangle.height,
                                             delay, show_mouse);

  cleanup:
  if (rbdata.size_window)
    gtk_widget_destroy (rbdata.size_window);
  gdk_seat_ungrab (seat);
  gdk_display_flush (display);

  return screenshot;
}



static GdkFilterReturn
region_filter_func (GdkXEvent *xevent, GdkEvent *event, RbData *rbdata)
{
  XEvent *x_event = (XEvent *) xevent;
  gint x2 = 0, y2 = 0;
  XIDeviceEvent *device_event;
  Display *display;
  Window root_window;
  int key;
  int event_type;
  gboolean is_xinput = FALSE;

  display = gdk_x11_get_default_xdisplay ();
  root_window = gdk_x11_get_default_root_xwindow ();

  event_type = x_event->type;

  if (event_type == GenericEvent)
    {
      event_type = x_event->xgeneric.evtype;
      is_xinput = TRUE;
    }

  switch (event_type)
    {
      /* Start dragging the rectangle out */
      case XI_ButtonPress: /* ButtonPress */
        TRACE ("Start dragging the rectangle");

        if (is_xinput)
          {
            device_event = (XIDeviceEvent*) x_event->xcookie.data;
            rbdata->rectangle.x = rbdata->x1 = device_event->root_x;
            rbdata->rectangle.y = rbdata->y1 = device_event->root_y;
          }
        else
          {
            rbdata->rectangle.x = rbdata->x1 = x_event->xkey.x_root;
            rbdata->rectangle.y = rbdata->y1 = x_event->xkey.y_root;
          }

        rbdata->rectangle.width = 0;
        rbdata->rectangle.height = 0;
        rbdata->pressed = TRUE;
        rbdata->move_rectangle = FALSE;
        rbdata->anchor = ANCHOR_UNSET;

        return GDK_FILTER_REMOVE;
      break;

      /* Finish dragging the rectangle out */
      case XI_ButtonRelease: /* ButtonRelease */
        if (rbdata->pressed)
          {
            if (rbdata->rectangle.width > 0 && rbdata->rectangle.height > 0)
              {
                /* Remove the rectangle drawn previously */
                TRACE ("Remove the rectangle drawn previously");

                XDrawRectangle (display,
                                root_window,
                                *rbdata->context,
                                rbdata->rectangle.x,
                                rbdata->rectangle.y,
                                (unsigned int) rbdata->rectangle.width-1,
                                (unsigned int) rbdata->rectangle.height-1);

                gtk_main_quit ();
              }
            else
              {
                /* The user has not dragged the mouse, start again */
                TRACE ("Mouse was not dragged, start again");

                rbdata->pressed = FALSE;
              }
          }
        return GDK_FILTER_REMOVE;
      break;

      /* The user is moving the mouse */
      case XI_Motion: /* MotionNotify */
        if (rbdata->pressed)
          {
            TRACE ("Mouse is moving");

            if (rbdata->rectangle.width > 0 && rbdata->rectangle.height > 0)
              {
                /* Remove the rectangle drawn previously */
                TRACE ("Remove the rectangle drawn previously");

                XDrawRectangle (display,
                                root_window,
                                *rbdata->context,
                                rbdata->rectangle.x,
                                rbdata->rectangle.y,
                                (unsigned int) rbdata->rectangle.width-1,
                                (unsigned int) rbdata->rectangle.height-1);
              }

            if (is_xinput)
              {
                device_event = (XIDeviceEvent*) x_event->xcookie.data;
                x2 = device_event->root_x;
                y2 = device_event->root_y;
              }
            else
              {
                x2 = x_event->xkey.x_root;
                y2 = x_event->xkey.y_root;
              }


            if (rbdata->move_rectangle)
              {
                /* Define the anchor right after the control key is pressed */
                if (rbdata->anchor == ANCHOR_UNSET)
                  {
                    rbdata->anchor = ANCHOR_NONE;
                    rbdata->anchor |= (x2 < rbdata->x1) ? ANCHOR_LEFT : 0;
                    rbdata->anchor |= (y2 < rbdata->y1) ? ANCHOR_TOP : 0;
                  }

                /* Do not resize, instead move the rubber banding rectangle around */
                if (rbdata->anchor & ANCHOR_LEFT)
                  rbdata->x1 = (rbdata->rectangle.x = x2) + rbdata->rectangle.width;
                else
                  rbdata->x1 = rbdata->rectangle.x = x2 - rbdata->rectangle.width;

                if (rbdata->anchor & ANCHOR_TOP)
                  rbdata->y1 = (rbdata->rectangle.y = y2) + rbdata->rectangle.height;
                else
                  rbdata->y1 = rbdata->rectangle.y = y2 - rbdata->rectangle.height;
              }
            else
              {
                rbdata->rectangle.x = MIN (rbdata->x1, x2);
                rbdata->rectangle.y = MIN (rbdata->y1, y2);
                rbdata->rectangle.width = ABS (x2 - rbdata->x1);
                rbdata->rectangle.height = ABS (y2 - rbdata->y1);
              }

            /* Draw  the rectangle as the user drags the mouse */
            TRACE ("Draw the new rectangle");
            if (rbdata->rectangle.width > 0 && rbdata->rectangle.height > 0)
              {
                XDrawRectangle (display,
                                root_window,
                                *rbdata->context,
                                rbdata->rectangle.x,
                                rbdata->rectangle.y,
                                (unsigned int) rbdata->rectangle.width-1,
                                (unsigned int) rbdata->rectangle.height-1);
              }
          }
        return GDK_FILTER_REMOVE;
        break;

      case XI_KeyPress: /* KeyPress */
        if (is_xinput)
          {
            device_event = (XIDeviceEvent*) x_event->xcookie.data;
            key = device_event->detail;
          }
        else
          {
            key = x_event->xkey.keycode;
          }

        if (rbdata->pressed && (
            key == XKeysymToKeycode (gdk_x11_get_default_xdisplay (), XK_Control_L) ||
            key == XKeysymToKeycode (gdk_x11_get_default_xdisplay (), XK_Control_R)))
          {
            TRACE ("Control key was pressed, move the selection area.");
            rbdata->move_rectangle = TRUE;
            return GDK_FILTER_REMOVE;
          }

        if (key == XKeysymToKeycode (gdk_x11_get_default_xdisplay (), XK_Escape))
          {
            TRACE ("Escape key was pressed, cancel the screenshot.");

            if (rbdata->pressed)
              {
                if (rbdata->rectangle.width > 0 && rbdata->rectangle.height > 0)
                  {
                    /* Remove the rectangle drawn previously */
                    TRACE ("Remove the rectangle drawn previously");

                    XDrawRectangle (display,
                                   root_window,
                                   *rbdata->context,
                                   rbdata->rectangle.x,
                                   rbdata->rectangle.y,
                                   (unsigned int) rbdata->rectangle.width-1,
                                   (unsigned int) rbdata->rectangle.height-1);
                  }
              }

            rbdata->cancelled = TRUE;
            gtk_main_quit ();
            return GDK_FILTER_REMOVE;
          }
        break;

      case XI_KeyRelease: /* KeyRelease */
        if (is_xinput)
          {
            device_event = (XIDeviceEvent*) x_event->xcookie.data;
            key = device_event->detail;
          }
        else
          {
            key = x_event->xkey.keycode;
          }

        if (rbdata->pressed && (
            key == XKeysymToKeycode (gdk_x11_get_default_xdisplay (), XK_Control_L) ||
            key == XKeysymToKeycode (gdk_x11_get_default_xdisplay (), XK_Control_R)))
          {
            TRACE ("Control key was released, resize the selection area.");
            rbdata->move_rectangle = FALSE;
            rbdata->anchor = ANCHOR_UNSET;
            return GDK_FILTER_REMOVE;
          }
        break;

      default:
        break;
    }

  return GDK_FILTER_CONTINUE;
}



static GdkPixbuf
*get_rectangle_screenshot (gint delay, gboolean show_mouse)
{
  GdkPixbuf *screenshot = NULL;
  GdkWindow *root_window;

  XGCValues gc_values;
  GC gc;

  Display *display;
  gint screen;
  gint scale;
  RbData rbdata;
  GdkCursor *xhair_cursor;
  GdkGrabStatus res;
  GdkSeat   *seat;
  long value_mask;

  /* Get root window */
  TRACE ("Get the root window");
  root_window = gdk_get_default_root_window ();
  display = gdk_x11_get_default_xdisplay ();
  screen = gdk_x11_get_default_screen ();
  scale = gdk_window_get_scale_factor (root_window);

  /* Change cursor to cross-hair */
  TRACE ("Set the cursor");
  xhair_cursor = gdk_cursor_new_for_display (gdk_display_get_default (),
                                             GDK_CROSSHAIR);

  gdk_window_show_unraised (root_window);

  /* Grab the mouse and the keyboard to prevent any interaction with other
   * applications */
  seat = gdk_display_get_default_seat (gdk_display_get_default ());

  res = try_grab (seat, root_window, xhair_cursor);

  if (res != GDK_GRAB_SUCCESS)
    {
      g_object_unref (xhair_cursor);
      g_warning ("Failed to grab seat");
      return NULL;
    }

  /*Set up graphics context for a XOR rectangle that will be drawn as
   * the user drags the mouse */
  TRACE ("Initialize the graphics context");

  gc_values.function = GXxor;
  gc_values.line_width = 2;
  gc_values.line_style = LineOnOffDash;
  gc_values.fill_style = FillSolid;
  gc_values.graphics_exposures = FALSE;
  gc_values.subwindow_mode = IncludeInferiors;
  gc_values.background = XBlackPixel (display, screen);
  gc_values.foreground = XWhitePixel (display, screen);

  value_mask = GCFunction | GCLineWidth | GCLineStyle |
               GCFillStyle | GCGraphicsExposures | GCSubwindowMode |
               GCBackground | GCForeground;

  gc = XCreateGC (display,
                  gdk_x11_get_default_root_xwindow (),
                  value_mask,
                  &gc_values);

  /* Initialize the rubber band data */
  TRACE ("Initialize the rubber band data");
  rbdata.context = &gc;
  rbdata.pressed = FALSE;
  rbdata.cancelled = FALSE;

  /* Set the filter function to handle the GDK events */
  TRACE ("Add the events filter");
  gdk_window_add_filter (root_window,
                         (GdkFilterFunc) region_filter_func, &rbdata);

  gdk_display_flush (gdk_display_get_default ());

  gtk_main ();

  gdk_window_remove_filter (root_window,
                            (GdkFilterFunc) region_filter_func,
                            &rbdata);

  gdk_seat_ungrab (seat);

  /* Get the screenshot's pixbuf */
  if (G_LIKELY (!rbdata.cancelled))
    {
      TRACE ("Get the pixbuf for the screenshot");
      screenshot = capture_rectangle_screenshot (rbdata.rectangle.x / scale,
                                                 rbdata.rectangle.y / scale,
                                                 rbdata.rectangle.width / scale,
                                                 rbdata.rectangle.height / scale,
                                                 delay, show_mouse);
    }

  if (G_LIKELY (gc != NULL))
    XFreeGC (display, gc);

  g_object_unref (xhair_cursor);

  return screenshot;
}



/* Public */



/**
 * screenshooter_capture_screenshot_x11:
 * @region: the region to be captured. It can be FULLSCREEN,
 *          ACTIVE_WINDOW or SELECT.
 * @delay: the delay before the screenshot is captured, in seconds.
 * @show_mouse: whether the mouse pointer should be displayed on the
 *              screenshot.
 * @show_border: whether the window border should be displayed on the
 *               screenshot.
 *
 * Captures a screenshot with the given options. If @region is FULLSCREEN,
 * the screenshot is captured after @delay seconds. If @region is
 * ACTIVE_WINDOW, a delay of @delay seconds elapses, then the active
 * window is detected and captured. If @region is SELECT, the user will
 * have to select a portion of the screen with the mouse. Then a delay of
 * @delay seconds elapses, and a screenshot is captured.
 *
 * @show_mouse is only taken into account when @region is FULLSCREEN
 * or ACTIVE_WINDOW.
 *
 * @show_border is only taken into account when @region is ACTIVE_WINDOW
 * and if the window uses server side decorations.
 *
 * Return value: a #GdkPixbuf containing the screenshot or %NULL
 * (if @region is SELECT, the user can cancel the operation).
 **/
GdkPixbuf *screenshooter_capture_screenshot_x11 (gint     region,
                                                 gint     delay,
                                                 gboolean show_mouse,
                                                 gboolean show_border)
{
  GdkPixbuf *screenshot = NULL;
  GdkWindow *window = NULL;
  GdkScreen *screen;
  GdkDisplay *display;

  screen = gdk_screen_get_default ();
  display = gdk_display_get_default ();

  /* Sync the display */
  gdk_display_sync (display);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_window_process_all_updates ();
G_GNUC_END_IGNORE_DEPRECATIONS

  if (region == FULLSCREEN)
    {
      TRACE ("Get the screenshot of the entire screen");
      window = gdk_get_default_root_window ();
      screenshot = get_window_screenshot (window, show_mouse, FALSE);
    }
  else if (region == ACTIVE_WINDOW)
    {
      gboolean border;
      gboolean needs_unref = TRUE;

      TRACE ("Get the screenshot of the active window");
      window = screenshooter_get_active_window (screen, &needs_unref, &border);
      screenshot = get_window_screenshot (window, show_mouse, show_border && border);
      if (needs_unref)
        g_object_unref (window);
    }
  else if (region == SELECT)
    {
      TRACE ("Let the user select the region to screenshot");
      screenshot = gdk_screen_is_composited (screen) ?
                    get_rectangle_screenshot_composited (delay, show_mouse) :
                    get_rectangle_screenshot (delay, show_mouse);
    }

  return screenshot;
}
