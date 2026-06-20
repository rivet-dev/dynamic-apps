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
 * XGrabDeviceKey - Grab a key on an extension device.
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/extensions/XI.h>
#include <X11/extensions/XIproto.h>
#include <X11/Xlibint.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/extutil.h>
#include "XIint.h"

int
XGrabDeviceKey(
    register Display	*dpy,
    XDevice		*dev,
    unsigned int	 key,		/* CARD8 */
    unsigned int	 modifiers,	/* CARD16 */
    XDevice		*modifier_device,
    Window		 grab_window,
    Bool		 owner_events,
    unsigned int	 event_count,
    XEventClass		*event_list,
    int			 this_device_mode,
    int			 other_devices_mode)
{
    register xGrabDeviceKeyReq *req;
    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_Initial_Release, info) == -1)
	return (NoSuchExtension);

    GetReq(GrabDeviceKey, req);

    req->reqType = info->codes->major_opcode;
    req->ReqType = X_GrabDeviceKey;
    req->grabbed_device = dev->device_id;
    req->key = key;
    req->modifiers = modifiers;
    if (modifier_device)
	req->modifier_device = modifier_device->device_id;
    else
	req->modifier_device = UseXKeyboard;
    req->grabWindow = grab_window;
    req->ownerEvents = owner_events;
    req->event_count = event_count;
    req->this_device_mode = this_device_mode;
    req->other_devices_mode = other_devices_mode;
    req->length += event_count;

    /* note: Data is a macro that uses its arguments multiple
     * times, so "nvalues" is changed in a separate assignment
     * statement */

    if (event_count) {
	event_count <<= 2;
	Data32(dpy, (long *)event_list, event_count);
    }

    UnlockDisplay(dpy);
    SyncHandle();
    return (Success);
}
