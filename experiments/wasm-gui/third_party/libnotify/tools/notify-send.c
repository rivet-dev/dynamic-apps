/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2004 Christian Hammond.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

#include "config.h"

#include <libnotify/notify.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib-unix.h>
#include <glib/gprintf.h>

#define N_(x) (x)
#define GETTEXT_PACKAGE NULL

static NotifyUrgency urgency = NOTIFY_URGENCY_NORMAL;
static GMainLoop *loop = NULL;

static gboolean
g_option_arg_urgency_cb (const char *option_name,
                         const char *value,
                         gpointer    data,
                         GError    **error)
{
        if (value != NULL) {
                if (!strcasecmp (value, "low"))
                        urgency = NOTIFY_URGENCY_LOW;
                else if (!strcasecmp (value, "normal"))
                        urgency = NOTIFY_URGENCY_NORMAL;
                else if (!strcasecmp (value, "critical"))
                        urgency = NOTIFY_URGENCY_CRITICAL;
                else {
                        *error = g_error_new (G_OPTION_ERROR,
                                              G_OPTION_ERROR_BAD_VALUE,
                                              N_("Unknown urgency %s specified. "
                                                 "Known urgency levels: low, "
                                                 "normal, critical."), value);

                        return FALSE;
                }
        }

        return TRUE;
}

static gboolean
notify_notification_set_hint_variant (NotifyNotification *notification,
                                      const char         *type,
                                      const char         *key,
                                      const char         *value,
                                      GError            **error)
{
        static gboolean conv_error = FALSE;
        if (!strcasecmp (type, "string")) {
                notify_notification_set_hint_string (notification,
                                                     key,
                                                     value);
        } else if (!strcasecmp (type, "int")) {
                if (!g_ascii_isdigit (*value))
                        conv_error = TRUE;
                else {
                        gint h_int = (gint) g_ascii_strtoull (value, NULL, 10);
                        notify_notification_set_hint_int32 (notification,
                                                            key,
                                                            h_int);
                }
        } else if (!strcasecmp (type, "double")) {
                if (!g_ascii_isdigit (*value))
                        conv_error = TRUE;
                else {
                        gdouble h_double = g_strtod (value, NULL);
                        notify_notification_set_hint_double (notification,
                                                             key, h_double);
                }
        } else if (!strcasecmp (type, "byte")) {
                gint h_byte = (gint) g_ascii_strtoull (value, NULL, 10);

                if (h_byte < 0 || h_byte > 0xFF)
                        conv_error = TRUE;
                else {
                        notify_notification_set_hint_byte (notification,
                                                           key,
                                                           (guchar) h_byte);
                }
        } else if (g_ascii_strcasecmp (type, "boolean") == 0) {
                gboolean h_boolean = FALSE;

                if (g_ascii_strcasecmp (value, "true") == 0) {
                        h_boolean = TRUE;
                } else if (g_ascii_isdigit (*value)) {
                        h_boolean = !!g_ascii_strtoull (value, NULL, 10);
                }

                notify_notification_set_hint (notification, key,
                                              g_variant_new_boolean (h_boolean));
        } else if (g_ascii_strcasecmp (type, "variant") == 0) {
                GVariant *variant = g_variant_parse (NULL, value, NULL, NULL, NULL);

                if (variant != NULL) {
                        notify_notification_set_hint (notification, key, variant);
                } else {
                        conv_error = TRUE;
                }

        } else {
                *error = g_error_new (G_OPTION_ERROR,
                                      G_OPTION_ERROR_BAD_VALUE,
                                      N_("Invalid hint type \"%s\". Valid types "
                                         "are int, double, string and byte."),
                                      type);
                return FALSE;
        }

        if (conv_error) {
                *error = g_error_new (G_OPTION_ERROR,
                                      G_OPTION_ERROR_BAD_VALUE,
                                      N_("Value \"%s\" of hint \"%s\" could not be "
                                         "parsed as type \"%s\"."), value, key,
                                      type);
                return FALSE;
        }

        return TRUE;
}

static void
handle_closed (NotifyNotification *notify,
               gpointer            user_data)
{
        g_main_loop_quit (loop);
}

static gboolean
on_sigint (gpointer data)
{
        NotifyNotification *notification = data;

        g_printerr ("Wait cancelled, closing notification\n");

        notify_notification_close (notification, NULL);
        g_main_loop_quit (loop);

        return FALSE;
}

static void
handle_action (NotifyNotification *notify,
               char               *action,
               gpointer            user_data)
{
        const char *action_name = user_data;
        const char *activation_token;

        activation_token = notify_notification_get_activation_token (notify);

        g_printf ("%s\n", action_name);

        if (activation_token) {
                g_debug ("Activation Token: %s", activation_token);
        }

        notify_notification_close (notify, NULL);
}

static gboolean
on_wait_timeout (gpointer data)
{
        fprintf (stderr, "Wait timeout expired\n");
        g_main_loop_quit (loop);

        return FALSE;
}

static gboolean
server_has_capability (const gchar *capability)
{
        GList *server_caps = notify_get_server_caps ();
        gboolean supported;

        supported = !!g_list_find_custom (server_caps,
                                          capability,
                                          (GCompareFunc) g_ascii_strcasecmp);

        g_list_foreach (server_caps, (GFunc) g_free, NULL);
        g_list_free (server_caps);

        return supported;
}

int
main (int argc, char *argv[])
{
        static const char  *summary = NULL;
        char               *body;
        static const char  *type = NULL;
        static char        *app_name = NULL;
        static char        *icon_str = NULL;
        static char       **n_text = NULL;
        static char       **hints = NULL;
        static char       **actions = NULL;
        static char        *server_name = NULL;
        static char        *server_vendor = NULL;
        static char        *server_version = NULL;
        static char        *server_spec_version = NULL;
        static gboolean     print_id = FALSE;
        static gint         notification_id = 0;
        static gboolean     do_version = FALSE;
        static gboolean     hint_error = FALSE, show_error = FALSE;
        static gboolean     transient = FALSE;
        static gboolean     wait = FALSE;
        static glong        expire_timeout = NOTIFY_EXPIRES_DEFAULT;
        GOptionContext     *opt_ctx;
        NotifyNotification *notify;
        GError             *error = NULL;
        gboolean            retval;

        static const GOptionEntry entries[] = {
                {"urgency", 'u', 0, G_OPTION_ARG_CALLBACK,
                 g_option_arg_urgency_cb,
                 N_("Specifies the urgency level (low, normal, critical)."),
                 N_("LEVEL")},
                {"expire-time", 't', 0, G_OPTION_ARG_INT, &expire_timeout,
                 N_
                 ("Specifies the timeout in milliseconds at which to expire the "
                  "notification."), N_("TIME")},
                {"app-name", 'a', 0, G_OPTION_ARG_STRING, &app_name,
                 N_("Specifies the app name for the icon"), N_("APP_NAME")},
                {"icon", 'i', 0, G_OPTION_ARG_FILENAME, &icon_str,
                 N_("Specifies an icon filename or stock icon to display."),
                 N_("ICON")},
                {"category", 'c', 0, G_OPTION_ARG_FILENAME, &type,
                 N_("Specifies the notification category."),
                 N_("TYPE[,TYPE...]")},
                {"transient", 'e', 0, G_OPTION_ARG_NONE, &transient,
                 N_("Create a transient notification"),
                 NULL},
                {"hint", 'h', 0, G_OPTION_ARG_FILENAME_ARRAY, &hints,
                 N_
                 ("Specifies basic extra data to pass. Valid types are boolean, int, double, string, byte and variant."),
                 N_("TYPE:NAME:VALUE")},
                {"print-id", 'p', 0, G_OPTION_ARG_NONE, &print_id,
                 N_ ("Print the notification ID."), NULL},
                {"replace-id", 'r', 0, G_OPTION_ARG_INT, &notification_id,
                 N_ ("The ID of the notification to replace."), N_("REPLACE_ID")},
                {"wait", 'w', 0, G_OPTION_ARG_NONE, &wait,
                 N_("Wait for the notification to be closed before exiting."),
                 NULL},
                {"action", 'A', 0, G_OPTION_ARG_FILENAME_ARRAY, &actions,
                 N_
                 ("Specifies the actions to display to the user. Implies --wait to wait for user input."
                 " May be set multiple times. The name of the action is output to stdout. If NAME is "
                 "not specified, the numerical index of the option is used (starting with 0)."),
                 N_("[NAME=]Text...")},
                {"version", 'v', 0, G_OPTION_ARG_NONE, &do_version,
                 N_("Version of the package."),
                 NULL},
                {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY,
                 &n_text, NULL,
                 NULL},
                {NULL}
        };

        body = NULL;

        setlocale (LC_ALL, "");

        g_set_prgname (argv[0]);
        g_log_set_always_fatal (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

        opt_ctx = g_option_context_new (N_("<SUMMARY> [BODY] - "
                                           "create a notification"));
        g_option_context_add_main_entries (opt_ctx, entries, GETTEXT_PACKAGE);
        retval = g_option_context_parse (opt_ctx, &argc, &argv, &error);
        g_option_context_free (opt_ctx);

        if (!retval) {
                fprintf (stderr, "%s\n", error->message);
                g_error_free (error);
                exit (1);
        }

        if (do_version) {
                g_printf ("%s %s\n", g_get_prgname (), VERSION);
                exit (0);
        }

        if (n_text != NULL && n_text[0] != NULL && *n_text[0] != '\0')
                summary = n_text[0];

        if (summary == NULL) {
                fprintf (stderr, "%s\n", N_("No summary specified."));
                exit (1);
        }

        if (n_text[1] != NULL) {
                body = g_strcompress (n_text[1]);

                if (n_text[2] != NULL) {
                        fprintf (stderr, "%s\n",
                                 N_("Invalid number of options."));
                        exit (1);
                }
        }

        if (!notify_init ("notify-send"))
                exit (1);

        notify_get_server_info (&server_name,
                                &server_vendor,
                                &server_version,
                                &server_spec_version);

        g_debug ("Using sever %s %s, v%s - Supporting Notification Spec %s",
                 server_name, server_vendor, server_version, server_spec_version);
        g_free (server_name);
        g_free (server_vendor);
        g_free (server_version);
        g_free (server_spec_version);

        notify = g_object_new (NOTIFY_TYPE_NOTIFICATION,
                               "summary", summary,
                               "body", body,
                               "icon-name", icon_str,
                               "id", notification_id,
                               NULL);

        notify_notification_set_category (notify, type);
        notify_notification_set_urgency (notify, urgency);
        notify_notification_set_timeout (notify, expire_timeout);
        notify_notification_set_app_name (notify, app_name);

        if (transient) {
                notify_notification_set_hint (notify, "transient",
                                              g_variant_new_boolean (TRUE));

                if (!server_has_capability ("persistence")) {
                        g_debug ("Persistence is not supported by the "
                                 "notifications server. "
                                 "All notifications are transient.");
                }
        }

        g_free (body);

        /* Set hints */
        if (hints != NULL) {
                gint            i = 0;
                gint            l;
                char          *hint = NULL;
                char         **tokens = NULL;

                while ((hint = hints[i++])) {
                        tokens = g_strsplit (hint, ":", 3);
                        l = g_strv_length (tokens);

                        if (l != 3) {
                                fprintf (stderr, "%s\n",
                                         N_("Invalid hint syntax specified. "
                                            "Use TYPE:NAME:VALUE."));
                                hint_error = TRUE;
                        } else {
                                retval = notify_notification_set_hint_variant (notify,
                                                                               tokens[0],
                                                                               tokens[1],
                                                                               tokens[2],
                                                                               &error);

                                if (!retval) {
                                        fprintf (stderr, "%s\n", error->message);
                                        g_clear_error (&error);
                                        hint_error = TRUE;
                                }
                        }

                        g_strfreev (tokens);
                        if (hint_error)
                                break;
                }
        }

        if (actions != NULL) {
                gint i = 0;
                char *action = NULL;
                gchar **spl = NULL;
                gboolean have_actions;

                have_actions = server_has_capability ("actions");
                if (!have_actions) {
                        g_printerr (N_("Actions are not supported by this "
                                       "notifications server. "
                                       "Displaying non-interactively.\n"));
                        show_error = TRUE;
                }

                while (have_actions && (action = actions[i++])) {
                        gchar *name;
                        const gchar *label;

                        spl = g_strsplit (action, "=", 2);

                        if (g_strv_length (spl) == 1) {
                                name = g_strdup_printf ("%d", i - 1);
                                label = g_strstrip (spl[0]);
                        } else {
                                name = g_strdup (g_strstrip (spl[0]));
                                label = g_strstrip (spl[1]);
                        }

                        if (*label != '\0' && *name != '\0') {
                                notify_notification_add_action (notify,
                                                                name,
                                                                label,
                                                                handle_action,
                                                                name,
                                                                g_free);
                                wait = TRUE;
                        }

                        g_strfreev (spl);
                }

                g_strfreev (actions);
        }

        if (wait) {
                g_signal_connect (G_OBJECT (notify),
                                  "closed",
                                  G_CALLBACK (handle_closed),
                                  NULL);

                if (expire_timeout > 0) {
                        g_timeout_add (expire_timeout, on_wait_timeout, NULL);
                }
        }

        if (!hint_error) {
                retval = notify_notification_show (notify, &error);

                if (!retval) {
                        fprintf (stderr, "%s\n", error->message);
                        g_clear_error (&error);
                        show_error = TRUE;
                }
        }

        if (print_id) {
                g_object_get (notify, "id", &notification_id, NULL);
                g_printf ("%d\n", notification_id);
        }

        if (wait) {
                g_unix_signal_add (SIGINT, on_sigint, notify);
                loop = g_main_loop_new (NULL, FALSE);
                g_main_loop_run (loop);
                g_main_loop_unref (loop);
                loop = NULL;
        }

        g_object_unref (G_OBJECT (notify));

        notify_uninit ();

        if (hint_error || show_error)
                exit (1);

        return 0;
}
