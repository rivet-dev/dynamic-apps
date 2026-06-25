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
#include "settings.h"

#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>



enum
{
    PROP_0,
    PROP_SHOW_TOOLBAR,
    PROP_NAVBAR_POSITION,
    PROP_SHOW_THUMBNAILBAR,
    PROP_SHOW_STATUSBAR,
    PROP_SHOW_CLOCK,
    PROP_LIMIT_QUALITY,
    PROP_HIDE_THUMBNAILS_FULLSCREEN,
    PROP_HIDE_MOUSE_CURSOR_FULLSCREEN_TIMEOUT,
    PROP_WINDOW_WIDTH,
    PROP_WINDOW_HEIGHT,
    PROP_BGCOLOR,
    PROP_BGCOLOR_OVERRIDE,
    PROP_BGCOLOR_FULLSCREEN,
    PROP_CURRENT_URI,
    PROP_SLIDESHOW_TIMEOUT,
    PROP_WRAP_IMAGES,
    PROP_DESKTOP_TYPE,
    PROP_INVERT_ZOOM_DIRECTION,
    PROP_USE_THUNAR_PROPERTIES,
    PROP_MAXIMIZE_ON_STARTUP,
    PROP_ERROR_MISSING_THUMBNAILER,
    PROP_SORT_TYPE,
    PROP_THUMBNAIL_SIZE,
    PROP_DEFAULT_ZOOM,
};

static RsttoSettings *settings_object;
static gboolean xfconf_disabled = TRUE;



static void
rstto_settings_finalize (GObject *object);
static void
rstto_settings_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec);
static void
rstto_settings_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec);

static void
rstto_xfconf_ensure_gdkrgba (XfconfChannel *channel,
                             const gchar *property);



struct _RsttoSettingsPrivate
{
    XfconfChannel *channel;

    gboolean  show_toolbar;
    gboolean  show_thumbnailbar;
    gboolean  show_statusbar;
    gboolean  show_clock;
    gboolean  limit_quality;
    gboolean  hide_thumbnails_fullscreen;
    guint     hide_mouse_cursor_fullscreen_timeout;
    gchar    *navigationbar_position;
    gboolean  invert_zoom_direction;
    guint     window_width;
    guint     window_height;
    gchar    *last_file_path;
    guint     slideshow_timeout;
    GdkRGBA  *bgcolor;
    gboolean  bgcolor_override;
    GdkRGBA  *bgcolor_fullscreen;
    gboolean  wrap_images;
    gchar    *desktop_type;
    gboolean  use_thunar_properties;
    gboolean  maximize_on_startup;
    RsttoThumbnailSize thumbnail_size;
    RsttoScale default_zoom;

    RsttoSortType sort_type;

    struct {
        gboolean missing_thumbnailer;
    } errors;
};



G_DEFINE_TYPE_WITH_PRIVATE (RsttoSettings, rstto_settings, G_TYPE_OBJECT)



static void
rstto_settings_init (RsttoSettings *settings)
{
    gchar *accelmap_path = NULL;

    settings->priv = rstto_settings_get_instance_private (settings);
    if (! xfconf_disabled)
        settings->priv->channel = xfconf_channel_new ("ristretto");

    accelmap_path = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, "ristretto/accels.scm");
    if (accelmap_path)
    {
        gtk_accel_map_load (accelmap_path);
        g_free (accelmap_path);
        accelmap_path = NULL;
    }
    else
    {
        /* If the accels.scm file is missing, we are probably
         * dealing with a first-boot. Add default accelerators.
         */
        gtk_accel_map_change_entry ("<Window>/fullscreen", GDK_KEY_F, 0, FALSE);
        gtk_accel_map_change_entry ("<Window>/unfullscreen", GDK_KEY_Escape, 0, FALSE);
        gtk_accel_map_change_entry ("<Window>/next-image", GDK_KEY_Page_Down, 0, FALSE);
        gtk_accel_map_change_entry ("<Window>/previous-image", GDK_KEY_Page_Up, 0, FALSE);
        gtk_accel_map_change_entry ("<Window>/quit", GDK_KEY_q, 0, FALSE);

        gtk_accel_map_change_entry ("<Window>/delete", GDK_KEY_Delete, GDK_SHIFT_MASK, FALSE);

        gtk_accel_map_change_entry ("<Window>/refresh", GDK_KEY_r, GDK_CONTROL_MASK, FALSE);

        gtk_accel_map_change_entry ("<Actions>/RsttoWindow/play", GDK_KEY_F5, 0, FALSE);
    }

    settings->priv->slideshow_timeout = 5;
    settings->priv->bgcolor = g_new0 (GdkRGBA, 1);
    settings->priv->bgcolor_fullscreen = g_new0 (GdkRGBA, 1);
    gdk_rgba_parse (settings->priv->bgcolor_fullscreen, "black"); // black by default
    settings->priv->navigationbar_position = g_strdup ("left");
    settings->priv->show_toolbar = TRUE;
    settings->priv->window_width = 600;
    settings->priv->window_height = 440;
    settings->priv->wrap_images = TRUE;
    settings->priv->show_thumbnailbar = TRUE;
    settings->priv->show_statusbar = TRUE;
    settings->priv->use_thunar_properties = TRUE;
    settings->priv->maximize_on_startup = TRUE;
    settings->priv->hide_thumbnails_fullscreen = TRUE;
    settings->priv->hide_mouse_cursor_fullscreen_timeout = 1;
    settings->priv->errors.missing_thumbnailer = TRUE;
    settings->priv->thumbnail_size = RSTTO_THUMBNAIL_SIZE_NORMAL;
    settings->priv->default_zoom = RSTTO_SCALE_NONE;

    if (xfconf_disabled)
        return;

    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/width",
            G_TYPE_UINT,
            settings,
            "window-width");
    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/height",
            G_TYPE_UINT,
            settings,
            "window-height");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/file/current-uri",
            G_TYPE_STRING,
            settings,
            "current-uri");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/toolbar/show",
            G_TYPE_BOOLEAN,
            settings,
            "show-toolbar");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/navigationbar/sort-type",
            G_TYPE_UINT,
            settings,
            "sort-type");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/navigationbar/position",
            G_TYPE_STRING,
            settings,
            "navigationbar-position");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/thumbnails/show",
            G_TYPE_BOOLEAN,
            settings,
            "show-thumbnailbar");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/statusbar/show",
            G_TYPE_BOOLEAN,
            settings,
            "show-statusbar");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/thumbnails/size",
            G_TYPE_UINT,
            settings,
            "thumbnail-size");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/thumbnails/hide-fullscreen",
            G_TYPE_BOOLEAN,
            settings,
            "hide-thumbnails-fullscreen");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/mouse-cursor/hide-fullscreen-timeout",
            G_TYPE_UINT,
            settings,
            "hide-mouse-cursor-fullscreen-timeout");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/slideshow/timeout",
            G_TYPE_UINT,
            settings,
            "slideshow-timeout");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/bgcolor-override",
            G_TYPE_BOOLEAN,
            settings,
            "bgcolor-override");

    rstto_xfconf_ensure_gdkrgba (settings->priv->channel, "/window/bgcolor");
    xfconf_g_property_bind_gdkrgba (
            settings->priv->channel,
            "/window/bgcolor",
            settings,
            "bgcolor");

    rstto_xfconf_ensure_gdkrgba (settings->priv->channel, "/window/bgcolor-fullscreen");
    xfconf_g_property_bind_gdkrgba (
            settings->priv->channel,
            "/window/bgcolor-fullscreen",
            settings,
            "bgcolor-fullscreen");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/invert-zoom-direction",
            G_TYPE_BOOLEAN,
            settings,
            "invert-zoom-direction");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/show-clock",
            G_TYPE_BOOLEAN,
            settings,
            "show-clock");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/image/wrap",
            G_TYPE_BOOLEAN,
            settings,
            "wrap-images");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/image/limit-quality",
            G_TYPE_BOOLEAN,
            settings,
            "limit-quality");

    /* not Thunar-specific but let's keep the old name for backwards-compatibility */
    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/use-thunar-properties",
            G_TYPE_BOOLEAN,
            settings,
            "use-thunar-properties");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/window/maximize-on-startup",
            G_TYPE_BOOLEAN,
            settings,
            "maximize-on-startup");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/errors/missing-thumbnailer",
            G_TYPE_BOOLEAN,
            settings,
            "show-error-missing-thumbnailer");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/desktop/type",
            G_TYPE_STRING,
            settings,
            "desktop-type");

    xfconf_g_property_bind (
            settings->priv->channel,
            "/image/default-zoom",
            G_TYPE_INT,
            settings,
            "default-zoom");
}


static void
rstto_settings_class_init (RsttoSettingsClass *klass)
{
    GParamSpec *pspec;
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = rstto_settings_finalize;
    object_class->set_property = rstto_settings_set_property;
    object_class->get_property = rstto_settings_get_property;

    pspec = g_param_spec_uint (
            "window-width",
            "",
            "",
            0,
            G_MAXUINT,
            600,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_WINDOW_WIDTH,
            pspec);

    pspec = g_param_spec_uint (
            "window-height",
            "",
            "",
            0,
            G_MAXUINT,
            400,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_WINDOW_HEIGHT,
            pspec);

    pspec = g_param_spec_boolean (
            "show-toolbar",
            "",
            "",
            TRUE,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_SHOW_TOOLBAR,
            pspec);

    pspec = g_param_spec_boolean (
            "show-thumbnailbar",
            "",
            "",
            TRUE,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_SHOW_THUMBNAILBAR,
            pspec);

    pspec = g_param_spec_boolean (
            "show-statusbar",
            "",
            "",
            TRUE,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_SHOW_STATUSBAR,
            pspec);

    pspec = g_param_spec_boolean (
            "show-clock",
            "",
            "",
            TRUE,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_SHOW_CLOCK,
            pspec);

    pspec = g_param_spec_boolean (
            "limit-quality",
            "",
            "",
            TRUE,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_LIMIT_QUALITY,
            pspec);

    pspec = g_param_spec_boolean (
            "hide-thumbnails-fullscreen",
            "",
            "",
            TRUE,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_HIDE_THUMBNAILS_FULLSCREEN,
            pspec);

    pspec = g_param_spec_uint (
            "hide-mouse-cursor-fullscreen-timeout",
            "",
            "",
            0,
            3600,
            1,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_HIDE_MOUSE_CURSOR_FULLSCREEN_TIMEOUT,
            pspec);

    pspec = g_param_spec_string (
            "navigationbar-position",
            "",
            "",
            "bottom",
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_NAVBAR_POSITION,
            pspec);

    pspec = g_param_spec_boolean (
            "invert-zoom-direction",
            "",
            "",
            TRUE,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_INVERT_ZOOM_DIRECTION,
            pspec);

    pspec = g_param_spec_string (
            "current-uri",
            "",
            "",
            "file://~/",
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_CURRENT_URI,
            pspec);

    pspec = g_param_spec_uint (
            "slideshow-timeout",
            "",
            "",
            1,
            300,
            5,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_SLIDESHOW_TIMEOUT,
            pspec);

    pspec = g_param_spec_boxed (
            "bgcolor",
            "",
            "",
            GDK_TYPE_RGBA,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_BGCOLOR,
            pspec);

    pspec = g_param_spec_boolean (
            "bgcolor-override",
            "",
            "",
            TRUE,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_BGCOLOR_OVERRIDE,
            pspec);

    pspec = g_param_spec_boxed (
            "bgcolor-fullscreen",
            "",
            "",
            GDK_TYPE_RGBA,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_BGCOLOR_FULLSCREEN,
            pspec);

    pspec = g_param_spec_boolean (
            "wrap-images",
            "",
            "",
            TRUE,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_WRAP_IMAGES,
            pspec);

    pspec = g_param_spec_string (
            "desktop-type",
            "",
            "",
            NULL,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_DESKTOP_TYPE,
            pspec);

    pspec = g_param_spec_boolean (
            "use-thunar-properties",
            "",
            "",
            TRUE,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_USE_THUNAR_PROPERTIES,
            pspec);

    pspec = g_param_spec_boolean (
            "maximize-on-startup",
            "",
            "",
            TRUE,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_MAXIMIZE_ON_STARTUP,
            pspec);

    pspec = g_param_spec_boolean (
            "show-error-missing-thumbnailer",
            "",
            "",
            TRUE,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_ERROR_MISSING_THUMBNAILER,
            pspec);

    pspec = g_param_spec_uint (
            "sort-type",
            "",
            "",
            0,
            SORT_TYPE_COUNT,
            0,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_SORT_TYPE,
            pspec);

    pspec = g_param_spec_uint (
            "thumbnail-size",
            "",
            "",
            0,
            RSTTO_THUMBNAIL_SIZE_COUNT,
            0,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_THUMBNAIL_SIZE,
            pspec);

    pspec = g_param_spec_int (
            "default-zoom",
            "",
            "",
            RSTTO_SCALE_NONE,
            RSTTO_SCALE_REAL_SIZE,
            RSTTO_SCALE_NONE,
            G_PARAM_READWRITE);
    g_object_class_install_property (
            object_class,
            PROP_DEFAULT_ZOOM,
            pspec);
}

/**
 * rstto_settings_finalize:
 * @object:
 *
 */
static void
rstto_settings_finalize (GObject *object)
{
    RsttoSettings *settings = RSTTO_SETTINGS (object);
    gchar         *accelmap_path = NULL;

    if (settings->priv->channel)
    {
        g_object_unref (settings->priv->channel);
        settings->priv->channel = NULL;
    }

    if (settings->priv->last_file_path)
    {
        g_free (settings->priv->last_file_path);
        settings->priv->last_file_path = NULL;
    }

    if (settings->priv->navigationbar_position)
    {
        g_free (settings->priv->navigationbar_position);
        settings->priv->navigationbar_position = NULL;
    }

    if (settings->priv->desktop_type)
    {
        g_free (settings->priv->desktop_type);
        settings->priv->desktop_type = NULL;
    }

    if (settings->priv->bgcolor)
    {
        g_free (settings->priv->bgcolor);
        settings->priv->bgcolor = NULL;
    }

    if (settings->priv->bgcolor_fullscreen)
    {
        g_free (settings->priv->bgcolor_fullscreen);
        settings->priv->bgcolor_fullscreen = NULL;
    }

    accelmap_path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "ristretto/accels.scm", TRUE);
    if (accelmap_path)
    {
        gtk_accel_map_save (accelmap_path);
        g_free (accelmap_path);
        accelmap_path = NULL;
    }

    G_OBJECT_CLASS (rstto_settings_parent_class)->finalize (object);
}

/**
 * rstto_xfconf_ensure_gdkrgba:
 * This method makes sure that the property contains a GdkRGBA value rather than
 * a GdkColor value, by converting the latter to the former if necessary.
 */
static void
rstto_xfconf_ensure_gdkrgba (XfconfChannel *channel, const gchar *property)
{
    guint    rc, gc, bc, ac;
    gboolean is_gdk_color = xfconf_channel_get_array (channel,
                                                      property,
                                                      G_TYPE_UINT, &rc,
                                                      G_TYPE_UINT, &gc,
                                                      G_TYPE_UINT, &bc,
                                                      G_TYPE_UINT, &ac,
                                                      G_TYPE_INVALID);

    if (is_gdk_color)
    {
        GdkRGBA bg = { (gdouble) rc / 65535, (gdouble) gc / 65535, (gdouble) bc / 65535, (gdouble) ac / 65535 };
        xfconf_channel_set_array (channel,
                                  property,
                                  G_TYPE_DOUBLE, &bg.red,
                                  G_TYPE_DOUBLE, &bg.green,
                                  G_TYPE_DOUBLE, &bg.blue,
                                  G_TYPE_DOUBLE, &bg.alpha,
                                  G_TYPE_INVALID);
    }
}



gboolean
rstto_settings_get_xfconf_disabled (void)
{
    return xfconf_disabled;
}

void
rstto_settings_set_xfconf_disabled (gboolean disabled)
{
    xfconf_disabled = disabled;
}

/**
 * rstto_settings_new:
 *
 *
 * Singleton
 */
RsttoSettings *
rstto_settings_new (void)
{
    if (settings_object == NULL)
    {
        settings_object = g_object_new (RSTTO_TYPE_SETTINGS, NULL);
    }
    else
    {
        g_object_ref (settings_object);
    }

    return settings_object;
}


static void
rstto_settings_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
    GdkRGBA *color;
    const gchar *str_val = NULL;
    RsttoSettings *settings = RSTTO_SETTINGS (object);

    switch (property_id)
    {
        case PROP_SHOW_TOOLBAR:
            settings->priv->show_toolbar = g_value_get_boolean (value);
            break;
        case PROP_SHOW_THUMBNAILBAR:
            settings->priv->show_thumbnailbar = g_value_get_boolean (value);
            break;
        case PROP_SHOW_STATUSBAR:
            settings->priv->show_statusbar = g_value_get_boolean (value);
            break;
        case PROP_SHOW_CLOCK:
            settings->priv->show_clock = g_value_get_boolean (value);
            break;
        case PROP_LIMIT_QUALITY:
            settings->priv->limit_quality = g_value_get_boolean (value);
            break;
        case PROP_HIDE_THUMBNAILS_FULLSCREEN:
            settings->priv->hide_thumbnails_fullscreen = g_value_get_boolean (value);
            break;
        case PROP_HIDE_MOUSE_CURSOR_FULLSCREEN_TIMEOUT:
            settings->priv->hide_mouse_cursor_fullscreen_timeout = g_value_get_uint (value);
            break;
        case PROP_NAVBAR_POSITION:
            str_val = g_value_get_string (value);

            if ((!g_ascii_strcasecmp (str_val, "left")) ||
                (!g_ascii_strcasecmp (str_val, "right")) ||
                (!g_ascii_strcasecmp (str_val, "bottom")) ||
                (!g_ascii_strcasecmp (str_val, "top")))
            {
                if (settings->priv->navigationbar_position)
                    g_free (settings->priv->navigationbar_position);
                settings->priv->navigationbar_position = g_strdup (str_val);
            }
            break;
        case PROP_INVERT_ZOOM_DIRECTION:
            settings->priv->invert_zoom_direction = g_value_get_boolean (value);
            break;
        case PROP_WINDOW_WIDTH:
            settings->priv->window_width = g_value_get_uint (value);
            break;
        case PROP_WINDOW_HEIGHT:
            settings->priv->window_height = g_value_get_uint (value);
            break;
        case PROP_BGCOLOR:
            color = g_value_get_boxed (value);
            settings->priv->bgcolor->red = color->red;
            settings->priv->bgcolor->green = color->green;
            settings->priv->bgcolor->blue = color->blue;
            settings->priv->bgcolor->alpha = color->alpha;
            break;
        case PROP_BGCOLOR_OVERRIDE:
            settings->priv->bgcolor_override = g_value_get_boolean (value);
            break;
        case PROP_CURRENT_URI:
            if (settings->priv->last_file_path)
                g_free (settings->priv->last_file_path);
            settings->priv->last_file_path = g_value_dup_string (value);
            break;
        case PROP_SLIDESHOW_TIMEOUT:
            settings->priv->slideshow_timeout = g_value_get_uint (value);
            break;
        case PROP_BGCOLOR_FULLSCREEN:
            color = g_value_get_boxed (value);
            settings->priv->bgcolor_fullscreen->red = color->red;
            settings->priv->bgcolor_fullscreen->green = color->green;
            settings->priv->bgcolor_fullscreen->blue = color->blue;
            settings->priv->bgcolor_fullscreen->alpha = color->alpha;
            break;
        case PROP_WRAP_IMAGES:
            settings->priv->wrap_images = g_value_get_boolean (value);
            break;
        case PROP_DESKTOP_TYPE:
            if (settings->priv->desktop_type)
                g_free (settings->priv->desktop_type);
            settings->priv->desktop_type = g_value_dup_string (value);
            break;
        case PROP_USE_THUNAR_PROPERTIES:
            settings->priv->use_thunar_properties = g_value_get_boolean (value);
            break;
        case PROP_MAXIMIZE_ON_STARTUP:
            settings->priv->maximize_on_startup = g_value_get_boolean (value);
            break;
        case PROP_ERROR_MISSING_THUMBNAILER:
            settings->priv->errors.missing_thumbnailer = g_value_get_boolean (value);
            break;
        case PROP_SORT_TYPE:
            settings->priv->sort_type = g_value_get_uint (value);
            break;
        case PROP_THUMBNAIL_SIZE:
            settings->priv->thumbnail_size = g_value_get_uint (value);
            break;
        case PROP_DEFAULT_ZOOM:
            settings->priv->default_zoom = g_value_get_int (value);
            break;
        default:
            break;
    }

}

static void
rstto_settings_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
    RsttoSettings *settings = RSTTO_SETTINGS (object);

    switch (property_id)
    {
        case PROP_SHOW_TOOLBAR:
            g_value_set_boolean (value, settings->priv->show_toolbar);
            break;
        case PROP_SHOW_THUMBNAILBAR:
            g_value_set_boolean (value, settings->priv->show_thumbnailbar);
            break;
        case PROP_SHOW_STATUSBAR:
            g_value_set_boolean (value, settings->priv->show_statusbar);
            break;
        case PROP_SHOW_CLOCK:
            g_value_set_boolean (value, settings->priv->show_clock);
            break;
        case PROP_LIMIT_QUALITY:
            g_value_set_boolean (value, settings->priv->limit_quality);
            break;
        case PROP_HIDE_THUMBNAILS_FULLSCREEN:
            g_value_set_boolean (value, settings->priv->hide_thumbnails_fullscreen);
            break;
        case PROP_HIDE_MOUSE_CURSOR_FULLSCREEN_TIMEOUT:
            g_value_set_uint (value, settings->priv->hide_mouse_cursor_fullscreen_timeout);
            break;
        case PROP_NAVBAR_POSITION:
            g_value_set_string (value, settings->priv->navigationbar_position);
            break;
        case PROP_INVERT_ZOOM_DIRECTION:
            g_value_set_boolean (value, settings->priv->invert_zoom_direction);
            break;
        case PROP_WINDOW_WIDTH:
            g_value_set_uint (value, settings->priv->window_width);
            break;
        case PROP_WINDOW_HEIGHT:
            g_value_set_uint (value, settings->priv->window_height);
            break;
        case PROP_CURRENT_URI:
            g_value_set_string (value, settings->priv->last_file_path);
            break;
        case PROP_SLIDESHOW_TIMEOUT:
            g_value_set_uint (value, settings->priv->slideshow_timeout);
            break;
        case PROP_BGCOLOR_FULLSCREEN:
            g_value_set_boxed (value, settings->priv->bgcolor_fullscreen);
            break;
        case PROP_BGCOLOR:
            g_value_set_boxed (value, settings->priv->bgcolor);
            break;
        case PROP_BGCOLOR_OVERRIDE:
            g_value_set_boolean (value, settings->priv->bgcolor_override);
            break;
        case PROP_WRAP_IMAGES:
            g_value_set_boolean (value, settings->priv->wrap_images);
            break;
        case PROP_DESKTOP_TYPE:
            g_value_set_string (value, settings->priv->desktop_type);
            break;
        case PROP_USE_THUNAR_PROPERTIES:
            g_value_set_boolean (value, settings->priv->use_thunar_properties);
            break;
        case PROP_MAXIMIZE_ON_STARTUP:
            g_value_set_boolean (value, settings->priv->maximize_on_startup);
            break;
        case PROP_ERROR_MISSING_THUMBNAILER:
            g_value_set_boolean (
                    value,
                    settings->priv->errors.missing_thumbnailer);
            break;
        case PROP_SORT_TYPE:
            g_value_set_uint (
                    value,
                    settings->priv->sort_type);
            break;
        case PROP_THUMBNAIL_SIZE:
            g_value_set_uint (
                    value,
                    settings->priv->thumbnail_size);
            break;
        case PROP_DEFAULT_ZOOM:
            g_value_set_int (value, settings->priv->default_zoom);
            break;
        default:
            break;

    }
}

void
rstto_settings_set_navbar_position (RsttoSettings *settings, guint pos)
{
    const gchar *str;

    switch (pos)
    {
        default:
            str = "left";
            break;
        case 1:
            str = "right";
            break;
        case 2:
            str = "top";
            break;
        case 3:
            str = "bottom";
            break;
    }

    g_object_set (settings, "navigationbar-position", str, NULL);
}

guint
rstto_settings_get_navbar_position (RsttoSettings *settings) {
    if (settings->priv->navigationbar_position == NULL)
        return 0;

    if (!strcmp (settings->priv->navigationbar_position, "left"))
        return 0;
    if (!strcmp (settings->priv->navigationbar_position, "right"))
        return 1;
    if (!strcmp (settings->priv->navigationbar_position, "top"))
        return 2;
    if (!strcmp (settings->priv->navigationbar_position, "bottom"))
        return 3;

    return 0;
}

/** Convenience functions */
void
rstto_settings_set_uint_property (RsttoSettings *settings,
                                  const gchar *property_name,
                                  guint value)
{
    g_object_set (settings, property_name, value, NULL);
}

guint
rstto_settings_get_uint_property (RsttoSettings *settings,
                                  const gchar *property_name)
{
    guint value;

    g_object_get (settings, property_name, &value, NULL);

    return value;
}

void
rstto_settings_set_int_property (RsttoSettings *settings,
                                 const gchar *property_name,
                                 gint value)
{
    g_object_set (settings, property_name, value, NULL);
}

gint
rstto_settings_get_int_property (RsttoSettings *settings,
                                 const gchar *property_name)
{
    gint value;

    g_object_get (settings, property_name, &value, NULL);

    return value;
}

void
rstto_settings_set_string_property (RsttoSettings *settings,
                                    const gchar *property_name,
                                    const gchar *value)
{
    g_object_set (settings, property_name, value, NULL);
}

gchar *
rstto_settings_get_string_property (RsttoSettings *settings,
                                    const gchar *property_name)
{
    gchar *value = NULL;

    g_object_get (settings, property_name, &value, NULL);

    return value;
}

void
rstto_settings_set_boolean_property (RsttoSettings *settings,
                                     const gchar *property_name,
                                     gboolean value)
{
    g_object_set (settings, property_name, value, NULL);
}

gboolean
rstto_settings_get_boolean_property (RsttoSettings *settings,
                                     const gchar *property_name)
{
    gboolean value;

    g_object_get (settings, property_name, &value, NULL);

    return value;
}
