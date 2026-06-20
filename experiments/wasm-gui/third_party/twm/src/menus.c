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
 * twm menu code
 *
 * 17-Nov-87 Thomas E. LaStrange                File created
 *
 ***********************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <X11/Xos.h>
#include "twm.h"
#include "gc.h"
#include "menus.h"
#include "resize.h"
#include "events.h"
#include "util.h"
#include "parse.h"
#include "gram.h"
#include "screen.h"
#include "menus.h"
#include "iconmgr.h"
#include "add_window.h"
#include "icons.h"
#include "session.h"
#include <X11/Xmu/CharSet.h>
#include "version.h"
#include <X11/extensions/sync.h>
#include <X11/SM/SMlib.h>

int RootFunction = 0;
MenuRoot *ActiveMenu = NULL;            /**< the active menu */
MenuItem *ActiveItem = NULL;            /**< the active menu item */
int MoveFunction;                       /**< either F_MOVE or F_FORCEMOVE */
int WindowMoved = FALSE;
int menuFromFrameOrWindowOrTitlebar = FALSE;

int ConstMove = FALSE;          /**< constrained move variables */
int ConstMoveDir;
int ConstMoveX;
int ConstMoveY;
int ConstMoveXL;
int ConstMoveXR;
int ConstMoveYT;
int ConstMoveYB;

/* Globals used to keep track of whether the mouse has moved during
   a resize function. */
int ResizeOrigX;
int ResizeOrigY;

int MenuDepth = 0;              /**< number of menus up */
static struct {
    int x;
    int y;
} MenuOrigins[MAXMENUDEPTH];
static Cursor LastCursor;

static Bool belongs_to_twm_window(TwmWindow *t, Window w);
static void Identify(TwmWindow *t);
static void send_clientmessage(Window w, Atom a, Time timestamp);
static void BumpWindowColormap(TwmWindow *tmp, int inc);
static int DeferExecution(int context, int func, Cursor cursor);
static Bool NeedToDefer(MenuRoot *root);
static void DestroyMenu(MenuRoot *menu);
static void MakeMenu(MenuRoot *mr);
static void Execute(const char *s);
static void HideIconManager(void);
static void WarpAlongRing(XButtonEvent *ev, Bool forward);
static int WarpThere(TwmWindow *t);
static void WarpToWindow(TwmWindow *t);

#define SHADOWWIDTH 5           /* in pixels */

/**
 * initialize menu roots
 */
void
InitMenus(void)
{
    int i, j, k;
    FuncKey *key;

    for (i = 0; i < MAX_BUTTONS + 1; i++)
        for (j = 0; j < NUM_CONTEXTS; j++)
            for (k = 0; k < MOD_SIZE; k++) {
                Scr->Mouse[i][j][k].func = 0;
                Scr->Mouse[i][j][k].item = NULL;
            }

    Scr->DefaultFunction.func = 0;
    Scr->WindowFunction.func = 0;

    if (FirstScreen) {
        for (key = Scr->FuncKeyRoot.next; key != NULL;) {
            FuncKey *tmp = key;

            free(key->name);
            key = key->next;
            free(tmp);
        }
        Scr->FuncKeyRoot.next = NULL;
    }

}

/**
 * add a function key to the list
 *
 *  \param name     the name of the key
 *  \param cont     the context to look for the key press in
 *  \param mods2    modifier keys that need to be pressed
 *  \param func     the function to perform
 *  \param win_name the window name (if any)
 *  \param action   the action string associated with the function (if any)
 */
Bool
AddFuncKey(char *name, int cont, int mods2, int func, char *win_name,
           char *action)
{
    FuncKey *tmp;
    KeySym keysym;
    KeyCode keycode;

    /*
     * Don't let a 0 keycode go through, since that means AnyKey to the
     * XGrabKey call in GrabKeys().
     */
    if ((keysym = XStringToKeysym(name)) == NoSymbol ||
        (keycode = XKeysymToKeycode(dpy, keysym)) == 0) {
        return False;
    }

    /* see if there already is a key defined for this context */
    for (tmp = Scr->FuncKeyRoot.next; tmp != NULL; tmp = tmp->next) {
        if (tmp->keysym == keysym && tmp->cont == cont && tmp->mods == mods2)
            break;
    }

    if (tmp == NULL) {
        tmp = malloc(sizeof(FuncKey));
        tmp->next = Scr->FuncKeyRoot.next;
        Scr->FuncKeyRoot.next = tmp;
    }

    tmp->name = name;
    tmp->keysym = keysym;
    tmp->keycode = keycode;
    tmp->cont = cont;
    tmp->mods = mods2;
    tmp->func = func;
    tmp->win_name = win_name;
    tmp->action = action;

    return True;
}

int
CreateTitleButton(const char *name, int func, const char *action,
                  MenuRoot *menuroot, Bool rightside, Bool append)
{
    TitleButton *tb = malloc(sizeof(TitleButton));

    if (!tb) {
        twmWarning("unable to allocate %lu bytes for title button",
                   (unsigned long) sizeof(TitleButton));
        return 0;
    }

    tb->next = NULL;
    tb->name = name;            /* note that we are not copying */
    tb->bitmap = None;          /* WARNING, values not set yet */
    tb->width = 0;              /* see InitTitlebarButtons */
    tb->height = 0;             /* ditto */
    tb->func = func;
    tb->action = action;
    tb->menuroot = menuroot;
    tb->rightside = rightside;
    if (rightside) {
        Scr->TBInfo.nright++;
    }
    else {
        Scr->TBInfo.nleft++;
    }

    /*
     * Cases for list:
     *
     *     1.  empty list, prepend left       put at head of list
     *     2.  append left, prepend right     put in between left and right
     *     3.  append right                   put at tail of list
     *
     * Do not refer to widths and heights yet since buttons not created
     * (since fonts not loaded and heights not known).
     */
    if ((!Scr->TBInfo.head) || ((!append) && (!rightside))) {   /* 1 */
        tb->next = Scr->TBInfo.head;
        Scr->TBInfo.head = tb;
    }
    else if (append && rightside) {     /* 3 */
        register TitleButton *t;

        /* SUPPRESS 530 */
        for (t = Scr->TBInfo.head; t->next; t = t->next);
        t->next = tb;
        tb->next = NULL;
    }
    else {                      /* 2 */
        register TitleButton *t, *prev = NULL;

        for (t = Scr->TBInfo.head; t && !t->rightside; t = t->next) {
            prev = t;
        }
        if (prev) {
            tb->next = prev->next;
            prev->next = tb;
        }
        else {
            tb->next = Scr->TBInfo.head;
            Scr->TBInfo.head = tb;
        }
    }

    return 1;
}

/**
 * Do all the necessary stuff to load in a titlebar button.  If we can't find
 * the button, then put in a question; if we can't find the question mark,
 * something is wrong and we are probably going to be in trouble later on.
 */
void
InitTitlebarButtons(void)
{
    TitleButton *tb;
    int h;

    /*
     * initialize dimensions
     */
    Scr->TBInfo.width = (Scr->TitleHeight -
                         2 * (Scr->FramePadding + Scr->ButtonIndent));
    Scr->TBInfo.pad = ((Scr->TitlePadding > 1)
                       ? ((Scr->TitlePadding + 1) / 2) : 1);
    h = Scr->TBInfo.width - 2 * Scr->TBInfo.border;

    /*
     * add in some useful buttons and bindings so that novices can still
     * use the system.
     */
    if (!Scr->NoDefaults) {
        /* insert extra buttons */
        if (!CreateTitleButton(TBPM_ICONIFY, F_ICONIFY, "", (MenuRoot *) NULL,
                               False, False)) {
            twmWarning("unable to add iconify button");
        }
        if (!CreateTitleButton(TBPM_RESIZE, F_RESIZE, "", (MenuRoot *) NULL,
                               True, True)) {
            twmWarning("unable to add resize button");
        }
        AddDefaultBindings();
    }
    ComputeCommonTitleOffsets();

    /*
     * load in images and do appropriate centering
     */

    for (tb = Scr->TBInfo.head; tb; tb = tb->next) {
        tb->bitmap = FindBitmap(tb->name, &tb->width, &tb->height);
        if (!tb->bitmap) {
            tb->bitmap = FindBitmap(TBPM_QUESTION, &tb->width, &tb->height);
            if (!tb->bitmap) {  /* cannot happen (see util.c) */
                twmWarning("unable to add titlebar button \"%s\"", tb->name);
            }
        }

        tb->dstx = (int) (((unsigned) h - tb->width + 1) / 2);
        if (tb->dstx < 0) {     /* clip to minimize copying */
            tb->srcx = -(tb->dstx);
            tb->width = (unsigned) h;
            tb->dstx = 0;
        }
        else {
            tb->srcx = 0;
        }
        tb->dsty = (int) (((unsigned) h - tb->height + 1) / 2);
        if (tb->dsty < 0) {
            tb->srcy = -(tb->dsty);
            tb->height = (unsigned) h;
            tb->dsty = 0;
        }
        else {
            tb->srcy = 0;
        }
    }
}

void
PaintEntry(MenuRoot *mr, MenuItem *mi, int exposure)
{
    int y_offset;
    int text_y;
    GC gc;

#ifdef DEBUG_MENUS
    fprintf(stderr, "Paint entry\n");
#endif
    y_offset = mi->item_num * Scr->EntryHeight;
    text_y = y_offset + Scr->MenuFont.y;

    if (mi->func != F_TITLE) {

        if (mi->state) {
            XSetForeground(dpy, Scr->NormalGC, mi->hi_back);

            XFillRectangle(dpy, mr->w, Scr->NormalGC, 0, y_offset,
                           (unsigned) mr->width, (unsigned) Scr->EntryHeight);

            MyFont_ChangeGC(mi->hi_fore, mi->hi_back, &Scr->MenuFont);

            MyFont_DrawString(dpy, mr->w, &Scr->MenuFont, Scr->NormalGC, mi->x,
                              text_y, mi->item, mi->strlen);

            gc = Scr->NormalGC;
        }
        else {
            if (mi->user_colors || !exposure) {
                XSetForeground(dpy, Scr->NormalGC, mi->back);

                XFillRectangle(dpy, mr->w, Scr->NormalGC, 0, y_offset,
                               (unsigned) mr->width,
                               (unsigned) Scr->EntryHeight);

                MyFont_ChangeGC(mi->fore, mi->back, &Scr->MenuFont);
                gc = Scr->NormalGC;
            }
            else
                gc = Scr->MenuGC;

            MyFont_DrawString(dpy, mr->w, &Scr->MenuFont, gc,
                              mi->x, text_y, mi->item, mi->strlen);

        }

        if (mi->func == F_MENU) {
            int x, y;

            /* create the pull right pixmap if needed */
            if (Scr->pullPm == None) {
                Scr->pullPm = CreateMenuIcon(Scr->MenuFont.height,
                                             &Scr->pullW, &Scr->pullH);
            }
            x = (int) ((unsigned) mr->width - (Scr->pullW + 5));
            y = (int) ((unsigned) y_offset +
                       (((unsigned) Scr->MenuFont.height - Scr->pullH) / 2));
            XCopyPlane(dpy, Scr->pullPm, mr->w, gc, 0, 0, Scr->pullW,
                       Scr->pullH, x, y, 1);
        }
    }
    else {
        XSetForeground(dpy, Scr->NormalGC, mi->back);

        /* fill the rectangle with the title background color */
        XFillRectangle(dpy, mr->w, Scr->NormalGC, 0, y_offset,
                       (unsigned) mr->width, (unsigned) Scr->EntryHeight);

        {
            int y;

            XSetForeground(dpy, Scr->NormalGC, mi->fore);
            /* now draw the dividing lines */
            if (y_offset)
                XDrawLine(dpy, mr->w, Scr->NormalGC, 0, y_offset,
                          mr->width, y_offset);
            y = ((mi->item_num + 1) * Scr->EntryHeight) - 1;
            XDrawLine(dpy, mr->w, Scr->NormalGC, 0, y, mr->width, y);
        }

        MyFont_ChangeGC(mi->fore, mi->back, &Scr->MenuFont);
        /* finally render the title */
        MyFont_DrawString(dpy, mr->w, &Scr->MenuFont, Scr->NormalGC, mi->x,
                          text_y, mi->item, mi->strlen);
    }
}

void
PaintMenu(MenuRoot *mr, XEvent *e)
{
    MenuItem *mi;

    for (mi = mr->first; mi != NULL; mi = mi->next) {
        int y_offset = mi->item_num * Scr->EntryHeight;

        /* be smart about handling the expose, redraw only the entries
         * that we need to
         */
        if (e->xexpose.y < (y_offset + Scr->EntryHeight) &&
            (e->xexpose.y + e->xexpose.height) > y_offset) {
            PaintEntry(mr, mi, True);
        }
    }
    XSync(dpy, 0);
}

static Bool fromMenu;

void
UpdateMenu(void)
{
    MenuItem *mi;
    int i, x, y, x_root, y_root, entry;
    int done;
    MenuItem *badItem = NULL;
    XPointer context_data;

    fromMenu = TRUE;

    while (TRUE) {
        /* block until there is an event */
        if (!menuFromFrameOrWindowOrTitlebar) {
            XMaskEvent(dpy,
                       ButtonPressMask | ButtonReleaseMask |
                       EnterWindowMask | ExposureMask |
                       VisibilityChangeMask | LeaveWindowMask |
                       ButtonMotionMask, &Event);
        }
        if (Event.type == MotionNotify) {
            /* discard any extra motion events before a release */
            while (XCheckMaskEvent(dpy,
                                   ButtonMotionMask | ButtonReleaseMask,
                                   &Event))
                if (Event.type == ButtonRelease)
                    break;
        }

        if (!DispatchEvent())
            continue;

        if (Event.type == ButtonRelease || Cancel) {
            menuFromFrameOrWindowOrTitlebar = FALSE;
            fromMenu = FALSE;
            return;
        }

        if (Event.type != MotionNotify)
            continue;

        if (!ActiveMenu)
            continue;

        done = FALSE;
        XQueryPointer(dpy, ActiveMenu->w, &JunkRoot, &JunkChild,
                      &x_root, &y_root, &x, &y, &JunkMask);

        /* if we haven't received the enter notify yet, wait */
        if (!ActiveMenu->entered)
            continue;

        if (XFindContext(dpy, ActiveMenu->w, ScreenContext, &context_data) == 0)
            Scr = (struct ScreenInfo *) context_data;

        if (x < 0 || y < 0 || x >= ActiveMenu->width || y >= ActiveMenu->height) {
            if (ActiveItem && ActiveItem->func != F_TITLE) {
                ActiveItem->state = 0;
                PaintEntry(ActiveMenu, ActiveItem, False);
            }
            ActiveItem = NULL;
            continue;
        }

        /* look for the entry that the mouse is in */
        entry = y / Scr->EntryHeight;
        for (i = 0, mi = ActiveMenu->first; mi != NULL; i++, mi = mi->next) {
            if (i == entry)
                break;
        }

        /* if there is an active item, we might have to turn it off */
        if (ActiveItem) {
            /* is the active item the one we are on ? */
            if (ActiveItem->item_num == entry && ActiveItem->state)
                done = TRUE;

            /* if we weren't on the active entry, let's turn the old
             * active one off
             */
            if (!done && ActiveItem->func != F_TITLE) {
                ActiveItem->state = 0;
                PaintEntry(ActiveMenu, ActiveItem, False);
            }
        }

        /* if we weren't on the active item, change the active item and turn
         * it on
         */
        if (!done) {
            ActiveItem = mi;
            if (ActiveItem && ActiveItem->func != F_TITLE && !ActiveItem->state) {
                ActiveItem->state = 1;
                PaintEntry(ActiveMenu, ActiveItem, False);
            }
        }

        /* now check to see if we were over the arrow of a pull right entry */
        if (ActiveItem && ActiveItem->func == F_MENU &&
            ((ActiveMenu->width - x) < (ActiveMenu->width >> 1))) {
            MenuRoot *save = ActiveMenu;
            int savex = MenuOrigins[MenuDepth - 1].x;
            int savey = MenuOrigins[MenuDepth - 1].y;

            if (MenuDepth < MAXMENUDEPTH) {
                PopUpMenu(ActiveItem->sub,
                          (savex + (ActiveMenu->width >> 1)),
                          (savey + ActiveItem->item_num * Scr->EntryHeight)
                          /*(savey + ActiveItem->item_num * Scr->EntryHeight +
                             (Scr->EntryHeight >> 1)) */
                          , False);
            }
            else if (!badItem) {
                Bell(XkbBI_MinorError, 0, None);
                badItem = ActiveItem;
            }

            /* if the menu did get popped up, unhighlight the active item */
            if (save != ActiveMenu && ActiveItem->state) {
                ActiveItem->state = 0;
                PaintEntry(save, ActiveItem, False);
                ActiveItem = NULL;
            }
        }
        if (badItem != ActiveItem)
            badItem = NULL;
        XFlush(dpy);
    }

}

/**
 * create a new menu root
 *
 *  \param name  the name of the menu root
 */
MenuRoot *
NewMenuRoot(const char *name)
{
    MenuRoot *tmp;

#define UNUSED_PIXEL ((unsigned long) (~0))     /* more than 24 bits */

    tmp = malloc(sizeof(MenuRoot));
    tmp->hi_fore = UNUSED_PIXEL;
    tmp->hi_back = UNUSED_PIXEL;
    tmp->name = name;
    tmp->prev = NULL;
    tmp->first = NULL;
    tmp->last = NULL;
    tmp->items = 0;
    tmp->width = 0;
    tmp->mapped = NEVER_MAPPED;
    tmp->pull = FALSE;
    tmp->w = None;
    tmp->shadow = None;
    tmp->real_menu = FALSE;

    if (Scr->MenuList == NULL) {
        Scr->MenuList = tmp;
        Scr->MenuList->next = NULL;
    }

    if (Scr->LastMenu == NULL) {
        Scr->LastMenu = tmp;
        Scr->LastMenu->next = NULL;
    }
    else {
        Scr->LastMenu->next = tmp;
        Scr->LastMenu = tmp;
        Scr->LastMenu->next = NULL;
    }

    if (strcmp(name, TWM_WINDOWS) == 0)
        Scr->Windows = tmp;

    return (tmp);
}

/**
 * add an item to a root menu
 *
 *  \param menu   pointer to the root menu to add the item
 *  \param item   the text to appear in the menu
 *  \param action the string to possibly execute
 *  \param sub    the menu root if it is a pull-right entry
 *  \param func   the numeric function
 *  \param fore   foreground color string
 *  \param back   background color string
 */
MenuItem *
AddToMenu(MenuRoot *menu, const char *item, const char *action,
          MenuRoot *sub, int func, const char *fore, const char *back)
{
    MenuItem *tmp;
    int width;

#ifdef DEBUG_MENUS
    fprintf(stderr, "adding menu item=\"%s\", action=%s, sub=%p, f=%d\n",
            item, action ? action : "<null>", sub, func);
#endif

    tmp = malloc(sizeof(MenuItem));
    tmp->root = menu;

    if (menu->first == NULL) {
        menu->first = tmp;
        tmp->prev = NULL;
    }
    else {
        menu->last->next = tmp;
        tmp->prev = menu->last;
    }
    menu->last = tmp;

    tmp->item = item;
    tmp->strlen = (short) strlen(item);
    tmp->action = action;
    tmp->next = NULL;
    tmp->sub = NULL;
    tmp->state = 0;
    tmp->func = (short) func;

    if (!Scr->HaveFonts)
        CreateFonts();
    width = MyFont_TextWidth(&Scr->MenuFont, item, tmp->strlen);
    if (width <= 0)
        width = 1;
    if (width > menu->width)
        menu->width = (short) width;

    tmp->user_colors = FALSE;
    if (Scr->Monochrome == COLOR && fore != NULL) {
        int save;

        save = Scr->FirstTime;
        Scr->FirstTime = TRUE;
        GetColor(COLOR, &tmp->fore, fore);
        GetColor(COLOR, &tmp->back, back);
        Scr->FirstTime = (short) save;
        tmp->user_colors = TRUE;
    }
    if (sub != NULL) {
        tmp->sub = sub;
        menu->pull = TRUE;
    }
    tmp->item_num = menu->items++;

    return (tmp);
}

void
MakeMenus(void)
{
    MenuRoot *mr;

    for (mr = Scr->MenuList; mr != NULL; mr = mr->next) {
        if (mr->real_menu == FALSE)
            continue;

        MakeMenu(mr);
    }
}

static void
MakeMenu(MenuRoot *mr)
{
    MenuItem *start, *end, *cur, *tmp;
    XColor f1, f2, f3;
    XColor b1, b2, b3;
    XColor save_fore, save_back;
    int i;
    XSetWindowAttributes attributes;
    Colormap cmap = Scr->TwmRoot.cmaps.cwins[0]->colormap->c;

    Scr->EntryHeight = Scr->MenuFont.height + 4;

    /* let's first size the window accordingly */
    if (mr->mapped == NEVER_MAPPED) {
        int width;
        unsigned long valuemask;

        if (mr->pull == TRUE) {
            mr->width = (short) (mr->width + (16 + 10));
        }

        width = mr->width + 10;

        for (cur = mr->first; cur != NULL; cur = cur->next) {
            if (cur->func != F_TITLE)
                cur->x = 5;
            else {
                cur->x =
                    (short) (width -
                             MyFont_TextWidth(&Scr->MenuFont, cur->item,
                                              cur->strlen));
                cur->x /= 2;
            }
        }
        mr->height = (short) (mr->items * Scr->EntryHeight);
        mr->width = (short) (mr->width + 10);

        if (Scr->Shadow) {
            /*
             * Make sure that you don't draw into the shadow window or else
             * the background bits there will get saved
             */
            valuemask = (CWBackPixel | CWBorderPixel);
            attributes.background_pixel = Scr->MenuShadowColor;
            attributes.border_pixel = Scr->MenuShadowColor;
            if (Scr->SaveUnder) {
                valuemask |= CWSaveUnder;
                attributes.save_under = True;
            }
            mr->shadow = XCreateWindow(dpy, Scr->Root, 0, 0,
                                       (unsigned int) mr->width,
                                       (unsigned int) mr->height,
                                       (unsigned int) 0,
                                       CopyFromParent,
                                       (unsigned int) CopyFromParent,
                                       (Visual *) CopyFromParent,
                                       valuemask, &attributes);
        }

        valuemask = (CWBackPixel | CWBorderPixel | CWEventMask);
        attributes.background_pixel = Scr->MenuC.back;
        attributes.border_pixel = Scr->MenuBorderColor;
        attributes.event_mask = (ExposureMask | EnterWindowMask);
        if (Scr->SaveUnder) {
            valuemask |= CWSaveUnder;
            attributes.save_under = True;
        }
        if (Scr->BackingStore) {
            valuemask |= CWBackingStore;
            attributes.backing_store = Always;
        }
        mr->w = XCreateWindow(dpy, Scr->Root, 0, 0, (unsigned int) mr->width,
                              (unsigned int) mr->height,
                              (unsigned int) Scr->MenuBorderWidth,
                              CopyFromParent, (unsigned int) CopyFromParent,
                              (Visual *) CopyFromParent,
                              valuemask, &attributes);

        XSaveContext(dpy, mr->w, MenuContext, (XPointer) mr);
        XSaveContext(dpy, mr->w, ScreenContext, (XPointer) Scr);

        mr->mapped = UNMAPPED;
    }

    /* get the default colors into the menus */
    for (tmp = mr->first; tmp != NULL; tmp = tmp->next) {
        if (!tmp->user_colors) {
            if (tmp->func != F_TITLE) {
                tmp->fore = Scr->MenuC.fore;
                tmp->back = Scr->MenuC.back;
            }
            else {
                tmp->fore = Scr->MenuTitleC.fore;
                tmp->back = Scr->MenuTitleC.back;
            }
        }

        if (mr->hi_fore != UNUSED_PIXEL) {
            tmp->hi_fore = mr->hi_fore;
            tmp->hi_back = mr->hi_back;
        }
        else {
            tmp->hi_fore = tmp->back;
            tmp->hi_back = tmp->fore;
        }
    }

    if (Scr->Monochrome == MONOCHROME || !Scr->InterpolateMenuColors)
        return;

    start = mr->first;

    while (TRUE) {
        int num;
        int fred, fgreen, fblue;
        int bred, bgreen, bblue;

        for (; start != NULL; start = start->next) {
            if (start->user_colors)
                break;
        }
        if (start == NULL)
            break;

        for (end = start->next; end != NULL; end = end->next) {
            if (end->user_colors)
                break;
        }
        if (end == NULL)
            break;

        /* we have a start and end to interpolate between */
        num = end->item_num - start->item_num;

        f1.pixel = start->fore;
        XQueryColor(dpy, cmap, &f1);
        f2.pixel = end->fore;
        XQueryColor(dpy, cmap, &f2);

        b1.pixel = start->back;
        XQueryColor(dpy, cmap, &b1);
        b2.pixel = end->back;
        XQueryColor(dpy, cmap, &b2);

        fred = ((int) f2.red - (int) f1.red) / num;
        fgreen = ((int) f2.green - (int) f1.green) / num;
        fblue = ((int) f2.blue - (int) f1.blue) / num;

        bred = ((int) b2.red - (int) b1.red) / num;
        bgreen = ((int) b2.green - (int) b1.green) / num;
        bblue = ((int) b2.blue - (int) b1.blue) / num;

        f3 = f1;
        f3.flags = DoRed | DoGreen | DoBlue;

        b3 = b1;
        b3.flags = DoRed | DoGreen | DoBlue;

        num -= 1;
        for (i = 0, cur = start->next; i < num && cur; i++, cur = cur->next) {
#define AddColor(target,source) target = (unsigned short)(target + source)
            AddColor(f3.red, fred);
            AddColor(f3.green, fgreen);
            AddColor(f3.blue, fblue);
            save_fore = f3;

            AddColor(b3.red, bred);
            AddColor(b3.green, bgreen);
            AddColor(b3.blue, bblue);
            save_back = b3;

            XAllocColor(dpy, cmap, &f3);
            XAllocColor(dpy, cmap, &b3);
            cur->hi_back = cur->fore = f3.pixel;
            cur->hi_fore = cur->back = b3.pixel;
            cur->user_colors = True;

            f3 = save_fore;
            b3 = save_back;
        }
        start = end;
    }
}

/**
 * pop up a pull down menu.
 *
 *  \param menu   the root pointer of the menu to pop up
 *  \param x,y    location of upper left of menu
 *  \param center whether or not to center horizontally over position
 */
Bool
PopUpMenu(MenuRoot *menu, int x, int y, Bool center)
{
    TwmWindow **WindowNames;
    TwmWindow *tmp_win2, *tmp_win3;
    int (*compar) (const char *, const char *) =
        (Scr->CaseSensitive ? strcmp : XmuCompareISOLatin1);

    if (!menu)
        return False;

    InstallRootColormap();

    if (menu == Scr->Windows) {
        TwmWindow *tmp_win;
        int WindowNameCount;

        /* this is the twm windows menu,  let's go ahead and build it */

        DestroyMenu(menu);

        menu->first = NULL;
        menu->last = NULL;
        menu->items = 0;
        menu->width = 0;
        menu->mapped = NEVER_MAPPED;
        AddToMenu(menu, "TWM Windows", NULLSTR, NULL, F_TITLE, NULLSTR,
                  NULLSTR);

        for (tmp_win = Scr->TwmRoot.next, WindowNameCount = 0;
             tmp_win != NULL; tmp_win = tmp_win->next)
            WindowNameCount++;

        if (WindowNameCount != 0) {
            int i;

            WindowNames =
                malloc(sizeof(TwmWindow *) * (size_t) WindowNameCount);
            WindowNames[0] = Scr->TwmRoot.next;

            for (tmp_win = Scr->TwmRoot.next->next, WindowNameCount = 1;
                 tmp_win != NULL; tmp_win = tmp_win->next, WindowNameCount++) {
                tmp_win2 = tmp_win;
                for (i = 0; i < WindowNameCount; i++) {
                    if ((*compar) (tmp_win2->name, WindowNames[i]->name) < 0) {
                        tmp_win3 = tmp_win2;
                        tmp_win2 = WindowNames[i];
                        WindowNames[i] = tmp_win3;
                    }
                }
                WindowNames[WindowNameCount] = tmp_win2;
            }
            for (i = 0; i < WindowNameCount; i++) {
                AddToMenu(menu, WindowNames[i]->name, (char *) WindowNames[i],
                          NULL, F_POPUP, NULL, NULL);
            }
            free(WindowNames);
        }

        MakeMenu(menu);
    }

    if (menu->w == None || menu->items == 0)
        return False;

    /* Prevent recursively bringing up menus. */
    if (menu->mapped == MAPPED)
        return False;

    /*
     * Dynamically set the parent;  this allows pull-ups to also be main
     * menus, or to be brought up from more than one place.
     */
    menu->prev = ActiveMenu;

    XGrabPointer(dpy, Scr->Root, True,
                 ButtonPressMask | ButtonReleaseMask |
                 ButtonMotionMask | PointerMotionHintMask,
                 GrabModeAsync, GrabModeAsync,
                 Scr->Root, Scr->MenuCursor, CurrentTime);

    ActiveMenu = menu;
    menu->mapped = MAPPED;
    menu->entered = FALSE;

    if (center) {
        x -= (menu->width / 2);
        y -= (Scr->EntryHeight / 2);    /* sticky menus would be nice here */
    }

    /*
     * clip to screen
     */
    if (x + menu->width > Scr->MyDisplayWidth) {
        x = Scr->MyDisplayWidth - menu->width;
    }
    if (x < 0)
        x = 0;
    if (y + menu->height > Scr->MyDisplayHeight) {
        y = Scr->MyDisplayHeight - menu->height;
    }
    if (y < 0)
        y = 0;

    MenuOrigins[MenuDepth].x = x;
    MenuOrigins[MenuDepth].y = y;
    MenuDepth++;

    XMoveWindow(dpy, menu->w, x, y);
    if (Scr->Shadow) {
        XMoveWindow(dpy, menu->shadow, x + SHADOWWIDTH, y + SHADOWWIDTH);
    }
    if (Scr->Shadow) {
        XRaiseWindow(dpy, menu->shadow);
    }
    XMapRaised(dpy, menu->w);
    if (Scr->Shadow) {
        XMapWindow(dpy, menu->shadow);
    }
    XSync(dpy, 0);
    return True;
}

/**
 * unhighlight the current menu selection and take down the menus
 */
void
PopDownMenu(void)
{
    MenuRoot *tmp;

    if (ActiveMenu == NULL)
        return;

    if (ActiveItem) {
        ActiveItem->state = 0;
        PaintEntry(ActiveMenu, ActiveItem, False);
    }

    for (tmp = ActiveMenu; tmp != NULL; tmp = tmp->prev) {
        if (Scr->Shadow) {
            XUnmapWindow(dpy, tmp->shadow);
        }
        XUnmapWindow(dpy, tmp->w);
        tmp->mapped = UNMAPPED;
        UninstallRootColormap();
    }

    XFlush(dpy);
    ActiveMenu = NULL;
    ActiveItem = NULL;
    MenuDepth = 0;
    if (Context == C_WINDOW || Context == C_FRAME || Context == C_TITLE)
        menuFromFrameOrWindowOrTitlebar = TRUE;
}

/**
 * look for a menu root
 *
 *  \return a pointer to the menu root structure
 *
 *  \param name the name of the menu root
 */
MenuRoot *
FindMenuRoot(const char *name)
{
    MenuRoot *tmp;

    for (tmp = Scr->MenuList; tmp != NULL; tmp = tmp->next) {
        if (strcmp(name, tmp->name) == 0)
            return (tmp);
    }
    return NULL;
}

static Bool
belongs_to_twm_window(TwmWindow *t, Window w)
{
    if (!t)
        return False;

    if (w == t->frame || w == t->title_w || w == t->hilite_w ||
        w == t->icon_w || w == t->icon_bm_w)
        return True;

    if (t && t->titlebuttons) {
        register TBWindow *tbw;
        register int nb = Scr->TBInfo.nleft + Scr->TBInfo.nright;

        for (tbw = t->titlebuttons; nb > 0; tbw++, nb--) {
            if (tbw->window == w)
                return True;
        }
    }
    return False;
}

static void
resizeFromCenter(Window w, TwmWindow *tmp_win)
{
    int lastx, lasty, bw2;
    XEvent event;

#if 0
    int namelen;
    int width, height;

    namelen = strlen(tmp_win->name);
#endif
    bw2 = tmp_win->frame_bw * 2;
    AddingW = tmp_win->attr.width + bw2;
    AddingH = tmp_win->attr.height + tmp_win->title_height + bw2;
#if 0
    width = (SIZE_HINDENT + MyFont_TextWidth(&Scr->SizeFont,
                                             tmp_win->name, namelen));
    height = Scr->SizeFont.height + SIZE_VINDENT * 2;
#endif
    XGetGeometry(dpy, w, &JunkRoot, &origDragX, &origDragY,
                 (unsigned int *) &DragWidth, (unsigned int *) &DragHeight,
                 &JunkBW, &JunkDepth);
    XWarpPointer(dpy, None, w, 0, 0, 0, 0, DragWidth / 2, DragHeight / 2);
    XQueryPointer(dpy, Scr->Root, &JunkRoot,
                  &JunkChild, &JunkX, &JunkY, &AddingX, &AddingY, &JunkMask);
#if 0
    Scr->SizeStringOffset = width + MyFont_TextWidth(&Scr->SizeFont, ": ", 2);
    XResizeWindow(dpy, Scr->SizeWindow, Scr->SizeStringOffset +
                  Scr->SizeStringWidth, height);
    MyFont_DrawImageString(dpy, Scr->SizeWindow, &Scr->SizeFont, Scr->NormalGC,
                           width, SIZE_VINDENT + Scr->SizeFont.ascent, ": ", 2);
#endif
    lastx = -10000;
    lasty = -10000;
#if 0
    MoveOutline(Scr->Root,
                origDragX - JunkBW, origDragY - JunkBW,
                DragWidth * JunkBW, DragHeight * JunkBW,
                tmp_win->frame_bw, tmp_win->title_height);
#endif
    MenuStartResize(tmp_win, origDragX, origDragY, DragWidth, DragHeight);
    while (TRUE) {
        XMaskEvent(dpy, ButtonPressMask | PointerMotionMask, &event);

        if (event.type == MotionNotify) {
            /* discard any extra motion events before a release */
            while (XCheckMaskEvent(dpy,
                                   ButtonMotionMask | ButtonPressMask, &event))
                if (event.type == ButtonPress)
                    break;
        }

        if (event.type == ButtonPress) {
            MenuEndResize(tmp_win);
            XMoveResizeWindow(dpy, w, AddingX, AddingY, (unsigned) AddingW,
                              (unsigned) AddingH);
            break;
        }

        /* if (!DispatchEvent ()) continue; */

        if (event.type != MotionNotify) {
            continue;
        }

        /*
         * XXX - if we are going to do a loop, we ought to consider
         * using multiple GXxor lines so that we don't need to
         * grab the server.
         */
        XQueryPointer(dpy, Scr->Root, &JunkRoot, &JunkChild,
                      &JunkX, &JunkY, &AddingX, &AddingY, &JunkMask);

        if (lastx != AddingX || lasty != AddingY) {
            MenuDoResize(AddingX, AddingY, tmp_win);

            lastx = AddingX;
            lasty = AddingY;
        }

    }
}

/** \fn ExecuteFunction
 * execute a twm root function.
 *
 *  \param func     the function to execute
 *  \param action   the menu action to execute
 *  \param w        the window to execute this function on
 *  \param tmp_win  the twm window structure
 *  \param event    the event that caused the function
 *  \param context  the context in which the button was pressed
 *  \param pulldown flag indicating execution from pull down menu
 *
 *  \return TRUE if should continue with remaining actions,
 *           else FALSE to abort
 */

static int
WarpThere(TwmWindow *t)
{
    if (Scr->WarpUnmapped || t->mapped) {
        if (!t->mapped)
            DeIconify(t);
        if (!Scr->NoRaiseWarp)
            XRaiseWindow(dpy, t->frame);
        WarpToWindow(t);
        return 1;
    }
    return 0;
}

int
ExecuteFunction(int func, const char *action, Window w, TwmWindow *tmp_win,
                XEvent *eventp, int context, int pulldown)
{
    static Time last_time = 0;
    char tmp[200];
    char *ptr;
    char buff[MAX_FILE_SIZE];
    int count, fd;
    Window rootw;
    int origX, origY;
    int do_next_action = TRUE;
    int moving_icon = FALSE;
    Bool fromtitlebar = False;

    RootFunction = 0;
    if (Cancel)
        return TRUE;            /* XXX should this be FALSE? */

    switch (func) {
    case F_UPICONMGR:
    case F_LEFTICONMGR:
    case F_RIGHTICONMGR:
    case F_DOWNICONMGR:
    case F_FORWICONMGR:
    case F_BACKICONMGR:
    case F_NEXTICONMGR:
    case F_PREVICONMGR:
    case F_NOP:
    case F_TITLE:
    case F_DELTASTOP:
    case F_RAISELOWER:
    case F_WARPTOSCREEN:
    case F_WARPTO:
    case F_WARPRING:
    case F_WARPTOICONMGR:
    case F_WARPNEXT:
    case F_WARPPREV:
    case F_COLORMAP:
        break;
    default:
        XGrabPointer(dpy, Scr->Root, True,
                     ButtonPressMask | ButtonReleaseMask,
                     GrabModeAsync, GrabModeAsync,
                     Scr->Root, Scr->WaitCursor, CurrentTime);
        break;
    }

    switch (func) {
    case F_NOP:
    case F_TITLE:
        break;

    case F_DELTASTOP:
        if (WindowMoved)
            do_next_action = FALSE;
        break;

    case F_RESTART:
    {
        XSync(dpy, 0);
        Reborder(eventp->xbutton.time);
        XSync(dpy, 0);
        if (smcConn)
            SmcCloseConnection(smcConn, 0, NULL);
        execvp(*Argv, Argv);
        twmWarning("unable to restart:  %s", *Argv);
        break;
    }

    case F_UPICONMGR:
    case F_DOWNICONMGR:
    case F_LEFTICONMGR:
    case F_RIGHTICONMGR:
    case F_FORWICONMGR:
    case F_BACKICONMGR:
        MoveIconManager(func);
        break;

    case F_NEXTICONMGR:
    case F_PREVICONMGR:
        JumpIconManager(func);
        break;

    case F_SHOWLIST:
        if (Scr->NoIconManagers)
            break;
        DeIconify(Scr->iconmgr.twm_win);
        XRaiseWindow(dpy, Scr->iconmgr.twm_win->frame);
        break;

    case F_HIDELIST:
        if (Scr->NoIconManagers)
            break;
        HideIconManager();
        break;

    case F_SORTICONMGR:
        if (DeferExecution(context, func, Scr->SelectCursor))
            return TRUE;

        {
            int save_sort;

            save_sort = Scr->SortIconMgr;
            Scr->SortIconMgr = TRUE;

            if (context == C_ICONMGR)
                SortIconManager((IconMgr *) NULL);
            else if (tmp_win->iconmgr)
                SortIconManager(tmp_win->iconmgrp);
            else
                Bell(XkbBI_Info, 0, tmp_win->w);

            Scr->SortIconMgr = (short) save_sort;
        }
        break;

    case F_IDENTIFY:
        if (DeferExecution(context, func, Scr->SelectCursor))
            return TRUE;

        Identify(tmp_win);
        break;

    case F_VERSION:
        Identify((TwmWindow *) NULL);
        break;

    case F_AUTORAISE:
        if (DeferExecution(context, func, Scr->SelectCursor))
            return TRUE;

        tmp_win->auto_raise = !tmp_win->auto_raise;
        if (tmp_win->auto_raise)
            ++(Scr->NumAutoRaises);
        else
            --(Scr->NumAutoRaises);
        break;

    case F_BEEP:
        Bell(XkbBI_Info, 0, None);
        break;

    case F_POPUP:
        tmp_win = (TwmWindow *) action;
        if (Scr->WindowFunction.func != 0) {
            ExecuteFunction(Scr->WindowFunction.func,
                            Scr->WindowFunction.item->action,
                            w, tmp_win, eventp, C_FRAME, FALSE);
        }
        else {
            DeIconify(tmp_win);
            XRaiseWindow(dpy, tmp_win->frame);
        }
        break;

    case F_RESIZE:
        EventHandler[EnterNotify] = HandleUnknown;
        EventHandler[LeaveNotify] = HandleUnknown;
        if (DeferExecution(context, func, Scr->MoveCursor))
            return TRUE;

        PopDownMenu();

        if (pulldown)
            XWarpPointer(dpy, None, Scr->Root,
                         0, 0, 0, 0, eventp->xbutton.x_root,
                         eventp->xbutton.y_root);

        if (w != tmp_win->icon_w) {     /* can't resize icons */

            if ((Context == C_FRAME || Context == C_WINDOW ||
                 Context == C_TITLE)
                && fromMenu)
                resizeFromCenter(w, tmp_win);
            else {
                /*
                 * see if this is being done from the titlebar
                 */
                fromtitlebar =
                    belongs_to_twm_window(tmp_win, eventp->xbutton.window);

                /* Save pointer position so we can tell if it was moved or
                   not during the resize. */
                ResizeOrigX = eventp->xbutton.x_root;
                ResizeOrigY = eventp->xbutton.y_root;

                StartResize(eventp, tmp_win, fromtitlebar);

                do {
                    XMaskEvent(dpy,
                               ButtonPressMask | ButtonReleaseMask |
                               EnterWindowMask | LeaveWindowMask |
                               ButtonMotionMask, &Event);

                    if (fromtitlebar && Event.type == ButtonPress) {
                        fromtitlebar = False;
                        continue;
                    }

                    if (Event.type == MotionNotify) {
                        /* discard any extra motion events before a release */
                        while
                            (XCheckMaskEvent
                             (dpy, ButtonMotionMask | ButtonReleaseMask,
                              &Event))
                            if (Event.type == ButtonRelease)
                                break;
                    }

                    if (!DispatchEvent())
                        continue;

                } while (!(Event.type == ButtonRelease || Cancel));
                return TRUE;
            }
        }
        break;

    case F_ZOOM:
    case F_HORIZOOM:
    case F_FULLZOOM:
    case F_LEFTZOOM:
    case F_RIGHTZOOM:
    case F_TOPZOOM:
    case F_BOTTOMZOOM:
        if (DeferExecution(context, func, Scr->SelectCursor))
            return TRUE;
        fullzoom(tmp_win, func);
        break;

    case F_MOVE:
    case F_FORCEMOVE:
        if (DeferExecution(context, func, Scr->MoveCursor))
            return TRUE;

        PopDownMenu();
        rootw = eventp->xbutton.root;
        MoveFunction = func;

        if (pulldown)
            XWarpPointer(dpy, None, Scr->Root,
                         0, 0, 0, 0, eventp->xbutton.x_root,
                         eventp->xbutton.y_root);

        EventHandler[EnterNotify] = HandleUnknown;
        EventHandler[LeaveNotify] = HandleUnknown;

        if (!Scr->NoGrabServer || !Scr->OpaqueMove) {
            XGrabServer(dpy);
        }
        XGrabPointer(dpy, eventp->xbutton.root, True, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | PointerMotionMask,       /* PointerMotionHintMask */
                     GrabModeAsync, GrabModeAsync,
                     Scr->Root, Scr->MoveCursor, CurrentTime);

        if (context == C_ICON && tmp_win->icon_w) {
            w = tmp_win->icon_w;
            DragX = eventp->xbutton.x;
            DragY = eventp->xbutton.y;
            moving_icon = TRUE;
        }

        else if (w != tmp_win->icon_w) {
            XTranslateCoordinates(dpy, w, tmp_win->frame,
                                  eventp->xbutton.x,
                                  eventp->xbutton.y,
                                  &DragX, &DragY, &JunkChild);

            w = tmp_win->frame;
        }

        DragWindow = None;

        XGetGeometry(dpy, w, &JunkRoot, &origDragX, &origDragY,
                     (unsigned int *) &DragWidth, (unsigned int *) &DragHeight,
                     &JunkBW, &JunkDepth);

        origX = eventp->xbutton.x_root;
        origY = eventp->xbutton.y_root;
        CurrentDragX = origDragX;
        CurrentDragY = origDragY;

        /*
         * only do the constrained move if timer is set; need to check it
         * in case of stupid or wicked fast servers
         */
        if (ConstrainedMoveTime &&
            (eventp->xbutton.time - last_time) < (Time) ConstrainedMoveTime) {
            int width, height;

            ConstMove = TRUE;
            ConstMoveDir = MOVE_NONE;
            ConstMoveX =
                (int) ((unsigned) eventp->xbutton.x_root - (unsigned) DragX -
                       JunkBW);
            ConstMoveY =
                (int) ((unsigned) eventp->xbutton.y_root - (unsigned) DragY -
                       JunkBW);
            width = (int) ((unsigned) DragWidth + 2 * JunkBW);
            height = (int) ((unsigned) DragHeight + 2 * JunkBW);
            ConstMoveXL = ConstMoveX + width / 3;
            ConstMoveXR = ConstMoveX + 2 * (width / 3);
            ConstMoveYT = ConstMoveY + height / 3;
            ConstMoveYB = ConstMoveY + 2 * (height / 3);

            XWarpPointer(dpy, None, w,
                         0, 0, 0, 0, DragWidth / 2, DragHeight / 2);

            XQueryPointer(dpy, w, &JunkRoot, &JunkChild,
                          &JunkX, &JunkY, &DragX, &DragY, &JunkMask);
        }
        last_time = eventp->xbutton.time;

        if (!Scr->OpaqueMove) {
            InstallRootColormap();
            if (!Scr->MoveDelta) {
                /*
                 * Draw initial outline.  This was previously done the
                 * first time though the outer loop by dropping out of
                 * the XCheckMaskEvent inner loop down to one of the
                 * MoveOutline's below.
                 */
                MoveOutline(rootw,
                            (int) ((unsigned) origDragX - JunkBW),
                            (int) ((unsigned) origDragY - JunkBW),
                            (int) ((unsigned) DragWidth + 2 * JunkBW),
                            (int) ((unsigned) DragHeight + 2 * JunkBW),
                            tmp_win->frame_bw,
                            moving_icon ? 0 : tmp_win->title_height);
                /*
                 * This next line causes HandleReleaseNotify to call
                 * XRaiseWindow().  This is solely to preserve the
                 * previous behaviour that raises a window being moved
                 * on button release even if you never actually moved
                 * any distance (unless you move less than MoveDelta or
                 * NoRaiseMove is set or OpaqueMove is set).
                 */
                DragWindow = w;
            }
        }

        /*
         * see if this is being done from the titlebar
         */
        fromtitlebar = belongs_to_twm_window(tmp_win, eventp->xbutton.window);

        if (menuFromFrameOrWindowOrTitlebar) {
            /* warp the pointer to the middle of the window */
            XWarpPointer(dpy, None, Scr->Root, 0, 0, 0, 0,
                         origDragX + DragWidth / 2, origDragY + DragHeight / 2);
            XFlush(dpy);
        }

        while (TRUE) {
            long releaseEvent = menuFromFrameOrWindowOrTitlebar ?
                ButtonPress : ButtonRelease;
            long movementMask = menuFromFrameOrWindowOrTitlebar ?
                PointerMotionMask : ButtonMotionMask;

            /* block until there is an interesting event */
            XMaskEvent(dpy, ButtonPressMask | ButtonReleaseMask |
                       EnterWindowMask | LeaveWindowMask |
                       ExposureMask | movementMask |
                       VisibilityChangeMask, &Event);

            /* throw away enter and leave events until release */
            if (Event.xany.type == EnterNotify ||
                Event.xany.type == LeaveNotify)
                continue;

            if (Event.type == MotionNotify) {
                /* discard any extra motion events before a logical release */
                while (XCheckMaskEvent(dpy,
                                       movementMask | releaseEvent, &Event))
                    if (Event.type == releaseEvent)
                        break;
            }

            /* test to see if we have a second button press to abort move */
            if (!menuFromFrameOrWindowOrTitlebar && !MovedFromKeyPress) {
                if (Event.type == ButtonPress && DragWindow != None) {
                    if (Scr->OpaqueMove)
                        XMoveWindow(dpy, DragWindow, origDragX, origDragY);
                    else
                        MoveOutline(Scr->Root, 0, 0, 0, 0, 0, 0);
                    DragWindow = None;
                }
            }
            if (fromtitlebar && Event.type == ButtonPress) {
                fromtitlebar = False;
                CurrentDragX = origX = Event.xbutton.x_root;
                CurrentDragY = origY = Event.xbutton.y_root;

                XTranslateCoordinates(dpy, rootw, tmp_win->frame,
                                      origX, origY, &DragX, &DragY, &JunkChild);
                continue;
            }

            if (!DispatchEvent2())
                continue;

            if (Cancel) {
                WindowMoved = FALSE;
                if (!Scr->OpaqueMove)
                    UninstallRootColormap();
                return TRUE;    /* XXX should this be FALSE? */
            }
            if (Event.type == releaseEvent) {
                MoveOutline(rootw, 0, 0, 0, 0, 0, 0);
                if (moving_icon &&
                    ((CurrentDragX != origDragX || CurrentDragY != origDragY)))
                    tmp_win->icon_moved = TRUE;
                if (!Scr->OpaqueMove && menuFromFrameOrWindowOrTitlebar)
                    XMoveWindow(dpy, DragWindow,
                                Event.xbutton.x_root - DragWidth / 2,
                                Event.xbutton.y_root - DragHeight / 2);
                break;
            }

            /* something left to do only if the pointer moved */
            if (Event.type != MotionNotify)
                continue;

            XQueryPointer(dpy, rootw, &(eventp->xmotion.root), &JunkChild,
                          &(eventp->xmotion.x_root), &(eventp->xmotion.y_root),
                          &JunkX, &JunkY, &JunkMask);

            if (DragWindow == None &&
                abs(eventp->xmotion.x_root - origX) < Scr->MoveDelta &&
                abs(eventp->xmotion.y_root - origY) < Scr->MoveDelta)
                continue;

            WindowMoved = TRUE;
            DragWindow = w;

            if (!Scr->NoRaiseMove && Scr->OpaqueMove)   /* can't restore... */
                XRaiseWindow(dpy, DragWindow);

            if (ConstMove) {
                switch (ConstMoveDir) {
                case MOVE_NONE:
                    if (eventp->xmotion.x_root < ConstMoveXL ||
                        eventp->xmotion.x_root > ConstMoveXR)
                        ConstMoveDir = MOVE_HORIZ;

                    if (eventp->xmotion.y_root < ConstMoveYT ||
                        eventp->xmotion.y_root > ConstMoveYB)
                        ConstMoveDir = MOVE_VERT;

                    XQueryPointer(dpy, DragWindow, &JunkRoot, &JunkChild,
                                  &JunkX, &JunkY, &DragX, &DragY, &JunkMask);
                    break;

                case MOVE_VERT:
                    ConstMoveY =
                        (int) ((unsigned) eventp->xmotion.y_root -
                               (unsigned) DragY - JunkBW);
                    break;

                case MOVE_HORIZ:
                    ConstMoveX =
                        (int) ((unsigned) eventp->xmotion.x_root -
                               (unsigned) DragX - JunkBW);
                    break;
                }

                if (ConstMoveDir != MOVE_NONE) {
                    int xl, yt, w2, h;

                    xl = ConstMoveX;
                    yt = ConstMoveY;
                    w2 = (int) ((unsigned) DragWidth + 2 * JunkBW);
                    h = (int) ((unsigned) DragHeight + 2 * JunkBW);

                    if (Scr->DontMoveOff && MoveFunction != F_FORCEMOVE) {
                        int xr = xl + w2;
                        int yb = yt + h;

                        if (xl < 0)
                            xl = 0;
                        if (xr > Scr->MyDisplayWidth)
                            xl = Scr->MyDisplayWidth - w2;

                        if (yt < 0)
                            yt = 0;
                        if (yb > Scr->MyDisplayHeight)
                            yt = Scr->MyDisplayHeight - h;
                    }
                    CurrentDragX = xl;
                    CurrentDragY = yt;
                    if (Scr->OpaqueMove)
                        XMoveWindow(dpy, DragWindow, xl, yt);
                    else
                        MoveOutline(eventp->xmotion.root, xl, yt, w2, h,
                                    tmp_win->frame_bw,
                                    moving_icon ? 0 : tmp_win->title_height);
                }
            }
            else if (DragWindow != None) {
                int xl, yt, w2, h;

                if (!menuFromFrameOrWindowOrTitlebar) {
                    xl = (int) ((unsigned) eventp->xmotion.x_root -
                                (unsigned) DragX - JunkBW);
                    yt = (int) ((unsigned) eventp->xmotion.y_root -
                                (unsigned) DragY - JunkBW);
                }
                else {
                    xl = (int) (eventp->xmotion.x_root - (DragWidth / 2));
                    yt = (int) (eventp->xmotion.y_root - (DragHeight / 2));
                }
                w2 = (int) ((unsigned) DragWidth + 2 * JunkBW);
                h = (int) ((unsigned) DragHeight + 2 * JunkBW);

                if (Scr->DontMoveOff && MoveFunction != F_FORCEMOVE) {
                    int xr = xl + w2;
                    int yb = yt + h;

                    if (xl < 0)
                        xl = 0;
                    if (xr > Scr->MyDisplayWidth)
                        xl = Scr->MyDisplayWidth - w2;

                    if (yt < 0)
                        yt = 0;
                    if (yb > Scr->MyDisplayHeight)
                        yt = Scr->MyDisplayHeight - h;
                }

                CurrentDragX = xl;
                CurrentDragY = yt;
                if (Scr->OpaqueMove)
                    XMoveWindow(dpy, DragWindow, xl, yt);
                else
                    MoveOutline(eventp->xmotion.root, xl, yt, w2, h,
                                tmp_win->frame_bw,
                                moving_icon ? 0 : tmp_win->title_height);
            }

        }
        MovedFromKeyPress = False;

        if (!Scr->OpaqueMove && DragWindow == None)
            UninstallRootColormap();

        break;

    case F_FUNCTION:
    {
        MenuRoot *mroot;
        MenuItem *mitem;

        if ((mroot = FindMenuRoot(action)) == NULL) {
            twmWarning("couldn't find function \"%s\"", action);
            return TRUE;
        }

        if (NeedToDefer(mroot) &&
            DeferExecution(context, func, Scr->SelectCursor))
            return TRUE;
        else {
            for (mitem = mroot->first; mitem != NULL; mitem = mitem->next) {
                if (!ExecuteFunction(mitem->func, mitem->action, w,
                                     tmp_win, eventp, context, pulldown))
                    break;
            }
        }
    }
        break;

    case F_DEICONIFY:
    case F_ICONIFY:
        if (DeferExecution(context, func, Scr->SelectCursor))
            return TRUE;

        if (tmp_win->icon) {
            DeIconify(tmp_win);
        }
        else if (func == F_ICONIFY) {
            Iconify(tmp_win, eventp->xbutton.x_root - 5,
                    eventp->xbutton.y_root - 5);
        }
        break;

    case F_RAISELOWER:
        if (DeferExecution(context, func, Scr->SelectCursor))
            return TRUE;

        if (!WindowMoved) {
            XWindowChanges xwc;

            xwc.stack_mode = Opposite;
            if (w != tmp_win->icon_w)
                w = tmp_win->frame;
            XConfigureWindow(dpy, w, CWStackMode, &xwc);
        }
        break;

    case F_RAISE:
        if (DeferExecution(context, func, Scr->SelectCursor))
            return TRUE;

        /* check to make sure raise is not from the WindowFunction */
        if (w == tmp_win->icon_w && Context != C_ROOT)
            XRaiseWindow(dpy, tmp_win->icon_w);
        else
            XRaiseWindow(dpy, tmp_win->frame);

        break;

    case F_LOWER:
        if (DeferExecution(context, func, Scr->SelectCursor))
            return TRUE;

        if (w == tmp_win->icon_w)
            XLowerWindow(dpy, tmp_win->icon_w);
        else
            XLowerWindow(dpy, tmp_win->frame);

        break;

    case F_FOCUS:
        if (DeferExecution(context, func, Scr->SelectCursor))
            return TRUE;

        if (tmp_win->icon == FALSE) {
            if (!Scr->FocusRoot && Scr->Focus == tmp_win) {
                FocusOnRoot();
            }
            else {
                if (Scr->Focus != NULL) {
                    SetBorder(Scr->Focus, False);
                    if (Scr->Focus->hilite_w)
                        XUnmapWindow(dpy, Scr->Focus->hilite_w);
                }

                InstallWindowColormaps(0, tmp_win);
                if (tmp_win->hilite_w)
                    XMapWindow(dpy, tmp_win->hilite_w);
                SetBorder(tmp_win, True);
                if (!tmp_win->wmhints || tmp_win->wmhints->input)
                    SetFocus(tmp_win, eventp->xbutton.time);
                Scr->FocusRoot = FALSE;
                Scr->Focus = tmp_win;
            }
        }
        break;

    case F_DESTROY:
        if (DeferExecution(context, func, Scr->DestroyCursor))
            return TRUE;

        if (tmp_win->iconmgr)
            Bell(XkbBI_MinorError, 0, tmp_win->w);
        else
            XKillClient(dpy, tmp_win->w);
        break;

    case F_DELETE:
        if (DeferExecution(context, func, Scr->DestroyCursor))
            return TRUE;

        if (tmp_win->iconmgr)   /* don't send ourself a message */
            HideIconManager();
        else if (tmp_win->protocols & DoesWmDeleteWindow)
            SendDeleteWindowMessage(tmp_win, LastTimestamp());
        else
            Bell(XkbBI_MinorError, 0, tmp_win->w);
        break;

    case F_SAVEYOURSELF:
        if (DeferExecution(context, func, Scr->SelectCursor))
            return TRUE;

        if (tmp_win->protocols & DoesWmSaveYourself)
            SendSaveYourselfMessage(tmp_win, LastTimestamp());
        else
            Bell(XkbBI_MinorError, 0, tmp_win->w);
        break;

    case F_CIRCLEUP:
        XCirculateSubwindowsUp(dpy, Scr->Root);
        break;

    case F_CIRCLEDOWN:
        XCirculateSubwindowsDown(dpy, Scr->Root);
        break;

    case F_EXEC:
        PopDownMenu();
        if (!Scr->NoGrabServer) {
            XUngrabServer(dpy);
            XSync(dpy, 0);
        }
        Execute(action);
        break;

    case F_UNFOCUS:
        FocusOnRoot();
        break;

    case F_CUT:
        strcpy(tmp, action);
        strcat(tmp, "\n");
        XStoreBytes(dpy, tmp, (int) strlen(tmp));
        break;

    case F_CUTFILE:
        ptr = XFetchBytes(dpy, &count);
        if (ptr) {
            if (sscanf(ptr, "%s", tmp) == 1) {
                XFree(ptr);
                ptr = ExpandFilename(tmp);
                if (ptr) {
                    fd = open(ptr, O_RDONLY);
                    if (fd >= 0) {
                        count = (int) read(fd, buff, MAX_FILE_SIZE - 1);
                        if (count > 0)
                            XStoreBytes(dpy, buff, count);
                        close(fd);
                    }
                    else {
                        twmWarning("unable to open cut file \"%s\"", tmp);
                    }
                    free(ptr);
                }
            }
            else {
                XFree(ptr);
            }
        }
        else {
            twmWarning("cut buffer is empty");
        }
        break;

    case F_WARPTOSCREEN:
    {
        if (strcmp(action, WARPSCREEN_NEXT) == 0) {
            WarpToScreen(Scr->screen + 1, 1);
        }
        else if (strcmp(action, WARPSCREEN_PREV) == 0) {
            WarpToScreen(Scr->screen - 1, -1);
        }
        else if (strcmp(action, WARPSCREEN_BACK) == 0) {
            WarpToScreen(PreviousScreen, 0);
        }
        else {
            WarpToScreen(atoi(action), 0);
        }
    }
        break;

    case F_COLORMAP:
    {
        if (strcmp(action, COLORMAP_NEXT) == 0) {
            BumpWindowColormap(tmp_win, 1);
        }
        else if (strcmp(action, COLORMAP_PREV) == 0) {
            BumpWindowColormap(tmp_win, -1);
        }
        else {
            BumpWindowColormap(tmp_win, 0);
        }
    }
        break;

    case F_WARPPREV:
    case F_WARPNEXT:
    {
        register TwmWindow *t;
        TwmWindow *of, *l, *n;
        int c = 0;

#define wseq(w) (func == F_WARPNEXT ? (w)->next : (w)->prev)
#define nwin(w) ((w) && (n=wseq(w)) != NULL && n != &Scr->TwmRoot ? n : l)
#define bwin(w) (!(w)||(w)->iconmgr||(w)==of||!(Scr->WarpUnmapped||(w)->mapped))

        of = (Scr->Focus ? Scr->Focus : &Scr->TwmRoot);

        for (t = Scr->TwmRoot.next; t; t = t->next)
            if (!bwin(t))
                break;
        if (!t)
            break;              /* no windows we can use */

        if (func == F_WARPPREV)
            for (l = of; l->next; l = l->next);
        else
            l = Scr->TwmRoot.next;

        for (t = of; bwin(t) && c < 2; t = nwin(t))
            if (t == of)
                c++;

        if (bwin(t) || c >= 2) {
            Bell(XkbBI_MinorError, 0, None);
        }
        else {
            static TwmWindow *savedwarp = NULL;

            if (of && of == savedwarp) {
                Iconify(of, 0, 0);
                savedwarp = NULL;
            }
            if (!t->mapped)
                savedwarp = t;
            else
                savedwarp = NULL;
            WarpThere(t);
        }
        break;
    }

    case F_WARPTO:
    {
        register TwmWindow *t;
        int len;

        len = (int) strlen(action);

        for (t = Scr->TwmRoot.next; t != NULL; t = t->next) {
            if (!strncmp(action, t->name, (size_t) len))
                if (WarpThere(t))
                    break;
        }
        if (!t) {
            for (t = Scr->TwmRoot.next; t != NULL; t = t->next) {
                if (!strncmp(action, t->class.res_name, (size_t) len))
                    if (WarpThere(t))
                        break;
            }
            if (!t) {
                for (t = Scr->TwmRoot.next; t != NULL; t = t->next) {
                    if (!strncmp(action, t->class.res_class, (size_t) len))
                        if (WarpThere(t))
                            break;
                }
            }
        }

        if (!t)
            Bell(XkbBI_MinorError, 0, None);
    }
        break;

    case F_WARPTOICONMGR:
    {
        TwmWindow *t;
        int len;
        Window raisewin = None, iconwin = None;

        len = (int) strlen(action);
        if (len == 0) {
            if (tmp_win && tmp_win->list) {
                raisewin = tmp_win->list->iconmgr->twm_win->frame;
                iconwin = tmp_win->list->icon;
            }
            else if (Scr->iconmgr.active) {
                raisewin = Scr->iconmgr.twm_win->frame;
                iconwin = Scr->iconmgr.active->w;
            }
        }
        else {
            for (t = Scr->TwmRoot.next; t != NULL; t = t->next) {
                if (strncmp(action, t->icon_name, (size_t) len) == 0) {
                    if (t->list && t->list->iconmgr->twm_win->mapped) {
                        raisewin = t->list->iconmgr->twm_win->frame;
                        iconwin = t->list->icon;
                        break;
                    }
                }
            }
        }

        if (raisewin) {
            XRaiseWindow(dpy, raisewin);
            XWarpPointer(dpy, None, iconwin, 0, 0, 0, 0, 5, 5);
        }
        else {
            Bell(XkbBI_MinorError, 0, None);
        }
    }
        break;

    case F_WARPRING:
        switch (action[0]) {
        case 'n':
            WarpAlongRing(&eventp->xbutton, True);
            break;
        case 'p':
            WarpAlongRing(&eventp->xbutton, False);
            break;
        default:
            Bell(XkbBI_MinorError, 0, None);
            break;
        }
        break;

    case F_FILE:
        ptr = ExpandFilename(action);
        if (ptr) {
            fd = open(ptr, O_RDONLY);
            if (fd >= 0) {
                count = (int) read(fd, buff, MAX_FILE_SIZE - 1);
                if (count > 0)
                    XStoreBytes(dpy, buff, count);

                close(fd);
            }
            else {
                twmWarning("unable to open file \"%s\"", ptr);
            }
            free(ptr);
        }
        else {
            twmWarning("error expanding filename");
        }
        break;

    case F_REFRESH:
    {
        XSetWindowAttributes attributes;
        unsigned long valuemask;

        valuemask = (CWBackPixel | CWBackingStore | CWSaveUnder);
        attributes.background_pixel = Scr->Black;
        attributes.backing_store = NotUseful;
        attributes.save_under = False;
        w = XCreateWindow(dpy, Scr->Root, 0, 0,
                          (unsigned int) Scr->MyDisplayWidth,
                          (unsigned int) Scr->MyDisplayHeight,
                          (unsigned int) 0,
                          CopyFromParent, (unsigned int) CopyFromParent,
                          (Visual *) CopyFromParent, valuemask, &attributes);
        XMapWindow(dpy, w);
        XDestroyWindow(dpy, w);
        XFlush(dpy);
    }
        break;

    case F_WINREFRESH:
        if (DeferExecution(context, func, Scr->SelectCursor))
            return TRUE;

        if (context == C_ICON && tmp_win->icon_w)
            w = XCreateSimpleWindow(dpy, tmp_win->icon_w,
                                    0, 0, 9999, 9999, 0, Scr->Black,
                                    Scr->Black);
        else
            w = XCreateSimpleWindow(dpy, tmp_win->frame,
                                    0, 0, 9999, 9999, 0, Scr->Black,
                                    Scr->Black);

        XMapWindow(dpy, w);
        XDestroyWindow(dpy, w);
        XFlush(dpy);
        break;

    case F_QUIT:
        Done(NULL, NULL);
        break;

    case F_PRIORITY:
        if (HasSync) {
            if (DeferExecution(context, func, Scr->SelectCursor))
                return TRUE;
            (void) XSyncSetPriority(dpy, tmp_win->w, atoi(action));
        }
        break;
    case F_STARTWM:
        execlp("/bin/sh", "sh", "-c", action, (void *) NULL);
        twmWarning("unable to start:  %s", *Argv);
        break;

    }

    if (ButtonPressed == -1)
        XUngrabPointer(dpy, CurrentTime);
    return do_next_action;
}

/**
 * defer the execution of a function to the next button press if the context
 *  is C_ROOT
 *
 *  \param context the context in which the mouse button was pressed
 *      \param func    the function to defer
 *  \param cursor  cursor the cursor to display while waiting
 */
static int
DeferExecution(int context, int func, Cursor cursor)
{
    if (context == C_ROOT) {
        LastCursor = cursor;
        XGrabPointer(dpy, Scr->Root, True,
                     ButtonPressMask | ButtonReleaseMask,
                     GrabModeAsync, GrabModeAsync,
                     Scr->Root, cursor, CurrentTime);

        RootFunction = func;

        return (TRUE);
    }

    return (FALSE);
}

/**
 *regrab the pointer with the LastCursor;
 */
void
ReGrab(void)
{
    XGrabPointer(dpy, Scr->Root, True,
                 ButtonPressMask | ButtonReleaseMask,
                 GrabModeAsync, GrabModeAsync,
                 Scr->Root, LastCursor, CurrentTime);
}

/**
 * checks each function in the list to see if it is one that needs
 * to be deferred.
 *
 *  \param root the menu root to check
 */
static Bool
NeedToDefer(MenuRoot *root)
{
    MenuItem *mitem;

    for (mitem = root->first; mitem != NULL; mitem = mitem->next) {
        switch (mitem->func) {
        case F_IDENTIFY:
        case F_RESIZE:
        case F_MOVE:
        case F_FORCEMOVE:
        case F_DEICONIFY:
        case F_ICONIFY:
        case F_RAISELOWER:
        case F_RAISE:
        case F_LOWER:
        case F_FOCUS:
        case F_DESTROY:
        case F_WINREFRESH:
        case F_ZOOM:
        case F_FULLZOOM:
        case F_HORIZOOM:
        case F_RIGHTZOOM:
        case F_LEFTZOOM:
        case F_TOPZOOM:
        case F_BOTTOMZOOM:
        case F_AUTORAISE:
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * We cannot safely free a value passed to putenv, but we can cache the most
 * recent value, reducing the memory leaks.
 */
#define cache_env(saved) \
    if (saved == NULL || strcmp(saved, update)) { \
        saved = update; \
    } else { \
        free(update); \
        update = saved; \
    }

static void
Execute(const char *s)
{
    static const char display_eqls[] = "DISPLAY=";
    static char *main_display;
    static char *exec_display;

    char *ds = DisplayString(dpy);
    char *colon;
    char *oldDisplay = NULL;
    char *value;
    Bool restorevar = False;

    value = getenv("DISPLAY");
    oldDisplay = strdup(value ? value : "");

    /*
     * Build a display string using the current screen number, so that
     * X programs which get fired up from a menu come up on the screen
     * that they were invoked from, unless specifically overridden on
     * their command line.
     */
    colon = strrchr(ds, ':');
    if (colon) {                /* if host[:]:dpy */
        size_t need = sizeof(display_eqls) + strlen(ds) + 10;
        char *update = malloc(need);

        if (update != NULL) {
            char *dot1;

            strcpy(update, display_eqls);
            strcat(update, ds);
            colon = strrchr(update, ':');
            dot1 = strchr(colon, '.');  /* first period after colon */
            if (dot1 == NULL)
                dot1 = colon + strlen(colon);   /* if not there, append */
            (void) sprintf(dot1, ".%d", Scr->screen);
            cache_env(exec_display);
            if (strcmp(update, oldDisplay)) {
                putenv(update);
                restorevar = True;
            }
        }
    }

    (void) system(s);

    if (restorevar) {
        size_t need = sizeof(display_eqls) + strlen(oldDisplay);
        char *update = malloc(need);

        if (update != NULL) {
            (void) snprintf(update, need, "%s%s", display_eqls, oldDisplay);
            cache_env(main_display);
            putenv(update);
        }
    }
    free(oldDisplay);
}

/**
 * put input focus on the root window.
 */
void
FocusOnRoot(void)
{
    SetFocus((TwmWindow *) NULL, LastTimestamp());
    if (Scr->Focus != NULL) {
        SetBorder(Scr->Focus, False);
        if (Scr->Focus->hilite_w)
            XUnmapWindow(dpy, Scr->Focus->hilite_w);
    }
    InstallWindowColormaps(0, &Scr->TwmRoot);
    Scr->Focus = NULL;
    Scr->FocusRoot = TRUE;
}

void
DeIconify(TwmWindow *tmp_win)
{
    TwmWindow *t;

    /* de-iconify the main window */
    if (tmp_win->icon) {
        if (tmp_win->icon_on)
            Zoom(tmp_win->icon_w, tmp_win->frame);
        else if (tmp_win->group != (Window) 0) {
            for (t = Scr->TwmRoot.next; t != NULL; t = t->next) {
                if (tmp_win->group == t->w && t->icon_on) {
                    Zoom(t->icon_w, tmp_win->frame);
                    break;
                }
            }
        }
    }

    XMapWindow(dpy, tmp_win->w);
    tmp_win->mapped = TRUE;
    if (Scr->NoRaiseDeicon)
        XMapWindow(dpy, tmp_win->frame);
    else
        XMapRaised(dpy, tmp_win->frame);
    SetMapStateProp(tmp_win, NormalState);

    if (tmp_win->icon_w) {
        XUnmapWindow(dpy, tmp_win->icon_w);
        IconDown(tmp_win);
    }
    if (tmp_win->list)
        XUnmapWindow(dpy, tmp_win->list->icon);
    if ((Scr->WarpCursor ||
         LookInList(Scr->WarpCursorL, tmp_win->full_name, &tmp_win->class)) &&
        tmp_win->icon)
        WarpToWindow(tmp_win);
    tmp_win->icon = FALSE;
    tmp_win->icon_on = FALSE;

    /* now de-iconify transients */
    for (t = Scr->TwmRoot.next; t != NULL; t = t->next) {
        if (t->transient && t->transientfor == tmp_win->w) {
            if (t->icon_on)
                Zoom(t->icon_w, t->frame);
            else
                Zoom(tmp_win->icon_w, t->frame);

            XMapWindow(dpy, t->w);
            t->mapped = TRUE;
            if (Scr->NoRaiseDeicon)
                XMapWindow(dpy, t->frame);
            else
                XMapRaised(dpy, t->frame);
            SetMapStateProp(t, NormalState);

            if (t->icon_w) {
                XUnmapWindow(dpy, t->icon_w);
                IconDown(t);
            }
            if (t->list)
                XUnmapWindow(dpy, t->list->icon);
            t->icon = FALSE;
            t->icon_on = FALSE;
        }
    }

    XSync(dpy, 0);
}

void
Iconify(TwmWindow *tmp_win, int def_x, int def_y)
{
    TwmWindow *t;
    int iconify;
    XWindowAttributes winattrs;
    unsigned long eventMask;

    iconify = ((!tmp_win->iconify_by_unmapping) || tmp_win->transient);
    if (iconify) {
        if (tmp_win->icon_w == (Window) 0)
            CreateIconWindow(tmp_win, def_x, def_y);
        else
            IconUp(tmp_win);
        XMapRaised(dpy, tmp_win->icon_w);
    }
    if (tmp_win->list)
        XMapWindow(dpy, tmp_win->list->icon);

    XGetWindowAttributes(dpy, tmp_win->w, &winattrs);
    eventMask = (unsigned long) winattrs.your_event_mask;

    /* iconify transients first */
    for (t = Scr->TwmRoot.next; t != NULL; t = t->next) {
        if (t->transient && t->transientfor == tmp_win->w) {
            if (iconify) {
                if (t->icon_on)
                    Zoom(t->icon_w, tmp_win->icon_w);
                else
                    Zoom(t->frame, tmp_win->icon_w);
            }

            /*
             * Prevent the receipt of an UnmapNotify, since that would
             * cause a transition to the Withdrawn state.
             */
            t->mapped = FALSE;
            XSelectInput(dpy, t->w,
                         (long) (eventMask &
                                 (unsigned long) (~StructureNotifyMask)));
            XUnmapWindow(dpy, t->w);
            XSelectInput(dpy, t->w, (long) eventMask);
            XUnmapWindow(dpy, t->frame);
            if (t->icon_w)
                XUnmapWindow(dpy, t->icon_w);
            SetMapStateProp(t, IconicState);
            SetBorder(t, False);
            if (t == Scr->Focus) {
                SetFocus((TwmWindow *) NULL, LastTimestamp());
                Scr->Focus = NULL;
                Scr->FocusRoot = TRUE;
            }
            if (t->list)
                XMapWindow(dpy, t->list->icon);
            t->icon = TRUE;
            t->icon_on = FALSE;
        }
    }

    if (iconify)
        Zoom(tmp_win->frame, tmp_win->icon_w);

    /*
     * Prevent the receipt of an UnmapNotify, since that would
     * cause a transition to the Withdrawn state.
     */
    tmp_win->mapped = FALSE;
    XSelectInput(dpy, tmp_win->w,
                 (long) (eventMask & (unsigned long) (~StructureNotifyMask)));
    XUnmapWindow(dpy, tmp_win->w);
    XSelectInput(dpy, tmp_win->w, (long) eventMask);
    XUnmapWindow(dpy, tmp_win->frame);
    SetMapStateProp(tmp_win, IconicState);

    SetBorder(tmp_win, False);
    if (tmp_win == Scr->Focus) {
        SetFocus((TwmWindow *) NULL, LastTimestamp());
        Scr->Focus = NULL;
        Scr->FocusRoot = TRUE;
    }
    tmp_win->icon = TRUE;
    if (iconify)
        tmp_win->icon_on = TRUE;
    else
        tmp_win->icon_on = FALSE;
    XSync(dpy, 0);
}

static void
Identify(TwmWindow *t)
{
    int i, n, width, height;
    int x, y;
    unsigned int wwidth, wheight, bw, depth;
    Window junk;
    int px, py, dummy;
    unsigned udummy;

    n = 0;
    snprintf(Info[n++], INFO_SIZE, "Twm version:  %s", Version);
    Info[n++][0] = '\0';

    if (t) {
        XGetGeometry(dpy, t->w, &JunkRoot, &JunkX, &JunkY,
                     &wwidth, &wheight, &bw, &depth);
        (void) XTranslateCoordinates(dpy, t->w, Scr->Root, 0, 0, &x, &y, &junk);
        snprintf(Info[n++], INFO_SIZE,
                 "Name             = \"%s\"", t->full_name);
        snprintf(Info[n++], INFO_SIZE,
                 "Class.res_name   = \"%s\"", t->class.res_name);
        snprintf(Info[n++], INFO_SIZE,
                 "Class.res_class  = \"%s\"", t->class.res_class);
        Info[n++][0] = '\0';
        snprintf(Info[n++], INFO_SIZE,
                 "Geometry/root    = %ux%u+%d+%d", wwidth, wheight, x, y);
        snprintf(Info[n++], INFO_SIZE, "Border width     = %u", bw);
        snprintf(Info[n++], INFO_SIZE, "Depth            = %u", depth);
        if (HasSync) {
            int priority;

            (void) XSyncGetPriority(dpy, t->w, &priority);
            snprintf(Info[n++], INFO_SIZE, "Priority         = %d", priority);
        }
    }

    Info[n++][0] = '\0';
    snprintf(Info[n++], INFO_SIZE, "Click to dismiss....");

    /* figure out the width and height of the info window */
    height = n * (Scr->DefaultFont.height + 2);
    width = 1;
    for (i = 0; i < n; i++) {
        int twidth = MyFont_TextWidth(&Scr->DefaultFont, Info[i],
                                      (int) strlen(Info[i]));

        if (twidth > width)
            width = twidth;
    }
    if (InfoLines)
        XUnmapWindow(dpy, Scr->InfoWindow);

    width += 10;                /* some padding */
    if (XQueryPointer(dpy, Scr->Root, &JunkRoot, &JunkChild, &px, &py,
                      &dummy, &dummy, &udummy)) {
        px -= (width / 2);
        py -= (height / 3);
        if (px + width + BW2 >= Scr->MyDisplayWidth)
            px = Scr->MyDisplayWidth - width - BW2;
        if (py + height + BW2 >= Scr->MyDisplayHeight)
            py = Scr->MyDisplayHeight - height - BW2;
        if (px < 0)
            px = 0;
        if (py < 0)
            py = 0;
    }
    else {
        px = py = 0;
    }
    XMoveResizeWindow(dpy, Scr->InfoWindow, px, py, (unsigned) width,
                      (unsigned) height);
    XMapRaised(dpy, Scr->InfoWindow);
    InfoLines = n;
}

void
SetMapStateProp(TwmWindow *tmp_win, int state)
{
    unsigned long data[2];      /* "suggested" by ICCCM version 1 */

    data[0] = (unsigned long) state;
    data[1] = (unsigned long) (tmp_win->iconify_by_unmapping ? None :
                               tmp_win->icon_w);

    XChangeProperty(dpy, tmp_win->w, _XA_WM_STATE, _XA_WM_STATE, 32,
                    PropModeReplace, (unsigned char *) data, 2);
}

Bool
GetWMState(Window w, int *statep, Window *iwp)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytesafter;
    unsigned char *prop_return = NULL;
    Bool retval = False;

    if (XGetWindowProperty(dpy, w, _XA_WM_STATE, 0L, 2L, False, _XA_WM_STATE,
                           &actual_type, &actual_format, &nitems, &bytesafter,
                           &prop_return) != Success || !prop_return)
        return False;

    if (nitems <= 2) {          /* "suggested" by ICCCM version 1 */
        unsigned long *datap = (unsigned long *) prop_return;

        *statep = (int) datap[0];
        *iwp = (Window) datap[1];
        retval = True;
    }

    XFree(prop_return);
    return retval;
}

void
WarpToScreen(int n, int inc)
{
    Window dumwin;
    int x, y, dumint;
    unsigned int dummask;
    ScreenInfo *newscr = NULL;

    while (!newscr) {
        /* wrap around */
        if (n < 0)
            n = NumScreens - 1;
        else if (n >= NumScreens)
            n = 0;

        newscr = ScreenList[n];
        if (!newscr) {          /* make sure screen is managed */
            if (inc) {          /* walk around the list */
                n += inc;
                continue;
            }
            twmWarning("unable to warp to unmanaged screen %d", n);
            Bell(XkbBI_MinorError, 0, None);
            return;
        }
    }

    if (Scr->screen == n)
        return;                 /* already on that screen */

    PreviousScreen = Scr->screen;
    XQueryPointer(dpy, Scr->Root, &dumwin, &dumwin, &x, &y,
                  &dumint, &dumint, &dummask);

    XWarpPointer(dpy, None, newscr->Root, 0, 0, 0, 0, x, y);
    return;
}

/**
 * rotate our internal copy of WM_COLORMAP_WINDOWS
 */
static void
BumpWindowColormap(TwmWindow *tmp, int inc)
{
    ColormapWindow **cwins;

    if (!tmp)
        return;

    if (inc && tmp->cmaps.number_cwins > 0) {
        cwins =
            malloc(sizeof(ColormapWindow *) * (size_t) tmp->cmaps.number_cwins);
        if (cwins) {
            int i;
            int previously_installed;

            /* SUPPRESS 560 */
            if ((previously_installed = (Scr->cmapInfo.cmaps == &tmp->cmaps &&
                                         tmp->cmaps.number_cwins))) {
                for (i = tmp->cmaps.number_cwins; i-- > 0;)
                    tmp->cmaps.cwins[i]->colormap->state = 0;
            }

            for (i = 0; i < tmp->cmaps.number_cwins; i++) {
                int j = i - inc;

                if (j >= tmp->cmaps.number_cwins)
                    j -= tmp->cmaps.number_cwins;
                else if (j < 0)
                    j += tmp->cmaps.number_cwins;
                cwins[j] = tmp->cmaps.cwins[i];
            }

            free(tmp->cmaps.cwins);

            tmp->cmaps.cwins = cwins;

            if (tmp->cmaps.number_cwins > 1)
                bzero(tmp->cmaps.scoreboard,
                      ColormapsScoreboardLength(&tmp->cmaps));

            if (previously_installed)
                InstallWindowColormaps(PropertyNotify, (TwmWindow *) NULL);
        }
    }
    else
        FetchWmColormapWindows(tmp);
}

static void
HideIconManager(void)
{
    SetMapStateProp(Scr->iconmgr.twm_win, WithdrawnState);
    XUnmapWindow(dpy, Scr->iconmgr.twm_win->frame);
    if (Scr->iconmgr.twm_win->icon_w)
        XUnmapWindow(dpy, Scr->iconmgr.twm_win->icon_w);
    Scr->iconmgr.twm_win->mapped = FALSE;
    Scr->iconmgr.twm_win->icon = TRUE;
}

void
SetBorder(TwmWindow *tmp, Bool onoroff)
{
    if (tmp->highlight) {
        if (onoroff) {
            XSetWindowBorder(dpy, tmp->frame, tmp->border);
            if (tmp->title_w)
                XSetWindowBorder(dpy, tmp->title_w, tmp->border);
        }
        else {
            XSetWindowBorderPixmap(dpy, tmp->frame, tmp->gray);
            if (tmp->title_w)
                XSetWindowBorderPixmap(dpy, tmp->title_w, tmp->gray);
        }
    }
}

static void
DestroyMenu(MenuRoot *menu)
{
    MenuItem *item;

    if (menu->w) {
        XDeleteContext(dpy, menu->w, MenuContext);
        XDeleteContext(dpy, menu->w, ScreenContext);
        if (Scr->Shadow)
            XDestroyWindow(dpy, menu->shadow);
        XDestroyWindow(dpy, menu->w);
    }

    for (item = menu->first; item;) {
        MenuItem *tmp = item;

        item = item->next;
        free(tmp);
    }
}

/*
 * warping routines
 */

static void
WarpAlongRing(XButtonEvent *ev, Bool forward)
{
    TwmWindow *r, *head;

    if (Scr->RingLeader)
        head = Scr->RingLeader;
    else if (!(head = Scr->Ring))
        return;

    if (forward) {
        for (r = head->ring.next; r != head; r = r->ring.next) {
            if (!r || r->mapped)
                break;
        }
    }
    else {
        for (r = head->ring.prev; r != head; r = r->ring.prev) {
            if (!r || r->mapped)
                break;
        }
    }

    if (r && r != head) {
        TwmWindow *p = Scr->RingLeader, *t;
        XPointer context_data;

        Scr->RingLeader = r;
        WarpToWindow(r);

        if (XFindContext(dpy, ev->window, TwmContext, &context_data) == 0)
            t = (TwmWindow *) context_data;
        else
            t = NULL;

        if (p && p->mapped && p == t) {
            p->ring.cursor_valid = True;
            p->ring.curs_x = ev->x_root - t->frame_x;
            p->ring.curs_y = ev->y_root - t->frame_y;
            if (p->ring.curs_x < -p->frame_bw ||
                p->ring.curs_x >= p->frame_width + p->frame_bw ||
                p->ring.curs_y < -p->frame_bw ||
                p->ring.curs_y >= p->frame_height + p->frame_bw) {
                /* somehow out of window */
                p->ring.curs_x = p->frame_width / 2;
                p->ring.curs_y = p->frame_height / 2;
            }
        }
    }
}

static void
WarpToWindow(TwmWindow *t)
{
    int x, y;

    if (t->auto_raise || !Scr->NoRaiseWarp)
        AutoRaiseWindow(t);
    if (t->ring.cursor_valid) {
        x = t->ring.curs_x;
        y = t->ring.curs_y;
    }
    else {
        x = t->frame_width / 2;
        y = t->frame_height / 2;
    }
    XWarpPointer(dpy, None, t->frame, 0, 0, 0, 0, x, y);
}

/*
 * ICCCM Client Messages - Section 4.2.8 of the ICCCM dictates that all
 * client messages will have the following form:
 *
 *     event type       ClientMessage
 *     message type     _XA_WM_PROTOCOLS
 *     window           tmp->w
 *     format           32
 *     data[0]          message atom
 *     data[1]          time stamp
 */
static void
send_clientmessage(Window w, Atom a, Time timestamp)
{
    XClientMessageEvent ev;

    ev.type = ClientMessage;
    ev.window = w;
    ev.message_type = _XA_WM_PROTOCOLS;
    ev.format = 32;
    ev.data.l[0] = (long) a;
    ev.data.l[1] = (long) timestamp;
    XSendEvent(dpy, w, False, 0L, (XEvent *) &ev);
}

void
SendDeleteWindowMessage(TwmWindow *tmp, Time timestamp)
{
    send_clientmessage(tmp->w, _XA_WM_DELETE_WINDOW, timestamp);
}

void
SendSaveYourselfMessage(TwmWindow *tmp, Time timestamp)
{
    send_clientmessage(tmp->w, _XA_WM_SAVE_YOURSELF, timestamp);
}

void
SendTakeFocusMessage(TwmWindow *tmp, Time timestamp)
{
    send_clientmessage(tmp->w, _XA_WM_TAKE_FOCUS, timestamp);
}
