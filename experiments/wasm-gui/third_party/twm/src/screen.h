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
 * */

/***********************************************************************
 *
 * twm per-screen data include file
 *
 * 11-3-88 Dave Payne, Apple Computer                   File created
 *
 ***********************************************************************/

#ifndef _SCREEN_
#define _SCREEN_

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include "list.h"
#include "menus.h"
#include "iconmgr.h"

typedef struct _StdCmap {
    struct _StdCmap *next;      /* next link in chain */
    Atom atom;                  /* property from which this came */
    int nmaps;                  /* number of maps below */
    XStandardColormap *maps;    /* the actual maps */
} StdCmap;

#define SIZE_HINDENT 10
#define SIZE_VINDENT 2

typedef struct _TitlebarPixmaps {
    Pixmap xlogo;
    Pixmap resize;
    Pixmap question;
    Pixmap menu;
    Pixmap delete;
} TitlebarPixmaps;

typedef struct ScreenInfo {
    int screen;                 /* the default screen */
    int d_depth;                /* copy of DefaultDepth(dpy, screen) */
    Visual *d_visual;           /* copy of DefaultVisual(dpy, screen) */
    int Monochrome;             /* is the display monochrome ? */
    int MyDisplayWidth;         /* my copy of DisplayWidth(dpy, screen) */
    int MyDisplayHeight;        /* my copy of DisplayHeight(dpy, screen) */
    int MaxWindowWidth;         /* largest window to allow */
    int MaxWindowHeight;        /* ditto */

    TwmWindow TwmRoot;          /* the head of the twm window list */

    Window Root;                /* the root window */
    Window SizeWindow;          /* the resize dimensions window */
    Window InfoWindow;          /* the information window */

    name_list *Icons;           /* list of icon pixmaps */
    TitlebarPixmaps tbpm;       /* titlebar pixmaps */
    Pixmap UnknownPm;           /* the unknown icon pixmap */
    Pixmap siconifyPm;          /* the icon manager iconify pixmap */
    Pixmap pullPm;              /* pull right menu icon */
    unsigned int pullW, pullH;  /* size of pull right menu icon */
    Pixmap hilitePm;            /* focus highlight window background */
    int hilite_pm_width, hilite_pm_height;      /* cache the size */

    MenuRoot *MenuList;         /* head of the menu list */
    MenuRoot *LastMenu;         /* the last menu (mostly unused?) */
    MenuRoot *Windows;          /* the TwmWindows menu */

    TwmWindow *Ring;            /* one of the windows in window ring */
    TwmWindow *RingLeader;      /* current window in ring */

    MouseButton Mouse[MAX_BUTTONS + 1][NUM_CONTEXTS][MOD_SIZE];
    MouseButton DefaultFunction;
    MouseButton WindowFunction;

    struct {
        Colormaps *cmaps;       /* current list of colormap windows */
        int maxCmaps;           /* maximum number of installed colormaps */
        unsigned long first_req;        /* seq # for first XInstallColormap() req in
                                           pass thru loading a colortable list */
        int root_pushes;        /* current push level to install root
                                   colormap windows */
        TwmWindow *pushed_window;       /* saved window to install when pushes drops
                                           to zero */
    } cmapInfo;

    struct {
        StdCmap *head, *tail;   /* list of maps */
        StdCmap *mru;           /* most recently used in list */
        int mruindex;           /* index of mru in entry */
    } StdCmapInfo;

    struct {
        int nleft, nright;      /* numbers of buttons in list */
        TitleButton *head;      /* start of list */
        int border;             /* button border */
        int pad;                /* button-padding */
        int width;              /* width of single button & border */
        int leftx;              /* start of left buttons */
        int titlex;             /* start of title string */
        int rightoff;           /* offset back from right edge */
    } TBInfo;
    ColorPair BorderTileC;      /* border tile colors */
    ColorPair TitleC;           /* titlebar colors */
    ColorPair MenuC;            /* menu colors */
    ColorPair MenuTitleC;       /* menu title colors */
    ColorPair IconC;            /* icon colors */
    ColorPair IconManagerC;     /* icon manager colors */
    XColor PointerForeground;   /* pointer foreground color */
    XColor PointerBackground;   /* pointer background color */
    ColorPair DefaultC;         /* default colors */
    Pixel BorderColor;          /* color of window borders */
    Pixel MenuShadowColor;      /* menu shadow color */
    Pixel MenuBorderColor;      /* menu border color */
    Pixel IconBorderColor;      /* icon border color */
    Pixel IconManagerHighlight; /* icon manager highlight */

    Cursor TitleCursor;         /* title bar cursor */
    Cursor FrameCursor;         /* frame cursor */
    Cursor IconCursor;          /* icon cursor */
    Cursor IconMgrCursor;       /* icon manager cursor */
    Cursor ButtonCursor;        /* title bar button cursor */
    Cursor MoveCursor;          /* move cursor */
    Cursor ResizeCursor;        /* resize cursor */
    Cursor WaitCursor;          /* wait a while cursor */
    Cursor MenuCursor;          /* menu cursor */
    Cursor SelectCursor;        /* dot cursor for f.move, etc. from menus */
    Cursor DestroyCursor;       /* skull and cross bones, f.destroy */

    name_list *BorderColorL;
    name_list *IconBorderColorL;
    name_list *BorderTileForegroundL;
    name_list *BorderTileBackgroundL;
    name_list *TitleForegroundL;
    name_list *TitleBackgroundL;
    name_list *IconForegroundL;
    name_list *IconBackgroundL;
    name_list *IconManagerFL;
    name_list *IconManagerBL;
    name_list *IconMgrs;
    name_list *NoTitle;         /* list of window names with no title bar */
    name_list *MakeTitle;       /* list of window names with title bar */
    name_list *AutoRaise;       /* list of window names to auto-raise */
    name_list *IconNames;       /* list of window names and icon names */
    name_list *NoHighlight;     /* list of windows to not highlight */
    name_list *NoStackModeL;    /* windows to ignore stack mode requests */
    name_list *NoTitleHighlight;        /* list of windows to not highlight the TB */
    name_list *DontIconify;     /* don't iconify by unmapping */
    name_list *IconMgrNoShow;   /* don't show in the icon manager */
    name_list *IconMgrShow;     /* show in the icon manager */
    name_list *IconifyByUn;     /* windows to iconify by unmapping */
    name_list *StartIconified;  /* windows to start iconic */
    name_list *IconManagerHighlightL;   /* icon manager highlight colors */
    name_list *SqueezeTitleL;   /* windows of which to squeeze title */
    name_list *DontSqueezeTitleL;       /* windows of which not to squeeze */
    name_list *WindowRingL;     /* windows in ring */
    name_list *WarpCursorL;     /* windows to warp cursor to on deiconify */

    GC NormalGC;                /* normal GC for everything */
    GC MenuGC;                  /* gc for menus */
    GC DrawGC;                  /* GC to draw lines for move and resize */

    unsigned long Black;
    unsigned long White;
    unsigned long XORvalue;     /* number to use when drawing xor'ed */
    MyFont TitleBarFont;        /* title bar font structure */
    MyFont MenuFont;            /* menu font structure */
    MyFont IconFont;            /* icon font structure */
    MyFont SizeFont;            /* resize font structure */
    MyFont IconManagerFont;     /* window list font structure */
    MyFont DefaultFont;
    IconMgr iconmgr;            /* default icon manager */
    struct IconRegion *FirstRegion;     /* pointer to icon regions */
    struct IconRegion *LastRegion;      /* pointer to the last icon region */
    char *IconDirectory;        /* icon directory to search */
    int SizeStringOffset;       /* x offset in size window for drawing */
    int SizeStringWidth;        /* minimum width of size window */
    int BorderWidth;            /* border width of twm windows */
    int IconBorderWidth;        /* border width of icon windows */
    int MenuBorderWidth;        /* border width of twm menus */
    int UnknownWidth;           /* width of the unknown icon */
    int UnknownHeight;          /* height of the unknown icon */
    int TitleHeight;            /* height of the title bar window */
    TwmWindow *Focus;           /* the twm window that has focus */
    int EntryHeight;            /* menu entry height */
    int FramePadding;           /* distance between decorations and border */
    int TitlePadding;           /* distance between items in titlebar */
    int ButtonIndent;           /* amount to shrink buttons on each side */
    int NumAutoRaises;          /* number of autoraise windows on screen */
    short NoDefaults;           /* do not add in default UI stuff */
    short UsePPosition;         /* what do with PPosition, see values below */
    short AutoRelativeResize;   /* start resize relative to position in quad */
    short FocusRoot;            /* is the input focus on the root ? */
    short WarpCursor;           /* warp cursor on de-iconify ? */
    short ForceIcon;            /* force the icon to the user specified */
    short NoGrabServer;         /* don't do server grabs */
    short NoRaiseMove;          /* don't raise window following move */
    short NoRaiseResize;        /* don't raise window following resize */
    short NoRaiseDeicon;        /* don't raise window on deiconify */
    short NoRaiseWarp;          /* don't raise window on warp */
    short DontMoveOff;          /* don't allow windows to be moved off */
    short DoZoom;               /* zoom in and out of icons */
    short TitleFocus;           /* focus on window in title bar ? */
    short NoTitlebar;           /* put title bars on windows */
    short DecorateTransients;   /* put title bars on transients */
    short IconifyByUnmapping;   /* simply unmap windows when iconifying */
    short ShowIconManager;      /* display the window list */
    short IconManagerDontShow;  /* show nothing in the icon manager */
    short BackingStore;         /* use backing store for menus */
    short SaveUnder;            /* use save under's for menus */
    short RandomPlacement;      /* randomly place windows that no give hints */
    short OpaqueMove;           /* move the window rather than outline */
    short Highlight;            /* should we highlight the window borders */
    short StackMode;            /* should we honor stack mode requests */
    short TitleHighlight;       /* should we highlight the titlebar */
    short MoveDelta;            /* number of pixels before f.move starts */
    short ZoomCount;            /* zoom outline count */
    short SortIconMgr;          /* sort entries in the icon manager */
    short Shadow;               /* show the menu shadow */
    short InterpolateMenuColors;        /* make pretty menus */
    short NoIconManagers;       /* Don't create any icon managers */
    short ClientBorderWidth;    /* respect client window border width */
    short SqueezeTitle;         /* make title as small as possible */
    short HaveFonts;            /* set if fonts have been loaded */
    short FirstTime;            /* first time we've read .twmrc */
    short CaseSensitive;        /* be case-sensitive when sorting names */
    short WarpUnmapped;         /* allow warping to unmapped windows */

    FuncKey FuncKeyRoot;
} ScreenInfo;

extern int MultiScreen;
extern int NumScreens;
extern ScreenInfo **ScreenList;
extern ScreenInfo *Scr;
extern int FirstScreen;

#define PPOS_OFF 0
#define PPOS_ON 1
#define PPOS_NON_ZERO 2
/* may eventually want an option for having the PPosition be the initial
   location for the drag lines */

#endif                          /* _SCREEN_ */
