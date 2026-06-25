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
#include "monitor_chooser.h"
#include "gnome_wallpaper_manager.h"

#include <libxfce4ui/libxfce4ui.h>



enum MonitorStyle
{
    MONITOR_STYLE_AUTOMATIC = 0,
    MONITOR_STYLE_CENTERED,
    MONITOR_STYLE_TILED,
    MONITOR_STYLE_STRETCHED,
    MONITOR_STYLE_SCALED,
    MONITOR_STYLE_ZOOMED
};

enum ColorStyle
{
    COLOR_STYLE_SOLID = 0,
    COLOR_STYLE_HORIZONTAL_GRADIENT,
    COLOR_STYLE_VERTICAL_GRADIENT
};

static RsttoWallpaperManager *gnome_wallpaper_manager_object = NULL;



static void
rstto_gnome_wallpaper_manager_iface_init (RsttoWallpaperManagerInterface *iface);

static void
rstto_gnome_wallpaper_manager_finalize (GObject *object);

static void
cb_monitor_chooser_changed (RsttoMonitorChooser *monitor_chooser,
                            RsttoGnomeWallpaperManager *manager);
static void
cb_style_combo_changed (GtkComboBox *style_combo,
                        RsttoGnomeWallpaperManager *manager);
static void
configure_monitor_chooser_pixbuf (RsttoGnomeWallpaperManager *manager);



struct _RsttoGnomeWallpaperManagerPrivate
{
    gint screen;
    gint monitor;
    enum MonitorStyle style;

    RsttoFile *file;
    GdkPixbuf *pixbuf;

    GtkWidget *monitor_chooser;
    GtkWidget *style_combo;

    GtkWidget *dialog;
};



G_DEFINE_TYPE_WITH_CODE (RsttoGnomeWallpaperManager, rstto_gnome_wallpaper_manager, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (RsttoGnomeWallpaperManager)
                         G_IMPLEMENT_INTERFACE (RSTTO_WALLPAPER_MANAGER_TYPE,
                                                rstto_gnome_wallpaper_manager_iface_init))



static gint
rstto_gnome_wallpaper_manager_configure_dialog_run (
        RsttoWallpaperManager *self,
        RsttoFile *file,
        GtkWindow *parent)
{
    RsttoGnomeWallpaperManager *manager = RSTTO_GNOME_WALLPAPER_MANAGER (self);
    gint response = GTK_RESPONSE_OK;
    manager->priv->file = file;

    if (manager->priv->pixbuf)
    {
        g_object_unref (manager->priv->pixbuf);
    }

    manager->priv->pixbuf = gdk_pixbuf_new_from_file_at_size (
            rstto_file_get_path (file),
            500,
            500,
            NULL);

    configure_monitor_chooser_pixbuf (manager);

    gtk_window_set_transient_for (GTK_WINDOW (manager->priv->dialog), parent);
    response = gtk_dialog_run (GTK_DIALOG (manager->priv->dialog));
    gtk_widget_hide (manager->priv->dialog);
    if ((response == GTK_RESPONSE_OK) || (response == GTK_RESPONSE_APPLY))
    {
        manager->priv->style = gtk_combo_box_get_active (
                GTK_COMBO_BOX (manager->priv->style_combo));
        manager->priv->monitor = rstto_monitor_chooser_get_selected (
                RSTTO_MONITOR_CHOOSER (manager->priv->monitor_chooser));
    }

    return response;
}

static gboolean
rstto_gnome_wallpaper_manager_check_running (
        RsttoWallpaperManager *self)
{
    return TRUE;
}

static gboolean
rstto_gnome_wallpaper_manager_set (
        RsttoWallpaperManager *self,
        RsttoFile *file)
{
    gchar *command = NULL;

    command = g_strdup_printf (
            "gsettings set org.gnome.desktop.background picture-uri %s",
            rstto_file_get_uri (file));

    g_spawn_command_line_async (command, NULL);
    g_free (command);

    command = g_strdup_printf (
            "gsettings set org.gnome.desktop.background picture-options %s",
            "scaled");

    g_spawn_command_line_async (command, NULL);
    g_free (command);

    command = g_strdup_printf (
            "gsettings set org.gnome.desktop.background primary-color '#000000000000'");

    g_spawn_command_line_async (command, NULL);
    g_free (command);

    return FALSE;
}

static void
rstto_gnome_wallpaper_manager_iface_init (RsttoWallpaperManagerInterface *iface)
{
    iface->configure_dialog_run = rstto_gnome_wallpaper_manager_configure_dialog_run;
    iface->check_running = rstto_gnome_wallpaper_manager_check_running;
    iface->set = rstto_gnome_wallpaper_manager_set;
}

static void
rstto_gnome_wallpaper_manager_init (RsttoGnomeWallpaperManager *manager)
{
    GtkWidget *image_prop_grid = gtk_grid_new ();
    GtkWidget *style_label = gtk_label_new (_("Style:"));
    GtkWidget *vbox;
    GtkWidget *button;

    GdkDisplay *display = gdk_display_get_default ();
    gint n_monitors = gdk_display_get_n_monitors (display);
    GdkRectangle monitor_geometry;
    gint i;

    manager->priv = rstto_gnome_wallpaper_manager_get_instance_private (manager);

    manager->priv->dialog = gtk_dialog_new ();
    gtk_window_set_title (GTK_WINDOW (manager->priv->dialog), _("Set as wallpaper"));

    button = xfce_gtk_button_new_mixed ("gtk-cancel", _("_Cancel"));
    gtk_dialog_add_action_widget (GTK_DIALOG (manager->priv->dialog), button, GTK_RESPONSE_CANCEL);
    gtk_widget_show (button);
    button = xfce_gtk_button_new_mixed ("gtk-apply", _("_Apply"));
    gtk_dialog_add_action_widget (GTK_DIALOG (manager->priv->dialog), button, GTK_RESPONSE_APPLY);
    gtk_widget_show (button);
    button = xfce_gtk_button_new_mixed ("gtk-ok", _("_OK"));
    gtk_dialog_add_action_widget (GTK_DIALOG (manager->priv->dialog), button, GTK_RESPONSE_OK);
    gtk_widget_show (button);

    vbox = gtk_dialog_get_content_area (GTK_DIALOG (manager->priv->dialog));

    manager->priv->monitor_chooser = rstto_monitor_chooser_new ();
    manager->priv->style_combo = gtk_combo_box_text_new ();

    for (i = 0; i < n_monitors; ++i)
    {
        gdk_monitor_get_geometry (
                gdk_display_get_monitor (display, i),
                &monitor_geometry);
        rstto_monitor_chooser_add (
                RSTTO_MONITOR_CHOOSER (manager->priv->monitor_chooser),
                monitor_geometry.width,
                monitor_geometry.height);
    }
    gtk_box_pack_start (
            GTK_BOX (vbox),
            manager->priv->monitor_chooser,
            FALSE,
            FALSE,
            0);
    gtk_box_pack_start (
            GTK_BOX (vbox),
            image_prop_grid,
            FALSE,
            FALSE,
            0);

    gtk_grid_attach (
            GTK_GRID (image_prop_grid),
            style_label,
            0,
            0,
            1,
            1);
    gtk_grid_attach (
            GTK_GRID (image_prop_grid),
            manager->priv->style_combo,
            1,
            0,
            1,
            1);

    gtk_widget_set_hexpand (manager->priv->style_combo, TRUE);
    gtk_combo_box_text_append_text (
            GTK_COMBO_BOX_TEXT (manager->priv->style_combo),
            _("Auto"));

    gtk_combo_box_set_active (
            GTK_COMBO_BOX (manager->priv->style_combo),
            0);

    // there's only one screen in GTK3
    manager->priv->screen = 0;

    gtk_window_set_resizable (GTK_WINDOW (manager->priv->dialog), FALSE);

    g_signal_connect (manager->priv->monitor_chooser, "changed",
                      G_CALLBACK (cb_monitor_chooser_changed), manager);
    g_signal_connect (manager->priv->style_combo, "changed",
                      G_CALLBACK (cb_style_combo_changed), manager);

    gtk_widget_show_all (vbox);
}


static void
rstto_gnome_wallpaper_manager_class_init (RsttoGnomeWallpaperManagerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = rstto_gnome_wallpaper_manager_finalize;
}

/**
 * rstto_gnome_wallpaper_manager_finalize:
 * @object:
 *
 */
static void
rstto_gnome_wallpaper_manager_finalize (GObject *object)
{
    if (gnome_wallpaper_manager_object)
    {
        gnome_wallpaper_manager_object = NULL;
    }

    G_OBJECT_CLASS (rstto_gnome_wallpaper_manager_parent_class)->finalize (object);
}



/**
 * rstto_gnome_wallpaper_manager_new:
 *
 *
 * Singleton
 */
RsttoWallpaperManager *
rstto_gnome_wallpaper_manager_new (void)
{
    if (gnome_wallpaper_manager_object == NULL)
    {
        gnome_wallpaper_manager_object = g_object_new (RSTTO_TYPE_GNOME_WALLPAPER_MANAGER, NULL);
    }
    else
    {
        g_object_ref (gnome_wallpaper_manager_object);
    }

    return gnome_wallpaper_manager_object;
}


static void
cb_monitor_chooser_changed (
        RsttoMonitorChooser *monitor_chooser,
        RsttoGnomeWallpaperManager *manager)
{
    rstto_monitor_chooser_set_image_surface (
            monitor_chooser,
            manager->priv->monitor,
            NULL,
            NULL);

    manager->priv->monitor = rstto_monitor_chooser_get_selected (monitor_chooser);

    configure_monitor_chooser_pixbuf (manager);
}

static void
configure_monitor_chooser_pixbuf (
    RsttoGnomeWallpaperManager *manager)
{
    cairo_surface_t *image_surface = NULL;
    cairo_t *ctx;
    GdkPixbuf *tmp_pixbuf;
    gint monitor_width, monitor_height, dest_width, dest_height,
         surface_width, surface_height, dest_x, dest_y;
    gdouble x_scale, y_scale;

    if (manager->priv->pixbuf)
    {
        tmp_pixbuf = gdk_pixbuf_copy (manager->priv->pixbuf);
        if (NULL != tmp_pixbuf)
        {
            rstto_monitor_chooser_get_dimensions (
                    RSTTO_MONITOR_CHOOSER (manager->priv->monitor_chooser),
                    manager->priv->monitor,
                    &monitor_width,
                    &monitor_height);

            surface_width = monitor_width * 0.2;
            surface_height = monitor_height * 0.2;

            image_surface = cairo_image_surface_create (
                    CAIRO_FORMAT_ARGB32,
                    surface_width,
                    surface_height);
            ctx = cairo_create (image_surface);

            cairo_set_source_rgb (ctx, 0, 0, 0);
            cairo_paint (ctx);

            x_scale = (gdouble) surface_width / gdk_pixbuf_get_width (tmp_pixbuf);
            y_scale = (gdouble) surface_height / gdk_pixbuf_get_height (tmp_pixbuf);

            switch (manager->priv->style)
            {
                case MONITOR_STYLE_ZOOMED:
                    if (x_scale > y_scale)
                    {
                        dest_width = gdk_pixbuf_get_width (tmp_pixbuf) * x_scale;
                        dest_height = gdk_pixbuf_get_height (tmp_pixbuf) * x_scale;
                        dest_x = (surface_width - dest_width) / 2.0;
                        dest_y = (surface_height - dest_height) / 2.0;
                        y_scale = x_scale;
                    }
                    else
                    {
                        dest_width = gdk_pixbuf_get_width (tmp_pixbuf) * y_scale;
                        dest_height = gdk_pixbuf_get_height (tmp_pixbuf) * y_scale;
                        dest_x = (surface_width - dest_width) / 2.0;
                        dest_y = (surface_height - dest_height) / 2.0;
                        x_scale = y_scale;
                    }
                    break;
                case MONITOR_STYLE_STRETCHED:
                    dest_x = 0;
                    dest_y = 0;
                    break;
                case MONITOR_STYLE_AUTOMATIC:
                case MONITOR_STYLE_SCALED:
                    if (x_scale < y_scale)
                    {
                        dest_width = gdk_pixbuf_get_width (tmp_pixbuf) * x_scale;
                        dest_height = gdk_pixbuf_get_height (tmp_pixbuf) * x_scale;
                        dest_x = (surface_width - dest_width) / 2.0;
                        dest_y = (surface_height - dest_height) / 2.0;
                        y_scale = x_scale;
                    }
                    else
                    {
                        dest_width = gdk_pixbuf_get_width (tmp_pixbuf) * y_scale;
                        dest_height = gdk_pixbuf_get_height (tmp_pixbuf) * y_scale;
                        dest_x = (surface_width - dest_width) / 2.0;
                        dest_y = (surface_height - dest_height) / 2.0;
                        x_scale = y_scale;
                    }
                    break;
                default:
                    dest_x = 0;
                    dest_y = 0;
                    gdk_pixbuf_saturate_and_pixelate (
                        tmp_pixbuf,
                        tmp_pixbuf,
                        0.0,
                        TRUE);
                    break;
            }
            cairo_scale (ctx, x_scale, y_scale);
            gdk_cairo_set_source_pixbuf (
                    ctx,
                    tmp_pixbuf,
                    dest_x / x_scale,
                    dest_y / y_scale);
            cairo_paint (ctx);
            cairo_destroy (ctx);

            g_object_unref (tmp_pixbuf);
        }
    }

    rstto_monitor_chooser_set_image_surface (
            RSTTO_MONITOR_CHOOSER (manager->priv->monitor_chooser),
            manager->priv->monitor,
            image_surface,
            NULL);
}


static void
cb_style_combo_changed (
        GtkComboBox *style_combo,
        RsttoGnomeWallpaperManager *manager)
{

    manager->priv->style = gtk_combo_box_get_active (style_combo);

    manager->priv->monitor = rstto_monitor_chooser_get_selected (
            RSTTO_MONITOR_CHOOSER (manager->priv->monitor_chooser));

    configure_monitor_chooser_pixbuf (manager);
}

