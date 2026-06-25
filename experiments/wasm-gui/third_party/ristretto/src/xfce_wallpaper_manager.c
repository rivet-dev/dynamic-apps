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
#include "xfce_wallpaper_manager.h"

#ifdef HAVE_LIBX11
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#endif

#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>



#define XFDESKTOP_SELECTION_FMT "XFDESKTOP_SELECTION_%d"
#define SINGLE_WORKSPACE_MODE "/backdrop/single-workspace-mode"

enum MonitorStyle
{
    MONITOR_STYLE_AUTOMATIC = 0,
    MONITOR_STYLE_CENTERED,
    MONITOR_STYLE_TILED,
    MONITOR_STYLE_STRETCHED,
    MONITOR_STYLE_SCALED,
    MONITOR_STYLE_ZOOMED
};

static RsttoWallpaperManager *xfce_wallpaper_manager_object;



static void
rstto_xfce_wallpaper_manager_iface_init (RsttoWallpaperManagerInterface *iface);

static void
rstto_xfce_wallpaper_manager_finalize (GObject *object);

static gint
rstto_get_active_workspace_number (GdkScreen *screen);
static void
configure_monitor_chooser_pixbuf (RsttoXfceWallpaperManager *manager);
static void
cb_style_combo_changed (GtkComboBox *style_combo,
                        RsttoXfceWallpaperManager *manager);
static void
cb_monitor_chooser_changed (RsttoMonitorChooser *monitor_chooser,
                            RsttoXfceWallpaperManager *manager);
static void
cb_workspace_mode_changed (GtkCheckButton *check_button,
                           RsttoXfceWallpaperManager *manager);



typedef struct {
    guint16 r;
    guint16 g;
    guint16 b;
    guint16 a;
} RsttoColor;

struct _RsttoXfceWallpaperManagerPrivate
{
    XfconfChannel *channel;
    gint    screen;
    gint    monitor;
    enum MonitorStyle style;
    RsttoColor *color1;
    RsttoColor *color2;
    gboolean workspace_mode;

    RsttoFile *file;
    GdkPixbuf *pixbuf;

    GtkWidget *monitor_chooser;
    GtkWidget *style_combo;
    GtkWidget *check_button;

    GtkWidget *dialog;
};



G_DEFINE_TYPE_WITH_CODE (RsttoXfceWallpaperManager, rstto_xfce_wallpaper_manager, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (RsttoXfceWallpaperManager)
                         G_IMPLEMENT_INTERFACE (RSTTO_WALLPAPER_MANAGER_TYPE,
                                                rstto_xfce_wallpaper_manager_iface_init))



static gint
rstto_xfce_wallpaper_manager_configure_dialog_run (
        RsttoWallpaperManager *self,
        RsttoFile *file,
        GtkWindow *parent)
{
    RsttoXfceWallpaperManager *manager = RSTTO_XFCE_WALLPAPER_MANAGER (self);
    gint response = 0;
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
        manager->priv->workspace_mode = gtk_toggle_button_get_active (
                GTK_TOGGLE_BUTTON (manager->priv->check_button));
    }

    return response;
}

static gboolean
rstto_xfce_wallpaper_manager_check_running (RsttoWallpaperManager *self)
{
#ifdef HAVE_LIBX11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
        gchar selection_name[100];
        Atom xfce_selection_atom;
        gint xscreen = 0;
        Display *gdk_display = gdk_x11_get_default_xdisplay ();

        g_snprintf (selection_name, 100, XFDESKTOP_SELECTION_FMT, xscreen);

        xfce_selection_atom = XInternAtom (gdk_display, selection_name, False);
        if ((XGetSelectionOwner (gdk_display, xfce_selection_atom)))
        {
            return TRUE;
        }
    }
#endif

    return FALSE;
}

static gboolean
rstto_xfce_wallpaper_manager_set (RsttoWallpaperManager *self, RsttoFile *file)
{
    RsttoXfceWallpaperManager *manager = RSTTO_XFCE_WALLPAPER_MANAGER (self);

    const gchar *uri = rstto_file_get_path (file);
    GdkDisplay *display = gdk_display_get_default ();
    GdkScreen *gdk_screen = gdk_display_get_default_screen (display);
    const gint workspace_nr = rstto_get_active_workspace_number (gdk_screen);
    const gchar *monitor_name = gdk_monitor_get_model (gdk_display_get_monitor (display, manager->priv->monitor));

    gchar *image_path_prop;
    gchar *image_style_prop;
    gchar *monitor_prop;
    GHashTable *props;

    /* New properties since xfdesktop >= 4.11 */
    if (monitor_name)
    {
        image_path_prop = g_strdup_printf ("/backdrop/screen%d/monitor%s/workspace%d/last-image",
                                           manager->priv->screen, monitor_name, workspace_nr);
        image_style_prop = g_strdup_printf ("/backdrop/screen%d/monitor%s/workspace%d/image-style",
                                            manager->priv->screen, monitor_name, workspace_nr);

        if (manager->priv->channel != NULL)
        {
            xfconf_channel_set_string (manager->priv->channel, image_path_prop, uri);
            xfconf_channel_set_int (manager->priv->channel, image_style_prop, manager->priv->style);
        }

        g_free (image_path_prop);
        g_free (image_style_prop);
    }

    /* Support for xfdesktop < 4.13 (gtk2) */
    monitor_prop = g_strdup_printf ("/backdrop/screen%d/monitor%d",
                                    manager->priv->screen, manager->priv->monitor);
    if (manager->priv->channel != NULL
        && (props = xfconf_channel_get_properties (manager->priv->channel, monitor_prop)) != NULL)
    {
        image_path_prop = g_strdup_printf ("/backdrop/screen%d/monitor%d/workspace%d/last-image",
                                           manager->priv->screen, manager->priv->monitor, workspace_nr);
        image_style_prop = g_strdup_printf ("/backdrop/screen%d/monitor%d/workspace%d/image-style",
                                            manager->priv->screen, manager->priv->monitor, workspace_nr);

        xfconf_channel_set_string (manager->priv->channel, image_path_prop, uri);
        xfconf_channel_set_int (manager->priv->channel, image_style_prop, manager->priv->style);

        g_free (image_path_prop);
        g_free (image_style_prop);
        g_hash_table_unref (props);
    }

    /* Don't force to add 'single-workspace-mode' property */
    if (manager->priv->channel != NULL
        && xfconf_channel_has_property (manager->priv->channel, SINGLE_WORKSPACE_MODE))
    {
        xfconf_channel_set_bool (manager->priv->channel, SINGLE_WORKSPACE_MODE, manager->priv->workspace_mode);
    }

    g_free (monitor_prop);

    return FALSE;
}

static void
rstto_xfce_wallpaper_manager_iface_init (RsttoWallpaperManagerInterface *iface)
{
    iface->configure_dialog_run = rstto_xfce_wallpaper_manager_configure_dialog_run;
    iface->check_running = rstto_xfce_wallpaper_manager_check_running;
    iface->set = rstto_xfce_wallpaper_manager_set;
}

static void
rstto_xfce_wallpaper_manager_init (RsttoXfceWallpaperManager *manager)
{
    gint i;
    GdkDisplay *display = gdk_display_get_default ();
    gint n_monitors = gdk_display_get_n_monitors (display);
    GdkRectangle monitor_geometry;
    GtkWidget *vbox;
    GtkWidget *button;
    GtkWidget *style_label = gtk_label_new (_("Style:"));
    GtkWidget *image_prop_grid = gtk_grid_new ();

    manager->priv = rstto_xfce_wallpaper_manager_get_instance_private (manager);
    if (! rstto_settings_get_xfconf_disabled ())
        manager->priv->channel = xfconf_channel_new ("xfce4-desktop");
    manager->priv->color1 = g_new0 (RsttoColor, 1);
    manager->priv->color1->a = 0xffff;
    manager->priv->color2 = g_new0 (RsttoColor, 1);
    manager->priv->color2->a = 0xffff;
    manager->priv->style = 3; /* stretched is now default value */
    manager->priv->check_button = gtk_check_button_new_with_label (
            _("Apply to all workspaces"));
    if (manager->priv->channel != NULL)
        manager->priv->workspace_mode = xfconf_channel_get_bool (
                manager->priv->channel,
                SINGLE_WORKSPACE_MODE,
                TRUE);

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

    gtk_grid_set_row_spacing (GTK_GRID (image_prop_grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (image_prop_grid), 6);

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
    gtk_combo_box_text_append_text (
            GTK_COMBO_BOX_TEXT (manager->priv->style_combo),
            _("Centered"));
    gtk_combo_box_text_append_text (
            GTK_COMBO_BOX_TEXT (manager->priv->style_combo),
            _("Tiled"));
    gtk_combo_box_text_append_text (
            GTK_COMBO_BOX_TEXT (manager->priv->style_combo),
            _("Stretched"));
    gtk_combo_box_text_append_text (
            GTK_COMBO_BOX_TEXT (manager->priv->style_combo),
            _("Scaled"));
    gtk_combo_box_text_append_text (
            GTK_COMBO_BOX_TEXT (manager->priv->style_combo),
            _("Zoomed"));

    gtk_combo_box_set_active (
            GTK_COMBO_BOX (manager->priv->style_combo),
            3);

    // there's only one screen in GTK3
    manager->priv->screen = 0;

    gtk_window_set_resizable (GTK_WINDOW (manager->priv->dialog), FALSE);

    g_signal_connect (manager->priv->monitor_chooser, "changed",
                      G_CALLBACK (cb_monitor_chooser_changed), manager);
    g_signal_connect (manager->priv->style_combo, "changed",
                      G_CALLBACK (cb_style_combo_changed), manager);

    gtk_toggle_button_set_active (
            GTK_TOGGLE_BUTTON (manager->priv->check_button),
            manager->priv->workspace_mode);
    gtk_grid_attach (
            GTK_GRID (image_prop_grid),
            manager->priv->check_button,
            0,
            1,
            2,
            1);
    g_signal_connect (manager->priv->check_button,
            "toggled",
            G_CALLBACK (cb_workspace_mode_changed),
            manager);

    gtk_widget_show_all (vbox);
}


static void
rstto_xfce_wallpaper_manager_class_init (RsttoXfceWallpaperManagerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = rstto_xfce_wallpaper_manager_finalize;
}

/**
 * rstto_xfce_wallpaper_manager_finalize:
 * @object:
 *
 */
static void
rstto_xfce_wallpaper_manager_finalize (GObject *object)
{
    RsttoXfceWallpaperManager *xfce_wallpaper_manager = RSTTO_XFCE_WALLPAPER_MANAGER (object);

    if (xfce_wallpaper_manager->priv->channel)
    {
        g_object_unref (xfce_wallpaper_manager->priv->channel);
        xfce_wallpaper_manager->priv->channel = NULL;
    }
    g_free (xfce_wallpaper_manager->priv->color1);
    g_free (xfce_wallpaper_manager->priv->color2);

    if (xfce_wallpaper_manager_object)
    {
        xfce_wallpaper_manager_object = NULL;
    }

    G_OBJECT_CLASS (rstto_xfce_wallpaper_manager_parent_class)->finalize (object);
}



/**
 * rstto_xfce_wallpaper_manager_new:
 *
 *
 * Singleton
 */
RsttoWallpaperManager *
rstto_xfce_wallpaper_manager_new (void)
{
    if (xfce_wallpaper_manager_object == NULL)
    {
        xfce_wallpaper_manager_object = g_object_new (
                RSTTO_TYPE_XFCE_WALLPAPER_MANAGER,
                NULL);
    }
    else
    {
        g_object_ref (xfce_wallpaper_manager_object);
    }

    return xfce_wallpaper_manager_object;
}

/* Based on xfce_spawn_get_active_workspace_number
 * http://git.xfce.org/xfce/libxfce4ui/tree/libxfce4ui/xfce-spawn.c#n193
 */
static gint
rstto_get_active_workspace_number (GdkScreen *screen)
{
    gint ws_num = 0;

#ifdef HAVE_LIBX11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
        GdkWindow *root;
        gulong     bytes_after_ret = 0;
        gulong     nitems_ret = 0;
        guint     *prop_ret = NULL;
        Atom       _NET_CURRENT_DESKTOP;
        Atom       _WIN_WORKSPACE;
        Atom       type_ret = None;
        gint       format_ret;

        gdk_x11_display_error_trap_push (gdk_screen_get_display (screen));

        root = gdk_screen_get_root_window (screen);

        /* determine the X atom values */
        _NET_CURRENT_DESKTOP = XInternAtom (GDK_WINDOW_XDISPLAY (root),
                "_NET_CURRENT_DESKTOP",
                False);
        _WIN_WORKSPACE = XInternAtom (GDK_WINDOW_XDISPLAY (root),
                "_WIN_WORKSPACE",
                False);

        if (XGetWindowProperty (GDK_WINDOW_XDISPLAY (root),
                gdk_x11_get_default_root_xwindow (),
                _NET_CURRENT_DESKTOP, 0, 32, False, XA_CARDINAL,
                &type_ret, &format_ret, &nitems_ret, &bytes_after_ret,
                (gpointer) &prop_ret) != Success)
        {
            if (XGetWindowProperty (GDK_WINDOW_XDISPLAY (root),
                    gdk_x11_get_default_root_xwindow (),
                    _WIN_WORKSPACE, 0, 32, False, XA_CARDINAL,
                    &type_ret, &format_ret, &nitems_ret, &bytes_after_ret,
                    (gpointer) &prop_ret) != Success)
            {
                if (G_UNLIKELY (prop_ret != NULL))
                {
                    XFree (prop_ret);
                    prop_ret = NULL;
                }
            }
        }

        if (G_LIKELY (prop_ret != NULL))
        {
            if (G_LIKELY (type_ret != None && format_ret != 0))
                ws_num = *prop_ret;
            XFree (prop_ret);
        }

        gdk_x11_display_error_trap_pop_ignored (gdk_screen_get_display (screen));
    }
#endif

    return ws_num;
}

static void
cb_style_combo_changed (
        GtkComboBox *style_combo,
        RsttoXfceWallpaperManager *manager)
{
    manager->priv->style = gtk_combo_box_get_active (style_combo);

    manager->priv->monitor = rstto_monitor_chooser_get_selected (
            RSTTO_MONITOR_CHOOSER (manager->priv->monitor_chooser));

    configure_monitor_chooser_pixbuf (manager);
}

static void
cb_workspace_mode_changed (
        GtkCheckButton *button,
        RsttoXfceWallpaperManager *manager)
{
    gboolean active;

    active = gtk_toggle_button_get_active (
            GTK_TOGGLE_BUTTON (button));

    if (manager->priv->channel != NULL)
        xfconf_channel_set_bool (manager->priv->channel,
                SINGLE_WORKSPACE_MODE,
                active);
}

static void
cb_monitor_chooser_changed (
        RsttoMonitorChooser *monitor_chooser,
        RsttoXfceWallpaperManager *manager)
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
    RsttoXfceWallpaperManager *manager)
{
    cairo_surface_t *image_surface = NULL;
    cairo_t *ctx;
    GdkPixbuf *tmp_pixbuf;
    gdouble x_scale, y_scale;
    gint monitor_width, monitor_height, surface_width, surface_height,
         dest_x, dest_y, dest_width, dest_height;

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
