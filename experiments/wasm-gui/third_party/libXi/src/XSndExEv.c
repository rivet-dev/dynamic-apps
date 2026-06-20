/************************************************************

Copyright 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Copyright 1989 by Hewlett-Packard Company, Palo Alto, California.

			All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Hewlett-Packard not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

HEWLETT-PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
HEWLETT-PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

********************************************************/

/***********************************************************************
 *
 * XSendExtensionEvent - send an extension event to a client.
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/extensions/XI.h>
#include <X11/extensions/XIproto.h>
#include <X11/Xlibint.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/extutil.h>
#include "XIint.h"

/* Xlib's wire_vec is defined for a single event only, libXi may return
 * multiple events.
 */
typedef Status (*ext_event_to_wire)(Display*, XEvent*, xEvent**, int*);

Status
XSendExtensionEvent(
    register Display	*dpy,
    XDevice		*dev,
    Window		 dest,
    Bool		 prop,
    int			 count,
    XEventClass		*list,
    XEvent		*event)
{
    int num_events;
    int ev_size;
    xSendExtensionEventReq *req;
    xEvent *ev;
    ext_event_to_wire *fp;
    Status status;
    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_Initial_Release, info) == -1)
	return (NoSuchExtension);

    /* call through display to find proper conversion routine */

    fp = (ext_event_to_wire*)&dpy->wire_vec[event->type & 0177];
    if (*fp == NULL)
	*fp = _XiEventToWire;
    status = (*fp) (dpy, event, &ev, &num_events);

    if (status) {
	GetReq(SendExtensionEvent, req);
	req->reqType = info->codes->major_opcode;
	req->ReqType = X_SendExtensionEvent;
	req->deviceid = dev->device_id;
	req->destination = dest;
	req->propagate = prop;
	req->count = count;
	req->num_events = num_events;
	ev_size = num_events * sizeof(xEvent);
	req->length += (count + (ev_size >> 2));

	/* note: Data is a macro that uses its arguments multiple
	 * times, so "count" is changed in a separate assignment
	 * statement.  Any extra events must be sent before the event
	 * list, in order to ensure quad alignment. */

	Data(dpy, (char *)ev, ev_size);

	count <<= 2;
	Data32(dpy, (long *)list, count);
	XFree((char *)ev);
    }

    UnlockDisplay(dpy);
    SyncHandle();
    return (status);
}
