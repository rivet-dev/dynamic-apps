/*
 * Copyright (c) 2009 Brian Tarricone <brian@terricone.org>
 * Copyright (C) 1999 Olivier Fourdan <fourdan@xfce.org>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

/**
 * SECTION:xfce-sm-client
 * @title: XfceSMClient
 * @short_description: Session management client
 * @stability: Stable
 * @include: libxfce4ui/libxfce4ui.h
 *
 * #XfceSMClient is a session management client that speaks the X Session
 * Management Protocol (XSMP).  It's designed to be easy to use and hide some
 * of the more esoteric feaures of XSMP from the API user.
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_LIBSM
#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>
#endif

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include <libxfce4ui/libxfce4ui-private.h>
#include <libxfce4ui/xfce-sm-client.h>
#include <libxfce4ui/libxfce4ui-marshal.h>
#include <libxfce4ui/libxfce4ui-enum-types.h>
#include <libxfce4ui/libxfce4ui-alias.h>

#define GsmPriority     "_GSM_Priority"
#define GsmDesktopFile  "_GSM_DesktopFile"

#define SM_ID_ARG "--sm-client-id"
#define DPY_ARG "--display"

#define SM_ARG_APPEND 1
#define SM_ARG_REMOVE 2

typedef enum
{
    XFCE_SM_CLIENT_STATE_DISCONNECTED = 0,
    XFCE_SM_CLIENT_STATE_REGISTERING,
    XFCE_SM_CLIENT_STATE_IDLE,
    XFCE_SM_CLIENT_STATE_SAVING_PHASE_1,
    XFCE_SM_CLIENT_STATE_WAITING_FOR_INTERACT,
    XFCE_SM_CLIENT_STATE_INTERACTING,
    XFCE_SM_CLIENT_STATE_WAITING_FOR_PHASE_2,
    XFCE_SM_CLIENT_STATE_SAVING_PHASE_2,
    XFCE_SM_CLIENT_STATE_FROZEN,
} XfceSMClientState;

/**
 * XfceSMClientPriority:
 * @XFCE_SM_CLIENT_PRIORITY_HIGHEST: A high priority value.  You probably
 *                                   don't want to use this.
 * @XFCE_SM_CLIENT_PRIORITY_WM: A priority value for use by the window manager.
 * @XFCE_SM_CLIENT_PRIORITY_CORE: A priority value for use by applications that
 *                                place windows on the screen and possibly set
 *                                window manager struts.
 * @XFCE_SM_CLIENT_PRIORITY_DESKTOP: A priority value for use by applications
 *                                   that draw on the desktop.
 * @XFCE_SM_CLIENT_PRIORITY_DEFAULT: A priority value for regular applications.
 * @XFCE_SM_CLIENT_PRIORITY_LOWEST: The lowest possible priority value.
 *
 * Some sample priority values for use with xfce_sm_client_set_priority().
 **/

/**
 * XfceSMClientRestartStyle:
 * @XFCE_SM_CLIENT_RESTART_NORMAL: Only restart the application if it is
 *                                 still running when the session is next
 *                                 saved.
 * @XFCE_SM_CLIENT_RESTART_IMMEDIATELY: Immediately restart the application
 *                                      if it ever quits.
 *
 * An enumeration describing how the session manager should restart
 * the application.
 **/

/**
 * XfceSMClientShutdownHint:
 * @XFCE_SM_CLIENT_SHUTDOWN_HINT_ASK: Prompt the user for a choice,
 * @XFCE_SM_CLIENT_SHUTDOWN_HINT_LOGOUT: End the current session,
 * @XFCE_SM_CLIENT_SHUTDOWN_HINT_HALT: Shut down the computer.
 * @XFCE_SM_CLIENT_SHUTDOWN_HINT_REBOOT: Restart the computer.
 *
 * Hints to the session manager what kind of shutdown the session manager
 * should perform.
 **/

/**
 * XfceSMClient:
 *
 * An opaque struct with only private fields.
 **/
struct _XfceSMClient
{
    GObject parent;

#ifdef HAVE_LIBSM
    SmcConn session_connection;
    IceConn ice_connection;
#endif

    XfceSMClientState state;
    XfceSMClientRestartStyle restart_style;

    guint8 priority;

    gchar *client_id;

    gchar *current_directory;
    gchar *program;
    gchar **clone_command;
    gchar **restart_command;

    guint32 resumed:1,
            needs_save_state:1,
            shutdown_cancelled:1;

    gint  argc;
    gchar **argv;

    gchar *state_file;
    gchar *desktop_file;
};

typedef struct _XfceSMClientClass
{
    GObjectClass parent;

    /*< signals >*/

    void (*save_state)(XfceSMClient *sm_client);
    void (*save_state_extended)(XfceSMClient *sm_client);

    gboolean (*quit_requested)(XfceSMClient *sm_client);

    void (*quit)(XfceSMClient *sm_client);

    void (*quit_cancelled)(XfceSMClient *sm_client);
} XfceSMClientClass;

typedef struct
{
    gint argc;
    gchar **argv;
    gchar *client_id;
    gboolean sm_disable;
} XfceSMClientStartupOptions;

enum
{
    SIG_SAVE_STATE = 0,
    SIG_SAVE_STATE_EXTENDED,
    SIG_QUIT_REQUESTED,
    SIG_QUIT,
    SIG_QUIT_CANCELLED,
    N_SIGS
};

enum
{
    PROP0 = 0,
    PROP_RESUMED,
    PROP_RESTART_STYLE,
    PROP_PRIORITY,
    PROP_CLIENT_ID,
    PROP_CURRENT_DIRECTORY,
    PROP_RESTART_COMMAND,
    PROP_DESKTOP_FILE,
    PROP_ARGC,
    PROP_ARGV,
};

static void xfce_sm_client_get_property(GObject *obj,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec);
static void xfce_sm_client_set_property(GObject *obj,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec);
static GObject *xfce_sm_client_constructor(GType type,
                                           guint n_construct_params,
                                           GObjectConstructParam *construct_params);
static void xfce_sm_client_finalize(GObject *obj);

static void xfce_sm_client_set_client_id(XfceSMClient *sm_client,
                                         const gchar *client_id);
static void xfce_sm_client_parse_argv(XfceSMClient *sm_client);
#ifdef HAVE_LIBSM
static void xfce_sm_client_set_property_from_command(XfceSMClient *sm_client,
                                                     const char *property_name,
                                                     gchar **command,
                                                     gint alter_sm_id);
#endif
static gchar **copy_command(gchar **command,
                            gchar **value);


static guint signals[N_SIGS] = { 0, };
static XfceSMClientStartupOptions startup_options = { 0, NULL, NULL, FALSE };
static XfceSMClient *sm_client_singleton = NULL;


G_DEFINE_TYPE(XfceSMClient, xfce_sm_client, G_TYPE_OBJECT)


static void
xfce_sm_client_class_init(XfceSMClientClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;

    gobject_class->get_property = xfce_sm_client_get_property;
    gobject_class->set_property = xfce_sm_client_set_property;
    gobject_class->constructor = xfce_sm_client_constructor;
    gobject_class->finalize = xfce_sm_client_finalize;

    /**
     * XfceSMClient::save-state:
     * @sm_client: An #XfceSMClient
     *
     * Signals the client that it should save a copy of its current state
     * such that it could be restarted later in exactly the same state as
     * it is at the time of signal emission.
     *
     * If the state is simple enough to be encoded in the application's
     * command line, xfce_sm_client_set_restart_command() can be used
     * to set that command line.  For more complex state data,
     * xfce_sm_client_get_state_file() should be used.
     *
     * The application should attempt to save its state as quickly as
     * possible, and MUST NOT interact with the user as a part of saving
     * state.
     **/
    signals[SIG_SAVE_STATE] = g_signal_new(I_("save-state"),
                                           G_TYPE_FROM_CLASS(klass),
                                           G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET(XfceSMClientClass,
                                                           save_state),
                                           NULL, NULL,
                                           g_cclosure_marshal_VOID__VOID,
                                           G_TYPE_NONE, 0);

    /**
     * XfceSMClient::save-state-extended:
     * @sm_client: An #XfceSMClient
     *
     * Allows the application to save extra state information after all
     * other applications in the session have had a chance to save their
     * state.  This is mainly used by the window manager to save window
     * positions.  Most applications should not need to connect to this
     * signal.
     **/
    signals[SIG_SAVE_STATE_EXTENDED] = g_signal_new(I_("save-state-extended"),
                                                    G_TYPE_FROM_CLASS(klass),
                                                    G_SIGNAL_RUN_LAST,
                                                    G_STRUCT_OFFSET(XfceSMClientClass,
                                                                    save_state_extended),
                                                    NULL, NULL,
                                                    g_cclosure_marshal_VOID__VOID,
                                                    G_TYPE_NONE, 0);

    /**
     * XfceSMClient::quit-requested:
     * @sm_client: An #XfceSMClient
     *
     * Signals the client that the session manager will soon want the
     * application to quit, perhaps as a part of ending the session
     * (but this should not be assumed).  The application can take
     * this opportunity to prompt the user to save any unsaved work
     * to disk.
     *
     * This signal also expects a return value from the handler.  If the
     * application wishes to cancel the quit request (perhaps because the
     * user selected "Cancel" in prompts to save unsaved work), it should
     * return %TRUE from the handler.  If the application is satisfied
     * with possibly needing to quit soon, the handler should return %FALSE.
     **/
    signals[SIG_QUIT_REQUESTED] = g_signal_new(I_("quit-requested"),
                                               G_TYPE_FROM_CLASS(klass),
                                               G_SIGNAL_RUN_LAST,
                                               G_STRUCT_OFFSET(XfceSMClientClass,
                                                               quit_requested),
                                               g_signal_accumulator_true_handled,
                                               NULL,
                                               _libxfce4ui_marshal_BOOLEAN__VOID,
                                               G_TYPE_BOOLEAN, 0);

    /**
     * XfceSMClient::quit:
     * @sm_client: An #XfceSMClient
     *
     * Emitted when the application is required to quit.  This is not
     * optional: if the client does not quit a short time after receiving
     * this signal, it will likely be terminated in some other way.  While
     * not required, the application will usually receive quit-requested
     * before receiving quit.  If the application does not connect to this
     * signal, #XfceSMClient will call <function>exit(3)</function> with
     * an exit code of zero on behalf of the application.
     **/
    signals[SIG_QUIT] = g_signal_new(I_("quit"),
                                     G_TYPE_FROM_CLASS(klass),
                                     G_SIGNAL_RUN_LAST,
                                     G_STRUCT_OFFSET(XfceSMClientClass,
                                                     quit),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);

    /**
     * XfceSMClient::quit-cancelled:
     * @sm_client: An #XfceSMClient
     *
     * Informs the application that it will not need to quit.  In most cases,
     * quit-cancelled will be emitted a short time after quit-requested.
     **/
    signals[SIG_QUIT_CANCELLED] = g_signal_new(I_("quit-cancelled"),
                                               G_TYPE_FROM_CLASS(klass),
                                               G_SIGNAL_RUN_LAST,
                                               G_STRUCT_OFFSET(XfceSMClientClass,
                                                               quit_cancelled),
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE, 0);

    g_object_class_install_property(gobject_class, PROP_RESUMED,
                                    g_param_spec_boolean("resumed",
                                                         "Resumed",
                                                         "Whether or not the client was resumed with previous state",
                                                         FALSE,
                                                         G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_RESTART_STYLE,
                                    g_param_spec_enum("restart-style",
                                                      "Restart style",
                                                      "Specifies how the client should be restarted by the session manager",
                                                      XFCE_TYPE_SM_CLIENT_RESTART_STYLE,
                                                      XFCE_SM_CLIENT_RESTART_NORMAL,
                                                      G_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_PRIORITY,
                                    g_param_spec_uchar("priority",
                                                       "Priority",
                                                       "Determines the ordering in which this client is restarted",
                                                       0, G_MAXUINT8,
                                                       XFCE_SM_CLIENT_PRIORITY_DEFAULT,
                                                       G_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_CLIENT_ID,
                                    g_param_spec_string("client-id",
                                                        "Client ID",
                                                        "A string uniquely identifying the current instance of this client",
                                                        NULL,
                                                        G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_CURRENT_DIRECTORY,
                                    g_param_spec_string("current-directory",
                                                        "Current working directory",
                                                        "The directory that should be used as the working directory the next time this client is restarted",
                                                        NULL,
                                                        G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_RESTART_COMMAND,
                                    g_param_spec_boxed("restart-command",
                                                       "Restart command",
                                                       "A command used to restart this application, preserving the current state",
                                                       G_TYPE_STRV,
                                                       G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_DESKTOP_FILE,
                                    g_param_spec_string("desktop-file",
                                                        "Desktop file",
                                                        "The application's .desktop file",
                                                        NULL,
                                                        G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS));

    /* these are "private" properties */
    g_object_class_install_property(gobject_class, PROP_ARGC,
                                    g_param_spec_int("argc",
                                                     "argc",
                                                     "Argument count passed to program",
                                                     G_MININT, G_MAXINT, 0,
                                                     G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_ARGV,
                                    g_param_spec_boxed("argv",
                                                       "argv",
                                                       "Argument vector passed to program",
                                                       G_TYPE_STRV,
                                                       G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS));
}

static void
xfce_sm_client_init(XfceSMClient *sm_client)
{
    sm_client->current_directory = g_strdup(xfce_get_homedir());
}

static void
xfce_sm_client_get_property(GObject *obj,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    XfceSMClient *sm_client = XFCE_SM_CLIENT(obj);

    switch(property_id) {
        case PROP_RESUMED:
            g_value_set_boolean(value, sm_client->resumed);
            break;

        case PROP_RESTART_STYLE:
            g_value_set_enum(value, sm_client->restart_style);
            break;

        case PROP_PRIORITY:
            g_value_set_uchar(value, sm_client->priority);
            break;

        case PROP_CLIENT_ID:
            g_value_set_string(value, sm_client->client_id);
            break;

        case PROP_CURRENT_DIRECTORY:
            g_value_set_string(value, sm_client->current_directory);
            break;

        case PROP_RESTART_COMMAND:
            g_value_set_boxed(value, sm_client->restart_command);
            break;

        case PROP_DESKTOP_FILE:
            g_value_set_string(value, sm_client->desktop_file);
            break;

        case PROP_ARGC:
        case PROP_ARGV:
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
    }
}

static void
xfce_sm_client_set_property(GObject *obj,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    XfceSMClient *sm_client = XFCE_SM_CLIENT(obj);

    switch(property_id) {
        case PROP_RESTART_STYLE:
            xfce_sm_client_set_restart_style(sm_client,
                                             g_value_get_enum(value));
            break;

        case PROP_PRIORITY:
            xfce_sm_client_set_priority(sm_client, g_value_get_uchar(value));
            break;

        case PROP_CLIENT_ID:
            xfce_sm_client_set_client_id(sm_client, g_value_get_string(value));
            break;

        case PROP_CURRENT_DIRECTORY:
            xfce_sm_client_set_current_directory(sm_client,
                                                 g_value_get_string(value));
            break;

        case PROP_RESTART_COMMAND:
            xfce_sm_client_set_restart_command(sm_client,
                                               g_value_get_boxed(value));
            break;

        case PROP_DESKTOP_FILE:
            xfce_sm_client_set_desktop_file(sm_client,
                                            g_value_get_string(value));
            break;

        case PROP_ARGC:
            if(sm_client->argc)
                g_critical("XfceSMClient: Received argc twice");
            else {
                sm_client->argc = g_value_get_int(value);
                xfce_sm_client_parse_argv(sm_client);
            }
            break;

        case PROP_ARGV:
            if(sm_client->argv)
                g_critical("XfceSMClient: Received argv twice");
            else {
                sm_client->argv = g_value_dup_boxed(value);
                xfce_sm_client_parse_argv(sm_client);
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
    }
}

static GObject *
xfce_sm_client_constructor(GType type,
                           guint n_construct_params,
                           GObjectConstructParam *construct_params)
{
    GObject *obj;
    const gchar *desktop_autostart_id;

    if(sm_client_singleton)
        return G_OBJECT(sm_client_singleton);

    obj = G_OBJECT_CLASS(xfce_sm_client_parent_class)->constructor(type,
                                                                   n_construct_params,
                                                                   construct_params);

    sm_client_singleton = XFCE_SM_CLIENT(obj);

    desktop_autostart_id = g_getenv("DESKTOP_AUTOSTART_ID");
    if(desktop_autostart_id) {
        XfceSMClient *sm_client = XFCE_SM_CLIENT(obj);

        if(sm_client->client_id
           && strcmp(sm_client->client_id, desktop_autostart_id))
        {
            g_warning("SM client ID specified on command line (%s) is " \
                      "different from ID specified by $DESKTOP_AUTOSTART_ID " \
                      "(%s); using env var", sm_client->client_id,
                      desktop_autostart_id);
        }

        xfce_sm_client_set_client_id(sm_client, desktop_autostart_id);
        g_unsetenv("DESKTOP_AUTOSTART_ID");
    }

    return obj;
}

static void
xfce_sm_client_finalize(GObject *obj)
{
    XfceSMClient *sm_client = XFCE_SM_CLIENT(obj);

    g_assert(sm_client == sm_client_singleton);
    sm_client_singleton = NULL;

    startup_options.argc = 0;
    g_strfreev(startup_options.argv);
    startup_options.argv = NULL;
    g_free(startup_options.client_id);
    startup_options.client_id = NULL;
    startup_options.sm_disable = FALSE;

#ifdef HAVE_LIBSM
    if(sm_client->session_connection)
        xfce_sm_client_disconnect(sm_client);
#endif

    g_free(sm_client->state_file);
    g_free(sm_client->desktop_file);

    g_free(sm_client->client_id);
    g_free(sm_client->current_directory);
    g_strfreev(sm_client->clone_command);
    g_strfreev(sm_client->restart_command);

    g_strfreev(sm_client->argv);

    G_OBJECT_CLASS(xfce_sm_client_parent_class)->finalize(obj);
}



static inline const gchar *
str_from_state(XfceSMClientState state)
{
    switch(state) {
        case XFCE_SM_CLIENT_STATE_DISCONNECTED:
            return "DISCONNECTED";
        case XFCE_SM_CLIENT_STATE_REGISTERING:
            return "REGISTERING";
        case XFCE_SM_CLIENT_STATE_IDLE:
            return "IDLE";
        case XFCE_SM_CLIENT_STATE_SAVING_PHASE_1:
            return "SAVING_PHASE_1";
        case XFCE_SM_CLIENT_STATE_WAITING_FOR_INTERACT:
            return "WAITING_FOR_INTERACT";
        case XFCE_SM_CLIENT_STATE_INTERACTING:
            return "INTERACTING";
        case XFCE_SM_CLIENT_STATE_WAITING_FOR_PHASE_2:
            return "WAITING_FOR_PHASE_2";
        case XFCE_SM_CLIENT_STATE_SAVING_PHASE_2:
            return "SAVING_PHASE_2";
        case XFCE_SM_CLIENT_STATE_FROZEN:
            return "FROZEN";
        default:
            return "(unknown)";
    }
}

static void
xfce_sm_client_set_client_id(XfceSMClient *sm_client,
                             const gchar *client_id)
{
    if(!g_strcmp0(sm_client->client_id, client_id))
        return;

    g_free(sm_client->client_id);
    sm_client->client_id = g_strdup(client_id);

    gdk_x11_set_sm_client_id(sm_client->client_id);

    g_object_notify(G_OBJECT(sm_client), "client-id");
}

static void
xfce_sm_client_set_clone_command(XfceSMClient *sm_client,
                                 gchar **clone_command)
{
    g_return_if_fail(XFCE_IS_SM_CLIENT(sm_client));
    sm_client->clone_command = copy_command(sm_client->clone_command,
                                            clone_command);
#ifdef HAVE_LIBSM
    xfce_sm_client_set_property_from_command(sm_client, SmCloneCommand,
                                             sm_client->clone_command,
                                             SM_ARG_REMOVE);
#endif
}

static void
xfce_sm_client_parse_argv(XfceSMClient *sm_client)
{
    gint argc;
    gchar **argv;
    gchar **clone_command = NULL, **restart_command = NULL;
    gint clone_argc = 0;
    const gchar *client_id = NULL;
    int i;
    gboolean got_display = FALSE;
    gchar *display_name = NULL;

    if(sm_client->argc == 0 || !sm_client->argv)
        return;

    argc = sm_client->argc;
    argv = sm_client->argv;

    /* prefer using Exec= from the .desktop file, if any */
    if(!sm_client->clone_command)
        clone_command = g_new0(gchar *, argc + 1);

    for(i = 0; i < argc; ++i) {
        if(!strncmp(argv[i], SM_ID_ARG, strlen(SM_ID_ARG))) {
            if(argv[i][strlen(SM_ID_ARG)] == '=')
                client_id = &(argv[i][strlen(SM_ID_ARG)+1]);
            else
                client_id = argv[++i];
        } else if(!strncmp(argv[i], DPY_ARG, strlen(DPY_ARG))) {
            got_display = TRUE;
            if(argv[i][strlen(DPY_ARG)] != '=')
                i++;
        } else {
            if(!strcmp(argv[i], "--sm-client-disable"))
                startup_options.sm_disable = TRUE;

            if(clone_command)
                clone_command[clone_argc++] = argv[i];
        }
    }

    if(client_id)
        xfce_sm_client_set_client_id(sm_client, client_id);

    if(!got_display) {
        GdkDisplay *gdpy = gdk_display_get_default();
        GdkScreen *gscreen = gdk_display_get_default_screen(gdpy);

        restart_command = g_new0(gchar *, argc + 3);
        for(i = 0; i < argc; ++i)
            restart_command[i] = argv[i];
        restart_command[i++] = "--display";
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        /* TODO: Move xfwm_make_display_name to libxfce4ui */
        restart_command[i++] = display_name = gdk_screen_make_display_name(gscreen);
G_GNUC_END_IGNORE_DEPRECATIONS
    } else
        restart_command = argv;

    DBG("setting restart and clone commands (%s, %s)",
        restart_command && restart_command[0] ? restart_command[0] : "(null)",
        clone_command && clone_command[0] ? clone_command[0] : "(null)");
    xfce_sm_client_set_restart_command(sm_client, restart_command);
    if(clone_command)
        xfce_sm_client_set_clone_command(sm_client, clone_command);

    if(restart_command != argv)
        g_free(restart_command);
    g_free(display_name);
    g_free(clone_command);

    sm_client->argc = 0;
    g_strfreev(sm_client->argv);
    sm_client->argv = NULL;
}

#ifdef HAVE_LIBSM

static void
xfce_sm_client_set_state(XfceSMClient *sm_client,
                         XfceSMClientState new_state)
{
    XfceSMClientState old_state = sm_client->state;

    if(G_UNLIKELY(old_state == new_state))
        return;

    sm_client->state = new_state;

    DBG("state change: %s -> %s", str_from_state(old_state),
        str_from_state(new_state));
}

static inline char
xsmp_restart_style_from_enum(XfceSMClientRestartStyle style)
{
    switch(style) {
        case XFCE_SM_CLIENT_RESTART_IMMEDIATELY:
            return SmRestartImmediately;
        case XFCE_SM_CLIENT_RESTART_NORMAL:
        default:
            return SmRestartIfRunning;
    }
}


static void xsmp_save_phase_2(SmcConn smc_conn,
                              SmPointer client_data);
static void xsmp_interact(SmcConn smc_conn,
                          SmPointer client_data);
static void xsmp_shutdown_cancelled(SmcConn smc_conn,
                                    SmPointer client_data);
static void xsmp_save_complete(SmcConn smc_conn,
                               SmPointer client_data);
static void xsmp_die(SmcConn smc_conn,
                     SmPointer client_data);
static void xsmp_save_yourself(SmcConn smc_conn,
                               SmPointer client_data,
                               int save_style,
                               Bool shutdown,
                               int interact_style,
                               Bool fast);

static void xfce_sm_client_set_clone_restart_commands(XfceSMClient *sm_client);


static IceIOErrorHandler xsmp_ice_installed_handler = NULL;


/* This is called when data is available on an ICE connection.  */
static gboolean
xsmp_process_ice_messages(GIOChannel *channel,
                          GIOCondition condition,
                          gpointer client_data)
{
    XfceSMClient *sm_client = client_data;
    IceConn connection = sm_client->ice_connection;
    IceProcessMessagesStatus status;

    status = IceProcessMessages(connection, NULL, NULL);
    if(status == IceProcessMessagesIOError) {
        g_warning("Disconnected from session manager.");
        /* We were disconnected */
        IceSetShutdownNegotiation(connection, False);
        if(sm_client->session_connection) {
            xfce_sm_client_disconnect(sm_client);
        } else {
            IceCloseConnection(connection);
        }
    }

    return TRUE;
}

/* This is called when a new ICE connection is made.  It arranges for
   the ICE connection to be handled via the event loop.  */
static void
xsmp_new_ice_connection(IceConn connection,
                        IcePointer client_data,
                        Bool opening,
                        IcePointer *watch_data)
{
    XfceSMClient *sm_client = client_data;
    guint input_id;

    if(opening) {
        /* Make sure we don't pass on these file descriptors to any
         * exec'ed children
         */
        GIOChannel *channel;

        sm_client->ice_connection = connection;
        fcntl(IceConnectionNumber(connection), F_SETFD,
              fcntl(IceConnectionNumber(connection), F_GETFD) | FD_CLOEXEC);

        channel = g_io_channel_unix_new(IceConnectionNumber(connection));

        input_id = g_io_add_watch(channel,
                                  G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI,
                                  xsmp_process_ice_messages, sm_client);

        g_io_channel_unref(channel);

        *watch_data = (IcePointer)GUINT_TO_POINTER(input_id);
    } else {
        input_id = GPOINTER_TO_UINT((gpointer)*watch_data);

        g_source_remove(input_id);
    }
}

static void
xsmp_ice_io_error_handler(IceConn connection)
{
    g_warning("ICE I/O Error");

    if(xsmp_ice_installed_handler)
        xsmp_ice_installed_handler(connection);
}

static void
xsmp_ice_init(XfceSMClient *sm_client)
{
    static gsize inited = 0;

    if(g_once_init_enter(&inited)) {
        IceIOErrorHandler default_handler;

        xsmp_ice_installed_handler = IceSetIOErrorHandler(NULL);
        default_handler = IceSetIOErrorHandler(xsmp_ice_io_error_handler);

        if(xsmp_ice_installed_handler == default_handler)
            xsmp_ice_installed_handler = NULL;

        IceAddConnectionWatch(xsmp_new_ice_connection, sm_client);

        g_once_init_leave(&inited, 1);
    }
}

static void
xfce_sm_client_handle_save_yourself(XfceSMClient *sm_client,
                                    gboolean do_quit_requested,
                                    int dialog_type,
                                    gboolean do_save_state)
{
    if(do_quit_requested
       && (g_signal_has_handler_pending(G_OBJECT(sm_client),
                                        signals[SIG_QUIT_REQUESTED],
                                        0, FALSE)
           || g_signal_has_handler_pending(G_OBJECT(sm_client),
                                           signals[SIG_SAVE_STATE_EXTENDED],
                                           0, FALSE)))
    {
        Status status;

        status = SmcInteractRequest(sm_client->session_connection,
                                    dialog_type, xsmp_interact,
                                    (SmPointer)sm_client);

        if(status) {
            xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_WAITING_FOR_INTERACT);
            sm_client->needs_save_state = do_save_state;
            /* we want save-state to happen *after* quit-requested if we're
             * doing both, but we can't do quit-requested until we hear
             * about our interact request */
            return;
        } else {
            /* if the interact request failed, fall through and at least
             * attempt to save state below if appropriate */
            g_warning("SmcInteractRequest failed!");
        }
    }

    if(do_save_state)
        g_signal_emit(G_OBJECT(sm_client), signals[SIG_SAVE_STATE], 0, NULL);

    if(sm_client->shutdown_cancelled) {
        /* this is a slightly bizarre case that probably won't happen.
         * if we got to this point, then we didn't do quit-requested,
         * but the system was shutting down, and then it was later
         * cancelled.  since we never did a quit-requested, the client
         * probably won't expect a quit-cancelled, so we do nothing here. */
        sm_client->shutdown_cancelled = FALSE;
        xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_IDLE);
    } else {
        /* otherwise, we're just done with the SaveYourself here */
        SmcSaveYourselfDone(sm_client->session_connection, True);
        /* the XSMP spec state diagram says to go right back to IDLE after a
         * non-shutdown SaveYourself, but everything else in the spec disagrees:
         * we need to wait for a SaveComplete before going back to IDLE */
        xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_FROZEN);
    }
}

static void
xsmp_save_phase_2(SmcConn smc_conn,
                  SmPointer client_data)
{
    XfceSMClient *sm_client = XFCE_SM_CLIENT(client_data);

    TRACE("entering");

    /* As a simplification, we won't support interacting in save phase 2,
     * even though XSMP allows an interact request (errors only) from
     * phase 2.  In our SM client's terminolgy, we support save-state
     * but not quit-requested for phase 2. */

    if(sm_client->state != XFCE_SM_CLIENT_STATE_WAITING_FOR_PHASE_2) {
        g_warning("Got SaveYourselfPhase2 in state %s, ignoring",
                  str_from_state(sm_client->state));
        SmcSaveYourselfDone(sm_client->session_connection, True);
        xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_FROZEN);
        return;
    }

    xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_SAVING_PHASE_2);

    g_signal_emit(G_OBJECT(sm_client), signals[SIG_SAVE_STATE_EXTENDED], 0, NULL);

    SmcSaveYourselfDone(sm_client->session_connection, True);
    /* the XSMP spec state diagram says to go right back to IDLE after a
     * non-shutdown SaveYourself, but everything else in the spec disagrees:
     * we need to wait for a SaveComplete before going back to IDLE */
    xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_FROZEN);

    if(sm_client->shutdown_cancelled) {
        /* if we get here, we received ShutdownCancelled while in a recursive
         * invocation of the main loop in save-state-extended.  in this case, we
         * go back to idle and send quit-cancelled. */
        sm_client->shutdown_cancelled = FALSE;

        xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_IDLE);
        g_signal_emit(G_OBJECT(sm_client), signals[SIG_QUIT_CANCELLED], 0,
                      NULL);
    }
}

static void
xsmp_save_yourself(SmcConn smc_conn,
                   SmPointer client_data,
                   int save_style,
                   Bool shutdown,
                   int interact_style,
                   Bool fast)
{
    XfceSMClient *sm_client = XFCE_SM_CLIENT(client_data);
    gboolean do_save_state, do_quit_requested;

    TRACE("entering (save_style=%s, shutdown=%s, interact_style=%s, fast=%s",
          save_style == SmSaveGlobal ? "global" : (save_style == SmSaveLocal ? "local" : "both"),
          shutdown ? "true" : "false",
          interact_style == SmInteractStyleNone ? "none" : (interact_style == SmInteractStyleErrors ? "errors" : "any"),
          fast ? "true" : "false");

    /* The first SaveYourself after registering for the first time
     * is a special case (SM specs 7.2).
     */
    if(sm_client->state == XFCE_SM_CLIENT_STATE_REGISTERING) {
        if(save_style == SmSaveLocal
           && interact_style == SmInteractStyleNone
           && !shutdown
           && !fast)
        {
            xfce_sm_client_set_clone_restart_commands(sm_client);
            SmcSaveYourselfDone(sm_client->session_connection, True);
            /* XSMP spec state diagram says idle, but the rest of the spec disagrees */
            xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_FROZEN);
        } else {
            g_warning("Initial SaveYourself had unexpected parameters");
            xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_IDLE);
        }

        return;
    }

    /* the spec says we can receive a SaveYourself even if we
     * haven't responded with SaveYourselfDone to a previous
     * SaveYourself.  in that case, we're supposed to immediately end
     * the previous SaveYourself and start handling the new one.  that's
     * a bit of a pain, so we're just gonna try to fixup our state on
     * this side and let things go. */
    if(sm_client->state != XFCE_SM_CLIENT_STATE_IDLE
       && sm_client->state != XFCE_SM_CLIENT_STATE_FROZEN)
    {
        SmcSaveYourselfDone(sm_client->session_connection, True);
        xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_FROZEN);
        return;
    }

    xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_SAVING_PHASE_1);

    /* Most of this logic is taken from EggSMClient.  There are many
     * combinations of parameters to the SaveYourself request, but we're
     * going to boil them down to a few possibilities:
     *
     * 1.  Do nothing if:
     *     a) Global save and we're not shutting down, as this is
     *        somewhat pointless.
     *     b) Global save and we're shutting down, but the SM isn't
     *        going to let us interact, because we can't do anything
     *        anyway.
     * 2.  Emit save-state if:
     *     a) Local (or Both) save and we're not shutting down.  We
     *        ignore the Global part of the Both save since prompting
     *        to save files when not shutting down is stupid.
     *     b) Local (or Both) save and we *are* shutting down, but
     *        aren't allowed to interact.  Here a save-state is just
     *        the best we can do.  We ignore the Global part of a
     *        Both save because it's unlikely we can (or should) save
     *        without user interaction.
     * 3.  Emit quit-requested if:
     *     a) Global save, we're shutting down, and we're allowed to
     *        interact.
     * 4.  Emit quit-requested followed by save-state if:
     *     a) Local or Both save, we're shutting down, and we're
     *        allowed to interact.  If it's a Local save, we promote
     *        it to a Both save since shutting down without asking
     *        the user to save their work when it's allowed is rude.
     *
     * We ignore the 'fast' parameter.  I really don't expect any
     * client to do anything differently based on its value.
     */

    do_quit_requested = (shutdown && interact_style != SmInteractStyleNone);
    do_save_state = (save_style != SmSaveGlobal);

    xfce_sm_client_handle_save_yourself(sm_client, do_quit_requested,
                                        (interact_style == SmInteractStyleAny
                                         ? SmDialogNormal : SmDialogError),
                                        do_save_state);
}

static void
xsmp_die(SmcConn smc_conn,
         SmPointer client_data)
{
    XfceSMClient *sm_client = XFCE_SM_CLIENT(client_data);

    TRACE("entering");

    xfce_sm_client_disconnect(sm_client);

    /* here we give the app a chance to gracefully quit.  if the app
     * has indicated it doesn't want to (by not connecting to the quit
     * signal), then we force the exit here. */

    if(g_signal_has_handler_pending(G_OBJECT(sm_client), signals[SIG_QUIT],
                                    0, FALSE))
    {
        g_signal_emit(G_OBJECT(sm_client), signals[SIG_QUIT], 0, NULL);
    } else {
        DBG("XfceSMClient will now call exit(0) which will abort your "
            "application. If you want to handle this yourself, you can "
            "implement the \"quit\"-signal.");

        exit(0);
    }
}

static void
xsmp_save_complete(SmcConn smc_conn,
                   SmPointer client_data)
{
    XfceSMClient *sm_client = XFCE_SM_CLIENT(client_data);

    TRACE("entering");

    if(sm_client->state != XFCE_SM_CLIENT_STATE_FROZEN) {
        g_warning("Got SaveComplete in state %s, ignoring",
                  str_from_state(sm_client->state));
    }

    xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_IDLE);
}

static void
xsmp_shutdown_cancelled(SmcConn smc_conn,
                        SmPointer client_data)
{
    XfceSMClient *sm_client = XFCE_SM_CLIENT(client_data);

    TRACE("entering");

    switch(sm_client->state) {
        case XFCE_SM_CLIENT_STATE_FROZEN:
        case XFCE_SM_CLIENT_STATE_WAITING_FOR_PHASE_2:
            /* if the client has already handled quit-requested, we just go
             * back to idle and inform the client that shutdown was
             * cancelled. */
            xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_IDLE);
            g_signal_emit(G_OBJECT(sm_client), signals[SIG_QUIT_CANCELLED],
                          0, NULL);
            break;

        case XFCE_SM_CLIENT_STATE_WAITING_FOR_INTERACT:
            /* if we're waiting to interact (thus waiting to send
             * quit-requested), we just cancel that, and in this case we finish
             * the SaveYourself and move on.  we don't inform the client of
             * the cancellation since we haven't gotten to inform them about
             * the shutdown in the first place. */
            SmcSaveYourselfDone(sm_client->session_connection, True);
            xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_IDLE);
            break;

        case XFCE_SM_CLIENT_STATE_INTERACTING:
        case XFCE_SM_CLIENT_STATE_SAVING_PHASE_1:
        case XFCE_SM_CLIENT_STATE_SAVING_PHASE_2:
            /* this should only happen if the client is currently *inside* one
             * of the quit-requested, save-state, or save-state-extended
             * handlers and is presumably inside a recursive invocation of the
             * main loop.  we shouldn't inform them of anything just yet, but
             * we'll set a flag so we can do so when they return. */
            sm_client->shutdown_cancelled = TRUE;
            break;

        default:
            g_warning("Got ShutdownCancelled in state %s, ignoring",
                      str_from_state(sm_client->state));
            xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_IDLE);
            break;
    }
}

static void
xsmp_interact(SmcConn smc_conn,
              SmPointer client_data)
{
    XfceSMClient *sm_client = XFCE_SM_CLIENT(client_data);
    gboolean cancel = FALSE;

    TRACE("entering");

    /* since we don't support interacting during phase 2, the only time
     * we should receive an interact message is during a normal save
     * yourself. */

    if(sm_client->state != XFCE_SM_CLIENT_STATE_WAITING_FOR_INTERACT) {
        g_warning("Got Interact message in state %s, ignoring",
                  str_from_state(sm_client->state));
        SmcInteractDone(sm_client->session_connection, False);
        SmcSaveYourselfDone(sm_client->session_connection, True);
        xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_FROZEN);
        return;
    }

    xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_INTERACTING);

    /* at this point we're ready to emit quit-requested */
    g_signal_emit(G_OBJECT(sm_client), signals[SIG_QUIT_REQUESTED], 0,
                  &cancel);

    if(sm_client->shutdown_cancelled) {
        /* if we get here, we received ShutdownCancelled while in a recursive
         * invocation of the main loop in quit-requested.  in this case, we
         * go back to idle and send quit-cancelled. */
        sm_client->shutdown_cancelled = FALSE;
        cancel = TRUE;

        xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_IDLE);
        g_signal_emit(G_OBJECT(sm_client), signals[SIG_QUIT_CANCELLED], 0,
                      NULL);
    } else {
        /* we only send InteractDone if we didn't get ShutdownCancelled */
        xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_FROZEN);
        SmcInteractDone(sm_client->session_connection, cancel);
    }

    if(cancel) {
        /* if we requested (or got) a shutdown cancellation, we skip the
         * save-state signal. */
        sm_client->needs_save_state = FALSE;
    } else if(sm_client->needs_save_state) {
        /* if we still have a pending save-state, send that now */
        sm_client->needs_save_state = FALSE;
        g_signal_emit(G_OBJECT(sm_client), signals[SIG_SAVE_STATE], 0, NULL);

        if(sm_client->shutdown_cancelled) {
            /* this is exceedingly unlikely, but it could happen here too */
            sm_client->shutdown_cancelled = FALSE;
            cancel = TRUE;

            xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_IDLE);
            g_signal_emit(G_OBJECT(sm_client), signals[SIG_QUIT_CANCELLED], 0,
                          NULL);
        }
    }

    /* if the app also wants a phase 2 save, we request it now */
    if(!cancel && g_signal_has_handler_pending(G_OBJECT(sm_client),
                                               signals[SIG_SAVE_STATE_EXTENDED],
                                               0, FALSE))
    {
        Status status;

        status = SmcRequestSaveYourselfPhase2(sm_client->session_connection,
                                              xsmp_save_phase_2,
                                              (SmPointer)sm_client);
        if(status) {
            xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_WAITING_FOR_PHASE_2);
            return;
        }
    }

    /* finally, signal to the SM that we're done.  we also fall through and
     * bail to this if a phase2 request failed for some reason. */
    SmcSaveYourselfDone(sm_client->session_connection, True);
}

static void
xfce_sm_client_set_property_from_command(XfceSMClient *sm_client,
                                         const char *property_name,
                                         gchar **command,
                                         gint alter_sm_id)
{
    SmProp prop, *props[1];
    gint i, argc;
    SmPropValue *vals;

    TRACE("entering (%s, %p, %s)", property_name, command,
          alter_sm_id == SM_ARG_APPEND ? "APPEND" : alter_sm_id == SM_ARG_REMOVE ? "REMOVE" : "FALSE");

    if(!command || !sm_client->session_connection)
        return;

    if(G_UNLIKELY(alter_sm_id == SM_ARG_APPEND && !sm_client->client_id)) {
        g_warning("Trying to add client ID to %s without having a client ID yet",
                  property_name);
        return;
    }

    for(argc = 0; command[argc]; ++argc) {
        if(alter_sm_id == SM_ARG_APPEND
           && !strncmp(command[argc], SM_ID_ARG, strlen(SM_ID_ARG)))
        {
            /* if we have SM_ID_ARG in the command, we still have to
             * make sure the command contains the *correct* client ID */
            gchar *cur_client_id = NULL;

            if(command[argc][strlen(SM_ID_ARG)] == '=')
                cur_client_id = &(command[argc][strlen(SM_ID_ARG)+1]);
            else
                cur_client_id = command[argc+1];

            /* if the client IDs don't match, we need to replace it, and
             * we'll do it in the same form (either 2 arg, or 1 arg with
             * the '=' separator) to avoid shifting the array around */
            if(strcmp(cur_client_id, sm_client->client_id)) {
                if(cur_client_id == command[argc+1]) {
                    g_free(command[argc+1]);
                    command[argc+1] = g_strdup(sm_client->client_id);
                } else {
                    g_free(command[argc]);
                    command[argc] = g_strdup_printf("%s=%s", SM_ID_ARG,
                                                    sm_client->client_id);
                }
            }

            alter_sm_id = FALSE;
        }
    }

    if(alter_sm_id == SM_ARG_APPEND)
        argc += 2;
    vals = g_new(SmPropValue, argc);

    i = 0;
    while(*command) {
        if(alter_sm_id == SM_ARG_REMOVE
           && !strncmp(*command, SM_ID_ARG, strlen(SM_ID_ARG)))
        {
            if((*command)[strlen(SM_ID_ARG)] != '=' && *(command+1)) {
                /* also skip the argument after SM_ID_ARG */
                command++;
            }
        } else {
            vals[i].length = strlen(*command);
            vals[i++].value = *command;
        }
        command++;
    }

    if(alter_sm_id == SM_ARG_APPEND) {
        vals[i].length = strlen(SM_ID_ARG);
        vals[i++].value = SM_ID_ARG;
        vals[i].length = strlen(sm_client->client_id);
        vals[i++].value = sm_client->client_id;
    }

    prop.name = (char *)property_name;
    prop.type = SmLISTofARRAY8;
    prop.vals = vals;
    prop.num_vals = i;

    props[0] = &prop;
    DBG("setting %s", property_name);
    SmcSetProperties(sm_client->session_connection, 1, props);
    g_free(vals);
}

static void
xfce_sm_client_set_clone_restart_commands(XfceSMClient *sm_client)
{
    TRACE("entering");

    /* Restart */
    xfce_sm_client_set_property_from_command(sm_client, SmRestartCommand,
                                             sm_client->restart_command,
                                             SM_ARG_APPEND);

    /* Clone */
    xfce_sm_client_set_property_from_command(sm_client, SmCloneCommand,
                                             (sm_client->clone_command
                                              ? sm_client->clone_command
                                              : sm_client->restart_command),
                                             SM_ARG_REMOVE);

    /* Discard */
    if(sm_client->state_file) {
        gchar *discard_command[4];
        gint i = 0;

        discard_command[i++] = "rm";
        discard_command[i++] = "-rf";
        discard_command[i++] = sm_client->state_file;
        discard_command[i++] = NULL;
        xfce_sm_client_set_property_from_command(sm_client, SmDiscardCommand,
                                                 discard_command, FALSE);
    }
}

#endif



/**
 * xfce_sm_client_error_quark:
 *
 * Gets the XfceSmClient Error Quark.
 *
 * Return value: a #GQuark.
 **/
GQuark
xfce_sm_client_error_quark (void)
{
  static GQuark q;

  if G_UNLIKELY (q == 0)
    q = g_quark_from_static_string ("xfce-sm-client-error-quark");

  return q;
}



/**
 * xfce_sm_client_get_option_group:
 * @argc: The application's argument count
 * @argv: The application's argument vector
 *
 * Constructs a #GOptionGroup suitable for use with Glib's
 * command-line option parser.
 *
 * This function is a bit sneaky in that it will make a copy of
 * the program's argc and argv <emphasis>before</emphasis> GTK+ etc.
 * has a chance to mess around with it, so #XfceSMClient can later
 * construct an accurate restart command.  Instead of calling
 * gtk_init() or gtk_init_with_args(), instead you'd do something
 * like:
 *
 * <informalexample><programlisting>
 * GOptionContext *context = g_option_context_new("");
 * g_option_context_add_group(context, gtk_get_option_group(TRUE));
 * g_option_context_add_group(context, xfce_sm_client_get_option_group(argc, argv);
 * g_option_context_parse(context, &argc, &argv, NULL);
 * </programlisting></informalexample>
 *
 * Error checking is omitted here for brevity, and of course you could
 * add your app's own options with g_option_context_add_main_entries()
 * or similar.
 *
 * Returns: A new #GOptionGroup
 **/
GOptionGroup *
xfce_sm_client_get_option_group(gint  argc,
                                gchar **argv)
{
    const GOptionEntry entries[] = {
        { "sm-client-id", 0, 0, G_OPTION_ARG_STRING, &startup_options.client_id, N_("Session management client ID"), N_("ID") },
        { "sm-client-disable", 0, 0, G_OPTION_ARG_NONE, &startup_options.sm_disable, N_("Disable session management"), NULL },
        { NULL, },
    };
    GOptionGroup *group = NULL;

    startup_options.argc = argc;
    g_strfreev(startup_options.argv);
    if(argv)
        startup_options.argv = g_strdupv(argv);

    group = g_option_group_new("sm-client", _("Session management options"),
                               _("Show session management options"), NULL,
                               NULL);
    g_option_group_add_entries(group, entries);
    g_option_group_set_translation_domain(group, GETTEXT_PACKAGE);

    return group;
}

/**
 * xfce_sm_client_get: (constructor)
 *
 * Gets the application's SM client instance.  This is best
 * used with xfce_sm_client_get_option_group() above (and using
 * the returned #GOptionGroup with g_option_context_parse()), as the
 * command line parsing will figure out many of the SM client's
 * required property values for you.
 *
 * If you have already created an #XfceSMClient instance using
 * this function, this will return the same instance, i.e. you should
 * *only* unref the first instance.
 *
 * Returns: A new #XfceSMClient instance -transfer full- on the first call only,
 * other calls to this function are transfer none
 **/
XfceSMClient *
xfce_sm_client_get(void)
{
    return g_object_new(XFCE_TYPE_SM_CLIENT,
                        "argc", startup_options.argc,
                        "argv", startup_options.argv,
                        "client-id", startup_options.client_id,
                        NULL);
}

/**
 * xfce_sm_client_get_with_argv: (constructor)
 * @argc: The number of arguments passed to main()
 * @argv: The argument vector passed to main()
 * @restart_style: An #XfceSMClientRestartStyle
 * @priority: A restart priority
 *
 * Creates a new #XfceSMClient instance.  It attempts to
 * set all required properties using the app's command line.
 * Note that this function does not actually connect to the session
 * manager, so other actions can be taken (such as setting custom
 * properties or connecting signals) before calling
 * xfce_sm_client_connect().
 *
 * If you are using Gtk or Glib's command-line option parser,
 * it is recommended that you use xfce_sm_client_get_option_group()
 * and xfce_sm_client_get() instead.
 *
 * Returns: (transfer full): A new #XfceSMClient instance
 **/
XfceSMClient *
xfce_sm_client_get_with_argv(gint argc,
                             gchar **argv,
                             XfceSMClientRestartStyle restart_style,
                             guchar priority)
{
    return g_object_new(XFCE_TYPE_SM_CLIENT,
                        "argc", argc,
                        "argv", argv,
                        "restart-style", restart_style,
                        "priority", priority,
                        NULL);
}

/**
 * xfce_sm_client_get_full: (constructor)
 * @restart_style: An XfceSMClientRestartStyle
 * @priority: A restart priority
 * @resumed_client_id: The client id used in the previous session
 * @current_directory: The application's working directory
 * @restart_command: A command that can resume the application's
 *                   saved state
 * @desktop_file: The application's .desktop file
 *
 * Creates a new SM client instance, allowing the application
 * fine-grained control over the initial properties set.
 * Note that this function does not actually connect to the session
 * manager, so other actions can be taken (such as setting custom
 * properties or connecting signals) before calling
 * xfce_sm_client_connect().
 *
 * It is recommended to use xfce_sm_client_get_with_argv(), or,
 * if you are using Gtk or Glib's command-line option parser,
 * xfce_sm_client_get_option_group() and xfce_sm_client_get() instead.
 *
 * Returns: (transfer full): A new #XfceSMClient instance
 **/
XfceSMClient *
xfce_sm_client_get_full(XfceSMClientRestartStyle restart_style,
                        guchar priority,
                        const gchar *resumed_client_id,
                        const gchar *current_directory,
                        const gchar **restart_command,
                        const gchar *desktop_file)
{
    return g_object_new(XFCE_TYPE_SM_CLIENT,
                        "restart-style", restart_style,
                        "priority", priority,
                        "client-id", resumed_client_id,
                        "current-directory", current_directory,
                        "restart-command", restart_command,
                        "desktop-file", desktop_file,
                        NULL);
}

/**
 * xfce_sm_client_connect:
 * @sm_client: An #XfceSMClient
 * @error: (out) (nullable) (transfer full): A #GError location.
 *
 * Attempts to connect to the session manager.
 *
 * Returns: %TRUE on success, %FALSE otherwise.  If an error
 *          occurs, @error will be set.
 **/
gboolean
xfce_sm_client_connect(XfceSMClient *sm_client,
                       GError **error)
{
#ifdef HAVE_LIBSM
    char buf[256] = "";
    unsigned long mask;
    SmcCallbacks callbacks;
    SmProp prop1, prop2, prop3, prop4, prop5, prop6, prop7, *props[7];
    SmPropValue prop1val, prop2val, prop3val, prop4val, prop5val, prop6val, prop7val;
    int n_props = 0;
    char pid[32];
    unsigned char hint = SmRestartIfRunning;
    char *given_client_id = NULL;
#endif

    g_return_val_if_fail(XFCE_IS_SM_CLIENT(sm_client), FALSE);
    g_return_val_if_fail(!error || !*error, FALSE);

    if(startup_options.sm_disable)
        return TRUE;

#ifdef HAVE_LIBSM
    xsmp_ice_init(sm_client);

    mask = SmcSaveYourselfProcMask | SmcDieProcMask | SmcSaveCompleteProcMask
           | SmcShutdownCancelledProcMask;

    callbacks.save_yourself.callback = xsmp_save_yourself;
    callbacks.save_yourself.client_data = (SmPointer)sm_client;

    callbacks.die.callback = xsmp_die;
    callbacks.die.client_data = (SmPointer)sm_client;

    callbacks.save_complete.callback = xsmp_save_complete;
    callbacks.save_complete.client_data = (SmPointer)sm_client;

    callbacks.shutdown_cancelled.callback = xsmp_shutdown_cancelled;
    callbacks.shutdown_cancelled.client_data = (SmPointer)sm_client;

    sm_client->session_connection = SmcOpenConnection(NULL, NULL,
                                                      SmProtoMajor,
                                                      SmProtoMinor,
                                                      mask,
                                                      &callbacks,
                                                      (char *)sm_client->client_id,
                                                      &given_client_id,
                                                      sizeof(buf)-1,
                                                      buf);

    if(!sm_client->session_connection) {
        if(error) {
            g_set_error(error, XFCE_SM_CLIENT_ERROR, XFCE_SM_CLIENT_ERROR_FAILED,
                        _("Failed to connect to the session manager: %s"), buf);
        }
        return FALSE;
    } else if(!given_client_id) {
        if(error) {
            g_set_error(error, XFCE_SM_CLIENT_ERROR, XFCE_SM_CLIENT_ERROR_INVALID_CLIENT,
                        _("Session manager did not return a valid client id"));
        }
        return FALSE;
    }

    if(sm_client->client_id
       && !strcmp(sm_client->client_id, given_client_id))
    {
        xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_IDLE);
        sm_client->resumed = TRUE;
        g_object_notify(G_OBJECT(sm_client), "resumed");
    } else {
        xfce_sm_client_set_client_id(sm_client, given_client_id);
        xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_REGISTERING);
    }

    free(given_client_id);

    hint = xsmp_restart_style_from_enum(sm_client->restart_style);

    prop1.name = SmProgram;
    prop1.type = SmARRAY8;
    prop1.num_vals = 1;
    prop1.vals = &prop1val;
    prop1val.value = (char *)g_get_prgname();
    if(G_UNLIKELY(!prop1val.value))
        prop1val.value = "<unknown program>";
    prop1val.length = strlen(prop1val.value);
    n_props++;

    prop2.name = SmUserID;
    prop2.type = SmARRAY8;
    prop2.num_vals = 1;
    prop2.vals = &prop2val;
    prop2val.value = (char *)g_get_user_name();
    prop2val.length = strlen(prop2val.value);
    n_props++;

    prop3.name = SmRestartStyleHint;
    prop3.type = SmCARD8;
    prop3.num_vals = 1;
    prop3.vals = &prop3val;
    prop3val.value = &hint;
    prop3val.length = 1;
    n_props++;

    g_snprintf(pid, sizeof(pid), "%d", getpid());
    prop4.name = SmProcessID;
    prop4.type = SmARRAY8;
    prop4.num_vals = 1;
    prop4.vals = &prop4val;
    prop4val.value = pid;
    prop4val.length = strlen(prop4val.value);
    n_props++;

    prop5.name = SmCurrentDirectory;
    prop5.type = SmARRAY8;
    prop5.num_vals = 1;
    prop5.vals = &prop5val;
    prop5val.value = (char *)sm_client->current_directory;
    prop5val.length = strlen(prop5val.value);
    n_props++;

    prop6.name = GsmPriority;
    prop6.type = SmCARD8;
    prop6.num_vals = 1;
    prop6.vals = &prop6val;
    prop6val.value = &sm_client->priority;
    prop6val.length = 1;
    n_props++;

    if(sm_client->desktop_file) {
        prop7.name = GsmDesktopFile;
        prop7.type = SmARRAY8;
        prop7.num_vals = 1;
        prop7.vals = &prop7val;
        prop7val.value = (char *)sm_client->desktop_file;
        prop7val.length = strlen(sm_client->desktop_file);
        n_props++;
    }

    props[0] = &prop1;
    props[1] = &prop2;
    props[2] = &prop3;
    props[3] = &prop4;
    props[4] = &prop5;
    props[5] = &prop6;
    props[6] = &prop7;

    SmcSetProperties(sm_client->session_connection, n_props, props);
#endif

    return TRUE;
}

/**
 * xfce_sm_client_disconnect:
 * @sm_client: An #XfceSMClient
 *
 * Disconnects the application from the session manager.
 *
 * <note><para>
 * This may not remove the application from the saved
 * session (if any) if the user later does not choose to save
 * the session when logging out.
 * </para></note>
 *
 **/
void
xfce_sm_client_disconnect(XfceSMClient *sm_client)
{
    if(startup_options.sm_disable)
        return;

#ifdef HAVE_LIBSM
    if(G_UNLIKELY(!sm_client->session_connection)) {
        g_warning("%s() called with no session connection", G_STRFUNC);
        return;
    }

    if(sm_client->restart_style == XFCE_SM_CLIENT_RESTART_IMMEDIATELY)
        xfce_sm_client_set_restart_style(sm_client, XFCE_SM_CLIENT_RESTART_NORMAL);

    SmcCloseConnection(sm_client->session_connection, 0, NULL);
    sm_client->session_connection = NULL;
    gdk_x11_set_sm_client_id(NULL);

    xfce_sm_client_set_state(sm_client, XFCE_SM_CLIENT_STATE_DISCONNECTED);
#endif
}

/**
 * xfce_sm_client_is_connected:
 * @sm_client: An #XfceSMClient
 *
 * Determines whether or not the application has connected to the
 * session manager.
 *
 * Returns: %TRUE if connected to the session manager, %FALSE otherwise
 **/
gboolean
xfce_sm_client_is_connected(XfceSMClient *sm_client)
{
    g_return_val_if_fail(XFCE_IS_SM_CLIENT(sm_client), FALSE);
#ifdef HAVE_LIBSM
    return !!sm_client->session_connection;
#else
    return FALSE;
#endif
}

/**
 * xfce_sm_client_is_resumed:
 * @sm_client: An #XfceSMClient
 *
 * Determines whether the application was resumed from a previous
 * session, or if the application has been started fresh with no
 * state information associated with it.
 *
 * Returns: %TRUE if resumed from a previous session, %FALSE otherwise
 **/
gboolean
xfce_sm_client_is_resumed(XfceSMClient *sm_client)
{
    g_return_val_if_fail(XFCE_IS_SM_CLIENT(sm_client), FALSE);
    return sm_client->resumed;
}

/**
 * xfce_sm_client_set_desktop_file:
 * @sm_client: An #XfceSMClient
 * @desktop_file: The path to the application's .desktop file
 *
 * Sets the application's .desktop file.  In addition to informing
 * the session manager of the .desktop file so it can present localized
 * names and an icon in session listings and the splash screen, this
 * also calls g_set_application_name() and
 * gtk_window_set_default_icon_name() (or
 * gtk_window_set_default_icon_from_file()) if the Name and Icon
 * keys are present, respectively.
 *
 * If a relative path to the file is provided, this function will search
 * the standard application directories as specified by the
 * <ulink type="http" url="http://standards.freedesktop.org/menu-spec/latest/">XDG
 * Desktop Menu Specification</ulink>.
 **/
void
xfce_sm_client_set_desktop_file(XfceSMClient *sm_client,
                                const gchar *desktop_file)
{
    XfceRc *rcfile = NULL;
    gchar *real_desktop_file = NULL;
    gboolean has_default_icon = FALSE;
    GList *icon_list;
    const gchar *name, *icon, *exec;

    g_return_if_fail(XFCE_IS_SM_CLIENT(sm_client));
    g_return_if_fail(desktop_file);

    if(!g_strcmp0(sm_client->desktop_file, desktop_file))
        return;

    if(!g_path_is_absolute(desktop_file)) {
        gchar res_name[1024];

        g_snprintf(res_name, sizeof(res_name), "applications/%s", desktop_file);
        real_desktop_file = xfce_resource_lookup(XFCE_RESOURCE_DATA, res_name);
        if(!real_desktop_file) {
            g_warning("Cannot find file \"%s\" in the standard search path",
                      desktop_file);
            return;
        }

        desktop_file = real_desktop_file;
    }

    rcfile = xfce_rc_simple_open(desktop_file, TRUE);
    if(!rcfile) {
        g_warning("Unable to open \"%s\"", desktop_file);
        goto out;
    }

    if(!xfce_rc_has_group(rcfile, "Desktop Entry")) {
        g_warning("File \"%s\" is not a valid .desktop file", desktop_file);
        goto out;
    }

    g_free(sm_client->desktop_file);
    sm_client->desktop_file = g_strdup(desktop_file);

    xfce_rc_set_group(rcfile, "Desktop Entry");

    if(!g_get_application_name()) {
        name = xfce_rc_read_entry(rcfile, "Name", NULL);
        if(name)
            g_set_application_name(name);
    }

    if(gtk_window_get_default_icon_name())
        has_default_icon = TRUE;

    icon_list = gtk_window_get_default_icon_list();
    if(icon_list) {
        has_default_icon = TRUE;
        g_list_free(icon_list);
    }

    if(!has_default_icon) {
        icon = xfce_rc_read_entry(rcfile, "Icon", NULL);
        if(icon) {
            if(g_path_is_absolute(icon))
                gtk_window_set_default_icon_from_file(icon, NULL);
            else
                gtk_window_set_default_icon_name(icon);
        }
    }

    exec = xfce_rc_read_entry(rcfile, "Exec", NULL);
    if(exec) {
        gchar **clone_argv = NULL;
        gint clone_argc = 0;

        /* FIXME: pull out the %-var substitutions first */

        if(g_shell_parse_argv(exec, &clone_argc, &clone_argv, NULL)) {
            xfce_sm_client_set_clone_command(sm_client, clone_argv);
            g_strfreev(clone_argv);
        }
    }

#ifdef HAVE_LIBSM
    if(sm_client->session_connection) {
        SmProp prop, *props[1];
        SmPropValue propval;

        prop.name = GsmDesktopFile;
        prop.type = SmARRAY8;
        prop.num_vals = 1;
        prop.vals = &propval;
        propval.value = sm_client->desktop_file;
        propval.length = strlen(sm_client->desktop_file);
        props[0] = &prop;

        SmcSetProperties(sm_client->session_connection, 1, props);
    }
#endif /* HAVE_LIBSM */

out:
    if(rcfile)
        xfce_rc_close(rcfile);
    g_free(real_desktop_file);
}

/**
 * xfce_sm_client_request_shutdown:
 * @sm_client: An #XfceSMClient
 * @shutdown_hint: The type of shutdown requested
 *
 * Sends a request to the session manager to end the session.
 * Depending on @hint, the session manager may prompt for a
 * certain action (log out, halt, reboot, etc.) or may take the
 * requested action without user intervention.
 *
 * <note><para>
 * The session manager may or may not support all requested
 * actions, and is also free to ignore the requested action.
 * </para></note>
 **/
void
xfce_sm_client_request_shutdown(XfceSMClient *sm_client,
                                XfceSMClientShutdownHint shutdown_hint)
{
    g_return_if_fail(XFCE_IS_SM_CLIENT(sm_client));

    if(startup_options.sm_disable)
        return;

    /* TODO: support xfce4-session's DBus interface */

#ifdef HAVE_LIBSM
    if(G_LIKELY(sm_client->session_connection)) {
        SmcRequestSaveYourself(sm_client->session_connection, SmSaveBoth,
                               True, SmInteractStyleAny, False, True);
    }
#endif
}

/**
 * xfce_sm_client_set_restart_style:
 * @sm_client: An #XfceSMClient
 * @restart_style: An #XfceSMClientRestartStyle value
 *
 * Sets the restart style hint to @restart_style.
 **/
void
xfce_sm_client_set_restart_style(XfceSMClient *sm_client,
                                 XfceSMClientRestartStyle restart_style)
{
    g_return_if_fail(XFCE_IS_SM_CLIENT(sm_client));

    if(sm_client->restart_style == restart_style)
        return;

    sm_client->restart_style = restart_style;

#ifdef HAVE_LIBSM
    if(sm_client->session_connection) {
        SmProp prop, *props[1];
        SmPropValue propval;
        char hint;

        hint = xsmp_restart_style_from_enum(sm_client->restart_style);

        prop.name = SmRestartStyleHint;
        prop.type = SmCARD8;
        prop.num_vals = 1;
        prop.vals = &propval;
        propval.value = &hint;
        propval.length = 1;
        props[0] = &prop;

        SmcSetProperties(sm_client->session_connection, 1, props);
    }
#endif /* HAVE_LIBSM */

    g_object_notify(G_OBJECT(sm_client), "restart-style");
}

/**
 * xfce_sm_client_set_priority:
 * @sm_client: An #XfceSMClient
 * @priority: A 8-bit signed priority value
 *
 * Sets the startup priority for @sm_client to @priority.  Note
 * that the default priority for applications is 50; lower values
 * should be reserved for components of the desktop environment.
 **/
void
xfce_sm_client_set_priority(XfceSMClient *sm_client,
                            guint8 priority)
{
    g_return_if_fail(XFCE_IS_SM_CLIENT(sm_client));

    if(sm_client->priority == priority)
        return;

    sm_client->priority = priority;

#ifdef HAVE_LIBSM
    if(sm_client->session_connection) {
        SmProp prop, *props[1];
        SmPropValue propval;

        prop.name = GsmPriority;
        prop.type = SmCARD8;
        prop.num_vals = 1;
        prop.vals = &propval;
        propval.value = &sm_client->priority;
        propval.length = 1;
        props[0] = &prop;

        SmcSetProperties(sm_client->session_connection, 1, props);
    }
#endif /* HAVE_LIBSM */

    g_object_notify(G_OBJECT(sm_client), "priority");
}

/**
 * xfce_sm_client_set_current_directory:
 * @sm_client: An #XfceSMClient
 * @current_directory: A valid path name
 *
 * Sets the startup working directory of @sm_client to
 * @current_directory.  If unset, defaults to the user's
 * home directory.
 **/
void
xfce_sm_client_set_current_directory(XfceSMClient *sm_client,
                                     const gchar *current_directory)
{
    g_return_if_fail(XFCE_IS_SM_CLIENT(sm_client));
    g_return_if_fail(current_directory && current_directory[0]);

    if(!g_strcmp0(sm_client->current_directory, current_directory))
        return;

    g_free(sm_client->current_directory);
    sm_client->current_directory = g_strdup(current_directory);

#ifdef HAVE_LIBSM
    if(sm_client->session_connection) {
        SmProp prop, *props[1];
        SmPropValue propval;

        prop.name = SmCurrentDirectory;
        prop.type = SmARRAY8;
        prop.num_vals = 1;
        prop.vals = &propval;
        propval.value = sm_client->current_directory;
        propval.length = strlen(propval.value);
        props[0] = &prop;

        SmcSetProperties(sm_client->session_connection, 1, props);
    }
#endif /* HAVE_LIBSM */

    g_object_notify(G_OBJECT(sm_client), "current-directory");
}

static gchar **
copy_command(gchar **command,
             gchar **value)
{
    if(command != value) {
        g_strfreev(command);
        command = NULL;
    }

    if(value)
        command = g_strdupv(value);

    return command;
}

/**
 * xfce_sm_client_set_restart_command:
 * @sm_client: An #XfceSMClient
 * @restart_command: An argument vector
 *
 * Sets the application's "restart" command, which is used to restart
 * the application and restore any saved state from the previous
 * run.
 *
 * If unset, defaults to the command used to start this instance
 * of the application, with session management related arguments
 * added (if not already present).
 **/
void
xfce_sm_client_set_restart_command(XfceSMClient *sm_client,
                                   gchar **restart_command)
{
    g_return_if_fail(XFCE_IS_SM_CLIENT(sm_client));
    sm_client->restart_command = copy_command(sm_client->restart_command,
                                              restart_command);
#ifdef HAVE_LIBSM
    xfce_sm_client_set_property_from_command(sm_client, SmRestartCommand,
                                             sm_client->restart_command,
                                             SM_ARG_APPEND);
#endif
    g_object_notify(G_OBJECT(sm_client), "restart-command");
}

/**
 * xfce_sm_client_get_restart_style:
 * @sm_client: An #XfceSMClient
 *
 * Retrieves the session client's restart style.  See
 * xfce_sm_client_set_restart_style() for more information.
 *
 * Returns: a value from the #XfceSMClientRestartStyle enum
 **/
XfceSMClientRestartStyle
xfce_sm_client_get_restart_style(XfceSMClient *sm_client)
{
    g_return_val_if_fail(XFCE_IS_SM_CLIENT(sm_client),
                         XFCE_SM_CLIENT_RESTART_NORMAL);
    return sm_client->restart_style;
}

/**
 * xfce_sm_client_get_priority:
 * @sm_client: An #XfceSMClient
 *
 * Retrieves the session client's restart priority.  See
 * xfce_sm_client_set_priority() for more information.
 *
 * Returns: a value from #G_MININT8 to #G_MAXINT8
 **/
guint8
xfce_sm_client_get_priority(XfceSMClient *sm_client)
{
    g_return_val_if_fail(XFCE_IS_SM_CLIENT(sm_client),
                         XFCE_SM_CLIENT_PRIORITY_DEFAULT);
    return sm_client->priority;
}

/**
 * xfce_sm_client_get_client_id:
 * @sm_client: An #XfceSMClient
 *
 * Retrieves the session client's unique ID.  This ID can
 * be used to construct a filename used to restore the
 * application's state.  Note that this value is only
 * guaranteed to be valid if connected to the session manager.
 *
 * <note><para>
 * Instead of constructing a state filename, it is
 * recommended to use xfce_sm_client_get_state_file().
 * </para></note>
 *
 * Returns: an opaque object-owned string
 **/
const gchar *
xfce_sm_client_get_client_id(XfceSMClient *sm_client)
{
    g_return_val_if_fail(XFCE_IS_SM_CLIENT(sm_client), NULL);
    return sm_client->client_id;
}

/**
 * xfce_sm_client_get_state_file:
 * @sm_client: An #XfceSMClient
 *
 * Constructs a filename that can be used to restore or save
 * state information.
 *
 * When saving state, ote that this file may already exist (and
 * may have been used for saving previous state for the
 * application), so the application should first remove or empty
 * the file if it requires a fresh state file.
 *
 * On the next application start, this function can be used to
 * check to see if there is any previous saved state, and, if so,
 * the state can be restored from the file.
 *
 * This function will use a standard location and naming scheme
 * and handle state cleanup (setting of the discard command) for you.
 *
 * Before calling this function, the application must have a
 * valid client ID (see xfce_sm_client_get_client_id()).
 *
 * Returns: a file name string, owned by the object or %NULL if
 *          the session client is disabled.
 **/
const gchar *
xfce_sm_client_get_state_file(XfceSMClient *sm_client)
{
    gchar *resource, *p;
    const gchar *prgname;

    g_return_val_if_fail(XFCE_IS_SM_CLIENT(sm_client), NULL);

    if(!sm_client->client_id)
        return NULL;

    if(sm_client->state_file)
        return sm_client->state_file;

    prgname = g_get_prgname();
    if(G_UNLIKELY(!prgname))
        prgname = "unknown";

    resource = g_strdup_printf("sessions/%s-%s.state",
                               prgname, sm_client->client_id);
    for(p = resource+9; *p; p++) {
        if(*p == '/')
            *p = '_';
    }

    sm_client->state_file = xfce_resource_save_location(XFCE_RESOURCE_CACHE,
                                                        resource, TRUE);
    if(!sm_client->state_file) {
        g_critical("XfceSMClient: Unable to create state file as "
                   "\"$XDG_CACHE_HOME/%s\"", resource);
    }

    g_free(resource);

#ifdef HAVE_LIBSM
    if(G_LIKELY(sm_client->state_file) && sm_client->session_connection)
    {
        gchar *discard_command[4];
        gint i = 0;

        discard_command[i++] = "rm";
        discard_command[i++] = "-rf";
        discard_command[i++] = sm_client->state_file;
        discard_command[i++] = NULL;

        xfce_sm_client_set_property_from_command(sm_client, SmDiscardCommand,
                                                 discard_command, FALSE);
    }
#endif

    return sm_client->state_file;
}

/**
 * xfce_sm_client_get_current_directory:
 * @sm_client: An #XfceSMClient
 *
 * Retrieves the session client's working directory.  See
 * xfce_sm_client_set_current_directory() for more information.
 *
 * Returns: an object-owned string
 **/
const gchar *
xfce_sm_client_get_current_directory(XfceSMClient *sm_client)
{
    g_return_val_if_fail(XFCE_IS_SM_CLIENT(sm_client), NULL);
    return sm_client->current_directory;
}

/**
 * xfce_sm_client_get_restart_command:
 * @sm_client: An #XfceSMClient
 *
 * Retrieves the session client's restart command.  See
 * xfce_sm_client_set_restart_command() for more information.
 *
 * Returns: an object-owned string vector
 **/
const gchar * const *
xfce_sm_client_get_restart_command(XfceSMClient *sm_client)
{
    g_return_val_if_fail(XFCE_IS_SM_CLIENT(sm_client), NULL);
    return (const gchar * const *)sm_client->restart_command;
}



#define __XFCE_SM_CLIENT_C__
#include <libxfce4ui/libxfce4ui-aliasdef.c>
