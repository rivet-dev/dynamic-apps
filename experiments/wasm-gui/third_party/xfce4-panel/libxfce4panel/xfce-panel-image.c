/*
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
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include <common/panel-private.h>
#include <libxfce4panel/xfce-panel-macros.h>
#include <libxfce4panel/xfce-panel-image.h>
#include <libxfce4panel/xfce-panel-convenience.h>
#include <libxfce4panel/libxfce4panel-alias.h>



/**
 * SECTION: xfce-panel-image
 * @title: XfcePanelImage
 * @short_description: Scalable image suitable for panel plugins
 * @include: libxfce4panel/libxfce4panel.h
 *
 * > #XfcePanelImage has been deprecated since 4.18.1. It was mainly useful with
 * > GTK 2, and is not really adapted to GTK 3. Use #GtkImage instead, and
 * > xfce_panel_set_image_from_source().
 *
 * The #XfcePanelImage is a widgets suitable for for example panel
 * buttons where the developer does not exacly know the size of the
 * image (due to theming and user setting).
 *
 * The #XfcePanelImage widget automatically scales to the allocated
 * size of the widget. Because of that nature it never requests a size,
 * so this will only work if you pack the image in another widget
 * that will expand it.
 * If you want to force an image size you can use xfce_panel_image_set_size()
 * to set a pixel size, in that case the widget will request an fixed size
 * which makes it usefull for usage in dialogs.
 **/



/* design limit for the panel, to reduce the uncached pixbuf size */
#define MAX_PIXBUF_SIZE (128)

#define xfce_panel_image_unref_null(obj)   G_STMT_START { if ((obj) != NULL) \
                                             { \
                                               g_object_unref (G_OBJECT (obj)); \
                                               (obj) = NULL; \
                                             } } G_STMT_END



struct _XfcePanelImagePrivate
{
  /* pixbuf set by the user */
  GdkPixbuf *pixbuf;

  /* internal cached pixbuf (resized) */
  GdkPixbuf *cache;

  /* source name */
  gchar     *source;

  /* fixed size */
  gint       size;

  /* whether we round to fixed icon sizes */
  guint      force_icon_sizes : 1;

  /* cached width and height */
  gint       width;
  gint       height;

  /* idle load timeout */
  guint      idle_load_id;
};

enum
{
  PROP_0,
  PROP_SOURCE,
  PROP_PIXBUF,
  PROP_SIZE
};



static void       xfce_panel_image_get_property         (GObject         *object,
                                                         guint            prop_id,
                                                         GValue          *value,
                                                         GParamSpec      *pspec);
static void       xfce_panel_image_set_property         (GObject         *object,
                                                         guint            prop_id,
                                                         const GValue    *value,
                                                         GParamSpec      *pspec);
static void       xfce_panel_image_finalize             (GObject         *object);
static void       xfce_panel_image_get_preferred_width  (GtkWidget       *widget,
                                                         gint            *minimum_width,
                                                         gint            *natural_width);
static void       xfce_panel_image_get_preferred_height (GtkWidget       *widget,
                                                         gint            *minimum_height,
                                                         gint            *natural_height);
static void       xfce_panel_image_size_allocate        (GtkWidget       *widget,
                                                         GtkAllocation   *allocation);
static gboolean   xfce_panel_image_draw                 (GtkWidget       *widget,
                                                         cairo_t         *cr);
static void       xfce_panel_image_style_updated        (GtkWidget       *widget);
static gboolean   xfce_panel_image_load                 (gpointer         data);
static void       xfce_panel_image_load_destroy         (gpointer         data);
static GdkPixbuf *xfce_panel_image_scale_pixbuf         (GdkPixbuf       *source,
                                                         gint             dest_width,
                                                         gint             dest_height);



G_DEFINE_TYPE_WITH_PRIVATE (XfcePanelImage, xfce_panel_image, GTK_TYPE_WIDGET)



static void
xfce_panel_image_class_init (XfcePanelImageClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = xfce_panel_image_set_property;
  gobject_class->get_property = xfce_panel_image_get_property;
  gobject_class->finalize = xfce_panel_image_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->get_preferred_width = xfce_panel_image_get_preferred_width;
  gtkwidget_class->get_preferred_height = xfce_panel_image_get_preferred_height;
  gtkwidget_class->size_allocate = xfce_panel_image_size_allocate;
  gtkwidget_class->draw = xfce_panel_image_draw;
  gtkwidget_class->style_updated = xfce_panel_image_style_updated;

/**
 * XfcePanelImage:source:
 *
 * Deprecated: 4.18.1: Use #GtkImage:storage-type instead.
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_SOURCE,
                                   g_param_spec_string ("source",
                                                        "Source",
                                                        "Icon or filename",
                                                        NULL,
                                                        G_PARAM_READWRITE
                                                        | G_PARAM_STATIC_STRINGS));

/**
 * XfcePanelImage:pixbuf:
 *
 * Deprecated: 4.18.1: Use #GtkImage:pixbuf instead.
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_PIXBUF,
                                   g_param_spec_object ("pixbuf",
                                                        "Pixbuf",
                                                        "Pixbuf image",
                                                        GDK_TYPE_PIXBUF,
                                                        G_PARAM_READWRITE
                                                        | G_PARAM_STATIC_STRINGS));

/**
 * XfcePanelImage:size:
 *
 * Deprecated: 4.18.1: Use #GtkImage:pixel-size instead.
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_SIZE,
                                   g_param_spec_int ("size",
                                                     "Size",
                                                     "Pixel size of the image",
                                                     -1, MAX_PIXBUF_SIZE, -1,
                                                     G_PARAM_READWRITE
                                                     | G_PARAM_STATIC_STRINGS));

  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_boolean ("force-gtk-icon-sizes",
                                                                 NULL,
                                                                 "Force the image to fix to GtkIconSizes",
                                                                 FALSE,
                                                                 G_PARAM_READWRITE
                                                                 | G_PARAM_STATIC_STRINGS));
}



static void
xfce_panel_image_init (XfcePanelImage *image)
{
  gtk_widget_set_has_window (GTK_WIDGET (image), FALSE);

  image->priv = xfce_panel_image_get_instance_private (image);

  image->priv->pixbuf = NULL;
  image->priv->cache = NULL;
  image->priv->source = NULL;
  image->priv->size = -1;
  image->priv->width = -1;
  image->priv->height = -1;
  image->priv->force_icon_sizes = FALSE;
}



static void
xfce_panel_image_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  XfcePanelImagePrivate *priv = XFCE_PANEL_IMAGE (object)->priv;

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_string (value, priv->source);
      break;

    case PROP_PIXBUF:
      g_value_set_object (value, priv->pixbuf);
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
xfce_panel_image_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_SOURCE:
      xfce_panel_image_set_from_source (XFCE_PANEL_IMAGE (object),
                                        g_value_get_string (value));
      break;

    case PROP_PIXBUF:
      xfce_panel_image_set_from_pixbuf (XFCE_PANEL_IMAGE (object),
                                        g_value_get_object (value));
      break;

    case PROP_SIZE:
      xfce_panel_image_set_size (XFCE_PANEL_IMAGE (object),
                                 g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_panel_image_finalize (GObject *object)
{
  xfce_panel_image_clear (XFCE_PANEL_IMAGE (object));

  (*G_OBJECT_CLASS (xfce_panel_image_parent_class)->finalize) (object);
}



#define GTK_BUTTON_SIZING_FIX

#ifdef GTK_BUTTON_SIZING_FIX
/* When can_focus is true, GtkButton allocates larger size than requested *
 * and causes the panel image to grow indefinitely.                       *
 * This workaround compensates for this difference.                       *
 * Details in https://bugzilla.gnome.org/show_bug.cgi?id=698030           *
 */
static gint
xfce_panel_image_padding_correction (GtkWidget *widget)
{
  GtkWidget             *parent;
  GtkStyleContext       *context;
  gint                   focus_width;
  gint                   focus_pad;
  gint                   correction;

  parent = gtk_widget_get_parent (widget);
  if (parent != NULL &&
      GTK_IS_BUTTON (parent) &&
      !gtk_widget_get_can_focus (parent))
    {
      context = gtk_widget_get_style_context (parent);
      gtk_style_context_get_style (context,
                                   "focus-line-width", &focus_width,
                                   "focus-padding", &focus_pad,
                                   NULL);
      correction = (focus_width + focus_pad) * 2;
    }
  else
    {
      correction = 0;
    }

  return correction;
}
#endif



static void
xfce_panel_image_get_preferred_width (GtkWidget *widget,
                                      gint      *minimum_width,
                                      gint      *natural_width)
{
  XfcePanelImagePrivate *priv = XFCE_PANEL_IMAGE (widget)->priv;
  GtkAllocation          alloc;
  gint                   width, width_min;

  if (priv->size > 0)
    width = priv->size;
  else if (priv->pixbuf != NULL)
    width = gdk_pixbuf_get_width (priv->pixbuf);
  else
    {
      gtk_widget_get_allocation (widget, &alloc);
      width = alloc.width;
    }

#ifdef GTK_BUTTON_SIZING_FIX
  width -= xfce_panel_image_padding_correction (widget);
  width = MAX (width, 0);
#endif

  if (priv->size > 0)
    width_min = width;
  else
    width_min = 0;

  if (minimum_width != NULL)
    *minimum_width = width_min;

  if (natural_width != NULL)
    *natural_width = width;
}



static void
xfce_panel_image_get_preferred_height (GtkWidget *widget,
                                       gint      *minimum_height,
                                       gint      *natural_height)
{
  XfcePanelImagePrivate *priv = XFCE_PANEL_IMAGE (widget)->priv;
  GtkAllocation          alloc;
  gint                   height;

  if (priv->size > 0)
    height = priv->size;
  else if (priv->pixbuf != NULL)
    height = gdk_pixbuf_get_height (priv->pixbuf);
  else
    {
      gtk_widget_get_allocation (widget, &alloc);
      height = alloc.height;
    }

#ifdef GTK_BUTTON_SIZING_FIX
  height -= xfce_panel_image_padding_correction (widget);
  height = MAX (height, 0);
#endif

  if (minimum_height != NULL)
    *minimum_height = priv->size > 0 ? height : 0;

  if (natural_height != NULL)
    *natural_height = height;
}



static void
xfce_panel_image_size_allocate (GtkWidget     *widget,
                                GtkAllocation *allocation)
{
  XfcePanelImagePrivate *priv = XFCE_PANEL_IMAGE (widget)->priv;

  gtk_widget_set_allocation (widget, allocation);

  /* check if the available size changed */
  if ((priv->pixbuf != NULL || priv->source != NULL)
      && allocation->width > 0
      && allocation->height > 0
      && (allocation->width != priv->width
      || allocation->height != priv->height))
    {
      /* store the new size */
      priv->width = allocation->width;
      priv->height = allocation->height;

      /* free cache */
      xfce_panel_image_unref_null (priv->cache);

      if (priv->pixbuf == NULL)
        {
          /* delay icon loading */
          priv->idle_load_id = gdk_threads_add_idle_full (G_PRIORITY_DEFAULT_IDLE, xfce_panel_image_load,
                                                          widget, xfce_panel_image_load_destroy);
        }
      else
        {
          /* directly render pixbufs */
          xfce_panel_image_load (widget);
        }
    }
}



static gboolean
xfce_panel_image_draw (GtkWidget *widget,
                       cairo_t   *cr)
{
  XfcePanelImagePrivate *priv = XFCE_PANEL_IMAGE (widget)->priv;
  gint                   source_width, source_height;
  gint                   dest_x, dest_y;
  GdkPixbuf             *rendered = NULL;
  GdkPixbuf             *pixbuf = priv->cache;
  GtkStyleContext       *context;
  GdkScreen             *screen;
  GtkIconTheme          *icon_theme;
  GtkIconInfo           *icon_info;

  if (G_LIKELY (pixbuf != NULL))
    {
      /* get the size of the cache pixbuf */
      source_width = gdk_pixbuf_get_width (pixbuf);
      source_height = gdk_pixbuf_get_height (pixbuf);

      /* position */
      dest_x = (priv->width - source_width) / 2;
      dest_y = (priv->height - source_height) / 2;

      context = gtk_widget_get_style_context (widget);

      if (!gtk_widget_is_sensitive (widget))
        {
          screen = gtk_widget_get_screen (widget);
          icon_theme = gtk_icon_theme_get_for_screen (screen);
          icon_info = gtk_icon_info_new_for_pixbuf (icon_theme, pixbuf);
          rendered = gtk_icon_info_load_icon (icon_info, NULL);

          if (G_LIKELY (rendered != NULL))
            pixbuf = rendered;
        }

      /* draw the icon */
      gtk_render_icon (context, cr, pixbuf, dest_x, dest_y);

      if (rendered != NULL)
        g_object_unref (G_OBJECT (rendered));
    }

  return FALSE;
}



static void
xfce_panel_image_style_updated (GtkWidget *widget)
{
  XfcePanelImagePrivate *priv = XFCE_PANEL_IMAGE (widget)->priv;
  gboolean               force;

  /* let gtk update the widget style */
  (*GTK_WIDGET_CLASS (xfce_panel_image_parent_class)->style_updated) (widget);

  /* get style property */
  gtk_widget_style_get (widget, "force-gtk-icon-sizes", &force, NULL);

  /* update if needed */
  if (priv->force_icon_sizes != force)
    {
      priv->force_icon_sizes = force;
      if (priv->size > 0)
        gtk_widget_queue_resize (widget);
    }

  /* update the icon if we have an icon-name source */
  /* and size is not set */
  if (priv->source != NULL
      && !g_path_is_absolute (priv->source)
      && priv->size <= 0)
    {
      /* unset the size to force an update */
      priv->width = priv->height = -1;
      gtk_widget_queue_resize (widget);
    }
}



static gboolean
xfce_panel_image_load (gpointer data)
{
  XfcePanelImagePrivate *priv = XFCE_PANEL_IMAGE (data)->priv;
  GdkPixbuf             *pixbuf;
  GdkScreen             *screen;
  GtkIconTheme          *icon_theme = NULL;
  gint                   dest_w, dest_h;

  dest_w = priv->width;
  dest_h = priv->height;

  if (G_UNLIKELY (priv->force_icon_sizes
      && dest_w < 32
      && dest_w == dest_h))
    {
      /* we use some hardcoded values here for convienence,
       * above 32 pixels svg icons will kick in */
      if (dest_w > 16 && dest_w < 22)
        dest_w = 16;
      else if (dest_w > 22 && dest_w < 24)
        dest_w = 22;
      else if (dest_w > 24 && dest_w < 32)
        dest_w = 24;

      dest_h = dest_w;
    }

  if (priv->pixbuf != NULL)
    {
      /* use the pixbuf set by the user */
      pixbuf = (GdkPixbuf *) g_object_ref (G_OBJECT (priv->pixbuf));

      if (G_LIKELY (pixbuf != NULL))
        {
          /* scale the icon to the correct size */
          priv->cache = xfce_panel_image_scale_pixbuf (pixbuf, dest_w, dest_h);
          g_object_unref (G_OBJECT (pixbuf));
        }
    }
  else
    {
      screen = gtk_widget_get_screen (GTK_WIDGET (data));
      if (G_LIKELY (screen != NULL))
        icon_theme = gtk_icon_theme_get_for_screen (screen);

      priv->cache = xfce_panel_pixbuf_from_source_at_size (priv->source, icon_theme, dest_w, dest_h);
    }

  if (G_LIKELY (priv->cache != NULL))
    gtk_widget_queue_draw (GTK_WIDGET (data));

  return FALSE;
}



static void
xfce_panel_image_load_destroy (gpointer data)
{
  XFCE_PANEL_IMAGE (data)->priv->idle_load_id = 0;
}



static GdkPixbuf *
xfce_panel_image_scale_pixbuf (GdkPixbuf *source,
                               gint       dest_width,
                               gint       dest_height)
{
  gdouble ratio;
  gint    source_width;
  gint    source_height;

  panel_return_val_if_fail (GDK_IS_PIXBUF (source), NULL);

  /* we fail on invalid sizes */
  if (G_UNLIKELY (dest_width <= 0 || dest_height <= 0))
    return NULL;

  source_width = gdk_pixbuf_get_width (source);
  source_height = gdk_pixbuf_get_height (source);

  /* check if we need to scale */
  if (source_width <= dest_width && source_height <= dest_height)
    return (GdkPixbuf *) g_object_ref (G_OBJECT (source));

  /* calculate the new dimensions */

  ratio = MIN ((gdouble) dest_width / (gdouble) source_width,
               (gdouble) dest_height / (gdouble) source_height);

  dest_width  = rint (source_width * ratio);
  dest_height = rint (source_height * ratio);

  return gdk_pixbuf_scale_simple (source, MAX (dest_width, 1),
                                  MAX (dest_height, 1),
                                  GDK_INTERP_BILINEAR);
}



/**
 * xfce_panel_image_new:
 *
 * Creates a new empty #XfcePanelImage widget.
 *
 * returns: a newly created XfcePanelImage widget.
 *
 * Since: 4.8
 *
 * Deprecated: 4.18.1: Use gtk_image_new() instead.
 **/
GtkWidget *
xfce_panel_image_new (void)
{
  return g_object_new (XFCE_TYPE_PANEL_IMAGE, NULL);
}



/**
 * xfce_panel_image_new_from_pixbuf:
 * @pixbuf : (allow-none): a #GdkPixbuf, or %NULL.
 *
 * Creates a new #XfcePanelImage displaying @pixbuf. #XfcePanelImage
 * will add its own reference rather than adopting yours. You don't
 * need to scale the pixbuf to the correct size, the #XfcePanelImage
 * will take care of that based on the allocation of the widget or
 * the size set with xfce_panel_image_set_size().
 *
 * returns: a newly created XfcePanelImage widget.
 *
 * Since: 4.8
 *
 * Deprecated: 4.18.1: Use gtk_image_new_from_pixbuf() instead.
 **/
GtkWidget *
xfce_panel_image_new_from_pixbuf (GdkPixbuf *pixbuf)
{
  g_return_val_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf), NULL);

  return g_object_new (XFCE_TYPE_PANEL_IMAGE,
                       "pixbuf", pixbuf, NULL);
}



/**
 * xfce_panel_image_new_from_source:
 * @source : (allow-none): source of the image. This can be an absolute path or
 *           an icon-name or %NULL.
 *
 * Creates a new #XfcePanelImage displaying @source. #XfcePanelImage
 * will detect if @source points to an absolute file or it and icon-name.
 * For icon-names it will also look for files in the pixbuf folder or
 * strip the extensions, which makes it suitable for usage with icon
 * keys in .desktop files.
 *
 * returns: a newly created XfcePanelImage widget.
 *
 * Since: 4.8
 *
 * Deprecated: 4.18.1: Use gtk_image_new() and xfce_panel_set_image_from_source() instead.
 **/
GtkWidget *
xfce_panel_image_new_from_source (const gchar *source)
{
  g_return_val_if_fail (source == NULL || *source != '\0', NULL);

  return g_object_new (XFCE_TYPE_PANEL_IMAGE,
                       "source", source, NULL);
}



/**
 * xfce_panel_image_set_from_pixbuf:
 * @image  : an #XfcePanelImage.
 * @pixbuf : (allow-none): a #GdkPixbuf, or %NULL.
 *
 * See xfce_panel_image_new_from_pixbuf() for details.
 *
 * Since: 4.8
 *
 * Deprecated: 4.18.1: Use gtk_image_set_from_pixbuf().
 **/
void
xfce_panel_image_set_from_pixbuf (XfcePanelImage *image,
                                  GdkPixbuf      *pixbuf)
{
  g_return_if_fail (XFCE_IS_PANEL_IMAGE (image));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  xfce_panel_image_clear (image);

  /* set the new pixbuf, scale it to the maximum size if needed */
  image->priv->pixbuf = xfce_panel_image_scale_pixbuf (pixbuf,
      MAX_PIXBUF_SIZE, MAX_PIXBUF_SIZE);

  gtk_widget_queue_resize (GTK_WIDGET (image));
}



/**
 * xfce_panel_image_set_from_source:
 * @image  : an #XfcePanelImage.
 * @source : (allow-none): source of the image. This can be an absolute path or
 *           an icon-name or %NULL.
 *
 * See xfce_panel_image_new_from_source() for details.
 *
 * Since: 4.8
 *
 * Deprecated: 4.18.1: Use xfce_panel_set_image_from_source() instead.
 **/
void
xfce_panel_image_set_from_source (XfcePanelImage *image,
                                  const gchar    *source)
{
  g_return_if_fail (XFCE_IS_PANEL_IMAGE (image));
  g_return_if_fail (source == NULL || *source != '\0');

  xfce_panel_image_clear (image);

  image->priv->source = g_strdup (source);

  gtk_widget_queue_resize (GTK_WIDGET (image));
}



/**
 * xfce_panel_image_set_size:
 * @image : an #XfcePanelImage.
 * @size  : a new size in pixels.
 *
 * This will force an image size, instead of looking at the allocation
 * size, see introduction for more details. You can set a @size of
 * -1 to turn this off.
 *
 * Since: 4.8
 *
 * Deprecated: 4.18.1: Use gtk_image_set_pixel_size() or set the size from the
 * #GtkImage storage type instead.
 **/
void
xfce_panel_image_set_size (XfcePanelImage *image,
                           gint            size)
{

  g_return_if_fail (XFCE_IS_PANEL_IMAGE (image));

  if (G_LIKELY (image->priv->size != size))
    {
      image->priv->size = size;
      gtk_widget_queue_resize (GTK_WIDGET (image));
    }
}



/**
 * xfce_panel_image_get_size:
 * @image : an #XfcePanelImage.
 *
 * The size of the image, set by xfce_panel_image_set_size() or -1
 * if no size is forced and the image is scaled to the allocation size.
 *
 * Returns: icon size in pixels of the image or -1.
 *
 * Since: 4.8
 *
 * Deprecated: 4.18.1: Use gtk_image_get_pixel_size() or get the size from the
 * #GtkImage storage type instead.
 **/
gint
xfce_panel_image_get_size (XfcePanelImage *image)
{
  g_return_val_if_fail (XFCE_IS_PANEL_IMAGE (image), -1);
  return image->priv->size;
}



/**
 * xfce_panel_image_clear:
 * @image : an #XfcePanelImage.
 *
 * Resets the image to be empty.
 *
 * Since: 4.8
 *
 * Deprecated: 4.18.1: Use gtk_image_clear() instead.
 **/
void
xfce_panel_image_clear (XfcePanelImage *image)
{
  XfcePanelImagePrivate *priv = XFCE_PANEL_IMAGE (image)->priv;

  g_return_if_fail (XFCE_IS_PANEL_IMAGE (image));

  if (priv->idle_load_id != 0)
    g_source_remove (priv->idle_load_id);

  if (priv->source != NULL)
    {
     g_free (priv->source);
     priv->source = NULL;
    }

  xfce_panel_image_unref_null (priv->pixbuf);
  xfce_panel_image_unref_null (priv->cache);

  /* reset values */
  priv->width = -1;
  priv->height = -1;
}



#define __XFCE_PANEL_IMAGE_C__
#include <libxfce4panel/libxfce4panel-aliasdef.c>
