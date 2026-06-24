/*
 *
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
 *
 */

/* RANDOM CRUFT! THIS HAS NO OFFICIAL X CONSORTIUM BLESSING */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/Xlibint.h>
#include <X11/extensions/MITMisc.h>
#include <X11/extensions/mitmiscproto.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>

static XExtensionInfo _mit_info_data;
static XExtensionInfo *mit_info = &_mit_info_data;
static const char *mit_extension_name = MITMISCNAME;

#define MITCheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, mit_extension_name, val)

/*****************************************************************************
 *                                                                           *
 *			   private utility routines                          *
 *                                                                           *
 *****************************************************************************/

static int close_display(Display *dpy, XExtCodes *codes);
static /* const */ XExtensionHooks mit_extension_hooks = {
    NULL,				/* create_gc */
    NULL,				/* copy_gc */
    NULL,				/* flush_gc */
    NULL,				/* free_gc */
    NULL,				/* create_font */
    NULL,				/* free_font */
    close_display,			/* close_display */
    NULL,				/* wire_to_event */
    NULL,				/* event_to_wire */
    NULL,				/* error */
    NULL				/* error_string */
};

static XEXT_GENERATE_FIND_DISPLAY (find_display, mit_info, mit_extension_name,
				   &mit_extension_hooks, MITMiscNumberEvents,
				   NULL)

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, mit_info)


/*****************************************************************************
 *                                                                           *
 *		    public routines               			     *
 *                                                                           *
 *****************************************************************************/

Bool XMITMiscQueryExtension (Display *dpy, int *event_basep, int *error_basep)
{
    XExtDisplayInfo *info = find_display (dpy);

    if (XextHasExtension(info)) {
	*event_basep = info->codes->first_event;
	*error_basep = info->codes->first_error;
	return True;
    } else {
	return False;
    }
}


Status XMITMiscSetBugMode(Display *dpy, Bool onOff)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xMITSetBugModeReq *req;

    MITCheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(MITSetBugMode, req);
    req->reqType = info->codes->major_opcode;
    req->mitReqType = X_MITSetBugMode;
    req->onOff = onOff;
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

Bool XMITMiscGetBugMode(Display *dpy)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xMITGetBugModeReq *req;
    xMITGetBugModeReply rep;

    MITCheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(MITGetBugMode, req);
    req->reqType = info->codes->major_opcode;
    req->mitReqType = X_MITGetBugMode;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return rep.onOff;
}
