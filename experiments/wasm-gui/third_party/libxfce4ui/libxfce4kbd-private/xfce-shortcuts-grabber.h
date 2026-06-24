/* vi:set expandtab sw=2 sts=2: */
/*
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __XFCE_SHORTCUTS_GRABBER_H__
#define __XFCE_SHORTCUTS_GRABBER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _XfceShortcutsGrabberPrivate XfceShortcutsGrabberPrivate;
typedef struct _XfceShortcutsGrabberClass   XfceShortcutsGrabberClass;
typedef struct _XfceShortcutsGrabber        XfceShortcutsGrabber;

#define XFCE_TYPE_SHORTCUTS_GRABBER            (xfce_shortcuts_grabber_get_type ())
#define XFCE_SHORTCUTS_GRABBER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_SHORTCUTS_GRABBER, XfceShortcutsGrabber))
#define XFCE_SHORTCUTS_GRABBER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_SHORTCUTS_GRABBER, XfceShortcutsGrabberClass))
#define XFCE_IS_SHORTCUTS_GRABBER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_SHORTCUTS_GRABBER))
#define XFCE_IS_SHORTCUTS_GRABBER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_SHORTCUTS_GRABBER)
#define XFCE_SHORTCUTS_GRABBER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_SHORTCUTS_GRABBER, XfceShortcutsGrabberClass))

GType                 xfce_shortcuts_grabber_get_type      (void) G_GNUC_CONST;

XfceShortcutsGrabber *xfce_shortcuts_grabber_new           (void) G_GNUC_MALLOC;
void                  xfce_shortcuts_grabber_add           (XfceShortcutsGrabber *grabber,
                                                            const gchar          *shortcut);
void                  xfce_shortcuts_grabber_remove        (XfceShortcutsGrabber *grabber,
                                                            const gchar          *shortcut);



struct _XfceShortcutsGrabberClass
{
  GObjectClass __parent__;
};

struct _XfceShortcutsGrabber
{
  GObject __parent__;

  XfceShortcutsGrabberPrivate *priv;
};

G_END_DECLS

#endif /* !__XFCE_SHORTCUTS_GRABBER_H__ */
