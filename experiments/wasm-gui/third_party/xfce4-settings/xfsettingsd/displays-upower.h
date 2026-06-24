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

#ifndef __DISPLAYS_UPOWER_H__
#define __DISPLAYS_UPOWER_H__

#include <glib-object.h>

typedef struct _XfceDisplaysUPowerClass XfceDisplaysUPowerClass;
typedef struct _XfceDisplaysUPower      XfceDisplaysUPower;

#define XFCE_TYPE_DISPLAYS_UPOWER            (xfce_displays_upower_get_type ())
#define XFCE_DISPLAYS_UPOWER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_DISPLAYS_UPOWER, XfceDisplaysUPower))
#define XFCE_DISPLAYS_UPOWER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_DISPLAYS_UPOWER, XfceDisplaysUPowerClass))
#define XFCE_IS_DISPLAYS_UPOWER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_DISPLAYS_UPOWER))
#define XFCE_IS_DISPLAYS_UPOWER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_DISPLAYS_UPOWER))
#define XFCE_DISPLAYS_UPOWER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_DISPLAYS_UPOWER, XfceDisplaysUPowerClass))

#define XFSD_LID_STR(b) (b ? "closed" : "open")

GType xfce_displays_upower_get_type (void) G_GNUC_CONST;

#endif /* !__DISPLAYS_UPOWER_H__ */
