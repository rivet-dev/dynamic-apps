/*
 * Copyright 1992 Network Computing Devices
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of NCD. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  NCD. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NCD. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NCD.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, Network Computing Devices
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <X11/Xlibint.h>
#include <X11/extensions/XLbx.h>
#include <X11/extensions/lbxproto.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>

static XExtensionInfo _lbx_info_data;
static XExtensionInfo *lbx_info = &_lbx_info_data;
static const char *lbx_extension_name = LBXNAME;

#define LbxCheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, lbx_extension_name, val)

static int close_display(Display *dpy, XExtCodes *codes);
static char *error_string(Display *dpy, int code, XExtCodes *codes,
			  char *buf, int n);
static /* const */ XExtensionHooks lbx_extension_hooks = {
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
    error_string,			/* error_string */
};

static const char *lbx_error_list[] = {
    "BadLbxClient",			/* BadLbxClient */
};

static XEXT_GENERATE_FIND_DISPLAY (find_display, lbx_info, lbx_extension_name,
				   &lbx_extension_hooks, LbxNumberEvents, NULL)

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, lbx_info)

static XEXT_GENERATE_ERROR_STRING (error_string, lbx_extension_name,
				   LbxNumberErrors, lbx_error_list)


Bool XLbxQueryExtension (
    Display *dpy,
    int *requestp, int *event_basep, int *error_basep)
{
    XExtDisplayInfo *info = find_display (dpy);

    if (XextHasExtension(info)) {
	*requestp = info->codes->major_opcode;
	*event_basep = info->codes->first_event;
	*error_basep = info->codes->first_error;
	return True;
    } else {
	return False;
    }
}


int XLbxGetEventBase(Display *dpy)
{
    XExtDisplayInfo *info = find_display (dpy);

    if (XextHasExtension(info)) {
	return info->codes->first_event;
    } else {
	return -1;
    }
}


Bool XLbxQueryVersion(Display *dpy, int *majorVersion, int *minorVersion)
{
    XExtDisplayInfo *info = find_display (dpy);
    xLbxQueryVersionReply rep;
    register xLbxQueryVersionReq *req;

    LbxCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(LbxQueryVersion, req);
    req->reqType = info->codes->major_opcode;
    req->lbxReqType = X_LbxQueryVersion;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }
    *majorVersion = rep.majorVersion;
    *minorVersion = rep.minorVersion;
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

/* all other requests will run after Xlib has lost the wire ... */
