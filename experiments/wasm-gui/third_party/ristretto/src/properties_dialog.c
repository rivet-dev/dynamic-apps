/*
 *  Copyright (c) Stephan Arts 2006-2012 <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "util.h"
#include "file.h"
#include "properties_dialog.h"
#include "thumbnailer.h"



#define EXIF_DATA_BUFFER_SIZE 40

enum
{
    EXIF_PROP_DATE_TIME = 0,
    EXIF_PROP_MODEL,
    EXIF_PROP_MAKE,
    EXIF_PROP_APERATURE,
    EXIF_PROP_COUNT
};

enum
{
    PROP_0,
    PROP_FILE,
};



static void
rstto_properties_dialog_finalize (GObject *object);
static void
rstto_properties_dialog_set_property (GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec);
static void
rstto_properties_dialog_get_property (GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec);
static void
properties_dialog_set_file (RsttoPropertiesDialog *dialog,
                            RsttoFile *file);



struct _RsttoPropertiesDialogPrivate
{
    RsttoFile *file;
    RsttoSettings *settings;

    GtkWidget *notebook;
    GtkWidget *image_table;
    GtkWidget *image_label;

    GtkWidget *image_thumbnail;
    GtkWidget *name_entry;
    GtkWidget *mime_content_label;
    GtkWidget *modified_content_label;
    GtkWidget *accessed_content_label;
    GtkWidget *size_content_label;
};



G_DEFINE_TYPE_WITH_PRIVATE (RsttoPropertiesDialog, rstto_properties_dialog, GTK_TYPE_DIALOG)



static void
rstto_properties_dialog_init (RsttoPropertiesDialog *dialog)
{
    GtkWidget *vbox;
    GtkWidget *grid;
    /* General tab */
    GtkWidget *general_label;
    GtkWidget *name_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *name_label = gtk_label_new (NULL);
    GtkWidget *mime_label = gtk_label_new (NULL);

    GtkWidget *modified_label = gtk_label_new (NULL);
    GtkWidget *accessed_label = gtk_label_new (NULL);

    GtkWidget *size_label = gtk_label_new (NULL);

    GtkWidget *button = xfce_gtk_button_new_mixed ("window-close", _("_Close"));

    dialog->priv = rstto_properties_dialog_get_instance_private (dialog);

    dialog->priv->settings = rstto_settings_new ();
    dialog->priv->image_thumbnail = gtk_image_new ();
    dialog->priv->name_entry = gtk_entry_new ();
    dialog->priv->mime_content_label = gtk_label_new (NULL);
    dialog->priv->modified_content_label = gtk_label_new (NULL);
    dialog->priv->accessed_content_label = gtk_label_new (NULL);
    dialog->priv->size_content_label = gtk_label_new (NULL);

    gtk_label_set_xalign (GTK_LABEL (dialog->priv->mime_content_label), 0.0);
    gtk_label_set_yalign (GTK_LABEL (dialog->priv->mime_content_label), 0.5);
    gtk_label_set_xalign (GTK_LABEL (dialog->priv->modified_content_label), 0.0);
    gtk_label_set_yalign (GTK_LABEL (dialog->priv->modified_content_label), 0.5);
    gtk_label_set_xalign (GTK_LABEL (dialog->priv->accessed_content_label), 0.0);
    gtk_label_set_yalign (GTK_LABEL (dialog->priv->accessed_content_label), 0.5);
    gtk_label_set_xalign (GTK_LABEL (dialog->priv->size_content_label), 0.0);
    gtk_label_set_yalign (GTK_LABEL (dialog->priv->size_content_label), 0.5);

    vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    dialog->priv->notebook = gtk_notebook_new ();

    grid = gtk_grid_new ();
    gtk_grid_set_column_spacing (GTK_GRID (grid), 4);
    gtk_grid_set_row_spacing (GTK_GRID (grid), 4);
    gtk_box_pack_start (
            GTK_BOX (name_hbox),
            dialog->priv->image_thumbnail,
            FALSE, TRUE, 3);
    gtk_box_pack_end (
            GTK_BOX (name_hbox),
            name_label,
            TRUE, TRUE, 0);
    gtk_label_set_markup (GTK_LABEL (name_label), _("<b>Name:</b>"));
    gtk_label_set_markup (GTK_LABEL (mime_label), _("<b>Kind:</b>"));
    gtk_label_set_markup (GTK_LABEL (modified_label), _("<b>Modified:</b>"));
    gtk_label_set_markup (GTK_LABEL (accessed_label), _("<b>Accessed:</b>"));
    gtk_label_set_markup (GTK_LABEL (size_label), _("<b>Size:</b>"));

    gtk_label_set_xalign (GTK_LABEL (name_label), 1.0);
    gtk_label_set_yalign (GTK_LABEL (name_label), 0.5);
    gtk_label_set_xalign (GTK_LABEL (mime_label), 1.0);
    gtk_label_set_yalign (GTK_LABEL (mime_label), 0.5);
    gtk_label_set_xalign (GTK_LABEL (modified_label), 1.0);
    gtk_label_set_yalign (GTK_LABEL (modified_label), 0.5);
    gtk_label_set_xalign (GTK_LABEL (accessed_label), 1.0);
    gtk_label_set_yalign (GTK_LABEL (accessed_label), 0.5);
    gtk_label_set_xalign (GTK_LABEL (size_label), 1.0);
    gtk_label_set_yalign (GTK_LABEL (size_label), 0.5);

    gtk_grid_attach (GTK_GRID (grid), name_hbox,                            0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), dialog->priv->name_entry,             1, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), mime_label,                           0, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), dialog->priv->mime_content_label,     1, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), modified_label,                       0, 2, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), dialog->priv->modified_content_label, 1, 2, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), accessed_label,                       0, 3, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), dialog->priv->accessed_content_label, 1, 3, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), size_label,                           0, 4, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), dialog->priv->size_content_label,     1, 4, 1, 1);

    general_label = gtk_label_new (_("General"));
    gtk_notebook_append_page (GTK_NOTEBOOK (dialog->priv->notebook), grid, general_label);

    dialog->priv->image_table = gtk_grid_new ();
    gtk_grid_set_column_spacing (GTK_GRID (dialog->priv->image_table), 4);
    gtk_grid_set_row_spacing (GTK_GRID (dialog->priv->image_table), 4);
    dialog->priv->image_label = gtk_label_new (_("Image"));

    gtk_notebook_append_page (
            GTK_NOTEBOOK (dialog->priv->notebook),
            dialog->priv->image_table,
            dialog->priv->image_label);

    gtk_box_pack_start (GTK_BOX (vbox), dialog->priv->notebook, TRUE, TRUE, 3);

    gtk_widget_show_all (vbox);

    /* Window should not be resizable */
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
    gtk_widget_show (button);
}

static void
rstto_properties_dialog_class_init (RsttoPropertiesDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = rstto_properties_dialog_finalize;
    object_class->set_property = rstto_properties_dialog_set_property;
    object_class->get_property = rstto_properties_dialog_get_property;

    g_object_class_install_property (object_class,
                                     PROP_FILE,
                                     g_param_spec_object ("file",
                                                          "",
                                                          "",
                                                          RSTTO_TYPE_FILE,
                                                          G_PARAM_READWRITE));
}

static void
rstto_properties_dialog_finalize (GObject *object)
{
    RsttoPropertiesDialog *dialog = RSTTO_PROPERTIES_DIALOG (object);

    if (dialog->priv->settings)
    {
        g_object_unref (dialog->priv->settings);
        dialog->priv->settings = NULL;
    }

    G_OBJECT_CLASS (rstto_properties_dialog_parent_class)->finalize (object);
}


static void
rstto_properties_dialog_set_property (
        GObject      *object,
        guint         property_id,
        const GValue *value,
        GParamSpec   *pspec)
{
    RsttoPropertiesDialog *dialog = RSTTO_PROPERTIES_DIALOG (object);

    switch (property_id)
    {
        case PROP_FILE:
            properties_dialog_set_file (dialog, g_value_get_object (value));
            break;
        default:
            break;
    }

}

static void
rstto_properties_dialog_get_property (
        GObject    *object,
        guint       property_id,
        GValue     *value,
        GParamSpec *pspec)
{
}

static gboolean
properties_dialog_set_icon (
        RsttoThumbnailer *thumbnailer,
        RsttoFile *file,
        RsttoPropertiesDialog *dialog)
{
    const GdkPixbuf *pixbuf;

    if (file != dialog->priv->file)
        return FALSE;

    pixbuf = rstto_file_get_thumbnail (file, RSTTO_THUMBNAIL_SIZE_VERY_LARGE);
    if (pixbuf != NULL)
    {
        gint scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (dialog));
        cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale_factor, NULL);
        gtk_image_set_from_surface (GTK_IMAGE (dialog->priv->image_thumbnail), surface);
        cairo_surface_destroy (surface);

        return TRUE;
    }

    return FALSE;
}

static void
properties_dialog_set_file (
        RsttoPropertiesDialog *dialog,
        RsttoFile *file)
{
    gchar  *description;
    time_t  mtime;
    time_t  atime;
    gchar   buf[20];
    guint64 size;

    ExifEntry   *exif_entry = NULL;
    ExifIfd      exif_ifd;
    const gchar *exif_title = NULL;
    gchar        exif_data[EXIF_DATA_BUFFER_SIZE];

    gchar       *label_string;
    GtkWidget   *exif_label;
    GtkWidget   *exif_content_label;
    gint i;

    GList *children = NULL;
    GList *child_iter = NULL;

    GFile  *g_file;
    GFileInfo *file_info = NULL;

    dialog->priv->file = file;

    if (dialog->priv->file)
    {
        if (! properties_dialog_set_icon (NULL, file, dialog))
        {
            RsttoThumbnailer *thumbnailer = rstto_thumbnailer_new ();
            g_signal_connect_object (thumbnailer, "ready",
                                     G_CALLBACK (properties_dialog_set_icon), dialog, 0);
            g_object_unref (thumbnailer);
        }

        g_file = rstto_file_get_file (file);
        file_info = g_file_query_info (
                g_file,
                "standard::content-type,standard::size,time::modified,time::access",
                0,
                NULL,
                NULL);
        description = g_content_type_get_description (
            g_file_info_get_attribute_string (file_info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE));
        mtime = g_file_info_get_attribute_uint64 (file_info, "time::modified");
        atime = g_file_info_get_attribute_uint64 (file_info, "time::access");
        size = g_file_info_get_attribute_uint64 (file_info, "standard::size");
        strftime (
                buf,
                20,
                "%Y/%m/%d",
                localtime (&mtime));
        gtk_label_set_text (
                GTK_LABEL (dialog->priv->modified_content_label),
                buf);
        strftime (
                buf,
                20,
                "%Y/%m/%d",
                localtime (&atime));
        gtk_label_set_text (
                GTK_LABEL (dialog->priv->accessed_content_label),
                buf);

        g_snprintf (
                buf,
                20,
                "%"G_GUINT64_FORMAT" bytes",
                size);
        gtk_label_set_text (
                GTK_LABEL (dialog->priv->size_content_label),
                buf);

        gtk_label_set_text (
                GTK_LABEL (dialog->priv->mime_content_label),
                description);
        gtk_entry_set_text (
                GTK_ENTRY (dialog->priv->name_entry),
                rstto_file_get_display_name (file));
        g_free (description);

        /* Show or hide the image tab containing exif data */
        if (rstto_file_has_exif (file))
        {
            children = gtk_container_get_children (
                    GTK_CONTAINER (dialog->priv->image_table));
            child_iter = children;

            while (NULL != child_iter)
            {
                gtk_container_remove (
                        GTK_CONTAINER (dialog->priv->image_table),
                        child_iter->data);
                child_iter = g_list_next (child_iter);
            }
            if (NULL != children)
            {
                g_list_free (children);
            }
            for (i = 0; i < EXIF_PROP_COUNT; ++i)
            {
                label_string = NULL;
                exif_data[0] = '\0';
                switch (i)
                {
                    case EXIF_PROP_DATE_TIME:
                        exif_entry = rstto_file_get_exif (file, EXIF_TAG_DATE_TIME);
                        if (NULL != exif_entry)
                        {
                            exif_entry_get_value (exif_entry, exif_data, EXIF_DATA_BUFFER_SIZE);
                            label_string = g_strdup_printf (_("<b>Date taken:</b>"));
                        }
                        break;
                    case EXIF_PROP_MODEL:
                        exif_entry = rstto_file_get_exif (file, EXIF_TAG_MODEL);
                        if (NULL != exif_entry)
                        {
                            exif_entry_get_value (exif_entry, exif_data, EXIF_DATA_BUFFER_SIZE);
                            exif_ifd = exif_entry_get_ifd (exif_entry);
                            exif_title = exif_tag_get_title_in_ifd (
                                    EXIF_TAG_MODEL,
                                    exif_ifd);
                            label_string = g_strdup_printf (_("<b>%s</b>"), exif_title);
                        }
                        break;
                    case EXIF_PROP_MAKE:
                        exif_entry = rstto_file_get_exif (file, EXIF_TAG_MAKE);
                        if (NULL != exif_entry)
                        {
                            exif_entry_get_value (exif_entry, exif_data, EXIF_DATA_BUFFER_SIZE);
                            exif_ifd = exif_entry_get_ifd (exif_entry);
                            exif_title = exif_tag_get_title_in_ifd (
                                    EXIF_TAG_MAKE,
                                    exif_ifd);
                            label_string = g_strdup_printf (_("<b>%s</b>"), exif_title);
                        }
                        break;
                    case EXIF_PROP_APERATURE:
                        exif_entry = rstto_file_get_exif (file, EXIF_TAG_APERTURE_VALUE);
                        if (NULL != exif_entry)
                        {
                            exif_entry_get_value (exif_entry, exif_data, EXIF_DATA_BUFFER_SIZE);
                            exif_ifd = exif_entry_get_ifd (exif_entry);
                            exif_title = exif_tag_get_title_in_ifd (
                                    EXIF_TAG_APERTURE_VALUE,
                                    exif_ifd);
                            label_string = g_strdup_printf (_("<b>%s</b>"), exif_title);
                        }
                        break;
                    default:
                        break;
                }
                exif_label = gtk_label_new (NULL);
                exif_content_label = gtk_label_new (NULL);
                gtk_label_set_markup (
                        GTK_LABEL (exif_label),
                        label_string);
                gtk_label_set_xalign (
                        GTK_LABEL (exif_label),
                        1.0);
                gtk_label_set_yalign (
                        GTK_LABEL (exif_label),
                        0.5);
                gtk_label_set_text (
                        GTK_LABEL (exif_content_label),
                        exif_data);

                gtk_grid_attach (
                        GTK_GRID (dialog->priv->image_table),
                        exif_label,
                        0,
                        i,
                        1,
                        1);
                gtk_grid_attach (
                        GTK_GRID (dialog->priv->image_table),
                        exif_content_label,
                        1,
                        i,
                        1,
                        1);
                if (NULL != label_string)
                {
                    g_free (label_string);
                }
           }

            gtk_widget_show_all (dialog->priv->image_table);
        }
        else
        {
            gtk_widget_hide (dialog->priv->image_table);
        }
    }
}

/********************/
/* Public functions */
/********************/

GtkWidget *
rstto_properties_dialog_new (
        GtkWindow *parent,
        RsttoFile *file)
{
    gchar *title = g_strdup_printf (_("%s - Properties"), rstto_file_get_display_name (file));
    GtkWidget *dialog = g_object_new (RSTTO_TYPE_PROPERTIES_DIALOG,
                                      "title", title,
                                      "icon-name", "document-properties",
                                      "file", file,
                                      NULL);
    g_free (title);

    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

    return dialog;
}

