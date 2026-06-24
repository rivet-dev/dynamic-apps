/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <exo/exo-gdk-pixbuf-extensions.h>
#include <exo/exo-private.h>
#include <exo/exo-thumbnail-preview.h>
#include <exo/exo-thumbnail.h>
#include <exo/exo-utils.h>
#include <exo/exo-alias.h>
#include <exo/exo-string.h>
#include <libxfce4util/libxfce4util.h>



static void exo_thumbnail_preview_style_set (GtkWidget           *ebox,
                                             GtkStyle            *previous_style,
                                             ExoThumbnailPreview *thumbnail_preview);



struct _ExoThumbnailPreviewClass
{
  GtkFrameClass __parent__;
};

struct _ExoThumbnailPreview
{
  GtkFrame   __parent__;
  GtkWidget *image;
  GtkWidget *name_label;
  GtkWidget *size_label;
};



G_DEFINE_TYPE (ExoThumbnailPreview, exo_thumbnail_preview, GTK_TYPE_FRAME)



static void
exo_thumbnail_preview_class_init (ExoThumbnailPreviewClass *klass)
{
}



static void
exo_thumbnail_preview_init (ExoThumbnailPreview *thumbnail_preview)
{
  GtkWidget *ebox;
  GtkWidget *vbox;
  GtkWidget *box;

  /* initialize the library's i18n support */
  _exo_i18n_init ();

  gtk_frame_set_shadow_type (GTK_FRAME (thumbnail_preview), GTK_SHADOW_IN);
  gtk_frame_set_label (GTK_FRAME (thumbnail_preview), _("Preview"));
  gtk_widget_set_sensitive (GTK_WIDGET (thumbnail_preview), FALSE);

  ebox = gtk_event_box_new ();
  g_signal_connect (G_OBJECT (ebox), "style-set", G_CALLBACK (exo_thumbnail_preview_style_set), thumbnail_preview);
  gtk_container_add (GTK_CONTAINER (thumbnail_preview), ebox);
  gtk_widget_show (ebox);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (ebox), vbox);
  gtk_widget_show (vbox);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  gtk_container_set_border_width (GTK_CONTAINER (box), 2);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);
  gtk_widget_show (box);

  thumbnail_preview->image = gtk_image_new_from_icon_name ("image-missing", GTK_ICON_SIZE_DIALOG);
  gtk_widget_set_size_request (thumbnail_preview->image, EXO_THUMBNAIL_SIZE_NORMAL + 2 * 12, EXO_THUMBNAIL_SIZE_NORMAL + 2 * 12);
  gtk_image_set_pixel_size (GTK_IMAGE (thumbnail_preview->image), EXO_THUMBNAIL_SIZE_NORMAL / 2);
  gtk_box_pack_start (GTK_BOX (box), thumbnail_preview->image, FALSE, FALSE, 0);
  gtk_widget_show (thumbnail_preview->image);

  thumbnail_preview->name_label = gtk_label_new (_("No file selected"));
  gtk_label_set_justify (GTK_LABEL (thumbnail_preview->name_label), GTK_JUSTIFY_CENTER);
  gtk_label_set_ellipsize (GTK_LABEL (thumbnail_preview->name_label), PANGO_ELLIPSIZE_MIDDLE);
  gtk_box_pack_start (GTK_BOX (box), thumbnail_preview->name_label, FALSE, FALSE, 0);
  gtk_widget_show (thumbnail_preview->name_label);

  thumbnail_preview->size_label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (box), thumbnail_preview->size_label, FALSE, FALSE, 0);
  gtk_widget_show (thumbnail_preview->size_label);
}



static void
exo_thumbnail_preview_style_set (GtkWidget           *ebox,
                                 GtkStyle            *previous_style,
                                 ExoThumbnailPreview *thumbnail_preview)
{
  _exo_return_if_fail (EXO_IS_THUMBNAIL_PREVIEW (thumbnail_preview));
  _exo_return_if_fail (GTK_IS_EVENT_BOX (ebox));

  /* check if the ebox is already realized */
  if (gtk_widget_get_realized (ebox))
    {
      /* set background color (using the base color) */
      g_signal_handlers_block_by_func (G_OBJECT (ebox), exo_thumbnail_preview_style_set, thumbnail_preview);
      g_signal_handlers_unblock_by_func (G_OBJECT (ebox), exo_thumbnail_preview_style_set, thumbnail_preview);
    }
}



/**
 * _exo_thumbnail_preview_new:
 *
 * Allocates a new #ExoThumbnailPreview instance.
 *
 * Returns: the newly allocated #ExoThumbnailPreview.
 **/
GtkWidget*
_exo_thumbnail_preview_new (void)
{
  return g_object_new (EXO_TYPE_THUMBNAIL_PREVIEW, NULL);
}



static inline GdkPixbuf*
thumbnail_add_frame (GdkPixbuf *thumbnail)
{
  const guchar *pixels;
  GdkPixbuf    *frame;
  gint          rowstride;
  gint          height;
  gint          width;
  gint          n;

  /* determine the thumbnail dimensions */
  width = gdk_pixbuf_get_width (thumbnail);
  height = gdk_pixbuf_get_height (thumbnail);

  /* don't add frames to small thumbnails */
  if (width < EXO_THUMBNAIL_SIZE_NORMAL && height < EXO_THUMBNAIL_SIZE_NORMAL)
    goto none;

  /* always add a frame to thumbnails w/o alpha channel */
  if (gdk_pixbuf_get_has_alpha (thumbnail))
    {
      /* get a pointer to the thumbnail data */
      pixels = gdk_pixbuf_get_pixels (thumbnail);

      /* check if we have a transparent pixel on the first row */
      for (n = width * 4; n > 0; n -= 4)
        if (pixels[n - 1] < 255u)
          goto none;

      /* determine the rowstride */
      rowstride = gdk_pixbuf_get_rowstride (thumbnail);

      /* skip the first row */
      pixels += rowstride;

      /* check if we have a transparent pixel in the first or last column */
      for (n = height - 2; n > 0; --n, pixels += rowstride)
        if (pixels[3] < 255u || pixels[width * 4 - 1] < 255u)
          goto none;

      /* check if we have a transparent pixel on the last row */
      for (n = width * 4; n > 0; n -= 4)
        if (pixels[n - 1] < 255u)
          goto none;
    }

  /* try to load the frame image */
  frame = gdk_pixbuf_new_from_file (DATADIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "exo"
                                    G_DIR_SEPARATOR_S "exo-thumbnail-frame.png", NULL);
  if (G_LIKELY (frame != NULL))
    {
      /* add a frame to the thumbnail */
      thumbnail = exo_gdk_pixbuf_frame (thumbnail, frame, 4, 3, 5, 6);
      g_object_unref (G_OBJECT (frame));
    }
  else
    {
none: /* just add a ref on the thumbnail */
      g_object_ref (G_OBJECT (thumbnail));
    }

  return thumbnail;
}



/**
 * _exo_thumbnail_preview_set_uri:
 * @thumbnail_preview : an #ExoThumbnailPreview.
 * @uri               : the new URI for which to show a preview or %NULL.
 *
 * Updates the @thumbnail_preview to display a preview of the specified @uri.
 **/
void
_exo_thumbnail_preview_set_uri (ExoThumbnailPreview *thumbnail_preview,
                                const gchar         *uri)
{
  struct stat      statb;
  GdkPixbuf       *thumbnail_framed;
  GdkPixbuf       *thumbnail;
  cairo_surface_t *surface;
  gchar           *icon_name = NULL;
  gchar           *size_name = NULL;
  gchar           *displayname;
  gchar           *filename;
  gchar           *slash;
  gint             scale_factor;
  ExoThumbnailSize thumbnail_size;

  _exo_return_if_fail (EXO_IS_THUMBNAIL_PREVIEW (thumbnail_preview));

  /* check if we have an URI to preview */
  if (G_UNLIKELY (uri == NULL))
    {
      /* the preview widget is insensitive if we don't have an URI */
      gtk_widget_set_sensitive (GTK_WIDGET (thumbnail_preview), FALSE);
      gtk_image_set_from_icon_name (GTK_IMAGE (thumbnail_preview->image), "image-missing", GTK_ICON_SIZE_DIALOG);
      gtk_label_set_text (GTK_LABEL (thumbnail_preview->name_label), _("No file selected"));
    }
  else
    {
      /* make the preview widget appear sensitive */
      gtk_widget_set_sensitive (GTK_WIDGET (thumbnail_preview), TRUE);

      /* check if we have a local file here */
      filename = g_filename_from_uri (uri, NULL, NULL);
      if (G_LIKELY (filename != NULL))
        {
          /* try to stat the file */
          if (stat (filename, &statb) == 0)
            {
              /* icon and size label depends on the mode */
              if (S_ISBLK (statb.st_mode))
                {
                  icon_name = g_strdup ("drive-harddisk");
                  size_name = g_strdup (_("Block Device"));
                }
              else if (S_ISCHR (statb.st_mode))
                {
                  icon_name = g_strdup ("drive-harddisk");
                  size_name = g_strdup (_("Character Device"));
                }
              else if (S_ISDIR (statb.st_mode))
                {
                  icon_name = g_strdup ("folder");
                  size_name = g_strdup (_("Folder"));
                }
              else if (S_ISFIFO (statb.st_mode))
                {
                  icon_name = g_strdup ("drive-harddisk");
                  size_name = g_strdup (_("FIFO"));
                }
              else if (S_ISSOCK (statb.st_mode))
                {
                  icon_name = g_strdup ("drive-harddisk");
                  size_name = g_strdup (_("Socket"));
                }
              else if (S_ISREG (statb.st_mode))
                {
                  if (G_UNLIKELY ((gulong) statb.st_size > 1024ul * 1024ul * 1024ul))
                    size_name = g_strdup_printf ("%0.1f GB", statb.st_size / (1024.0 * 1024.0 * 1024.0));
                  else if ((gulong) statb.st_size > 1024ul * 1024ul)
                    size_name = g_strdup_printf ("%0.1f MB", statb.st_size / (1024.0 * 1024.0));
                  else if ((gulong) statb.st_size > 1024ul)
                    size_name = g_strdup_printf ("%0.1f kB", statb.st_size / 1024.0);
                  else
                    size_name = g_strdup_printf ("%lu B", (gulong) statb.st_size);
                }
            }

          /* determine the basename from the filename */
          displayname = g_filename_display_basename (filename);
        }
      else
        {
          /* determine the basename from the URI */
          slash = strrchr (uri, '/');
          if (G_LIKELY (!xfce_str_is_empty (slash)))
            displayname = g_filename_display_name (slash + 1);
          else
            displayname = g_filename_display_name (uri);
        }

      /* check if we have an icon-name */
      if (G_UNLIKELY (icon_name != NULL))
        {
          /* setup the named icon then */
          gtk_image_set_from_icon_name (GTK_IMAGE (thumbnail_preview->image), icon_name, GTK_ICON_SIZE_DIALOG);
          g_free (icon_name);
        }
      else
        {
          /* try to load a thumbnail for the URI */
          scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (thumbnail_preview));
          thumbnail_size = scale_factor > 1 ? EXO_THUMBNAIL_SIZE_LARGE : EXO_THUMBNAIL_SIZE_NORMAL;

          thumbnail = _exo_thumbnail_get_for_uri (uri, thumbnail_size, NULL);
          if (thumbnail == NULL && G_LIKELY (filename != NULL))
            {
              /* but we can try to generate a thumbnail */
              thumbnail = _exo_thumbnail_get_for_file (filename, thumbnail_size, NULL);
            }

          /* check if we have a thumbnail */
          if (G_LIKELY (thumbnail != NULL))
            {
              /* setup the thumbnail for the image (using a frame if possible) */
              thumbnail_framed = thumbnail_add_frame (thumbnail);
              surface = gdk_cairo_surface_create_from_pixbuf (thumbnail_framed, scale_factor, gtk_widget_get_window (GTK_WIDGET (thumbnail_preview)));
              gtk_image_set_from_surface (GTK_IMAGE (thumbnail_preview->image), surface);
              g_object_unref (G_OBJECT (thumbnail_framed));
              g_object_unref (G_OBJECT (thumbnail));
              cairo_surface_destroy (surface);
            }
          else
            {
              /* no thumbnail, cannot display anything useful then */
              gtk_image_set_from_icon_name (GTK_IMAGE (thumbnail_preview->image), "image-missing", GTK_ICON_SIZE_DIALOG);
            }
        }

      /* setup the name label */
      gtk_label_set_text (GTK_LABEL (thumbnail_preview->name_label), displayname);

      /* cleanup */
      g_free (displayname);
      g_free (filename);
    }

  /* setup the new size label */
  gtk_label_set_text (GTK_LABEL (thumbnail_preview->size_label), (size_name != NULL) ? size_name : "");
  g_free (size_name);
}



#define __EXO_THUMBNAIL_PREVIEW_C__
#include <exo/exo-aliasdef.c>
