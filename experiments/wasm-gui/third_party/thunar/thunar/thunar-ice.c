/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_LIBSM
#include <X11/ICE/ICElib.h>
#endif

#include <thunar/thunar-ice.h>



#ifdef HAVE_LIBSM
static void
thunar_ice_error_handler (IceConn connection)
{
  /*
   * The I/O error handler does whatever is necessary to respond
   * to the I/O error and then returns, but it does not call
   * IceCloseConnection. The ICE connection is given a "bad IO"
   * status, and all future reads and writes to the connection
   * are ignored. The next time IceProcessMessages is called it
   * will return a status of IceProcessMessagesIOError. At that
   * time, the application should call IceCloseConnection.
   */
}



static gboolean
thunar_ice_process_messages (GIOChannel  *channel,
                             GIOCondition condition,
                             gpointer     client_data)
{
  IceProcessMessagesStatus status;
  IceConn                  connection = client_data;

  /* try to process pending messages */
  status = IceProcessMessages (connection, NULL, NULL);
  if (G_UNLIKELY (status == IceProcessMessagesIOError))
    {
      /* we were disconnected from the session manager */
      IceSetShutdownNegotiation (connection, False);
      IceCloseConnection (connection);
    }

  return TRUE;
}



static void
thunar_ice_connection_watch (IceConn     connection,
                             IcePointer  client_data,
                             Bool        opening,
                             IcePointer *watch_data)
{
  GIOChannel *channel;
  guint       watch_id;
  gint        fd;

  if (G_LIKELY (opening))
    {
      /* determine the file descriptor */
      fd = IceConnectionNumber (connection);

      /* Make sure we don't pass on these file descriptors to an exec'd child process */
      fcntl (fd, F_SETFD, fcntl (fd, F_GETFD, 0) | FD_CLOEXEC);

      /* allocate an I/O channel to watch the connection */
      channel = g_io_channel_unix_new (fd);
      watch_id = g_io_add_watch (channel, G_IO_ERR | G_IO_HUP | G_IO_IN, thunar_ice_process_messages, connection);
      g_io_channel_unref (channel);

      /* remember the watch id */
      *watch_data = GUINT_TO_POINTER (watch_id);
    }
  else
    {
      /* remove the watch */
      watch_id = GPOINTER_TO_UINT (*watch_data);
      g_source_remove (watch_id);
    }
}
#endif /* !HAVE_LIBSM */



/**
 * thunar_ice_init:
 *
 * This function should be called before you use any ICE functions.
 * It will arrange for ICE connections to be read and dispatched via
 * the Glib event loop mechanism. This function can be called any number
 * of times without any harm.
 **/
void
thunar_ice_init (void)
{
#ifdef HAVE_LIBSM
  static gboolean initialized = FALSE;

  if (G_LIKELY (!initialized))
    {
      /* setup our custom I/O error handler */
      IceSetIOErrorHandler (thunar_ice_error_handler);

      /* add the connection watch */
      IceAddConnectionWatch (thunar_ice_connection_watch, NULL);

      initialized = TRUE;
    }
#endif /* !HAVE_LIBSM */
}

