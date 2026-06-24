/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Matthias Clasen
 * Copyright (C) 2007 Anders Carlsson
 * Copyright (C) 2007 Rodrigo Moya
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2011 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "clipboard-manager.h"
#include "xsettings.h"

struct _GsdClipboardManagerPrivate
{
        guint    start_idle_id;
        Display *display;
        Window   window;
        Time     timestamp;

        GSList  *contents;
        GSList  *conversions;

        Window   requestor;
        Atom     property;
        Time     time;
};

typedef struct
{
        guchar *data;
        gulong  length;
        Atom    target;
        Atom    type;
        gint    format;
        gint    refcount;
} TargetData;

typedef struct
{
        Atom        target;
        TargetData *data;
        Atom        property;
        Window      requestor;
        gint        offset;
} IncrConversion;

static void     gsd_clipboard_manager_finalize    (GObject                  *object);
static void     clipboard_manager_watch_cb        (GsdClipboardManager *manager,
                                                   Window               window,
                                                   Bool                 is_start,
                                                   long                 mask,
                                                   void                *cb_data);

static gulong SELECTION_MAX_SIZE = 0;

static Atom XA_ATOM_PAIR = None;
static Atom XA_CLIPBOARD_MANAGER = None;
static Atom XA_CLIPBOARD = None;
static Atom XA_DELETE = None;
static Atom XA_INCR = None;
static Atom XA_INSERT_PROPERTY = None;
static Atom XA_INSERT_SELECTION = None;
static Atom XA_MANAGER = None;
static Atom XA_MULTIPLE = None;
static Atom XA_NULL = None;
static Atom XA_SAVE_TARGETS = None;
static Atom XA_TARGETS = None;
static Atom XA_TIMESTAMP = None;



G_DEFINE_TYPE_WITH_PRIVATE (GsdClipboardManager, gsd_clipboard_manager, G_TYPE_OBJECT)



static void
gsd_clipboard_manager_class_init (GsdClipboardManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gsd_clipboard_manager_finalize;
}

static void
gsd_clipboard_manager_init (GsdClipboardManager *manager)
{
        manager->priv = gsd_clipboard_manager_get_instance_private (manager);

        manager->priv->display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

}

static void
gsd_clipboard_manager_finalize (GObject *object)
{
        GsdClipboardManager *clipboard_manager = GSD_CLIPBOARD_MANAGER (object);

        if (clipboard_manager->priv->start_idle_id !=0)
                g_source_remove (clipboard_manager->priv->start_idle_id);

        G_OBJECT_CLASS (gsd_clipboard_manager_parent_class)->finalize (object);
}

/* We need to use reference counting for the target data, since we may
 * need to keep the data around after loosing the CLIPBOARD ownership
 * to complete incremental transfers.
 */
static TargetData *
target_data_ref (TargetData *data)
{
        data->refcount++;
        return data;
}

static void
target_data_unref (TargetData *data)
{
        data->refcount--;
        if (data->refcount == 0) {
                g_free (data->data);
                g_slice_free (TargetData, data);
        }
}

static void
conversion_free (IncrConversion *rdata)
{
        if (rdata->data)
                target_data_unref (rdata->data);
        g_slice_free (IncrConversion, rdata);
}

static void
send_selection_notify (GsdClipboardManager *manager,
                       Bool                 success)
{
        XSelectionEvent notify;

        notify.type = SelectionNotify;
        notify.serial = 0;
        notify.send_event = True;
        notify.display = manager->priv->display;
        notify.requestor = manager->priv->requestor;
        notify.selection = XA_CLIPBOARD_MANAGER;
        notify.target = XA_SAVE_TARGETS;
        notify.property = success ? manager->priv->property : None;
        notify.time = manager->priv->time;

        gdk_x11_display_error_trap_push (gdk_display_get_default ());

        XSendEvent (manager->priv->display,
                    manager->priv->requestor,
                    False,
                    NoEventMask,
                    (XEvent *)&notify);
        XSync (manager->priv->display, False);

        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
        {
                g_critical ("Failed to notify clipboard selection");
        }
}

static void
finish_selection_request (GsdClipboardManager *manager,
                          XEvent              *xev,
                          Bool                 success)
{
        XSelectionEvent notify;

        notify.type = SelectionNotify;
        notify.serial = 0;
        notify.send_event = True;
        notify.display = xev->xselectionrequest.display;
        notify.requestor = xev->xselectionrequest.requestor;
        notify.selection = xev->xselectionrequest.selection;
        notify.target = xev->xselectionrequest.target;
        notify.property = success ? xev->xselectionrequest.property : None;
        notify.time = xev->xselectionrequest.time;

        gdk_x11_display_error_trap_push (gdk_display_get_default ());

        XSendEvent (xev->xselectionrequest.display,
                    xev->xselectionrequest.requestor,
                    False, NoEventMask, (XEvent *) &notify);
        XSync (manager->priv->display, False);

        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
        {
                g_critical ("Failed to send selection request");
        }
}

static int
clipboard_bytes_per_item (int format)
{
        switch (format) {
        case 8: return sizeof (char);
        case 16: return sizeof (short);
        case 32: return sizeof (long);
        default: ;
        }

        return 0;
}

static void
save_targets (GsdClipboardManager *manager,
              Atom                *targets,
              int                  nitems)
{
        gint        nout, i;
        Atom       *multiple;
        TargetData *tdata;

        multiple = g_new (Atom, 2 * nitems);

        nout = 0;
        for (i = 0; i < nitems; i++) {
                if (targets[i] != XA_TARGETS &&
                    targets[i] != XA_MULTIPLE &&
                    targets[i] != XA_DELETE &&
                    targets[i] != XA_INSERT_PROPERTY &&
                    targets[i] != XA_INSERT_SELECTION &&
                    targets[i] != XA_PIXMAP) {
                        tdata = g_slice_new (TargetData);
                        tdata->data = NULL;
                        tdata->length = 0;
                        tdata->target = targets[i];
                        tdata->type = None;
                        tdata->format = 0;
                        tdata->refcount = 1;
                        manager->priv->contents = g_slist_prepend (manager->priv->contents, tdata);

                        multiple[nout++] = targets[i];
                        multiple[nout++] = targets[i];
                }
        }

        XFree (targets);

        XChangeProperty (manager->priv->display, manager->priv->window,
                         XA_MULTIPLE, XA_ATOM_PAIR,
                         32, PropModeReplace, (const guchar *) multiple, nout);
        g_free (multiple);

        XConvertSelection (manager->priv->display, XA_CLIPBOARD,
                           XA_MULTIPLE, XA_MULTIPLE,
                           manager->priv->window, manager->priv->time);
}

static int
find_content_target (TargetData *tdata,
                     Atom       *target)
{
        return !(tdata->target == *target);
}

static int
find_content_type (TargetData *tdata,
                   Atom        *type)
{
        return !(tdata->type == *type);
}

static int
find_conversion_requestor (IncrConversion *rdata,
                           XEvent         *xev)
{
        return !(rdata->requestor == xev->xproperty.window
                 && rdata->property == xev->xproperty.atom);
}

static void
get_property (TargetData          *tdata,
              GsdClipboardManager *manager)
{
        Atom    type;
        gint    format;
        gulong  length;
        gulong  remaining;
        guchar *data;

        XGetWindowProperty (manager->priv->display,
                            manager->priv->window,
                            tdata->target,
                            0,
                            0x1FFFFFFF,
                            True,
                            AnyPropertyType,
                            &type,
                            &format,
                            &length,
                            &remaining,
                            &data);

        if (type == None) {
                manager->priv->contents = g_slist_remove (manager->priv->contents, tdata);
                g_slice_free (TargetData, tdata);
        } else if (type == XA_INCR) {
                tdata->type = type;
                tdata->length = 0;
                XFree (data);
        } else {
                tdata->type = type;
                tdata->data = data;
                tdata->length = length * clipboard_bytes_per_item (format);
                tdata->format = format;
        }
}

static Bool
receive_incrementally (GsdClipboardManager *manager,
                       XEvent              *xev)
{
        GSList     *list;
        TargetData *tdata;
        Atom        type;
        gint        format;
        gulong      length, nitems, remaining;
        guchar     *data;

        if (xev->xproperty.window != manager->priv->window)
                return False;

        list = g_slist_find_custom (manager->priv->contents,
                                    &xev->xproperty.atom,
                                    (GCompareFunc) find_content_target);

        if (!list)
                return False;

        tdata = (TargetData *) list->data;

        if (tdata->type != XA_INCR)
                return False;

        XGetWindowProperty (xev->xproperty.display,
                            xev->xproperty.window,
                            xev->xproperty.atom,
                            0, 0x1FFFFFFF, True, AnyPropertyType,
                            &type, &format, &nitems, &remaining, &data);

        length = nitems * clipboard_bytes_per_item (format);
        if (length == 0) {
                tdata->type = type;
                tdata->format = format;

                if (!g_slist_find_custom (manager->priv->contents,
                                          &XA_INCR, (GCompareFunc) find_content_type)) {

                        /* all incremental transfers done */
                        send_selection_notify (manager, True);
                        manager->priv->requestor = None;
                }

                XFree (data);
        } else {
                if (!tdata->data) {
                        tdata->data = data;
                        tdata->length = length;
                } else {
                        tdata->data = g_realloc (tdata->data, tdata->length + length + 1);
                        memcpy (tdata->data + tdata->length, data, length + 1);
                        tdata->length += length;
                        XFree (data);
                }
        }

        return True;
}

static Bool
send_incrementally (GsdClipboardManager *manager,
                    XEvent              *xev)
{
        GSList         *list;
        IncrConversion *rdata;
        gulong          length;
        gulong          items;
        gulong          bytes;
        guchar         *data;

        list = g_slist_find_custom (manager->priv->conversions, xev,
                                    (GCompareFunc) find_conversion_requestor);
        if (list == NULL)
                return False;

        rdata = (IncrConversion *) list->data;

        data = rdata->data->data + rdata->offset;
        length = rdata->data->length - rdata->offset;
        if (length > SELECTION_MAX_SIZE)
                length = SELECTION_MAX_SIZE;

        rdata->offset += length;

        bytes = clipboard_bytes_per_item (rdata->data->format);
        items = bytes == 0 ? 0 : length / bytes;

        XChangeProperty (manager->priv->display, rdata->requestor,
                         rdata->property, rdata->data->type,
                         rdata->data->format, PropModeAppend,
                         data, items);

        if (length == 0) {
                clipboard_manager_watch_cb (manager, rdata->requestor, False,
                                            PropertyChangeMask, NULL);
                manager->priv->conversions = g_slist_remove (manager->priv->conversions, rdata);
                conversion_free (rdata);
        }

        return True;
}

static void
convert_clipboard_manager (GsdClipboardManager *manager,
                           XEvent              *xev)
{
        Atom    type = None;
        gint    format;
        gulong  nitems;
        gulong  remaining;
        Atom   *targets = NULL;
        Atom    targets2[3];
        gint    n_targets;

        if (xev->xselectionrequest.target == XA_SAVE_TARGETS) {
                if (manager->priv->requestor != None || manager->priv->contents != NULL) {
                        /* We're in the middle of a conversion request, or own
                         * the CLIPBOARD already
                         */
                        finish_selection_request (manager, xev, False);
                } else {
                        gdk_x11_display_error_trap_push (gdk_display_get_default ());

                        clipboard_manager_watch_cb (manager,
                                                    xev->xselectionrequest.requestor,
                                                    True,
                                                    StructureNotifyMask,
                                                    NULL);
                        XSelectInput (manager->priv->display,
                                      xev->xselectionrequest.requestor,
                                      StructureNotifyMask);
                        XSync (manager->priv->display, False);

                        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != Success)
                                return;

                        gdk_x11_display_error_trap_push (gdk_display_get_default ());

                        if (xev->xselectionrequest.property != None) {
                                XGetWindowProperty (manager->priv->display,
                                                    xev->xselectionrequest.requestor,
                                                    xev->xselectionrequest.property,
                                                    0, 0x1FFFFFFF, False, XA_ATOM,
                                                    &type, &format, &nitems, &remaining,
                                                    (guchar **) &targets);

                                if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != Success) {
                                        if (targets)
                                                XFree (targets);

                                        return;
                                }
                        }

                        manager->priv->requestor = xev->xselectionrequest.requestor;
                        manager->priv->property = xev->xselectionrequest.property;
                        manager->priv->time = xev->xselectionrequest.time;

                        if (type == None)
                                XConvertSelection (manager->priv->display, XA_CLIPBOARD,
                                                   XA_TARGETS, XA_TARGETS,
                                                   manager->priv->window, manager->priv->time);
                        else
                                save_targets (manager, targets, nitems);
                }
        } else if (xev->xselectionrequest.target == XA_TIMESTAMP) {
                XChangeProperty (manager->priv->display,
                                 xev->xselectionrequest.requestor,
                                 xev->xselectionrequest.property,
                                 XA_INTEGER, 32, PropModeReplace,
                                 (guchar *) &manager->priv->timestamp, 1);

                finish_selection_request (manager, xev, True);
        } else if (xev->xselectionrequest.target == XA_TARGETS) {
                n_targets = 0;
                targets2[n_targets++] = XA_TARGETS;
                targets2[n_targets++] = XA_TIMESTAMP;
                targets2[n_targets++] = XA_SAVE_TARGETS;

                XChangeProperty (manager->priv->display,
                                 xev->xselectionrequest.requestor,
                                 xev->xselectionrequest.property,
                                 XA_ATOM, 32, PropModeReplace,
                                 (guchar *) targets2, n_targets);

                finish_selection_request (manager, xev, True);
        } else
                finish_selection_request (manager, xev, False);
}

static void
convert_clipboard_target (IncrConversion      *rdata,
                          GsdClipboardManager *manager)
{
        TargetData        *tdata;
        Atom              *targets;
        gint               n_targets;
        GSList            *list;
        gulong             items;
        gulong             bytes;
        XWindowAttributes  atts;

        if (rdata->target == XA_TARGETS) {
                n_targets = g_slist_length (manager->priv->contents) + 2;
                targets = g_new (Atom, n_targets);

                n_targets = 0;
                targets[n_targets++] = XA_TARGETS;
                targets[n_targets++] = XA_MULTIPLE;

                for (list = manager->priv->contents; list; list = list->next) {
                        tdata = (TargetData *) list->data;
                        targets[n_targets++] = tdata->target;
                }

                XChangeProperty (manager->priv->display, rdata->requestor,
                                 rdata->property,
                                 XA_ATOM, 32, PropModeReplace,
                                 (guchar *) targets, n_targets);
                g_free (targets);
        } else  {
                /* Convert from stored CLIPBOARD data */
                list = g_slist_find_custom (manager->priv->contents,
                                            &rdata->target,
                                            (GCompareFunc) find_content_target);

                /* We got a target that we don't support */
                if (!list)
                        return;

                tdata = (TargetData *)list->data;
                if (tdata->type == XA_INCR) {
                        /* we haven't completely received this target yet  */
                        rdata->property = None;
                        return;
                }

                rdata->data = target_data_ref (tdata);
                bytes = clipboard_bytes_per_item (tdata->format);
                items = bytes == 0 ? 0 : tdata->length / bytes;
                if (tdata->length <= SELECTION_MAX_SIZE)
                        XChangeProperty (manager->priv->display, rdata->requestor,
                                         rdata->property,
                                         tdata->type, tdata->format, PropModeReplace,
                                         tdata->data, items);
                else {
                        /* start incremental transfer */
                        rdata->offset = 0;

                        gdk_x11_display_error_trap_push (gdk_display_get_default ());

                        XGetWindowAttributes (manager->priv->display, rdata->requestor, &atts);

                        clipboard_manager_watch_cb (manager, rdata->requestor,
                                                    True, PropertyChangeMask,
                                                    NULL);

                        XSelectInput (manager->priv->display, rdata->requestor,
                                      atts.your_event_mask | PropertyChangeMask);

                        XChangeProperty (manager->priv->display, rdata->requestor,
                                         rdata->property,
                                         XA_INCR, 32, PropModeReplace,
                                         (guchar *) &items, 1);

                        XSync (manager->priv->display, False);

                        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
                        {
                                g_critical ("Failed to transfer clipboard contents");
                        }
                }
        }
}

static void
collect_incremental (IncrConversion      *rdata,
                     GsdClipboardManager *manager)
{
        if (rdata->offset >= 0)
                manager->priv->conversions = g_slist_prepend (manager->priv->conversions, rdata);
        else
                conversion_free (rdata);
}

static void
convert_clipboard (GsdClipboardManager *manager,
                   XEvent              *xev)
{
        GSList         *list;
        GSList         *conversions = NULL;
        IncrConversion *rdata;
        Atom            type = None;
        gint            format;
        gulong          i, nitems;
        gulong          remaining;
        Atom           *multiple;

        if (xev->xselectionrequest.target == XA_MULTIPLE) {
                XGetWindowProperty (xev->xselectionrequest.display,
                                    xev->xselectionrequest.requestor,
                                    xev->xselectionrequest.property,
                                    0, 0x1FFFFFFF, False, XA_ATOM_PAIR,
                                    &type, &format, &nitems, &remaining,
                                    (guchar **) &multiple);

                if (type != XA_ATOM_PAIR || nitems == 0) {
                        if (multiple)
                                g_free (multiple);
                        return;
                }

                for (i = 0; i < nitems; i += 2) {
                        rdata = g_slice_new (IncrConversion);
                        rdata->requestor = xev->xselectionrequest.requestor;
                        rdata->target = multiple[i];
                        rdata->property = multiple[i+1];
                        rdata->data = NULL;
                        rdata->offset = -1;
                        conversions = g_slist_prepend (conversions, rdata);
                }
        } else {
                multiple = NULL;

                rdata = g_slice_new (IncrConversion);
                rdata->requestor = xev->xselectionrequest.requestor;
                rdata->target = xev->xselectionrequest.target;
                rdata->property = xev->xselectionrequest.property;
                rdata->data = NULL;
                rdata->offset = -1;
                conversions = g_slist_prepend (conversions, rdata);
        }

        g_slist_foreach (conversions, (GFunc) convert_clipboard_target, manager);

        if (conversions->next == NULL &&
            ((IncrConversion *) conversions->data)->property == None) {
                finish_selection_request (manager, xev, False);
        } else {
                if (multiple) {
                        i = 0;
                        for (list = conversions; list; list = list->next) {
                                rdata = (IncrConversion *)list->data;
                                multiple[i++] = rdata->target;
                                multiple[i++] = rdata->property;
                        }
                        XChangeProperty (xev->xselectionrequest.display,
                                         xev->xselectionrequest.requestor,
                                         xev->xselectionrequest.property,
                                         XA_ATOM_PAIR, 32, PropModeReplace,
                                         (guchar *) multiple, nitems);
                }
                finish_selection_request (manager, xev, True);
        }

        g_slist_foreach (conversions, (GFunc) collect_incremental, manager);
        g_slist_free (conversions);

        g_free (multiple);
}

static Bool
clipboard_manager_process_event (GsdClipboardManager *manager,
                                 XEvent              *xev)
{
        Atom    type;
        gint    format;
        gulong  nitems;
        gulong  remaining;
        Atom   *targets = NULL;
        GSList *tmp;

        switch (xev->xany.type) {
        case DestroyNotify:
                if (xev->xdestroywindow.window == manager->priv->requestor) {
                        g_slist_foreach (manager->priv->contents, (GFunc) (void (*)(void)) target_data_unref, NULL);
                        g_slist_free (manager->priv->contents);
                        manager->priv->contents = NULL;

                        clipboard_manager_watch_cb (manager,
                                                    manager->priv->requestor,
                                                    False,
                                                    0,
                                                    NULL);
                        manager->priv->requestor = None;
                }
                break;

        case PropertyNotify:
                if (xev->xproperty.state == PropertyNewValue) {
                        return receive_incrementally (manager, xev);
                } else {
                        return send_incrementally (manager, xev);
                }
                break;

        case SelectionClear:
                if (xev->xany.window != manager->priv->window)
                        return False;

                if (xev->xselectionclear.selection == XA_CLIPBOARD_MANAGER) {
                        /* We lost the manager selection */
                        if (manager->priv->contents) {
                                g_slist_foreach (manager->priv->contents, (GFunc) (void (*)(void)) target_data_unref, NULL);
                                g_slist_free (manager->priv->contents);
                                manager->priv->contents = NULL;

                                XSetSelectionOwner (manager->priv->display,
                                                    XA_CLIPBOARD,
                                                    None, manager->priv->time);
                        }

                        return True;
                }
                if (xev->xselectionclear.selection == XA_CLIPBOARD) {
                        /* We lost the clipboard selection */
                        g_slist_foreach (manager->priv->contents, (GFunc) (void (*)(void)) target_data_unref, NULL);
                        g_slist_free (manager->priv->contents);
                        manager->priv->contents = NULL;
                        clipboard_manager_watch_cb (manager,
                                                    manager->priv->requestor,
                                                    False,
                                                    0,
                                                    NULL);
                        manager->priv->requestor = None;

                        return True;
                }
                break;

        case SelectionNotify:
                if (xev->xany.window != manager->priv->window)
                        return False;

                if (xev->xselection.selection == XA_CLIPBOARD) {
                        /* a CLIPBOARD conversion is done */
                        if (xev->xselection.property == XA_TARGETS) {
                                XGetWindowProperty (xev->xselection.display,
                                                    xev->xselection.requestor,
                                                    xev->xselection.property,
                                                    0, 0x1FFFFFFF, True, XA_ATOM,
                                                    &type, &format, &nitems, &remaining,
                                                    (guchar **) &targets);

                                save_targets (manager, targets, nitems);
                        } else if (xev->xselection.property == XA_MULTIPLE) {
                                tmp = g_slist_copy (manager->priv->contents);
                                g_slist_foreach (tmp, (GFunc) get_property, manager);
                                g_slist_free (tmp);

                                manager->priv->time = xev->xselection.time;
                                XSetSelectionOwner (manager->priv->display, XA_CLIPBOARD,
                                                    manager->priv->window, manager->priv->time);

                                if (manager->priv->property != None)
                                        XChangeProperty (manager->priv->display,
                                                         manager->priv->requestor,
                                                         manager->priv->property,
                                                         XA_ATOM, 32, PropModeReplace,
                                                         (guchar *)&XA_NULL, 1);

                                if (!g_slist_find_custom (manager->priv->contents,
                                                          &XA_INCR, (GCompareFunc) find_content_type)) {
                                        /* all transfers done */
                                        send_selection_notify (manager, True);
                                        clipboard_manager_watch_cb (manager,
                                                                    manager->priv->requestor,
                                                                    False,
                                                                    0,
                                                                    NULL);
                                        manager->priv->requestor = None;
                                }
                        }
                        else if (xev->xselection.property == None) {
                                send_selection_notify (manager, False);
                                clipboard_manager_watch_cb (manager,
                                                            manager->priv->requestor,
                                                            False,
                                                            0,
                                                            NULL);
                                manager->priv->requestor = None;
                        }

                        return True;
                }
                break;

        case SelectionRequest:
                if (xev->xany.window != manager->priv->window) {
                        return False;
                }

                if (xev->xselectionrequest.selection == XA_CLIPBOARD_MANAGER) {
                        convert_clipboard_manager (manager, xev);
                        return True;
                } else if (xev->xselectionrequest.selection == XA_CLIPBOARD) {
                        convert_clipboard (manager, xev);
                        return True;
                }
                break;

        default:;
        }

        return False;
}

static GdkFilterReturn
clipboard_manager_event_filter (GdkXEvent           *xevent,
                                GdkEvent            *event,
                                GsdClipboardManager *manager)
{
        if (clipboard_manager_process_event (manager, (XEvent *)xevent)) {
                return GDK_FILTER_REMOVE;
        } else {
                return GDK_FILTER_CONTINUE;
        }
}

static void
clipboard_manager_watch_cb (GsdClipboardManager *manager,
                            Window               window,
                            Bool                 is_start,
                            long                 mask,
                            void                *cb_data)
{
        GdkWindow  *gdkwin;
        GdkDisplay *display;

        display = gdk_display_get_default ();
        gdkwin = gdk_x11_window_lookup_for_display (display, window);

        if (is_start) {
                if (gdkwin == NULL) {
                        gdkwin = gdk_x11_window_foreign_new_for_display (display, window);
                } else {
                        g_object_ref (gdkwin);
                }

                gdk_window_add_filter (gdkwin,
                                       (GdkFilterFunc) clipboard_manager_event_filter,
                                       manager);
        } else {
                if (gdkwin == NULL) {
                        return;
                }
                gdk_window_remove_filter (gdkwin,
                                          (GdkFilterFunc) clipboard_manager_event_filter,
                                          manager);
                g_object_unref (gdkwin);
        }
}

static void
init_atoms (Display *display)
{
    gulong max_request_size;

    if (SELECTION_MAX_SIZE > 0)
      return;

    XA_ATOM_PAIR = XInternAtom (display, "ATOM_PAIR", False);
    XA_CLIPBOARD_MANAGER = XInternAtom (display, "CLIPBOARD_MANAGER", False);
    XA_CLIPBOARD = XInternAtom (display, "CLIPBOARD", False);
    XA_DELETE = XInternAtom (display, "DELETE", False);
    XA_INCR = XInternAtom (display, "INCR", False);
    XA_INSERT_PROPERTY = XInternAtom (display, "INSERT_PROPERTY", False);
    XA_INSERT_SELECTION = XInternAtom (display, "INSERT_SELECTION", False);
    XA_MANAGER = XInternAtom (display, "MANAGER", False);
    XA_MULTIPLE = XInternAtom (display, "MULTIPLE", False);
    XA_NULL = XInternAtom (display, "NULL", False);
    XA_SAVE_TARGETS = XInternAtom (display, "SAVE_TARGETS", False);
    XA_TARGETS = XInternAtom (display, "TARGETS", False);
    XA_TIMESTAMP = XInternAtom (display, "TIMESTAMP", False);

    max_request_size = XExtendedMaxRequestSize (display);
    if (max_request_size == 0)
      max_request_size = XMaxRequestSize (display);

    SELECTION_MAX_SIZE = max_request_size - 100;
    if (SELECTION_MAX_SIZE > 262144)
      SELECTION_MAX_SIZE =  262144;
}

gboolean
gsd_clipboard_manager_start (GsdClipboardManager *manager,
                             gboolean             replace)
{
        XClientMessageEvent xev;

        init_atoms (manager->priv->display);

        /* check if there is a clipboard manager running */
        if (!replace
            && XGetSelectionOwner (manager->priv->display, XA_CLIPBOARD_MANAGER)) {
                return FALSE;
        }

        manager->priv->contents = NULL;
        manager->priv->conversions = NULL;
        manager->priv->requestor = None;

        manager->priv->window = XCreateSimpleWindow (manager->priv->display,
                                                     DefaultRootWindow (manager->priv->display),
                                                     0, 0, 10, 10, 0,
                                                     WhitePixel (manager->priv->display,
                                                                 DefaultScreen (manager->priv->display)),
                                                     WhitePixel (manager->priv->display,
                                                                 DefaultScreen (manager->priv->display)));
        clipboard_manager_watch_cb (manager,
                                    manager->priv->window,
                                    True,
                                    PropertyChangeMask,
                                    NULL);
        XSelectInput (manager->priv->display,
                      manager->priv->window,
                      PropertyChangeMask);
        manager->priv->timestamp = xfce_xsettings_get_server_time (manager->priv->display, manager->priv->window);

        XSetSelectionOwner (manager->priv->display,
                            XA_CLIPBOARD_MANAGER,
                            manager->priv->window,
                            manager->priv->timestamp);

        /* Check to see if we managed to claim the selection. If not,
         * we treat it as if we got it then immediately lost it
         */
        if (XGetSelectionOwner (manager->priv->display, XA_CLIPBOARD_MANAGER) == manager->priv->window) {
                xev.type = ClientMessage;
                xev.window = DefaultRootWindow (manager->priv->display);
                xev.message_type = XA_MANAGER;
                xev.format = 32;
                xev.data.l[0] = manager->priv->timestamp;
                xev.data.l[1] = XA_CLIPBOARD_MANAGER;
                xev.data.l[2] = manager->priv->window;
                xev.data.l[3] = 0;      /* manager specific data */
                xev.data.l[4] = 0;      /* manager specific data */

                XSendEvent (manager->priv->display,
                            DefaultRootWindow (manager->priv->display),
                            False,
                            StructureNotifyMask,
                            (XEvent *)&xev);
        } else {
                clipboard_manager_watch_cb (manager,
                                            manager->priv->window,
                                            False,
                                            0,
                                            NULL);
        }

        manager->priv->start_idle_id = 0;

        return TRUE;
}

void
gsd_clipboard_manager_stop (GsdClipboardManager *manager)
{
        if (manager->priv->window != None) {
                clipboard_manager_watch_cb (manager,
                                            manager->priv->window,
                                            False,
                                            0,
                                            NULL);
                XDestroyWindow (manager->priv->display, manager->priv->window);
                manager->priv->window = None;
        }

        if (manager->priv->conversions != NULL) {
                g_slist_foreach (manager->priv->conversions, (GFunc) (void (*)(void)) conversion_free, NULL);
                g_slist_free (manager->priv->conversions);
                manager->priv->conversions = NULL;
        }

        if (manager->priv->contents != NULL) {
                g_slist_foreach (manager->priv->contents, (GFunc) (void (*)(void)) target_data_unref, NULL);
                g_slist_free (manager->priv->contents);
                manager->priv->contents = NULL;
        }
}
