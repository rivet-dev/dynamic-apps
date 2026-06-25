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
#include "main_window.h"



#define RSTTO_THUMBNAIL_FLAVOR_NORMAL_N_PIXELS 128
#define RSTTO_THUMBNAIL_FLAVOR_LARGE_N_PIXELS 256
#define RSTTO_THUMBNAIL_FLAVOR_X_LARGE_N_PIXELS 512
#define RSTTO_THUMBNAIL_FLAVOR_XX_LARGE_N_PIXELS 1024

static const gchar *rstto_thumbnail_flavor_names[] = { "normal", "large", "x-large", "xx-large" };
static const guint rstto_thumbnail_n_pixels_unscaled[] = { 32, 48, 64, 96, 128, 192, 256 };
static guint rstto_thumbnail_n_pixels[RSTTO_THUMBNAIL_SIZE_COUNT];



static void
rstto_util_source_remove_all (gpointer  data,
                              GObject  *object)
{
    while (g_source_remove_by_user_data (object));
}



/* an often sufficient way to automate memory management of sources, without having
 * to store a source id and use an ad hoc handler */
gpointer
rstto_util_source_autoremove (gpointer object)
{
    g_return_val_if_fail (G_IS_OBJECT (object), object);

    if (! rstto_object_get_data (object, "source-autoremove"))
    {
        g_object_weak_ref (object, rstto_util_source_remove_all, NULL);
        rstto_object_set_data (object, "source-autoremove", GINT_TO_POINTER (TRUE));
    }

    return object;
}



/*
 * Workaround to avoid using Cairo Xlib backend which has some memory issues:
 * https://gitlab.freedesktop.org/cairo/cairo/-/issues/500
 * https://gitlab.freedesktop.org/cairo/cairo/-/issues/510
 */
cairo_pattern_t *
rstto_util_set_source_pixbuf (cairo_t *ctx,
                              const GdkPixbuf *pixbuf,
                              gdouble pixbuf_x,
                              gdouble pixbuf_y)
{
    cairo_t *cr;
    cairo_surface_t *surface;
    cairo_pattern_t *pattern;
    cairo_format_t format;

    /* for non-Xlib backends, this is just a wrapper */
    if (ctx != NULL && cairo_surface_get_type (cairo_get_target (ctx)) != CAIRO_SURFACE_TYPE_XLIB)
    {
        gdk_cairo_set_source_pixbuf (ctx, pixbuf, pixbuf_x, pixbuf_y);
        return NULL;
    }

    /* copied from gdk_cairo_set_source_pixbuf() */
    if (gdk_pixbuf_get_n_channels (pixbuf) == 3)
        format = CAIRO_FORMAT_RGB24;
    else
        format = CAIRO_FORMAT_ARGB32;

    /* create a generic image surface on which to apply gdk_cairo_set_source_pixbuf() */
    surface = cairo_image_surface_create (format,
                                          gdk_pixbuf_get_width (pixbuf),
                                          gdk_pixbuf_get_height (pixbuf));
    cr = cairo_create (surface);
    cairo_surface_destroy (surface);

    /* apply it and get the resulting source */
    gdk_cairo_set_source_pixbuf (cr, pixbuf, pixbuf_x, pixbuf_y);
    pattern = cairo_pattern_reference (cairo_get_source (cr));
    cairo_destroy (cr);

    /* put the source in the original context, if any  */
    if (ctx != NULL)
    {
        cairo_set_source (ctx, pattern);
        cairo_pattern_destroy (pattern);
        pattern = NULL;
    }

    return pattern;
}



void
rstto_util_paint_background_color (GtkWidget *widget,
                                   RsttoSettings *settings,
                                   cairo_t *ctx)
{
    GdkWindow *window;
    GdkRGBA *bgcolor = NULL;
    gboolean bgcolor_override = FALSE;

    /* see if we have a non-default background color */
    window = gdk_window_get_toplevel (gtk_widget_get_window (widget));
    if (gdk_window_get_state (window) & GDK_WINDOW_STATE_FULLSCREEN)
        g_object_get (settings, "bgcolor-fullscreen", &bgcolor, NULL);
    else
    {
        g_object_get (settings, "bgcolor-override", &bgcolor_override, NULL);
        if (bgcolor_override)
            g_object_get (settings, "bgcolor", &bgcolor, NULL);
    }

    /* override default background color if needed */
    if (bgcolor != NULL)
    {
        cairo_save (ctx);
        gdk_cairo_set_source_rgba (ctx, bgcolor);
        cairo_paint (ctx);
        cairo_restore (ctx);
        gdk_rgba_free (bgcolor);
    }
}



void
rstto_util_dialog_error (const gchar *message,
                         GError *error)
{
    GtkWidget *dialog;
    GtkWindow *window;

    window = GTK_WINDOW (rstto_main_window_get_app_window ());
    if (message != NULL && error != NULL)
        dialog = gtk_message_dialog_new (window, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                         "%s: %s", message, error->message);
    else if (message != NULL)
        dialog = gtk_message_dialog_new (window, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", message);
    else if (error != NULL)
        dialog = gtk_message_dialog_new (window, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", error->message);
    else
    {
        g_warn_if_reached ();
        return;
    }

    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}



void
rstto_util_set_scale_factor (gint scale_factor)
{
    for (gint n = 0; n < RSTTO_THUMBNAIL_SIZE_COUNT; n++)
        rstto_thumbnail_n_pixels[n] = rstto_thumbnail_n_pixels_unscaled[n] * scale_factor;
}



RsttoThumbnailFlavor
rstto_util_get_thumbnail_flavor (RsttoThumbnailSize size)
{
    if (rstto_thumbnail_n_pixels[size] <= RSTTO_THUMBNAIL_FLAVOR_NORMAL_N_PIXELS)
        return RSTTO_THUMBNAIL_FLAVOR_NORMAL;
    else if (rstto_thumbnail_n_pixels[size] <= RSTTO_THUMBNAIL_FLAVOR_LARGE_N_PIXELS)
        return RSTTO_THUMBNAIL_FLAVOR_LARGE;
    else if (rstto_thumbnail_n_pixels[size] <= RSTTO_THUMBNAIL_FLAVOR_X_LARGE_N_PIXELS)
        return RSTTO_THUMBNAIL_FLAVOR_X_LARGE;
    else
        return RSTTO_THUMBNAIL_FLAVOR_XX_LARGE;
}



const gchar *
rstto_util_get_thumbnail_flavor_name (RsttoThumbnailFlavor flavor)
{
    return rstto_thumbnail_flavor_names[flavor];
}



guint
rstto_util_get_thumbnail_n_pixels (RsttoThumbnailSize size)
{
    return rstto_thumbnail_n_pixels[size];
}
