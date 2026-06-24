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
 * XListInputDevices - Request the server to return a list of 
 *			 available input devices.
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

/* Calculate length field to a multiples of sizeof(XID). XIDs are typedefs
 * to ulong and thus may be 8 bytes on some platforms. This can trigger a
 * SIGBUS if a class ends up not being 8-aligned (e.g. after XAxisInfo).
 */
static int pad_to_xid(int base_size)
{
    int padsize = sizeof(XID);

    return ((base_size + padsize - 1)/padsize) * padsize;
}

static int
SizeClassInfo(xAnyClassPtr *any, size_t len, int num_classes, size_t *size)
{
    int j;
    size_t sz = 0;

    for (j = 0; j < num_classes; j++) {
        switch ((*any)->class) {
            case KeyClass:
                sz += pad_to_xid(sizeof(XKeyInfo));
                break;
            case ButtonClass:
                sz += pad_to_xid(sizeof(XButtonInfo));
                break;
            case ValuatorClass:
                {
                    xValuatorInfoPtr v;

                    if (len < sizeof(v))
                        return 1;
                    v = (xValuatorInfoPtr) *any;
                    sz += pad_to_xid(sizeof(XValuatorInfo) +
                        (v->num_axes * sizeof(XAxisInfo)));
                    break;
                }
            default:
                break;
        }
        if ((*any)->length > len)
            return 1;
        *any = (xAnyClassPtr) ((char *)(*any) + (*any)->length);
    }

    *size = sz;

    return 0;
}

static void
ParseClassInfo(xAnyClassPtr *any, XAnyClassPtr *Any, int num_classes)
{
    int j;

    for (j = 0; j < num_classes; j++) {
        switch ((*any)->class) {
            case KeyClass:
                {
                    XKeyInfoPtr K = (XKeyInfoPtr) *Any;
                    xKeyInfoPtr k = (xKeyInfoPtr) *any;

                    K->class = KeyClass;
                    K->length = pad_to_xid(sizeof(XKeyInfo));
                    K->min_keycode = k->min_keycode;
                    K->max_keycode = k->max_keycode;
                    K->num_keys = k->num_keys;
                    break;
                }
            case ButtonClass:
                {
                    XButtonInfoPtr B = (XButtonInfoPtr) *Any;
                    xButtonInfoPtr b = (xButtonInfoPtr) *any;

                    B->class = ButtonClass;
                    B->length = pad_to_xid(sizeof(XButtonInfo));
                    B->num_buttons = b->num_buttons;
                    break;
                }
            case ValuatorClass:
                {
                    int k;
                    XValuatorInfoPtr V = (XValuatorInfoPtr) *Any;
                    xValuatorInfoPtr v = (xValuatorInfoPtr) *any;
                    XAxisInfoPtr A;
                    xAxisInfoPtr a;

                    V->class = ValuatorClass;
                    V->length = pad_to_xid(sizeof(XValuatorInfo) +
                        (v->num_axes * sizeof(XAxisInfo)));
                    V->num_axes = v->num_axes;
                    V->motion_buffer = v->motion_buffer_size;
                    V->mode = v->mode;
                    A = (XAxisInfoPtr) ((char *)V + sizeof(XValuatorInfo));
                    V->axes = A;
                    a = (xAxisInfoPtr) ((char *)(*any) + sizeof(xValuatorInfo));
                    for (k = 0; k < (int)v->num_axes; k++, a++, A++) {
                        A->min_value = a->min_value;
                        A->max_value = a->max_value;
                        A->resolution = a->resolution;
                    }
                    break;
                }
            default:
                break;
        }
        *any = (xAnyClassPtr) ((char *)(*any) + (*any)->length);
        *Any = (XAnyClassPtr) ((char *)(*Any) + (*Any)->length);
    }
}

XDeviceInfo *
XListInputDevices(
    register Display	*dpy,
    int			*ndevices_return)
{
    size_t s, size;
    xListInputDevicesReq *req;
    xListInputDevicesReply rep;
    xDeviceInfo *list, *slist = NULL;
    XDeviceInfo *sclist = NULL;
    XDeviceInfo *clist = NULL;
    xAnyClassPtr any, sav_any;
    XAnyClassPtr Any;
    char *end = NULL;
    unsigned char *nptr, *Nptr;
    int i;
    unsigned long rlen;
    XExtDisplayInfo *info = XInput_find_display(dpy);
    int ndevices;

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_Initial_Release, info) == -1)
	return ((XDeviceInfo *) NULL);

    GetReq(ListInputDevices, req);
    req->reqType = info->codes->major_opcode;
    req->ReqType = X_ListInputDevices;

    if (!_XReply(dpy, (xReply *) & rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return (XDeviceInfo *) NULL;
    }

    if ((ndevices = rep.ndevices)) {	/* at least 1 input device */
	size = ndevices * sizeof(XDeviceInfo);
	if (rep.length < (INT_MAX >> 2)) {
	    rlen = rep.length << 2;	/* multiply length by 4    */
	    slist = list = Xmalloc(rlen);
	}
	if (!slist) {
	    _XEatDataWords(dpy, rep.length);
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return (XDeviceInfo *) NULL;
	}
	_XRead(dpy, (char *)list, rlen);

	any = (xAnyClassPtr) ((char *)list + (ndevices * sizeof(xDeviceInfo)));
	sav_any = any;
	end = (char *)list + rlen;
	for (i = 0; i < ndevices; i++, list++) {
            if(SizeClassInfo(&any, end - (char *)any, (int)list->num_classes, &s))
                goto out;
            size += s;
	}

	Nptr = ((unsigned char *)list) + rlen;
	for (i = 0, nptr = (unsigned char *)any; i < ndevices; i++) {
	    if (nptr >= Nptr)
		goto out;
	    size += *nptr + 1;
	    nptr += (*nptr + 1);
	}

	clist = (XDeviceInfoPtr) Xmalloc(size);
	if (!clist) {
	    XFree((char *)slist);
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return (XDeviceInfo *) NULL;
	}
	sclist = clist;
	Any = (XAnyClassPtr) ((char *)clist +
			      (ndevices * sizeof(XDeviceInfo)));
	list = slist;
	any = sav_any;
	for (i = 0; i < ndevices; i++, list++, clist++) {
	    clist->type = list->type;
	    clist->id = list->id;
	    clist->use = list->use;
	    clist->num_classes = list->num_classes;
	    clist->inputclassinfo = Any;

            ParseClassInfo(&any, &Any, (int)list->num_classes);
	}

	clist = sclist;
	nptr = (unsigned char *)any;
	Nptr = (unsigned char *)Any;
	for (i = 0; i < ndevices; i++, clist++) {
	    clist->name = (char *)Nptr;
	    memcpy(Nptr, nptr + 1, *nptr);
	    Nptr += (*nptr);
	    *Nptr++ = '\0';
	    nptr += (*nptr + 1);
	}
    }

    *ndevices_return = ndevices;

  out:
    XFree((char *)slist);
    UnlockDisplay(dpy);
    SyncHandle();
    return (sclist);
}

/***********************************************************************
 *
 * Free the list of input devices.
 *
 */

void
XFreeDeviceList(XDeviceInfo *list)
{
    if (list != NULL) {
	XFree((char *)list);
    }
}
