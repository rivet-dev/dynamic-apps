/**
 * @file pager.c
 * @author Joe Wingbermuehle
 * @date 2004-2007
 *
 * @brief Pager tray component.
 *
 */

#include "jwm.h"
#include "pager.h"

#include "client.h"
#include "clientlist.h"
#include "color.h"
#include "cursor.h"
#include "desktop.h"
#include "event.h"
#include "tray.h"
#include "timing.h"
#include "popup.h"
#include "font.h"
#include "settings.h"

/** Structure to represent a pager tray component. */
typedef struct PagerType {

   TrayComponentType *cp;  /**< Common tray component data. */

   int deskWidth;          /**< Width of a desktop. */
   int deskHeight;         /**< Height of a desktop. */
   int scalex;             /**< Horizontal scale factor (fixed point). */
   int scaley;             /**< Vertical scale factor (fixed point). */
   char labeled;           /**< Set to label the pager. */

   Pixmap buffer;          /**< Buffer for rendering the pager. */

   TimeType mouseTime;     /**< Timestamp of last mouse movement. */
   int mousex, mousey;     /**< Coordinates of last mouse location. */

   struct PagerType *next; /**< Next pager in the list. */

} PagerType;

static PagerType *pagers = NULL;

static char shouldStopMove;

static void Create(TrayComponentType *cp);

static void SetSize(TrayComponentType *cp, int width, int height);

static int GetPagerDesktop(PagerType *pp, int x, int y);

static void ProcessPagerButtonEvent(TrayComponentType *cp,
                                    int x, int y, int mask);

static void ProcessPagerMotionEvent(TrayComponentType *cp,
                                    int x, int y, int mask);

static void StartPagerMove(TrayComponentType *cp, int x, int y);

static void StopPagerMove(ClientNode *np,
                          int x, int y, int desktop, MaxFlags maxFlags);

static void PagerMoveController(int wasDestroyed);

static void DrawPager(const PagerType *pp);

static void DrawPagerClient(const PagerType *pp, const ClientNode *np);

static void SignalPager(const TimeType *now, int x, int y, Window w,
                        void *data);


/** Shutdown the pager. */
void ShutdownPager(void)
{
   PagerType *pp;
   for(pp = pagers; pp; pp = pp->next) {
      JXFreePixmap(display, pp->buffer);
   }
}

/** Release pager data. */
void DestroyPager(void)
{
   PagerType *pp;
   while(pagers) {
      UnregisterCallback(SignalPager, pagers);
      pp = pagers->next;
      Release(pagers);
      pagers = pp;
   }
}

/** Create a new pager tray component. */
TrayComponentType *CreatePager(char labeled)
{

   TrayComponentType *cp;
   PagerType *pp;

   pp = Allocate(sizeof(PagerType));
   pp->next = pagers;
   pagers = pp;
   pp->labeled = labeled;
   pp->mousex = -settings.doubleClickDelta;
   pp->mousey = -settings.doubleClickDelta;
   pp->mouseTime.seconds = 0;
   pp->mouseTime.ms = 0;
   pp->buffer = None;

   cp = CreateTrayComponent();
   cp->object = pp;
   pp->cp = cp;
   cp->Create = Create;
   cp->SetSize = SetSize;
   cp->ProcessButtonPress = ProcessPagerButtonEvent;
   cp->ProcessMotionEvent = ProcessPagerMotionEvent;

   RegisterCallback(settings.popupDelay / 2, SignalPager, pp);

   return cp;
}

/** Initialize a pager tray component. */
void Create(TrayComponentType *cp)
{

   PagerType *pp;

   Assert(cp);

   pp = (PagerType*)cp->object;

   Assert(pp);

   Assert(cp->width > 0);
   Assert(cp->height > 0);

   cp->pixmap = JXCreatePixmap(display, rootWindow, cp->width,
                               cp->height, rootDepth);
   pp->buffer = cp->pixmap;

}

/** Set the size of a pager tray component. */
void SetSize(TrayComponentType *cp, int width, int height)
{

   PagerType *pp = (PagerType*)cp->object;

   if(width) {

      /* Vertical pager. */
      cp->width = width;

      pp->deskWidth = width / settings.desktopWidth;
      pp->deskHeight = (pp->deskWidth * rootHeight) / rootWidth;

      cp->height = pp->deskHeight * settings.desktopHeight
                 + settings.desktopHeight - 1;

   } else if(height) {

      /* Horizontal pager. */
      cp->height = height;

      pp->deskHeight = height / settings.desktopHeight;
      pp->deskWidth = (pp->deskHeight * rootWidth) / rootHeight;

      cp->width = pp->deskWidth * settings.desktopWidth
                + settings.desktopWidth - 1;

   } else {
      Assert(0);
   }

   if(pp->buffer != None) {
      JXFreePixmap(display, pp->buffer);
      pp->buffer = JXCreatePixmap(display, rootWindow, cp->width,
                                  cp->height, rootDepth);
      cp->pixmap = pp->buffer;
      DrawPager(pp);
   }

   pp->scalex = ((pp->deskWidth - 2) << 16) / rootWidth;
   pp->scaley = ((pp->deskHeight - 2) << 16) / rootHeight;

}

/** Get the desktop for a pager given a set of coordinates. */
int GetPagerDesktop(PagerType *pp, int x, int y)
{

   int pagerx, pagery;

   pagerx = x / (pp->deskWidth + 1);
   pagery = y / (pp->deskHeight + 1);

   return pagery * settings.desktopWidth + pagerx;

}

/** Process a button event on a pager tray component. */
void ProcessPagerButtonEvent(TrayComponentType *cp, int x, int y, int mask)
{

   PagerType *pp;

   switch(mask) {
   case Button1:
   case Button2:

      /* Change to the selected desktop. */
      pp = (PagerType*)cp->object;
      ChangeDesktop(GetPagerDesktop(pp, x, y));
      break;

   case Button3:

      /* Move a client and possibly change its desktop. */
      StartPagerMove(cp, x, y);
      break;

   case Button4:

      /* Change to the previous desktop. */
      LeftDesktop();
      break;

   case Button5:

      /* Change to the next desktop. */
      RightDesktop();
      break;

   default:
      break;
   }
}

/** Process a motion event on a pager tray component. */
void ProcessPagerMotionEvent(TrayComponentType *cp, int x, int y, int mask)
{

   PagerType *pp = (PagerType*)cp->object;

   pp->mousex = cp->screenx + x;
   pp->mousey = cp->screeny + y;
   GetCurrentTime(&pp->mouseTime);
}

/** Start a pager move operation. */
void StartPagerMove(TrayComponentType *cp, int x, int y)
{

   XEvent event;
   PagerType *pp;
   ClientNode *np;
   int layer;
   int desktop;
   int cx, cy;
   int cwidth, cheight;

   int north, south, east, west;
   int oldx, oldy;
   int oldDesk;
   int startx, starty;
   MaxFlags maxFlags;

   pp = (PagerType*)cp->object;

   /* Determine the selected desktop. */
   desktop = GetPagerDesktop(pp, x, y);
   x -= (desktop % settings.desktopWidth) * (pp->deskWidth + 1);
   y -= (desktop / settings.desktopWidth) * (pp->deskHeight + 1);

   /* Find the client under the specified coordinates. */
   for(layer = LAST_LAYER; layer >= FIRST_LAYER; layer--) {
      for(np = nodes[layer]; np; np = np->next) {

         /* Skip this client if it isn't mapped. */
         if(!(np->state.status & STAT_MAPPED)) {
            continue;
         }
         if(np->state.status & STAT_NOPAGER) {
            continue;
         }

         /* Skip this client if it isn't on the selected desktop. */
         if(np->state.status & STAT_STICKY) {
            if(currentDesktop != desktop) {
               continue;
            }
         } else {
            if(np->state.desktop != desktop) {
               continue;
            }
         }

         /* Get the offset and size of the client on the pager. */
         cx = 1 + ((np->x * pp->scalex) >> 16);
         cy = 1 + ((np->y * pp->scaley) >> 16);
         cwidth = (np->width * pp->scalex) >> 16;
         cheight = (np->height * pp->scaley) >> 16;

         /* Normalize the offset and size. */
         if(cx + cwidth > pp->deskWidth) {
            cwidth = pp->deskWidth - cx;
         }
         if(cy + cheight > pp->deskHeight) {
            cheight = pp->deskHeight - cy;
         }
         if(cx < 0) {
            cwidth += cx;
            cx = 0;
         }
         if(cy < 0) {
            cheight += cy;
            cy = 0;
         }

         /* Skip the client if we are no longer in bounds. */
         if(cwidth <= 0 || cheight <= 0) {
            continue;
         }

         /* Check the y-coordinate. */
         if(y < cy || y > cy + cheight) {
            continue;
         }

         /* Check the x-coordinate. */
         if(x < cx || x > cx + cwidth) {
            continue;
         }

         /* Found it. Exit. */
         goto ClientFound;

      }
   }

   /* Client wasn't found. Just return. */
   return;

ClientFound:

   Assert(np);

   /* The selected client was found. Now make sure we can move it. */
   if(!(np->state.border & BORDER_MOVE)) {
      return;
   }

   /* Start the move. */
   if(!GrabMouseForMove()) {
      return;
   }

   /* If the client is maximized, unmaximize it. */
   maxFlags = np->state.maxFlags;
   if(np->state.maxFlags) {
      MaximizeClient(np, MAX_NONE);
   }

   GetBorderSize(&np->state, &north, &south, &east, &west);

   np->controller = PagerMoveController;
   shouldStopMove = 0;

   oldx = np->x;
   oldy = np->y;
   oldDesk = np->state.desktop;

   startx = x;
   starty = y;

   if(!(GetMouseMask() & Button3Mask)) {
      StopPagerMove(np, oldx, oldy, oldDesk, maxFlags);
   }

   for(;;) {

      WaitForEvent(&event);

      if(shouldStopMove) {
         np->controller = NULL;
         return;
      }

      switch(event.type) {
      case ButtonRelease:

         /* Done when the 3rd mouse button is released. */
         if(event.xbutton.button == Button3) {
            StopPagerMove(np, oldx, oldy, oldDesk, maxFlags);
            return;
         }
         break;

      case MotionNotify:

         SetMousePosition(event.xmotion.x_root, event.xmotion.y_root,
                          event.xmotion.window);

         /* Get the mouse position on the pager. */
         x = event.xmotion.x_root - cp->screenx;
         y = event.xmotion.y_root - cp->screeny;

         /* Don't move if we are off of the pager. */
         if(x < 0 || x > cp->width) {
            break;
         }
         if(y < 0 || y > cp->height) {
            break;
         }

         /* Determine the new client desktop. */
         desktop = GetPagerDesktop(pp, x, y);
         x -= pp->deskWidth * (desktop % settings.desktopWidth);
         y -= pp->deskHeight * (desktop / settings.desktopWidth);

         /* If this client isn't sticky and now on a different desktop
          * change the client's desktop. */
         if(!(np->state.status & STAT_STICKY)) {
            if(desktop != oldDesk) {
               SetClientDesktop(np, (unsigned int)desktop);
               oldDesk = desktop;
            }
         }

         /* Get new client coordinates. */
         oldx = startx + (x - startx);
         oldx = (oldx << 16) / pp->scalex;
         oldx -= (np->width + east + west) / 2;
         oldy = starty + (y - starty);
         oldy = (oldy << 16) / pp->scaley;
         oldy -= (np->height + north + south) / 2;

         /* Move the window. */
         np->x = oldx;
         np->y = oldy;
         JXMoveWindow(display, np->parent, np->x - west, np->y - north);
         SendConfigureEvent(np);
         RequirePagerUpdate();

         break;

      default:
         break;
      }

   }

}

/** Stop an active pager move. */
void StopPagerMove(ClientNode *np,
                   int x, int y, int desktop, MaxFlags maxFlags)
{

   int north, south, east, west;

   Assert(np);
   Assert(np->controller);

   /* Release grabs. */
   (np->controller)(0);

   np->x = x;
   np->y = y;

   GetBorderSize(&np->state, &north, &south, &east, & west);
   JXMoveWindow(display, np->parent, np->x - west, np->y - north);
   SendConfigureEvent(np);

   /* Restore the maximized state of the client. */
   if(maxFlags != MAX_NONE) {
      MaximizeClient(np, maxFlags);
   }

   /* Redraw the pager. */
   RequirePagerUpdate();

}

/** Client-terminated pager move. */
void PagerMoveController(int wasDestroyed)
{

   JXUngrabPointer(display, CurrentTime);
   JXUngrabKeyboard(display, CurrentTime);
   shouldStopMove = 1;

}

/** Draw a pager. */
void DrawPager(const PagerType *pp)
{
   ClientNode *np;
   Pixmap buffer;
   int width, height;
   int deskWidth, deskHeight;
   unsigned int x;
   const char *name;
   int xc, yc;
   int textWidth, textHeight;
   int dx, dy;

   buffer = pp->cp->pixmap;
   width = pp->cp->width;
   height = pp->cp->height;
   deskWidth = pp->deskWidth;
   deskHeight = pp->deskHeight;

   /* Draw the background. */
   JXSetForeground(display, rootGC, colors[COLOR_PAGER_BG]);
   JXFillRectangle(display, buffer, rootGC, 0, 0, width, height);

   /* Highlight the current desktop. */
   JXSetForeground(display, rootGC, colors[COLOR_PAGER_ACTIVE_BG]);
   dx = currentDesktop % settings.desktopWidth;
   dy = currentDesktop / settings.desktopWidth;
   JXFillRectangle(display, buffer, rootGC,
                   dx * (deskWidth + 1), dy * (deskHeight + 1),
                   deskWidth, deskHeight);

   /* Draw the labels. */
   if(pp->labeled) {
      textHeight = GetStringHeight(FONT_PAGER);
      if(textHeight < deskHeight) {
         for(x = 0; x < settings.desktopCount; x++) {
            dx = x % settings.desktopWidth;
            dy = x / settings.desktopWidth;
            name = GetDesktopName(x);
            textWidth = GetStringWidth(FONT_PAGER, name);
            if(textWidth < deskWidth) {
               xc = dx * (deskWidth + 1) + (deskWidth - textWidth) / 2;
               yc = dy * (deskHeight + 1) + (deskHeight - textHeight) / 2;
               RenderString(buffer, FONT_PAGER,
                            COLOR_PAGER_TEXT, xc, yc, deskWidth, name);
            }
         }
      }
   }

   /* Draw the clients. */
   for(x = FIRST_LAYER; x <= LAST_LAYER; x++) {
      for(np = nodeTail[x]; np; np = np->prev) {
         DrawPagerClient(pp, np);
      }
   }

   /* Draw the desktop dividers. */
   JXSetForeground(display, rootGC, colors[COLOR_PAGER_OUTLINE]);
   for(x = 1; x < settings.desktopHeight; x++) {
      JXDrawLine(display, buffer, rootGC,
                 0, (deskHeight + 1) * x - 1,
                 width, (deskHeight + 1) * x - 1);
   }
   for(x = 1; x < settings.desktopWidth; x++) {
      JXDrawLine(display, buffer, rootGC,
                 (deskWidth + 1) * x - 1, 0,
                 (deskWidth + 1) * x - 1, height);
   }

}

/** Update the pager. */
void UpdatePager(void)
{

   PagerType *pp;

   if(JUNLIKELY(shouldExit)) {
      return;
   }

   for(pp = pagers; pp; pp = pp->next) {

      /* Draw the pager. */
      DrawPager(pp);

      /* Tell the tray to redraw. */
      UpdateSpecificTray(pp->cp->tray, pp->cp);

   }

}

/** Signal pagers (for popups). */
void SignalPager(const TimeType *now, int x, int y, Window w, void *data)
{
   PagerType *pp = (PagerType*)data;
   if(pp->cp->tray->window == w &&
      abs(pp->mousex - x) < settings.doubleClickDelta &&
      abs(pp->mousey - y) < settings.doubleClickDelta) {
      if(GetTimeDifference(now, &pp->mouseTime) >= settings.popupDelay) {
         const int desktop = GetPagerDesktop(pp, x - pp->cp->screenx,
                                                 y - pp->cp->screeny);
         if(desktop >= 0 && desktop < settings.desktopCount) {
            const char *desktopName = GetDesktopName(desktop);
            if(desktopName) {
               ShowPopup(x, y, desktopName, POPUP_PAGER);
            }
         }

      }
   }
}

/** Draw a client on the pager. */
void DrawPagerClient(const PagerType *pp, const ClientNode *np)
{

   int x, y;
   int width, height;
   int offx, offy;

   /* Don't draw the client if it isn't mapped. */
   if(!(np->state.status & STAT_MAPPED)) {
      return;
   }
   /* The user will probably expect to see windows providing background
    * images and/or desktop file icons as "unoccupied space", as if there
    * were no non-root window there really. */
   if((np->state.windowType == WINDOW_TYPE_DESKTOP) ||
      (np->state.layer == LAYER_DESKTOP)) {
      return;
   }
   /* Skip anything we're specifically told to skip too. */
   if(np->state.status & STAT_NOPAGER) {
      return;
   }

   /* Determine the desktop for the client. */
   if(np->state.status & STAT_STICKY) {
      offx = currentDesktop % settings.desktopWidth;
      offy = currentDesktop / settings.desktopWidth;
   } else {
      offx = np->state.desktop % settings.desktopWidth;
      offy = np->state.desktop / settings.desktopWidth;
   }
   offx *= pp->deskWidth + 1;
   offy *= pp->deskHeight + 1;

   /* Determine the location and size of the client on the pager. */
   x = 1 + ((np->x * pp->scalex) >> 16);
   y = 1 + ((np->y * pp->scaley) >> 16);
   width = (np->width * pp->scalex) >> 16;
   height = (np->height * pp->scaley) >> 16;

   /* Normalize the size and offset. */
   if(x + width > pp->deskWidth) {
      width = pp->deskWidth - x;
   }
   if(y + height > pp->deskHeight) {
      height = pp->deskHeight - y;
   }
   if(x < 0) {
      width += x;
      x = 0;
   }
   if(y < 0) {
      height += y;
      y = 0;
   }

   /* Return if there's nothing to do. */
   if(width <= 0 || height <= 0) {
      return;
   }

   /* Move to the correct desktop on the pager. */
   x += offx;
   y += offy;

   /* Draw the client outline. */
   JXSetForeground(display, rootGC, colors[COLOR_PAGER_OUTLINE]);
   JXDrawRectangle(display, pp->cp->pixmap, rootGC, x, y, width, height);

   /* Fill the client if there's room. */
   if(width > 1 && height > 1) {
      ColorType fillColor;
      if((np->state.status & STAT_ACTIVE)
         && (np->state.desktop == currentDesktop
         || (np->state.status & STAT_STICKY))) {
         fillColor = COLOR_PAGER_ACTIVE_FG;
      } else if(np->state.status & STAT_FLASH) {
         fillColor = COLOR_PAGER_ACTIVE_FG;
      } else {
         fillColor = COLOR_PAGER_FG;
      }
      JXSetForeground(display, rootGC, colors[fillColor]);
      JXFillRectangle(display, pp->cp->pixmap, rootGC, x + 1, y + 1,
                      width - 1, height - 1);
   }

}

