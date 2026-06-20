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
 * XDeleteDeviceProperties - delete an input device's properties.
 *
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
XDeleteDeviceProperty(Display* dpy, XDevice* dev, Atom property)
{
    xDeleteDevicePropertyReq   *req;

    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_Initial_Release, info) == -1)
	return;

    GetReq(DeleteDeviceProperty, req);
    req->reqType    = info->codes->major_opcode;
    req->ReqType    = X_DeleteDeviceProperty;
    req->deviceid   = dev->device_id;
    req->property   = property;

    UnlockDisplay(dpy);
    SyncHandle();
    return;
}

