/*
 * Copyright (c) 2002      Anders Carlsson <andersca@gnu.org>
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2003-2004 Olivier Fourdan <fourdan@xfce.org>
 * Copyright (c) 2003-2006 Vincent Untz
 * Copyright (c) 2007-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <common/panel-private.h>
#include <common/panel-debug.h>

#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4util/libxfce4util.h>

#include "systray-manager.h"
#include "systray-socket.h"
#include "systray-marshal.h"



#define XFCE_SYSTRAY_MANAGER_REQUEST_DOCK   0
#define XFCE_SYSTRAY_MANAGER_BEGIN_MESSAGE  1
#define XFCE_SYSTRAY_MANAGER_CANCEL_MESSAGE 2

#define XFCE_SYSTRAY_MANAGER_ORIENTATION_HORIZONTAL 0
#define XFCE_SYSTRAY_MANAGER_ORIENTATION_VERTICAL   1



static void            systray_manager_finalize                           (GObject             *object);
static void            systray_manager_remove_socket                      (gpointer             key,
                                                                           gpointer             value,
                                                                           gpointer             user_data);
static GdkFilterReturn systray_manager_window_filter                      (GdkXEvent           *xev,
                                                                           GdkEvent            *event,
                                                                           gpointer             user_data);
static GdkFilterReturn systray_manager_handle_client_message_opcode       (GdkXEvent           *xevent,
                                                                           GdkEvent            *event,
                                                                           gpointer             user_data);
static GdkFilterReturn systray_manager_handle_client_message_message_data (GdkXEvent           *xevent,
                                                                           GdkEvent            *event,
                                                                           gpointer             user_data);
static void            systray_manager_handle_begin_message               (SystrayManager      *manager,
                                                                           XClientMessageEvent *xevent);
static void            systray_manager_handle_cancel_message              (SystrayManager      *manager,
                                                                           XClientMessageEvent *xevent);
static void            systray_manager_handle_dock_request                (SystrayManager      *manager,
                                                                           XClientMessageEvent *xevent);
static gboolean        systray_manager_handle_undock_request              (GtkSocket           *socket,
                                                                           gpointer             user_data);
static void            systray_manager_set_visual                         (SystrayManager      *manager);
static void            systray_manager_set_colors_property                (SystrayManager      *manager);
static void            systray_manager_message_free                       (SystrayMessage      *message);
static void            systray_manager_message_remove_from_list           (SystrayManager      *manager,
                                                                           XClientMessageEvent *xevent);



enum
{
  ICON_ADDED,
  ICON_REMOVED,
  MESSAGE_SENT,
  MESSAGE_CANCELLED,
  LOST_SELECTION,
  LAST_SIGNAL
};

struct _SystrayManagerClass
{
  GObjectClass __parent__;
};

struct _SystrayManager
{
  GObject __parent__;

  /* invisible window */
  GtkWidget      *invisible;

  /* list of client sockets */
  GHashTable     *sockets;

  /* symbolic colors */
  GdkRGBA fg;
  GdkRGBA error;
  GdkRGBA warning;
  GdkRGBA success;

  /* orientation of the tray */
  GtkOrientation  orientation;

  /* list of pending messages */
  GSList         *messages;

  /* _net_system_tray_opcode atom */
  Atom            opcode_atom;

  /* _net_system_tray_message_data atom */
  Atom            data_atom;

  /* _net_system_tray_s%d atom */
  GdkAtom         selection_atom;
};

struct _SystrayMessage
{
  /* message string */
  gchar          *string;

  /* message id */
  glong           id;

  /* x11 window */
  Window          window;

  /* numb3rs */
  glong           length;
  glong           remaining_length;
  glong           timeout;
};



static guint  systray_manager_signals[LAST_SIGNAL];



XFCE_PANEL_DEFINE_TYPE (SystrayManager, systray_manager, G_TYPE_OBJECT)



static void
systray_manager_class_init (SystrayManagerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = systray_manager_finalize;

  systray_manager_signals[ICON_ADDED] =
      g_signal_new (g_intern_static_string ("icon-added"),
                    G_OBJECT_CLASS_TYPE (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__OBJECT,
                    G_TYPE_NONE, 1,
                    GTK_TYPE_SOCKET);

  systray_manager_signals[ICON_REMOVED] =
      g_signal_new (g_intern_static_string ("icon-removed"),
                    G_OBJECT_CLASS_TYPE (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__OBJECT,
                    G_TYPE_NONE, 1,
                    GTK_TYPE_SOCKET);

  systray_manager_signals[MESSAGE_SENT] =
      g_signal_new (g_intern_static_string ("message-sent"),
                    G_OBJECT_CLASS_TYPE (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    _systray_marshal_VOID__OBJECT_STRING_LONG_LONG,
                    G_TYPE_NONE, 4,
                    GTK_TYPE_SOCKET,
                    G_TYPE_STRING,
                    G_TYPE_LONG,
                    G_TYPE_LONG);

  systray_manager_signals[MESSAGE_CANCELLED] =
      g_signal_new (g_intern_static_string ("message-cancelled"),
                    G_OBJECT_CLASS_TYPE (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    _systray_marshal_VOID__OBJECT_LONG,
                    G_TYPE_NONE, 2,
                    GTK_TYPE_SOCKET,
                    G_TYPE_LONG);

  systray_manager_signals[LOST_SELECTION] =
      g_signal_new (g_intern_static_string ("lost-selection"),
                    G_OBJECT_CLASS_TYPE (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);
}



static void
systray_manager_init (SystrayManager *manager)
{
  manager->invisible = NULL;
  manager->orientation = GTK_ORIENTATION_HORIZONTAL;
  manager->messages = NULL;
  manager->sockets = g_hash_table_new (NULL, NULL);

  /* initialize symbolic colors */
  manager->fg.red = 0.0;
  manager->fg.green = 0.0;
  manager->fg.blue = 0.0;

  manager->error.red = 1.0;
  manager->error.green = 0.0;
  manager->error.blue = 0.0;

  manager->warning.red = 1.0;
  manager->warning.green = 1.0;
  manager->warning.blue = 0.0;

  manager->success.red = 0.0;
  manager->success.green = 1.0;
  manager->success.blue = 0.0;
}



GQuark
systray_manager_error_quark (void)
{
  static GQuark q = 0;

  if (q == 0)
    q = g_quark_from_static_string ("systray-manager-error-quark");

  return q;
}



static void
systray_manager_finalize (GObject *object)
{
  SystrayManager *manager = XFCE_SYSTRAY_MANAGER (object);

  panel_return_if_fail (manager->invisible == NULL);

  /* destroy the hash table */
  g_hash_table_destroy (manager->sockets);

  if (manager->messages)
    {
      /* cleanup all pending messages */
      g_slist_foreach (manager->messages,
                       (GFunc) (void (*)(void)) systray_manager_message_free, NULL);

      /* free the list */
      g_slist_free (manager->messages);
    }

  G_OBJECT_CLASS (systray_manager_parent_class)->finalize (object);
}



SystrayManager *
systray_manager_new (void)
{
  return g_object_new (XFCE_TYPE_SYSTRAY_MANAGER, NULL);
}



gboolean
systray_manager_register (SystrayManager  *manager,
                          GdkScreen       *screen,
                          GError         **error)
{
  GdkDisplay          *display;
  gchar               *selection_name;
  gboolean             succeed;
  gint                 screen_number;
  GtkWidget           *invisible;
  guint32              timestamp;
  GdkAtom              opcode_atom;
  GdkAtom              data_atom;
  XClientMessageEvent  xevent;
  Window               root_window;

  panel_return_val_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager), FALSE);
  panel_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  panel_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* create invisible window */
  invisible = gtk_invisible_new_for_screen (screen);
  gtk_widget_realize (invisible);

  /* let the invisible window monitor property and configuration changes */
  gtk_widget_add_events (invisible, GDK_PROPERTY_CHANGE_MASK | GDK_STRUCTURE_MASK);

  /* get the screen number */
  screen_number = panel_screen_get_number (screen);

  /* create the selection atom name */
  selection_name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d", screen_number);

  /* get the selection atom */
  manager->selection_atom = gdk_atom_intern (selection_name, FALSE);

  g_free (selection_name);

  /* get the display */
  display = gdk_screen_get_display (screen);

  /* set the invisible window and take a reference */
  manager->invisible = GTK_WIDGET (g_object_ref (G_OBJECT (invisible)));

  /* set the visual property for transparent tray icons */
  systray_manager_set_visual (manager);

  /* set the property for symbolic color support */
  systray_manager_set_colors_property (manager);

  /* get the current x server time stamp */
  timestamp = gdk_x11_get_server_time (gtk_widget_get_window (GTK_WIDGET (invisible)));

  /* try to become the selection owner of this display */
  succeed = gdk_selection_owner_set_for_display (display,
                                                 gtk_widget_get_window (GTK_WIDGET (invisible)),
                                                 manager->selection_atom,
                                                 timestamp, TRUE);

  if (G_LIKELY (succeed))
    {
      /* get the root window */
      root_window = RootWindowOfScreen (GDK_SCREEN_XSCREEN (screen));

      /* send a message to x11 that we're going to handle this display */
      xevent.type = ClientMessage;
      xevent.window = root_window;
      xevent.message_type = gdk_x11_get_xatom_by_name_for_display (display, "MANAGER");
      xevent.format = 32;
      xevent.data.l[0] = timestamp;
      xevent.data.l[1] = gdk_x11_atom_to_xatom_for_display (display,
                                                            manager->selection_atom);
      xevent.data.l[2] = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (invisible)));
      xevent.data.l[3] = 0;
      xevent.data.l[4] = 0;

      /* send the message */
      XSendEvent (GDK_DISPLAY_XDISPLAY (display), root_window,
                  False, StructureNotifyMask, (XEvent *)&xevent);

      /* system_tray_request_dock, system_tray_begin_message, system_tray_cancel_message and selectionclear */
      gdk_window_add_filter (gtk_widget_get_window (GTK_WIDGET (invisible)),
                             systray_manager_window_filter, manager);

      /* get the opcode atom (for both gdk and x11) */
      opcode_atom = gdk_atom_intern ("_NET_SYSTEM_TRAY_OPCODE", FALSE);
      manager->opcode_atom = gdk_x11_atom_to_xatom_for_display (display, opcode_atom);

      data_atom = gdk_atom_intern ("_NET_SYSTEM_TRAY_MESSAGE_DATA", FALSE);
      manager->data_atom = gdk_x11_atom_to_xatom_for_display (display, data_atom);

      panel_debug (PANEL_DEBUG_SYSTRAY, "registered manager on screen %d", screen_number);
    }
  else
    {
      /* release the invisible */
      g_object_unref (G_OBJECT (manager->invisible));
      manager->invisible = NULL;

      /* desktroy the invisible window */
      gtk_widget_destroy (invisible);

      /* set an error */
      g_set_error (error, XFCE_SYSTRAY_MANAGER_ERROR,
                   XFCE_SYSTRAY_MANAGER_ERROR_SELECTION_FAILED,
                   _("Failed to acquire manager selection for screen %d"),
                   screen_number);
    }

  return succeed;
}



static void
systray_manager_remove_socket (gpointer key,
                               gpointer value,
                               gpointer user_data)
{
  SystrayManager *manager = XFCE_SYSTRAY_MANAGER (user_data);
  GtkSocket      *socket = GTK_SOCKET (value);

  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));
  panel_return_if_fail (GTK_IS_SOCKET (socket));

  /* properly undock from the tray */
  g_signal_emit (manager, systray_manager_signals[ICON_REMOVED], 0, socket);
}



void
systray_manager_unregister (SystrayManager *manager)
{
  GdkDisplay *display;
  GtkWidget  *invisible = manager->invisible;
  GdkWindow  *owner;

  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));

  /* leave when there is no invisible window */
  if (G_UNLIKELY (invisible == NULL))
    return;

  panel_return_if_fail (GTK_IS_INVISIBLE (invisible));
  panel_return_if_fail (gtk_widget_get_realized (invisible));
  panel_return_if_fail (GDK_IS_WINDOW (gtk_widget_get_window (GTK_WIDGET (invisible))));

  /* get the display of the invisible window */
  display = gtk_widget_get_display (invisible);

  /* remove our handling of the selection if we're the owner */
  owner = gdk_selection_owner_get_for_display (display, manager->selection_atom);
  if (owner == gtk_widget_get_window (GTK_WIDGET (invisible)))
    {
      gdk_selection_owner_set_for_display (display, NULL,
                                           manager->selection_atom,
                                           gdk_x11_get_server_time (gtk_widget_get_window (GTK_WIDGET (invisible))),
                                           TRUE);
    }

  /* remove window filter */
  gdk_window_remove_filter (gtk_widget_get_window (GTK_WIDGET (invisible)),
      systray_manager_window_filter, manager);

  /* remove all sockets from the hash table */
  g_hash_table_foreach (manager->sockets,
      systray_manager_remove_socket, manager);

  /* destroy and unref the invisible window */
  manager->invisible = NULL;
  gtk_widget_destroy (invisible);
  g_object_unref (G_OBJECT (invisible));

  panel_debug (PANEL_DEBUG_SYSTRAY, "unregistered manager");
}



static GdkFilterReturn
systray_manager_window_filter (GdkXEvent *xev,
                               GdkEvent  *event,
                               gpointer   user_data)
{
  XEvent         *xevent = (XEvent *)xev;
  SystrayManager *manager = XFCE_SYSTRAY_MANAGER (user_data);

  panel_return_val_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager), GDK_FILTER_CONTINUE);

  if (xevent->type == ClientMessage)
    {
      if (xevent->xclient.message_type == manager->opcode_atom)
        return systray_manager_handle_client_message_opcode (xevent, event, user_data);

      if (xevent->xclient.message_type == manager->data_atom)
        return systray_manager_handle_client_message_message_data (xevent, event, user_data);
    }
  else if (xevent->type == SelectionClear)
    {
      /* emit the signal */
      g_signal_emit (manager, systray_manager_signals[LOST_SELECTION], 0);

      /* unregister the manager */
      systray_manager_unregister (manager);
    }

  return GDK_FILTER_CONTINUE;
}



static GdkFilterReturn
systray_manager_handle_client_message_opcode (GdkXEvent *xevent,
                                              GdkEvent  *event,
                                              gpointer   user_data)
{
  XClientMessageEvent *xev;
  SystrayManager      *manager = XFCE_SYSTRAY_MANAGER (user_data);

  panel_return_val_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager), GDK_FILTER_REMOVE);

  /* cast to x11 event */
  xev = (XClientMessageEvent *) xevent;

  switch (xev->data.l[1])
    {
    case XFCE_SYSTRAY_MANAGER_REQUEST_DOCK:
        systray_manager_handle_dock_request (manager, xev);
        return GDK_FILTER_REMOVE;

    case XFCE_SYSTRAY_MANAGER_BEGIN_MESSAGE:
        systray_manager_handle_begin_message (manager, xev);
        return GDK_FILTER_REMOVE;

    case XFCE_SYSTRAY_MANAGER_CANCEL_MESSAGE:
        systray_manager_handle_cancel_message (manager, xev);
        return GDK_FILTER_REMOVE;

    default:
        break;
    }

  return GDK_FILTER_CONTINUE;
}



static GdkFilterReturn
systray_manager_handle_client_message_message_data (GdkXEvent *xevent,
                                                    GdkEvent  *event,
                                                    gpointer   user_data)
{
  XClientMessageEvent *xev = xevent;
  SystrayManager      *manager = XFCE_SYSTRAY_MANAGER (user_data);
  GSList              *li;
  SystrayMessage      *message;
  glong                length;
  GtkSocket           *socket;

  panel_return_val_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager), GDK_FILTER_REMOVE);

  /* try to find the pending message in the list */
  for (li = manager->messages; li != NULL; li = li->next)
    {
      message = li->data;

      if (xev->window == message->window)
        {
          /* copy the data of this message */
          length = MIN (message->remaining_length, 20);
          memcpy ((message->string + message->length - message->remaining_length), &xev->data, length);
          message->remaining_length -= length;

          /* check if we have the complete message */
          if (message->remaining_length == 0)
            {
              /* try to get the socket from the known tray icons */
              socket = g_hash_table_lookup (manager->sockets, GUINT_TO_POINTER (message->window));

              if (G_LIKELY (socket))
                {
                  /* known socket, send the signal */
                  g_signal_emit (manager, systray_manager_signals[MESSAGE_SENT], 0,
                                 socket, message->string, message->id, message->timeout);
                }

              /* delete the message from the list */
              manager->messages = g_slist_delete_link (manager->messages, li);

              /* free the message */
              systray_manager_message_free (message);
            }

          /* stop searching */
          break;
        }
    }

  return GDK_FILTER_REMOVE;
}



static void
systray_manager_handle_begin_message (SystrayManager      *manager,
                                      XClientMessageEvent *xevent)
{
  GtkSocket      *socket;
  SystrayMessage *message;
  glong           length, timeout, id;

  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));

  /* try to find the window in the list of known tray icons */
  socket = g_hash_table_lookup (manager->sockets, GUINT_TO_POINTER (xevent->window));

  /* unkown tray icon: ignore the message */
  if (G_UNLIKELY (socket == NULL))
    return;

  /* remove the same message from the list */
  systray_manager_message_remove_from_list (manager, xevent);

  /* get some message information */
  timeout = xevent->data.l[2];
  length = xevent->data.l[3];
  id = xevent->data.l[4];

  if (length == 0)
    {
      /* directly emit empty messages */
      g_signal_emit (manager, systray_manager_signals[MESSAGE_SENT], 0,
                     socket, "", id, timeout);
    }
  else
    {
      /* create new structure */
      message = g_slice_new0 (SystrayMessage);

      /* set message data */
      message->window           = xevent->window;
      message->timeout          = timeout;
      message->length           = length;
      message->id               = id;
      message->remaining_length = length;
      message->string           = g_malloc (length + 1);
      message->string[length]   = '\0';

      /* add this message to the list of pending messages */
      manager->messages = g_slist_prepend (manager->messages, message);
    }
}



static void
systray_manager_handle_cancel_message (SystrayManager      *manager,
                                       XClientMessageEvent *xevent)
{
  GtkSocket       *socket;
  Window           window = xevent->data.l[2];

  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));

  /* remove the same message from the list */
  systray_manager_message_remove_from_list (manager, xevent);

  /* try to find the window in the list of known tray icons */
  socket = g_hash_table_lookup (manager->sockets, GUINT_TO_POINTER (xevent->window));

  /* emit the cancelled signal */
  if (G_LIKELY (socket != NULL))
    g_signal_emit (manager, systray_manager_signals[MESSAGE_CANCELLED],
                   0, socket, window);
}



static void
systray_manager_handle_dock_request (SystrayManager      *manager,
                                     XClientMessageEvent *xevent)
{
  GtkWidget       *socket;
  GdkScreen       *screen;
  Window           window = xevent->data.l[2];

  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));
  panel_return_if_fail (GTK_IS_INVISIBLE (manager->invisible));

  /* check if we already have this window */
  if (g_hash_table_lookup (manager->sockets, GUINT_TO_POINTER (window)) != NULL)
    return;

  /* create the socket */
  screen = gtk_widget_get_screen (manager->invisible);
  socket = systray_socket_new (screen, window);
  if (G_UNLIKELY (socket == NULL))
    return;

  /* add the icon to the tray */
  g_signal_emit (manager, systray_manager_signals[ICON_ADDED], 0, socket);

  /* check if the widget has been attached. if the widget has no
     toplevel window, we cannot set the socket id. */
  if (G_LIKELY (GTK_IS_WINDOW (gtk_widget_get_toplevel (socket))))
    {
      /* signal to monitor if the client is removed from the socket */
      g_signal_connect (G_OBJECT (socket), "plug-removed",
          G_CALLBACK (systray_manager_handle_undock_request), manager);

      /* register the xembed client window id for this socket */
      gtk_socket_add_id (GTK_SOCKET (socket), window);

      /* add the socket to the list of known sockets */
      g_hash_table_insert (manager->sockets, GUINT_TO_POINTER (window), socket);
    }
  else
    {
      /* warning */
      g_warning ("No parent window set, destroying socket");

      /* not attached successfully, destroy it */
      gtk_widget_destroy (socket);
    }
}



static gboolean
systray_manager_handle_undock_request (GtkSocket *socket,
                                       gpointer   user_data)
{
  SystrayManager  *manager = XFCE_SYSTRAY_MANAGER (user_data);
  Window          *window;

  panel_return_val_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager), FALSE);

  /* remove the socket from the list */
  window = systray_socket_get_window (XFCE_SYSTRAY_SOCKET (socket));
  g_hash_table_remove (manager->sockets, GUINT_TO_POINTER (*window));

  /* emit signal that the socket will be removed */
  g_signal_emit (manager, systray_manager_signals[ICON_REMOVED], 0, socket);

  /* destroy the socket */
  return FALSE;
}



static void
systray_manager_set_visual (SystrayManager *manager)
{
  GdkDisplay  *display;
  GdkVisual   *visual;
  Visual      *xvisual;
  Atom         visual_atom;
  gulong       data[1];
  GdkScreen   *screen;

  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));
  panel_return_if_fail (GTK_IS_INVISIBLE (manager->invisible));
  panel_return_if_fail (GDK_IS_WINDOW (gtk_widget_get_window (GTK_WIDGET (manager->invisible))));

  /* get invisible display and screen */
  display = gtk_widget_get_display (manager->invisible);
  screen = gtk_invisible_get_screen (GTK_INVISIBLE (manager->invisible));

  /* get the xatom for the visual property */
  visual_atom = gdk_x11_get_xatom_by_name_for_display (display,
      "_NET_SYSTEM_TRAY_VISUAL");

  visual = gdk_screen_get_rgba_visual (screen);
  panel_debug (PANEL_DEBUG_SYSTRAY, "rgba visual is %p", visual);
  if (visual != NULL)
    {
      /* use the rgba visual */
      xvisual = GDK_VISUAL_XVISUAL (visual);
    }
  else
    {
      /* use the default visual for the screen */
      xvisual = GDK_VISUAL_XVISUAL (gdk_screen_get_system_visual (screen));
    }

  data[0] = XVisualIDFromVisual (xvisual);
  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                   GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (manager->invisible))),
                   visual_atom,
                   XA_VISUALID, 32,
                   PropModeReplace,
                   (guchar *) &data, 1);
}



void
systray_manager_set_colors (SystrayManager *manager,
                            GdkRGBA        *fg,
                            GdkRGBA        *error,
                            GdkRGBA        *warning,
                            GdkRGBA        *success)
{
  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));

  manager->fg = *fg;
  manager->error = *error;
  manager->warning = *warning;
  manager->success = *success;

  systray_manager_set_colors_property (manager);
}



static void
systray_manager_set_colors_property (SystrayManager *manager)
{
  GdkWindow  *window;
  GdkDisplay *display;
  Atom        atom;
  gulong      data[12];

  g_return_if_fail (manager->invisible != NULL);
  window = gtk_widget_get_window (manager->invisible);
  g_return_if_fail (window != NULL);

  display = gtk_widget_get_display (manager->invisible);
  atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_SYSTEM_TRAY_COLORS");

  data[0] = manager->fg.red;
  data[1] = manager->fg.green;
  data[2] = manager->fg.blue;
  data[3] = manager->error.red;
  data[4] = manager->error.green;
  data[5] = manager->error.blue;
  data[6] = manager->warning.red;
  data[7] = manager->warning.green;
  data[8] = manager->warning.blue;
  data[9] = manager->success.red;
  data[10] = manager->success.green;
  data[11] = manager->success.blue;

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                   GDK_WINDOW_XID (window),
                   atom,
                   XA_CARDINAL, 32,
                   PropModeReplace,
                   (guchar *) &data, 12);
}



void
systray_manager_set_orientation (SystrayManager *manager,
                                 GtkOrientation  orientation)
{
  GdkDisplay *display;
  Atom        orientation_atom;
  gulong      data[1];

  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));
  panel_return_if_fail (GTK_IS_INVISIBLE (manager->invisible));
  panel_return_if_fail (GDK_IS_WINDOW (gtk_widget_get_window (GTK_WIDGET (manager->invisible))));

  /* set the new orientation */
  manager->orientation = orientation;

  /* get invisible display */
  display = gtk_widget_get_display (manager->invisible);

  /* get the xatom for the orientation property */
  orientation_atom = gdk_x11_get_xatom_by_name_for_display (display,
      "_NET_SYSTEM_TRAY_ORIENTATION");

  /* set the data we're going to send to x */
  data[0] = (manager->orientation == GTK_ORIENTATION_HORIZONTAL ?
             XFCE_SYSTRAY_MANAGER_ORIENTATION_HORIZONTAL
             : XFCE_SYSTRAY_MANAGER_ORIENTATION_VERTICAL);

  /* change the x property */
  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                   GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (manager->invisible))),
                   orientation_atom,
                   XA_CARDINAL, 32,
                   PropModeReplace,
                   (guchar *) &data, 1);
}



/**
 * tray messages
 **/
static void
systray_manager_message_free (SystrayMessage *message)
{
  g_free (message->string);
  g_slice_free (SystrayMessage, message);
}



static void
systray_manager_message_remove_from_list (SystrayManager      *manager,
                                          XClientMessageEvent *xevent)
{
  GSList         *li;
  SystrayMessage *message;

  panel_return_if_fail (XFCE_IS_SYSTRAY_MANAGER (manager));

  /* seach for the same message in the list of pending messages */
  for (li = manager->messages; li != NULL; li = li->next)
    {
      message = li->data;

      /* check if this is the same message */
      if (xevent->window == message->window && xevent->data.l[4] == message->id)
        {
          /* delete the message from the list */
          manager->messages = g_slist_delete_link (manager->messages, li);

          /* free the message */
          systray_manager_message_free (message);

          break;
        }
    }
}
