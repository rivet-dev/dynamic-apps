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
#include <limits.h>
#include "XIint.h"


Status
XIGrabDevice(Display* dpy, int deviceid, Window grab_window, Time time,
             Cursor cursor, int grab_mode, int paired_device_mode,
             Bool owner_events, XIEventMask *mask)
{
    xXIGrabDeviceReq *req;
    xXIGrabDeviceReply reply;
    char* buff;
    int len;

    XExtDisplayInfo *extinfo = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_2_0, extinfo) == -1)
	return (NoSuchExtension);

    if (mask->mask_len > INT_MAX - 3 ||
        (mask->mask_len + 3)/4 >= 0xffff)
    {
        reply.status = BadValue;
        goto out;
    }

    /* mask->mask_len is in bytes, but we need 4-byte units on the wire,
     * and they need to be padded with 0 */
    len = (mask->mask_len + 3)/4;
    buff = calloc(4, len);
    if (!buff)
    {
        reply.status =  BadAlloc;
        goto out;
    }

    GetReq(XIGrabDevice, req);
    req->reqType  = extinfo->codes->major_opcode;
    req->ReqType  = X_XIGrabDevice;
    req->deviceid = deviceid;
    req->grab_window = grab_window;
    req->time = time;
    req->grab_mode = grab_mode;
    req->paired_device_mode = paired_device_mode;
    req->owner_events = owner_events;
    req->mask_len = len;
    req->cursor = cursor;

    memcpy(buff, mask->mask, mask->mask_len);

    SetReqLen(req, len, len);
    Data(dpy, buff, len * 4);
    free(buff);

    if (_XReply(dpy, (xReply *)&reply, 0, xTrue) == 0)
	reply.status = GrabSuccess;

out:
    UnlockDisplay(dpy);
    SyncHandle();

    return reply.status;
}

Status
XIUngrabDevice(Display* dpy, int deviceid, Time time)
{
    xXIUngrabDeviceReq *req;

    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_2_0, info) == -1)
	return (NoSuchExtension);

    GetReq(XIUngrabDevice, req);
    req->reqType = info->codes->major_opcode;
    req->ReqType = X_XIUngrabDevice;

    req->deviceid = deviceid;
    req->time = time;

    UnlockDisplay(dpy);
    SyncHandle();
    return (Success);
}


