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

void
XIBarrierReleasePointers(Display *dpy,
                         XIBarrierReleasePointerInfo *barriers,
                         int num_barriers)
{
    XExtDisplayInfo	        *info = XInput_find_display(dpy);
    xXIBarrierReleasePointerReq *req;
    int extra = 0;
    int i;
    xXIBarrierReleasePointerInfo *b;

    if (!num_barriers)
        return;

    extra = (num_barriers * sizeof(xXIBarrierReleasePointerInfo));

    LockDisplay (dpy);
    GetReqExtra (XIBarrierReleasePointer, extra, req);
    req->reqType = info->codes->major_opcode;
    req->ReqType = X_XIBarrierReleasePointer;
    req->num_barriers = num_barriers;

    b = (xXIBarrierReleasePointerInfo *) &req[1];
    for (i = 0; i < num_barriers; i++, b++) {
        b->deviceid = barriers[i].deviceid;
        b->eventid = barriers[i].eventid;
        b->barrier = barriers[i].barrier;
    }

    UnlockDisplay (dpy);
    SyncHandle ();
}

void
XIBarrierReleasePointer(Display *dpy,
                        int            deviceid,
                        PointerBarrier barrier,
                        BarrierEventID eventid)
{
    XIBarrierReleasePointerInfo info;
    info.deviceid = deviceid;
    info.barrier = barrier;
    info.eventid = eventid;

    XIBarrierReleasePointers(dpy, &info, 1);
}
