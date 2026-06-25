/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * NOTE: THIS FILE WAS COPIED FROM THUNAR. FUNCTION PREFIXES WERE
 * ALIGNED TO XFDESKTOP AND A FEW TRANSLATOR HINTS WERE ADDED.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gio/gio.h>

#include <libnotify/notify.h>

#include <libxfce4util/libxfce4util.h>

#include "xfdesktop-notify.h"



static gboolean xfdesktop_notify_initted = FALSE;



static gboolean
xfdesktop_notify_init (void)
{
  gchar *spec_version = NULL;

  if (!xfdesktop_notify_initted
      && notify_init (PACKAGE_NAME))
    {
      /* we do this to work around bugs in libnotify < 0.6.0. Older
       * versions crash in notify_uninit() when no notifications are
       * displayed before. These versions also segfault when the
       * ret_spec_version parameter of notify_get_server_info is
       * NULL... */
      notify_get_server_info (NULL, NULL, NULL, &spec_version);
      g_free (spec_version);

      xfdesktop_notify_initted = TRUE;
    }

  return xfdesktop_notify_initted;
}



void
xfdesktop_notify_unmount (GMount *mount)
{
  const gchar * const *icon_names;
  NotifyNotification  *notification = NULL;
  const gchar         *summary;
  GFileInfo           *info;
  gboolean             read_only = FALSE;
  GFile               *icon_file;
  GFile               *mount_point;
  GIcon               *icon;
  gchar               *icon_name = NULL;
  gchar               *message;
  gchar               *name;

  g_return_if_fail (G_IS_MOUNT (mount));

  if (!xfdesktop_notify_init ())
    return;

  mount_point = g_mount_get_root (mount);

  info = g_file_query_info (mount_point, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
                            G_FILE_QUERY_INFO_NONE, NULL, NULL);

  if (info != NULL)
    {
      read_only = !g_file_info_get_attribute_boolean (info,
                                                      G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);

      g_object_unref (info);
    }

  g_object_unref (mount_point);

  name = g_mount_get_name (mount);

  icon = g_mount_get_icon (mount);
  if (G_IS_THEMED_ICON (icon))
    {
      icon_names = g_themed_icon_get_names (G_THEMED_ICON (icon));
      if (icon_names != NULL)
        icon_name = g_strdup (icon_names[0]);
    }
  else if (G_IS_FILE_ICON (icon))
    {
      icon_file = g_file_icon_get_file (G_FILE_ICON (icon));
      if (icon_file != NULL)
        {
          icon_name = g_file_get_path (icon_file);
          g_object_unref (icon_file);
        }
    }
  g_object_unref (icon);

  if (icon_name == NULL)
    icon_name = g_strdup ("drive-removable-media");

  if (read_only)
    {
      /* TRANSLATORS: Please use the same translation here as in Thunar */
      summary = _("Unmounting device");

      /* TRANSLATORS: Please use the same translation here as in Thunar */
      message = g_strdup_printf (_("The device \"%s\" is being unmounted by the system. "
                                   "Please do not remove the media or disconnect the "
                                   "drive"), name);
    }
  else
    {
      /* TRANSLATORS: Please use the same translation here as in Thunar */
      summary = _("Writing data to device");

      /* TRANSLATORS: Please use the same translation here as in Thunar */
      message = g_strdup_printf (_("There is data that needs to be written to the "
                                   "device \"%s\" before it can be removed. Please "
                                   "do not remove the media or disconnect the drive"),
                                   name);
    }

#ifdef NOTIFY_CHECK_VERSION
#if NOTIFY_CHECK_VERSION (0, 7, 0)
  notification = notify_notification_new (summary, message, icon_name);
#else
  notification = notify_notification_new (summary, message, icon_name, NULL);
#endif
#else
  notification = notify_notification_new (summary, message, icon_name, NULL);
#endif
  notify_notification_set_urgency (notification, NOTIFY_URGENCY_CRITICAL);
  notify_notification_set_timeout (notification, NOTIFY_EXPIRES_NEVER);
  notify_notification_show (notification, NULL);

  g_object_set_data_full (G_OBJECT (mount), "xfdesktop-notification", notification,
                          g_object_unref);

  g_free (message);
  g_free (icon_name);
  g_free (name);
}



void
xfdesktop_notify_unmount_finish (GMount *mount, gboolean unmount_successful)
{
  const gchar * const *icon_names;
  NotifyNotification  *notification = NULL;
  const gchar         *summary;
  GFile               *icon_file;
  GIcon               *icon;
  gchar               *icon_name = NULL;
  gchar               *message;
  gchar               *name;

  g_return_if_fail (G_IS_MOUNT (mount));

  if (!xfdesktop_notify_init ())
    return;

  name = g_mount_get_name (mount);

  icon = g_mount_get_icon (mount);
  if (G_IS_THEMED_ICON (icon))
    {
      icon_names = g_themed_icon_get_names (G_THEMED_ICON (icon));
      if (icon_names != NULL)
        icon_name = g_strdup (icon_names[0]);
    }
  else if (G_IS_FILE_ICON (icon))
    {
      icon_file = g_file_icon_get_file (G_FILE_ICON (icon));
      if (icon_file != NULL)
        {
          icon_name = g_file_get_path (icon_file);
          g_object_unref (icon_file);
        }
    }
  g_object_unref (icon);

  if (icon_name == NULL)
    icon_name = g_strdup ("drive-removable-media");

  /* close any open notifications since the operation finished */
  notification = g_object_get_data (G_OBJECT (mount), "xfdesktop-notification");
  if (notification != NULL)
    {
      notify_notification_close (notification, NULL);
      g_object_set_data (G_OBJECT (mount), "xfdesktop-notification", NULL);
    }

  /* if the unmount operation was successful then display a message letting
   * the user know it has been removed */
  if (unmount_successful)
    {
      summary = _("Unmount Finished");

      message = g_strdup_printf (_("The device \"%s\" has been safely removed from the system. "), name);

#ifdef NOTIFY_CHECK_VERSION
#if NOTIFY_CHECK_VERSION (0, 7, 0)
      notification = notify_notification_new (summary, message, icon_name);
#else
      notification = notify_notification_new (summary, message, icon_name, NULL);
#endif
#else
      notification = notify_notification_new (summary, message, icon_name, NULL);
#endif
      notify_notification_set_urgency (notification, NOTIFY_URGENCY_NORMAL);
      notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);
      notify_notification_show (notification, NULL);

      g_free (message);
    }

  g_free (icon_name);
  g_free (name);
}



void
xfdesktop_notify_eject (GVolume *volume)
{
  const gchar * const *icon_names;
  NotifyNotification  *notification = NULL;
  const gchar         *summary;
  GFileInfo           *info;
  gboolean             read_only = FALSE;
  GMount              *mount;
  GFile               *icon_file;
  GFile               *mount_point;
  GIcon               *icon;
  gchar               *icon_name = NULL;
  gchar               *message;
  gchar               *name;

  g_return_if_fail (G_IS_VOLUME (volume));

  if (!xfdesktop_notify_init ())
    return;

  mount = g_volume_get_mount (volume);
  if (mount != NULL)
    {
      mount_point = g_mount_get_root (mount);

      info = g_file_query_info (mount_point, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
                                G_FILE_QUERY_INFO_NONE, NULL, NULL);

      if (info != NULL)
        {
          read_only =
            !g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);

          g_object_unref (info);
        }

      g_object_unref (mount_point);
    }

  name = g_volume_get_name (volume);

  icon = g_volume_get_icon (volume);
  if (G_IS_THEMED_ICON (icon))
    {
      icon_names = g_themed_icon_get_names (G_THEMED_ICON (icon));
      if (icon_names != NULL)
        icon_name = g_strdup (icon_names[0]);
    }
  else if (G_IS_FILE_ICON (icon))
    {
      icon_file = g_file_icon_get_file (G_FILE_ICON (icon));
      if (icon_file != NULL)
        {
          icon_name = g_file_get_path (icon_file);
          g_object_unref (icon_file);
        }
    }
  g_object_unref (icon);

  if (icon_name == NULL)
    icon_name = g_strdup ("drive-removable-media");

  if (read_only)
    {
      /* TRANSLATORS: Please use the same translation here as in Thunar */
      summary = _("Ejecting device");

      /* TRANSLATORS: Please use the same translation here as in Thunar */
      message = g_strdup_printf (_("The device \"%s\" is being ejected. "
                                   "This may take some time"), name);
    }
  else
    {
      /* TRANSLATORS: Please use the same translation here as in Thunar */
      summary = _("Writing data to device");

      /* TRANSLATORS: Please use the same translation here as in Thunar */
      message = g_strdup_printf (_("There is data that needs to be written to the "
                                   "device \"%s\" before it can be removed. Please "
                                   "do not remove the media or disconnect the drive"),
                                   name);
    }

#ifdef NOTIFY_CHECK_VERSION
#if NOTIFY_CHECK_VERSION (0, 7, 0)
  notification = notify_notification_new (summary, message, icon_name);
#else
  notification = notify_notification_new (summary, message, icon_name, NULL);
#endif
#else
  notification = notify_notification_new (summary, message, icon_name, NULL);
#endif
  notify_notification_set_urgency (notification, NOTIFY_URGENCY_CRITICAL);
  notify_notification_set_timeout (notification, NOTIFY_EXPIRES_NEVER);
  notify_notification_show (notification, NULL);

  g_object_set_data_full (G_OBJECT (volume), "xfdesktop-notification", notification,
                          g_object_unref);

  g_free (message);
  g_free (icon_name);
  g_free (name);
}



void
xfdesktop_notify_eject_finish (GVolume *volume, gboolean eject_successful)
{
  const gchar * const *icon_names;
  NotifyNotification  *notification = NULL;
  const gchar         *summary;
  GFile               *icon_file;
  GIcon               *icon;
  gchar               *icon_name = NULL;
  gchar               *message;
  gchar               *name;

  g_return_if_fail (G_IS_VOLUME (volume));

  if (!xfdesktop_notify_init ())
    return;

  name = g_volume_get_name (volume);

  icon = g_volume_get_icon (volume);
  if (G_IS_THEMED_ICON (icon))
    {
      icon_names = g_themed_icon_get_names (G_THEMED_ICON (icon));
      if (icon_names != NULL)
        icon_name = g_strdup (icon_names[0]);
    }
  else if (G_IS_FILE_ICON (icon))
    {
      icon_file = g_file_icon_get_file (G_FILE_ICON (icon));
      if (icon_file != NULL)
        {
          icon_name = g_file_get_path (icon_file);
          g_object_unref (icon_file);
        }
    }
  g_object_unref (icon);

  if (icon_name == NULL)
    icon_name = g_strdup ("drive-removable-media");

  /* close any open notifications since the operation finished */
  notification = g_object_get_data (G_OBJECT (volume), "xfdesktop-notification");
  if (notification != NULL)
    {
      notify_notification_close (notification, NULL);
      g_object_set_data (G_OBJECT (volume), "xfdesktop-notification", NULL);
    }

  /* if the eject operation was successful then display a message letting the
   * user know it has been removed */
  if(eject_successful)
    {
      summary = _("Eject Finished");

      message = g_strdup_printf (_("The device \"%s\" has been safely removed from the system. "), name);

#ifdef NOTIFY_CHECK_VERSION
#if NOTIFY_CHECK_VERSION (0, 7, 0)
      notification = notify_notification_new (summary, message, icon_name);
#else
      notification = notify_notification_new (summary, message, icon_name, NULL);
#endif
#else
      notification = notify_notification_new (summary, message, icon_name, NULL);
#endif
      notify_notification_set_urgency (notification, NOTIFY_URGENCY_NORMAL);
      notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);
      notify_notification_show (notification, NULL);

      g_free (message);
    }

  g_free (icon_name);
  g_free (name);
}



void
xfdesktop_notify_uninit (void)
{
  if (xfdesktop_notify_initted
      && notify_is_initted ())
    notify_uninit ();
}
