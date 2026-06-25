/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-file-monitor.h>
#include <thunar/thunar-image.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-private.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
};



static void thunar_image_finalize             (GObject           *object);
static void thunar_image_get_property         (GObject           *object,
                                               guint              prop_id,
                                               GValue            *value,
                                               GParamSpec        *pspec);
static void thunar_image_set_property         (GObject           *object,
                                               guint              prop_id,
                                               const GValue      *value,
                                               GParamSpec        *pspec);
static void thunar_image_scale_changed        (GObject           *object,
                                               GParamSpec        *pspec,
                                               gpointer           user_data);
static void thunar_image_file_changed         (ThunarFileMonitor *monitor,
                                               ThunarFile        *file,
                                               ThunarImage       *image);
static void thunar_image_update               (ThunarImage       *image);



struct _ThunarImageClass
{
  GtkImageClass __parent__;
};

struct _ThunarImage
{
  GtkImage __parent__;

  ThunarImagePrivate *priv;
};

struct _ThunarImagePrivate
{
  ThunarFileMonitor *monitor;
  ThunarFile        *file;
};



G_DEFINE_TYPE_WITH_PRIVATE (ThunarImage, thunar_image, GTK_TYPE_IMAGE);



static void
thunar_image_class_init (ThunarImageClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_image_finalize;
  gobject_class->get_property = thunar_image_get_property;
  gobject_class->set_property = thunar_image_set_property;

  g_object_class_install_property (gobject_class, PROP_FILE,
                                   g_param_spec_object ("file",
                                                        "file",
                                                        "file",
                                                        THUNAR_TYPE_FILE,
                                                        G_PARAM_READWRITE));
}



static void
thunar_image_init (ThunarImage *image)
{
  image->priv = thunar_image_get_instance_private (image);
  image->priv->file = NULL;

  image->priv->monitor = thunar_file_monitor_get_default ();
  g_signal_connect (image->priv->monitor, "file-changed",
                    G_CALLBACK (thunar_image_file_changed), image);

  g_signal_connect (G_OBJECT (image), "notify::scale-factor", G_CALLBACK (thunar_image_scale_changed), NULL);
}



static void
thunar_image_finalize (GObject *object)
{
  ThunarImage *image = THUNAR_IMAGE (object);

  g_signal_handlers_disconnect_by_func (image->priv->monitor,
                                        thunar_image_file_changed, image);
  g_object_unref (image->priv->monitor);

  thunar_image_set_file (image, NULL);

  (*G_OBJECT_CLASS (thunar_image_parent_class)->finalize) (object);
}



static void
thunar_image_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  ThunarImage *image = THUNAR_IMAGE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, image->priv->file);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_image_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ThunarImage *image = THUNAR_IMAGE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      thunar_image_set_file (image, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_image_scale_changed (GObject    *object,
                            GParamSpec *pspec,
                            gpointer    user_data)
{
    thunar_image_update (THUNAR_IMAGE (object));
}



static void
thunar_image_update (ThunarImage *image)
{
  ThunarIconFactory *icon_factory;
  GtkIconTheme      *icon_theme;
  GdkPixbuf         *icon;
  GdkScreen         *screen;
  gint               scale_factor;
  cairo_surface_t   *surface;

  _thunar_return_if_fail (THUNAR_IS_IMAGE (image));

  if (THUNAR_IS_FILE (image->priv->file))
    {
      screen = gtk_widget_get_screen (GTK_WIDGET (image));
      icon_theme = gtk_icon_theme_get_for_screen (screen);
      icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

      scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (image));
      icon = thunar_icon_factory_load_file_icon (icon_factory, image->priv->file,
                                                 THUNAR_FILE_ICON_STATE_DEFAULT, 48 * scale_factor);

      surface = gdk_cairo_surface_create_from_pixbuf (icon, scale_factor, gtk_widget_get_window (GTK_WIDGET (image)));
      gtk_image_set_from_surface (GTK_IMAGE (image), surface);

      g_object_unref (icon_factory);
      g_object_unref (icon);
      cairo_surface_destroy (surface);
    }
}



static void
thunar_image_file_changed (ThunarFileMonitor *monitor,
                           ThunarFile        *file,
                           ThunarImage       *image)
{
  _thunar_return_if_fail (THUNAR_IS_FILE_MONITOR (monitor));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_IS_IMAGE (image));

  if (file == image->priv->file)
    thunar_image_update (image);
}



GtkWidget *
thunar_image_new (void)
{
  return g_object_new (THUNAR_TYPE_IMAGE, NULL);
}



void
thunar_image_set_file (ThunarImage *image,
                       ThunarFile  *file)
{
  _thunar_return_if_fail (THUNAR_IS_IMAGE (image));

  if (image->priv->file != NULL)
    {
      if (image->priv->file == file)
        return;

      g_object_unref (image->priv->file);
    }

  if (file != NULL)
    image->priv->file = g_object_ref (file);
  else
    image->priv->file = NULL;

  thunar_image_update (image);

  g_object_notify (G_OBJECT (image), "file");
}
