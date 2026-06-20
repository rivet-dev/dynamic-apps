/**
 * @file hint.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Functions for reading and writing X properties.
 *
 */

#include "jwm.h"
#include "hint.h"
#include "client.h"
#include "desktop.h"
#include "misc.h"
#include "font.h"
#include "settings.h"

#include <X11/Xlibint.h>

/* MWM Defines */
#define MWM_HINTS_FUNCTIONS   (1L << 0)
#define MWM_HINTS_DECORATIONS (1L << 1)
#define MWM_HINTS_INPUT_MODE  (1L << 2)
#define MWM_HINTS_STATUS      (1L << 3)

#define MWM_FUNC_ALL      (1L << 0)
#define MWM_FUNC_RESIZE   (1L << 1)
#define MWM_FUNC_MOVE     (1L << 2)
#define MWM_FUNC_MINIMIZE (1L << 3)
#define MWM_FUNC_MAXIMIZE (1L << 4)
#define MWM_FUNC_CLOSE    (1L << 5)

#define MWM_DECOR_ALL      (1L << 0)
#define MWM_DECOR_BORDER   (1L << 1)
#define MWM_DECOR_RESIZEH  (1L << 2)
#define MWM_DECOR_TITLE    (1L << 3)
#define MWM_DECOR_MENU     (1L << 4)
#define MWM_DECOR_MINIMIZE (1L << 5)
#define MWM_DECOR_MAXIMIZE (1L << 6)

#define MWM_INPUT_MODELESS                  0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL 1
#define MWM_INPUT_SYSTEM_MODAL              2
#define MWM_INPUT_FULL_APPLICATION_MODAL    3

#define MWM_TEAROFF_WINDOW (1L << 0)

typedef struct {

   unsigned long flags;
   unsigned long functions;
   unsigned long decorations;
   long          inputMode;
   unsigned long status;

} PropMwmHints;

typedef struct {
   Atom *atom;
   const char *name;
} ProtocolNode;

typedef struct {
   Atom *atom;
   const char *name;
} AtomNode;

Atom atoms[ATOM_COUNT];

const char jwmRestart[]       = "_JWM_RESTART";
const char jwmExit[]          = "_JWM_EXIT";
const char jwmReload[]        = "_JWM_RELOAD";
const char managerProperty[]  = "MANAGER";

static const AtomNode atomList[] = {

   { &atoms[ATOM_COMPOUND_TEXT],             "COMPOUND_TEXT"               },
   { &atoms[ATOM_UTF8_STRING],               "UTF8_STRING"                 },
   { &atoms[ATOM_XROOTPMAP_ID],              "_XROOTPMAP_ID"               },
   { &atoms[ATOM_MANAGER],                   &managerProperty[0]           },

   { &atoms[ATOM_WM_STATE],                  "WM_STATE"                    },
   { &atoms[ATOM_WM_PROTOCOLS],              "WM_PROTOCOLS"                },
   { &atoms[ATOM_WM_DELETE_WINDOW],          "WM_DELETE_WINDOW"            },
   { &atoms[ATOM_WM_TAKE_FOCUS],             "WM_TAKE_FOCUS"               },
   { &atoms[ATOM_WM_CHANGE_STATE],           "WM_CHANGE_STATE"             },
   { &atoms[ATOM_WM_COLORMAP_WINDOWS],       "WM_COLORMAP_WINDOWS"         },

   { &atoms[ATOM_NET_SUPPORTED],             "_NET_SUPPORTED"              },
   { &atoms[ATOM_NET_NUMBER_OF_DESKTOPS],    "_NET_NUMBER_OF_DESKTOPS"     },
   { &atoms[ATOM_NET_DESKTOP_NAMES],         "_NET_DESKTOP_NAMES"          },
   { &atoms[ATOM_NET_DESKTOP_GEOMETRY],      "_NET_DESKTOP_GEOMETRY"       },
   { &atoms[ATOM_NET_DESKTOP_VIEWPORT],      "_NET_DESKTOP_VIEWPORT"       },
   { &atoms[ATOM_NET_CURRENT_DESKTOP],       "_NET_CURRENT_DESKTOP"        },
   { &atoms[ATOM_NET_ACTIVE_WINDOW],         "_NET_ACTIVE_WINDOW"          },
   { &atoms[ATOM_NET_WORKAREA],              "_NET_WORKAREA"               },
   { &atoms[ATOM_NET_SUPPORTING_WM_CHECK],   "_NET_SUPPORTING_WM_CHECK"    },
   { &atoms[ATOM_NET_SHOWING_DESKTOP],       "_NET_SHOWING_DESKTOP"        },
   { &atoms[ATOM_NET_FRAME_EXTENTS],         "_NET_FRAME_EXTENTS"          },
   { &atoms[ATOM_NET_WM_DESKTOP],            "_NET_WM_DESKTOP"             },
   { &atoms[ATOM_NET_WM_STATE],              "_NET_WM_STATE"               },
   { &atoms[ATOM_NET_WM_STATE_STICKY],       "_NET_WM_STATE_STICKY"        },
   { &atoms[ATOM_NET_WM_STATE_MAXIMIZED_VERT], "_NET_WM_STATE_MAXIMIZED_VERT"},
   { &atoms[ATOM_NET_WM_STATE_MAXIMIZED_HORZ], "_NET_WM_STATE_MAXIMIZED_HORZ"},
   { &atoms[ATOM_NET_WM_STATE_SHADED],       "_NET_WM_STATE_SHADED"        },
   { &atoms[ATOM_NET_WM_STATE_FULLSCREEN],   "_NET_WM_STATE_FULLSCREEN"    },
   { &atoms[ATOM_NET_WM_STATE_HIDDEN],       "_NET_WM_STATE_HIDDEN"        },
   { &atoms[ATOM_NET_WM_STATE_SKIP_TASKBAR], "_NET_WM_STATE_SKIP_TASKBAR"  },
   { &atoms[ATOM_NET_WM_STATE_SKIP_PAGER],   "_NET_WM_STATE_SKIP_PAGER"    },
   { &atoms[ATOM_NET_WM_STATE_BELOW],        "_NET_WM_STATE_BELOW"         },
   { &atoms[ATOM_NET_WM_STATE_ABOVE],        "_NET_WM_STATE_ABOVE"         },
   { &atoms[ATOM_NET_WM_STATE_DEMANDS_ATTENTION],
      "_NET_WM_STATE_DEMANDS_ATTENTION"},
   { &atoms[ATOM_NET_WM_STATE_FOCUSED],      "_NET_WM_STATE_FOCUSED"       },
   { &atoms[ATOM_NET_WM_ALLOWED_ACTIONS],    "_NET_WM_ALLOWED_ACTIONS"     },
   { &atoms[ATOM_NET_WM_ACTION_MOVE],        "_NET_WM_ACTION_MOVE"         },
   { &atoms[ATOM_NET_WM_ACTION_RESIZE],      "_NET_WM_ACTION_RESIZE"       },
   { &atoms[ATOM_NET_WM_ACTION_MINIMIZE],    "_NET_WM_ACTION_MINIMIZE"     },
   { &atoms[ATOM_NET_WM_ACTION_SHADE],       "_NET_WM_ACTION_SHADE"        },
   { &atoms[ATOM_NET_WM_ACTION_STICK],       "_NET_WM_ACTION_STICK"        },
   { &atoms[ATOM_NET_WM_ACTION_FULLSCREEN],  "_NET_WM_ACTION_FULLSCREEN"   },
   { &atoms[ATOM_NET_WM_ACTION_MAXIMIZE_HORZ], "_NET_WM_ACTION_MAXIMIZE_HORZ"},
   { &atoms[ATOM_NET_WM_ACTION_MAXIMIZE_VERT], "_NET_WM_ACTION_MAXIMIZE_VERT"},
   { &atoms[ATOM_NET_WM_ACTION_CHANGE_DESKTOP],
      "_NET_WM_ACTION_CHANGE_DESKTOP"},
   { &atoms[ATOM_NET_WM_ACTION_CLOSE],       "_NET_WM_ACTION_CLOSE"        },
   { &atoms[ATOM_NET_WM_ACTION_BELOW],       "_NET_WM_ACTION_BELOW"        },
   { &atoms[ATOM_NET_WM_ACTION_ABOVE],       "_NET_WM_ACTION_ABOVE"        },
   { &atoms[ATOM_NET_CLOSE_WINDOW],          "_NET_CLOSE_WINDOW"           },
   { &atoms[ATOM_NET_MOVERESIZE_WINDOW],     "_NET_MOVERESIZE_WINDOW"      },
   { &atoms[ATOM_NET_RESTACK_WINDOW],        "_NET_RESTACK_WINDOW"         },
   { &atoms[ATOM_NET_REQUEST_FRAME_EXTENTS], "_NET_REQUEST_FRAME_EXTENTS"  },
   { &atoms[ATOM_NET_WM_PID],                "_NET_WM_PID"                 },
   { &atoms[ATOM_NET_WM_NAME],               "_NET_WM_NAME"                },
   { &atoms[ATOM_NET_WM_VISIBLE_NAME],       "_NET_WM_VISIBLE_NAME"        },
   { &atoms[ATOM_NET_WM_HANDLED_ICONS],      "_NET_WM_HANDLED_ICONS"       },
   { &atoms[ATOM_NET_WM_ICON],               "_NET_WM_ICON"                },
   { &atoms[ATOM_NET_WM_ICON_NAME],          "_NET_WM_ICON_NAME"           },
   { &atoms[ATOM_NET_WM_USER_TIME],          "_NET_WM_USER_TIME"           },
   { &atoms[ATOM_NET_WM_USER_TIME_WINDOW],   "_NET_WM_USER_TIME_WINDOW"    },
   { &atoms[ATOM_NET_WM_VISIBLE_ICON_NAME],  "_NET_WM_VISIBLE_ICON_NAME"   },
   { &atoms[ATOM_NET_WM_WINDOW_TYPE],        "_NET_WM_WINDOW_TYPE"         },
   { &atoms[ATOM_NET_WM_WINDOW_TYPE_DESKTOP],"_NET_WM_WINDOW_TYPE_DESKTOP" },
   { &atoms[ATOM_NET_WM_WINDOW_TYPE_DOCK],   "_NET_WM_WINDOW_TYPE_DOCK"    },
   { &atoms[ATOM_NET_WM_WINDOW_TYPE_SPLASH], "_NET_WM_WINDOW_TYPE_SPLASH"  },
   { &atoms[ATOM_NET_WM_WINDOW_TYPE_DIALOG], "_NET_WM_WINDOW_TYPE_DIALOG"  },
   { &atoms[ATOM_NET_WM_WINDOW_TYPE_NORMAL], "_NET_WM_WINDOW_TYPE_NORMAL"  },
   { &atoms[ATOM_NET_WM_WINDOW_TYPE_MENU],   "_NET_WM_WINDOW_TYPE_MENU"    },
   { &atoms[ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION],
      "_NET_WM_WINDOW_TYPE_NOTIFICATION" },
   { &atoms[ATOM_NET_WM_WINDOW_TYPE_TOOLBAR], "_NET_WM_WINDOW_TYPE_TOOLBAR"},
   { &atoms[ATOM_NET_WM_WINDOW_TYPE_UTILITY], "_NET_WM_WINDOW_TYPE_UTILITY"},
   { &atoms[ATOM_NET_CLIENT_LIST],           "_NET_CLIENT_LIST"            },
   { &atoms[ATOM_NET_CLIENT_LIST_STACKING],  "_NET_CLIENT_LIST_STACKING"   },
   { &atoms[ATOM_NET_WM_STRUT_PARTIAL],      "_NET_WM_STRUT_PARTIAL"       },
   { &atoms[ATOM_NET_WM_STRUT],              "_NET_WM_STRUT"               },
   { &atoms[ATOM_NET_WM_WINDOW_OPACITY],     "_NET_WM_WINDOW_OPACITY"      },
   { &atoms[ATOM_NET_WM_MOVERESIZE],         "_NET_WM_MOVERESIZE"          },
   { &atoms[ATOM_NET_SYSTEM_TRAY_OPCODE],    "_NET_SYSTEM_TRAY_OPCODE"     },
   { &atoms[ATOM_NET_SYSTEM_TRAY_ORIENTATION],
      "_NET_SYSTEM_TRAY_ORIENTATION" },

   { &atoms[ATOM_MOTIF_WM_HINTS],            "_MOTIF_WM_HINTS"             },

   { &atoms[ATOM_JWM_RESTART],               &jwmRestart[0]                },
   { &atoms[ATOM_JWM_EXIT],                  &jwmExit[0]                   },
   { &atoms[ATOM_JWM_RELOAD],                &jwmReload[0]                 },
   { &atoms[ATOM_JWM_WM_STATE_MAXIMIZED_TOP],
      "_JWM_WM_STATE_MAXIMIZED_TOP" },
   { &atoms[ATOM_JWM_WM_STATE_MAXIMIZED_BOTTOM],
      "_JWM_WM_STATE_MAXIMIZED_BOTTOM" },
   { &atoms[ATOM_JWM_WM_STATE_MAXIMIZED_LEFT],
      "_JWM_WM_STATE_MAXIMIZED_LEFT" },
   { &atoms[ATOM_JWM_WM_STATE_MAXIMIZED_RIGHT],
      "_JWM_WM_STATE_MAXIMIZED_RIGHT" }

};

static char CheckShape(Window win);
static void WriteNetAllowed(ClientNode *np);
static void ReadWMState(Window win, ClientState *state);
static void ReadMotifHints(Window win, ClientState *state);

/** Set root hints and intern atoms. */
void StartupHints(void)
{

   unsigned long *array;
   char *data;
   Atom *supported;
   Window win;
   unsigned int x;
   unsigned int count;

   /* Determine how much space we will need on the stack and allocate it. */
   count = 0;
   for(x = 0; x < settings.desktopCount; x++) {
      count += strlen(GetDesktopName(x)) + 1;
   }
   if(count < 2 * sizeof(unsigned long)) {
      count = 2 * sizeof(unsigned long);
   }
   if(count < ATOM_COUNT * sizeof(Atom)) {
      count = ATOM_COUNT * sizeof(Atom);
   }
   data = AllocateStack(count);
   array = (unsigned long*)data;
   supported = (Atom*)data;

   /* Intern the atoms */
   for(x = 0; x < ATOM_COUNT; x++) {
      *atomList[x].atom = JXInternAtom(display, atomList[x].name, False);
   }

   /* _NET_SUPPORTED */
   for(x = FIRST_NET_ATOM; x <= LAST_NET_ATOM; x++) {
      supported[x - FIRST_NET_ATOM] = atoms[x];
   }
   JXChangeProperty(display, rootWindow, atoms[ATOM_NET_SUPPORTED],
                    XA_ATOM, 32, PropModeReplace, (unsigned char*)supported,
                    LAST_NET_ATOM - FIRST_NET_ATOM + 1);

   /* _NET_NUMBER_OF_DESKTOPS */
   SetCardinalAtom(rootWindow, ATOM_NET_NUMBER_OF_DESKTOPS,
                   settings.desktopCount);

   /* _NET_DESKTOP_NAMES */
   count = 0;
   for(x = 0; x < settings.desktopCount; x++) {
      const char *name = GetDesktopName(x);
      const unsigned len = strlen(name);
      memcpy(&data[count], name, len + 1);
      count += len + 1;
   }
   JXChangeProperty(display, rootWindow, atoms[ATOM_NET_DESKTOP_NAMES],
                    atoms[ATOM_UTF8_STRING], 8, PropModeReplace,
                    (unsigned char*)data, count);

   /* _NET_DESKTOP_GEOMETRY */
   array[0] = rootWidth;
   array[1] = rootHeight;
   JXChangeProperty(display, rootWindow, atoms[ATOM_NET_DESKTOP_GEOMETRY],
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char*)array, 2);

   /* _NET_DESKTOP_VIEWPORT */
   array[0] = 0;
   array[1] = 0;
   JXChangeProperty(display, rootWindow, atoms[ATOM_NET_DESKTOP_VIEWPORT],
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char*)array, 2);

   /* _NET_WM_NAME */
   win = supportingWindow;
   JXChangeProperty(display, win, atoms[ATOM_NET_WM_NAME],
                    atoms[ATOM_UTF8_STRING], 8, PropModeReplace,
                    (unsigned char*)"JWM", 3);

   /* _NET_WM_PID */
   array[0] = getpid();
   JXChangeProperty(display, win, atoms[ATOM_NET_WM_PID],
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char*)array, 1);

   /* _NET_SUPPORTING_WM_CHECK */
   SetWindowAtom(rootWindow, ATOM_NET_SUPPORTING_WM_CHECK, win);
   SetWindowAtom(win, ATOM_NET_SUPPORTING_WM_CHECK, win);

   ReleaseStack(data);

}

/** Determine the current desktop. */
void ReadCurrentDesktop(void)
{
   unsigned long temp;
   currentDesktop = 0;
   if(GetCardinalAtom(rootWindow, ATOM_NET_CURRENT_DESKTOP, &temp)) {
      ChangeDesktop(temp);
   } else {
      SetCardinalAtom(rootWindow, ATOM_NET_CURRENT_DESKTOP, currentDesktop);
   }
}

/** Read client hints.
 * This is called while the client is being added to management.
 */
void ReadClientInfo(ClientNode *np, char alreadyMapped)
{

   Status status;
   ClientNode *pp;

   Assert(np);

   ReadWMName(np);
   ReadWMClass(np);
   ReadWMNormalHints(np);
   ReadWMColormaps(np);
   ReadWMMachine(np);

   status = JXGetTransientForHint(display, np->window, &np->owner);
   if(!status) {
      np->owner = None;
   }

   /* Read the window state. */
   np->state = ReadWindowState(np->window, alreadyMapped);
   if(np->minWidth == np->maxWidth && np->minHeight == np->maxHeight) {
      np->state.border &= ~BORDER_RESIZE;
      np->state.border &= ~BORDER_MAX;
      if(np->minWidth * np->xinc >= rootWidth
         && np->minHeight * np->yinc >= rootHeight) {
         np->state.status |= STAT_FULLSCREEN;
      }
   }

   /* Make sure this client is on at least as high of a layer
    * as its owner. */
   if(np->owner != None) {
      pp = FindClientByWindow(np->owner);
      if(pp) {
         np->state.layer = Max(pp->state.layer, np->state.layer);
      }
   }

}

/** Write the window state hint for a client. */
void WriteState(ClientNode *np)
{
   unsigned long data[2];

   if(np->state.status & STAT_MAPPED) {
      data[0] = NormalState;
   } else if(np->state.status & STAT_MINIMIZED) {
      data[0] = IconicState;
   } else if(np->state.status & STAT_SHADED) {
      data[0] = NormalState;
   } else {
      data[0] = WithdrawnState;
   }
   data[1] = None;

   if(data[0] == WithdrawnState) {
      JXDeleteProperty(display, np->window, atoms[ATOM_WM_STATE]);
   } else {
      JXChangeProperty(display, np->window, atoms[ATOM_WM_STATE],
                       atoms[ATOM_WM_STATE], 32, PropModeReplace,
                       (unsigned char*)data, 2);
   }

   WriteNetState(np);
   WriteFrameExtents(np->window, &np->state);
   WriteNetAllowed(np);
}

/** Set the opacity of a client. */
void SetOpacity(ClientNode *np, unsigned int opacity, char force)
{
   Window w;
   if(np->state.opacity == opacity && !force) {
      return;
   }

   w = np->parent != None ? np->parent : np->window;
   np->state.opacity = opacity;
   if(opacity == 0xFFFFFFFF) {
      JXDeleteProperty(display, w, atoms[ATOM_NET_WM_WINDOW_OPACITY]);
   } else {
      SetCardinalAtom(w, ATOM_NET_WM_WINDOW_OPACITY, opacity);
   }
}

/** Write the net state hint for a client. */
void WriteNetState(ClientNode *np)
{
   unsigned long values[16];
   int index;

   Assert(np);

   /* We remove the _NET_WM_STATE and _NET_WM_DESKTOP for withdrawn windows. */
   if(!(np->state.status & (STAT_MAPPED | STAT_MINIMIZED | STAT_SHADED))) {
      JXDeleteProperty(display, np->window, atoms[ATOM_NET_WM_STATE]);
      JXDeleteProperty(display, np->window, atoms[ATOM_NET_WM_DESKTOP]);
      return;
   } 

   index = 0;
   if(np->state.status & STAT_MINIMIZED) {
      values[index++] = atoms[ATOM_NET_WM_STATE_HIDDEN];
   }

   if(np->state.maxFlags & MAX_HORIZ) {
      values[index++] = atoms[ATOM_NET_WM_STATE_MAXIMIZED_HORZ];
   }
   if(np->state.maxFlags & MAX_VERT) {
      values[index++] = atoms[ATOM_NET_WM_STATE_MAXIMIZED_VERT];
   }
   if(np->state.maxFlags & MAX_TOP) {
      values[index++] = atoms[ATOM_JWM_WM_STATE_MAXIMIZED_TOP];
   }
   if(np->state.maxFlags & MAX_BOTTOM) {
      values[index++] = atoms[ATOM_JWM_WM_STATE_MAXIMIZED_BOTTOM];
   }
   if(np->state.maxFlags & MAX_LEFT) {
      values[index++] = atoms[ATOM_JWM_WM_STATE_MAXIMIZED_LEFT];
   }
   if(np->state.maxFlags & MAX_RIGHT) {
      values[index++] = atoms[ATOM_JWM_WM_STATE_MAXIMIZED_RIGHT];
   }

   if(np->state.status & STAT_SHADED) {
      values[index++] = atoms[ATOM_NET_WM_STATE_SHADED];
   }

   if(np->state.status & STAT_STICKY) {
      values[index++] = atoms[ATOM_NET_WM_STATE_STICKY];
   }

   if(np->state.status & STAT_FULLSCREEN) {
      values[index++] = atoms[ATOM_NET_WM_STATE_FULLSCREEN];
   }

   if(np->state.status & STAT_NOLIST) {
      values[index++] = atoms[ATOM_NET_WM_STATE_SKIP_TASKBAR];
   }

   if(np->state.status & STAT_NOPAGER) {
      values[index++] = atoms[ATOM_NET_WM_STATE_SKIP_PAGER];
   }

   if(np->state.layer != np->state.defaultLayer) {
      if(np->state.layer == LAYER_BELOW) {
         values[index++] = atoms[ATOM_NET_WM_STATE_BELOW];
      } else if(np->state.layer == LAYER_ABOVE) {
         values[index++] = atoms[ATOM_NET_WM_STATE_ABOVE];
      }
   }

   if(np->state.status & STAT_URGENT) {
      values[index++] = atoms[ATOM_NET_WM_STATE_DEMANDS_ATTENTION];
   }
   if(np->state.status & STAT_ACTIVE) {
      values[index++] = atoms[ATOM_NET_WM_STATE_FOCUSED];
   }

   JXChangeProperty(display, np->window, atoms[ATOM_NET_WM_STATE],
                    XA_ATOM, 32, PropModeReplace,
                    (unsigned char*)values, index);
}

/** Set _NET_FRAME_EXTENTS. */
void WriteFrameExtents(Window win, const ClientState *state)
{
   unsigned long values[4];
   int north, south, east, west;

   GetBorderSize(state, &north, &south, &east, &west);

   /* left, right, top, bottom */
   values[0] = west;
   values[1] = east;
   values[2] = north;
   values[3] = south;

   JXChangeProperty(display, win, atoms[ATOM_NET_FRAME_EXTENTS],
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char*)values, 4);

}

/** Write the allowed action property. */
void WriteNetAllowed(ClientNode *np)
{

   unsigned long values[12];
   unsigned int index;

   Assert(np);

   index = 0;

   if(np->state.border & BORDER_SHADE) {
      values[index++] = atoms[ATOM_NET_WM_ACTION_SHADE];
   }

   if(np->state.border & BORDER_MIN) {
      values[index++] = atoms[ATOM_NET_WM_ACTION_MINIMIZE];
   }

   if(np->state.border & BORDER_MAX) {
      values[index++] = atoms[ATOM_NET_WM_ACTION_MAXIMIZE_HORZ];
      values[index++] = atoms[ATOM_NET_WM_ACTION_MAXIMIZE_VERT];
      values[index++] = atoms[ATOM_NET_WM_ACTION_FULLSCREEN];
   }

   if(np->state.border & BORDER_CLOSE) {
      values[index++] = atoms[ATOM_NET_WM_ACTION_CLOSE];
   }

   if(np->state.border & BORDER_RESIZE) {
      values[index++] = atoms[ATOM_NET_WM_ACTION_RESIZE];
   }

   if(np->state.border & BORDER_MOVE) {
      values[index++] = atoms[ATOM_NET_WM_ACTION_MOVE];
   }

   if(!(np->state.status & STAT_STICKY)) {
      values[index++] = atoms[ATOM_NET_WM_ACTION_CHANGE_DESKTOP];
   }

   values[index++] = atoms[ATOM_NET_WM_ACTION_STICK];
   values[index++] = atoms[ATOM_NET_WM_ACTION_BELOW];
   values[index++] = atoms[ATOM_NET_WM_ACTION_ABOVE];

   JXChangeProperty(display, np->window, atoms[ATOM_NET_WM_ALLOWED_ACTIONS],
                    XA_ATOM, 32, PropModeReplace,
                    (unsigned char*)values, index);

}

/** Check if a window uses the shape extension. */
char CheckShape(Window win)
{
#ifdef USE_SHAPE
   int shaped = 0;
   int r1;
   unsigned int r2;
   if(haveShape) {
      JXShapeSelectInput(display, win, ShapeNotifyMask);
      XShapeQueryExtents(display, win, &shaped,
                         &r1, &r1, &r2, &r2,
                         &r1, &r1, &r1, &r2, &r2);
      return shaped ? 1 : 0;
   } else {
      return 0;
   }
#else
   return 0;
#endif
}

/** Read all hints needed to determine the current window state. */
ClientState ReadWindowState(Window win, char alreadyMapped)
{

   ClientState result;
   Status status;
   unsigned long count, x;
   unsigned long extra;
   Atom realType;
   int realFormat;
   unsigned char *temp;
   Atom *state;
   unsigned long card;
   Window utwin;

   Assert(win != None);

   /* !! Keep in sync: defaults for ClientState in hint.h */
   result.status = STAT_MAPPED;
   result.maxFlags = MAX_NONE;
   result.border = BORDER_DEFAULT;
   result.layer = LAYER_NORMAL;
   result.defaultLayer = LAYER_NORMAL;
   result.windowType = WINDOW_TYPE_NORMAL;
   result.desktop = currentDesktop;
   result.opacity = UINT_MAX;

   ReadWMProtocols(win, &result);
   ReadWMHints(win, &result, alreadyMapped);
   ReadWMState(win, &result);
   ReadMotifHints(win, &result);
   ReadWMOpacity(win, &result.opacity);

   /* _NET_WM_DESKTOP */
   if(GetCardinalAtom(win, ATOM_NET_WM_DESKTOP, &card)) {
      if(card == ~0UL) {
         result.status |= STAT_STICKY;
      } else if(card < settings.desktopCount) {
         result.desktop = card;
      } else {
         result.desktop = settings.desktopCount - 1;
      }
   }

   /* _NET_WM_STATE */
   status = JXGetWindowProperty(display, win, atoms[ATOM_NET_WM_STATE], 0, 32,
                                False, XA_ATOM, &realType, &realFormat,
                                &count, &extra, &temp);
   if(status == Success && realFormat != 0) {
      if(count > 0) {
         state = (Atom*)temp;
         for(x = 0; x < count; x++) {
            if(state[x] == atoms[ATOM_NET_WM_STATE_STICKY]) {
               result.status |= STAT_STICKY;
            } else if(state[x] == atoms[ATOM_NET_WM_STATE_SHADED]) {
               result.status |= STAT_SHADED;
            } else if(state[x] == atoms[ATOM_NET_WM_STATE_MAXIMIZED_VERT]) {
               result.maxFlags |= MAX_VERT;
            } else if(state[x] == atoms[ATOM_NET_WM_STATE_MAXIMIZED_HORZ]) {
               result.maxFlags |= MAX_HORIZ;
            } else if(state[x] == atoms[ATOM_JWM_WM_STATE_MAXIMIZED_TOP]) {
               result.maxFlags |= MAX_TOP;
            } else if(state[x] == atoms[ATOM_JWM_WM_STATE_MAXIMIZED_BOTTOM]) {
               result.maxFlags |= MAX_BOTTOM;
            } else if(state[x] == atoms[ATOM_JWM_WM_STATE_MAXIMIZED_LEFT]) {
               result.maxFlags |= MAX_LEFT;
            } else if(state[x] == atoms[ATOM_JWM_WM_STATE_MAXIMIZED_RIGHT]) {
               result.maxFlags |= MAX_RIGHT;
            } else if(state[x] == atoms[ATOM_NET_WM_STATE_FULLSCREEN]) {
               result.status |= STAT_FULLSCREEN;
            } else if(state[x] == atoms[ATOM_NET_WM_STATE_HIDDEN]) {
               result.status |= STAT_MINIMIZED;
            } else if(state[x] == atoms[ATOM_NET_WM_STATE_SKIP_TASKBAR]) {
               result.status |= STAT_NOLIST;
            } else if(state[x] == atoms[ATOM_NET_WM_STATE_SKIP_PAGER]) {
               result.status |= STAT_NOPAGER;
            } else if(state[x] == atoms[ATOM_NET_WM_STATE_ABOVE]) {
               result.layer = LAYER_ABOVE;
            } else if(state[x] == atoms[ATOM_NET_WM_STATE_BELOW]) {
               result.layer = LAYER_BELOW;
            } else if(state[x] == atoms[ATOM_NET_WM_STATE_DEMANDS_ATTENTION]) {
               result.status |= STAT_URGENT;
            }
         }
      }
      if(temp) {
         JXFree(temp);
      }
   }

   /* _NET_WM_WINDOW_TYPE */
   status = JXGetWindowProperty(display, win, atoms[ATOM_NET_WM_WINDOW_TYPE],
                                0, 32, False, XA_ATOM, &realType, &realFormat,
                                &count, &extra, &temp);
   if(status == Success && realFormat != 0) {
      /* Loop until we hit a window type we recognize. */
      state = (Atom*)temp;
      for(x = 0; x < count; x++) {
         if(         state[x] == atoms[ATOM_NET_WM_WINDOW_TYPE_NORMAL]) {
            result.windowType = WINDOW_TYPE_NORMAL;
            break;
         } else if(  state[x] == atoms[ATOM_NET_WM_WINDOW_TYPE_DESKTOP]) {
            result.defaultLayer  = LAYER_DESKTOP;
            result.border        = BORDER_NONE;
            result.status       |= STAT_STICKY;
            result.status       |= STAT_NOLIST;
            result.status       |= STAT_NOFOCUS;
            result.windowType    = WINDOW_TYPE_DESKTOP;
            break;
         } else if(  state[x] == atoms[ATOM_NET_WM_WINDOW_TYPE_DOCK]) {
            result.border        = BORDER_NONE;
            result.defaultLayer  = LAYER_ABOVE;
            result.status       |= STAT_NOFOCUS;
            result.windowType    = WINDOW_TYPE_DOCK;
            break;
         } else if(  state[x] == atoms[ATOM_NET_WM_WINDOW_TYPE_SPLASH]) {
            result.border     = BORDER_NONE;
            result.windowType = WINDOW_TYPE_SPLASH;
            break;
         } else if(  state[x] == atoms[ATOM_NET_WM_WINDOW_TYPE_DIALOG]) {
            result.border    &= ~BORDER_MIN;
            result.border    &= ~BORDER_MAX;
            result.windowType = WINDOW_TYPE_DIALOG;
            break;
         } else if(  state[x] == atoms[ATOM_NET_WM_WINDOW_TYPE_MENU]) {
            result.border       &= ~BORDER_MAX;
            result.status       |= STAT_NOLIST;
            result.windowType    = WINDOW_TYPE_MENU;
         } else if(  state[x] == atoms[ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION]) {
            result.border        = BORDER_NONE;
            result.status       |= STAT_NOLIST;
            result.status       |= STAT_NOFOCUS;
            result.windowType    = WINDOW_TYPE_NOTIFICATION;
         } else if(  state[x] == atoms[ATOM_NET_WM_WINDOW_TYPE_TOOLBAR]) {
            result.border       &= ~BORDER_MAX;
            result.defaultLayer  = LAYER_ABOVE;
            result.status       |= STAT_STICKY;
            result.status       |= STAT_NOLIST;
            result.status       |= STAT_NOFOCUS;
            result.windowType    = WINDOW_TYPE_TOOLBAR;
         } else if(  state[x] == atoms[ATOM_NET_WM_WINDOW_TYPE_UTILITY]) {
            result.border       &= ~BORDER_MAX;
            result.status       |= STAT_NOFOCUS;
            result.windowType    = WINDOW_TYPE_UTILITY;
         } else {
            Debug("Unknown _NET_WM_WINDOW_TYPE: %lu", state[x]);
         }
      }
      if(temp) {
         JXFree(temp);
      }
   }

   /* _NET_WM_USER_TIME_WINDOW */
   if(!GetWindowAtom(win, ATOM_NET_WM_USER_TIME_WINDOW, &utwin)) {
      utwin = win;
   }

   /* _NET_WM_USER_TIME */
   if(GetCardinalAtom(utwin, ATOM_NET_WM_USER_TIME, &card)) {
      if(card == 0) {
         result.status |= STAT_NOFOCUS;
      }
   }

   /* Use the default layer if the layer wasn't set explicitly. */
   if(result.layer == LAYER_NORMAL) {
      result.layer = result.defaultLayer;
   }

   /* Check if this window uses the shape extension. */
   if(CheckShape(win)) {
      result.status |= STAT_SHAPED;
   }

   return result;

}

/** Determine the title to display for a client. */
void ReadWMName(ClientNode *np)
{

   unsigned long count;
   int status;
   unsigned long extra;
   Atom realType;
   int realFormat;
   unsigned char *name;

   if(np->name) {
      Release(np->name);
   }

   status = JXGetWindowProperty(display, np->window,
                                atoms[ATOM_NET_WM_NAME], 0, 1024, False,
                                atoms[ATOM_UTF8_STRING], &realType,
                                &realFormat, &count, &extra, &name);
   if(status != Success || realFormat == 0) {
      np->name = NULL;
   } else {
      np->name = Allocate(count + 1);
      memcpy(np->name, name, count);
      np->name[count] = 0;
      JXFree(name);
      np->name = ConvertFromUTF8(np->name);
   }

#ifdef USE_XUTF8
   if(!np->name) {
      status = JXGetWindowProperty(display, np->window,
                                   XA_WM_NAME, 0, 1024, False,
                                   atoms[ATOM_COMPOUND_TEXT],
                                   &realType, &realFormat, &count,
                                   &extra, &name);
      if(status == Success && realFormat != 0) {
         char **tlist;
         XTextProperty tprop;
         int tcount;
         tprop.value = name;
         tprop.encoding = atoms[ATOM_COMPOUND_TEXT];
         tprop.format = realFormat;
         tprop.nitems = count;
         if(XmbTextPropertyToTextList(display, &tprop, &tlist, &tcount)
            == Success && tcount > 0) {
            const size_t len = strlen(tlist[0]) + 1;
            np->name = Allocate(len);
            memcpy(np->name, tlist[0], len);
            XFreeStringList(tlist);
         }
         JXFree(name);
      }
   }
#endif

   if(!np->name) {
      char *temp = NULL;
      if(JXFetchName(display, np->window, &temp)) {
         const size_t len = strlen(temp) + 1;
         np->name = Allocate(len);
         memcpy(np->name, temp, len);
         JXFree(temp);
      }
   }

}

/** Read the machine name of a client. */
void ReadWMMachine(ClientNode *np)
{
   XTextProperty tprop;
   char **tlist;
   int tcount;

   if(np->clientName) {
      Release(np->clientName);
   }

   XGetWMClientMachine(display, np->window, &tprop);
   if(XmbTextPropertyToTextList(display, &tprop, &tlist, &tcount)
      == Success && tcount > 0) {
      const size_t len = strlen(tlist[0]) + 1;
      np->clientName = Allocate(len);
      memcpy(np->clientName, tlist[0], len);
      XFreeStringList(tlist);
   } else {
      np->clientName = NULL;
   }
}

/** Read the window class for a client. */
void ReadWMClass(ClientNode *np)
{
   XClassHint hint;
   Assert(np);
   if(JXGetClassHint(display, np->window, &hint)) {
      np->instanceName = hint.res_name;
      np->className = hint.res_class;
   }
}

/** Read the protocols hint for a window. */
void ReadWMProtocols(Window w, ClientState *state)
{

   unsigned long count, x;
   int status;
   unsigned long extra;
   Atom realType;
   int realFormat;
   unsigned char *temp;
   Atom *p;

   Assert(w != None);

   state->status &= ~STAT_TAKEFOCUS;
   state->status &= ~STAT_DELETE;
   status = JXGetWindowProperty(display, w, atoms[ATOM_WM_PROTOCOLS],
                                0, 32, False, XA_ATOM, &realType, &realFormat,
                                &count, &extra, &temp);
   p = (Atom*)temp;
   if(status != Success || realFormat == 0 || !p) {
      return;
   }

   for(x = 0; x < count; x++) {
      if(p[x] == atoms[ATOM_WM_DELETE_WINDOW]) {
         state->status |= STAT_DELETE;
      } else if(p[x] == atoms[ATOM_WM_TAKE_FOCUS]) {
         state->status |= STAT_TAKEFOCUS;
      }
   }

   JXFree(p);

}

/** Read the "normal hints" for a client. */
void ReadWMNormalHints(ClientNode *np)
{

   XSizeHints hints;
   long temp;

   Assert(np);

   if(!JXGetWMNormalHints(display, np->window, &hints, &temp)) {
      np->sizeFlags = 0;
   } else {
      np->sizeFlags = hints.flags;
   }

   if(np->sizeFlags & PResizeInc) {
      np->xinc = Max(1, hints.width_inc);
      np->yinc = Max(1, hints.height_inc);
   } else {
      np->xinc = 1;
      np->yinc = 1;
   }

   if(np->sizeFlags & PMinSize) {
      np->minWidth = Max(0, hints.min_width);
      np->minHeight = Max(0, hints.min_height);
   } else {
      np->minWidth = 1;
      np->minHeight = 1;
   }

   if(np->sizeFlags & PMaxSize) {
      np->maxWidth =  hints.max_width;
      np->maxHeight = hints.max_height;
      if(np->maxWidth <= 0) {
         np->maxWidth = rootWidth;
      }
      if(np->maxHeight <= 0) {
         np->maxHeight = rootHeight;
      }
   } else {
      np->maxWidth = MAX_WINDOW_WIDTH;
      np->maxHeight = MAX_WINDOW_HEIGHT;
   }

   if(np->sizeFlags & PBaseSize) {
      np->baseWidth = hints.base_width;
      np->baseHeight = hints.base_height;
   } else if(np->sizeFlags & PMinSize) {
      np->baseWidth = np->minWidth;
      np->baseHeight = np->minHeight;
   } else {
      np->baseWidth = 0;
      np->baseHeight = 0;
   }

   if(np->sizeFlags & PAspect) {
      np->aspect.minx = hints.min_aspect.x;
      np->aspect.miny = hints.min_aspect.y;
      np->aspect.maxx = hints.max_aspect.x;
      np->aspect.maxy = hints.max_aspect.y;
   }

   if(np->sizeFlags & PWinGravity) {
      np->gravity = hints.win_gravity;
   } else {
      np->gravity = 1;
   }

}

/** Read colormap information for a client. */
void ReadWMColormaps(ClientNode *np)
{

   Window *windows;
   ColormapNode *cp;
   int count;

   Assert(np);

   if(JXGetWMColormapWindows(display, np->window, &windows, &count)) {
      if(count > 0) {
         int x;

         /* Free old colormaps. */
         while(np->colormaps) {
            cp = np->colormaps->next;
            Release(np->colormaps);
            np->colormaps = cp;
         }

         /* Put the maps in the list in order so they will come out in
          * reverse order. This way they will be installed with the
          * most important last.
          * Keep track of at most colormapCount colormaps for each
          * window to avoid doing extra work. */
         count = Min(colormapCount, count);
         for(x = 0; x < count; x++) {
            cp = Allocate(sizeof(ColormapNode));
            cp->window = windows[x];
            cp->next = np->colormaps;
            np->colormaps = cp;
         }

         JXFree(windows);

      }
   }

}

/** Read the WM state for a window. */
void ReadWMState(Window win, ClientState *state)
{

   Status status;
   unsigned long count;
   unsigned long extra;
   Atom realType;
   int realFormat;
   unsigned long *temp;

   count = 0;
   status = JXGetWindowProperty(display, win, atoms[ATOM_WM_STATE], 0, 2,
                                False, atoms[ATOM_WM_STATE],
                                &realType, &realFormat,
                                &count, &extra, (unsigned char**)&temp);
   if(JLIKELY(status == Success && realFormat != 0)) {
      if(JLIKELY(count == 2)) {
         switch(temp[0]) {
         case IconicState:
            state->status |= STAT_MINIMIZED;
            break;
         case WithdrawnState:
            state->status &= ~STAT_MAPPED;
            break;
         default:
            break;
         }
      }
      JXFree(temp);
   }

}

/** Read the WM hints for a window. */
void ReadWMHints(Window win, ClientState *state, char alreadyMapped)
{

   XWMHints *wmhints;

   Assert(win != None);
   Assert(state);

   state->status |= STAT_CANFOCUS;
   wmhints = JXGetWMHints(display, win);
   if(wmhints) {
      if(!alreadyMapped && (wmhints->flags & StateHint)) {
         switch(wmhints->initial_state) {
         case IconicState:
            state->status |= STAT_MINIMIZED;
            break;
         default:
            break;
         }
      }
      if((wmhints->flags & InputHint) && wmhints->input == False) {
         state->status &= ~STAT_CANFOCUS;
      }
      if(wmhints->flags & XUrgencyHint) {
         state->status |= STAT_URGENT;
      } else {
         state->status &= ~(STAT_URGENT | STAT_FLASH);
      }
      JXFree(wmhints);
   }

}

/** Read _NET_WM_WINDOW_OPACITY. */
void ReadWMOpacity(Window win, unsigned *opacity)
{
   unsigned long card;
   if(GetCardinalAtom(win, ATOM_NET_WM_WINDOW_OPACITY, &card)) {
      *opacity = card;
   } else {
      *opacity = UINT_MAX;
   }
}

/** Read _MOTIF_WM_HINTS */
void ReadMotifHints(Window win, ClientState *state)
{

   PropMwmHints *mhints;
   Atom type;
   unsigned long itemCount, bytesLeft;
   unsigned char *data;
   int format;
   int status;

   Assert(win != None);
   Assert(state);

   status = JXGetWindowProperty(display, win, atoms[ATOM_MOTIF_WM_HINTS],
                                0L, 20L, False, atoms[ATOM_MOTIF_WM_HINTS],
                                &type, &format, &itemCount, &bytesLeft, &data);
   if(status != Success || type == 0) {
      return;
   }

   mhints = (PropMwmHints*)data;
   if(JLIKELY(mhints)) {

      if((mhints->flags & MWM_HINTS_FUNCTIONS)
         && !(mhints->functions & MWM_FUNC_ALL)) {

         if(!(mhints->functions & MWM_FUNC_RESIZE)) {
            state->border &= ~BORDER_RESIZE;
         }
         if(!(mhints->functions & MWM_FUNC_MOVE)) {
            state->border &= ~BORDER_MOVE;
         }
         if(!(mhints->functions & MWM_FUNC_MINIMIZE)) {
            state->border &= ~BORDER_MIN;
         }
         if(!(mhints->functions & MWM_FUNC_MAXIMIZE)) {
            state->border &= ~BORDER_MAX;
         }
         if(!(mhints->functions & MWM_FUNC_CLOSE)) {
            state->border &= ~BORDER_CLOSE;
         }
      }

      if((mhints->flags & MWM_HINTS_DECORATIONS)
         && !(mhints->decorations & MWM_DECOR_ALL)) {

         if(!(mhints->decorations & MWM_DECOR_BORDER)) {
            state->border &= ~BORDER_OUTLINE;
         }
         if(!(mhints->decorations & MWM_DECOR_TITLE)) {
            state->border &= ~BORDER_TITLE;
         }
      }

      JXFree(mhints);
   }
}

/** Read a cardinal atom. */
char GetCardinalAtom(Window window, AtomType atom, unsigned long *value)
{

   unsigned long count;
   int status;
   unsigned long extra;
   Atom realType;
   int realFormat;
   unsigned char *data;
   char ret;

   Assert(window != None);
   Assert(value);

   count = 0;
   status = JXGetWindowProperty(display, window, atoms[atom], 0, 1, False,
                                XA_CARDINAL, &realType, &realFormat,
                                &count, &extra, &data);
   ret = 0;
   if(status == Success && realFormat != 0 && data) {
      if(JLIKELY(count == 1)) {
         *value = *(unsigned long*)data;
         ret = 1;
      }
      JXFree(data);
   }

   return ret;

}

/** Set a cardinal atom. */
void SetCardinalAtom(Window window, AtomType atom, unsigned long value)
{
   Assert(window != None);
   JXChangeProperty(display, window, atoms[atom], XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char*)&value, 1);
}

/** Read a window atom. */
char GetWindowAtom(Window window, AtomType atom, Window *value)
{

   unsigned long count;
   int status;
   unsigned long extra;
   Atom realType;
   int realFormat;
   unsigned char *data;
   char ret;

   Assert(window != None);
   Assert(value);

   count = 0;
   status = JXGetWindowProperty(display, window, atoms[atom], 0, 1, False,
                                XA_WINDOW, &realType, &realFormat,
                                &count, &extra, &data);
   ret = 0;
   if(status == Success && realFormat != 0 && data) {
      if(JLIKELY(count == 1)) {
         *value = *(Window*)data;
         ret = 1;
      }
      JXFree(data);
   }

   return ret;

}

/** Set a window atom. */
void SetWindowAtom(Window window, AtomType atom, unsigned long value)
{
   Assert(window != None);
   JXChangeProperty(display, window, atoms[atom], XA_WINDOW, 32,
                    PropModeReplace, (unsigned char*)&value, 1);
}

/** Set a pixmap atom. */
void SetPixmapAtom(Window window, AtomType atom, Pixmap value)
{
   Assert(window != None);
   JXChangeProperty(display, window, atoms[atom], XA_PIXMAP, 32,
                    PropModeReplace, (unsigned char*)&value, 1);
}

/** Set an atom atom. */
void SetAtomAtom(Window window, AtomType atom, AtomType value)
{
   Assert(window != None);
   JXChangeProperty(display, window, atoms[atom], XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)&atoms[value], 1);
}
