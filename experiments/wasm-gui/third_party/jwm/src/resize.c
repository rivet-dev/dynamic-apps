/**
 * @file resize.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Functions to handle resizing client windows.
 *
 */

#include "jwm.h"
#include "resize.h"
#include "client.h"
#include "outline.h"
#include "cursor.h"
#include "misc.h"
#include "pager.h"
#include "status.h"
#include "binding.h"
#include "event.h"
#include "settings.h"

static char shouldStopResize;

static void StopResize(ClientNode *np);
static void ResizeController(int wasDestroyed);
static void UpdateSize(ClientNode *np, const MouseContextType context,
                       const int x, const int y,
                       const int startx, const int starty,
                       const int oldx, const int oldy,
                       const int oldw, const int oldh);
static void FixWidth(ClientNode *np);
static void FixHeight(ClientNode *np);

/** Callback to stop a resize. */
void ResizeController(int wasDestroyed)
{
   if(settings.resizeMode == RESIZE_OUTLINE) {
      ClearOutline();
   }
   JXUngrabPointer(display, CurrentTime);
   JXUngrabKeyboard(display, CurrentTime);
   DestroyResizeWindow();
   shouldStopResize = 1;
}

/** Update the size of a client window. */
void UpdateSize(ClientNode *np, const MouseContextType context,
                const int x, const int y,
                const int startx, const int starty,
                const int oldx, const int oldy,
                const int oldw, const int oldh)
{
   if(context & MC_BORDER_N) {
      int delta = (y - starty) / np->yinc;
      delta *= np->yinc;
      if(oldh - delta >= np->minHeight
         && (oldh - delta <= np->maxHeight || delta > 0)) {
         np->height = oldh - delta;
         np->y = oldy + delta;
      }
      if(!(context & (MC_BORDER_E | MC_BORDER_W))) {
         FixWidth(np);
      }
   }
   if(context & MC_BORDER_S) {
      int delta = (y - starty) / np->yinc;
      delta *= np->yinc;
      np->height = oldh + delta;
      np->height = Max(np->height, np->minHeight);
      np->height = Min(np->height, np->maxHeight);
      if(!(context & (MC_BORDER_E | MC_BORDER_W))) {
         FixWidth(np);
      }
   }
   if(context & MC_BORDER_E) {
      int delta = (x - startx) / np->xinc;
      delta *= np->xinc;
      np->width = oldw + delta;
      np->width = Max(np->width, np->minWidth);
      np->width = Min(np->width, np->maxWidth);
      if(!(context & (MC_BORDER_N | MC_BORDER_S))) {
         FixHeight(np);
      }
   }
   if(context & MC_BORDER_W) {
      int delta = (x - startx) / np->xinc;
      delta *= np->xinc;
      if(oldw - delta >= np->minWidth
         && (oldw - delta <= np->maxWidth || delta > 0)) {
         np->width = oldw - delta;
         np->x = oldx + delta;
      }
      if(!(context & (MC_BORDER_N | MC_BORDER_S))) {
         FixHeight(np);
      }
   }

   if(np->sizeFlags & PAspect) {
      if((context & (MC_BORDER_N | MC_BORDER_S)) &&
         (context & (MC_BORDER_E | MC_BORDER_W))) {

         if(np->width * np->aspect.miny < np->height * np->aspect.minx) {
            const int delta = np->width;
            np->width = (np->height * np->aspect.minx) / np->aspect.miny;
            if(context & MC_BORDER_W) {
               np->x -= np->width - delta;
            }
         }
         if(np->width * np->aspect.maxy > np->height * np->aspect.maxx) {
            const int delta = np->height;
            np->height = (np->width * np->aspect.maxy) / np->aspect.maxx;
            if(context & MC_BORDER_N) {
               np->y -= np->height - delta;
            }
         }
      }
   }
}

/** Resize a client window (mouse initiated). */
void ResizeClient(ClientNode *np, MouseContextType context,
                  int startx, int starty)
{

   XEvent event;
   int oldx, oldy;
   int oldw, oldh;
   int gwidth, gheight;
   int lastgwidth, lastgheight;
   int north, south, east, west;

   Assert(np);

   if(!(np->state.border & BORDER_RESIZE)) {
      return;
   }
   if(np->state.status & STAT_FULLSCREEN) {
      return;
   }
   if(np->state.maxFlags) {
      np->state.maxFlags = MAX_NONE;
      WriteState(np);
      ResetBorder(np);
   }
   if(JUNLIKELY(!GrabMouseForResize(context))) {
      return;
   }

   np->controller = ResizeController;
   shouldStopResize = 0;

   oldx = np->x;
   oldy = np->y;
   oldw = np->width;
   oldh = np->height;

   gwidth = (np->width - np->baseWidth) / np->xinc;
   gheight = (np->height - np->baseHeight) / np->yinc;

   GetBorderSize(&np->state, &north, &south, &east, &west);

   startx += np->x - west;
   starty += np->y - north;

   CreateResizeWindow(np);
   UpdateResizeWindow(np, gwidth, gheight);

   if(!(GetMouseMask() & (Button1Mask | Button3Mask))) {
      StopResize(np);
      return;
   }

   for(;;) {

      WaitForEvent(&event);

      if(shouldStopResize) {
         np->controller = NULL;
         return;
      }

      switch(event.type) {
      case ButtonRelease:
         if(   event.xbutton.button == Button1
            || event.xbutton.button == Button3) {
            StopResize(np);
            return;
         }
         break;
      case MotionNotify:

         SetMousePosition(event.xmotion.x_root, event.xmotion.y_root,
                          event.xmotion.window);
         DiscardMotionEvents(&event, np->window);

         UpdateSize(np, context, event.xmotion.x, event.xmotion.y,
                    startx, starty, oldx, oldy, oldw, oldh);

         lastgwidth = gwidth;
         lastgheight = gheight;

         gwidth = (np->width - np->baseWidth) / np->xinc;
         gheight = (np->height - np->baseHeight) / np->yinc;

         if(lastgheight != gheight || lastgwidth != gwidth) {

            UpdateResizeWindow(np, gwidth, gheight);

            if(settings.resizeMode == RESIZE_OUTLINE) {
               ClearOutline();
               if(np->state.status & STAT_SHADED) {
                  DrawOutline(np->x - west, np->y - north,
                     np->width + west + east, north + south);
               } else {
                  DrawOutline(np->x - west, np->y - north,
                     np->width + west + east,
                     np->height + north + south);
               }
            } else {
               ResetBorder(np);
               SendConfigureEvent(np);
            }

            RequirePagerUpdate();

         }

         break;
      default:
         break;
      }
   }

}

/** Resize a client window (keyboard or menu initiated). */
void ResizeClientKeyboard(ClientNode *np, MouseContextType context)
{

   XEvent event;
   int gwidth, gheight;
   int lastgwidth, lastgheight;
   int north, south, east, west;
   int startx, starty;
   int oldx, oldy, oldw, oldh;
   int ratio, minr, maxr;

   Assert(np);

   if(!(np->state.border & BORDER_RESIZE)) {
      return;
   }
   if(np->state.status & STAT_FULLSCREEN) {
      return;
   }
   if(np->state.maxFlags) {
      np->state.maxFlags = MAX_NONE;
      WriteState(np);
      ResetBorder(np);
   }

   if(JUNLIKELY(JXGrabKeyboard(display, np->parent, True, GrabModeAsync,
                               GrabModeAsync, CurrentTime) != GrabSuccess)) {
      return;
   }
   if(!GrabMouseForResize(context)) {
      JXUngrabKeyboard(display, CurrentTime);
      return;
   }

   np->controller = ResizeController;
   shouldStopResize = 0;

   oldx = np->x;
   oldy = np->y;
   oldw = np->width;
   oldh = np->height;

   gwidth = (np->width - np->baseWidth) / np->xinc;
   gheight = (np->height - np->baseHeight) / np->yinc;

   GetBorderSize(&np->state, &north, &south, &east, &west);

   CreateResizeWindow(np);
   UpdateResizeWindow(np, gwidth, gheight);

   if(context & MC_BORDER_N) {
      starty = np->y - north;
   } else if(context & MC_BORDER_S) {
      if(np->state.status & STAT_SHADED) {
         starty = np->y;
      } else {
         starty = np->y + np->height;
      }
   } else {
      starty = np->y + np->height / 2;
   }
   if(context & MC_BORDER_W) {
      startx = np->x - west;
   } else if(context & MC_BORDER_E) {
      startx = np->x + np->width;
   } else {
      startx = np->x + np->width / 2;
   }
   MoveMouse(rootWindow, startx, starty);
   DiscardMotionEvents(&event, np->window);

   for(;;) {

      WaitForEvent(&event);

      if(shouldStopResize) {
         np->controller = NULL;
         return;
      }

      if(event.type == KeyPress) {
         int deltax = 0;
         int deltay = 0;
         ActionType action;

         DiscardKeyEvents(&event, np->window);
         action = GetKey(MC_NONE, event.xkey.state, event.xkey.keycode);
         switch(action.action) {
         case ACTION_UP:
            deltay = Min(-np->yinc, -10);
            break;
         case ACTION_DOWN:
            deltay = Max(np->yinc, 10);
            break;
         case ACTION_RIGHT:
            deltax = Max(np->xinc, 10);
            break;
         case ACTION_LEFT:
            deltax = Min(-np->xinc, -10);
            break;
         default:
            StopResize(np);
            return;
         }

         deltay -= deltay % np->yinc;
         np->height += deltay;
         np->height = Max(np->height, np->minHeight);
         np->height = Min(np->height, np->maxHeight);
         deltax -= deltax % np->xinc;
         np->width += deltax;
         np->width = Max(np->width, np->minWidth);
         np->width = Min(np->width, np->maxWidth);

         if(np->sizeFlags & PAspect) {

            ratio = (np->width << 16) / np->height;

            minr = (np->aspect.minx << 16) / np->aspect.miny;
            if(ratio < minr) {
               np->width = (np->height * minr) >> 16;
            }

            maxr = (np->aspect.maxx << 16) / np->aspect.maxy;
            if(ratio > maxr) {
               np->height = (np->width << 16) / maxr;
            }

         }

      } else if(event.type == MotionNotify) {

         SetMousePosition(event.xmotion.x_root, event.xmotion.y_root,
                          event.xmotion.window);
         DiscardMotionEvents(&event, np->window);

         UpdateSize(np, context, event.xmotion.x, event.xmotion.y,
                    startx, starty, oldx, oldy, oldw, oldh);

      } else if(event.type == ButtonRelease) {

         StopResize(np);
         return;

      }

      lastgwidth = gwidth;
      lastgheight = gheight;
      gwidth = (np->width - np->baseWidth) / np->xinc;
      gheight = (np->height - np->baseHeight) / np->yinc;

      if(lastgwidth != gwidth || lastgheight != gheight) {

         UpdateResizeWindow(np, gwidth, gheight);

         if(settings.resizeMode == RESIZE_OUTLINE) {
            ClearOutline();
            if(np->state.status & STAT_SHADED) {
               DrawOutline(np->x - west, np->y - north,
                  np->width + west + east,
                  north + south);
            } else {
               DrawOutline(np->x - west, np->y - north,
                  np->width + west + east,
                  np->height + north + south);
            }
         } else {
            ResetBorder(np);
            SendConfigureEvent(np);
         }

         RequirePagerUpdate();

      }

   }

}

/** Stop a resize action. */
void StopResize(ClientNode *np)
{

   np->controller = NULL;

   /* Set the old width/height if maximized so the window
    * is restored to the new size. */
   if(np->state.maxFlags & MAX_VERT) {
      np->oldWidth = np->width;
      np->oldx = np->x;
   }
   if(np->state.maxFlags & MAX_HORIZ) {
      np->oldHeight = np->height;
      np->oldy = np->y;
   }

   if(settings.resizeMode == RESIZE_OUTLINE) {
      ClearOutline();
   }

   JXUngrabPointer(display, CurrentTime);
   JXUngrabKeyboard(display, CurrentTime);

   DestroyResizeWindow();

   ResetBorder(np);
   SendConfigureEvent(np);

}

/** Fix the width to match the aspect ratio. */
void FixWidth(ClientNode *np)
{

   Assert(np);

   if((np->sizeFlags & PAspect) && np->height > 0) {
      if(np->width * np->aspect.miny < np->height * np->aspect.minx) {
         np->width = (np->height * np->aspect.minx) / np->aspect.miny;
      }
      if(np->width * np->aspect.maxy > np->height * np->aspect.maxx) {
         np->width = (np->height * np->aspect.maxx) / np->aspect.maxy;
      }
   }

}

/** Fix the height to match the aspect ratio. */
void FixHeight(ClientNode *np)
{

   Assert(np);

   if((np->sizeFlags & PAspect) && np->height > 0) {
      if(np->width * np->aspect.miny < np->height * np->aspect.minx) {
         np->height = (np->width * np->aspect.miny) / np->aspect.minx;
      }
      if(np->width * np->aspect.maxy > np->height * np->aspect.maxx) {
         np->height = (np->width * np->aspect.maxy) / np->aspect.maxx;
      }
   }

}

