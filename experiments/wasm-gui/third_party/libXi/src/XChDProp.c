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
 * XChangeDeviceProperties - change an input device's properties.
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

void
XChangeDeviceProperty(Display* dpy, XDevice* dev,
                      Atom property, Atom type,
                      int format, int mode,
                      _Xconst unsigned char *data, int nelements)
{
    xChangeDevicePropertyReq   *req;
    int                         len;

    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_Initial_Release, info) == -1)
	return;

    GetReq(ChangeDeviceProperty, req);
    req->reqType    = info->codes->major_opcode;
    req->ReqType    = X_ChangeDeviceProperty;
    req->deviceid   = dev->device_id;
    req->property   = property;
    req->type       = type;
    req->mode       = mode;
    if (nelements < 0) {
	req->nUnits = 0;
	req->format = 0; /* ask for garbage, get garbage */
    } else {
	req->nUnits = nelements;
	req->format = format;
    }

    switch (req->format) {
    case 8:
	len = ((long)nelements + 3) >> 2;
	if (dpy->bigreq_size || req->length + len <= (unsigned) 65535) {
	    SetReqLen(req, len, len);
	    Data (dpy, (_Xconst char *)data, nelements);
	} /* else force BadLength */
	break;

    case 16:
	len = ((long)nelements + 1) >> 1;
	if (dpy->bigreq_size || req->length + len <= (unsigned) 65535) {
	    SetReqLen(req, len, len);
	    len = (long)nelements << 1;
	    Data16 (dpy, (_Xconst short *) data, len);
	} /* else force BadLength */
	break;

    case 32:
	len = nelements;
	if (dpy->bigreq_size || req->length + len <= (unsigned) 65535) {
	    SetReqLen(req, len, len);
	    len = (long)nelements << 2;
	    Data32 (dpy, (_Xconst long *) data, len);
	} /* else force BadLength */
	break;

    default:
	/* BadValue will be generated */ ;
    }

    UnlockDisplay(dpy);
    SyncHandle();
}

