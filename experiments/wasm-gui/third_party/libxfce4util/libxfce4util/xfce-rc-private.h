/* $Id$ */
/*-
 * Copyright (c) 2003-2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __LIBXFCE4UTIL_XFCE_RC_PRIVATE_H__
#define __LIBXFCE4UTIL_XFCE_RC_PRIVATE_H__

#include <libxfce4util/libxfce4util.h>

G_BEGIN_DECLS

typedef struct _XfceRcConfig XfceRcConfig;
typedef struct _XfceRcSimple XfceRcSimple;

struct _XfceRc
{
  /*< private >*/
  void          (*close)        (XfceRc       *rc);
  void          (*flush)        (XfceRc       *rc);
  void          (*rollback)     (XfceRc       *rc);
  gboolean      (*is_dirty)     (const XfceRc *rc);
  gboolean      (*is_readonly)  (const XfceRc *rc);
  gchar       **(*get_groups)   (const XfceRc *rc);
  gchar       **(*get_entries)  (const XfceRc *rc,
                                 const gchar  *group);
  void          (*delete_group) (XfceRc       *rc,
                                 const gchar  *group, gboolean global);
  const gchar  *(*get_group)    (const XfceRc *rc);
  gboolean      (*has_group)    (const XfceRc *rc,
                                 const gchar  *group);
  void          (*set_group)    (XfceRc       *rc,
                                 const gchar  *group);
  void          (*delete_entry) (XfceRc       *rc,
                                 const gchar *key,
                                 gboolean     global);
  gboolean      (*has_entry)    (const XfceRc *rc,
                                 const gchar  *key);
  const gchar  *(*read_entry)   (const XfceRc *rc,
                                 const gchar  *key,
                                 gboolean      translated);
  /* write_entry == NULL means readonly */
  void          (*write_entry)  (XfceRc       *rc,
                                 const gchar  *key,
                                 const gchar  *value);

  gchar   *locale;
  gchar  **languages;
};

#define XFCE_RC_CONFIG(obj)       ((XfceRcConfig *) (obj))
#define XFCE_RC_CONFIG_CONST(obj) ((const XfceRcConfig *) (obj))

#define XFCE_RC_SIMPLE(obj)       ((XfceRcSimple *) (obj))
#define XFCE_RC_SIMPLE_CONST(obj) ((const XfceRcSimple *) (obj))


G_GNUC_INTERNAL void          _xfce_rc_init               (XfceRc       *rc);

G_GNUC_INTERNAL XfceRcSimple* _xfce_rc_simple_new         (XfceRcSimple *shared,
                                                           const gchar  *filename,
                                                           gboolean      readonly);
G_GNUC_INTERNAL gboolean      _xfce_rc_simple_parse       (XfceRcSimple *simple);
G_GNUC_INTERNAL void          _xfce_rc_simple_close       (XfceRc       *rc);
G_GNUC_INTERNAL void          _xfce_rc_simple_flush       (XfceRc       *rc);
G_GNUC_INTERNAL void          _xfce_rc_simple_rollback    (XfceRc       *rc);
G_GNUC_INTERNAL gboolean      _xfce_rc_simple_is_dirty    (const XfceRc *rc) G_GNUC_CONST;
G_GNUC_INTERNAL gboolean      _xfce_rc_simple_is_readonly (const XfceRc *rc) G_GNUC_CONST;
G_GNUC_INTERNAL const gchar*  _xfce_rc_simple_get_filename(const XfceRc *rc) G_GNUC_CONST;
G_GNUC_INTERNAL gchar**       _xfce_rc_simple_get_groups  (const XfceRc *rc) G_GNUC_CONST;
G_GNUC_INTERNAL gchar**       _xfce_rc_simple_get_entries (const XfceRc *rc,
                                                           const gchar  *name) G_GNUC_CONST;
G_GNUC_INTERNAL void          _xfce_rc_simple_delete_group(XfceRc       *rc,
                                                           const gchar  *name,
                                                           gboolean      global);
G_GNUC_INTERNAL const gchar*  _xfce_rc_simple_get_group   (const XfceRc *rc) G_GNUC_CONST;
G_GNUC_INTERNAL gboolean      _xfce_rc_simple_has_group   (const XfceRc *rc,
                                                           const gchar  *name) G_GNUC_CONST;
G_GNUC_INTERNAL void          _xfce_rc_simple_set_group   (XfceRc       *rc,
                                                           const gchar  *name);
G_GNUC_INTERNAL void          _xfce_rc_simple_delete_entry(XfceRc       *rc,
                                                           const gchar  *key,
                                                           gboolean      global);
G_GNUC_INTERNAL gboolean      _xfce_rc_simple_has_entry   (const XfceRc *rc,
                                                           const gchar  *key) G_GNUC_CONST;
G_GNUC_INTERNAL const gchar*  _xfce_rc_simple_read_entry  (const XfceRc *rc,
                                                           const gchar  *key,
                                                           gboolean      translated) G_GNUC_CONST;
G_GNUC_INTERNAL void          _xfce_rc_simple_write_entry (XfceRc       *rc,
                                                           const gchar  *key,
                                                           const gchar  *value);

G_GNUC_INTERNAL XfceRcConfig* _xfce_rc_config_new         (XfceResourceType type,
                                                           const gchar  *resource,
                                                           gboolean      readonly);
G_GNUC_INTERNAL void          _xfce_rc_config_close       (XfceRc       *rc);
G_GNUC_INTERNAL void          _xfce_rc_config_flush       (XfceRc       *rc);
G_GNUC_INTERNAL void          _xfce_rc_config_rollback    (XfceRc       *rc);
G_GNUC_INTERNAL gboolean      _xfce_rc_config_is_dirty    (const XfceRc *rc) G_GNUC_CONST;
G_GNUC_INTERNAL gboolean      _xfce_rc_config_is_readonly (const XfceRc *rc) G_GNUC_CONST;
G_GNUC_INTERNAL gchar**       _xfce_rc_config_get_groups  (const XfceRc *rc) G_GNUC_CONST;
G_GNUC_INTERNAL gchar**       _xfce_rc_config_get_entries (const XfceRc *rc,
                                                           const gchar  *name) G_GNUC_CONST;
G_GNUC_INTERNAL void          _xfce_rc_config_delete_group(XfceRc       *rc,
                                                           const gchar  *name,
                                                           gboolean      global);
G_GNUC_INTERNAL const gchar*  _xfce_rc_config_get_group   (const XfceRc *rc) G_GNUC_CONST;
G_GNUC_INTERNAL gboolean      _xfce_rc_config_has_group   (const XfceRc *rc,
                                                           const gchar  *name) G_GNUC_CONST;
G_GNUC_INTERNAL void          _xfce_rc_config_set_group   (XfceRc       *rc,
                                                           const gchar  *name);
G_GNUC_INTERNAL void          _xfce_rc_config_delete_entry(XfceRc       *rc,
                                                           const gchar  *key,
                                                           gboolean      global);
G_GNUC_INTERNAL gboolean      _xfce_rc_config_has_entry   (const XfceRc *rc,
                                                           const gchar  *key) G_GNUC_CONST;
G_GNUC_INTERNAL const gchar*  _xfce_rc_config_read_entry  (const XfceRc *rc,
                                                           const gchar  *key,
                                                           gboolean      translated) G_GNUC_CONST;
G_GNUC_INTERNAL void          _xfce_rc_config_write_entry (XfceRc       *rc,
                                                           const gchar  *key,
                                                           const gchar  *value);

G_END_DECLS

#endif /* !__LIBXFCE4UTIL_XFCE_RC_PRIVATE_H__ */
