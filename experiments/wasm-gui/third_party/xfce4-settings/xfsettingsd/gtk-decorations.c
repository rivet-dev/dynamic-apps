/*
 *  Copyright (c) 2014 Olivier Fourdan <fourdan@xfce.org>
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include "gtk-decorations.h"

#define DEFAULT_LAYOUT "O|HMC"

static void xfce_decorations_helper_finalize                  (GObject                  *object);
static void xfce_decorations_helper_channel_property_changed  (XfconfChannel            *channel,
                                                               const gchar              *property_name,
                                                               const GValue             *value,
                                                               XfceDecorationsHelper    *helper);

struct _XfceDecorationsHelperClass
{
    GObjectClass __parent__;
};

struct _XfceDecorationsHelper
{
    GObject  __parent__;

    /* xfconf channel */
    XfconfChannel *wm_channel;
    XfconfChannel *xsettings_channel;
};

G_DEFINE_TYPE (XfceDecorationsHelper, xfce_decorations_helper, G_TYPE_OBJECT)

static void
xfce_decorations_helper_class_init (XfceDecorationsHelperClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_decorations_helper_finalize;
}

static const gchar *
xfce_decorations_button_layout_xlate (char c)
{
    switch (c)
    {
        case 'O':
            return "menu";
            break;
        case 'H':
            return "minimize";
            break;
        case 'M':
            return "maximize";
            break;
        case 'C':
            return "close";
            break;
        case '|':
            return ":";
            break;
        default:
            return NULL;
    }
    return NULL;
}

static void
xfce_decorations_set_decoration_layout (XfceDecorationsHelper *helper,
                                        const gchar *value)
{
    GString *join;
    const gchar *gtk_name;
    gchar *gtk_decoration_layout;
    gboolean add_comma;
    int len, i;

    add_comma = FALSE;
    len = strlen (value);
    join = g_string_new (NULL);
    for (i = 0; i < len; i++)
    {
        gtk_name = xfce_decorations_button_layout_xlate(value[i]);
        if (gtk_name)
        {
            if (add_comma && value[i] != '|')
                join = g_string_append (join, ",");
            join = g_string_append (join, gtk_name);
            add_comma = (value[i] != '|');
        }
    }

    gtk_decoration_layout = g_string_free (join, FALSE);
    xfconf_channel_set_string (helper->xsettings_channel,
                             "/Gtk/DecorationLayout", gtk_decoration_layout);
    g_free (gtk_decoration_layout);
}

static void
xfce_decorations_helper_channel_property_changed (XfconfChannel         *channel,
                                                  const gchar           *property_name,
                                                  const GValue          *value,
                                                  XfceDecorationsHelper *helper)
{
    if (strcmp (property_name, "/general/button_layout") == 0)
    {
        xfce_decorations_set_decoration_layout (helper, g_value_get_string (value));
    }
}

static void
xfce_decorations_helper_init (XfceDecorationsHelper *helper)
{
    gchar *layout;

    helper->wm_channel = xfconf_channel_get ("xfwm4");
    helper->xsettings_channel = xfconf_channel_get ("xsettings");

    layout = xfconf_channel_get_string  (helper->wm_channel,
                                         "/general/button_layout", DEFAULT_LAYOUT);
    xfce_decorations_set_decoration_layout (helper, layout);
    g_free (layout);

    /* monitor WM channel changes */
    g_signal_connect (G_OBJECT (helper->wm_channel), "property-changed",
        G_CALLBACK (xfce_decorations_helper_channel_property_changed), helper);
}

static void
xfce_decorations_helper_finalize (GObject *object)
{
    (*G_OBJECT_CLASS (xfce_decorations_helper_parent_class)->finalize) (object);
}

