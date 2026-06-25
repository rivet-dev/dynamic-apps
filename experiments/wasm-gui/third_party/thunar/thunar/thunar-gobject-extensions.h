/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __THUNAR_GOBJECT_EXTENSIONS_H__
#define __THUNAR_GOBJECT_EXTENSIONS_H__

#include <glib-object.h>

G_BEGIN_DECLS;

/* We don't need to implement all the G_OBJECT_WARN_INVALID_PROPERTY_ID()
 * macros for regular builds, as all properties are only accessible from
 * within Thunar and they will be tested during debug builds. Besides that
 * GObject already performs the required checks prior to calling get_property()
 * and set_property().
 */
#ifndef G_ENABLE_DEBUG
#undef G_OBJECT_WARN_INVALID_PROPERTY_ID
#define G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec) G_STMT_START{ (void)0; }G_STMT_END
#endif

void     thunar_g_initialize_transformations (void);
gchar*   thunar_g_strescape                  (const gchar *source);
gchar*   thunar_g_utf8_normalize_for_search  (const gchar *str,
                                              gboolean     strip_diacritics,
                                              gboolean     casefold);
gboolean thunar_g_app_info_equal             (gpointer     appinfo1,
                                              gpointer     appinfo2);
void     thunar_g_object_set_guint_data      (GObject     *object,
                                              const gchar *key,
                                              guint        data);

G_END_DECLS;

#endif /* !__THUNAR_GOBJECT_EXTENSIONS_H__ */
