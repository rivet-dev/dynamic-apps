/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2012 Nick Schermer <nick@xfce.org>
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

#ifndef __THUNAR_DEVICE_H__
#define __THUNAR_DEVICE_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS

typedef struct _ThunarDeviceClass ThunarDeviceClass;
typedef struct _ThunarDevice      ThunarDevice;
typedef enum   _ThunarDeviceKind  ThunarDeviceKind;

typedef void   (*ThunarDeviceCallback) (ThunarDevice *device,
                                        const GError *error,
                                        gpointer      user_data);

enum _ThunarDeviceKind
{
  THUNAR_DEVICE_KIND_VOLUME,
  THUNAR_DEVICE_KIND_MOUNT_LOCAL,
  THUNAR_DEVICE_KIND_MOUNT_REMOTE
};

#define THUNAR_TYPE_DEVICE             (thunar_device_get_type ())
#define THUNAR_DEVICE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_DEVICE, ThunarDevice))
#define THUNAR_DEVICE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_DEVICE, ThunarDeviceClass))
#define THUNAR_IS_DEVICE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_DEVICE))
#define THUNAR_IS_DEVICE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((obj), THUNAR_TYPE_DEVICE))
#define THUNAR_DEVICE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_DEVICE, ThunarDeviceClass))

GType                thunar_device_get_type         (void) G_GNUC_CONST;

const gchar         *thunar_device_get_eject_label  (const ThunarDevice   *device);

gchar               *thunar_device_get_name         (const ThunarDevice   *device) G_GNUC_MALLOC;

GIcon               *thunar_device_get_icon         (const ThunarDevice   *device);

ThunarDeviceKind     thunar_device_get_kind         (const ThunarDevice   *device) G_GNUC_PURE;

gchar               *thunar_device_get_identifier   (const ThunarDevice   *device) G_GNUC_MALLOC;

gchar               *thunar_device_get_identifier_unix   (const ThunarDevice   *device) G_GNUC_MALLOC;

gboolean             thunar_device_get_hidden       (const ThunarDevice   *device);

gboolean             thunar_device_can_eject        (const ThunarDevice   *device);

gboolean             thunar_device_can_mount        (const ThunarDevice   *device);

gboolean             thunar_device_can_unmount      (const ThunarDevice   *device);

gboolean             thunar_device_is_mounted       (const ThunarDevice   *device);

GFile               *thunar_device_get_root         (const ThunarDevice   *device);

gint                 thunar_device_compare_by_name  (const ThunarDevice   *device1,
                                                     const ThunarDevice   *device2);

void                 thunar_device_mount            (ThunarDevice         *device,
                                                     GMountOperation      *mount_operation,
                                                     GCancellable         *cancellable,
                                                     ThunarDeviceCallback  callback,
                                                     gpointer              user_data);

void                 thunar_device_unmount          (ThunarDevice         *device,
                                                     GMountOperation      *mount_operation,
                                                     GCancellable         *cancellable,
                                                     ThunarDeviceCallback  callback,
                                                     gpointer              user_data);

void                 thunar_device_eject            (ThunarDevice         *device,
                                                     GMountOperation      *mount_operation,
                                                     GCancellable         *cancellable,
                                                     ThunarDeviceCallback  callback,
                                                     gpointer              user_data);

void                 thunar_device_reload_file      (ThunarDevice         *device);

G_END_DECLS

#endif /* !__THUNAR_DEVICE_H__ */
