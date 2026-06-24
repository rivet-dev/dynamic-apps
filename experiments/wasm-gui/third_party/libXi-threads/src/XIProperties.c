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

/***********************************************************************
 * XI2 property requests, list, change, delete and get properties.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <X11/Xlibint.h>
#include <X11/extensions/XI2.h>
#include <X11/extensions/XI2proto.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/extutil.h>
#include "XIint.h"
#include <limits.h>

Atom*
XIListProperties(Display* dpy, int deviceid, int *num_props_return)
{
    xXIListPropertiesReq        *req;
    xXIListPropertiesReply      rep;
    XExtDisplayInfo             *info = XInput_find_display(dpy);
    Atom                        *props = NULL;

    LockDisplay(dpy);
    *num_props_return = 0;
    if (_XiCheckExtInit(dpy, XInput_2_0, info) == -1)
        goto cleanup_unlocked;

    GetReq(XIListProperties, req);
    req->reqType = info->codes->major_opcode;
    req->ReqType = X_XIListProperties;
    req->deviceid = deviceid;

    if (!_XReply(dpy, (xReply*)&rep, 0, xFalse))
        goto cleanup;

    if (rep.num_properties) {
        props = (Atom*)Xmalloc(rep.num_properties * sizeof(Atom));
        if (!props)
        {
            _XEatDataWords(dpy, rep.length);
            goto cleanup;
        }

        _XRead32(dpy, (long*)props, rep.num_properties << 2);
    }

    *num_props_return = rep.num_properties;

cleanup:
    UnlockDisplay(dpy);
cleanup_unlocked:
    SyncHandle();
    return props;
}


void
XIDeleteProperty(Display* dpy, int deviceid, Atom property)
{
    xXIDeletePropertyReq   *req;

    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_2_0, info) == -1)
	return;

    GetReq(XIDeleteProperty, req);
    req->reqType    = info->codes->major_opcode;
    req->ReqType    = X_XIDeleteProperty;
    req->deviceid   = deviceid;
    req->property   = property;

    UnlockDisplay(dpy);
    SyncHandle();
}

void
XIChangeProperty(Display* dpy, int deviceid, Atom property, Atom type,
                 int format, int mode, unsigned char *data, int num_items)
{
    xXIChangePropertyReq *req;
    int len;

    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_2_0, info) == -1)
	return;

    GetReq(XIChangeProperty, req);
    req->reqType    = info->codes->major_opcode;
    req->ReqType    = X_XIChangeProperty;
    req->deviceid   = deviceid;
    req->property   = property;
    req->type       = type;
    req->mode       = mode;
    if (num_items < 0) {
	req->num_items = 0;
	req->format = 0; /* ask for garbage, get garbage */
    } else {
	req->num_items = num_items;
	req->format = format;
    }

    switch (req->format) {
    case 8:
	len = (num_items + 3)/4;
        SetReqLen(req, len, len);
        len = num_items;
	break;

    case 16:
	len = (num_items + 1)/2;
        SetReqLen(req, len, len);
        len = num_items * 2;
	break;

    case 32:
	len = num_items;
        SetReqLen(req, len, len);
        len = num_items * 4;
	break;

    default:
	/* BadValue will be generated */ ;
        len = 0;
    }

    /* we use data instead of Data32 and friends to avoid Xlib's braindead
     * 64 bit handling.*/

    Data(dpy, (const char*)data, len);
    UnlockDisplay(dpy);
    SyncHandle();
}

Status
XIGetProperty(Display* dpy, int deviceid, Atom property, long offset,
              long length, Bool delete_property, Atom type,
              Atom *type_return, int *format_return,
              unsigned long *num_items_return,unsigned long *bytes_after_return,
              unsigned char **data)
{
    xXIGetPropertyReq   *req;
    xXIGetPropertyReply rep;
    unsigned long       nbytes, rbytes;

    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_2_0, info) == -1)
	return 1;

    GetReq(XIGetProperty, req);
    req->reqType    = info->codes->major_opcode;
    req->ReqType    = X_XIGetProperty;
    req->deviceid   = deviceid;
    req->property   = property;
    req->type       = type;
    req->offset     = offset;
    req->len        = length;
    req->delete     = delete_property;

    if (!_XReply (dpy, (xReply *) &rep, 0, xFalse))
    {
	UnlockDisplay (dpy);
	SyncHandle ();
	return 1;
    }

    *data = NULL;

    if (rep.type != None) {
        if (rep.format != 8 && rep.format != 16 && rep.format != 32) {
	    /*
	     * This part of the code should never be reached.  If it is,
	     * the server sent back a property with an invalid format.
	     */
	    _XEatDataWords(dpy, rep.length);
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return(BadImplementation);
        }

	/*
	 * One extra byte is malloced than is needed to contain the property
	 * data, but this last byte is null terminated and convenient for
	 * returning string properties, so the client doesn't then have to
	 * recopy the string to make it null terminated.
	 */

	if (rep.num_items < (INT_MAX / (rep.format/8))) {
	    nbytes = rep.num_items * rep.format/8;
	    rbytes = nbytes + 1;
	    *data = Xmalloc(rbytes);
	}

	if (!(*data)) {
	    _XEatDataWords(dpy, rep.length);
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return(BadAlloc);
	}

        _XReadPad (dpy, (char *)*data, nbytes);
	(*data)[rbytes - 1] = '\0';
    }

    *type_return        = rep.type;
    *format_return      = rep.format;
    *num_items_return   = rep.num_items;
    *bytes_after_return = rep.bytes_after;
    UnlockDisplay (dpy);
    SyncHandle();
    return Success;
}
