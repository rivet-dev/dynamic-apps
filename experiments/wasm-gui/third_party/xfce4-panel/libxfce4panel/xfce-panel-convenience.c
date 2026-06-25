/*
 * Copyright (C) 2006-2007 Jasper Huijsmans <jasper@xfce.org>
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <gtk/gtk.h>

#include <libxfce4panel/xfce-panel-macros.h>
#include <libxfce4panel/xfce-panel-convenience.h>
#include <libxfce4panel/libxfce4panel-alias.h>



/**
 * SECTION: convenience
 * @title: Convenience Functions
 * @short_description: Special purpose widgets and utilities
 * @include: libxfce4panel/libxfce4panel.h
 *
 * This section describes a number of functions that were created
 * to help developers of Xfce Panel plugins.
 **/

/**
 * xfce_panel_create_button:
 *
 * Create regular #GtkButton with a few properties set to be useful in the
 * Xfce panel: Flat (%GTK_RELIEF_NONE), no focus on click and minimal padding.
 *
 * Returns: (transfer floating): newly created #GtkButton.
 **/
GtkWidget *
xfce_panel_create_button (void)
{
  GtkWidget       *button = gtk_button_new ();
  GtkStyleContext *context;
  GtkCssProvider  *provider;

  gtk_widget_set_can_default (GTK_WIDGET (button), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (button), FALSE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_widget_set_focus_on_click (GTK_WIDGET (button), FALSE);
  gtk_widget_set_name (button, "xfce-panel-button");

  /* Make sure themes like Adwaita, which set excessive padding, don't cause the
     launcher buttons to overlap when panels have a fairly normal size */
  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, ".xfce4-panel button { padding: 1px; }", -1, NULL);
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  return button;
}



/**
 * xfce_panel_create_toggle_button:
 *
 * Create regular #GtkToggleButton with a few properties set to be useful in
 * Xfce panel: Flat (%GTK_RELIEF_NONE), no focus on click and minimal padding.
 *
 * Returns: (transfer floating): newly created #GtkToggleButton.
 **/
GtkWidget *
xfce_panel_create_toggle_button (void)
{
  GtkStyleContext *context;
  GtkCssProvider  *provider;

  GtkWidget *button = gtk_toggle_button_new ();

  gtk_widget_set_can_default (GTK_WIDGET (button), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (button), FALSE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_widget_set_focus_on_click (GTK_WIDGET (button), FALSE);
  gtk_widget_set_name (button, "xfce-panel-toggle-button");

  /* Make sure themes like Adwaita, which set excessive padding, don't cause the
     launcher buttons to overlap when panels have a fairly normal size */
  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, ".xfce4-panel button { padding: 1px; }", -1, NULL);
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  return button;
}



/**
 * xfce_panel_get_channel_name:
 *
 * Function for the name of the Xfconf channel used by the panel. By default
 * this returns "xfce4-panel", but you can override this value with the
 * environment variable XFCE_PANEL_CHANNEL_NAME.
 *
 * Returns: name of the Xfconf channel
 *
 * See also: XFCE_PANEL_CHANNEL_NAME,
 *           xfce_panel_plugin_xfconf_channel_new and
 *           xfce_panel_plugin_get_property_base
 *
 * Since: 4.8
 **/
const gchar *
xfce_panel_get_channel_name (void)
{
  static const gchar *name = NULL;

  if (G_UNLIKELY (name == NULL))
    {
      name = g_getenv ("XFCE_PANEL_CHANNEL_NAME");
      if (G_LIKELY (name == NULL))
        name = "xfce4-panel";
    }

  return name;
}



/**
 * xfce_panel_pixbuf_from_source_at_size:
 * @source: string that contains the location of an icon
 * @icon_theme: (allow-none): icon theme or %NULL to use the default icon theme
 * @dest_width: the maximum returned width of the GdkPixbuf
 * @dest_height: the maximum returned height of the GdkPixbuf
 *
 * Try to load a pixbuf from a source string. The source could be
 * an abolute path, an icon name or a filename that points to a
 * file in the pixmaps directory.
 *
 * This function is particularly usefull for loading names from
 * the Icon key of desktop files.
 *
 * The pixbuf is never bigger than @dest_width and @dest_height.
 * If it is when loaded from the disk, the pixbuf is scaled
 * preserving the aspect ratio.
 *
 * Returns: (transfer full): a GdkPixbuf or %NULL if nothing was found. The value should
 *          be released with g_object_unref when no longer used.
 *
 * See also: XfcePanelImage
 *
 * Since: 4.10
 **/
GdkPixbuf *
xfce_panel_pixbuf_from_source_at_size (const gchar  *source,
                                       GtkIconTheme *icon_theme,
                                       gint          dest_width,
                                       gint          dest_height)
{
  GdkPixbuf *pixbuf = NULL;
  gchar     *p;
  gchar     *name;
  gchar     *filename;
  gint       src_w, src_h;
  gdouble    ratio;
  GdkPixbuf *dest;
  GError    *error = NULL;
  gint       size = MIN (dest_width, dest_height);

  g_return_val_if_fail (source != NULL, NULL);
  g_return_val_if_fail (icon_theme == NULL || GTK_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (dest_width > 0, NULL);
  g_return_val_if_fail (dest_height > 0, NULL);

  if (G_UNLIKELY (g_path_is_absolute (source)))
    {
      pixbuf = gdk_pixbuf_new_from_file (source, &error);
      if (G_UNLIKELY (pixbuf == NULL))
        {
          g_message ("Failed to load image \"%s\": %s",
                     source, error->message);
          g_error_free (error);
        }
    }
  else
    {
      if (G_UNLIKELY (icon_theme == NULL))
        icon_theme = gtk_icon_theme_get_default ();

      /* try to load from the icon theme */
      pixbuf = gtk_icon_theme_load_icon (icon_theme, source, size, 0, NULL);
      if (G_UNLIKELY (pixbuf == NULL))
        {
          /* try to lookup names like application.png in the theme */
          p = strrchr (source, '.');
          if (p != NULL)
            {
              name = g_strndup (source, p - source);
              pixbuf = gtk_icon_theme_load_icon (icon_theme, name, size, 0, NULL);
              g_free (name);
            }

          /* maybe they point to a file in the pixbufs folder */
          if (G_UNLIKELY (pixbuf == NULL))
            {
              filename = g_build_filename ("pixmaps", source, NULL);
              name = xfce_resource_lookup (XFCE_RESOURCE_DATA, filename);
              g_free (filename);

              if (name != NULL)
                {
                  pixbuf = gdk_pixbuf_new_from_file (name, NULL);
                  g_free (name);
                }
            }
        }
    }

  if (G_UNLIKELY (pixbuf == NULL))
    {
      if (G_UNLIKELY (icon_theme == NULL))
        icon_theme = gtk_icon_theme_get_default ();

      /* bit ugly as a fallback, but in most cases better then no icon */
      pixbuf = gtk_icon_theme_load_icon (icon_theme, "image-missing",
                                         size, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
    }

  /* scale the pixbug if required */
  if (G_LIKELY (pixbuf != NULL))
    {
      src_w = gdk_pixbuf_get_width (pixbuf);
      src_h = gdk_pixbuf_get_height (pixbuf);

      if (src_w > dest_width || src_h > dest_height)
        {
          /* calculate the new dimensions */
          ratio = MIN ((gdouble) dest_width / (gdouble) src_w,
                       (gdouble) dest_height / (gdouble) src_h);

          dest_width  = rint (src_w * ratio);
          dest_height = rint (src_h * ratio);

          dest = gdk_pixbuf_scale_simple (pixbuf,
                                          MAX (dest_width, 1),
                                          MAX (dest_height, 1),
                                          GDK_INTERP_BILINEAR);

          g_object_unref (G_OBJECT (pixbuf));
          pixbuf = dest;
        }
    }

  return pixbuf;
}



/**
 * xfce_panel_pixbuf_from_source:
 * @source: string that contains the location of an icon
 * @icon_theme: (allow-none): icon theme or %NULL to use the default icon theme
 * @size: size the icon that should be loaded
 *
 * See xfce_panel_pixbuf_from_source_at_size
 *
 * Returns: (transfer full): a GdkPixbuf or %NULL if nothing was found. The value should
 *          be released with g_object_unref when no longer used.
 *
 * See also: XfcePanelImage
 *
 * Since: 4.8
 **/
GdkPixbuf *
xfce_panel_pixbuf_from_source (const gchar  *source,
                               GtkIconTheme *icon_theme,
                               gint          size)
{
  return xfce_panel_pixbuf_from_source_at_size (source, icon_theme, size, size);
}



/**
 * xfce_panel_set_image_from_source:
 * @image: #GtkImage to be set
 * @source: string that contains the location of an icon
 * @icon_theme: (allow-none): icon theme or %NULL to use the default icon theme
 * @size: size the icon that should be loaded
 * @scale: desired scale (see gtk_widget_get_scale_factor())
 *
 * See xfce_panel_pixbuf_from_source_at_size()
 *
 * See also: #XfcePanelImage
 *
 * Since: 4.17.4
 **/
void
xfce_panel_set_image_from_source (GtkImage     *image,
                                  const gchar  *source,
                                  GtkIconTheme *icon_theme,
                                  gint          size,
                                  gint          scale)
{
  GdkPixbuf *pixbuf;

  g_return_if_fail (GTK_IS_IMAGE (image));

  pixbuf = xfce_panel_pixbuf_from_source (source, icon_theme, size * scale);
  if (G_LIKELY (pixbuf != NULL))
    {
      cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, NULL);
      gtk_image_set_from_surface (image, surface);
      cairo_surface_destroy (surface);
      g_object_unref (pixbuf);
    }
}



#define __XFCE_PANEL_CONVENIENCE_C__
#include <libxfce4panel/libxfce4panel-aliasdef.c>
