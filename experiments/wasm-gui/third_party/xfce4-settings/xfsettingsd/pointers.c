/*
 *  Copyright (c) 2008-2011 Nick Schermer <nick@xfce.org>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_LIBINPUT
#include "libinput-properties.h"
#endif /* HAVE_LIBINPUT */

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <locale.h>

#include "debug.h"
#include "pointers.h"
#include "pointers-defines.h"

#define MAX_DENOMINATOR (100.00)

#ifdef XI_PROP_ENABLED
#define DEVICE_ENABLED XI_PROP_ENABLED
#else
#define DEVICE_ENABLED "Device Enabled"
#endif /* XI_PROP_ENABLED */

static void             xfce_pointers_helper_finalize                 (GObject            *object);
static void             xfce_pointers_helper_syndaemon_stop           (XfcePointersHelper *helper);
static void             xfce_pointers_helper_syndaemon_check          (XfcePointersHelper *helper);
static void             xfce_pointers_helper_restore_devices          (XfcePointersHelper *helper,
                                                                       XID                *xid);
static void             xfce_pointers_helper_channel_property_changed (XfconfChannel      *channel,
                                                                       const gchar        *property_name,
                                                                       const GValue       *value,
                                                                       XfcePointersHelper *helper);
#ifdef DEVICE_HOTPLUGGING
static GdkFilterReturn  xfce_pointers_helper_event_filter             (GdkXEvent          *xevent,
                                                                       GdkEvent           *gdk_event,
                                                                       gpointer            user_data);
#endif
#if defined(DEVICE_PROPERTIES) || defined(HAVE_LIBINPUT)
static void             xfce_pointers_helper_change_property          (XDeviceInfo        *device_info,
                                                                       XDevice            *device,
                                                                       Display            *xdisplay,
                                                                       const gchar        *prop_name,
                                                                       const GValue       *value);
#endif /* DEVICE_PROPERTIES || HAVE_LIBINPUT */



struct _XfcePointersHelperClass
{
    GObjectClass __parent__;
};

struct _XfcePointersHelper
{
    GObject  __parent__;

    /* xfconf channel */
    XfconfChannel *channel;

#ifdef DEVICE_PROPERTIES
    GPid           syndaemon_pid;
#endif

#ifdef DEVICE_HOTPLUGGING
    /* device presence event type */
    gint           device_presence_event_type;
#endif
};

typedef struct
{
    Display     *xdisplay;
    XDevice     *device;
    XDeviceInfo *device_info;
    gsize        prop_name_len;
}
XfcePointerData;



G_DEFINE_TYPE (XfcePointersHelper, xfce_pointers_helper, G_TYPE_OBJECT);



static void
xfce_pointers_helper_class_init (XfcePointersHelperClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = xfce_pointers_helper_finalize;
}



static void
xfce_pointers_helper_init (XfcePointersHelper *helper)
{
    XExtensionVersion *version = NULL;
    Display           *xdisplay;
#ifdef DEVICE_HOTPLUGGING
    XEventClass        event_class;
#endif

    /* get the default display */
    xdisplay = gdk_x11_display_get_xdisplay (gdk_display_get_default ());

    /* query the extension version */
    version = XGetExtensionVersion (xdisplay, INAME);

    /* check for Xi */
    if (version == NULL || ((long) version) == NoSuchExtension
        || !version->present)
    {
        g_critical ("XI is not present.");
    }
    else if (version->major_version < MIN_XI_VERS_MAJOR
             || (version->major_version == MIN_XI_VERS_MAJOR
                 && version->minor_version < MIN_XI_VERS_MINOR))
    {
        g_critical ("Your XI is too old (%d.%d) version %d.%d is required.",
                    version->major_version, version->minor_version,
                    MIN_XI_VERS_MAJOR, MIN_XI_VERS_MINOR);
    }
    else
    {
        xfsettings_dbg (XFSD_DEBUG_POINTERS, "initialized xi %d.%d",
                        version->major_version, version->minor_version);

        /* open the channel */
        helper->channel = xfconf_channel_get ("pointers");

        /* restore the pointer devices */
        xfce_pointers_helper_restore_devices (helper, NULL);

        /* monitor the channel */
        g_signal_connect (G_OBJECT (helper->channel), "property-changed",
             G_CALLBACK (xfce_pointers_helper_channel_property_changed), helper);

        /* launch syndaemon if required */
        xfce_pointers_helper_syndaemon_check (helper);

#ifdef DEVICE_HOTPLUGGING
        if (G_LIKELY (xdisplay != NULL))
        {
            /* monitor device changes */
            gdk_x11_display_error_trap_push (gdk_display_get_default ());
            DevicePresence (xdisplay, helper->device_presence_event_type, event_class);
            XSelectExtensionEvent (xdisplay, RootWindow (xdisplay, DefaultScreen (xdisplay)), &event_class, 1);

            /* add an event filter */
            if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) == 0)
                gdk_window_add_filter (NULL, xfce_pointers_helper_event_filter, helper);
            else
                g_warning ("Failed to create device filter");
        }
#endif
    }
}



static void
xfce_pointers_helper_finalize (GObject *object)
{
    xfce_pointers_helper_syndaemon_stop (XFCE_POINTERS_HELPER (object));

    (*G_OBJECT_CLASS (xfce_pointers_helper_parent_class)->finalize) (object);
}



#ifdef HAVE_LIBINPUT
static gboolean
xfce_pointers_is_enabled (Display *xdisplay,
                          XDevice *device)
{
    Atom     prop, type;
    gulong   n_items, bytes_after;
    gint     rc, format;
    guchar  *data;
    gboolean enabled;

    prop = XInternAtom (xdisplay, DEVICE_ENABLED, False);
    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    rc = XGetDeviceProperty (xdisplay, device, prop, 0, 1, False,
                             XA_INTEGER, &type, &format, &n_items,
                             &bytes_after, &data);
    gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
    if (rc == Success)
    {
        enabled = (gboolean) *data;
        XFree (data);
        return (enabled);
    }

    return FALSE;
}



static gboolean
xfce_pointers_is_libinput (Display *xdisplay,
                           XDevice *device)
{
    Atom     prop, type;
    gulong   n_items, bytes_after;
    gint     rc, format;
    guchar  *data;

    prop = XInternAtom (xdisplay, LIBINPUT_PROP_LEFT_HANDED, False);
    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    rc = XGetDeviceProperty (xdisplay, device, prop, 0, 1, False,
                             XA_INTEGER, &type, &format, &n_items,
                             &bytes_after, &data);
    gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
    if (rc == Success)
    {
        XFree (data);
        return (n_items > 0);
    }

    return FALSE;
}
#endif /* HAVE_LIBINPUT */



static void
xfce_pointers_helper_syndaemon_stop (XfcePointersHelper *helper)
{
#ifdef DEVICE_PROPERTIES
    if (helper->syndaemon_pid != 0)
    {
        xfsettings_dbg (XFSD_DEBUG_POINTERS, "Killed syndaemon with pid %d",
                        helper->syndaemon_pid);

        kill (helper->syndaemon_pid, SIGHUP);
        g_spawn_close_pid (helper->syndaemon_pid);
        helper->syndaemon_pid = 0;
    }
#endif
}



static void
xfce_pointers_helper_syndaemon_check (XfcePointersHelper *helper)
{
#ifdef DEVICE_PROPERTIES
    Display     *xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    XDeviceInfo *device_list;
    XDevice     *device;
    gint         n, ndevices;
    Atom         touchpad_type;
    Atom         touchpad_off_prop;
    Atom        *props;
    gint         i, nprops;
    gboolean     have_synaptics = FALSE;
    gdouble      disable_duration;
    gchar        disable_duration_string[64];
    gchar       *args[] = { "syndaemon", "-i", disable_duration_string, "-K", "-R", NULL };
    GError      *error = NULL;

    /* only stop a running daemon */
    if (!xfconf_channel_get_bool (helper->channel, "/DisableTouchpadWhileTyping", FALSE))
        goto start_stop_daemon;

    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    device_list = XListInputDevices (xdisplay, &ndevices);
    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0 || device_list == NULL)
        goto start_stop_daemon;

    touchpad_type = XInternAtom (xdisplay, XI_TOUCHPAD, True);
    touchpad_off_prop = XInternAtom (xdisplay, "Synaptics Off", True);

    for (n = 0; n < ndevices; n++)
    {
        /* search for a touchpad */
        if (device_list[n].type != touchpad_type)
            continue;

        gdk_x11_display_error_trap_push (gdk_display_get_default ());
        device = XOpenDevice (xdisplay, device_list[n].id);
        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0 || device == NULL)
        {
            g_critical ("Unable to open device %s", device_list[n].name);
            break;
        }

        /* look for the Synaptics Off property */
        gdk_x11_display_error_trap_push (gdk_display_get_default ());
        props = XListDeviceProperties (xdisplay, device, &nprops);
        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) == 0
            && props != NULL)
        {
            for (i = 0; !have_synaptics && i < nprops; i++)
                have_synaptics = props[i] == touchpad_off_prop;

            XFree (props);
        }

        XCloseDevice (xdisplay, device);

        if (have_synaptics)
            break;
    }

    XFreeDeviceList (device_list);

    start_stop_daemon:

    /* stop the daemon in any case */
    xfce_pointers_helper_syndaemon_stop (helper);

    if (have_synaptics)
    {
        disable_duration = xfconf_channel_get_double (helper->channel,
                                                      "/DisableTouchpadDuration",
                                                      2.0);
        setlocale(LC_NUMERIC, "C"); /* syndaemon needs a dot for the float. Nothing localized! */
        g_snprintf (disable_duration_string, sizeof (disable_duration_string),
                    "%.1f", disable_duration);

        if (!g_spawn_async (NULL, args, NULL, G_SPAWN_SEARCH_PATH,
                            NULL, NULL, &helper->syndaemon_pid, &error))
        {
            g_critical ("Spawning syndaemon failed: %s", error->message);
            g_error_free (error);
        }

        xfsettings_dbg (XFSD_DEBUG_POINTERS, "Started syndaemon with pid %d",
                        helper->syndaemon_pid);
    }
#endif
}



static gboolean
xfce_pointers_helper_change_button_mapping_swap (guchar   *buttonmap,
                                                 gshort    num_buttons,
                                                 gint      id_1,
                                                 gint      id_2,
                                                 gboolean  reverse)
{
    gshort n;
    gint   id_a = -1;
    gint   id_b = -1;

    /* figure out the position of the id_1 and id_2 buttons in the map */
    for (n = 0; n < num_buttons; n++)
    {
        if (buttonmap[n] == id_1)
            id_a = n;
        else if (buttonmap[n] == id_2)
            id_b = n;
    }

    /* only change the map when id_a and id_b where found */
    if (G_LIKELY (id_a != -1 && id_b != -1))
    {
        /* check if we need to change the buttonmap */
        if ((!reverse && (id_a < id_b))
            || (reverse && (id_a > id_b)))
        {
            /* swap the buttons in the button map */
            buttonmap[id_a] = id_2;
            buttonmap[id_b] = id_1;

            return TRUE;
        }
    }

    return FALSE;
}



static void
xfce_pointers_helper_change_button_mapping (XDeviceInfo *device_info,
                                            XDevice     *device,
                                            Display     *xdisplay,
                                            gint         right_handed,
                                            gint         reverse_scrolling)
{
    XAnyClassPtr  ptr;
    gshort        num_buttons = 0;
    guchar       *buttonmap;
    gboolean      map_changed = FALSE;
    gint          n;
    gint          right_button;
    GString      *readable_map;

#ifdef HAVE_LIBINPUT
    if (xfce_pointers_is_libinput (xdisplay, device))
    {
        if (right_handed != -1)
        {
            GValue value = G_VALUE_INIT;

            g_value_init (&value, G_TYPE_INT);
            g_value_set_int (&value, !right_handed);

            xfce_pointers_helper_change_property (device_info, device, xdisplay,
                                                  LIBINPUT_PROP_LEFT_HANDED, &value);
        }

        if (reverse_scrolling != -1)
        {
            GValue value = G_VALUE_INIT;

            g_value_init (&value, G_TYPE_INT);
            g_value_set_int (&value, reverse_scrolling);

            xfce_pointers_helper_change_property (device_info, device, xdisplay,
                                                  LIBINPUT_PROP_NATURAL_SCROLL, &value);
        }

        return;
    }
#endif /* HAVE_LIBINPUT */

    /* search the number of buttons */
    for (n = 0, ptr = device_info->inputclassinfo; n < device_info->num_classes; n++)
    {
        if (ptr->class == ButtonClass)
        {
            num_buttons = ((XButtonInfoPtr) ptr)->num_buttons;
            break;
        }

        /* advance the offset */
        ptr = (XAnyClassPtr) ((gchar *) ptr + ptr->length);
    }

    if (num_buttons == 0)
    {
        g_critical ("Device %s has no buttons", device_info->name);
        return;
    }

    /* allocate the button map */
    buttonmap = g_new0 (guchar, num_buttons);

    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    XGetDeviceButtonMapping (xdisplay, device, buttonmap, num_buttons);
    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
    {
        g_warning ("Failed to get button mapping");
        goto leave;
    }

    /* -1 means we don't change this in the mapping */
    if (right_handed != -1)
    {
        /* get the right button number */
        right_button = MIN (num_buttons, 3);

        /* check the buttons and swap them if needed */
        if (xfce_pointers_helper_change_button_mapping_swap (buttonmap, num_buttons,
                                                             1 /* left button */,
                                                             right_button,
                                                             right_handed))
            map_changed = TRUE;
    }

    /* -1 means we don't change this in the mapping */
    if (reverse_scrolling != -1 && num_buttons >= 5)
    {
        /* check the buttons and swap them if needed */
        if (xfce_pointers_helper_change_button_mapping_swap (buttonmap, num_buttons,
                                                             4 /* scroll up */,
                                                             5 /* scroll down */,
                                                             !reverse_scrolling))
            map_changed = TRUE;
    }

    /* only set on changes */
    if (map_changed)
    {
        gdk_x11_display_error_trap_push (gdk_display_get_default ());
        XSetDeviceButtonMapping (xdisplay, device, buttonmap, num_buttons);
        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
            g_warning ("Failed to set button mapping");

        /* don't put a hard time on ourselves and make debugging a lot better */
        readable_map = g_string_sized_new (num_buttons);
        for (n = 0; n < num_buttons; n++)
            g_string_append_printf (readable_map, "%d ", buttonmap[n]);
        xfsettings_dbg (XFSD_DEBUG_POINTERS, "[%s] new buttonmap is [%s]",
                        device_info->name, readable_map->str);
        g_string_free (readable_map, TRUE);
    }
    else
    {
        xfsettings_dbg (XFSD_DEBUG_POINTERS, "[%s] buttonmap not changed",
                        device_info->name);
    }

    leave:

    g_free (buttonmap);
}



static gint
xfce_pointers_helper_gcd (gint num,
                          gint denom)
{
    /* calc the greatest common divisor using euclidean's algorithm */
    return (denom != 0 ? xfce_pointers_helper_gcd (denom, num % denom) : num);
}



static void
xfce_pointers_helper_change_feedback (XDeviceInfo *device_info,
                                      XDevice     *device,
                                      Display     *xdisplay,
                                      gint         threshold,
                                      gdouble      acceleration)
{
    XFeedbackState      *states, *pt;
    gint                 num_feedbacks;
    XPtrFeedbackControl  feedback;
    gint                 n;
    gulong               mask = 0;
    gint                 num, denom, gcd;
    gboolean             found = FALSE;

#ifdef HAVE_LIBINPUT
    if (xfce_pointers_is_libinput (xdisplay, device))
    {
        gdouble libinput_accel;
        GValue value = G_VALUE_INIT;

        libinput_accel = CLAMP ((acceleration / 5) - 1.0, -1.0, 1.0);
        g_value_init (&value, G_TYPE_DOUBLE);
        g_value_set_double (&value, libinput_accel);

        xfce_pointers_helper_change_property (device_info, device, xdisplay,
                                              LIBINPUT_PROP_ACCEL, &value);
        return;
    }
#endif /* HAVE_LIBINPUT */
    /* get the feedback states for this device */
    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    states = XGetFeedbackControl (xdisplay, device, &num_feedbacks);
    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0 || states == NULL)
    {
        g_critical ("Failed to get the feedback states of device %s",
                    device_info->name);
        return;
    }

    for (pt = states, n = 0; n < num_feedbacks; n++)
    {
        /* find the pointer feedback class */
        if (pt->class != PtrFeedbackClass)
        {
            /* advance the offset */
            pt = (XFeedbackState *) ((gchar *) pt + pt->length);
            continue;
        }

        /* initialize the feedback, -1 for reset if the
         * mask matches in XChangeFeedbackControl */
        feedback.class = PtrFeedbackClass;
        feedback.length = sizeof (XPtrFeedbackControl);
        feedback.id = pt->id;
        feedback.threshold = -1;
        feedback.accelNum = -1;
        feedback.accelDenom = -1;

        found = TRUE;

        /* above 0 is a valid value, -1 is reset, -2.00
         * is passed if no change is required */
        if (acceleration >= 0 || acceleration == -1)
        {
            if (acceleration >= 0)
            {
                /* calculate the faction of the acceleration */
                num = acceleration * MAX_DENOMINATOR;
                denom = MAX_DENOMINATOR;
                gcd = xfce_pointers_helper_gcd (num, denom);
                num /= gcd;
                denom /= gcd;

                feedback.accelNum = num;
                feedback.accelDenom = denom;
            }

            /* include acceleration in the mask */
            mask |= DvAccelNum | DvAccelDenom;
        }

        /* above 0 is a valid value, -1 is reset, -2 is
         * passed if no change is required */
        if (threshold > 0 || threshold == -1)
        {
            if (threshold > 0)
                feedback.threshold = threshold;

            mask |= DvThreshold;
        }

        /* update the feedback of the device */
        gdk_x11_display_error_trap_push (gdk_display_get_default ());
        XChangeFeedbackControl (xdisplay, device, mask,
                                (XFeedbackControl *) &feedback);
        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
        {
            g_warning ("Failed to set feedback states for device %s",
                       device_info->name);
        }

        xfsettings_dbg (XFSD_DEBUG_POINTERS,
                        "[%s] change feedback (threshold=%d, "
                        "accelNum=%d, accelDenom=%d)",
                        device_info->name, feedback.threshold,
                        feedback.accelNum, feedback.accelDenom);

        break;
    }

    if (!found)
    {
        g_critical ("Unable to find PtrFeedbackClass for %s",
                    device_info->name);
    }

    XFreeFeedbackList (states);
}



static void
xfce_pointers_helper_change_mode (XDeviceInfo  *device_info,
                                  XDevice      *device,
                                  Display      *xdisplay,
                                  const gchar  *mode_name)
{
    gint mode;

    if (strcmp (mode_name, "RELATIVE") == 0)
        mode = Relative;
    else if (strcmp (mode_name, "ABSOLUTE") == 0)
        mode = Absolute;
    else
    {
        g_warning ("Unknown device mode %s, only RELATIVE and ABSOLUTE are valid", mode_name);
        return;
    }

    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    XSetDeviceMode (xdisplay, device, mode);
    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
        g_critical ("Failed to change the device mode");

    xfsettings_dbg (XFSD_DEBUG_POINTERS,
                    "[%s] Set mode to %s", device_info->name, mode_name);
}



static gchar *
xfce_pointers_helper_device_xfconf_name (const gchar *name)
{
    GString     *string;
    const gchar *p;

    /* NOTE: this function exists in both the dialog and
     *       helper code and they have to identical! */

    /* allocate a string */
    string = g_string_sized_new (strlen (name));

    /* create a name with only valid chars */
    for (p = name; *p != '\0'; p++)
    {
        if ((*p >= 'A' && *p <= 'Z')
            || (*p >= 'a' && *p <= 'z')
            || (*p >= '0' && *p <= '9')
            || *p == '_' || *p == '-')
        {
            g_string_append_c (string, *p);
        }
        else if (*p == ' ')
        {
            string = g_string_append_c (string, '_');
        }
    }

    /* return the new string */
    return g_string_free (string, FALSE);
}



#if defined(DEVICE_PROPERTIES) || defined(HAVE_LIBINPUT)
static void
xfce_pointers_helper_change_property (XDeviceInfo  *device_info,
                                      XDevice      *device,
                                      Display      *xdisplay,
                                      const gchar  *prop_name,
                                      const GValue *value)
{
    Atom         *props;
    gint          n, n_props;
    Atom          prop;
    gchar        *atom_name;
    Atom          type;
    gint          format;
    gulong        n_items, bytes_after, i;
    gulong        n_succeeds;
    Atom          float_atom;
    GPtrArray    *array = NULL;
    int           rc;
    const GValue *val;
    union {
        guchar *c;
        gshort *s;
        glong  *l;
        float  *f;
        Atom   *a;
    } data;

    /* assuming the device property never contained underscores... */
    atom_name = g_strdup (prop_name);
    g_strdelimit (atom_name, "_", ' ');
    prop = XInternAtom (xdisplay, atom_name, True);
    g_free (atom_name);

    /* because of the True in XInternAtom we quit here if the property
     * does not exists on any of the devices */
    if (prop == None)
        return;

#ifdef HAVE_LIBINPUT
    /*
     * libinput cannot change properties on disabled devices
     * see: https://bugs.freedesktop.org/show_bug.cgi?id=89296
     * and: http://lists.x.org/archives/xorg-devel/2015-February/045716.html
     */
    if (prop != XInternAtom (xdisplay, DEVICE_ENABLED, True) &&
        !xfce_pointers_is_enabled (xdisplay, device))
        return;
#endif /* HAVE_LIBINPUT */

    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    props = XListDeviceProperties (xdisplay, device, &n_props);
    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) || props == NULL)
        return;

    float_atom = XInternAtom (xdisplay, "FLOAT", False);

    for (n = 0; n < n_props; n++)
    {
        /* find the matching property */
        if (props[n] != prop)
            continue;

        gdk_x11_display_error_trap_push (gdk_display_get_default ());
        rc = XGetDeviceProperty (xdisplay, device, prop, 0, 1000, False,
                                 AnyPropertyType, &type, &format,
                                 &n_items, &bytes_after, &data.c);
        if (!gdk_x11_display_error_trap_pop (gdk_display_get_default ()) && rc == Success)
        {
            if (n_items == 1
                && (G_VALUE_HOLDS_INT (value)
                    || G_VALUE_HOLDS_STRING (value)
                    || G_VALUE_HOLDS_DOUBLE (value)))
            {
                /* only 1 items to set */
                val = value;
            }
            else if (G_VALUE_TYPE (value) == G_TYPE_PTR_ARRAY)
            {
                array = g_value_get_boxed (value);
                if (array->len != n_items)
                {
                    g_critical ("Nr device property items (%ld) and xfconf value (%d) differ",
                                n_items, array->len);
                    break;
                }
            }
            else
            {
                g_critical ("Invalid device property combination");
                break;
            }

            /* reset check counter */
            n_succeeds = 0;

            for (i = 0; i < n_items; i++)
            {
                /* get value from pointer array */
                if (array != NULL)
                    val = g_ptr_array_index (array, i);
                else
                    val = value;

                if (G_VALUE_HOLDS_INT (val)
                    && type == XA_INTEGER)
                {
                    if (format == 8)
                        data.c[i] = g_value_get_int (val);
                    else if (format == 16)
                        data.s[i] = g_value_get_int (val);
                    else if (format == 32)
                        data.l[i] = g_value_get_int (val);
                    else
                    {
                        g_critical ("Unknown format %d for integer", format);
                        break;
                    }
                }
                else if (G_VALUE_HOLDS_STRING (val)
                         && type == XA_ATOM
                         && format == 32)
                {
                    /* set atom (reference to a string) */
                    data.a[i] = XInternAtom (xdisplay, g_value_get_string (val), False);
                }
                else if (G_VALUE_HOLDS_DOUBLE (val) /* xfconf doesn't support floats */
                         && type == float_atom
                         && format == 32)
                {
                    /* Xorg actually uses sizeof(long) bytes per element if format == 32 */
                    /* See https://gitlab.freedesktop.org/xorg/app/xinput/-/blob/cef07c0c8280d7e7b82c3bcc62a1dfbe8cc43ff8/src/property.c#L80 */
                    *(float*) &data.l[i] = (float) g_value_get_double (val);
                }
                else
                {
                    g_critical ("Unknown property type %s: target = %s, format = %d",
                                G_VALUE_TYPE_NAME (val), XGetAtomName (xdisplay, type), format);
                    break;
                }

                /* the item was successfully updated */
                n_succeeds++;
            }

            if (n_succeeds == n_items)
            {
                gdk_x11_display_error_trap_push (gdk_display_get_default ());
                XChangeDeviceProperty (xdisplay, device, prop, type, format,
                                       PropModeReplace, data.c, n_items);
                XSync (xdisplay, FALSE);
                if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()))
                {
                    g_critical ("Failed to set device property %s for %s",
                                prop_name, device_info->name);
                }

                xfsettings_dbg (XFSD_DEBUG_POINTERS,
                                "[%s] Changed device property %s",
                                device_info->name, prop_name);
            }
        }

        if (data.c)
            XFree (data.c);

        break;
    }

    XFree (props);
}
#endif /* DEVICE_PROPERTIES || HAVE_LIBINPUT */


#ifdef DEVICE_PROPERTIES
static void
xfce_pointers_helper_change_properties (gpointer key,
                                        gpointer value,
                                        gpointer user_data)
{
    XfcePointerData *pointer_data = user_data;
    const gchar     *prop_name = ((gchar *) key) + pointer_data->prop_name_len;

    xfce_pointers_helper_change_property (pointer_data->device_info,
                                          pointer_data->device,
                                          pointer_data->xdisplay,
                                          prop_name, value);
}
#endif



static void
xfce_pointers_helper_restore_devices (XfcePointersHelper *helper,
                                      XID                *xid)
{
    Display         *xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    XDeviceInfo     *device_list, *device_info;
    gint             n, ndevices;
    XDevice         *device;
    gchar           *device_name;
    gchar            prop[256];
    gboolean         right_handed;
    gboolean         reverse_scrolling;
    gint             threshold;
    gdouble          acceleration;
#ifdef DEVICE_PROPERTIES
    GHashTable      *props;
    XfcePointerData  pointer_data;
#endif

    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    device_list = XListInputDevices (xdisplay, &ndevices);
    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0 || device_list == NULL)
    {
        g_message ("No input devices found");
        return;
    }

    for (n = 0; n < ndevices; n++)
    {
        gchar *mode;

        /* filter the pointer devices */
        device_info = &device_list[n];
        if (device_info->use != IsXExtensionPointer
            || device_info->name == NULL)
            continue;

        /* filter out the device if one is set */
        if (xid != NULL && device_info->id != *xid)
            continue;

        /* open the device */
        gdk_x11_display_error_trap_push (gdk_display_get_default ());
        device = XOpenDevice (xdisplay, device_info->id);
        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0 || device == NULL)
        {
            g_critical ("Unable to open device %s", device_info->name);
            continue;
        }

        /* create a valid xfconf property name for the device */
        device_name = xfce_pointers_helper_device_xfconf_name (device_info->name);

        /* read buttonmap properties */
        g_snprintf (prop, sizeof (prop), "/%s/RightHanded", device_name);
        right_handed = xfconf_channel_get_bool (helper->channel, prop, -1);

        g_snprintf (prop, sizeof (prop), "/%s/ReverseScrolling", device_name);
        reverse_scrolling = xfconf_channel_get_bool (helper->channel, prop, -1);

        if (right_handed != -1 || reverse_scrolling != -1)
        {
            xfce_pointers_helper_change_button_mapping (device_info, device, xdisplay,
                                                        right_handed, reverse_scrolling);
        }

        /* read feedback settings */
        g_snprintf (prop, sizeof (prop), "/%s/Threshold", device_name);
        threshold = xfconf_channel_get_int (helper->channel, prop, -1);

        g_snprintf (prop, sizeof (prop), "/%s/Acceleration", device_name);
        acceleration = xfconf_channel_get_double (helper->channel, prop, -1.00);

        if (threshold != -1 || acceleration != -1.00)
        {
            xfce_pointers_helper_change_feedback (device_info, device, xdisplay,
                                                  threshold, acceleration);
        }

        /* read mode settings */
        g_snprintf (prop, sizeof (prop), "/%s/Mode", device_name);
        mode =  xfconf_channel_get_string  (helper->channel, prop, NULL);

        if (mode != NULL)
        {
            xfce_pointers_helper_change_mode (device_info, device, xdisplay, mode);
            g_free (mode);
            mode = NULL;
        }

#ifdef DEVICE_PROPERTIES
        /* set device properties */
        g_snprintf (prop, sizeof (prop), "/%s/Properties", device_name);
        props = xfconf_channel_get_properties (helper->channel, prop);

        if (props != NULL)
        {
            pointer_data.xdisplay = xdisplay;
            pointer_data.device = device;
            pointer_data.device_info = device_info;
            pointer_data.prop_name_len = strlen (prop) + 1;

            g_hash_table_foreach (props, xfce_pointers_helper_change_properties, &pointer_data);

            g_hash_table_destroy (props);
        }
#endif

        g_free (device_name);
        XCloseDevice (xdisplay, device);
    }

    XFreeDeviceList (device_list);
}



static void
xfce_pointers_helper_channel_property_changed (XfconfChannel      *channel,
                                               const gchar        *property_name,
                                               const GValue       *value,
                                               XfcePointersHelper *helper)
{
    Display      *xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    XDeviceInfo  *device_list, *device_info;
    XDevice      *device;
    gint          n, ndevices;
    gchar       **names;
    gchar        *device_name;

    if (G_UNLIKELY (property_name == NULL))
         return;

    /* check the daemon status */
    if ((strcmp (property_name, "/DisableTouchpadWhileTyping") == 0) ||
        (strcmp (property_name, "/DisableTouchpadDuration") == 0))
    {
        xfce_pointers_helper_syndaemon_check (helper);
        return;
    }

    /* split the property name (+1 so skip the first slash in the name) */
    names = g_strsplit (property_name + 1, "/", -1);

    if (names != NULL && g_strv_length (names) >= 2)
    {
        gdk_x11_display_error_trap_push (gdk_display_get_default ());
        device_list = XListInputDevices (xdisplay, &ndevices);
        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0 || device_list == NULL)
        {
            g_message ("No input devices found");
            return;
        }

        for (n = 0; n < ndevices; n++)
        {
            /* filter the pointer devices */
            device_info = &device_list[n];
            if (device_info->use != IsXExtensionPointer
                || device_info->name == NULL)
                continue;

            /* search the device name */
            device_name = xfce_pointers_helper_device_xfconf_name (device_info->name);
            if (strcmp (names[0], device_name) == 0)
            {
                /* open the device */
                gdk_x11_display_error_trap_push (gdk_display_get_default ());
                device = XOpenDevice (xdisplay, device_info->id);
                if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0 || device == NULL)
                {
                    g_critical ("Unable to open device %s", device_info->name);
                    continue;
                }

                /* check the property that requires updating */
                if (strcmp (names[1], "RightHanded") == 0)
                {
                    xfce_pointers_helper_change_button_mapping (device_info, device, xdisplay,
                                                                g_value_get_boolean (value), -1);
                }
                else if (strcmp (names[1], "ReverseScrolling") == 0)
                {
                    xfce_pointers_helper_change_button_mapping (device_info, device, xdisplay,
                                                                -1, g_value_get_boolean (value));
                }
                else if (strcmp (names[1], "Threshold") == 0)
                {
                    xfce_pointers_helper_change_feedback (device_info, device, xdisplay,
                                                          g_value_get_int (value), -2.00);
                }
                else if (strcmp (names[1], "Acceleration") == 0)
                {
                    xfce_pointers_helper_change_feedback (device_info, device, xdisplay,
                                                          -2, g_value_get_double (value));
                }
#ifdef DEVICE_PROPERTIES
                else if (strcmp (names[1], "Properties") == 0)
                {
                    xfce_pointers_helper_change_property (device_info, device, xdisplay,
                                                          names[2], value);
                }
#endif
                else if (strcmp (names[1], "Mode") == 0)
                {
                    xfce_pointers_helper_change_mode (device_info, device, xdisplay,
                                                      g_value_get_string (value));
                }
                else
                {
                    g_warning ("Unknown property %s set for device %s",
                               property_name, device_info->name);
                }

                XCloseDevice (xdisplay, device);

                /* stop searching */
                n = ndevices;
            }

            g_free (device_name);
        }

        XFreeDeviceList (device_list);
    }

    g_strfreev (names);
}



#ifdef DEVICE_HOTPLUGGING
static GdkFilterReturn
xfce_pointers_helper_event_filter (GdkXEvent *xevent,
                                   GdkEvent  *gdk_event,
                                   gpointer   user_data)
{
    XEvent                     *event = xevent;
    XDevicePresenceNotifyEvent *dpn_event = xevent;
    XfcePointersHelper         *helper = XFCE_POINTERS_HELPER (user_data);

    if (event->type == helper->device_presence_event_type)
    {
        /* restore device settings */
        if (dpn_event->devchange == DeviceAdded)
            xfce_pointers_helper_restore_devices (helper, &dpn_event->deviceid);

        /* check if we need to launch syndaemon */
        xfce_pointers_helper_syndaemon_check (helper);
    }

    return GDK_FILTER_CONTINUE;
}
#endif
