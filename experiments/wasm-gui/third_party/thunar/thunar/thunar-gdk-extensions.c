/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2003-2007 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-gdk-extensions.h>
#include <thunar/thunar-private.h>



static const cairo_user_data_key_t cairo_key;



static cairo_surface_t *
thunar_gdk_cairo_create_surface (const GdkPixbuf *pixbuf,
                                 gint             scale_factor)
{
  gint             width;
  gint             height;
  guchar          *gdk_pixels;
  gint             gdk_rowstride;
  gint             n_channels;
  gint             cairo_stride;
  guchar          *cairo_pixels;
  cairo_format_t   format;
  cairo_surface_t *surface;
  gint             j;
  guchar          *p, *q;
  guchar          *end;

  _thunar_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  /* get pixbuf information */
  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  gdk_pixels = gdk_pixbuf_get_pixels (pixbuf);
  gdk_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  n_channels = gdk_pixbuf_get_n_channels (pixbuf);

  if (n_channels == 3)
    format = CAIRO_FORMAT_RGB24;
  else
    format = CAIRO_FORMAT_ARGB32;

  /* prepare pixel data and surface */
  cairo_stride = cairo_format_stride_for_width (format, width);
  cairo_pixels = g_malloc_n (height, cairo_stride);
  surface = cairo_image_surface_create_for_data (cairo_pixels, format,
                                                 width, height, cairo_stride);
  cairo_surface_set_user_data (surface, &cairo_key, cairo_pixels, g_free);

  /* convert format */
  if (G_UNLIKELY (n_channels == 3))
    {
      for (j = height; j; j--)
        {
          p = gdk_pixels;
          q = cairo_pixels;
          end = p + 3 * width;

          while (p < end)
            {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
              q[0] = p[2];
              q[1] = p[1];
              q[2] = p[0];
#else
              q[1] = p[0];
              q[2] = p[1];
              q[3] = p[2];
#endif
              p += 3;
              q += 4;
            }

          gdk_pixels += gdk_rowstride;
          cairo_pixels += cairo_stride;
        }
    }
  else
    {
#define MULT(d,c,a) G_STMT_START { guint t = c * a + 0x7f; d = ((t >> 8) + t) >> 8; } G_STMT_END
      for (j = height; j; j--)
        {
          p = gdk_pixels;
          q = cairo_pixels;
          end = p + 4 * width;

          while (p < end)
            {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
              MULT(q[0], p[2], p[3]);
              MULT(q[1], p[1], p[3]);
              MULT(q[2], p[0], p[3]);
              q[3] = p[3];
#else
              q[0] = p[3];
              MULT(q[1], p[0], p[3]);
              MULT(q[2], p[1], p[3]);
              MULT(q[3], p[2], p[3]);
#endif

              p += 4;
              q += 4;
            }

          gdk_pixels += gdk_rowstride;
          cairo_pixels += cairo_stride;
        }
#undef MULT
    }

  cairo_surface_set_device_scale (surface, scale_factor, scale_factor);

  return surface;
}



/**
 * thunar_gdk_screen_open:
 * @display_name : a fully qualified display name.
 * @error        : return location for errors or %NULL.
 *
 * Opens the screen referenced by the @display_name. Returns a
 * reference on the #GdkScreen for @display_name or %NULL if
 * @display_name couldn't be opened.
 *
 * If @display_name is the empty string "", a reference on the
 * default screen will be returned.
 *
 * The caller is responsible to free the returned object
 * using g_object_unref() when no longer needed.
 *
 * Return value: the #GdkScreen for @display_name or %NULL if
 *               @display_name could not be opened.
 **/
GdkScreen*
thunar_gdk_screen_open (const gchar *display_name,
                        GError     **error)
{
  const gchar *other_name;
  GdkDisplay  *display = NULL;
  GdkScreen   *screen = NULL;
  GSList      *displays;
  GSList      *dp;

  _thunar_return_val_if_fail (display_name != NULL, NULL);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* check if the default screen should be opened */
  if (G_UNLIKELY (*display_name == '\0'))
    return g_object_ref (gdk_screen_get_default ());

  /* check if we already have that display */
  displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
  for (dp = displays; dp != NULL; dp = dp->next)
    {
      other_name = gdk_display_get_name (dp->data);
      if (strncmp (other_name, display_name, strlen (display_name)) == 0)
        {
          display = dp->data;
          break;
        }
      /* This second comparison will as well match the short notation, ":0" with the long notation ":0.0" */
      if (strncmp (other_name, display_name, strlen (other_name)) == 0)
        {
          display = dp->data;
          break;
        }
    }
  g_slist_free (displays);

  /* try to open the display if not already done */
  if (display == NULL)
    display = gdk_display_open (display_name);

  /* try to locate the screen on the display */
  if (display != NULL)
    {
      screen = gdk_display_get_default_screen (display);

      if (screen != NULL)
        g_object_ref (G_OBJECT (screen));
    }

  /* check if we were successfull here */
  if (G_UNLIKELY (screen == NULL))
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to open display \"%s\"", display_name);

  return screen;
}



/**
 * thunar_gdk_cairo_set_source_pixbuf:
 * cr           : a Cairo context
 * pixbuf       : a GdkPixbuf
 * pixbuf_x     : X coordinate of location to place upper left corner of pixbuf
 * pixbuf_y     : Y coordinate of location to place upper left corner of pixbuf
 * scale_factor : UI scaling factor
 *
 * Works like gdk_cairo_set_source_pixbuf but we try to cache the surface
 * on the pixbuf, which is efficient within Thunar because we also share
 * the pixbufs using the icon cache.
 **/
void
thunar_gdk_cairo_set_source_pixbuf (cairo_t   *cr,
                                    GdkPixbuf *pixbuf,
                                    gdouble    pixbuf_x,
                                    gdouble    pixbuf_y,
                                    gint       scale_factor)
{
  cairo_surface_t *surface;
  static GQuark    surface_quark = 0;

  if (G_UNLIKELY (surface_quark == 0))
    surface_quark = g_quark_from_static_string ("thunar-gdk-surface");

  /* peek if there is already a surface */
  surface = g_object_get_qdata (G_OBJECT (pixbuf), surface_quark);
  if (surface == NULL)
    {
      /* create a new surface */
      surface = thunar_gdk_cairo_create_surface (pixbuf, scale_factor);

      /* store the pixbuf on the pixbuf */
      g_object_set_qdata_full (G_OBJECT (pixbuf), surface_quark,
                               surface, (GDestroyNotify) cairo_surface_destroy);
    }

  /* apply */
  cairo_set_source_surface (cr, surface, pixbuf_x, pixbuf_y);
}
