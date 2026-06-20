/**
 * @file clientlist.c
 * @author Joe Wingbermuehle
 * @date 2007
 *
 * @brief Functions to manage lists of clients.
 *
 */

#include "jwm.h"
#include "clientlist.h"
#include "client.h"
#include "binding.h"
#include "event.h"
#include "tray.h"
#include "settings.h"

ClientNode *nodes[LAYER_COUNT];
ClientNode *nodeTail[LAYER_COUNT];

static Window *windowStack = NULL;  /**< Image of the window stack. */
static int windowStackSize = 0;     /**< Size of the image. */
static int windowStackCurrent = 0;  /**< Current location in the image. */
static char walkingWindows = 0;     /**< Are we walking windows? */
static char wasMinimized = 0;       /**< Was the current window minimized? */

/** Determine if a client is allowed focus. */
char ShouldFocus(const ClientNode *np, char current)
{

   /* Only display clients on the current desktop or clients that are sticky. */
   if(!settings.listAllTasks || current) {
      if(!IsClientOnCurrentDesktop(np)) {
         return 0;
      }
   }

   /* Don't display a client if it doesn't want to be displayed. */
   if(np->state.status & STAT_NOLIST) {
      return 0;
   }

   /* Don't display a client on the tray if it has an owner. */
   if(np->owner != None) {
      return 0;
   }

   if(!(np->state.status & (STAT_MAPPED | STAT_MINIMIZED | STAT_SHADED))) {
      return 0;
   }

   return 1;

}

/** Start walking windows in client list order. */
void StartWindowWalk(void)
{
   JXGrabKeyboard(display, rootWindow, False, GrabModeAsync,
                  GrabModeAsync, CurrentTime);
   RaiseTrays();
   walkingWindows = 1;
}

/** Start walking the window stack. */
void StartWindowStackWalk(void)
{

   /* Get an image of the window stack.
    * Here we get the Window IDs rather than client pointers so
    * clients can be added/removed without disrupting the stack walk.
    */

   ClientNode *np;
   int layer;
   int count;

   /* If we are already walking the stack, just return. */
   if(windowStack != NULL) {
      return;
   }

   /* First determine how much space to allocate for windows. */
   count = 0;
   for(layer = LAST_LAYER; layer >= FIRST_LAYER; layer--) {
      for(np = nodes[layer]; np; np = np->next) {
         if(ShouldFocus(np, 1)) {
            ++count;
         }
      }
   }

   /* If there were no windows to walk, don't even start. */
   if(count == 0) {
      return;
   }

   /* Allocate space for the windows. */
   windowStack = Allocate(sizeof(Window) * count);

   /* Copy windows into the array. */
   windowStackSize = 0;
   for(layer = LAST_LAYER; layer >= FIRST_LAYER; layer--) {
      for(np = nodes[layer]; np; np = np->next) {
         if(ShouldFocus(np, 1)) {
            windowStack[windowStackSize++] = np->window;
         }
      }
   }

   Assert(windowStackSize == count);

   windowStackCurrent = 0;

   JXGrabKeyboard(display, rootWindow, False, GrabModeAsync,
                  GrabModeAsync, CurrentTime);

   RaiseTrays();

   walkingWindows = 1;
   wasMinimized = 0;

}

/** Move to the next window in the window stack. */
void WalkWindowStack(char forward)
{

   ClientNode *np;

   if(windowStack != NULL) {
      int x;

      if(wasMinimized) {
         np = FindClientByWindow(windowStack[windowStackCurrent]);
         if(np) {
            MinimizeClient(np, 1);
         }
      }

      /* Loop until we either raise a window or go through them all. */
      for(x = 0; x < windowStackSize; x++) {

         /* Move to the next/previous window (wrap if needed). */
         if(forward) {
             windowStackCurrent = (windowStackCurrent + 1) % windowStackSize;
         } else {
             if(windowStackCurrent == 0) {
                 windowStackCurrent = windowStackSize;
             }
             windowStackCurrent -= 1;
         }

         /* Look up the window. */
         np = FindClientByWindow(windowStack[windowStackCurrent]);

         /* Skip this window if it no longer exists or is currently in
          * a state that doesn't allow focus.
          */
         if(np == NULL || !ShouldFocus(np, 1)
            || (np->state.status & STAT_ACTIVE)) {
            continue;
         }

         /* Show the window.
          * Only when the walk completes do we update the stacking order. */
         RestackClients();
         if(np->state.status & STAT_MINIMIZED) {
            RestoreClient(np, 1);
            wasMinimized = 1;
         } else {
            wasMinimized = 0;
         }
         JXRaiseWindow(display, np->parent ? np->parent : np->window);
         FocusClient(np);
         break;

      }

   }

}

/** Stop walking the window stack or client list. */
void StopWindowWalk(void)
{

   ClientNode *np;

   /* Raise the selected window and free the window array. */
   if(windowStack != NULL) {

      /* Look up the current window. */
      np = FindClientByWindow(windowStack[windowStackCurrent]);
      if(np) {
         if(np->state.status & STAT_MINIMIZED) {
            RestoreClient(np, 1);
         } else {
            RaiseClient(np);
         }
      }

      Release(windowStack);
      windowStack = NULL;

      windowStackSize = 0;
      windowStackCurrent = 0;

   }

   if(walkingWindows) {
      JXUngrabKeyboard(display, CurrentTime);
      LowerTrays();
      walkingWindows = 0;
   }

}

/** Focus the next client in the stacking order. */
void FocusNextStacked(ClientNode *np)
{

   int x;
   ClientNode *tp;

   Assert(np);

   for(tp = np->next; tp; tp = tp->next) {
      if((tp->state.status & (STAT_MAPPED | STAT_SHADED))
         && !(tp->state.status & STAT_HIDDEN)) {
         FocusClient(tp);
         return;
      }
   }
   for(x = np->state.layer - 1; x >= FIRST_LAYER; x--) {
      for(tp = nodes[x]; tp; tp = tp->next) {
         if((tp->state.status & (STAT_MAPPED | STAT_SHADED))
            && !(tp->state.status & STAT_HIDDEN)) {
            FocusClient(tp);
            return;
         }
      }
   }

   /* No client to focus. */
   JXSetInputFocus(display, rootWindow, RevertToParent, eventTime);

}

