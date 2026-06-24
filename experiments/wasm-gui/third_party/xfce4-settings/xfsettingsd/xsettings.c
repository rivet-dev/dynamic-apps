/*
 * Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 * Copyright (c) 2011 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * See http://standards.freedesktop.org/xsettings-spec/xsettings-spec-0.5.html
 * for the description of the xsetting specification
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/Xatom.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>

#include <gio/gio.h>
#include <fontconfig/fontconfig.h>

#include "xsettings.h"
#include "debug.h"

#define XSettingsTypeInteger 0
#define XSettingsTypeString  1
#define XSettingsTypeColor   2

#define XSETTINGS_PAD(n,m) ((n + m - 1) & (~(m-1)))

#define DPI_FALLBACK        96
#define DPI_LOW_REASONABLE  50
#define DPI_HIGH_REASONABLE 500

#define FC_TIMEOUT_SEC 2 /* timeout before xsettings notify */
#define FC_PROPERTY    "/Fontconfig/Timestamp"



typedef struct _XfceXSettingsScreen XfceXSettingsScreen;
typedef struct _XfceXSetting        XfceXSetting;
typedef struct _XfceXSettingsNotify XfceXSettingsNotify;



static void     xfce_xsettings_helper_finalize     (GObject             *object);
static void     xfce_xsettings_helper_fc_free      (XfceXSettingsHelper *helper);
static gboolean xfce_xsettings_helper_fc_init      (gpointer             data);
static gboolean xfce_xsettings_helper_notify_idle  (gpointer             data);
static void     xfce_xsettings_helper_setting_free (gpointer             data);
static void     xfce_xsettings_helper_prop_changed (XfconfChannel       *channel,
                                                    const gchar         *prop_name,
                                                    const GValue        *value,
                                                    XfceXSettingsHelper *helper);
static void     xfce_xsettings_helper_load         (XfceXSettingsHelper *helper);
static void     xfce_xsettings_helper_screen_free  (XfceXSettingsScreen *screen);
static void     xfce_xsettings_helper_notify_xft   (XfceXSettingsHelper *helper);
static void     xfce_xsettings_helper_notify       (XfceXSettingsHelper *helper);



struct _XfceXSettingsHelperClass
{
    GObjectClass __parent__;
};

struct _XfceXSettingsHelper
{
    GObject  __parent__;

    XfconfChannel *channel;

    /* list of XfceXSettingsScreen we handle */
    GSList        *screens;

    /* table with xfconf property keyd and XfceXSetting */
    GHashTable    *settings;

    /* auto increasing serial for each time we notify */
    gulong         serial;

    /* idle notifications */
    guint          notify_idle_id;
    guint          notify_xft_idle_id;

    /* atom for xsetting property changes */
    Atom           xsettings_atom;

    /* fontconfig monitoring */
    GPtrArray     *fc_monitors;
    guint          fc_notify_timeout_id;
    guint          fc_init_id;
};

struct _XfceXSetting
{
    GValue *value;
    gulong  last_change_serial;
};

struct _XfceXSettingsNotify
{
    guchar *buf;
    gsize   buf_len;
    gint    n_settings;
    gsize   dpi_offset;
};

struct _XfceXSettingsScreen
{
    Display *xdisplay;
    Window   window;
    Atom     selection_atom;
    gint     screen_num;
};

struct _XfceTimestamp
{
    Window window;
    Atom   atom;
};



G_DEFINE_TYPE (XfceXSettingsHelper, xfce_xsettings_helper, G_TYPE_OBJECT);



static void
xfce_xsettings_helper_class_init (XfceXSettingsHelperClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_xsettings_helper_finalize;
}



static void
xfce_xsettings_helper_init (XfceXSettingsHelper *helper)
{
    helper->channel = xfconf_channel_new ("xsettings");

    helper->settings = g_hash_table_new_full (g_str_hash, g_str_equal,
        g_free, xfce_xsettings_helper_setting_free);

    xfce_xsettings_helper_load (helper);

    g_signal_connect (G_OBJECT (helper->channel), "property-changed",
        G_CALLBACK (xfce_xsettings_helper_prop_changed), helper);
}



static void
xfce_xsettings_helper_finalize (GObject *object)
{
    XfceXSettingsHelper *helper = XFCE_XSETTINGS_HELPER (object);
    GSList              *li;

    /* stop fontconfig monitoring */
    xfce_xsettings_helper_fc_free (helper);

    /* stop pending update */
    if (helper->notify_idle_id != 0)
        g_source_remove (helper->notify_idle_id);

    if (helper->notify_xft_idle_id != 0)
        g_source_remove (helper->notify_xft_idle_id);

    g_object_unref (G_OBJECT (helper->channel));

    /* remove screens */
    for (li = helper->screens; li != NULL; li = li->next)
        xfce_xsettings_helper_screen_free (li->data);
    g_slist_free (helper->screens);

    g_hash_table_destroy (helper->settings);

    (*G_OBJECT_CLASS (xfce_xsettings_helper_parent_class)->finalize) (object);
}



static gboolean
xfce_xsettings_helper_fc_notify (gpointer data)
{
    XfceXSettingsHelper *helper = XFCE_XSETTINGS_HELPER (data);
    XfceXSetting        *setting;

    helper->fc_notify_timeout_id = 0;

    /* check if the font config setup changed */
    if (!FcConfigUptoDate (NULL) && FcInitReinitialize ())
    {
        /* stop the monitors */
        xfce_xsettings_helper_fc_free (helper);

        setting = g_hash_table_lookup (helper->settings, FC_PROPERTY);
        if (setting == NULL)
        {
            /* create new setting */
            setting = g_slice_new0 (XfceXSetting);
            setting->value = g_new0 (GValue, 1);
            g_value_init (setting->value, G_TYPE_INT);
            g_hash_table_insert (helper->settings, g_strdup (FC_PROPERTY), setting);
        }

        /* update setting */
        setting->last_change_serial = helper->serial;
        g_value_set_int (setting->value, time (NULL));

        xfsettings_dbg (XFSD_DEBUG_FONTCONFIG, "timestamp updated (time=%d)",
                        g_value_get_int (setting->value));

        /* schedule xsettings update */
        if (helper->notify_idle_id == 0)
            helper->notify_idle_id = g_idle_add (xfce_xsettings_helper_notify_idle, helper);

        /* restart monitoring */
        helper->fc_init_id = g_idle_add (xfce_xsettings_helper_fc_init, helper);
    }

    return FALSE;
}



static void
xfce_xsettings_helper_fc_changed (XfceXSettingsHelper *helper)
{
    /* reschedule monitor */
    if (helper->fc_notify_timeout_id != 0)
        g_source_remove (helper->fc_notify_timeout_id);

    helper->fc_notify_timeout_id = g_timeout_add_seconds (FC_TIMEOUT_SEC,
        xfce_xsettings_helper_fc_notify, helper);
}



static void
xfce_xsettings_helper_fc_free (XfceXSettingsHelper *helper)
{
    if (helper->fc_notify_timeout_id != 0)
    {
        /* stop update timeout */
        g_source_remove (helper->fc_notify_timeout_id);
        helper->fc_notify_timeout_id = 0;
    }

    if (helper->fc_init_id != 0)
    {
        /* stop startup timeout */
        g_source_remove (helper->fc_init_id);
        helper->fc_init_id = 0;
    }

    if (helper->fc_monitors != NULL)
    {
        /* remove monitors */
        g_ptr_array_foreach (helper->fc_monitors, (GFunc) (void (*)(void)) g_object_unref, NULL);
        g_ptr_array_free (helper->fc_monitors, TRUE);
        helper->fc_monitors = NULL;
    }
}



static void
xfce_xsettings_helper_fc_monitor (XfceXSettingsHelper *helper,
                                  FcStrList           *files)
{
    const gchar  *path;
    GFile        *file;
    GFileMonitor *monitor;

    if (G_UNLIKELY (files == NULL))
        return;

    for (;;)
    {
        path = (const gchar *) FcStrListNext (files);
        if (G_UNLIKELY (path == NULL))
            break;

        file = g_file_new_for_path (path);
        monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, NULL);
        g_object_unref (G_OBJECT (file));

        if (G_LIKELY (monitor != NULL))
        {
            g_ptr_array_add (helper->fc_monitors, monitor);
            g_signal_connect_swapped (G_OBJECT (monitor), "changed",
                G_CALLBACK (xfce_xsettings_helper_fc_changed), helper);

            xfsettings_dbg_filtered (XFSD_DEBUG_FONTCONFIG, "monitoring \"%s\"",
                                     path);
        }
    }

    FcStrListDone (files);
}



static gboolean
xfce_xsettings_helper_fc_init (gpointer data)
{
    XfceXSettingsHelper *helper = XFCE_XSETTINGS_HELPER (data);

    g_return_val_if_fail (helper->fc_monitors == NULL, FALSE);

    helper->fc_init_id = 0;

    if (FcInit ())
    {
        helper->fc_monitors = g_ptr_array_new ();

        /* start monitoring config files and font directories */
        xfce_xsettings_helper_fc_monitor (helper, FcConfigGetConfigFiles (NULL));
        xfce_xsettings_helper_fc_monitor (helper, FcConfigGetFontDirs (NULL));

        xfsettings_dbg (XFSD_DEBUG_FONTCONFIG, "monitoring %d paths",
                        helper->fc_monitors->len);
    }

    return FALSE;
}



static gboolean
xfce_xsettings_helper_notify_idle (gpointer data)
{
    XfceXSettingsHelper *helper = XFCE_XSETTINGS_HELPER (data);

    /* only update if there are screen registered */
    if (helper->screens != NULL)
        xfce_xsettings_helper_notify (helper);

    helper->notify_idle_id = 0;

    return FALSE;
}



static gboolean
xfce_xsettings_helper_notify_xft_idle (gpointer data)
{
    XfceXSettingsHelper *helper = XFCE_XSETTINGS_HELPER (data);

    /* only update if there are screen registered */
    if (helper->screens != NULL)
        xfce_xsettings_helper_notify_xft (helper);

    helper->notify_xft_idle_id = 0;

    return FALSE;
}



static gboolean
xfce_xsettings_helper_prop_valid (const gchar  *prop_name,
                                  const GValue *value)
{
    /* only accept properties in valid domains */
    if (!g_str_has_prefix (prop_name, "/Net/")
        && !g_str_has_prefix (prop_name, "/Xft/")
        && !g_str_has_prefix (prop_name, "/Gtk/")
        && !g_str_has_prefix (prop_name, "/Gdk/"))
        return FALSE;

    /* notify if the property has an unsupported type */
    if (!G_VALUE_HOLDS_BOOLEAN (value)
        && !G_VALUE_HOLDS_INT (value)
        && !G_VALUE_HOLDS_STRING (value))
    {
        g_warning ("Property \"%s\" has an unsupported type \"%s\".",
                   prop_name, G_VALUE_TYPE_NAME (value));

        return FALSE;
    }

    return TRUE;
}



static gboolean
xfce_xsettings_helper_prop_load (gchar               *prop_name,
                                 GValue              *value,
                                 XfceXSettingsHelper *helper)
{
    XfceXSetting *setting;

    /* check if the property is valid */
    if (!xfce_xsettings_helper_prop_valid (prop_name, value))
        return FALSE;

    setting = g_slice_new0 (XfceXSetting);
    setting->value = value;
    setting->last_change_serial = helper->serial;

    xfsettings_dbg_filtered (XFSD_DEBUG_XSETTINGS, "prop \"%s\" loaded (type=%s)",
                             prop_name, G_VALUE_TYPE_NAME (value));

    g_hash_table_insert (helper->settings, prop_name, setting);

    /* we've stolen the value */
    return TRUE;
}



static void
xfce_xsettings_helper_prop_changed (XfconfChannel       *channel,
                                    const gchar         *prop_name,
                                    const GValue        *value,
                                    XfceXSettingsHelper *helper)
{
    XfceXSetting *setting;

    g_return_if_fail (helper->channel == channel);

    xfsettings_dbg_filtered (XFSD_DEBUG_XSETTINGS, "prop \"%s\" changed (type=%s)",
                             prop_name, G_VALUE_TYPE_NAME (value));

    if (G_LIKELY (G_VALUE_TYPE (value) != G_TYPE_INVALID))
    {
        setting = g_hash_table_lookup (helper->settings, prop_name);
        if (G_LIKELY (setting != NULL))
        {
            /* update the value, without assuming the types match because
             * you can change type in xfconf without removing it first
             * e.g. via xfconf_channel_set_property() */
            g_value_unset (setting->value);
            g_value_init (setting->value, G_VALUE_TYPE (value));
            g_value_copy (value, setting->value);

            /* update the serial */
            setting->last_change_serial = helper->serial;
        }
        else if (xfce_xsettings_helper_prop_valid (prop_name, value))
        {
            /* insert a new setting */
            setting = g_slice_new0 (XfceXSetting);
            setting->value = g_new0 (GValue, 1);
            setting->last_change_serial = helper->serial;

            g_value_init (setting->value, G_VALUE_TYPE (value));
            g_value_copy (value, setting->value);

            g_hash_table_insert (helper->settings, g_strdup (prop_name), setting);
        }
        else
        {
           /* leave, so not notification is scheduled */
           return;
        }
    }
    else
    {
        /* maybe the value is not found, because we haven't
         * checked if the property is valid, but that's not
         * a problem */
        g_hash_table_remove (helper->settings, prop_name);
    }

    if (helper->notify_idle_id == 0)
    {
        /* schedule an update */
        helper->notify_idle_id = g_idle_add (xfce_xsettings_helper_notify_idle, helper);
    }

    if (helper->notify_xft_idle_id == 0
        && (g_str_has_prefix (prop_name, "/Xft/")
            || g_str_has_prefix (prop_name, "/Gtk/CursorTheme")))
    {
        helper->notify_xft_idle_id = g_idle_add (xfce_xsettings_helper_notify_xft_idle, helper);
    }
}



static void
xfce_xsettings_helper_load (XfceXSettingsHelper *helper)
{
    GHashTable *props;

    props = xfconf_channel_get_properties (helper->channel, NULL);
    if (G_LIKELY (props != NULL))
      {
        /* steal properties and put them in the settings table */
        g_hash_table_foreach_steal (props,
            (GHRFunc) xfce_xsettings_helper_prop_load, helper);

        /* destroy the remaining properties */
        g_hash_table_destroy (props);
      }
}



static void
xfce_xsettings_helper_setting_free (gpointer data)
{
    XfceXSetting *setting = data;

    g_value_unset (setting->value);
    g_free (setting->value);
    g_slice_free (XfceXSetting, setting);
}



static gint
xfce_xsettings_helper_screen_dpi (XfceXSettingsScreen *screen)
{
    Screen *xscreen;
    gint    width_mm, width_dpi;
    gint    height_mm, height_dpi;
    gint    dpi = DPI_FALLBACK;

    xscreen = ScreenOfDisplay (screen->xdisplay, screen->screen_num);
    if (G_LIKELY (xscreen != NULL))
    {
        width_mm = WidthMMOfScreen (xscreen);
        height_mm = HeightMMOfScreen (xscreen);

        if (G_LIKELY (width_mm > 0 && height_mm > 0))
        {
            width_dpi = 25.4 * WidthOfScreen (xscreen) / width_mm;
            height_dpi = 25.4 * HeightOfScreen (xscreen) / height_mm;

            /* both values need to be reasonable */
            if (width_dpi > DPI_LOW_REASONABLE && width_dpi < DPI_HIGH_REASONABLE
                && height_dpi > DPI_LOW_REASONABLE && height_dpi < DPI_HIGH_REASONABLE)
            {
                /* gnome takes the average between the two, however the
                 * minimin seems to result in sharper font in more cases */
                dpi = MIN (width_dpi, height_dpi);
            }
        }
    }

    xfsettings_dbg_filtered (XFSD_DEBUG_XSETTINGS, "calculated dpi of %d for screen %d",
                             dpi, screen->screen_num);

    return dpi;
}



static void
xfce_xsettings_helper_notify_xft_update (GString      *resource,
                                         const gchar  *name,
                                         const GValue *value)
{
    gchar       *found;
    gchar       *end;
    const gchar *str = NULL;
    gchar        s[64];
    gint         num;

    g_return_if_fail (g_str_has_suffix (name, ":"));

    /* remove the old property */
    found = strstr (resource->str, name);
    if (found != NULL)
    {
        end = strchr (found, '\n');
        g_string_erase (resource, found - resource->str,
                        end != NULL ? end - found + 1 : -1);
    }

    switch (G_VALUE_TYPE (value))
    {
        case G_TYPE_STRING:
            str = g_value_get_string (value);
            break;

        case G_TYPE_BOOLEAN:
            str = g_value_get_boolean (value) ? "1" : "0";
            break;

        case G_TYPE_INT:
            num = g_value_get_int (value);

            /* -1 means default in xft, so only remove it */
            if (num == -1)
                return;

            /* special case for dpi */
            if (strcmp (name, "Xft.dpi:") == 0)
                num = CLAMP (num, DPI_LOW_REASONABLE, DPI_HIGH_REASONABLE);

            g_snprintf  (s, sizeof (s), "%d", num);
            str = s;
            break;

        default:
            g_assert_not_reached ();
    }

    if (str != NULL)
    {
        /* append a new line if required */
        if (!g_str_has_suffix (resource->str, "\n"))
            g_string_append_c (resource, '\n');

        g_string_append_printf (resource, "%s\t%s\n", name, str);
    }
}



static void
xfce_xsettings_helper_notify_xft (XfceXSettingsHelper *helper)
{
    Display      *xdisplay;
    gchar        *str;
    GString      *resource;
    XfceXSetting *setting;
    guint         i;
    GValue        bool_val = { 0, };
    const gchar  *props[][2] =
    {
        /* { xfconf name}, { xft name } */
        { "/Xft/Antialias", "Xft.antialias:" },
        { "/Xft/Hinting", "Xft.hinting:" },
        { "/Xft/HintStyle", "Xft.hintstyle:" },
        { "/Xft/RGBA", "Xft.rgba:" },
        { "/Xft/Lcdfilter", "Xft.lcdfilter:" },
        { "/Xft/DPI", "Xft.dpi:" },
        { "/Gtk/CursorThemeName", "Xcursor.theme:" },
        { "/Gtk/CursorThemeSize", "Xcursor.size:" }
    };

    g_return_if_fail (XFCE_IS_XSETTINGS_HELPER (helper));

    if (G_LIKELY (helper->screens == NULL))
        return;

    xdisplay = XOpenDisplay (NULL);
    g_return_if_fail (xdisplay != NULL);

    /* get the resource string from this display from screen zero */
    str = XResourceManagerString (xdisplay);
    resource = g_string_new (str);

    /* update/insert the properties */
    for (i = 0; i < G_N_ELEMENTS (props); i++)
    {
        setting = g_hash_table_lookup (helper->settings, props[i][0]);
        if (G_LIKELY (setting != NULL))
        {
            xfce_xsettings_helper_notify_xft_update (resource, props[i][1],
                                                     setting->value);
        }
    }

    /* set for Xcursor.theme */
    g_value_init (&bool_val, G_TYPE_BOOLEAN);
    g_value_set_boolean (&bool_val, TRUE);
    xfce_xsettings_helper_notify_xft_update (resource, "Xcursor.theme_core:", &bool_val);
    g_value_unset (&bool_val);

    gdk_x11_display_error_trap_push (gdk_display_get_default ());

    /* set the new resource manager string */
    XChangeProperty (xdisplay,
                     RootWindow (xdisplay, 0),
                     XA_RESOURCE_MANAGER, XA_STRING, 8,
                     PropModeReplace,
                     (guchar *) resource->str,
                     resource->len);

    XCloseDisplay (xdisplay);

    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
        g_critical ("Failed to update the resource manager string");

    xfsettings_dbg (XFSD_DEBUG_XSETTINGS,
                    "resource manager (xft) changed (len=%"G_GSIZE_FORMAT")",
                    resource->len);

    g_string_free (resource, TRUE);
}



static void
xfce_xsettings_helper_setting_append (const gchar         *name,
                                      XfceXSetting        *setting,
                                      XfceXSettingsNotify *notify)
{
    gsize        buf_len, new_len;
    gsize        name_len, name_len_pad;
    gsize        value_len, value_len_pad;
    const gchar *str = NULL;
    guchar      *needle;
    guchar       type = 0;
    gint         num;

    name_len = strlen (name) - 1 /* -1 for the xfconf slash */;
    name_len_pad = XSETTINGS_PAD (name_len, 4);

    buf_len = 8 + name_len_pad;
    value_len_pad = value_len = 0;

    /* get the total size of this setting */
    switch (G_VALUE_TYPE (setting->value))
    {
        case G_TYPE_INT:
        case G_TYPE_BOOLEAN:
            type = XSettingsTypeInteger;
            buf_len += 4;
            break;

        case G_TYPE_STRING:
            type = XSettingsTypeString;
            buf_len += 4;
            str = g_value_get_string (setting->value);
            if (str != NULL)
            {
                value_len = strlen (str);
                value_len_pad = XSETTINGS_PAD (value_len, 4);
                buf_len += value_len_pad;
            }
            break;

        case G_TYPE_INT64 /* TODO */:
            type = XSettingsTypeColor;
            buf_len += 8;
            break;

        default:
            g_assert_not_reached ();
            break;
    }

    /* additional length for this setting */
    new_len = notify->buf_len + buf_len;

    /* resize the buffer to fit this setting */
    notify->buf = g_renew (guchar, notify->buf, new_len);
    if (G_UNLIKELY (notify->buf == NULL))
      return;
    needle = notify->buf + notify->buf_len;
    notify->buf_len = new_len;

    /* setting record:
     *
     * 1  SETTING_TYPE  type
     * 1                unused
     * 2  n             name-len
     * n  STRING8       name
     * P                unused, p=pad(n)
     * 4  CARD32        last-change-serial
     */

    /* setting type */
    *needle++ = type;

    /* unused */
    *needle++ = 0;

    /* name length */
    *(CARD16 *)needle = name_len;
    needle += 2;

    /* name */
    memcpy (needle, name + 1 /* +1 for the xfconf slash */, name_len);
    needle += name_len;

    /* zero the padding */
    for (; name_len_pad > name_len; name_len_pad--)
        *needle++ = 0;

    /* setting's last change serial */
    *(CARD32 *)needle = setting->last_change_serial;
    needle += 4;

    /* set setting value */
    switch (type)
    {
        case XSettingsTypeString:
            /* body for XSettingsTypeString:
             *
             * 4  n        value-len
             * n  STRING8  value
             * P           unused, p=pad(n)
             */
            if (G_LIKELY (value_len > 0 && str != NULL))
            {
                /* value length */
                *(CARD32 *)needle = value_len;
                needle += 4;

                /* value */
                memcpy (needle, str, value_len);
                needle += value_len;

                /* zero the padding */
                for (; value_len_pad > value_len; value_len_pad--)
                    *needle++ = 0;
            }
            else
            {
                /* value length */
                *(CARD32 *)needle = 0;
                needle += 4;
            }
            break;

        case XSettingsTypeInteger:
            /* Body for XSettingsTypeInteger:
             *
             * 4  INT32  value
             */
            if (G_VALUE_TYPE (setting->value) == G_TYPE_INT)
            {
                num = g_value_get_int (setting->value);

                /* special case handling for DPI */
                if (strcmp (name, "/Xft/DPI") == 0)
                {
                    /* remember the offset for screen dependend dpi
                     * or clamp the value and set 1/1024ths of an inch
                     * for Xft */
                    if (num < 1)
                        notify->dpi_offset = needle - notify->buf;
                    else
                        num = CLAMP (num, DPI_LOW_REASONABLE, DPI_HIGH_REASONABLE) * 1024;
                }
            }
            else
            {
                num = g_value_get_boolean (setting->value);
            }

            *(INT32 *)needle = num;
            needle += 4;
            break;

        /* TODO */
        case XSettingsTypeColor:
            /* body for XSettingsTypeColor:
            *
            * 2  CARD16  red
            * 2  CARD16  blue
            * 2  CARD16  green
            * 2  CARD16  alpha
            */
            *(CARD16 *)needle = 0;
            *(CARD16 *)(needle + 2) = 0;
            *(CARD16 *)(needle + 4) = 0;
            *(CARD16 *)(needle + 6) = 0;
            needle += 8;
            break;

        default:
            g_assert_not_reached ();
            break;
    }

    notify->n_settings++;
}



static void
xfce_xsettings_helper_notify (XfceXSettingsHelper *helper)
{
    XfceXSettingsNotify *notify;
    CARD32               orderint = 0x01020304;
    guchar              *needle;
    XfceXSettingsScreen *screen;
    GSList              *li;
    gint                 dpi;

    g_return_if_fail (XFCE_IS_XSETTINGS_HELPER (helper));

    notify = g_slice_new0 (XfceXSettingsNotify);
    notify->buf = g_new0 (guchar, 12);
    if (G_UNLIKELY (notify->buf == NULL))
      goto errnomem;
    notify->buf_len = 12;
    needle = notify->buf;

    /* general notification form:
     *
     * 1  CARD8   byte-order
     * 3          unused
     * 4  CARD32  SERIAL
     * 4  CARD32  N_SETTINGS
     */

    /* byte-order */
    *(CARD8 *)needle = (*(char *)&orderint == 1) ? MSBFirst : LSBFirst;
    needle += 4;

    /* serial for this notification */
    *(CARD32 *)needle = helper->serial++;

    /* add all the settings */
    g_hash_table_foreach (helper->settings,
        (GHFunc) xfce_xsettings_helper_setting_append, notify);

    if (G_UNLIKELY (notify->buf == NULL))
      goto errnomem;

    /* number of settings */
    needle = notify->buf + 8;
    *(CARD32 *)needle = notify->n_settings;

    gdk_x11_display_error_trap_push (gdk_display_get_default ());

    /* set new xsettings buffer to the screens */
    for (li = helper->screens; li != NULL; li = li->next)
    {
        screen = li->data;

        /* set the accurate dpi for this screen */
        if (notify->dpi_offset > 0)
        {
            dpi = xfce_xsettings_helper_screen_dpi (screen);
            needle = notify->buf + notify->dpi_offset;
            *(INT32 *)needle = dpi * 1024;
        }

        XChangeProperty (screen->xdisplay, screen->window,
                         helper->xsettings_atom, helper->xsettings_atom,
                         8, PropModeReplace, notify->buf, notify->buf_len);
    }

    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
    {
        g_critical ("Failed to set properties");
    }

    xfsettings_dbg (XFSD_DEBUG_XSETTINGS,
                    "%d settings changed (serial=%lu, len=%"G_GSIZE_FORMAT")",
                    notify->n_settings, helper->serial - 1, notify->buf_len);

    g_free (notify->buf);
  errnomem:
    g_slice_free (XfceXSettingsNotify, notify);
}



static void
xfce_xsettings_helper_screen_free (XfceXSettingsScreen *screen)
{
    XDestroyWindow (screen->xdisplay, screen->window);
    g_slice_free (XfceXSettingsScreen, screen);
}



static GdkFilterReturn
xfce_xsettings_helper_event_filter (GdkXEvent *gdkxevent,
                                    GdkEvent  *gdkevent,
                                    gpointer   data)
{
    XfceXSettingsHelper *helper = XFCE_XSETTINGS_HELPER (data);
    GSList              *li;
    XfceXSettingsScreen *screen;
    XEvent              *xevent = gdkxevent;

    /* check if another settings manager took over the selection
     * of one of the windows */
    if (xevent->xany.type == SelectionClear)
    {
        for (li = helper->screens; li != NULL; li = li->next)
        {
            screen = li->data;

            if (xevent->xany.window == screen->window
                && xevent->xselectionclear.selection == screen->selection_atom)
            {
                /* remove the screen */
                helper->screens = g_slist_delete_link (helper->screens, li);
                xfce_xsettings_helper_screen_free (screen);

                xfsettings_dbg (XFSD_DEBUG_XSETTINGS, "lost selection, %d screens left",
                                g_slist_length (helper->screens));

                /* remove this filter if there are no screens */
                if (helper->screens == NULL)
                    gdk_window_remove_filter (NULL, xfce_xsettings_helper_event_filter, data);

                return GDK_FILTER_REMOVE;
            }
        }
    }

    return GDK_FILTER_CONTINUE;
}



static Bool
xfce_xsettings_helper_timestamp_predicate (Display  *xdisplay,
                                           XEvent   *xevent,
                                           XPointer  arg)
{
    struct _XfceTimestamp *ts = (struct _XfceTimestamp *)arg;

    return (xevent->type == PropertyNotify
            && xevent->xproperty.window == ts->window
            && xevent->xproperty.atom == ts->atom);
}



Time
xfce_xsettings_get_server_time (Display *xdisplay,
                                Window   window)
{
    struct _XfceTimestamp *ts = g_malloc(sizeof(struct _XfceTimestamp));
    guchar c = 'a';
    XEvent xevent;

    /* get the current xserver timestamp */
    ts->atom = XInternAtom (xdisplay, "_TIMESTAMP_PROP", False);
    ts->window = window;
    XChangeProperty (xdisplay, window, ts->atom, ts->atom,
                     8, PropModeReplace, &c, 1);
    XIfEvent (xdisplay, &xevent, xfce_xsettings_helper_timestamp_predicate,
              (XPointer)ts);
    g_free(ts);
    return xevent.xproperty.time;
}



gboolean
xfce_xsettings_helper_register (XfceXSettingsHelper *helper,
                                GdkDisplay          *gdkdisplay,
                                gboolean             force_replace)
{
    Display             *xdisplay;
    Window               root_window;
    Window               window;
    gchar                atom_name[64];
    Atom                 selection_atom;
    gint                 n_screens, n;

    XfceXSettingsScreen *screen;
    Time                 timestamp;
    XClientMessageEvent  xev;
    gboolean             succeed;

    g_return_val_if_fail (GDK_IS_DISPLAY (gdkdisplay), FALSE);
    g_return_val_if_fail (XFCE_IS_XSETTINGS_HELPER (helper), FALSE);
    g_return_val_if_fail (helper->screens == NULL, FALSE);

    xdisplay = GDK_DISPLAY_XDISPLAY (gdkdisplay);
    helper->xsettings_atom = XInternAtom (xdisplay, "_XSETTINGS_SETTINGS", False);

    gdk_x11_display_grab (gdkdisplay);
    gdk_x11_display_error_trap_push (gdkdisplay);

    /* Previously, gdk_display_get_n_screens. Since Gtk 3.10, the number of screens is always 1. */
    n_screens = 1;
    for (n = 0; n < n_screens; n++)
    {
        g_snprintf (atom_name, sizeof (atom_name), "_XSETTINGS_S%d", n);
        selection_atom = XInternAtom (xdisplay, atom_name, False);

        if (!force_replace
            && XGetSelectionOwner (xdisplay, selection_atom) != None)
        {
            g_message ("Skipping screen %d, it already has an xsettings manager...", n);
            continue;
        }

        succeed = FALSE;

        /* create new window */
        root_window = RootWindow (xdisplay, n);
        window = XCreateSimpleWindow (xdisplay, root_window, -1, -1, 1, 1, 0, 0, 0);
        g_assert (window != 0);
        XSelectInput (xdisplay, window, PropertyChangeMask);

        /* get the current xserver timestamp */
        timestamp = xfce_xsettings_get_server_time (xdisplay, window);

        /* request ownership of the xsettings selection on this screen */
        XSetSelectionOwner (xdisplay, selection_atom, window, timestamp);

        /* check if the have the selection */
        if (G_LIKELY (XGetSelectionOwner (xdisplay, selection_atom) == window))
        {
            /* register this xsettings window for this screen */
            xev.type = ClientMessage;
            xev.window = root_window;
            xev.message_type = XInternAtom (xdisplay, "MANAGER", True);
            xev.format = 32;
            xev.data.l[0] = timestamp;
            xev.data.l[1] = selection_atom;
            xev.data.l[2] = window;
            xev.data.l[3] = 0;  /* manager specific data */
            xev.data.l[4] = 0;  /* manager specific data */

            if (XSendEvent (xdisplay, root_window, False,
                            StructureNotifyMask, (XEvent *)&xev) != 0)
            {
                /* the window was successfully registered as the new
                 * xsettings window for this screen */
                succeed = TRUE;
            }
            else
            {
                g_warning ("Failed to register the xsettings window for screen %d", n);
            }
        }
        else
        {
            g_warning ("Unable to get the xsettings selection for screen %d", n);
        }

        if (G_LIKELY (succeed))
        {
            /* add the window to the internal list */
            screen = g_slice_new0 (XfceXSettingsScreen);
            screen->window = window;
            screen->selection_atom = selection_atom;
            screen->xdisplay = xdisplay;
            screen->screen_num = n;

            xfsettings_dbg (XFSD_DEBUG_XSETTINGS, "%s registered on screen %d", atom_name, n);

            helper->screens = g_slist_prepend (helper->screens, screen);
        }
        else
        {
            XDestroyWindow (xdisplay, window);
        }
    }

    gdk_display_sync (gdkdisplay);
    gdk_x11_display_ungrab (gdkdisplay);
    if (gdk_x11_display_error_trap_pop (gdkdisplay) != 0)
        g_critical ("Failed to initialize screens");

    if (helper->screens != NULL)
    {
        /* watch for selection changes */
        gdk_window_add_filter (NULL, xfce_xsettings_helper_event_filter, helper);

        /* send notifications */
        xfce_xsettings_helper_notify (helper);
        xfce_xsettings_helper_notify_xft (helper);

        /* startup fontconfig monitoring */
        helper->fc_init_id = g_idle_add (xfce_xsettings_helper_fc_init, helper);

        return TRUE;
    }

    return FALSE;
}
