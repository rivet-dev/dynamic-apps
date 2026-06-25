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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar/thunar-device.h>
#include <thunar/thunar-device-monitor.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-file.h>
#include <thunar/thunar-notify.h>



typedef gboolean   (*AsyncCallbackFinish) (GObject       *object,
                                           GAsyncResult  *result,
                                           GError       **error);



enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_HIDDEN,
  PROP_KIND
};



static void           thunar_device_finalize               (GObject                *object);
static void           thunar_device_get_property           (GObject                 *object,
                                                            guint                    prop_id,
                                                            GValue                  *value,
                                                            GParamSpec              *pspec);
static void           thunar_device_set_property           (GObject                 *object,
                                                            guint                    prop_id,
                                                            const GValue            *value,
                                                            GParamSpec              *pspec);



struct _ThunarDeviceClass
{
  GObjectClass __parent__;
};

struct _ThunarDevice
{
  GObject __parent__;

  /* a GVolume/GMount/GDrive */
  gpointer          device;

  ThunarDeviceKind  kind;

  /* if the device is the list of hidden names */
  guint             hidden : 1;

  /* added time for sorting */
  gint64            stamp;
};

typedef struct
{
  /* the device the operation is working on */
  ThunarDevice         *device;

  /* for user interaction during the [un]mount operation */
  GMountOperation *mount_operation;

  /* the handler ID for the show-umount-progress signal */
  gulong unmount_progress_signal_id;

  /* finish function for the async callback */
  AsyncCallbackFinish   callback_finish;

  /* callback for the user */
  ThunarDeviceCallback  callback;
  gpointer              user_data;
}
ThunarDeviceOperation;



G_DEFINE_TYPE (ThunarDevice, thunar_device, G_TYPE_OBJECT)



static void
thunar_device_class_init (ThunarDeviceClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_device_finalize;
  gobject_class->get_property = thunar_device_get_property;
  gobject_class->set_property = thunar_device_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_DEVICE,
                                   g_param_spec_object ("device",
                                                        "device",
                                                        "device",
                                                        G_TYPE_OBJECT,
                                                        EXO_PARAM_READWRITE
                                                        | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                   PROP_HIDDEN,
                                   g_param_spec_boolean ("hidden",
                                                         "hidden",
                                                         "hidden",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_KIND,
                                   g_param_spec_uint ("kind",
                                                      "kind",
                                                      "kind",
                                                      THUNAR_DEVICE_KIND_VOLUME,
                                                      THUNAR_DEVICE_KIND_MOUNT_REMOTE,
                                                      THUNAR_DEVICE_KIND_VOLUME,
                                                      EXO_PARAM_READWRITE
                                                      | G_PARAM_CONSTRUCT_ONLY));
}



static void
thunar_device_init (ThunarDevice *device)
{
  device->kind = THUNAR_DEVICE_KIND_VOLUME;
  device->stamp = g_get_real_time ();
}



static void
thunar_device_finalize (GObject *object)
{
  ThunarDevice *device = THUNAR_DEVICE (object);

  if (device->device != NULL)
    g_object_unref (G_OBJECT (device->device));

  (*G_OBJECT_CLASS (thunar_device_parent_class)->finalize) (object);
}



static void
thunar_device_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ThunarDevice *device = THUNAR_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, device->device);
      break;

    case PROP_HIDDEN:
      g_value_set_boolean (value, device->hidden);
      break;

    case PROP_KIND:
      g_value_set_uint (value, device->kind);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_device_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  ThunarDevice *device = THUNAR_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      device->device = g_value_dup_object (value);
      _thunar_assert (G_IS_VOLUME (device->device) || G_IS_MOUNT (device->device));
      break;

    case PROP_HIDDEN:
      device->hidden = g_value_get_boolean (value);
      break;

    case PROP_KIND:
      device->kind = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
show_unmount_progress_cb (GMountOperation *mount_operation,
                          const gchar     *message_to_show,
                          gint64           time_left,
                          gint64           bytes_left,
                          ThunarDevice    *device)
{
  if (message_to_show != NULL)
    thunar_notify_progress (device, message_to_show);
}



static ThunarDeviceOperation *
thunar_device_operation_new (ThunarDevice         *device,
                             GMountOperation      *mount_operation,
                             ThunarDeviceCallback  callback,
                             gpointer              user_data,
                             gpointer              callback_finish)
{
  ThunarDeviceOperation *op;

  op = g_slice_new0 (ThunarDeviceOperation);
  op->device = g_object_ref (device);
  op->mount_operation = g_object_ref (mount_operation);
  op->unmount_progress_signal_id = g_signal_connect (mount_operation,
      "show-unmount-progress", G_CALLBACK (show_unmount_progress_cb), device);
  op->callback = callback;
  op->callback_finish = callback_finish;
  op->user_data = user_data;

  return op;
}



static void
thunar_device_operation_finish (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  ThunarDeviceOperation *op = user_data;
  GError                *error = NULL;

  _thunar_return_if_fail (G_IS_OBJECT (object));
  _thunar_return_if_fail (G_IS_ASYNC_RESULT (result));

  /* remove notification */
  thunar_notify_finish (op->device);

  /* finish the operation */
  if (!(op->callback_finish) (object, result, &error))
    {
      /* unset the error if a helper program has already interacted with the user */
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED))
        g_clear_error (&error);

      if (op->callback_finish == (AsyncCallbackFinish) g_volume_mount_finish)
        {
          /* special handling for mount operation */
          if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_ALREADY_MOUNTED)
              || g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PENDING))
            g_clear_error (&error);
        }
    }

  /* user callback */
  (op->callback) (op->device, error, op->user_data);

  /* cleanup */
  g_clear_error (&error);
  g_signal_handler_disconnect (op->mount_operation, op->unmount_progress_signal_id);
  g_object_unref (G_OBJECT (op->mount_operation));
  g_object_unref (G_OBJECT (op->device));
  g_slice_free (ThunarDeviceOperation, op);
}



static void
thunar_device_emit_pre_unmount (ThunarDevice *device,
                                gboolean      all_volumes)
{
  ThunarDeviceMonitor *monitor;
  GFile               *root_file;

  root_file = thunar_device_get_root (device);
  if (root_file != NULL)
    {
      /* make sure the pre-unmount event is emitted, this is important
       * for the interface */
      monitor = thunar_device_monitor_get ();
      g_signal_emit_by_name (monitor, "device-pre-unmount", device, root_file);
      g_object_unref (monitor);
      g_object_unref (root_file);
    }
}



/**
 * thunar_device_get_name:
 * @device     : a #ThunarDevice instance.
 *
 * Returns the name of the @device if the @device
 * is mounted.
 *
 * The caller is responsible to free the returned string
 * using g_free() when no longer needed.
 *
 * Return value: the name of @device if @device is mounted
 **/
gchar *
thunar_device_get_name (const ThunarDevice *device)
{
  GFile *mount_point;
  gchar *display_name = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), NULL);

  if (G_IS_VOLUME (device->device))
    {
      return g_volume_get_name (device->device);
    }
  else if (G_IS_MOUNT (device->device))
    {
      /* probably we can make a nicer name for this mount */
      mount_point = thunar_device_get_root (device);
      if (mount_point != NULL)
        {
          display_name = thunar_g_file_get_display_name_remote (mount_point);
          g_object_unref (mount_point);
        }

      if (display_name == NULL)
        display_name = g_mount_get_name (device->device);

      return display_name;
    }
  else
    _thunar_assert_not_reached ();

  return NULL;
}



GIcon *
thunar_device_get_icon (const ThunarDevice *device)
{
  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), NULL);

  if (G_IS_VOLUME (device->device))
    return g_volume_get_icon (device->device);
  else if (G_IS_MOUNT (device->device))
   return g_mount_get_icon (device->device);
  else
    _thunar_assert_not_reached ();

  return NULL;
}



ThunarDeviceKind
thunar_device_get_kind (const ThunarDevice *device)
{
  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), THUNAR_DEVICE_KIND_VOLUME);
  return device->kind;
}



gchar *
thunar_device_get_identifier (const ThunarDevice *device)
{
  gchar *ident = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), NULL);

  if (G_IS_VOLUME (device->device))
    {
      ident = g_volume_get_uuid (device->device);
      if (ident == NULL)
        ident = g_volume_get_identifier (device->device, G_VOLUME_IDENTIFIER_KIND_UUID);
      if (ident == NULL)
        ident = g_volume_get_name (device->device);
    }
  else if (G_IS_MOUNT (device->device))
    {
      ident = g_mount_get_uuid (device->device);
      if (ident == NULL)
        ident = g_mount_get_name (device->device);
    }
  else
    _thunar_assert_not_reached ();

  return ident;
}



gchar *
thunar_device_get_identifier_unix (const ThunarDevice *device)
{
  gchar *ident = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), NULL);

  if (G_IS_VOLUME (device->device))
    ident = g_volume_get_identifier (device->device, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
  else if (G_IS_MOUNT (device->device))
    return NULL;
  else
    _thunar_assert_not_reached ();

  return ident;
}



gboolean
thunar_device_get_hidden (const ThunarDevice *device)
{
  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), FALSE);
  return device->hidden;
}



/**
 * thunar_device_can_eject:
 *
 * If the user should see the option to eject this device.
 **/
gboolean
thunar_device_can_eject (const ThunarDevice *device)
{
  gboolean  can_eject = FALSE;
  GDrive   *drive;

  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), FALSE);

  if (G_IS_VOLUME (device->device))
    {
      drive = g_volume_get_drive (device->device);
      if (drive != NULL)
        {
          can_eject = g_drive_can_eject (drive) || g_drive_can_stop (drive);
          g_object_unref (drive);
        }
      else
        {
          /* check if the volume can eject */
          can_eject = g_volume_can_eject (device->device);
        }
    }
  else if (G_IS_MOUNT (device->device))
    {
      /* eject or unmount because thats for the user the same
       * because we prefer volumes over mounts as devices */
      can_eject = g_mount_can_eject (device->device) || g_mount_can_unmount (device->device);
    }
  else
    _thunar_assert_not_reached ();

  return can_eject;
}



/**
 * thunar_device_get_eject_label:
 * @device: a #ThunarDevice
 *
 * Obtains the recommended name for the eject action,
 * specific to GIO's drive type.
 *
 * Return value: The recommended name of the eject action. The returned string must not be freed.
 **/
const gchar *
thunar_device_get_eject_label (const ThunarDevice *device)
{
    GDrive              *drive = NULL;
    GDriveStartStopType  start_stop_type = G_DRIVE_START_STOP_TYPE_UNKNOWN;

    if (G_IS_VOLUME (device->device))
      drive = g_volume_get_drive (device->device);

    if (drive != NULL)
      {
        start_stop_type = g_drive_get_start_stop_type (drive);
        g_object_unref (drive);
      }

    switch (start_stop_type)
      {
      case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
        return _("_Safely Remove");
      case G_DRIVE_START_STOP_TYPE_NETWORK:
        return _("_Disconnect");
      case G_DRIVE_START_STOP_TYPE_MULTIDISK:
        return _("_Stop the Multi-Disk Drive");
      case G_DRIVE_START_STOP_TYPE_PASSWORD:
        return _("_Lock");
      case G_DRIVE_START_STOP_TYPE_UNKNOWN:
      default:
        return _("_Eject");
      }
}



/**
 * thunar_device_can_mount:
 *
 * If the user should see the option to mount this device.
 **/
gboolean
thunar_device_can_mount (const ThunarDevice *device)
{
  gboolean  can_mount = FALSE;
  GMount   *volume_mount;

  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), FALSE);

  if (G_IS_VOLUME (device->device))
    {
      /* only volumes without a mountpoint and mount capability */
      volume_mount = g_volume_get_mount (device->device);
      if (volume_mount == NULL)
        can_mount = g_volume_can_mount (device->device);
      else
        g_object_unref (volume_mount);
    }
  else if (G_IS_MOUNT (device->device))
    {
      /* a mount is already mounted... */
      can_mount = FALSE;
    }
  else
    _thunar_assert_not_reached ();

  return can_mount;
}



/**
 * thunar_device_can_unmount:
 *
 * If the user should see the option to unmount this device.
 **/
gboolean
thunar_device_can_unmount (const ThunarDevice *device)
{
  gboolean  can_unmount = FALSE;
  GMount   *volume_mount;

  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), FALSE);

  if (G_IS_VOLUME (device->device))
    {
      /* only volumes with a mountpoint and unmount capability */
      volume_mount = g_volume_get_mount (device->device);
      if (volume_mount != NULL)
        {
          can_unmount = g_mount_can_unmount (volume_mount);
          g_object_unref (volume_mount);
        }
    }
  else if (G_IS_MOUNT (device->device))
    {
      /* check if the mount can unmount */
      can_unmount = g_mount_can_unmount (device->device);
    }
  else
    _thunar_assert_not_reached ();

  return can_unmount;
}



gboolean
thunar_device_is_mounted (const ThunarDevice *device)
{
  gboolean  is_mounted = FALSE;
  GMount   *volume_mount;

  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), FALSE);

  if (G_IS_VOLUME (device->device))
    {
      /* a volume with a mount point is mounted */
      volume_mount = g_volume_get_mount (device->device);
      if (volume_mount != NULL)
        {
          is_mounted = TRUE;
          g_object_unref (volume_mount);
        }
    }
  else if (G_IS_MOUNT (device->device))
    {
      /* mounter are always mounted... */
      is_mounted = TRUE;
    }
  else
    _thunar_assert_not_reached ();

  return is_mounted;
}



GFile *
thunar_device_get_root (const ThunarDevice *device)
{
  GFile  *root = NULL;
  GMount *volume_mount;

  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), NULL);

  if (G_IS_VOLUME (device->device))
    {
      volume_mount = g_volume_get_mount (device->device);
      if (volume_mount != NULL)
        {
          root = g_mount_get_root (volume_mount);
          g_object_unref (volume_mount);
        }

      if (root == NULL)
        root = g_volume_get_activation_root (device->device);
    }
  else if (G_IS_MOUNT (device->device))
    {
      root = g_mount_get_root (device->device);
    }
  else
    _thunar_assert_not_reached ();

  return root;
}



gint
thunar_device_compare_by_name (const ThunarDevice *device1,
                               const ThunarDevice *device2)
{
  gchar* name1;
  gchar* name2;
  gint   result;

  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device1), 0);
  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device2), 0);

  /* sort mounts above volumes */
  if (G_OBJECT_TYPE (device1->device) != G_OBJECT_TYPE (device2->device))
    return G_IS_MOUNT (device1->device) ? 1 : -1;

  name1 = thunar_device_get_name (device1);
  name2 = thunar_device_get_name (device2);

  result = g_strcmp0 (name1, name2);

  g_free (name1);
  g_free (name2);

  return result;
}



void
thunar_device_mount (ThunarDevice         *device,
                     GMountOperation      *mount_operation,
                     GCancellable         *cancellable,
                     ThunarDeviceCallback  callback,
                     gpointer              user_data)
{
  ThunarDeviceOperation *op;

  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (G_IS_MOUNT_OPERATION (mount_operation));
  _thunar_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  _thunar_return_if_fail (callback != NULL);

  if (G_IS_VOLUME (device->device))
    {
      op = thunar_device_operation_new (device, mount_operation, callback, user_data,
                                        g_volume_mount_finish);
      g_volume_mount (G_VOLUME (device->device),
                      G_MOUNT_MOUNT_NONE,
                      mount_operation,
                      cancellable,
                      thunar_device_operation_finish,
                      op);
    }
}



/**
 * thunar_device_unmount:
 *
 * Unmount a #ThunarDevice. Don't try to eject.
 **/
void
thunar_device_unmount (ThunarDevice         *device,
                       GMountOperation      *mount_operation,
                       GCancellable         *cancellable,
                       ThunarDeviceCallback  callback,
                       gpointer              user_data)
{
  ThunarDeviceOperation *op;
  GMount                *mount;

  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (G_IS_MOUNT_OPERATION (mount_operation));
  _thunar_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  _thunar_return_if_fail (callback != NULL);

  /* get the mount from the volume or use existing mount */
  if (G_IS_VOLUME (device->device))
    mount = g_volume_get_mount (device->device);
  else if (G_IS_MOUNT (device->device))
    mount = g_object_ref (device->device);
  else
    mount = NULL;

  if (G_LIKELY (mount != NULL))
    {
      /* only handle mounts that can be unmounted here */
      if (g_mount_can_unmount (mount))
        {
          /* inform user */
          thunar_notify_unmount (device);

          /* try unmounting the mount */
          thunar_device_emit_pre_unmount (device, FALSE);
          op = thunar_device_operation_new (device, mount_operation, callback, user_data,
                                            g_mount_unmount_with_operation_finish);
          g_mount_unmount_with_operation (mount,
                                          G_MOUNT_UNMOUNT_NONE,
                                          mount_operation,
                                          cancellable,
                                          thunar_device_operation_finish,
                                          op);
        }

      g_object_unref (G_OBJECT (mount));
    }
}



/**
 * thunar_device_unmount:
 *
 * Try to eject a #ThunarDevice, fall-back to unmounting
 **/
void
thunar_device_eject (ThunarDevice         *device,
                     GMountOperation      *mount_operation,
                     GCancellable         *cancellable,
                     ThunarDeviceCallback  callback,
                     gpointer              user_data)
{
  ThunarDeviceOperation *op;
  GMount                *mount = NULL;
  GVolume               *volume;
  GDrive                *drive;

  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (G_IS_MOUNT_OPERATION (mount_operation));
  _thunar_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  _thunar_return_if_fail (callback != NULL);

  if (G_IS_VOLUME (device->device))
    {
      volume = device->device;
      drive = g_volume_get_drive (volume);

      if (drive != NULL)
        {
          if (g_drive_can_stop (drive))
            {
              /* inform user */
              thunar_notify_eject (device);

              /* try to stop the drive */
              thunar_device_emit_pre_unmount (device, TRUE);
              op = thunar_device_operation_new (device, mount_operation, callback, user_data,
                                                g_drive_stop_finish);
              g_drive_stop (drive,
                            G_MOUNT_UNMOUNT_NONE,
                            mount_operation,
                            cancellable,
                            thunar_device_operation_finish,
                            op);

              g_object_unref (drive);

              /* done */
              return;
            }
          else if (g_drive_can_eject (drive))
            {
              /* inform user */
              thunar_notify_eject (device);

              /* try to stop the drive */
              thunar_device_emit_pre_unmount (device, TRUE);
              op = thunar_device_operation_new (device, mount_operation, callback, user_data,
                                                g_drive_eject_with_operation_finish);
              g_drive_eject_with_operation (drive,
                                            G_MOUNT_UNMOUNT_NONE,
                                            mount_operation,
                                            cancellable,
                                            thunar_device_operation_finish,
                                            op);

              g_object_unref (drive);

              /* done */
              return;
            }

          g_object_unref (drive);
        }

      if (g_volume_can_eject (volume))
        {
          /* inform user */
          thunar_notify_eject (device);

          /* try ejecting the volume */
          thunar_device_emit_pre_unmount (device, TRUE);
          op = thunar_device_operation_new (device, mount_operation, callback, user_data,
                                            g_volume_eject_with_operation_finish);
          g_volume_eject_with_operation (volume,
                                         G_MOUNT_UNMOUNT_NONE,
                                         mount_operation,
                                         cancellable,
                                         thunar_device_operation_finish,
                                         op);

          /* done */
          return;
        }
      else
        {
          /* get the mount and fall-through */
          mount = g_volume_get_mount (volume);
        }
    }
  else if (G_IS_MOUNT (device->device))
    {
      /* set the mount and fall-through */
      mount = g_object_ref (device->device);
    }

  /* handle mounts */
  if (mount != NULL)
    {
      /* distinguish between ejectable and unmountable mounts */
      if (g_mount_can_eject (mount))
        {
          /* inform user */
          thunar_notify_eject (device);

          /* try ejecting the mount */
          thunar_device_emit_pre_unmount (device, FALSE);
          op = thunar_device_operation_new (device, mount_operation, callback, user_data,
                                            g_mount_eject_with_operation_finish);
          g_mount_eject_with_operation (mount,
                                        G_MOUNT_UNMOUNT_NONE,
                                        mount_operation,
                                        cancellable,
                                        thunar_device_operation_finish,
                                        op);
        }
      else if (g_mount_can_unmount (mount))
        {
          /* inform user */
          thunar_notify_unmount (device);

          /* try unmounting the mount */
          thunar_device_emit_pre_unmount (device, FALSE);
          op = thunar_device_operation_new (device, mount_operation, callback, user_data,
                                            g_mount_unmount_with_operation_finish);
          g_mount_unmount_with_operation (mount,
                                          G_MOUNT_UNMOUNT_NONE,
                                          mount_operation,
                                          cancellable,
                                          thunar_device_operation_finish,
                                          op);
        }

      g_object_unref (G_OBJECT (mount));
    }
}



/**
 * thunar_device_reload_file:
 *
 * Reload the related #ThunarFile of the #ThunarDevice
 **/
void
thunar_device_reload_file (ThunarDevice *device)
{
  ThunarFile *file;
  GFile      *mount_point;

  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));

  mount_point = thunar_device_get_root (device);

  if (mount_point != NULL)
    {
      /* try to determine the file for the mount point */
      file = thunar_file_get (mount_point, NULL);
      if (file != NULL)
        {
          thunar_file_reload (file);
          g_object_unref (file);
        }
      g_object_unref (mount_point);
    }
}
