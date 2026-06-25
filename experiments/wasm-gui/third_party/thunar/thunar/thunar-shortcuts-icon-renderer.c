/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#include <gio/gio.h>

#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gdk-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-shortcuts-icon-renderer.h>
#include <thunar/thunar-device.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_GICON,
};



static void thunar_shortcuts_icon_renderer_finalize     (GObject                          *object);
static void thunar_shortcuts_icon_renderer_get_property (GObject                          *object,
                                                         guint                             prop_id,
                                                         GValue                           *value,
                                                         GParamSpec                       *pspec);
static void thunar_shortcuts_icon_renderer_set_property (GObject                          *object,
                                                         guint                             prop_id,
                                                         const GValue                     *value,
                                                         GParamSpec                       *pspec);
static void thunar_shortcuts_icon_renderer_render       (GtkCellRenderer                  *renderer,
                                                         cairo_t                          *cr,
                                                         GtkWidget                        *widget,
                                                         const GdkRectangle               *background_area,
                                                         const GdkRectangle               *cell_area,
                                                         GtkCellRendererState              flags);



struct _ThunarShortcutsIconRendererClass
{
  ThunarIconRendererClass __parent__;
};

struct _ThunarShortcutsIconRenderer
{
  ThunarIconRenderer __parent__;

  ThunarDevice      *device;
  GIcon             *gicon;
};



G_DEFINE_TYPE (ThunarShortcutsIconRenderer, thunar_shortcuts_icon_renderer, THUNAR_TYPE_ICON_RENDERER)



static void
thunar_shortcuts_icon_renderer_class_init (ThunarShortcutsIconRendererClass *klass)
{
  GtkCellRendererClass *gtkcell_renderer_class;
  GObjectClass         *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_shortcuts_icon_renderer_finalize;
  gobject_class->get_property = thunar_shortcuts_icon_renderer_get_property;
  gobject_class->set_property = thunar_shortcuts_icon_renderer_set_property;

  gtkcell_renderer_class = GTK_CELL_RENDERER_CLASS (klass);
  gtkcell_renderer_class->render = thunar_shortcuts_icon_renderer_render;

  /**
   * ThunarShortcutsIconRenderer:device:
   *
   * The #ThunarDevice for which to render an icon or %NULL to fallback
   * to the default icon renderering (see #ThunarIconRenderer).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DEVICE,
                                   g_param_spec_object ("device", "device", "device",
                                                        THUNAR_TYPE_DEVICE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarIconRenderer:gicon:
   *
   * The GIcon to render, this property has preference over the the icon returned
   * by the ThunarFile property.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_GICON,
                                   g_param_spec_object ("gicon", "gicon", "gicon",
                                                        G_TYPE_ICON,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_shortcuts_icon_renderer_init (ThunarShortcutsIconRenderer *shortcuts_icon_renderer)
{
  /* no padding please */
  gtk_cell_renderer_set_padding (GTK_CELL_RENDERER (shortcuts_icon_renderer), 0, 0);
}



static void
thunar_shortcuts_icon_renderer_finalize (GObject *object)
{
  ThunarShortcutsIconRenderer *renderer = THUNAR_SHORTCUTS_ICON_RENDERER (object);

  if (G_UNLIKELY (renderer->device != NULL))
    g_object_unref (renderer->device);

  if (G_UNLIKELY (renderer->gicon != NULL))
    g_object_unref (renderer->gicon);

  (*G_OBJECT_CLASS (thunar_shortcuts_icon_renderer_parent_class)->finalize) (object);
}



static void
thunar_shortcuts_icon_renderer_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  ThunarShortcutsIconRenderer *renderer = THUNAR_SHORTCUTS_ICON_RENDERER (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, renderer->device);
      break;

    case PROP_GICON:
      g_value_set_object (value, renderer->gicon);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_shortcuts_icon_renderer_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  ThunarShortcutsIconRenderer *renderer = THUNAR_SHORTCUTS_ICON_RENDERER (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      if (G_UNLIKELY (renderer->device != NULL))
        g_object_unref (renderer->device);
      renderer->device = g_value_dup_object (value);
      break;

    case PROP_GICON:
      if (G_UNLIKELY (renderer->gicon != NULL))
        g_object_unref (renderer->gicon);
      renderer->gicon = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_shortcuts_icon_renderer_render (GtkCellRenderer     *renderer,
                                       cairo_t             *cr,
                                       GtkWidget           *widget,
                                       const GdkRectangle  *background_area,
                                       const GdkRectangle  *cell_area,
                                       GtkCellRendererState flags)
{
  ThunarShortcutsIconRenderer *shortcuts_icon_renderer = THUNAR_SHORTCUTS_ICON_RENDERER (renderer);
  GtkIconTheme                *icon_theme;
  GdkRectangle                 icon_area;
  GdkRectangle                 clip_area;
  GtkIconInfo                 *icon_info;
  GdkPixbuf                   *icon = NULL;
  GdkPixbuf                   *temp;
  GIcon                       *gicon;
  gdouble                      alpha;
  gint                         scale_factor;

  if (!gdk_cairo_get_clip_rectangle (cr, &clip_area))
    return;

  /* check if we have a volume set */
  if (G_UNLIKELY (shortcuts_icon_renderer->gicon != NULL
      ||  shortcuts_icon_renderer->device != NULL))
    {
      /* load the volume icon */
      icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));

      /* look up the icon info */
      if (shortcuts_icon_renderer->gicon != NULL)
        gicon = g_object_ref (shortcuts_icon_renderer->gicon);
      else
        gicon = thunar_device_get_icon (shortcuts_icon_renderer->device);

      scale_factor = gtk_widget_get_scale_factor (widget);
      icon_info = gtk_icon_theme_lookup_by_gicon (icon_theme, gicon, cell_area->width * scale_factor,
                                                  GTK_ICON_LOOKUP_USE_BUILTIN |
                                                  GTK_ICON_LOOKUP_FORCE_SIZE);
      g_object_unref (gicon);

      /* try to load the icon */
      if (G_LIKELY (icon_info != NULL))
        {
          icon = gtk_icon_info_load_icon (icon_info, NULL);
          g_object_unref (icon_info);
        }

      /* render the icon (if any) */
      if (G_LIKELY (icon != NULL))
        {
          /* determine the real icon size */
          icon_area.width = gdk_pixbuf_get_width (icon) / scale_factor;
          icon_area.height = gdk_pixbuf_get_height (icon) / scale_factor;

          /* scale down the icon on-demand */
          if (G_UNLIKELY (icon_area.width > cell_area->width || icon_area.height > cell_area->height))
            {
              /* scale down to fit */
              temp = exo_gdk_pixbuf_scale_down (icon, TRUE, MAX (1, cell_area->width * scale_factor), MAX (1, cell_area->height * scale_factor));
              g_object_unref (G_OBJECT (icon));
              icon = temp;

              /* determine the icon dimensions again */
              icon_area.width = gdk_pixbuf_get_width (icon) / scale_factor;
              icon_area.height = gdk_pixbuf_get_height (icon) / scale_factor;
            }

          /* 50% translucent for unmounted volumes */
          if (shortcuts_icon_renderer->device != NULL
              && !thunar_device_is_mounted (shortcuts_icon_renderer->device))
            alpha = 0.50;
          else
            alpha = 1.00;

          icon_area.x = cell_area->x + (cell_area->width - icon_area.width) / 2;
          icon_area.y = cell_area->y + (cell_area->height - icon_area.height) / 2;

          /* check whether the icon is affected by the expose event */
          if (gdk_rectangle_intersect (&clip_area, &icon_area, NULL))
            {
              /* render the invalid parts of the icon */
              thunar_gdk_cairo_set_source_pixbuf (cr, icon, icon_area.x, icon_area.y, scale_factor);
              cairo_paint_with_alpha (cr, alpha);
            }

          /* cleanup */
          g_object_unref (G_OBJECT (icon));
        }
    }
  else
    {
      /* fallback to the default icon renderering */
      (*GTK_CELL_RENDERER_CLASS (thunar_shortcuts_icon_renderer_parent_class)->render) (renderer, cr, widget, background_area,
                                                                                        cell_area, flags);
    }
}



/**
 * thunar_shortcuts_icon_renderer_new:
 *
 * Allocates a new #ThunarShortcutsIconRenderer instance.
 *
 * Return value: the newly allocated #ThunarShortcutsIconRenderer.
 **/
GtkCellRenderer*
thunar_shortcuts_icon_renderer_new (void)
{
  return g_object_new (THUNAR_TYPE_SHORTCUTS_ICON_RENDERER, NULL);
}

