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
 * XGetFeedbackControl - get the feedback attributes of an extension device.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/extensions/XI.h>
#include <X11/extensions/XIproto.h>
#include <X11/Xlibint.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/extutil.h>
#include "XIint.h"
#include <limits.h>

XFeedbackState *
XGetFeedbackControl(
    register Display	*dpy,
    XDevice		*dev,
    int			*num_feedbacks_out)
{
    XFeedbackState *Feedback = NULL;
    XFeedbackState *Sav = NULL;
    xFeedbackState *f = NULL;
    xFeedbackState *sav = NULL;
    char *end = NULL;
    xGetFeedbackControlReq *req;
    xGetFeedbackControlReply rep;
    XExtDisplayInfo *info = XInput_find_display(dpy);
    int num_feedbacks = 0;
    int known_feedbacks = 0;

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_Initial_Release, info) == -1)
        return NULL;

    GetReq(GetFeedbackControl, req);
    req->reqType = info->codes->major_opcode;
    req->ReqType = X_GetFeedbackControl;
    req->deviceid = dev->device_id;

    if (!_XReply(dpy, (xReply *) & rep, 0, xFalse))
	goto out;

    if (rep.length > 0) {
	unsigned long nbytes;
	size_t size = 0;
	int i;

	num_feedbacks = rep.num_feedbacks;

	if (rep.length < (INT_MAX >> 2)) {
	    nbytes = rep.length << 2;
	    f = Xmalloc(nbytes);
	}
	if (!f) {
	    _XEatDataWords(dpy, rep.length);
	    goto out;
	}
	sav = f;
	end = (char *)f + nbytes;
	_XRead(dpy, (char *)f, nbytes);

	for (i = 0; i < num_feedbacks; i++) {
	    if ((char *)f + sizeof(*f) > end ||
	        f->length == 0 || f->length > nbytes)
		goto out;
	    nbytes -= f->length;

	    switch (f->class) {
	    case KbdFeedbackClass:
		size += sizeof(XKbdFeedbackState);
		break;
	    case PtrFeedbackClass:
		size += sizeof(XPtrFeedbackState);
		break;
	    case IntegerFeedbackClass:
		size += sizeof(XIntegerFeedbackState);
		break;
	    case StringFeedbackClass:
	    {
		xStringFeedbackState *strf = (xStringFeedbackState *) f;
		if ((char *)f + sizeof(*strf) > end)
		    goto out;
		size += sizeof(XStringFeedbackState) +
		    (strf->num_syms_supported * sizeof(KeySym));
	    }
		break;
	    case LedFeedbackClass:
		size += sizeof(XLedFeedbackState);
		break;
	    case BellFeedbackClass:
		size += sizeof(XBellFeedbackState);
		break;
	    default:
		/* unknown classes are skipped */
		break;
	    }
	    if (size > INT_MAX)
		goto out;
	    f = (xFeedbackState *) ((char *)f + f->length);
	}

	Feedback = Xmalloc(size);
	if (!Feedback)
	    goto out;

	Sav = Feedback;
	known_feedbacks = num_feedbacks;

	f = sav;
	for (i = 0; i < num_feedbacks; i++) {
	    Bool skip = False;
	    switch (f->class) {
	    case KbdFeedbackClass:
	    {
		xKbdFeedbackState *k;
		XKbdFeedbackState *K;

		k = (xKbdFeedbackState *) f;
		K = (XKbdFeedbackState *) Feedback;

		K->class = k->class;
		K->length = sizeof(XKbdFeedbackState);
		K->id = k->id;
		K->click = k->click;
		K->percent = k->percent;
		K->pitch = k->pitch;
		K->duration = k->duration;
		K->led_mask = k->led_mask;
		K->global_auto_repeat = k->global_auto_repeat;
		memcpy((char *)&K->auto_repeats[0],
		       (char *)&k->auto_repeats[0], 32);
		break;
	    }
	    case PtrFeedbackClass:
	    {
		xPtrFeedbackState *p;
		XPtrFeedbackState *P;

		p = (xPtrFeedbackState *) f;
		P = (XPtrFeedbackState *) Feedback;

		P->class = p->class;
		P->length = sizeof(XPtrFeedbackState);
		P->id = p->id;
		P->accelNum = p->accelNum;
		P->accelDenom = p->accelDenom;
		P->threshold = p->threshold;
		break;
	    }
	    case IntegerFeedbackClass:
	    {
		xIntegerFeedbackState *ifs;
		XIntegerFeedbackState *I;

		ifs = (xIntegerFeedbackState *) f;
		I = (XIntegerFeedbackState *) Feedback;

		I->class = ifs->class;
		I->length = sizeof(XIntegerFeedbackState);
		I->id = ifs->id;
		I->resolution = ifs->resolution;
		I->minVal = ifs->min_value;
		I->maxVal = ifs->max_value;
		break;
	    }
	    case StringFeedbackClass:
	    {
		xStringFeedbackState *s;
		XStringFeedbackState *S;

		s = (xStringFeedbackState *) f;
		S = (XStringFeedbackState *) Feedback;

		S->class = s->class;
		S->length = sizeof(XStringFeedbackState) +
		    (s->num_syms_supported * sizeof(KeySym));
		S->id = s->id;
		S->max_symbols = s->max_symbols;
		S->num_syms_supported = s->num_syms_supported;
		S->syms_supported = (KeySym *) (S + 1);
		memcpy((char *)S->syms_supported, (char *)(s + 1),
		       (S->num_syms_supported * sizeof(KeySym)));
		break;
	    }
	    case LedFeedbackClass:
	    {
		xLedFeedbackState *l;
		XLedFeedbackState *L;

		l = (xLedFeedbackState *) f;
		L = (XLedFeedbackState *) Feedback;

		L->class = l->class;
		L->length = sizeof(XLedFeedbackState);
		L->id = l->id;
		L->led_values = l->led_values;
		L->led_mask = l->led_mask;
		break;
	    }
	    case BellFeedbackClass:
	    {
		xBellFeedbackState *b;
		XBellFeedbackState *B;

		b = (xBellFeedbackState *) f;
		B = (XBellFeedbackState *) Feedback;

		B->class = b->class;
		B->length = sizeof(XBellFeedbackState);
		B->id = b->id;
		B->percent = b->percent;
		B->pitch = b->pitch;
		B->duration = b->duration;
		break;
	    }
	    default:
		--known_feedbacks;
		skip = True;
		break;
	    }
	    f = (xFeedbackState *) ((char *)f + f->length);
	    if (!skip)
		Feedback = (XFeedbackState *) ((char *)Feedback + Feedback->length);
	}
    }
out:
    XFree((char *)sav);
    *num_feedbacks_out = known_feedbacks;

    UnlockDisplay(dpy);
    SyncHandle();
    return (Sav);
}

void
XFreeFeedbackList(XFeedbackState *list)
{
    XFree((char *)list);
}
