/*
 * Copyright (c) 2007 Brian Tarricone <bjt23@cornell.edu>
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

/**
 * SECTION: xfce-posix-signal-handler
 * @title: POSIX Signal Handling
 * @short_description: a callback system for handling POSIX signals safely
 *
 * Due to reentrancy issues, there is a restricted set of functions/syscalls
 * that are allowed to be performed inside a POSIX signal handler.  In
 * general, it's safer to defer any signal-related processing until after the
 * signal handler has run.  The functionality in this module automatically
 * handles this, and allows you to set a handler function (with optional user
 * data) for any signal.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <glib.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4util/libxfce4util-alias.h>

#define SIGNAL_PIPE_READ   __signal_pipe[0]
#define SIGNAL_PIPE_WRITE  __signal_pipe[1]

typedef struct
{
    gint signal_id;
    XfcePosixSignalHandler handler;
    gpointer user_data;
    struct sigaction old_sa;
} XfcePosixSignalHandlerData;

static gboolean __inited = FALSE;
static int __signal_pipe[2] = { -1, -1 };
static GHashTable *__handlers = NULL;
static GIOChannel *__signal_io = NULL;
static guint __io_watch_id = 0;


static void
xfce_posix_signal_handler_data_free(XfcePosixSignalHandlerData *hdata)
{
    if(!hdata)
        return;

    sigaction(hdata->signal_id, &hdata->old_sa, NULL);
    g_free(hdata);
}

static gboolean
xfce_posix_signal_handler_pipe_io(GIOChannel *source,
                                  GIOCondition condition,
                                  gpointer data)
{
    gint signal_id = 0;
    GError *error = NULL;
    gsize bin = 0;
    XfcePosixSignalHandlerData *hdata;

    if(G_IO_STATUS_NORMAL == g_io_channel_read_chars(source, (gchar *)&signal_id,
                                                     sizeof(signal_id), &bin,
                                                     &error)
       && bin == sizeof(signal_id))
    {
        hdata = g_hash_table_lookup(__handlers, GINT_TO_POINTER(signal_id));
        if(hdata)
            hdata->handler(signal_id, hdata->user_data);
    } else {
        if(error) {
            g_critical("Signal pipe read failed: %s\n", error->message);
            g_error_free(error);
        } else {
            g_critical("Short read from signal pipe (expected %d, got %d)\n",
                       (int)sizeof(signal_id), (int)bin);
        }
    }

    return TRUE;
}

static void
xfce_posix_signal_handler(gint signal_id)
{
    write(SIGNAL_PIPE_WRITE, &signal_id, sizeof(signal_id));
}


/**
 * xfce_posix_signal_handler_init:
 * @error: (out) (allow-none) (transfer full): Location of a #GError to store any possible errors.
 *
 * Initializes the POSIX signal handler system.  Must be called
 * before setting any POSIX signal handlers.
 *
 * Returns: %TRUE on success, %FALSE on failure, in which case
 *          @error will be set.
 **/
gboolean
xfce_posix_signal_handler_init(GError **error)
{
    if(G_UNLIKELY(__inited))
        return TRUE;

    if(pipe(__signal_pipe)) {
        if(error) {
            g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                        _("pipe() failed: %s"), strerror(errno));
        }
        return FALSE;
    }

    __handlers = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
                                       (GDestroyNotify)xfce_posix_signal_handler_data_free);

    __signal_io = g_io_channel_unix_new(SIGNAL_PIPE_READ);
    g_io_channel_set_close_on_unref(__signal_io, FALSE);
    g_io_channel_set_encoding(__signal_io, NULL, NULL);
    g_io_channel_set_buffered(__signal_io, FALSE);
    __io_watch_id = g_io_add_watch(__signal_io, G_IO_IN,
                                   xfce_posix_signal_handler_pipe_io, NULL);

    __inited = TRUE;
    return TRUE;
}

/**
 * xfce_posix_signal_handler_shutdown:
 *
 * Frees all memory associated with the POSIX signal handling system
 * and restores all default signal handlers.
 **/
void
xfce_posix_signal_handler_shutdown(void)
{
    if(G_UNLIKELY(!__inited))
        return;

    g_source_remove(__io_watch_id);
    __io_watch_id = 0;
    g_io_channel_unref(__signal_io);
    __signal_io = NULL;

    g_hash_table_destroy(__handlers);
    __handlers = NULL;

    close(SIGNAL_PIPE_READ);
    SIGNAL_PIPE_READ = -1;
    close(SIGNAL_PIPE_WRITE);
    SIGNAL_PIPE_WRITE = -1;

    __inited = FALSE;
}

/**
 * xfce_posix_signal_handler_set_handler:
 * @signal: A POSIX signal id number.
 * @handler: (scope call): A callback function.
 * @user_data: Arbitrary data that will be passed to @handler.
 * @error: (out) (allow-none) (transfer full): Location of a #GError to store any possible errors.
 *
 * Sets @handler to be called whenever @signal is caught by the
 * application.  The @user_data parameter will be passed as an argument
 * to @handler.
 *
 * Returns: %TRUE on success, %FALSE otherwise, in which case
 *          @error will be set.
 **/
gboolean
xfce_posix_signal_handler_set_handler(gint signal,
                                      XfcePosixSignalHandler handler,
                                      gpointer user_data,
                                      GError **error)
{
    XfcePosixSignalHandlerData *hdata;
    struct sigaction sa;

    if(G_UNLIKELY(!__inited)) {
        if(error) {
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                        _("xfce_posix_signal_handler_init() must be called first"));
        }
        return FALSE;
    }

    if(!handler) {
        g_warning("NULL signal handler supplied; removing existing handler");
        xfce_posix_signal_handler_restore_handler(signal);
        return TRUE;
    }

    if(g_hash_table_lookup(__handlers, GINT_TO_POINTER(signal)))
        xfce_posix_signal_handler_restore_handler(signal);

    hdata = g_new0(XfcePosixSignalHandlerData, 1);
    hdata->signal_id = signal;
    hdata->handler = handler;
    hdata->user_data = user_data;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = xfce_posix_signal_handler;
    sa.sa_flags = SA_RESTART;

    if(sigaction(signal, &sa, &hdata->old_sa)) {
        if(error) {
            g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                        _("sigaction() failed: %s\n"), strerror(errno));
        }
        g_free(hdata);
        return FALSE;
    }

    g_hash_table_insert(__handlers, GINT_TO_POINTER(signal), hdata);

    return TRUE;
}

/**
 * xfce_posix_signal_handler_restore_handler:
 * @signal: A POSIX signal id number.
 *
 * Restores the default handler for @signal.
 **/
void
xfce_posix_signal_handler_restore_handler(gint signal)
{
    if(G_UNLIKELY(!__inited))
        return;

    g_hash_table_remove(__handlers, GINT_TO_POINTER(signal));
}



#define __XFCE_POSIX_SIGNAL_HANDLER_C__
#include <libxfce4util/libxfce4util-aliasdef.c>
