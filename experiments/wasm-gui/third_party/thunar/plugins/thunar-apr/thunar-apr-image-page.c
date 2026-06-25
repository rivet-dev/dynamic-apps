/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <exo/exo.h>

#include <thunar-apr/thunar-apr-image-page.h>

#ifdef HAVE_EXIF
#include <libexif/exif-data.h>
#endif



static void thunar_apr_image_page_file_changed  (ThunarAprAbstractPage    *abstract_page,
                                                 ThunarxFileInfo          *file);



#ifdef HAVE_EXIF
static const struct
{
  const gchar *name;
  ExifTag      tag;
} TAIP_EXIF[] =
{
  { N_ ("Date Taken:"),        EXIF_TAG_DATE_TIME_ORIGINAL,  },
  { N_ ("Camera Brand:"),      EXIF_TAG_MAKE,                },
  { N_ ("Camera Model:"),      EXIF_TAG_MODEL,               },
  { N_ ("Exposure Time:"),     EXIF_TAG_EXPOSURE_TIME,       },
  { N_ ("Exposure Program:"),  EXIF_TAG_EXPOSURE_PROGRAM,    },
  { N_ ("Aperture Value:"),    EXIF_TAG_APERTURE_VALUE,      },
  { N_ ("Metering Mode:"),     EXIF_TAG_METERING_MODE,       },
  { N_ ("Flash Fired:"),       EXIF_TAG_FLASH,               },
  { N_ ("Focal Length:"),      EXIF_TAG_FOCAL_LENGTH,        },
  { N_ ("Shutter Speed:"),     EXIF_TAG_SHUTTER_SPEED_VALUE, },
  { N_ ("ISO Speed Ratings:"), EXIF_TAG_ISO_SPEED_RATINGS,   },
  { N_ ("Software:"),          EXIF_TAG_SOFTWARE,            },
  { N_ ("Description:"),       EXIF_TAG_IMAGE_DESCRIPTION,   },
  { N_ ("Comment:"),           EXIF_TAG_USER_COMMENT,        },
};
#endif



struct _ThunarAprImagePageClass
{
  ThunarAprAbstractPageClass __parent__;
};

struct _ThunarAprImagePage
{
  ThunarAprAbstractPage __parent__;
  GtkWidget            *type_label;
  GtkWidget            *dimensions_label;

#ifdef HAVE_EXIF
  GtkWidget            *exif_labels[G_N_ELEMENTS (TAIP_EXIF)];
#endif
};



THUNARX_DEFINE_TYPE (ThunarAprImagePage,
                     thunar_apr_image_page,
                     THUNAR_APR_TYPE_ABSTRACT_PAGE);



static void
thunar_apr_image_page_class_init (ThunarAprImagePageClass *klass)
{
  ThunarAprAbstractPageClass *thunarapr_abstract_page_class;

  thunarapr_abstract_page_class = THUNAR_APR_ABSTRACT_PAGE_CLASS (klass);
  thunarapr_abstract_page_class->file_changed = thunar_apr_image_page_file_changed;
}



static void
thunar_apr_image_page_init (ThunarAprImagePage *image_page)
{
  AtkRelationSet *relations;
  PangoAttribute *attribute;
  PangoAttrList  *attr_list;
  AtkRelation    *relation;
  AtkObject      *object;
  GtkWidget      *label;
  GtkWidget      *grid;
#ifdef HAVE_EXIF
  GtkWidget      *spacer;
  guint           n;
#endif

  gtk_container_set_border_width (GTK_CONTAINER (image_page), 12);
  thunarx_property_page_set_label (THUNARX_PROPERTY_PAGE (image_page), _("Image"));

  /* allocate shared bold Pango attributes */
  attr_list = pango_attr_list_new ();
  attribute = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
  attribute->start_index = 0;
  attribute->end_index = -1;
  pango_attr_list_insert (attr_list, attribute);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_container_add (GTK_CONTAINER (image_page), grid);
  gtk_widget_show (grid);

  label = gtk_label_new (_("Image Type:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), attr_list);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_widget_show (label);

  image_page->type_label = gtk_label_new ("");
  gtk_label_set_selectable (GTK_LABEL (image_page->type_label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (image_page->type_label), 0.0f);
  gtk_label_set_ellipsize (GTK_LABEL (image_page->type_label), PANGO_ELLIPSIZE_END);
  gtk_widget_set_hexpand (image_page->type_label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), image_page->type_label, 1, 0, 1, 1);
  gtk_widget_show (image_page->type_label);

  /* set Atk label relation for the label */
  object = gtk_widget_get_accessible (image_page->type_label);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  label = gtk_label_new (_("Image Size:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), attr_list);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
  gtk_widget_show (label);

  image_page->dimensions_label = gtk_label_new ("");
  gtk_label_set_selectable (GTK_LABEL (image_page->dimensions_label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (image_page->dimensions_label), 0.0f);
  gtk_label_set_ellipsize (GTK_LABEL (image_page->dimensions_label), PANGO_ELLIPSIZE_END);
  gtk_widget_set_hexpand (image_page->dimensions_label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), image_page->dimensions_label, 1, 1, 1, 1);
  gtk_widget_show (image_page->dimensions_label);

  /* set Atk label relation for the label */
  object = gtk_widget_get_accessible (image_page->dimensions_label);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

#ifdef HAVE_EXIF
  /* some spacing between the General info and the Exif info */
  spacer = g_object_new (GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "height-request", 6, NULL);
  gtk_grid_attach (GTK_GRID (grid), spacer, 0, 2, 2, 1);
  gtk_widget_show (spacer);

  /* add labels for the Exif info */
  for (n = 0; n < G_N_ELEMENTS (TAIP_EXIF); ++n)
    {
      label = gtk_label_new (_(TAIP_EXIF[n].name));
      gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
      gtk_label_set_attributes (GTK_LABEL (label), attr_list);
      gtk_grid_attach (GTK_GRID (grid), label, 0, n + 3, 1, 1);
      gtk_widget_show (label);

      image_page->exif_labels[n] = gtk_label_new ("");
      gtk_label_set_selectable (GTK_LABEL (image_page->exif_labels[n]), TRUE);
      gtk_label_set_xalign (GTK_LABEL (image_page->exif_labels[n]), 0.0f);
      gtk_label_set_ellipsize (GTK_LABEL (image_page->exif_labels[n]), PANGO_ELLIPSIZE_END);
      gtk_widget_set_hexpand (image_page->exif_labels[n], TRUE);
      gtk_grid_attach (GTK_GRID (grid), image_page->exif_labels[n], 1, n + 3, 1, 1);
      gtk_widget_show (image_page->exif_labels[n]);

      g_object_bind_property (G_OBJECT (image_page->exif_labels[n]), "visible", G_OBJECT (label), "visible", G_BINDING_SYNC_CREATE);

      /* set Atk label relation for the label */
      object = gtk_widget_get_accessible (image_page->exif_labels[n]);
      relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
      relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
      atk_relation_set_add (relations, relation);
      g_object_unref (G_OBJECT (relation));
    }
#endif

  /* release shared bold Pango attributes */
  pango_attr_list_unref (attr_list);
}



static void
thunar_apr_image_page_file_changed (ThunarAprAbstractPage *abstract_page,
                                    ThunarxFileInfo       *file)
{
  ThunarAprImagePage *image_page = THUNAR_APR_IMAGE_PAGE (abstract_page);
  GdkPixbufFormat    *format;
  gchar              *filename;
  gchar              *text;
  gchar              *uri;
  gint                height;
  gint                width;
#ifdef HAVE_EXIF
  ExifEntry          *exif_entry;
  ExifData           *exif_data;
  gchar               exif_buffer[1024];
  guint               n;
#endif

  /* determine the URI for the file */
  uri = thunarx_file_info_get_uri (file);
  if (G_UNLIKELY (uri == NULL))
    return;

  /* determine the local path of the file */
  filename = g_filename_from_uri (uri, NULL, NULL);
  if (G_LIKELY (filename != NULL))
    {
      /* determine the pixbuf format */
      format = gdk_pixbuf_get_file_info (filename, &width, &height);
      if (G_LIKELY (format != NULL))
        {
          /* update the "Image Type" label */
          text = g_strdup_printf ("%s (%s)", gdk_pixbuf_format_get_name (format), gdk_pixbuf_format_get_description (format));
          gtk_label_set_text (GTK_LABEL (image_page->type_label), text);
          g_free (text);

          /* update the "Image Size" label */
          text = g_strdup_printf (ngettext ("%dx%d pixel", "%dx%d pixels", width + height), width, height);
          gtk_label_set_text (GTK_LABEL (image_page->dimensions_label), text);
          g_free (text);

#ifdef HAVE_EXIF
          /* hide all Exif labels (will be shown again if data is available) */
          for (n = 0; n < G_N_ELEMENTS (TAIP_EXIF); ++n)
            gtk_widget_hide (image_page->exif_labels[n]);

          /* try to load the Exif data for the file */
          exif_data = exif_data_new_from_file (filename);
          if (G_LIKELY (exif_data != NULL))
            {
              /* update all Exif labels */
              for (n = 0; n < G_N_ELEMENTS (TAIP_EXIF); ++n)
                {
                  /* lookup the entry for the tag */
                  exif_entry = exif_data_get_entry (exif_data, TAIP_EXIF[n].tag);
                  if (G_LIKELY (exif_entry != NULL))
                    {
                      /* determine the value */
                      if (exif_entry_get_value (exif_entry, exif_buffer, sizeof (exif_buffer)) != NULL)
                        {
                          /* setup the label text */
                          text = (g_utf8_validate (exif_buffer, -1, NULL)) ? g_strdup (exif_buffer) : g_filename_display_name (exif_buffer);
                          gtk_label_set_text (GTK_LABEL (image_page->exif_labels[n]), text);
                          g_free (text);

                          /* show the label */
                          gtk_widget_show (image_page->exif_labels[n]);
                        }
                    }
                }

              /* cleanup */
              exif_data_free (exif_data);
            }
#endif
        }
      else
        {
          /* tell the user that we're unable to determine the file info */
          gtk_label_set_text (GTK_LABEL (image_page->type_label), _("Unknown"));
          gtk_label_set_text (GTK_LABEL (image_page->dimensions_label), _("Unknown"));

#ifdef HAVE_EXIF
          /* hide all Exif labels */
          for (n = 0; n < G_N_ELEMENTS (TAIP_EXIF); ++n)
            gtk_widget_hide (image_page->exif_labels[n]);
#endif
        }
    }

  /* cleanup */
  g_free (filename);
  g_free (uri);
}




