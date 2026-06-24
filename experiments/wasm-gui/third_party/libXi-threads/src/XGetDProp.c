/************************************************************

Copyright 2008 Peter Hutterer

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
AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the author shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the author.

*/

/***********************************************************************
 * XGetDeviceProperties - get an input device's properties.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlibint.h>
#include <X11/extensions/XI.h>
#include <X11/extensions/XIproto.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/extutil.h>
#include "XIint.h"
#include <limits.h>

int
XGetDeviceProperty(Display* dpy, XDevice* dev,
			 Atom property, long offset, long length, Bool delete,
                         Atom req_type, Atom *actual_type, int *actual_format,
                         unsigned long *nitems, unsigned long *bytes_after,
                         unsigned char **prop)
{
    xGetDevicePropertyReq   *req;
    xGetDevicePropertyReply rep;
    unsigned long           nbytes, rbytes;
    int                     ret = Success;

    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_Initial_Release, info) == -1)
	return 1;

    GetReq(GetDeviceProperty, req);
    req->reqType    = info->codes->major_opcode;
    req->ReqType    = X_GetDeviceProperty;
    req->deviceid   = dev->device_id;
    req->property   = property;
    req->type       = req_type;
    req->longOffset = offset;
    req->longLength = length;
    req->delete     = delete;

    if (!_XReply (dpy, (xReply *) &rep, 0, xFalse))
    {
	UnlockDisplay (dpy);
	SyncHandle ();
	return 1;
    }

    *prop = (unsigned char *) NULL;

    if (rep.propertyType != None) {
	/*
	 * One extra byte is malloced than is needed to contain the property
	 * data, but this last byte is null terminated and convenient for
	 * returning string properties, so the client doesn't then have to
	 * recopy the string to make it null terminated.
	 *
	 * Maximum item limits are set to both prevent integer overflow when
	 * calculating the amount of memory to malloc, and to limit how much
	 * memory will be used if a server provides an insanely high count.
	 */
	switch (rep.format) {
	case 8:
	    if (rep.nItems < INT_MAX) {
		nbytes = rep.nItems;
		rbytes = rep.nItems + 1;
		if ((*prop = Xmalloc (rbytes)))
		    _XReadPad (dpy, (char *) *prop, nbytes);
		else
		    ret = BadAlloc;
	    }
	    break;

	case 16:
	    if (rep.nItems < (INT_MAX / sizeof (short))) {
		nbytes = rep.nItems << 1;
		rbytes = rep.nItems * sizeof (short) + 1;
		if ((*prop = Xmalloc (rbytes)))
		    _XRead16Pad (dpy, (short *) *prop, nbytes);
		else
		    ret = BadAlloc;
	    }
	    break;

	case 32:
	    if (rep.nItems < (INT_MAX / sizeof (long))) {
		nbytes = rep.nItems << 2;
		rbytes = rep.nItems * sizeof (long) + 1;
		if ((*prop = Xmalloc (rbytes)))
		    _XRead32 (dpy, (long *) *prop, nbytes);
		else
		    ret = BadAlloc;
	    }
	    break;

	default:
	    /*
	     * This part of the code should never be reached.  If it is,
	     * the server sent back a property with an invalid format.
	     */
	    ret = BadImplementation;
	}
	if (! *prop) {
	    _XEatDataWords(dpy, rep.length);
	    if (ret == Success)
		ret = BadAlloc;
	    goto out;
	}
	(*prop)[rbytes - 1] = '\0';
    }

    *actual_type = rep.propertyType;
    *actual_format = rep.format;
    *nitems = rep.nItems;
    *bytes_after = rep.bytesAfter;
  out:
    UnlockDisplay (dpy);
    SyncHandle ();

    return ret;
}

