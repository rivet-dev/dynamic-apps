/*
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
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

#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#endif

#include "debug.h"
#include "workspaces.h"

#define WORKSPACES_CHANNEL    "xfwm4"
#define WORKSPACE_NAMES_PROP  "/general/workspace_names"
#define WORKSPACE_COUNT_PROP  "/general/workspace_count"



static void             xfce_workspaces_helper_finalize     (GObject              *object);
static guint            xfce_workspaces_helper_get_count    (void);
static GdkFilterReturn  xfce_workspaces_helper_filter_func  (GdkXEvent            *gdkxevent,
                                                             GdkEvent             *event,
                                                             gpointer              user_data);
static GPtrArray       *xfce_workspaces_helper_get_names    (void);
static void             xfce_workspaces_helper_set_names    (XfceWorkspacesHelper *helper,
                                                             gboolean              disable_wm_check);
static void             xfce_workspaces_helper_save_names   (XfceWorkspacesHelper *helper);
static void             xfce_workspaces_helper_prop_changed (XfconfChannel        *channel,
                                                             const gchar          *property,
                                                             const GValue         *value,
                                                             XfceWorkspacesHelper *helper);



struct _XfceWorkspacesHelper
{
    GObject parent;

    XfconfChannel *channel;

    gint64         timestamp;

#ifdef GDK_WINDOWING_X11
    guint          wait_for_wm_timeout_id;
#endif
};

struct _XfceWorkspacesHelperClass
{
    GObjectClass parent;
};

#ifdef GDK_WINDOWING_X11
static Atom atom_net_number_of_desktops = 0;
static Atom atom_net_desktop_names = 0;
static gboolean xfsettingsd_disable_wm_check = FALSE;

typedef struct
{
  XfceWorkspacesHelper *helper;

  Display              *dpy;
  Atom                 *atoms;
  guint                 atom_count;
  guint                 have_wm : 1;
  guint                 counter;
}
WaitForWM;
#endif



G_DEFINE_TYPE(XfceWorkspacesHelper, xfce_workspaces_helper, G_TYPE_OBJECT)



static void
xfce_workspaces_helper_class_init(XfceWorkspacesHelperClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = xfce_workspaces_helper_finalize;

#ifdef GDK_WINDOWING_X11
    atom_net_number_of_desktops = gdk_x11_get_xatom_by_name ("_NET_NUMBER_OF_DESKTOPS");
    atom_net_desktop_names = gdk_x11_get_xatom_by_name ("_NET_DESKTOP_NAMES");
#endif
}


static void
xfce_workspaces_helper_init (XfceWorkspacesHelper *helper)
{
    GdkWindow    *root_window;
    GdkEventMask  events;

    helper->channel = xfconf_channel_get(WORKSPACES_CHANNEL);

    /* monitor root window property changes */
    root_window = gdk_get_default_root_window ();
    events = gdk_window_get_events (root_window);
    gdk_window_set_events (root_window, events | GDK_PROPERTY_CHANGE_MASK);
    gdk_window_add_filter (root_window, xfce_workspaces_helper_filter_func, helper);

    xfce_workspaces_helper_set_names (helper, FALSE);

    g_signal_connect (G_OBJECT(helper->channel),
                      "property-changed::" WORKSPACE_NAMES_PROP,
                      G_CALLBACK( xfce_workspaces_helper_prop_changed), helper);
}



static void
xfce_workspaces_helper_finalize (GObject *object)
{
    XfceWorkspacesHelper *helper = XFCE_WORKSPACES_HELPER (object);

    g_signal_handlers_disconnect_by_func(G_OBJECT (helper->channel),
                                         G_CALLBACK (xfce_workspaces_helper_prop_changed),
                                         helper);

    G_OBJECT_CLASS (xfce_workspaces_helper_parent_class)->finalize (object);
}



static GdkFilterReturn
xfce_workspaces_helper_filter_func (GdkXEvent  *gdkxevent,
                                    GdkEvent   *event,
                                    gpointer    user_data)
{
#ifdef GDK_WINDOWING_X11
    XfceWorkspacesHelper  *helper = XFCE_WORKSPACES_HELPER (user_data);
    XEvent                *xevent = gdkxevent;

    if (xevent->type == PropertyNotify)
    {
        if (xevent->xproperty.atom == atom_net_number_of_desktops)
        {
            /* new workspace was added or removed */
            xfce_workspaces_helper_set_names (helper, TRUE);

            xfsettings_dbg (XFSD_DEBUG_WORKSPACES, "number of desktops changed");
        }
        else if (xevent->xproperty.atom == atom_net_desktop_names)
        {
            /* don't respond to our own name changes (1 sec) */
            if (g_get_real_time () > helper->timestamp)
            {
                /* someone changed (possibly another application that does
                 * not update xfconf) the name of a desktop, store the
                 * new names in xfconf if different*/
                xfce_workspaces_helper_save_names (helper);

                xfsettings_dbg (XFSD_DEBUG_WORKSPACES, "someone else changed the desktop names");
            }
        }
    }
#endif

    return GDK_FILTER_CONTINUE;
}



static GPtrArray *
xfce_workspaces_helper_get_names (void)
{
    gboolean     succeed;
    GdkAtom      utf8_atom, type_returned;
    gint         i, length, num;
    GPtrArray   *names = NULL;
    gchar       *data = NULL;
    GValue      *val;
    const gchar *p;

    gdk_x11_display_error_trap_push (gdk_display_get_default ());

    utf8_atom = gdk_atom_intern_static_string ("UTF8_STRING");
    succeed = gdk_property_get (gdk_get_default_root_window (),
                                gdk_atom_intern_static_string ("_NET_DESKTOP_NAMES"),
                                utf8_atom,
                                0L, G_MAXLONG,
                                FALSE, &type_returned, NULL, &length,
                                (guchar **) &data);

    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) == 0
        && succeed
        && type_returned == utf8_atom
        && data != NULL
        && length > 0)
    {
        names = g_ptr_array_new ();

        for (i = 0, num = 0; i < length - 1;)
        {
            p = data + i;

            if (!g_utf8_validate (p, -1, NULL))
            {
                g_warning ("Name of workspace %d is not UTF-8 valid.", num + 1);
                xfconf_array_free (names);
                g_free (data);

                return NULL;
            }

            val = g_new0 (GValue, 1);
            g_value_init (val, G_TYPE_STRING);
            g_value_set_string (val, p);
            g_ptr_array_add (names, val);

            i += strlen (p) + 1;
            num++;
        }
    }

    g_free (data);

    return names;
}



static guint
xfce_workspaces_helper_get_count (void)
{
    guint     result = 0;
    guchar   *data = NULL;
    gboolean  succeed;
    GdkAtom   cardinal_atom, type_returned;
    gint      format_returned;

    gdk_x11_display_error_trap_push (gdk_display_get_default ());

    cardinal_atom = gdk_atom_intern_static_string ("CARDINAL");
    succeed = gdk_property_get (gdk_get_default_root_window (),
                                gdk_atom_intern_static_string ("_NET_NUMBER_OF_DESKTOPS"),
                                cardinal_atom,
                                0L, 1L,
                                FALSE, &type_returned, &format_returned, NULL,
                                &data);

    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) == 0
        && succeed
        && data != NULL
        && type_returned == cardinal_atom
        && format_returned == 32)
    {
        result = (guint) *data;
    }
    else
    {
        g_warning ("Failed to get the _NET_NUMBER_OF_DESKTOPS property.");
    }

    g_free (data);

    return result;
}



static void
xfce_workspaces_helper_set_names_real (XfceWorkspacesHelper *helper)
{
    GString       *names_str;
    guint          i;
    guint          n_workspaces;
    GPtrArray     *names, *existing_names;
    GValue        *val;
    gchar         *new_name;
    const gchar   *name;

    g_return_if_fail (XFCE_IS_WORKSPACES_HELPER (helper));

    n_workspaces = xfce_workspaces_helper_get_count ();
    if (n_workspaces < 1)
        return;

    /* check if there are enough names in xfconf, else we save new
     * names first and set the names the next time property-changed is
     * triggered on the channel */
    names = xfconf_channel_get_arrayv (helper->channel, WORKSPACE_NAMES_PROP);
    if (names != NULL && names->len >= n_workspaces)
    {
        /* store this in xfconf (for no really good reason actually) */
        xfconf_channel_set_int (helper->channel, WORKSPACE_COUNT_PROP, n_workspaces);

        /* create nul-separated string of names */
        names_str = g_string_new (NULL);

        for (i = 0; i < names->len && i < n_workspaces; i++)
        {
            val = g_ptr_array_index (names, i);
            if (G_VALUE_HOLDS_STRING(val))
            {
                name = g_value_get_string (val);

                /* insert the name with nul */
                g_string_append_len (names_str, name, strlen (name) + 1);
            }
            else
            {
                /* value in xfconf isn't a string, so make a default one */
                new_name = g_strdup_printf (_("Workspace %d"), i + 1);
                /* insert the name with nul */
                g_string_append_len (names_str, new_name, strlen (new_name) + 1);
                g_free (new_name);
            }
        }

        /* update stamp so new names is not handled for the next second */
        helper->timestamp = g_get_real_time () + G_USEC_PER_SEC;

        gdk_x11_display_error_trap_push (gdk_display_get_default ());

        gdk_property_change (gdk_get_default_root_window (),
                             gdk_atom_intern_static_string ("_NET_DESKTOP_NAMES"),
                             gdk_atom_intern_static_string ("UTF8_STRING"),
                             8, GDK_PROP_MODE_REPLACE,
                             (guchar *) names_str->str,
                             names_str->len + 1);

        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
            g_warning ("Failed to change _NET_DESKTOP_NAMES.");

        xfsettings_dbg (XFSD_DEBUG_WORKSPACES, "%d desktop names set from xfconf", i);

        g_string_free (names_str, TRUE);
    }
    else
    {
        if (names == NULL)
            names = g_ptr_array_sized_new (n_workspaces);

        /* get current names set in x */
        existing_names = xfce_workspaces_helper_get_names ();

        for (i = names->len; i < n_workspaces; i++)
        {
            if (existing_names != NULL
                && existing_names->len > i)
            {
                val = g_ptr_array_index (existing_names, i);
                name = g_value_get_string (val);

                if (name != NULL && *name != '\0')
                {
                    /* use the existing name */
                    val = g_new0 (GValue, 1);
                    g_value_init (val, G_TYPE_STRING);
                    new_name = g_strdup (name);
                    g_value_take_string (val, new_name);
                    g_ptr_array_add (names, val);
                    continue;
                }
            }

            /* no existing name, create a new name */
            val = g_new0 (GValue, 1);
            g_value_init (val, G_TYPE_STRING);
            new_name = g_strdup_printf (_("Workspace %d"), i + 1);
            g_value_take_string (val, new_name);
            g_ptr_array_add (names, val);
        }

        /* store new array in xfconf */
        if (!xfconf_channel_set_arrayv (helper->channel, WORKSPACE_NAMES_PROP, names))
             g_critical ("Failed to save xfconf property %s", WORKSPACE_NAMES_PROP);

        xfsettings_dbg (XFSD_DEBUG_WORKSPACES, "extended names in xfconf, waiting for property-change");

        xfconf_array_free (existing_names);
    }

    xfconf_array_free (names);
}



#ifdef GDK_WINDOWING_X11
static gboolean
xfce_workspaces_helper_wait_for_window_manager (gpointer data)
{
  WaitForWM *wfwm = data;
  guint      i;
  gboolean   have_wm = TRUE;

  for (i = 0; i < wfwm->atom_count; i++)
    {
      if (XGetSelectionOwner (wfwm->dpy, wfwm->atoms[i]) == None)
        {
          DBG ("window manager not ready on screen %d, waiting...", i);

          have_wm = FALSE;
          break;
        }
    }

  wfwm->have_wm = have_wm;

  /* abort if a window manager is found or 5 seconds expired */
  return wfwm->counter++ < 20 * 5 && !wfwm->have_wm;
}



static void
xfce_workspaces_helper_wait_for_window_manager_destroyed (gpointer data)
{
  WaitForWM            *wfwm = data;
  XfceWorkspacesHelper *helper = wfwm->helper;

  helper->wait_for_wm_timeout_id = 0;

  if (!wfwm->have_wm)
    {
      g_printerr (G_LOG_DOMAIN ": No window manager registered on screen 0.\n");
    }
  else
    {
      DBG ("found window manager after %d tries", wfwm->counter);
    }

  g_free (wfwm->atoms);
  XCloseDisplay (wfwm->dpy);
  g_slice_free (WaitForWM, wfwm);

  /* set the names anyway... */
  xfce_workspaces_helper_set_names_real (helper);
}
#endif



static void
xfce_workspaces_helper_set_names (XfceWorkspacesHelper *helper,
                                  gboolean              disable_wm_check)
{
#ifdef GDK_WINDOWING_X11
    WaitForWM  *wfwm;
    guint       i;
    gchar     **atom_names;

    if (!disable_wm_check && !xfsettingsd_disable_wm_check)
    {
        /* setup data for wm checking */
        wfwm = g_slice_new0 (WaitForWM);
        wfwm->helper = helper;
        wfwm->dpy = XOpenDisplay (NULL);
        wfwm->have_wm = FALSE;
        wfwm->counter = 0;

        /* preload wm atoms for all screens */
        wfwm->atom_count = XScreenCount (wfwm->dpy);
        wfwm->atoms = g_new (Atom, wfwm->atom_count);
        atom_names = g_new0 (gchar *, wfwm->atom_count + 1);

        for (i = 0; i < wfwm->atom_count; i++)
            atom_names[i] = g_strdup_printf ("WM_S%d", i);

        if (!XInternAtoms (wfwm->dpy, atom_names, wfwm->atom_count, False, wfwm->atoms))
            wfwm->atom_count = 0;

        g_strfreev (atom_names);

        /* setup timeout to check for a window manager */
        helper->wait_for_wm_timeout_id =
          g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 50, xfce_workspaces_helper_wait_for_window_manager,
                              wfwm, xfce_workspaces_helper_wait_for_window_manager_destroyed);
    }
    else
#endif
    {
        /* directly launch */
        xfce_workspaces_helper_set_names_real (helper);
    }
}



static void
xfce_workspaces_helper_save_names (XfceWorkspacesHelper *helper)
{
    GPtrArray   *xfconf_names, *new_names;
    GValue      *val_b;
    const gchar *name_a, *name_b;
    gboolean     save_array = FALSE;
    guint        i;

    g_return_if_fail (XFCE_IS_WORKSPACES_HELPER (helper));

    new_names = xfce_workspaces_helper_get_names ();
    if (new_names == NULL)
        return;

    xfconf_names = xfconf_channel_get_arrayv (helper->channel, WORKSPACE_NAMES_PROP);

    if (xfconf_names == NULL
       || xfconf_names->len < new_names->len)
    {
        /* store the new names in xfconf, should not append often because
         * we probably saved (and set) a name when the workspace count
         * was changed */
        xfconf_channel_set_arrayv (helper->channel, WORKSPACE_NAMES_PROP, new_names);

        xfsettings_dbg (XFSD_DEBUG_WORKSPACES, "storing %d names in xfconf", new_names->len);
    }
    else if (xfconf_names != NULL
             && xfconf_names->len >= new_names->len)
    {
        /* update the new names in the xfconf array */
        for (i = 0; i < new_names->len; i++)
        {
             name_a = g_value_get_string (g_ptr_array_index (new_names, i));

             val_b = g_ptr_array_index (xfconf_names, i);
             name_b = g_value_get_string (val_b);

             if (g_strcmp0 (name_a, name_b) != 0)
             {
                 /* set the old xfconf name to the new name */
                 g_value_unset (val_b);
                 g_value_init (val_b, G_TYPE_STRING);
                 g_value_set_string (val_b, name_a);

                 save_array = TRUE;
             }
        }

        if (save_array)
        {
            xfconf_channel_set_arrayv (helper->channel, WORKSPACE_NAMES_PROP, xfconf_names);

            xfsettings_dbg (XFSD_DEBUG_WORKSPACES, "merged %d xfconf and %d desktop names",
                            xfconf_names->len, new_names->len);
        }
    }

    if (xfconf_names != NULL)
        xfconf_array_free (xfconf_names);
    xfconf_array_free (new_names);
}



static void
xfce_workspaces_helper_prop_changed (XfconfChannel        *channel,
                                     const gchar          *property,
                                     const GValue         *value,
                                     XfceWorkspacesHelper *helper)
{
    g_return_if_fail (XFCE_IS_WORKSPACES_HELPER (helper));

    if (helper->wait_for_wm_timeout_id == 0)
    {
        /* only set the names if the initial start is not running anymore */
        xfce_workspaces_helper_set_names (helper, TRUE);
    }
}



#ifdef GDK_WINDOWING_X11
void
xfce_workspaces_helper_disable_wm_check (gboolean disable_wm_check)
{
    xfsettingsd_disable_wm_check = disable_wm_check;
}
#endif
