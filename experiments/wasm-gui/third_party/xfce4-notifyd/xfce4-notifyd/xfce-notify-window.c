/*
 *  xfce4-notifyd
 *
 *  Copyright (c) 2008-2009 Brian Tarricone <bjt23@cornell.edu>
 *  Copyright (c) 2009 Jérôme Guelfucci <jeromeg@xfce.org>
 *  Copyright (c) 2015 Ali Abdallah    <ali@xfce.org>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <math.h>

#include <libxfce4ui/libxfce4ui.h>

#ifdef ENABLE_X11
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#endif

#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#include <gtk-layer-shell.h>
#endif

#include <common/xfce-notify-common.h>
#include <common/xfce-notify-enum-types.h>

#include "xfce-notify-types.h"
#include "xfce-notify-window.h"

#define DEFAULT_EXPIRE_TIMEOUT 10000
#define DEFAULT_NORMAL_OPACITY 0.85
#define DEFAULT_DO_FADEOUT     TRUE
#define DEFAULT_DO_SLIDEOUT    FALSE
#define FADE_TIME              800
#define FADE_CHANGE_TIMEOUT    50
#define DEFAULT_RADIUS         10
#define DEFAULT_PADDING        14.0
#define BASE_CSS               ".xfce4-notifyd { font-size: initial; }"
#define NO_COMPOSITING_CSS     ".xfce4-notifyd { border-radius: 0px; }"

struct _XfceNotifyWindow
{
    GtkWindow parent;

    guint id;
    GdkMonitor *monitor;

    GtkCssProvider *css_provider;

    GdkRectangle geometry;
    gboolean override_redirect;

    XfceNotifyUrgency urgency;
    guint expire_timeout;

    gboolean mouse_hover;

    gdouble normal_opacity;
    gint original_x, original_y;
    gint draw_offset_x, draw_offset_y;

    guint32 icon_only:1,
            has_summary_text:1,
            has_body_text:1,
            gauge_value_set:1,
            show_text_with_gauge:1;
    XfceNotificationActions *actions;

    GtkWidget *icon_box;
    GtkWidget *icon;
    GtkWidget *content_box;
    GtkWidget *gauge;
    GtkWidget *summary;
    GtkWidget *body;
    GtkWidget *button_box;

    guint64 expire_start_timestamp;
    guint expire_id;
    guint fade_id;
    guint op_change_steps;
    gdouble op_change_delta;
    gboolean do_fadeout;
    gboolean do_slideout;
    XfceNotifyPosition notify_location;
};

typedef struct
{
    GtkWindowClass parent;

    /*< signals >*/
    void (*closed)(XfceNotifyWindow *window,
                   XfceNotifyCloseReason reason);
    void (*action_invoked)(XfceNotifyWindow *window,
                           const gchar *action_id);
} XfceNotifyWindowClass;

enum {
    PROP0,
    PROP_ID,
    PROP_MONITOR,
    PROP_OVERRIDE_REDIRECT,
    PROP_NORMAL_OPACITY,
    PROP_SHOW_TEXT_WITH_GAUGE,
    PROP_LOCATION,

    PROP_SUMMARY,
    PROP_BODY,
    PROP_GAUGE_VALUE,
    PROP_GAUGE_VALUE_SET,
    PROP_ICON_ONLY,
    PROP_ICON_NAME,
    PROP_ICON_PIXBUF,
    PROP_ACTIONS,
    PROP_URGENCY,
    PROP_EXPIRE_TIMEOUT,
    PROP_DO_FADEOUT,
    PROP_DO_SLIDEOUT,
    PROP_CSS_PROVIDER,

    N_PROPS,
};

enum
{
    SIG_CLOSED = 0,
    SIG_NEED_POSITION,
    SIG_ACTION_INVOKED,
    N_SIGS,
};

static void xfce_notify_window_constructed(GObject *object);
static void xfce_notify_window_dispose(GObject *object);
static void xfce_notify_window_finalize(GObject *object);
static void xfce_notify_window_set_property(GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void xfce_notify_window_get_property(GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec);
static void xfce_notify_window_notify(GObject *object,
                                      GParamSpec *pspec);

static void xfce_notify_window_size_allocate(GtkWidget *widget,
                                             GtkAllocation *allocation);
static void xfce_notify_window_realize(GtkWidget *widget);
static void xfce_notify_window_show(GtkWidget *widget);
static void xfce_notify_window_unrealize(GtkWidget *widget);
static gboolean xfce_notify_window_draw (GtkWidget *widget,
                                         cairo_t *cr);
static gboolean xfce_notify_window_enter_leave(GtkWidget *widget,
                                               GdkEventCrossing *evt);
static gboolean xfce_notify_window_button_release(GtkWidget *widget,
                                                  GdkEventButton *evt);
static gboolean xfce_notify_window_motion_notify(GtkWidget *widget,
                                                 GdkEventMotion *evt);
static gboolean xfce_notify_window_configure_event(GtkWidget *widget,
                                                   GdkEventConfigure *evt);
static gboolean xfce_notify_window_expire_timeout(gpointer data);
static gboolean xfce_notify_window_fade_timeout(gpointer data);
static void xfce_notify_window_reset_fade_and_slide(XfceNotifyWindow *window);

static void xfce_notify_window_button_clicked(GtkWidget *widget,
                                              gpointer user_data);

static void xfce_notify_window_set_summary(XfceNotifyWindow *window,
                                           const gchar *summary);
static void xfce_notify_window_set_body(XfceNotifyWindow *window,
                                        const gchar *body);
static void xfce_notify_window_set_gauge_value(XfceNotifyWindow *window,
                                               guint value);
static void xfce_notify_window_set_gauge_value_set(XfceNotifyWindow *window,
                                                   gboolean gauge_value_set);
static void xfce_notify_window_set_icon_name(XfceNotifyWindow *window,
                                             const gchar *icon_name);
static void xfce_notify_window_set_icon_pixbuf(XfceNotifyWindow *window,
                                               GdkPixbuf *pixbuf);
static void xfce_notify_window_set_expire_timeout(XfceNotifyWindow *window,
                                                  gint expire_timeout);
static void xfce_notify_window_set_urgency(XfceNotifyWindow *window,
                                           XfceNotifyUrgency urgency);
static void xfce_notify_window_set_actions(XfceNotifyWindow *window,
                                           XfceNotificationActions *actions);
static void xfce_notify_window_set_icon_only(XfceNotifyWindow *window,
                                             gboolean icon_only);
static void xfce_notify_window_set_css_provider(XfceNotifyWindow *window,
                                                GtkCssProvider *css_provider);
static void xfce_notify_window_set_do_fadeout(XfceNotifyWindow *window,
                                              gboolean do_fadeout);
static void xfce_notify_window_set_do_slideout(XfceNotifyWindow *window,
                                               gboolean do_slideout);

static void xfce_notify_window_move(XfceNotifyWindow *window, gint x, gint y);

static void xfce_notify_window_ensure_widgets(XfceNotifyWindow *window);


static guint signals[N_SIGS] = { 0, };

G_DEFINE_TYPE(XfceNotifyWindow, xfce_notify_window, GTK_TYPE_WINDOW)


static void
xfce_notify_window_class_init(XfceNotifyWindowClass *klass)
{
    static GParamSpec *properties[N_PROPS] = { NULL, };
    GObjectClass *gobject_class = (GObjectClass *)klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

    gobject_class->constructed = xfce_notify_window_constructed;
    gobject_class->dispose = xfce_notify_window_dispose;
    gobject_class->finalize = xfce_notify_window_finalize;
    gobject_class->set_property = xfce_notify_window_set_property;
    gobject_class->get_property = xfce_notify_window_get_property;
    gobject_class->notify = xfce_notify_window_notify;

    widget_class->size_allocate = xfce_notify_window_size_allocate;
    widget_class->realize = xfce_notify_window_realize;
    widget_class->unrealize = xfce_notify_window_unrealize;
    widget_class->show = xfce_notify_window_show;

    widget_class->draw = xfce_notify_window_draw;
    widget_class->enter_notify_event = xfce_notify_window_enter_leave;
    widget_class->leave_notify_event = xfce_notify_window_enter_leave;
    widget_class->motion_notify_event =xfce_notify_window_motion_notify;
    widget_class->button_release_event = xfce_notify_window_button_release;
    widget_class->configure_event = xfce_notify_window_configure_event;

    properties[PROP_ID] = g_param_spec_uint("id",
                                            "id",
                                            "Notification ID handle",
                                            1, G_MAXUINT, 1,
                                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    properties[PROP_MONITOR] = g_param_spec_object("monitor",
                                                   "monitor",
                                                   "Monitor the notification should be displayed on",
                                                   GDK_TYPE_MONITOR,
                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    properties[PROP_OVERRIDE_REDIRECT] = g_param_spec_boolean("override-redirect",
                                                              "override-redirect",
                                                              "Whether or not to use an override-redirect window",
                                                              FALSE,
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    properties[PROP_NORMAL_OPACITY] = g_param_spec_double("normal-opacity",
                                                          "normal-opacity",
                                                          "Opacity of the window when it is first shown",
                                                          0.0, 1.0, 1.0,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    properties[PROP_SHOW_TEXT_WITH_GAUGE] = g_param_spec_boolean("show-text-with-gauge",
                                                                 "show-text-with-gauge",
                                                                 "Whether or not to show the summary and body when a gauge is shown",
                                                                 FALSE,
                                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    properties[PROP_LOCATION] = g_param_spec_enum("location",
                                                  "location",
                                                  "Location for the initial position of notification windows",
                                                  XFCE_TYPE_NOTIFY_POSITION,
                                                  XFCE_NOTIFY_POS_TOP_RIGHT,
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

    g_object_class_install_properties(gobject_class, N_PROPS, properties);

    signals[SIG_CLOSED] = g_signal_new("closed",
                                       XFCE_TYPE_NOTIFY_WINDOW,
                                       G_SIGNAL_RUN_LAST,
                                       G_STRUCT_OFFSET(XfceNotifyWindowClass,
                                                       closed),
                                       NULL, NULL,
                                       g_cclosure_marshal_VOID__ENUM,
                                       G_TYPE_NONE, 1,
                                       XFCE_TYPE_NOTIFY_CLOSE_REASON);

    signals[SIG_NEED_POSITION] = g_signal_new("need-position",
                                              XFCE_TYPE_NOTIFY_WINDOW,
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              NULL,
                                              NULL,
                                              g_cclosure_marshal_VOID__VOID,
                                              G_TYPE_NONE,
                                              0);

    signals[SIG_ACTION_INVOKED] = g_signal_new("action-invoked",
                                               XFCE_TYPE_NOTIFY_WINDOW,
                                               G_SIGNAL_RUN_LAST,
                                               G_STRUCT_OFFSET(XfceNotifyWindowClass,
                                                               action_invoked),
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__STRING,
                                               G_TYPE_NONE,
                                               1, G_TYPE_STRING);


    gtk_widget_class_install_style_property(widget_class,
                                            g_param_spec_double("padding",
                                                                "padding",
                                                                "the padding of the text/icon to the notification's border",
                                                                0.0, 30.0,
                                                                DEFAULT_PADDING,
                                                                G_PARAM_READABLE));
}

static void
xfce_notify_window_init(XfceNotifyWindow *window)
{
    GdkScreen *screen;

    window->urgency = XFCE_NOTIFY_URGENCY_NORMAL;
    window->expire_timeout = DEFAULT_EXPIRE_TIMEOUT;
    window->normal_opacity = DEFAULT_NORMAL_OPACITY;
    window->do_fadeout = DEFAULT_DO_FADEOUT;
    window->do_slideout = DEFAULT_DO_SLIDEOUT;
    window->original_x = G_MININT;;
    window->original_y = G_MININT;
    window->op_change_steps = FADE_TIME / FADE_CHANGE_TIMEOUT;

    gtk_widget_set_name (GTK_WIDGET(window), "XfceNotifyWindow");
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
    gtk_window_stick(GTK_WINDOW(window));
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_accept_focus(GTK_WINDOW(window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(window), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(window),
                             GDK_WINDOW_TYPE_HINT_NOTIFICATION);
    gtk_container_set_border_width(GTK_CONTAINER(window), 0);
    gtk_widget_set_app_paintable(GTK_WIDGET(window), TRUE);

    gtk_widget_add_events(GTK_WIDGET(window),
                          GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                          | GDK_POINTER_MOTION_MASK);

    screen = gtk_widget_get_screen(GTK_WIDGET(window));
    if(gdk_screen_is_composited(screen)) {
        GdkVisual *visual = gdk_screen_get_rgba_visual (screen);
        if (visual == NULL)
            visual = gdk_screen_get_system_visual (screen);

        gtk_widget_set_visual (GTK_WIDGET(window), visual);
    }
}

static void
xfce_notify_window_constructed(GObject *object) {
    XfceNotifyWindow *window = XFCE_NOTIFY_WINDOW(object);
    GtkCssProvider *provider;

    G_OBJECT_CLASS(xfce_notify_window_parent_class)->constructed(object);

#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default())) {
        gtk_layer_init_for_window(GTK_WINDOW(window));
        if (window->monitor != NULL) {
            gtk_layer_set_monitor(GTK_WINDOW(window), window->monitor);
        }
        gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_namespace(GTK_WINDOW(window), "notification");

        switch (window->notify_location) {
            case XFCE_NOTIFY_POS_TOP_LEFT:
            case XFCE_NOTIFY_POS_TOP_RIGHT:
            case XFCE_NOTIFY_POS_TOP_CENTER:
                gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
                break;
            case XFCE_NOTIFY_POS_BOTTOM_LEFT:
            case XFCE_NOTIFY_POS_BOTTOM_RIGHT:
            case XFCE_NOTIFY_POS_BOTTOM_CENTER:
                gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
                break;
            default:
                g_assert_not_reached();
                break;
        }

        switch (window->notify_location) {
            case XFCE_NOTIFY_POS_TOP_LEFT:
            case XFCE_NOTIFY_POS_BOTTOM_LEFT:
                gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
                break;
            case XFCE_NOTIFY_POS_TOP_RIGHT:
            case XFCE_NOTIFY_POS_BOTTOM_RIGHT:
                gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
                break;
            case XFCE_NOTIFY_POS_TOP_CENTER:
            case XFCE_NOTIFY_POS_BOTTOM_CENTER:
                // We're not quite sure which side we're going to anchor to
                // yet, but we'll decide when we're told our position.
                break;
            default:
                g_assert_not_reached();
                break;
        }
    }
#endif

    xfce_notify_window_ensure_widgets(window);

    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (window)), "xfce4-notifyd");
    provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (provider, BASE_CSS, -1, NULL);
    gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (window)),
                                    GTK_STYLE_PROVIDER (provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref (provider);
}

static void
xfce_notify_window_start_expiration(XfceNotifyWindow *window)
{
    if (window->expire_id == 0 && window->expire_timeout > 0 && window->urgency != XFCE_NOTIFY_URGENCY_CRITICAL) {
        gint64 ct;
        guint timeout;
        gboolean fade_transparent;

        ct = g_get_real_time();

        fade_transparent =
            gdk_screen_is_composited(gtk_window_get_screen(GTK_WINDOW (window)));

        if(!fade_transparent)
            timeout = window->expire_timeout;
        else if(window->expire_timeout > FADE_TIME)
            timeout = window->expire_timeout - FADE_TIME;
        else
            timeout = FADE_TIME;

        window->expire_start_timestamp = ct / 1000;
        window->expire_id = g_timeout_add(timeout,
                                          xfce_notify_window_expire_timeout,
                                          window);
    }
    gtk_widget_set_opacity(GTK_WIDGET(window), window->normal_opacity);
}

static void
xfce_notify_window_dispose(GObject *object) {
    XfceNotifyWindow *window = XFCE_NOTIFY_WINDOW(object);

    if (window->fade_id != 0) {
        g_source_remove(window->fade_id);
        window->fade_id = 0;
    }

    if (window->expire_id != 0) {
        g_source_remove(window->expire_id);
        window->expire_id = 0;
    }

    G_OBJECT_CLASS(xfce_notify_window_parent_class)->dispose(object);
}

static void
xfce_notify_window_finalize(GObject *object)
{
    XfceNotifyWindow *window = XFCE_NOTIFY_WINDOW(object);

    if (window->monitor != NULL) {
        g_object_unref(window->monitor);
    }

    if (window->css_provider != NULL) {
        g_object_unref(window->css_provider);
    }

    G_OBJECT_CLASS(xfce_notify_window_parent_class)->finalize(object);
}

static void
xfce_notify_window_set_property(GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
    XfceNotifyWindow *window = XFCE_NOTIFY_WINDOW(object);

    switch (prop_id) {
        case PROP_ID:
            window->id = g_value_get_uint(value);
            break;

        case PROP_MONITOR:
            window->monitor = g_value_dup_object(value);
            break;

        case PROP_OVERRIDE_REDIRECT:
            window->override_redirect = g_value_get_boolean(value);
            break;

        case PROP_NORMAL_OPACITY:
            window->normal_opacity = g_value_get_double(value);
            window->op_change_delta = window->normal_opacity / window->op_change_steps;
            break;

        case PROP_SHOW_TEXT_WITH_GAUGE:
            window->show_text_with_gauge = g_value_get_boolean(value);
            break;

        case PROP_LOCATION:
            window->notify_location = g_value_get_enum(value);
            break;

        case PROP_SUMMARY:
            xfce_notify_window_set_summary(window, g_value_get_string(value));
            break;

        case PROP_BODY:
            xfce_notify_window_set_body(window, g_value_get_string(value));
            break;

        case PROP_GAUGE_VALUE:
            xfce_notify_window_set_gauge_value(window, g_value_get_uint(value));
            break;

        case PROP_GAUGE_VALUE_SET:
            xfce_notify_window_set_gauge_value_set(window, g_value_get_boolean(value));
            break;

        case PROP_ICON_ONLY:
            xfce_notify_window_set_icon_only(window, g_value_get_boolean(value));
            break;

        case PROP_ICON_NAME:
            xfce_notify_window_set_icon_name(window, g_value_get_string(value));
            break;

        case PROP_ICON_PIXBUF:
            xfce_notify_window_set_icon_pixbuf(window, g_value_get_object(value));
            break;

        case PROP_URGENCY:
            xfce_notify_window_set_urgency(window, g_value_get_enum(value));
            break;

        case PROP_EXPIRE_TIMEOUT:
            xfce_notify_window_set_expire_timeout(window, g_value_get_uint(value));
            break;

        case PROP_ACTIONS:
            xfce_notify_window_set_actions(window, g_value_get_pointer(value));
            break;

        case PROP_DO_FADEOUT:
            xfce_notify_window_set_do_fadeout(window, g_value_get_boolean(value));
            break;

        case PROP_DO_SLIDEOUT:
            xfce_notify_window_set_do_slideout(window, g_value_get_boolean(value));
            break;

        case PROP_CSS_PROVIDER:
            xfce_notify_window_set_css_provider(window, g_value_get_object(value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
xfce_notify_window_get_property(GObject *object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
    XfceNotifyWindow *window = XFCE_NOTIFY_WINDOW(object);

    switch (prop_id) {
        case PROP_ID:
            g_value_set_uint(value, window->id);
            break;

        case PROP_MONITOR:
            g_value_set_object(value, window->monitor);
            break;

        case PROP_OVERRIDE_REDIRECT:
            g_value_set_boolean(value, window->override_redirect);
            break;

        case PROP_NORMAL_OPACITY:
            g_value_set_double(value, window->normal_opacity);
            break;

        case PROP_SHOW_TEXT_WITH_GAUGE:
            g_value_set_boolean(value, window->show_text_with_gauge);
            break;

        case PROP_LOCATION:
            g_value_set_enum(value, window->notify_location);
            break;

        case PROP_SUMMARY:
            g_value_set_string(value, gtk_label_get_text(GTK_LABEL(window->summary)));
            break;

        case PROP_BODY:
            g_value_set_string(value, gtk_label_get_text(GTK_LABEL(window->body)));
            break;

        case PROP_GAUGE_VALUE:
            g_value_set_uint(value, gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(window->gauge)) * 100);
            break;

        case PROP_GAUGE_VALUE_SET:
            g_value_set_boolean(value, window->gauge_value_set);
            break;

        case PROP_ICON_ONLY:
            g_value_set_boolean(value, window->icon_only);
            break;

        case PROP_ICON_NAME:
            // FIXME: doesn't work when it's an absolute file name
            if (gtk_image_get_storage_type(GTK_IMAGE(window->icon)) == GTK_IMAGE_ICON_NAME) {
                const gchar *icon_name = NULL;
                GtkIconSize size = GTK_ICON_SIZE_INVALID;
                gtk_image_get_icon_name(GTK_IMAGE(window->icon), &icon_name, &size);
                g_value_set_string(value, icon_name);
            }
            break;

        case PROP_ICON_PIXBUF:
            // FIXME: won't work if we set via surface
            if (gtk_image_get_storage_type(GTK_IMAGE(window->icon)) == GTK_IMAGE_PIXBUF) {
                g_value_set_object(value, gtk_image_get_pixbuf(GTK_IMAGE(window->icon)));
            }
            break;

        case PROP_URGENCY:
            g_value_set_enum(value, window->urgency);
            break;

        case PROP_EXPIRE_TIMEOUT:
            g_value_set_uint(value, window->expire_timeout);
            break;

        case PROP_ACTIONS:
            g_value_set_pointer(value, window->actions);
            break;

        case PROP_CSS_PROVIDER:
            g_value_set_object(value, window->css_provider);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
xfce_notify_window_notify(GObject *object, GParamSpec *pspec) {
    if (gtk_widget_get_realized(GTK_WIDGET(object))) {
        gtk_widget_queue_draw(GTK_WIDGET(object));
    }
}

static void
xfce_notify_window_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
    GTK_WIDGET_CLASS(xfce_notify_window_parent_class)->size_allocate(widget, allocation);

    XfceNotifyWindow *window = XFCE_NOTIFY_WINDOW(widget);
    if (window->geometry.width != allocation->width || window->geometry.height != allocation->height) {
        g_signal_emit(widget, signals[SIG_NEED_POSITION], 0);
    }
}

static void
xfce_notify_window_realize(GtkWidget *widget)
{
    XfceNotifyWindow *window = XFCE_NOTIFY_WINDOW(widget);

#ifdef ENABLE_WAYLAND
    if (window->monitor != NULL && GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default())) {
        gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, window->geometry.x);
        gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, window->geometry.y);
    }
#endif

    GTK_WIDGET_CLASS(xfce_notify_window_parent_class)->realize(widget);

    gdk_window_set_override_redirect(gtk_widget_get_window(widget), window->override_redirect);

#ifdef ENABLE_X11
    // GDK includes WM_TAKE_FOCUS in WM_PROTOCOLS, even for non-focusable
    // windows, and (notably) xfwm4 will still try to give focus to the window,
    // which will cause any fullscreen windows to un-fullscreen.  The ICCCM
    // says that windows that do not take input should not advertise support
    // for WM_TAKE_FOCUS, so I believe GDK is in the wrong here.  Even if it
    // does make sense for the WM to handle this differently, our windows
    // should follow the spec!
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        Display *dpy = gdk_x11_display_get_xdisplay(gdk_display_get_default());
        Window win = gdk_x11_window_get_xid(gtk_widget_get_window(widget));
        Atom *wm_protocols = NULL;
        int n_protocols = 0;

        gdk_x11_display_error_trap_push(gdk_display_get_default());

        if (XGetWMProtocols(dpy, win, &wm_protocols, &n_protocols) != 0 && wm_protocols != NULL) {
            Atom wm_take_focus = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
            gboolean found = FALSE;

            for (int i = 0; i < n_protocols; ++i) {
                if (wm_protocols[i] == wm_take_focus) {
                    found = TRUE;
                }
                if (found && i + 1 < n_protocols) {
                    wm_protocols[i] = wm_protocols[i + 1];
                }
            }

            if (found) {
                XSetWMProtocols(dpy, win, wm_protocols, n_protocols - 1);
            }

            XFree(wm_protocols);
        }

        gdk_x11_display_error_trap_pop_ignored(gdk_display_get_default());
    }
#endif
}

static void
xfce_notify_window_show(GtkWidget *widget) {
    XfceNotifyWindow *window = XFCE_NOTIFY_WINDOW(widget);

    GTK_WIDGET_CLASS(xfce_notify_window_parent_class)->show(widget);

    if (window->monitor != NULL) {
        xfce_notify_window_start_expiration(window);
    } else {
        // If we don't know the monitor yet, we need to 'hide' the window until
        // we figure out what monitor it's on.
        gtk_widget_set_opacity(widget, 0.0);
    }
}

static void
xfce_notify_window_unrealize(GtkWidget *widget)
{
    XfceNotifyWindow *window = XFCE_NOTIFY_WINDOW(widget);

    if(window->fade_id) {
        g_source_remove(window->fade_id);
        window->fade_id = 0;
    }

    if(window->expire_id) {
        g_source_remove(window->expire_id);
        window->expire_id = 0;
    }

    GTK_WIDGET_CLASS(xfce_notify_window_parent_class)->unrealize(widget);

}

static gboolean
xfce_notify_window_draw (GtkWidget *widget,
                         cairo_t *cr)
{
    GtkStyleContext  *context;
    GtkAllocation     allocation;
    GdkScreen        *screen;
    GtkCssProvider   *provider;
    GtkStateFlags     state;
    XfceNotifyWindow *window = XFCE_NOTIFY_WINDOW (widget);

    state = GTK_STATE_FLAG_NORMAL;
    if (window->mouse_hover)
        state = GTK_STATE_FLAG_PRELIGHT;

    context = gtk_widget_get_style_context (widget);
    gtk_widget_get_allocation (widget, &allocation);

    /* Remove rounded corners when compositing is disabled */
    screen = gtk_widget_get_screen (widget);
    if (!gdk_screen_is_composited (screen)) {
        provider = gtk_css_provider_new ();
        gtk_css_provider_load_from_data (provider, NO_COMPOSITING_CSS, -1, NULL);
        gtk_style_context_add_provider (context,
                                        GTK_STYLE_PROVIDER (provider),
                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref (provider);
    }

    /* First make the window transparent */
    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
    cairo_fill (cr);

    // Translate drawing for slideout
    cairo_translate(cr, window->draw_offset_x, window->draw_offset_y);

    /* Then render the background and border based on the Gtk theme */
    gtk_style_context_set_state (context, state);
    gtk_render_background (context, cr, allocation.x, allocation.y, allocation.width, allocation.height);
    gtk_render_frame (context, cr, allocation.x, allocation.y, allocation.width, allocation.height);

    /* Then draw the rest of the window */
    GTK_WIDGET_CLASS (xfce_notify_window_parent_class)->draw (widget, cr);

    return FALSE;
}

static gboolean
xfce_notify_window_enter_leave(GtkWidget *widget,
                               GdkEventCrossing *evt)
{
    XfceNotifyWindow *window = XFCE_NOTIFY_WINDOW(widget);

    if (evt->type == GDK_LEAVE_NOTIFY && evt->detail != GDK_NOTIFY_INFERIOR) {
        if (window->expire_id == 0) {
            xfce_notify_window_start_expiration(window);
        }
        window->mouse_hover = FALSE;
        gtk_widget_queue_draw(widget);
    }

    return FALSE;
}

static gboolean
xfce_notify_window_motion_notify(GtkWidget *widget,
                                 GdkEventMotion *evt)
{
    XfceNotifyWindow *window = XFCE_NOTIFY_WINDOW(widget);

    if (window->expire_timeout != 0) {
        if (window->expire_id != 0) {
            g_source_remove(window->expire_id);
            window->expire_id = 0;
        }

        xfce_notify_window_reset_fade_and_slide(window);
    }

    if (!window->mouse_hover) {
        gtk_widget_set_opacity(GTK_WIDGET(widget), 1.0);
        window->mouse_hover = TRUE;
        gtk_widget_queue_draw(widget);
    }

    return FALSE;
}

static gboolean
xfce_notify_window_button_release(GtkWidget *widget,
                                  GdkEventButton *evt)
{
    g_signal_emit(G_OBJECT(widget), signals[SIG_CLOSED], 0,
                  XFCE_NOTIFY_CLOSE_REASON_DISMISSED);

    return FALSE;
}

static gboolean
xfce_notify_window_configure_event(GtkWidget *widget,
                                   GdkEventConfigure *evt)
{
    gboolean ret;

    ret = GTK_WIDGET_CLASS(xfce_notify_window_parent_class)->configure_event(widget,
                                                                             evt);

    gtk_widget_queue_draw(widget);

    return ret;
}

#ifdef ENABLE_WAYLAND
static gint
wayland_get_x_margin(XfceNotifyWindow *window) {
    if (gtk_layer_get_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT)) {
        return gtk_layer_get_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT);
    } else if (gtk_layer_get_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT)) {
        return gtk_layer_get_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT);
    } else {
        g_assert_not_reached();
    }
}

static gint
wayland_get_y_margin(XfceNotifyWindow *window) {
    if (gtk_layer_get_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP)) {
        return gtk_layer_get_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP);
    } else if (gtk_layer_get_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM)) {
        return gtk_layer_get_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM);
    } else {
        g_assert_not_reached();
    }
}
#endif

static gboolean
xfce_notify_window_expire_timeout(gpointer data)
{
    XfceNotifyWindow *window = data;
    gboolean          fade_transparent;
    gint              animation_timeout;

    g_return_val_if_fail(XFCE_IS_NOTIFY_WINDOW(data), FALSE);

    window->expire_id = 0;

    fade_transparent =
        gdk_screen_is_composited(gtk_window_get_screen(GTK_WINDOW(window)));

    if((fade_transparent && window->do_fadeout) || window->do_slideout) {
        if (window->fade_id == 0) {
            /* remember the original position of the window before we slide it out */
            if (window->do_slideout) {
#ifdef ENABLE_WAYLAND
                if (GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default())) {
                    window->original_x = wayland_get_x_margin(window);
                    window->original_y = wayland_get_y_margin(window);
                } else
#endif
                {
                    gtk_window_get_position (GTK_WINDOW (window), &window->original_x, &window->original_y);
                }
                animation_timeout = FADE_CHANGE_TIMEOUT / 2;
            } else {
                animation_timeout = FADE_CHANGE_TIMEOUT;
            }

            window->fade_id = g_timeout_add(animation_timeout,
                                            xfce_notify_window_fade_timeout,
                                            window);
        }
    } else {
        /* it might be 800ms early, but that's ok */
        g_signal_emit(G_OBJECT(window), signals[SIG_CLOSED], 0,
                      XFCE_NOTIFY_CLOSE_REASON_EXPIRED);
    }

    return FALSE;
}

static gboolean
xfce_notify_window_fade_timeout(gpointer data)
{
    XfceNotifyWindow *window = data;
    gboolean ret = G_SOURCE_CONTINUE;

    g_return_val_if_fail(XFCE_IS_NOTIFY_WINDOW(data), FALSE);

    /* slide out animation */
    if (window->do_slideout) {
        gint x, y;
        GdkRectangle monitor_geom;
        gboolean is_ltr = gtk_widget_get_direction(GTK_WIDGET(window)) != GTK_TEXT_DIR_RTL;
        gboolean add_pixels = window->notify_location == XFCE_NOTIFY_POS_TOP_RIGHT ||
            window->notify_location == XFCE_NOTIFY_POS_BOTTOM_RIGHT ||
            (is_ltr &&
             (window->notify_location == XFCE_NOTIFY_POS_TOP_CENTER ||
              window->notify_location == XFCE_NOTIFY_POS_BOTTOM_CENTER));

#ifdef ENABLE_WAYLAND
        if (GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default())) {
            // On Wayland we are always considering a margin, or a distance
            // from the edge of the screen, so we always subtract pixels.
            add_pixels = FALSE;
        }
#endif

        gdk_monitor_get_geometry(window->monitor, &monitor_geom);

        if (window->draw_offset_x == 0 && window->draw_offset_y == 0) {

#ifdef ENABLE_WAYLAND
            if (GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default())) {
                x = wayland_get_x_margin(window);
                y = wayland_get_y_margin(window);
            } else
#endif
            {
                gtk_window_get_position(GTK_WINDOW (window), &x, &y);
            }

            x = CLAMP(x + (add_pixels ? 10 : -10), monitor_geom.x, monitor_geom.x + monitor_geom.width - window->geometry.width);
            DBG("sliding to (%d, %d)", x, y);
            xfce_notify_window_move(window, x, y);

            // Some WMs won't let us push the window entirely off-screen, so instead we just push it
            // against the end of the monitor and then start drawing the window content offset.  As
            // a nice bonus, this also avoids the issue where if there's another monitor in the path
            // of the slideout, the notification window won't appear on that other monitor while
            // sliding.
            if (x == monitor_geom.x || x == monitor_geom.x + monitor_geom.width - window->geometry.width) {
                window->draw_offset_x = window->draw_offset_x + (add_pixels ? 1 : -1);
                gtk_widget_queue_draw(GTK_WIDGET(window));
            }
        } else {
            window->draw_offset_x = window->draw_offset_x + (add_pixels ? 10 : -10);
            gtk_widget_queue_draw(GTK_WIDGET(window));

            if (window->draw_offset_x >= window->geometry.width) {
                ret = G_SOURCE_REMOVE;
            }
        }
    }

    if (window->do_fadeout) {
        /* fade-out animation */
        gdouble op = gtk_widget_get_opacity(GTK_WIDGET(window));
        op -= window->op_change_delta;
        if (op < 0.0) {
            op = 0.0;
        }

        gtk_widget_set_opacity(GTK_WIDGET(window), op);

        if (op <= 0.0001) {
            ret = G_SOURCE_REMOVE;
        }
    }

    if (ret == G_SOURCE_REMOVE) {
        window->fade_id = 0;
        g_signal_emit(G_OBJECT(window), signals[SIG_CLOSED], 0,
                      XFCE_NOTIFY_CLOSE_REASON_EXPIRED);
    }

    return ret;
}

static void
xfce_notify_window_reset_fade_and_slide(XfceNotifyWindow *window) {
    if (window->fade_id != 0) {
        g_source_remove(window->fade_id);
        window->fade_id = 0;
    }

    if (window->original_x != G_MININT && window->original_y != G_MININT) {
        window->draw_offset_x = 0;
        window->draw_offset_y = 0;
        xfce_notify_window_move(window, window->original_x, window->original_y);
        window->original_x = G_MININT;
        window->original_y = G_MININT;
    }

    gtk_widget_queue_draw(GTK_WIDGET(window));
}

static void
xfce_notify_window_button_clicked(GtkWidget *widget,
                                  gpointer user_data)
{
    XfceNotifyWindow *window;
    gchar *action_id;

    g_return_if_fail(XFCE_IS_NOTIFY_WINDOW(user_data));

    window = XFCE_NOTIFY_WINDOW(user_data);

    action_id = g_object_get_data(G_OBJECT(widget), "--action-id");
    g_assert(action_id);

    g_signal_emit(G_OBJECT(window), signals[SIG_ACTION_INVOKED], 0,
                  action_id);
    g_signal_emit(G_OBJECT(window), signals[SIG_CLOSED], 0,
                  XFCE_NOTIFY_CLOSE_REASON_DISMISSED);
}

static void
xfce_notify_window_update_child_visibility(XfceNotifyWindow *window) {
    if (window->gauge != NULL) {
        gtk_widget_set_visible(gtk_widget_get_parent(window->gauge), window->gauge_value_set);
    }
    gtk_widget_set_visible(window->summary, window->has_summary_text && (!window->gauge_value_set || window->show_text_with_gauge) && !window->icon_only);
    gtk_widget_set_visible(window->body, window->has_body_text && (!window->gauge_value_set || window->show_text_with_gauge) && !window->icon_only);
    gtk_widget_set_visible(window->button_box, window->actions && window->actions->n_actions > 0 && !window->gauge_value_set);
}

static void
xfce_notify_window_set_summary(XfceNotifyWindow *window,
                               const gchar *summary)
{
    g_return_if_fail(XFCE_IS_NOTIFY_WINDOW(window));

    gtk_label_set_text(GTK_LABEL(window->summary), summary);
    if(summary && *summary) {
        gtk_widget_show(window->summary);
        window->has_summary_text = TRUE;
    } else {
        gtk_widget_hide(window->summary);
        window->has_summary_text = FALSE;
    }

    xfce_notify_window_update_child_visibility(window);

    g_object_notify(G_OBJECT(window), "summary");
}

static void
xfce_notify_window_set_body(XfceNotifyWindow *window,
                            const gchar *body)
{
    g_return_if_fail(XFCE_IS_NOTIFY_WINDOW(window));

    if(body && *body) {
        gchar *sanitized_body = xfce_notify_sanitize_markup(body);
        gtk_label_set_markup (GTK_LABEL (window->body), sanitized_body);
        g_free(sanitized_body);
        gtk_widget_show(window->body);
        window->has_body_text = TRUE;
    } else {
        gtk_label_set_markup(GTK_LABEL(window->body), "");
        gtk_widget_hide(window->body);
        window->has_body_text = FALSE;
    }

    xfce_notify_window_update_child_visibility(window);

    g_object_notify(G_OBJECT(window), "body");
}

static void
xfce_notify_window_set_icon_name (XfceNotifyWindow *window,
                                  const gchar *icon_name)
{
    GIcon *icon = NULL;

    g_return_if_fail (XFCE_IS_NOTIFY_WINDOW (window));

    if (icon_name && *icon_name) {
        gboolean is_absolute, is_uri;

        is_absolute = g_path_is_absolute (icon_name);
        is_uri = g_str_has_prefix(icon_name, "file://");

        if (is_absolute || is_uri) {
            GFile *file = is_absolute ? g_file_new_for_path (icon_name) : g_file_new_for_uri (icon_name);

            if (g_file_query_exists (file, NULL) && g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_REGULAR) {
                icon = g_file_icon_new (file);
            }
            g_object_unref (file);
        } else {
            icon = g_themed_icon_new_with_default_fallbacks (icon_name);
        }
    }

    if (icon != NULL) {
        gtk_image_set_from_gicon (GTK_IMAGE (window->icon), icon, GTK_ICON_SIZE_DIALOG);
        gtk_widget_show (window->icon_box);
        g_object_unref (icon);
    } else {
        if (gtk_image_get_storage_type(GTK_IMAGE(window->icon)) == GTK_IMAGE_GICON) {
            gtk_image_clear (GTK_IMAGE (window->icon));
            gtk_widget_hide (window->icon_box);
        }
    }

    xfce_notify_window_update_child_visibility(window);

    g_object_notify(G_OBJECT(window), "icon-name");
}

static void
xfce_notify_window_set_icon_pixbuf(XfceNotifyWindow *window,
                                   GdkPixbuf *pixbuf)
{
    g_return_if_fail(XFCE_IS_NOTIFY_WINDOW(window)
                     && (!pixbuf || GDK_IS_PIXBUF(pixbuf)));

    if(pixbuf) {
        GdkWindow *for_window = gtk_widget_get_window(GTK_WIDGET(window));
        gint scale_factor = gtk_widget_get_scale_factor(GTK_WIDGET(window));
        cairo_surface_t *surface;
        gint w, h, size, pw, ph;

        gtk_icon_size_lookup(GTK_ICON_SIZE_DIALOG, &w, &h);
        size = MIN(w, h) * scale_factor;
        pw = gdk_pixbuf_get_width(pixbuf);
        ph = gdk_pixbuf_get_height(pixbuf);

        if(pw > size || ph > size) {
            GdkPixbuf *pix_scaled;
            gint nw, nh;

            if(pw > ph) {
                nw = size;
                nh = size * ((gdouble)ph/pw);
            } else {
                nw = size * ((gdouble)pw/ph);
                nh = size;
            }

            pix_scaled = gdk_pixbuf_scale_simple(pixbuf, nw, nh,
                                                 GDK_INTERP_BILINEAR);
            surface = gdk_cairo_surface_create_from_pixbuf(pix_scaled, scale_factor, for_window);
            g_object_unref(pix_scaled);
        } else {
            surface = gdk_cairo_surface_create_from_pixbuf(pixbuf, scale_factor, for_window);
        }

        gtk_image_set_from_surface(GTK_IMAGE(window->icon), surface);
        gtk_widget_show(window->icon_box);
        cairo_surface_destroy(surface);
    } else {
        if (gtk_image_get_storage_type(GTK_IMAGE(window->icon)) == GTK_IMAGE_SURFACE) {
            gtk_image_clear(GTK_IMAGE(window->icon));
            gtk_widget_hide(window->icon_box);
        }
    }

    xfce_notify_window_update_child_visibility(window);

    g_object_notify(G_OBJECT(window), "icon-pixbuf");
}

static void
xfce_notify_window_set_expire_timeout(XfceNotifyWindow *window,
                                      gint expire_timeout)
{
    g_return_if_fail(XFCE_IS_NOTIFY_WINDOW(window));

    if(expire_timeout >= 0)
        window->expire_timeout = expire_timeout;
    else
        window->expire_timeout = DEFAULT_EXPIRE_TIMEOUT;

    if(gtk_widget_get_realized(GTK_WIDGET(window))) {
        if(window->expire_id) {
            g_source_remove(window->expire_id);
            window->expire_id = 0;
        }

        xfce_notify_window_reset_fade_and_slide(window);
        xfce_notify_window_start_expiration(window);
    }
}

static void
xfce_notify_window_set_urgency(XfceNotifyWindow *window,
                               XfceNotifyUrgency urgency)
{
    g_return_if_fail(XFCE_IS_NOTIFY_WINDOW(window));

    if (window->urgency != urgency) {
        if (window->urgency == XFCE_NOTIFY_URGENCY_CRITICAL) {
            // No longer critical, so start expiration
            if (window->expire_id == 0 && window->fade_id == 0 && gtk_widget_get_realized(GTK_WIDGET(window))) {
                xfce_notify_window_start_expiration(window);
            }
        }

        window->urgency = urgency;

        if (window->urgency == XFCE_NOTIFY_URGENCY_CRITICAL) {
            if (window->expire_id != 0) {
                g_source_remove(window->expire_id);
                window->expire_id = 0;
            }
            xfce_notify_window_reset_fade_and_slide(window);
        }
    }
}

static void
xfce_notify_window_set_actions(XfceNotifyWindow *window, XfceNotificationActions *actions) {

    g_return_if_fail(XFCE_IS_NOTIFY_WINDOW(window));

    if (window->actions != actions) {
        GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
        GList *children = gtk_container_get_children(GTK_CONTAINER(window->button_box));
        guint n_actions = actions != NULL ? actions->n_actions : 0;

        for(GList *l = children; l; l = l->next) {
            gtk_widget_destroy(GTK_WIDGET(l->data));
        }
        g_list_free(children);

        window->actions = actions;
        gtk_widget_set_visible(window->button_box, actions != NULL);

        for (guint i = 0; i < n_actions; ++i) {
            const gchar *cur_action_id = window->actions->actions[i].id;
            const gchar *cur_button_text = window->actions->actions[i].label;
            const gchar *icon_name = NULL;
            GtkWidget *btn, *img = NULL, *lbl;
            gdouble padding;

            if(!cur_button_text || !cur_action_id || !*cur_action_id)
                break;

            if (actions->ids_are_icon_names) {
                icon_name = cur_action_id;
            }

            if (cur_button_text == NULL || g_strcmp0 (cur_button_text, "") == 0) {
                // Actions with no label are intended to be the 'default' action; the spec
                // suggests that clicking the notification will activate the default action,
                // but we just close them on click, so let's create a button for it.
                if (icon_name == NULL) {
                    icon_name = "emblem-default-symbolic";
                }
            }

            gtk_widget_style_get(GTK_WIDGET(window),
                                 "padding", &padding,
                                 NULL);
            btn = gtk_button_new();
            g_object_set_data_full(G_OBJECT(btn), "--action-id",
                                   g_strdup(cur_action_id),
                                   (GDestroyNotify)g_free);
            gtk_widget_show(btn);
            gtk_widget_set_margin_top (btn, padding / 2);
            gtk_container_add(GTK_CONTAINER(window->button_box), btn);
            g_signal_connect(G_OBJECT(btn), "clicked",
                             G_CALLBACK(xfce_notify_window_button_clicked),
                             window);

            if (icon_name != NULL && gtk_icon_theme_has_icon(icon_theme, icon_name)) {
                GIcon *icon = g_themed_icon_new_with_default_fallbacks(icon_name);
                const gchar *desc;

                if (cur_button_text == NULL || g_strcmp0 (cur_button_text, "") == 0) {
                    desc = _("Default Action");
                } else {
                    desc = cur_button_text;
                }

                img = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_BUTTON);
                gtk_widget_set_tooltip_text(img, cur_button_text);
                atk_object_set_description(gtk_widget_get_accessible(img), desc);
                gtk_widget_show(img);
                gtk_container_add(GTK_CONTAINER(btn), img);

                gtk_widget_set_tooltip_text(btn, desc);

                g_object_unref(icon);
            }

            if (img == NULL) {
                gchar *cur_button_text_escaped = g_markup_printf_escaped("<span size='small'>%s</span>",
                                                                         cur_button_text);

                lbl = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(lbl), cur_button_text_escaped);
                gtk_label_set_use_markup(GTK_LABEL(lbl), TRUE);
                gtk_widget_show(lbl);
                gtk_container_add(GTK_CONTAINER(btn), lbl);
                if (window->css_provider != NULL) {
                    gtk_style_context_add_provider(gtk_widget_get_style_context(btn),
                                                   GTK_STYLE_PROVIDER(window->css_provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
                }

                g_free(cur_button_text_escaped);
            }
        }

        xfce_notify_window_update_child_visibility(window);

        g_object_notify(G_OBJECT(window), "actions");
    }
}

static void
xfce_notify_window_set_icon_only(XfceNotifyWindow *window,
                                 gboolean icon_only)
{
    g_return_if_fail(XFCE_IS_NOTIFY_WINDOW(window));

    if(icon_only == window->icon_only)
        return;

    window->icon_only = !!icon_only;

    if(icon_only) {
        GtkRequisition req;

        if(!gtk_widget_get_visible(window->icon_box)) {
            g_warning("Attempt to set icon-only mode with no icon");
            return;
        }

        gtk_widget_hide(window->content_box);

        /* set a wider size on the icon box so it takes up more space */
        gtk_widget_realize(window->icon);
        gtk_widget_get_preferred_size (window->icon, NULL, &req);
        gtk_widget_set_size_request(window->icon_box, req.width * 4, -1);
        /* and center it */
        g_object_set (window->icon_box,
                      "halign", GTK_ALIGN_CENTER,
                      NULL);
    } else {
        g_object_set (window->icon_box,
                      "halign", GTK_ALIGN_START,
                      NULL);
        gtk_widget_set_size_request(window->icon_box, -1, -1);
        gtk_widget_show(window->content_box);
    }

    xfce_notify_window_update_child_visibility(window);

    g_object_notify(G_OBJECT(window), "icon-only");
}

static void
xfce_notify_window_set_gauge_value(XfceNotifyWindow *window, guint value) {
    g_return_if_fail(XFCE_IS_NOTIFY_WINDOW(window));
    g_return_if_fail(value <= 100);

    if (window->gauge == NULL) {
        GtkWidget *box;
        gint width;

        if(gtk_widget_get_visible(window->icon)) {
            /* size the pbar in relation to the icon */
            GtkRequisition req;

            gtk_widget_realize(window->icon);
            gtk_widget_get_preferred_size(window->icon, NULL, &req);
            width = req.width * 4;
        } else {
            /* FIXME: do something less arbitrary */
            width = 120;
        }

        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_show(box);

        g_object_set(box, "valign", GTK_ALIGN_CENTER, NULL);
        gtk_box_pack_start(GTK_BOX(window->content_box), box, TRUE, TRUE, 0);

        window->gauge = gtk_progress_bar_new();
        gtk_widget_set_size_request(window->gauge, width, -1);
        gtk_widget_show(window->gauge);
        gtk_container_add(GTK_CONTAINER(box), window->gauge);
        if (window->css_provider != NULL) {
            gtk_style_context_add_provider(gtk_widget_get_style_context(window->gauge),
                                           GTK_STYLE_PROVIDER(window->css_provider),
                                           GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        }
    }

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(window->gauge),
                                  value / 100.0);

    xfce_notify_window_update_child_visibility(window);

    g_object_notify(G_OBJECT(window), "gauge-value");
}

static void
xfce_notify_window_set_gauge_value_set(XfceNotifyWindow *window, gboolean gauge_value_set) {
    g_return_if_fail(XFCE_IS_NOTIFY_WINDOW(window));

    if (window->gauge_value_set != gauge_value_set) {
        window->gauge_value_set = gauge_value_set;
        xfce_notify_window_update_child_visibility(window);
        g_object_notify(G_OBJECT(window), "gauge-value-set");
    }
}

static void
xfce_notify_window_set_css_provider(XfceNotifyWindow *window, GtkCssProvider *css_provider) {
    if (window->css_provider != css_provider) {
        g_clear_object(&window->css_provider);
        if (css_provider != NULL) {
            window->css_provider = g_object_ref(css_provider);
            // TODO: apply to widgets
        }
        g_object_notify(G_OBJECT(window), "css-provider");
    }
}

static void
xfce_notify_window_set_do_fadeout(XfceNotifyWindow *window, gboolean do_fadeout) {
    if (window->do_fadeout != do_fadeout && window->fade_id == 0) {
        window->do_fadeout = do_fadeout;
        g_object_notify(G_OBJECT(window), "do-fadeout");
    }
}

static void
xfce_notify_window_set_do_slideout(XfceNotifyWindow *window, gboolean do_slideout) {
    if (window->do_slideout != do_slideout && window->fade_id == 0) {
        window->do_slideout = do_slideout;
        g_object_notify(G_OBJECT(window), "do-slideout");
    }
}

static void
xfce_notify_window_move(XfceNotifyWindow *window, gint x, gint y) {
    DBG("entering, (%d, %d)", x, y);

#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        gtk_window_move(GTK_WINDOW(window), x, y);
    }
#endif

#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default())) {
        // Only two anchors are actually set, one for x and one for y, but it
        // doesn't hurt to set them all, and this way we don't have to check.
        gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, x);
        gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, x);
        gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, y);
        gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, y);
    }
#endif
}

static void
xfce_notify_window_ensure_widgets(XfceNotifyWindow *window) {
    if (window->icon == NULL) {
        GtkWidget *topvbox, *tophbox, *vbox;
        gdouble padding = DEFAULT_PADDING;

        gtk_widget_style_get(GTK_WIDGET(window),
                             "padding", &padding,
                             NULL);

        topvbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_set_homogeneous (GTK_BOX (topvbox), FALSE);
        gtk_container_set_border_width (GTK_CONTAINER(topvbox), padding);
        gtk_widget_show (topvbox);
        gtk_container_add (GTK_CONTAINER(window), topvbox);
        tophbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_set_homogeneous (GTK_BOX (tophbox), FALSE);
        gtk_widget_show (tophbox);
        gtk_container_add (GTK_CONTAINER(topvbox), tophbox);

        window->icon_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width(GTK_CONTAINER(window->icon_box), 0);
        gtk_widget_set_margin_end (GTK_WIDGET (window->icon_box), padding);

        gtk_box_pack_start(GTK_BOX(tophbox), window->icon_box, FALSE, TRUE, 0);

        window->icon = gtk_image_new();
        gtk_widget_show(window->icon);
        gtk_container_add(GTK_CONTAINER(window->icon_box), window->icon);

        window->content_box = vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_set_homogeneous(GTK_BOX (vbox), FALSE);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
        gtk_widget_show(vbox);
        gtk_box_pack_start(GTK_BOX(tophbox), vbox, TRUE, TRUE, 0);

        window->summary = gtk_label_new(NULL);
        gtk_widget_set_name (window->summary, "summary");
        gtk_label_set_ellipsize (GTK_LABEL (window->summary), PANGO_ELLIPSIZE_END);
        gtk_label_set_line_wrap (GTK_LABEL(window->summary), TRUE);
        gtk_label_set_line_wrap_mode (GTK_LABEL (window->summary), PANGO_WRAP_WORD_CHAR);
        gtk_label_set_lines (GTK_LABEL (window->summary), 1);
        gtk_widget_set_halign (window->summary, GTK_ALIGN_FILL);
        gtk_label_set_xalign (GTK_LABEL(window->summary), 0);
        gtk_widget_set_valign (window->summary, GTK_ALIGN_BASELINE);
        gtk_box_pack_start(GTK_BOX(vbox), window->summary, TRUE, TRUE, 0);

        window->body = gtk_label_new(NULL);
        gtk_widget_set_name (window->body, "body");
        gtk_label_set_ellipsize (GTK_LABEL (window->body), PANGO_ELLIPSIZE_END);
        gtk_label_set_line_wrap (GTK_LABEL(window->body), TRUE);
        gtk_label_set_line_wrap_mode (GTK_LABEL (window->body), PANGO_WRAP_WORD_CHAR);
        gtk_label_set_lines (GTK_LABEL (window->body), 6);
        gtk_widget_set_halign (window->body, GTK_ALIGN_FILL);
        gtk_label_set_xalign (GTK_LABEL(window->body), 0);
        gtk_widget_set_valign (window->body, GTK_ALIGN_BASELINE);
        gtk_box_pack_start(GTK_BOX(vbox), window->body, TRUE, TRUE, 0);

        window->button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_button_box_set_layout(GTK_BUTTON_BOX(window->button_box),
                                  GTK_BUTTONBOX_END);
        gtk_box_set_spacing (GTK_BOX(window->button_box), padding / 2);
        gtk_box_set_homogeneous (GTK_BOX(window->button_box), FALSE);
        gtk_box_pack_end (GTK_BOX(topvbox), window->button_box, FALSE, FALSE, 0);
    }

    if (window->monitor != NULL) {
        GdkRectangle geometry;
        gint screen_width;

        /* Use the monitor width to get a maximum width for the notification bubble.
           This assumes that a character is 10px wide and we want a third of the
           monitor as maximum width. */
        gdk_monitor_get_geometry(window->monitor, &geometry);
        screen_width = geometry.width / 30;

        gtk_label_set_max_width_chars(GTK_LABEL(window->summary), screen_width);
        gtk_label_set_max_width_chars(GTK_LABEL(window->body), screen_width);
    }
}


GtkWidget *
xfce_notify_window_new(guint id,
                       GdkMonitor *monitor,
                       gboolean override_redirect,
                       XfceNotifyPosition location,
                       gdouble normal_opacity,
                       gboolean show_text_with_gauge)
{
    return g_object_new(XFCE_TYPE_NOTIFY_WINDOW,
                        "type", GTK_WINDOW_TOPLEVEL,
                        "id", id,
                        "monitor", monitor,
                        "override-redirect", override_redirect,
                        "normal-opacity", normal_opacity,
                        "show-text-with-gauge", show_text_with_gauge,
                        "location", location,
                        NULL);
}

guint
xfce_notify_window_get_id(XfceNotifyWindow *window) {
    g_return_val_if_fail(XFCE_IS_NOTIFY_WINDOW(window), 0);
    return window->id;
}

GdkMonitor *
xfce_notify_window_get_monitor(XfceNotifyWindow *window) {
    g_return_val_if_fail(XFCE_IS_NOTIFY_WINDOW(window), NULL);
    return window->monitor;
}

void
xfce_notify_window_update_monitor(XfceNotifyWindow *window, GdkMonitor *monitor) {
    g_return_if_fail(XFCE_IS_NOTIFY_WINDOW(window));
    g_return_if_fail(GDK_IS_MONITOR(monitor));
    g_return_if_fail(window->monitor == NULL);

    window->monitor = g_object_ref(monitor);
    xfce_notify_window_ensure_widgets(window);
    g_object_notify(G_OBJECT(window), "monitor");

    xfce_notify_window_start_expiration(window);
}

void
xfce_notify_window_set_geometry(XfceNotifyWindow *window, GdkRectangle *rectangle, GdkRectangle *monitor_workarea) {
    g_return_if_fail(XFCE_IS_NOTIFY_WINDOW(window));

    gint pos_x, pos_y;

#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        switch (window->notify_location) {
            case XFCE_NOTIFY_POS_TOP_LEFT:
                pos_x = monitor_workarea->x + rectangle->x;
                pos_y = monitor_workarea->y + rectangle->y;
                break;

            case XFCE_NOTIFY_POS_TOP_RIGHT:
                pos_x = monitor_workarea->x + monitor_workarea->width - rectangle->x - rectangle->width;
                pos_y = monitor_workarea->y + rectangle->y;
                break;

            case XFCE_NOTIFY_POS_BOTTOM_LEFT:
                pos_x = monitor_workarea->x + rectangle->x;
                pos_y = monitor_workarea->y + monitor_workarea->height - rectangle->y - rectangle->height;
                break;

            case XFCE_NOTIFY_POS_BOTTOM_RIGHT:
                pos_x = monitor_workarea->x + monitor_workarea->width - rectangle->x - rectangle->width;
                pos_y = monitor_workarea->y + monitor_workarea->height - rectangle->y - rectangle->height;
                break;

            case XFCE_NOTIFY_POS_TOP_CENTER:
            case XFCE_NOTIFY_POS_BOTTOM_CENTER: {
                gint half_width = monitor_workarea->width / 2;
                gboolean is_second_half = rectangle->x >= half_width;
                gboolean is_ltr = gtk_widget_get_direction(GTK_WIDGET(window)) != GTK_TEXT_DIR_RTL;
                if (!is_second_half) {
                    // Place to the right (or left, if RTL) of the center.
                    if (is_ltr) {
                        pos_x = monitor_workarea->x + half_width + rectangle->x;
                    } else {
                        pos_x = monitor_workarea->x + monitor_workarea->width - rectangle->x - rectangle->width - half_width;
                    }
                } else {
                    // The right (or left, if RTL) half of the screen is full, so
                    // place on the left (or right, if RTL) side.
                    if (is_ltr) {
                        pos_x = monitor_workarea->x + monitor_workarea->width - rectangle->x - rectangle->width;
                    } else {
                        pos_x = monitor_workarea->x + rectangle->x;
                    }
                }

                if (window->notify_location == XFCE_NOTIFY_POS_TOP_CENTER) {
                    pos_y = monitor_workarea->y + rectangle->y;
                } else {
                    pos_y = monitor_workarea->y + monitor_workarea->height - rectangle->y - rectangle->height;
                }

                break;
            }

            default:
                g_assert_not_reached();
        }
    } else
#endif
#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default())) {
        switch (window->notify_location) {
            case XFCE_NOTIFY_POS_TOP_LEFT:
            case XFCE_NOTIFY_POS_TOP_RIGHT:
            case XFCE_NOTIFY_POS_BOTTOM_LEFT:
            case XFCE_NOTIFY_POS_BOTTOM_RIGHT:
                // Since the daemon is giving us positioning that's agnostic of
                // the origin point for the notifications, and we've anchored
                // the window in the appropriate origin corner, we can simply
                // use the untranslated coordinates.
                pos_x = rectangle->x;
                break;

            case XFCE_NOTIFY_POS_TOP_CENTER:
            case XFCE_NOTIFY_POS_BOTTOM_CENTER: {
                // We can use the untranslated y coordinate as above, but we
                // need to move things around in the x direction so that we
                // start in the center, and then "wrap" properly.  Compositors
                // implementing wlr-layer-shell don't necessarily support
                // negative margins, so instead of "non-anchoring" to the
                // center, we have to calculate positions from one of the
                // sides.
                gboolean is_ltr = gtk_widget_get_direction(GTK_WIDGET(window)) != GTK_TEXT_DIR_RTL;
                if (rectangle->x + rectangle->width < monitor_workarea->width / 2) {
                    // First half of the monitor, so anchor to the left (or
                    // right, for RTL) and then add half the monitor width so
                    // it's placed in the center.
                    gtk_layer_set_anchor(GTK_WINDOW(window),
                                         is_ltr ? GTK_LAYER_SHELL_EDGE_LEFT : GTK_LAYER_SHELL_EDGE_RIGHT,
                                         TRUE);
                    pos_x = rectangle->x + monitor_workarea->width / 2;
                } else {
                    // Second half of the monitor, so anchor to the right (or
                    // left, for RTL).  We don't need to modify the x
                    // coordinate, as it's already set past the halfway point
                    // on the monitor.
                    gtk_layer_set_anchor(GTK_WINDOW(window),
                                         is_ltr ? GTK_LAYER_SHELL_EDGE_RIGHT : GTK_LAYER_SHELL_EDGE_LEFT,
                                         TRUE);
                    pos_x = rectangle->x;
                }
                break;
            }

            default:
                g_assert_not_reached();
        }

        pos_y = rectangle->y;
    } else
#endif
    {
        g_assert_not_reached();
    }

    window->geometry = *rectangle;
    DBG("Translated position: (%d,%d)", pos_x, pos_y);
    xfce_notify_window_move(window, pos_x, pos_y);
}

GdkRectangle *
xfce_notify_window_get_geometry(XfceNotifyWindow *window) {
   return &window->geometry;
}
