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
 * XListDeviceProperties - List an input device's properties.
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

Atom*
XListDeviceProperties(Display* dpy, XDevice* dev, int *nprops_return)
{
    xListDevicePropertiesReq    *req;
    xListDevicePropertiesReply  rep;
    XExtDisplayInfo             *info = XInput_find_display(dpy);
    Atom                        *props = NULL;

    LockDisplay(dpy);
    *nprops_return = 0;
    if (_XiCheckExtInit(dpy, XInput_Initial_Release, info) == -1)
        return NULL;

    GetReq(ListDeviceProperties, req);
    req->reqType = info->codes->major_opcode;
    req->ReqType = X_ListDeviceProperties;
    req->deviceid = dev->device_id;

    if (!_XReply(dpy, (xReply*)&rep, 0, xFalse))
        goto cleanup;

    if (rep.nAtoms) {
        props = (Atom*)Xmalloc(rep.nAtoms * sizeof(Atom));
        if (!props)
        {
            _XEatDataWords(dpy, rep.length);
            goto cleanup;
        }

        _XRead32(dpy, (long*)props, rep.nAtoms << 2);
    }

    *nprops_return = rep.nAtoms;

cleanup:
    UnlockDisplay(dpy);
    SyncHandle();
    return props;
}

