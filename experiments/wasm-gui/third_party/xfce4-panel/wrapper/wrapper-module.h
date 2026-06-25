/*
 * Copyright (C) 2009 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __WRAPPER_MODULE_H__
#define __WRAPPER_MODULE_H__

#include <gtk/gtk.h>
#include <libxfce4panel/xfce-panel-plugin-provider.h>

G_BEGIN_DECLS

typedef struct _WrapperModuleClass WrapperModuleClass;
typedef struct _WrapperModule      WrapperModule;

#define WRAPPER_TYPE_MODULE            (wrapper_module_get_type ())
#define WRAPPER_MODULE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WRAPPER_TYPE_MODULE, WrapperModule))
#define WRAPPER_MODULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WRAPPER_TYPE_MODULE, WrapperModuleClass))
#define WRAPPER_IS_MODULE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WRAPPER_TYPE_MODULE))
#define WRAPPER_IS_MODULE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), WRAPPER_TYPE_MODULE))
#define WRAPPER_MODULE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), WRAPPER_TYPE_MODULE, WrapperModuleClass))

GType          wrapper_module_get_type     (void) G_GNUC_CONST;

WrapperModule *wrapper_module_new          (GModule        *library) G_GNUC_MALLOC;

GtkWidget     *wrapper_module_new_provider (WrapperModule  *module,
                                            GdkScreen      *screen,
                                            const gchar    *name,
                                            gint            unique_id,
                                            const gchar    *display_name,
                                            const gchar    *comment,
                                            gchar         **arguments) G_GNUC_MALLOC;

G_END_DECLS

#endif /* !__WRAPPER_MODULE_H__ */
