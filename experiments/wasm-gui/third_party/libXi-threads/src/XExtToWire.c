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

/****************************************************************
 *
 *	XExtToWire.c - reformat an XEvent into a wire event.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/extensions/XIproto.h>
#include <X11/Xlibint.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/extutil.h>
#include <X11/extensions/Xge.h>
#include <X11/extensions/geproto.h>
#include "XIint.h"

_X_HIDDEN Status
_XiEventToWire(
    register Display	 *dpy,		/* pointer to display structure */
    register XEvent	 *re,		/* pointer to client event */
    register xEvent	**event,	/* wire protocol event */
    register int	 *count)
{
    XExtDisplayInfo *info = XInput_find_display(dpy);
    int i;

    if ((re->type & 0x7f) != GenericEvent) {
        switch ((re->type & 0x7f) - info->codes->first_event) {
            case XI_DeviceKeyPress:
            case XI_DeviceKeyRelease:
                {
                    register XDeviceKeyEvent *ev = (XDeviceKeyEvent *) re;
                    register deviceKeyButtonPointer *kev;
                    register deviceValuator *vev;

                    *count = 2;
                    kev = (deviceKeyButtonPointer *) Xmalloc(*count * sizeof(xEvent));
                    if (!kev)
                        return (_XUnknownNativeEvent(dpy, re, *event));
                    *event = (xEvent *) kev;

                    kev->type = ev->type;
                    kev->root = ev->root;
                    kev->event = ev->window;
                    kev->child = ev->subwindow;
                    kev->time = ev->time;
                    kev->event_x = ev->x;
                    kev->event_y = ev->y;
                    kev->root_x = ev->x_root;
                    kev->root_y = ev->y_root;
                    kev->state = ev->state;
                    kev->same_screen = ev->same_screen;
                    kev->detail = ev->keycode;
                    kev->deviceid = ev->deviceid | MORE_EVENTS;

                    vev = (deviceValuator *)++ kev;
                    vev->type = info->codes->first_event + XI_DeviceValuator;
                    vev->deviceid = ev->deviceid;
                    vev->device_state = ev->device_state;
                    vev->first_valuator = ev->first_axis;
                    vev->num_valuators = ev->axes_count;
                    i = vev->num_valuators;
                    if (i > 6)
                        i = 6;
                    switch (i) {
                        case 6:
                            vev->valuator5 = ev->axis_data[5];
                        case 5:
                            vev->valuator4 = ev->axis_data[4];
                        case 4:
                            vev->valuator3 = ev->axis_data[3];
                        case 3:
                            vev->valuator2 = ev->axis_data[2];
                        case 2:
                            vev->valuator1 = ev->axis_data[1];
                        case 1:
                            vev->valuator0 = ev->axis_data[0];
                    }
                    break;
                }
            case XI_ProximityIn:
            case XI_ProximityOut:
                {
                    register XProximityNotifyEvent *ev = (XProximityNotifyEvent *) re;
                    register deviceKeyButtonPointer *pev;
                    register deviceValuator *vev;

                    *count = 2;
                    pev = (deviceKeyButtonPointer *) Xmalloc(*count * sizeof(xEvent));
                    if (!pev)
                        return (_XUnknownNativeEvent(dpy, re, *event));
                    *event = (xEvent *) pev;

                    pev->type = ev->type;
                    pev->root = ev->root;
                    pev->event = ev->window;
                    pev->child = ev->subwindow;
                    pev->time = ev->time;
                    pev->event_x = ev->x;
                    pev->event_y = ev->y;
                    pev->root_x = ev->x_root;
                    pev->root_y = ev->y_root;
                    pev->state = ev->state;
                    pev->same_screen = ev->same_screen;
                    pev->deviceid = ev->deviceid | MORE_EVENTS;

                    vev = (deviceValuator *)++ pev;
                    vev->type = info->codes->first_event + XI_DeviceValuator;
                    vev->deviceid = ev->deviceid;
                    vev->device_state = ev->device_state;
                    vev->first_valuator = ev->first_axis;
                    vev->num_valuators = ev->axes_count;

                    i = vev->num_valuators;
                    if (i > 6)
                        i = 6;
                    switch (i) {
                        case 6:
                            vev->valuator5 = ev->axis_data[5];
                        case 5:
                            vev->valuator4 = ev->axis_data[4];
                        case 4:
                            vev->valuator3 = ev->axis_data[3];
                        case 3:
                            vev->valuator2 = ev->axis_data[2];
                        case 2:
                            vev->valuator1 = ev->axis_data[1];
                        case 1:
                            vev->valuator0 = ev->axis_data[0];
                    }
                    break;
                }
            case XI_DeviceButtonPress:
            case XI_DeviceButtonRelease:
                {
                    register XDeviceButtonEvent *ev = (XDeviceButtonEvent *) re;
                    register deviceKeyButtonPointer *bev;
                    register deviceValuator *vev;

                    *count = 2;
                    bev = (deviceKeyButtonPointer *) Xmalloc(*count * sizeof(xEvent));
                    if (!bev)
                        return (_XUnknownNativeEvent(dpy, re, *event));
                    *event = (xEvent *) bev;

                    bev->type = ev->type;
                    bev->root = ev->root;
                    bev->event = ev->window;
                    bev->child = ev->subwindow;
                    bev->time = ev->time;
                    bev->event_x = ev->x;
                    bev->event_y = ev->y;
                    bev->root_x = ev->x_root;
                    bev->root_y = ev->y_root;
                    bev->state = ev->state;
                    bev->same_screen = ev->same_screen;
                    bev->detail = ev->button;
                    bev->deviceid = ev->deviceid | MORE_EVENTS;

                    vev = (deviceValuator *)++ bev;
                    vev->type = info->codes->first_event + XI_DeviceValuator;
                    vev->deviceid = ev->deviceid;
                    vev->device_state = ev->device_state;
                    vev->first_valuator = ev->first_axis;
                    vev->num_valuators = ev->axes_count;

                    i = vev->num_valuators;
                    if (i > 6)
                        i = 6;
                    switch (i) {
                        case 6:
                            vev->valuator5 = ev->axis_data[5];
                        case 5:
                            vev->valuator4 = ev->axis_data[4];
                        case 4:
                            vev->valuator3 = ev->axis_data[3];
                        case 3:
                            vev->valuator2 = ev->axis_data[2];
                        case 2:
                            vev->valuator1 = ev->axis_data[1];
                        case 1:
                            vev->valuator0 = ev->axis_data[0];
                    }
                    break;
                }
            case XI_DeviceMotionNotify:
                {
                    register XDeviceMotionEvent *ev = (XDeviceMotionEvent *) re;
                    register deviceKeyButtonPointer *mev;
                    register deviceValuator *vev;

                    *count = 2;
                    mev = (deviceKeyButtonPointer *) Xmalloc(*count * sizeof(xEvent));
                    if (!mev)
                        return (_XUnknownNativeEvent(dpy, re, *event));
                    *event = (xEvent *) mev;

                    mev->type = ev->type;
                    mev->root = ev->root;
                    mev->event = ev->window;
                    mev->child = ev->subwindow;
                    mev->time = ev->time;
                    mev->event_x = ev->x;
                    mev->event_y = ev->y;
                    mev->root_x = ev->x_root;
                    mev->root_y = ev->y_root;
                    mev->state = ev->state;
                    mev->same_screen = ev->same_screen;
                    mev->detail = ev->is_hint;
                    mev->deviceid = ev->deviceid | MORE_EVENTS;

                    vev = (deviceValuator *)++ mev;
                    vev->type = info->codes->first_event + XI_DeviceValuator;
                    vev->deviceid = ev->deviceid;
                    vev->device_state = ev->device_state;
                    vev->first_valuator = ev->first_axis;
                    vev->num_valuators = ev->axes_count;

                    i = vev->num_valuators;
                    if (i > 6)
                        i = 6;
                    switch (i) {
                        case 6:
                            vev->valuator5 = ev->axis_data[5];
                        case 5:
                            vev->valuator4 = ev->axis_data[4];
                        case 4:
                            vev->valuator3 = ev->axis_data[3];
                        case 3:
                            vev->valuator2 = ev->axis_data[2];
                        case 2:
                            vev->valuator1 = ev->axis_data[1];
                        case 1:
                            vev->valuator0 = ev->axis_data[0];
                    }
                    break;
                }
            case XI_DeviceFocusIn:
            case XI_DeviceFocusOut:
                {
                    register XDeviceFocusChangeEvent *ev = (XDeviceFocusChangeEvent *) re;
                    register deviceFocus *fev;

                    *count = 1;
                    fev = (deviceFocus *) Xmalloc(*count * sizeof(xEvent));
                    if (!fev)
                        return (_XUnknownNativeEvent(dpy, re, *event));
                    *event = (xEvent *) fev;

                    fev->type = ev->type;
                    fev->window = ev->window;
                    fev->mode = ev->mode;
                    fev->detail = ev->detail;
                    fev->time = ev->time;
                    fev->deviceid = ev->deviceid;
                    break;
                }
            case XI_DeviceMappingNotify:
                {
                    register XDeviceMappingEvent *ev = (XDeviceMappingEvent *) re;
                    register deviceMappingNotify *mev;

                    *count = 1;
                    mev = (deviceMappingNotify *) Xmalloc(*count * sizeof(xEvent));
                    if (!mev)
                        return (_XUnknownNativeEvent(dpy, re, *event));
                    *event = (xEvent *) mev;

                    mev->type = ev->type;
                    mev->firstKeyCode = ev->first_keycode;
                    mev->request = ev->request;
                    mev->count = ev->count;
                    mev->time = ev->time;
                    mev->deviceid = ev->deviceid;
                    break;
                }
            case XI_DeviceStateNotify:
                {
                    register XDeviceStateNotifyEvent *ev = (XDeviceStateNotifyEvent *) re;
                    register deviceStateNotify *sev;
                    register xEvent *tev;
                    XInputClass *any = (XInputClass *) & ev->data[0];
                    unsigned char *sav_id;

                    *count = 1;

                    for (i = 0; i < ev->num_classes; i++) {
                        if (any->class == KeyClass) {
                            XKeyStatus *k = (XKeyStatus *) any;

                            if (k->num_keys > 32)
                                (*count)++;
                        } else if (any->class == ButtonClass) {
                            XButtonStatus *b = (XButtonStatus *) any;

                            if (b->num_buttons > 32)
                                (*count)++;
                        } else if (any->class == ValuatorClass) {
                            XValuatorStatus *v = (XValuatorStatus *) any;

                            if (v->num_valuators > 3)
                                (*count)++;
                        }
                        any = (XInputClass *) ((char *)any + any->length);
                    }

                    sev = (deviceStateNotify *) Xmalloc(*count * sizeof(xEvent));
                    if (!sev)
                        return (_XUnknownNativeEvent(dpy, re, *event));
                    *event = (xEvent *) sev;
                    tev = (xEvent *) (sev + 1);

                    sev->type = ev->type;
                    sev->deviceid = ev->deviceid;
                    sav_id = &(sev->deviceid);
                    sev->time = ev->time;
                    sev->classes_reported = 0;

                    any = (XInputClass *) & ev->data[0];
                    for (i = 0; i < ev->num_classes; i++) {
                        if (any->class == KeyClass) {
                            XKeyStatus *k = (XKeyStatus *) any;
                            register deviceKeyStateNotify *kev;

                            sev->classes_reported |= (1 << KeyClass);
                            sev->num_keys = k->num_keys;
                            memcpy((char *)(sev->keys), (char *)(k->keys), 4);
                            if (k->num_keys > 32) {
                                kev = (deviceKeyStateNotify *) tev++;
                                kev->type = info->codes->first_event +
                                    XI_DeviceKeystateNotify;
                                kev->deviceid = ev->deviceid;
                                *sav_id |= MORE_EVENTS;
                                sav_id = &(kev->deviceid);
                                memcpy((char *)(kev->keys), (char *)(&k->keys[4]), 28);
                            }
                        } else if (any->class == ButtonClass) {
                            XButtonStatus *b = (XButtonStatus *) any;
                            register deviceButtonStateNotify *bev;

                            sev->classes_reported |= (1 << ButtonClass);
                            sev->num_buttons = b->num_buttons;
                            memcpy((char *)(sev->buttons), (char *)(b->buttons), 4);
                            if (b->num_buttons > 32) {
                                bev = (deviceButtonStateNotify *) tev++;
                                bev->type = info->codes->first_event +
                                    XI_DeviceButtonstateNotify;
                                bev->deviceid = ev->deviceid;
                                *sav_id |= MORE_EVENTS;
                                sav_id = &(bev->deviceid);
                                memcpy((char *)(bev->buttons), (char *)(&b->buttons[4]),
                                        28);
                            }
                        } else if (any->class == ValuatorClass) {
                            XValuatorStatus *val = (XValuatorStatus *) any;
                            register deviceValuator *vev;

                            sev->classes_reported |= (1 << ValuatorClass);
                            sev->num_valuators = val->num_valuators < 3 ?
                                val->num_valuators : 3;
                            switch (sev->num_valuators) {
                                case 3:
                                    sev->valuator2 = val->valuators[2];
                                case 2:
                                    sev->valuator1 = val->valuators[1];
                                case 1:
                                    sev->valuator0 = val->valuators[0];
                            }
                            if (val->num_valuators > 3) {
                                vev = (deviceValuator *) tev++;
                                vev->type = info->codes->first_event + XI_DeviceValuator;
                                vev->deviceid = ev->deviceid;
                                vev->first_valuator = 3;
                                vev->num_valuators = val->num_valuators - 3;
                                *sav_id |= MORE_EVENTS;
                                sav_id = &(vev->deviceid);
                                i = val->num_valuators;
                                if (i > 6)
                                    i = 6;
                                switch (i) {
                                    case 6:
                                        vev->valuator2 = val->valuators[5];
                                    case 5:
                                        vev->valuator1 = val->valuators[4];
                                    case 4:
                                        vev->valuator0 = val->valuators[3];
                                }
                            }
                        }
                        any = (XInputClass *) ((char *)any + any->length);
                    }
                    break;
                }
            case XI_ChangeDeviceNotify:
                {
                    register XChangeDeviceNotifyEvent *ev = (XChangeDeviceNotifyEvent *) re;
                    register changeDeviceNotify *cev;

                    *count = 1;
                    cev = (changeDeviceNotify *) Xmalloc(*count * sizeof(xEvent));
                    if (!cev)
                        return (_XUnknownNativeEvent(dpy, re, *event));
                    *event = (xEvent *) cev;

                    cev->type = ev->type;
                    cev->request = ev->request;
                    cev->time = ev->time;
                    cev->deviceid = ev->deviceid;
                    break;
                }
            default:
                return (_XUnknownNativeEvent(dpy, re, *event));
        } /* switch */
    } else /* if type != GenericEvent */
    {
#if 0
        switch (((XGenericEvent*)re)->evtype)
        {
            case XI_HierarchyChangedNotify:
                {
                    XDeviceHierarchyChangedEvent *ev =
                        (XDeviceHierarchyChangedEvent*)re;
                    XIDeviceHierarchyEvent *dhcev;
                    *count = 1;
                    dhcev = (XIDeviceHierarchyEvent *)Xmalloc(*count * sizeof(xEvent));
                    if (!dhcev)
                        return (_XUnknownNativeEvent(dpy, re, *event));

                    *event = (xEvent *) dhcev;
                    dhcev->type = ev->type;
                    dhcev->time = ev->time;
                    break;
                }
            default:
                return (_XUnknownNativeEvent(dpy, re, *event));
        }
#endif
    }
    return (1);
}
