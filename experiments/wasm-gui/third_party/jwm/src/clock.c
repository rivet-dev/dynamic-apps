/**
 * @file clock.c
 * @author Joe Wingbermuehle
 * @date 2005-2006
 *
 * @brief Clock tray component.
 *
 */

#include "jwm.h"
#include "clock.h"
#include "tray.h"
#include "color.h"
#include "font.h"
#include "timing.h"
#include "main.h"
#include "command.h"
#include "cursor.h"
#include "popup.h"
#include "misc.h"
#include "settings.h"
#include "event.h"
#include "action.h"

/** Structure to respresent a clock tray component. */
typedef struct ClockType {

   TrayComponentType *cp;        /**< Common component data. */

   char *format;                 /**< The time format to use. */
   char *zone;                   /**< The time zone to use (NULL = local). */
   struct ActionNode *actions;   /**< Actions */
   TimeType lastTime;            /**< Currently displayed time. */

   /* The following are used to control popups. */
   int mousex;                /**< Last mouse x-coordinate. */
   int mousey;                /**< Last mouse y-coordinate. */
   TimeType mouseTime;        /**< Time of the last mouse motion. */

   int userWidth;             /**< User-specified clock width (or 0). */

   struct ClockType *next;    /**< Next clock in the list. */

} ClockType;

/** The default time format to use. */
static const char *DEFAULT_FORMAT = "%I:%M %p";

static ClockType *clocks;

static void Create(TrayComponentType *cp);
static void Resize(TrayComponentType *cp);
static void Destroy(TrayComponentType *cp);
static void ProcessClockButtonPress(TrayComponentType *cp,
                                    int x, int y, int button);
static void ProcessClockButtonRelease(TrayComponentType *cp,
                                      int x, int y, int button);
static void ProcessClockMotionEvent(TrayComponentType *cp,
                                    int x, int y, int mask);

static void DrawClock(ClockType *clk, const TimeType *now);

static void SignalClock(const struct TimeType *now, int x, int y, Window w,
                        void *data);


/** Initialize clocks. */
void InitializeClock(void)
{
   clocks = NULL;
}

/** Start clock(s). */
void StartupClock(void)
{
   ClockType *clk;
   for(clk = clocks; clk; clk = clk->next) {
      if(clk->cp->requestedWidth == 0) {
         clk->cp->requestedWidth = 1;
      }
      if(clk->cp->requestedHeight == 0) {
         clk->cp->requestedHeight = GetStringHeight(FONT_CLOCK) + 4;
      }
   }
}

/** Destroy clock(s). */
void DestroyClock(void)
{
   while(clocks) {
      ClockType *cp = clocks->next;

      if(clocks->format) {
         Release(clocks->format);
      }
      if(clocks->zone) {
         Release(clocks->zone);
      }
      DestroyActions(clocks->actions);
      UnregisterCallback(SignalClock, clocks);

      Release(clocks);
      clocks = cp;
   }
}

/** Create a clock tray component. */
TrayComponentType *CreateClock(const char *format, const char *zone,
                               int width, int height)
{

   TrayComponentType *cp;
   ClockType *clk;

   clk = Allocate(sizeof(ClockType));
   clk->next = clocks;
   clocks = clk;

   clk->mousex = -settings.doubleClickDelta;
   clk->mousey = -settings.doubleClickDelta;
   clk->mouseTime.seconds = 0;
   clk->mouseTime.ms = 0;
   clk->userWidth = 0;

   if(!format) {
      format = DEFAULT_FORMAT;
   }
   clk->format = CopyString(format);
   clk->zone = CopyString(zone);
   clk->actions = NULL;
   memset(&clk->lastTime, 0, sizeof(clk->lastTime));

   cp = CreateTrayComponent();
   cp->object = clk;
   clk->cp = cp;
   if(width > 0) {
      cp->requestedWidth = width;
      clk->userWidth = 1;
   } else {
      cp->requestedWidth = 0;
      clk->userWidth = 0;
   }
   cp->requestedHeight = height;

   cp->Create = Create;
   cp->Resize = Resize;
   cp->Destroy = Destroy;
   cp->ProcessButtonPress = ProcessClockButtonPress;
   cp->ProcessButtonRelease = ProcessClockButtonRelease;
   cp->ProcessMotionEvent = ProcessClockMotionEvent;

   RegisterCallback(Min(900, settings.popupDelay / 2), SignalClock, clk);

   return cp;
}

/** Add an action to a clock. */
void AddClockAction(TrayComponentType *cp,
                    const char *action,
                    int mask)
{
   ClockType *clock = (ClockType*)cp->object;
   AddAction(&clock->actions, action, mask);
}

/** Initialize a clock tray component. */
void Create(TrayComponentType *cp)
{
   cp->pixmap = JXCreatePixmap(display, rootWindow, cp->width, cp->height,
                               rootDepth);
}

/** Resize a clock tray component. */
void Resize(TrayComponentType *cp)
{

   ClockType *clk;
   TimeType now;

   Assert(cp);

   clk = (ClockType*)cp->object;

   Assert(clk);

   if(cp->pixmap != None) {
      JXFreePixmap(display, cp->pixmap);
   }

   cp->pixmap = JXCreatePixmap(display, rootWindow, cp->width, cp->height,
                               rootDepth);

   memset(&clk->lastTime, 0, sizeof(clk->lastTime));

   GetCurrentTime(&now);
   DrawClock(clk, &now);

}

/** Destroy a clock tray component. */
void Destroy(TrayComponentType *cp)
{
   Assert(cp);
   if(cp->pixmap != None) {
      JXFreePixmap(display, cp->pixmap);
   }
}

/** Process a press event on a clock tray component. */
void ProcessClockButtonPress(TrayComponentType *cp, int x, int y, int button)
{
   const ClockType *clk = (ClockType*)cp->object;
   ProcessActionPress(clk->actions, cp, x, y, button);
}

void ProcessClockButtonRelease(TrayComponentType *cp, int x, int y, int button)
{
   const ClockType *clk = (ClockType*)cp->object;
   ProcessActionRelease(clk->actions, cp, x, y, button);
}

/** Process a motion event on a clock tray component. */
void ProcessClockMotionEvent(TrayComponentType *cp,
                             int x, int y, int mask)
{
   ClockType *clk = (ClockType*)cp->object;
   clk->mousex = cp->screenx + x;
   clk->mousey = cp->screeny + y;
   GetCurrentTime(&clk->mouseTime);
}

/** Update a clock tray component. */
void SignalClock(const TimeType *now, int x, int y, Window w, void *data)
{

   ClockType *cp = (ClockType*)data;
   const char *longTime;

   DrawClock(cp, now);
   if(cp->cp->tray->window == w &&
      abs(cp->mousex - x) < settings.doubleClickDelta &&
      abs(cp->mousey - y) < settings.doubleClickDelta) {
      if(GetTimeDifference(now, &cp->mouseTime) >= settings.popupDelay) {
         longTime = GetTimeString("%c", cp->zone);
         ShowPopup(x, y, longTime, POPUP_CLOCK);
      }
   }

}

/** Draw a clock tray component. */
void DrawClock(ClockType *clk, const TimeType *now)
{

   TrayComponentType *cp;
   const char *timeString;
   int width;
   int rwidth;

   /* Only draw if the time changed. */
   if(now->seconds == clk->lastTime.seconds) {
      return;
   }

   /* Clear the area. */
   cp = clk->cp;
   if(colors[COLOR_CLOCK_BG1] == colors[COLOR_CLOCK_BG2]) {
      JXSetForeground(display, rootGC, colors[COLOR_CLOCK_BG1]);
      JXFillRectangle(display, cp->pixmap, rootGC, 0, 0,
                      cp->width, cp->height);
   } else {
      DrawGradient(cp->pixmap, rootGC,
                   colors[COLOR_CLOCK_BG1], colors[COLOR_CLOCK_BG2],
                   0, 0, cp->width, cp->height, gradients[COLOR_CLOCK_BG1]);
   }

   /* Determine if the clock is the right size. */
   timeString = GetTimeString(clk->format, clk->zone);
   width = GetStringWidth(FONT_CLOCK, timeString);
   rwidth = width + 4;
   if(rwidth == clk->cp->requestedWidth || clk->userWidth) {

      /* Draw the clock. */
      RenderString(cp->pixmap, FONT_CLOCK, COLOR_CLOCK_FG,
                   (cp->width - width) / 2,
                   (cp->height - GetStringHeight(FONT_CLOCK)) / 2,
                   cp->width, timeString);

      UpdateSpecificTray(clk->cp->tray, clk->cp);

   } else {

      /* Wrong size. Resize. */
      clk->cp->requestedWidth = rwidth;
      ResizeTray(clk->cp->tray);

   }

}
