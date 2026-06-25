/*
 *  Copyright (c) 2017 Viktor Odintsev <ninetls@xfce.org>
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



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4panel/libxfce4panel.h>

#include "sn-icon-box.h"
#include "sn-util.h"



static void                  sn_icon_box_icon_changed                (GtkWidget               *widget);

static void                  sn_icon_box_get_preferred_width         (GtkWidget               *widget,
                                                                      gint                    *minimum_width,
                                                                      gint                    *natural_width);

static void                  sn_icon_box_get_preferred_height        (GtkWidget               *widget,
                                                                      gint                    *minimum_height,
                                                                      gint                    *natural_height);

static void                  sn_icon_box_size_allocate               (GtkWidget               *widget,
                                                                      GtkAllocation           *allocation);

static void                  sn_icon_box_remove                      (GtkContainer            *container,
                                                                      GtkWidget               *child);

static GType                 sn_icon_box_child_type                  (GtkContainer            *container);

static void                  sn_icon_box_forall                      (GtkContainer            *container,
                                                                      gboolean                 include_internals,
                                                                      GtkCallback              callback,
                                                                      gpointer                 callback_data);


struct _SnIconBoxClass
{
  GtkContainerClass    __parent__;
};

struct _SnIconBox
{
  GtkContainer         __parent__;

  SnItem              *item;
  SnConfig            *config;

  GtkWidget           *icon;
  GtkWidget           *overlay;
};

G_DEFINE_TYPE (SnIconBox, sn_icon_box, GTK_TYPE_CONTAINER)



static void
sn_icon_box_class_init (SnIconBoxClass *klass)
{
  GtkWidgetClass    *widget_class;
  GtkContainerClass *container_class;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->get_preferred_width = sn_icon_box_get_preferred_width;
  widget_class->get_preferred_height = sn_icon_box_get_preferred_height;
  widget_class->size_allocate = sn_icon_box_size_allocate;

  container_class = GTK_CONTAINER_CLASS (klass);
  container_class->remove = sn_icon_box_remove;
  container_class->child_type = sn_icon_box_child_type;
  container_class->forall = sn_icon_box_forall;
}



static void
sn_icon_box_init (SnIconBox *box)
{
  gtk_widget_set_has_window (GTK_WIDGET (box), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (box), FALSE);
  gtk_widget_set_can_default (GTK_WIDGET (box), FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (box), 0);
  gtk_widget_set_name (GTK_WIDGET (box), "sn-button-box");

  box->item = NULL;
  box->config = NULL;

  box->icon = NULL;
  box->overlay = NULL;
}



static GType
sn_icon_box_child_type (GtkContainer *container)
{
  return GTK_TYPE_WIDGET;
}



static void
sn_icon_box_forall (GtkContainer *container,
                    gboolean      include_internals,
                    GtkCallback   callback,
                    gpointer      callback_data)
{
  SnIconBox *box = XFCE_SN_ICON_BOX (container);

  /* z-order depends on forall order */

  if (box->overlay != NULL)
    (*callback) (box->overlay, callback_data);

  if (box->icon != NULL)
    (*callback) (box->icon, callback_data);
}



static void
sn_icon_box_remove (GtkContainer *container,
                    GtkWidget    *child)
{
  SnIconBox *box;

  g_return_if_fail (XFCE_IS_SN_ICON_BOX (container));

  box = XFCE_SN_ICON_BOX (container);

  if (child == box->icon)
    {
      gtk_widget_unparent (child);
      box->icon = NULL;
    }
  else if (child == box->overlay)
    {
      gtk_widget_unparent (child);
      box->overlay = NULL;
    }

  gtk_widget_queue_resize (GTK_WIDGET (container));
}



GtkWidget *
sn_icon_box_new (SnItem   *item,
                 SnConfig *config)
{
  SnIconBox   *box = g_object_new (XFCE_TYPE_SN_ICON_BOX, NULL);
  GtkSettings *settings;

  g_return_val_if_fail (XFCE_IS_SN_CONFIG (config), NULL);

  box->item = item;
  box->config = config;

  box->icon = gtk_image_new ();
  gtk_widget_set_parent (box->icon, GTK_WIDGET (box));
  gtk_widget_show (box->icon);

  box->overlay = gtk_image_new ();
  gtk_widget_set_parent (box->overlay, GTK_WIDGET (box));
  gtk_widget_show (box->overlay);

  settings = gtk_settings_get_default ();

  sn_signal_connect_weak_swapped (config, "icons-changed",
                                  G_CALLBACK (sn_icon_box_icon_changed), box);
  sn_signal_connect_weak_swapped (config, "notify::icon-size",
                                  G_CALLBACK (sn_icon_box_icon_changed), box);
  sn_signal_connect_weak_swapped (config, "notify::symbolic-icons",
                                  G_CALLBACK (sn_icon_box_icon_changed), box);
  sn_signal_connect_weak_swapped (item, "icon-changed",
                                  G_CALLBACK (sn_icon_box_icon_changed), box);
  sn_signal_connect_weak_swapped (settings, "notify::gtk-theme-name",
                                  G_CALLBACK (sn_icon_box_icon_changed), box);
  sn_signal_connect_weak_swapped (settings, "notify::gtk-icon-theme-name",
                                  G_CALLBACK (sn_icon_box_icon_changed), box);
  sn_signal_connect_weak_swapped (box, "notify::scale-factor",
                                  G_CALLBACK (sn_icon_box_icon_changed), box);
  sn_icon_box_icon_changed (GTK_WIDGET (box));

  return GTK_WIDGET (box);
}



static GdkPixbuf *
sn_icon_box_load_icon (GtkWidget    *image,
                       GtkIconTheme *icon_theme,
                       const gchar  *icon_name,
                       gint          icon_size,
                       gboolean      prefer_symbolic)
{
  GtkIconInfo *info;
  GdkPixbuf   *pixbuf = NULL;

  info = gtk_icon_theme_lookup_icon (icon_theme, icon_name, icon_size,
                                     prefer_symbolic ? GTK_ICON_LOOKUP_FORCE_SYMBOLIC : 0);
  if (info != NULL)
    {
      pixbuf = gtk_icon_info_load_symbolic_for_context (info,
                                                        gtk_widget_get_style_context (image),
                                                        NULL, NULL);
      g_object_unref (info);
    }

  return pixbuf;
}



static void
sn_icon_box_apply_icon (GtkWidget    *image,
                        GtkIconTheme *icon_theme,
                        GtkIconTheme *icon_theme_from_path,
                        const gchar  *icon_name,
                        GdkPixbuf    *icon_pixbuf,
                        gint          icon_size,
                        gboolean      prefer_symbolic)
{
  GdkPixbuf *work_pixbuf = NULL;
  gchar     *work_icon_name = NULL;
  gint       width, height, scale_factor;
  gchar     *s1, *s2;

  gtk_image_clear (GTK_IMAGE (image));
  scale_factor = gtk_widget_get_scale_factor (image);
  icon_size *= scale_factor;

  if (icon_name != NULL)
    {
      if (icon_name[0] == '/')
        {
          /* it's a path to file */
          if (g_file_test (icon_name, G_FILE_TEST_IS_REGULAR))
            work_pixbuf = gdk_pixbuf_new_from_file (icon_name, NULL);

          if (work_pixbuf == NULL)
            {
              /* try to extract icon name from path */
              s1 = g_strrstr (icon_name, "/");
              s2 = g_strrstr (icon_name, ".");

              if (s2 != NULL)
                work_icon_name = g_strndup (&s1[1], (gint) (s2 - s1) - 1);
              else
                work_icon_name = g_strdup (&s1[1]);
            }
        }
      else
        work_icon_name = g_strdup (icon_name);

      if (work_pixbuf == NULL && icon_theme_from_path != NULL)
        work_pixbuf = sn_icon_box_load_icon (image, icon_theme_from_path,
                                             work_icon_name, icon_size, prefer_symbolic);

      if (work_pixbuf == NULL)
        work_pixbuf = sn_icon_box_load_icon (image, icon_theme, work_icon_name,
                                             icon_size, prefer_symbolic);

      g_free (work_icon_name);
    }

  if (work_pixbuf == NULL && icon_pixbuf != NULL)
    work_pixbuf = g_object_ref (icon_pixbuf);

  if (work_pixbuf != NULL)
    {
      cairo_surface_t *surface;

      width = gdk_pixbuf_get_width (work_pixbuf);
      height = gdk_pixbuf_get_height (work_pixbuf);

      if (width > icon_size || height > icon_size)
        {
          /* scale pixbuf */
          if (height < width)
            {
              height = icon_size * height / width;
              width = icon_size;
            }
          else
            {
              width = icon_size * width / height;
              height = icon_size;
            }

          icon_pixbuf = gdk_pixbuf_scale_simple (work_pixbuf, width, height, GDK_INTERP_BILINEAR);
          g_object_unref (work_pixbuf);
          work_pixbuf = icon_pixbuf;
        }

      surface = gdk_cairo_surface_create_from_pixbuf (work_pixbuf, scale_factor, NULL);
      gtk_image_set_from_surface (GTK_IMAGE (image), surface);
      cairo_surface_destroy (surface);
      g_object_unref (work_pixbuf);
    }
}



static void
sn_icon_box_icon_changed (GtkWidget *widget)
{
  SnIconBox    *box;
  const gchar  *icon_name;
  GdkPixbuf    *icon_pixbuf;
  const gchar  *overlay_icon_name;
  GdkPixbuf    *overlay_icon_pixbuf;
  const gchar  *theme_path;
  GtkIconTheme *icon_theme;
  GtkIconTheme *icon_theme_from_path = NULL;
  gint          icon_size;
  gboolean      symbolic_icons;

  box = XFCE_SN_ICON_BOX (widget);
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (widget)));

  sn_config_get_dimensions (box->config, &icon_size, NULL, NULL, NULL);

  symbolic_icons = sn_config_get_symbolic_icons (box->config);

  sn_item_get_icon (box->item, &theme_path,
                    &icon_name, &icon_pixbuf,
                    &overlay_icon_name, &overlay_icon_pixbuf);

  if (theme_path != NULL)
    {
      icon_theme_from_path = gtk_icon_theme_new ();
      gtk_icon_theme_prepend_search_path (icon_theme_from_path, theme_path);
    }

  if (icon_size > 0)
    {
      sn_icon_box_apply_icon (box->icon, icon_theme, icon_theme_from_path,
                              icon_name, icon_pixbuf, icon_size, symbolic_icons);
      sn_icon_box_apply_icon (box->overlay, icon_theme, icon_theme_from_path,
                              overlay_icon_name, overlay_icon_pixbuf, icon_size, symbolic_icons);
    }

  if (icon_theme_from_path != NULL)
    g_object_unref (icon_theme_from_path);
}



static void
sn_icon_box_get_preferred_size (GtkWidget *widget,
                                gint      *minimum_size,
                                gint      *natural_size,
                                gboolean   horizontal)
{
  SnIconBox      *box = XFCE_SN_ICON_BOX (widget);
  gint            icon_size;
  GtkRequisition  child_req;
  GdkPixbuf      *pixbuf1, *pixbuf2;

  icon_size = sn_config_get_icon_size (box->config);

  pixbuf1 = gtk_image_get_pixbuf (GTK_IMAGE (box->icon));
  pixbuf2 = gtk_image_get_pixbuf (GTK_IMAGE (box->overlay));
  if (pixbuf2 != NULL && (pixbuf1 == NULL ||
      gdk_pixbuf_get_width (pixbuf2) > gdk_pixbuf_get_width (pixbuf1) ||
      gdk_pixbuf_get_height (pixbuf2) > gdk_pixbuf_get_height (pixbuf1)))
    {
      pixbuf1 = pixbuf2;
    }

  if (box->icon != NULL)
    gtk_widget_get_preferred_size (box->icon, NULL, &child_req);

  if (box->overlay != NULL)
    gtk_widget_get_preferred_size (box->overlay, NULL, &child_req);

  if (minimum_size != NULL)
    *minimum_size = icon_size;

  if (natural_size != NULL)
    {
      *natural_size = 0;
      if (pixbuf1 != NULL)
        {
          if (horizontal)
            *natural_size = gdk_pixbuf_get_width (pixbuf1);
          else
            *natural_size = gdk_pixbuf_get_height (pixbuf1);
        }
      *natural_size = MAX (*natural_size, icon_size);
    }
}



static void
sn_icon_box_get_preferred_width (GtkWidget *widget,
                                 gint      *minimum_width,
                                 gint      *natural_width)
{
  sn_icon_box_get_preferred_size (widget, minimum_width, natural_width, TRUE);
}



static void
sn_icon_box_get_preferred_height (GtkWidget *widget,
                                  gint      *minimum_height,
                                  gint      *natural_height)
{
  sn_icon_box_get_preferred_size (widget, minimum_height, natural_height, FALSE);
}



static void
sn_icon_box_size_allocate (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
  SnIconBox *box = XFCE_SN_ICON_BOX (widget);

  gtk_widget_set_allocation (widget, allocation);

  if (box->icon != NULL)
    gtk_widget_size_allocate (box->icon, allocation);

  if (box->overlay != NULL)
    gtk_widget_size_allocate (box->overlay, allocation);
}
