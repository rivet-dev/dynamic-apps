/*
 *  Copyright (c) 2008-2011 Nick Schermer <nick@xfce.org>
 *  Copyright (c) 2008      Jannis Pohlmann <jannis@xfce.org>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <xfsettingsd/pointers-defines.h>
#ifdef HAVE_XCURSOR
#include <X11/Xcursor/Xcursor.h>

#include <gio/gio.h>
#endif /* !HAVE_XCURSOR */

#ifdef HAVE_LIBINPUT
#include "libinput-properties.h"
#endif /* HAVE_LIBINPUT */

#include <cairo-gobject.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <gdk/gdkx.h>

#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "mouse-dialog_ui.h"

/* settings */
#ifdef HAVE_XCURSOR
#define PREVIEW_ROWS    (3)
#define PREVIEW_COLUMNS (6)
#define PREVIEW_SIZE    (24)
#define PREVIEW_SPACING (2)
#endif /* !HAVE_XCURSOR */


/* global setting channels */
static XfconfChannel *xsettings_channel;
static XfconfChannel *pointers_channel;
static GSettings *desktop_interface_gsettings;

/* lock counter to avoid signals during updates */
static gint locked = 0;

/* device update id */
static guint timeout_id = 0;

#ifdef DEVICE_HOTPLUGGING
/* event id for device add/remove */
static gint device_presence_event_type = 0;
#endif

/* option entries */
static gint opt_socket_id = 0;
static gchar *opt_device_name = NULL;
static gboolean opt_version = FALSE;
static GOptionEntry option_entries[] =
{
    { "device", 'd', 0, G_OPTION_ARG_STRING, &opt_device_name, N_("Active device in the dialog"), N_("DEVICE NAME") },
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};

#ifdef HAVE_XCURSOR
/* icon names for the preview widget */
static const gchar *preview_names[] = {
    "left_ptr",            "left_ptr_watch",    "watch",             "hand2",
    "question_arrow",      "sb_h_double_arrow", "sb_v_double_arrow", "bottom_left_corner",
    "bottom_right_corner", "fleur",             "pirate",            "cross",
    "X_cursor",            "right_ptr",         "right_side",        "right_tee",
    "sb_right_arrow",      "sb_right_tee",      "base_arrow_down",   "base_arrow_up",
    "bottom_side",         "bottom_tee",        "center_ptr",        "circle",
    "dot",                 "dot_box_mask",      "dot_box_mask",      "double_arrow",
    "draped_box",          "left_side",         "left_tee",          "ll_angle",
    "top_side",            "top_tee"
};

enum
{
    COLUMN_THEME_PIXBUF,
    COLUMN_THEME_PATH,
    COLUMN_THEME_NAME,
    COLUMN_THEME_DISPLAY_NAME,
    COLUMN_THEME_COMMENT,
    N_THEME_COLUMNS
};
#endif /* !HAVE_XCURSOR */

enum
{
    COLUMN_DEVICE_NAME,
    COLUMN_DEVICE_XFCONF_NAME,
    COLUMN_DEVICE_XID,
    N_DEVICE_COLUMNS
};

typedef union
{
    gchar   c;
    guchar  uc;
    gint16  i16;
    guint16 u16;
    gint32  i32;
    guint32 u32;
    float   f;
    Atom    a;
} propdata_t;



static gchar *
mouse_settings_format_value_px (GtkScale *scale,
                                gdouble   value)
{
   /* pixel value for some of the scales in the dialog */
   return g_strdup_printf (_("%g px"), value);
}



static gchar *
mouse_settings_format_value_ms (GtkScale *scale,
                                gdouble   value)
{
   /* miliseconds value for some of the scales in the dialog */
   return g_strdup_printf (_("%g ms"), value);
}



#ifdef DEVICE_PROPERTIES
static gchar *
mouse_settings_format_value_s (GtkScale *scale,
                               gdouble   value)
{
   /* seconds value for some of the scales in the dialog */
   return g_strdup_printf (_("%.1f s"), value);
}
#endif




#ifdef HAVE_XCURSOR
static cairo_surface_t *
mouse_settings_themes_pixbuf_from_filename (const gchar *filename,
                                            guint        size,
                                            gint         scale_factor)
{
    XcursorImage *image;
    GdkPixbuf    *scaled, *pixbuf = NULL;
    gsize         bsize;
    guchar       *buffer, *p, tmp;
    gdouble       wratio, hratio;
    gint          dest_width, dest_height;
    guint         full_size = size * scale_factor;

    /* load the image */
    image = XcursorFilenameLoadImage (filename, full_size);
    if (G_LIKELY (image))
    {
        /* buffer size */
        bsize = image->width * image->height * 4;

        /* allocate buffer */
        buffer = g_malloc (bsize);

        /* copy pixel data to buffer */
        memcpy (buffer, image->pixels, bsize);

        /* swap bits */
        for (p = buffer; p < buffer + bsize; p += 4)
        {
            tmp = p[0];
            p[0] = p[2];
            p[2] = tmp;
        }

        /* create pixbuf */
        pixbuf = gdk_pixbuf_new_from_data (buffer, GDK_COLORSPACE_RGB, TRUE,
                                           8, image->width, image->height,
                                           4 * image->width,
                                           (GdkPixbufDestroyNotify) (void (*)(void)) g_free, NULL);

        /* don't leak when creating the pixbuf failed */
        if (G_UNLIKELY (pixbuf == NULL))
            g_free (buffer);

        /* scale pixbuf if needed */
        if (pixbuf && (image->height > full_size || image->width > full_size))
        {
            /* calculate the ratio */
            wratio = (gdouble) image->width / (gdouble) full_size;
            hratio = (gdouble) image->height / (gdouble) full_size;

            /* init */
            dest_width = dest_height = full_size;

            /* set dest size */
            if (hratio > wratio)
                dest_width  = rint (image->width / hratio);
            else
                dest_height = rint (image->height / wratio);

            /* scale pixbuf */
            scaled = gdk_pixbuf_scale_simple (pixbuf, MAX (dest_width, 1), MAX (dest_height, 1), GDK_INTERP_BILINEAR);

            /* release and set scaled pixbuf */
            g_object_unref (G_OBJECT (pixbuf));
            pixbuf = scaled;
        }

        /* cleanup */
        XcursorImageDestroy (image);
    }

    if (G_LIKELY (pixbuf != NULL))
    {
        cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale_factor, NULL);
        g_object_unref (pixbuf);
        return surface;
    }
    else
    {
        return NULL;
    }
}



static cairo_surface_t *
mouse_settings_themes_preview_icon (const gchar *path,
                                    gint         scale_factor)
{
    cairo_surface_t *surface= NULL;
    gchar     *filename;

    /* we only try the normal cursor, it is (most likely) always there */
    filename = g_build_filename (path, "left_ptr", NULL);

    /* try to load the preview */
    surface = mouse_settings_themes_pixbuf_from_filename (filename, PREVIEW_SIZE, scale_factor);

    /* cleanup */
    g_free (filename);

    return surface;
}



static void
mouse_settings_themes_preview_image (const gchar *path,
                                     GtkImage    *image)
{
    cairo_surface_t *preview;
    cairo_t         *cr;
    guint            i, position;
    gint             scale_factor;

    /* create an empty preview image */
    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (image));
    preview = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                          ((PREVIEW_SIZE + PREVIEW_SPACING) * PREVIEW_COLUMNS - PREVIEW_SPACING) * scale_factor,
                                          ((PREVIEW_SIZE + PREVIEW_SPACING) * PREVIEW_ROWS - PREVIEW_SPACING) * scale_factor);
    cairo_surface_set_device_scale (preview, scale_factor, scale_factor);
    cr = cairo_create (preview);

    for (i = 0, position = 0; i < G_N_ELEMENTS (preview_names); i++)
    {
        /* create cursor filename and try to load the pixbuf */
        gchar     *filename = g_build_filename (path, preview_names[i], NULL);
        cairo_surface_t *surface = mouse_settings_themes_pixbuf_from_filename (filename, PREVIEW_SIZE, scale_factor);

        g_free (filename);

        if (G_LIKELY (surface))
        {
            gint dest_x, dest_y;

            cairo_save (cr);

            /* calculate the icon position */
            dest_x = (position % PREVIEW_COLUMNS) * (PREVIEW_SIZE + PREVIEW_SPACING);
            dest_y = (position / PREVIEW_COLUMNS) * (PREVIEW_SIZE + PREVIEW_SPACING);
            cairo_translate (cr, dest_x, dest_y);

            cairo_set_source_surface (cr, surface, 0, 0);
            cairo_paint (cr);

            cairo_restore (cr);
            cairo_surface_destroy (surface);

            /* break if we've added enough icons */
            if (++position >= PREVIEW_ROWS * PREVIEW_COLUMNS)
                break;
        }
    }

    cairo_destroy (cr);

    gtk_image_set_from_surface (image, preview);
    cairo_surface_destroy (preview);
}



static void
mouse_settings_themes_selection_changed (GtkTreeSelection *selection,
                                         GtkBuilder       *builder)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    gboolean      has_selection;
    gchar        *path, *name;
    GObject      *image;

    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        /* get theme information from model */
        gtk_tree_model_get (model, &iter, COLUMN_THEME_PATH, &path,
                            COLUMN_THEME_NAME, &name, -1);

        /* update the preview widget */
        image = gtk_builder_get_object (builder, "theme-preview");
        mouse_settings_themes_preview_image (path, GTK_IMAGE (image));

        /* write configuration (not during a lock) */
        if (locked == 0)
        {
            xfconf_channel_set_string (xsettings_channel, "/Gtk/CursorThemeName", name);

            /* Keep gsettings in sync */
            if (desktop_interface_gsettings != NULL)
            {
                g_settings_set_string (desktop_interface_gsettings, "cursor-theme", name);
            }
        }

        /* cleanup */
        g_free (path);
        g_free (name);
    }
}



static gint
mouse_settings_themes_sort_func (GtkTreeModel *model,
                                 GtkTreeIter  *a,
                                 GtkTreeIter  *b,
                                 gpointer      user_data)
{
    gchar *name_a, *name_b;
    gint   retval;

    /* get the names from the model */
    gtk_tree_model_get (model, a, COLUMN_THEME_DISPLAY_NAME, &name_a, -1);
    gtk_tree_model_get (model, b, COLUMN_THEME_DISPLAY_NAME, &name_b, -1);

    /* make sure the names are not null */
    if (G_UNLIKELY (name_a == NULL))
        name_a = g_strdup ("");
    if (G_UNLIKELY (name_b == NULL))
        name_b = g_strdup ("");

    /* sort the names but keep Default on top */
    if (g_utf8_collate (name_a, _("Default")) == 0)
        retval = -1;
    else if (g_utf8_collate (name_b, _("Default")) == 0)
        retval = 1;
    else
        retval = g_utf8_collate (name_a, name_b);

    /* cleanup */
    g_free (name_a);
    g_free (name_b);

    return retval;
}



static void
mouse_settings_themes_populate_store (GtkBuilder *builder)
{
    const gchar        *path;
    gchar             **basedirs;
    gint                i;
    gchar              *homedir;
    GDir               *dir;
    const gchar        *theme;
    gchar              *filename;
    gchar              *index_file;
    XfceRc             *rc;
    const gchar        *name;
    const gchar        *comment;
    GtkTreeIter         iter;
    gint                position = 0;
    gchar              *active_theme;
    GtkTreePath        *active_path = NULL;
    GtkListStore       *store;
    GtkCellRenderer    *renderer;
    GtkTreeViewColumn  *column;
    GObject            *treeview;
    GtkTreeSelection   *selection;
    gchar              *comment_escaped;
    gint                scale_factor;

    /* get the cursor paths */
#if XCURSOR_LIB_MAJOR == 1 && XCURSOR_LIB_MINOR < 1
    path = "~/.icons:/usr/share/icons:/usr/share/pixmaps:/usr/X11R6/lib/X11/icons";
#else
    path = XcursorLibraryPath ();
#endif

    /* split the paths */
    basedirs = g_strsplit (path, ":", -1);

    /* get the active theme */
    active_theme = xfconf_channel_get_string (xsettings_channel, "/Gtk/CursorThemeName", "default");

    treeview = gtk_builder_get_object (builder, "theme-treeview");
    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (treeview));

    /* create the store */
    store = gtk_list_store_new (N_THEME_COLUMNS, CAIRO_GOBJECT_TYPE_SURFACE, G_TYPE_STRING,
                                G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    /* insert default */
    gtk_list_store_insert_with_values (store, &iter, position++,
                                       COLUMN_THEME_NAME, "default",
                                       COLUMN_THEME_DISPLAY_NAME, _("Default"), -1);

    /* store the default path, so we always select a theme */
    active_path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);

    if (G_LIKELY (basedirs))
    {
        /* walk the base directories */
        for (i = 0; basedirs[i] != NULL; i++)
        {
            /* init */
            homedir = NULL;

            /* parse the homedir if needed */
            if (strstr (basedirs[i], "~/") != NULL)
                path = homedir = g_strconcat (g_get_home_dir (), basedirs[i] + 1, NULL);
            else
                path = basedirs[i];

            /* open directory */
            dir = g_dir_open (path, 0, NULL);
            if (G_LIKELY (dir))
            {
                for (;;)
                {
                    /* get the directory name */
                    theme = g_dir_read_name (dir);
                    if (G_UNLIKELY (theme == NULL))
                        break;

                    /* build the full cursor path */
                    filename = g_build_filename (path, theme, "cursors", NULL);

                    /* check if it looks like a cursor theme */
                    if (g_file_test (filename, G_FILE_TEST_IS_DIR))
                    {
                        cairo_surface_t *surface = mouse_settings_themes_preview_icon (filename, scale_factor);

                        /* insert in the store */
                        gtk_list_store_insert_with_values (store, &iter, position++,
                                                           COLUMN_THEME_PIXBUF, surface,
                                                           COLUMN_THEME_NAME, theme,
                                                           COLUMN_THEME_DISPLAY_NAME, theme,
                                                           COLUMN_THEME_PATH, filename, -1);

                        if (G_LIKELY (surface != NULL))
                        {
                            cairo_surface_destroy (surface);
                        }

                        /* check if this is the active theme, set the path */
                        if (strcmp (active_theme, theme) == 0)
                        {
                            gtk_tree_path_free (active_path);
                            active_path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
                        }

                        /* check for a index.theme file for additional information */
                        index_file = g_build_filename (path, theme, "index.theme", NULL);
                        if (g_file_test (index_file, G_FILE_TEST_IS_REGULAR))
                        {
                            /* open theme desktop file */
                            rc = xfce_rc_simple_open (index_file, TRUE);
                            if (G_LIKELY (rc))
                            {
                                /* check for the theme group */
                                if (xfce_rc_has_group (rc, "Icon Theme"))
                                {
                                    /* set group */
                                    xfce_rc_set_group (rc, "Icon Theme");

                                    /* read values */
                                    name = xfce_rc_read_entry (rc, "Name", theme);
                                    comment = xfce_rc_read_entry (rc, "Comment", NULL);

                                    /* escape the comment */
                                    comment_escaped = comment ? g_markup_escape_text (comment, -1) : NULL;

                                    /* update store */
                                    gtk_list_store_set (store, &iter,
                                                        COLUMN_THEME_DISPLAY_NAME, name,
                                                        COLUMN_THEME_COMMENT, comment_escaped, -1);

                                    /* cleanup */
                                    g_free (comment_escaped);
                                }

                                /* close rc file */
                                xfce_rc_close (rc);
                            }
                        }

                        /* cleanup */
                        g_free (index_file);
                    }

                    /* cleanup */
                    g_free (filename);
                }

                /* close directory */
                g_dir_close (dir);
            }

            /* cleanup */
            g_free (homedir);
        }

        /* cleanup */
        g_strfreev (basedirs);
    }

    /* cleanup */
    g_free (active_theme);

    /* set the treeview store */
    gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (treeview), COLUMN_THEME_COMMENT);

    /* setup the columns */
    renderer = gtk_cell_renderer_pixbuf_new ();
    column = gtk_tree_view_column_new_with_attributes ("", renderer, "surface", COLUMN_THEME_PIXBUF, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("", renderer, "text", COLUMN_THEME_DISPLAY_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    /* setup selection */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (mouse_settings_themes_selection_changed), builder);

    /* select the active theme in the treeview */
    gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), active_path, NULL, FALSE);
    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (treeview), active_path, NULL, TRUE, 0.5, 0.0);
    gtk_tree_path_free (active_path);

    /* sort the store */
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store), COLUMN_THEME_DISPLAY_NAME, mouse_settings_themes_sort_func, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), COLUMN_THEME_DISPLAY_NAME, GTK_SORT_ASCENDING);

    /* release the store */
    g_object_unref (G_OBJECT (store));
}
#endif /* !HAVE_XCURSOR */



#ifdef HAVE_LIBINPUT
/* FIXME: Completely overkill here and better suited in some common file */
static gboolean
mouse_settings_get_device_prop (Display     *xdisplay,
                                XDevice     *device,
                                const gchar *prop_name,
                                Atom         type,
                                guint        n_items,
                                propdata_t  *retval)
{
    Atom     prop, float_type, type_ret;
    gulong   n_items_ret, bytes_after;
    gint     rc, format, size;
    guint    i;
    guchar  *data, *ptr;
    gboolean success;

    prop = XInternAtom (xdisplay, prop_name, False);
    float_type = XInternAtom (xdisplay, "FLOAT", False);

    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    rc = XGetDeviceProperty (xdisplay, device, prop, 0, 1, False,
                             type, &type_ret, &format, &n_items_ret,
                             &bytes_after, &data);
    gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
    if (rc == Success && type_ret == type && n_items_ret >= n_items)
    {
        success = TRUE;
        switch(format)
        {
            case 8:
                size = sizeof (gchar);
                break;
            case 16:
                size = sizeof (gint16);
                break;
            case 32:
            default:
                size = sizeof (gint32);
                break;
        }
        ptr = data;

        for (i = 0; i < n_items; i++)
        {
            switch (type_ret)
            {
                case XA_INTEGER:
                    switch (format)
                    {
                        case 8:
                            retval[i].c = *((gchar*) ptr);
                            break;
                        case 16:
                            retval[i].i16 = *((gint16 *) ptr);
                            break;
                        case 32:
                            retval[i].i32 = *((gint32 *) ptr);
                            break;
                    }
                    break;
                case XA_CARDINAL:
                    switch (format)
                    {
                        case 8:
                            retval[i].uc = *((guchar*) ptr);
                            break;
                        case 16:
                            retval[i].u16 = *((guint16 *) ptr);
                            break;
                        case 32:
                            retval[i].u32 = *((guint32 *) ptr);
                            break;
                    }
                    break;
                case XA_ATOM:
                    retval[i].a = *((Atom *) ptr);
                    break;
                default:
                    if (type_ret == float_type)
                    {
                        retval[i].f = *((float*) ptr);
                    }
                    else
                    {
                        success = FALSE;
                        g_warning ("Unhandled type, please implement it");
                    }
                    break;
            }
            ptr += size;
        }
        XFree (data);

        return success;
    }

    return FALSE;
}

static gboolean
mouse_settings_get_libinput_accel (Display *xdisplay,
                                   XDevice *device,
                                   gdouble *val)
{
    propdata_t pdata[1];
    Atom float_type;

    float_type = XInternAtom (xdisplay, "FLOAT", False);
    if (mouse_settings_get_device_prop (xdisplay, device, LIBINPUT_PROP_ACCEL, float_type, 1, &pdata[0]))
    {
        /* We use double internally, for whatever reason */
        *val = (gdouble) (pdata[0].f + 1.0) * 5.0;

        return TRUE;
    }

    return FALSE;
}



static gboolean
mouse_settings_get_libinput_boolean (Display     *xdisplay,
                                     XDevice     *device,
                                     const gchar *prop_name,
                                     gboolean    *val)
{
    propdata_t pdata[1];

    if (mouse_settings_get_device_prop (xdisplay, device, prop_name, XA_INTEGER, 1, &pdata[0]))
    {
        *val = (gboolean) (pdata[0].c);

        return TRUE;
    }

    return FALSE;
}
#endif /* HAVE_LIBINPUT */



#ifdef DEVICE_PROPERTIES
static gint
mouse_settings_device_get_int_property (XDevice *device,
                                        Atom     prop,
                                        guint    offset,
                                        gint    *horiz)
{
    Atom     type;
    gint     format;
    gulong   n_items, bytes_after;
    guchar  *data;
    gint     val = -1;
    gint     res;

    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    res = XGetDeviceProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
                              device, prop, 0, 1000, False,
                              AnyPropertyType, &type, &format,
                              &n_items, &bytes_after, &data);
    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) == 0 && res == Success)
    {
        if (type == XA_INTEGER)
        {
            if (n_items > offset)
                val = data[offset];

            if (n_items > 1 + offset && horiz != NULL)
                *horiz = data[offset + 1];
        }

        XFree (data);
    }

    return val;
}
#endif



static gboolean
mouse_settings_device_get_selected (GtkBuilder  *builder,
                                    XDevice    **device,
                                    gchar      **xfconf_name)
{
    GObject      *combobox;
    GtkTreeIter   iter;
    gboolean      found = FALSE;
    gulong        xid;
    GtkTreeModel *model;

    /* get the selected item */
    combobox = gtk_builder_get_object (builder, "device-combobox");
    found = gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combobox), &iter);
    if (found)
    {
        /* get the device id  */
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
        gtk_tree_model_get (model, &iter, COLUMN_DEVICE_XID, &xid, -1);

        if (xfconf_name != NULL)
            gtk_tree_model_get (model, &iter, COLUMN_DEVICE_XFCONF_NAME, xfconf_name, -1);

        if (device != NULL)
        {
            /* open the device */
            gdk_x11_display_error_trap_push (gdk_display_get_default ());
            *device = XOpenDevice (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), xid);
            if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0 || *device == NULL)
            {
                g_critical ("Unable to open device %ld", xid);
                *device = NULL;
                found = FALSE;
            }
        }
    }

    return found;
}



#ifdef DEVICE_PROPERTIES
static void
mouse_settings_wacom_set_rotation (GtkComboBox *combobox,
                                   GtkBuilder  *builder)
{
    XDevice      *device;
    GtkTreeIter   iter;
    GtkTreeModel *model;
    gint          rotation = 0;
    gchar        *name = NULL;
    gchar        *prop;

    if (locked > 0)
        return;

    if (mouse_settings_device_get_selected (builder, &device, &name))
    {
        if (gtk_combo_box_get_active_iter (combobox, &iter))
        {
            model = gtk_combo_box_get_model (combobox);
            gtk_tree_model_get (model, &iter, 0, &rotation, -1);

            prop = g_strconcat ("/", name, "/Properties/Wacom_Rotation", NULL);
            xfconf_channel_set_int (pointers_channel, prop, rotation);
            g_free (prop);
        }

        XCloseDevice (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device);
    }

    g_free (name);
}
#endif



#ifdef DEVICE_PROPERTIES
static void
mouse_settings_wacom_set_mode (GtkComboBox *combobox,
                               GtkBuilder  *builder)
{
    XDevice      *device;
    Display      *xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    GtkTreeIter   iter;
    GtkTreeModel *model;
    gchar        *mode = NULL;
    gchar        *name = NULL;
    gchar        *prop;

    if (locked > 0)
        return;

    if (mouse_settings_device_get_selected (builder, &device, &name))
    {
        if (gtk_combo_box_get_active_iter (combobox, &iter))
        {
            model = gtk_combo_box_get_model (combobox);
            gtk_tree_model_get (model, &iter, 0, &mode, -1);

            prop = g_strconcat ("/", name, "/Mode", NULL);
            xfconf_channel_set_string (pointers_channel, prop, mode);
            g_free (prop);

            g_free (mode);
        }

        XCloseDevice (xdisplay, device);
    }

    g_free (name);
}
#endif



#if defined(DEVICE_PROPERTIES) || defined (HAVE_LIBINPUT)
static void
mouse_settings_synaptics_set_tap_to_click (GtkBuilder *builder)
{
    Display   *xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    XDevice   *device;
    gchar     *name = NULL;
    Atom       tap_ation_prop;
    Atom       type;
    gint       format;
    gulong     n, n_items, bytes_after;
    guchar    *data;
    gboolean   tap_to_click;
    GPtrArray *array;
    gint       res;
    GObject   *object;
    gchar     *prop;
    GValue    *val;

    if (mouse_settings_device_get_selected (builder, &device, &name))
    {
        object = gtk_builder_get_object (builder, "synaptics-tap-to-click");
        tap_to_click = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object));

        gdk_x11_display_error_trap_push (gdk_display_get_default ());
        tap_ation_prop = XInternAtom (xdisplay, "Synaptics Tap Action", True);
        res = XGetDeviceProperty (xdisplay, device, tap_ation_prop, 0, 1000, False,
                                  AnyPropertyType, &type, &format,
                                  &n_items, &bytes_after, &data);
        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) == 0
            && res == Success)
        {
            if (type == XA_INTEGER
                && format == 8
                && n_items >= 7)
            {

                /* format: RT, RB, LT, LB, F1, F2, F3 */
                data[4] = tap_to_click ? 1 : 0;
                data[5] = tap_to_click ? 3 : 0;
                data[6] = tap_to_click ? 2 : 0;

                array = g_ptr_array_sized_new (n_items);
                for (n = 0; n < n_items; n++)
                {
                    val = g_new0 (GValue, 1);
                    g_value_init (val, G_TYPE_INT);
                    g_value_set_int (val, data[n]);
                    g_ptr_array_add (array, val);
                }

                prop = g_strconcat ("/", name, "/Properties/Synaptics_Tap_Action", NULL);
                xfconf_channel_set_arrayv (pointers_channel, prop, array);
                g_free (prop);

                xfconf_array_free (array);
            }

            XFree (data);
        }

#ifdef HAVE_LIBINPUT
        /* Set the corresponding libinput property as well */
        prop = g_strdup_printf ("/%s/Properties/%s", name, LIBINPUT_PROP_TAP);
        g_strdelimit (prop, " ", '_');
        xfconf_channel_set_int (pointers_channel, prop, (int) tap_to_click);
        g_free (prop);
#endif /* HAVE_LIBINPUT */
    }
    g_free (name);
}
#endif /* DEVICE_PROPERTIES || HAVE_LIBINPUT */



#ifdef DEVICE_PROPERTIES
static void
mouse_settings_synaptics_hscroll_sensitive (GtkBuilder *builder)
{
    gint      active;
    gboolean  sensitive = FALSE;
    GObject  *object;

    /* Values for active:
     * -1 no selection
     *  0 disabled
     *  1 edge scrolling
     *  2 two-finger scrolling
     *  3 circular scrolling
     */
    object = gtk_builder_get_object (builder, "synaptics-scroll");
    active = gtk_combo_box_get_active (GTK_COMBO_BOX (object));
    if (gtk_widget_get_sensitive (GTK_WIDGET (object))
        && active > 0)
        sensitive = TRUE;

    object = gtk_builder_get_object (builder, "synaptics-scroll-horiz");
    gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);
}
#endif



#if defined(DEVICE_PROPERTIES) || defined(HAVE_LIBINPUT)
static void
mouse_settings_synaptics_set_scrolling (GtkComboBox *combobox,
                                        GtkBuilder  *builder)
{
    gint      edge_scroll[3] = { 0, 0, 0 };
    gint      two_scroll[2] = { 0, 0 };
    gint      circ_scroll = 0;
    gint      circ_trigger = 0;
#ifdef HAVE_LIBINPUT
    gint      button_scroll = 0;
#endif /* HAVE_LIBINPUT */
    GObject  *object;
    gboolean  horizontal = FALSE;
    gint      active;
    gchar    *name = NULL, *prop;

    if (locked > 0)
        return;

    mouse_settings_synaptics_hscroll_sensitive (builder);

    object = gtk_builder_get_object (builder, "synaptics-scroll-horiz");
    if (gtk_widget_get_sensitive (GTK_WIDGET (object)))
        horizontal = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object));

    /* Values for active:
     * -1 no selection
     *  0 disabled
     *  1 edge scrolling
     *  2 two-finger scrolling
     *  3 circular scrolling
     */
    object = gtk_builder_get_object (builder, "synaptics-scroll");
    active = gtk_combo_box_get_active (GTK_COMBO_BOX (object));
    if (!gtk_widget_get_sensitive (GTK_WIDGET (object)))
        active = -1;


    if (active == 1)
    {
        edge_scroll[0] = TRUE;
        edge_scroll[1] = horizontal;
    }

    if (active == 2)
    {
        two_scroll[0] = TRUE;
        two_scroll[1] = horizontal;
    }

    if (active == 3)
    {
        circ_scroll = TRUE;
        if (horizontal)
        {
            circ_trigger = 3;
            edge_scroll[1] = TRUE;
        }
    }

    if (mouse_settings_device_get_selected (builder, NULL, &name))
    {
        /* 3 values: vertical, horizontal, corner. */
        prop = g_strconcat ("/", name, "/Properties/Synaptics_Edge_Scrolling", NULL);
        xfconf_channel_set_array (pointers_channel, prop,
                                  G_TYPE_INT, &edge_scroll[0],
                                  G_TYPE_INT, &edge_scroll[1],
                                  G_TYPE_INT, &edge_scroll[2],
                                  G_TYPE_INVALID);
        g_free (prop);

        /* 2 values: vertical, horizontal. */
        prop = g_strconcat ("/", name, "/Properties/Synaptics_Two-Finger_Scrolling", NULL);
        xfconf_channel_set_array (pointers_channel, prop,
                                  G_TYPE_INT, &two_scroll[0],
                                  G_TYPE_INT, &two_scroll[1],
                                  G_TYPE_INVALID);
        g_free (prop);

        /* 1 value: circular. */
        prop = g_strconcat ("/", name, "/Properties/Synaptics_Circular_Scrolling", NULL);
        xfconf_channel_set_int (pointers_channel, prop, circ_scroll);
        g_free (prop);

        /* 1 value: location. */
        prop = g_strconcat ("/", name, "/Properties/Synaptics_Circular_Scrolling_Trigger", NULL);
        xfconf_channel_set_int (pointers_channel, prop, circ_trigger);
        g_free (prop);

#ifdef HAVE_LIBINPUT
        /* Set the corresponding libinput property as well */
        prop = g_strdup_printf ("/%s/Properties/%s", name, LIBINPUT_PROP_SCROLL_METHOD_ENABLED);
        g_strdelimit (prop, " ", '_');
        xfconf_channel_set_array (pointers_channel, prop,
                                  G_TYPE_INT, &two_scroll[0],
                                  G_TYPE_INT, &edge_scroll[0],
                                  G_TYPE_INT, &button_scroll,
                                  G_TYPE_INVALID);
        g_free (prop);
#endif /* HAVE_LIBINPUT */

    }

    g_free (name);
}
#endif /* DEVICE_PROPERTIES || HAVE_LIBINPUT */



#ifdef DEVICE_PROPERTIES
static void
mouse_settings_synaptics_set_scroll_horiz (GtkWidget  *widget,
                                           GtkBuilder *builder)
{
    GObject  *object;

    if (locked > 0)
        return;

    object = gtk_builder_get_object (builder, "synaptics-scroll");

    mouse_settings_synaptics_set_scrolling (GTK_COMBO_BOX (object), builder);
}
#endif



#if defined(DEVICE_PROPERTIES) || defined (HAVE_LIBINPUT)
static void
mouse_settings_device_set_enabled (GtkSwitch  *widget,
                                   GParamSpec *pspec,
                                   GtkBuilder *builder)
{
    gchar    *name = NULL;
    gchar    *prop;
    gboolean  enabled;
    GObject  *object;

    enabled = gtk_switch_get_active (widget);
    object = gtk_builder_get_object (builder, "device-notebook");
    gtk_widget_set_sensitive (GTK_WIDGET (object), enabled);

    if (locked > 0)
        return;

    if (mouse_settings_device_get_selected (builder, NULL, &name))
    {
        prop = g_strconcat ("/", name, "/Properties/Device_Enabled", NULL);
        xfconf_channel_set_int (pointers_channel, prop, enabled);
        g_free (prop);
    }

    g_free (name);
}
#endif /* DEVICE_PROPERTIES || HAVE_LIBINPUT */



static void
mouse_settings_device_selection_changed (GtkBuilder *builder)
{
    gint               nbuttons = 0;
    Display           *xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    XDevice           *device;
    XDeviceInfo       *device_info;
    XFeedbackState    *states, *pt;
    XAnyClassPtr       any;
    gint               nstates;
    XPtrFeedbackState *state;
    gint               i, n;
    guchar            *buttonmap;
    gint               id_1 = 0, id_3 = 0;
    gint               id_4 = 0, id_5 = 0;
    gdouble            acceleration = -1.00;
    gint               threshold = -1;
    GObject           *object;
    gint               ndevices;
    gboolean           is_synaptics = FALSE;
    gboolean           is_wacom = FALSE;
    gboolean           left_handed = FALSE;
    gboolean           reverse_scrolling = FALSE;
#ifdef HAVE_LIBINPUT
    gboolean           is_libinput = FALSE;
#endif /* HAVE_LIBINPUT */
#if defined(DEVICE_PROPERTIES) || defined (HAVE_LIBINPUT)
#ifdef HAVE_LIBINPUT
    Atom               libinput_tap_prop;
    Atom               libinput_scroll_methods_prop;
#endif /* HAVE_LIBINPUT */
    Atom               synaptics_prop;
    Atom               wacom_prop;
    Atom               synaptics_tap_prop;
    Atom               synaptics_edge_scroll_prop;
    Atom               synaptics_two_scroll_prop;
    Atom               synaptics_circ_scroll_prop;
    Atom               device_enabled_prop;
    Atom               wacom_rotation_prop;
    gint               is_enabled = -1;
    gint               synaptics_tap_to_click = -1;
    gint               synaptics_edge_scroll = -1;
    gint               synaptics_edge_hscroll = -1;
    gint               synaptics_two_scroll = -1;
    gint               synaptics_two_hscroll = -1;
    gint               synaptics_circ_scroll = -1;
    gint               synaptics_scroll_mode = 0;
    GtkTreeIter        iter;
    gint               wacom_rotation = -1;
    Atom              *props;
    gint               nprops;
    gint               wacom_mode = -1;
#endif /* DEVICE_PROPERTIES || HAVE_LIBINPUT */

    /* lock the dialog */
    locked++;

    /* get the selected item */
    if (mouse_settings_device_get_selected (builder, &device, NULL))
    {
        gdk_x11_display_error_trap_push (gdk_display_get_default ());
        device_info = XListInputDevices (xdisplay, &ndevices);
        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) == 0 && device_info != NULL)
        {
            /* find mode and number of buttons */
            for (i = 0; i < ndevices; i++)
            {
                if (device_info[i].id != device->device_id)
                    continue;

                any = device_info[i].inputclassinfo;
                for (n = 0; n < device_info[i].num_classes; n++)
                {
                    if (any->class == ButtonClass)
                        nbuttons = ((XButtonInfoPtr) any)->num_buttons;
#ifdef DEVICE_PROPERTIES
                    else if (any->class == ValuatorClass)
                        wacom_mode = ((XValuatorInfoPtr) any)->mode == Absolute ? 0 : 1;
#endif

                    any = (XAnyClassPtr) ((gchar *) any + any->length);
                }

                break;
            }

            XFreeDeviceList (device_info);
        }
#ifdef HAVE_LIBINPUT
        is_libinput = mouse_settings_get_libinput_boolean (xdisplay, device, LIBINPUT_PROP_LEFT_HANDED, &left_handed);
        mouse_settings_get_libinput_boolean (xdisplay, device, LIBINPUT_PROP_NATURAL_SCROLL, &reverse_scrolling);
        if (!is_libinput)
#endif /* HAVE_LIBINPUT */
        {
            /* get the button mapping */
            if (nbuttons > 0)
            {
                buttonmap = g_new0 (guchar, nbuttons);
                gdk_x11_display_error_trap_push (gdk_display_get_default ());
                XGetDeviceButtonMapping (xdisplay, device, buttonmap, nbuttons);
                if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
                    g_critical ("Failed to get button map");

                /* figure out the position of the first and second/third button in the map */
                for (i = 0; i < nbuttons; i++)
                {
                    if (buttonmap[i] == 1)
                        id_1 = i;
                    else if (buttonmap[i] == (nbuttons < 3 ? 2 : 3))
                        id_3 = i;
                    else if (buttonmap[i] == 4)
                        id_4 = i;
                    else if (buttonmap[i] == 5)
                        id_5 = i;
                }
                g_free (buttonmap);
                left_handed = (id_1 > id_3);
                reverse_scrolling = !!(id_5 < id_4);
            }
            else
            {
                g_critical ("Device has no buttons");
            }
        }
#ifdef HAVE_LIBINPUT
        if (!mouse_settings_get_libinput_accel (xdisplay, device, &acceleration))
#endif /* HAVE_LIBINPUT */
        {
            /* get the feedback states for this device */
            gdk_x11_display_error_trap_push (gdk_display_get_default ());
            states = XGetFeedbackControl (xdisplay, device, &nstates);
            if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0 || states == NULL)
            {
                 g_critical ("Failed to get feedback states");
            }
            else
            {
                /* get the pointer feedback class */
                for (pt = states, i = 0; i < nstates; i++)
                {
                    if (pt->class == PtrFeedbackClass)
                    {
                        /* get the state */
                        state = (XPtrFeedbackState *) pt;
                        acceleration = (gdouble) state->accelNum / (gdouble) state->accelDenom;
                        threshold = state->threshold;
                    }

                    /* advance the offset */
                    pt = (XFeedbackState *) ((gchar *) pt + pt->length);
                }

                XFreeFeedbackList (states);
            }
        }
#if defined(DEVICE_PROPERTIES) || defined (HAVE_LIBINPUT)
#ifdef HAVE_LIBINPUT
        /* lininput properties */
        libinput_tap_prop = XInternAtom (xdisplay, LIBINPUT_PROP_TAP, True);
        libinput_scroll_methods_prop = XInternAtom (xdisplay, LIBINPUT_PROP_SCROLL_METHOD_ENABLED, True);
#endif /* HAVE_LIBINPUT */
        /* wacom and synaptics specific properties */
        device_enabled_prop = XInternAtom (xdisplay, "Device Enabled", True);
        synaptics_prop = XInternAtom (xdisplay, "Synaptics Off", True);
        wacom_prop = XInternAtom (xdisplay, "Wacom Tool Type", True);
        synaptics_tap_prop = XInternAtom (xdisplay, "Synaptics Tap Action", True);
        synaptics_edge_scroll_prop = XInternAtom (xdisplay, "Synaptics Edge Scrolling", True);
        synaptics_two_scroll_prop = XInternAtom (xdisplay, "Synaptics Two-Finger Scrolling", True);
        synaptics_circ_scroll_prop = XInternAtom (xdisplay, "Synaptics Circular Scrolling", True);
        wacom_rotation_prop = XInternAtom (xdisplay, "Wacom Rotation", True);

        /* check if this is a synaptics or wacom device */
        gdk_x11_display_error_trap_push (gdk_display_get_default ());
        props = XListDeviceProperties (xdisplay, device, &nprops);
        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) == 0 && props != NULL)
        {
            for (i = 0; i < nprops; i++)
            {
                if (props[i] == device_enabled_prop)
                    is_enabled = mouse_settings_device_get_int_property (device, props[i], 0, NULL);
                else if (props[i] == synaptics_prop)
                    is_synaptics = TRUE;
                else if (props[i] == wacom_prop)
                    is_wacom = TRUE;
                else if (props[i] == synaptics_tap_prop)
                    synaptics_tap_to_click = mouse_settings_device_get_int_property (device, props[i], 4, NULL);
                else if (props[i] == synaptics_edge_scroll_prop)
                    synaptics_edge_scroll = mouse_settings_device_get_int_property (device, props[i], 0, &synaptics_edge_hscroll);
                else if (props[i] == synaptics_two_scroll_prop)
                    synaptics_two_scroll = mouse_settings_device_get_int_property (device, props[i], 0, &synaptics_two_hscroll);
                else if (props[i] == synaptics_circ_scroll_prop)
                    synaptics_circ_scroll = mouse_settings_device_get_int_property (device, props[i], 0, NULL);
                else if (props[i] == wacom_rotation_prop)
                    wacom_rotation = mouse_settings_device_get_int_property (device, props[i], 0, NULL);
#ifdef HAVE_LIBINPUT
                else if (props[i] == libinput_tap_prop)
                {
                    is_synaptics = TRUE;
                    mouse_settings_get_libinput_boolean (xdisplay, device, LIBINPUT_PROP_TAP, &synaptics_tap_to_click);
                }
                else if (props[i] == libinput_scroll_methods_prop)
                {
                    propdata_t pdata[3];
                    gboolean success;

                    success = mouse_settings_get_device_prop (xdisplay,
                                                              device,
                                                              LIBINPUT_PROP_SCROLL_METHOD_ENABLED,
                                                              XA_INTEGER, 3, &pdata[0]);
                    if (success)
                    {
                        synaptics_two_scroll = (gint) pdata[0].c;
                        synaptics_edge_scroll = (gint) pdata[1].c;
                        synaptics_circ_scroll = -1; /* libinput does not expose this method */
                    }

                    success = mouse_settings_get_device_prop (xdisplay,
                                                              device,
                                                              LIBINPUT_PROP_SCROLL_METHODS_AVAILABLE,
                                                              XA_INTEGER, 3, &pdata[0]);
                    if (success)
                    {
                        if (!pdata[0].c)
                            synaptics_two_scroll = -1;
                        if (!pdata[1].c)
                            synaptics_edge_scroll = -1;
                    }
                }
#endif /* HAVE_LIBINPUT */
            }

            XFree (props);
        }
#endif /* DEVICE_PROPERTIES || HAVE_LIBINPUT */

        /* close the device */
        XCloseDevice (xdisplay, device);
    }

    /* update button order */
    object = gtk_builder_get_object (builder, left_handed ? "device-left-handed" : "device-right-handed");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), TRUE);

    object = gtk_builder_get_object (builder, "device-reverse-scrolling");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), reverse_scrolling);
    gtk_widget_set_sensitive (GTK_WIDGET (object), nbuttons >= 5);

    /* update acceleration scale */
    object = gtk_builder_get_object (builder, "device-acceleration-scale");
    gtk_range_set_value (GTK_RANGE (object), acceleration);
    gtk_widget_set_sensitive (GTK_WIDGET (object), acceleration != -1);

    /* update threshold scale */
    object = gtk_builder_get_object (builder, "device-threshold-scale");
    gtk_range_set_value (GTK_RANGE (object), threshold);
    gtk_widget_set_visible (GTK_WIDGET (object), threshold != -1);
    object = gtk_builder_get_object (builder, "device-threshold-label");
    gtk_widget_set_visible (GTK_WIDGET (object), threshold != -1);

    object = gtk_builder_get_object (builder, "device-enabled");
#ifdef DEVICE_PROPERTIES
    gtk_widget_set_sensitive (GTK_WIDGET (object), is_enabled != -1);
    gtk_switch_set_active (GTK_SWITCH (object), is_enabled > 0);

    object = gtk_builder_get_object (builder, "device-notebook");
    gtk_widget_set_sensitive (GTK_WIDGET (object), is_enabled == 1);
#else
    gtk_widget_set_visible (GTK_WIDGET (object), FALSE);
#endif

#ifdef HAVE_LIBINPUT
    object = gtk_builder_get_object (builder, "device-reset-feedback");
    gtk_widget_set_visible (GTK_WIDGET (object), !is_libinput);
#endif /* HAVE_LIBINPUT */

    /* synaptics options */
    object = gtk_builder_get_object (builder, "synaptics-tab");
    gtk_widget_set_visible (GTK_WIDGET (object), is_synaptics);

#if defined(DEVICE_PROPERTIES) || defined (HAVE_LIBINPUT)
    if (is_synaptics)
    {
        object = gtk_builder_get_object (builder, "synaptics-tap-to-click");
        gtk_widget_set_sensitive (GTK_WIDGET (object), synaptics_tap_to_click != -1);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), synaptics_tap_to_click > 0);

        /* Values for synaptics_scroll_mode:
         * -1 no selection
         *  0 disabled
         *  1 edge scrolling
         *  2 two-finger scrolling
         *  3 circular scrolling
         */
        if (synaptics_edge_scroll > 0)
            synaptics_scroll_mode = 1;

        if (synaptics_two_scroll > 0)
            synaptics_scroll_mode = 2;

        if (synaptics_circ_scroll > 0)
            synaptics_scroll_mode = 3;

        object = gtk_builder_get_object (builder, "synaptics-scroll-store");
        if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (object), &iter, NULL, 1))
            gtk_list_store_set (GTK_LIST_STORE (object), &iter, 1, synaptics_edge_scroll != -1, -1);

        if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (object), &iter, NULL, 2))
            gtk_list_store_set (GTK_LIST_STORE (object), &iter, 1, synaptics_two_scroll != -1, -1);

        if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (object), &iter, NULL, 3))
            gtk_list_store_set (GTK_LIST_STORE (object), &iter, 1, synaptics_circ_scroll != -1, -1);

        object = gtk_builder_get_object (builder, "synaptics-scroll");
        gtk_combo_box_set_active (GTK_COMBO_BOX (object), synaptics_scroll_mode);

        object = gtk_builder_get_object (builder, "synaptics-scroll-horiz");
        mouse_settings_synaptics_hscroll_sensitive (builder);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object),
                                      synaptics_edge_hscroll == 1 || synaptics_two_hscroll == 1);
#ifdef HAVE_LIBINPUT
        gtk_widget_set_visible (GTK_WIDGET (object), !is_libinput);

        object = gtk_builder_get_object (builder, "synaptics-disable-while-type");
        gtk_widget_set_visible (GTK_WIDGET (object), !is_libinput);

        object = gtk_builder_get_object (builder, "synaptics-disable-duration-box");
        gtk_widget_set_visible (GTK_WIDGET (object), !is_libinput);
#endif /* HAVE_LIBINPUT */
    }
#endif /* DEVICE_PROPERTIES || HAVE_LIBINPUT */

    /* wacom options */
    object = gtk_builder_get_object (builder, "wacom-tab");
    gtk_widget_set_visible (GTK_WIDGET (object), is_wacom);

#ifdef DEVICE_PROPERTIES
    if (is_wacom)
    {
        object = gtk_builder_get_object (builder, "wacom-mode");
        gtk_widget_set_sensitive (GTK_WIDGET (object), wacom_mode != -1);
        gtk_combo_box_set_active (GTK_COMBO_BOX (object), wacom_mode == -1 ? 1 : wacom_mode);

        object = gtk_builder_get_object (builder, "wacom-rotation");
        gtk_widget_set_sensitive (GTK_WIDGET (object), wacom_rotation != -1);
        /* 3 (half) comes after none */
        if (wacom_rotation == 3)
            wacom_rotation = 1;
        else if (wacom_rotation > 0)
            wacom_rotation++;
        else if (wacom_rotation == -1)
            wacom_rotation = 0;
        gtk_combo_box_set_active (GTK_COMBO_BOX (object), wacom_rotation);
    }
#endif

    /* unlock */
    locked--;
}



static void
mouse_settings_device_save (GtkBuilder *builder)
{
    GObject          *combobox;
    GtkTreeModel     *model;
    GtkTreeIter       iter;
    gchar            *name;
    GObject          *object;
    gchar             property_name[512];
    gboolean          righthanded;
    gint              threshold;
    gdouble           acceleration;
    gboolean          reverse_scrolling;

    /* leave when locked */
    if (locked > 0)
        return;

    combobox = gtk_builder_get_object (builder, "device-combobox");
    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combobox), &iter))
    {
        /* get device id and number of buttons */
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
        gtk_tree_model_get (model, &iter, COLUMN_DEVICE_XFCONF_NAME, &name, -1);

        if (G_LIKELY (name))
        {
            /* store the button order */
            object = gtk_builder_get_object (builder, "device-right-handed");
            g_snprintf (property_name, sizeof (property_name), "/%s/RightHanded", name);
            righthanded = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object));
            if (!xfconf_channel_has_property (pointers_channel, property_name)
                || xfconf_channel_get_bool (pointers_channel, property_name, TRUE) != righthanded)
                xfconf_channel_set_bool (pointers_channel, property_name, righthanded);

            /* store reverse scrolling */
            object = gtk_builder_get_object (builder, "device-reverse-scrolling");
            g_snprintf (property_name, sizeof (property_name), "/%s/ReverseScrolling", name);
            reverse_scrolling = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object));
            if (xfconf_channel_get_bool (pointers_channel, property_name, FALSE) != reverse_scrolling)
                xfconf_channel_set_bool (pointers_channel, property_name, reverse_scrolling);

            /* store the threshold */
            object = gtk_builder_get_object (builder, "device-threshold-scale");
            g_snprintf (property_name, sizeof (property_name), "/%s/Threshold", name);
            threshold = gtk_range_get_value (GTK_RANGE (object));
            if (xfconf_channel_get_int (pointers_channel, property_name, -1) != threshold)
                xfconf_channel_set_int (pointers_channel, property_name, threshold);

            /* store the acceleration */
            object = gtk_builder_get_object (builder, "device-acceleration-scale");
            g_snprintf (property_name, sizeof (property_name), "/%s/Acceleration", name);
            acceleration = gtk_range_get_value (GTK_RANGE (object));
            if (xfconf_channel_get_double (pointers_channel, property_name, -1) != acceleration)
                xfconf_channel_set_double (pointers_channel, property_name, acceleration);

            /* cleanup */
            g_free (name);
        }
    }
}



static gchar *
mouse_settings_device_xfconf_name (const gchar *name)
{
    GString     *string;
    const gchar *p;

    /* NOTE: this function exists in both the dialog and
     *       helper code and they have to identical! */

    /* allocate a string */
    string = g_string_sized_new (strlen (name));

    /* create a name with only valid chars */
    for (p = name; *p != '\0'; p++)
    {
        if ((*p >= 'A' && *p <= 'Z')
            || (*p >= 'a' && *p <= 'z')
            || (*p >= '0' && *p <= '9')
            || *p == '_' || *p == '-')
          string = g_string_append_c (string, *p);
        else if (*p == ' ')
            string = g_string_append_c (string, '_');
    }

    /* return the new string */
    return g_string_free (string, FALSE);
}



static void
mouse_settings_device_populate_store (GtkBuilder *builder,
                                      gboolean    create_store)
{
    XDeviceInfo     *device_list, *device_info;
    gint             ndevices;
    gint             i;
    GtkTreeIter      iter;
    GtkListStore    *store;
    GObject         *combobox;
    GtkCellRenderer *renderer;
    gchar           *xfconf_name;
    gboolean         has_active_item = FALSE;

    /* lock */
    locked++;

    combobox = gtk_builder_get_object (builder, "device-combobox");

    /* create or get the store */
    if (G_LIKELY (create_store))
    {
        store = gtk_list_store_new (N_DEVICE_COLUMNS,
                                    G_TYPE_STRING /* COLUMN_DEVICE_NAME */,
                                    G_TYPE_STRING /* COLUMN_DEVICE_XFCONF_NAME */,
                                    G_TYPE_ULONG /* COLUMN_DEVICE_XID */);
        gtk_combo_box_set_model (GTK_COMBO_BOX (combobox), GTK_TREE_MODEL (store));

        /* text renderer */
        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "text", COLUMN_DEVICE_NAME, NULL);

        g_signal_connect_swapped (G_OBJECT (combobox), "changed",
            G_CALLBACK (mouse_settings_device_selection_changed), builder);
    }
    else
    {
        store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (combobox)));
        gtk_list_store_clear (store);
    }

    /* get all the registered devices */
    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    device_list = XListInputDevices (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), &ndevices);
    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0 || device_list == NULL)
    {
        g_message ("No devices found");
        goto bailout;
    }

    for (i = 0; i < ndevices; i++)
    {
        /* get the device */
        device_info = &device_list[i];

        /* filter out the pointer and virtual devices */
        if (device_info->use != IsXExtensionPointer
            || g_str_has_prefix (device_info->name, "Virtual core XTEST"))
            continue;

        /* cannot go any further without device name */
        if (device_info->name == NULL)
            continue;

        /* create a valid xfconf device name */
        xfconf_name = mouse_settings_device_xfconf_name (device_info->name);

        /* insert in the store */
        gtk_list_store_insert_with_values (store, &iter, i,
                                           COLUMN_DEVICE_XFCONF_NAME, xfconf_name,
                                           COLUMN_DEVICE_NAME, device_info->name,
                                           COLUMN_DEVICE_XID, device_info->id,
                                           -1);

        /* check if we should select this device */
        if (opt_device_name != NULL
            && strcmp (opt_device_name, device_info->name) == 0)
        {
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
            g_free (opt_device_name);
            opt_device_name = NULL;
            has_active_item = TRUE;
        }

        g_free (xfconf_name);

    }

    XFreeDeviceList (device_list);

    if (!has_active_item)
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);

    bailout:

    /* unlock */
    locked--;
}



static gboolean
mouse_settings_device_update_sliders (gpointer user_data)
{
    GtkBuilder *builder = GTK_BUILDER (user_data);
    GObject    *button;

    /* update */
    mouse_settings_device_selection_changed (builder);

    /* make the button sensitive again */
    button = gtk_builder_get_object (builder, "device-reset-feedback");
    gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);

    return FALSE;
}



static void
mouse_settings_device_list_changed_timeout_destroyed (gpointer user_data)
{
    /* reset the timeout id */
    timeout_id = 0;
}



static void
mouse_settings_device_reset (GtkWidget  *button,
                             GtkBuilder *builder)
{
    gchar        *name, *property_name;
    GtkTreeModel *model;
    GtkTreeIter   iter;
    GObject      *combobox;

    /* leave when locked */
    if (locked > 0)
        return;

    /* get the selected item */
    combobox = gtk_builder_get_object (builder, "device-combobox");
    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combobox), &iter))
    {
        /* get device id and number of buttons */
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
        gtk_tree_model_get (model, &iter, COLUMN_DEVICE_XFCONF_NAME, &name, -1);

        if (G_LIKELY (name != NULL && timeout_id == 0))
        {
            /* make the button insensitive */
            gtk_widget_set_sensitive (button, FALSE);

            /* set the threshold to -1 */
            property_name = g_strdup_printf ("/%s/Threshold", name);
            xfconf_channel_set_int (pointers_channel, property_name, -1);
            g_free (property_name);

            /* set the acceleration to -1 */
            property_name = g_strdup_printf ("/%s/Acceleration", name);
            xfconf_channel_set_double (pointers_channel, property_name, -1.00);
            g_free (property_name);

            /* update the sliders in 500ms */
            timeout_id = g_timeout_add_full (G_PRIORITY_LOW, 500, mouse_settings_device_update_sliders,
                                             builder, mouse_settings_device_list_changed_timeout_destroyed);
        }

        /* cleanup */
        g_free (name);
    }
}



#ifdef DEVICE_HOTPLUGGING
static GdkFilterReturn
mouse_settings_event_filter (GdkXEvent *xevent,
                             GdkEvent  *gdk_event,
                             gpointer   user_data)
{
    XEvent                     *event = xevent;
    XDevicePresenceNotifyEvent *dpn_event = xevent;

    /* update on device changes */
    if (event->type == device_presence_event_type
        && (dpn_event->devchange == DeviceAdded
            || dpn_event->devchange == DeviceRemoved))
        mouse_settings_device_populate_store (GTK_BUILDER (user_data), FALSE);

    return GDK_FILTER_CONTINUE;
}



static void
mouse_settings_create_event_filter (GtkBuilder *builder)
{
    Display     *xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    XEventClass  event_class;

    /* monitor device change events */
    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    DevicePresence (xdisplay, device_presence_event_type, event_class);
    XSelectExtensionEvent (xdisplay, RootWindow (xdisplay, DefaultScreen (xdisplay)), &event_class, 1);
    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
    {
        g_critical ("Failed to setup the device event filter");
        return;
    }

    /* add an event filter */
    gdk_window_add_filter (NULL, mouse_settings_event_filter, builder);
}
#endif



static void
mouse_settings_dialog_response (GtkWidget *dialog,
                                gint       response_id)
{
    if (response_id == GTK_RESPONSE_HELP)
        xfce_dialog_show_help_with_version (GTK_WINDOW (dialog), "xfce4-settings", "mouse",
                                            NULL, XFCE4_SETTINGS_VERSION_SHORT);
    else
        gtk_main_quit ();
}



gint
main (gint argc, gchar **argv)
{
    GObject           *dialog;
    GtkWidget         *plug;
    GObject           *plug_child;
    GtkBuilder        *builder;
    GError            *error = NULL;
    GObject           *object;
    XExtensionVersion *version = NULL;
#ifdef DEVICE_PROPERTIES
    gchar             *syndaemon;
    GObject           *synaptics_disable_while_type;
    GObject           *synaptics_disable_duration_table;
#endif

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize Gtk+ */
    if (!gtk_init_with_args (&argc, &argv, NULL, option_entries, GETTEXT_PACKAGE, &error))
    {
        if (G_LIKELY (error))
        {
            /* print error */
            g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print ("\n");

            /* cleanup */
            g_error_free (error);
        }
        else
        {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    /* print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2004-2023");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* initialize xfconf */
    if (G_UNLIKELY (!xfconf_init (&error)))
    {
        /* print error and leave */
        g_critical ("Failed to connect to Xfconf daemon: %s", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* check for Xi */
    version = XGetExtensionVersion (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), INAME);
    if (version == NULL || ((long) version) == NoSuchExtension
        || !version->present)
    {
        g_critical ("XI is not present.");
        return EXIT_FAILURE;
    }
    else if (version->major_version < MIN_XI_VERS_MAJOR
             || (version->major_version == MIN_XI_VERS_MAJOR
                 && version->minor_version < MIN_XI_VERS_MINOR))
    {
        g_critical ("Your XI is too old (%d.%d) version %d.%d is required.",
                    version->major_version, version->minor_version,
                    MIN_XI_VERS_MAJOR, MIN_XI_VERS_MINOR);
        return EXIT_FAILURE;
    }

    /* hook to make sure the libxfce4ui library is linked */
    if (xfce_titled_dialog_get_type () == 0)
        return EXIT_FAILURE;

    /* open the xsettings and pointers channel */
    xsettings_channel = xfconf_channel_new ("xsettings");
    pointers_channel = xfconf_channel_new ("pointers");

    if (G_LIKELY (pointers_channel && xsettings_channel))
    {
        GSettingsSchemaSource *source = g_settings_schema_source_get_default ();
        if (source != NULL)
          {
            GSettingsSchema *schema = g_settings_schema_source_lookup (source, "org.gnome.desktop.interface", TRUE);
            if (schema != NULL)
              {
                desktop_interface_gsettings = g_settings_new ("org.gnome.desktop.interface");
                g_settings_schema_unref (schema);
              }
          }

        /* load the Gtk+ user-interface file */
        builder = gtk_builder_new ();
        if (gtk_builder_add_from_string (builder, mouse_dialog_ui,
                                         mouse_dialog_ui_length, &error) != 0)
        {
            /* lock */
            locked++;

            /* populate the devices combobox */
            mouse_settings_device_populate_store (builder, TRUE);

            /* connect signals */
#ifdef DEVICE_PROPERTIES
            object = gtk_builder_get_object (builder, "device-enabled");
            g_signal_connect (G_OBJECT (object), "notify::active",
                              G_CALLBACK (mouse_settings_device_set_enabled), builder);
#endif

            object = gtk_builder_get_object (builder, "device-acceleration-scale");
            g_signal_connect_swapped (G_OBJECT (object), "value-changed",
                                      G_CALLBACK (mouse_settings_device_save), builder);

            object = gtk_builder_get_object (builder, "device-threshold-scale");
            g_signal_connect (G_OBJECT (object), "format-value",
                              G_CALLBACK (mouse_settings_format_value_px), NULL);
            g_signal_connect_swapped (G_OBJECT (object), "value-changed",
                                      G_CALLBACK (mouse_settings_device_save), builder);

            object = gtk_builder_get_object (builder, "device-left-handed");
            g_signal_connect_swapped (G_OBJECT (object), "toggled",
                                      G_CALLBACK (mouse_settings_device_save), builder);

            object = gtk_builder_get_object (builder, "device-right-handed");
            g_signal_connect_swapped (G_OBJECT (object), "toggled",
                                      G_CALLBACK (mouse_settings_device_save), builder);

            object = gtk_builder_get_object (builder, "device-reverse-scrolling");
            g_signal_connect_swapped (G_OBJECT (object), "toggled",
                                      G_CALLBACK (mouse_settings_device_save), builder);

            object = gtk_builder_get_object (builder, "device-reset-feedback");
            g_signal_connect (G_OBJECT (object), "clicked",
                              G_CALLBACK (mouse_settings_device_reset), builder);

#if defined (DEVICE_PROPERTIES) || defined (HAVE_LIBINPUT)
            synaptics_disable_while_type = gtk_builder_get_object (builder, "synaptics-disable-while-type");
            syndaemon = g_find_program_in_path ("syndaemon");
            gtk_widget_set_sensitive (GTK_WIDGET (object), syndaemon != NULL);
            g_free (syndaemon);
            xfconf_g_property_bind (pointers_channel, "/DisableTouchpadWhileTyping",
                                    G_TYPE_BOOLEAN, G_OBJECT (synaptics_disable_while_type), "active");

            synaptics_disable_duration_table = gtk_builder_get_object (builder, "synaptics-disable-duration-box");

            g_object_bind_property (G_OBJECT (synaptics_disable_while_type), "active",
                                    G_OBJECT (synaptics_disable_duration_table), "sensitive",
                                    G_BINDING_SYNC_CREATE);

            object = gtk_builder_get_object (builder, "synaptics-disable-duration-scale");
            g_signal_connect (G_OBJECT (object), "format-value",
                              G_CALLBACK (mouse_settings_format_value_s), NULL);

            object = gtk_builder_get_object (builder, "synaptics-disable-duration");
            xfconf_g_property_bind (pointers_channel, "/DisableTouchpadDuration",
                                    G_TYPE_DOUBLE, G_OBJECT (object), "value");

            object = gtk_builder_get_object (builder, "synaptics-tap-to-click");
            g_signal_connect_swapped (G_OBJECT (object), "toggled",
                                      G_CALLBACK (mouse_settings_synaptics_set_tap_to_click), builder);

            object = gtk_builder_get_object (builder, "synaptics-scroll");
            g_signal_connect (G_OBJECT (object), "changed",
                              G_CALLBACK (mouse_settings_synaptics_set_scrolling), builder);

            object = gtk_builder_get_object (builder, "synaptics-scroll-horiz");
            g_signal_connect (G_OBJECT (object), "toggled",
                              G_CALLBACK (mouse_settings_synaptics_set_scroll_horiz), builder);

            object = gtk_builder_get_object (builder, "wacom-mode");
            g_signal_connect (G_OBJECT (object), "changed",
                              G_CALLBACK (mouse_settings_wacom_set_mode), builder);

            object = gtk_builder_get_object (builder, "wacom-rotation");
            g_signal_connect (G_OBJECT (object), "changed",
                              G_CALLBACK (mouse_settings_wacom_set_rotation), builder);
#endif /* DEVICE_PROPERTIES || HAVE_LIBINPUT */

#ifdef HAVE_XCURSOR
            /* populate the themes treeview */
            mouse_settings_themes_populate_store (builder);

            /* connect the cursor size in the cursor tab */
            object = gtk_builder_get_object (builder, "theme-cursor-size");
            xfconf_g_property_bind (xsettings_channel, "/Gtk/CursorThemeSize",
                                    G_TYPE_INT, G_OBJECT (object), "value");
#else
            /* hide the themes tab */
            object = gtk_builder_get_object (builder, "themes-hbox");
            gtk_widget_hide (GTK_WIDGET (object));
#endif /* !HAVE_XCURSOR */

            /* connect sliders in the gtk tab */
            object = gtk_builder_get_object (builder, "dnd-threshold");
            xfconf_g_property_bind (xsettings_channel, "/Net/DndDragThreshold",
                                    G_TYPE_INT, G_OBJECT (object), "value");\

            object = gtk_builder_get_object (builder, "dnd-threshold-scale");
            g_signal_connect (G_OBJECT (object), "format-value",
                              G_CALLBACK (mouse_settings_format_value_px), NULL);

            object = gtk_builder_get_object (builder, "dclick-time");
            xfconf_g_property_bind (xsettings_channel, "/Net/DoubleClickTime",
                                    G_TYPE_INT, G_OBJECT (object), "value");

            object = gtk_builder_get_object (builder, "dclick-time-scale");
            g_signal_connect (G_OBJECT (object), "format-value",
                              G_CALLBACK (mouse_settings_format_value_ms), NULL);

            object = gtk_builder_get_object (builder, "dclick-distance");
            xfconf_g_property_bind (xsettings_channel, "/Net/DoubleClickDistance",
                                    G_TYPE_INT, G_OBJECT (object), "value");

            object = gtk_builder_get_object (builder, "dclick-distance-scale");
            g_signal_connect (G_OBJECT (object), "format-value",
                              G_CALLBACK (mouse_settings_format_value_px), NULL);

#ifdef DEVICE_HOTPLUGGING
            /* create the event filter for device monitoring */
            mouse_settings_create_event_filter (builder);
#endif

            if (G_UNLIKELY (opt_socket_id == 0))
            {
                /* get the dialog */
                dialog = gtk_builder_get_object (builder, "mouse-dialog");

                /* unlock */
                locked--;

                g_signal_connect (dialog, "response",
                    G_CALLBACK (mouse_settings_dialog_response), NULL);
                gtk_window_present (GTK_WINDOW (dialog));

                /* To prevent the settings dialog to be saved in the session */
                gdk_x11_set_sm_client_id ("FAKE ID");

                gtk_main ();

                gtk_widget_destroy (GTK_WIDGET (dialog));
            }
            else
            {
                /* Create plug widget */
                plug = gtk_plug_new (opt_socket_id);
                g_signal_connect (plug, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
                gtk_widget_show (plug);

                /* Stop startup notification */
                gdk_notify_startup_complete ();

                /* Get plug child widget */
                plug_child = gtk_builder_get_object (builder, "plug-child");
                xfce_widget_reparent (GTK_WIDGET (plug_child), plug);
                gtk_widget_show (GTK_WIDGET (plug_child));

                /* Unlock */
                locked--;

                /* To prevent the settings dialog to be saved in the session */
                gdk_x11_set_sm_client_id ("FAKE ID");

                /* Enter main loop */
                gtk_main ();
            }
        }
        else
        {
            g_error ("Failed to load the UI file: %s.", error->message);
            g_error_free (error);
        }

        /* release the Gtk+ user-interface file */
        g_object_unref (G_OBJECT (builder));

        /* release the channels */
        g_object_unref (G_OBJECT (xsettings_channel));
        g_object_unref (G_OBJECT (pointers_channel));
        if (desktop_interface_gsettings != NULL)
          g_object_unref (desktop_interface_gsettings);
    }

    /* shutdown xfconf */
    xfconf_shutdown ();

    /* cleanup */
    g_free (opt_device_name);

    return EXIT_SUCCESS;
}
