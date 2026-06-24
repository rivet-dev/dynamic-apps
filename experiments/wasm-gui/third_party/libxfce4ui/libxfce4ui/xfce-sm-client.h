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

#ifndef __XFCE_SM_CLIENT_H__
#define __XFCE_SM_CLIENT_H__

#if !defined(_LIBXFCE4UI_INSIDE_LIBXFCE4UI_H) && !defined(LIBXFCE4UI_COMPILATION)
#error "Only <libxfce4ui/libxfce4ui.h> can be included directly, this file is not part of the public API."
#endif

#include <glib-object.h>

#define XFCE_TYPE_SM_CLIENT             (xfce_sm_client_get_type())
#define XFCE_SM_CLIENT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), XFCE_TYPE_SM_CLIENT, XfceSMClient))
#define XFCE_IS_SM_CLIENT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFCE_TYPE_SM_CLIENT))
#define XFCE_SM_CLIENT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), XFCE_TYPE_SM_CLIENT, XfceSMClientClass))
#define XFCE_IS_SM_CLIENT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), XFCE_TYPE_SM_CLIENT))
#define XFCE_SM_CLIENT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), XFCE_TYPE_SM_CLIENT, XfceSMClientClass))

/**
 * XFCE_SM_CLIENT_ERROR:
 *
 * Error domain for XfceSmCLient. Errors in this domain will be from the #XfceSmCLientErrorEnum enumeration.
 * See #GError for more information on error domains.
 **/
#define XFCE_SM_CLIENT_ERROR xfce_sm_client_error_quark()

/**
 * XfceSmCLientErrorEnum:
 * @XFCE_SM_CLIENT_ERROR_FAILED: Failed to connect to the session manager.
 * @XFCE_SM_CLIENT_ERROR_INVALID_CLIENT: Session does not have a valid client id.
 *
 * Error codes returned by XfceSmCLient functions.
 **/
typedef enum {
  XFCE_SM_CLIENT_ERROR_FAILED,
  XFCE_SM_CLIENT_ERROR_INVALID_CLIENT
} XfceSmCLientErrorEnum;

G_BEGIN_DECLS

typedef enum
{
    XFCE_SM_CLIENT_RESTART_NORMAL = 0,
    XFCE_SM_CLIENT_RESTART_IMMEDIATELY,
} XfceSMClientRestartStyle;

typedef enum /*< skip >*/
{
    XFCE_SM_CLIENT_PRIORITY_HIGHEST = 0,
    XFCE_SM_CLIENT_PRIORITY_WM = 15,
    XFCE_SM_CLIENT_PRIORITY_CORE = 25,
    XFCE_SM_CLIENT_PRIORITY_DESKTOP = 35,
    XFCE_SM_CLIENT_PRIORITY_DEFAULT = 50,
    XFCE_SM_CLIENT_PRIORITY_LOWEST = 255,
} XfceSMClientPriority;

typedef enum
{
    XFCE_SM_CLIENT_SHUTDOWN_HINT_ASK = 0,
    XFCE_SM_CLIENT_SHUTDOWN_HINT_LOGOUT,
    XFCE_SM_CLIENT_SHUTDOWN_HINT_HALT,
    XFCE_SM_CLIENT_SHUTDOWN_HINT_REBOOT,
} XfceSMClientShutdownHint;

typedef struct _XfceSMClient  XfceSMClient;

GType xfce_sm_client_get_type(void) G_GNUC_CONST;

GQuark xfce_sm_client_error_quark (void);

GOptionGroup *xfce_sm_client_get_option_group(gint argc,
                                              gchar **argv);

XfceSMClient *xfce_sm_client_get(void);

XfceSMClient *xfce_sm_client_get_with_argv(gint argc,
                                           gchar **argv,
                                           XfceSMClientRestartStyle restart_style,
                                           guchar priority);

XfceSMClient *xfce_sm_client_get_full(XfceSMClientRestartStyle restart_style,
                                      guchar priority,
                                      const gchar *resumed_client_id,
                                      const gchar *current_directory,
                                      const gchar **restart_command,
                                      const gchar *desktop_file);

gboolean xfce_sm_client_connect(XfceSMClient *sm_client,
                                GError **error);
void xfce_sm_client_disconnect(XfceSMClient *sm_client);

void xfce_sm_client_request_shutdown(XfceSMClient *sm_client,
                                     XfceSMClientShutdownHint shutdown_hint);

gboolean xfce_sm_client_is_connected(XfceSMClient *sm_client);
gboolean xfce_sm_client_is_resumed(XfceSMClient *sm_client);

void xfce_sm_client_set_desktop_file(XfceSMClient *sm_client,
                                     const gchar *desktop_file);

const gchar *xfce_sm_client_get_client_id(XfceSMClient *sm_client);

const gchar *xfce_sm_client_get_state_file(XfceSMClient *sm_client);

void xfce_sm_client_set_restart_style(XfceSMClient *sm_client,
                                      XfceSMClientRestartStyle restart_style);
XfceSMClientRestartStyle xfce_sm_client_get_restart_style(XfceSMClient *sm_client);

void xfce_sm_client_set_priority(XfceSMClient *sm_client,
                                 guint8 priority);
guint8 xfce_sm_client_get_priority(XfceSMClient *sm_client);

void xfce_sm_client_set_current_directory(XfceSMClient *sm_client,
                                          const gchar *current_directory);
const gchar *xfce_sm_client_get_current_directory(XfceSMClient *sm_client);

void xfce_sm_client_set_restart_command(XfceSMClient *sm_client,
                                        gchar **restart_command);
const gchar * const *
xfce_sm_client_get_restart_command(XfceSMClient *sm_client);

G_END_DECLS

#endif  /* __XFCE_SM_CLIENT_H__ */
