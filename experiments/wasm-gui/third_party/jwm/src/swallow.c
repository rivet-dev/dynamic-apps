/**
 * @file swallow.c
 * @author Joe Wingbermuehle
 * @date 2005-2006
 *
 * @brief Swallow tray component.
 *
 */

#include "jwm.h"
#include "swallow.h"
#include "main.h"
#include "tray.h"
#include "error.h"
#include "command.h"
#include "color.h"
#include "client.h"
#include "misc.h"

typedef struct SwallowNode {

   TrayComponentType *cp;

   char *name;
   char *command;
   int border;
   int userWidth;
   int userHeight;

   struct SwallowNode *next;

} SwallowNode;

static SwallowNode *pendingNodes = NULL;
static SwallowNode *swallowNodes = NULL;

static void ReleaseNodes(SwallowNode *nodes);
static void Destroy(TrayComponentType *cp);
static void Resize(TrayComponentType *cp);

/** Start swallow processing. */
void StartupSwallow(void)
{
   SwallowNode *np;
   for(np = pendingNodes; np; np = np->next) {
      if(np->command) {
         RunCommand(np->command);
      }
   }
}

/** Destroy swallow data. */
void DestroySwallow(void)
{
   ReleaseNodes(pendingNodes);
   ReleaseNodes(swallowNodes);
   pendingNodes = NULL;
   swallowNodes = NULL;
}

/** Release a linked list of swallow nodes. */
void ReleaseNodes(SwallowNode *nodes)
{
   while(nodes) {
      SwallowNode *np = nodes->next;
      Assert(nodes->name);
      Release(nodes->name);
      if(nodes->command) {
         Release(nodes->command);
      }
      Release(nodes);
      nodes = np;
   }
}

/** Create a swallowed application tray component. */
TrayComponentType *CreateSwallow(const char *name, const char *command,
                                 int width, int height)
{

   TrayComponentType *cp;
   SwallowNode *np;

   if(JUNLIKELY(!name)) {
      Warning(_("cannot swallow a client with no name"));
      return NULL;
   }

   np = Allocate(sizeof(SwallowNode));
   np->name = CopyString(name);
   np->command = CopyString(command);

   np->next = pendingNodes;
   pendingNodes = np;

   cp = CreateTrayComponent();
   np->cp = cp;
   cp->object = np;
   cp->Destroy = Destroy;
   cp->Resize = Resize;

   if(width) {
      cp->requestedWidth = width;
      np->userWidth = 1;
   } else {
      cp->requestedWidth = 1;
      np->userWidth = 0;
   }
   if(height) {
      cp->requestedHeight = height;
      np->userHeight = 1;
   } else {
      cp->requestedHeight = 1;
      np->userHeight = 0;
   }

   return cp;

}

/** Process an event on a swallowed window. */
char ProcessSwallowEvent(const XEvent *event)
{

   SwallowNode *np;
   int width, height;

   for(np = swallowNodes; np; np = np->next) {
      if(event->xany.window == np->cp->window) {
         switch(event->type) {
         case DestroyNotify:
            np->cp->window = None;
            np->cp->requestedWidth = 1;
            np->cp->requestedHeight = 1;
            ResizeTray(np->cp->tray);
            break;
         case ResizeRequest:
            np->cp->requestedWidth
               = event->xresizerequest.width + np->border * 2;
            np->cp->requestedHeight
               = event->xresizerequest.height + np->border * 2;
            ResizeTray(np->cp->tray);
            break;
         case ConfigureNotify:
            /* I don't think this should be necessary, but somehow
             * resize requests slip by sometimes... */
            width = event->xconfigure.width + np->border * 2;
            height = event->xconfigure.height + np->border * 2;
            if(   width != np->cp->requestedWidth
               && height != np->cp->requestedHeight) {
               np->cp->requestedWidth = width;
               np->cp->requestedHeight = height;
               ResizeTray(np->cp->tray);
            }
            break;
         default:
            break;
         }
         return 1;
      }
   }

   return 0;

}

/** Handle a tray resize. */
void Resize(TrayComponentType *cp)
{


   SwallowNode *np = (SwallowNode*)cp->object;

   if(cp->window != None) {
      const unsigned int width = cp->width - np->border * 2;
      const unsigned int height = cp->height - np->border * 2;
      JXResizeWindow(display, cp->window, width, height);
   }

}

/** Destroy a swallow tray component. */
void Destroy(TrayComponentType *cp)
{

   /* Destroy the window if there is one. */
   if(cp->window) {

      JXReparentWindow(display, cp->window, rootWindow, 0, 0);
      JXRemoveFromSaveSet(display, cp->window);

      ClientState state;
      memset(&state, 0, sizeof(state));
      ReadWMProtocols(cp->window, &state);
      if(state.status & STAT_DELETE) {
         SendClientMessage(cp->window, ATOM_WM_PROTOCOLS,
                           ATOM_WM_DELETE_WINDOW);
      } else {
         JXKillClient(display, cp->window);
      }

   }

}

/** Determine if this is a window to be swallowed, if it is, swallow it. */
char CheckSwallowMap(Window win)
{

   SwallowNode **npp;
   XClassHint hint;
   XWindowAttributes attr;
   char result;

   /* Return if there are no programs left to swallow. */
   if(!pendingNodes) {
      return 0;
   }

   /* Get the name of the window. */
   if(JXGetClassHint(display, win, &hint) == 0) {
      return 0;
   }

   /* Check if we should swallow this window. */
   result = 0;
   npp = &pendingNodes;
   while(*npp) {

      SwallowNode *np = *npp;
      Assert(np->cp->tray->window != None);

      if(!strcmp(hint.res_name, np->name)) {

         /* Swallow the window. */
         JXSelectInput(display, win,
                       StructureNotifyMask | ResizeRedirectMask);
         JXAddToSaveSet(display, win);
         JXSetWindowBorder(display, win, colors[COLOR_TRAY_BG2]);
         JXReparentWindow(display, win,
                          np->cp->tray->window, 0, 0);
         JXMapRaised(display, win);
         np->cp->window = win;

         /* Remove this node from the pendingNodes list and place it
          * on the swallowNodes list. */
         *npp = np->next;
         np->next = swallowNodes;
         swallowNodes = np;

         /* Update the size. */
         JXGetWindowAttributes(display, win, &attr);
         np->border = attr.border_width;
         if(!np->userWidth) {
            np->cp->requestedWidth = attr.width + 2 * np->border;
         }
         if(!np->userHeight) {
            np->cp->requestedHeight = attr.height + 2 * np->border;
         }

         ResizeTray(np->cp->tray);
         result = 1;

         break;
      }

      npp = &np->next;

   }
   JXFree(hint.res_name);
   JXFree(hint.res_class);

   return result;

}

/** Determine if there are swallow processes pending. */
char IsSwallowPending(void)
{
   return pendingNodes ? 1 : 0;
}

