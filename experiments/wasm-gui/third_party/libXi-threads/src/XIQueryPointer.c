/************************************************************

Copyright 2006 Peter Hutterer <peter@cs.unisa.edu.au>

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

*/

/***********************************************************************
 *
 * XIQueryPointer - Query the pointer of an extension input device.
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <X11/extensions/XI2proto.h>
#include <X11/Xlibint.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/extutil.h>
#include "XIint.h"

#define FP1616toDBL(x) ((x) * 1.0 / (1 << 16))

Bool
XIQueryPointer(Display     *dpy,
               int         deviceid,
               Window      w,
               Window      *root,
               Window      *child,
               double      *root_x,
               double      *root_y,
               double      *win_x,
               double      *win_y,
               XIButtonState       *buttons,
               XIModifierState     *mods,
               XIGroupState        *group)
{
    xXIQueryPointerReq *req;
    xXIQueryPointerReply rep;

    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_2_0, info) == -1)
	return False;

    GetReq(XIQueryPointer, req);
    req->reqType = info->codes->major_opcode;
    req->ReqType = X_XIQueryPointer;
    req->deviceid = deviceid;
    req->win = w;

    if (!_XReply(dpy, (xReply *)&rep,
                 (sizeof(xXIQueryPointerReply) - sizeof(xReply))/4, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }


    *root = rep.root;
    *child = rep.child;
    *root_x = FP1616toDBL(cvtINT16toInt(rep.root_x));
    *root_y = FP1616toDBL(cvtINT16toInt(rep.root_y));
    *win_x = FP1616toDBL(cvtINT16toInt(rep.win_x));
    *win_y = FP1616toDBL(cvtINT16toInt(rep.win_y));

    mods->base          = rep.mods.base_mods;
    mods->latched       = rep.mods.latched_mods;
    mods->locked        = rep.mods.locked_mods;
    mods->effective     = mods->base | mods->latched | mods->locked;

    group->base         = rep.group.base_group;
    group->latched      = rep.group.latched_group;
    group->locked       = rep.group.locked_group;
    group->effective    = group->base | group->latched | group->locked;

    buttons->mask_len   = rep.buttons_len * 4;
    buttons->mask       = malloc(buttons->mask_len);
    if (buttons->mask)
        _XRead(dpy, (char*)buttons->mask, buttons->mask_len);

    UnlockDisplay(dpy);
    SyncHandle();
    return rep.same_screen;
}
