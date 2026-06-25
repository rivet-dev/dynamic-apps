/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-clipboard-manager.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gdk-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-icon-renderer.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>



enum
{
  PROP_0,
  PROP_DROP_FILE,
  PROP_FILE,
  PROP_EMBLEMS,
  PROP_FOLLOW_STATE,
  PROP_SIZE,
  PROP_HIGHLIGHT_COLOR,
  PROP_ROUNDED_CORNERS,
  PROP_HIGHLIGHTING_ENABLED,
};



static void thunar_icon_renderer_finalize      (GObject                 *object);
static void thunar_icon_renderer_get_property  (GObject                 *object,
                                                guint                    prop_id,
                                                GValue                  *value,
                                                GParamSpec              *pspec);
static void thunar_icon_renderer_set_property  (GObject                 *object,
                                                guint                    prop_id,
                                                const GValue            *value,
                                                GParamSpec              *pspec);
static void thunar_icon_renderer_get_preferred_width (GtkCellRenderer   *renderer,
                                                GtkWidget               *widget,
                                                gint                    *minimum,
                                                gint                    *natural);
static void thunar_icon_renderer_get_preferred_height (GtkCellRenderer  *renderer,
                                                GtkWidget               *widget,
                                                gint                    *minimum,
                                                gint                    *natural);
static void thunar_icon_renderer_render        (GtkCellRenderer         *renderer,
                                                cairo_t                 *cr,
                                                GtkWidget               *widget,
                                                const GdkRectangle      *background_area,
                                                const GdkRectangle      *cell_area,
                                                GtkCellRendererState     flags);



G_DEFINE_TYPE (ThunarIconRenderer, thunar_icon_renderer, GTK_TYPE_CELL_RENDERER)



static void
thunar_icon_renderer_class_init (ThunarIconRendererClass *klass)
{
  GtkCellRendererClass *gtkcell_renderer_class;
  GObjectClass         *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_icon_renderer_finalize;
  gobject_class->get_property = thunar_icon_renderer_get_property;
  gobject_class->set_property = thunar_icon_renderer_set_property;

  gtkcell_renderer_class = GTK_CELL_RENDERER_CLASS (klass);
  gtkcell_renderer_class->get_preferred_width = thunar_icon_renderer_get_preferred_width;
  gtkcell_renderer_class->get_preferred_height = thunar_icon_renderer_get_preferred_height;
  gtkcell_renderer_class->render = thunar_icon_renderer_render;

  /**
   * ThunarIconRenderer:drop-file:
   *
   * The file which should be rendered in the drop
   * accept state.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DROP_FILE,
                                   g_param_spec_object ("drop-file",
                                                        "drop-file",
                                                        "drop-file",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarIconRenderer:file:
   *
   * The file whose icon to render.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file", "file", "file",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarIconRenderer:emblems:
   *
   * Specifies whether to render emblems in addition to the file icons.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_EMBLEMS,
                                   g_param_spec_boolean ("emblems",
                                                         "emblems",
                                                         "emblems",
                                                         TRUE,
                                                         G_PARAM_CONSTRUCT | EXO_PARAM_READWRITE));

  /**
   * ThunarIconRenderer:follow-state:
   *
   * Specifies whether the icon renderer should render icons
   * based on the selection state of the items. This is necessary
   * for #ExoIconView, which doesn't draw any item state indicators
   * itself.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FOLLOW_STATE,
                                   g_param_spec_boolean ("follow-state",
                                                         "follow-state",
                                                         "follow-state",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarIconRenderer:size:
   *
   * The size at which icons should be rendered by this
   * #ThunarIconRenderer instance.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SIZE,
                                   g_param_spec_enum ("size", "size", "size",
                                                      THUNAR_TYPE_ICON_SIZE,
                                                      THUNAR_ICON_SIZE_32,
                                                      G_PARAM_CONSTRUCT | EXO_PARAM_READWRITE));



  /**
   * ThunarIconRenderer:highlight-color:
   *
   * The color with which the cell should be highlighted.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_HIGHLIGHT_COLOR,
                                   g_param_spec_string ("highlight-color", "highlight-color", "highlight-color",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));



  /**
   * ThunarIconRenderer:rounded-corners:
   *
   * Determines if the cell should be clipped to rounded-corners.
   * Useful when highlighting is enabled & a highlight color is set.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ROUNDED_CORNERS,
                                   g_param_spec_boolean ("rounded-corners", "rounded-corners", "rounded-corners",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));



  /**
   * ThunarIconRenderer:highlighting-enabled:
   *
   * Determines if the cell background should be drawn with highlight color.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_HIGHLIGHTING_ENABLED,
                                   g_param_spec_boolean ("highlighting-enabled", "highlighting-enabled", "highlighting-enabled",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_icon_renderer_init (ThunarIconRenderer *icon_renderer)
{
  /* use 1px padding */
  gtk_cell_renderer_set_padding (GTK_CELL_RENDERER (icon_renderer), 1, 1);
}



static void
thunar_icon_renderer_finalize (GObject *object)
{
  ThunarIconRenderer *icon_renderer = THUNAR_ICON_RENDERER (object);

  /* free the icon data */
  if (G_UNLIKELY (icon_renderer->drop_file != NULL))
    g_object_unref (G_OBJECT (icon_renderer->drop_file));
  if (G_LIKELY (icon_renderer->file != NULL))
    g_object_unref (G_OBJECT (icon_renderer->file));
  g_free (icon_renderer->highlight_color);

  (*G_OBJECT_CLASS (thunar_icon_renderer_parent_class)->finalize) (object);
}



static void
thunar_icon_renderer_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarIconRenderer *icon_renderer = THUNAR_ICON_RENDERER (object);

  switch (prop_id)
    {
    case PROP_DROP_FILE:
      g_value_set_object (value, icon_renderer->drop_file);
      break;

    case PROP_FILE:
      g_value_set_object (value, icon_renderer->file);
      break;

    case PROP_EMBLEMS:
      g_value_set_boolean (value, icon_renderer->emblems);
      break;

    case PROP_FOLLOW_STATE:
      g_value_set_boolean (value, icon_renderer->follow_state);
      break;

    case PROP_SIZE:
      g_value_set_enum (value, icon_renderer->size);
      break;

    case PROP_HIGHLIGHT_COLOR:
      g_value_set_string (value, icon_renderer->highlight_color);
      break;

    case PROP_ROUNDED_CORNERS:
      g_value_set_boolean (value, icon_renderer->rounded_corners);
      break;

    case PROP_HIGHLIGHTING_ENABLED:
      g_value_set_boolean (value, icon_renderer->highlighting_enabled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_icon_renderer_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ThunarIconRenderer *icon_renderer = THUNAR_ICON_RENDERER (object);

  switch (prop_id)
    {
    case PROP_DROP_FILE:
      if (G_LIKELY (icon_renderer->drop_file != NULL))
        g_object_unref (G_OBJECT (icon_renderer->drop_file));
      icon_renderer->drop_file = (gpointer) g_value_dup_object (value);
      break;

    case PROP_FILE:
      if (G_LIKELY (icon_renderer->file != NULL))
        g_object_unref (G_OBJECT (icon_renderer->file));
      icon_renderer->file = (gpointer) g_value_dup_object (value);
      break;

    case PROP_EMBLEMS:
      icon_renderer->emblems = g_value_get_boolean (value);
      break;

    case PROP_FOLLOW_STATE:
      icon_renderer->follow_state = g_value_get_boolean (value);
      break;

    case PROP_SIZE:
      icon_renderer->size = g_value_get_enum (value);
      break;

    case PROP_HIGHLIGHT_COLOR:
      g_free (icon_renderer->highlight_color);
      icon_renderer->highlight_color = g_value_dup_string (value);
      break;

    case PROP_ROUNDED_CORNERS:
      icon_renderer->rounded_corners = g_value_get_boolean (value);
      break;

    case PROP_HIGHLIGHTING_ENABLED:
      icon_renderer->highlighting_enabled = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}




static void
thunar_icon_renderer_get_preferred_width (GtkCellRenderer *renderer,
                                          GtkWidget       *widget,
                                          gint            *minimum,
                                          gint            *natural)
{
  ThunarIconRenderer *icon_renderer = THUNAR_ICON_RENDERER (renderer);
  int xpad;

  gtk_cell_renderer_get_padding (renderer, &xpad, NULL);

  if (G_LIKELY (minimum)) *minimum = (gint) xpad * 2 + icon_renderer->size;
  if (G_LIKELY (natural)) *natural = (gint) xpad * 2 + icon_renderer->size;
}



static void
thunar_icon_renderer_get_preferred_height (GtkCellRenderer *renderer,
                                           GtkWidget       *widget,
                                           gint            *minimum,
                                           gint            *natural)
{
  ThunarIconRenderer *icon_renderer = THUNAR_ICON_RENDERER (renderer);
  int ypad;

  gtk_cell_renderer_get_padding (renderer, NULL, &ypad);

  if (G_LIKELY (minimum)) *minimum = (gint) ypad * 2 + icon_renderer->size;
  if (G_LIKELY (natural)) *natural = (gint) ypad * 2 + icon_renderer->size;
}



static void
thunar_icon_renderer_color_insensitive (cairo_t   *cr,
                                        GtkWidget *widget)
{
  cairo_pattern_t *source;
  GdkRGBA          *color;
  GtkStyleContext *context = gtk_widget_get_style_context (widget);

  cairo_save (cr);

  source = cairo_pattern_reference (cairo_get_source (cr));
  gtk_style_context_get (context, GTK_STATE_FLAG_INSENSITIVE, GTK_STYLE_PROPERTY_COLOR, &color, NULL);
  gdk_cairo_set_source_rgba (cr, color);
  gdk_rgba_free (color);
  cairo_set_operator (cr, CAIRO_OPERATOR_MULTIPLY);

  cairo_mask (cr, source);

  cairo_pattern_destroy (source);
  cairo_restore (cr);
}



static void
thunar_icon_renderer_color_selected (cairo_t   *cr,
                                     GtkWidget *widget)
{
  cairo_pattern_t *source;
  GtkStateFlags    state;
  GdkRGBA          *color;
  GtkStyleContext *context = gtk_widget_get_style_context (widget);

  cairo_save (cr);

  source = cairo_pattern_reference (cairo_get_source (cr));
  state = gtk_widget_has_focus (widget) ? GTK_STATE_FLAG_SELECTED : GTK_STATE_FLAG_ACTIVE;
  gtk_style_context_get (context, state, GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &color, NULL);
  gdk_cairo_set_source_rgba (cr, color);
  gdk_rgba_free (color);
  cairo_set_operator (cr, CAIRO_OPERATOR_MULTIPLY);

  cairo_mask (cr, source);

  cairo_pattern_destroy (source);
  cairo_restore (cr);
}



static void
thunar_icon_renderer_color_lighten (cairo_t   *cr,
                                    GtkWidget *widget)
{
  cairo_pattern_t *source;

  cairo_save (cr);

  source = cairo_pattern_reference (cairo_get_source (cr));
  cairo_set_source_rgb (cr, .15, .15, .15);
  cairo_set_operator (cr, CAIRO_OPERATOR_COLOR_DODGE);

  cairo_mask (cr, source);

  cairo_pattern_destroy (source);
  cairo_restore (cr);
}



static void
thunar_icon_renderer_render (GtkCellRenderer     *renderer,
                             cairo_t             *cr,
                             GtkWidget           *widget,
                             const GdkRectangle  *background_area,
                             const GdkRectangle  *cell_area,
                             GtkCellRendererState flags)
{
  ThunarClipboardManager *clipboard;
  ThunarFileIconState     icon_state;
  ThunarIconRenderer     *icon_renderer = THUNAR_ICON_RENDERER (renderer);
  ThunarIconFactory      *icon_factory;
  GtkIconTheme           *icon_theme;
  GdkRectangle            emblem_area;
  GdkRectangle            icon_area;
  GdkRectangle            clip_area;
  GdkPixbuf              *emblem;
  GdkPixbuf              *icon;
  GdkPixbuf              *temp;
  GList                  *emblems;
  GList                  *lp;
  gint                    scale_factor;
  gint                    max_emblems;
  gint                    position;
  gdouble                 alpha;
  gint                    emblem_size;
  gboolean                color_selected;
  gboolean                color_lighten;
  gboolean                is_expanded;

  if (G_UNLIKELY (icon_renderer->file == NULL))
    return;

  if (G_UNLIKELY (!gdk_cairo_get_clip_rectangle (cr, &clip_area)))
    return;

  g_object_get (renderer, "is-expanded", &is_expanded, NULL);

  if (THUNAR_ICON_RENDERER (renderer)->highlighting_enabled)
    thunar_util_clip_view_background (renderer, cr, background_area, widget, flags);

  /* determine the icon state */
  icon_state = (icon_renderer->drop_file != icon_renderer->file)
             ? is_expanded
              ? THUNAR_FILE_ICON_STATE_OPEN
              : THUNAR_FILE_ICON_STATE_DEFAULT
             : THUNAR_FILE_ICON_STATE_DROP;

  /* load the main icon */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));
  icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);
  scale_factor = gtk_widget_get_scale_factor (widget);
  icon = thunar_icon_factory_load_file_icon (icon_factory, icon_renderer->file, icon_state, icon_renderer->size * scale_factor);
  if (G_UNLIKELY (icon == NULL))
    {
      g_object_unref (G_OBJECT (icon_factory));
      return;
    }

  /* pre-light the item if we're dragging about it */
  if (G_UNLIKELY (icon_state == THUNAR_FILE_ICON_STATE_DROP))
    flags |= GTK_CELL_RENDERER_PRELIT;

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

  icon_area.x = cell_area->x + (cell_area->width - icon_area.width) / 2;
  icon_area.y = cell_area->y + (cell_area->height - icon_area.height) / 2;

  /* bools for cairo transformations */
  color_selected = (flags & GTK_CELL_RENDERER_SELECTED) != 0 && icon_renderer->follow_state;
  color_lighten = (flags & GTK_CELL_RENDERER_PRELIT) != 0 && icon_renderer->follow_state;

  /* check whether the icon is affected by the expose event */
  if (gdk_rectangle_intersect (&clip_area, &icon_area, NULL))
    {
      /* use a translucent icon to represent cutted and hidden files to the user */
      clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (widget));
      if (thunar_clipboard_manager_has_cutted_file (clipboard, icon_renderer->file) && thunar_file_is_writable (icon_renderer->file))
        {
          /* 50% translucent for cutted files */
          alpha = 0.50;
        }
      else if (thunar_file_is_hidden (icon_renderer->file))
        {
          /* 75% translucent for hidden files */
          alpha = 0.75;
        }
      else
        {
          alpha = 1.00;
        }
      g_object_unref (G_OBJECT (clipboard));

      /* render the invalid parts of the icon */
      thunar_gdk_cairo_set_source_pixbuf (cr, icon, icon_area.x, icon_area.y, scale_factor);
      cairo_paint_with_alpha (cr, alpha);

      /* check if we should render an insensitive icon */
      if (G_UNLIKELY (gtk_widget_get_state_flags (widget) == GTK_STATE_FLAG_INSENSITIVE || !gtk_cell_renderer_get_sensitive (renderer)))
        thunar_icon_renderer_color_insensitive(cr,widget);

      /* paint the lighten mask */
      if (color_lighten)
        thunar_icon_renderer_color_lighten (cr, widget);

      /* paint the selected mask */
      if (color_selected)
        thunar_icon_renderer_color_selected (cr, widget);
    }

  /* release the file's icon */
  g_object_unref (G_OBJECT (icon));

  /* check if we should render emblems as well */
  if (G_LIKELY (icon_renderer->emblems))
    {
      /* display the primary emblem as well (if any) */
      emblems = thunar_file_get_emblem_names (icon_renderer->file);
      if (G_UNLIKELY (emblems != NULL))
        {
          /* render up to four emblems for sizes from 48 onwards, else up to 2 emblems */
          max_emblems = (icon_renderer->size < 48) ? 2 : 4;

          /* render the emblems */
          for (lp = emblems, position = 0; lp != NULL && position < max_emblems; lp = lp->next)
            {
              /* calculate the emblem size */
              emblem_size = MIN ((2 * icon_renderer->size) / 3, 32);

              /* check if we have the emblem in the icon theme */
              emblem = thunar_icon_factory_load_icon (icon_factory, lp->data, emblem_size * scale_factor, FALSE);
              if (G_UNLIKELY (emblem == NULL))
                continue;

              /* determine the dimensions of the emblem */
              emblem_area.width = gdk_pixbuf_get_width (emblem) / scale_factor;
              emblem_area.height = gdk_pixbuf_get_height (emblem) / scale_factor;

              /* shrink insane emblems */
              if (G_UNLIKELY (MAX (emblem_area.width, emblem_area.height) > emblem_size))
                {
                  /* scale down the emblem */
                  temp = exo_gdk_pixbuf_scale_ratio (emblem, emblem_size * scale_factor);
                  g_object_unref (G_OBJECT (emblem));
                  emblem = temp;

                  /* determine the size again */
                  emblem_area.width = gdk_pixbuf_get_width (emblem) / scale_factor;
                  emblem_area.height = gdk_pixbuf_get_height (emblem) / scale_factor;
                }

              /* determine a good position for the emblem, depending on the position index */
              switch (position)
                {
                case 0: /* right/bottom */
                  emblem_area.x = MIN (icon_area.x + icon_area.width - emblem_area.width / 2,
                                       cell_area->x + cell_area->width - emblem_area.width);
                  emblem_area.y = MIN (icon_area.y + icon_area.height - emblem_area.height / 2,
                                       cell_area->y + cell_area->height -emblem_area.height);
                  break;

                case 1: /* left/bottom */
                  emblem_area.x = MAX (icon_area.x - emblem_area.width / 2,
                                       cell_area->x);
                  emblem_area.y = MIN (icon_area.y + icon_area.height - emblem_area.height / 2,
                                       cell_area->y + cell_area->height -emblem_area.height);
                  break;

                case 2: /* left/top */
                  emblem_area.x = MAX (icon_area.x - emblem_area.width / 2,
                                       cell_area->x);
                  emblem_area.y = MAX (icon_area.y - emblem_area.height / 2,
                                       cell_area->y);
                  break;

                case 3: /* right/top */
                  emblem_area.x = MIN (icon_area.x + icon_area.width - emblem_area.width / 2,
                                       cell_area->x + cell_area->width - emblem_area.width);
                  emblem_area.y = MAX (icon_area.y - emblem_area.height / 2,
                                       cell_area->y);
                  break;

                default:
                  _thunar_assert_not_reached ();
                }

              /* render the emblem */
              if (gdk_rectangle_intersect (&clip_area, &emblem_area, NULL))
                {
                  /* render the invalid parts of the icon */
                  thunar_gdk_cairo_set_source_pixbuf (cr, emblem, emblem_area.x, emblem_area.y, scale_factor);
                  cairo_paint (cr);

                  /* paint the lighten mask */
                  if (color_lighten)
                    thunar_icon_renderer_color_lighten (cr, widget);

                  /* paint the selected mask */
                  if (color_selected)
                    thunar_icon_renderer_color_selected (cr, widget);
                }

              /* release the emblem */
              g_object_unref (G_OBJECT (emblem));

              /* advance the position index */
              ++position;
            }

          /* release the emblem name list */
          g_list_free (emblems);
        }
    }

  /* release our reference on the icon factory */
  g_object_unref (G_OBJECT (icon_factory));
}



/**
 * thunar_icon_renderer_new:
 *
 * Creates a new #ThunarIconRenderer. Adjust rendering
 * parameters using object properties. Object properties can be
 * set globally with #g_object_set. Also, with #GtkTreeViewColumn,
 * you can bind a property to a value in a #GtkTreeModel.
 *
 * Return value: the newly allocated #ThunarIconRenderer.
 **/
GtkCellRenderer*
thunar_icon_renderer_new (void)
{
  return g_object_new (THUNAR_TYPE_ICON_RENDERER, NULL);
}


