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
 * XGetExtensionVersion - Get the version of the input extension.
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

XExtensionVersion *
XGetExtensionVersion(register Display * dpy, _Xconst char *name)
{
    XExtensionVersion *ext;
    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);

    if (_XiCheckExtInit(dpy, Dont_Check, info) == -1)
        return NULL;

    ext = _XiGetExtensionVersionRequest(dpy, name, info->codes->major_opcode);

    UnlockDisplay(dpy);
    SyncHandle();

    return ext;
}

_X_HIDDEN XExtensionVersion*
_XiGetExtensionVersionRequest(Display *dpy, _Xconst char *name, int xi_opcode)
{
    xGetExtensionVersionReq *req;
    xGetExtensionVersionReply rep;
    XExtensionVersion *ext;

    GetReq(GetExtensionVersion, req);
    req->reqType = xi_opcode;
    req->ReqType = X_GetExtensionVersion;
    req->nbytes = strlen(name);
    req->length += (unsigned)(req->nbytes + 3) >> 2;
    _XSend(dpy, name, (long)req->nbytes);

    if (!_XReply(dpy, (xReply *) & rep, 0, xTrue)) {
	return NULL;
    }

    ext = (XExtensionVersion *) Xmalloc(sizeof(XExtensionVersion));
    if (ext) {
	ext->present = rep.present;
	if (ext->present) {
	    ext->major_version = rep.major_version;
	    ext->minor_version = rep.minor_version;
	}
    }

    return ext;
}
