/*****************************************************************************/
/*

Copyright 1989, 1998  The Open Group
Copyright 2005 Hitachi, Ltd.

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
 * twm - "Tom's Window Manager"
 *
 * 27-Oct-1987 Thomas E. LaStrange    File created
 * 10-Oct-1990 David M. Sternlicht    Storing saved colors on root
 * 19-Feb-2005 Julien Lafon           Handle print screens for unified Xserver
 ***********************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <stdarg.h>

#include "twm.h"
#include "iconmgr.h"
#include "add_window.h"
#include "gc.h"
#include "parse.h"
#include "menus.h"
#include "events.h"
#include "util.h"
#include "gram.h"
#include "screen.h"
#include "parse.h"
#include "session.h"

#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <X11/SM/SMlib.h>
#include <X11/Xmu/Error.h>
#include <X11/extensions/sync.h>
#include <X11/Xlocale.h>

#ifdef XPRINT
#include <X11/extensions/Print.h>
#endif                          /* XPRINT */

#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif

static void InitVariables(void);

XtAppContext appContext;        /* Xt application context */
XtSignalId si;

Display *dpy = NULL;            /* which display are we talking to */
Window ResizeWindow;            /* the window we are resizing */

int MultiScreen = TRUE;         /* try for more than one screen? */
int NoPrintscreens = False;     /* ignore special handling of print screens? */
int NumScreens;                 /* number of screens in ScreenList */
int HasShape;                   /* server supports shape extension? */

#ifdef HAVE_XRANDR
int HasXrandr;                  /* server supports Xrandr extension? */
int XrandrEventBase, XrandrErrorBase;
#endif

int ShapeEventBase, ShapeErrorBase;
int HasSync;                    /* server supports SYNC extension? */
int SyncEventBase, SyncErrorBase;
ScreenInfo **ScreenList;        /* structures for each screen */
ScreenInfo *Scr = NULL;         /* the cur and prev screens */
int PreviousScreen;             /* last screen that we were on */
int FirstScreen;                /* TRUE ==> first screen of display */
int message_level = 1;          /* controls error messages */
static int RedirectError;       /* TRUE ==> another window manager running */
static int TwmErrorHandler(Display *dpy, XErrorEvent *event);   /* for setting RedirectError */
static int CatchRedirectError(Display *dpy, XErrorEvent *event);        /* for everything else */
static void sigHandler(int);
char Info[INFO_LINES][INFO_SIZE];       /* info strings to print */
int InfoLines;
static char *InitFile = NULL;

Cursor UpperLeftCursor;         /* upper Left corner cursor */
Cursor RightButt;
Cursor MiddleButt;
Cursor LeftButt;

XContext TwmContext;            /* context for twm windows */
XContext MenuContext;           /* context for all menu windows */
XContext IconManagerContext;    /* context for all window list windows */
XContext ScreenContext;         /* context to get screen data */
XContext ColormapContext;       /* context for colormap operations */

XClassHint NoClass;             /* for applications with no class */

XGCValues Gcv;

const char *Home;               /* the HOME environment variable */
int HomeLen;                    /* length of Home */
int ParseError;                 /* error parsing the .twmrc file */

int HandlingEvents = FALSE;     /* are we handling events yet? */

Window JunkRoot;                /* junk window */
Window JunkChild;               /* junk window */
int JunkX;                      /* junk variable */
int JunkY;                      /* junk variable */
unsigned int JunkWidth, JunkHeight, JunkBW, JunkDepth, JunkMask;

char *ProgramName;
int Argc;
char **Argv;

Bool RestartPreviousState = False;      /* try to restart in previous state */

static unsigned long black, white;

Atom TwmAtoms[11];

Bool use_fontset;               /* use XFontSet-related functions or not */

/* don't change the order of these strings */
static char *atom_names[11] = {
    "_MIT_PRIORITY_COLORS",
    "WM_CHANGE_STATE",
    "WM_STATE",
    "WM_COLORMAP_WINDOWS",
    "WM_PROTOCOLS",
    "WM_TAKE_FOCUS",
    "WM_SAVE_YOURSELF",
    "WM_DELETE_WINDOW",
    "SM_CLIENT_ID",
    "WM_CLIENT_LEADER",
    "WM_WINDOW_ROLE"
};

#ifdef XPRINT
/* |hasExtension()| and |IsPrintScreen()| have been stolen from
 * xc/programs/xdpyinfo/xdpyinfo.c */
static Bool
hasExtension(Display *dpy2, char *extname)
{
    int num_extensions, i;
    char **extensions;

    extensions = XListExtensions(dpy2, &num_extensions);
    for (i = 0; i < num_extensions &&
         (strcmp(extensions[i], extname) != 0); i++);
    XFreeExtensionList(extensions);
    return i != num_extensions;
}

static Bool
IsPrintScreen(Screen *s)
{
    Display *dpy2 = XDisplayOfScreen(s);

    /* Check whether this is a screen of a print DDX */
    if (hasExtension(dpy2, XP_PRINTNAME)) {
        Screen **pscreens;
        int pscrcount;
        int i;

        pscreens = XpQueryScreens(dpy2, &pscrcount);
        for (i = 0; (i < pscrcount) && pscreens; i++) {
            if (s == pscreens[i]) {
                return True;
            }
        }
        XFree(pscreens);
    }
    return False;
}
#endif                          /* XPRINT */

static void
usage(void)
{
    fprintf(stderr, "usage:  %s [-display dpy] [-f file] [-s] [-q] [-v] [-V]"
#ifdef XPRINT
            " [-noprint]"
#endif                          /* XPRINT */
            " [-clientId id] [-restore file]\n", ProgramName);
    exit(EXIT_FAILURE);
}

static Bool
brief_opt(const char *param, const char *option)
{
    size_t have = strlen(++param);
    size_t want = strlen(option);
    Bool result = False;

    if (have <= want) {
        if (!strncmp(param, option, have))
            result = True;
    }
    return result;
}

/***********************************************************************
 *
 *  Procedure:
 *      main - start of twm
 *
 ***********************************************************************
 */

int
main(int argc, char *argv[])
{
    Window root, parent, *children;
    unsigned int nchildren;
    int i, j;
    char *display_name = NULL;
    unsigned long valuemask;    /* mask for create windows */
    XSetWindowAttributes attributes;    /* attributes for create windows */
    int numManaged, firstscrn, lastscrn, scrnum;
    int zero = 0;
    char *restore_filename = NULL;
    char *client_id = NULL;
    char *loc;

    ProgramName = argv[0];
    Argc = argc;
    Argv = argv;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'V':
                printf("%s %s\n", APP_NAME, APP_VERSION);
                exit(EXIT_SUCCESS);
            case 'd':          /* -display dpy */
                if (!brief_opt(argv[i], "display"))
                    usage();
                if (++i >= argc)
                    usage();
                display_name = argv[i];
                continue;
            case 's':          /* -single */
                if (!brief_opt(argv[i], "single"))
                    usage();
                MultiScreen = FALSE;
                continue;
#ifdef XPRINT
            case 'n':          /* -noprint */
                if (!brief_opt(argv[i], "noprint"))
                    usage();
                NoPrintscreens = True;
                continue;
#endif                          /* XPRINT */
            case 'f':          /* -file twmrcfilename */
                if (!brief_opt(argv[i], "file"))
                    usage();
                if (++i >= argc)
                    usage();
                InitFile = argv[i];
                continue;
            case 'v':          /* -verbose */
                if (!brief_opt(argv[i], "verbose"))
                    usage();
                message_level++;
                continue;
            case 'c':          /* -clientId */
                if (!brief_opt(argv[i], "clientId"))
                    usage();
                if (++i >= argc)
                    usage();
                client_id = argv[i];
                continue;
            case 'r':          /* -restore */
                if (!brief_opt(argv[i], "restore"))
                    usage();
                if (++i >= argc)
                    usage();
                restore_filename = argv[i];
                continue;
            case 'q':          /* -quiet */
                if (!brief_opt(argv[i], "quiet"))
                    usage();
                --message_level;
                continue;
            }
        }
        usage();
    }

    loc = setlocale(LC_ALL, "");
    if (!loc || !strcmp(loc, "C") || !strcmp(loc, "POSIX") ||
        !XSupportsLocale()) {
        use_fontset = False;
    }
    else {
        use_fontset = True;
    }

#define newhandler(sig) \
    if (signal (sig, SIG_IGN) != SIG_IGN) (void) signal (sig, sigHandler)

    newhandler(SIGINT);
    newhandler(SIGHUP);
    newhandler(SIGQUIT);
    newhandler(SIGTERM);

#undef newhandler

    Home = getenv("HOME");
    if (Home != NULL) {
        char *temp_p;

        /*
         * Make a copy of Home because the string returned by getenv() can be
         * overwritten by some POSIX.1 and ANSI-C implementations of getenv()
         * when further calls to getenv() are made
         */

        temp_p = strdup(Home);
        Home = temp_p;
    }

    if (Home == NULL)
        Home = "./";

    HomeLen = (int) strlen(Home);

    NoClass.res_name = NoName;
    NoClass.res_class = NoName;

    XtToolkitInitialize();
    appContext = XtCreateApplicationContext();

    si = XtAppAddSignal(appContext, Done, NULL);

    if (!(dpy = XtOpenDisplay(appContext, display_name, "twm", "twm",
                              NULL, 0, &zero, NULL))) {
        twmError("unable to open display \"%s\"", XDisplayName(display_name));
    }

    /* secure-exec: host_net socket fds are not in the WASI descriptor table, so fcntl(F_SETFD)
     * returns -1. There is no exec() in the sandbox, so close-on-exec is irrelevant; ignore it. */
    if (fcntl(ConnectionNumber(dpy), F_SETFD, 1) == -1) {
        /* non-fatal under secure-exec */
    }

    if (restore_filename)
        ReadWinConfigFile(restore_filename);

    HasShape = XShapeQueryExtension(dpy, &ShapeEventBase, &ShapeErrorBase);
    HasSync = XSyncQueryExtension(dpy, &SyncEventBase, &SyncErrorBase);
#ifdef HAVE_XRANDR
    HasXrandr = XRRQueryExtension(dpy, &XrandrEventBase, &XrandrErrorBase);
#endif
    TwmContext = XUniqueContext();
    MenuContext = XUniqueContext();
    IconManagerContext = XUniqueContext();
    ScreenContext = XUniqueContext();
    ColormapContext = XUniqueContext();

    (void) XInternAtoms(dpy, atom_names, sizeof TwmAtoms / sizeof TwmAtoms[0],
                        False, TwmAtoms);

    /* Set up the per-screen global information. */

    NumScreens = ScreenCount(dpy);

    if (MultiScreen) {
        firstscrn = 0;
        lastscrn = NumScreens - 1;
    }
    else {
        firstscrn = lastscrn = DefaultScreen(dpy);
    }

    InfoLines = 0;

    /* for simplicity, always allocate NumScreens ScreenInfo struct pointers */
    ScreenList = calloc((size_t) NumScreens, sizeof(ScreenInfo *));
    if (ScreenList == NULL) {
        twmError("Unable to allocate memory for screen list, exiting");
    }
    numManaged = 0;
    PreviousScreen = DefaultScreen(dpy);
    FirstScreen = TRUE;
    for (scrnum = firstscrn; scrnum <= lastscrn; scrnum++) {
#ifdef XPRINT
        /* Ignore print screens to avoid that users accidentally warp on a
         * print screen (which are not visible on video displays) */
        if ((!NoPrintscreens) && IsPrintScreen(XScreenOfDisplay(dpy, scrnum))) {
            twmWarning("skipping print screen %d", scrnum);
            continue;
        }
#endif                          /* XPRINT */

        /* Make sure property priority colors is empty */
        XChangeProperty(dpy, RootWindow(dpy, scrnum), _XA_MIT_PRIORITY_COLORS,
                        XA_CARDINAL, 32, PropModeReplace, NULL, 0);
        RedirectError = FALSE;
        XSetErrorHandler(CatchRedirectError);
        XSelectInput(dpy, RootWindow(dpy, scrnum),
                     ColormapChangeMask | EnterWindowMask | PropertyChangeMask |
                     SubstructureRedirectMask | KeyPressMask |
                     ButtonPressMask | ButtonReleaseMask);
        XSync(dpy, 0);
        XSetErrorHandler(TwmErrorHandler);

        if (RedirectError) {
            if (MultiScreen && NumScreens > 0) {
                twmWarning("another window manager is already running."
                           " on screen %d?\n", scrnum);
            }
            else {
                twmWarning("another window manager is already running.");
            }
            continue;
        }

        numManaged++;

        /* Note:  ScreenInfo struct is calloc'ed to initialize to zero. */
        Scr = ScreenList[scrnum] = calloc(1, sizeof(ScreenInfo));
        if (Scr == NULL) {
            twmWarning
                ("unable to allocate memory for ScreenInfo structure for screen %d.",
                 scrnum);
            continue;
        }

        /* initialize list pointers, remember to put an initialization
         * in InitVariables also
         */
        Scr->BorderColorL = NULL;
        Scr->IconBorderColorL = NULL;
        Scr->BorderTileForegroundL = NULL;
        Scr->BorderTileBackgroundL = NULL;
        Scr->TitleForegroundL = NULL;
        Scr->TitleBackgroundL = NULL;
        Scr->IconForegroundL = NULL;
        Scr->IconBackgroundL = NULL;
        Scr->NoTitle = NULL;
        Scr->MakeTitle = NULL;
        Scr->AutoRaise = NULL;
        Scr->IconNames = NULL;
        Scr->NoHighlight = NULL;
        Scr->NoStackModeL = NULL;
        Scr->NoTitleHighlight = NULL;
        Scr->DontIconify = NULL;
        Scr->IconMgrNoShow = NULL;
        Scr->IconMgrShow = NULL;
        Scr->IconifyByUn = NULL;
        Scr->IconManagerFL = NULL;
        Scr->IconManagerBL = NULL;
        Scr->IconMgrs = NULL;
        Scr->StartIconified = NULL;
        Scr->SqueezeTitleL = NULL;
        Scr->DontSqueezeTitleL = NULL;
        Scr->WindowRingL = NULL;
        Scr->WarpCursorL = NULL;
        /* remember to put an initialization in InitVariables also
         */

        Scr->screen = scrnum;
        Scr->d_depth = DefaultDepth(dpy, scrnum);
        Scr->d_visual = DefaultVisual(dpy, scrnum);
        Scr->Root = RootWindow(dpy, scrnum);
        XSaveContext(dpy, Scr->Root, ScreenContext, (XPointer) Scr);

        Scr->TwmRoot.cmaps.number_cwins = 1;
        Scr->TwmRoot.cmaps.cwins = malloc(sizeof(ColormapWindow *));
        Scr->TwmRoot.cmaps.cwins[0] =
            CreateColormapWindow(Scr->Root, True, False);
        Scr->TwmRoot.cmaps.cwins[0]->visibility = VisibilityPartiallyObscured;

        Scr->cmapInfo.cmaps = NULL;
        Scr->cmapInfo.maxCmaps =
            MaxCmapsOfScreen(ScreenOfDisplay(dpy, Scr->screen));
        Scr->cmapInfo.root_pushes = 0;
        InstallWindowColormaps(0, &Scr->TwmRoot);

        Scr->StdCmapInfo.head = Scr->StdCmapInfo.tail =
            Scr->StdCmapInfo.mru = NULL;
        Scr->StdCmapInfo.mruindex = 0;
        LocateStandardColormaps();

        Scr->TBInfo.nleft = Scr->TBInfo.nright = 0;
        Scr->TBInfo.head = NULL;
        Scr->TBInfo.border = 1;
        Scr->TBInfo.width = 0;
        Scr->TBInfo.leftx = 0;
        Scr->TBInfo.titlex = 0;

        Scr->MyDisplayWidth = DisplayWidth(dpy, scrnum);
        Scr->MyDisplayHeight = DisplayHeight(dpy, scrnum);
        Scr->MaxWindowWidth = 32767 - Scr->MyDisplayWidth;
        Scr->MaxWindowHeight = 32767 - Scr->MyDisplayHeight;

        Scr->XORvalue = (((unsigned long) 1) << Scr->d_depth) - 1;

        if (DisplayCells(dpy, scrnum) < 3)
            Scr->Monochrome = MONOCHROME;
        else if (DefaultVisual(dpy, scrnum)->class == GrayScale)
            Scr->Monochrome = GRAYSCALE;
        else
            Scr->Monochrome = COLOR;

        /* setup default colors */
        Scr->FirstTime = TRUE;
        GetColor(Scr->Monochrome, &black, "black");
        Scr->Black = black;
        GetColor(Scr->Monochrome, &white, "white");
        Scr->White = white;

        if (FirstScreen) {
            SetFocus((TwmWindow *) NULL, CurrentTime);

            /* define cursors */

            NewFontCursor(&UpperLeftCursor, "top_left_corner");
            NewFontCursor(&RightButt, "rightbutton");
            NewFontCursor(&LeftButt, "leftbutton");
            NewFontCursor(&MiddleButt, "middlebutton");
        }

        Scr->iconmgr.x = 0;
        Scr->iconmgr.y = 0;
        Scr->iconmgr.width = 150;
        Scr->iconmgr.height = 5;
        Scr->iconmgr.next = NULL;
        Scr->iconmgr.prev = NULL;
        Scr->iconmgr.lasti = &(Scr->iconmgr);
        Scr->iconmgr.first = NULL;
        Scr->iconmgr.last = NULL;
        Scr->iconmgr.active = NULL;
        Scr->iconmgr.scr = Scr;
        Scr->iconmgr.columns = 1;
        Scr->iconmgr.count = 0;
        Scr->iconmgr.name = "TWM";
        Scr->iconmgr.icon_name = "Icons";

        Scr->IconDirectory = NULL;

        Scr->siconifyPm = None;
        Scr->pullPm = None;
        Scr->hilitePm = None;
        Scr->tbpm.xlogo = None;
        Scr->tbpm.resize = None;
        Scr->tbpm.question = None;
        Scr->tbpm.menu = None;
        Scr->tbpm.delete = None;

        InitVariables();
        InitMenus();

        /* Parse it once for each screen. */
        ParseTwmrc(InitFile);
        assign_var_savecolor(); /* storing pixels for twmrc "entities" */
        if (Scr->SqueezeTitle == -1)
            Scr->SqueezeTitle = FALSE;
        if (!Scr->HaveFonts)
            CreateFonts();
        CreateGCs();
        MakeMenus();

        Scr->TitleBarFont.y += Scr->FramePadding;
        Scr->TitleHeight = Scr->TitleBarFont.height + Scr->FramePadding * 2;
        /* make title height be odd so buttons look nice and centered */
        if (!(Scr->TitleHeight & 1))
            Scr->TitleHeight++;

        InitTitlebarButtons();  /* menus are now loaded! */

        XGrabServer(dpy);
        XSync(dpy, 0);

        JunkX = 0;
        JunkY = 0;

        XQueryTree(dpy, Scr->Root, &root, &parent, &children, &nchildren);
        CreateIconManagers();
        if (!Scr->NoIconManagers)
            Scr->iconmgr.twm_win->icon = TRUE;

        /*
         * weed out icon windows
         */
        for (i = 0; (unsigned) i < nchildren; i++) {
            if (children[i]) {
                XWMHints *wmhintsp = XGetWMHints(dpy, children[i]);

                if (wmhintsp) {
                    if (wmhintsp->flags & IconWindowHint) {
                        for (j = 0; (unsigned) j < nchildren; j++) {
                            if (children[j] == wmhintsp->icon_window) {
                                children[j] = None;
                                break;
                            }
                        }
                    }
                    XFree(wmhintsp);
                }
            }
        }

        /*
         * map all of the non-override windows
         */
        for (i = 0; (unsigned) i < nchildren; i++) {
            if (children[i] && MappedNotOverride(children[i])) {
                XUnmapWindow(dpy, children[i]);
                SimulateMapRequest(children[i]);
            }
        }

        if (Scr->ShowIconManager && !Scr->NoIconManagers) {
            Scr->iconmgr.twm_win->icon = FALSE;
            if (Scr->iconmgr.count) {
                SetMapStateProp(Scr->iconmgr.twm_win, NormalState);
                XMapWindow(dpy, Scr->iconmgr.w);
                XMapWindow(dpy, Scr->iconmgr.twm_win->frame);
            }
        }

        attributes.border_pixel = Scr->DefaultC.fore;
        attributes.background_pixel = Scr->DefaultC.back;
        attributes.event_mask = (ExposureMask | ButtonPressMask |
                                 KeyPressMask | ButtonReleaseMask);
        attributes.backing_store = NotUseful;
        attributes.cursor = XCreateFontCursor(dpy, XC_hand2);
        valuemask = (CWBorderPixel | CWBackPixel | CWEventMask |
                     CWBackingStore | CWCursor);
        Scr->InfoWindow = XCreateWindow(dpy, Scr->Root, 0, 0,
                                        (unsigned int) 5, (unsigned int) 5,
                                        (unsigned int) BW, 0,
                                        (unsigned int) CopyFromParent,
                                        (Visual *) CopyFromParent,
                                        valuemask, &attributes);

        Scr->SizeStringWidth = MyFont_TextWidth(&Scr->SizeFont,
                                                " 8888 x 8888 ", 13);
        valuemask = (CWBorderPixel | CWBackPixel | CWBitGravity);
        attributes.bit_gravity = NorthWestGravity;
        Scr->SizeWindow = XCreateWindow(dpy, Scr->Root, 0, 0,
                                        (unsigned int) Scr->SizeStringWidth,
                                        (unsigned int) (Scr->SizeFont.height +
                                                        SIZE_VINDENT * 2),
                                        (unsigned int) BW, 0,
                                        (unsigned int) CopyFromParent,
                                        (Visual *) CopyFromParent,
                                        valuemask, &attributes);

        XUngrabServer(dpy);

        FirstScreen = FALSE;
        Scr->FirstTime = FALSE;
    }                           /* for */

    if (numManaged == 0) {
        if (MultiScreen && NumScreens > 0) {
            twmError("unable to find any unmanaged %sscreens.\n",
                     NoPrintscreens ? "" : "video ");
        }
        exit(EXIT_FAILURE);
    }

    (void) ConnectToSessionManager(client_id);

    RestartPreviousState = False;
    HandlingEvents = TRUE;
    InitEvents();
    HandleEvents();
    exit(EXIT_SUCCESS);
}

/**
 * initialize twm variables
 */
static void
InitVariables(void)
{
    FreeList(&Scr->BorderColorL);
    FreeList(&Scr->IconBorderColorL);
    FreeList(&Scr->BorderTileForegroundL);
    FreeList(&Scr->BorderTileBackgroundL);
    FreeList(&Scr->TitleForegroundL);
    FreeList(&Scr->TitleBackgroundL);
    FreeList(&Scr->IconForegroundL);
    FreeList(&Scr->IconBackgroundL);
    FreeList(&Scr->IconManagerFL);
    FreeList(&Scr->IconManagerBL);
    FreeList(&Scr->IconMgrs);
    FreeList(&Scr->NoTitle);
    FreeList(&Scr->MakeTitle);
    FreeList(&Scr->AutoRaise);
    FreeList(&Scr->IconNames);
    FreeList(&Scr->NoHighlight);
    FreeList(&Scr->NoStackModeL);
    FreeList(&Scr->NoTitleHighlight);
    FreeList(&Scr->DontIconify);
    FreeList(&Scr->IconMgrNoShow);
    FreeList(&Scr->IconMgrShow);
    FreeList(&Scr->IconifyByUn);
    FreeList(&Scr->StartIconified);
    FreeList(&Scr->IconManagerHighlightL);
    FreeList(&Scr->SqueezeTitleL);
    FreeList(&Scr->DontSqueezeTitleL);
    FreeList(&Scr->WindowRingL);
    FreeList(&Scr->WarpCursorL);

    NewFontCursor(&Scr->FrameCursor, "top_left_arrow");
    NewFontCursor(&Scr->TitleCursor, "top_left_arrow");
    NewFontCursor(&Scr->IconCursor, "top_left_arrow");
    NewFontCursor(&Scr->IconMgrCursor, "top_left_arrow");
    NewFontCursor(&Scr->MoveCursor, "fleur");
    NewFontCursor(&Scr->ResizeCursor, "fleur");
    NewFontCursor(&Scr->MenuCursor, "sb_left_arrow");
    NewFontCursor(&Scr->ButtonCursor, "hand2");
    NewFontCursor(&Scr->WaitCursor, "watch");
    NewFontCursor(&Scr->SelectCursor, "dot");
    NewFontCursor(&Scr->DestroyCursor, "pirate");

    Scr->Ring = NULL;
    Scr->RingLeader = NULL;

    Scr->DefaultC.fore = black;
    Scr->DefaultC.back = white;
    Scr->BorderColor = black;
    Scr->BorderTileC.fore = black;
    Scr->BorderTileC.back = white;
    Scr->TitleC.fore = black;
    Scr->TitleC.back = white;
    Scr->MenuC.fore = black;
    Scr->MenuC.back = white;
    Scr->MenuTitleC.fore = black;
    Scr->MenuTitleC.back = white;
    Scr->MenuShadowColor = black;
    Scr->MenuBorderColor = black;
    Scr->IconC.fore = black;
    Scr->IconC.back = white;
    Scr->IconBorderColor = black;
    Scr->PointerForeground.pixel = black;
    XQueryColor(dpy, Scr->TwmRoot.cmaps.cwins[0]->colormap->c,
                &Scr->PointerForeground);
    Scr->PointerBackground.pixel = white;
    XQueryColor(dpy, Scr->TwmRoot.cmaps.cwins[0]->colormap->c,
                &Scr->PointerBackground);
    Scr->IconManagerC.fore = black;
    Scr->IconManagerC.back = white;
    Scr->IconManagerHighlight = black;

    Scr->FramePadding = 2;      /* values that look "nice" on */
    Scr->TitlePadding = 8;      /* 75 and 100dpi displays */
    Scr->ButtonIndent = 1;
    Scr->SizeStringOffset = 0;
    Scr->BorderWidth = BW;
    Scr->IconBorderWidth = BW;
    Scr->MenuBorderWidth = BW;
    Scr->UnknownWidth = 0;
    Scr->UnknownHeight = 0;
    Scr->NumAutoRaises = 0;
    Scr->NoDefaults = FALSE;
    Scr->UsePPosition = PPOS_OFF;
    Scr->FocusRoot = TRUE;
    Scr->Focus = NULL;
    Scr->WarpCursor = FALSE;
    Scr->ForceIcon = FALSE;
    Scr->NoGrabServer = FALSE;
    Scr->NoRaiseMove = FALSE;
    Scr->NoRaiseResize = FALSE;
    Scr->NoRaiseDeicon = FALSE;
    Scr->NoRaiseWarp = FALSE;
    Scr->DontMoveOff = FALSE;
    Scr->DoZoom = FALSE;
    Scr->TitleFocus = TRUE;
    Scr->NoTitlebar = FALSE;
    Scr->DecorateTransients = FALSE;
    Scr->IconifyByUnmapping = FALSE;
    Scr->ShowIconManager = FALSE;
    Scr->IconManagerDontShow = FALSE;
    Scr->BackingStore = TRUE;
    Scr->SaveUnder = TRUE;
    Scr->RandomPlacement = FALSE;
    Scr->OpaqueMove = FALSE;
    Scr->Highlight = TRUE;
    Scr->StackMode = TRUE;
    Scr->TitleHighlight = TRUE;
    Scr->MoveDelta = 1;         /* so that f.deltastop will work */
    Scr->ZoomCount = 8;
    Scr->SortIconMgr = FALSE;
    Scr->Shadow = TRUE;
    Scr->InterpolateMenuColors = FALSE;
    Scr->NoIconManagers = FALSE;
    Scr->ClientBorderWidth = FALSE;
    Scr->SqueezeTitle = -1;
    Scr->FirstRegion = NULL;
    Scr->LastRegion = NULL;
    Scr->FirstTime = TRUE;
    Scr->HaveFonts = FALSE;     /* i.e. not loaded yet */
    Scr->CaseSensitive = TRUE;
    Scr->WarpUnmapped = FALSE;

    /* setup default fonts; overridden by defaults from system.twmrc */
#define DEFAULT_NICE_FONT "variable"
#define DEFAULT_FAST_FONT "fixed"

    Scr->TitleBarFont.font = NULL;
    Scr->TitleBarFont.fontset = NULL;
    Scr->TitleBarFont.name = DEFAULT_NICE_FONT;
    Scr->MenuFont.font = NULL;
    Scr->MenuFont.fontset = NULL;
    Scr->MenuFont.name = DEFAULT_NICE_FONT;
    Scr->IconFont.font = NULL;
    Scr->IconFont.fontset = NULL;
    Scr->IconFont.name = DEFAULT_NICE_FONT;
    Scr->SizeFont.font = NULL;
    Scr->SizeFont.fontset = NULL;
    Scr->SizeFont.name = DEFAULT_FAST_FONT;
    Scr->IconManagerFont.font = NULL;
    Scr->IconManagerFont.fontset = NULL;
    Scr->IconManagerFont.name = DEFAULT_NICE_FONT;
    Scr->DefaultFont.font = NULL;
    Scr->DefaultFont.fontset = NULL;
    Scr->DefaultFont.name = DEFAULT_FAST_FONT;

}

void
CreateFonts(void)
{
    GetFont(&Scr->TitleBarFont);
    GetFont(&Scr->MenuFont);
    GetFont(&Scr->IconFont);
    GetFont(&Scr->SizeFont);
    GetFont(&Scr->IconManagerFont);
    GetFont(&Scr->DefaultFont);
    Scr->HaveFonts = TRUE;
}

void
RestoreWithdrawnLocation(TwmWindow *tmp)
{
    int gravx, gravy;
    unsigned int bw;
    XWindowChanges xwc;

    if (XGetGeometry(dpy, tmp->w, &JunkRoot, &xwc.x, &xwc.y,
                     &JunkWidth, &JunkHeight, &bw, &JunkDepth)) {
        unsigned mask;

        GetGravityOffsets(tmp, &gravx, &gravy);
        if (gravy < 0)
            xwc.y -= tmp->title_height;

        if (bw != (unsigned) tmp->old_bw) {
            int xoff, yoff;

            if (!Scr->ClientBorderWidth) {
                xoff = gravx;
                yoff = gravy;
            }
            else {
                xoff = 0;
                yoff = 0;
            }

            xwc.x -= (xoff + 1) * tmp->old_bw;
            xwc.y -= (yoff + 1) * tmp->old_bw;
        }
        if (!Scr->ClientBorderWidth) {
            xwc.x += gravx * tmp->frame_bw;
            xwc.y += gravy * tmp->frame_bw;
        }

        mask = (CWX | CWY);
        if (bw != (unsigned) tmp->old_bw) {
            xwc.border_width = tmp->old_bw;
            mask |= CWBorderWidth;
        }

        XConfigureWindow(dpy, tmp->w, mask, &xwc);

        if (tmp->wmhints && (tmp->wmhints->flags & IconWindowHint)) {
            XUnmapWindow(dpy, tmp->wmhints->icon_window);
        }

    }
}

void
Reborder(Time time)
{
    TwmWindow *tmp;             /* temp twm window structure */
    int scrnum;

    /* put a border back around all windows */

    XGrabServer(dpy);
    for (scrnum = 0; scrnum < NumScreens; scrnum++) {
        if ((Scr = ScreenList[scrnum]) == NULL)
            continue;

        InstallWindowColormaps(0, &Scr->TwmRoot);       /* force reinstall */
        for (tmp = Scr->TwmRoot.next; tmp != NULL; tmp = tmp->next) {
            RestoreWithdrawnLocation(tmp);
            XMapWindow(dpy, tmp->w);
        }
    }

    XUngrabServer(dpy);
    SetFocus((TwmWindow *) NULL, time);
}

static void
sigHandler(int sig _X_UNUSED)
{
    XtNoticeSignal(si);
}

/**
 * cleanup and exit twm
 */
void
Done(XtPointer client_data _X_UNUSED, XtSignalId *si2 _X_UNUSED)
{
    if (dpy) {
        Reborder(CurrentTime);
        XCloseDisplay(dpy);
    }
    exit(EXIT_SUCCESS);
}

/*
 * Error Handlers.  If a client dies, we'll get a BadWindow error (except for
 * GetGeometry which returns BadDrawable) for most operations that we do before
 * manipulating the client's window.
 */

Bool ErrorOccurred = False;
XErrorEvent LastErrorEvent;

static int
TwmErrorHandler(Display *dpy2, XErrorEvent *event)
{
    LastErrorEvent = *event;
    ErrorOccurred = True;

    if ((message_level > 1) &&  /* don't be too obnoxious */
        event->error_code != BadWindow &&       /* watch for dead puppies */
        (event->request_code != X_GetGeometry &&        /* of all styles */
         event->error_code != BadDrawable))
        XmuPrintDefaultErrorMessage(dpy2, event, stderr);
    return 0;
}

static int
CatchRedirectError(Display *dpy2 _X_UNUSED, XErrorEvent *event)
{
    RedirectError = TRUE;
    LastErrorEvent = *event;
    ErrorOccurred = True;
    return 0;
}

void
twmError(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    fprintf(stderr, "%s: error: ", ProgramName);
    vfprintf(stderr, format, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(EXIT_FAILURE);
}

void
twmWarning(const char *format, ...)
{
    if (message_level > 0) {
        va_list ap;

        va_start(ap, format);
        fprintf(stderr, "%s: warning: ", ProgramName);
        vfprintf(stderr, format, ap);
        fputc('\n', stderr);
        va_end(ap);
    }
}

void
twmVerbose(const char *format, ...)
{
    if (message_level > 1) {
        va_list ap;

        va_start(ap, format);
        fprintf(stderr, "%s: warning: ", ProgramName);
        vfprintf(stderr, format, ap);
        fputc('\n', stderr);
        va_end(ap);
    }
}

void
twmMessage(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    printf("%s: ", ProgramName);
    vprintf(format, ap);
    putc('\n', stdout);
    va_end(ap);

    fflush(stdout);
}
