/* $Id$ */
/*-
 * Copyright (c) 2003-2007 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

#if !defined(_LIBXFCE4UTIL_INSIDE_LIBXFCE4UTIL_H) && !defined(LIBXFCE4UTIL_COMPILATION)
#error "Only <libxfce4util/libxfce4util.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __XFCE_RC_H__
#define __XFCE_RC_H__

#include <glib-object.h>
#include <libxfce4util/xfce-resource.h>

G_BEGIN_DECLS

typedef struct _XfceRc XfceRc;

#define XFCE_TYPE_RC       (xfce_rc_get_type ())
#define XFCE_RC(obj)       ((XfceRc *) (obj))
#define XFCE_RC_CONST(obj) ((const XfceRc *) (obj))

GType        xfce_rc_get_type                (void);

XfceRc*      xfce_rc_simple_open             (const gchar     *filename,
                                              gboolean         readonly) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

XfceRc*      xfce_rc_config_open             (XfceResourceType type,
                                              const gchar     *resource,
                                              gboolean         readonly) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

void         xfce_rc_close                   (XfceRc *rc);
void         xfce_rc_flush                   (XfceRc *rc);
void         xfce_rc_rollback                (XfceRc *rc);

gboolean     xfce_rc_is_dirty                (const XfceRc *rc) G_GNUC_WARN_UNUSED_RESULT;
gboolean     xfce_rc_is_readonly             (const XfceRc *rc) G_GNUC_WARN_UNUSED_RESULT;

const gchar* xfce_rc_get_locale              (const XfceRc *rc) G_GNUC_WARN_UNUSED_RESULT;

gchar**      xfce_rc_get_groups              (const XfceRc *rc) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar**      xfce_rc_get_entries             (const XfceRc *rc,
                                              const gchar  *group) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

void         xfce_rc_delete_group            (XfceRc       *rc,
                                              const gchar  *group,
                                              gboolean      global);
const gchar* xfce_rc_get_group               (const XfceRc *rc) G_GNUC_WARN_UNUSED_RESULT;
gboolean     xfce_rc_has_group               (const XfceRc *rc,
                                              const gchar  *group) G_GNUC_WARN_UNUSED_RESULT;
void         xfce_rc_set_group               (XfceRc       *rc,
                                              const gchar  *group);

void         xfce_rc_delete_entry            (XfceRc       *rc,
                                              const gchar  *key,
                                              gboolean      global);
gboolean     xfce_rc_has_entry               (const XfceRc *rc,
                                              const gchar  *key) G_GNUC_WARN_UNUSED_RESULT;

const gchar* xfce_rc_read_entry              (const XfceRc *rc,
                                              const gchar  *key,
                                              const gchar  *fallback) G_GNUC_WARN_UNUSED_RESULT;
const gchar* xfce_rc_read_entry_untranslated (const XfceRc *rc,
                                              const gchar  *key,
                                              const gchar  *fallback) G_GNUC_WARN_UNUSED_RESULT;
gboolean     xfce_rc_read_bool_entry         (const XfceRc *rc,
                                              const gchar  *key,
                                              gboolean      fallback) G_GNUC_WARN_UNUSED_RESULT;
gint         xfce_rc_read_int_entry          (const XfceRc *rc,
                                              const gchar  *key,
                                              gint          fallback) G_GNUC_WARN_UNUSED_RESULT;
gchar**      xfce_rc_read_list_entry         (const XfceRc *rc,
                                              const gchar  *key,
                                              const gchar  *delimiter) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
void         xfce_rc_write_entry             (XfceRc       *rc,
                                              const gchar  *key,
                                              const gchar  *value);
void         xfce_rc_write_bool_entry        (XfceRc       *rc,
                                              const gchar  *key,
                                              gboolean      value);
void         xfce_rc_write_int_entry         (XfceRc       *rc,
                                              const gchar  *key,
                                              gint          value);
void         xfce_rc_write_list_entry        (XfceRc       *rc,
                                              const gchar  *key,
                                              gchar       **value,
                                              const gchar  *separator);

G_END_DECLS

#endif /* !__XFCE_RC_H__ */

