/**
 * @file move.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Client window move functions.
 *
 */

#include "jwm.h"
#include "move.h"

#include "border.h"
#include "client.h"
#include "clientlist.h"
#include "cursor.h"
#include "event.h"
#include "binding.h"
#include "outline.h"
#include "pager.h"
#include "screen.h"
#include "status.h"
#include "tray.h"
#include "desktop.h"
#include "settings.h"
#include "timing.h"

typedef struct {
   int left, right;
   int top, bottom;
   char valid;
} RectangleType;

static char shouldStopMove;
static char atLeft;
static char atRight;
static char atBottom;
static char atTop;
static char atSideFirst;
static ClientNode *currentClient;
static TimeType moveTime;

static void StopMove(ClientNode *np, int doMove, int oldx, int oldy);
static void RestartMove(ClientNode *np, int *doMove);
static void MoveController(int wasDestroyed);

static void DoSnap(ClientNode *np);
static void DoSnapScreen(ClientNode *np);
static void DoSnapBorder(ClientNode *np);
static char ShouldSnap(const ClientNode *np);
static void GetClientRectangle(const ClientNode *np, RectangleType *r);

static char CheckOverlapTopBottom(const RectangleType *a,
                                  const RectangleType *b);
static char CheckOverlapLeftRight(const RectangleType *a,
                                  const RectangleType *b);

static char CheckLeftValid(const RectangleType *client,
                           const RectangleType *other,
                           const RectangleType *left);
static char CheckRightValid(const RectangleType *client,
                            const RectangleType *other,
                            const RectangleType *right);
static char CheckTopValid(const RectangleType *client,
                          const RectangleType *other,
                          const RectangleType *top);
static char CheckBottomValid(const RectangleType *client,
                             const RectangleType *other,
                             const RectangleType *bottom);

static void SignalMove(const TimeType *now, int x, int y, Window w, void *data);
static void UpdateDesktop(const TimeType *now);

/** Callback for stopping moves. */
void MoveController(int wasDestroyed)
{
   if(settings.moveMode == MOVE_OUTLINE) {
      ClearOutline();
   }

   JXUngrabPointer(display, CurrentTime);
   JXUngrabKeyboard(display, CurrentTime);

   DestroyMoveWindow();
   shouldStopMove = 1;
   atTop = 0;
   atBottom = 0;
   atLeft = 0;
   atRight = 0;
   atSideFirst = 0;
}

/** Move a client window. */
char MoveClient(ClientNode *np, int startx, int starty)
{
   XEvent event;
   const ScreenType *sp;
   MaxFlags flags;
   int oldx, oldy;
   int doMove;
   int north, south, east, west;
   int height;

   Assert(np);

   if(!(np->state.border & BORDER_MOVE)) {
      return 0;
   }
   if(np->state.status & STAT_FULLSCREEN) {
      return 0;
   }

   if(!GrabMouseForMove()) {
      return 0;
   }

   RegisterCallback(0, SignalMove, NULL);
   np->controller = MoveController;
   shouldStopMove = 0;

   oldx = np->x;
   oldy = np->y;

   if(!(GetMouseMask() & (Button1Mask | Button2Mask))) {
      StopMove(np, 0, oldx, oldy);
      return 0;
   }

   GetBorderSize(&np->state, &north, &south, &east, &west);
   startx -= west;
   starty -= north;

   currentClient = np;
   atTop = atBottom = atLeft = atRight = atSideFirst = 0;
   doMove = 0;
   for(;;) {

      WaitForEvent(&event);

      if(shouldStopMove) {
         np->controller = NULL;
         SetDefaultCursor(np->parent);
         UnregisterCallback(SignalMove, NULL);
         return doMove;
      }

      switch(event.type) {
      case ButtonRelease:
         if(event.xbutton.button == Button1
            || event.xbutton.button == Button2) {
            StopMove(np, doMove, oldx, oldy);
            return doMove;
         }
         break;
      case MotionNotify:

         DiscardMotionEvents(&event, np->window);

         np->x = event.xmotion.x_root - startx;
         np->y = event.xmotion.y_root - starty;

         /* Get the move time used for desktop switching. */
         if(!(atLeft | atTop | atRight | atBottom)) {
            if(event.xmotion.state & Mod1Mask) {
               moveTime.seconds = 0;
               moveTime.ms = 0;
            } else {
               GetCurrentTime(&moveTime);
            }
         }

         /* Determine if we are at a border for desktop switching. */
         sp = GetCurrentScreen(np->x + np->width / 2, np->y + np->height / 2);
         atLeft = atTop = atRight = atBottom = 0;
         if(event.xmotion.x_root <= sp->x) {
            atLeft = 1;
         } else if(event.xmotion.x_root >= sp->x + sp->width - 1) {
            atRight = 1;
         }
         if(event.xmotion.y_root <= sp->y) {
            atTop = 1;
         } else if(event.xmotion.y_root >= sp->y + sp->height - 1) {
            atBottom = 1;
         }

         flags = MAX_NONE;
         if(event.xmotion.state & Mod1Mask) {
            /* Switch desktops immediately if alt is pressed. */
            if(atLeft | atRight | atTop | atBottom) {
               TimeType now;
               GetCurrentTime(&now);
               UpdateDesktop(&now);
            }
         } else {
            /* If alt is not pressed, snap to borders. */
            if(np->state.status & STAT_AEROSNAP) {
               if(atTop & atLeft) {
                  if(atSideFirst) {
                     flags = MAX_TOP | MAX_LEFT;
                  } else {
                     flags = MAX_TOP | MAX_HORIZ;
                  }
               } else if(atTop & atRight) {
                  if(atSideFirst) {
                     flags = MAX_TOP | MAX_RIGHT;
                  } else {
                     flags = MAX_TOP | MAX_HORIZ;
                  }
               } else if(atBottom & atLeft) {
                  if(atSideFirst) {
                     flags = MAX_BOTTOM | MAX_LEFT;
                  } else {
                     flags = MAX_BOTTOM | MAX_HORIZ;
                  }
               } else if(atBottom & atRight) {
                  if(atSideFirst) {
                     flags = MAX_BOTTOM | MAX_RIGHT;
                  } else {
                     flags = MAX_BOTTOM | MAX_HORIZ;
                  }
               } else if(atLeft) {
                  flags = MAX_LEFT | MAX_VERT;
                  atSideFirst = 1;
               } else if(atRight) {
                  flags = MAX_RIGHT | MAX_VERT;
                  atSideFirst = 1;
               } else if(atTop | atBottom) {
                  flags = MAX_VERT | MAX_HORIZ;
                  atSideFirst = 0;
               }
               if(flags != np->state.maxFlags) {
                  if(settings.moveMode == MOVE_OUTLINE) {
                     ClearOutline();
                  }
                  MaximizeClient(np, flags);
               }
               if(!np->state.maxFlags) {
                  DoSnap(np);
               }
            } else {
               DoSnap(np);
            }
         }

         if(flags != MAX_NONE) {
            RestartMove(np, &doMove);
         } else if(!doMove && (abs(np->x - oldx) > MOVE_DELTA
            || abs(np->y - oldy) > MOVE_DELTA)) {

            if(np->state.maxFlags) {
               MaximizeClient(np, MAX_NONE);
            }

            CreateMoveWindow(np);
            doMove = 1;
         }

         if(doMove) {
            if(settings.moveMode == MOVE_OUTLINE) {
               ClearOutline();
               height = north + south;
               if(!(np->state.status & STAT_SHADED)) {
                  height += np->height;
               }
               DrawOutline(np->x - west, np->y - north,
                           np->width + west + east, height);
            } else {
               if(np->parent != None) {
                  JXMoveWindow(display, np->parent, np->x - west,
                               np->y - north);
               } else {
                  JXMoveWindow(display, np->window, np->x, np->y);
               }
               SendConfigureEvent(np);
            }
            UpdateMoveWindow(np);
            RequirePagerUpdate();
         }

         break;
      default:
         break;
      }
   }
}

/** Move a client window (keyboard or menu initiated). */
char MoveClientKeyboard(ClientNode *np)
{
   XEvent event;
   int oldx, oldy;
   int moved;
   int height;
   int north, south, east, west;
   Window win;

   Assert(np);

   if(!(np->state.border & BORDER_MOVE)) {
      return 0;
   }
   if(np->state.status & STAT_FULLSCREEN) {
      return 0;
   }

   if(np->state.maxFlags != MAX_NONE) {
      MaximizeClient(np, MAX_NONE);
   }

   win = np->parent != None ? np->parent : np->window;
   if(JUNLIKELY(JXGrabKeyboard(display, win, True, GrabModeAsync,
                               GrabModeAsync, CurrentTime))) {
      Debug("MoveClient: could not grab keyboard");
      return 0;
   }
   if(!GrabMouseForMove()) {
      JXUngrabKeyboard(display, CurrentTime);
      return 0;
   }

   GetBorderSize(&np->state, &north, &south, &east, &west);

   oldx = np->x;
   oldy = np->y;

   RegisterCallback(0, SignalMove, NULL);
   np->controller = MoveController;
   shouldStopMove = 0;

   CreateMoveWindow(np);
   UpdateMoveWindow(np);

   MoveMouse(rootWindow, np->x, np->y);
   DiscardMotionEvents(&event, np->window);

   if(np->state.status & STAT_SHADED) {
      height = 0;
   } else {
      height = np->height;
   }
   currentClient = np;

   for(;;) {

      WaitForEvent(&event);

      if(shouldStopMove) {
         np->controller = NULL;
         SetDefaultCursor(np->parent);
         UnregisterCallback(SignalMove, NULL);
         return 1;
      }

      moved = 0;

      if(event.type == KeyPress) {
         ActionType action;

         DiscardKeyEvents(&event, np->window);
         action = GetKey(MC_NONE, event.xkey.state, event.xkey.keycode);
         switch(action.action) {
         case ACTION_UP:
            if(np->y + height > 0) {
               np->y -= 10;
            }
            break;
         case ACTION_DOWN:
            if(np->y < rootHeight) {
               np->y += 10;
            }
            break;
         case ACTION_RIGHT:
            if(np->x < rootWidth) {
               np->x += 10;
            }
            break;
         case ACTION_LEFT:
            if(np->x + np->width > 0) {
               np->x -= 10;
            }
            break;
         default:
            StopMove(np, 1, oldx, oldy);
            return 1;
         }

         MoveMouse(rootWindow, np->x, np->y);
         DiscardMotionEvents(&event, np->window);

         moved = 1;

      } else if(event.type == MotionNotify) {

         DiscardMotionEvents(&event, np->window);

         np->x = event.xmotion.x;
         np->y = event.xmotion.y;

         moved = 1;

      } else if(event.type == ButtonRelease) {

         StopMove(np, 1, oldx, oldy);
         return 1;

      }

      if(moved) {

         if(settings.moveMode == MOVE_OUTLINE) {
            ClearOutline();
            DrawOutline(np->x - west, np->y - west,
                        np->width + west + east, height + north + west);
         } else {
            JXMoveWindow(display, win, np->x - west, np->y - north);
            SendConfigureEvent(np);
         }

         UpdateMoveWindow(np);
         RequirePagerUpdate();

      }

   }

}

/** Stop move. */
void StopMove(ClientNode *np, int doMove, int oldx, int oldy)
{
   int north, south, east, west;

   Assert(np);
   Assert(np->controller);

   (np->controller)(0);

   np->controller = NULL;

   SetDefaultCursor(np->parent);
   UnregisterCallback(SignalMove, NULL);

   if(!doMove) {
      np->x = oldx;
      np->y = oldy;
      return;
   }

   GetBorderSize(&np->state, &north, &south, &east, &west);
   if(np->parent != None) {
      JXMoveWindow(display, np->parent, np->x - west, np->y - north);
   } else {
      JXMoveWindow(display, np->window, np->x - west, np->y - north);
   }
   SendConfigureEvent(np);
}

/** Restart a move. */
void RestartMove(ClientNode *np, int *doMove)
{
   if(*doMove) {
      int north, south, east, west;
      *doMove = 0;
      DestroyMoveWindow();
      GetBorderSize(&np->state, &north, &south, &east, &west);
      if(np->parent != None) {
         JXMoveWindow(display, np->parent, np->x - west, np->y - north);
      } else {
         JXMoveWindow(display, np->window, np->x - west, np->y - north);
      }
      SendConfigureEvent(np);
   }
}

/** Snap to the screen and/or neighboring windows. */
void DoSnap(ClientNode *np)
{
   switch(settings.snapMode) {
   case SNAP_BORDER:
      DoSnapBorder(np);
      DoSnapScreen(np);
      break;
   case SNAP_SCREEN:
      DoSnapScreen(np);
      break;
   default:
      break;
   }
}

/** Snap to the screen. */
void DoSnapScreen(ClientNode *np)
{

   RectangleType client;
   int screen;
   const ScreenType *sp;
   int screenCount;
   int north, south, east, west;

   GetClientRectangle(np, &client);

   GetBorderSize(&np->state, &north, &south, &east, &west);

   screenCount = GetScreenCount();
   for(screen = 0; screen < screenCount; screen++) {

      sp = GetScreen(screen);

      if(abs(client.right - sp->width - sp->x) <= settings.snapDistance) {
         np->x = sp->x + sp->width - west - np->width;
      }
      if(abs(client.left - sp->x) <= settings.snapDistance) {
         np->x = sp->x + east;
      }
      if(abs(client.bottom - sp->height - sp->y) <= settings.snapDistance) {
         np->y = sp->y + sp->height - south;
         if(!(np->state.status & STAT_SHADED)) {
            np->y -= np->height;
         }
      }
      if(abs(client.top - sp->y) <= settings.snapDistance) {
         np->y = north + sp->y;
      }

   }

}

/** Snap to window borders. */
void DoSnapBorder(ClientNode *np)
{

   const ClientNode *tp;
   const TrayType *tray;
   RectangleType client, other;
   RectangleType left = { 0 };
   RectangleType right = { 0 };
   RectangleType top = { 0 };
   RectangleType bottom = { 0 };
   int layer;
   int north, south, east, west;

   GetClientRectangle(np, &client);

   GetBorderSize(&np->state, &north, &south, &east, &west);

   other.valid = 1;

   /* Work from the bottom of the window stack to the top. */
   for(layer = 0; layer < LAYER_COUNT; layer++) {

      /* Check tray windows. */
      for(tray = GetTrays(); tray; tray = tray->next) {

         if(tray->hidden) {
            continue;
         }

         other.left = tray->x;
         other.right = tray->x + tray->width;
         other.top = tray->y;
         other.bottom = tray->y + tray->height;

         left.valid = CheckLeftValid(&client, &other, &left);
         right.valid = CheckRightValid(&client, &other, &right);
         top.valid = CheckTopValid(&client, &other, &top);
         bottom.valid = CheckBottomValid(&client, &other, &bottom);

         if(CheckOverlapTopBottom(&client, &other)) {
            if(abs(client.left - other.right) <= settings.snapDistance) {
               left = other;
            }
            if(abs(client.right - other.left) <= settings.snapDistance) {
               right = other;
            }
         }
         if(CheckOverlapLeftRight(&client, &other)) {
            if(abs(client.top - other.bottom) <= settings.snapDistance) {
               top = other;
            }
            if(abs(client.bottom - other.top) <= settings.snapDistance) {
               bottom = other;
            }
         }

      }

      /* Check client windows. */
      for(tp = nodeTail[layer]; tp; tp = tp->prev) {

         if(tp == np || !ShouldSnap(tp)) {
            continue;
         }

         GetClientRectangle(tp, &other);

         /* Check if this border invalidates any previous value. */
         left.valid = CheckLeftValid(&client, &other, &left);
         right.valid = CheckRightValid(&client, &other, &right);
         top.valid = CheckTopValid(&client, &other, &top);
         bottom.valid = CheckBottomValid(&client, &other, &bottom);

         /* Compute the new snap values. */
         if(CheckOverlapTopBottom(&client, &other)) {
            if(abs(client.left - other.right) <= settings.snapDistance) {
               left = other;
            }
            if(abs(client.right - other.left) <= settings.snapDistance) {
               right = other;
            }
         }
         if(CheckOverlapLeftRight(&client, &other)) {
            if(abs(client.top - other.bottom) <= settings.snapDistance) {
               top = other;
            }
            if(abs(client.bottom - other.top) <= settings.snapDistance) {
               bottom = other;
            }
         }

      }

   }

   if(right.valid) {
      np->x = right.left - np->width - west;
   }
   if(left.valid) {
      np->x = left.right + east;
   }
   if(bottom.valid) {
      np->y = bottom.top - south;
      if(!(np->state.status & STAT_SHADED)) {
         np->y -= np->height;
      }
   }
   if(top.valid) {
      np->y = top.bottom + north;
   }

}

/** Determine if we should snap to the specified client. */
char ShouldSnap(const ClientNode *np)
{
   if(np->state.status & STAT_HIDDEN) {
      return 0;
   } else if(np->state.status & STAT_MINIMIZED) {
      return 0;
   } else {
      return 1;
   }
}

/** Get a rectangle to represent a client window. */
void GetClientRectangle(const ClientNode *np, RectangleType *r)
{

   int north, south, east, west;

   GetBorderSize(&np->state, &north, &south, &east, &west);

   r->left = np->x - west;
   r->right = np->x + np->width + east;
   r->top = np->y - north;
   if(np->state.status & STAT_SHADED) {
      r->bottom = np->y + south;
   } else {
      r->bottom = np->y + np->height + south;
   }

   r->valid = 1;

}

/** Check for top/bottom overlap. */
char CheckOverlapTopBottom(const RectangleType *a, const RectangleType *b)
{
   if(a->top >= b->bottom) {
      return 0;
   } else if(a->bottom <= b->top) {
      return 0;
   } else {
      return 1;
   }
}

/** Check for left/right overlap. */
char CheckOverlapLeftRight(const RectangleType *a, const RectangleType *b)
{
   if(a->left >= b->right) {
      return 0;
   } else if(a->right <= b->left) {
      return 0;
   } else {
      return 1;
   }
}

/** Check if the current left snap position is valid.
 * @param client The window being moved.
 * @param other A window higher in stacking order than
 * previously check windows.
 * @param left The top/bottom of the current left snap window.
 * @return 1 if the current left snap position is still valid, otherwise 0.
 */
char CheckLeftValid(const RectangleType *client,
                    const RectangleType *other, const RectangleType *left)
{

   if(!left->valid) {
      return 0;
   }

   if(left->right > other->right) {
      return 1;
   }

   /* If left and client go higher than other then still valid. */
   if(left->top < other->top && client->top < other->top) {
      return 1;
   }

   /* If left and client go lower than other then still valid. */
   if(left->bottom > other->bottom && client->bottom > other->bottom) {
      return 1;
   }

   if(other->left >= left->right) {
      return 1;
   }

   return 0;

}

/** Check if the current right snap position is valid. */
char CheckRightValid(const RectangleType *client,
                     const RectangleType *other, const RectangleType *right)
{

   if(!right->valid) {
      return 0;
   }

   if(right->left < other->left) {
      return 1;
   }

   /* If right and client go higher than other then still valid. */
   if(right->top < other->top && client->top < other->top) {
      return 1;
   }

   /* If right and client go lower than other then still valid. */
   if(right->bottom > other->bottom && client->bottom > other->bottom) {
      return 1;
   }

   if(other->right <= right->left) {
      return 1;
   }

   return 0;

}

/** Check if the current top snap position is valid. */
char CheckTopValid(const RectangleType *client,
                   const RectangleType *other, const RectangleType *top)
{

   if(!top->valid) {
      return 0;
   }

   if(top->bottom > other->bottom) {
      return 1;
   }

   /* If top and client are to the left of other then still valid. */
   if(top->left < other->left && client->left < other->left) {
      return 1;
   }

   /* If top and client are to the right of other then still valid. */
   if(top->right > other->right && client->right > other->right) {
      return 1;
   }

   if(other->top >= top->bottom) {
      return 1;
   }

   return 0;

}

/** Check if the current bottom snap position is valid. */
char CheckBottomValid(const RectangleType *client,
                      const RectangleType *other, const RectangleType *bottom)
{

   if(!bottom->valid) {
      return 0;
   }

   if(bottom->top < other->top) {
      return 1;
   }

   /* If bottom and client are to the left of other then still valid. */
   if(bottom->left < other->left && client->left < other->left) {
      return 1;
   }

   /* If bottom and client are to the right of other then still valid. */
   if(bottom->right > other->right && client->right > other->right) {
      return 1;
   }

   if(other->bottom <= bottom->top) {
      return 1;
   }

   return 0;

}

/** Switch desktops if appropriate. */
void SignalMove(const TimeType *now, int x, int y, Window w, void *data)
{
   UpdateDesktop(now);
}

/** Switch to the specified desktop. */
void UpdateDesktop(const TimeType *now)
{
   if(settings.desktopDelay == 0) {
      return;
   }
   if(GetTimeDifference(now, &moveTime) < settings.desktopDelay) {
      return;
   }
   moveTime = *now;

   /* We temporarily mark the client as hidden to avoid hidding it
    * when changing desktops. */
   currentClient->state.status |= STAT_HIDDEN;
   if(atLeft && LeftDesktop()) {
      SetClientDesktop(currentClient, currentDesktop);
      RequireRestack();
   } else if(atRight && RightDesktop()) {
      SetClientDesktop(currentClient, currentDesktop);
      RequireRestack();
   } else if(atTop && AboveDesktop()) {
      SetClientDesktop(currentClient, currentDesktop);
      RequireRestack();
   } else if(atBottom && BelowDesktop()) {
      SetClientDesktop(currentClient, currentDesktop);
      RequireRestack();
   }
   currentClient->state.status &= ~STAT_HIDDEN;
}
