/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2022 Amrit Borah <elessar1802@gmail.com>
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

#include <thunar/thunar-text-renderer.h>
#include <thunar/thunar-util.h>



enum
{
  PROP_0,
  PROP_HIGHLIGHT_COLOR,
  PROP_ROUNDED_CORNERS,
  PROP_HIGHLIGHTING_ENABLED,
};



static void thunar_text_renderer_finalize       (GObject               *object);
static void thunar_text_renderer_get_property   (GObject               *object,
                                                 guint                  prop_id,
                                                 GValue                *value,
                                                 GParamSpec            *pspec);
static void thunar_text_renderer_set_property   (GObject               *object,
                                                 guint                  prop_id,
                                                 const GValue          *value,
                                                 GParamSpec            *pspec);
static void thunar_text_renderer_render         (GtkCellRenderer       *renderer,
                                                 cairo_t               *cr,
                                                 GtkWidget             *widget,
                                                 const GdkRectangle    *background_area,
                                                 const GdkRectangle    *cell_area,
                                                 GtkCellRendererState   flags);



struct _ThunarTextRendererClass
{
  GtkCellRendererTextClass __parent__;

  void (*default_render_function) (GtkCellRenderer      *cell,
                                   cairo_t              *cr,
                                   GtkWidget            *widget,
                                   const GdkRectangle   *background_area,
                                   const GdkRectangle   *cell_area,
                                   GtkCellRendererState  flags);
};

struct _ThunarTextRenderer
{
  GtkCellRendererText  __parent__;

  gchar               *highlight_color;
  gboolean             rounded_corners;
  gboolean             highlighting_enabled;
};



G_DEFINE_TYPE (ThunarTextRenderer, thunar_text_renderer, GTK_TYPE_CELL_RENDERER_TEXT);



static void
thunar_text_renderer_class_init (ThunarTextRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

  object_class->finalize = thunar_text_renderer_finalize;

  object_class->get_property = thunar_text_renderer_get_property;
  object_class->set_property = thunar_text_renderer_set_property;

  klass->default_render_function = cell_class->render;
  cell_class->render = thunar_text_renderer_render;

  /**
   * ThunarTextRenderer:highlight-color:
   *
   * The color with which the cell should be highlighted.
   **/
  g_object_class_install_property (object_class,
                                   PROP_HIGHLIGHT_COLOR,
                                   g_param_spec_string ("highlight-color", "highlight-color", "highlight-color",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));



  /**
   * ThunarTextRenderer:rounded-corners:
   *
   * Determines if the cell should be clipped to rounded corners.
   * Useful when highlighting is enabled & a highlight color is set.
   **/
  g_object_class_install_property (object_class,
                                   PROP_ROUNDED_CORNERS,
                                   g_param_spec_boolean ("rounded-corners", "rounded-corners", "rounded-corners",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));



  /**
   * ThunarTextRenderer:highlighting-enabled:
   *
   * Determines if the cell background should be drawn with highlight color.
   **/
  g_object_class_install_property (object_class,
                                   PROP_HIGHLIGHTING_ENABLED,
                                   g_param_spec_boolean ("highlighting-enabled", "highlighting-enabled", "highlighting-enabled",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_text_renderer_init (ThunarTextRenderer *text_renderer)
{
  text_renderer->highlight_color = NULL;
}



static void
thunar_text_renderer_finalize (GObject *object)
{
  ThunarTextRenderer *text_renderer = THUNAR_TEXT_RENDERER (object);

  g_free (text_renderer->highlight_color);

  G_OBJECT_CLASS (thunar_text_renderer_parent_class)->finalize (object);
}



static void
thunar_text_renderer_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarTextRenderer *text_renderer = THUNAR_TEXT_RENDERER (object);

  switch (prop_id)
    {
    case PROP_HIGHLIGHT_COLOR:
      g_value_set_string (value, text_renderer->highlight_color);
      break;

    case PROP_ROUNDED_CORNERS:
      g_value_set_boolean (value, text_renderer->rounded_corners);
      break;

    case PROP_HIGHLIGHTING_ENABLED:
      g_value_set_boolean (value, text_renderer->highlighting_enabled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_text_renderer_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ThunarTextRenderer *text_renderer = THUNAR_TEXT_RENDERER (object);

  switch (prop_id)
    {
    case PROP_HIGHLIGHT_COLOR:
      g_free (text_renderer->highlight_color);
      text_renderer->highlight_color = g_value_dup_string (value);
      break;

    case PROP_ROUNDED_CORNERS:
      text_renderer->rounded_corners = g_value_get_boolean (value);
      break;

    case PROP_HIGHLIGHTING_ENABLED:
      text_renderer->highlighting_enabled = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/**
 * thunar_text_renderer_new:
 *
 * Creates a new #ThunarTextRenderer. Adjust rendering
 * parameters using object properties. Object properties can be
 * set globally with #g_object_set. Also, with #GtkTreeViewColumn,
 * you can bind a property to a value in a #GtkTreeModel.
 *
 * Return value: (transfer full) The newly allocated #ThunarTextRenderer.
 **/
GtkCellRenderer*
thunar_text_renderer_new (void)
{
  return g_object_new (THUNAR_TYPE_TEXT_RENDERER, NULL);
}



static void
thunar_text_renderer_clear_background (cairo_t   *cr,
                                       GtkWidget *widget)
{
  GtkStyleContext *context;
  GdkRGBA         *color;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_get (context, GTK_STATE_FLAG_NORMAL, GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &color, NULL);
  gdk_cairo_set_source_rgba (cr, color);

  gdk_rgba_free (color);
  cairo_paint (cr);
}



static void
thunar_text_renderer_render (GtkCellRenderer      *cell,
                             cairo_t              *cr,
                             GtkWidget            *widget,
                             const GdkRectangle   *background_area,
                             const GdkRectangle   *cell_area,
                             GtkCellRendererState  flags)
{
  if (THUNAR_TEXT_RENDERER (cell)->highlighting_enabled)
    {
      /* This should paint on top of the current surface. This should hide the highlight
         that is drawn by the default render function of GtkCellRendererText.
         Note: This will unfortunately also hide the prelight. Hence, limiting it to rounded corners only. */
      if (THUNAR_TEXT_RENDERER (cell)->rounded_corners)
        thunar_text_renderer_clear_background (cr, widget);

      thunar_util_clip_view_background (cell, cr, background_area, widget, flags);
    }

  /* we only needed to manipulate the background_area, otherwise everything remains the same.
     Hence, we are simply running the original render function now */
  THUNAR_TEXT_RENDERER_GET_CLASS (THUNAR_TEXT_RENDERER (cell))
    ->default_render_function (cell, cr, widget, background_area, cell_area, flags);
}
