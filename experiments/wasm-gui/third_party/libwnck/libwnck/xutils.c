/* Xlib utils */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2005-2007 Vincent Untz
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "xutils.h"
#include <string.h>
#include <stdio.h>
#include <cairo-xlib.h>
#include "screen.h"
#include "window.h"
#include "private.h"
#include "inlinepixbufs.h"

gboolean
_wnck_get_cardinal (Screen *screen,
                    Window  xwindow,
                    Atom    atom,
                    int    *val)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gulong *num;
  int err, result;

  display = DisplayOfScreen (screen);

  *val = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_CARDINAL, &type, &format, &nitems,
			       &bytes_after, (void*)&num);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_CARDINAL)
    {
      XFree (num);
      return FALSE;
    }

  *val = *num;

  XFree (num);

  return TRUE;
}

int
_wnck_get_wm_state (Screen *screen,
                    Window  xwindow)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gulong *num;
  int err, result;
  Atom wm_state;
  int retval;

  display = DisplayOfScreen (screen);

  wm_state = _wnck_atom_get ("WM_STATE");
  retval = NormalState;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       wm_state,
			       0, G_MAXLONG,
			       False, wm_state, &type, &format, &nitems,
			       &bytes_after, (void*)&num);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return retval;

  if (type != wm_state)
    {
      XFree (num);
      return retval;
    }

  retval = *num;

  XFree (num);

  return retval;
}

gboolean
_wnck_get_window (Screen *screen,
                  Window  xwindow,
                  Atom    atom,
                  Window *val)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Window *w;
  int err, result;

  display = DisplayOfScreen (screen);

  *val = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_WINDOW, &type, &format, &nitems,
			       &bytes_after, (void*)&w);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_WINDOW)
    {
      XFree (w);
      return FALSE;
    }

  *val = *w;

  XFree (w);

  return TRUE;
}

gboolean
_wnck_get_pixmap (Screen *screen,
                  Window  xwindow,
                  Atom    atom,
                  Pixmap *val)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Window *w;
  int err, result;

  display = DisplayOfScreen (screen);

  *val = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_PIXMAP, &type, &format, &nitems,
			       &bytes_after, (void*)&w);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_PIXMAP)
    {
      XFree (w);
      return FALSE;
    }

  *val = *w;

  XFree (w);

  return TRUE;
}

gboolean
_wnck_get_atom (Screen *screen,
                Window  xwindow,
                Atom    atom,
                Atom   *val)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Atom *a;
  int err, result;

  display = DisplayOfScreen (screen);

  *val = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_ATOM, &type, &format, &nitems,
			       &bytes_after, (void*)&a);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_ATOM)
    {
      XFree (a);
      return FALSE;
    }

  *val = *a;

  XFree (a);

  return TRUE;
}

static char*
text_property_to_utf8 (Display             *display,
                       const XTextProperty *prop)
{
  GdkDisplay *gdkdisplay;
  char **list;
  int count;
  char *retval;

  list = NULL;

  gdkdisplay = _wnck_gdk_display_lookup_from_display (display);
  if (!gdkdisplay)
    return NULL;

  count = gdk_text_property_to_utf8_list_for_display (gdkdisplay,
                                          gdk_x11_xatom_to_atom (prop->encoding),
                                          prop->format,
                                          prop->value,
                                          prop->nitems,
                                          &list);

  if (count == 0)
    retval = NULL;
  else
    {
      retval = list[0];
      list[0] = g_strdup (""); /* something to free */
    }

  g_strfreev (list);

  return retval;
}

char*
_wnck_get_text_property (Screen *screen,
                         Window  xwindow,
                         Atom    atom)
{
  Display *display;
  XTextProperty text;
  char *retval;

  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);

  text.nitems = 0;
  if (XGetTextProperty (display,
                        xwindow,
                        &text,
                        atom))
    {
      retval = text_property_to_utf8 (display, &text);

      if (text.value)
        XFree (text.value);
    }
  else
    {
      retval = NULL;
    }

  _wnck_error_trap_pop (display);

  return retval;
}

static char*
_wnck_get_string_property_latin1 (Screen *screen,
                                  Window  xwindow,
                                  Atom    atom)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gchar *str;
  int err, result;
  char *retval;

  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);
  str = NULL;
  result = XGetWindowProperty (display,
			       xwindow, atom,
			       0, G_MAXLONG,
			       False, XA_STRING, &type, &format, &nitems,
			       &bytes_after, (guchar **)&str);

  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return NULL;

  if (type != XA_STRING)
    {
      XFree (str);
      return NULL;
    }

  retval = g_strdup (str);

  XFree (str);

  return retval;
}

char*
_wnck_get_utf8_property (Screen *screen,
                         Window  xwindow,
                         Atom    atom)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gchar *val;
  int err, result;
  char *retval;
  Atom utf8_string;

  display = DisplayOfScreen (screen);

  utf8_string = _wnck_atom_get ("UTF8_STRING");

  _wnck_error_trap_push (display);
  type = None;
  val = NULL;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, utf8_string,
			       &type, &format, &nitems,
			       &bytes_after, (guchar **)&val);
  err = _wnck_error_trap_pop (display);

  if (err != Success ||
      result != Success)
    return NULL;

  if (type != utf8_string ||
      format != 8 ||
      nitems == 0)
    {
      if (val)
        XFree (val);
      return NULL;
    }

  if (!g_utf8_validate (val, nitems, NULL))
    {
      g_warning ("Property %s contained invalid UTF-8\n",
                 _wnck_atom_name (atom));
      XFree (val);
      return NULL;
    }

  retval = g_strndup (val, nitems);

  XFree (val);

  return retval;
}

gboolean
_wnck_get_window_list (Screen  *screen,
                       Window   xwindow,
                       Atom     atom,
                       Window **windows,
                       int     *len)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Window *data;
  int err, result;

  display = DisplayOfScreen (screen);

  *windows = NULL;
  *len = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_WINDOW, &type, &format, &nitems,
			       &bytes_after, (void*)&data);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_WINDOW)
    {
      XFree (data);
      return FALSE;
    }

  *windows = g_new (Window, nitems);
  memcpy (*windows, data, sizeof (Window) * nitems);
  *len = nitems;

  XFree (data);

  return TRUE;
}

gboolean
_wnck_get_atom_list (Screen  *screen,
                     Window   xwindow,
                     Atom     atom,
                     Atom   **atoms,
                     int     *len)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Atom *data;
  int err, result;

  display = DisplayOfScreen (screen);

  *atoms = NULL;
  *len = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_ATOM, &type, &format, &nitems,
			       &bytes_after, (void*)&data);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_ATOM)
    {
      XFree (data);
      return FALSE;
    }

  *atoms = g_new (Atom, nitems);
  memcpy (*atoms, data, sizeof (Atom) * nitems);
  *len = nitems;

  XFree (data);

  return TRUE;
}

gboolean
_wnck_get_cardinal_list (Screen  *screen,
                         Window   xwindow,
                         Atom     atom,
                         gulong **cardinals,
                         int     *len)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  gulong *nums;
  int err, result;

  display = DisplayOfScreen (screen);

  *cardinals = NULL;
  *len = 0;

  _wnck_error_trap_push (display);
  type = None;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_CARDINAL, &type, &format, &nitems,
			       &bytes_after, (void*)&nums);
  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_CARDINAL)
    {
      XFree (nums);
      return FALSE;
    }

  *cardinals = g_new (gulong, nitems);
  memcpy (*cardinals, nums, sizeof (gulong) * nitems);
  *len = nitems;

  XFree (nums);

  return TRUE;
}

char**
_wnck_get_utf8_list (Screen *screen,
                     Window  xwindow,
                     Atom    atom)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  char *val;
  int err, result;
  Atom utf8_string;
  char **retval;
  guint i;
  guint n_strings;
  char *p;

  display = DisplayOfScreen (screen);

  utf8_string = _wnck_atom_get ("UTF8_STRING");

  _wnck_error_trap_push (display);
  type = None;
  val = NULL;
  result = XGetWindowProperty (display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, utf8_string,
			       &type, &format, &nitems,
			       &bytes_after, (void*)&val);
  err = _wnck_error_trap_pop (display);

  if (err != Success ||
      result != Success)
    return NULL;

  if (type != utf8_string ||
      format != 8 ||
      nitems == 0)
    {
      if (val)
        XFree (val);
      return NULL;
    }

  /* I'm not sure this is right, but I'm guessing the
   * property is nul-separated
   */
  i = 0;
  n_strings = 0;
  while (i < nitems)
    {
      if (val[i] == '\0')
        ++n_strings;
      ++i;
    }

  if (val[nitems - 1] != '\0')
    ++n_strings;

  /* we're guaranteed that val has a nul on the end
   * by XGetWindowProperty
   */

  retval = g_new0 (char*, n_strings + 1);

  p = val;
  i = 0;
  while (i < n_strings)
    {
      if (!g_utf8_validate (p, -1, NULL))
        {
          g_warning ("Property %s contained invalid UTF-8\n",
                     _wnck_atom_name (atom));
          XFree (val);
          g_strfreev (retval);
          return NULL;
        }

      retval[i] = g_strdup (p);

      p = p + strlen (p) + 1;
      ++i;
    }

  XFree (val);

  return retval;
}

void
_wnck_set_utf8_list (Screen  *screen,
                     Window   xwindow,
                     Atom     atom,
                     char   **list)
{
  Display *display;
  Atom utf8_string;
  GString *flattened;
  int i;

  display = DisplayOfScreen (screen);

  utf8_string = _wnck_atom_get ("UTF8_STRING");

  /* flatten to nul-separated list */
  flattened = g_string_new ("");
  i = 0;
  while (list[i] != NULL)
    {
      g_string_append_len (flattened, list[i],
                           strlen (list[i]) + 1);
      ++i;
    }

  _wnck_error_trap_push (display);

  XChangeProperty (display,
		   xwindow,
                   atom,
		   utf8_string, 8, PropModeReplace,
		   (guchar *) flattened->str, flattened->len);

  _wnck_error_trap_pop (display);

  g_string_free (flattened, TRUE);
}

void
_wnck_error_trap_push (Display *display)
{
  gdk_error_trap_push ();
}

int
_wnck_error_trap_pop (Display *display)
{
  gdk_flush ();
  return gdk_error_trap_pop ();
}

static GdkFilterReturn
filter_func (GdkXEvent  *gdkxevent,
             GdkEvent   *event,
             gpointer    data)
{
  XEvent *xevent = gdkxevent;
#ifdef HAVE_STARTUP_NOTIFICATION
  int i;
  Display *display;
#endif /* HAVE_STARTUP_NOTIFICATION */

  switch (xevent->type)
    {
    case PropertyNotify:
      {
        WnckScreen *screen;

        screen = wnck_screen_get_for_root (xevent->xany.window);
        if (screen != NULL)
          _wnck_screen_process_property_notify (screen, xevent);
        else
          {
            WnckWindow *window;
            WnckApplication *app;

            window = wnck_window_get (xevent->xany.window);
            app = wnck_application_get (xevent->xany.window);

            if (app)
              _wnck_application_process_property_notify (app, xevent);

            if (window)
              _wnck_window_process_property_notify (window, xevent);
          }
      }
      break;

    case ConfigureNotify:
      {
        WnckWindow *window;

        window = wnck_window_get (xevent->xconfigure.window);

        if (window)
          _wnck_window_process_configure_notify (window, xevent);
      }
      break;

    case SelectionClear:
      {
        _wnck_desktop_layout_manager_process_event (xevent);
      }
      break;

    case ClientMessage:
#ifdef HAVE_STARTUP_NOTIFICATION
      /* We're cheating as officially libsn requires
       * us to send all events through sn_display_process_event
       */
      i = 0;
      display = ((XAnyEvent *) xevent)->display;

      while (i < ScreenCount (display))
        {
          WnckScreen *s;

          s = _wnck_screen_get_existing (i);
          if (s != NULL)
            sn_display_process_event (_wnck_screen_get_sn_display (s),
                                      xevent);

          ++i;
        }
#endif /* HAVE_STARTUP_NOTIFICATION */
      break;
    }

  return GDK_FILTER_CONTINUE;
}

static gboolean _wnck_event_filter_initialized = FALSE;

void
_wnck_event_filter_init (void)
{

  if (!_wnck_event_filter_initialized)
    {
      gdk_window_add_filter (NULL, filter_func, NULL);
      _wnck_event_filter_initialized = TRUE;
    }
}

void
_wnck_event_filter_shutdown (void)
{

  if (_wnck_event_filter_initialized)
    {
      gdk_window_remove_filter (NULL, filter_func, NULL);
      _wnck_event_filter_initialized = FALSE;
    }
}

int
_wnck_xid_equal (gconstpointer v1,
                 gconstpointer v2)
{
  return *((const gulong*) v1) == *((const gulong*) v2);
}

guint
_wnck_xid_hash (gconstpointer v)
{
  gulong val = * (const gulong *) v;

  /* I'm not sure this works so well. */
#if G_SIZEOF_LONG > 4
  return (guint) (val ^ (val >> 32));
#else
  return val;
#endif
}

void
_wnck_iconify (Screen *screen,
               Window  xwindow)
{
  Display *display;

  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);
  XIconifyWindow (display, xwindow, DefaultScreen (display));
  _wnck_error_trap_pop (display);
}

void
_wnck_deiconify (Screen *screen,
                 Window  xwindow)
{
  /* We need special precautions, because GDK doesn't like
   * XMapWindow() called on its windows, need to use the
   * GDK functions
   */
  Display   *display;
  GdkWindow *gdkwindow;

  display = DisplayOfScreen (screen);
  gdkwindow = _wnck_gdk_window_lookup_from_window (screen, xwindow);

  _wnck_error_trap_push (display);
  if (gdkwindow)
    gdk_window_show (gdkwindow);
  else
    XMapRaised (display, xwindow);
  _wnck_error_trap_pop (display);
}

void
_wnck_close (Screen *screen,
	     Window  xwindow,
	     Time    timestamp)
{
  Display *display;
  Window   root;
  XEvent   xev;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_CLOSE_WINDOW");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = timestamp;
  xev.xclient.data.l[1] = _wnck_get_client_type ();
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  _wnck_error_trap_push (display);
  XSendEvent (display,
              root,
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
  _wnck_error_trap_pop (display);
}

#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10

void
_wnck_keyboard_move (Screen *screen,
                     Window  xwindow)
{
  Display *display;
  Window   root;
  XEvent   xev;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_WM_MOVERESIZE");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = 0; /* unused */
  xev.xclient.data.l[1] = 0; /* unused */
  xev.xclient.data.l[2] = _NET_WM_MOVERESIZE_MOVE_KEYBOARD;
  xev.xclient.data.l[3] = 0; /* unused */
  xev.xclient.data.l[4] = _wnck_get_client_type ();

  _wnck_error_trap_push (display);
  XSendEvent (display,
              root,
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_keyboard_size (Screen *screen,
                     Window  xwindow)
{
  Display *display;
  Window   root;
  XEvent   xev;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_WM_MOVERESIZE");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = 0; /* unused */
  xev.xclient.data.l[1] = 0; /* unused */
  xev.xclient.data.l[2] = _NET_WM_MOVERESIZE_SIZE_KEYBOARD;
  xev.xclient.data.l[3] = 0; /* unused */
  xev.xclient.data.l[4] = _wnck_get_client_type ();

  _wnck_error_trap_push (display);
  XSendEvent (display,
              root,
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_change_state (Screen  *screen,
		    Window   xwindow,
                    gboolean add,
                    Atom     state1,
                    Atom     state2)
{
  Display *display;
  Window   root;
  XEvent   xev;

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_WM_STATE");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
  xev.xclient.data.l[1] = state1;
  xev.xclient.data.l[2] = state2;
  xev.xclient.data.l[3] = _wnck_get_client_type ();
  xev.xclient.data.l[4] = 0;

  _wnck_error_trap_push (display);
  XSendEvent (display,
	      root,
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_change_workspace (Screen     *screen,
			Window      xwindow,
                        int         new_space)
{
  Display *display;
  Window   root;
  XEvent   xev;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_WM_DESKTOP");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = new_space;
  xev.xclient.data.l[1] = _wnck_get_client_type ();
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  _wnck_error_trap_push (display);
  XSendEvent (display,
	      root,
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_activate (Screen *screen,
                Window  xwindow,
                Time    timestamp)
{
  Display *display;
  Window   root;
  XEvent   xev;

  if (timestamp == 0)
    g_warning ("Received a timestamp of 0; window activation may not "
               "function properly.\n");

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_ACTIVE_WINDOW");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = _wnck_get_client_type ();
  xev.xclient.data.l[1] = timestamp;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  _wnck_error_trap_push (display);
  XSendEvent (display,
	      root,
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_activate_workspace (Screen *screen,
                          int     new_active_space,
                          Time    timestamp)
{
  Display *display;
  Window   root;
  XEvent   xev;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = root;
  xev.xclient.message_type = _wnck_atom_get ("_NET_CURRENT_DESKTOP");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = new_active_space;
  xev.xclient.data.l[1] = timestamp;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  _wnck_error_trap_push (display);
  XSendEvent (display,
	      root,
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_change_viewport (Screen *screen,
		       int     x,
		       int     y)
{
  Display *display;
  Window   root;
  XEvent   xev;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = root;
  xev.xclient.message_type = _wnck_atom_get ("_NET_DESKTOP_VIEWPORT");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = x;
  xev.xclient.data.l[1] = y;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  _wnck_error_trap_push (display);
  XSendEvent (display,
	      root,
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_toggle_showing_desktop (Screen  *screen,
                              gboolean show)
{
  Display *display;
  Window   root;
  XEvent   xev;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = root;
  xev.xclient.message_type = _wnck_atom_get ("_NET_SHOWING_DESKTOP");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = show != FALSE;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  _wnck_error_trap_push (display);
  XSendEvent (display,
	      root,
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
  _wnck_error_trap_pop (display);
}

char*
_wnck_get_session_id (Screen *screen,
                      Window  xwindow)
{
  Window client_leader;

  client_leader = None;
  _wnck_get_window (screen, xwindow,
                    _wnck_atom_get ("WM_CLIENT_LEADER"),
                    &client_leader);

  if (client_leader == None)
    return NULL;

  return _wnck_get_string_property_latin1 (screen, client_leader,
                                           _wnck_atom_get ("SM_CLIENT_ID"));
}

int
_wnck_get_pid (Screen *screen,
               Window  xwindow)
{
  int val;

  if (!_wnck_get_cardinal (screen, xwindow,
                           _wnck_atom_get ("_NET_WM_PID"),
                           &val))
    return 0;
  else
    return val;
}

char*
_wnck_get_name (Screen *screen,
                Window  xwindow)
{
  char *name;

  name = _wnck_get_utf8_property (screen, xwindow,
                                  _wnck_atom_get ("_NET_WM_VISIBLE_NAME"));

  if (name == NULL)
    name = _wnck_get_utf8_property (screen, xwindow,
                                    _wnck_atom_get ("_NET_WM_NAME"));

  if (name == NULL)
    name = _wnck_get_text_property (screen, xwindow,
                                    XA_WM_NAME);

  return name;
}

char*
_wnck_get_icon_name (Screen *screen,
                     Window  xwindow)
{
  char *name;

  name = _wnck_get_utf8_property (screen, xwindow,
                                  _wnck_atom_get ("_NET_WM_VISIBLE_ICON_NAME"));

  if (name == NULL)
    name = _wnck_get_utf8_property (screen, xwindow,
                                    _wnck_atom_get ("_NET_WM_ICON_NAME"));

  if (name == NULL)
    name = _wnck_get_text_property (screen, xwindow,
                                    XA_WM_ICON_NAME);

  return name;
}

static char*
latin1_to_utf8 (const char *latin1)
{
  GString *str;
  const char *p;

  str = g_string_new (NULL);

  p = latin1;
  while (*p)
    {
      g_string_append_unichar (str, (gunichar) *p);
      ++p;
    }

  return g_string_free (str, FALSE);
}

char*
_wnck_get_res_class_utf8 (Screen *screen,
                          Window  xwindow)
{
  char *res_class;

  _wnck_get_wmclass (screen, xwindow, &res_class, NULL);

  return res_class;
}

void
_wnck_get_wmclass (Screen *screen,
                   Window  xwindow,
                   char **res_class,
                   char **res_name)
{
  Display *display;
  XClassHint ch;

  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);

  ch.res_name = NULL;
  ch.res_class = NULL;

  XGetClassHint (display, xwindow, &ch);

  _wnck_error_trap_pop (display);

  if (res_class)
    *res_class = NULL;

  if (res_name)
    *res_name = NULL;

  if (ch.res_name)
    {
      if (res_name)
        *res_name = latin1_to_utf8 (ch.res_name);

      XFree (ch.res_name);
    }

  if (ch.res_class)
    {
      if (res_class)
        *res_class = latin1_to_utf8 (ch.res_class);

      XFree (ch.res_class);
    }
}

gboolean
_wnck_get_frame_extents (Screen *screen,
                         Window  xwindow,
                         int    *left_frame,
                         int    *right_frame,
                         int    *top_frame,
                         int    *bottom_frame)
{
  gulong   *p_size;
  int       n_size;
  gboolean  retval;

  retval = FALSE;
  p_size = NULL;
  n_size = 0;

  _wnck_get_cardinal_list (screen, xwindow,
                           _wnck_atom_get ("_NET_FRAME_EXTENTS"),
                           &p_size, &n_size);

  if (p_size != NULL && n_size == 4)
    {
      *left_frame   = p_size[0];
      *right_frame  = p_size[1];
      *top_frame    = p_size[2];
      *bottom_frame = p_size[3];

      retval = TRUE;
    }

  if (p_size == NULL)
    {
      _wnck_get_cardinal_list (screen, xwindow,
                               _wnck_atom_get ("_GTK_FRAME_EXTENTS"),
                               &p_size, &n_size);

      if (p_size != NULL && n_size == 4)
        {
          *left_frame   = -p_size[0];
          *right_frame  = -p_size[1];
          *top_frame    = -p_size[2];
          *bottom_frame = -p_size[3];

          retval = TRUE;
        }
    }

  if (p_size != NULL)
    g_free (p_size);

  return retval;
}

int
_wnck_select_input (Screen *screen,
                    Window  xwindow,
                    int     mask,
                    gboolean update)
{
  Display   *display;
  GdkWindow *gdkwindow;
  int old_mask = 0;

  display = DisplayOfScreen (screen);

  gdkwindow = _wnck_gdk_window_lookup_from_window (screen, xwindow);

  _wnck_error_trap_push (display);
  if (gdkwindow)
    {
      /* Avoid breaking GDK's setup,
       * this somewhat relies on people setting
       * event masks right after realization
       * and not changing them again
       */
      XWindowAttributes attrs;
      XGetWindowAttributes (display, xwindow, &attrs);
      old_mask = attrs.your_event_mask;

      if (update)
        mask |= attrs.your_event_mask;
    }

  XSelectInput (display, xwindow, mask);
  _wnck_error_trap_pop (display);

  return old_mask;
}

/* The icon-reading code is copied
 * from metacity, please sync bugfixes
 */
static gboolean
find_largest_sizes (gulong *data,
                    gulong  nitems,
                    int    *width,
                    int    *height)
{
  *width = 0;
  *height = 0;

  while (nitems > 0)
    {
      int w, h;

      if (nitems < 3)
        return FALSE; /* no space for w, h */

      w = data[0];
      h = data[1];

      if (nitems < ((w * h) + 2))
        return FALSE; /* not enough data */

      *width = MAX (w, *width);
      *height = MAX (h, *height);

      data += (w * h) + 2;
      nitems -= (w * h) + 2;
    }

  return TRUE;
}

static gboolean
find_best_size (gulong  *data,
                gulong   nitems,
                int      ideal_width,
                int      ideal_height,
                int     *width,
                int     *height,
                gulong **start)
{
  int best_w;
  int best_h;
  gulong *best_start;
  int max_width, max_height;

  *width = 0;
  *height = 0;
  *start = NULL;

  if (!find_largest_sizes (data, nitems, &max_width, &max_height))
    return FALSE;

  if (ideal_width < 0)
    ideal_width = max_width;
  if (ideal_height < 0)
    ideal_height = max_height;

  best_w = 0;
  best_h = 0;
  best_start = NULL;

  while (nitems > 0)
    {
      int w, h;
      gboolean replace;

      replace = FALSE;

      if (nitems < 3)
        return FALSE; /* no space for w, h */

      w = data[0];
      h = data[1];

      if (nitems < ((w * h) + 2))
        break; /* not enough data */

      if (best_start == NULL)
        {
          replace = TRUE;
        }
      else
        {
          /* work with averages */
          const int ideal_size = (ideal_width + ideal_height) / 2;
          int best_size = (best_w + best_h) / 2;
          int this_size = (w + h) / 2;

          /* larger than desired is always better than smaller */
          if (best_size < ideal_size &&
              this_size >= ideal_size)
            replace = TRUE;
          /* if we have too small, pick anything bigger */
          else if (best_size < ideal_size &&
                   this_size > best_size)
            replace = TRUE;
          /* if we have too large, pick anything smaller
           * but still >= the ideal
           */
          else if (best_size > ideal_size &&
                   this_size >= ideal_size &&
                   this_size < best_size)
            replace = TRUE;
        }

      if (replace)
        {
          best_start = data + 2;
          best_w = w;
          best_h = h;
        }

      data += (w * h) + 2;
      nitems -= (w * h) + 2;
    }

  if (best_start)
    {
      *start = best_start;
      *width = best_w;
      *height = best_h;
      return TRUE;
    }
  else
    return FALSE;
}

static void
argbdata_to_pixdata (gulong *argb_data, int len, guchar **pixdata)
{
  guchar *p;
  int i;

  *pixdata = g_new (guchar, len * 4);
  p = *pixdata;

  /* One could speed this up a lot. */
  i = 0;
  while (i < len)
    {
      guint argb;
      guint rgba;

      argb = argb_data[i];
      rgba = (argb << 8) | (argb >> 24);

      *p = rgba >> 24;
      ++p;
      *p = (rgba >> 16) & 0xff;
      ++p;
      *p = (rgba >> 8) & 0xff;
      ++p;
      *p = rgba & 0xff;
      ++p;

      ++i;
    }
}

static gboolean
read_rgb_icon (Screen        *screen,
               Window         xwindow,
               int            ideal_width,
               int            ideal_height,
               int            ideal_mini_width,
               int            ideal_mini_height,
               int           *width,
               int           *height,
               guchar       **pixdata,
               int           *mini_width,
               int           *mini_height,
               guchar       **mini_pixdata)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  int result, err;
  gulong *data;
  gulong *best;
  int w, h;
  gulong *best_mini;
  int mini_w, mini_h;

  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);
  type = None;
  data = NULL;
  result = XGetWindowProperty (display,
			       xwindow,
			       _wnck_atom_get ("_NET_WM_ICON"),
			       0, G_MAXLONG,
			       False, XA_CARDINAL, &type, &format, &nitems,
			       &bytes_after, (void*)&data);

  err = _wnck_error_trap_pop (display);

  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_CARDINAL)
    {
      XFree (data);
      return FALSE;
    }

  if (!find_best_size (data, nitems,
                       ideal_width, ideal_height,
                       &w, &h, &best))
    {
      XFree (data);
      return FALSE;
    }

  if (!find_best_size (data, nitems,
                       ideal_mini_width, ideal_mini_height,
                       &mini_w, &mini_h, &best_mini))
    {
      XFree (data);
      return FALSE;
    }

  *width = w;
  *height = h;

  *mini_width = mini_w;
  *mini_height = mini_h;

  argbdata_to_pixdata (best, w * h, pixdata);
  argbdata_to_pixdata (best_mini, mini_w * mini_h, mini_pixdata);

  XFree (data);

  return TRUE;
}

static void
free_pixels (guchar *pixels, gpointer data)
{
  g_free (pixels);
}

static cairo_surface_t *
_wnck_cairo_surface_get_from_pixmap (Screen *screen,
                                     Pixmap  xpixmap)
{
  cairo_surface_t *surface;
  Display *display;
  Window root_return;
  int x_ret, y_ret;
  unsigned int w_ret, h_ret, bw_ret, depth_ret;
  XWindowAttributes attrs;

  surface = NULL;
  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);

  if (!XGetGeometry (display, xpixmap, &root_return,
                     &x_ret, &y_ret, &w_ret, &h_ret, &bw_ret, &depth_ret))
    goto TRAP_POP;

  if (depth_ret == 1)
    {
      surface = cairo_xlib_surface_create_for_bitmap (display,
                                                      xpixmap,
                                                      screen,
                                                      w_ret,
                                                      h_ret);
    }
  else
    {
      if (!XGetWindowAttributes (display, root_return, &attrs))
        goto TRAP_POP;

      surface = cairo_xlib_surface_create (display,
                                           xpixmap,
                                           attrs.visual,
                                           w_ret, h_ret);
    }

TRAP_POP:
  _wnck_error_trap_pop (display);

  return surface;
}

GdkPixbuf*
_wnck_gdk_pixbuf_get_from_pixmap (Screen *screen,
                                  Pixmap  xpixmap)
{
  cairo_surface_t *surface;
  GdkPixbuf *retval;

  surface = _wnck_cairo_surface_get_from_pixmap (screen, xpixmap);

  if (surface == NULL)
    return NULL;

  retval = gdk_pixbuf_get_from_surface (surface,
                                        0,
                                        0,
                                        cairo_xlib_surface_get_width (surface),
                                        cairo_xlib_surface_get_height (surface));
  cairo_surface_destroy (surface);

  return retval;
}

static gboolean
try_pixmap_and_mask (Screen     *screen,
                     Pixmap      src_pixmap,
                     Pixmap      src_mask,
                     GdkPixbuf **iconp,
                     int         ideal_width,
                     int         ideal_height,
                     GdkPixbuf **mini_iconp,
                     int         ideal_mini_width,
                     int         ideal_mini_height)
{
  cairo_surface_t *surface, *mask_surface, *image;
  GdkPixbuf *unscaled;
  int width, height;
  cairo_t *cr;

  if (src_pixmap == None)
    return FALSE;

  surface = _wnck_cairo_surface_get_from_pixmap (screen, src_pixmap);

  if (surface && src_mask != None)
    mask_surface = _wnck_cairo_surface_get_from_pixmap (screen, src_mask);
  else
    mask_surface = NULL;

  if (surface == NULL)
    return FALSE;

  width = cairo_xlib_surface_get_width (surface);
  height = cairo_xlib_surface_get_height (surface);

  image = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                      width, height);
  cr = cairo_create (image);

  /* Need special code for alpha-only surfaces. We only get those
   * for bitmaps. And in that case, it's a differentiation between
   * foreground (white) and background (black).
   */
  if (cairo_surface_get_content (surface) & CAIRO_CONTENT_ALPHA)
    {
      cairo_push_group (cr);

      /* black background */
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_paint (cr);
      /* mask with white foreground */
      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_mask_surface (cr, surface, 0, 0);

      cairo_pop_group_to_source (cr);
    }
  else
    cairo_set_source_surface (cr, surface, 0, 0);

  if (mask_surface)
    {
      cairo_mask_surface (cr, mask_surface, 0, 0);
      cairo_surface_destroy (mask_surface);
    }
  else
    cairo_paint (cr);

  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  unscaled = gdk_pixbuf_get_from_surface (image,
                                          0, 0,
                                          width, height);

  cairo_surface_destroy (image);

  if (unscaled)
    {
      *iconp =
        gdk_pixbuf_scale_simple (unscaled,
                                 ideal_width > 0 ? ideal_width :
                                 gdk_pixbuf_get_width (unscaled),
                                 ideal_height > 0 ? ideal_height :
                                 gdk_pixbuf_get_height (unscaled),
                                 GDK_INTERP_BILINEAR);
      *mini_iconp =
        gdk_pixbuf_scale_simple (unscaled,
                                 ideal_mini_width > 0 ? ideal_mini_width :
                                 gdk_pixbuf_get_width (unscaled),
                                 ideal_mini_height > 0 ? ideal_mini_height :
                                 gdk_pixbuf_get_height (unscaled),
                                 GDK_INTERP_BILINEAR);

      g_object_unref (G_OBJECT (unscaled));
      return TRUE;
    }
  else
    return FALSE;
}

static void
get_kwm_win_icon (Screen *screen,
                  Window  xwindow,
                  Pixmap *pixmap,
                  Pixmap *mask)
{
  Display *display;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Pixmap *icons;
  int err, result;

  display = DisplayOfScreen (screen);

  *pixmap = None;
  *mask = None;

  _wnck_error_trap_push (display);
  icons = NULL;
  result = XGetWindowProperty (display, xwindow,
			       _wnck_atom_get ("KWM_WIN_ICON"),
			       0, G_MAXLONG,
			       False,
			       _wnck_atom_get ("KWM_WIN_ICON"),
			       &type, &format, &nitems,
			       &bytes_after, (void*)&icons);

  err = _wnck_error_trap_pop (display);
  if (err != Success ||
      result != Success)
    return;

  if (type != _wnck_atom_get ("KWM_WIN_ICON"))
    {
      XFree (icons);
      return;
    }

  *pixmap = icons[0];
  *mask = icons[1];

  XFree (icons);

  return;
}

typedef enum
{
  /* These MUST be in ascending order of preference;
   * i.e. if we get _NET_WM_ICON and already have
   * WM_HINTS, we prefer _NET_WM_ICON
   */
  USING_NO_ICON,
  USING_FALLBACK_ICON,
  USING_KWM_WIN_ICON,
  USING_WM_HINTS,
  USING_NET_WM_ICON
} IconOrigin;

struct _WnckIconCache
{
  IconOrigin origin;
  Pixmap prev_pixmap;
  Pixmap prev_mask;
  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;
  int ideal_width;
  int ideal_height;
  int ideal_mini_width;
  int ideal_mini_height;
  guint want_fallback : 1;
  /* TRUE if these props have changed */
  guint wm_hints_dirty : 1;
  guint kwm_win_icon_dirty : 1;
  guint net_wm_icon_dirty : 1;
};

WnckIconCache*
_wnck_icon_cache_new (void)
{
  WnckIconCache *icon_cache;

  icon_cache = g_slice_new0 (WnckIconCache);

  icon_cache->origin = USING_NO_ICON;
  icon_cache->prev_pixmap = None;
  icon_cache->icon = NULL;
  icon_cache->mini_icon = NULL;
  icon_cache->ideal_width = -1; /* won't be a legit width */
  icon_cache->ideal_height = -1;
  icon_cache->ideal_mini_width = -1;
  icon_cache->ideal_mini_height = -1;
  icon_cache->want_fallback = TRUE;
  icon_cache->wm_hints_dirty = TRUE;
  icon_cache->kwm_win_icon_dirty = TRUE;
  icon_cache->net_wm_icon_dirty = TRUE;

  return icon_cache;
}

static void
clear_icon_cache (WnckIconCache *icon_cache,
                  gboolean       dirty_all)
{
  if (icon_cache->icon)
    g_object_unref (G_OBJECT (icon_cache->icon));
  icon_cache->icon = NULL;

  if (icon_cache->mini_icon)
    g_object_unref (G_OBJECT (icon_cache->mini_icon));
  icon_cache->mini_icon = NULL;

  icon_cache->origin = USING_NO_ICON;

  if (dirty_all)
    {
      icon_cache->wm_hints_dirty = TRUE;
      icon_cache->kwm_win_icon_dirty = TRUE;
      icon_cache->net_wm_icon_dirty = TRUE;
    }
}

void
_wnck_icon_cache_free (WnckIconCache *icon_cache)
{
  clear_icon_cache (icon_cache, FALSE);

  g_slice_free (WnckIconCache, icon_cache);
}

void
_wnck_icon_cache_property_changed (WnckIconCache *icon_cache,
                                   Atom           atom)
{
  if (atom == _wnck_atom_get ("_NET_WM_ICON"))
    icon_cache->net_wm_icon_dirty = TRUE;
  else if (atom == _wnck_atom_get ("KWM_WIN_ICON"))
    icon_cache->kwm_win_icon_dirty = TRUE;
  else if (atom == _wnck_atom_get ("WM_HINTS"))
    icon_cache->wm_hints_dirty = TRUE;
}

gboolean
_wnck_icon_cache_get_icon_invalidated (WnckIconCache *icon_cache)
{
  if (icon_cache->origin <= USING_KWM_WIN_ICON &&
      icon_cache->kwm_win_icon_dirty)
    return TRUE;
  else if (icon_cache->origin <= USING_WM_HINTS &&
           icon_cache->wm_hints_dirty)
    return TRUE;
  else if (icon_cache->origin <= USING_NET_WM_ICON &&
           icon_cache->net_wm_icon_dirty)
    return TRUE;
  else if (icon_cache->origin < USING_FALLBACK_ICON &&
           icon_cache->want_fallback)
    return TRUE;
  else if (icon_cache->origin == USING_NO_ICON)
    return TRUE;
  else if (icon_cache->origin == USING_FALLBACK_ICON &&
           !icon_cache->want_fallback)
    return TRUE;
  else
    return FALSE;
}

void
_wnck_icon_cache_set_want_fallback (WnckIconCache *icon_cache,
                                    gboolean       setting)
{
  icon_cache->want_fallback = setting;
}

gboolean
_wnck_icon_cache_get_is_fallback (WnckIconCache *icon_cache)
{
  return icon_cache->origin == USING_FALLBACK_ICON;
}

static void
replace_cache (WnckIconCache *icon_cache,
               IconOrigin     origin,
               GdkPixbuf     *new_icon,
               GdkPixbuf     *new_mini_icon)
{
  clear_icon_cache (icon_cache, FALSE);

  icon_cache->origin = origin;

  if (new_icon)
    g_object_ref (G_OBJECT (new_icon));

  icon_cache->icon = new_icon;

  if (new_mini_icon)
    g_object_ref (G_OBJECT (new_mini_icon));

  icon_cache->mini_icon = new_mini_icon;
}

static GdkPixbuf*
scaled_from_pixdata (guchar *pixdata,
                     int     w,
                     int     h,
                     int     new_w,
                     int     new_h)
{
  GdkPixbuf *src;
  GdkPixbuf *dest;

  src = gdk_pixbuf_new_from_data (pixdata,
                                  GDK_COLORSPACE_RGB,
                                  TRUE,
                                  8,
                                  w, h, w * 4,
                                  free_pixels,
                                  NULL);

  if (src == NULL)
    return NULL;

  if (w != h)
    {
      GdkPixbuf *tmp;
      int size;

      size = MAX (w, h);

      tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, size, size);

      if (tmp != NULL)
	{
	  gdk_pixbuf_fill (tmp, 0);
	  gdk_pixbuf_copy_area (src, 0, 0, w, h,
				tmp,
				(size - w) / 2, (size - h) / 2);

	  g_object_unref (src);
	  src = tmp;
	}
    }

  if (w != new_w || h != new_h)
    {
      dest = gdk_pixbuf_scale_simple (src, new_w, new_h, GDK_INTERP_BILINEAR);

      g_object_unref (G_OBJECT (src));
    }
  else
    {
      dest = src;
    }

  return dest;
}

gboolean
_wnck_read_icons (Screen        *screen,
                  Window         xwindow,
                  WnckIconCache *icon_cache,
                  GdkPixbuf    **iconp,
                  int            ideal_width,
                  int            ideal_height,
                  GdkPixbuf    **mini_iconp,
                  int            ideal_mini_width,
                  int            ideal_mini_height)
{
  Display *display;
  guchar *pixdata;
  int w, h;
  guchar *mini_pixdata;
  int mini_w, mini_h;
  Pixmap pixmap;
  Pixmap mask;
  XWMHints *hints;

  /* Return value is whether the icon changed */

  g_return_val_if_fail (icon_cache != NULL, FALSE);

  display = DisplayOfScreen (screen);

  *iconp = NULL;
  *mini_iconp = NULL;

  if (ideal_width != icon_cache->ideal_width ||
      ideal_height != icon_cache->ideal_height ||
      ideal_mini_width != icon_cache->ideal_mini_width ||
      ideal_mini_height != icon_cache->ideal_mini_height)
    clear_icon_cache (icon_cache, TRUE);

  icon_cache->ideal_width = ideal_width;
  icon_cache->ideal_height = ideal_height;
  icon_cache->ideal_mini_width = ideal_mini_width;
  icon_cache->ideal_mini_height = ideal_mini_height;

  if (!_wnck_icon_cache_get_icon_invalidated (icon_cache))
    return FALSE; /* we have no new info to use */

  pixdata = NULL;

  /* Our algorithm here assumes that we can't have for example origin
   * < USING_NET_WM_ICON and icon_cache->net_wm_icon_dirty == FALSE
   * unless we have tried to read NET_WM_ICON.
   *
   * Put another way, if an icon origin is not dirty, then we have
   * tried to read it at the current size. If it is dirty, then
   * we haven't done that since the last change.
   */

  if (icon_cache->origin <= USING_NET_WM_ICON &&
      icon_cache->net_wm_icon_dirty)

    {
      icon_cache->net_wm_icon_dirty = FALSE;

      if (read_rgb_icon (screen, xwindow,
                         ideal_width, ideal_height,
                         ideal_mini_width, ideal_mini_height,
                         &w, &h, &pixdata,
                         &mini_w, &mini_h, &mini_pixdata))
        {
          *iconp = scaled_from_pixdata (pixdata, w, h, ideal_width, ideal_height);

          *mini_iconp = scaled_from_pixdata (mini_pixdata, mini_w, mini_h,
                                             ideal_mini_width, ideal_mini_height);

          replace_cache (icon_cache, USING_NET_WM_ICON,
                         *iconp, *mini_iconp);

          return TRUE;
        }
    }

  if (icon_cache->origin <= USING_WM_HINTS &&
      icon_cache->wm_hints_dirty)
    {
      icon_cache->wm_hints_dirty = FALSE;

      _wnck_error_trap_push (display);
      hints = XGetWMHints (display, xwindow);
      _wnck_error_trap_pop (display);
      pixmap = None;
      mask = None;
      if (hints)
        {
          if (hints->flags & IconPixmapHint)
            pixmap = hints->icon_pixmap;
          if (hints->flags & IconMaskHint)
            mask = hints->icon_mask;

          XFree (hints);
          hints = NULL;
        }

      /* We won't update if pixmap is unchanged;
       * avoids a get_from_drawable() on every geometry
       * hints change
       */
      if ((pixmap != icon_cache->prev_pixmap ||
           mask != icon_cache->prev_mask) &&
          pixmap != None)
        {
          if (try_pixmap_and_mask (screen, pixmap, mask,
                                   iconp, ideal_width, ideal_height,
                                   mini_iconp, ideal_mini_width, ideal_mini_height))
            {
              icon_cache->prev_pixmap = pixmap;
              icon_cache->prev_mask = mask;

              replace_cache (icon_cache, USING_WM_HINTS,
                             *iconp, *mini_iconp);

              return TRUE;
            }
        }
    }

  if (icon_cache->origin <= USING_KWM_WIN_ICON &&
      icon_cache->kwm_win_icon_dirty)
    {
      icon_cache->kwm_win_icon_dirty = FALSE;

      get_kwm_win_icon (screen, xwindow, &pixmap, &mask);

      if ((pixmap != icon_cache->prev_pixmap ||
           mask != icon_cache->prev_mask) &&
          pixmap != None)
        {
          if (try_pixmap_and_mask (screen, pixmap, mask,
                                   iconp, ideal_width, ideal_height,
                                   mini_iconp, ideal_mini_width, ideal_mini_height))
            {
              icon_cache->prev_pixmap = pixmap;
              icon_cache->prev_mask = mask;

              replace_cache (icon_cache, USING_KWM_WIN_ICON,
                             *iconp, *mini_iconp);

              return TRUE;
            }
        }
    }

  if (icon_cache->want_fallback &&
      icon_cache->origin < USING_FALLBACK_ICON)
    {
      _wnck_get_fallback_icons (iconp,
                                ideal_width,
                                ideal_height,
                                mini_iconp,
                                ideal_mini_width,
                                ideal_mini_height);

      replace_cache (icon_cache, USING_FALLBACK_ICON,
                     *iconp, *mini_iconp);

      return TRUE;
    }

  if (!icon_cache->want_fallback &&
      icon_cache->origin == USING_FALLBACK_ICON)
    {
      /* Get rid of current icon */
      clear_icon_cache (icon_cache, FALSE);

      return TRUE;
    }

  /* found nothing new */
  return FALSE;
}

static GdkPixbuf*
default_icon_at_size (int width,
                      int height)
{

  GdkPixbuf *base;

  base = gdk_pixbuf_new_from_inline (-1, default_icon_data,
                                     FALSE,
                                     NULL);

  g_assert (base);

  if ((width < 0 && height < 0) ||
      (gdk_pixbuf_get_width (base) == width &&
       gdk_pixbuf_get_height (base) == height))
    {
      return base;
    }
  else
    {
      GdkPixbuf *scaled;

      scaled = gdk_pixbuf_scale_simple (base,
                                        width > 0 ? width :
                                        gdk_pixbuf_get_width (base),
                                        height > 0 ? height :
                                        gdk_pixbuf_get_height (base),
                                        GDK_INTERP_BILINEAR);

      g_object_unref (G_OBJECT (base));

      return scaled;
    }
}

void
_wnck_get_fallback_icons (GdkPixbuf **iconp,
                          int         ideal_width,
                          int         ideal_height,
                          GdkPixbuf **mini_iconp,
                          int         ideal_mini_width,
                          int         ideal_mini_height)
{
  if (iconp)
    *iconp = default_icon_at_size (ideal_width > 0 ? ideal_width :
                                   _wnck_get_default_icon_size (),
                                   ideal_height > 0 ? ideal_height :
                                   _wnck_get_default_icon_size ());

  if (mini_iconp)
    *mini_iconp = default_icon_at_size (ideal_mini_width > 0 ? ideal_mini_width :
                                        _wnck_get_default_mini_icon_size (),
                                        ideal_mini_height > 0 ? ideal_mini_height :
                                        _wnck_get_default_mini_icon_size ());
}


void
_wnck_get_window_geometry (Screen *screen,
			   Window  xwindow,
                           int    *xp,
                           int    *yp,
                           int    *widthp,
                           int    *heightp)
{
  Display *display;
  int x, y;
  unsigned int width, height, bw, depth;
  Window root_window;

  width = 1;
  height = 1;

  display = DisplayOfScreen (screen);

  _wnck_error_trap_push (display);

  XGetGeometry (display,
                xwindow,
                &root_window,
                &x, &y, &width, &height, &bw, &depth);

  _wnck_error_trap_pop (display);

  _wnck_get_window_position (screen, xwindow, xp, yp);

  if (widthp)
    *widthp = width;
  if (heightp)
    *heightp = height;
}

void _wnck_set_window_geometry (Screen *screen,
                                Window  xwindow,
                                int     gravity_and_flags,
                                int     x,
                                int     y,
                                int     width,
                                int     height)
{
  Display *display;
  Window   root;
  XEvent   xev;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = _wnck_atom_get ("_NET_MOVERESIZE_WINDOW");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = gravity_and_flags;
  xev.xclient.data.l[1] = x;
  xev.xclient.data.l[2] = y;
  xev.xclient.data.l[3] = width;
  xev.xclient.data.l[4] = height;

  _wnck_error_trap_push (display);
  XSendEvent (display,
              root,
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
  _wnck_error_trap_pop (display);
}

void
_wnck_get_window_position (Screen *screen,
			   Window  xwindow,
                           int    *xp,
                           int    *yp)
{
  Display *display;
  Window   root;
  int      x, y;
  Window   child;

  x = 0;
  y = 0;

  display = DisplayOfScreen (screen);
  root = RootWindowOfScreen (screen);

  _wnck_error_trap_push (display);
  XTranslateCoordinates (display,
                         xwindow,
			 root,
                         0, 0,
                         &x, &y, &child);
  _wnck_error_trap_pop (display);

  if (xp)
    *xp = x;
  if (yp)
    *yp = y;
}

void
_wnck_set_icon_geometry  (Screen *screen,
                          Window  xwindow,
			  int     x,
			  int     y,
			  int     width,
			  int     height)
{
  Display *display;
  gulong data[4];

  display = DisplayOfScreen (screen);

  data[0] = x;
  data[1] = y;
  data[2] = width;
  data[3] = height;

  _wnck_error_trap_push (display);

  XChangeProperty (display,
		   xwindow,
		   _wnck_atom_get ("_NET_WM_ICON_GEOMETRY"),
		   XA_CARDINAL, 32, PropModeReplace,
		   (guchar *)&data, 4);

  _wnck_error_trap_pop (display);
}

GdkDisplay*
_wnck_gdk_display_lookup_from_display (Display *display)
{
  GdkDisplay *gdkdisplay = NULL;

  gdkdisplay = gdk_x11_lookup_xdisplay (display);

  if (!gdkdisplay)
    g_warning ("No GdkDisplay matching Display \"%s\" was found.\n",
               DisplayString (display));

  return gdkdisplay;
}

GdkWindow*
_wnck_gdk_window_lookup_from_window (Screen *screen,
                                     Window  xwindow)
{
  Display    *display;
  GdkDisplay *gdkdisplay;

  display = DisplayOfScreen (screen);
  gdkdisplay = _wnck_gdk_display_lookup_from_display (display);
  if (!gdkdisplay)
    return NULL;

  return gdk_x11_window_lookup_for_display (gdkdisplay, xwindow);
}

/* orientation of pager */
#define _NET_WM_ORIENTATION_HORZ 0
#define _NET_WM_ORIENTATION_VERT 1

/* starting corner for counting spaces */
#define _NET_WM_TOPLEFT     0
#define _NET_WM_TOPRIGHT    1
#define _NET_WM_BOTTOMRIGHT 2
#define _NET_WM_BOTTOMLEFT  3

void
_wnck_set_desktop_layout (Screen *xscreen,
                          int     rows,
                          int     columns)
{
  Display *display;
  Window   root;
  gulong   data[4];

  /* FIXME: hack, hack, hack so as not
   * to have to add a orientation param
   * to wnck_screen_try_set_workspace_layout.
   *
   * Remove this crack asap.
   */
  g_assert ((rows == 0) || (columns == 0));

  display = DisplayOfScreen (xscreen);
  root = RootWindowOfScreen (xscreen);

  data[0] = (columns == 0) ? _NET_WM_ORIENTATION_HORZ : _NET_WM_ORIENTATION_VERT;
  data[1] = columns;
  data[2] = rows;
  data[3] = _NET_WM_TOPLEFT;

  _wnck_error_trap_push (display);

  XChangeProperty (display,
                   root,
		   _wnck_atom_get ("_NET_DESKTOP_LAYOUT"),
		   XA_CARDINAL, 32, PropModeReplace,
		   (guchar *)&data, 4);

  _wnck_error_trap_pop (display);
}

typedef struct
{
  Window window;
  Atom timestamp_prop_atom;
} TimeStampInfo;

static Bool
timestamp_predicate (Display *display,
		     XEvent  *xevent,
		     XPointer arg)
{
  TimeStampInfo *info = (TimeStampInfo *)arg;

  if (xevent->type == PropertyNotify &&
      xevent->xproperty.window == info->window &&
      xevent->xproperty.atom == info->timestamp_prop_atom)
    return True;

  return False;
}

/**
 * get_server_time:
 * @display: display from which to get the time
 * @window: a #Window, used for communication with the server.
 *          The window must have PropertyChangeMask in its
 *          events mask or a hang will result.
 *
 * Routine to get the current X server time stamp.
 *
 * Return value: the time stamp.
 **/
static Time
get_server_time (Display *display,
                 Window   window)
{
  unsigned char c = 'a';
  XEvent xevent;
  TimeStampInfo info;

  info.timestamp_prop_atom = _wnck_atom_get ("_TIMESTAMP_PROP");
  info.window = window;

  XChangeProperty (display, window,
		   info.timestamp_prop_atom, info.timestamp_prop_atom,
		   8, PropModeReplace, &c, 1);

  XIfEvent (display, &xevent,
	    timestamp_predicate, (XPointer)&info);

  return xevent.xproperty.time;
}

typedef struct
{
  Display *display;
  int screen_number;
  int token;
  Window window;
  Atom selection_atom;
  Atom manager_atom;
} LayoutManager;

static GSList *layout_managers = NULL;
static int next_token = 1;

static void
_wnck_free_layout_manager (LayoutManager *lm)
{
  _wnck_error_trap_push (lm->display);
  XDestroyWindow (lm->display, lm->window);
  _wnck_error_trap_pop (lm->display);

  g_slice_free (LayoutManager, lm);

  layout_managers = g_slist_remove (layout_managers, lm);
}

int
_wnck_try_desktop_layout_manager (Screen *xscreen,
                                  int     current_token)
{
  Display *display;
  Window root;
  Atom selection_atom;
  Window owner;
  GSList *tmp;
  int number;
  Time timestamp;
  XClientMessageEvent xev;
  char buffer[256];
  LayoutManager *lm;

  display = DisplayOfScreen (xscreen);
  root = RootWindowOfScreen (xscreen);
  number = XScreenNumberOfScreen (xscreen);

  sprintf (buffer, "_NET_DESKTOP_LAYOUT_S%d", number);
  selection_atom = _wnck_atom_get (buffer);

  owner = XGetSelectionOwner (display, selection_atom);

  tmp = layout_managers;
  while (tmp != NULL)
    {
      lm = tmp->data;

      if (display == lm->display &&
          number == lm->screen_number)
        {
          if (current_token == lm->token)
            {
              if (owner == lm->window)
                return current_token; /* we still have the selection */
              else
                { /* we lost the selection */
                  _wnck_free_layout_manager (lm);
                  break;
                }
            }
          else
            return WNCK_NO_MANAGER_TOKEN; /* someone else has it */
        }

      tmp = tmp->next;
    }

  if (owner != None)
    return WNCK_NO_MANAGER_TOKEN; /* someone else has the selection */

  /* No one has the selection at the moment */

  lm = g_slice_new0 (LayoutManager);

  lm->display = display;
  lm->screen_number = number;
  lm->token = next_token;
  ++next_token;

  lm->selection_atom = selection_atom;
  lm->manager_atom = _wnck_atom_get ("MANAGER");

  _wnck_error_trap_push (display);

  lm->window = XCreateSimpleWindow (display,
                                    root,
                                    0, 0, 10, 10, 0,
                                    WhitePixel (display, number),
                                    WhitePixel (display, number));

  XSelectInput (display, lm->window, PropertyChangeMask);
  timestamp = get_server_time (display, lm->window);

  XSetSelectionOwner (display, lm->selection_atom,
		      lm->window, timestamp);

  _wnck_error_trap_pop (display);

  /* Check to see if we managed to claim the selection. */

  if (XGetSelectionOwner (display, lm->selection_atom) !=
      lm->window)
    {
      g_free (lm);
      return WNCK_NO_MANAGER_TOKEN;
    }

  xev.type = ClientMessage;
  xev.window = root;
  xev.message_type = lm->manager_atom;
  xev.format = 32;
  xev.data.l[0] = timestamp;
  xev.data.l[1] = lm->selection_atom;
  xev.data.l[2] = lm->window;
  xev.data.l[3] = 0;	/* manager specific data */
  xev.data.l[4] = 0;	/* manager specific data */

  _wnck_error_trap_push (display);
  XSendEvent (display, root,
              False, StructureNotifyMask, (XEvent *)&xev);
  _wnck_error_trap_pop (display);

  layout_managers = g_slist_prepend (layout_managers,
                                     lm);

  return lm->token;
}

void
_wnck_release_desktop_layout_manager (Screen *xscreen,
                                      int     current_token)
{
  Display *display;
  GSList *tmp;
  int number;
  LayoutManager *lm;

  display = DisplayOfScreen (xscreen);
  number = XScreenNumberOfScreen (xscreen);

  tmp = layout_managers;
  while (tmp != NULL)
    {
      lm = tmp->data;

      if (display == lm->display &&
          number == lm->screen_number)
        {
          if (current_token == lm->token)
            {
              _wnck_error_trap_push (display);

              /* release selection ownership */
              if (XGetSelectionOwner (display, lm->selection_atom) !=
                  lm->window)
                {
                  Time timestamp;

                  timestamp = get_server_time (display, lm->window);
                  XSetSelectionOwner (display, lm->selection_atom,
                                      None, timestamp);
                }

              _wnck_error_trap_pop (display);

              _wnck_free_layout_manager (lm);
              return;
            }
        }

      tmp = tmp->next;
    }
}

gboolean
_wnck_desktop_layout_manager_process_event (XEvent *xev)
{
  GSList *tmp;
  LayoutManager *lm;

  if (xev->type != SelectionClear)
    return FALSE;

  tmp = layout_managers;
  while (tmp != NULL)
    {
      lm = tmp->data;

      if (xev->xany.display == lm->display &&
          xev->xany.window == lm->window &&
          xev->xselectionclear.selection == lm->selection_atom)
        {
          _wnck_free_layout_manager (lm);
          return TRUE;
        }

      tmp = tmp->next;
    }

  return FALSE;
}
