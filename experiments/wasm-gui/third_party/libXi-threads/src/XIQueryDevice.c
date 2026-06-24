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

#include <limits.h>
#include <stdint.h>
#include <X11/Xlibint.h>
#include <X11/extensions/XI2proto.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/extutil.h>
#include "XIint.h"

extern int copy_classes(XIDeviceInfo* to, xXIAnyInfo* from, int *nclasses);
extern int size_classes(xXIAnyInfo* from, int nclasses);

XIDeviceInfo*
XIQueryDevice(Display *dpy, int deviceid, int *ndevices_return)
{
    XIDeviceInfo        *info = NULL;
    xXIQueryDeviceReq   *req;
    xXIQueryDeviceReply reply;
    char                *ptr;
    char                *end;
    int                 i;
    char                *buf = NULL;

    XExtDisplayInfo *extinfo = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_2_0, extinfo) == -1)
        goto error_unlocked;

    GetReq(XIQueryDevice, req);
    req->reqType  = extinfo->codes->major_opcode;
    req->ReqType  = X_XIQueryDevice;
    req->deviceid = deviceid;

    if (!_XReply(dpy, (xReply*) &reply, 0, xFalse))
        goto error;

    if (reply.length < INT_MAX / 4)
    {
	*ndevices_return = reply.num_devices;
	info = Xmalloc((reply.num_devices + 1) * sizeof(XIDeviceInfo));
	buf = Xmalloc(reply.length * 4);
    }
    else
    {
	*ndevices_return = 0;
	info = NULL;
	buf = NULL;
    }

    if (!info || !buf)
        goto error;

    _XRead(dpy, buf, reply.length * 4);
    ptr = buf;
    end = buf + reply.length * 4;

    /* info is a null-terminated array */
    info[reply.num_devices].name = NULL;

    for (i = 0; i < reply.num_devices; i++)
    {
        int             nclasses;
        size_t          sz;
        XIDeviceInfo    *lib = &info[i];
        xXIDeviceInfo   *wire = (xXIDeviceInfo*)ptr;

        if (ptr + sizeof(xXIDeviceInfo) > end)
            goto error_loop;

        lib->deviceid    = wire->deviceid;
        lib->use         = wire->use;
        lib->attachment  = wire->attachment;
        lib->enabled     = wire->enabled;
        nclasses         = wire->num_classes;

        ptr += sizeof(xXIDeviceInfo);

        if (ptr + wire->name_len > end)
            goto error_loop;

        lib->name = Xcalloc(wire->name_len + 1, 1);
        if (lib->name == NULL)
            goto error_loop;
        strncpy(lib->name, ptr, wire->name_len);
        lib->name[wire->name_len] = '\0';
        ptr += ((wire->name_len + 3)/4) * 4;

        sz = size_classes((xXIAnyInfo*)ptr, nclasses);
        lib->classes = Xmalloc(sz);
        if (lib->classes == NULL)
        {
            Xfree(lib->name);
            goto error_loop;
        }
        ptr += copy_classes(lib, (xXIAnyInfo*)ptr, &nclasses);
        /* We skip over unused classes */
        lib->num_classes = nclasses;
    }

    Xfree(buf);
    UnlockDisplay(dpy);
    SyncHandle();
    return info;

error_loop:
    while (--i >= 0)
    {
        Xfree(info[i].name);
        Xfree(info[i].classes);
    }
error:
    Xfree(info);
    Xfree(buf);
    UnlockDisplay(dpy);
error_unlocked:
    SyncHandle();
    *ndevices_return = -1;
    return NULL;
}

void
XIFreeDeviceInfo(XIDeviceInfo* info)
{
    XIDeviceInfo *ptr = info;
    while(ptr && ptr->name)
    {
        Xfree(ptr->classes);
        Xfree(ptr->name);
        ptr++;
    }
    Xfree(info);
}
