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

Status
XIQueryVersion(Display *dpy, int *major_inout, int *minor_inout)
{
    int rc = Success;

    XExtDisplayInfo *info = XInput_find_display(dpy);

    rc = _xiQueryVersion(dpy, major_inout, minor_inout, info);

    SyncHandle();
    return rc;
}

_X_HIDDEN Status
_xiQueryVersion(Display * dpy, int *major, int *minor, XExtDisplayInfo *info)
{
    xXIQueryVersionReq *req;
    xXIQueryVersionReply rep;

    LockDisplay(dpy);

    /* This could mean either a malloc problem, or supported
        version < XInput_2_0 */
    if (_XiCheckExtInit(dpy, XInput_2_0, info) == -1)
    {
        XExtensionVersion *ext;
        XExtDisplayInfo *extinfo = XInput_find_display(dpy);

        if (!extinfo || !extinfo->data) {
            *major = 0;
            *minor = 0;
            return BadRequest;
        }

        ext = ((XInputData*)extinfo->data)->vers;

        *major = ext->major_version;
        *minor = ext->minor_version;
	return BadRequest;
    }

    GetReq(XIQueryVersion, req);
    req->reqType = info->codes->major_opcode;
    req->ReqType = X_XIQueryVersion;
    req->major_version = *major;
    req->minor_version = *minor;

    if (!_XReply(dpy, (xReply*)&rep, 0, xTrue)) {
        UnlockDisplay(dpy);
	return BadImplementation;
    }

    *major = rep.major_version;
    *minor = rep.minor_version;

    UnlockDisplay(dpy);
    return Success;
}
