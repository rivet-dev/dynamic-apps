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
 * XChangeDeviceControl - Change the control attributes of an extension
 * input device.
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

int
XChangeDeviceControl(
    register Display	*dpy,
    XDevice		*dev,
    int			 control,
    XDeviceControl	*d)
{
    int length;
    xChangeDeviceControlReq *req;
    xChangeDeviceControlReply rep;
    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_Add_XChangeDeviceControl, info) == -1)
	return (NoSuchExtension);

    GetReq(ChangeDeviceControl, req);
    req->reqType = info->codes->major_opcode;
    req->ReqType = X_ChangeDeviceControl;
    req->deviceid = dev->device_id;
    req->control = control;

    switch (control) {
    case DEVICE_RESOLUTION:
    {
	XDeviceResolutionControl *R;
	xDeviceResolutionCtl r;

	R = (XDeviceResolutionControl *) d;
	r.control = DEVICE_RESOLUTION;
	r.length = sizeof(xDeviceResolutionCtl) +
        R->num_valuators * sizeof(int);
	r.first_valuator = R->first_valuator;
	r.num_valuators = R->num_valuators;
	req->length += ((unsigned)(r.length + 3) >> 2);
	length = sizeof(xDeviceResolutionCtl);
	Data(dpy, (char *)&r, length);
	length = r.num_valuators * sizeof(int);
	Data(dpy, (char *)R->resolutions, length);
	if (!_XReply(dpy, (xReply *) & rep, 0, xTrue)) {
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return (NoSuchExtension);
	} else {
            UnlockDisplay(dpy);
            SyncHandle();
	    return (rep.status);
        }
    }
    case DEVICE_ABS_CALIB:
    {
        XDeviceAbsCalibControl *C = (XDeviceAbsCalibControl *) d;
        xDeviceAbsCalibCtl c;

        c.control = DEVICE_ABS_CALIB;
        c.length = sizeof(c);
        c.min_x = C->min_x;
        c.max_x = C->max_x;
        c.min_y = C->min_y;
        c.max_y = C->max_y;
        c.flip_x = C->flip_x;
        c.flip_y = C->flip_y;
        c.rotation = C->rotation;
        c.button_threshold = C->button_threshold;

        req->length += (sizeof(c) + 3) >> 2;
        Data(dpy, (char *) &c, sizeof(c));

        if (!_XReply(dpy, (xReply *) &rep, 0, xTrue)) {
            UnlockDisplay(dpy);
            SyncHandle();
            return NoSuchExtension;
        }
        else {
            UnlockDisplay(dpy);
            SyncHandle();
            return rep.status;
        }
    }
    case DEVICE_ABS_AREA:
    {
        XDeviceAbsAreaControl *A = (XDeviceAbsAreaControl *) d;
        xDeviceAbsAreaCtl a;

        a.control = DEVICE_ABS_AREA;
        a.length = sizeof(a);
        a.offset_x = A->offset_x;
        a.offset_y = A->offset_y;
        a.width = A->width;
        a.height = A->height;
        a.screen = A->screen;
        a.following = A->following;

        req->length += (sizeof(a) + 3) >> 2;
        Data(dpy, (char *) &a, sizeof(a));

        if (!_XReply(dpy, (xReply *) &rep, 0, xTrue)) {
            UnlockDisplay(dpy);
            SyncHandle();
            return NoSuchExtension;
        }
        else {
            UnlockDisplay(dpy);
            SyncHandle();
            return rep.status;
        }
    }
    case DEVICE_CORE:
    {
        XDeviceCoreControl *C = (XDeviceCoreControl *) d;
        xDeviceCoreCtl c;

        c.control = DEVICE_CORE;
        c.length = sizeof(c);
        c.status = C->status;

        req->length += (sizeof(c) + 3) >> 2;
        Data (dpy, (char *) &c, sizeof(c));

        if (!_XReply(dpy, (xReply *) &rep, 0, xTrue)) {
            UnlockDisplay(dpy);
            SyncHandle();
            return NoSuchExtension;
        }
        else {
            UnlockDisplay(dpy);
            SyncHandle();
            return rep.status;
        }
    }
    case DEVICE_ENABLE:
    {
        XDeviceEnableControl *E = (XDeviceEnableControl *) d;
        xDeviceEnableCtl e;

        e.control = DEVICE_ENABLE;
        e.length = sizeof(e);
        e.enable = E->enable;

        req->length += (sizeof(e) + 3) >> 2;
        Data (dpy, (char *) &e, sizeof(e));

        if (!_XReply(dpy, (xReply *) &rep, 0, xTrue)) {
            UnlockDisplay(dpy);
            SyncHandle();
            return NoSuchExtension;
        }
        else {
            UnlockDisplay(dpy);
            SyncHandle();
            return rep.status;
        }
    }
    default:
    {
	xDeviceCtl u;

	u.control = d->control;
	u.length = d->length - sizeof(int);
	length = ((unsigned)(u.length + 3) >> 2);
	req->length += length;
	length <<= 2;
	Data(dpy, (char *)&u, length);
    }
    }

    UnlockDisplay(dpy);
    SyncHandle();
    return (Success);
}
