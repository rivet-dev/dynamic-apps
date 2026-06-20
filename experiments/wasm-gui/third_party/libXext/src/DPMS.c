/*****************************************************************

Copyright (c) 1996 Digital Equipment Corporation, Maynard, Massachusetts.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
DIGITAL EQUIPMENT CORPORATION BE LIABLE FOR ANY CLAIM, DAMAGES, INCLUDING,
BUT NOT LIMITED TO CONSEQUENTIAL OR INCIDENTAL DAMAGES, OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Digital Equipment Corporation
shall not be used in advertising or otherwise to promote the sale, use or other
dealings in this Software without prior written authorization from Digital
Equipment Corporation.

******************************************************************/

/*
 * HISTORY
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/Xlibint.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/dpmsproto.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>
#include <stdio.h>

static XExtensionInfo _dpms_info_data;
static XExtensionInfo *dpms_info = &_dpms_info_data;
static const char *dpms_extension_name = DPMSExtensionName;

#define DPMSCheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, dpms_extension_name, val)

/*****************************************************************************
 *                                                                           *
 *                         private utility routines                          *
 *                                                                           *
 *****************************************************************************/

static int close_display(Display *dpy, XExtCodes *codes);
static /* const */ XExtensionHooks dpms_extension_hooks = {
    NULL,                               /* create_gc */
    NULL,                               /* copy_gc */
    NULL,                               /* flush_gc */
    NULL,                               /* free_gc */
    NULL,                               /* create_font */
    NULL,                               /* free_font */
    close_display,                      /* close_display */
    NULL,                               /* wire_to_event */
    NULL,                               /* event_to_wire */
    NULL,                               /* error */
    NULL                                /* error_string */
};

static XEXT_GENERATE_FIND_DISPLAY (find_display, dpms_info,
				   dpms_extension_name,
                                   &dpms_extension_hooks, DPMSNumberEvents,
                                   NULL)

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, dpms_info)

/*****************************************************************************
 *                                                                           *
 *                  public routines                                          *
 *                                                                           *
 *****************************************************************************/

Bool
DPMSQueryExtension (Display *dpy, int *event_basep, int *error_basep)
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

Status
DPMSGetVersion(Display *dpy, int *major_versionp, int *minor_versionp)
{
    XExtDisplayInfo *info = find_display (dpy);
    xDPMSGetVersionReply	    rep;
    register xDPMSGetVersionReq  *req;

    DPMSCheckExtension (dpy, info, 0);

    LockDisplay (dpy);
    GetReq (DPMSGetVersion, req);
    req->reqType = info->codes->major_opcode;
    req->dpmsReqType = X_DPMSGetVersion;
    if (!_XReply (dpy, (xReply *) &rep, 0, xTrue)) {
	UnlockDisplay (dpy);
	SyncHandle ();
	return 0;
    }
    *major_versionp = rep.majorVersion;
    *minor_versionp = rep.minorVersion;
    UnlockDisplay (dpy);
    SyncHandle ();
    return 1;
}

Bool
DPMSCapable(Display *dpy)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xDPMSCapableReq *req;
    xDPMSCapableReply rep;

    DPMSCheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(DPMSCapable, req);
    req->reqType = info->codes->major_opcode;
    req->dpmsReqType = X_DPMSCapable;

    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return rep.capable;
}

Status
DPMSSetTimeouts(Display *dpy, CARD16 standby, CARD16 suspend, CARD16 off)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xDPMSSetTimeoutsReq *req;

    if ((off != 0)&&(off < suspend))
    {
	return BadValue;
    }
    if ((suspend != 0)&&(suspend < standby))
    {
	return BadValue;
    }

    DPMSCheckExtension (dpy, info, 0);
    LockDisplay(dpy);
    GetReq(DPMSSetTimeouts, req);
    req->reqType = info->codes->major_opcode;
    req->dpmsReqType = X_DPMSSetTimeouts;
    req->standby = standby;
    req->suspend = suspend;
    req->off = off;

    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

Bool
DPMSGetTimeouts(Display *dpy, CARD16 *standby, CARD16 *suspend, CARD16 *off)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xDPMSGetTimeoutsReq *req;
    xDPMSGetTimeoutsReply rep;

    DPMSCheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(DPMSGetTimeouts, req);
    req->reqType = info->codes->major_opcode;
    req->dpmsReqType = X_DPMSGetTimeouts;

    if (!_XReply(dpy, (xReply *)&rep, 0, xTrue)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    *standby = rep.standby;
    *suspend = rep.suspend;
    *off = rep.off;
    return 1;
}

Status
DPMSEnable(Display *dpy)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xDPMSEnableReq *req;

    DPMSCheckExtension (dpy, info, 0);
    LockDisplay(dpy);
    GetReq(DPMSEnable, req);
    req->reqType = info->codes->major_opcode;
    req->dpmsReqType = X_DPMSEnable;

    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

Status
DPMSDisable(Display *dpy)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xDPMSDisableReq *req;

    DPMSCheckExtension (dpy, info, 0);
    LockDisplay(dpy);
    GetReq(DPMSDisable, req);
    req->reqType = info->codes->major_opcode;
    req->dpmsReqType = X_DPMSDisable;

    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}


Status
DPMSForceLevel(Display *dpy, CARD16 level)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xDPMSForceLevelReq *req;

    DPMSCheckExtension (dpy, info, 0);

    if ((level != DPMSModeOn) &&
	(level != DPMSModeStandby) &&
	(level != DPMSModeSuspend) &&
	(level != DPMSModeOff))
	return BadValue;

    LockDisplay(dpy);
    GetReq(DPMSForceLevel, req);
    req->reqType = info->codes->major_opcode;
    req->dpmsReqType = X_DPMSForceLevel;
    req->level = level;

    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

Status
DPMSInfo(Display *dpy, CARD16 *power_level, BOOL *state)
{
    XExtDisplayInfo *info = find_display (dpy);
    register xDPMSInfoReq *req;
    xDPMSInfoReply rep;

    DPMSCheckExtension (dpy, info, 0);

    LockDisplay(dpy);
    GetReq(DPMSInfo, req);
    req->reqType = info->codes->major_opcode;
    req->dpmsReqType = X_DPMSInfo;

    if (!_XReply(dpy, (xReply *)&rep, 0, xTrue)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    *power_level = rep.power_level;
    *state = rep.state;
    return 1;
}



