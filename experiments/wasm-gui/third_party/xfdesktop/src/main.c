/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2009 Brian Tarricone, <bjt23@cornell.edu>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 *  Random portions taken from or inspired by the original xfdesktop for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 *  X event forwarding code:
 *     Copyright (c) 2004 Nils Rennebarth
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include "xfdesktop-application.h"

int
main(int argc, char **argv)
{
    XfdesktopApplication *app;
    int ret = 0;

#ifdef G_ENABLE_DEBUG
    /* do NOT remove this line. If something doesn't work,
     * fix your code instead! */
    g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

    /* bind gettext textdomain */
    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    app = xfdesktop_application_get();

    ret = xfdesktop_application_run(app, argc, argv);

    g_object_unref(app);

    return ret;
}
