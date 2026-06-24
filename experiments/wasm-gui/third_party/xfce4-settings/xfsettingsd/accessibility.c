/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  XKB Extension code taken from the original mcs-keyboard-plugin written
 *  by Olivier Fourdan.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif /* !HAVE_LIBNOTIFY */

#include "debug.h"
#include "accessibility.h"



#define SET_FLAG(mask,flag)   G_STMT_START{ ((mask) |= (flag)); }G_STMT_END
#define UNSET_FLAG(mask,flag) G_STMT_START{ ((mask) &= ~(flag)); }G_STMT_END
#define HAS_FLAG(mask,flag)   (((mask) & (flag)) != 0)



static void            xfce_accessibility_helper_finalize                       (GObject                      *object);
static void            xfce_accessibility_helper_set_xkb                        (XfceAccessibilityHelper      *helper,
                                                                                 gulong                        mask);
static void            xfce_accessibility_helper_channel_property_changed       (XfconfChannel                *channel,
                                                                                 const gchar                  *property_name,
                                                                                 const GValue                 *value,
                                                                                 XfceAccessibilityHelper      *helper);
#ifdef HAVE_LIBNOTIFY
static GdkFilterReturn xfce_accessibility_helper_event_filter                   (GdkXEvent                    *xevent,
                                                                                 GdkEvent                     *gdk_event,
                                                                                 gpointer                      user_data);
static void            xfce_accessibility_helper_notification_closed            (NotifyNotification           *notification,
                                                                                 XfceAccessibilityHelper      *helper);
static void            xfce_accessibility_helper_notification_show              (XfceAccessibilityHelper      *helper,
                                                                                 const gchar                  *summary,
                                                                                 const gchar                  *body);
#endif /* !HAVE_LIBNOTIFY */



struct _XfceAccessibilityHelperClass
{
    GObjectClass __parent__;
};

struct _XfceAccessibilityHelper
{
    GObject  __parent__;

    /* xfconf channel */
    XfconfChannel      *channel;

#ifdef HAVE_LIBNOTIFY
    NotifyNotification *notification;
#endif /* !HAVE_LIBNOTIFY */
};



G_DEFINE_TYPE (XfceAccessibilityHelper, xfce_accessibility_helper, G_TYPE_OBJECT);



static void
xfce_accessibility_helper_class_init (XfceAccessibilityHelperClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_accessibility_helper_finalize;
}



static void
xfce_accessibility_helper_init (XfceAccessibilityHelper *helper)
{
    gint dummy;

    helper->channel = NULL;
#ifdef HAVE_LIBNOTIFY
    helper->notification = NULL;
#endif /* !HAVE_LIBNOTIFY */

    if (XkbQueryExtension (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), &dummy, &dummy, &dummy, &dummy, &dummy))
    {
        /* open the channel */
        helper->channel = xfconf_channel_get ("accessibility");

        /* monitor channel changes */
        g_signal_connect (G_OBJECT (helper->channel), "property-changed", G_CALLBACK (xfce_accessibility_helper_channel_property_changed), helper);

        /* restore the xbd configuration */
        xfce_accessibility_helper_set_xkb (helper, XkbStickyKeysMask | XkbSlowKeysMask | XkbBounceKeysMask | XkbMouseKeysMask | XkbAccessXKeysMask);

#ifdef HAVE_LIBNOTIFY
        /* setup a connection with the notification daemon */
        if (!notify_init ("xfce4-settings-helper"))
            g_critical ("Failed to connect to the notification daemon.");

        /* add event filter */
        XkbSelectEvents (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), XkbUseCoreKbd, XkbControlsNotifyMask, XkbControlsNotifyMask);

        /* monitor all window events */
        gdk_window_add_filter (NULL, xfce_accessibility_helper_event_filter, helper);
#endif /* !HAVE_LIBNOTIFY */
    }
    else
    {
        /* warning */
        g_critical ("Failed to initialize the Accessibility extension.");
    }
}



static void
xfce_accessibility_helper_finalize (GObject *object)
{
#ifdef HAVE_LIBNOTIFY
    XfceAccessibilityHelper *helper = XFCE_ACCESSIBILITY_HELPER (object);

    /* close an opened notification */
    if (G_UNLIKELY (helper->notification))
        notify_notification_close (helper->notification, NULL);
#endif /* !HAVE_LIBNOTIFY */

    (*G_OBJECT_CLASS (xfce_accessibility_helper_parent_class)->finalize) (object);
}



static void
xfce_accessibility_helper_set_xkb (XfceAccessibilityHelper *helper,
                                   gulong                   mask)
{

    XkbDescPtr xkb;
    gint       delay, interval, time_to_max;
    gint       max_speed, curve;

    gdk_x11_display_error_trap_push (gdk_display_get_default ());

    /* allocate */
    xkb = XkbAllocKeyboard ();
    if (G_LIKELY (xkb))
    {
        /* we always change this, so add it to the mask */
        SET_FLAG (mask, XkbControlsEnabledMask);

        /* if setting sticky keys, we set expiration too */
        if (HAS_FLAG (mask, XkbStickyKeysMask) ||
                HAS_FLAG (mask, XkbSlowKeysMask) ||
                HAS_FLAG (mask, XkbBounceKeysMask) ||
                HAS_FLAG (mask, XkbMouseKeysMask) ||
                HAS_FLAG (mask, XkbAccessXKeysMask))
          SET_FLAG (mask, XkbAccessXTimeoutMask);

        /* add the mouse keys values mask if needed */
        if (HAS_FLAG (mask, XkbMouseKeysMask))
            SET_FLAG (mask, XkbMouseKeysAccelMask);

        /* load the xkb controls into the structure */
        XkbGetControls (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), mask, xkb);

        /* AccessXKeys */
        if (HAS_FLAG (mask, XkbAccessXKeysMask))
        {
            if (xfconf_channel_get_bool (helper->channel, "/AccessXKeys", FALSE))
            {
                SET_FLAG (xkb->ctrls->enabled_ctrls, XkbAccessXKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_mask, XkbAccessXKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_values, XkbAccessXKeysMask);

                xfsettings_dbg (XFSD_DEBUG_ACCESSIBILITY, "AccessXKeys enabled");
            }
            else
            {
                UNSET_FLAG (xkb->ctrls->enabled_ctrls, XkbAccessXKeysMask);
                SET_FLAG (xkb->ctrls->axt_ctrls_mask, XkbAccessXKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_values, XkbAccessXKeysMask);

                xfsettings_dbg (XFSD_DEBUG_ACCESSIBILITY, "AccessXKeys disabled");
            }
        }

        /* Sticky keys */
        if (HAS_FLAG (mask, XkbStickyKeysMask))
        {
            if (xfconf_channel_get_bool (helper->channel, "/StickyKeys", FALSE))
            {
                SET_FLAG (xkb->ctrls->enabled_ctrls, XkbStickyKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_mask, XkbStickyKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_values, XkbStickyKeysMask);

                if (xfconf_channel_get_bool (helper->channel, "/StickyKeys/LatchToLock", FALSE))
                    SET_FLAG (xkb->ctrls->ax_options, XkbAX_LatchToLockMask);
                else
                    UNSET_FLAG (xkb->ctrls->ax_options, XkbAX_LatchToLockMask);

                if (xfconf_channel_get_bool (helper->channel, "/StickyKeys/TwoKeysDisable", FALSE))
                    SET_FLAG (xkb->ctrls->ax_options, XkbAX_TwoKeysMask);
                else
                    UNSET_FLAG (xkb->ctrls->ax_options, XkbAX_TwoKeysMask);

                xfsettings_dbg (XFSD_DEBUG_ACCESSIBILITY, "stickykeys enabled (ax_options=%d)",
                                xkb->ctrls->ax_options);
            }
            else
            {
                UNSET_FLAG (xkb->ctrls->enabled_ctrls, XkbStickyKeysMask);
                SET_FLAG (xkb->ctrls->axt_ctrls_mask, XkbStickyKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_values, XkbStickyKeysMask);

                xfsettings_dbg (XFSD_DEBUG_ACCESSIBILITY, "stickykeys disabled");
            }
        }

        /* Slow keys */
        if (HAS_FLAG (mask, XkbSlowKeysMask))
        {
            if (xfconf_channel_get_bool (helper->channel, "/SlowKeys", FALSE))
            {
                SET_FLAG (xkb->ctrls->enabled_ctrls, XkbSlowKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_mask, XkbSlowKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_values, XkbSlowKeysMask);

                delay = xfconf_channel_get_int (helper->channel, "/SlowKeys/Delay", 100);
                xkb->ctrls->slow_keys_delay = CLAMP (delay, 1, G_MAXUSHORT);

                xfsettings_dbg (XFSD_DEBUG_ACCESSIBILITY, "slowkeys enabled (delay=%d)",
                                xkb->ctrls->slow_keys_delay);
            }
            else
            {
                UNSET_FLAG (xkb->ctrls->enabled_ctrls, XkbSlowKeysMask);
                SET_FLAG (xkb->ctrls->axt_ctrls_mask, XkbSlowKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_values, XkbSlowKeysMask);

                xfsettings_dbg (XFSD_DEBUG_ACCESSIBILITY, "slowkeys disabled");
            }
        }

        /* Bounce keys */
        if (HAS_FLAG (mask, XkbBounceKeysMask))
        {
            if (xfconf_channel_get_bool (helper->channel, "/BounceKeys", FALSE))
            {
                SET_FLAG (xkb->ctrls->enabled_ctrls, XkbBounceKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_mask, XkbBounceKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_values, XkbBounceKeysMask);

                delay = xfconf_channel_get_int (helper->channel, "/BounceKeys/Delay", 100);
                xkb->ctrls->debounce_delay = CLAMP (delay, 1, G_MAXUSHORT);

                xfsettings_dbg (XFSD_DEBUG_ACCESSIBILITY, "bouncekeys enabled (delay=%d)",
                                xkb->ctrls->debounce_delay);
            }
            else
            {
                UNSET_FLAG (xkb->ctrls->enabled_ctrls, XkbBounceKeysMask);
                SET_FLAG (xkb->ctrls->axt_ctrls_mask, XkbBounceKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_values, XkbBounceKeysMask);

                xfsettings_dbg (XFSD_DEBUG_ACCESSIBILITY, "bouncekeys disabled");
            }
        }

        /* Mouse keys */
        if (HAS_FLAG (mask, XkbMouseKeysMask))
        {
            if (xfconf_channel_get_bool (helper->channel, "/MouseKeys", FALSE))
            {
                SET_FLAG (xkb->ctrls->enabled_ctrls, XkbMouseKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_mask, XkbMouseKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_values, XkbMouseKeysMask);

                /* get values */
                delay = xfconf_channel_get_int (helper->channel, "/MouseKeys/Delay", 160);
                interval = xfconf_channel_get_int (helper->channel, "/MouseKeys/Interval", 20);
                time_to_max = xfconf_channel_get_int (helper->channel, "/MouseKeys/TimeToMax", 3000);
                max_speed = xfconf_channel_get_int (helper->channel, "/MouseKeys/MaxSpeed", 1000);
                curve = xfconf_channel_get_int (helper->channel, "/MouseKeys/Curve", 0);

                /* calculate maximum speed and to to reach it */
                interval = CLAMP (interval, 1, G_MAXUSHORT);
                max_speed = (max_speed * interval) / 1000;
                time_to_max = (time_to_max + interval / 2) / interval;

                /* set new values, clamp to limits */
                xkb->ctrls->mk_delay = CLAMP (delay, 1, G_MAXUSHORT);
                xkb->ctrls->mk_interval = interval;
                xkb->ctrls->mk_time_to_max = CLAMP (time_to_max, 1, G_MAXUSHORT);
                xkb->ctrls->mk_max_speed = CLAMP (max_speed, 1, G_MAXUSHORT);
                xkb->ctrls->mk_curve = CLAMP (curve, -1000, 1000);

                xfsettings_dbg (XFSD_DEBUG_ACCESSIBILITY, "mousekeys enabled (delay=%d, interval=%d, "
                                "time_to_max=%d, max_speed=%d, curve=%d)",
                                xkb->ctrls->mk_delay, xkb->ctrls->mk_interval,
                                xkb->ctrls->mk_time_to_max, xkb->ctrls->mk_max_speed,
                                xkb->ctrls->mk_curve);
            }
            else
            {
                UNSET_FLAG (xkb->ctrls->enabled_ctrls, XkbMouseKeysMask);
                SET_FLAG (xkb->ctrls->axt_ctrls_mask, XkbMouseKeysMask);
                UNSET_FLAG (xkb->ctrls->axt_ctrls_values, XkbMouseKeysMask);
                UNSET_FLAG (mask, XkbMouseKeysAccelMask);

                xfsettings_dbg (XFSD_DEBUG_ACCESSIBILITY, "mousekeys disabled");
            }
        }

        /* set the modified controls */
        if (!XkbSetControls (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), mask, xkb))
            g_message ("Setting the xkb controls failed");

        /* free the structure */
        XkbFreeControls (xkb, mask, True);
        XFree (xkb);
    }
    else
    {
        /* warning */
        g_critical ("XkbAllocKeyboard() returned a null pointer");
    }

    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
       g_critical ("Failed to set keyboard controls");
}



static void
xfce_accessibility_helper_channel_property_changed (XfconfChannel           *channel,
                                                    const gchar             *property_name,
                                                    const GValue            *value,
                                                    XfceAccessibilityHelper *helper)
{
    gulong mask;

    g_return_if_fail (helper->channel == channel);

    if (strncmp (property_name, "/StickyKeys", 11) == 0)
        mask = XkbStickyKeysMask;
    else if (strncmp (property_name, "/SlowKeys", 9) == 0)
        mask = XkbSlowKeysMask;
    else if (strncmp (property_name, "/BounceKeys", 11) == 0)
        mask = XkbBounceKeysMask;
    else if (strncmp (property_name, "/MouseKeys", 10) == 0)
        mask = XkbMouseKeysMask;
    else
        return;

    /* update the xkb settings */
    xfce_accessibility_helper_set_xkb (helper, mask);
}


#ifdef HAVE_LIBNOTIFY
static GdkFilterReturn
xfce_accessibility_helper_event_filter (GdkXEvent *xevent,
                                        GdkEvent  *gdk_event,
                                        gpointer   user_data)
{
    XkbEvent                *event = xevent;
    XfceAccessibilityHelper *helper = XFCE_ACCESSIBILITY_HELPER (user_data);
    const gchar             *body;

    switch (event->any.xkb_type)
    {
        case XkbControlsNotify:
            if (HAS_FLAG (event->ctrls.enabled_ctrl_changes, XkbStickyKeysMask))
            {
                if (HAS_FLAG (event->ctrls.enabled_ctrls, XkbStickyKeysMask))
                    body = _("Sticky keys are enabled");
                else
                    body = _("Sticky keys are disabled");

                xfce_accessibility_helper_notification_show (helper, _("Sticky keys"), body);
            }
            else if (HAS_FLAG (event->ctrls.enabled_ctrl_changes, XkbSlowKeysMask))
            {
                if (HAS_FLAG (event->ctrls.enabled_ctrls, XkbSlowKeysMask))
                    body = _("Slow keys are enabled");
                else
                    body = _("Slow keys are disabled");

                xfce_accessibility_helper_notification_show (helper, _("Slow keys"), body);
            }
            else if (HAS_FLAG (event->ctrls.enabled_ctrl_changes, XkbBounceKeysMask))
            {
                if (HAS_FLAG (event->ctrls.enabled_ctrls, XkbBounceKeysMask))
                    body = _("Bounce keys are enabled");
                else
                    body = _("Bounce keys are disabled");

                xfce_accessibility_helper_notification_show (helper, _("Bounce keys"), body);
            }

            break;

        default:
            break;
    }

    return GDK_FILTER_CONTINUE;
}



static void
xfce_accessibility_helper_notification_closed (NotifyNotification      *notification,
                                               XfceAccessibilityHelper *helper)
{
    g_return_if_fail (helper->notification == notification);

    /* set to null */
    helper->notification = NULL;
}



static void
xfce_accessibility_helper_notification_show (XfceAccessibilityHelper *helper,
                                             const gchar             *summary,
                                             const gchar             *body)
{
    /* early leave the avoid dbus errors, we already
     * told we were unable to connect during init */
    if (notify_is_initted () == FALSE)
        return;

    /* close the running notification */
    if (helper->notification == NULL)
    {
        /* create a new notification */
#ifdef NOTIFY_CHECK_VERSION
#if NOTIFY_CHECK_VERSION (0, 7, 0)
        helper->notification = notify_notification_new (summary, body, "keyboard");
#else
        helper->notification = notify_notification_new (summary, body, "keyboard", NULL);
#endif
#else
        helper->notification = notify_notification_new (summary, body, "keyboard", NULL);
#endif

        /* don't log notification */
        notify_notification_set_hint (helper->notification, "transient", g_variant_new_boolean (FALSE));

        /* close signal */
        g_signal_connect (G_OBJECT (helper->notification), "closed", G_CALLBACK (xfce_accessibility_helper_notification_closed), helper);
    }
    else
    {
        /* update the current notification */
        notify_notification_update (helper->notification, summary, body, "keyboard");
    }

    if (G_LIKELY (helper->notification))
    {
        /* show the notification for (another) 2 seconds */
        notify_notification_set_timeout (helper->notification, 2000);

        /* show the notification */
        if (!notify_notification_show (helper->notification, NULL))
        {
            /* show warning with the notification information */
            g_warning ("Failed to show notification: %s (%s).", summary, body);

            /* failed to show the notification */
            notify_notification_close (helper->notification, NULL);
            helper->notification = NULL;
        }
    }
}
#endif /* !HAVE_LIBNOTIFY */
