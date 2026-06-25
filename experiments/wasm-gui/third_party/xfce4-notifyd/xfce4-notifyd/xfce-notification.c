/* vi:set et ai sw=4 sts=4 ts=4: */
/*
 *  xfce4-notifyd
 *
 *  Copyright (c) 2023 Brian Tarricone <brian@tarricone.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef ENABLE_SOUND
#include <canberra-gtk.h>
#endif

#include <common/xfce-notify-enum-types.h>

#include "xfce-notification.h"
#include "xfce-notify-window.h"

typedef struct _XfceNotification {
    GObject parent;

    guint id;
    gchar *log_id;

    gchar *summary;
    gchar *body;
    guint gauge_value;

    gchar *icon_name;
    GdkPixbuf *icon_pixbuf;
    gchar *icon_id;

    XfceNotifyUrgency urgency;
    guint expire_timeout;

    XfceNotificationActions *actions;

    guint32 gauge_value_set:1,
            icon_only:1,
            do_fadeout:1,
            do_slideout:1;

    GtkCssProvider *css_provider;

#ifdef ENABLE_SOUND
    ca_proplist *sound_props;
#endif

    GList *windows;
} XfceNotification;

enum {
    PROP0,
    PROP_ID,
    PROP_LOG_ID,

    PROP_SUMMARY,
    PROP_BODY,
    PROP_GAUGE_VALUE,
    PROP_GAUGE_VALUE_SET,
    PROP_ICON_ONLY,
    PROP_ICON_NAME,
    PROP_ICON_PIXBUF,
    PROP_ICON_ID,
    PROP_URGENCY,
    PROP_EXPIRE_TIMEOUT,
    PROP_ACTIONS,
    PROP_DO_FADEOUT,
    PROP_DO_SLIDEOUT,
    PROP_CSS_PROVIDER,
#ifdef ENABLE_SOUND
    PROP_SOUND_PROPS,
#endif

    N_PROPS,
};

enum
{
    SIG_CLOSED = 0,
    SIG_ACTION_INVOKED,

    N_SIGS,
};

static void xfce_notification_constructed(GObject *object);
static void xfce_notification_finalize(GObject *object);
static void xfce_notification_set_property(GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec);
static void xfce_notification_get_property(GObject *object,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec);

static void xfce_notification_set_summary(XfceNotification *notification,
                                          const gchar *summary);
static void xfce_notification_set_body(XfceNotification *notification,
                                       const gchar *body);
static void xfce_notification_set_gauge_value(XfceNotification *notification,
                                              guint gauge_value);
static void xfce_notification_set_gauge_value_set(XfceNotification *notification,
                                                  gboolean gauge_value_set);
static void xfce_notification_set_icon_only(XfceNotification *notification,
                                            gboolean icon_only);
static void xfce_notification_set_icon_name(XfceNotification *notification,
                                            const gchar *icon_name);
static void xfce_notification_set_icon_pixbuf(XfceNotification *notification,
                                              GdkPixbuf *icon_pixbuf);
static void xfce_notification_set_icon_id(XfceNotification *notification,
                                          const gchar *icon_id);
static void xfce_notification_set_urgency(XfceNotification *notification,
                                          XfceNotifyUrgency urgency);
static void xfce_notification_set_expire_timeout(XfceNotification *notification,
                                                 guint expire_timeout);
static void xfce_notification_set_actions(XfceNotification *notification,
                                          XfceNotificationActions *actions);
static void xfce_notification_set_log_id(XfceNotification *notification,
                                         const gchar *log_id);
static void xfce_notification_set_css_provider(XfceNotification *notification,
                                               GtkCssProvider *css_provider);
#ifdef ENABLE_SOUND
static void xfce_notification_set_sound_props(XfceNotification *notification,
                                              ca_proplist *sound_props);
#endif


G_DEFINE_TYPE(XfceNotification, xfce_notification, G_TYPE_OBJECT)


guint signals[N_SIGS] = { 0, };


static void
xfce_notification_class_init(XfceNotificationClass *klass) {
    static GParamSpec *properties[N_PROPS] = { NULL, };
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->constructed = xfce_notification_constructed;
    gobject_class->finalize = xfce_notification_finalize;
    gobject_class->set_property = xfce_notification_set_property;
    gobject_class->get_property = xfce_notification_get_property;

    properties[PROP_ID] = g_param_spec_uint("id",
                                            "id",
                                            "Notification ID handle",
                                            1, G_MAXUINT, 1,
                                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    properties[PROP_LOG_ID] = g_param_spec_string("log-id",
                                                  "log-id",
                                                  "Internal ID of the notification's log entry, if any",
                                                  NULL,
                                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    properties[PROP_SUMMARY] = g_param_spec_string("summary",
                                                   "summary",
                                                   "Displayed notification summary/title",
                                                   NULL,
                                                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_BODY] = g_param_spec_string("body",
                                                "body",
                                                "Displayed notification body text",
                                                NULL,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_GAUGE_VALUE] = g_param_spec_uint("gauge-value",
                                                     "gauge-value",
                                                     "Percentage value that should be displayed as a gauge instead of text",
                                                     0, 100, 0,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_GAUGE_VALUE_SET] = g_param_spec_boolean("gauge-value-set",
                                                            "gauge-value-set",
                                                            "Whether or not the gauge-value property should be used",
                                                            FALSE,
                                                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ICON_ONLY] = g_param_spec_boolean("icon-only",
                                                      "icon-only",
                                                      "Whether or not to only show the icon",
                                                      FALSE,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ICON_NAME] = g_param_spec_string("icon-name",
                                                     "icon-name",
                                                     "Themed icon name or icon filename to use to display in the notification",
                                                     NULL,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ICON_PIXBUF] = g_param_spec_object("icon-pixbuf",
                                                       "icon-pixbuf",
                                                       "Pixel data for an icon to display in the notification",
                                                       GDK_TYPE_PIXBUF,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ICON_ID] = g_param_spec_string("icon-id",
                                                   "icon-id",
                                                   "Hash of icon contents",
                                                   NULL,
                                                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_URGENCY] = g_param_spec_enum("urgency",
                                                 "urgency",
                                                 "Notification urgency level",
                                                 XFCE_TYPE_NOTIFY_URGENCY,
                                                 XFCE_NOTIFY_URGENCY_NORMAL,
                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_EXPIRE_TIMEOUT] = g_param_spec_uint("expire-timeout",
                                                        "expire-timeout",
                                                        "Timeout (in ms) after which the notification disappears",
                                                        0, G_MAXINT, 10000,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ACTIONS] = g_param_spec_pointer("actions",
                                                    "actions",
                                                    "Actions to be displayed as buttons in the notification",
                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_DO_FADEOUT] = g_param_spec_boolean("do-fadeout",
                                                       "do-fadeout",
                                                       "Whether or not to fade out the opacity of the notification on expiration",
                                                       FALSE,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_DO_SLIDEOUT] = g_param_spec_boolean("do-slideout",
                                                        "do-slideout",
                                                        "Whether or not to slide the notification off the screen on expiration",
                                                        FALSE,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_CSS_PROVIDER] = g_param_spec_object("css-provider",
                                                        "css-provider",
                                                        "CSS provider used to theme windows",
                                                        GTK_TYPE_CSS_PROVIDER,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

#ifdef ENABLE_SOUND
    properties[PROP_SOUND_PROPS] = g_param_spec_pointer("sound-props",
                                                        "sound-props",
                                                        "Property list describing sound to be played with notification",
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
#endif

    g_object_class_install_properties(gobject_class, N_PROPS, properties);

    signals[SIG_CLOSED] = g_signal_new("closed",
                                       XFCE_TYPE_NOTIFICATION,
                                       G_SIGNAL_RUN_LAST,
                                       0,
                                       NULL, NULL,
                                       g_cclosure_marshal_VOID__ENUM,
                                       G_TYPE_NONE, 1,
                                       XFCE_TYPE_NOTIFY_CLOSE_REASON);

    signals[SIG_ACTION_INVOKED] = g_signal_new("action-invoked",
                                               XFCE_TYPE_NOTIFICATION,
                                               G_SIGNAL_RUN_LAST,
                                               0,
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__STRING,
                                               G_TYPE_NONE,
                                               1, G_TYPE_STRING);


}

static void
xfce_notification_init(XfceNotification *notification) {}

static void
xfce_notification_constructed(GObject *object) {
    G_OBJECT_CLASS(xfce_notification_parent_class)->constructed(object);
}

static void
xfce_notification_finalize(GObject *object) {
    XfceNotification *notification = XFCE_NOTIFICATION(object);

    g_free(notification->summary);
    g_free(notification->body);
    g_free(notification->icon_name);
    g_free(notification->icon_id);
    g_free(notification->log_id);
    xfce_notification_actions_free(notification->actions);
    if (notification->icon_pixbuf != NULL) {
        g_object_unref(notification->icon_pixbuf);
    }
    if (notification->css_provider != NULL) {
        g_object_unref(notification->css_provider);
    }

#ifdef ENABLE_SOUND
    ca_proplist_destroy(notification->sound_props);
#endif

    // FIXME: probably not what we want?
    g_list_free_full(notification->windows, (GDestroyNotify)gtk_widget_destroy);

    G_OBJECT_CLASS(xfce_notification_parent_class)->finalize(object);
}

static void
xfce_notification_set_property(GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    XfceNotification *notification = XFCE_NOTIFICATION(object);

    switch (prop_id) {
        case PROP_ID:
            notification->id = g_value_get_uint(value);
            break;

        case PROP_LOG_ID:
            xfce_notification_set_log_id(notification, g_value_get_string(value));
            break;

        case PROP_SUMMARY:
            xfce_notification_set_summary(notification, g_value_get_string(value));
            break;

        case PROP_BODY:
            xfce_notification_set_body(notification, g_value_get_string(value));
            break;

        case PROP_GAUGE_VALUE:
            xfce_notification_set_gauge_value(notification, g_value_get_uint(value));
            break;

        case PROP_GAUGE_VALUE_SET:
            xfce_notification_set_gauge_value_set(notification, g_value_get_boolean(value));
            break;

        case PROP_ICON_ONLY:
            xfce_notification_set_icon_only(notification, g_value_get_boolean(value));
            break;

        case PROP_ICON_NAME:
            xfce_notification_set_icon_name(notification, g_value_get_string(value));
            break;

        case PROP_ICON_PIXBUF:
            xfce_notification_set_icon_pixbuf(notification, g_value_get_object(value));
            break;

        case PROP_ICON_ID:
            xfce_notification_set_icon_id(notification, g_value_get_string(value));
            break;

        case PROP_EXPIRE_TIMEOUT:
            xfce_notification_set_expire_timeout(notification, g_value_get_uint(value));
            break;

        case PROP_URGENCY:
            xfce_notification_set_urgency(notification, g_value_get_enum(value));
            break;

        case PROP_ACTIONS:
            xfce_notification_set_actions(notification, g_value_get_pointer(value));
            break;

        case PROP_DO_FADEOUT:
            xfce_notification_set_do_fadeout(notification, g_value_get_boolean(value));
            break;

        case PROP_DO_SLIDEOUT:
            xfce_notification_set_do_slideout(notification, g_value_get_boolean(value));
            break;

        case PROP_CSS_PROVIDER:
            xfce_notification_set_css_provider(notification, g_value_get_object(value));
            break;

#ifdef ENABLE_SOUND
        case PROP_SOUND_PROPS:
            xfce_notification_set_sound_props(notification, g_value_get_pointer(value));
            break;
#endif

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
xfce_notification_get_property(GObject *object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
    XfceNotification *notification = XFCE_NOTIFICATION(object);

    switch (prop_id) {
        case PROP_ID:
            g_value_set_uint(value, notification->id);
            break;

        case PROP_LOG_ID:
            g_value_set_string(value, notification->log_id);
            break;

        case PROP_SUMMARY:
            g_value_set_string(value, notification->summary);
            break;

        case PROP_BODY:
            g_value_set_string(value, notification->body);
            break;

        case PROP_GAUGE_VALUE:
            g_value_set_uint(value, notification->gauge_value);
            break;

        case PROP_GAUGE_VALUE_SET:
            g_value_set_boolean(value, notification->gauge_value_set);
            break;

        case PROP_ICON_ONLY:
            g_value_set_boolean(value, notification->icon_only);
            break;

        case PROP_ICON_NAME:
            g_value_set_string(value, notification->icon_name);
            break;

        case PROP_ICON_PIXBUF:
            g_value_set_object(value, notification->icon_pixbuf);
            break;

        case PROP_ICON_ID:
            g_value_set_string(value, notification->icon_id);
            break;

        case PROP_EXPIRE_TIMEOUT:
            g_value_set_uint(value, notification->expire_timeout);
            break;

        case PROP_URGENCY:
            g_value_set_enum(value, notification->urgency);
            break;

        case PROP_ACTIONS:
            g_value_set_pointer(value, notification->actions);
            break;

        case PROP_DO_FADEOUT:
            g_value_set_boolean(value, notification->do_fadeout);
            break;

        case PROP_DO_SLIDEOUT:
            g_value_set_boolean(value, notification->do_slideout);
            break;

        case PROP_CSS_PROVIDER:
            g_value_set_object(value, notification->css_provider);
            break;

#ifdef ENABLE_SOUND
        case PROP_SOUND_PROPS:
            g_value_set_pointer(value, notification->sound_props);
            break;
#endif

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
xfce_notification_set_summary(XfceNotification *notification, const gchar *summary) {
    if (g_strcmp0(notification->summary, summary) != 0) {
        g_free(notification->summary);
        notification->summary = g_strdup(summary);
        g_object_notify(G_OBJECT(notification), "summary");
    }
}

static void
xfce_notification_set_body(XfceNotification *notification, const gchar *body) {
    if (g_strcmp0(notification->body, body) != 0) {
        g_free(notification->body);
        notification->body = g_strdup(body);
        g_object_notify(G_OBJECT(notification), "body");
    }
}

static void
xfce_notification_set_gauge_value(XfceNotification *notification, guint gauge_value) {
    g_return_if_fail(gauge_value <= 100);
    if (notification->gauge_value != gauge_value) {
        notification->gauge_value = gauge_value;
        g_object_notify(G_OBJECT(notification), "gauge-value");
    }
}

static void
xfce_notification_set_gauge_value_set(XfceNotification *notification, gboolean gauge_value_set) {
    if (notification->gauge_value_set != gauge_value_set) {
        notification->gauge_value_set = gauge_value_set;
        g_object_notify(G_OBJECT(notification), "gauge-value-set");
    }
}

static void
xfce_notification_set_icon_only(XfceNotification *notification, gboolean icon_only) {
    if (notification->icon_only != icon_only) {
        notification->icon_only = icon_only;
        g_object_notify(G_OBJECT(notification), "icon-only");
    }
}

static void
xfce_notification_set_icon_name(XfceNotification *notification, const gchar *icon_name) {
    if (g_strcmp0(notification->icon_name, icon_name) != 0) {
        g_free(notification->icon_name);
        notification->icon_name = g_strdup(icon_name);
        g_object_notify(G_OBJECT(notification), "icon-name");
    }
}

static void
xfce_notification_set_icon_pixbuf(XfceNotification *notification, GdkPixbuf *icon_pixbuf) {
    if (notification->icon_pixbuf != icon_pixbuf) {
        g_clear_object(&notification->icon_pixbuf);
        if (icon_pixbuf != NULL) {
            notification->icon_pixbuf = g_object_ref(icon_pixbuf);
        }
        g_object_notify(G_OBJECT(notification), "icon-pixbuf");
    }
}

static void
xfce_notification_set_icon_id(XfceNotification *notification, const gchar *icon_id) {
    if (g_strcmp0(notification->icon_id, icon_id) != 0) {
        g_free(notification->icon_id);
        notification->icon_id = g_strdup(icon_id);
        g_object_notify(G_OBJECT(notification), "icon-id");
    }
}

static void
xfce_notification_set_urgency(XfceNotification *notification, XfceNotifyUrgency urgency) {
    if (notification->urgency != urgency) {
        notification->urgency = urgency;
        g_object_notify(G_OBJECT(notification), "urgency");
    }
}

static void
xfce_notification_set_expire_timeout(XfceNotification *notification, guint expire_timeout) {
    if (notification->expire_timeout != expire_timeout) {
        notification->expire_timeout = expire_timeout;
        g_object_notify(G_OBJECT(notification), "expire-timeout");
    }
}

static void
xfce_notification_set_actions(XfceNotification *notification, XfceNotificationActions *actions) {
    // TODO: deep compare
    if (notification->actions != actions) {
        xfce_notification_actions_free(notification->actions);
        notification->actions = actions;
        g_object_notify(G_OBJECT(notification), "actions");
    }
}

static void
xfce_notification_set_log_id(XfceNotification *notification, const gchar *log_id) {
    g_return_if_fail(log_id == NULL || log_id[0] != '\0');
    if (g_strcmp0(notification->log_id, log_id) != 0) {
        g_free(notification->log_id);
        notification->log_id = g_strdup(log_id);
        g_object_notify(G_OBJECT(notification), "log-id");
    }
}

static void
xfce_notification_set_css_provider(XfceNotification *notification, GtkCssProvider *css_provider) {
    g_return_if_fail(GTK_IS_CSS_PROVIDER(css_provider));
    if (notification->css_provider != css_provider) {
        g_clear_object(&notification->css_provider);
        notification->css_provider = g_object_ref(css_provider);
        g_object_notify(G_OBJECT(notification), "css-provider");
    }
}

#ifdef ENABLE_SOUND
static void
xfce_notification_play_sound(XfceNotification *notification) {
    if (notification->sound_props != NULL && notification->windows != NULL) {
        GtkWidget *window = GTK_WIDGET(notification->windows->data);
        ca_context *ctx;

        if (notification->summary != NULL) {
            ca_proplist_sets(notification->sound_props, CA_PROP_EVENT_DESCRIPTION, notification->summary);
        }
        ca_gtk_proplist_set_for_widget(notification->sound_props, window);

        ctx = ca_gtk_context_get_for_screen(gtk_widget_get_screen(window));
        ca_context_play_full(ctx, notification->id, notification->sound_props, NULL, NULL);

        ca_proplist_destroy(notification->sound_props);
        notification->sound_props = NULL;
    }
}

static void
xfce_notification_set_sound_props(XfceNotification *notification, ca_proplist *sound_props) {
    if (notification->sound_props != sound_props) {
        if (notification->sound_props != NULL) {
            ca_proplist_destroy(notification->sound_props);
            notification->sound_props = NULL;
        }
        notification->sound_props = sound_props;
        xfce_notification_play_sound(notification);
        g_object_notify(G_OBJECT(notification), "sound-props");
    }
}
#endif

static void
xfce_notification_window_action_invoked(XfceNotifyWindow *window,
                                        const gchar *action_id,
                                        XfceNotification *notification)
{
    g_signal_emit(notification, signals[SIG_ACTION_INVOKED], 0, action_id);
}

static void
xfce_notification_window_closed(XfceNotifyWindow *window,
                                XfceNotifyCloseReason reason,
                                XfceNotification *notification)
{
    g_object_ref(notification);  // ensure we don't get destroyed during signal emission

    g_signal_emit(notification, signals[SIG_CLOSED], 0, reason);

    g_list_free_full(notification->windows, (GDestroyNotify)gtk_widget_destroy);
    notification->windows = NULL;

    g_object_unref(notification);
}

XfceNotification *
xfce_notification_new(guint id,
                      const gchar *log_id,
                      const gchar *summary,
                      const gchar *body,
                      gboolean icon_only,
                      const gchar *icon_name,
                      GdkPixbuf *icon_pixbuf,
                      const gchar *icon_id,
                      guint gauge_value,
                      gboolean gauge_value_set,
                      XfceNotificationActions *actions,
                      guint expire_timeout,
                      XfceNotifyUrgency urgency,
                      gboolean do_fadeout,
                      gboolean do_slideout,
#ifdef ENABLE_SOUND
                      ca_proplist *sound_props,
#endif
                      GtkCssProvider *css_provider)
{
    g_return_val_if_fail(id > 0, NULL);
    g_return_val_if_fail(GTK_IS_CSS_PROVIDER(css_provider), NULL);

    return g_object_new(XFCE_TYPE_NOTIFICATION,
                        "id", id,
                        "log-id", log_id,
                        "summary", summary,
                        "body", body,
                        "icon-only", icon_only,
                        "icon-name", icon_name,
                        "icon-pixbuf", icon_pixbuf,
                        "icon-id", icon_id,
                        "gauge-value", gauge_value,
                        "gauge-value-set", gauge_value_set,
                        "actions", actions,
                        "expire-timeout", expire_timeout,
                        "urgency", urgency,
                        "do-fadeout", do_fadeout,
                        "do-slideout", do_slideout,
#ifdef ENABLE_SOUND
                        "sound-props", sound_props,
#endif
                        "css-provider", css_provider,
                        NULL);

}

void
xfce_notification_update(XfceNotification *notification,
                         const gchar *summary,
                         const gchar *body,
                         gboolean icon_only,
                         const gchar *icon_name,
                         GdkPixbuf *icon_pixbuf,
                         const gchar *icon_id,
                         guint gauge_value,
                         gboolean gauge_value_set,
                         XfceNotificationActions *actions,
                         guint expire_timeout,
                         XfceNotifyUrgency urgency
#ifdef ENABLE_SOUND
                         , ca_proplist *sound_props
#endif
                         )
{
    g_return_if_fail(XFCE_IS_NOTIFICATION(notification));

    g_object_freeze_notify(G_OBJECT(notification));
    g_object_set(notification,
                 "summary", summary,
                 "body", body,
                 "icon-only", icon_only,
                 "icon-name", icon_name,
                 "icon-pixbuf", icon_pixbuf,
                 "icon-id", icon_id,
                 "gauge-value", gauge_value,
                 "gauge-value-set", gauge_value_set,
                 "actions", actions,
                 "expire-timeout", expire_timeout,
                 "urgency", urgency,
#ifdef ENABLE_SOUND
                 "sound-props", sound_props,
#endif
                 NULL);
    g_object_thaw_notify(G_OBJECT(notification));
}

static XfceNotifyWindow *
create_notify_window(XfceNotification *notification,
                     GdkMonitor *monitor,
                     gboolean override_redirect,
                     XfceNotifyPosition location,
                     gdouble normal_opacity,
                     gboolean show_text_with_gauge)
{
    static const gchar *bind_properties[] = {
        "summary",
        "body",
        "gauge-value",
        "gauge_value-set",
        "icon-only",
        "icon-name",
        "icon-pixbuf",
        "expire-timeout",
        "urgency",
        "actions",
        "css-provider",
        "do-fadeout",
        "do-slideout",
    };

    XfceNotifyWindow *window = XFCE_NOTIFY_WINDOW(xfce_notify_window_new(notification->id,
                                                                         monitor,
                                                                         override_redirect,
                                                                         location,
                                                                         normal_opacity,
                                                                         show_text_with_gauge));

    for (gsize i = 0; i < G_N_ELEMENTS(bind_properties); ++i) {
        g_object_bind_property(notification, bind_properties[i],
                               window, bind_properties[i],
                               G_BINDING_SYNC_CREATE);
    }

    g_signal_connect(window, "action-invoked",
                     G_CALLBACK(xfce_notification_window_action_invoked), notification);
    g_signal_connect(window, "closed",
                     G_CALLBACK(xfce_notification_window_closed), notification);

    return window;
}

GList *
xfce_notification_create_windows(XfceNotification *notification,
                                 GList *monitors,
                                 gboolean override_redirect,
                                 XfceNotifyPosition location,
                                 gdouble normal_opacity,
                                 gboolean show_text_with_gauge)
{
    g_return_val_if_fail(XFCE_IS_NOTIFICATION(notification), NULL);
    g_return_val_if_fail(notification->windows == NULL, NULL);

    if (monitors == NULL) {
        // On wayland, NULL monitors means "active monitor".  We'll need to
        // figure out the window's location after the compositor maps it.
        XfceNotifyWindow *window = create_notify_window(notification,
                                                        NULL,
                                                        override_redirect,
                                                        location,
                                                        normal_opacity,
                                                        show_text_with_gauge);
        notification->windows = g_list_prepend(notification->windows, window);
    } else {
        for (GList *l = monitors; l != NULL; l = l->next) {
            GdkMonitor *monitor = GDK_MONITOR(l->data);
            XfceNotifyWindow *window = create_notify_window(notification,
                                                            monitor,
                                                            override_redirect,
                                                            location,
                                                            normal_opacity,
                                                            show_text_with_gauge);
            notification->windows = g_list_prepend(notification->windows, window);
        }
        notification->windows = g_list_reverse(notification->windows);
    }

    g_assert(notification->windows != NULL);
    return notification->windows;
}


void
xfce_notification_realize(XfceNotification *notification) {
    g_return_if_fail(XFCE_IS_NOTIFICATION(notification));
    g_return_if_fail(notification->windows != NULL);

    for (GList *l = notification->windows; l != NULL; l = l->next) {
        gtk_widget_realize(GTK_WIDGET(l->data));
    }

#ifdef ENABLE_SOUND
    xfce_notification_play_sound(notification);
#endif
}

void
xfce_notification_set_do_fadeout(XfceNotification *notification, gboolean do_fadeout) {
    g_return_if_fail(XFCE_IS_NOTIFICATION(notification));
    if (notification->do_fadeout != do_fadeout) {
        notification->do_fadeout = do_fadeout;
        g_object_notify(G_OBJECT(notification), "do-fadeout");
    }
}

void
xfce_notification_set_do_slideout(XfceNotification *notification, gboolean do_slideout) {
    g_return_if_fail(XFCE_IS_NOTIFICATION(notification));
    if (notification->do_slideout != do_slideout) {
        notification->do_slideout = do_slideout;
        g_object_notify(G_OBJECT(notification), "do-slideout");
    }
}

guint
xfce_notification_get_id(XfceNotification *notification) {
    g_return_val_if_fail(XFCE_IS_NOTIFICATION(notification), 0);
    return notification->id;
}

const gchar *
xfce_notification_get_log_id(XfceNotification *notification) {
    g_return_val_if_fail(XFCE_IS_NOTIFICATION(notification), NULL);
    return notification->log_id;
}

const gchar *
xfce_notification_get_summary(XfceNotification *notification) {
    g_return_val_if_fail(XFCE_IS_NOTIFICATION(notification), NULL);
    return notification->summary;
}

const gchar *
xfce_notification_get_body(XfceNotification *notification) {
    g_return_val_if_fail(XFCE_IS_NOTIFICATION(notification), NULL);
    return notification->body;
}

const gchar *
xfce_notification_get_icon_id(XfceNotification *notification) {
    g_return_val_if_fail(XFCE_IS_NOTIFICATION(notification), NULL);
    return notification->icon_id;
}

XfceNotifyUrgency
xfce_notification_get_urgency(XfceNotification *notification) {
    g_return_val_if_fail(XFCE_IS_NOTIFICATION(notification), XFCE_NOTIFY_URGENCY_NORMAL);
    return notification->urgency;
}

GList *
xfce_notification_get_windows(XfceNotification *notification) {
    g_return_val_if_fail(XFCE_IS_NOTIFICATION(notification), NULL);
    return notification->windows;
}

void
xfce_notification_closed(XfceNotification *notification, XfceNotifyCloseReason reason) {
    g_return_if_fail(XFCE_IS_NOTIFICATION(notification));
    g_signal_emit(notification, signals[SIG_CLOSED], 0, reason);
}
