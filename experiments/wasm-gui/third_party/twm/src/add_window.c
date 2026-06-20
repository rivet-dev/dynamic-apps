/*****************************************************************************/
/*

Copyright 1989,1998  The Open Group

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

/**********************************************************************
 *
 * Add a new window, put the titlebar and other stuff around
 * the window
 *
 * 31-Mar-88 Tom LaStrange        Initial Version.
 *
 **********************************************************************/

#include <stdio.h>
#include "twm.h"
#include <X11/Xatom.h>
#include "util.h"
#include "resize.h"
#include "parse.h"
#include "list.h"
#include "events.h"
#include "menus.h"
#include "screen.h"
#include "iconmgr.h"
#include "session.h"
#include "add_window.h"

#define short_lookup (short)(long)(void*)

#define gray_width 2
#define gray_height 2
static char gray_bits[] = {
    0x02, 0x01
};

int AddingX;
int AddingY;
int AddingW;
int AddingH;

static int PlaceX = 50;
static int PlaceY = 50;

static void do_add_binding(int button, int context, int modifier, int func);
static Window CreateHighlightWindow(TwmWindow *tmp_win);
static void CreateWindowTitlebarButtons(TwmWindow *tmp_win);

char NoName[] = "Untitled";     /* name if no name is specified */

/**
 * map gravity to (x,y) offset signs for adding to x and y when window is
 * mapped to get proper placement.
 *
 *  \param tmp    window from which to get gravity
 *  \param xp,yp  return values
 *
 */
void
GetGravityOffsets(TwmWindow *tmp, int *xp, int *yp)
{
    /* *INDENT-OFF* */
    static struct _gravity_offset {
        int x, y;
    } gravity_offsets[11] = {
        {  0,  0 },                     /* ForgetGravity */
        { -1, -1 },                     /* NorthWestGravity */
        {  0, -1 },                     /* NorthGravity */
        {  1, -1 },                     /* NorthEastGravity */
        { -1,  0 },                     /* WestGravity */
        {  0,  0 },                     /* CenterGravity */
        {  1,  0 },                     /* EastGravity */
        { -1,  1 },                     /* SouthWestGravity */
        {  0,  1 },                     /* SouthGravity */
        {  1,  1 },                     /* SouthEastGravity */
        {  0,  0 },                     /* StaticGravity */
    };
    /* *INDENT-ON* */
    register int g = ((tmp->hints.flags & PWinGravity)
                      ? tmp->hints.win_gravity : NorthWestGravity);

    if (g < ForgetGravity || g > StaticGravity) {
        *xp = *yp = 0;
    }
    else {
        *xp = gravity_offsets[g].x;
        *yp = gravity_offsets[g].y;
    }
}

/**
 * add a new window to the twm list.
 *
 *  \return  pointer to the TwmWindow structure
 *
 *      \param w      the window id of the window to add
 *      \param iconm  flag to tell if this is an icon manager window
 *      \param iconp  pointer to icon manager struct
 */
TwmWindow *
AddWindow(Window w, int iconm, IconMgr *iconp)
{
    TwmWindow *tmp_win;         /* new twm window structure */
    XEvent event;
    unsigned long valuemask;    /* mask for create windows */
    XSetWindowAttributes attributes;    /* attributes for create windows */
    int ask_user;               /* don't know where to put the window */
    int gravx, gravy;           /* gravity signs for positioning */
    int namelen;
    int bw2;
    short saved_x, saved_y, restore_icon_x, restore_icon_y;
    unsigned short saved_width, saved_height;
    Bool restore_iconified = 0;
    Bool restore_icon_info_present = 0;
    int restoredFromPrevSession;
    Bool width_ever_changed_by_user;
    Bool height_ever_changed_by_user;
    char *name;

#ifdef DEBUG
    fprintf(stderr, "AddWindow: w = 0x%lx\n", (unsigned long) w);
#endif

    /* allocate space for the twm window */
    tmp_win = calloc(1, sizeof(TwmWindow));
    if (tmp_win == NULL) {
        twmWarning("Unable to allocate memory to manage window ID %lx.", w);
        return NULL;
    }
    tmp_win->w = w;
    tmp_win->zoomed = ZOOM_NONE;
    tmp_win->iconmgr = (short) iconm;
    tmp_win->iconmgrp = iconp;
    tmp_win->cmaps.number_cwins = 0;

    XSelectInput(dpy, tmp_win->w, PropertyChangeMask);

    XGetWindowAttributes(dpy, tmp_win->w, &tmp_win->attr);

    if (!I18N_FetchName(dpy, tmp_win->w, &name))
        name = NULL;
    tmp_win->class = NoClass;
    XGetClassHint(dpy, tmp_win->w, &tmp_win->class);
    FetchWmProtocols(tmp_win);
    FetchWmColormapWindows(tmp_win);

    if (name == NULL)
        tmp_win->name = strdup(NoName);
    else {
        tmp_win->name = strdup(name);
        free(name);
    }

    if (GetWindowConfig(tmp_win,
                        &saved_x, &saved_y, &saved_width, &saved_height,
                        &restore_iconified, &restore_icon_info_present,
                        &restore_icon_x, &restore_icon_y,
                        &width_ever_changed_by_user,
                        &height_ever_changed_by_user)) {
        tmp_win->attr.x = saved_x;
        tmp_win->attr.y = saved_y;

        tmp_win->widthEverChangedByUser = width_ever_changed_by_user;
        tmp_win->heightEverChangedByUser = height_ever_changed_by_user;

        if (width_ever_changed_by_user)
            tmp_win->attr.width = saved_width;

        if (height_ever_changed_by_user)
            tmp_win->attr.height = saved_height;

        restoredFromPrevSession = 1;
    }
    else {
        tmp_win->widthEverChangedByUser = False;
        tmp_win->heightEverChangedByUser = False;

        restoredFromPrevSession = 0;
    }

    /*
     * do initial clip; should look at window gravity
     */
    if (tmp_win->attr.width > Scr->MaxWindowWidth)
        tmp_win->attr.width = Scr->MaxWindowWidth;
    if (tmp_win->attr.height > Scr->MaxWindowHeight)
        tmp_win->attr.height = Scr->MaxWindowHeight;

    tmp_win->wmhints = XGetWMHints(dpy, tmp_win->w);

    if (tmp_win->wmhints) {
        if (restore_iconified) {
            tmp_win->wmhints->initial_state = IconicState;
            tmp_win->wmhints->flags |= StateHint;
        }

        if (restore_icon_info_present) {
            tmp_win->wmhints->icon_x = restore_icon_x;
            tmp_win->wmhints->icon_y = restore_icon_y;
            tmp_win->wmhints->flags |= IconPositionHint;
        }
    }

    if (tmp_win->wmhints && (tmp_win->wmhints->flags & WindowGroupHint))
        tmp_win->group = tmp_win->wmhints->window_group;
    else
        tmp_win->group = tmp_win->w /* NULL */ ;

    /*
     * The July 27, 1988 draft of the ICCCM ignores the size and position
     * fields in the WM_NORMAL_HINTS property.
     */

    tmp_win->transient = (short) Transient(tmp_win->w, &tmp_win->transientfor);

    tmp_win->nameChanged = 0;
    if (tmp_win->class.res_name == NULL)
        tmp_win->class.res_name = NoName;
    if (tmp_win->class.res_class == NULL)
        tmp_win->class.res_class = NoName;

    tmp_win->full_name = strdup(tmp_win->name);
    namelen = (int) strlen(tmp_win->name);

    tmp_win->highlight = Scr->Highlight &&
        (!short_lookup LookInList(Scr->NoHighlight, tmp_win->full_name,
                                  &tmp_win->class));

    tmp_win->stackmode = Scr->StackMode &&
        (!short_lookup LookInList(Scr->NoStackModeL, tmp_win->full_name,
                                  &tmp_win->class));

    tmp_win->titlehighlight = Scr->TitleHighlight &&
        (!short_lookup LookInList(Scr->NoTitleHighlight, tmp_win->full_name,
                                  &tmp_win->class));

    tmp_win->auto_raise = short_lookup LookInList(Scr->AutoRaise,
                                                  tmp_win->full_name,
                                                  &tmp_win->class);
    if (tmp_win->auto_raise)
        Scr->NumAutoRaises++;
    if (Scr->IconifyByUnmapping) {
        tmp_win->iconify_by_unmapping = iconm ? FALSE :
            !short_lookup LookInList(Scr->DontIconify, tmp_win->full_name,
                                     &tmp_win->class);
    }
    else {
        tmp_win->iconify_by_unmapping = Scr->IconifyByUnmapping;
    }
    tmp_win->iconify_by_unmapping |=
        short_lookup LookInList(Scr->IconifyByUn, tmp_win->full_name,
                                &tmp_win->class);

    if (LookInList(Scr->WindowRingL, tmp_win->full_name, &tmp_win->class)) {
        if (Scr->Ring) {
            tmp_win->ring.next = Scr->Ring->ring.next;
            if (Scr->Ring->ring.next->ring.prev)
                Scr->Ring->ring.next->ring.prev = tmp_win;
            Scr->Ring->ring.next = tmp_win;
            tmp_win->ring.prev = Scr->Ring;
        }
        else {
            tmp_win->ring.next = tmp_win->ring.prev = Scr->Ring = tmp_win;
        }
    }
    else
        tmp_win->ring.next = tmp_win->ring.prev = NULL;
    tmp_win->ring.cursor_valid = False;

    tmp_win->squeeze_info = NULL;
    /*
     * get the squeeze information; note that this does not have to be freed
     * since it is coming from the screen list
     */
    if (HasShape) {
        if (!LookInList(Scr->DontSqueezeTitleL, tmp_win->full_name,
                        &tmp_win->class)) {
            tmp_win->squeeze_info = (SqueezeInfo *)
                LookInList(Scr->SqueezeTitleL, tmp_win->full_name,
                           &tmp_win->class);
            if (!tmp_win->squeeze_info) {
                static SqueezeInfo default_squeeze = { J_LEFT, 0, 0 };
                if (Scr->SqueezeTitle)
                    tmp_win->squeeze_info = &default_squeeze;
            }
        }
    }

    tmp_win->old_bw = tmp_win->attr.border_width;

    if (Scr->ClientBorderWidth) {
        tmp_win->frame_bw = tmp_win->old_bw;
    }
    else {
        tmp_win->frame_bw = Scr->BorderWidth;
    }
    bw2 = tmp_win->frame_bw * 2;

    tmp_win->title_height = Scr->TitleHeight + tmp_win->frame_bw;
    if (Scr->NoTitlebar)
        tmp_win->title_height = 0;
    if (LookInList(Scr->MakeTitle, tmp_win->full_name, &tmp_win->class))
        tmp_win->title_height = Scr->TitleHeight + tmp_win->frame_bw;
    if (LookInList(Scr->NoTitle, tmp_win->full_name, &tmp_win->class))
        tmp_win->title_height = 0;

    /* if it is a transient window, don't put a title on it */
    if (tmp_win->transient && !Scr->DecorateTransients)
        tmp_win->title_height = 0;

    if (LookInList(Scr->StartIconified, tmp_win->full_name, &tmp_win->class)) {
        if (!tmp_win->wmhints) {
            tmp_win->wmhints = malloc(sizeof(XWMHints));
            tmp_win->wmhints->flags = 0;
        }
        tmp_win->wmhints->initial_state = IconicState;
        tmp_win->wmhints->flags |= StateHint;
    }

    GetWindowSizeHints(tmp_win);

    if (restoredFromPrevSession) {
        /*
         * When restoring window positions from the previous session,
         * we always use NorthWest gravity.
         */

        gravx = gravy = -1;
    }
    else {
        GetGravityOffsets(tmp_win, &gravx, &gravy);
    }

    /*
     * Don't bother user if:
     *
     *     o  the window is a transient, or
     *
     *     o  a USPosition was requested, or
     *
     *     o  a PPosition was requested and UsePPosition is ON or
     *        NON_ZERO if the window is at other than (0,0)
     */
    ask_user = TRUE;
    if (tmp_win->transient ||
        (tmp_win->hints.flags & USPosition) ||
        ((tmp_win->hints.flags & PPosition) && Scr->UsePPosition &&
         (Scr->UsePPosition == PPOS_ON ||
          tmp_win->attr.x != 0 || tmp_win->attr.y != 0)))
        ask_user = FALSE;

    /*
     * do any prompting for position
     */
    if (HandlingEvents && ask_user && !restoredFromPrevSession) {
        if (Scr->RandomPlacement) {     /* just stick it somewhere */
            /* Avoid putting the new window off-screen */
            if ((PlaceX + tmp_win->attr.width) > Scr->MyDisplayWidth) {
                PlaceX = Scr->MyDisplayWidth - tmp_win->attr.width;
                if (PlaceX < 0)
                    PlaceX = 0;
                if (PlaceX > 50)
                    PlaceX = 50;
            }
            if ((PlaceY + tmp_win->attr.height) > Scr->MyDisplayHeight) {
                PlaceY = Scr->MyDisplayHeight - tmp_win->attr.height;
                if (PlaceY < 0)
                    PlaceY = 0;
                if (PlaceY > 50)
                    PlaceY = 50;
            }

            tmp_win->attr.x = PlaceX;
            tmp_win->attr.y = PlaceY;
            PlaceX += 30;
            PlaceY += 30;
        }
        else {                  /* else prompt */
            if (!(tmp_win->wmhints && tmp_win->wmhints->flags & StateHint &&
                  tmp_win->wmhints->initial_state == IconicState)) {
                Bool firsttime = True;
                int height, width;

                /* better wait until all the mouse buttons have been
                 * released.
                 */
                while (TRUE) {
                    int stat;

                    XUngrabServer(dpy);
                    XSync(dpy, 0);
                    XGrabServer(dpy);

                    JunkMask = 0;
                    if (!XQueryPointer(dpy, Scr->Root, &JunkRoot,
                                       &JunkChild, &JunkX, &JunkY,
                                       &AddingX, &AddingY, &JunkMask))
                        JunkMask = 0;

                    JunkMask &= (Button1Mask | Button2Mask | Button3Mask |
                                 Button4Mask | Button5Mask);

                    /*
                     * watch out for changing screens
                     */
                    if (firsttime) {
                        if (JunkRoot != Scr->Root) {
                            register int scrnum;

                            for (scrnum = 0; scrnum < NumScreens; scrnum++) {
                                if (JunkRoot == RootWindow(dpy, scrnum))
                                    break;
                            }

                            if (scrnum != NumScreens)
                                PreviousScreen = scrnum;
                        }
                        firsttime = False;
                    }

                    /*
                     * wait for buttons to come up; yuck
                     */
                    if (JunkMask != 0)
                        continue;

                    /*
                     * this will cause a warp to the indicated root
                     */
                    stat = XGrabPointer(dpy, Scr->Root, False,
                                        ButtonPressMask | ButtonReleaseMask |
                                        PointerMotionMask |
                                        PointerMotionHintMask, GrabModeAsync,
                                        GrabModeAsync, Scr->Root,
                                        UpperLeftCursor, CurrentTime);

                    if (stat == GrabSuccess)
                        break;
                }

                width = (SIZE_HINDENT + MyFont_TextWidth(&Scr->SizeFont,
                                                         tmp_win->name,
                                                         namelen));
                height = Scr->SizeFont.height + SIZE_VINDENT * 2;

                XResizeWindow(dpy, Scr->SizeWindow,
                              (unsigned) (width + SIZE_HINDENT),
                              (unsigned) height);
                XMapRaised(dpy, Scr->SizeWindow);
                InstallRootColormap();

                MyFont_ChangeGC(Scr->DefaultC.fore, Scr->DefaultC.back,
                                &Scr->SizeFont);
                MyFont_DrawImageString(dpy, Scr->SizeWindow, &Scr->SizeFont,
                                       Scr->NormalGC,
                                       SIZE_HINDENT,
                                       SIZE_VINDENT + Scr->SizeFont.ascent,
                                       tmp_win->name, namelen);

                AddingW = tmp_win->attr.width + bw2;
                AddingH = tmp_win->attr.height + tmp_win->title_height + bw2;

                if (Scr->DontMoveOff) {
                    /*
                     * Make sure the initial outline comes up on the screen.
                     */
                    if (AddingX < 0)
                        AddingX = 0;
                    if (AddingX > Scr->MyDisplayWidth - AddingW)
                        AddingX = Scr->MyDisplayWidth - AddingW;

                    if (AddingY < 0)
                        AddingY = 0;
                    if (AddingY > Scr->MyDisplayHeight - AddingH)
                        AddingY = Scr->MyDisplayHeight - AddingH;
                }

                MoveOutline(Scr->Root, AddingX, AddingY, AddingW, AddingH,
                            tmp_win->frame_bw, tmp_win->title_height);

                while (TRUE) {
                    XMaskEvent(dpy, ButtonPressMask | PointerMotionMask,
                               &event);

                    if (Event.type == MotionNotify) {
                        /* discard any extra motion events before a release */
                        while (XCheckMaskEvent(dpy,
                                               ButtonMotionMask |
                                               ButtonPressMask, &Event))
                            if (Event.type == ButtonPress)
                                break;
                    }

                    if (event.type == ButtonPress) {
                        AddingX = event.xbutton.x_root;
                        AddingY = event.xbutton.y_root;

                        /* DontMoveOff prohibits user form off-screen placement */
                        if (Scr->DontMoveOff) {
                            int AddingR, AddingB;

                            AddingR = AddingX + AddingW;
                            AddingB = AddingY + AddingH;

                            if (AddingX < 0)
                                AddingX = 0;
                            if (AddingR > Scr->MyDisplayWidth)
                                AddingX = Scr->MyDisplayWidth - AddingW;

                            if (AddingY < 0)
                                AddingY = 0;
                            if (AddingB > Scr->MyDisplayHeight)
                                AddingY = Scr->MyDisplayHeight - AddingH;

                        }
                        break;
                    }

                    if (event.type != MotionNotify) {
                        continue;
                    }

                    XQueryPointer(dpy, Scr->Root, &JunkRoot, &JunkChild,
                                  &JunkX, &JunkY, &AddingX, &AddingY,
                                  &JunkMask);

                    if (Scr->DontMoveOff) {
                        int AddingR, AddingB;

                        AddingR = AddingX + AddingW;
                        AddingB = AddingY + AddingH;

                        if (AddingX < 0)
                            AddingX = 0;
                        if (AddingR > Scr->MyDisplayWidth)
                            AddingX = Scr->MyDisplayWidth - AddingW;

                        if (AddingY < 0)
                            AddingY = 0;
                        if (AddingB > Scr->MyDisplayHeight)
                            AddingY = Scr->MyDisplayHeight - AddingH;
                    }

                    MoveOutline(Scr->Root, AddingX, AddingY, AddingW, AddingH,
                                tmp_win->frame_bw, tmp_win->title_height);

                }

                if (event.xbutton.button == Button2) {
                    int lastx, lasty;

                    Scr->SizeStringOffset = width +
                        MyFont_TextWidth(&Scr->SizeFont, ": ", 2);
                    XResizeWindow(dpy, Scr->SizeWindow,
                                  (unsigned) (Scr->SizeStringOffset +
                                              Scr->SizeStringWidth),
                                  (unsigned) height);
                    MyFont_DrawImageString(dpy, Scr->SizeWindow, &Scr->SizeFont,
                                           Scr->NormalGC, width,
                                           SIZE_VINDENT + Scr->SizeFont.ascent,
                                           ": ", 2);
                    if (0 /*Scr->AutoRelativeResize */ ) {
                        int dx = (tmp_win->attr.width / 4);
                        int dy = (tmp_win->attr.height / 4);

#define HALF_AVE_CURSOR_SIZE 8  /* so that it is visible */
                        if (dx < HALF_AVE_CURSOR_SIZE)
                            dx = HALF_AVE_CURSOR_SIZE;
                        if (dy < HALF_AVE_CURSOR_SIZE)
                            dy = HALF_AVE_CURSOR_SIZE;
#undef HALF_AVE_CURSOR_SIZE
                        dx += (tmp_win->frame_bw + 1);
                        dy += (bw2 + tmp_win->title_height + 1);
                        if (AddingX + dx >= Scr->MyDisplayWidth)
                            dx = Scr->MyDisplayWidth - AddingX - 1;
                        if (AddingY + dy >= Scr->MyDisplayHeight)
                            dy = Scr->MyDisplayHeight - AddingY - 1;
                        if (dx > 0 && dy > 0)
                            XWarpPointer(dpy, None, None, 0, 0, 0, 0, dx, dy);
                    }
                    else {
                        XWarpPointer(dpy, None, Scr->Root, 0, 0, 0, 0,
                                     AddingX + AddingW / 2,
                                     AddingY + AddingH / 2);
                    }
                    AddStartResize(tmp_win, AddingX, AddingY, AddingW, AddingH);

                    lastx = -10000;
                    lasty = -10000;
                    while (TRUE) {
                        XMaskEvent(dpy,
                                   ButtonReleaseMask | ButtonMotionMask,
                                   &event);

                        if (Event.type == MotionNotify) {
                            /* discard any extra motion events before a release */
                            while (XCheckMaskEvent(dpy,
                                                   ButtonMotionMask |
                                                   ButtonReleaseMask, &Event))
                                if (Event.type == ButtonRelease)
                                    break;
                        }

                        if (event.type == ButtonRelease) {
                            AddEndResize(tmp_win);
                            break;
                        }

                        if (event.type != MotionNotify) {
                            continue;
                        }

                        /*
                         * XXX - if we are going to do a loop, we ought to consider
                         * using multiple GXxor lines so that we don't need to
                         * grab the server.
                         */
                        XQueryPointer(dpy, Scr->Root, &JunkRoot, &JunkChild,
                                      &JunkX, &JunkY, &AddingX, &AddingY,
                                      &JunkMask);

                        if (lastx != AddingX || lasty != AddingY) {
                            DoResize(AddingX, AddingY, tmp_win);

                            lastx = AddingX;
                            lasty = AddingY;
                        }

                    }
                }
                else if (event.xbutton.button == Button3) {
                    int maxw = Scr->MyDisplayWidth - AddingX;
                    int maxh = Scr->MyDisplayHeight - AddingY;

                    /*
                     * Make window go to bottom of screen, and clip to right edge.
                     * This is useful when popping up large windows and fixed
                     * column text windows.
                     */
                    if (AddingW > maxw)
                        AddingW = maxw;
                    AddingH = maxh;

                    AddingW -= bw2;
                    AddingH -= bw2;
                    ConstrainSize(tmp_win, &AddingW, &AddingH); /* w/o borders */
                    AddingW += bw2;
                    AddingH += bw2;
                }
                else {
                    XMaskEvent(dpy, ButtonReleaseMask, &event);
                }

                MoveOutline(Scr->Root, 0, 0, 0, 0, 0, 0);
                XUnmapWindow(dpy, Scr->SizeWindow);
                UninstallRootColormap();
                XUngrabPointer(dpy, CurrentTime);

                tmp_win->attr.x = AddingX;
                tmp_win->attr.y = AddingY + tmp_win->title_height;
                tmp_win->attr.width = AddingW - bw2;
                tmp_win->attr.height = AddingH - tmp_win->title_height - bw2;

                XUngrabServer(dpy);
            }
        }
    }
    else {                      /* put it where asked, mod title bar */
        /* if the gravity is towards the top, move it by the title height */
        if (gravy < 0)
            tmp_win->attr.y -= gravy * tmp_win->title_height;
    }

#ifdef DEBUG
    fprintf(stderr, "  position window  %d, %d  %dx%d\n",
            tmp_win->attr.x,
            tmp_win->attr.y, tmp_win->attr.width, tmp_win->attr.height);
#endif

    if (!Scr->ClientBorderWidth) {      /* need to adjust for twm borders */
        int delta = tmp_win->attr.border_width - tmp_win->frame_bw;

        tmp_win->attr.x += gravx * delta;
        tmp_win->attr.y += gravy * delta;
    }

    tmp_win->title_width = tmp_win->attr.width;

    if (tmp_win->old_bw)
        XSetWindowBorderWidth(dpy, tmp_win->w, 0);

    tmp_win->name_width = MyFont_TextWidth(&Scr->TitleBarFont, tmp_win->name,
                                           namelen);

    if (!I18N_GetIconName(dpy, tmp_win->w, &name)) {
        tmp_win->icon_name = strdup(tmp_win->name);
    }
    else {
        if (name == NULL) {
            tmp_win->icon_name = strdup(tmp_win->name);
        }
        else {
            tmp_win->icon_name = strdup(name);
            free(name);
        }
    }

    tmp_win->iconified = FALSE;
    tmp_win->icon = FALSE;
    tmp_win->icon_on = FALSE;

    XGrabServer(dpy);

    /*
     * Make sure the client window still exists.  We don't want to leave an
     * orphan frame window if it doesn't.  Since we now have the server
     * grabbed, the window can't disappear later without having been
     * reparented, so we'll get a DestroyNotify for it.  We won't have
     * gotten one for anything up to here, however.
     */
    if (XGetGeometry(dpy, tmp_win->w, &JunkRoot, &JunkX, &JunkY,
                     &JunkWidth, &JunkHeight, &JunkBW, &JunkDepth) == 0) {
        free(tmp_win);
        XUngrabServer(dpy);
        return (NULL);
    }

    /* add the window into the twm list */
    tmp_win->next = Scr->TwmRoot.next;
    if (Scr->TwmRoot.next != NULL)
        Scr->TwmRoot.next->prev = tmp_win;
    tmp_win->prev = &Scr->TwmRoot;
    Scr->TwmRoot.next = tmp_win;

    /* get all the colors for the window */

    tmp_win->border = Scr->BorderColor;
    tmp_win->icon_border = Scr->IconBorderColor;
    tmp_win->border_tile.fore = Scr->BorderTileC.fore;
    tmp_win->border_tile.back = Scr->BorderTileC.back;
    tmp_win->title.fore = Scr->TitleC.fore;
    tmp_win->title.back = Scr->TitleC.back;
    tmp_win->iconc.fore = Scr->IconC.fore;
    tmp_win->iconc.back = Scr->IconC.back;

    GetColorFromList(Scr->BorderColorL, tmp_win->full_name, &tmp_win->class,
                     &tmp_win->border);
    GetColorFromList(Scr->IconBorderColorL, tmp_win->full_name, &tmp_win->class,
                     &tmp_win->icon_border);
    GetColorFromList(Scr->BorderTileForegroundL, tmp_win->full_name,
                     &tmp_win->class, &tmp_win->border_tile.fore);
    GetColorFromList(Scr->BorderTileBackgroundL, tmp_win->full_name,
                     &tmp_win->class, &tmp_win->border_tile.back);
    GetColorFromList(Scr->TitleForegroundL, tmp_win->full_name, &tmp_win->class,
                     &tmp_win->title.fore);
    GetColorFromList(Scr->TitleBackgroundL, tmp_win->full_name, &tmp_win->class,
                     &tmp_win->title.back);
    GetColorFromList(Scr->IconForegroundL, tmp_win->full_name, &tmp_win->class,
                     &tmp_win->iconc.fore);
    GetColorFromList(Scr->IconBackgroundL, tmp_win->full_name, &tmp_win->class,
                     &tmp_win->iconc.back);

    /* create windows */

    tmp_win->frame_x = tmp_win->attr.x + tmp_win->old_bw - tmp_win->frame_bw;
    tmp_win->frame_y = tmp_win->attr.y - tmp_win->title_height +
        tmp_win->old_bw - tmp_win->frame_bw;
    tmp_win->frame_width = tmp_win->attr.width;
    tmp_win->frame_height = tmp_win->attr.height + tmp_win->title_height;

    valuemask = CWBackPixmap | CWBorderPixel | CWCursor | CWEventMask;
    attributes.background_pixmap = None;
    attributes.border_pixel = tmp_win->border;
    attributes.cursor = Scr->FrameCursor;
    attributes.event_mask = (SubstructureRedirectMask |
                             ButtonPressMask | ButtonReleaseMask |
                             EnterWindowMask | LeaveWindowMask);
    if (tmp_win->attr.save_under) {
        attributes.save_under = True;
        valuemask |= CWSaveUnder;
    }

    tmp_win->frame = XCreateWindow(dpy, Scr->Root, tmp_win->frame_x,
                                   tmp_win->frame_y,
                                   (unsigned int) tmp_win->frame_width,
                                   (unsigned int) tmp_win->frame_height,
                                   (unsigned int) tmp_win->frame_bw,
                                   Scr->d_depth,
                                   (unsigned int) CopyFromParent,
                                   Scr->d_visual, valuemask, &attributes);

    if (tmp_win->title_height) {
        valuemask = (CWEventMask | CWBorderPixel | CWBackPixel);
        attributes.event_mask = (KeyPressMask | ButtonPressMask |
                                 ButtonReleaseMask | ExposureMask);
        attributes.border_pixel = tmp_win->border;
        attributes.background_pixel = tmp_win->title.back;
        tmp_win->title_w = XCreateWindow(dpy, tmp_win->frame,
                                         -tmp_win->frame_bw,
                                         -tmp_win->frame_bw,
                                         (unsigned int) tmp_win->attr.width,
                                         (unsigned int) Scr->TitleHeight,
                                         (unsigned int) tmp_win->frame_bw,
                                         Scr->d_depth,
                                         (unsigned int) CopyFromParent,
                                         Scr->d_visual, valuemask, &attributes);
    }
    else {
        tmp_win->title_w = 0;
        tmp_win->squeeze_info = NULL;
    }

    if (tmp_win->highlight) {
        tmp_win->gray = XCreatePixmapFromBitmapData(dpy, Scr->Root,
                                                    gray_bits, gray_width,
                                                    gray_height,
                                                    tmp_win->border_tile.fore,
                                                    tmp_win->border_tile.back,
                                                    (unsigned) Scr->d_depth);

        SetBorder(tmp_win, False);
    }
    else
        tmp_win->gray = None;

    if (tmp_win->title_w) {
        CreateWindowTitlebarButtons(tmp_win);
        ComputeTitleLocation(tmp_win);
        XMoveWindow(dpy, tmp_win->title_w, tmp_win->title_x, tmp_win->title_y);
        XDefineCursor(dpy, tmp_win->title_w, Scr->TitleCursor);
    }

    valuemask = (CWEventMask | CWDontPropagate);
    attributes.event_mask = (StructureNotifyMask | PropertyChangeMask |
                             ColormapChangeMask | VisibilityChangeMask |
                             EnterWindowMask | LeaveWindowMask);
    attributes.do_not_propagate_mask = ButtonPressMask | ButtonReleaseMask;
    XChangeWindowAttributes(dpy, tmp_win->w, valuemask, &attributes);

    if (HasShape)
        XShapeSelectInput(dpy, tmp_win->w, ShapeNotifyMask);

    if (tmp_win->title_w) {
        XMapWindow(dpy, tmp_win->title_w);
    }

    if (HasShape) {
        int xws, yws, xbs, ybs;
        unsigned wws, hws, wbs, hbs;
        int boundingShaped, clipShaped;

        XShapeSelectInput(dpy, tmp_win->w, ShapeNotifyMask);
        XShapeQueryExtents(dpy, tmp_win->w,
                           &boundingShaped, &xws, &yws, &wws, &hws,
                           &clipShaped, &xbs, &ybs, &wbs, &hbs);
        tmp_win->wShaped = (short) boundingShaped;
    }

    if (!tmp_win->iconmgr)
        XAddToSaveSet(dpy, tmp_win->w);

    XReparentWindow(dpy, tmp_win->w, tmp_win->frame, 0, tmp_win->title_height);
    /*
     * Reparenting generates an UnmapNotify event, followed by a MapNotify.
     * Set the map state to FALSE to prevent a transition back to
     * WithdrawnState in HandleUnmapNotify.  Map state gets set correctly
     * again in HandleMapNotify.
     */
    tmp_win->mapped = FALSE;

    SetupFrame(tmp_win, tmp_win->frame_x, tmp_win->frame_y,
               tmp_win->frame_width, tmp_win->frame_height, -1, True);

    /* wait until the window is iconified and the icon window is mapped
     * before creating the icon window
     */
    tmp_win->icon_w = (Window) 0;

    if (!tmp_win->iconmgr) {
        GrabButtons(tmp_win);
        GrabKeys(tmp_win);
    }

    (void) AddIconManager(tmp_win);

    XSaveContext(dpy, tmp_win->w, TwmContext, (XPointer) tmp_win);
    XSaveContext(dpy, tmp_win->w, ScreenContext, (XPointer) Scr);
    XSaveContext(dpy, tmp_win->frame, TwmContext, (XPointer) tmp_win);
    XSaveContext(dpy, tmp_win->frame, ScreenContext, (XPointer) Scr);
    if (tmp_win->title_height) {
        int i;
        int nb = Scr->TBInfo.nleft + Scr->TBInfo.nright;

        XSaveContext(dpy, tmp_win->title_w, TwmContext, (XPointer) tmp_win);
        XSaveContext(dpy, tmp_win->title_w, ScreenContext, (XPointer) Scr);
        for (i = 0; i < nb; i++) {
            XSaveContext(dpy, tmp_win->titlebuttons[i].window, TwmContext,
                         (XPointer) tmp_win);
            XSaveContext(dpy, tmp_win->titlebuttons[i].window, ScreenContext,
                         (XPointer) Scr);
        }
        if (tmp_win->hilite_w) {
            XSaveContext(dpy, tmp_win->hilite_w, TwmContext,
                         (XPointer) tmp_win);
            XSaveContext(dpy, tmp_win->hilite_w, ScreenContext, (XPointer) Scr);
        }
    }

    XUngrabServer(dpy);

    /* if we were in the middle of a menu activated function, regrab
     * the pointer
     */
    if (RootFunction)
        ReGrab();

    return (tmp_win);
}

/**
 * checks to see if we should really put a twm frame on the window
 *
 *  \return TRUE  - go ahead and place the window
 *  \return FALSE - don't frame the window
 *      \param w the window to check
 */
int
MappedNotOverride(Window w)
{
    XWindowAttributes wa;

    XGetWindowAttributes(dpy, w, &wa);
    return ((wa.map_state != IsUnmapped) && (wa.override_redirect != True));
}

/**
 * attach default bindings so that naive users don't get messed up if they
 * provide a minimal twmrc.
 */
static void
do_add_binding(int button, int context, int modifier, int func)
{
    MouseButton *mb = &Scr->Mouse[button][context][modifier];

    if (mb->func)
        return;                 /* already defined */

    mb->func = func;
    mb->item = NULL;
}

void
AddDefaultBindings(void)
{
    /*
     * The bindings are stored in Scr->Mouse, indexed by
     * Mouse[button_number][C_context][modifier].
     */

#define NoModifierMask 0

    do_add_binding(Button1, C_TITLE, NoModifierMask, F_MOVE);
    do_add_binding(Button1, C_ICON, NoModifierMask, F_ICONIFY);
    do_add_binding(Button1, C_ICONMGR, NoModifierMask, F_ICONIFY);

    do_add_binding(Button2, C_TITLE, NoModifierMask, F_RAISELOWER);
    do_add_binding(Button2, C_ICON, NoModifierMask, F_ICONIFY);
    do_add_binding(Button2, C_ICONMGR, NoModifierMask, F_ICONIFY);

#undef NoModifierMask
}

/**
 * grab needed buttons for the window
 *
 *  \param[in] tmp_win the twm window structure to use
 */
void
GrabButtons(TwmWindow *tmp_win)
{
    int i, j;

    for (i = 0; i < MAX_BUTTONS + 1; i++) {
        for (j = 0; j < MOD_SIZE; j++) {
            if (Scr->Mouse[i][C_WINDOW][j].func != 0) {
                /* twm used to do this grab on the application main window,
                 * tmp_win->w . This was not ICCCM complient and was changed.
                 */
                XGrabButton(dpy, (unsigned) i, (unsigned) j, tmp_win->frame,
                            True, ButtonPressMask | ButtonReleaseMask,
                            GrabModeAsync, GrabModeAsync, None,
                            Scr->FrameCursor);
            }
        }
    }
}

/**
 * grab needed keys for the window
 *
 *  \param[in] tmp_win the twm window structure to use
 */
void
GrabKeys(TwmWindow *tmp_win)
{
    FuncKey *tmp;
    IconMgr *p;

    for (tmp = Scr->FuncKeyRoot.next; tmp != NULL; tmp = tmp->next) {
        switch (tmp->cont) {
        case C_WINDOW:
            XGrabKey(dpy, tmp->keycode, (unsigned) tmp->mods, tmp_win->w, True,
                     GrabModeAsync, GrabModeAsync);
            break;

        case C_ICON:
            if (tmp_win->icon_w)
                XGrabKey(dpy, tmp->keycode, (unsigned) tmp->mods,
                         tmp_win->icon_w, True, GrabModeAsync, GrabModeAsync);
            /* FALLTHRU */

        case C_TITLE:
            if (tmp_win->title_w)
                XGrabKey(dpy, tmp->keycode, (unsigned) tmp->mods,
                         tmp_win->title_w, True, GrabModeAsync, GrabModeAsync);
            break;

        case C_NAME:
            XGrabKey(dpy, tmp->keycode, (unsigned) tmp->mods, tmp_win->w, True,
                     GrabModeAsync, GrabModeAsync);
            if (tmp_win->icon_w)
                XGrabKey(dpy, tmp->keycode, (unsigned) tmp->mods,
                         tmp_win->icon_w, True, GrabModeAsync, GrabModeAsync);
            if (tmp_win->title_w)
                XGrabKey(dpy, tmp->keycode, (unsigned) tmp->mods,
                         tmp_win->title_w, True, GrabModeAsync, GrabModeAsync);
            break;
            /*
               case C_ROOT:
               XGrabKey(dpy, tmp->keycode, tmp->mods, Scr->Root, True,
               GrabModeAsync, GrabModeAsync);
               break;
             */
        }
    }
    for (tmp = Scr->FuncKeyRoot.next; tmp != NULL; tmp = tmp->next) {
        if (tmp->cont == C_ICONMGR && !Scr->NoIconManagers) {
            for (p = &Scr->iconmgr; p != NULL; p = p->next) {
                XUngrabKey(dpy, tmp->keycode, (unsigned) tmp->mods,
                           p->twm_win->w);
            }
        }
    }
}

static Window
CreateHighlightWindow(TwmWindow *tmp_win)
{
    XSetWindowAttributes attributes;    /* attributes for create windows */
    Pixmap pm = None;
    GC gc;
    XGCValues gcv;
    unsigned long valuemask;
    int h = (Scr->TitleHeight - 2 * Scr->FramePadding);
    Window w;

    /*
     * If a special highlight pixmap was given, use that.  Otherwise,
     * use a nice, even gray pattern.  The old horizontal lines look really
     * awful on interlaced monitors (as well as resembling other looks a
     * little bit too closely), but can be used by putting
     *
     *                 Pixmaps { TitleHighlight "hline2" }
     *
     * (or whatever the horizontal line bitmap is named) in the startup
     * file.  If all else fails, use the foreground color to look like a
     * solid line.
     */
    if (!Scr->hilitePm) {
        Scr->hilitePm = XCreateBitmapFromData(dpy, tmp_win->title_w,
                                              gray_bits, gray_width,
                                              gray_height);
        Scr->hilite_pm_width = gray_width;
        Scr->hilite_pm_height = gray_height;
    }
    if (Scr->hilitePm) {
        pm = XCreatePixmap(dpy, tmp_win->title_w,
                           (unsigned) Scr->hilite_pm_width,
                           (unsigned) Scr->hilite_pm_height,
                           (unsigned) Scr->d_depth);
        gcv.foreground = tmp_win->title.fore;
        gcv.background = tmp_win->title.back;
        gcv.graphics_exposures = False;
        gc = XCreateGC(dpy, pm,
                       (GCForeground | GCBackground | GCGraphicsExposures),
                       &gcv);
        if (gc) {
            XCopyPlane(dpy, Scr->hilitePm, pm, gc, 0, 0,
                       (unsigned) Scr->hilite_pm_width,
                       (unsigned) Scr->hilite_pm_height, 0, 0, 1);
            XFreeGC(dpy, gc);
        }
        else {
            XFreePixmap(dpy, pm);
            pm = None;
        }
    }
    if (pm) {
        valuemask = CWBackPixmap;
        attributes.background_pixmap = pm;
    }
    else {
        valuemask = CWBackPixel;
        attributes.background_pixel = tmp_win->title.fore;
    }

    w = XCreateWindow(dpy, tmp_win->title_w, 0, Scr->FramePadding,
                      (unsigned int) Scr->TBInfo.width, (unsigned int) h,
                      (unsigned int) 0,
                      Scr->d_depth, (unsigned int) CopyFromParent,
                      Scr->d_visual, valuemask, &attributes);
    if (pm)
        XFreePixmap(dpy, pm);
    return w;
}

void
ComputeCommonTitleOffsets(void)
{
    int buttonwidth = (Scr->TBInfo.width + Scr->TBInfo.pad);

    Scr->TBInfo.leftx = Scr->TBInfo.rightoff = Scr->FramePadding;
    if (Scr->TBInfo.nleft > 0)
        Scr->TBInfo.leftx += Scr->ButtonIndent;
    Scr->TBInfo.titlex = (Scr->TBInfo.leftx +
                          (Scr->TBInfo.nleft * buttonwidth) -
                          Scr->TBInfo.pad + Scr->TitlePadding);
    if (Scr->TBInfo.nright > 0)
        Scr->TBInfo.rightoff += (Scr->ButtonIndent +
                                 ((Scr->TBInfo.nright * buttonwidth) -
                                  Scr->TBInfo.pad));
    return;
}

void
ComputeWindowTitleOffsets(TwmWindow *tmp_win, int width, Bool squeeze)
{
    tmp_win->highlightx = (Scr->TBInfo.titlex + tmp_win->name_width);
    if (tmp_win->hilite_w || Scr->TBInfo.nright > 0)
        tmp_win->highlightx += Scr->TitlePadding;
    tmp_win->rightx = width - Scr->TBInfo.rightoff;
    if (squeeze && tmp_win->squeeze_info) {
        int rx = (tmp_win->highlightx +
                  (tmp_win->hilite_w
                   ? Scr->TBInfo.width * 2 : 0) +
                  (Scr->TBInfo.nright > 0 ? Scr->TitlePadding : 0) +
                  Scr->FramePadding);
        if (rx < tmp_win->rightx)
            tmp_win->rightx = rx;
    }
    return;
}

/**
 * calculate the position of the title window.  We need to take the frame_bw
 * into account since we want (0,0) of the title window to line up with (0,0)
 * of the frame window.
 */
void
ComputeTitleLocation(register TwmWindow *tmp)
{
    tmp->title_x = -tmp->frame_bw;
    tmp->title_y = -tmp->frame_bw;

    if (tmp->squeeze_info) {
        register SqueezeInfo *si = tmp->squeeze_info;
        int basex;
        int maxwidth = tmp->frame_width;
        int tw = tmp->title_width;

        /*
         * figure label base from squeeze info (justification fraction)
         */
        if (si->denom == 0) {   /* num is pixel based */
            if ((basex = si->num) == 0) {       /* look for special cases */
                switch (si->justify) {
                case J_RIGHT:
                    basex = maxwidth;
                    break;
                case J_CENTER:
                    basex = maxwidth / 2;
                    break;
                }
            }
        }
        else {                  /* num/denom is fraction */
            basex = ((si->num * maxwidth) / si->denom);
            if (si->num < 0)
                basex += maxwidth;
        }

        /*
         * adjust for left (nop), center, right justify and clip
         */
        switch (si->justify) {
        case J_CENTER:
            basex -= tw / 2;
            break;
        case J_RIGHT:
            basex -= tw - 1;
            break;
        }
        if (basex > maxwidth - tw + 1)
            basex = maxwidth - tw + 1;
        if (basex < 0)
            basex = 0;

        tmp->title_x = basex - tmp->frame_bw;
    }
}

static void
CreateWindowTitlebarButtons(TwmWindow *tmp_win)
{
    unsigned long valuemask;    /* mask for create windows */
    XSetWindowAttributes attributes;    /* attributes for create windows */
    int leftx, rightx, y;
    TitleButton *tb;
    int nb;

    if (tmp_win->title_height == 0) {
        tmp_win->hilite_w = 0;
        return;
    }

    /*
     * create the title bar windows; let the event handler deal with painting
     * so that we don't have to spend two pixmaps (or deal with hashing)
     */
    ComputeWindowTitleOffsets(tmp_win, tmp_win->attr.width, False);

    leftx = y = Scr->TBInfo.leftx;
    rightx = tmp_win->rightx;

    attributes.win_gravity = NorthWestGravity;
    attributes.background_pixel = tmp_win->title.back;
    attributes.border_pixel = tmp_win->title.fore;
    attributes.event_mask = (ButtonPressMask | ButtonReleaseMask |
                             ExposureMask);
    attributes.cursor = Scr->ButtonCursor;
    valuemask = (CWWinGravity | CWBackPixel | CWBorderPixel | CWEventMask |
                 CWCursor);

    tmp_win->titlebuttons = NULL;
    nb = Scr->TBInfo.nleft + Scr->TBInfo.nright;
    if (nb > 0) {
        tmp_win->titlebuttons = malloc((size_t) nb * sizeof(TBWindow));
        if (!tmp_win->titlebuttons) {
            twmWarning("unable to allocate %d titlebuttons", nb);
        }
        else {
            TBWindow *tbw;
            int boxwidth = (Scr->TBInfo.width + Scr->TBInfo.pad);
            unsigned h =
                (unsigned) (Scr->TBInfo.width - Scr->TBInfo.border * 2);

            for (tb = Scr->TBInfo.head, tbw = tmp_win->titlebuttons; tb;
                 tb = tb->next, tbw++) {
                int x;

                if (tb->rightside) {
                    x = rightx;
                    rightx += boxwidth;
                    attributes.win_gravity = NorthEastGravity;
                }
                else {
                    x = leftx;
                    leftx += boxwidth;
                    attributes.win_gravity = NorthWestGravity;
                }
                tbw->window = XCreateWindow(dpy, tmp_win->title_w, x, y, h, h,
                                            (unsigned int) Scr->TBInfo.border,
                                            0, (unsigned int) CopyFromParent,
                                            (Visual *) CopyFromParent,
                                            valuemask, &attributes);
                tbw->info = tb;
            }
        }
    }

    tmp_win->hilite_w = (tmp_win->titlehighlight
                         ? CreateHighlightWindow(tmp_win) : None);

    XMapSubwindows(dpy, tmp_win->title_w);
    if (tmp_win->hilite_w)
        XUnmapWindow(dpy, tmp_win->hilite_w);
    return;
}

void
SetHighlightPixmap(char *filename)
{
    Pixmap pm = GetBitmap(filename);

    if (pm) {
        if (Scr->hilitePm) {
            XFreePixmap(dpy, Scr->hilitePm);
        }
        Scr->hilitePm = pm;
        Scr->hilite_pm_width = (int) JunkWidth;
        Scr->hilite_pm_height = (int) JunkHeight;
    }
}

void
FetchWmProtocols(TwmWindow *tmp)
{
    unsigned long flags = 0L;
    Atom *protocols = NULL;
    int n;

    if (XGetWMProtocols(dpy, tmp->w, &protocols, &n)) {
        register int i;
        register Atom *ap;

        for (i = 0, ap = protocols; i < n; i++, ap++) {
            if (*ap == _XA_WM_TAKE_FOCUS)
                flags |= DoesWmTakeFocus;
            if (*ap == _XA_WM_SAVE_YOURSELF)
                flags |= DoesWmSaveYourself;
            if (*ap == _XA_WM_DELETE_WINDOW)
                flags |= DoesWmDeleteWindow;
        }
        if (protocols)
            XFree(protocols);
    }
    tmp->protocols = flags;
}

TwmColormap *
CreateTwmColormap(Colormap c)
{
    TwmColormap *cmap;

    cmap = malloc(sizeof(TwmColormap));
    if (!cmap || XSaveContext(dpy, c, ColormapContext, (XPointer) cmap)) {
        if (cmap)
            free(cmap);
        return (NULL);
    }
    cmap->c = c;
    cmap->state = 0;
    cmap->install_req = 0;
    cmap->w = None;
    cmap->refcnt = 1;
    return (cmap);
}

ColormapWindow *
CreateColormapWindow(Window w, Bool creating_parent, Bool property_window)
{
    ColormapWindow *cwin;
    TwmColormap *cmap;
    XWindowAttributes attributes;

    cwin = malloc(sizeof(ColormapWindow));
    if (cwin) {
        if (!XGetWindowAttributes(dpy, w, &attributes) ||
            XSaveContext(dpy, w, ColormapContext, (XPointer) cwin)) {
            free(cwin);
            return (NULL);
        }

        if (XFindContext(dpy, attributes.colormap, ColormapContext,
                         (XPointer *) &cwin->colormap) == XCNOENT) {
            cwin->colormap = cmap = CreateTwmColormap(attributes.colormap);
            if (!cmap) {
                XDeleteContext(dpy, w, ColormapContext);
                free(cwin);
                return (NULL);
            }
        }
        else {
            cwin->colormap->refcnt++;
        }

        cwin->w = w;
        /*
         * Assume that windows in colormap list are
         * obscured if we are creating the parent window.
         * Otherwise, we assume they are unobscured.
         */
        cwin->visibility = creating_parent ?
            VisibilityPartiallyObscured : VisibilityUnobscured;
        cwin->refcnt = 1;

        /*
         * If this is a ColormapWindow property window and we
         * are not monitoring ColormapNotify or VisibilityNotify
         * events, we need to.
         */
        if (property_window &&
            (attributes.your_event_mask &
             (ColormapChangeMask | VisibilityChangeMask)) !=
            (ColormapChangeMask | VisibilityChangeMask)) {
            XSelectInput(dpy, w, attributes.your_event_mask |
                         (ColormapChangeMask | VisibilityChangeMask));
        }
    }

    return (cwin);
}

void
FetchWmColormapWindows(TwmWindow *tmp)
{
    register int i, j;
    Window *cmap_windows = NULL;
    Bool can_free_cmap_windows = False;
    int number_cmap_windows = 0;
    ColormapWindow **cwins = NULL;
    int previously_installed;

    number_cmap_windows = 0;

    /* SUPPRESS 560 */
    if ((previously_installed =
         (Scr->cmapInfo.cmaps == &tmp->cmaps && tmp->cmaps.number_cwins))) {
        cwins = tmp->cmaps.cwins;
        for (i = 0; i < tmp->cmaps.number_cwins; i++)
            cwins[i]->colormap->state = 0;
    }

    if (XGetWMColormapWindows(dpy, tmp->w, &cmap_windows,
                              &number_cmap_windows) &&
        number_cmap_windows > 0) {

        can_free_cmap_windows = False;
        /*
         * check if the top level is in the list, add to front if not
         */
        for (i = 0; i < number_cmap_windows; i++) {
            if (cmap_windows[i] == tmp->w)
                break;
        }
        if (i == number_cmap_windows) { /* not in list */
            Window *new_cmap_windows =
                malloc(sizeof(Window) * (size_t) (number_cmap_windows + 1));

            if (!new_cmap_windows) {
                twmWarning
                    ("unable to allocate %d element colormap window array",
                     number_cmap_windows + 1);
                goto done;
            }
            new_cmap_windows[0] = tmp->w;       /* add to front */
            for (i = 0; i < number_cmap_windows; i++) { /* append rest */
                new_cmap_windows[i + 1] = cmap_windows[i];
            }
            XFree(cmap_windows);
            can_free_cmap_windows = True;       /* do not use XFree any more */
            cmap_windows = new_cmap_windows;
            number_cmap_windows++;
        }

        cwins = malloc(sizeof(ColormapWindow *) * (size_t) number_cmap_windows);
        if (cwins) {
            for (i = 0; i < number_cmap_windows; i++) {

                /*
                 * Copy any existing entries into new list.
                 */
                for (j = 0; j < tmp->cmaps.number_cwins; j++) {
                    if (tmp->cmaps.cwins[j]->w == cmap_windows[i]) {
                        cwins[i] = tmp->cmaps.cwins[j];
                        cwins[i]->refcnt++;
                        break;
                    }
                }

                /*
                 * If the colormap window is not being pointed by
                 * some other applications colormap window list,
                 * create a new entry.
                 */
                if (j == tmp->cmaps.number_cwins) {
                    if (XFindContext(dpy, cmap_windows[i], ColormapContext,
                                     (XPointer *) &cwins[i]) == XCNOENT) {
                        if ((cwins[i] =
                             CreateColormapWindow(cmap_windows[i],
                                                  (tmp->cmaps.number_cwins == 0
                                                   ? True
                                                   : False), True)) == NULL) {
                            int k;

                            for (k = i + 1; k < number_cmap_windows; k++)
                                cmap_windows[k - 1] = cmap_windows[k];
                            i--;
                            number_cmap_windows--;
                        }
                    }
                    else
                        cwins[i]->refcnt++;
                }
            }
        }
    }

    /* No else here, in case we bailed out of clause above.
     */
    if (number_cmap_windows == 0) {

        number_cmap_windows = 1;

        cwins = malloc(sizeof(ColormapWindow *));
        if (XFindContext(dpy, tmp->w, ColormapContext, (XPointer *) &cwins[0])
            == XCNOENT) {
            cwins[0] =
                CreateColormapWindow(tmp->w,
                                     (Bool) tmp->cmaps.number_cwins == 0,
                                     False);
            if (cwins[0] == NULL)
                number_cmap_windows = 0;
        }
        else
            cwins[0]->refcnt++;
    }

    if (tmp->cmaps.number_cwins)
        free_cwins(tmp);

    tmp->cmaps.cwins = cwins;
    tmp->cmaps.number_cwins = number_cmap_windows;
    if (number_cmap_windows > 1)
        tmp->cmaps.scoreboard =
            calloc(1, ColormapsScoreboardLength(&tmp->cmaps));

    if (previously_installed)
        InstallWindowColormaps(PropertyNotify, (TwmWindow *) NULL);

 done:
    if (cmap_windows) {
        if (can_free_cmap_windows)
            free(cmap_windows);
        else
            XFree(cmap_windows);
    }

    return;
}

void
GetWindowSizeHints(TwmWindow *tmp)
{
    long supplied = 0;

    if (!XGetWMNormalHints(dpy, tmp->w, &tmp->hints, &supplied))
        tmp->hints.flags = 0;

    if (tmp->hints.flags & PResizeInc) {
        if (tmp->hints.width_inc == 0)
            tmp->hints.width_inc = 1;
        if (tmp->hints.height_inc == 0)
            tmp->hints.height_inc = 1;
    }

    if (!(supplied & PWinGravity) && (tmp->hints.flags & USPosition)) {
        static int gravs[] = { SouthEastGravity, SouthWestGravity,
            NorthEastGravity, NorthWestGravity
        };
        int right = tmp->attr.x + tmp->attr.width + 2 * tmp->old_bw;
        int bottom = tmp->attr.y + tmp->attr.height + 2 * tmp->old_bw;

        tmp->hints.win_gravity =
            gravs[((Scr->MyDisplayHeight - bottom <
                    tmp->title_height) ? 0 : 2) | ((Scr->MyDisplayWidth -
                                                    right <
                                                    tmp->title_height) ? 0 :
                                                   1)];
        tmp->hints.flags |= PWinGravity;
    }
}
