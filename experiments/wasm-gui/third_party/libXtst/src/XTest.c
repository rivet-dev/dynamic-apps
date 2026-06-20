/*
Copyright 1990, 1991 by UniSoft Group Limited
*/

/*

Copyright 1992, 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

#include <X11/Xlibint.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/xtestproto.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XIproto.h>

static XExtensionInfo _xtest_info_data;
static XExtensionInfo *xtest_info = &_xtest_info_data;
static const char *xtest_extension_name = XTestExtensionName;

#define XTestCheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, xtest_extension_name, val)

#define XTestICheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, xtest_extension_name, val); \
  if (!i->data) return val

/*****************************************************************************
 *                                                                           *
 *			   private utility routines                          *
 *                                                                           *
 *****************************************************************************/

static int close_display(Display *dpy, XExtCodes *codes);
static /* const */ XExtensionHooks xtest_extension_hooks = {
    NULL,				/* create_gc */
    NULL,				/* copy_gc */
    NULL,				/* flush_gc */
    NULL,				/* free_gc */
    NULL,				/* create_font */
    NULL,				/* free_font */
    close_display,			/* close_display */
    NULL,				/* wire_to_event */
    NULL,				/* event_to_wire */
    NULL,				/* error */
    NULL				/* error_string */
};

static XPointer
get_xinput_base(Display *dpy)
{
    int major_opcode, first_event, first_error;
    first_event = 0;

    XQueryExtension(dpy, INAME, &major_opcode, &first_event, &first_error);
    return (XPointer)(long)first_event;
}

static XEXT_GENERATE_FIND_DISPLAY (find_display, xtest_info,
				   xtest_extension_name,
				   &xtest_extension_hooks, XTestNumberEvents,
				   get_xinput_base(dpy))

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, xtest_info)

/*****************************************************************************
 *                                                                           *
 *		    public routines               			     *
 *                                                                           *
 *****************************************************************************/

Bool
XTestQueryExtension (Display *dpy,
		     int *event_base_return, int *error_base_return,
		     int *major_return, int *minor_return)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXTestGetVersionReply rep;

    if (XextHasExtension(info)) {
	xXTestGetVersionReq *req;

	LockDisplay(dpy);
	GetReq(XTestGetVersion, req);
	req->reqType = info->codes->major_opcode;
	req->xtReqType = X_XTestGetVersion;
	req->majorVersion = XTestMajorVersion;
	req->minorVersion = XTestMinorVersion;
	if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return False;
	}
	UnlockDisplay(dpy);
	SyncHandle();
	*event_base_return = info->codes->first_event;
	*error_base_return = info->codes->first_error;
	*major_return = rep.majorVersion;
	*minor_return = rep.minorVersion;
	return True;
    } else {
	return False;
    }
}

Bool
XTestCompareCursorWithWindow(Display *dpy, Window window, Cursor cursor)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xXTestCompareCursorReq *req;
    xXTestCompareCursorReply rep;

    XTestCheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(XTestCompareCursor, req);
    req->reqType = info->codes->major_opcode;
    req->xtReqType = X_XTestCompareCursor;
    req->window = window;
    req->cursor = cursor;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return rep.same;
}

Bool
XTestCompareCurrentCursorWithWindow(Display *dpy, Window window)
{
    return XTestCompareCursorWithWindow(dpy, window, XTestCurrentCursor);
}

int
XTestFakeKeyEvent(Display *dpy, unsigned int keycode,
		  Bool is_press, unsigned long delay)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xXTestFakeInputReq *req;

    XTestCheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(XTestFakeInput, req);
    req->reqType = info->codes->major_opcode;
    req->xtReqType = X_XTestFakeInput;
    req->type = is_press ? KeyPress : KeyRelease;
    req->detail = keycode;
    req->time = delay;
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

int
XTestFakeButtonEvent(Display *dpy, unsigned int button,
		     Bool is_press, unsigned long delay)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xXTestFakeInputReq *req;

    XTestCheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(XTestFakeInput, req);
    req->reqType = info->codes->major_opcode;
    req->xtReqType = X_XTestFakeInput;
    req->type = is_press ? ButtonPress : ButtonRelease;
    req->detail = button;
    req->time = delay;
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

int
XTestFakeMotionEvent(Display *dpy, int screen, int x, int y, unsigned long delay)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xXTestFakeInputReq *req;

    XTestCheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(XTestFakeInput, req);
    req->reqType = info->codes->major_opcode;
    req->xtReqType = X_XTestFakeInput;
    req->type = MotionNotify;
    req->detail = False;
    if (screen == -1)
	req->root = None;
    else
	req->root = RootWindow(dpy, screen);
    req->rootX = x;
    req->rootY = y;
    req->time = delay;
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

int
XTestFakeRelativeMotionEvent(Display *dpy, int dx, int dy, unsigned long delay)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xXTestFakeInputReq *req;

    XTestCheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(XTestFakeInput, req);
    req->reqType = info->codes->major_opcode;
    req->xtReqType = X_XTestFakeInput;
    req->type = MotionNotify;
    req->detail = True;
    req->root = None;
    req->rootX = dx;
    req->rootY = dy;
    req->time = delay;
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

static void
send_axes(
    Display *dpy,
    XExtDisplayInfo *info,
    xXTestFakeInputReq *req,
    XDevice *dev,
    int first_axis,
    int *axes,
    int n_axes)
{
    deviceValuator ev;

    req->deviceid |= MORE_EVENTS;
    req->length += ((n_axes + 5) / 6) * (SIZEOF(xEvent) >> 2);
    ev.type = XI_DeviceValuator + (long)info->data;
    ev.deviceid = dev->device_id;
    ev.first_valuator = first_axis;
    while (n_axes > 0) {
	int n = n_axes > 6 ? 6 : n_axes;
	ev.num_valuators = n;
	switch (n) {
	case 6:
	    ev.valuator5 = *(axes+5);
	    /* fallthrough */
	case 5:
	    ev.valuator4 = *(axes+4);
	    /* fallthrough */
	case 4:
	    ev.valuator3 = *(axes+3);
	    /* fallthrough */
	case 3:
	    ev.valuator2 = *(axes+2);
	    /* fallthrough */
	case 2:
	    ev.valuator1 = *(axes+1);
	    /* fallthrough */
	case 1:
	    ev.valuator0 = *axes;
	}
	Data(dpy, (char *)&ev, SIZEOF(xEvent));
	axes += n;
	n_axes -= n;
	ev.first_valuator += n;
    }
}

int
XTestFakeDeviceKeyEvent(Display *dpy, XDevice *dev,
			unsigned int keycode, Bool is_press,
			int *axes, int n_axes, unsigned long delay)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xXTestFakeInputReq *req;

    XTestICheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(XTestFakeInput, req);
    req->reqType = info->codes->major_opcode;
    req->xtReqType = X_XTestFakeInput;
    req->type = is_press ? XI_DeviceKeyPress : XI_DeviceKeyRelease;
    req->type += (int)(long)info->data;
    req->detail = keycode;
    req->time = delay;
    req->deviceid = dev->device_id;
    if (n_axes)
	send_axes(dpy, info, req, dev, 0, axes, n_axes);
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

int
XTestFakeDeviceButtonEvent(Display *dpy, XDevice *dev,
			   unsigned int button, Bool is_press,
			   int *axes, int n_axes, unsigned long delay)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xXTestFakeInputReq *req;

    XTestICheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(XTestFakeInput, req);
    req->reqType = info->codes->major_opcode;
    req->xtReqType = X_XTestFakeInput;
    req->type = is_press ? XI_DeviceButtonPress : XI_DeviceButtonRelease;
    req->type += (int)(long)info->data;
    req->detail = button;
    req->time = delay;
    req->deviceid = dev->device_id;
    if (n_axes)
	send_axes(dpy, info, req, dev, 0, axes, n_axes);
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

int
XTestFakeProximityEvent(Display *dpy, XDevice *dev, Bool in_prox,
			int *axes, int n_axes, unsigned long delay)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xXTestFakeInputReq *req;

    XTestICheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(XTestFakeInput, req);
    req->reqType = info->codes->major_opcode;
    req->xtReqType = X_XTestFakeInput;
    req->type = in_prox ? XI_ProximityIn : XI_ProximityOut;
    req->type += (int)(long)info->data;
    req->time = delay;
    req->deviceid = dev->device_id;
    if (n_axes)
	send_axes(dpy, info, req, dev, 0, axes, n_axes);
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

int
XTestFakeDeviceMotionEvent(Display *dpy, XDevice *dev,
			   Bool is_relative, int first_axis,
			   int *axes, int n_axes, unsigned long delay)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xXTestFakeInputReq *req;

    XTestICheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(XTestFakeInput, req);
    req->reqType = info->codes->major_opcode;
    req->xtReqType = X_XTestFakeInput;
    req->type = XI_DeviceMotionNotify + (int)(long)info->data;
    req->detail = is_relative;
    req->time = delay;
    req->deviceid = dev->device_id;
    send_axes(dpy, info, req, dev, first_axis, axes, n_axes);
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

int
XTestGrabControl(Display *dpy, Bool impervious)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xXTestGrabControlReq *req;

    XTestCheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(XTestGrabControl, req);
    req->reqType = info->codes->major_opcode;
    req->xtReqType = X_XTestGrabControl;
    req->impervious = impervious;
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

void
XTestSetGContextOfGC(GC gc, GContext gid)
{
    gc->gid = gid;
}

void
XTestSetVisualIDOfVisual(Visual *visual, VisualID visualid)
{
    visual->visualid = visualid;
}

static xReq _dummy_request = {
	0, 0, 0
};

Status
XTestDiscard(Display *dpy)
{
    Bool something;

    LockDisplay(dpy);
    if ((something = (dpy->bufptr != dpy->buffer))) {
	for (char *ptr = dpy->buffer;
	     ptr < dpy->bufptr;
	     ptr += (((xReq *)ptr)->length << 2))
	    dpy->request--;
	dpy->bufptr = dpy->buffer;
	dpy->last_req = (char *)&_dummy_request;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return something;
}
