/*
 * Copyright (C) 2008-2009 Nick Schermer <nick@xfce.org>
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

#ifndef __WRAPPER_PLUG_H__
#define __WRAPPER_PLUG_H__

#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin-provider.h>

G_BEGIN_DECLS

typedef struct _WrapperPlugClass WrapperPlugClass;
typedef struct _WrapperPlug      WrapperPlug;

#define WRAPPER_TYPE_PLUG            (wrapper_plug_get_type ())
#define WRAPPER_PLUG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WRAPPER_TYPE_PLUG, WrapperPlug))
#define WRAPPER_PLUG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WRAPPER_TYPE_PLUG, WrapperPlugClass))
#define WRAPPER_IS_PLUG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WRAPPER_TYPE_PLUG))
#define WRAPPER_IS_PLUG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), WRAPPER_TYPE_PLUG))
#define WRAPPER_PLUG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), WRAPPER_TYPE_PLUG, WrapperPlugClass))

extern gchar *wrapper_name;

GType         wrapper_plug_get_type             (void) G_GNUC_CONST;

WrapperPlug  *wrapper_plug_new                  (Window           socket_id);

void          wrapper_plug_set_opacity          (WrapperPlug     *plug,
                                                 gdouble          opacity);

void          wrapper_plug_set_background_alpha (WrapperPlug     *plug,
                                                 gdouble          alpha);

void          wrapper_plug_set_background_color (WrapperPlug     *plug,
                                                 const gchar     *color_string);

void          wrapper_plug_set_background_image (WrapperPlug     *plug,
                                                 const gchar     *image);

G_END_DECLS

#endif /* !__WRAPPER_PLUG_H__ */
