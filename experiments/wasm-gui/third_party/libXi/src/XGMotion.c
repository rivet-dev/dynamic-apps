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
 * XGetDeviceMotionEvents - Get the motion history of an input device.
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
#include <limits.h>

XDeviceTimeCoord *
XGetDeviceMotionEvents(
    register Display	*dpy,
    XDevice		*dev,
    Time		 start,
    Time		 stop,
    int			*nEvents,
    int			*mode,
    int			*axis_count)
{
    xGetDeviceMotionEventsReq *req;
    xGetDeviceMotionEventsReply rep;
    XDeviceTimeCoord *tc;
    int *data, *bufp, *readp, *savp;
    unsigned long size;
    int i, j;
    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_Initial_Release, info) == -1)
        return NULL;

    GetReq(GetDeviceMotionEvents, req);
    req->reqType = info->codes->major_opcode;
    req->ReqType = X_GetDeviceMotionEvents;
    req->start = start;
    req->stop = stop;
    req->deviceid = dev->device_id;

    if (!_XReply(dpy, (xReply *) & rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	*nEvents = 0;
	return (NULL);
    }

    *mode = rep.mode;
    *axis_count = rep.axes;
    *nEvents = rep.nEvents;
    if (!rep.nEvents) {
	UnlockDisplay(dpy);
	SyncHandle();
	return (NULL);
    }
    if (rep.length < (INT_MAX >> 2)) {
	size = rep.length << 2;
	savp = readp = Xmalloc(size);
    } else {
	size = 0;
	savp = readp = NULL;
    }
    /* rep.axes is a CARD8, so assume max number of axes for bounds check */
    if (rep.nEvents <
	(INT_MAX / (sizeof(XDeviceTimeCoord) + (UCHAR_MAX * sizeof(int)))) &&
	rep.nEvents * (rep.axes + 1) <= rep.length) {
	size_t bsize = rep.nEvents *
	    (sizeof(XDeviceTimeCoord) + (rep.axes * sizeof(int)));
	bufp = Xmalloc(bsize);
    } else
	bufp = NULL;
    if (!bufp || !savp) {
	Xfree(bufp);
	Xfree(savp);
	*nEvents = 0;
	_XEatDataWords(dpy, rep.length);
	UnlockDisplay(dpy);
	SyncHandle();
	return (NULL);
    }
    _XRead(dpy, (char *)readp, size);

    tc = (XDeviceTimeCoord *) bufp;
    data = (int *)(tc + rep.nEvents);
    for (i = 0; i < *nEvents; i++, tc++) {
	tc->time = *readp++;
	tc->data = data;
	for (j = 0; j < *axis_count; j++)
	    *data++ = *readp++;
    }
    XFree((char *)savp);
    UnlockDisplay(dpy);
    SyncHandle();
    return ((XDeviceTimeCoord *) bufp);
}

void
XFreeDeviceMotionEvents(XDeviceTimeCoord *events)
{
    XFree((char *)events);
}
