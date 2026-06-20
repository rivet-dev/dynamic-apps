/**
 * @file cursor.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Cursor functions.
 *
 */

#include "jwm.h"
#include "cursor.h"
#include "main.h"

/** Cursor types. */
typedef enum {
   CURSOR_DEFAULT,
   CURSOR_MOVE,
   CURSOR_NORTH,
   CURSOR_SOUTH,
   CURSOR_EAST,
   CURSOR_WEST,
   CURSOR_NE,
   CURSOR_NW,
   CURSOR_SE,
   CURSOR_SW,
   CURSOR_CHOOSE,
   CURSOR_COUNT
} CursorType;

/** Cursors to load for the various cursor types.
 * This must be ordered the same as CursorType.
 */
static const unsigned int cursor_shapes[CURSOR_COUNT] = {
   XC_left_ptr,
   XC_fleur,
   XC_top_side,
   XC_bottom_side,
   XC_right_side,
   XC_left_side,
   XC_top_right_corner,
   XC_top_left_corner,
   XC_bottom_right_corner,
   XC_bottom_left_corner,
   XC_tcross
};

static Cursor cursors[CURSOR_COUNT];

static Cursor GetResizeCursor(MouseContextType context);
static Cursor CreateCursor(unsigned int shape);

static int mousex;
static int mousey;
static Window mousew;

/** Startup cursor support. */
void StartupCursors(void)
{

   Window win1;
   int winx, winy;
   unsigned int mask;
   int x;

   for(x = 0; x < CURSOR_COUNT; x++) {
      cursors[x] = CreateCursor(cursor_shapes[x]);
   }

   JXQueryPointer(display, rootWindow, &win1, &mousew,
                  &mousex, &mousey, &winx, &winy, &mask);

}

/** Create a cursor for the specified shape. */
Cursor CreateCursor(unsigned int shape)
{
   return JXCreateFontCursor(display, shape);
}

/** Shutdown cursor support. */
void ShutdownCursors(void)
{
   int x;
   for(x = 0; x < CURSOR_COUNT; x++) {
      JXFreeCursor(display, cursors[x]);
   }
}

/** Get the cursor for the specified location on the frame. */
Cursor GetFrameCursor(MouseContextType context)
{
   switch(context & MC_MASK) {
   case MC_BORDER:
      return GetResizeCursor(context);
   case MC_CLOSE:
      break;
   case MC_MAXIMIZE:
      break;
   case MC_MINIMIZE:
      break;
   case MC_MOVE:
      break;
   default:
      break;
   }
   return cursors[CURSOR_DEFAULT];
}

/** Get the cursor for resizing on the specified frame location. */
Cursor GetResizeCursor(MouseContextType context)
{
   if(context & MC_BORDER_N) {
      if(context & MC_BORDER_E) {
         return cursors[CURSOR_NE];
      } else if(context & MC_BORDER_W) {
         return cursors[CURSOR_NW];
      } else {
         return cursors[CURSOR_NORTH];
      }
   } else if(context & MC_BORDER_S) {
      if(context & MC_BORDER_E) {
         return cursors[CURSOR_SE];
      } else if(context & MC_BORDER_W) {
         return cursors[CURSOR_SW];
      } else {
         return cursors[CURSOR_SOUTH];
      }
   } else if(context & MC_BORDER_E) {
         return cursors[CURSOR_EAST];
   } else if(context & MC_BORDER_W) {
      return cursors[CURSOR_WEST];
   } else {
      return cursors[CURSOR_DEFAULT];
   }
}

/** Grab the mouse for resizing a window. */
char GrabMouseForResize(MouseContextType context)
{
   Cursor cur;
   int result;
   unsigned int mask;

   cur = GetFrameCursor(context);
   mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
   result = JXGrabPointer(display, rootWindow, False, mask,
                          GrabModeAsync, GrabModeAsync, None,
                          cur, CurrentTime);
   if(JLIKELY(result == GrabSuccess)) {
      mousew = rootWindow;
      return 1;
   } else {
      return 0;
   }
}

/** Grab the mouse for moving a window. */
char GrabMouseForMove(void)
{
   int result;
   unsigned int mask;
   mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
   result = JXGrabPointer(display, rootWindow, False, mask,
                          GrabModeAsync, GrabModeAsync, None,
                          cursors[CURSOR_MOVE], CurrentTime);
   if(JLIKELY(result == GrabSuccess)) {
      mousew = rootWindow;
      return 1;
   } else {
      return 0;
   }
}

/** Grab the mouse. */
char GrabMouse(Window w)
{
   int result;
   unsigned int mask;
   mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
   result = JXGrabPointer(display, w, False, mask,
                          GrabModeAsync, GrabModeAsync, None,
                          cursors[CURSOR_DEFAULT], CurrentTime);
   if(JLIKELY(result == GrabSuccess)) {
      mousew = w;
      return 1;
   } else {
      return 0;
   }
}

/** Grab the mouse for choosing a window. */
char GrabMouseForChoose(void)
{
   int result;
   unsigned int mask;
   mask = ButtonPressMask | ButtonReleaseMask;
   result = JXGrabPointer(display, rootWindow, False, mask,
                          GrabModeAsync, GrabModeAsync, None,
                          cursors[CURSOR_CHOOSE], CurrentTime);
   if(JLIKELY(result == GrabSuccess)) {
      mousew = rootWindow;
      return 1;
   } else {
      return 0;
   }
}

/** Set the default cursor for a window. */
void SetDefaultCursor(Window w)
{
   if(JLIKELY(w != None)) {
      JXDefineCursor(display, w, cursors[CURSOR_DEFAULT]);
   }
}

/** Move the mouse to the specified coordinates on a window. */
void MoveMouse(Window win, int x, int y)
{
   Window win1;
   int winx, winy;
   unsigned int mask;
   JXWarpPointer(display, None, win, 0, 0, 0, 0, x, y);
   JXQueryPointer(display, rootWindow, &win1, &mousew,
                  &mousex, &mousey, &winx, &winy, &mask);
}

/** Set the current mouse position. */
void SetMousePosition(int x, int y, Window w)
{
   mousex = x;
   mousey = y;
   mousew = w;
}

/** Get the current mouse position. */
void GetMousePosition(int *x, int *y, Window *w)
{
   *x = mousex;
   *y = mousey;
   *w = mousew;
}

/** Get the current mouse buttons pressed. */
unsigned int GetMouseMask(void)
{
   Window win1;
   int winx, winy;
   unsigned int mask;
   JXQueryPointer(display, rootWindow, &win1, &mousew,
                  &mousex, &mousey, &winx, &winy, &mask);
   return mask;
}
