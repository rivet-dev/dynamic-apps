/*
 * System tray plugin to lxpanel
 *
 * Copyright (C) 2008-2009 Marty Jack <martyj19@comcast.net>
 *               2009 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *               2010-2013 Julien Lavergne <julien.lavergne@gmail.com>
 *               2012-2013 Henry Gebhardt <hsggebhardt@gmail.com>
 *               2014 Andriy Grytsenko <andrej@rep.kiev.ua>
 *
 * This file is a part of LXPanel project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/** Contains code adapted from na-tray-manager.c
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2003-2006 Vincent Untz */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>

#include "plugin.h"
#include "misc.h"
#include "icon-grid.h"

#if GTK_CHECK_VERSION(3, 0, 0)
#include <gtk/gtkx.h>
#endif

/* Standards reference:  http://standards.freedesktop.org/systemtray-spec/ */

/* Protocol constants. */
#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define SYSTEM_TRAY_ORIENTATION_HORZ 0
#define SYSTEM_TRAY_ORIENTATION_VERT 1

struct _balloon_message;
struct _tray_client;
struct _tray_plugin;

/* Representative of a balloon message. */
typedef struct _balloon_message {
    struct _balloon_message * flink;		/* Forward link */
    Window window;				/* X window ID */
    long timeout;				/* Time in milliseconds to display message; 0 if no timeout */
    long length;				/* Message string length */
    long id;					/* Client supplied unique message ID */
    long remaining_length;			/* Remaining length expected of incomplete message */
    char * string;				/* Message string */
} BalloonMessage;

/* Representative of a tray client. */
typedef struct _tray_client {
    struct _tray_client * client_flink;		/* Forward link to next task in X window ID order */
    struct _tray_plugin * tr;			/* Back pointer to tray plugin */
    Window window;				/* X window ID */
    GtkWidget * socket;				/* Socket */
} TrayClient;

/* Private context for system tray plugin. */
typedef struct _tray_plugin {
    GtkWidget * plugin;				/* Back pointer to Plugin */
    LXPanel * panel;
    TrayClient * client_list;			/* List of tray clients */
    BalloonMessage * incomplete_messages;	/* List of balloon messages for which we are awaiting data */
    BalloonMessage * messages;			/* List of balloon messages actively being displayed or waiting to be displayed */
    GtkWidget * balloon_message_popup;		/* Popup showing balloon message */
    guint balloon_message_timer;		/* Timer controlling balloon message */
    GtkWidget * invisible;			/* Invisible window that holds manager selection */
    Window invisible_window;			/* X window ID of invisible window */
    GdkAtom selection_atom;			/* Atom for _NET_SYSTEM_TRAY_S%d */
} TrayPlugin;

static void balloon_message_display(TrayPlugin * tr, BalloonMessage * msg);
static void balloon_incomplete_message_remove(TrayPlugin * tr, Window window, gboolean all_ids, long id);
static void balloon_message_remove(TrayPlugin * tr, Window window, gboolean all_ids, long id);
static void tray_unmanage_selection(TrayPlugin * tr);
static void tray_destructor(gpointer user_data);

/* Look up a client in the client list. */
static TrayClient * client_lookup(TrayPlugin * tr, Window window)
{
    TrayClient * tc;
    for (tc = tr->client_list; tc != NULL; tc = tc->client_flink)
        {
        if (tc->window == window)
	    return tc;
        if (tc->window > window)
            break;
        }
    return NULL;
}

#if 0
static void client_print(TrayPlugin * tr, char c, TrayClient * tc, XClientMessageEvent * xevent)
{
        char *name = get_utf8_property(tc->window, a_NET_WM_NAME);
        int pid = get_net_wm_pid(tc->window);
        XClientMessageEvent xcm = {0};
        if (!xevent)
            xevent = &xcm;
        g_debug("tray: %c%p, winid 0x%lx: %s (PID %d), plug %p, serial no %lu, send_event %c, format %d",
                c, tc, tc->window, name, pid,
                gtk_socket_get_plug_window(GTK_SOCKET(tc->socket)),
                xevent->serial, xevent->send_event ? 'y' : 'n', xevent->format);
        g_free(name);
}
#endif

/* Delete a client. */
static void client_delete(TrayPlugin * tr, TrayClient * tc, gboolean unlink, gboolean remove)
{
    //client_print(tr, '-', tc, NULL);

    if (unlink)
    {
        if (tr->client_list == tc)
            tr->client_list = tc->client_flink;
        else
        {
            /* Locate the task and its predecessor in the list and then remove it.  For safety, ensure it is found. */
            TrayClient * tc_pred = NULL;
            TrayClient * tc_cursor;
            for (
              tc_cursor = tr->client_list;
              ((tc_cursor != NULL) && (tc_cursor != tc));
              tc_pred = tc_cursor, tc_cursor = tc_cursor->client_flink) ;
            if (tc_cursor == tc)
                tc_pred->client_flink = tc->client_flink;
        }
    }

    /* Clear out any balloon messages. */
    balloon_incomplete_message_remove(tr, tc->window, TRUE, 0);
    balloon_message_remove(tr, tc->window, TRUE, 0);

    /* Remove the socket from the icon grid. */
    if (remove)
        gtk_widget_destroy(tc->socket);

    /* Deallocate the client structure. */
    g_free(tc);
}

/*** Balloon message display ***/

/* Free a balloon message structure. */
static void balloon_message_free(BalloonMessage * message)
{
    g_free(message->string);
    g_free(message);
}

/* General code to deactivate a message and optionally display the next.
 * This is used in three scenarios: balloon clicked, timeout expired, destructor. */
static void balloon_message_advance(TrayPlugin * tr, gboolean destroy_timer, gboolean display_next)
{
    /* Remove the message from the queue. */
    BalloonMessage * msg = tr->messages;
    tr->messages = msg->flink;

    /* Cancel the timer, if set.  This is not done when the timer has expired. */
    if ((destroy_timer) && (tr->balloon_message_timer != 0))
        g_source_remove(tr->balloon_message_timer);
    tr->balloon_message_timer = 0;

    /* Destroy the widget. */
    if (tr->balloon_message_popup != NULL)
        gtk_widget_destroy(tr->balloon_message_popup);
    tr->balloon_message_popup = NULL;

    /* Free the message. */
    balloon_message_free(msg);

    /* If there is another message waiting in the queue, display it.  This is not done in the destructor. */
    if ((display_next) && (tr->messages != NULL))
        balloon_message_display(tr, tr->messages);
}

/* Handler for "button-press-event" from balloon message popup menu item. */
static gboolean balloon_message_activate_event(GtkWidget * widget, GdkEventButton * event, TrayPlugin * tr)
{
    balloon_message_advance(tr, TRUE, TRUE);
    return TRUE;
}

/* Timer expiration for balloon message. */
static gboolean balloon_message_timeout(TrayPlugin * tr)
{
    if (!g_source_is_destroyed(g_main_current_source()))
        balloon_message_advance(tr, FALSE, TRUE);
    return FALSE;
}

/* Create the graphic elements to display a balloon message. */
static void balloon_message_display(TrayPlugin * tr, BalloonMessage * msg)
{
    /* Create a window and an item containing the text. */
    tr->balloon_message_popup = gtk_window_new(GTK_WINDOW_POPUP);
    GtkWidget * balloon_text = gtk_label_new(msg->string);
    gtk_label_set_line_wrap(GTK_LABEL(balloon_text), TRUE);
    gtk_misc_set_alignment(GTK_MISC(balloon_text), 0.5, 0.5);
    gtk_container_add(GTK_CONTAINER(tr->balloon_message_popup), balloon_text);
    gtk_widget_show(balloon_text);
    gtk_container_set_border_width(GTK_CONTAINER(tr->balloon_message_popup), 4);

    /* Connect signals.  Clicking the popup dismisses it and displays the next message, if any. */
    gtk_widget_add_events(tr->balloon_message_popup, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(tr->balloon_message_popup, "button-press-event", G_CALLBACK(balloon_message_activate_event), (gpointer) tr);

    /* Compute the desired position in screen coordinates near the tray plugin. */
    int x;
    int y;
    lxpanel_plugin_popup_set_position_helper(tr->panel, tr->plugin, tr->balloon_message_popup, &x, &y);

    /* Show the popup. */
    gtk_window_move(GTK_WINDOW(tr->balloon_message_popup), x, y);
    gtk_widget_show(tr->balloon_message_popup);

    /* Set a timer, if the client specified one.  Both are in units of milliseconds. */
    if (msg->timeout != 0)
        tr->balloon_message_timer = g_timeout_add(msg->timeout, (GSourceFunc) balloon_message_timeout, tr);
}

/* Add a balloon message to the tail of the message queue.  If it is the only element, display it immediately. */
static void balloon_message_queue(TrayPlugin * tr, BalloonMessage * msg)
{
    if (tr->messages == NULL)
    {
        tr->messages = msg;
        balloon_message_display(tr, msg);
    }
    else
    {
        BalloonMessage * msg_pred;
        for (msg_pred = tr->messages; ((msg_pred != NULL) && (msg_pred->flink != NULL)); msg_pred = msg_pred->flink) ;
        if (msg_pred != NULL)
            msg_pred->flink = msg;
    }
}

/* Remove an incomplete message from the queue, selected by window and optionally also client's ID.
 * Used in two scenarios: client issues CANCEL (ID significant), client plug removed (ID don't care). */
static void balloon_incomplete_message_remove(TrayPlugin * tr, Window window, gboolean all_ids, long id)
{
    BalloonMessage * msg_pred = NULL;
    BalloonMessage * msg = tr->incomplete_messages;
    while (msg != NULL)
    {
        /* Establish successor in case of deletion. */
        BalloonMessage * msg_succ = msg->flink;

        if ((msg->window == window) && ((all_ids) || (msg->id == id)))
        {
            /* Found a message matching the criteria.  Unlink and free it. */
            if (msg_pred == NULL)
                tr->incomplete_messages = msg->flink;
            else
                msg_pred->flink = msg->flink;
            balloon_message_free(msg);
        }
        else
            msg_pred = msg;

        /* Advance to successor. */
        msg = msg_succ;
    }
}

/* Remove a message from the message queue, selected by window and optionally also client's ID.
 * Used in two scenarios: client issues CANCEL (ID significant), client plug removed (ID don't care). */
static void balloon_message_remove(TrayPlugin * tr, Window window, gboolean all_ids, long id)
{
    BalloonMessage * msg_pred = NULL;
    BalloonMessage * msg_head = tr->messages;
    BalloonMessage * msg = msg_head;
    while (msg != NULL)
    {
        /* Establish successor in case of deletion. */
        BalloonMessage * msg_succ = msg->flink;

        if ((msg->window == window) && ((all_ids) || (msg->id == id)))
        {
            /* Found a message matching the criteria. */
            if (msg_pred == NULL)
            {
                /* The message is at the queue head, so is being displayed.  Stop the display. */
                tr->messages = msg->flink;
                if (tr->balloon_message_timer != 0)
                {
                    g_source_remove(tr->balloon_message_timer);
                    tr->balloon_message_timer = 0;
                }
                if (tr->balloon_message_popup != NULL)
                {
                    gtk_widget_destroy(tr->balloon_message_popup);
                    tr->balloon_message_popup = NULL;
                }
            }
            else
                msg_pred->flink = msg->flink;

            /* Free the message. */
            balloon_message_free(msg);
        }
        else
            msg_pred = msg;

        /* Advance to successor. */
        msg = msg_succ;
    }

    /* If there is a new message head, display it now. */
    if ((tr->messages != msg_head) && (tr->messages != NULL))
        balloon_message_display(tr, tr->messages);
}

/*** Event interfaces ***/

/* Handle a balloon message SYSTEM_TRAY_BEGIN_MESSAGE event. */
static void balloon_message_begin_event(TrayPlugin * tr, XClientMessageEvent * xevent)
{
    TrayClient * client = client_lookup(tr, xevent->window);
    if (client != NULL)
    {
        /* Check if the message ID already exists. */
        balloon_incomplete_message_remove(tr, xevent->window, FALSE, xevent->data.l[4]);

        /* Allocate a BalloonMessage structure describing the message. */
        BalloonMessage * msg = g_new0(BalloonMessage, 1);
        msg->window = xevent->window;
        msg->timeout = xevent->data.l[2];
        msg->length = xevent->data.l[3];
        msg->id = xevent->data.l[4];
        msg->remaining_length = msg->length;
        msg->string = g_new0(char, msg->length + 1);

        /* Message length of 0 indicates that no follow-on messages will be sent. */
        if (msg->length == 0)
            balloon_message_queue(tr, msg);
        else
        {
            /* Add the new message to the queue to await its message text. */
            msg->flink = tr->incomplete_messages;
            tr->incomplete_messages = msg;
        }
    }
}

/* Handle a balloon message SYSTEM_TRAY_CANCEL_MESSAGE event. */
static void balloon_message_cancel_event(TrayPlugin * tr, XClientMessageEvent * xevent)
{
    /* Remove any incomplete messages on this window with the specified ID. */
    balloon_incomplete_message_remove(tr, xevent->window, TRUE, 0);

    /* Remove any displaying or waiting messages on this window with the specified ID. */
    TrayClient * client = client_lookup(tr, xevent->window);
    if (client != NULL)
        balloon_message_remove(tr, xevent->window, FALSE, xevent->data.l[2]);
}

/* Handle a balloon message _NET_SYSTEM_TRAY_MESSAGE_DATA event. */
static void balloon_message_data_event(TrayPlugin * tr, XClientMessageEvent * xevent)
{
    /* Look up the pending message in the list. */
    BalloonMessage * msg_pred = NULL;
    BalloonMessage * msg;
    for (msg = tr->incomplete_messages; msg != NULL; msg_pred = msg, msg = msg->flink)
    {
        if (xevent->window == msg->window)
        {
            /* Append the message segment to the message. */
            int length = MIN(msg->remaining_length, 20);
            memcpy((msg->string + msg->length - msg->remaining_length), &xevent->data, length);
            msg->remaining_length -= length;

            /* If the message has been completely collected, display it. */
            if (msg->remaining_length == 0)
            {
                /* Unlink the message from the structure. */
                if (msg_pred == NULL)
                    tr->incomplete_messages = msg->flink;
                else
                    msg_pred->flink = msg->flink;

                /* If the client window is valid, queue the message.  Otherwise discard it. */
                TrayClient * client = client_lookup(tr, msg->window);
                if (client != NULL)
                    balloon_message_queue(tr, msg);
                else
                    balloon_message_free(msg);
            }
            break;
        }
    }
}

/* Handler for request dock message. */
static void trayclient_request_dock(TrayPlugin * tr, XClientMessageEvent * xevent)
{
    /* Search for the window in the client list.  Set up context to do an insert right away if needed. */
    TrayClient * tc_pred = NULL;
    TrayClient * tc_cursor;
    for (tc_cursor = tr->client_list; tc_cursor != NULL; tc_pred = tc_cursor, tc_cursor = tc_cursor->client_flink)
    {
        if (tc_cursor->window == (Window)xevent->data.l[2])
            return;		/* We already got this notification earlier, ignore this one. */
        if (tc_cursor->window > (Window)xevent->data.l[2])
            break;
    }

    /* Allocate and initialize new client structure. */
    TrayClient * tc = g_new0(TrayClient, 1);
    tc->window = xevent->data.l[2];
    tc->tr = tr;

    /* Allocate a socket.  This is the tray side of the Xembed connection. */
    tc->socket = gtk_socket_new();

    /* Add the socket to the icon grid. */
    gtk_container_add(GTK_CONTAINER(tr->plugin), tc->socket);
    gtk_widget_show(tc->socket);

    /* Connect the socket to the plug.  This can only be done after the socket is realized. */
    gtk_socket_add_id(GTK_SOCKET(tc->socket), tc->window);

    //fprintf(stderr, "Notice: checking plug %ud\n", tc->window );
    /* Checks if the plug has been created inside of the socket. */
    if (gtk_socket_get_plug_window ( GTK_SOCKET(tc->socket) ) == NULL) {
        //fprintf(stderr, "Notice: removing plug %ud\n", tc->window );
        gtk_widget_destroy(tc->socket);
        g_free(tc);
        return;
    }

    /* Link the client structure into the client list. */
    if (tc_pred == NULL)
    {
        tc->client_flink = tr->client_list;
        tr->client_list = tc;
    }
    else
    {
        tc->client_flink = tc_pred->client_flink;
        tc_pred->client_flink = tc;
    }
}

/* GDK event filter. */
static GdkFilterReturn tray_event_filter(XEvent * xev, GdkEvent * event, TrayPlugin * tr)
{
    if (xev->type == DestroyNotify)
    {
        /* Look for DestroyNotify events on tray icon windows and update state.
         * We do it this way rather than with a "plug_removed" event because delivery
         * of plug_removed events is observed to be unreliable if the client
         * disconnects within less than 10 ms. */
        XDestroyWindowEvent * xev_destroy = (XDestroyWindowEvent *) xev;
        TrayClient * tc = client_lookup(tr, xev_destroy->window);
        if (tc != NULL)
            client_delete(tr, tc, TRUE, TRUE);
    }

    else if (xev->type == ClientMessage)
    {
        if (xev->xclient.message_type == a_NET_SYSTEM_TRAY_OPCODE)
        {
            /* Client message of type _NET_SYSTEM_TRAY_OPCODE.
             * Dispatch on the request. */
            switch (xev->xclient.data.l[1])
            {
                case SYSTEM_TRAY_REQUEST_DOCK:
                    /* If a Request Dock event on the invisible window, which is holding the manager selection, execute it. */
                    if (xev->xclient.window == tr->invisible_window)
                    {
                        trayclient_request_dock(tr, (XClientMessageEvent *) xev);
                        return GDK_FILTER_REMOVE;
                    }
                    break;

                case SYSTEM_TRAY_BEGIN_MESSAGE:
                    /* If a Begin Message event. look up the tray icon and execute it. */
                    balloon_message_begin_event(tr, (XClientMessageEvent *) xev);
                    return GDK_FILTER_REMOVE;

                case SYSTEM_TRAY_CANCEL_MESSAGE:
                    /* If a Cancel Message event. look up the tray icon and execute it. */
                    balloon_message_cancel_event(tr, (XClientMessageEvent *) xev);
                    return GDK_FILTER_REMOVE;
            }
        }

        else if (xev->xclient.message_type == a_NET_SYSTEM_TRAY_MESSAGE_DATA)
        {
            /* Client message of type _NET_SYSTEM_TRAY_MESSAGE_DATA.
             * Look up the tray icon and execute it. */
            balloon_message_data_event(tr, (XClientMessageEvent *) xev);
            return GDK_FILTER_REMOVE;
        }
    }

    else if ((xev->type == SelectionClear)
    && (xev->xclient.window == tr->invisible_window))
    {
        /* Look for SelectionClear events on the invisible window, which is holding the manager selection.
         * This should not happen. */
        tray_unmanage_selection(tr);
    }

    return GDK_FILTER_CONTINUE;
}

/* Delete the selection on the invisible window. */
static void tray_unmanage_selection(TrayPlugin * tr)
{
    if (tr->invisible != NULL)
    {
        GtkWidget * invisible = tr->invisible;
        GdkDisplay * display = gtk_widget_get_display(invisible);
        if (gdk_selection_owner_get_for_display(display, tr->selection_atom) == gtk_widget_get_window(invisible))
        {
            guint32 timestamp = gdk_x11_get_server_time(gtk_widget_get_window(invisible));
            gdk_selection_owner_set_for_display(
                display,
                NULL,
                tr->selection_atom,
                timestamp,
                TRUE);
        }

        /* Destroy the invisible window. */
        tr->invisible = NULL;
        tr->invisible_window = None;
        gtk_widget_destroy(invisible);
        g_object_unref(G_OBJECT(invisible));
    }
}

/* Plugin constructor. */
static GtkWidget *tray_constructor(LXPanel *panel, config_setting_t *settings)
{
    GtkWidget *p;

    /* Get the screen and display. */
    GdkScreen * screen = gtk_widget_get_screen(GTK_WIDGET(panel));
    Screen * xscreen = GDK_SCREEN_XSCREEN(screen);
    GdkDisplay * display = gdk_screen_get_display(screen);

    /* Create the selection atom.  This has the screen number in it, so cannot be done ahead of time. */
    char * selection_atom_name = g_strdup_printf("_NET_SYSTEM_TRAY_S%d", gdk_screen_get_number(screen));
    Atom selection_atom = gdk_x11_get_xatom_by_name_for_display(display, selection_atom_name);
    GdkAtom gdk_selection_atom = gdk_atom_intern(selection_atom_name, FALSE);
    g_free(selection_atom_name);

    /* If the selection is already owned, there is another tray running. */
    if (XGetSelectionOwner(GDK_DISPLAY_XDISPLAY(display), selection_atom) != None)
    {
        g_warning("tray: another systray already running");
        return NULL;
    }

    /* Create an invisible window to hold the selection. */
    GtkWidget * invisible = gtk_invisible_new_for_screen(screen);
    gtk_widget_realize(invisible);
    gtk_widget_add_events(invisible, GDK_PROPERTY_CHANGE_MASK | GDK_STRUCTURE_MASK);

    /* Try to claim the _NET_SYSTEM_TRAY_Sn selection. */
    guint32 timestamp = gdk_x11_get_server_time(gtk_widget_get_window(invisible));
    if (gdk_selection_owner_set_for_display(
        display,
        gtk_widget_get_window(invisible),
        gdk_selection_atom,
        timestamp,
        TRUE))
    {
        /* Send MANAGER client event (ICCCM). */
        XClientMessageEvent xev;
        xev.type = ClientMessage;
        xev.window = RootWindowOfScreen(xscreen);
        xev.message_type = a_MANAGER;
        xev.format = 32;
        xev.data.l[0] = timestamp;
        xev.data.l[1] = selection_atom;
        xev.data.l[2] = GDK_WINDOW_XID(gtk_widget_get_window(invisible));
        xev.data.l[3] = 0;    /* manager specific data */
        xev.data.l[4] = 0;    /* manager specific data */
        XSendEvent(GDK_DISPLAY_XDISPLAY(display), RootWindowOfScreen(xscreen), False, StructureNotifyMask, (XEvent *) &xev);

        /* Set the orientation property.
         * We always set "horizontal" since even vertical panels are designed to use a lot of width. */
        gulong data = SYSTEM_TRAY_ORIENTATION_HORZ;
        XChangeProperty(
            GDK_DISPLAY_XDISPLAY(display),
            GDK_WINDOW_XID(gtk_widget_get_window(invisible)),
            a_NET_SYSTEM_TRAY_ORIENTATION,
            XA_CARDINAL, 32,
            PropModeReplace,
            (guchar *) &data, 1);
    }
    else
    {
        gtk_widget_destroy(invisible);
        g_printerr("tray: System tray didn't get the system tray manager selection\n");
        return 0;
    }

    /* Allocate plugin context and set into Plugin private data pointer and static variable. */
    TrayPlugin * tr = g_new0(TrayPlugin, 1);
    tr->panel = panel;
    tr->selection_atom = gdk_selection_atom;
    /* Add GDK event filter. */
    gdk_window_add_filter(NULL, (GdkFilterFunc) tray_event_filter, tr);
    /* Reference the window since it is never added to a container. */
    tr->invisible = g_object_ref_sink(G_OBJECT(invisible));
    tr->invisible_window = GDK_WINDOW_XID(gtk_widget_get_window(invisible));

    /* Allocate top level widget and set into Plugin widget pointer. */
    tr->plugin = p = panel_icon_grid_new(panel_get_orientation(panel),
                                         panel_get_icon_size(panel),
                                         panel_get_icon_size(panel),
                                         3, 0, panel_get_height(panel));
    lxpanel_plugin_set_data(p, tr, tray_destructor);
    gtk_widget_set_name(p, "tray");
    panel_icon_grid_set_aspect_width(PANEL_ICON_GRID(p), TRUE);

    return p;
}

/* Plugin destructor. */
static void tray_destructor(gpointer user_data)
{
    TrayPlugin * tr = user_data;

    /* Remove GDK event filter. */
    gdk_window_remove_filter(NULL, (GdkFilterFunc) tray_event_filter, tr);

    /* Make sure we drop the manager selection. */
    tray_unmanage_selection(tr);

    /* Deallocate incomplete messages. */
    while (tr->incomplete_messages != NULL)
    {
        BalloonMessage * msg_succ = tr->incomplete_messages->flink;
        balloon_message_free(tr->incomplete_messages);
        tr->incomplete_messages = msg_succ;
    }

    /* Terminate message display and deallocate messages. */
    while (tr->messages != NULL)
        balloon_message_advance(tr, TRUE, FALSE);

    /* Deallocate client list - widgets are already destroyed. */
    while (tr->client_list != NULL)
        client_delete(tr, tr->client_list, TRUE, FALSE);

    g_free(tr);
}

/* Callback when panel configuration changes. */
static void tray_panel_configuration_changed(LXPanel *panel, GtkWidget *p)
{
    /* Set orientation into the icon grid. */
    panel_icon_grid_set_geometry(PANEL_ICON_GRID(p), panel_get_orientation(panel),
                                 panel_get_icon_size(panel),
                                 panel_get_icon_size(panel),
                                 3, 0, panel_get_height(panel));
}

/* Plugin descriptor. */
LXPanelPluginInit lxpanel_static_plugin_tray = {
    .name = N_("System Tray"),
    .description = N_("System tray"),

    /* Set a flag to identify the system tray.  It is special in that only one per system can exist. */
    .one_per_system = TRUE,

    .new_instance = tray_constructor,
    .reconfigure = tray_panel_configuration_changed
};

/* vim: set sw=4 sts=4 et : */
