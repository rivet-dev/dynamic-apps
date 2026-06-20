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
 * XQueryDeviceState - Query the state of an extension input device.
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
#include <limits.h>

XDeviceState *
XQueryDeviceState(
    register Display	*dpy,
    XDevice		*dev)
{
    int i, j;
    unsigned long rlen;
    size_t size = 0;
    xQueryDeviceStateReq *req;
    xQueryDeviceStateReply rep;
    XDeviceState *state = NULL;
    XInputClass *any, *Any;
    char *data = NULL, *end = NULL;
    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_Initial_Release, info) == -1)
        return NULL;

    GetReq(QueryDeviceState, req);
    req->reqType = info->codes->major_opcode;
    req->ReqType = X_QueryDeviceState;
    req->deviceid = dev->device_id;

    if (!_XReply(dpy, (xReply *) & rep, 0, xFalse))
        goto out;

    if (rep.length > 0) {
	if (rep.length < (INT_MAX >> 2)) {
	    rlen = (unsigned long) rep.length << 2;
	    data = Xmalloc(rlen);
	    end = data + rlen;
	}
	if (!data) {
	    _XEatDataWords(dpy, rep.length);
	    goto out;
	}
	_XRead(dpy, data, rlen);

	for (i = 0, any = (XInputClass *) data; i < (int)rep.num_classes; i++) {
	    if ((char *)any + sizeof(XInputClass) > end ||
		any->length == 0 || any->length > rlen)
		goto out;
	    rlen -= any->length;

	    switch (any->class) {
	    case KeyClass:
		size += sizeof(XKeyState);
		break;
	    case ButtonClass:
		size += sizeof(XButtonState);
		break;
	    case ValuatorClass:
	    {
		xValuatorState *v = (xValuatorState *) any;
		if ((char *)any + sizeof(xValuatorState) > end)
		    goto out;
		size += (sizeof(XValuatorState) +
			 (v->num_valuators * sizeof(int)));
	    }
		break;
	    }
	    any = (XInputClass *) ((char *)any + any->length);
	}
	state = (XDeviceState *) Xmalloc(size + sizeof(XDeviceState));
	if (!state)
            goto out;

	state->device_id = dev->device_id;
	state->num_classes = rep.num_classes;
	state->data = (XInputClass *) (state + 1);

	Any = state->data;
	for (i = 0, any = (XInputClass *) data; i < (int)rep.num_classes; i++) {
	    switch (any->class) {
	    case KeyClass:
	    {
		xKeyState *k = (xKeyState *) any;
		XKeyState *K = (XKeyState *) Any;

		K->class = k->class;
		K->length = sizeof(XKeyState);
		K->num_keys = k->num_keys;
		memcpy((char *)&K->keys[0], (char *)&k->keys[0], 32);
		Any = (XInputClass *) (K + 1);
	    }
		break;
	    case ButtonClass:
	    {
		xButtonState *b = (xButtonState *) any;
		XButtonState *B = (XButtonState *) Any;

		B->class = b->class;
		B->length = sizeof(XButtonState);
		B->num_buttons = b->num_buttons;
		memcpy((char *)&B->buttons[0], (char *)&b->buttons[0], 32);
		Any = (XInputClass *) (B + 1);
	    }
		break;
	    case ValuatorClass:
	    {
		xValuatorState *v = (xValuatorState *) any;
		XValuatorState *V = (XValuatorState *) Any;
		CARD32 *valuators = (CARD32 *) (v + 1);

		V->class = v->class;
		V->length = sizeof(XValuatorState) +
			    v->num_valuators * sizeof(int);
		V->num_valuators = v->num_valuators;
		V->mode = v->mode;
		Any = (XInputClass *) (V + 1);
		V->valuators = (int *)Any;
		for (j = 0; j < (int)V->num_valuators; j++)
		    *(V->valuators + j) = *valuators++;
		Any = (XInputClass *) ((char *)Any +
				       V->num_valuators * sizeof(int));
	    }
		break;
	    }
	    any = (XInputClass *) ((char *)any + any->length);
	}
    }
out:
    Xfree(data);

    UnlockDisplay(dpy);
    SyncHandle();
    return (state);
}

void
XFreeDeviceState(XDeviceState *list)
{
    XFree((char *)list);
}
