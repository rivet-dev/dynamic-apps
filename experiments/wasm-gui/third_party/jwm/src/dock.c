/**
 * @file dock.c
 * @author Joe Wingbermuehle
 * @date 2006
 *
 * @brief Dock tray component.
 *
 */

#include "jwm.h"
#include "dock.h"
#include "tray.h"
#include "main.h"
#include "error.h"
#include "color.h"
#include "misc.h"
#include "settings.h"

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define SYSTEM_TRAY_ORIENTATION_HORZ 0
#define SYSTEM_TRAY_ORIENTATION_VERT 1

/** Structure to represent a docked window. */
typedef struct DockNode {

   Window window;
   char needs_reparent;

   struct DockNode *next;

} DockNode;

/** Structure to represent a dock tray component. */
typedef struct DockType {

   TrayComponentType *cp;

   Window window;
   int itemSize;

   Pixmap buffer;

   DockNode *nodes;

} DockType;

static const char BASE_SELECTION_NAME[] = "_NET_SYSTEM_TRAY_S%d";

static DockType *dock = NULL;
static char owner;
static Atom dockAtom;
static unsigned long orientation;

static void SetSize(TrayComponentType *cp, int width, int height);
static void Create(TrayComponentType *cp);
static void Resize(TrayComponentType *cp);

static void DockWindow(Window win);
static void UpdateDock(void);
static void GetDockItemSize(int *size);
static void GetDockItemOffset(int *size, int *xoffset, int *yoffset);
static void GetDockSize(int *width, int *height);

/** Initialize dock data. */
void InitializeDock(void)
{
   owner = 0;
}

/** Startup the dock. */
void StartupDock(void)
{

   char *selectionName;

   if(!dock) {
      /* No dock has been requested. */
      return;
   }

   if(dock->window == None) {

      /* No dock yet. */

      /* Get the selection atom. */
      selectionName = AllocateStack(sizeof(BASE_SELECTION_NAME));
      snprintf(selectionName, sizeof(BASE_SELECTION_NAME),
               BASE_SELECTION_NAME, rootScreen);
      dockAtom = JXInternAtom(display, selectionName, False);
      ReleaseStack(selectionName);

      /* The location and size of the window doesn't matter here. */
      dock->window = JXCreateSimpleWindow(display, rootWindow,
         /* x, y, width, height */ 0, 0, 1, 1,
         /* border_size, border_color */ 0, 0,
         /* background */ colors[COLOR_TRAY_BG2]);
      JXSelectInput(display, dock->window,
           SubstructureNotifyMask
         | SubstructureRedirectMask
         | EnterWindowMask
         | PointerMotionMask | PointerMotionHintMask);

   }
   dock->cp->window = dock->window;

}

/** Shutdown the dock. */
void ShutdownDock(void)
{

   DockNode *np;

   if(dock) {

      /* Release memory used by the dock list. */
      while(dock->nodes) {
         np = dock->nodes->next;
         JXReparentWindow(display, dock->nodes->window, rootWindow, 0, 0);
         Release(dock->nodes);
         dock->nodes = np;
      }

      /* Release the selection. */
      if(owner) {
         JXSetSelectionOwner(display, dockAtom, None, CurrentTime);
      }

      /* Free dock pixmap and destroy the window. */
      JXFreePixmap(display, dock->buffer);
      JXDestroyWindow(display, dock->window);

   }

}

/** Destroy dock data. */
void DestroyDock(void)
{
   if(dock) {
      Release(dock);
      dock = NULL;
   }
}

/** Create a dock component. */
TrayComponentType *CreateDock(int width)
{
   TrayComponentType *cp;

   if(JUNLIKELY(dock != NULL && dock->cp != NULL)) {
      Warning(_("only one Dock allowed"));
      return NULL;
   } else if(dock == NULL) {
      dock = Allocate(sizeof(DockType));
      dock->nodes = NULL;
      dock->window = None;
   }

   cp = CreateTrayComponent();
   cp->object = dock;
   cp->requestedWidth = 1;
   cp->requestedHeight = 1;
   dock->cp = cp;
   dock->itemSize = width;

   cp->SetSize = SetSize;
   cp->Create = Create;
   cp->Resize = Resize;

   return cp;

}

/** Set the size of a dock component. */
void SetSize(TrayComponentType *cp, int width, int height)
{

   Assert(cp);
   Assert(dock);

   /* Set the orientation. */
   if(width == 0) {
      orientation = SYSTEM_TRAY_ORIENTATION_HORZ;
   } else if(height == 0) {
      orientation = SYSTEM_TRAY_ORIENTATION_VERT;
   }

   /* Get the size. */
   cp->width = width;
   cp->height = height;
   GetDockSize(&cp->width, &cp->height);
   if(width == 0) {
      cp->requestedWidth = cp->width;
      cp->requestedHeight = 0;
   } else {
      cp->requestedWidth = 0;
      cp->requestedHeight = cp->height;
   }

}

/** Initialize a dock component. */
void Create(TrayComponentType *cp)
{

   DockType *tp = (DockType*)cp->object;
   XEvent event;

   Assert(cp);

   /* Map the dock window. */
   if(cp->window != None) {
      JXResizeWindow(display, cp->window, cp->width, cp->height);
      JXMapRaised(display, cp->window);
   }

   /* Set the orientation atom. */
   SetCardinalAtom(dock->cp->window, ATOM_NET_SYSTEM_TRAY_ORIENTATION,
                   orientation);

   /* Get the selection if we don't already own it.
    * If we did already own it, getting it again would cause problems
    * with some clients due to the way restarts are handled.
    */
   if(!owner) {

      owner = 1;
      JXSetSelectionOwner(display, dockAtom, dock->cp->window, CurrentTime);
      if(JUNLIKELY(JXGetSelectionOwner(display, dockAtom)
                   != dock->cp->window)) {

         owner = 0;
         Warning(_("could not acquire system tray selection"));

      } else {

         memset(&event, 0, sizeof(event));
         event.xclient.type = ClientMessage;
         event.xclient.window = rootWindow;
         event.xclient.message_type = atoms[ATOM_MANAGER];
         event.xclient.format = 32;
         event.xclient.data.l[0] = CurrentTime;
         event.xclient.data.l[1] = dockAtom;
         event.xclient.data.l[2] = dock->cp->window;
         event.xclient.data.l[3] = 0;
         event.xclient.data.l[4] = 0;

         JXSendEvent(display, rootWindow, False, StructureNotifyMask, &event);

      }

   }

   /* Create and assign dock background pixmap. */
   cp->pixmap = JXCreatePixmap(display, rootWindow, cp->width, cp->height,
                               rootDepth);
   tp->buffer = cp->pixmap;
   ClearTrayDrawable(cp);
   JXSetWindowBackgroundPixmap(display, cp->window, tp->buffer);

}

/** Resize a dock component. */
void Resize(TrayComponentType *cp)
{
   DockType *tp = (DockType*)cp->object;

   JXResizeWindow(display, cp->window, cp->width, cp->height);

   /* Recreate and assign dock background pixmap. */
   if(tp->buffer != None) {
      JXFreePixmap(display, tp->buffer);
   }
   cp->pixmap = JXCreatePixmap(display, rootWindow, cp->width, cp->height,
                               rootDepth);
   tp->buffer = cp->pixmap;
   ClearTrayDrawable(cp);
   JXSetWindowBackgroundPixmap(display, cp->window, tp->buffer);
   UpdateDock();
}

/** Handle a dock event. */
void HandleDockEvent(const XClientMessageEvent *event)
{
   Assert(event);
   switch(event->data.l[1]) {
   case SYSTEM_TRAY_REQUEST_DOCK:
      DockWindow(event->data.l[2]);
      break;
   case SYSTEM_TRAY_BEGIN_MESSAGE:
      break;
   case SYSTEM_TRAY_CANCEL_MESSAGE:
      break;
   default:
      Debug("invalid opcode in dock event");
      break;
   }
}

/** Handle a resize request event. */
char HandleDockResizeRequest(const XResizeRequestEvent *event)
{
   DockNode *np;

   Assert(event);

   if(!dock) {
      return 0;
   }

   for(np = dock->nodes; np; np = np->next) {
      if(np->window == event->window) {
         UpdateDock();
         return 1;
      }
   }

   return 0;
}

/** Handle a configure request event. */
char HandleDockConfigureRequest(const XConfigureRequestEvent *event)
{

   DockNode *np;

   Assert(event);

   if(!dock) {
      return 0;
   }

   for(np = dock->nodes; np; np = np->next) {
      if(np->window == event->window) {
         UpdateDock();
         return 1;
      }
   }

   return 0;

}

/** Handle a reparent notify event. */
char HandleDockReparentNotify(const XReparentEvent *event)
{

   DockNode *np;
   char handled;

   Assert(event);

   /* Just return if there is no dock. */
   if(!dock) {
      return 0;
   }

   /* Check each docked window. */
   handled = 0;
   for(np = dock->nodes; np; np = np->next) {
      if(np->window == event->window) {
         if(event->parent != dock->cp->window) {
            /* For some reason the application reparented the window.
             * We make note of this condition and reparent every time
             * the dock is updated. Unfortunately we can't do this for
             * all applications because some won't deal with it.
             */
            np->needs_reparent = 1;
            handled = 1;
         }
      }
   }

   /* Layout the stuff on the dock again if something happened. */
   if(handled) {
      UpdateDock();
   }

   return handled;

}

/** Handle a selection clear event. */
char HandleDockSelectionClear(const XSelectionClearEvent *event)
{
   if(event->selection == dockAtom) {
      Debug("lost _NET_SYSTEM_TRAY selection");
      owner = 0;
   }
   return 0;
}

/** Add a window to the dock. */
void DockWindow(Window win)
{
   DockNode *np;

   /* If no dock is running, just return. */
   if(!dock) {
      return;
   }

   /* Make sure we have a valid window to add. */
   if(JUNLIKELY(win == None)) {
      return;
   }

   /* If this window is already docked ignore it. */
   for(np = dock->nodes; np; np = np->next) {
      if(np->window == win) {
         return;
      }
   }

   /* Add the window to our list. */
   np = Allocate(sizeof(DockNode));
   np->window = win;
   np->needs_reparent = 0;
   np->next = dock->nodes;
   dock->nodes = np;

   /* Update the requested size. */
   GetDockSize(&dock->cp->requestedWidth, &dock->cp->requestedHeight);

   /* It's safe to reparent at (0, 0) since we call
    * ResizeTray which will invoke the Resize callback.
    */
   JXAddToSaveSet(display, win);
   JXReparentWindow(display, win, dock->cp->window, 0, 0);
   JXMapRaised(display, win);

   /* Resize the tray containing the dock. */
   ResizeTray(dock->cp->tray);

}

/** Remove a window from the dock. */
char HandleDockDestroy(Window win)
{
   DockNode **np;

   /* If no dock is running, just return. */
   if(!dock) {
      return 0;
   }

   for(np = &dock->nodes; *np; np = &(*np)->next) {
      DockNode *dp = *np;
      if(dp->window == win) {

         /* Remove the window from our list. */
         *np = dp->next;
         Release(dp);

         /* Update the requested size. */
         GetDockSize(&dock->cp->requestedWidth, &dock->cp->requestedHeight);

         /* Resize the tray. */
         ResizeTray(dock->cp->tray);
         return 1;
      }
   }

   return 0;
}

/** Layout items on the dock. */
void UpdateDock(void)
{

   XConfigureEvent event;
   DockNode *np;
   int x, y;
   int xoffset, yoffset;
   int itemSize;

   Assert(dock);

   /* Determine the size of items in the dock. */
   GetDockItemSize(&itemSize);

   /* Determine the offset of the items in the dock. */
   GetDockItemOffset(&itemSize, &xoffset, &yoffset);

   x = 0;
   y = 0;
   memset(&event, 0, sizeof(event));
   for(np = dock->nodes; np; np = np->next) {

      JXMoveResizeWindow(display, np->window, x + xoffset, y + yoffset, itemSize, itemSize);

      /* Reparent if this window likes to go other places. */
      if(np->needs_reparent) {
         JXReparentWindow(display, np->window, dock->cp->window, x + xoffset, y + yoffset);
      }

      event.type = ConfigureNotify;
      event.event = np->window;
      event.window = np->window;
      event.x = dock->cp->screenx + x;
      event.y = dock->cp->screeny + y;
      event.width = itemSize;
      event.height = itemSize;
      JXSendEvent(display, np->window, False, StructureNotifyMask,
                  (XEvent*)&event);

      if(orientation == SYSTEM_TRAY_ORIENTATION_HORZ) {
         x += itemSize + settings.dockSpacing;
      } else {
         y += itemSize + settings.dockSpacing;
      }

   }

}

/** Get the size of a particular window on the dock. */
void GetDockItemSize(int *size)
{
   /* Determine the default size of items in the dock. */
   if(orientation == SYSTEM_TRAY_ORIENTATION_HORZ) {
      *size = dock->cp->height;
   } else {
      *size = dock->cp->width;
   }
   if(dock->itemSize > 0 && *size > dock->itemSize) {
      *size = dock->itemSize;
   }
}

/** Get the offset of a particular window on the dock. */
void GetDockItemOffset(int *size, int *xoffset, int *yoffset)
{
   /* Calculate the offset of the item in the dock. */
   if(orientation == SYSTEM_TRAY_ORIENTATION_HORZ) {
      *yoffset = (dock->cp->height - *size) / 2;
      *xoffset = 0;
   } else {
      *xoffset = (dock->cp->width - *size) / 2;
      *yoffset = 0;
   }
}

/** Get the size of the dock. */
void GetDockSize(int *width, int *height)
{
   DockNode *np;
   int itemSize;

   Assert(dock != NULL);

   /* Get the dock item size. */
   GetDockItemSize(&itemSize);

   /* Determine the size of the items on the dock. */
   for(np = dock->nodes; np; np = np->next) {
      const unsigned spacing = (np->next ? settings.dockSpacing : 0);
      if(orientation == SYSTEM_TRAY_ORIENTATION_HORZ) {
         /* Horizontal tray; height fixed, placement is left to right. */
         *width += itemSize + spacing;
      } else {
         /* Vertical tray; width fixed, placement is top to bottom. */
         *height += itemSize + spacing;
      }
   }

   /* Don't allow the dock to have zero size since a size of
    * zero indicates a variable sized component. */
   if(orientation == SYSTEM_TRAY_ORIENTATION_HORZ) {
      *width = Max(*width, 1);
   } else {
      *height = Max(*height, 1);
   }

}
