/*****************************************************************************/
/*

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

*/
/**       Copyright 1988 by Evans & Sutherland Computer Corporation,        **/
/**                          Salt Lake City, Utah                           **/
/**                        Cambridge, Massachusetts                         **/
/**                                                                         **/
/**                           All Rights Reserved                           **/
/**                                                                         **/
/**    Permission to use, copy, modify, and distribute this software and    **/
/**    its documentation  for  any  purpose  and  without  fee is hereby    **/
/**    granted, provided that the above copyright notice appear  in  all    **/
/**    copies and that both  that  copyright  notice  and  this  permis-    **/
/**    sion  notice appear in supporting  documentation,  and  that  the    **/
/**    name of Evans & Sutherland not be used in advertising    **/
/**    in publicity pertaining to distribution of the  software  without    **/
/**    specific, written prior permission.                                  **/
/**                                                                         **/
/**    EVANS & SUTHERLAND DISCLAIMs ALL WARRANTIES WITH REGARD    **/
/**    TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES  OF  MERCHANT-    **/
/**    ABILITY  AND  FITNESS,  IN  NO  EVENT SHALL EVANS & SUTHERLAND    **/
/**    BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL  DAM-    **/
/**    AGES OR  ANY DAMAGES WHATSOEVER  RESULTING FROM LOSS OF USE, DATA    **/
/**    OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER    **/
/**    TORTIOUS ACTION, ARISING OUT OF OR IN  CONNECTION  WITH  THE  USE    **/
/**    OR PERFORMANCE OF THIS SOFTWARE.                                     **/
/*****************************************************************************/

/***********************************************************************
 *
 * twm event handler include file
 *
 * 17-Nov-87 Thomas E. LaStrange		File created
 *
 ***********************************************************************/

#ifndef _EVENTS_
#define _EVENTS_

#include "screen.h"
#include "twm.h"

typedef void (*event_proc) (void);

extern Time lastTimestamp;

#define LastTimestamp() lastTimestamp

extern void AutoRaiseWindow(TwmWindow *tmp);
extern void SetRaiseWindow(TwmWindow *tmp);
extern void InitEvents(void);
extern Bool StashEventTime(XEvent *ev);
extern Window WindowOfEvent(XEvent *e);
extern Bool DispatchEvent2(void);
extern Bool DispatchEvent(void);
extern void HandleEvents(void);
extern void HandleColormapNotify(void);
extern void HandleVisibilityNotify(void);
extern void HandleKeyPress(void);
extern void free_cwins(TwmWindow *tmp);
extern void HandlePropertyNotify(void);
extern void HandleClientMessage(void);
extern void HandleExpose(void);
extern void HandleDestroyNotify(void);
extern void HandleCreateNotify(void);
extern void HandleMapRequest(void);
extern void SimulateMapRequest(Window w);
extern void HandleMapNotify(void);
extern void HandleUnmapNotify(void);
extern void HandleMotionNotify(void);
extern void HandleButtonRelease(void);
extern void HandleButtonPress(void);
extern void HandleEnterNotify(void);
extern void HandleLeaveNotify(void);
extern void HandleConfigureRequest(void);
extern void HandleShapeNotify(void);
extern void HandleScreenChangeNotify(void);
extern void HandleUnknown(void);
extern int Transient(Window w, Window *propw);
extern ScreenInfo *FindScreenInfo(Window w);
extern void InstallWindowColormaps(int type, TwmWindow *tmp);
extern void InstallRootColormap(void);
extern void UninstallRootColormap(void);

extern event_proc EventHandler[];
extern Window DragWindow;
extern int origDragX;
extern int origDragY;
extern int DragX;
extern int DragY;
extern int DragWidth;
extern int DragHeight;
extern int CurrentDragX;
extern int CurrentDragY;

extern int ButtonPressed;
extern int Cancel;

extern int Context;

extern XEvent Event;

extern unsigned int mods_used;

extern int MovedFromKeyPress;

#endif                          /* _EVENTS_ */
