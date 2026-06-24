/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/display.c for the Openbox window manager
   Copyright (c) 2007        Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include "obt/display.h"
#include "obt/prop.h"
#include "obt/internal.h"
#include "obt/keyboard.h"
#include "obt/xqueue.h"

#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

/* from xqueue.c */
extern void xqueue_init(void);
extern void xqueue_destroy(void);

Display* obt_display = NULL;

gboolean obt_display_error_occured = FALSE;

gboolean obt_display_extension_xkb       = FALSE;
gint     obt_display_extension_xkb_basep;
gboolean obt_display_extension_shape     = FALSE;
gint     obt_display_extension_shape_basep;
gboolean obt_display_extension_xinerama  = FALSE;
gint     obt_display_extension_xinerama_basep;
gboolean obt_display_extension_randr     = FALSE;
gint     obt_display_extension_randr_basep;
gboolean obt_display_extension_sync      = FALSE;
gint     obt_display_extension_sync_basep;

static gint xerror_handler(Display *d, XErrorEvent *e);

static gboolean xerror_ignore = FALSE;

gboolean obt_display_open(const char *display_name)
{
    gchar *n;
    Display *d = NULL;

    n = display_name ? g_strdup(display_name) : NULL;
    obt_display = d = XOpenDisplay(n);
    if (d) {
        gint junk, major, minor;
        (void)junk, (void)major, (void)minor;

        if (fcntl(ConnectionNumber(d), F_SETFD, 1) == -1)
            g_message("Failed to set display as close-on-exec");
        XSetErrorHandler(xerror_handler);

        /* read what extensions are present */
#ifdef XKB
        major = XkbMajorVersion;
        minor = XkbMinorVersion;
        obt_display_extension_xkb =
            XkbQueryExtension(d, &junk,
                              &obt_display_extension_xkb_basep, &junk,
                              &major, &minor);
        if (!obt_display_extension_xkb)
            g_message("XKB extension is not present on the server or too old");
#endif

#ifdef SHAPE
        obt_display_extension_shape =
            XShapeQueryExtension(d, &obt_display_extension_shape_basep,
                                 &junk);
        if (!obt_display_extension_shape)
            g_message("X Shape extension is not present on the server");
#endif

#ifdef XINERAMA
        obt_display_extension_xinerama =
            XineramaQueryExtension(d,
                                   &obt_display_extension_xinerama_basep,
                                   &junk) && XineramaIsActive(d);
        if (!obt_display_extension_xinerama)
            g_message("Xinerama extension is not present on the server");
#endif

#ifdef XRANDR
        obt_display_extension_randr =
            XRRQueryExtension(d, &obt_display_extension_randr_basep,
                              &junk);
        if (!obt_display_extension_randr)
            g_message("XRandR extension is not present on the server");
#endif

#ifdef SYNC
        obt_display_extension_sync =
            XSyncQueryExtension(d, &obt_display_extension_sync_basep,
                                &junk) && XSyncInitialize(d, &junk, &junk);
        if (!obt_display_extension_sync)
            g_message("X Sync extension is not present on the server or is an "
                      "incompatible version");
#endif

        obt_prop_startup();
        obt_keyboard_reload();
    }
    g_free(n);

    if (obt_display)
        xqueue_init();

    return obt_display != NULL;
}

void obt_display_close(void)
{
    obt_keyboard_shutdown();
    if (obt_display) {
        xqueue_destroy();
        XCloseDisplay(obt_display);
    }
}

static gint xerror_handler(Display *d, XErrorEvent *e)
{
#ifdef DEBUG
    gchar errtxt[128];

    XGetErrorText(d, e->error_code, errtxt, 127);
    if (!xerror_ignore) {
        if (e->error_code == BadWindow)
            /*g_debug(_("X Error: %s\n"), errtxt)*/;
        else
            g_error("X Error: %s", errtxt);
    } else
        g_debug("Ignoring XError code %d '%s'", e->error_code, errtxt);
#else
    (void)d; (void)e;
#endif

    obt_display_error_occured = TRUE;
    return 0;
}

void obt_display_ignore_errors(gboolean ignore)
{
    XSync(obt_display, FALSE);
    xerror_ignore = ignore;
    if (ignore) obt_display_error_occured = FALSE;
}
