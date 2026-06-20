/**
 * @file status.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Functions for display window move/resize status.
 *
 */

#include "jwm.h"
#include "status.h"
#include "font.h"
#include "screen.h"
#include "main.h"
#include "client.h"
#include "settings.h"
#include "hint.h"

static Window statusWindow;
static Pixmap statusPixmap;
static unsigned int statusWindowHeight;
static unsigned int statusWindowWidth;
static int statusWindowX, statusWindowY;

static void CreateMoveResizeWindow(const ClientNode *np,
                                   StatusWindowType type);
static void DrawMoveResizeWindow(const ClientNode *np,
                                 StatusWindowType type,
                                 const char *str);
static void DestroyMoveResizeWindow(void);
static void GetMoveResizeCoordinates(const ClientNode *np,
                                     StatusWindowType type, int *x, int *y);

/** Get the location to place the status window. */
void GetMoveResizeCoordinates(const ClientNode *np, StatusWindowType type,
                              int *x, int *y)
{

   const ScreenType *sp;

   if(type == SW_WINDOW) {
      *x = np->x + (np->width - statusWindowWidth) / 2;
      *y = np->y + (np->height - statusWindowHeight) / 2;
      return;
   }

   sp = GetCurrentScreen(np->x, np->y);

   if(type == SW_CORNER) {
      *x = sp->x;
      *y = sp->y;
      return;
   }

   /* SW_SCREEN */
   *x = sp->x + (sp->width - statusWindowWidth) / 2;
   *y = sp->y + (sp->height - statusWindowHeight) / 2;

}

/** Create the status window. */
void CreateMoveResizeWindow(const ClientNode *np, StatusWindowType type)
{

   XSetWindowAttributes attrs;
   long attrMask;

   if(type == SW_OFF) {
      return;
   }

   statusWindowHeight = GetStringHeight(FONT_MENU) + 10;
   statusWindowWidth = GetStringWidth(FONT_MENU, " 00000 x 00000 ") + 2;

   GetMoveResizeCoordinates(np, type, &statusWindowX, &statusWindowY);

   attrMask = 0;

   attrMask |= CWBackPixel;
   attrs.background_pixel = colors[COLOR_MENU_BG];

   attrMask |= CWSaveUnder;
   attrs.save_under = True;

   attrMask |= CWOverrideRedirect;
   attrs.override_redirect = True;

   statusWindow = JXCreateWindow(display, rootWindow,
      statusWindowX, statusWindowY,
      statusWindowWidth, statusWindowHeight, 0,
      rootDepth, InputOutput, rootVisual,
      attrMask, &attrs);
   SetAtomAtom(statusWindow, ATOM_NET_WM_WINDOW_TYPE,
               ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION);
   statusPixmap = JXCreatePixmap(display, statusWindow,
      statusWindowWidth, statusWindowHeight, rootDepth);

   JXMapRaised(display, statusWindow);

}

/** Draw the status window. */
void DrawMoveResizeWindow(const ClientNode *np,
                          StatusWindowType type,
                          const char *str)
{

   int x, y;
   int width;

   GetMoveResizeCoordinates(np, type, &x, &y);
   if(x != statusWindowX || y != statusWindowX) {
      statusWindowX = x;
      statusWindowY = y;
      JXMoveResizeWindow(display, statusWindow, x, y,
                         statusWindowWidth, statusWindowHeight);
   }

   /* Clear the background. */
   JXSetForeground(display, rootGC, colors[COLOR_MENU_BG]);
   JXFillRectangle(display, statusPixmap, rootGC, 0, 0,
                   statusWindowWidth, statusWindowHeight);

   /* Draw the border. */
   if(settings.menuDecorations == DECO_MOTIF) {
      JXSetForeground(display, rootGC, colors[COLOR_MENU_UP]);
      JXDrawLine(display, statusPixmap, rootGC,
                 0, 0, statusWindowWidth, 0);
      JXDrawLine(display, statusPixmap, rootGC,
                 0, 0, 0, statusWindowHeight);
      JXSetForeground(display, rootGC, colors[COLOR_MENU_DOWN]);
      JXDrawLine(display, statusPixmap, rootGC, 0, statusWindowHeight - 1,
                 statusWindowWidth, statusWindowHeight - 1);
      JXDrawLine(display, statusPixmap, rootGC, statusWindowWidth - 1, 0,
                 statusWindowWidth - 1, statusWindowHeight);
   } else {
      JXSetForeground(display, rootGC, colors[COLOR_MENU_DOWN]);
      JXDrawRectangle(display, statusPixmap, rootGC, 0, 0,
                      statusWindowWidth - 1, statusWindowHeight - 1);
   }

   width = GetStringWidth(FONT_MENU, str);
   RenderString(statusPixmap, FONT_MENU, COLOR_MENU_FG,
                (statusWindowWidth - width) / 2, 5, width, str);

   JXCopyArea(display, statusPixmap, statusWindow, rootGC,
      0, 0, statusWindowWidth, statusWindowHeight, 0, 0);
}

/** Destroy the status window. */
void DestroyMoveResizeWindow(void)
{
   if(statusWindow != None) {
      JXDestroyWindow(display, statusWindow);
      statusWindow = None;
   }
   if(statusPixmap != None) {
      JXFreePixmap(display, statusPixmap);
      statusPixmap = None;
   }
}

/** Create a move status window. */
void CreateMoveWindow(ClientNode *np)
{
   CreateMoveResizeWindow(np, settings.moveStatusType);
}

/** Update the move status window. */
void UpdateMoveWindow(ClientNode *np)
{
   char str[80];

   if(settings.moveStatusType == SW_OFF) {
      return;
   }

   snprintf(str, sizeof(str), "(%d, %d)", np->x, np->y);
   DrawMoveResizeWindow(np, settings.moveStatusType, str);
}

/** Destroy the move status window. */
void DestroyMoveWindow(void)
{
   DestroyMoveResizeWindow();
}

/** Create a resize status window. */
void CreateResizeWindow(ClientNode *np)
{
   CreateMoveResizeWindow(np, settings.resizeStatusType);
}

/** Update the resize status window. */
void UpdateResizeWindow(ClientNode *np, int gwidth, int gheight)
{
   char str[80];

   if(settings.resizeStatusType == SW_OFF) {
      return;
   }

   snprintf(str, sizeof(str), "%d x %d", gwidth, gheight);
   DrawMoveResizeWindow(np, settings.resizeStatusType, str);
}

/** Destroy the resize status window. */
void DestroyResizeWindow(void)
{
   DestroyMoveResizeWindow();
}
