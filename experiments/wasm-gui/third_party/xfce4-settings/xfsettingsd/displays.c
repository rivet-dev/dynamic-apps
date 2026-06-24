/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *  Copyright (C) 2010-2012 Lionel Le Folgoc <lionel@lefolgoc.net>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>

#include <X11/extensions/Xrandr.h>

#include "common/display-profiles.h"
#include "common/xfce-randr.h"

#include "debug.h"
#include "displays.h"
#ifdef HAVE_UPOWERGLIB
#include "displays-upower.h"
#endif

/* check for randr 1.3 or better */
#if RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 3)
#define HAS_RANDR_ONE_POINT_THREE
#else
#undef HAS_RANDR_ONE_POINT_THREE
#endif

/* Xfconf properties */
#define APPLY_SCHEME_PROP    "/Schemes/Apply"
#define DEFAULT_SCHEME_NAME  "Default"
#define ACTIVE_PROFILE       "/ActiveProfile"
#define AUTO_ENABLE_PROFILES "/AutoEnableProfiles"
#define OUTPUT_FMT           "/%s/%s"
#define PRIMARY_PROP         OUTPUT_FMT "/Primary"
#define ACTIVE_PROP          OUTPUT_FMT "/Active"
#define ROTATION_PROP        OUTPUT_FMT "/Rotation"
#define REFLECTION_PROP      OUTPUT_FMT "/Reflection"
#define RESOLUTION_PROP      OUTPUT_FMT "/Resolution"
#define SCALEX_PROP          OUTPUT_FMT "/Scale/X"
#define SCALEY_PROP          OUTPUT_FMT "/Scale/Y"
#define RRATE_PROP           OUTPUT_FMT "/RefreshRate"
#define POSX_PROP            OUTPUT_FMT "/Position/X"
#define POSY_PROP            OUTPUT_FMT "/Position/Y"
#define NOTIFY_PROP          "/Notify"



/* wrappers to avoid querying too often */
typedef struct _XfceRRCrtc   XfceRRCrtc;
typedef struct _XfceRROutput XfceRROutput;



static void             xfce_displays_helper_dispose                        (GObject                 *object);
static void             xfce_displays_helper_finalize                       (GObject                 *object);
static void             xfce_displays_helper_reload                         (XfceDisplaysHelper      *helper);
static gchar           *xfce_displays_helper_get_matching_profile           (XfceDisplaysHelper      *helper);
static GdkFilterReturn  xfce_displays_helper_screen_on_event                (GdkXEvent               *xevent,
                                                                             GdkEvent                *event,
                                                                             gpointer                 data);
static void             xfce_displays_helper_set_screen_size                (XfceDisplaysHelper      *helper);
static gboolean         xfce_displays_helper_load_from_xfconf               (XfceDisplaysHelper      *helper,
                                                                             const gchar             *scheme,
                                                                             GHashTable              *saved_outputs,
                                                                             XfceRROutput            *output);
static GPtrArray       *xfce_displays_helper_list_outputs                   (XfceDisplaysHelper      *helper);
static void             xfce_displays_helper_free_output                    (XfceRROutput            *output);
static GPtrArray       *xfce_displays_helper_list_crtcs                     (XfceDisplaysHelper      *helper);
static XfceRRCrtc      *xfce_displays_helper_find_crtc_by_id                (XfceDisplaysHelper      *helper,
                                                                             RRCrtc                   id);
static void             xfce_displays_helper_free_crtc                      (XfceRRCrtc              *crtc);
static XfceRRCrtc      *xfce_displays_helper_find_usable_crtc               (XfceDisplaysHelper      *helper,
                                                                             XfceRROutput            *output);
static void             xfce_displays_helper_get_topleftmost_pos            (XfceRRCrtc              *crtc,
                                                                             XfceDisplaysHelper      *helper);
static void             xfce_displays_helper_normalize_crtc                 (XfceRRCrtc              *crtc,
                                                                             XfceDisplaysHelper      *helper);
static Status           xfce_displays_helper_disable_crtc                   (XfceDisplaysHelper      *helper,
                                                                             RRCrtc                   crtc);
static void             xfce_displays_helper_workaround_crtc_size           (XfceRRCrtc              *crtc,
                                                                             XfceDisplaysHelper      *helper);
static void             xfce_displays_helper_apply_crtc_transform           (XfceRRCrtc              *crtc,
                                                                             XfceDisplaysHelper      *helper);
static void             xfce_displays_helper_apply_crtc                     (XfceRRCrtc              *crtc,
                                                                             XfceDisplaysHelper      *helper);
static void             xfce_displays_helper_set_outputs                    (XfceRRCrtc              *crtc,
                                                                             XfceRROutput            *output);
static void             xfce_displays_helper_apply_all                      (XfceDisplaysHelper      *helper);
static void             xfce_displays_helper_channel_apply                  (XfceDisplaysHelper      *helper,
                                                                             const gchar             *scheme);
static void             xfce_displays_helper_channel_property_changed       (XfconfChannel           *channel,
                                                                             const gchar             *property_name,
                                                                             const GValue            *value,
                                                                             XfceDisplaysHelper      *helper);
static void             xfce_displays_helper_toggle_internal                (gpointer                *power,
                                                                             gboolean                 lid_is_closed,
                                                                             XfceDisplaysHelper      *helper);



struct _XfceDisplaysHelperClass
{
    GObjectClass __parent__;
};

struct _XfceDisplaysHelper
{
    GObject  __parent__;

    /* xfconf channel */
    XfconfChannel      *channel;
    guint               handler;

#ifdef HAS_RANDR_ONE_POINT_THREE
    gint                has_1_3;
    gint                primary;
#endif

#ifdef HAVE_UPOWERGLIB
    XfceDisplaysUPower *power;
    gint                phandler;
#endif

    GdkDisplay         *display;
    GdkWindow          *root_window;
    Display            *xdisplay;
    gint                event_base;

    /* RandR cache */
    XRRScreenResources *resources;
    GPtrArray          *crtcs;
    GPtrArray          *outputs;

    /* screen size */
    gint                width;
    gint                height;
    gint                mm_width;
    gint                mm_height;

    /* used to normalize positions */
    gint                min_x;
    gint                min_y;
};

struct _XfceRRCrtc
{
    RRCrtc    id;
    RRMode    mode;
    Rotation  rotation;
    Rotation  rotations;
    gint      width;
    gint      height;
    gint      x;
    gint      y;
    gdouble   scalex;
    gdouble   scaley;
    gint      noutput;
    RROutput *outputs;
    gint      npossible;
    RROutput *possible;
    gint      changed;
};

struct _XfceRROutput
{
    RROutput       id;
    XRROutputInfo *info;
    RRMode         preferred_mode;
    guint          active : 1;
};


G_DEFINE_TYPE (XfceDisplaysHelper, xfce_displays_helper, G_TYPE_OBJECT);



static void
xfce_displays_helper_class_init (XfceDisplaysHelperClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->dispose = xfce_displays_helper_dispose;
    gobject_class->finalize = xfce_displays_helper_finalize;
}



static void
xfce_displays_helper_init (XfceDisplaysHelper *helper)
{
    gint major = 0, minor = 0;
    gint error_base, err;

#ifdef HAVE_UPOWERGLIB
    helper->power = NULL;
    helper->phandler = 0;
#endif
    helper->resources = NULL;
    helper->outputs = NULL;
    helper->crtcs = NULL;
    helper->handler = 0;

    /* get the default display */
    helper->display = gdk_display_get_default ();
    helper->xdisplay = gdk_x11_display_get_xdisplay (helper->display);
    helper->root_window = gdk_get_default_root_window ();

    /* check if the randr extension is running */
    if (XRRQueryExtension (helper->xdisplay, &helper->event_base, &error_base))
    {
        /* query the version */
        if (XRRQueryVersion (helper->xdisplay, &major, &minor)
            && (major > 1 || (major == 1 && minor >= 2)))
        {
            gdk_x11_display_error_trap_push (helper->display);
            /* get the screen resource */
            helper->resources = XRRGetScreenResources (helper->xdisplay,
                                                       GDK_WINDOW_XID (helper->root_window));
            gdk_display_flush (helper->display);
            err = gdk_x11_display_error_trap_pop (helper->display);
            if (err)
            {
                g_critical ("XRRGetScreenResources failed (err: %d). "
                            "Display settings won't be applied.", err);
                return;
            }

            /* get all existing CRTCs and connected outputs */
            helper->crtcs = xfce_displays_helper_list_crtcs (helper);
            helper->outputs = xfce_displays_helper_list_outputs (helper);

            /* Set up RandR notifications */
            XRRSelectInput (helper->xdisplay,
                            GDK_WINDOW_XID (helper->root_window),
                            RRScreenChangeNotifyMask);
            gdk_x11_register_standard_event_type (helper->display,
                                                  helper->event_base,
                                                  RRNotify + 1);
            gdk_window_add_filter (helper->root_window,
                                   xfce_displays_helper_screen_on_event,
                                   helper);

#ifdef HAVE_UPOWERGLIB
            helper->power = g_object_new (XFCE_TYPE_DISPLAYS_UPOWER, NULL);
            helper->phandler = g_signal_connect (G_OBJECT (helper->power),
                                                 "lid-changed",
                                                 G_CALLBACK (xfce_displays_helper_toggle_internal),
                                                 helper);
#endif

            /* open the channel */
            helper->channel = xfconf_channel_get ("displays");

            /* remove any leftover apply property before setting the monitor */
            xfconf_channel_reset_property (helper->channel, APPLY_SCHEME_PROP, FALSE);
            xfconf_channel_set_string (helper->channel, ACTIVE_PROFILE, DEFAULT_SCHEME_NAME);

            /* monitor channel changes */
            helper->handler = g_signal_connect (G_OBJECT (helper->channel),
                                                "property-changed",
                                                G_CALLBACK (xfce_displays_helper_channel_property_changed),
                                                helper);

#ifdef HAS_RANDR_ONE_POINT_THREE
            helper->has_1_3 = (major > 1 || (major == 1 && minor >= 3));
#endif

            /*  check if we can auto-enable a profile */
            if (xfconf_channel_get_bool (helper->channel, AUTO_ENABLE_PROFILES, FALSE) &&
                xfconf_channel_get_int (helper->channel, NOTIFY_PROP, 1) > 0)
            {
                gchar *matching_profile = NULL;

                matching_profile = xfce_displays_helper_get_matching_profile (helper);
                if (matching_profile)
                {
                    xfce_displays_helper_channel_apply (helper, matching_profile);
                }
                else {
                    xfce_displays_helper_channel_apply (helper, DEFAULT_SCHEME_NAME);
                }
            }
            /* restore the default scheme */
            else {
                xfce_displays_helper_channel_apply (helper, DEFAULT_SCHEME_NAME);
            }
        }
        else
        {
             g_critical ("RANDR extension is too old, version %d.%d. "
                         "Display settings won't be applied.",
                         major, minor);
        }
    }
    else
    {
        g_critical ("No RANDR extension found in display %s. Display settings won't be applied.",
                    gdk_display_get_name (helper->display));
    }
}



static void
xfce_displays_helper_dispose (GObject *object)
{
    XfceDisplaysHelper *helper = XFCE_DISPLAYS_HELPER (object);

    if (helper->handler > 0)
    {
        g_signal_handler_disconnect (G_OBJECT (helper->channel),
                                     helper->handler);
        helper->handler = 0;
    }

#ifdef HAVE_UPOWERGLIB
    if (helper->phandler > 0)
    {
        g_signal_handler_disconnect (G_OBJECT (helper->power),
                                     helper->phandler);
        g_object_unref (helper->power);
        helper->phandler = 0;
    }
#endif

    gdk_window_remove_filter (helper->root_window,
                              xfce_displays_helper_screen_on_event,
                              helper);

    if (helper->outputs)
    {
        g_ptr_array_unref (helper->outputs);
        helper->outputs = NULL;
    }

    if (helper->crtcs)
    {
        g_ptr_array_unref (helper->crtcs);
        helper->crtcs = NULL;
    }

    (*G_OBJECT_CLASS (xfce_displays_helper_parent_class)->dispose) (object);
}



static void
xfce_displays_helper_finalize (GObject *object)
{
    XfceDisplaysHelper *helper = XFCE_DISPLAYS_HELPER (object);

    /* Free the screen resources */
    if (helper->resources)
    {
        gdk_x11_display_error_trap_push (helper->display);
        XRRFreeScreenResources (helper->resources);
        gdk_display_flush (helper->display);
        if (gdk_x11_display_error_trap_pop (helper->display) != 0)
        {
            g_critical ("Failed to free screen resources");
        }
        helper->resources = NULL;
    }

    (*G_OBJECT_CLASS (xfce_displays_helper_parent_class)->finalize) (object);
}



static void
xfce_displays_helper_reload (XfceDisplaysHelper *helper)
{
    gint err;

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Refreshing RandR cache.");

    /* Free the caches */
    g_ptr_array_unref (helper->outputs);
    g_ptr_array_unref (helper->crtcs);

    gdk_x11_display_error_trap_push (helper->display);

    /* Free the screen resources */
    XRRFreeScreenResources (helper->resources);

    /* get the screen resource */
#ifdef HAS_RANDR_ONE_POINT_THREE
    /* xfce_displays_helper_reload () is usually called after a xrandr notification,
       which means that X is aware of the new hardware already. So, if possible,
       do not reprobe the hardware again. */
    if (helper->has_1_3)
        helper->resources = XRRGetScreenResourcesCurrent (helper->xdisplay,
                                                          GDK_WINDOW_XID (helper->root_window));
    else
#endif
    helper->resources = XRRGetScreenResources (helper->xdisplay,
                                               GDK_WINDOW_XID (helper->root_window));

    gdk_display_flush (helper->display);
    err = gdk_x11_display_error_trap_pop (helper->display);
    if (err)
        g_critical ("Failed to reload the RandR cache (err: %d).", err);

    /* recreate the caches */
    helper->crtcs = xfce_displays_helper_list_crtcs (helper);
    helper->outputs = xfce_displays_helper_list_outputs (helper);
}



static gchar **
xfce_displays_helper_get_display_infos (gint       noutput,
                                        Display   *xdisplay,
                                        GPtrArray *outputs)
{
    gchar    **display_infos;
    gint       m;
    guint8    *edid_data;

    display_infos = g_new0 (gchar *, noutput + 1);
    /* get all display edids, to only query randr once */
    for (m = 0; m < noutput; ++m)
    {
        XfceRROutput *output;

        output = g_ptr_array_index (outputs, m);
        edid_data = xfce_randr_read_edid_data (xdisplay, output->id);

        if (edid_data)
            display_infos[m] = g_compute_checksum_for_data (G_CHECKSUM_SHA1 , edid_data, 128);
        else
            display_infos[m] = g_strdup ("");
    }

    return display_infos;
}



static gchar *
xfce_displays_helper_get_matching_profile (XfceDisplaysHelper *helper)
{
    GList              *profiles = NULL;
    gpointer           *profile;
    gchar              *profile_name;
    gchar              *property;
    gchar             **display_infos;

    display_infos = xfce_displays_helper_get_display_infos (helper->outputs->len,
                                                            helper->xdisplay,
                                                            helper->outputs);
    if (display_infos)
    {
        profiles = display_settings_get_profiles (display_infos, helper->channel);
        g_strfreev (display_infos);
    }

    if (profiles == NULL)
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "No matching display profiles found.");
    }
    else if (g_list_length (profiles) == 1)
    {
        profile = g_list_nth_data (profiles, 0);
        property = g_strdup_printf ("/%s", (gchar *) profile);
        profile_name = xfconf_channel_get_string (helper->channel, property, NULL);
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Applied the only matching display profile: %s", profile_name);
        g_free (profile_name);
        g_free (property);
        return (gchar *)profile;
    }
    else
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Found %d matching display profiles.", g_list_length (profiles));
    }

    return NULL;
}



static GdkFilterReturn
xfce_displays_helper_screen_on_event (GdkXEvent *xevent,
                                      GdkEvent  *event,
                                      gpointer   data)
{
    XfceDisplaysHelper *helper = XFCE_DISPLAYS_HELPER (data);
    GPtrArray          *old_outputs;
    XfceRRCrtc         *crtc = NULL;
    XfceRROutput       *output, *o;
    XEvent             *e = xevent;
    gint                event_num;
    gint                j;
    guint               n, m, nactive = 0;
    guint               autoconnect_mode;
    gboolean            found = FALSE, changed = FALSE;

    if (!e)
        return GDK_FILTER_CONTINUE;

    event_num = e->type - helper->event_base;

    if (event_num == RRScreenChangeNotify)
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "RRScreenChangeNotify event received.");

        old_outputs = g_ptr_array_ref (helper->outputs);
        xfce_displays_helper_reload (helper);

        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Noutput: before = %d, after = %d.",
                        old_outputs->len, helper->outputs->len);

        autoconnect_mode = xfconf_channel_get_int (helper->channel, NOTIFY_PROP, 1);

        /* Check if we have different amount of outputs and a matching profile and
           apply it if there's only one */
        if (old_outputs->len > helper->outputs->len ||
            old_outputs->len < helper->outputs->len)
        {
            if (xfconf_channel_get_bool (helper->channel, AUTO_ENABLE_PROFILES, FALSE)
                && autoconnect_mode > 0)
            {
                gchar *matching_profile = NULL;

                matching_profile = xfce_displays_helper_get_matching_profile (helper);
                if (matching_profile)
                {
                    xfce_displays_helper_channel_apply (helper, matching_profile);
                    return GDK_FILTER_CONTINUE;
                }
            }
            xfconf_channel_set_string (helper->channel, ACTIVE_PROFILE, DEFAULT_SCHEME_NAME);
        }

        if (old_outputs->len > helper->outputs->len)
        {
            /* Diff the new and old output list to find removed outputs */
            for (n = 0; n < old_outputs->len; ++n)
            {
                found = FALSE;
                output = g_ptr_array_index (old_outputs, n);
                for (m = 0; m < helper->outputs->len && !found; ++m)
                {
                    o = g_ptr_array_index (helper->outputs, m);
                    found = o->id == output->id;
                }
                if (!found)
                {
                    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Output disconnected: %s",
                                    output->info->name);
                    /* force deconfiguring the crtc for the removed output */
                    if (output->info->crtc != None)
                        crtc = xfce_displays_helper_find_crtc_by_id (helper,
                                                                     output->info->crtc);
                    if (crtc)
                    {
                        crtc->mode = None;
                        xfce_displays_helper_disable_crtc (helper, crtc->id);
                    }
                    /* if the output was active, we must recalculate the screen size */
                    changed |= output->active;
                }
            }

            /* Basically, this means the external output was disconnected,
               so reenable the internal one if needed. */
            for (n = 0; n < helper->outputs->len; ++n)
            {
                output = g_ptr_array_index (helper->outputs, n);
                if (output->active)
                    ++nactive;
            }
            if (nactive == 0)
            {
                xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "No active output anymore! "
                                "Attempting to re-enable the internal output.");
                xfce_displays_helper_toggle_internal (NULL, FALSE, helper);
            }
            else if (changed)
                xfce_displays_helper_apply_all (helper);
        }
        else
        {
            /* Diff the new and old output list to find new outputs */
            for (n = 0; n < helper->outputs->len; ++n)
            {
                found = FALSE;
                output = g_ptr_array_index (helper->outputs, n);
                for (m = 0; m < old_outputs->len && !found; ++m)
                {
                    o = g_ptr_array_index (old_outputs, m);
                    found = o->id == output->id;
                }
                if (!found)
                {
                    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "New output connected: %s",
                                    output->info->name);
                    /* need to enable crtc for output ? */
                    if (output->info->crtc == None)
                    {
                        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "enabling crtc for %s", output->info->name);
                        crtc = xfce_displays_helper_find_usable_crtc (helper, output);
                        if (crtc)
                        {
                            crtc->mode = output->preferred_mode;
                            crtc->rotation = RR_Rotate_0;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                            if ((crtc->x > gdk_screen_width() + 1) || (crtc->y > gdk_screen_height() + 1)
                                || autoconnect_mode == 2) {
G_GNUC_END_IGNORE_DEPRECATIONS
                                crtc->x = crtc->y = 0;
                            }
                            /* Extend to the right if configured */
                            else if (autoconnect_mode == 3)
                            {
                                crtc->x = helper->width - crtc->width;
                                crtc->y = 0;
                            } /* else - leave values from last time we saw the monitor */
                            /* set width and height */
                            for (j = 0; j < helper->resources->nmode; ++j)
                            {
                                if (helper->resources->modes[j].id == output->preferred_mode)
                                {
                                    crtc->width = helper->resources->modes[j].width;
                                    crtc->height = helper->resources->modes[j].height;
                                    break;
                                }
                            }
                            xfce_displays_helper_set_outputs (crtc, output);
                            crtc->changed = TRUE;
                        }
                    }

                    changed = TRUE;
                }
            }
            if (changed)
                xfce_displays_helper_apply_all (helper);

            /* Start the minimal dialog according to the user preferences */
            if (changed && autoconnect_mode == 1)
                xfce_spawn_command_line (NULL, "xfce4-display-settings -m", FALSE, FALSE, TRUE, NULL);
        }
        g_ptr_array_unref (old_outputs);
    }

    /* Pass the event on to GTK+ */
    return GDK_FILTER_CONTINUE;
}



static void
xfce_displays_helper_set_screen_size (XfceDisplaysHelper *helper)
{
    gint min_width, min_height, max_width, max_height;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->xdisplay && helper->resources);

    /* get the screen size extremums */
    if (!XRRGetScreenSizeRange (helper->xdisplay, GDK_WINDOW_XID (helper->root_window),
                                &min_width, &min_height, &max_width, &max_height))
    {
        g_warning ("Unable to get the range of screen sizes. "
                   "Display settings may fail to apply.");
        return;
    }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "min_h = %d, min_w = %d, max_h = %d, max_w = %d, "
                    "prev_h = %d, prev_w = %d, prev_hmm = %d, prev_wmm = %d, h = %d, w = %d, "
                    "hmm = %d, wmm = %d.", min_height, min_width, max_height, max_width,
                    gdk_screen_height (), gdk_screen_width (), gdk_screen_height_mm (),
                    gdk_screen_width_mm (), helper->height, helper->width, helper->mm_height,
                    helper->mm_width);
    if (helper->width > max_width || helper->height > max_height)
    {
        g_warning ("Your screen can't handle the requested size. "
                   "%dx%d exceeds the maximum: %dx%d",
                   helper->width, helper->height, max_width, max_height);
    }
    /* set the screen size only if it's really needed and valid */
    if (helper->width >= min_width && helper->width <= max_width
        && helper->height >= min_height && helper->height <= max_height
        && (helper->width != gdk_screen_width ()
            || helper->height != gdk_screen_height ()
            || helper->mm_width != gdk_screen_width_mm ()
            || helper->mm_height != gdk_screen_height_mm ()))
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Applying desktop dimensions: %dx%d (px), %dx%d (mm).",
                        helper->width, helper->height, helper->mm_width, helper->mm_height);
        XRRSetScreenSize (helper->xdisplay, GDK_WINDOW_XID (helper->root_window),
                          helper->width, helper->height, helper->mm_width, helper->mm_height);
    }
G_GNUC_END_IGNORE_DEPRECATIONS
}



static gboolean
xfce_displays_helper_load_from_xfconf (XfceDisplaysHelper *helper,
                                       const gchar        *scheme,
                                       GHashTable         *saved_outputs,
                                       XfceRROutput       *output)
{
    XfceRRCrtc  *crtc = NULL;
    GValue      *value;
    const gchar *str_value;
    gchar        property[512];
    gdouble      output_rate, rate;
    gdouble      scalex, scaley;
    RRMode       valid_mode;
    Rotation     rot;
    gint         x, y, n, m, int_value;
    gboolean     active;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->resources && output);

    active = output->active;

    /* does this output exist in xfconf? */
    g_snprintf (property, sizeof (property), OUTPUT_FMT, scheme, output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);

    if (value == NULL || !G_VALUE_HOLDS_STRING (value))
        return active;

#ifdef HAS_RANDR_ONE_POINT_THREE
    if (helper->has_1_3)
    {
        /* is it the primary output? */
        g_snprintf (property, sizeof (property), PRIMARY_PROP, scheme,
                    output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);
        if (G_VALUE_HOLDS_BOOLEAN (value) && g_value_get_boolean (value))
            helper->primary = output->id;
    }
#endif

    /* status */
    g_snprintf (property, sizeof (property), ACTIVE_PROP, scheme, output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);

    if (value == NULL || !G_VALUE_HOLDS_BOOLEAN (value))
        return active;

    /* Get the associated CRTC */
    crtc = xfce_displays_helper_find_usable_crtc (helper, output);
    if (!crtc)
        return active;

    /* disable inactive outputs */
    if (!g_value_get_boolean (value))
    {
        if (crtc->mode != None)
        {
            active = FALSE;
            xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "%s will be disabled by configuration.",
                            output->info->name);

            crtc->mode = None;
            crtc->noutput = 0;
            crtc->changed = TRUE;
        }
        return active;
    }

    /* rotation */
    g_snprintf (property, sizeof (property), ROTATION_PROP, scheme, output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (G_VALUE_HOLDS_INT (value))
        int_value = g_value_get_int (value);
    else
        int_value = 0;

    /* convert to a Rotation */
    switch (int_value)
    {
        case 90:  rot = RR_Rotate_90;  break;
        case 180: rot = RR_Rotate_180; break;
        case 270: rot = RR_Rotate_270; break;
        default:  rot = RR_Rotate_0;   break;
    }

    /* reflection */
    g_snprintf (property, sizeof (property), REFLECTION_PROP, scheme, output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (G_VALUE_HOLDS_STRING (value))
        str_value = g_value_get_string (value);
    else
        str_value = "0";

    /* convert to a Rotation */
    if (g_strcmp0 (str_value, "X") == 0)
        rot |= RR_Reflect_X;
    else if (g_strcmp0 (str_value, "Y") == 0)
        rot |= RR_Reflect_Y;
    else if (g_strcmp0 (str_value, "XY") == 0)
        rot |= (RR_Reflect_X|RR_Reflect_Y);

    /* check rotation support */
    if ((crtc->rotations & rot) == 0)
    {
        g_warning ("Unsupported rotation for %s. Fallback to RR_Rotate_0.", output->info->name);
        rot = RR_Rotate_0;
    }

    /* update CRTC rotation */
    if (crtc->rotation != rot)
    {
        crtc->rotation = rot;
        crtc->changed = TRUE;
    }

    /* resolution */
    g_snprintf (property, sizeof (property), RESOLUTION_PROP, scheme, output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (value == NULL || !G_VALUE_HOLDS_STRING (value))
        str_value = "";
    else
        str_value = g_value_get_string (value);

    /* refresh rate */
    g_snprintf (property, sizeof (property), RRATE_PROP, scheme, output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (G_VALUE_HOLDS_DOUBLE (value))
        output_rate = g_value_get_double (value);
    else
        output_rate = 0.0;

#ifdef HAS_RANDR_ONE_POINT_THREE
    if (helper->has_1_3)
    {
        /* scaling X */
        g_snprintf (property, sizeof (property), SCALEX_PROP, scheme,
                    output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);
        if (G_VALUE_HOLDS_DOUBLE (value))
            scalex = g_value_get_double (value);
        else
            scalex = 1.0;

        /* scaling Y */
        g_snprintf (property, sizeof (property), SCALEY_PROP, scheme,
                    output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);
        if (G_VALUE_HOLDS_DOUBLE (value))
            scaley = g_value_get_double (value);
        else
            scaley = 1.0;

        if (scalex <= 0.0 || scaley <= 0.0) {
            scalex = 1.0;
            scaley = 1.0;
        }

        if (crtc->scalex != scalex || crtc->scaley != scaley)
        {
            crtc->scalex = scalex;
            crtc->scaley = scaley;
            crtc->changed = TRUE;
        }
    }
#endif

    /* check mode validity */
    valid_mode = None;
    for (n = 0; n < output->info->nmode; ++n)
    {
        /* walk all modes */
        for (m = 0; m < helper->resources->nmode; ++m)
        {
            /* does the mode info match the mode we seek? */
            if (helper->resources->modes[m].id != output->info->modes[n])
                continue;

            /* calculate the refresh rate */
            rate = (gdouble) helper->resources->modes[m].dotClock /
                    ((gdouble) helper->resources->modes[m].hTotal * (gdouble) helper->resources->modes[m].vTotal);

            /* construct a string equivalent to the mode generated in displays */
            /* property is the resources mode translated into display panel name */
            g_snprintf (property, sizeof (property), "%dx%d", helper->resources->modes[m].width,
                        helper->resources->modes[m].height);

            /* find the mode corresponding to the saved values */
            if (rint (rate * 100) == rint (output_rate * 100)
                && (g_strcmp0 (property, str_value) == 0))
            {
                valid_mode = helper->resources->modes[m].id;
                break;
            }
        }
        /* found it */
        if (valid_mode != None)
            break;
    }

    if (valid_mode == None)
    {
        /* unsupported mode, abort for this output */
        g_warning ("Unknown mode '%s @ %.1f' for output %s, aborting.",
                   str_value, output_rate, output->info->name);
        return active;
    }
    else if (crtc->mode != valid_mode)
    {
        if (crtc->mode == None)
            active = TRUE;

        /* update CRTC mode */
        crtc->mode = valid_mode;
        crtc->changed = TRUE;
    }

    /* recompute dimensions according to the selected rotation */
    if ((crtc->rotation & (RR_Rotate_90|RR_Rotate_270)) != 0)
    {
        crtc->width = helper->resources->modes[m].height;
        crtc->height = helper->resources->modes[m].width;
    }
    else
    {
        crtc->width = helper->resources->modes[m].width;
        crtc->height = helper->resources->modes[m].height;
    }

    /* position, x */
    g_snprintf (property, sizeof (property), POSX_PROP, scheme,
                output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (G_VALUE_HOLDS_INT (value))
        x = g_value_get_int (value);
    else
        x = 0;

    /* position, y */
    g_snprintf (property, sizeof (property), POSY_PROP, scheme,
                output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (G_VALUE_HOLDS_INT (value))
        y = g_value_get_int (value);
    else
        y = 0;

    /* update CRTC position */
    if (crtc->x != x || crtc->y != y)
    {
        crtc->x = x;
        crtc->y = y;
        crtc->changed = TRUE;
    }

    xfce_displays_helper_set_outputs (crtc, output);

    return active;
}



static GPtrArray *
xfce_displays_helper_list_outputs (XfceDisplaysHelper *helper)
{
    GPtrArray     *outputs;
    XRROutputInfo *output_info;
    XfceRROutput  *output;
    XfceRRCrtc    *crtc;
    gint           best_dist, dist, n, m, l, err;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->xdisplay && helper->resources);

    /* get all connected outputs */
    outputs = g_ptr_array_new_with_free_func ((GDestroyNotify) xfce_displays_helper_free_output);
    for (n = 0; n < helper->resources->noutput; ++n)
    {
        gdk_x11_display_error_trap_push (helper->display);
        output_info = XRRGetOutputInfo (helper->xdisplay, helper->resources, helper->resources->outputs[n]);
        gdk_display_flush (helper->display);
        err = gdk_x11_display_error_trap_pop (helper->display);
        if (err || !output_info)
        {
            g_warning ("Failed to load info for output %lu (err: %d). Skipping.",
                       helper->resources->outputs[n], err);
            continue;
        }

        if (output_info->connection != RR_Connected)
        {
            XRRFreeOutputInfo (output_info);
            continue;
        }

        output = g_new0 (XfceRROutput, 1);
        output->id = helper->resources->outputs[n];
        output->info = output_info;

        /* find the preferred mode */
        output->preferred_mode = None;
        best_dist = 0;
        for (l = 0; l < output->info->nmode; ++l)
        {
            /* walk all modes */
            for (m = 0; m < helper->resources->nmode; ++m)
            {
                /* does the mode info match the mode we seek? */
                if (helper->resources->modes[m].id != output->info->modes[l])
                    continue;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                if (l < output->info->npreferred)
                    dist = 0;
                else if ((output->info->mm_height != 0) && (gdk_screen_height_mm () != 0))
                    dist = (1000 * gdk_screen_height () / gdk_screen_height_mm () -
                            1000 * helper->resources->modes[m].height / output->info->mm_height);
                else
                    dist = gdk_screen_height () - helper->resources->modes[m].height;
G_GNUC_END_IGNORE_DEPRECATIONS

                dist = ABS (dist);

                if (output->preferred_mode == None || dist < best_dist)
                {
                    output->preferred_mode = helper->resources->modes[m].id;
                    best_dist = dist;
                }
            }
        }

        /* track active outputs */
        crtc = xfce_displays_helper_find_crtc_by_id (helper, output->info->crtc);
        output->active = crtc && crtc->mode != None;

        /* Translate output->name into xfconf compatible format in place */
        g_strcanon(output->info->name, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_<>", '_');

        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Detected output %lu %s.", output->id,
                        output->info->name);

        /* cache it */
        g_ptr_array_add (outputs, output);
    }

    return outputs;
}



static void
xfce_displays_helper_free_output (XfceRROutput *output)
{
    if (output == NULL)
        return;

    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    XRRFreeOutputInfo (output->info);
    gdk_display_flush (gdk_display_get_default ());
    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
    {
        g_critical ("Failed to free output info");
    }
    g_free (output);
}



static GPtrArray *
xfce_displays_helper_list_crtcs (XfceDisplaysHelper *helper)
{
    GPtrArray   *crtcs;
    XRRCrtcInfo *crtc_info;
    XfceRRCrtc  *crtc;
    gint         n, err;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->xdisplay && helper->resources);

    /* get all existing CRTCs */
    crtcs = g_ptr_array_new_with_free_func ((GDestroyNotify) xfce_displays_helper_free_crtc);
    for (n = 0; n < helper->resources->ncrtc; ++n)
    {
        XRRCrtcTransformAttributes  *attr;
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Detected CRTC %lu.", helper->resources->crtcs[n]);

        gdk_x11_display_error_trap_push (helper->display);
        crtc_info = XRRGetCrtcInfo (helper->xdisplay, helper->resources, helper->resources->crtcs[n]);
        gdk_display_flush (helper->display);
        err = gdk_x11_display_error_trap_pop (helper->display);
        if (err || !crtc_info)
        {
            g_warning ("Failed to load info for CRTC %lu (err: %d). Skipping.",
                       helper->resources->crtcs[n], err);
            continue;
        }

        crtc = g_new0 (XfceRRCrtc, 1);
        crtc->id = helper->resources->crtcs[n];
        crtc->mode = crtc_info->mode;
        crtc->rotation = crtc_info->rotation;
        crtc->rotations = crtc_info->rotations;
        crtc->width = crtc_info->width;
        crtc->height = crtc_info->height;
        crtc->x = crtc_info->x;
        crtc->y = crtc_info->y;
        if (XRRGetCrtcTransform (helper->xdisplay, helper->resources->crtcs[n], &attr) && attr)
        {
            crtc->scalex = XFixedToDouble (attr->currentTransform.matrix[0][0]);
            crtc->scaley = XFixedToDouble (attr->currentTransform.matrix[1][1]);
            XFree (attr);
        }
        else
        {
            crtc->scalex = 1.0;
            crtc->scaley = 1.0;
        }

        crtc->noutput = crtc_info->noutput;
        crtc->outputs = NULL;
        if (crtc_info->noutput > 0)
            crtc->outputs = g_memdup (crtc_info->outputs,
                                      crtc_info->noutput * sizeof (RROutput));

        crtc->npossible = crtc_info->npossible;
        crtc->possible = NULL;
        if (crtc_info->npossible > 0)
            crtc->possible = g_memdup (crtc_info->possible,
                                       crtc_info->npossible * sizeof (RROutput));

        crtc->changed = FALSE;
        XRRFreeCrtcInfo (crtc_info);

        /* cache it */
        g_ptr_array_add (crtcs, crtc);
    }

    return crtcs;
}



static XfceRRCrtc *
xfce_displays_helper_find_crtc_by_id (XfceDisplaysHelper *helper,
                                      RRCrtc              id)
{
    XfceRRCrtc *crtc;
    guint       n;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->crtcs);

    for (n = 0; n < helper->crtcs->len; ++n)
    {
        crtc = g_ptr_array_index (helper->crtcs, n);
        if (crtc->id == id)
            return crtc;
    }

    return NULL;
}



static void
xfce_displays_helper_free_crtc (XfceRRCrtc *crtc)
{
    if (crtc == NULL)
        return;

    if (crtc->outputs != NULL)
        g_free (crtc->outputs);
    if (crtc->possible != NULL)
        g_free (crtc->possible);
    g_free (crtc);
}



static XfceRRCrtc *
xfce_displays_helper_find_usable_crtc (XfceDisplaysHelper *helper,
                                       XfceRROutput       *output)
{
    XfceRRCrtc *crtc = NULL;
    guint       n;
    gint        m;
    gboolean    found = FALSE;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->crtcs && output);

    if (output->info->crtc != None)
    {
        crtc = xfce_displays_helper_find_crtc_by_id (helper, output->info->crtc);
        found = crtc != NULL;
    }

    /* try to find one that is not already used by another output */
    for (n = 0; n < helper->crtcs->len && !found; ++n)
    {
        crtc = g_ptr_array_index (helper->crtcs, n);
        if (crtc->noutput > 0 || crtc->changed)
            continue;

        for (m = 0; m < crtc->npossible; ++m)
        {
            if (crtc->possible[m] == output->id)
            {
                found = TRUE;
                break;
            }
        }
    }

    if (found)
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "CRTC %lu assigned to %s.", crtc->id,
                        output->info->name);
    else
        g_warning ("No available CRTC for %s.", output->info->name);

    return crtc;
}



static void
xfce_displays_helper_get_topleftmost_pos (XfceRRCrtc         *crtc,
                                          XfceDisplaysHelper *helper)
{
    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && crtc);

    if (crtc->mode == None)
        return;

    /* used to normalize positions later */
    helper->min_x = MIN (helper->min_x, crtc->x);
    helper->min_y = MIN (helper->min_y, crtc->y);
}



static void
xfce_displays_helper_normalize_crtc (XfceRRCrtc         *crtc,
                                     XfceDisplaysHelper *helper)
{
    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && crtc);

    /* ignore disabled outputs for size computations */
    if (crtc->mode == None)
        return;

    /* normalize positions to ensure the upper left corner is at (0,0) */
    if (helper->min_x || helper->min_y)
    {
        crtc->x -= helper->min_x;
        crtc->y -= helper->min_y;
        crtc->changed = TRUE;
    }

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Normalized CRTC %lu: size=%dx%d, pos=%dx%d.",
                    crtc->id, crtc->width, crtc->height, crtc->x, crtc->y);

    /* calculate the total screen size */
    helper->width = MAX (helper->width, crtc->x + crtc->width * crtc->scalex);
    helper->height = MAX (helper->height, crtc->y + crtc->height * crtc->scaley);

    /* The 'physical size' of an X screen is meaningless if that screen
     * can consist of many monitors. So just pick a size that make the
     * dpi 96.
     *
     * Firefox and Evince apparently believe what X tells them.
     */
    helper->mm_width = (helper->width / 96.0) * 25.4 + 0.5;
    helper->mm_height = (helper->height / 96.0) * 25.4 + 0.5;
}



static Status
xfce_displays_helper_disable_crtc (XfceDisplaysHelper *helper,
                                   RRCrtc              crtc)
{
    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->xdisplay && helper->resources);

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Disabling CRTC %lu.", crtc);

    return XRRSetCrtcConfig (helper->xdisplay, helper->resources, crtc,
                             CurrentTime, 0, 0, None, RR_Rotate_0, NULL, 0);
}



static void
xfce_displays_helper_workaround_crtc_size (XfceRRCrtc         *crtc,
                                           XfceDisplaysHelper *helper)
{
    XRRCrtcInfo *crtc_info;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->xdisplay && helper->resources && crtc);

    /* The CRTC needs to be disabled if its previous mode won't fit in the new screen.
       It will be reenabled with its new mode (known to fit) after the screen size is
       changed, unless the user disabled it (no need to reenable it then). */
    crtc_info = XRRGetCrtcInfo (helper->xdisplay, helper->resources, crtc->id);
    if ((crtc_info->x + crtc_info->width > (guint) helper->width) ||
        (crtc_info->y + crtc_info->height > (guint) helper->height))
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "CRTC %lu must be temporarily disabled.", crtc->id);
        if (xfce_displays_helper_disable_crtc (helper, crtc->id) == RRSetConfigSuccess)
            crtc->changed = (crtc->mode != None);
        else
            g_warning ("Failed to temporarily disable CRTC %lu.", crtc->id);
    }
    XRRFreeCrtcInfo (crtc_info);
}



static void
xfce_displays_helper_apply_crtc_transform (XfceRRCrtc         *crtc,
                                           XfceDisplaysHelper *helper)
{
    XTransform transform;
    gchar *filter;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->xdisplay && crtc);

    if (!crtc->changed)
        return;

#ifdef HAS_RANDR_ONE_POINT_THREE
    if (helper->has_1_3)
    {
        if (crtc->scalex == 1 && crtc->scaley == 1)
            filter = "nearest";
        else
            filter = "bilinear";

        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Applying CRTC %lu Transform: x=%lf y=%lf, filter=%s.", crtc->id,
                        crtc->scalex, crtc->scaley, filter);

        memset(&transform, '\0', sizeof(transform));

        transform.matrix[0][0] = XDoubleToFixed(crtc->scalex);
        transform.matrix[1][1] = XDoubleToFixed(crtc->scaley);
        transform.matrix[2][2] = XDoubleToFixed(1.0);

        gdk_x11_display_error_trap_push (helper->display);
        XRRSetCrtcTransform(helper->xdisplay, crtc->id,
                            &transform,
                            filter,
                            NULL,
                            0);
        if (gdk_x11_display_error_trap_pop (helper->display) != 0)
        {
            g_warning ("Failed to apply the scale, maybe the CRTC does not support transforms");
        }
    }
#endif
}



static void
xfce_displays_helper_apply_crtc (XfceRRCrtc         *crtc,
                                 XfceDisplaysHelper *helper)
{
    Status ret;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->xdisplay && helper->resources && crtc);

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Configuring CRTC %lu.", crtc->id);

    /* check if we really need to do something */
    if (crtc->changed)
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Applying changes to CRTC %lu.", crtc->id);

        if (crtc->mode == None) {
            ret = xfce_displays_helper_disable_crtc (helper, crtc->id);
        } else {
            xfce_displays_helper_apply_crtc_transform (crtc, helper);

            ret = XRRSetCrtcConfig (helper->xdisplay, helper->resources, crtc->id,
                                    CurrentTime, crtc->x, crtc->y, crtc->mode,
                                    crtc->rotation, crtc->outputs, crtc->noutput);
        }

        if (ret == RRSetConfigSuccess)
            crtc->changed = FALSE;
        else
            g_warning ("Failed to configure CRTC %lu.", crtc->id);
    }
}



static void
xfce_displays_helper_set_outputs (XfceRRCrtc   *crtc,
                                  XfceRROutput *output)
{
    gint n;

    g_assert (crtc && output);

    for (n = 0; n < crtc->noutput; ++n)
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "CRTC %lu, output list[%d] -> %lu.", crtc->id, n,
                        crtc->outputs[n]);

    /* check if the output is already present */
    for (n = 0; n < crtc->noutput; ++n)
    {
        if (crtc->outputs[n] == output->id)
            return;
    }


    if (crtc->outputs)
        crtc->outputs = g_realloc (crtc->outputs, (crtc->noutput + 1) * sizeof (RROutput));
    else
        crtc->outputs = g_new0 (RROutput, 1);

    g_assert (crtc->outputs);

    crtc->outputs [crtc->noutput++] = output->id;
    crtc->changed = TRUE;

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "CRTC %lu, output list[%d] -> %lu.", crtc->id,
                    crtc->noutput - 1, crtc->outputs[crtc->noutput - 1]);
}



static void
xfce_displays_helper_apply_all (XfceDisplaysHelper *helper)
{
    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->crtcs);

    helper->mm_width = helper->mm_height = helper->width = helper->height = 0;
    helper->min_x = helper->min_y = 32768;

    /* normalization and screen size calculation */
    g_ptr_array_foreach (helper->crtcs, (GFunc) xfce_displays_helper_get_topleftmost_pos, helper);
    g_ptr_array_foreach (helper->crtcs, (GFunc) xfce_displays_helper_normalize_crtc, helper);

    gdk_x11_display_error_trap_push (helper->display);

    /* grab server to prevent clients from thinking no output is enabled */
    gdk_x11_display_grab (helper->display);

    /* disable CRTCs that won't fit in the new screen */
    g_ptr_array_foreach (helper->crtcs, (GFunc) xfce_displays_helper_workaround_crtc_size, helper);

    /* set the screen size only if it's really needed and valid */
    xfce_displays_helper_set_screen_size (helper);

    /* final loop, apply crtc changes */
    g_ptr_array_foreach (helper->crtcs, (GFunc) xfce_displays_helper_apply_crtc, helper);

#ifdef HAS_RANDR_ONE_POINT_THREE
        if (helper->has_1_3)
            XRRSetOutputPrimary (helper->xdisplay, GDK_WINDOW_XID (helper->root_window),
                                 helper->primary);
#endif

    /* release the grab, changes are done */
    gdk_display_sync (helper->display);
    gdk_x11_display_ungrab (helper->display);
    if (gdk_x11_display_error_trap_pop (helper->display) != 0)
    {
        g_critical ("Failed to apply display settings");
    }
}



static void
xfce_displays_helper_channel_apply (XfceDisplaysHelper *helper,
                                    const gchar        *scheme)
{
    gchar       property[512];
    guint       n, nactive;
    GHashTable *saved_outputs;

    saved_outputs = NULL;
#ifdef HAS_RANDR_ONE_POINT_THREE
    helper->primary = None;
#endif

    xfconf_channel_set_string (helper->channel, ACTIVE_PROFILE, scheme);

    /* finally the list of saved outputs from xfconf */
    g_snprintf (property, sizeof (property), "/%s", scheme);
    saved_outputs = xfconf_channel_get_properties (helper->channel, property);

    /* nothing saved, nothing to do */
    if (saved_outputs == NULL)
        goto err_cleanup;

    /* first loop, loads all the outputs, and gets the number of active ones */
    nactive = 0;
    for (n = 0; n < helper->outputs->len; ++n)
    {
        if (xfce_displays_helper_load_from_xfconf (helper, scheme, saved_outputs,
                                                   g_ptr_array_index (helper->outputs,
                                                                      n)))
            ++nactive;
    }

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Total %d active output(s).", nactive);

    /* safety check */
    if (nactive == 0)
    {
        g_warning ("Stored Xfconf properties disable all outputs, aborting.");
        goto err_cleanup;
    }

    /* apply settings */
    xfce_displays_helper_apply_all (helper);

err_cleanup:
    /* Free the xfconf properties */
    if (saved_outputs)
        g_hash_table_destroy (saved_outputs);
}



static void
xfce_displays_helper_channel_property_changed (XfconfChannel      *channel,
                                               const gchar        *property_name,
                                               const GValue       *value,
                                               XfceDisplaysHelper *helper)
{
    if (G_UNLIKELY (G_VALUE_HOLDS_STRING (value) &&
        g_strcmp0 (property_name, APPLY_SCHEME_PROP) == 0))
    {
        /* apply */
        xfce_displays_helper_channel_apply (helper, g_value_get_string (value));
        /* remove the apply property */
        xfconf_channel_reset_property (channel, APPLY_SCHEME_PROP, FALSE);
    }
}



static void
xfce_displays_helper_toggle_internal (gpointer           *power,
                                      gboolean            lid_is_closed,
                                      XfceDisplaysHelper *helper)
{
    GHashTable    *saved_outputs;
    XfceRRCrtc    *crtc = NULL;
    XfceRROutput  *output, *lvds = NULL;
    gboolean       active = FALSE;
    guint          n;
    gint           m;

    for (n = 0; n < helper->outputs->len; ++n)
    {
        output = g_ptr_array_index (helper->outputs, n);
        g_assert (output);

        /* Try to find the internal display */
        if (g_str_has_prefix (output->info->name, "LVDS")
            || g_str_has_prefix (output->info->name, "eDP")
            || strcmp (output->info->name, "PANEL") == 0)
        {
            lvds = output;
            break;
        }
    }

    if (!lvds)
        return;

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Toggling internal output %s.",
                    lvds->info->name);

    if (lvds->active && lid_is_closed)
    {
        /* if active and the lid is closed, deactivate it */
        crtc = xfce_displays_helper_find_usable_crtc (helper, lvds);
        if (!crtc)
            return;
        crtc->mode = None;
        crtc->noutput = 0;
        crtc->changed = TRUE;
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "%s will be disabled.", lvds->info->name);
    }
    else if (!lvds->active && !lid_is_closed)
    {
        /* re-activate it because the user opened the lid */
        saved_outputs = xfconf_channel_get_properties (helper->channel, "/" DEFAULT_SCHEME_NAME);
        if (saved_outputs)
        {
            /* first, ensure the position of the other outputs is correct */
            for (n = 0; n < helper->outputs->len; ++n)
            {
                output = g_ptr_array_index (helper->outputs, n);
                g_assert (output);

                if (output->id == lvds->id)
                    continue;

                xfce_displays_helper_load_from_xfconf (helper, DEFAULT_SCHEME_NAME,
                                                       saved_outputs, output);
            }

            /* try to load user saved settings for lvds */
            active = xfce_displays_helper_load_from_xfconf (helper, DEFAULT_SCHEME_NAME,
                                                            saved_outputs, lvds);
            g_hash_table_destroy (saved_outputs);
        }
        if (!active)
        {
            /* autoset the preferred mode */
            crtc = xfce_displays_helper_find_usable_crtc (helper, lvds);
            if (!crtc)
                return;
            crtc->mode = lvds->preferred_mode;
            crtc->rotation = RR_Rotate_0;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            if ((crtc->x > gdk_screen_width() + 1) || (crtc->y > gdk_screen_height() + 1)) {
G_GNUC_END_IGNORE_DEPRECATIONS
                crtc->x = crtc->y = 0;
            } /* else - leave values from last time we saw the monitor */
            /* set width and height */
            for (m = 0; m < helper->resources->nmode; ++m)
            {
                if (helper->resources->modes[m].id == lvds->preferred_mode)
                {
                    crtc->width = helper->resources->modes[m].width;
                    crtc->height = helper->resources->modes[m].height;
                    break;
                }
            }
            xfce_displays_helper_set_outputs (crtc, lvds);
            crtc->changed = TRUE;
        }
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "%s will be re-enabled.",
                        lvds->info->name);
    }
    else
        return;

    /* apply settings */
    xfce_displays_helper_apply_all (helper);
}
