/*
 *
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
 * */

/***********************************************************************
 *
 * Icon Manager routines
 *
 * 09-Mar-89 Tom LaStrange              File Created
 *
 ***********************************************************************/

#include <stdio.h>
#include "twm.h"
#include "util.h"
#include "parse.h"
#include "screen.h"
#include "resize.h"
#include "add_window.h"
#include "siconify.bm"
#include <X11/Xos.h>
#include <X11/Xmu/CharSet.h>

static void InsertInIconManager(IconMgr *ip, WList *tmp, TwmWindow *tmp_win);

int iconmgr_textx = siconify_width + 11;
static WList *Active = NULL;
WList *DownIconManager = NULL;
int iconifybox_width = siconify_width;
int iconifybox_height = siconify_height;

/**
 * create all the icon manager windows for this screen.
 */
void
CreateIconManagers(void)
{
    IconMgr *p;
    char str[100];
    char str1[100];
    Pixel background;
    const char *icon_name;

    if (Scr->NoIconManagers)
        return;

    if (Scr->siconifyPm == None) {
        Scr->siconifyPm = XCreatePixmapFromBitmapData(dpy, Scr->Root,
                                                      (char *) siconify_bits,
                                                      siconify_width,
                                                      siconify_height, 1, 0, 1);
    }

    for (p = &Scr->iconmgr; p != NULL; p = p->next) {
        int mask = XParseGeometry(p->geometry, &JunkX, &JunkY,
                                  (unsigned int *) &p->width,
                                  (unsigned int *) &p->height);

        if (mask & XNegative)
            JunkX = Scr->MyDisplayWidth - p->width -
                (2 * Scr->BorderWidth) + JunkX;

        if (mask & YNegative)
            JunkY = Scr->MyDisplayHeight - p->height -
                (2 * Scr->BorderWidth) + JunkY;

        background = Scr->IconManagerC.back;
        GetColorFromList(Scr->IconManagerBL, p->name, (XClassHint *) NULL,
                         &background);

        p->w = XCreateSimpleWindow(dpy, Scr->Root,
                                   JunkX, JunkY, (unsigned) p->width,
                                   (unsigned) p->height, 1, Scr->Black,
                                   background);

        snprintf(str, sizeof(str), "%s Icon Manager", p->name);
        snprintf(str1, sizeof(str1), "%s Icons", p->name);
        if (p->icon_name)
            icon_name = p->icon_name;
        else
            icon_name = str1;

        XSetStandardProperties(dpy, p->w, str, icon_name, None, NULL, 0, NULL);

        p->twm_win = AddWindow(p->w, TRUE, p);
        SetMapStateProp(p->twm_win, WithdrawnState);
    }
    for (p = &Scr->iconmgr; p != NULL; p = p->next) {
        GrabButtons(p->twm_win);
        GrabKeys(p->twm_win);
    }
}

/**
 * allocate a new icon manager
 *
 *  \param name     the name of this icon manager
 *  \param con_name the name of the associated icon
 *  \param geom     a geometry string to eventually parse
 *      \param columns  the number of columns this icon manager has
 */
IconMgr *
AllocateIconManager(char *name, char *icon_name, char *geom, int columns)
{
    IconMgr *p;

#ifdef DEBUG_ICONMGR
    fprintf(stderr, "AllocateIconManager\n");
    fprintf(stderr, "  name=\"%s\" icon_name=\"%s\", geom=\"%s\", col=%d\n",
            name, icon_name ? icon_name : "<null>", geom, columns);
#endif

    if (Scr->NoIconManagers)
        return NULL;

    p = malloc(sizeof(IconMgr));
    p->name = name;
    p->icon_name = icon_name;
    p->geometry = geom;
    p->columns = columns;
    p->first = NULL;
    p->last = NULL;
    p->active = NULL;
    p->scr = Scr;
    p->count = 0;
    p->x = 0;
    p->y = 0;
    p->width = 150;
    p->height = 10;

    Scr->iconmgr.lasti->next = p;
    p->prev = Scr->iconmgr.lasti;
    Scr->iconmgr.lasti = p;
    p->next = NULL;

    return (p);
}

/**
 * move the pointer around in an icon manager
 *
 *  \param dir one of the following:
 *    - F_FORWICONMGR:  forward in the window list
 *    - F_BACKICONMGR:  backward in the window list
 *    - F_UPICONMGR:    up one row
 *    - F_DOWNICONMG:   down one row
 *    - F_LEFTICONMGR:  left one column
 *    - F_RIGHTICONMGR: right one column
 */
void
MoveIconManager(int dir)
{
    IconMgr *ip;
    WList *tmp = NULL;
    int cur_row, cur_col, new_row, new_col;
    int row_inc, col_inc;
    int got_it;

    if (!Active)
        return;

    cur_row = Active->row;
    cur_col = Active->col;
    ip = Active->iconmgr;

    row_inc = 0;
    col_inc = 0;
    got_it = FALSE;

    switch (dir) {
    case F_FORWICONMGR:
        if ((tmp = Active->next) == NULL)
            tmp = ip->first;
        got_it = TRUE;
        break;

    case F_BACKICONMGR:
        if ((tmp = Active->prev) == NULL)
            tmp = ip->last;
        got_it = TRUE;
        break;

    case F_UPICONMGR:
        row_inc = -1;
        break;

    case F_DOWNICONMGR:
        row_inc = 1;
        break;

    case F_LEFTICONMGR:
        col_inc = -1;
        break;

    case F_RIGHTICONMGR:
        col_inc = 1;
        break;
    }

    /* If got_it is FALSE ast this point then we got a left, right,
     * up, or down, command.  We will enter this loop until we find
     * a window to warp to.
     */
    new_row = cur_row;
    new_col = cur_col;

    while (!got_it) {
        new_row += row_inc;
        new_col += col_inc;
        if (new_row < 0)
            new_row = ip->cur_rows - 1;
        if (new_col < 0)
            new_col = ip->cur_columns - 1;
        if (new_row >= ip->cur_rows)
            new_row = 0;
        if (new_col >= ip->cur_columns)
            new_col = 0;

        /* Now let's go through the list to see if there is an entry with this
         * new position
         */
        for (tmp = ip->first; tmp != NULL; tmp = tmp->next) {
            if (tmp->row == new_row && tmp->col == new_col) {
                got_it = TRUE;
                break;
            }
        }
    }

    if (!got_it) {
        twmWarning("unable to find window (%d, %d) in icon manager",
                   new_row, new_col);
        return;
    }

    if (tmp == NULL)
        return;

    /* raise the frame so the icon manager is visible */
    if (ip->twm_win->mapped) {
        XRaiseWindow(dpy, ip->twm_win->frame);
        XWarpPointer(dpy, None, tmp->icon, 0, 0, 0, 0, 5, 5);
    }
    else {
        if (tmp->twm->title_height) {
            int tbx = Scr->TBInfo.titlex;
            int x = tmp->twm->highlightx;

            XWarpPointer(dpy, None, tmp->twm->title_w, 0, 0, 0, 0,
                         tbx + (x - tbx) / 2, Scr->TitleHeight / 4);
        }
        else {
            XWarpPointer(dpy, None, tmp->twm->w, 0, 0, 0, 0, 5, 5);
        }
    }
}

/**
 * jump from one icon manager to another, possibly even on another screen
 *  \param dir one of the following:
 *    - F_NEXTICONMGR - go to the next icon manager
 *    - F_PREVICONMGR - go to the previous one
 */

void
JumpIconManager(int dir)
{
    IconMgr *ip, *tmp_ip = NULL;
    int got_it = FALSE;

    if (!Active)
        return;

#define ITER(i) (dir == F_NEXTICONMGR ? (i)->next : (i)->prev)
#define IPOFSP(sp) (dir == F_NEXTICONMGR ? &(sp->iconmgr) : sp->iconmgr.lasti)
#define TEST(ip) if ((ip)->count != 0 && (ip)->twm_win->mapped) \
                 { got_it = TRUE; break; }

    ip = Active->iconmgr;
    for (tmp_ip = ITER(ip); tmp_ip; tmp_ip = ITER(tmp_ip)) {
        TEST(tmp_ip);
    }

    if (!got_it) {
        int origscreen = ip->scr->screen;
        int inc = (dir == F_NEXTICONMGR ? 1 : -1);
        int screen;

        for (screen = origscreen + inc;; screen += inc) {
            ScreenInfo *sp;

            if (screen >= NumScreens)
                screen = 0;
            else if (screen < 0)
                screen = NumScreens - 1;

            sp = ScreenList[screen];
            if (sp) {
                for (tmp_ip = IPOFSP(sp); tmp_ip; tmp_ip = ITER(tmp_ip)) {
                    TEST(tmp_ip);
                }
            }
            if (got_it || screen == origscreen)
                break;
        }
    }

#undef ITER
#undef IPOFSP
#undef TEST

    if (!got_it) {
        Bell(XkbBI_MinorError, 0, None);
        return;
    }

    /* raise the frame so it is visible */
    XRaiseWindow(dpy, tmp_ip->twm_win->frame);
    if (tmp_ip->active)
        XWarpPointer(dpy, None, tmp_ip->active->icon, 0, 0, 0, 0, 5, 5);
    else
        XWarpPointer(dpy, None, tmp_ip->w, 0, 0, 0, 0, 5, 5);
}

/**
 * add a window to an icon manager
 *
 *  \param tmp_win the TwmWindow structure
 */
WList *
AddIconManager(TwmWindow *tmp_win)
{
    WList *tmp;
    int h;
    unsigned long valuemask;    /* mask for create windows */
    XSetWindowAttributes attributes;    /* attributes for create windows */
    IconMgr *ip;

    tmp_win->list = NULL;

    if (tmp_win->iconmgr || tmp_win->transient || Scr->NoIconManagers)
        return NULL;

    if (LookInList(Scr->IconMgrNoShow, tmp_win->full_name, &tmp_win->class))
        return NULL;
    if (Scr->IconManagerDontShow &&
        !LookInList(Scr->IconMgrShow, tmp_win->full_name, &tmp_win->class))
        return NULL;
    if ((ip = (IconMgr *) LookInList(Scr->IconMgrs, tmp_win->full_name,
                                     &tmp_win->class)) == NULL)
        ip = &Scr->iconmgr;

    tmp = malloc(sizeof(WList));
    tmp->iconmgr = ip;
    tmp->next = NULL;
    tmp->active = FALSE;
    tmp->down = FALSE;

    InsertInIconManager(ip, tmp, tmp_win);

    tmp->twm = tmp_win;

    tmp->fore = Scr->IconManagerC.fore;
    tmp->back = Scr->IconManagerC.back;
    tmp->highlight = Scr->IconManagerHighlight;

    GetColorFromList(Scr->IconManagerFL, tmp_win->full_name, &tmp_win->class,
                     &tmp->fore);
    GetColorFromList(Scr->IconManagerBL, tmp_win->full_name, &tmp_win->class,
                     &tmp->back);
    GetColorFromList(Scr->IconManagerHighlightL, tmp_win->full_name,
                     &tmp_win->class, &tmp->highlight);

    h = Scr->IconManagerFont.height + 10;
    if (h < (siconify_height + 4))
        h = siconify_height + 4;

    ip->height = h * ip->count;
    tmp->me = ip->count;
    tmp->x = -1;
    tmp->y = -1;

    valuemask = (CWBackPixel | CWBorderPixel | CWEventMask | CWCursor);
    attributes.background_pixel = tmp->back;
    attributes.border_pixel = tmp->back;
    attributes.event_mask = (KeyPressMask | ButtonPressMask |
                             ButtonReleaseMask | ExposureMask |
                             EnterWindowMask | LeaveWindowMask);
    attributes.cursor = Scr->IconMgrCursor;
    tmp->w = XCreateWindow(dpy, ip->w, 0, 0, (unsigned int) 1,
                           (unsigned int) h, (unsigned int) 0,
                           CopyFromParent, (unsigned int) CopyFromParent,
                           (Visual *) CopyFromParent, valuemask, &attributes);

    valuemask = (CWBackPixel | CWBorderPixel | CWEventMask | CWCursor);
    attributes.background_pixel = tmp->back;
    attributes.border_pixel = Scr->Black;
    attributes.event_mask = (ButtonReleaseMask | ButtonPressMask |
                             ExposureMask);
    attributes.cursor = Scr->ButtonCursor;
    tmp->icon = XCreateWindow(dpy, tmp->w, 5, (int) (h - siconify_height) / 2,
                              (unsigned int) siconify_width,
                              (unsigned int) siconify_height,
                              (unsigned int) 0, CopyFromParent,
                              (unsigned int) CopyFromParent,
                              (Visual *) CopyFromParent,
                              valuemask, &attributes);

    ip->count += 1;
    PackIconManager(ip);
    XMapWindow(dpy, tmp->w);

    XSaveContext(dpy, tmp->w, IconManagerContext, (XPointer) tmp);
    XSaveContext(dpy, tmp->w, TwmContext, (XPointer) tmp_win);
    XSaveContext(dpy, tmp->w, ScreenContext, (XPointer) Scr);
    XSaveContext(dpy, tmp->icon, TwmContext, (XPointer) tmp_win);
    XSaveContext(dpy, tmp->icon, ScreenContext, (XPointer) Scr);
    tmp_win->list = tmp;

    if (!ip->twm_win->icon) {
        XMapWindow(dpy, ip->w);
        XMapWindow(dpy, ip->twm_win->frame);
    }

    if (Active == NULL)
        Active = tmp;

    return (tmp);
}

/**
 * put an allocated entry into an icon manager
 *
 *  \param ip  the icon manager pointer
 *  \param tmp the entry to insert
 */
static void
InsertInIconManager(IconMgr *ip, WList *tmp, TwmWindow *tmp_win)
{
    WList *tmp1;
    int added;
    int (*compar) (const char *, const char *)
        = (Scr->CaseSensitive ? strcmp : XmuCompareISOLatin1);

    added = FALSE;
    if (ip->first == NULL) {
        ip->first = tmp;
        tmp->prev = NULL;
        ip->last = tmp;
        added = TRUE;
    }
    else if (Scr->SortIconMgr) {
        for (tmp1 = ip->first; tmp1 != NULL; tmp1 = tmp1->next) {
            if ((*compar) (tmp_win->icon_name, tmp1->twm->icon_name) < 0) {
                tmp->next = tmp1;
                tmp->prev = tmp1->prev;
                tmp1->prev = tmp;
                if (tmp->prev == NULL)
                    ip->first = tmp;
                else
                    tmp->prev->next = tmp;
                added = TRUE;
                break;
            }
        }
    }

    if (!added) {
        ip->last->next = tmp;
        tmp->prev = ip->last;
        ip->last = tmp;
    }
}

static void
RemoveFromIconManager(IconMgr *ip, WList *tmp)
{
    if (tmp->prev == NULL)
        ip->first = tmp->next;
    else
        tmp->prev->next = tmp->next;

    if (tmp->next == NULL)
        ip->last = tmp->prev;
    else
        tmp->next->prev = tmp->prev;
}

/**
 * remove a window from the icon manager
 *  \param tmp_win the TwmWindow structure
 */
void
RemoveIconManager(TwmWindow *tmp_win)
{
    IconMgr *ip;
    WList *tmp;

    if (tmp_win->list == NULL)
        return;

    tmp = tmp_win->list;
    tmp_win->list = NULL;
    ip = tmp->iconmgr;

    RemoveFromIconManager(ip, tmp);

    XDeleteContext(dpy, tmp->icon, TwmContext);
    XDeleteContext(dpy, tmp->icon, ScreenContext);
    XDestroyWindow(dpy, tmp->icon);
    XDeleteContext(dpy, tmp->w, IconManagerContext);
    XDeleteContext(dpy, tmp->w, TwmContext);
    XDeleteContext(dpy, tmp->w, ScreenContext);
    XDestroyWindow(dpy, tmp->w);
    ip->count -= 1;
    free(tmp);

    PackIconManager(ip);

    if (ip->count == 0) {
        XUnmapWindow(dpy, ip->twm_win->frame);
    }

}

void
ActiveIconManager(WList *active)
{
    active->active = TRUE;
    Active = active;
    Active->iconmgr->active = active;
    DrawIconManagerBorder(active);
}

void
NotActiveIconManager(WList *active)
{
    active->active = FALSE;
    DrawIconManagerBorder(active);
}

void
DrawIconManagerBorder(WList *tmp)
{
    {
        XSetForeground(dpy, Scr->NormalGC, tmp->fore);
        XDrawRectangle(dpy, tmp->w, Scr->NormalGC, 2, 2,
                       (unsigned) (tmp->width - 5),
                       (unsigned) (tmp->height - 5));

        if (tmp->active && Scr->Highlight)
            XSetForeground(dpy, Scr->NormalGC, tmp->highlight);
        else
            XSetForeground(dpy, Scr->NormalGC, tmp->back);

        XDrawRectangle(dpy, tmp->w, Scr->NormalGC, 0, 0,
                       (unsigned) (tmp->width - 1),
                       (unsigned) (tmp->height - 1));
        XDrawRectangle(dpy, tmp->w, Scr->NormalGC, 1, 1,
                       (unsigned) (tmp->width - 3),
                       (unsigned) (tmp->height - 3));
    }
}

/**
 * sort The Dude
 *
 *  \param ip a pointer to the icon manager structure
 */
void
SortIconManager(IconMgr *ip)
{
    WList *tmp1, *tmp2;
    int done;
    int (*compar) (const char *, const char *)
        = (Scr->CaseSensitive ? strcmp : XmuCompareISOLatin1);

    if (ip == NULL)
        ip = Active->iconmgr;

    done = FALSE;
    do {
        for (tmp1 = ip->first; tmp1 != NULL; tmp1 = tmp1->next) {
            if ((tmp2 = tmp1->next) == NULL) {
                done = TRUE;
                break;
            }
            if ((*compar) (tmp1->twm->icon_name, tmp2->twm->icon_name) > 0) {
                /* take it out and put it back in */
                RemoveFromIconManager(ip, tmp2);
                InsertInIconManager(ip, tmp2, tmp2->twm);
                break;
            }
        }
    }
    while (!done);
    PackIconManager(ip);
}

/**
 * pack the icon manager windows following
 *              an addition or deletion
 *
 *  \param ip a pointer to the icon manager structure
 */
void
PackIconManager(IconMgr *ip)
{
    int newwidth, i, row, col, maxcol, colinc, rowinc, wheight, wwidth;
    int savewidth;
    WList *tmp;

    wheight = Scr->IconManagerFont.height + 10;
    if (wheight < (siconify_height + 4))
        wheight = siconify_height + 4;

    wwidth = ip->width / ip->columns;

    rowinc = wheight;
    colinc = wwidth;

    row = 0;
    col = ip->columns;
    maxcol = 0;

    for (i = 0, tmp = ip->first; tmp != NULL; i++, tmp = tmp->next) {
        int new_x, new_y;

        tmp->me = i;
        if (++col >= ip->columns) {
            col = 0;
            row += 1;
        }
        if (col > maxcol)
            maxcol = col;

        new_x = col * colinc;
        new_y = (row - 1) * rowinc;

        /* if the position or size has not changed, don't touch it */
        if (tmp->x != new_x || tmp->y != new_y ||
            tmp->width != wwidth || tmp->height != wheight) {
            XMoveResizeWindow(dpy, tmp->w,
                              new_x, new_y,
                              (unsigned) wwidth, (unsigned) wheight);

            tmp->row = row - 1;
            tmp->col = col;
            tmp->x = new_x;
            tmp->y = new_y;
            tmp->width = wwidth;
            tmp->height = wheight;
        }
    }
    maxcol += 1;

    ip->cur_rows = row;
    ip->cur_columns = maxcol;
    ip->height = row * rowinc;
    if (ip->height == 0)
        ip->height = rowinc;
    newwidth = maxcol * colinc;
    if (newwidth == 0)
        newwidth = colinc;

    XResizeWindow(dpy, ip->w, (unsigned) newwidth, (unsigned) ip->height);

    savewidth = ip->width;
    if (ip->twm_win)
        SetupWindow(ip->twm_win,
                    ip->twm_win->frame_x, ip->twm_win->frame_y,
                    newwidth, ip->height + ip->twm_win->title_height, -1);
    ip->width = savewidth;
}
