/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(THUNARX_INSIDE_THUNARX_H) && !defined(THUNARX_COMPILATION)
#error "Only <thunarx/thunarx.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __THUNARX_FILE_INFO_H__
#define __THUNARX_FILE_INFO_H__

#include <gio/gio.h>

#include <thunarx/thunarx-config.h>

G_BEGIN_DECLS

/*
 * File information namespaces available in the #GFileInfo returned by
 * thunarx_file_info_get_file_info().
 */
#define THUNARX_FILE_INFO_NAMESPACE \
  "access::*," \
  "id::filesystem," \
  "mountable::can-mount,standard::target-uri," \
  "preview::*," \
  "standard::type,standard::is-hidden,standard::is-backup," \
  "standard::is-symlink,standard::name,standard::display-name," \
  "standard::size,standard::symlink-target," \
  "time::*," \
  "trash::*," \
  "recent::*," \
  "unix::gid,unix::uid,unix::mode," \
  "metadata::emblems," \
  "metadata::thunar-view-type," \
  "metadata::thunar-sort-column,metadata::thunar-sort-order," \
  "metadata::thunar-zoom-level," \
  "metadata::thunar-highlight-color-background,metadata::thunar-highlight-color-foreground" \


/*
 * Filesystem information namespaces available in the #GFileInfo
 * returned by thunarx_file_info_get_filesystem_info().
 */
#define THUNARX_FILESYSTEM_INFO_NAMESPACE \
  "filesystem::*"

typedef struct _ThunarxFileInfoIface ThunarxFileInfoIface;
typedef struct _ThunarxFileInfo      ThunarxFileInfo;

#define THUNARX_TYPE_FILE_INFO            (thunarx_file_info_get_type ())
#define THUNARX_FILE_INFO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNARX_TYPE_FILE_INFO, ThunarxFileInfo))
#define THUNARX_IS_FILE_INFO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNARX_TYPE_FILE_INFO))
#define THUNARX_FILE_INFO_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), THUNARX_TYPE_FILE_INFO, ThunarxFileInfoIface))

/**
 * ThunarxFileInfoIface:
 * @get_name: See thunarx_file_info_get_name().
 * @get_uri: See thunarx_file_info_get_uri().
 * @get_parent_uri: See thunarx_file_info_get_parent_uri().
 * @get_uri_scheme: See thunarx_file_info_get_uri_scheme().
 * @get_mime_type: See thunarx_file_info_get_mime_type().
 * @has_mime_type: See thunarx_file_info_has_mime_type().
 * @is_directory: See thunarx_file_info_is_directory().
 * @get_file_info: See thunarx_file_info_get_file_info().
 * @get_filesystem_info: See thunarx_filesystem_info_get_filesystem_info().
 * @get_location: See thunarx_location_get_location().
 * @changed: See thunarx_file_info_changed().
 * @renamed: See thunarx_file_info_renamed().
 *
 * Interface with virtual methods implemented by the file manager and accessible
 * from the extensions.
 */

struct _ThunarxFileInfoIface
{
  /*< private >*/
  GTypeInterface __parent__;

  /*< public >*/

  /* virtual methods */
  gchar     *(*get_name)            (ThunarxFileInfo *file_info);

  gchar     *(*get_uri)             (ThunarxFileInfo *file_info);
  gchar     *(*get_parent_uri)      (ThunarxFileInfo *file_info);
  gchar     *(*get_uri_scheme)      (ThunarxFileInfo *file_info);

  gchar     *(*get_mime_type)       (ThunarxFileInfo *file_info);
  gboolean   (*has_mime_type)       (ThunarxFileInfo *file_info,
                                     const gchar     *mime_type);

  gboolean   (*is_directory)        (ThunarxFileInfo *file_info);

  GFileInfo *(*get_file_info)       (ThunarxFileInfo *file_info);
  GFileInfo *(*get_filesystem_info) (ThunarxFileInfo *file_info);
  GFile     *(*get_location)        (ThunarxFileInfo *file_info);

  /*< private >*/
  void (*reserved0) (void);
  void (*reserved1) (void);
  void (*reserved2) (void);
  void (*reserved3) (void);
  void (*reserved4) (void);
  void (*reserved5) (void);
  void (*reserved6) (void);

  /*< public >*/

  /* signals */
  void (*changed) (ThunarxFileInfo *file_info);
  void (*renamed) (ThunarxFileInfo *file_info);

  /*< private >*/
  void (*reserved7) (void);
  void (*reserved8) (void);
  void (*reserved9) (void);
};

GType      thunarx_file_info_get_type            (void) G_GNUC_CONST;

gchar     *thunarx_file_info_get_name            (ThunarxFileInfo *file_info);
gchar     *thunarx_file_info_get_uri             (ThunarxFileInfo *file_info);
gchar     *thunarx_file_info_get_parent_uri      (ThunarxFileInfo *file_info);
gchar     *thunarx_file_info_get_uri_scheme      (ThunarxFileInfo *file_info);

gchar     *thunarx_file_info_get_mime_type       (ThunarxFileInfo *file_info);
gboolean   thunarx_file_info_has_mime_type       (ThunarxFileInfo *file_info,
                                                  const gchar     *mime_type);

gboolean   thunarx_file_info_is_directory        (ThunarxFileInfo *file_info);

GFileInfo *thunarx_file_info_get_file_info       (ThunarxFileInfo *file_info);
GFileInfo *thunarx_file_info_get_filesystem_info (ThunarxFileInfo *file_info);
GFile     *thunarx_file_info_get_location        (ThunarxFileInfo *file_info);

void       thunarx_file_info_changed             (ThunarxFileInfo *file_info);
void       thunarx_file_info_renamed             (ThunarxFileInfo *file_info);


#define THUNARX_TYPE_FILE_INFO_LIST (thunarx_file_info_list_get_type ())

GType      thunarx_file_info_list_get_type       (void) G_GNUC_CONST;

GList     *thunarx_file_info_list_copy           (GList           *file_infos);
void       thunarx_file_info_list_free           (GList           *file_infos);

G_END_DECLS

#endif /* !__THUNARX_FILE_INFO_H__ */
