/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Christian Hammond
 * Copyright (C) 2006 John Palmieri
 * Copyright (C) 2010 Red Hat, Inc.
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

#ifndef _NOTIFY_NOTIFICATION_H_
#define _NOTIFY_NOTIFICATION_H_

#include <glib.h>
#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

/**
 * NOTIFY_EXPIRES_DEFAULT:
 *
 * The default expiration time on a notification.
 */
#define NOTIFY_EXPIRES_DEFAULT -1

/**
 * NOTIFY_EXPIRES_NEVER:
 *
 * The notification never expires.
 *
 * It stays open until closed by the calling API or the user.
 */
#define NOTIFY_EXPIRES_NEVER    0

#define NOTIFY_TYPE_NOTIFICATION         (notify_notification_get_type ())
#define NOTIFY_NOTIFICATION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NOTIFY_TYPE_NOTIFICATION, NotifyNotification))
#define NOTIFY_NOTIFICATION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NOTIFY_TYPE_NOTIFICATION, NotifyNotificationClass))
#define NOTIFY_IS_NOTIFICATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NOTIFY_TYPE_NOTIFICATION))
#define NOTIFY_IS_NOTIFICATION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NOTIFY_TYPE_NOTIFICATION))
#define NOTIFY_NOTIFICATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NOTIFY_TYPE_NOTIFICATION, NotifyNotificationClass))

typedef struct _NotifyNotification NotifyNotification;
typedef struct _NotifyNotificationClass NotifyNotificationClass;
typedef struct _NotifyNotificationPrivate NotifyNotificationPrivate;

/**
 * NotifyNotification:
 *
 * A passive pop-up notification.
 *
 * #NotifyNotification represents a passive pop-up notification. It can
 * contain summary text, body text, and an icon, as well as hints specifying
 * how the notification should be presented. The notification is rendered
 * by a notification daemon, and may present the notification in any number
 * of ways. As such, there is a clear separation of content and presentation,
 * and this API enforces that.
 */
struct _NotifyNotification
{
        /*< private >*/
        GObject                    parent_object;

        NotifyNotificationPrivate *priv;
};

struct _NotifyNotificationClass
{
        GObjectClass    parent_class;

        /* Signals */
        void            (*closed) (NotifyNotification *notification);
};


/**
 * NotifyUrgency:
 * @NOTIFY_URGENCY_LOW: Low urgency. Used for unimportant notifications.
 * @NOTIFY_URGENCY_NORMAL: Normal urgency. Used for most standard notifications.
 * @NOTIFY_URGENCY_CRITICAL: Critical urgency. Used for very important notifications.
 *
 * The urgency level of the notification.
 */
typedef enum
{
        NOTIFY_URGENCY_LOW,
        NOTIFY_URGENCY_NORMAL,
        NOTIFY_URGENCY_CRITICAL,

} NotifyUrgency;


/**
 * NotifyClosedReason:
 * @NOTIFY_CLOSED_REASON_UNSET: Notification not closed.
 * @NOTIFY_CLOSED_REASON_EXPIRED: Timeout has expired.
 * @NOTIFY_CLOSED_REASON_DISMISSED: It has been dismissed by the user.
 * @NOTIFY_CLOSED_REASON_API_REQUEST: It has been closed by a call to
 *   [method@NotifyNotification.close].
 * @NOTIFY_CLOSED_REASON_UNDEFIEND: Closed by undefined/reserved reasons.
 *
 * The reason for which the notification has been closed.
 *
 * Since: 0.8.0
 */
typedef enum
{
        NOTIFY_CLOSED_REASON_UNSET = -1,
        NOTIFY_CLOSED_REASON_EXPIRED = 1,
        NOTIFY_CLOSED_REASON_DISMISSED = 2,
        NOTIFY_CLOSED_REASON_API_REQUEST = 3,
        NOTIFY_CLOSED_REASON_UNDEFIEND = 4,
} NotifyClosedReason;

/**
 * NotifyActionCallback:
 * @notification: a #NotifyActionCallback notification
 * @action: (transfer none): The activated action name
 * @user_data: (nullable) (transfer none): User provided data
 *
 * An action callback function.
 */
typedef void    (*NotifyActionCallback) (NotifyNotification *notification,
                                         char               *action,
                                         gpointer            user_data);

/**
 * NOTIFY_ACTION_CALLBACK:
 * @func: The function to cast.
 *
 * A convenience macro for casting a function to a [callback@ActionCallback].
 *
 * This is much like [func@GObject.CALLBACK].
 */
#define NOTIFY_ACTION_CALLBACK(func) ((NotifyActionCallback)(func))

GType               notify_notification_get_type             (void);

NotifyNotification *notify_notification_new                  (const char         *summary,
                                                              const char         *body,
                                                              const char         *icon);

gboolean            notify_notification_update                (NotifyNotification *notification,
                                                               const char         *summary,
                                                               const char         *body,
                                                               const char         *icon);

gboolean            notify_notification_show                  (NotifyNotification *notification,
                                                               GError            **error);

void                notify_notification_set_timeout           (NotifyNotification *notification,
                                                               gint                timeout);

void                notify_notification_set_category          (NotifyNotification *notification,
                                                               const char         *category);

void                notify_notification_set_urgency           (NotifyNotification *notification,
                                                               NotifyUrgency       urgency);

void                notify_notification_set_image_from_pixbuf (NotifyNotification *notification,
                                                               GdkPixbuf          *pixbuf);

#ifndef LIBNOTIFY_DISABLE_DEPRECATED
void                notify_notification_set_icon_from_pixbuf  (NotifyNotification *notification,
                                                               GdkPixbuf          *icon);

void                notify_notification_set_hint_int32        (NotifyNotification *notification,
                                                               const char         *key,
                                                               gint                value);
void                notify_notification_set_hint_uint32       (NotifyNotification *notification,
                                                               const char         *key,
                                                               guint               value);

void                notify_notification_set_hint_double       (NotifyNotification *notification,
                                                               const char         *key,
                                                               gdouble             value);

void                notify_notification_set_hint_string       (NotifyNotification *notification,
                                                               const char         *key,
                                                               const char         *value);

void                notify_notification_set_hint_byte         (NotifyNotification *notification,
                                                               const char         *key,
                                                               guchar              value);

void                notify_notification_set_hint_byte_array   (NotifyNotification *notification,
                                                               const char         *key,
                                                               const guchar       *value,
                                                               gsize               len);
#endif

void                notify_notification_set_hint              (NotifyNotification *notification,
                                                               const char         *key,
                                                               GVariant           *value);

void                notify_notification_set_app_name          (NotifyNotification *notification,
                                                               const char         *app_name);

void                notify_notification_clear_hints           (NotifyNotification *notification);

void                notify_notification_add_action            (NotifyNotification *notification,
                                                               const char         *action,
                                                               const char         *label,
                                                               NotifyActionCallback callback,
                                                               gpointer            user_data,
                                                               GFreeFunc           free_func);

const char         *notify_notification_get_activation_token  (NotifyNotification *notification);

void                notify_notification_clear_actions         (NotifyNotification *notification);
gboolean            notify_notification_close                 (NotifyNotification *notification,
                                                               GError            **error);

gint                notify_notification_get_closed_reason     (const NotifyNotification *notification);

G_END_DECLS
#endif /* NOTIFY_NOTIFICATION_H */
