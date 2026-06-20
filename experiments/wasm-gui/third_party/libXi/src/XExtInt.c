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
 * Input Extension library internal functions.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <X11/extensions/XI.h>
#include <X11/extensions/XI2.h>
#include <X11/extensions/XIproto.h>
#include <X11/extensions/XI2proto.h>
#include <X11/Xlibint.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/extutil.h>
#include <X11/extensions/geproto.h>
#include <X11/extensions/ge.h>
#include <X11/extensions/Xge.h>
#include "XIint.h"

#define ENQUEUE_EVENT	True
#define DONT_ENQUEUE	False
#define FP1616toDBL(x) ((x) * 1.0 / (1 << 16))

int copy_classes(XIDeviceInfo *to, xXIAnyInfo* from, int *nclasses);
int size_classes(xXIAnyInfo* from, int nclasses);

static XExtensionInfo *xinput_info;
static const char *xinput_extension_name = INAME;

static int XInputClose(
    Display *		/* dpy */,
    XExtCodes *		/* codes */
);

static char *XInputError(
    Display *		/* dpy */,
    int			/* code */,
    XExtCodes *		/* codes */,
    char *		/* buf */,
    int			/* n */
);

static Bool XInputWireToEvent(
    Display *		/* dpy */,
    XEvent *		/* re */,
    xEvent *		/* event */
);
static Bool XInputWireToCookie(
    Display*	        /* display */,
    XGenericEventCookie*	/* re */,
    xEvent*	        /* event */
);

static Bool XInputCopyCookie(
    Display*	        /* display */,
    XGenericEventCookie*	/* in */,
    XGenericEventCookie*	/* out */
);

static int
wireToDeviceEvent(xXIDeviceEvent *in, XGenericEventCookie* out);
static int
wireToDeviceChangedEvent(xXIDeviceChangedEvent *in, XGenericEventCookie *cookie);
static int
wireToHierarchyChangedEvent(xXIHierarchyEvent *in, XGenericEventCookie *cookie);
static int
wireToRawEvent(XExtDisplayInfo *info, xXIRawEvent *in, XGenericEventCookie *cookie);
static int
wireToEnterLeave(xXIEnterEvent *in, XGenericEventCookie *cookie);
static int
wireToPropertyEvent(xXIPropertyEvent *in, XGenericEventCookie *cookie);
static int
wireToTouchOwnershipEvent(xXITouchOwnershipEvent *in,
                          XGenericEventCookie *cookie);
static int
wireToBarrierEvent(xXIBarrierEvent *in,
                   XGenericEventCookie *cookie);
static int
wireToPinchEvent(xXIGesturePinchEvent *in,
                 XGenericEventCookie *cookie);
static int
wireToSwipeEvent(xXIGestureSwipeEvent *in,
                 XGenericEventCookie *cookie);

static /* const */ XEvent emptyevent;

typedef Status (*core_event_to_wire)(Display*, XEvent*, xEvent*);

static /* const */ XExtensionHooks xinput_extension_hooks = {
    NULL,	/* create_gc */
    NULL,	/* copy_gc */
    NULL,	/* flush_gc */
    NULL,	/* free_gc */
    NULL,	/* create_font */
    NULL,	/* free_font */
    XInputClose,	/* close_display */
    XInputWireToEvent,	/* wire_to_event */
    (core_event_to_wire)_XiEventToWire, /* event_to_wire */
    NULL,	/* error */
    XInputError,	/* error_string */
};

static const char *XInputErrorList[] = {
    "BadDevice, invalid or uninitialized input device",	/* BadDevice */
    "BadEvent, invalid event type",	/* BadEvent */
    "BadMode, invalid mode parameter",	/* BadMode  */
    "DeviceBusy, device is busy",	/* DeviceBusy */
    "BadClass, invalid event class",	/* BadClass */
};

/* Get the version supported by the server to know which number of
* events are support. Otherwise, a wrong number of events may smash
* the Xlib-internal event processing vector.
*
* Since the extension hasn't been initialized yet, we need to
* manually get the opcode, then the version.
*/
static int
_XiFindEventsSupported(Display *dpy)
{
    XExtCodes codes;
    XExtensionVersion *extversion = NULL;
    int nevents = 0;

    if (!XQueryExtension(dpy, INAME, &codes.major_opcode,
                         &codes.first_event, &codes.first_error))
        goto out;

    LockDisplay(dpy);
    extversion = _XiGetExtensionVersionRequest(dpy, INAME, codes.major_opcode);
    UnlockDisplay(dpy);
    SyncHandle();

    if (!extversion || !extversion->present)
        goto out;

    if (extversion->major_version >= 2)
        nevents = IEVENTS; /* number is fixed, XI2 adds GenericEvents only */
    else if (extversion->major_version <= 0)
    {
        printf("XInput_find_display: invalid extension version %d.%d\n",
                extversion->major_version, extversion->minor_version);
        goto out;
    }
    else
    {
        switch(extversion->minor_version)
        {
            case XI_Add_DeviceProperties_Minor:
                nevents = XI_DevicePropertyNotify + 1;
                break;
            case  XI_Add_DevicePresenceNotify_Minor:
                nevents = XI_DevicePresenceNotify + 1;
                break;
            default:
                nevents = XI_DeviceButtonstateNotify + 1;
                break;
        }
    }

out:
    if (extversion)
        XFree(extversion);
    return nevents;
}


_X_HIDDEN
XExtDisplayInfo *XInput_find_display (Display *dpy)
{
    XExtDisplayInfo *dpyinfo;
    if (!xinput_info) { if (!(xinput_info = XextCreateExtension())) return NULL; }
    if (!(dpyinfo = XextFindDisplay (xinput_info, dpy)))
    {
      int nevents = _XiFindEventsSupported(dpy);

      dpyinfo = XextAddDisplay (xinput_info, dpy,
                                xinput_extension_name,
                                &xinput_extension_hooks,
                                nevents, NULL);
      if (XextHasExtension(dpyinfo)) /* skip if XI doesn't exist on the server */
      {
          XESetWireToEventCookie(dpy, dpyinfo->codes->major_opcode, XInputWireToCookie);
          XESetCopyEventCookie(dpy, dpyinfo->codes->major_opcode, XInputCopyCookie);
      }
    }
    return dpyinfo;
}

static XEXT_GENERATE_ERROR_STRING(XInputError, xinput_extension_name,
                                  IERRORS, XInputErrorList)
/*******************************************************************
*
* Input extension versions.
*
*/
static XExtensionVersion versions[] = { {XI_Absent, 0, 0},
{XI_Present, XI_Initial_Release_Major, XI_Initial_Release_Minor},
{XI_Present, XI_Add_XDeviceBell_Major, XI_Add_XDeviceBell_Minor},
{XI_Present, XI_Add_XSetDeviceValuators_Major,
 XI_Add_XSetDeviceValuators_Minor},
{XI_Present, XI_Add_XChangeDeviceControl_Major,
 XI_Add_XChangeDeviceControl_Minor},
{XI_Present, XI_Add_DevicePresenceNotify_Major,
 XI_Add_DevicePresenceNotify_Minor},
{XI_Present, XI_Add_DeviceProperties_Major,
 XI_Add_DeviceProperties_Minor},
{XI_Present, 2, 0},
{XI_Present, 2, 1},
{XI_Present, 2, 2},
{XI_Present, 2, 3},
{XI_Present, 2, 4},
};

/***********************************************************************
 *
 * Return errors reported by this extension.
 *
 */

void
_xibaddevice(
    Display	*dpy,
    int		*error)
{
    XExtDisplayInfo *info = XInput_find_display(dpy);

    *error = info->codes->first_error + XI_BadDevice;
}

void
_xibadclass(
    Display	*dpy,
    int		*error)
{
    XExtDisplayInfo *info = XInput_find_display(dpy);

    *error = info->codes->first_error + XI_BadClass;
}

void
_xibadevent(
    Display	*dpy,
    int		*error)
{
    XExtDisplayInfo *info = XInput_find_display(dpy);

    *error = info->codes->first_error + XI_BadEvent;
}

void
_xibadmode(
    Display	*dpy,
    int		*error)
{
    XExtDisplayInfo *info = XInput_find_display(dpy);

    *error = info->codes->first_error + XI_BadMode;
}

void
_xidevicebusy(
    Display	*dpy,
    int		*error)
{
    XExtDisplayInfo *info = XInput_find_display(dpy);

    *error = info->codes->first_error + XI_DeviceBusy;
}

static int XInputCheckExtension(Display *dpy, XExtDisplayInfo *info)
{
    XextCheckExtension (dpy, info, xinput_extension_name, 0);
    return 1;
}

/*****************************************************************
 * Compare version numbers between info and the built-in version table.
 * Returns
 *   -1 if info's version is less than version_index's version,
 *   0 if equal (or DontCheck),
 *   1 if info's version is greater than version_index's version.
 * Returns -2 on initialization errors which shouldn't happen if you call it
 * correctly.
 */
_X_HIDDEN int
_XiCheckVersion(XExtDisplayInfo *info,
                int version_index)
{
    XExtensionVersion *ext;

    if (versions[version_index].major_version == Dont_Check)
        return 0;

    if (!info->data)
        return -2;

    ext = ((XInputData *) info->data)->vers;
    if (!ext)
        return -2;

    if (ext->major_version == versions[version_index].major_version &&
        ext->minor_version == versions[version_index].minor_version)
        return 0;

    if (ext->major_version < versions[version_index].major_version ||
        (ext->major_version == versions[version_index].major_version &&
         ext->minor_version < versions[version_index].minor_version))
        return -1;
    else
        return 1;
}

/***********************************************************************
 *
 * Check to see if the input extension is installed in the server.
 * Also check to see if the version is >= the requested version.
 *
 */

_X_HIDDEN int
_XiCheckExtInit(
    register Display	*dpy,
    register int	 version_index,
    XExtDisplayInfo	*info)
{
    if (!XInputCheckExtension(dpy, info)) {
	UnlockDisplay(dpy);
	return (-1);
    }

    if (info->data == NULL) {
	info->data = (XPointer) Xmalloc(sizeof(XInputData));
	if (!info->data) {
	    UnlockDisplay(dpy);
	    return (-1);
	}
	((XInputData *) info->data)->vers =
	    _XiGetExtensionVersionRequest(dpy, "XInputExtension", info->codes->major_opcode);
    }

    if (_XiCheckVersion(info, version_index) < 0) {
	UnlockDisplay(dpy);
	return -1;
    }

    return (0);
}

/***********************************************************************
 *
 * Close display routine.
 *
 */

static int
XInputClose(
    Display	*dpy,
    XExtCodes	*codes)
{
    XExtDisplayInfo *info = XInput_find_display(dpy);

    if (info->data != NULL) {
	XFree((char *)((XInputData *) info->data)->vers);
	XFree((char *)info->data);
    }

    if (!XextRemoveDisplay(xinput_info, dpy))
        return 0;

    if (xinput_info->ndisplays == 0) {
        XextDestroyExtension(xinput_info);
        xinput_info = NULL;
    }

    return 1;
}

static int
Ones(Mask mask)
{
    register Mask y;

    y = (mask >> 1) & 033333333333;
    y = mask - y - ((y >> 1) & 033333333333);
    return (((y + (y >> 3)) & 030707070707) % 077);
}

static int count_bits(unsigned char* ptr, int len)
{
    int bits = 0;
    unsigned int i;
    unsigned char x;

    for (i = 0; i < len; i++)
    {
        x = ptr[i];
        while(x > 0)
        {
            bits += (x & 0x1);
            x >>= 1;
        }
    }
    return bits;
}

int
_XiGetDevicePresenceNotifyEvent(Display * dpy)
{
    XExtDisplayInfo *info = XInput_find_display(dpy);

    return info->codes->first_event + XI_DevicePresenceNotify;
}

/***********************************************************************
 *
 * Handle Input extension events.
 * Reformat a wire event into an XEvent structure of the right type.
 *
 */

static Bool
XInputWireToEvent(
    Display	*dpy,
    XEvent	*re,
    xEvent	*event)
{
    unsigned int type, reltype;
    XExtDisplayInfo *info = XInput_find_display(dpy);
    XEvent *save = (XEvent *) info->data;

    type = event->u.u.type & 0x7f;
    reltype = (type - info->codes->first_event);

    if (type == GenericEvent || 
        (reltype != XI_DeviceValuator &&
	reltype != XI_DeviceKeystateNotify &&
	reltype != XI_DeviceButtonstateNotify)) {
	*save = emptyevent;
	save->type = type;
	((XAnyEvent *) save)->serial = _XSetLastRequestRead(dpy,
							    (xGenericReply *)
							    event);
	((XAnyEvent *) save)->send_event = ((event->u.u.type & 0x80) != 0);
	((XAnyEvent *) save)->display = dpy;
    }

    /* Process traditional events */
    if (type != GenericEvent)
    {
        switch (reltype) {
            case XI_DeviceMotionNotify:
                {
                    register XDeviceMotionEvent *ev = (XDeviceMotionEvent *) save;
                    deviceKeyButtonPointer *ev2 = (deviceKeyButtonPointer *) event;

                    ev->root = ev2->root;
                    ev->window = ev2->event;
                    ev->subwindow = ev2->child;
                    ev->time = ev2->time;
                    ev->x_root = ev2->root_x;
                    ev->y_root = ev2->root_y;
                    ev->x = ev2->event_x;
                    ev->y = ev2->event_y;
                    ev->state = ev2->state;
                    ev->same_screen = ev2->same_screen;
                    ev->is_hint = ev2->detail;
                    ev->deviceid = ev2->deviceid & DEVICE_BITS;
                    return (DONT_ENQUEUE);
                }
                break;
            case XI_DeviceKeyPress:
            case XI_DeviceKeyRelease:
                {
                    register XDeviceKeyEvent *ev = (XDeviceKeyEvent *) save;
                    deviceKeyButtonPointer *ev2 = (deviceKeyButtonPointer *) event;

                    ev->root = ev2->root;
                    ev->window = ev2->event;
                    ev->subwindow = ev2->child;
                    ev->time = ev2->time;
                    ev->x_root = ev2->root_x;
                    ev->y_root = ev2->root_y;
                    ev->x = ev2->event_x;
                    ev->y = ev2->event_y;
                    ev->state = ev2->state;
                    ev->same_screen = ev2->same_screen;
                    ev->keycode = ev2->detail;
                    ev->deviceid = ev2->deviceid & DEVICE_BITS;
                    if (ev2->deviceid & MORE_EVENTS)
                        return (DONT_ENQUEUE);
                    else {
                        *re = *save;
                        return (ENQUEUE_EVENT);
                    }
                }
                break;
            case XI_DeviceButtonPress:
            case XI_DeviceButtonRelease:
                {
                    register XDeviceButtonEvent *ev = (XDeviceButtonEvent *) save;
                    deviceKeyButtonPointer *ev2 = (deviceKeyButtonPointer *) event;

                    ev->root = ev2->root;
                    ev->window = ev2->event;
                    ev->subwindow = ev2->child;
                    ev->time = ev2->time;
                    ev->x_root = ev2->root_x;
                    ev->y_root = ev2->root_y;
                    ev->x = ev2->event_x;
                    ev->y = ev2->event_y;
                    ev->state = ev2->state;
                    ev->same_screen = ev2->same_screen;
                    ev->button = ev2->detail;
                    ev->deviceid = ev2->deviceid & DEVICE_BITS;
                    if (ev2->deviceid & MORE_EVENTS)
                        return (DONT_ENQUEUE);
                    else {
                        *re = *save;
                        return (ENQUEUE_EVENT);
                    }
                }
                break;
            case XI_ProximityIn:
            case XI_ProximityOut:
                {
                    register XProximityNotifyEvent *ev = (XProximityNotifyEvent *) save;
                    deviceKeyButtonPointer *ev2 = (deviceKeyButtonPointer *) event;

                    ev->root = ev2->root;
                    ev->window = ev2->event;
                    ev->subwindow = ev2->child;
                    ev->time = ev2->time;
                    ev->x_root = ev2->root_x;
                    ev->y_root = ev2->root_y;
                    ev->x = ev2->event_x;
                    ev->y = ev2->event_y;
                    ev->state = ev2->state;
                    ev->same_screen = ev2->same_screen;
                    ev->deviceid = ev2->deviceid & DEVICE_BITS;
                    if (ev2->deviceid & MORE_EVENTS)
                        return (DONT_ENQUEUE);
                    else {
                        *re = *save;
                        return (ENQUEUE_EVENT);
                    }
                }
                break;
            case XI_DeviceValuator:
                {
                    deviceValuator *xev = (deviceValuator *) event;
                    int save_type = save->type - info->codes->first_event;
                    int i;

                    if (save_type == XI_DeviceKeyPress || save_type == XI_DeviceKeyRelease) {
                        XDeviceKeyEvent *kev = (XDeviceKeyEvent *) save;

                        kev->device_state = xev->device_state;
                        kev->axes_count = xev->num_valuators;
                        kev->first_axis = xev->first_valuator;
                        i = xev->num_valuators;
                        if (i > 6)
                            i = 6;
                        switch (i) {
                            case 6:
                                kev->axis_data[5] = xev->valuator5;
                            case 5:
                                kev->axis_data[4] = xev->valuator4;
                            case 4:
                                kev->axis_data[3] = xev->valuator3;
                            case 3:
                                kev->axis_data[2] = xev->valuator2;
                            case 2:
                                kev->axis_data[1] = xev->valuator1;
                            case 1:
                                kev->axis_data[0] = xev->valuator0;
                        }
                    } else if (save_type == XI_DeviceButtonPress ||
                            save_type == XI_DeviceButtonRelease) {
                        XDeviceButtonEvent *bev = (XDeviceButtonEvent *) save;

                        bev->device_state = xev->device_state;
                        bev->axes_count = xev->num_valuators;
                        bev->first_axis = xev->first_valuator;
                        i = xev->num_valuators;
                        if (i > 6)
                            i = 6;
                        switch (i) {
                            case 6:
                                bev->axis_data[5] = xev->valuator5;
                            case 5:
                                bev->axis_data[4] = xev->valuator4;
                            case 4:
                                bev->axis_data[3] = xev->valuator3;
                            case 3:
                                bev->axis_data[2] = xev->valuator2;
                            case 2:
                                bev->axis_data[1] = xev->valuator1;
                            case 1:
                                bev->axis_data[0] = xev->valuator0;
                        }
                    } else if (save_type == XI_DeviceMotionNotify) {
                        XDeviceMotionEvent *mev = (XDeviceMotionEvent *) save;

                        mev->device_state = xev->device_state;
                        mev->axes_count = xev->num_valuators;
                        mev->first_axis = xev->first_valuator;
                        i = xev->num_valuators;
                        if (i > 6)
                            i = 6;
                        switch (i) {
                            case 6:
                                mev->axis_data[5] = xev->valuator5;
                            case 5:
                                mev->axis_data[4] = xev->valuator4;
                            case 4:
                                mev->axis_data[3] = xev->valuator3;
                            case 3:
                                mev->axis_data[2] = xev->valuator2;
                            case 2:
                                mev->axis_data[1] = xev->valuator1;
                            case 1:
                                mev->axis_data[0] = xev->valuator0;
                        }
                    } else if (save_type == XI_ProximityIn || save_type == XI_ProximityOut) {
                        XProximityNotifyEvent *pev = (XProximityNotifyEvent *) save;

                        pev->device_state = xev->device_state;
                        pev->axes_count = xev->num_valuators;
                        pev->first_axis = xev->first_valuator;
                        i = xev->num_valuators;
                        if (i > 6)
                            i = 6;
                        switch (i) {
                            case 6:
                                pev->axis_data[5] = xev->valuator5;
                            case 5:
                                pev->axis_data[4] = xev->valuator4;
                            case 4:
                                pev->axis_data[3] = xev->valuator3;
                            case 3:
                                pev->axis_data[2] = xev->valuator2;
                            case 2:
                                pev->axis_data[1] = xev->valuator1;
                            case 1:
                                pev->axis_data[0] = xev->valuator0;
                        }
                    } else if (save_type == XI_DeviceStateNotify) {
                        int j;
                        XDeviceStateNotifyEvent *sev = (XDeviceStateNotifyEvent *) save;
                        XInputClass *any = (XInputClass *) & sev->data[0];
                        XValuatorStatus *v;

                        for (i = 0; i < sev->num_classes; i++)
                            if (any->class != ValuatorClass)
                                any = (XInputClass *) ((char *)any + any->length);
                        v = (XValuatorStatus *) any;
                        i = v->num_valuators;
                        j = xev->num_valuators;
                        if (j > 3)
                            j = 3;
                        switch (j) {
                            case 3:
                                v->valuators[i + 2] = xev->valuator2;
                            case 2:
                                v->valuators[i + 1] = xev->valuator1;
                            case 1:
                                v->valuators[i + 0] = xev->valuator0;
                        }
                        v->num_valuators += j;

                    }
                    *re = *save;
                    return (ENQUEUE_EVENT);
                }
                break;
            case XI_DeviceFocusIn:
            case XI_DeviceFocusOut:
                {
                    register XDeviceFocusChangeEvent *ev = (XDeviceFocusChangeEvent *) re;
                    deviceFocus *fev = (deviceFocus *) event;

                    *ev = *((XDeviceFocusChangeEvent *) save);
                    ev->window = fev->window;
                    ev->time = fev->time;
                    ev->mode = fev->mode;
                    ev->detail = fev->detail;
                    ev->deviceid = fev->deviceid & DEVICE_BITS;
                    return (ENQUEUE_EVENT);
                }
                break;
            case XI_DeviceStateNotify:
                {
                    int j;
                    XDeviceStateNotifyEvent *stev = (XDeviceStateNotifyEvent *) save;
                    deviceStateNotify *sev = (deviceStateNotify *) event;
                    char *data;

                    stev->window = None;
                    stev->deviceid = sev->deviceid & DEVICE_BITS;
                    stev->time = sev->time;
                    stev->num_classes = Ones((Mask) sev->classes_reported & InputClassBits);
                    data = (char *)&stev->data[0];
                    if (sev->classes_reported & (1 << KeyClass)) {
                        register XKeyStatus *kstev = (XKeyStatus *) data;

                        kstev->class = KeyClass;
                        kstev->length = sizeof(XKeyStatus);
                        kstev->num_keys = sev->num_keys;
                        memcpy((char *)&kstev->keys[0], (char *)&sev->keys[0], 4);
                        data += sizeof(XKeyStatus);
                    }
                    if (sev->classes_reported & (1 << ButtonClass)) {
                        register XButtonStatus *bev = (XButtonStatus *) data;

                        bev->class = ButtonClass;
                        bev->length = sizeof(XButtonStatus);
                        bev->num_buttons = sev->num_buttons;
                        memcpy((char *)bev->buttons, (char *)sev->buttons, 4);
                        data += sizeof(XButtonStatus);
                    }
                    if (sev->classes_reported & (1 << ValuatorClass)) {
                        register XValuatorStatus *vev = (XValuatorStatus *) data;

                        vev->class = ValuatorClass;
                        vev->length = sizeof(XValuatorStatus);
                        vev->num_valuators = sev->num_valuators;
                        vev->mode = sev->classes_reported >> ModeBitsShift;
                        j = sev->num_valuators;
                        if (j > 3)
                            j = 3;
                        switch (j) {
                            case 3:
                                vev->valuators[2] = sev->valuator2;
                            case 2:
                                vev->valuators[1] = sev->valuator1;
                            case 1:
                                vev->valuators[0] = sev->valuator0;
                        }
                        data += sizeof(XValuatorStatus);
                    }
                    if (sev->deviceid & MORE_EVENTS)
                        return (DONT_ENQUEUE);
                    else {
                        *re = *save;
                        return (ENQUEUE_EVENT);
                    }
                }
                break;
            case XI_DeviceKeystateNotify:
                {
                    int i;
                    XInputClass *anyclass;
                    register XKeyStatus *kv;
                    deviceKeyStateNotify *ksev = (deviceKeyStateNotify *) event;
                    XDeviceStateNotifyEvent *kstev = (XDeviceStateNotifyEvent *) save;

                    anyclass = (XInputClass *) & kstev->data[0];
                    for (i = 0; i < kstev->num_classes; i++)
                        if (anyclass->class == KeyClass)
                            break;
                        else
                            anyclass = (XInputClass *) ((char *)anyclass +
                                    anyclass->length);

                    kv = (XKeyStatus *) anyclass;
                    kv->num_keys = 256;
                    memcpy((char *)&kv->keys[4], (char *)ksev->keys, 28);
                    if (ksev->deviceid & MORE_EVENTS)
                        return (DONT_ENQUEUE);
                    else {
                        *re = *save;
                        return (ENQUEUE_EVENT);
                    }
                }
                break;
            case XI_DeviceButtonstateNotify:
                {
                    int i;
                    XInputClass *anyclass;
                    register XButtonStatus *bv;
                    deviceButtonStateNotify *bsev = (deviceButtonStateNotify *) event;
                    XDeviceStateNotifyEvent *bstev = (XDeviceStateNotifyEvent *) save;

                    anyclass = (XInputClass *) & bstev->data[0];
                    for (i = 0; i < bstev->num_classes; i++)
                        if (anyclass->class == ButtonClass)
                            break;
                        else
                            anyclass = (XInputClass *) ((char *)anyclass +
                                    anyclass->length);

                    bv = (XButtonStatus *) anyclass;
                    bv->num_buttons = 256;
                    memcpy((char *)&bv->buttons[4], (char *)bsev->buttons, 28);
                    if (bsev->deviceid & MORE_EVENTS)
                        return (DONT_ENQUEUE);
                    else {
                        *re = *save;
                        return (ENQUEUE_EVENT);
                    }
                }
                break;
            case XI_DeviceMappingNotify:
                {
                    register XDeviceMappingEvent *ev = (XDeviceMappingEvent *) re;
                    deviceMappingNotify *ev2 = (deviceMappingNotify *) event;

                    *ev = *((XDeviceMappingEvent *) save);
                    ev->window = 0;
                    ev->first_keycode = ev2->firstKeyCode;
                    ev->request = ev2->request;
                    ev->count = ev2->count;
                    ev->time = ev2->time;
                    ev->deviceid = ev2->deviceid & DEVICE_BITS;
                    return (ENQUEUE_EVENT);
                }
                break;
            case XI_ChangeDeviceNotify:
                {
                    register XChangeDeviceNotifyEvent *ev = (XChangeDeviceNotifyEvent *) re;
                    changeDeviceNotify *ev2 = (changeDeviceNotify *) event;

                    *ev = *((XChangeDeviceNotifyEvent *) save);
                    ev->window = 0;
                    ev->request = ev2->request;
                    ev->time = ev2->time;
                    ev->deviceid = ev2->deviceid & DEVICE_BITS;
                    return (ENQUEUE_EVENT);
                }
                break;

            case XI_DevicePresenceNotify:
                {
                    XDevicePresenceNotifyEvent *ev = (XDevicePresenceNotifyEvent *) re;
                    devicePresenceNotify *ev2 = (devicePresenceNotify *) event;

                    *ev = *(XDevicePresenceNotifyEvent *) save;
                    ev->window = 0;
                    ev->time = ev2->time;
                    ev->devchange = ev2->devchange;
                    ev->deviceid = ev2->deviceid;
                    ev->control = ev2->control;
                    return (ENQUEUE_EVENT);
                }
                break;
            case XI_DevicePropertyNotify:
                {
                    XDevicePropertyNotifyEvent* ev = (XDevicePropertyNotifyEvent*)re;
                    devicePropertyNotify *ev2 = (devicePropertyNotify*)event;

                    *ev = *(XDevicePropertyNotifyEvent*)save;
                    ev->time = ev2->time;
                    ev->deviceid = ev2->deviceid;
                    ev->atom = ev2->atom;
                    ev->state = ev2->state;
                    return ENQUEUE_EVENT;
                }
                break;
            default:
                printf("XInputWireToEvent: UNKNOWN WIRE EVENT! type=%d\n", type);
                break;
        }
    }
    return (DONT_ENQUEUE);
}

static void xge_copy_to_cookie(xGenericEvent* ev,
                               XGenericEventCookie *cookie)
{
    cookie->type = ev->type;
    cookie->evtype = ev->evtype;
    cookie->extension = ev->extension;
}

static Bool
XInputWireToCookie(
    Display	*dpy,
    XGenericEventCookie *cookie,
    xEvent	*event)
{
    XExtDisplayInfo *info = XInput_find_display(dpy);
    XEvent *save = (XEvent *) info->data;
    xGenericEvent* ge = (xGenericEvent*)event;

    if (ge->extension != info->codes->major_opcode)
    {
        printf("XInputWireToCookie: wrong extension opcode %d\n",
                ge->extension);
        return DONT_ENQUEUE;
    }

    *save = emptyevent;
    save->type = event->u.u.type;
    ((XAnyEvent*)save)->serial = _XSetLastRequestRead(dpy, (xGenericReply *) event);
    ((XAnyEvent*)save)->send_event = ((event->u.u.type & 0x80) != 0);
    ((XAnyEvent*)save)->display = dpy;

    xge_copy_to_cookie((xGenericEvent*)event, (XGenericEventCookie*)save);
    switch(ge->evtype)
    {
        case XI_Motion:
        case XI_ButtonPress:
        case XI_ButtonRelease:
        case XI_KeyPress:
        case XI_KeyRelease:
        case XI_TouchBegin:
        case XI_TouchUpdate:
        case XI_TouchEnd:
            *cookie = *(XGenericEventCookie*)save;
            if (!wireToDeviceEvent((xXIDeviceEvent*)event, cookie))
            {
                printf("XInputWireToCookie: CONVERSION FAILURE!  evtype=%d\n",
                        ge->evtype);
                break;
            }
            return ENQUEUE_EVENT;
        case XI_DeviceChanged:
            *cookie = *(XGenericEventCookie*)save;
            if (!wireToDeviceChangedEvent((xXIDeviceChangedEvent*)event, cookie))
            {
                printf("XInputWireToCookie: CONVERSION FAILURE!  evtype=%d\n",
                        ge->evtype);
                break;
            }
            return ENQUEUE_EVENT;
        case XI_HierarchyChanged:
            *cookie = *(XGenericEventCookie*)save;
            if (!wireToHierarchyChangedEvent((xXIHierarchyEvent*)event, cookie))
            {
                printf("XInputWireToCookie: CONVERSION FAILURE!  evtype=%d\n",
                        ge->evtype);
                break;
            }
            return ENQUEUE_EVENT;
        case XI_TouchOwnership:
            *cookie = *(XGenericEventCookie*)save;
            if (!wireToTouchOwnershipEvent((xXITouchOwnershipEvent*)event,
                                           cookie))
            {
                printf("XInputWireToCookie: CONVERSION FAILURE!  evtype=%d\n",
                        ge->evtype);
                break;
            }
            return ENQUEUE_EVENT;

        case XI_RawKeyPress:
        case XI_RawKeyRelease:
        case XI_RawButtonPress:
        case XI_RawButtonRelease:
        case XI_RawMotion:
        case XI_RawTouchBegin:
        case XI_RawTouchUpdate:
        case XI_RawTouchEnd:
            *cookie = *(XGenericEventCookie*)save;
            if (!wireToRawEvent(info, (xXIRawEvent*)event, cookie))
            {
                printf("XInputWireToCookie: CONVERSION FAILURE!  evtype=%d\n",
                        ge->evtype);
                break;
            }
            return ENQUEUE_EVENT;
        case XI_Enter:
        case XI_Leave:
        case XI_FocusIn:
        case XI_FocusOut:
            *cookie = *(XGenericEventCookie*)save;
            if (!wireToEnterLeave((xXIEnterEvent*)event, cookie))
            {
                printf("XInputWireToCookie: CONVERSION FAILURE!  evtype=%d\n",
                        ge->evtype);
                break;
            }
            return ENQUEUE_EVENT;
        case XI_PropertyEvent:
            *cookie = *(XGenericEventCookie*)save;
            if (!wireToPropertyEvent((xXIPropertyEvent*)event, cookie))
            {
                printf("XInputWireToCookie: CONVERSION FAILURE!  evtype=%d\n",
                        ge->evtype);
                break;
            }
            return ENQUEUE_EVENT;
        case XI_BarrierHit:
        case XI_BarrierLeave:
            *cookie = *(XGenericEventCookie*)save;
            if (!wireToBarrierEvent((xXIBarrierEvent*)event, cookie))
            {
                printf("XInputWireToCookie: CONVERSION FAILURE!  evtype=%d\n",
                        ge->evtype);
                break;
            }
            return ENQUEUE_EVENT;
        case XI_GesturePinchBegin:
        case XI_GesturePinchUpdate:
        case XI_GesturePinchEnd:
            *cookie = *(XGenericEventCookie*)save;
            if (!wireToPinchEvent((xXIGesturePinchEvent*)event, cookie))
            {
                printf("XInputWireToCookie: CONVERSION FAILURE!  evtype=%d\n",
                        ge->evtype);
                break;
            }
            return ENQUEUE_EVENT;
        case XI_GestureSwipeBegin:
        case XI_GestureSwipeUpdate:
        case XI_GestureSwipeEnd:
            *cookie = *(XGenericEventCookie*)save;
            if (!wireToSwipeEvent((xXIGestureSwipeEvent*)event, cookie))
            {
                printf("XInputWireToCookie: CONVERSION FAILURE!  evtype=%d\n",
                        ge->evtype);
                break;
            }
            return ENQUEUE_EVENT;
        default:
            printf("XInputWireToCookie: Unknown generic event. type %d\n", ge->evtype);

    }
    return DONT_ENQUEUE;
}

/**
 * Calculate length in bytes needed for the device event with the given
 * button mask length, valuator mask length + valuator mask. All parameters
 * in bytes.
 */
static inline int
sizeDeviceEvent(int buttons_len, int valuators_len,
                unsigned char *valuators_mask)
{
    int len;

    len = sizeof(XIDeviceEvent);
    len += sizeof(XIButtonState) + buttons_len;
    len += sizeof(XIValuatorState) + valuators_len;
    len += count_bits(valuators_mask, valuators_len) * sizeof(double);
    len += sizeof(XIModifierState) + sizeof(XIGroupState);

    return len;
}

/* Return the size with added padding so next element would be
   double-aligned unless the architecture is known to allow unaligned
   data accesses.  Not doing this can cause a bus error on
   MIPS N32. */
static int
pad_to_double(int size)
{
#if !defined(__i386__) && !defined(__sh__)
    if (size % sizeof(double) != 0)
        size += sizeof(double) - size % sizeof(double);
#endif
    return size;
}

/**
 * Set structure and atoms to size in bytes of XIButtonClassInfo, its
 * button state mask and labels array.
 */
static void
sizeXIButtonClassType(int num_buttons, int* structure, int* state, int* atoms)
{
    int size;
    int labels;

    *structure = pad_to_double(sizeof(XIButtonClassInfo));
    size = ((((num_buttons + 7)/8) + 3)/4);

    /* Force mask alignment with longs to avoid unaligned
     * access when accessing the atoms. */
    *state = pad_to_double(size * 4);
    labels = num_buttons * sizeof(Atom);

    /* Force mask alignment with longs to avoid
     * unaligned access when accessing the atoms. */
    labels += ((((num_buttons + 7)/8) + 3)/4) * sizeof(Atom);
    *atoms = pad_to_double(labels);
}

/**
 * Set structure and keycodes to size in bytes of XIKeyClassInfo and
 * its keycodes array.
 */
static void
sizeXIKeyClassType(int num_keycodes, int* structure, int* keycodes)
{
    *structure = pad_to_double(sizeof(XIKeyClassInfo));
    *keycodes = pad_to_double(num_keycodes * sizeof(int));
}

/**
 * Return the size in bytes required to store the matching class type
 * num_elements is num_buttons for XIButtonClass or num_keycodes for
 * XIKeyClass.
 *
 * Also used from copy_classes in XIQueryDevice.c
 */
static int
sizeDeviceClassType(int type, int num_elements)
{
    int l = 0;
    int extra1 = 0;
    int extra2 = 0;
    switch(type)
    {
        case XIButtonClass:
            sizeXIButtonClassType(num_elements, &l, &extra1, &extra2);
            l += extra1 + extra2;
            break;
        case XIKeyClass:
            sizeXIKeyClassType(num_elements, &l, &extra1);
            l += extra1;
            break;
        case XIValuatorClass:
            l = pad_to_double(sizeof(XIValuatorClassInfo));
            break;
        case XIScrollClass:
            l = pad_to_double(sizeof(XIScrollClassInfo));
            break;
        case XITouchClass:
            l = pad_to_double(sizeof(XITouchClassInfo));
            break;
        case XIGestureClass:
            l = pad_to_double(sizeof(XIGestureClassInfo));
            break;
        default:
            printf("sizeDeviceClassType: unknown type %d\n", type);
            break;
    }
    return l;
}

static Bool
copyHierarchyEvent(XGenericEventCookie *cookie_in,
                   XGenericEventCookie *cookie_out)
{
    XIHierarchyEvent *in, *out;
    void *ptr;

    in = cookie_in->data;

    ptr = cookie_out->data = malloc(sizeof(XIHierarchyEvent) +
                                    in->num_info * sizeof(XIHierarchyInfo));
    if (!ptr)
        return False;

    out = next_block(&ptr, sizeof(XIHierarchyEvent));
    *out = *in;
    out->info = next_block(&ptr, in->num_info * sizeof(XIHierarchyInfo));
    memcpy(out->info, in->info, in->num_info * sizeof(XIHierarchyInfo));

    return True;
}

static Bool
copyDeviceChangedEvent(XGenericEventCookie *in_cookie,
                       XGenericEventCookie *out_cookie)
{
    int len, i;
    XIDeviceChangedEvent *in, *out;
    XIAnyClassInfo *any;
    void *ptr;

    in = in_cookie->data;

    len = sizeof(XIDeviceChangedEvent);
    len += in->num_classes * sizeof(XIAnyClassInfo*);

    for (i = 0; i < in->num_classes; i++)
    {
        any = in->classes[i];
        switch(any->type)
        {
            case XIButtonClass:
                len += sizeDeviceClassType(XIButtonClass,
                        ((XIButtonClassInfo*)any)->num_buttons);
                break;
            case XIKeyClass:
                len += sizeDeviceClassType(XIKeyClass,
                        ((XIKeyClassInfo*)any)->num_keycodes);
                break;
            case XIValuatorClass:
                len += sizeDeviceClassType(XIValuatorClass, 0);
                break;
            case XIScrollClass:
                len += sizeDeviceClassType(XIScrollClass, 0);
                break;
            default:
                printf("copyDeviceChangedEvent: unknown type %d\n",
                        any->type);
                break;
        }

    }

    ptr = out_cookie->data = malloc(len);
    if (!ptr)
        return False;
    out = next_block(&ptr, sizeof(XIDeviceChangedEvent));
    *out = *in;

    out->classes = next_block(&ptr,
                              out->num_classes * sizeof(XIAnyClassInfo*));

    for (i = 0; i < in->num_classes; i++)
    {
        any = in->classes[i];

        switch(any->type)
        {
            case XIButtonClass:
                {
                    int struct_size;
                    int state_size;
                    int labels_size;
                    XIButtonClassInfo *bin, *bout;
                    bin = (XIButtonClassInfo*)any;
                    sizeXIButtonClassType(bin->num_buttons, &struct_size,
                                          &state_size, &labels_size);
                    bout = next_block(&ptr, struct_size);

                    *bout = *bin;
                    bout->state.mask = next_block(&ptr, state_size);
                    memcpy(bout->state.mask, bin->state.mask,
                            bout->state.mask_len);

                    bout->labels = next_block(&ptr, labels_size);
                    memcpy(bout->labels, bin->labels, bout->num_buttons * sizeof(Atom));
                    out->classes[i] = (XIAnyClassInfo*)bout;
                    break;
                }
            case XIKeyClass:
                {
                    XIKeyClassInfo *kin, *kout;
                    int struct_size;
                    int keycodes_size;
                    kin = (XIKeyClassInfo*)any;
                    sizeXIKeyClassType(kin->num_keycodes, &struct_size,
                                       &keycodes_size);

                    kout = next_block(&ptr, struct_size);
                    *kout = *kin;
                    kout->keycodes = next_block(&ptr, keycodes_size);
                    memcpy(kout->keycodes, kin->keycodes, kout->num_keycodes * sizeof(int));
                    out->classes[i] = (XIAnyClassInfo*)kout;
                    break;
                }
            case XIValuatorClass:
                {
                    XIValuatorClassInfo *vin, *vout;
                    vin = (XIValuatorClassInfo*)any;
                    vout = next_block(&ptr,
                                      sizeDeviceClassType(XIValuatorClass, 0));
                    *vout = *vin;
                    out->classes[i] = (XIAnyClassInfo*)vout;
                    break;
                }
            case XIScrollClass:
                {
                    XIScrollClassInfo *sin, *sout;
                    sin = (XIScrollClassInfo*)any;
                    sout = next_block(&ptr,
                                      sizeDeviceClassType(XIScrollClass, 0));
                    *sout = *sin;
                    out->classes[i] = (XIAnyClassInfo*)sout;
                    break;
                }
        }
    }

    return True;
}

static Bool
copyDeviceEvent(XGenericEventCookie *cookie_in,
                XGenericEventCookie *cookie_out)
{
    int len;
    XIDeviceEvent *in, *out;
    int bits; /* valuator bits */
    void *ptr;

    in = cookie_in->data;
    bits = count_bits(in->valuators.mask, in->valuators.mask_len);

    len = sizeDeviceEvent(in->buttons.mask_len, in->valuators.mask_len,
                          in->valuators.mask);

    ptr = cookie_out->data = malloc(len);
    if (!ptr)
        return False;

    out = next_block(&ptr, sizeof(XIDeviceEvent));
    *out = *in;

    out->buttons.mask = next_block(&ptr, in->buttons.mask_len);
    memcpy(out->buttons.mask, in->buttons.mask,
           out->buttons.mask_len);
    out->valuators.mask = next_block(&ptr, in->valuators.mask_len);
    memcpy(out->valuators.mask, in->valuators.mask,
           out->valuators.mask_len);
    out->valuators.values = next_block(&ptr, bits * sizeof(double));
    memcpy(out->valuators.values, in->valuators.values,
           bits * sizeof(double));

    return True;
}

static Bool
copyEnterEvent(XGenericEventCookie *cookie_in,
               XGenericEventCookie *cookie_out)
{
    int len;
    XIEnterEvent *in, *out;
    void *ptr;

    in = cookie_in->data;

    len = sizeof(XIEnterEvent) + in->buttons.mask_len;

    ptr = cookie_out->data = malloc(len);
    if (!ptr)
        return False;

    out = next_block(&ptr, sizeof(XIEnterEvent));
    *out = *in;

    out->buttons.mask = next_block(&ptr, in->buttons.mask_len);
    memcpy(out->buttons.mask, in->buttons.mask, out->buttons.mask_len);

    return True;
}

static Bool
copyPropertyEvent(XGenericEventCookie *cookie_in,
                  XGenericEventCookie *cookie_out)
{
    XIPropertyEvent *in, *out;

    in = cookie_in->data;

    out = cookie_out->data = malloc(sizeof(XIPropertyEvent));
    if (!out)
        return False;

    *out = *in;
    return True;
}

static Bool
copyTouchOwnershipEvent(XGenericEventCookie *cookie_in,
                        XGenericEventCookie *cookie_out)
{
    XITouchOwnershipEvent *in, *out;

    in = cookie_in->data;

    out = cookie_out->data = malloc(sizeof(XITouchOwnershipEvent));
    if (!out)
        return False;

    *out = *in;
    return True;
}

static Bool
copyRawEvent(XGenericEventCookie *cookie_in,
             XGenericEventCookie *cookie_out)
{
    XIRawEvent *in, *out;
    void *ptr;
    int len;
    int bits;

    in = cookie_in->data;

    bits = count_bits(in->valuators.mask, in->valuators.mask_len);
    len = pad_to_double(sizeof(XIRawEvent))
        + pad_to_double(in->valuators.mask_len);
    len += bits * sizeof(double) * 2;

    ptr = cookie_out->data = malloc(len);
    if (!ptr)
        return False;

    out = next_block(&ptr, pad_to_double(sizeof(XIRawEvent)));
    *out = *in;
    out->valuators.mask
        = next_block(&ptr, pad_to_double(out->valuators.mask_len));
    memcpy(out->valuators.mask, in->valuators.mask, out->valuators.mask_len);

    out->valuators.values = next_block(&ptr, bits * sizeof(double));
    memcpy(out->valuators.values, in->valuators.values, bits * sizeof(double));

    out->raw_values = next_block(&ptr, bits * sizeof(double));
    memcpy(out->raw_values, in->raw_values, bits * sizeof(double));

    return True;
}

static Bool
copyBarrierEvent(XGenericEventCookie *in_cookie,
                 XGenericEventCookie *out_cookie)
{
    XIBarrierEvent *in, *out;

    in = in_cookie->data;

    out = out_cookie->data = calloc(1, sizeof(XIBarrierEvent));
    if (!out)
        return False;
    *out = *in;

    return True;
}


static Bool
copyGesturePinchEvent(XGenericEventCookie *cookie_in,
                      XGenericEventCookie *cookie_out)
{
    XIGesturePinchEvent *in, *out;

    in = cookie_in->data;

    out = cookie_out->data = malloc(sizeof(XIGesturePinchEvent));
    if (!out)
        return False;

    *out = *in;

    return True;
}

static Bool
copyGestureSwipeEvent(XGenericEventCookie *cookie_in,
                      XGenericEventCookie *cookie_out)
{
    XIGestureSwipeEvent *in, *out;

    in = cookie_in->data;

    out = cookie_out->data = malloc(sizeof(XIGestureSwipeEvent));
    if (!out)
        return False;

    *out = *in;

    return True;
}

static Bool
XInputCopyCookie(Display *dpy, XGenericEventCookie *in, XGenericEventCookie *out)
{
    int ret = True;

    XExtDisplayInfo *info = XInput_find_display(dpy);

    if (in->extension != info->codes->major_opcode)
    {
        printf("XInputCopyCookie: wrong extension opcode %d\n",
                in->extension);
        return False;
    }

    *out = *in;
    out->data = NULL;
    out->cookie = 0;

    switch(in->evtype) {
        case XI_Motion:
        case XI_ButtonPress:
        case XI_ButtonRelease:
        case XI_KeyPress:
        case XI_KeyRelease:
        case XI_TouchBegin:
        case XI_TouchUpdate:
        case XI_TouchEnd:
            ret = copyDeviceEvent(in, out);
            break;
        case XI_DeviceChanged:
            ret = copyDeviceChangedEvent(in, out);
            break;
        case XI_HierarchyChanged:
            ret = copyHierarchyEvent(in, out);
            break;
        case XI_Enter:
        case XI_Leave:
        case XI_FocusIn:
        case XI_FocusOut:
            ret = copyEnterEvent(in, out);
            break;
        case XI_PropertyEvent:
            ret = copyPropertyEvent(in, out);
            break;
        case XI_TouchOwnership:
            ret = copyTouchOwnershipEvent(in, out);
            break;
        case XI_RawKeyPress:
        case XI_RawKeyRelease:
        case XI_RawButtonPress:
        case XI_RawButtonRelease:
        case XI_RawMotion:
        case XI_RawTouchBegin:
        case XI_RawTouchUpdate:
        case XI_RawTouchEnd:
            ret = copyRawEvent(in, out);
            break;
        case XI_BarrierHit:
        case XI_BarrierLeave:
            ret = copyBarrierEvent(in, out);
            break;
        case XI_GesturePinchBegin:
        case XI_GesturePinchUpdate:
        case XI_GesturePinchEnd:
            ret = copyGesturePinchEvent(in, out);
            break;
        case XI_GestureSwipeBegin:
        case XI_GestureSwipeUpdate:
        case XI_GestureSwipeEnd:
            ret = copyGestureSwipeEvent(in, out);
            break;
        default:
            printf("XInputCopyCookie: unknown evtype %d\n", in->evtype);
            ret = False;
    }

    if (!ret)
        printf("XInputCopyCookie: Failed to copy evtype %d", in->evtype);
    return ret;
}

static int
wireToDeviceEvent(xXIDeviceEvent *in, XGenericEventCookie* cookie)
{
    int len, i;
    unsigned char *ptr;
    void *ptr_lib;
    FP3232 *values;
    XIDeviceEvent *out;

    ptr = (unsigned char*)&in[1] + in->buttons_len * 4;

    len = sizeDeviceEvent(in->buttons_len * 4, in->valuators_len * 4, ptr);

    cookie->data = ptr_lib = malloc(len);
    if (!ptr_lib)
        return 0;

    out = next_block(&ptr_lib, sizeof(XIDeviceEvent));
    out->display = cookie->display;
    out->type = in->type;
    out->serial = cookie->serial;
    out->extension = in->extension;
    out->evtype = in->evtype;
    out->send_event = ((in->type & 0x80) != 0);
    out->time = in->time;
    out->deviceid = in->deviceid;
    out->sourceid = in->sourceid;
    out->detail = in->detail;
    out->root = in->root;
    out->event = in->event;
    out->child = in->child;
    out->root_x = FP1616toDBL(in->root_x);
    out->root_y = FP1616toDBL(in->root_y);
    out->event_x = FP1616toDBL(in->event_x);
    out->event_y = FP1616toDBL(in->event_y);
    out->flags = in->flags;
    out->mods.base = in->mods.base_mods;
    out->mods.locked = in->mods.locked_mods;
    out->mods.latched = in->mods.latched_mods;
    out->mods.effective = in->mods.effective_mods;
    out->group.base = in->group.base_group;
    out->group.locked = in->group.locked_group;
    out->group.latched = in->group.latched_group;
    out->group.effective = in->group.effective_group;
    out->buttons.mask_len = in->buttons_len * 4;
    out->valuators.mask_len = in->valuators_len * 4;

    out->buttons.mask = next_block(&ptr_lib, out->buttons.mask_len);

    /* buttons */
    ptr = (unsigned char*)&in[1];
    memcpy(out->buttons.mask, ptr, out->buttons.mask_len);
    ptr += in->buttons_len * 4;

    /* valuators */
    out->valuators.mask = next_block(&ptr_lib, out->valuators.mask_len);
    memcpy(out->valuators.mask, ptr, out->valuators.mask_len);
    ptr += in->valuators_len * 4;

    len = count_bits(out->valuators.mask, out->valuators.mask_len);
    out->valuators.values = next_block(&ptr_lib, len * sizeof(double));

    values = (FP3232*)ptr;
    for (i = 0; i < len; i++, values++)
    {
        out->valuators.values[i] = values->integral;
        out->valuators.values[i] += ((double)values->frac / (1 << 16) / (1 << 16));
    }


    return 1;
}

_X_HIDDEN int
size_classes(xXIAnyInfo* from, int nclasses)
{
    int len, i;
    xXIAnyInfo *any_wire;
    char *ptr_wire;

    /* len for to->classes */
    len = pad_to_double(nclasses * sizeof(XIAnyClassInfo*));
    ptr_wire = (char*)from;
    for (i = 0; i < nclasses; i++)
    {
        int l = 0;
        any_wire = (xXIAnyInfo*)ptr_wire;
        switch(any_wire->type)
        {
            case XIButtonClass:
                l = sizeDeviceClassType(XIButtonClass,
                        ((xXIButtonInfo*)any_wire)->num_buttons);
                break;
            case XIKeyClass:
                l = sizeDeviceClassType(XIKeyClass,
                        ((xXIKeyInfo*)any_wire)->num_keycodes);
                break;
            case XIValuatorClass:
                l = sizeDeviceClassType(XIValuatorClass, 0);
                break;
            case XIScrollClass:
                l = sizeDeviceClassType(XIScrollClass, 0);
                break;
            case XITouchClass:
                l = sizeDeviceClassType(XITouchClass, 0);
                break;
            case XIGestureClass:
                l = sizeDeviceClassType(XIGestureClass, 0);
                break;
        }

        len += l;
        ptr_wire += any_wire->length * 4;
    }

    return len;
}

#define FP3232_TO_DOUBLE(x) ((double) (x).integral + (double) (x).frac / (1ULL << 32))

/* Copy classes from any into to->classes and return the number of bytes
 * copied. Memory layout of to->classes is
 * [clsptr][clsptr][clsptr][classinfo][classinfo]...
 *    |________|___________^
 *             |______________________^
 */
_X_HIDDEN int
copy_classes(XIDeviceInfo* to, xXIAnyInfo* from, int *nclasses)
{
    XIAnyClassInfo *any_lib;
    xXIAnyInfo *any_wire;
    void *ptr_lib;
    char *ptr_wire;
    int i, len;
    int cls_idx = 0;

    if (!to->classes)
        return -1;

    ptr_wire = (char*)from;
    ptr_lib = to->classes;
    to->classes = next_block(&ptr_lib,
                             pad_to_double((*nclasses) * sizeof(XIAnyClassInfo*)));
    memset(to->classes, 0, (*nclasses) * sizeof(XIAnyClassInfo*));
    len = 0; /* count wire length */

    for (i = 0; i < *nclasses; i++)
    {
        any_lib = (XIAnyClassInfo*)ptr_lib;
        any_wire = (xXIAnyInfo*)ptr_wire;

        switch(any_wire->type)
        {
            case XIButtonClass:
                {
                    XIButtonClassInfo *cls_lib;
                    xXIButtonInfo *cls_wire;
                    uint32_t *atoms;
                    int j;
                    int struct_size;
                    int state_size;
                    int labels_size;
                    int wire_mask_size;

                    cls_wire = (xXIButtonInfo*)any_wire;
                    sizeXIButtonClassType(cls_wire->num_buttons,
                                          &struct_size, &state_size,
                                          &labels_size);
                    cls_lib = next_block(&ptr_lib, struct_size);
                    wire_mask_size = ((cls_wire->num_buttons + 7)/8 + 3)/4 * 4;

                    cls_lib->type = cls_wire->type;
                    cls_lib->sourceid = cls_wire->sourceid;
                    cls_lib->num_buttons = cls_wire->num_buttons;
                    cls_lib->state.mask_len = state_size;
                    cls_lib->state.mask = next_block(&ptr_lib, state_size);
                    memcpy(cls_lib->state.mask, &cls_wire[1],
                           wire_mask_size);
                    if (state_size != wire_mask_size)
                        memset(&cls_lib->state.mask[wire_mask_size], 0,
                               state_size - wire_mask_size);

                    cls_lib->labels = next_block(&ptr_lib, labels_size);

                    atoms =(uint32_t*)((char*)&cls_wire[1] + wire_mask_size);
                    for (j = 0; j < cls_lib->num_buttons; j++)
                        cls_lib->labels[j] = *atoms++;

                    to->classes[cls_idx++] = any_lib;
                    break;
                }
            case XIKeyClass:
                {
                    XIKeyClassInfo *cls_lib;
                    xXIKeyInfo *cls_wire;
                    int struct_size;
                    int keycodes_size;

                    cls_wire = (xXIKeyInfo*)any_wire;
                    sizeXIKeyClassType(cls_wire->num_keycodes,
                                       &struct_size, &keycodes_size);
                    cls_lib = next_block(&ptr_lib, struct_size);

                    cls_lib->type = cls_wire->type;
                    cls_lib->sourceid = cls_wire->sourceid;
                    cls_lib->num_keycodes = cls_wire->num_keycodes;
                    cls_lib->keycodes = next_block(&ptr_lib, keycodes_size);
                    memcpy(cls_lib->keycodes, &cls_wire[1],
                            cls_lib->num_keycodes);

                    to->classes[cls_idx++] = any_lib;
                    break;
                }
            case XIValuatorClass:
                {
                    XIValuatorClassInfo *cls_lib;
                    xXIValuatorInfo *cls_wire;

                    cls_lib =
                      next_block(&ptr_lib,
                                 sizeDeviceClassType(XIValuatorClass, 0));
                    cls_wire = (xXIValuatorInfo*)any_wire;

                    cls_lib->type = cls_wire->type;
                    cls_lib->sourceid = cls_wire->sourceid;
                    cls_lib->number = cls_wire->number;
                    cls_lib->label  = cls_wire->label;
                    cls_lib->resolution = cls_wire->resolution;
                    cls_lib->min        = FP3232_TO_DOUBLE(cls_wire->min);
                    cls_lib->max        = FP3232_TO_DOUBLE(cls_wire->max);
                    cls_lib->value      = FP3232_TO_DOUBLE(cls_wire->value);
                    cls_lib->mode       = cls_wire->mode;

                    to->classes[cls_idx++] = any_lib;
                }
                break;
            case XIScrollClass:
                {
                    XIScrollClassInfo *cls_lib;
                    xXIScrollInfo *cls_wire;

                    cls_lib =
                      next_block(&ptr_lib,
                                 sizeDeviceClassType(XIScrollClass, 0));
                    cls_wire = (xXIScrollInfo*)any_wire;

                    cls_lib->type = cls_wire->type;
                    cls_lib->sourceid = cls_wire->sourceid;
                    cls_lib->number     = cls_wire->number;
                    cls_lib->scroll_type= cls_wire->scroll_type;
                    cls_lib->flags      = cls_wire->flags;
                    cls_lib->increment  = FP3232_TO_DOUBLE(cls_wire->increment);

                    to->classes[cls_idx++] = any_lib;
                }
                break;
            case XITouchClass:
                {
                    XITouchClassInfo *cls_lib;
                    xXITouchInfo *cls_wire;

                    cls_wire = (xXITouchInfo*)any_wire;
                    cls_lib = next_block(&ptr_lib, sizeof(XITouchClassInfo));

                    cls_lib->type = cls_wire->type;
                    cls_lib->sourceid = cls_wire->sourceid;
                    cls_lib->mode = cls_wire->mode;
                    cls_lib->num_touches = cls_wire->num_touches;

                    to->classes[cls_idx++] = any_lib;
                }
                break;
            case XIGestureClass:
                {
                    XIGestureClassInfo *cls_lib;
                    xXIGestureInfo *cls_wire;

                    cls_wire = (xXIGestureInfo*)any_wire;
                    cls_lib = next_block(&ptr_lib, sizeof(XIGestureClassInfo));

                    cls_lib->type = cls_wire->type;
                    cls_lib->sourceid = cls_wire->sourceid;
                    cls_lib->num_touches = cls_wire->num_touches;

                    to->classes[cls_idx++] = any_lib;
                }
                break;
        }
        len += any_wire->length * 4;
        ptr_wire += any_wire->length * 4;
    }

    /* we may have skipped unknown classes, reset nclasses */
    *nclasses = cls_idx;
    return len;
}


static int
wireToDeviceChangedEvent(xXIDeviceChangedEvent *in, XGenericEventCookie *cookie)
{
    XIDeviceChangedEvent *out;
    XIDeviceInfo info;
    int len;
    int nclasses = in->num_classes;

    len = size_classes((xXIAnyInfo*)&in[1], in->num_classes);

    cookie->data = out = malloc(sizeof(XIDeviceChangedEvent) + len);
    if (!out)
        return 0;

    out->type = in->type;
    out->serial = cookie->serial;
    out->display = cookie->display;
    out->extension = in->extension;
    out->evtype = in->evtype;
    out->send_event = ((in->type & 0x80) != 0);
    out->time = in->time;
    out->deviceid = in->deviceid;
    out->sourceid = in->sourceid;
    out->reason = in->reason;

    out->classes = (XIAnyClassInfo**)&out[1];

    info.classes = out->classes;

    copy_classes(&info, (xXIAnyInfo*)&in[1], &nclasses);
    out->num_classes = nclasses;

    return 1;
}

static int
wireToHierarchyChangedEvent(xXIHierarchyEvent *in, XGenericEventCookie *cookie)
{
    int i;
    XIHierarchyInfo *info_out;
    xXIHierarchyInfo *info_in;
    XIHierarchyEvent *out;

    cookie->data = out = malloc(sizeof(XIHierarchyEvent) + in->num_info * sizeof(XIHierarchyInfo));
    if (!out)
        return 0;

    out->info           = (XIHierarchyInfo*)&out[1];
    out->display        = cookie->display;
    out->type           = in->type;
    out->serial         = cookie->serial;
    out->extension      = in->extension;
    out->evtype         = in->evtype;
    out->send_event = ((in->type & 0x80) != 0);
    out->time           = in->time;
    out->flags          = in->flags;
    out->num_info       = in->num_info;

    info_out            = out->info;
    info_in             = (xXIHierarchyInfo*)&in[1];

    for (i = 0; i < out->num_info; i++, info_out++, info_in++)
    {
        info_out->deviceid      = info_in->deviceid;
        info_out->attachment    = info_in->attachment;
        info_out->use           = info_in->use;
        info_out->enabled       = info_in->enabled;
        info_out->flags         = info_in->flags;
    }

    return 1;
}

static int
wireToRawEvent(XExtDisplayInfo *info, xXIRawEvent *in, XGenericEventCookie *cookie)
{
    int len, i, bits;
    FP3232 *values;
    XIRawEvent *out;
    void *ptr;

    len = pad_to_double(sizeof(XIRawEvent))
        + pad_to_double(in->valuators_len * 4);
    bits = count_bits((unsigned char*)&in[1], in->valuators_len * 4);
    len += bits * sizeof(double) * 2; /* raw + normal */

    cookie->data = ptr = calloc(1, len);
    if (!ptr)
        return 0;

    out = next_block(&ptr, pad_to_double(sizeof(XIRawEvent)));
    out->type           = in->type;
    out->serial         = cookie->serial;
    out->display        = cookie->display;
    out->extension      = in->extension;
    out->evtype         = in->evtype;
    out->send_event = ((in->type & 0x80) != 0);
    out->time           = in->time;
    out->detail         = in->detail;
    out->deviceid       = in->deviceid;
    out->flags          = in->flags;

    /* https://bugs.freedesktop.org/show_bug.cgi?id=34240 */
    if (_XiCheckVersion(info, XInput_2_2) >= 0)
        out->sourceid       = in->sourceid;
    else
        out->sourceid       = 0;

    out->valuators.mask_len = in->valuators_len * 4;
    out->valuators.mask
        = next_block(&ptr, pad_to_double(out->valuators.mask_len));
    memcpy(out->valuators.mask, &in[1], out->valuators.mask_len);

    out->valuators.values = next_block(&ptr, bits * sizeof(double));
    out->raw_values = next_block(&ptr, bits * sizeof(double));

    values = (FP3232*)(((char*)&in[1]) + in->valuators_len * 4);
    for (i = 0; i < bits; i++)
    {
        out->valuators.values[i] = values->integral;
        out->valuators.values[i] += ((double)values->frac / (1 << 16) / (1 << 16));
        out->raw_values[i] = (values + bits)->integral;
        out->raw_values[i] += ((double)(values + bits)->frac / (1 << 16) / (1 << 16));
        values++;
    }

    return 1;
}

/* Memory layout of XIEnterEvents:
   [event][modifiers][group][button]
 */
static int
wireToEnterLeave(xXIEnterEvent *in, XGenericEventCookie *cookie)
{
    int len;
    XIEnterEvent *out;

    len = sizeof(XIEnterEvent) + in->buttons_len * 4;

    cookie->data = out = malloc(len);
    if (!out)
        return 0;

    out->buttons.mask = (unsigned char*)&out[1];

    out->type           = in->type;
    out->serial         = cookie->serial;
    out->display        = cookie->display;
    out->extension      = in->extension;
    out->evtype         = in->evtype;
    out->send_event = ((in->type & 0x80) != 0);
    out->time           = in->time;
    out->detail         = in->detail;
    out->deviceid       = in->deviceid;
    out->root           = in->root;
    out->event          = in->event;
    out->child          = in->child;
    out->sourceid       = in->sourceid;
    out->root_x         = FP1616toDBL(in->root_x);
    out->root_y         = FP1616toDBL(in->root_y);
    out->event_x        = FP1616toDBL(in->event_x);
    out->event_y        = FP1616toDBL(in->event_y);
    out->mode           = in->mode;
    out->focus          = in->focus;
    out->same_screen    = in->same_screen;

    out->mods.base = in->mods.base_mods;
    out->mods.locked = in->mods.locked_mods;
    out->mods.latched = in->mods.latched_mods;
    out->mods.effective = in->mods.effective_mods;
    out->group.base = in->group.base_group;
    out->group.locked = in->group.locked_group;
    out->group.latched = in->group.latched_group;
    out->group.effective = in->group.effective_group;

    out->buttons.mask_len = in->buttons_len * 4;
    memcpy(out->buttons.mask, &in[1], out->buttons.mask_len);

    return 1;
}

static int
wireToPropertyEvent(xXIPropertyEvent *in, XGenericEventCookie *cookie)
{
    XIPropertyEvent *out = malloc(sizeof(XIPropertyEvent));

    cookie->data = out;
    if (!out)
        return 0;

    out->type           = in->type;
    out->serial         = cookie->serial;
    out->extension      = in->extension;
    out->evtype         = in->evtype;
    out->send_event = ((in->type & 0x80) != 0);
    out->time           = in->time;
    out->property       = in->property;
    out->what           = in->what;
    out->deviceid       = in->deviceid;

    return 1;
}

static int
wireToTouchOwnershipEvent(xXITouchOwnershipEvent *in,
                          XGenericEventCookie *cookie)
{
    XITouchOwnershipEvent *out = malloc(sizeof(XITouchOwnershipEvent));

    cookie->data = out;
    if (!out)
        return 0;

    out->type           = in->type;
    out->serial         = cookie->serial;
    out->display        = cookie->display;
    out->extension      = in->extension;
    out->evtype         = in->evtype;
    out->send_event     = ((in->type & 0x80) != 0);
    out->time           = in->time;
    out->deviceid       = in->deviceid;
    out->sourceid       = in->sourceid;
    out->touchid        = in->touchid;
    out->root           = in->root;
    out->event          = in->event;
    out->child          = in->child;
    out->flags          = in->flags;

    return 1;
}

static int
wireToBarrierEvent(xXIBarrierEvent *in, XGenericEventCookie *cookie)
{
    XIBarrierEvent *out = malloc(sizeof(XIBarrierEvent));

    cookie->data = out;
    if (!out)
        return 0;

    out->display    = cookie->display;
    out->type       = in->type;
    out->serial     = cookie->serial;
    out->extension  = in->extension;
    out->evtype     = in->evtype;
    out->send_event = ((in->type & 0x80) != 0);
    out->time       = in->time;
    out->deviceid   = in->deviceid;
    out->sourceid   = in->sourceid;
    out->event      = in->event;
    out->root       = in->root;
    out->root_x     = FP1616toDBL(in->root_x);
    out->root_y     = FP1616toDBL(in->root_y);
    out->dx         = FP3232_TO_DOUBLE (in->dx);
    out->dy         = FP3232_TO_DOUBLE (in->dy);
    out->dtime      = in->dtime;
    out->flags      = in->flags;
    out->barrier    = in->barrier;
    out->eventid    = in->eventid;

    return 1;
}

static int
wireToPinchEvent(xXIGesturePinchEvent *in,
                 XGenericEventCookie *cookie)
{
    XIGesturePinchEvent *out;

    cookie->data = out = malloc(sizeof(XIGesturePinchEvent));
    if (!out)
        return 0;

    out->display    = cookie->display;
    out->type       = in->type;
    out->serial     = cookie->serial;
    out->extension  = in->extension;
    out->evtype     = in->evtype;
    out->send_event = ((in->type & 0x80) != 0);
    out->time       = in->time;
    out->deviceid   = in->deviceid;
    out->sourceid   = in->sourceid;
    out->detail     = in->detail;
    out->root       = in->root;
    out->event      = in->event;
    out->child      = in->child;
    out->root_x     = FP1616toDBL(in->root_x);
    out->root_y     = FP1616toDBL(in->root_y);
    out->event_x    = FP1616toDBL(in->event_x);
    out->event_y    = FP1616toDBL(in->event_y);
    out->delta_x    = FP1616toDBL(in->delta_x);
    out->delta_y    = FP1616toDBL(in->delta_y);
    out->delta_unaccel_x = FP1616toDBL(in->delta_unaccel_x);
    out->delta_unaccel_y = FP1616toDBL(in->delta_unaccel_y);
    out->scale      = FP1616toDBL(in->scale);
    out->delta_angle = FP1616toDBL(in->delta_angle);
    out->flags      = in->flags;

    out->mods.base = in->mods.base_mods;
    out->mods.locked = in->mods.locked_mods;
    out->mods.latched = in->mods.latched_mods;
    out->mods.effective = in->mods.effective_mods;
    out->group.base = in->group.base_group;
    out->group.locked = in->group.locked_group;
    out->group.latched = in->group.latched_group;
    out->group.effective = in->group.effective_group;

    return 1;
}

static int
wireToSwipeEvent(xXIGestureSwipeEvent *in,
                 XGenericEventCookie *cookie)
{
    XIGestureSwipeEvent *out;

    cookie->data = out = malloc(sizeof(XIGestureSwipeEvent));
    if (!out)
        return 0;

    out->display    = cookie->display;
    out->type       = in->type;
    out->serial     = cookie->serial;
    out->extension  = in->extension;
    out->evtype     = in->evtype;
    out->send_event = ((in->type & 0x80) != 0);
    out->time       = in->time;
    out->deviceid   = in->deviceid;
    out->sourceid   = in->sourceid;
    out->detail     = in->detail;
    out->root       = in->root;
    out->event      = in->event;
    out->child      = in->child;
    out->root_x     = FP1616toDBL(in->root_x);
    out->root_y     = FP1616toDBL(in->root_y);
    out->event_x    = FP1616toDBL(in->event_x);
    out->event_y    = FP1616toDBL(in->event_y);
    out->delta_x    = FP1616toDBL(in->delta_x);
    out->delta_y    = FP1616toDBL(in->delta_y);
    out->delta_unaccel_x = FP1616toDBL(in->delta_unaccel_x);
    out->delta_unaccel_y = FP1616toDBL(in->delta_unaccel_y);
    out->flags      = in->flags;

    out->mods.base = in->mods.base_mods;
    out->mods.locked = in->mods.locked_mods;
    out->mods.latched = in->mods.latched_mods;
    out->mods.effective = in->mods.effective_mods;
    out->group.base = in->group.base_group;
    out->group.locked = in->group.locked_group;
    out->group.latched = in->group.latched_group;
    out->group.effective = in->group.effective_group;

    return 1;
}
