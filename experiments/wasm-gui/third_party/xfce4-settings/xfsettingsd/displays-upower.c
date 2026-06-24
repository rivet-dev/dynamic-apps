/*
 *  Copyright (C) 2012 Lionel Le Folgoc <lionel@lefolgoc.net>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <upower.h>

#include <X11/extensions/Xrandr.h>

#include "debug.h"
#include "displays-upower.h"



static void             xfce_displays_upower_dispose                        (GObject                 *object);

#if UP_CHECK_VERSION(0, 99, 0)
static void             xfce_displays_upower_property_changed               (UpClient                *client,
                                                                             GParamSpec              *pspec,
                                                                             XfceDisplaysUPower      *upower);
#else
static void             xfce_displays_upower_property_changed               (UpClient                *client,
                                                                             XfceDisplaysUPower      *upower);
#endif


struct _XfceDisplaysUPowerClass
{
    GObjectClass __parent__;

    void         (*lid_changed)     (XfceDisplaysUPower *upower,
                                     gboolean            lid_is_closed);
};

struct _XfceDisplaysUPower
{
    GObject   __parent__;

    UpClient *client;
    gint      handler;

    guint     lid_is_closed : 1;
};

enum
{
    LID_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};



G_DEFINE_TYPE (XfceDisplaysUPower, xfce_displays_upower, G_TYPE_OBJECT);



static void
xfce_displays_upower_class_init (XfceDisplaysUPowerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->dispose = xfce_displays_upower_dispose;

    signals[LID_CHANGED] =
        g_signal_new ("lid-changed",
                      XFCE_TYPE_DISPLAYS_UPOWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (XfceDisplaysUPowerClass, lid_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}



static void
xfce_displays_upower_init (XfceDisplaysUPower *upower)
{
    upower->client = up_client_new ();
    if (!UP_IS_CLIENT (upower->client))
    {
        upower->handler = 0;
        upower->lid_is_closed = 0;
        return;
    }

    upower->lid_is_closed = up_client_get_lid_is_closed (upower->client);
#if UP_CHECK_VERSION(0, 99, 0)
    upower->handler = g_signal_connect (G_OBJECT (upower->client),
                                        "notify",
                                        G_CALLBACK (xfce_displays_upower_property_changed),
                                        upower);
#else
    upower->handler = g_signal_connect (G_OBJECT (upower->client),
                                        "changed",
                                        G_CALLBACK (xfce_displays_upower_property_changed),
                                        upower);
#endif
}



static void
xfce_displays_upower_dispose (GObject *object)
{
    XfceDisplaysUPower *upower = XFCE_DISPLAYS_UPOWER (object);

    if (upower->handler > 0)
    {
        g_signal_handler_disconnect (G_OBJECT (upower->client),
                                     upower->handler);
        g_object_unref (upower->client);
        upower->handler = 0;
    }

    (*G_OBJECT_CLASS (xfce_displays_upower_parent_class)->dispose) (object);
}



static void
#if UP_CHECK_VERSION(0, 99, 0)
xfce_displays_upower_property_changed (UpClient           *client,
                                       GParamSpec         *pspec,
                                       XfceDisplaysUPower *upower)
#else
xfce_displays_upower_property_changed (UpClient           *client,
                                       XfceDisplaysUPower *upower)
#endif
{
    gboolean lid_is_closed;

    /* no lid, no chocolate */
    if (!up_client_get_lid_is_present (client))
        return;

    lid_is_closed = up_client_get_lid_is_closed (client);
    if (upower->lid_is_closed != lid_is_closed)
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "UPower lid event received (%s -> %s).",
                        XFSD_LID_STR (upower->lid_is_closed), XFSD_LID_STR (lid_is_closed));

        upower->lid_is_closed = lid_is_closed;
        g_signal_emit (G_OBJECT (upower), signals[LID_CHANGED], 0, upower->lid_is_closed);
    }
}
