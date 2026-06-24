/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>

#include <exo/exo-cell-renderer-icon.h>
#include <exo/exo-gdk-pixbuf-extensions.h>
#include <exo/exo-private.h>
#include <exo/exo-thumbnail.h>
#include <exo/exo-alias.h>

/**
 * SECTION: exo-cell-renderer-icon
 * @title: ExoCellRendererIcon
 * @short_description: Renders an icon in a cell
 * @include: exo/exo.h
 * @see_also: <link linkend="ExoIconView">ExoIconView</link>
 *
 * An #ExoCellRendererIcon can be used to render an icon in a cell. It
 * allows to render either a named icon, which is looked up using the
 * #GtkIconTheme, or an image file loaded from the file system. The icon
 * name or absolute path to the image file is set via the
 * <link linkend="ExoCellRendererIcon--icon">icon</link> property.
 *
 * To support the <link linkend="ExoIconView">ExoIconView</link> (and <link
 * linkend="GtkIconView">GtkIconView</link>), #ExoCellRendererIcon supports
 * rendering icons based on the state of the view if the
 * <link linkend="ExoCellRendererIcon--follow-state">follow-state</link>
 * property is set.
 **/

/* HACK: fix dead API via #define */
# define gtk_icon_info_free(info) g_object_unref (info)

/* Property identifiers */
enum
{
  PROP_0,
  PROP_FOLLOW_STATE,
  PROP_ICON,
  PROP_GICON,
  PROP_SIZE,
};



static void exo_cell_renderer_icon_finalize     (GObject                  *object);
static void exo_cell_renderer_icon_get_property (GObject                  *object,
                                                 guint                     prop_id,
                                                 GValue                   *value,
                                                 GParamSpec               *pspec);
static void exo_cell_renderer_icon_set_property (GObject                  *object,
                                                 guint                     prop_id,
                                                 const GValue             *value,
                                                 GParamSpec               *pspec);
static void exo_cell_renderer_icon_get_size     (GtkCellRenderer          *renderer,
                                                 GtkWidget                *widget,
                                                 const GdkRectangle       *cell_area,
                                                 gint                     *x_offset,
                                                 gint                     *y_offset,
                                                 gint                     *width,
                                                 gint                     *height);
static void exo_cell_renderer_icon_render       (GtkCellRenderer          *renderer,
                                                 cairo_t                  *cr,
                                                 GtkWidget                *widget,
                                                 const GdkRectangle       *background_area,
                                                 const GdkRectangle       *cell_area,
                                                 GtkCellRendererState      flags);



struct _ExoCellRendererIconPrivate
{
  guint  follow_state : 1;
  guint  icon_static : 1;
  gchar *icon;
  GIcon *gicon;
  gint   size;
};



G_DEFINE_TYPE_WITH_PRIVATE (ExoCellRendererIcon, exo_cell_renderer_icon, GTK_TYPE_CELL_RENDERER)



static void
exo_cell_renderer_icon_class_init (ExoCellRendererIconClass *klass)
{
  GtkCellRendererClass *gtkcell_renderer_class;
  GObjectClass         *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = exo_cell_renderer_icon_finalize;
  gobject_class->get_property = exo_cell_renderer_icon_get_property;
  gobject_class->set_property = exo_cell_renderer_icon_set_property;

  gtkcell_renderer_class = GTK_CELL_RENDERER_CLASS (klass);
  gtkcell_renderer_class->get_size = exo_cell_renderer_icon_get_size;
  gtkcell_renderer_class->render = exo_cell_renderer_icon_render;

  /* initialize the library's i18n support */
  _exo_i18n_init ();

  /**
   * ExoCellRendererIcon:follow-state:
   *
   * Specifies whether the icon renderer should render icon based on the
   * selection state of the items. This is necessary for #ExoIconView,
   * which doesn't draw any item state indicators itself.
   *
   * Since: 0.3.1.9
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FOLLOW_STATE,
                                   g_param_spec_boolean ("follow-state",
                                                         _("Follow state"),
                                                         _("Render differently based on the selection state."),
                                                         TRUE,
                                                         EXO_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * ExoCellRendererIcon:icon:
   *
   * The name of the themed icon to render or an absolute path to an image file
   * to render. May also be %NULL in which case no icon will be rendered for the
   * cell.
   *
   * Image files are loaded via the thumbnail database, creating a thumbnail
   * as necessary. The thumbnail database is also used to load scalable icons
   * in the icon theme, because loading scalable icons is quite expensive
   * these days.
   *
   * Since: 0.3.1.9
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON,
                                   g_param_spec_string ("icon",
                                                        _("Icon"),
                                                        _("The icon to render."),
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  /**
   * ExoCellRendererIcon:gicon:
   *
   * The #GIcon to render. May also be %NULL in which case no icon will be
   * rendered for the cell.
   *
   * Currently only #GThemedIcon<!---->s are supported which are loaded
   * using the current icon theme.
   *
   * Since: 0.4.0
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_GICON,
                                   g_param_spec_object ("gicon",
                                                        _("GIcon"),
                                                        _("The GIcon to render."),
                                                        G_TYPE_ICON,
                                                        EXO_PARAM_READWRITE));

  /**
   * ExoCellRendererIcon:size:
   *
   * The size in pixel at which to render the icon. This is also the fixed
   * size that the renderer will request no matter if the actual icons are
   * smaller than this size.
   *
   * This improves the performance of the layouting in the icon and tree
   * view, because during the layouting phase no icons will need to be
   * loaded, but the icons will only be loaded when they need to be rendered,
   * i.e. the view scrolls to the cell.
   *
   * Since: 0.3.1.9
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SIZE,
                                   g_param_spec_int ("size",
                                                     _("size"),
                                                     _("The size of the icon to render in pixels."),
                                                     1, G_MAXINT, 48,
                                                     EXO_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}



static void
exo_cell_renderer_icon_init (ExoCellRendererIcon *icon_undocked)
{
}



static void
exo_cell_renderer_icon_finalize (GObject *object)
{
  const ExoCellRendererIconPrivate *priv = exo_cell_renderer_icon_get_instance_private (EXO_CELL_RENDERER_ICON (object));

  /* free the icon if not static */
  if (!priv->icon_static)
    g_free (priv->icon);

  /* free the GICon */
  if (priv->gicon != NULL)
    g_object_unref (priv->gicon);

  (*G_OBJECT_CLASS (exo_cell_renderer_icon_parent_class)->finalize) (object);
}



static void
exo_cell_renderer_icon_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  const ExoCellRendererIconPrivate *priv = exo_cell_renderer_icon_get_instance_private (EXO_CELL_RENDERER_ICON (object));

  switch (prop_id)
    {
    case PROP_FOLLOW_STATE:
      g_value_set_boolean (value, priv->follow_state);
      break;

    case PROP_ICON:
      g_value_set_string (value, priv->icon);
      break;

    case PROP_GICON:
      g_value_set_object (value, priv->gicon);
      break;

    case PROP_SIZE:
      g_value_set_int (value, priv->size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
exo_cell_renderer_icon_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ExoCellRendererIconPrivate *priv = exo_cell_renderer_icon_get_instance_private (EXO_CELL_RENDERER_ICON (object));
  const gchar                *icon;

  switch (prop_id)
    {
    case PROP_FOLLOW_STATE:
      priv->follow_state = g_value_get_boolean (value);
      break;

    case PROP_ICON:
      /* release the previous icon (if not static) */
      if (!priv->icon_static)
        g_free (priv->icon);
      icon = g_value_get_string (value);
      priv->icon_static = (value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS) ? TRUE : FALSE;
      priv->icon = (gchar *) ((icon == NULL) ? "" : icon);
      if (!priv->icon_static)
        priv->icon = g_strdup (priv->icon);
      break;

    case PROP_GICON:
      if (priv->gicon != NULL)
        g_object_unref (priv->gicon);
      priv->gicon = g_value_dup_object (value);
      break;

    case PROP_SIZE:
      priv->size = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
exo_cell_renderer_icon_get_size (GtkCellRenderer *renderer,
                                 GtkWidget       *widget,
                                 const GdkRectangle *cell_area,
                                 gint            *x_offset,
                                 gint            *y_offset,
                                 gint            *width,
                                 gint            *height)
{
  const ExoCellRendererIconPrivate *priv = exo_cell_renderer_icon_get_instance_private (EXO_CELL_RENDERER_ICON (renderer));
  gfloat xalign, yalign;
  gint   xpad, ypad;

  gtk_cell_renderer_get_alignment (renderer, &xalign, &yalign);
  gtk_cell_renderer_get_padding (renderer, &xpad, &ypad);

  if (cell_area != NULL)
    {
      if (x_offset != NULL)
        {
          *x_offset = ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ? 1.0 - xalign : xalign)
                    * (cell_area->width - priv->size);
          *x_offset = MAX (*x_offset, 0) + xpad;
        }

      if (y_offset != NULL)
        {
          *y_offset = yalign * (cell_area->height - priv->size);
          *y_offset = MAX (*y_offset, 0) + ypad;
        }
    }
  else
    {
      if (x_offset != NULL)
        *x_offset = 0;

      if (y_offset != NULL)
        *y_offset = 0;
    }

  if (G_LIKELY (width != NULL))
    *width = (gint) xpad * 2 + priv->size;

  if (G_LIKELY (height != NULL))
    *height = (gint) ypad * 2 + priv->size;
}


static void
exo_cell_renderer_icon_render (GtkCellRenderer     *renderer,
                               cairo_t             *cr,
                               GtkWidget           *widget,
                               const GdkRectangle  *background_area,
                               const GdkRectangle  *cell_area,
                               GtkCellRendererState flags)
{
  GdkRectangle        clip_area;
  GdkRectangle       *expose_area = &clip_area;
  GdkRGBA            *color_rgba;
  GdkColor            color_gdk;
  GtkStyleContext    *style_context;
  const ExoCellRendererIconPrivate *priv = exo_cell_renderer_icon_get_instance_private (EXO_CELL_RENDERER_ICON (renderer));
  GtkIconTheme                     *icon_theme;
  GdkRectangle                      icon_area;
  GdkRectangle                      draw_area;
  const gchar                      *filename;
  GtkIconInfo                      *icon_info = NULL;
  GdkPixbuf                        *icon = NULL;
  GdkPixbuf                        *temp;
  cairo_surface_t                  *surface;
  GError                           *err = NULL;
  gchar                            *display_name = NULL;
  gint                             *theme_icon_sizes;
  gint                              requested_icon_size;
  gint                              selected_icon_size;
  gint                              scale_factor;
  gint                              n;

  gdk_cairo_get_clip_rectangle (cr, expose_area);

  /* verify that we have an icon */
  if (G_UNLIKELY (priv->icon == NULL && priv->gicon == NULL))
    return;

  scale_factor = gtk_widget_get_scale_factor (widget);
  requested_icon_size = priv->size * scale_factor;

  /* icon may be either an image file or a named icon */
  if (priv->icon != NULL && g_path_is_absolute (priv->icon))
    {
      /* load the icon via the thumbnail database */
      icon = _exo_thumbnail_get_for_file (priv->icon, (requested_icon_size > 128) ? EXO_THUMBNAIL_SIZE_LARGE : EXO_THUMBNAIL_SIZE_NORMAL, &err);
    }
  else if (priv->icon != NULL || priv->gicon != NULL)
    {
      /* determine the best icon size (GtkIconTheme is somewhat messy scaling up small icons) */
      icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));

      if (priv->icon != NULL)
        {
          theme_icon_sizes = gtk_icon_theme_get_icon_sizes (icon_theme, priv->icon);
          for (selected_icon_size = -1, n = 0; theme_icon_sizes[n] != 0; ++n)
            {
              /* we can use any size if scalable, because we load the file directly. also bail if we get an exact match. */
              if (theme_icon_sizes[n] == -1 || theme_icon_sizes[n] == requested_icon_size)
                {
                  selected_icon_size = requested_icon_size;
                  break;
                }
              /* selected icon size is still too small, but this icon size is better */
              else if (selected_icon_size < requested_icon_size && theme_icon_sizes[n] > selected_icon_size)
                selected_icon_size = theme_icon_sizes[n];
              /* this icon is large enough, and is a closer match than what we've selected */
              else if (theme_icon_sizes[n] > requested_icon_size && theme_icon_sizes[n] < selected_icon_size)
                selected_icon_size = theme_icon_sizes[n];
            }
          g_free (theme_icon_sizes);

          /* if we don't know any icon sizes at all, the icon is probably not present */
          if (G_UNLIKELY (selected_icon_size < 0))
            selected_icon_size = requested_icon_size;

          /* lookup the icon in the icon theme */
          icon_info = gtk_icon_theme_lookup_icon (icon_theme, priv->icon, selected_icon_size, GTK_ICON_LOOKUP_FORCE_SIZE);
        }
      else if (priv->gicon != NULL)
        {
          icon_info = gtk_icon_theme_lookup_by_gicon (icon_theme,
                                                      priv->gicon,
                                                      requested_icon_size,
                                                      GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_FORCE_SIZE);
        }

      if (G_UNLIKELY (icon_info == NULL))
        return;

      /* check if we have an SVG icon here */
      filename = gtk_icon_info_get_filename (icon_info);
      if (filename != NULL && g_str_has_suffix (filename, ".svg"))
        {
          /* loading SVG icons is terribly slow, so we try to use thumbnail instead, and we use the
           * real available cell area directly here, because loading thumbnails involves scaling anyway
           * and this way we need to the thumbnail pixbuf scale only once.
           */
          icon = _exo_thumbnail_get_for_file (filename, (requested_icon_size > 128) ? EXO_THUMBNAIL_SIZE_LARGE : EXO_THUMBNAIL_SIZE_NORMAL, &err);
        }
      else
        {
          /* regularly load the icon from the theme */
          icon = gtk_icon_info_load_icon (icon_info, &err);
        }
      gtk_icon_info_free (icon_info);
    }

  /* check if we failed */
  if (G_UNLIKELY (icon == NULL))
    {
      /* better let the user know whats going on, might be surprising otherwise */
      if (G_LIKELY (priv->icon != NULL))
        {
          display_name = g_filename_display_name (priv->icon);
        }
      else if (G_UNLIKELY (priv->gicon != NULL
                           && g_object_class_find_property (G_OBJECT_GET_CLASS (priv->gicon),
                                                            "name")))
        {
          g_object_get (priv->gicon, "name", &display_name, NULL);
        }

      if (display_name != NULL)
        {
          g_warning ("Failed to load \"%s\": %s", display_name, err->message);
          g_free (display_name);
        }

      g_error_free (err);
      return;
    }

  /* determine the real icon size */
  icon_area.width = gdk_pixbuf_get_width (icon) / scale_factor;
  icon_area.height = gdk_pixbuf_get_height (icon) / scale_factor;

  /* scale down the icon on-demand */
  if (G_UNLIKELY (icon_area.width > cell_area->width || icon_area.height > cell_area->height))
    {
      /* scale down to fit */
      temp = exo_gdk_pixbuf_scale_down (icon, TRUE, cell_area->width * scale_factor, cell_area->height * scale_factor);
      g_object_unref (G_OBJECT (icon));
      icon = temp;

      /* determine the icon dimensions again */
      icon_area.width = gdk_pixbuf_get_width (icon) / scale_factor;
      icon_area.height = gdk_pixbuf_get_height (icon) / scale_factor;
    }

  icon_area.x = cell_area->x + (cell_area->width - icon_area.width) / 2;
  icon_area.y = cell_area->y + (cell_area->height - icon_area.height) / 2;

  /* Gtk3: we don't have any expose rectangle and just draw everything */
  if (gdk_rectangle_intersect (expose_area, &icon_area, &draw_area))
    {
      /* colorize the icon if we should follow the selection state */
      if ((flags & (GTK_CELL_RENDERER_SELECTED | GTK_CELL_RENDERER_PRELIT)) != 0 && priv->follow_state)
        {
          if ((flags & GTK_CELL_RENDERER_SELECTED) != 0)
            {
              style_context = gtk_widget_get_style_context (widget);
              gtk_style_context_get (style_context, gtk_widget_has_focus (widget) ? GTK_STATE_FLAG_SELECTED : GTK_STATE_FLAG_ACTIVE,
                                     GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &color_rgba,
                                     NULL);

              color_gdk.pixel = 0;
              color_gdk.red = color_rgba->red * 65535;
              color_gdk.blue = color_rgba->blue * 65535;
              color_gdk.green = color_rgba->green * 65535;
              gdk_rgba_free (color_rgba);
              temp = exo_gdk_pixbuf_colorize (icon, &color_gdk);
              g_object_unref (G_OBJECT (icon));
              icon = temp;
            }

          if ((flags & GTK_CELL_RENDERER_PRELIT) != 0)
            {
              temp = exo_gdk_pixbuf_spotlight (icon);
              g_object_unref (G_OBJECT (icon));
              icon = temp;
            }
        }

      /* check if we should render an insensitive icon */
      if (G_UNLIKELY (gtk_widget_get_state_flags(widget) & GTK_STATE_INSENSITIVE || !gtk_cell_renderer_get_sensitive (renderer)))
        {
          style_context = gtk_widget_get_style_context (widget);
          gtk_style_context_get (style_context, GTK_STATE_FLAG_INSENSITIVE,
                                 GTK_STYLE_PROPERTY_COLOR, &color_rgba,
                                 NULL);

          color_gdk.pixel = 0;
          color_gdk.red = color_rgba->red * 65535;
          color_gdk.blue = color_rgba->blue * 65535;
          color_gdk.green = color_rgba->green * 65535;
          gdk_rgba_free (color_rgba);
          temp = exo_gdk_pixbuf_colorize (icon, &color_gdk);

          g_object_unref (G_OBJECT (icon));
          icon = temp;
        }

      /* render the invalid parts of the icon */
      surface = gdk_cairo_surface_create_from_pixbuf (icon, scale_factor, gtk_widget_get_window (widget));
      cairo_set_source_surface (cr, surface, icon_area.x, icon_area.y);
      cairo_rectangle (cr, draw_area.x, draw_area.y, draw_area.width, draw_area.height);
      cairo_fill (cr);

      cairo_surface_destroy (surface);
    }

  /* release the file's icon */
  g_object_unref (G_OBJECT (icon));
}



/**
 * exo_cell_renderer_icon_new:
 *
 * Creates a new #ExoCellRendererIcon. Adjust rendering parameters using object properties,
 * which can be set globally via g_object_set(). Also, with #GtkCellLayout and
 * #GtkTreeViewColumn, you can bind a property to a value in a #GtkTreeModel. For example
 * you can bind the <link linkend="ExoCellRendererIcon--icon">icon</link> property on the
 * cell renderer to an icon name in the model, thus rendering a different icon in each row
 * of the #GtkTreeView.
 *
 * Returns: the newly allocated #ExoCellRendererIcon.
 *
 * Since: 0.3.1.9
 **/
GtkCellRenderer*
exo_cell_renderer_icon_new (void)
{
  return g_object_new (EXO_TYPE_CELL_RENDERER_ICON, NULL);
}



#define __EXO_CELL_RENDERER_ICON_C__
#include <exo/exo-aliasdef.c>
