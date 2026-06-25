/*
 *  Copyright (c) Gaël Bonithon 2023 <gael@xfce.org>
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
#include "print.h"

#include <libxfce4util/libxfce4util.h>


static void
rstto_print_draw_page (GtkPrintOperation *operation,
                       GtkPrintContext *context,
                       gint page_nr);
static void
rstto_print_done (GtkPrintOperation *operation,
                  GtkPrintOperationResult result);



struct _RsttoPrint
{
    GtkPrintOperation parent;

    RsttoImageViewer *viewer;
};



G_DEFINE_TYPE (RsttoPrint, rstto_print, GTK_TYPE_PRINT_OPERATION)



static void
rstto_print_class_init (RsttoPrintClass *klass)
{
    GtkPrintOperationClass *op_class;

    op_class = GTK_PRINT_OPERATION_CLASS (klass);
    op_class->draw_page = rstto_print_draw_page;
    op_class->done = rstto_print_done;
}



static void
rstto_print_init (RsttoPrint *print)
{
}



static void
rstto_print_draw_page (GtkPrintOperation *operation,
                       GtkPrintContext *context,
                       gint page_nr)
{
    RsttoPrint *print = RSTTO_PRINT (operation);
    cairo_t *cr;
    gdouble x0, y0;
    gdouble scale;
    gdouble p_width, p_height;
    gint width, height;
    GtkPageSetup *page_setup;
    GdkPixbuf *pixbuf;

    page_setup = gtk_print_context_get_page_setup (context);
    x0 = gtk_page_setup_get_left_margin (page_setup, GTK_UNIT_POINTS);
    y0 = gtk_page_setup_get_top_margin (page_setup, GTK_UNIT_POINTS);
    cr = gtk_print_context_get_cairo_context (context);
    cairo_translate (cr, x0, y0);

    p_width =  gtk_page_setup_get_page_width (page_setup, GTK_UNIT_POINTS);
    p_height = gtk_page_setup_get_page_height (page_setup, GTK_UNIT_POINTS);
    width = rstto_image_viewer_get_width (print->viewer);
    height = rstto_image_viewer_get_height (print->viewer);
    scale = rstto_image_viewer_get_scale (print->viewer);
    cairo_rectangle (cr, 0, 0, MIN (width * scale, p_width), MIN (height * scale, p_height));
    cairo_clip (cr);
    cairo_scale (cr, scale, scale);

    pixbuf = rstto_image_viewer_get_pixbuf (print->viewer);
    if (pixbuf != NULL)
    {
        gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
        cairo_paint (cr);
        g_object_unref (pixbuf);
    }
}



static void
rstto_print_settings_save_foreach (const gchar *key,
                                   const gchar *value,
                                   gpointer rc)
{
    if (G_LIKELY (key != NULL && value != NULL))
        xfce_rc_write_entry (rc, key, value);
}



static void
rstto_print_settings_save (GtkPrintOperation *operation)
{
    XfceRc *rc;
    GtkPrintSettings *settings;
    GtkPageSetup *page_setup;

    settings = gtk_print_operation_get_print_settings (operation);
    if (G_UNLIKELY (settings == NULL))
        return;

    rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "ristretto/print-settings", FALSE);
    if (G_UNLIKELY (rc == NULL))
        return;

    page_setup = gtk_print_operation_get_default_page_setup (operation);

    gtk_print_settings_set_orientation (settings, gtk_page_setup_get_orientation (page_setup));
    gtk_print_settings_set_paper_size (settings, gtk_page_setup_get_paper_size (page_setup));

    gtk_print_settings_set_double (settings, "top-margin",
      gtk_page_setup_get_top_margin (page_setup, GTK_UNIT_MM));
    gtk_print_settings_set_double (settings, "bottom-margin",
      gtk_page_setup_get_bottom_margin (page_setup, GTK_UNIT_MM));
    gtk_print_settings_set_double (settings, "right-margin",
      gtk_page_setup_get_right_margin (page_setup, GTK_UNIT_MM));
    gtk_print_settings_set_double (settings, "left-margin",
      gtk_page_setup_get_left_margin (page_setup, GTK_UNIT_MM));

    xfce_rc_set_group (rc, "Print Settings");
    gtk_print_settings_foreach (settings, rstto_print_settings_save_foreach, rc);

    xfce_rc_close (rc);
}



static void
rstto_print_done (GtkPrintOperation *operation,
                  GtkPrintOperationResult result)
{
    if (result == GTK_PRINT_OPERATION_RESULT_APPLY)
        rstto_print_settings_save (operation);
}



RsttoPrint *
rstto_print_new (RsttoImageViewer *viewer)
{
    RsttoPrint *print;

    g_return_val_if_fail (RSTTO_IS_IMAGE_VIEWER (viewer), NULL);

    print = g_object_new (RSTTO_TYPE_PRINT, "embed-page-setup", TRUE, NULL);
    print->viewer = viewer;

    return print;
}



static void
rstto_print_settings_load (GtkPrintOperation *operation)
{
    XfceRc *rc;
    GtkPrintSettings *settings = NULL;
    GtkPageSetup *page_setup;
    GtkPaperSize *paper_size;
    gchar **keys;
    const gchar *value;
    gdouble margin;

    rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "ristretto/print-settings", TRUE);
    if (G_UNLIKELY (rc == NULL))
        return;

    keys = xfce_rc_get_entries (rc, "Print Settings");
    if (G_LIKELY (keys != NULL))
    {
        xfce_rc_set_group (rc, "Print Settings");
        settings = gtk_print_settings_new ();
        for (gchar **key = keys; *key != NULL; key++)
        {
            value = xfce_rc_read_entry (rc, *key, NULL);
            if (G_LIKELY (value != NULL))
                gtk_print_settings_set (settings, *key, value);
        }

        g_strfreev (keys);
    }

    xfce_rc_close (rc);

    if (G_LIKELY (settings != NULL))
    {
        page_setup = gtk_page_setup_new ();

        gtk_page_setup_set_orientation (page_setup, gtk_print_settings_get_orientation (settings));

        paper_size = gtk_print_settings_get_paper_size (settings);
        gtk_page_setup_set_paper_size (page_setup, paper_size);
        gtk_paper_size_free (paper_size);

        margin = gtk_print_settings_get_double (settings, "top-margin");
        gtk_page_setup_set_top_margin (page_setup, margin, GTK_UNIT_MM);
        margin = gtk_print_settings_get_double (settings, "bottom-margin");
        gtk_page_setup_set_bottom_margin (page_setup, margin, GTK_UNIT_MM);
        margin = gtk_print_settings_get_double (settings, "right-margin");
        gtk_page_setup_set_right_margin (page_setup, margin, GTK_UNIT_MM);
        margin = gtk_print_settings_get_double (settings, "left-margin");
        gtk_page_setup_set_left_margin (page_setup, margin, GTK_UNIT_MM);

        gtk_print_operation_set_default_page_setup (operation, page_setup);
        gtk_print_operation_set_print_settings (operation, settings);

        g_object_unref (page_setup);
        g_object_unref (settings);
    }
}



gboolean
rstto_print_image_interactive (RsttoPrint *print,
                               GtkWindow *parent,
                               GError **error)
{
    GtkPrintOperationResult result;

    g_return_val_if_fail (RSTTO_IS_PRINT (print), FALSE);
    g_return_val_if_fail (GTK_IS_WINDOW (parent), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    rstto_print_settings_load (GTK_PRINT_OPERATION (print));
    gtk_print_operation_set_n_pages (GTK_PRINT_OPERATION (print), 1);
    gtk_print_operation_set_allow_async (GTK_PRINT_OPERATION (print), TRUE);

    result = gtk_print_operation_run (GTK_PRINT_OPERATION (print),
                                      GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                      parent, error);

    return (result != GTK_PRINT_OPERATION_RESULT_ERROR);
}
