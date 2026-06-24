/*
 * Copyright © 2009 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <X11/Xlibint.h>
#include <X11/extensions/XI2proto.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/extutil.h>
#include "XIint.h"

/* for GetRequest() to work */
#define X_XI2_2AllowEvents X_XIAllowEvents

static Status
_XIAllowEvents(Display *dpy, int deviceid, int event_mode, Time time,
                    unsigned int touchid, Window grab_window)
{
    Bool have_XI22 = False;
    xXIAllowEventsReq *req;
    xXI2_2AllowEventsReq *req_XI22;

    XExtDisplayInfo *extinfo = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_2_0, extinfo) == -1)
	return (NoSuchExtension);

    if (_XiCheckVersion(extinfo, XInput_2_2) >= 0)
        have_XI22 = True;

    if (have_XI22)
    {
        GetReq(XI2_2AllowEvents, req_XI22);
        req = (xXIAllowEventsReq*)req_XI22;
    } else
        GetReq(XIAllowEvents, req);

    req->reqType = extinfo->codes->major_opcode;
    req->ReqType = X_XIAllowEvents;
    req->deviceid = deviceid;
    req->mode = event_mode;
    req->time = time;

    if (have_XI22) {
        req_XI22->touchid = touchid;
        req_XI22->grab_window = grab_window;
    }

    UnlockDisplay(dpy);
    SyncHandle();
    return Success;
}

Status
XIAllowEvents(Display *dpy, int deviceid, int event_mode, Time time)
{
    return _XIAllowEvents(dpy, deviceid, event_mode, time, 0, None);
}

Status
XIAllowTouchEvents(Display *dpy, int deviceid, unsigned int touchid,
                   Window grab_window, int event_mode)
{
    XExtDisplayInfo *extinfo = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_2_2, extinfo) == -1)
	return (NoSuchExtension);
    UnlockDisplay(dpy);

    return _XIAllowEvents(dpy, deviceid, event_mode, CurrentTime, touchid, grab_window);
}
