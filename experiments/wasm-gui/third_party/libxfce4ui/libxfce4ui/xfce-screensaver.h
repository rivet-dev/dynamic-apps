/*
 * * Copyright (C) 2016 Eric Koegel <eric@xfce.org>
 *
 * Licensed under the GNU General Public License Version 2
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined (_LIBXFCE4UI_INSIDE_LIBXFCE4UI_H) && !defined (LIBXFCE4UI_COMPILATION)
#error "Only <libxfce4ui/libxfce4ui.h> can be included directly, this file is not part of the public API."
#endif

/* do not change this in __XFCE_SCREENSAVER_H__ or so: it allows xfce4-session and
 * xfce4-power-manager <= 4.18.0 to build with libxfce4ui >= 4.18.2 by avoiding
 * conflicting types */
#ifndef __XFCE_SCREENSAVER_H
#define __XFCE_SCREENSAVER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFCE_TYPE_SCREENSAVER (xfce_screensaver_get_type ())
G_DECLARE_FINAL_TYPE (XfceScreensaver, xfce_screensaver, XFCE, SCREENSAVER, GObject)

/* for compatibility with xfce4-session and xfce4-power-manager <= 4.18.0 as above */
typedef XfceScreensaver XfceScreenSaver;

XfceScreensaver    *xfce_screensaver_new           (void);

void                xfce_screensaver_inhibit       (XfceScreensaver     *saver,
                                                    gboolean             inhibit);

gboolean            xfce_screensaver_lock          (XfceScreensaver     *saver);

G_END_DECLS

#endif /* __XFCE_SCREENSAVER_H */
