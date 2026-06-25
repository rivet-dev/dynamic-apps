/*
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *  Copyright (C) 2003 Benedikt Meurer (benedikt.meurer@unix-ag.uni-siegen.de)
 *  Copyright (c) 2004-2007 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include "xfdesktop-common.h"
#include "xfce-backdrop.h" /* for XfceBackdropImageStyle */

/* Free the string whe done using it */
gchar*
xfdesktop_get_monitor_name_from_gtk_widget(GtkWidget *widget, gint monitor_num)
{
    GdkWindow     *window = NULL;
    GdkDisplay    *display = NULL;
    GdkMonitor    *monitor = NULL;

    window = gtk_widget_get_window(widget);
    display = gdk_window_get_display(window);
    monitor = gdk_display_get_monitor(display, monitor_num);

    return g_strdup(gdk_monitor_get_model(monitor));
}

gint
xfdesktop_compare_paths(GFile *a, GFile *b)
{
    gchar *path_a, *path_b;
    gboolean ret;

    path_a = g_file_get_path(a);
    path_b = g_file_get_path(b);

    XF_DEBUG("a %s, b %s", path_a, path_b);

    ret = g_strcmp0(path_a, path_b);

    g_free(path_a);
    g_free(path_b);

    return ret;
}

gchar *
xfdesktop_get_file_mimetype(const gchar *file)
{
    GFile *temp_file;
    GFileInfo *file_info;
    gchar *mime_type = NULL;

    g_return_val_if_fail(file != NULL, NULL);

    temp_file = g_file_new_for_path(file);

    g_return_val_if_fail(temp_file != NULL, NULL);

    file_info = g_file_query_info(temp_file,
                                  "standard::content-type",
                                  0,
                                  NULL,
                                  NULL);

    if(file_info != NULL) {
        mime_type = g_strdup(g_file_info_get_content_type(file_info));

        g_object_unref(file_info);
    }

    g_object_unref(temp_file);

    return mime_type;
}

gboolean
xfdesktop_image_file_is_valid(const gchar *filename)
{
    static GSList *pixbuf_formats = NULL;
    GSList *l;
    gboolean image_valid = FALSE;
    gchar *file_mimetype;

    g_return_val_if_fail(filename, FALSE);

    if(pixbuf_formats == NULL) {
        pixbuf_formats = gdk_pixbuf_get_formats();
    }

    file_mimetype = xfdesktop_get_file_mimetype(filename);

    if(file_mimetype == NULL)
        return FALSE;

    /* Every pixbuf format has a list of mime types we can compare against */
    for(l = pixbuf_formats; l != NULL && image_valid == FALSE; l = g_slist_next(l)) {
        gint i;
        gchar ** mimetypes = gdk_pixbuf_format_get_mime_types(l->data);

        for(i = 0; mimetypes[i] != NULL && image_valid == FALSE; i++) {
            if(g_strcmp0(file_mimetype, mimetypes[i]) == 0)
                image_valid = TRUE;
        }
         g_strfreev(mimetypes);
    }

    g_free(file_mimetype);

    return image_valid;
}

/* The image styles changed from versions prior to 4.11.
 * Auto isn't an option anymore, additionally we should handle invalid
 * values. Set them to the default of stretched. */
gint
xfce_translate_image_styles(gint input)
{
    gint style = input;

    if(style <= 0 || style > XFCE_BACKDROP_IMAGE_SPANNING_SCREENS)
        style = XFCE_BACKDROP_IMAGE_STRETCHED;

    return style;
}



/*
 * xfdesktop_remove_whitspaces:
 * remove all whitespaces from string (not only trailing or leading)
 */
gchar*
xfdesktop_remove_whitspaces(gchar* str)
{
    gchar* dest;
    guint offs, curr;

    g_return_val_if_fail(str, NULL);

    offs = 0;
    dest = str;
    for(curr=0; curr<=strlen(str); curr++) {
        if(*dest == ' ' || *dest == '\t')
            offs++;
        else if(0 != offs)
            *(dest-offs) = *dest;
        dest++;
    }

    return str;
}


static GtkWidget*
create_menu_item(GtkWidget* label, GtkWidget* image)
{
    GtkWidget *mi;
    GtkWidget *box;
    GtkSettings *settings;
    gboolean show_icons = TRUE;

    /* create item */
    mi = gtk_menu_item_new ();

    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);

    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign (label, GTK_ALIGN_START);

    /* Add the image and label to the box, add the box to the menu item */
    if (image && GTK_IS_WIDGET(image)) {
        if (GTK_IS_IMAGE (image))
            gtk_image_set_pixel_size (GTK_IMAGE (image), 16);

        /* only add the widget if it exists */
        gtk_widget_show (image);

        gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);
    }

    gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 6);
    gtk_widget_show_all (box);
    gtk_container_add (GTK_CONTAINER (mi), box);

    if (gtk_widget_has_screen(label)) {
        settings = gtk_settings_get_for_screen(gtk_widget_get_screen(label));
    } else {
        settings = gtk_settings_get_default();
    }
    g_object_get(settings, "gtk-menu-images", &show_icons, NULL);
    gtk_widget_set_visible(image, show_icons);

    return mi;
}

/* Adapted from garcon_gtk_menu_create_menu_item because I don't want
 * to write it over and over.
 */
GtkWidget*
xfdesktop_menu_create_menu_item_with_markup(const gchar *name,
                                            GtkWidget   *image)
{
    GtkWidget *label = gtk_label_new (NULL);

    gtk_label_set_markup(GTK_LABEL(label), name);
    return create_menu_item(label, image);
}



GtkWidget*
xfdesktop_menu_create_menu_item_with_mnemonic(const gchar *name,
                                              GtkWidget   *image)
{
    GtkWidget *label = gtk_label_new_with_mnemonic(name);

    return create_menu_item(label, image);
}



/* Replacement for gdk_screen_width/gdk_screen_height */
void
xfdesktop_get_screen_dimensions (GdkScreen *screen,
                                 gint      *width,
                                 gint      *height)
{
    gint x, y, w, h;
    GdkDisplay *display = gdk_screen_get_display(screen);
    int num_monitors = gdk_display_get_n_monitors(display);

    x = y = G_MAXINT;
    w = h = G_MININT;

    for(int i = 0; i < num_monitors; i++) {
        GdkRectangle rect;
        GdkMonitor *monitor = gdk_display_get_monitor(display, i);
        gdk_monitor_get_geometry(monitor, &rect);

        x = MIN(x, rect.x);
        y = MIN(y, rect.y);
        w = MAX(w, rect.x + rect.width);
        h = MAX(h, rect.y + rect.height);
    }

    if(width != NULL)
        *width = w - x;
    if(height != NULL)
        *height = h - y;
}



gint
xfdesktop_get_monitor_num(GdkDisplay *display,
                          GdkMonitor *monitor)
{
    gint i;

    g_return_val_if_fail(GDK_IS_DISPLAY(display), 0);
    g_return_val_if_fail(GDK_IS_MONITOR(monitor), 0);

    for(i=0; i<gdk_display_get_n_monitors(display); i++) {
        if(monitor == gdk_display_get_monitor(display, i))
            return i;
    }

    g_warning("unable to get the monitor number");
    return 0;
}



gint
xfdesktop_get_current_monitor_num(GdkDisplay *display)
{
    GdkSeat    *seat;
    GdkMonitor *monitor;
    gint        rootx, rooty;

    g_return_val_if_fail(GDK_IS_DISPLAY(display), 0);

    seat = gdk_display_get_default_seat(display);
    gdk_device_get_position(gdk_seat_get_pointer(seat), NULL, &rootx, &rooty);
    monitor = gdk_display_get_monitor_at_point(display, rootx, rooty);

    return xfdesktop_get_monitor_num(display, monitor);
}



void
xfdesktop_tree_path_free(gpointer data)
{
    gtk_tree_path_free((GtkTreePath *) data);
}



void
xfdesktop_widget_unrealize(GtkWidget *widget,
                           gpointer data)
{
    gtk_widget_unrealize(widget);
}



void
xfdesktop_object_ref(gpointer data,
                     gpointer user_data)
{
    g_object_ref(data);
}



void
xfdesktop_object_unref(gpointer data,
                       GClosure *closure)
{
    g_object_unref(data);
}



#ifdef G_ENABLE_DEBUG
/* With --enable-debug=full turn on debugging messages from the start */
static gboolean enable_debug = TRUE;
#else
static gboolean enable_debug = FALSE;
#endif /* G_ENABLE_DEBUG */

#if defined(G_HAVE_ISO_VARARGS)
void
xfdesktop_debug(const char *func, const char *file, int line, const char *format, ...)
{
    va_list args;

    if(!enable_debug)
        return;

    va_start(args, format);

    fprintf(stdout, "DBG[%s:%d] %s(): ", file, line, func);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");

    va_end(args);
}
#endif /* defined(G_HAVE_ISO_VARARGS) */

/**
 * xfdesktop_debug_set:
 * debug: TRUE to turn on the XF_DEBUG mesages.
 */
void
xfdesktop_debug_set(gboolean debug)
{
    enable_debug = debug;
    if(enable_debug)
        XF_DEBUG("debugging enabled");
}
