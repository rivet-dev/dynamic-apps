/*
 *  xfconf
 *
 *  Copyright (c) 2016 Ali Abdallah <ali@xfce.org>
 *  Copyright (c) 2007 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <gio/gio.h>

#include "xfconf-daemon.h"
#include "xfconf-backend-factory.h"

#define DEFAULT_BACKEND  "xfce-perchannel-xml"
#define XFCONF_DBUS_NAME XFCONF_SERVICE_NAME_PREFIX ".Xfconf"
#define XFCONF_DBUS_NAME_TEST XFCONF_SERVICE_NAME_PREFIX ".XfconfTest"

enum
{
    SIGNAL_NONE = 0,
    SIGNAL_RESTART,
    SIGNAL_QUIT,
};

static int signal_pipe[2] = { -1, -1 };

static void
sighandler(int sig)
{
    guint32 sigstate;
    gint avoid_gcc_warning G_GNUC_UNUSED;
    
    switch(sig) {
        case SIGUSR1:
            sigstate = SIGNAL_RESTART;
            break;
        
        default:
            sigstate = SIGNAL_QUIT;
            break;
    }

    avoid_gcc_warning = write(signal_pipe[1], &sigstate, sizeof(sigstate));
}

static gboolean
signal_pipe_io(GIOChannel *source,
               GIOCondition condition,
               gpointer data)
{
    guint32 sigstate = 0;
    gsize bread = 0;
    
    if(G_IO_STATUS_NORMAL == g_io_channel_read_chars(source, (gchar *)&sigstate,
                                                     sizeof(sigstate), &bread,
                                                     NULL)
       && sizeof(sigstate) == bread)
    {
        switch(sigstate)
        {
            case SIGNAL_RESTART:
                /* FIXME: implement */
                break;
            
            case SIGNAL_QUIT:
                g_main_loop_quit((GMainLoop *)data);
                break;
            
            default:
                break;
        }
        
    }
    
    return TRUE;
}

static void
xfconf_dbus_name_lost (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data) {
    GMainLoop *main_loop;

    g_warning("Name %s lost on the message dbus, exiting.", name);
    main_loop = (GMainLoop*)user_data;
    g_main_loop_quit(main_loop);
}


int
main(int argc,
     char **argv)
{
    GMainLoop *mloop;
    XfconfDaemon *xfconfd;
    XfconfLifecycleManager *manager;
    GError *error = NULL;
    struct sigaction act = {0};
    GIOChannel *signal_io;
    const gchar *is_test_mode;
    guint signal_watch = 0;
    
    GOptionContext *opt_ctx;
    gchar **backends = NULL;
    gboolean print_version = FALSE;
    gboolean do_daemon = FALSE;
    GOptionEntry options[] = {
        { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &print_version,
            N_("Prints the xfconfd version."), NULL },
        { "backends", 'b', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING_ARRAY, &backends,
            N_("Configuration backends to use.  The first backend specified " \
               "is opened read/write; the others, read-only."), NULL },
        { "daemon", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &do_daemon,
            N_("Fork into background after starting; only useful for " \
                "testing purposes"), NULL },
        { NULL, 0, 0, 0, 0, NULL, NULL },
    };

    act.sa_handler = sighandler;
    act.sa_flags = SA_RESTART;
    
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGUSR1, &act, NULL);
    
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);
    
    xfce_textdomain(PACKAGE, LOCALEDIR, "UTF-8");
    
    g_set_application_name(_("Xfce Configuration Daemon"));
    g_set_prgname(G_LOG_DOMAIN);
    
    opt_ctx = g_option_context_new(NULL);
    g_option_context_set_translation_domain(opt_ctx, PACKAGE);
    g_option_context_set_summary(opt_ctx, _("Xfce configuration daemon"));
    g_option_context_set_description(opt_ctx,
                                     _("Report bugs to http://bugs.xfce.org/\n"));
    g_option_context_add_main_entries(opt_ctx, options, PACKAGE);
    if(!g_option_context_parse(opt_ctx, &argc, &argv, &error)) {
        g_printerr(_("Error parsing options: %s\n"), error->message);
        g_error_free(error);
        g_option_context_free(opt_ctx);
        return EXIT_FAILURE;
    }
    g_option_context_free(opt_ctx);
    
    if(print_version) {
        g_print("Xfconfd " VERSION "\n");
        return EXIT_SUCCESS;
    }
    
    mloop = g_main_loop_new(NULL, FALSE);
    
    if(pipe(signal_pipe))
        g_warning("Unable to create signal-watch pipe: %s.  Signals will be ignored.", strerror(errno));
    else {
        /* set writing end to non-blocking */
        int oldflags = fcntl(signal_pipe[1], F_GETFL);
        
        if(fcntl(signal_pipe[1], F_SETFL, oldflags | O_NONBLOCK)) {
            g_warning("Unable to set signal-watch pipe to non-blocking mode: %s.  Signals will be ignored.", strerror(errno));
            close(signal_pipe[0]);
            close(signal_pipe[1]);
        } else {
            signal_io = g_io_channel_unix_new(signal_pipe[0]);
            g_io_channel_set_encoding(signal_io, NULL, NULL);
            g_io_channel_set_close_on_unref(signal_io, FALSE);
            signal_watch = g_io_add_watch(signal_io, G_IO_IN | G_IO_PRI,
                                          signal_pipe_io, mloop);
            g_io_channel_unref(signal_io);
        }
    }
    
    if(!backends) {
        backends = g_new0(gchar *, 2);
        backends[0] = g_strdup(DEFAULT_BACKEND);
    }

    manager = xfconf_lifecycle_manager_new ();
    xfconfd = xfconf_daemon_new_unique (backends, manager, &error);
    if(!xfconfd) {
        g_critical("Xfconfd failed to start: %s\n", error->message);
        g_object_unref (manager);
        g_error_free(error);
        return EXIT_FAILURE;
    }
    g_strfreev(backends);

    /* quit the main loop when the xfconf daemon asks us to shutdown */
    g_signal_connect_swapped (manager, "shutdown", G_CALLBACK (g_main_loop_quit), mloop);
    xfconf_lifecycle_manager_start (manager);

    /* acquire name */
    is_test_mode = g_getenv ("XFCONF_RUN_IN_TEST_MODE");
    g_bus_own_name (G_BUS_TYPE_SESSION,
                    is_test_mode == NULL ? XFCONF_DBUS_NAME : XFCONF_DBUS_NAME_TEST,
                    G_BUS_NAME_OWNER_FLAGS_NONE,
                    NULL,
                    NULL,
                    xfconf_dbus_name_lost,
                    mloop, NULL);

    if(do_daemon) {
        pid_t child_pid;

        child_pid = fork();
        if(child_pid < 0) {
            g_printerr("Failed to fork()\n");
            return 1;
        } else if(child_pid > 0) {
            fprintf(stdout, "XFCONFD_PID=%d; export XFCONFD_PID;", child_pid);
            exit(0);
        }

        close(fileno(stdout));
    }
    
    g_main_loop_run(mloop);
    
    g_object_unref (xfconfd);
    g_object_unref (manager);

    xfconf_backend_factory_cleanup();
    
    if(signal_watch) {
        g_source_remove(signal_watch);
        close(signal_pipe[0]);
        close(signal_pipe[1]);
    }
    
    g_main_loop_unref(mloop);
    
    return EXIT_SUCCESS;
}
