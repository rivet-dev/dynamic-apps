/**
 * @file outline.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Outlines for moving and resizing client windows.
 *
 */

#include "jwm.h"
#include "outline.h"
#include "main.h"
#include "grab.h"

static GC outlineGC = None;
static int lastX, lastY;
static int lastWidth, lastHeight;

/** Draw an outline. */
void DrawOutline(int x, int y, int width, int height)
{
   XGCValues gcValues;
   gcValues.function = GXinvert;
   gcValues.subwindow_mode = IncludeInferiors;
   gcValues.line_width = 2;
   outlineGC = JXCreateGC(display, rootWindow,
                          GCFunction | GCSubwindowMode | GCLineWidth,
                          &gcValues);
   GrabServer();
   JXDrawRectangle(display, rootWindow, outlineGC, x, y, width, height);
   lastX = x;
   lastY = y;
   lastWidth = width;
   lastHeight = height;
}

/** Clear the last outline. */
void ClearOutline(void)
{
   if(outlineGC != None) {
      JXDrawRectangle(display, rootWindow, outlineGC,
                      lastX, lastY, lastWidth, lastHeight);
      UngrabServer();
      JXFreeGC(display, outlineGC);
      outlineGC = None;
   }
}

