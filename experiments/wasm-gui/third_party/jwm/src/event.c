/**
 * @file event.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Functions to handle X11 events.
 *
 */

#include "jwm.h"
#include "event.h"

#include "client.h"
#include "clientlist.h"
#include "confirm.h"
#include "cursor.h"
#include "desktop.h"
#include "dock.h"
#include "icon.h"
#include "binding.h"
#include "move.h"
#include "place.h"
#include "resize.h"
#include "root.h"
#include "swallow.h"
#include "taskbar.h"
#include "timing.h"
#include "winmenu.h"
#include "settings.h"
#include "tray.h"
#include "popup.h"
#include "pager.h"
#include "grab.h"
#include "screen.h"

#define MIN_TIME_DELTA 50

Time eventTime = CurrentTime;

typedef struct CallbackNode {
   TimeType last;
   int freq;
   SignalCallback callback;
   void *data;
   struct CallbackNode *next;
} CallbackNode;

static CallbackNode *callbacks = NULL;

static char restack_pending = 0;
static char task_update_pending = 0;
static char pager_update_pending = 0;

static void Signal(void);

static void ProcessBinding(MouseContextType context, ClientNode *np,
                           unsigned state, int code, int x, int y);

static void HandleConfigureRequest(const XConfigureRequestEvent *event);
static char HandleConfigureNotify(const XConfigureEvent *event);
static char HandleExpose(const XExposeEvent *event);
static char HandlePropertyNotify(const XPropertyEvent *event);
static void HandleClientMessage(const XClientMessageEvent *event);
static void HandleColormapChange(const XColormapEvent *event);
static char HandleDestroyNotify(const XDestroyWindowEvent *event);
static void HandleMapRequest(const XMapEvent *event);
static void HandleUnmapNotify(const XUnmapEvent *event);
static void HandleButtonEvent(const XButtonEvent *event);
static void ToggleMaximized(ClientNode *np, MaxFlags flags);
static void HandleKeyPress(const XKeyEvent *event);
static void HandleKeyRelease(const XKeyEvent *event);
static void HandleEnterNotify(const XCrossingEvent *event);
static void HandleMotionNotify(const XMotionEvent *event);
static char HandleSelectionClear(const XSelectionClearEvent *event);

static void HandleNetMoveResize(const XClientMessageEvent *event,
                                ClientNode *np);
static void HandleNetWMMoveResize(const XClientMessageEvent *evnet,
                                  ClientNode *np);
static void HandleNetRestack(const XClientMessageEvent *event,
                             ClientNode *np);
static void HandleNetWMState(const XClientMessageEvent *event,
                             ClientNode *np);
static void HandleFrameExtentsRequest(const XClientMessageEvent *event);
static void UpdateState(ClientNode *np);
static void DiscardEnterEvents();
static char ClientCanReceiveSloppyFocus(const ClientNode *np);

#ifdef USE_SHAPE
static void HandleShapeEvent(const XShapeEvent *event);
#endif

/** Wait for an event and process it. */
char WaitForEvent(XEvent *event)
{
   struct timeval timeout;
   CallbackNode *cp;
   fd_set fds;
   long sleepTime;
   int fd;
   char handled;

#ifdef ConnectionNumber
   fd = ConnectionNumber(display);
#else
   fd = JXConnectionNumber(display);
#endif

   /* Compute how long we should sleep. */
   sleepTime = 10 * 1000;  /* 10 seconds. */
   for(cp = callbacks; cp; cp = cp->next) {
      if(cp->freq > 0 && cp->freq < sleepTime) {
         sleepTime = cp->freq;
      }
   }

   do {

      while(JXPending(display) == 0) {
         FD_ZERO(&fds);
         FD_SET(fd, &fds);
         timeout.tv_sec = sleepTime / 1000;
         timeout.tv_usec = (sleepTime % 1000) * 1000;
         if(select(fd + 1, &fds, NULL, NULL, &timeout) <= 0) {
            Signal();
         }
         if(JUNLIKELY(shouldExit)) {
            return 0;
         }
      }

      Signal();

      JXNextEvent(display, event);
      UpdateTime(event);

      switch(event->type) {
      case ConfigureRequest:
         HandleConfigureRequest(&event->xconfigurerequest);
         handled = 1;
         break;
      case MapRequest:
         HandleMapRequest(&event->xmap);
         handled = 1;
         break;
      case PropertyNotify:
         handled = HandlePropertyNotify(&event->xproperty);
         break;
      case ClientMessage:
         HandleClientMessage(&event->xclient);
         handled = 1;
         break;
      case UnmapNotify:
         HandleUnmapNotify(&event->xunmap);
         handled = 1;
         break;
      case Expose:
         handled = HandleExpose(&event->xexpose);
         break;
      case ColormapNotify:
         HandleColormapChange(&event->xcolormap);
         handled = 1;
         break;
      case DestroyNotify:
         handled = HandleDestroyNotify(&event->xdestroywindow);
         break;
      case SelectionClear:
         handled = HandleSelectionClear(&event->xselectionclear);
         break;
      case ResizeRequest:
         handled = HandleDockResizeRequest(&event->xresizerequest);
         break;
      case MotionNotify:
         SetMousePosition(event->xmotion.x_root, event->xmotion.y_root,
                          event->xmotion.window);
         handled = 0;
         break;
      case ButtonPress:
      case ButtonRelease:
         SetMousePosition(event->xbutton.x_root, event->xbutton.y_root,
                          event->xbutton.window);
         handled = 0;
         break;
      case EnterNotify:
         SetMousePosition(event->xcrossing.x_root, event->xcrossing.y_root,
                          event->xcrossing.window);
         handled = 0;
         break;
      case LeaveNotify:
         SetMousePosition(event->xcrossing.x_root, event->xcrossing.y_root,
                          None);
         handled = 0;
         break;
      case ReparentNotify:
         HandleDockReparentNotify(&event->xreparent);
         handled = 1;
         break;
      case ConfigureNotify:
         handled = HandleConfigureNotify(&event->xconfigure);
         break;
      case CreateNotify:
      case MapNotify:
      case GraphicsExpose:
      case NoExpose:
         handled = 1;
         break;
      default:
         if(0) {
#ifdef USE_SHAPE
         } else if(haveShape && event->type == shapeEvent) {
            HandleShapeEvent((XShapeEvent*)event);
            handled = 1;
#endif
         } else {
            handled = 0;
         }
         break;
      }

      if(!handled) {
         handled = ProcessTrayEvent(event);
      }
      if(!handled) {
         handled = ProcessDialogEvent(event);
      }
      if(!handled) {
         handled = ProcessSwallowEvent(event);
      }
      if(!handled) {
         handled = ProcessPopupEvent(event);
      }

   } while(handled && JLIKELY(!shouldExit));

   return !handled;

}

/** Wake up components that need to run at certain times. */
void Signal(void)
{
   static TimeType last = ZERO_TIME;

   CallbackNode *cp;   
   TimeType now;
   Window w;
   int x, y;

   if(restack_pending) {
      RestackClients();
      restack_pending = 0;
   }
   if(task_update_pending) {
      UpdateTaskBar();
      task_update_pending = 0;
   }
   if(pager_update_pending) {
      UpdatePager();
      pager_update_pending = 0;
   }

   GetCurrentTime(&now);
   if(GetTimeDifference(&now, &last) < MIN_TIME_DELTA) {
      return;
   }
   last = now;

   GetMousePosition(&x, &y, &w);
   for(cp = callbacks; cp; cp = cp->next) {
      if(cp->freq == 0 || GetTimeDifference(&now, &cp->last) >= cp->freq) {
         cp->last = now;
         (cp->callback)(&now, x, y, w, cp->data);
      }
   }
}

/** Process an event. */
void ProcessEvent(XEvent *event)
{
   switch(event->type) {
   case ButtonPress:
   case ButtonRelease:
      HandleButtonEvent(&event->xbutton);
      break;
   case KeyPress:
      HandleKeyPress(&event->xkey);
      break;
   case KeyRelease:
      HandleKeyRelease(&event->xkey);
      break;
   case EnterNotify:
      HandleEnterNotify(&event->xcrossing);
      break;
   case MotionNotify:
      while(JXCheckTypedEvent(display, MotionNotify, event));
      UpdateTime(event);
      HandleMotionNotify(&event->xmotion);
      break;
   case LeaveNotify:
   case DestroyNotify:
   case Expose:
   case ConfigureNotify:
      break;
   default:
      Debug("Unknown event type: %d", event->type);
      break;
   }
}

/** Discard button events for the specified windows. */
void DiscardButtonEvents()
{
   XEvent event;
   JXSync(display, False);
   while(JXCheckMaskEvent(display, ButtonPressMask | ButtonReleaseMask,
			  &event)) {
      UpdateTime(&event);
   }
}

/** Discard motion events for the specified window. */
void DiscardMotionEvents(XEvent *event, Window w)
{
   XEvent temp;
   JXSync(display, False);
   while(JXCheckTypedEvent(display, MotionNotify, &temp)) {
      UpdateTime(&temp);
      SetMousePosition(temp.xmotion.x_root, temp.xmotion.y_root,
                       temp.xmotion.window);
      if(temp.xmotion.window == w) {
         *event = temp;
      }
   }
}

/** Discard key events for the specified window. */
void DiscardKeyEvents(XEvent *event, Window w)
{
   JXSync(display, False);
   while(JXCheckTypedWindowEvent(display, w, KeyPress, event)) {
      UpdateTime(event);
   }
}

/** Discard enter notify events. */
void DiscardEnterEvents()
{
   XEvent event;
   JXSync(display, False);
   while(JXCheckMaskEvent(display, EnterWindowMask, &event)) {
      UpdateTime(&event);
      SetMousePosition(event.xmotion.x_root, event.xmotion.y_root,
                       event.xmotion.window);
   }
}

/** Process a selection clear event. */
char HandleSelectionClear(const XSelectionClearEvent *event)
{
   if(event->selection == managerSelection) {
      /* Lost WM selection. */
      shouldExit = 1;
      return 1;
   }
   return HandleDockSelectionClear(event);
}

/** Process a button event. */
void HandleButtonEvent(const XButtonEvent *event)
{
   static Time lastClickTime = 0;
   static int lastX = 0, lastY = 0;
   static unsigned doubleClickActive = 0;

   ClientNode *np;
   int button;
   int north, south, east, west;
   MouseContextType context;

   /* Determine the button to present for processing.
    * Press is positive, release is negative, double clicks
    * are multiplied by 11.
    */
   if(event->type == ButtonPress) {
      if(doubleClickActive == event->button
         && event->time != lastClickTime
         && event->time - lastClickTime <= settings.doubleClickSpeed
         && abs(event->x - lastX) <= settings.doubleClickDelta
         && abs(event->y - lastY) <= settings.doubleClickDelta) {
         button = event->button * 11;
      } else {
         button = event->button;
      }
      doubleClickActive = 0;
   } else {
      button = event->button;
   }
   if(event->type == ButtonRelease) {
      button = -button;
   }
   if(button < 11) {
      doubleClickActive = event->button;
      lastClickTime = event->time;
      lastX = event->x;
      lastY = event->y;
   }

   /* Dispatch the event. */
   np = FindClientByParent(event->window);
   if(np) {
      /* Click on the border. */
      if(event->type == ButtonPress) {
         FocusClient(np);
         RaiseClient(np);
      }
      context = GetBorderContext(np, event->x, event->y);
      ProcessBinding(context, np, event->state, button, event->x, event->y);
   } else if(event->window == rootWindow) {
      /* Click on the root.
       * Note that we use the raw button from the event for ShowRootMenu. */
      if(!ShowRootMenu(event->button, event->x, event->y, 0)) {
         ProcessBinding(MC_ROOT, NULL, event->state, button, 0, 0);
      }
   } else {
      /* Click over window content. */
      const unsigned int mask = event->state & ~lockMask;
      np = FindClientByWindow(event->window);
      if(np) {
         const char move_resize = (np->state.status & STAT_DRAG)
            || ((mask == settings.moveMask)
               && !(np->state.status & STAT_NODRAG));
         switch(event->button) {
         case Button1:
         case Button2:
            FocusClient(np);
            if(settings.focusModel == FOCUS_SLOPPY
               || settings.focusModel == FOCUS_CLICK) {
               RaiseClient(np);
            }
            if(move_resize) {
               GetBorderSize(&np->state, &north, &south, &east, &west);
               MoveClient(np, event->x + west, event->y + north);
            }
            break;
         case Button3:
            if(move_resize) {
               GetBorderSize(&np->state, &north, &south, &east, &west);
               ResizeClient(np, MC_BORDER | MC_BORDER_E | MC_BORDER_S,
                            event->x + west, event->y + north);
            } else {
               FocusClient(np);
               if(settings.focusModel == FOCUS_SLOPPY
                  || settings.focusModel == FOCUS_CLICK) {
                  RaiseClient(np);
               }
            }
            break;
         default:
            break;
         }
         JXAllowEvents(display, ReplayPointer, eventTime);
      }
   }
}

/** Toggle maximized state. */
void ToggleMaximized(ClientNode *np, MaxFlags flags)
{
   if(np) {
      if(np->state.maxFlags == flags) {
         MaximizeClient(np, MAX_NONE);
      } else {
         MaximizeClient(np, flags);
      }
   }
}

/** Process a key or mouse binding. */
void ProcessBinding(MouseContextType context, ClientNode *np,
                    unsigned state, int code, int x, int y)
{
   const ActionType key = GetKey(context, state, code);
   const char keyAction = context == MC_NONE;
   unsigned int desktop;
   switch(key.action) {
   case ACTION_EXEC:
      RunKeyCommand(context, state, code);
      break;
   case ACTION_DESKTOP:
      desktop = key.extra;
      if(currentDesktop == desktop &&
         settings.desktopBackAndForth) {
           desktop = previousDesktop;
      }
      ChangeDesktop(desktop);
      break;
   case ACTION_RDESKTOP:
      RightDesktop();
      break;
   case ACTION_LDESKTOP:
      LeftDesktop();
      break;
   case ACTION_UDESKTOP:
      AboveDesktop();
      break;
   case ACTION_DDESKTOP:
      BelowDesktop();
      break;
   case ACTION_SHOWDESK:
      ShowDesktop();
      break;
   case ACTION_SHOWTRAY:
      ShowAllTrays();
      break;
   case ACTION_NEXT:
      StartWindowWalk();
      FocusNext();
      break;
   case ACTION_NEXTSTACK:
      StartWindowStackWalk();
      WalkWindowStack(1);
      break;
   case ACTION_PREV:
      StartWindowWalk();
      FocusPrevious();
      break;
   case ACTION_PREVSTACK:
      StartWindowStackWalk();
      WalkWindowStack(0);
      break;
   case ACTION_CLOSE:
      if(np) {
         DeleteClient(np);
      }
      break;
   case ACTION_SHADE:
      if(np) {
         if(np->state.status & STAT_SHADED) {
            UnshadeClient(np);
         } else {
            ShadeClient(np);
         }
      }
      break;
   case ACTION_STICK:
      if(np) {
         if(np->state.status & STAT_STICKY) {
            SetClientSticky(np, 0);
         } else {
            SetClientSticky(np, 1);
         }
      }
      break;
   case ACTION_MOVE:
      if(np) {
         if(keyAction) {
            MoveClientKeyboard(np);
         } else {
            MoveClient(np, x, y);
         }
      }
      break;
   case ACTION_RESIZE:
      if(np) {
         /* Use provided context by default. */
         const unsigned char corner = key.extra;
         MouseContextType resizeContext = context;
         if(corner) {
            /* Custom corner specified. */
            resizeContext = MC_BORDER;
            resizeContext |= (corner & ACTION_RESIZE_N) ? MC_BORDER_N : MC_NONE;
            resizeContext |= (corner & ACTION_RESIZE_S) ? MC_BORDER_S : MC_NONE;
            resizeContext |= (corner & ACTION_RESIZE_E) ? MC_BORDER_E : MC_NONE;
            resizeContext |= (corner & ACTION_RESIZE_W) ? MC_BORDER_W : MC_NONE;
         } else if(keyAction) {
            /* No corner specified for a key action, assume SE. */
            resizeContext = MC_BORDER | MC_BORDER_S | MC_BORDER_E;
         }
         if(keyAction) {
            ResizeClientKeyboard(np, resizeContext);
         } else {
            ResizeClient(np, resizeContext, x, y);
         }
      }
      break;
   case ACTION_MIN:
      if(np && (np->state.border & BORDER_MIN)) {
         MinimizeClient(np, 1);
      }
      break;
   case ACTION_MAX:
      if(np) {
         if(keyAction) {
            ToggleMaximized(np, MAX_HORIZ | MAX_VERT);
         } else {
            MaximizeClientDefault(np);
         }
      }
      break;
   case ACTION_RESTORE:
      if(np) {
         if(np->state.maxFlags) {
            MaximizeClient(np, MAX_NONE);
         } else if(np->state.border & BORDER_MIN) {
            MinimizeClient(np, 1);
         }
      }
      break;
   case ACTION_MAXTOP:
      ToggleMaximized(np, MAX_TOP | MAX_HORIZ);
      break;
   case ACTION_MAXTOPLEFT:
      ToggleMaximized(np, MAX_TOP | MAX_LEFT);
      break;
   case ACTION_MAXTOPRIGHT:
      ToggleMaximized(np, MAX_TOP | MAX_RIGHT);
      break;
   case ACTION_MAXBOTTOM:
      ToggleMaximized(np, MAX_BOTTOM | MAX_HORIZ);
      break;
   case ACTION_MAXBOTTOMLEFT:
      ToggleMaximized(np, MAX_BOTTOM | MAX_LEFT);
      break;
   case ACTION_MAXBOTTOMRIGHT:
      ToggleMaximized(np, MAX_BOTTOM | MAX_RIGHT);
      break;
   case ACTION_MAXLEFT:
      ToggleMaximized(np, MAX_LEFT | MAX_VERT);
      break;
   case ACTION_MAXRIGHT:
      ToggleMaximized(np, MAX_RIGHT | MAX_VERT);
      break;
   case ACTION_MAXV:
      ToggleMaximized(np, MAX_VERT);
      break;
   case ACTION_MAXH:
      ToggleMaximized(np, MAX_HORIZ);
      break;
   case ACTION_ROOT:
      ShowKeyMenu(context, state, code);
      break;
   case ACTION_WIN:
      if(np) {
         if(keyAction) {
            RaiseClient(np);
            ShowWindowMenu(np, np->x, np->y, 1);
         } else {
            const unsigned bsize = (np->state.border & BORDER_OUTLINE)
                                 ? settings.borderWidth : 0;
            const unsigned titleHeight = GetTitleHeight();
            const int mx = np->x + x - bsize;
            const int my = np->y + y - titleHeight - bsize;
            ShowWindowMenu(np, mx, my, 0);
         }
      }
      break;
   case ACTION_RESTART:
      Restart();
      break;
   case ACTION_EXIT:
      Exit(1);
      break;
   case ACTION_FULLSCREEN:
      if(np) {
         if(np->state.status & STAT_FULLSCREEN) {
            SetClientFullScreen(np, 0);
         } else {
            SetClientFullScreen(np, 1);
         }
      }
      break;
   case ACTION_SEND:
      if(np) {
         const unsigned desktop = key.extra;
         SetClientDesktop(np, desktop);
         ChangeDesktop(desktop);
      }
      break;
   case ACTION_SENDR:
      if(np) {
         const unsigned desktop = GetRightDesktop(np->state.desktop);
         SetClientDesktop(np, desktop);
         ChangeDesktop(desktop);
      }
      break;
   case ACTION_SENDL:
      if(np) {
         const unsigned desktop = GetLeftDesktop(np->state.desktop);
         SetClientDesktop(np, desktop);
         ChangeDesktop(desktop);
      }
      break;
   case ACTION_SENDU:
      if(np) {
         const unsigned desktop = GetAboveDesktop(np->state.desktop);
         SetClientDesktop(np, desktop);
         ChangeDesktop(desktop);
      }
      break;
   case ACTION_SENDD:
      if(np) {
         const unsigned desktop = GetBelowDesktop(np->state.desktop);
         SetClientDesktop(np, desktop);
         ChangeDesktop(desktop);
      }
      break;
   case ACTION_CENTER:
      if(np) {
         const ScreenType *sp = GetCurrentScreen(np->x, np->y);
         if(np->state.maxFlags) {
            MaximizeClient(np, MAX_NONE);
         }

         np->x = sp->x + (sp->width - np->width) / 2;
         np->y = sp->y + (sp->height - np->height) / 2;
         ConstrainPosition(np);
         ResetBorder(np);
         SendConfigureEvent(np);
         RequirePagerUpdate();
      }
      break;
    case ACTION_AT:
      StartWindowWalk();
      FocusAt(key.extra);
      break;
   case ACTION_KILL:
      KillClient(np);
      break;

   default:
      break;
   }
   DiscardEnterEvents();
}

/** Process a key press event. */
void HandleKeyPress(const XKeyEvent *event)
{
   ClientNode *np;
   SetMousePosition(event->x_root, event->y_root, event->window);
   np = GetActiveClient();
   ProcessBinding(MC_NONE, np, event->state, event->keycode, 0, 0);
}

/** Handle a key release event. */
void HandleKeyRelease(const XKeyEvent *event)
{
   const ActionType key = GetKey(MC_NONE, event->state, event->keycode);
   if(   key.action != ACTION_NEXTSTACK
      && key.action != ACTION_NEXT
      && key.action != ACTION_PREV
      && key.action != ACTION_PREVSTACK) {
      StopWindowWalk();
   }
}

/** Process a configure request. */
void HandleConfigureRequest(const XConfigureRequestEvent *event)
{
   ClientNode *np;

   if(HandleDockConfigureRequest(event)) {
      return;
   }

   np = FindClientByWindow(event->window);
   if(np) {

      int deltax, deltay;
      char changed = 0;
      char resized = 0;

      GetGravityDelta(np, np->gravity, &deltax, &deltay);
      if((event->value_mask & CWWidth) && (event->width != np->width)) {
         switch(np->gravity) {
         case EastGravity:
         case NorthEastGravity:
         case SouthEastGravity:
            /* Right side should not move. */
            np->x -= event->width - np->width;
            break;
         case WestGravity:
         case NorthWestGravity:
         case SouthWestGravity:
            /* Left side should not move. */
            break;
         case CenterGravity:
            /* Center of the window should not move. */
            np->x -= (event->width - np->width) / 2;
            break;
         default:
            break;
         }
         np->width = event->width;
         changed = 1;
         resized = 1;
      }
      if((event->value_mask & CWHeight) && (event->height != np->height)) {
         switch(np->gravity) {
         case NorthGravity:
         case NorthEastGravity:
         case NorthWestGravity:
            /* Top should not move. */
            break;
         case SouthGravity:
         case SouthEastGravity:
         case SouthWestGravity:
            /* Bottom should not move. */
            np->y -= event->height - np->height;
            break;
         case CenterGravity:
            /* Center of the window should not move. */
            np->y -= (event->height - np->height) / 2;
            break;
         default:
            break;
         }
         np->height = event->height;
         changed = 1;
         resized = 1;
      }
      if((event->value_mask & CWX) && (event->x - deltax != np->x)) {
         np->x = event->x - deltax;
         changed = 1;
      }
      if((event->value_mask & CWY) && (event->y - deltay != np->y)) {
         np->y = event->y - deltay;
         changed = 1;
      }

      /* Update stacking. */
      if((event->value_mask & CWStackMode)) {
         Window above = None;
         if(event->value_mask & CWSibling) {
            above = event->above;
         }
         RestackClient(np, above, event->detail);
      }

      /* Return early if there's nothing to do. */
      if(!changed) {
         /* Nothing changed; send a synthetic configure event. */
         SendConfigureEvent(np);
         return;
      }

      /* Stop any move/resize that may be in progress. */
      if(np->controller) {
         (np->controller)(0);
      }

      /* If the client is maximized, restore it first. */
      if(np->state.maxFlags) {
         MaximizeClient(np, MAX_NONE);
      }

      if(np->state.border & BORDER_CONSTRAIN) {
         resized = 1;
      }
      if(resized) {
         /* The size changed so the parent will need to be redrawn. */
         ConstrainSize(np);
         ConstrainPosition(np);
         ResetBorder(np);
      } else {
         /* Only the position changed; move the client. */
         int north, south, east, west;
         GetBorderSize(&np->state, &north, &south, &east, &west);

         if(np->parent != None) {
            JXMoveWindow(display, np->parent, np->x - west, np->y - north);
            SendConfigureEvent(np);
         } else {
            JXMoveWindow(display, np->window, np->x, np->y);
         }
      }

      RequirePagerUpdate();

   } else {

      /* We don't know about this window, just let the configure through. */

      XWindowChanges wc;
      wc.stack_mode = event->detail;
      wc.sibling = event->above;
      wc.border_width = event->border_width;
      wc.x = event->x;
      wc.y = event->y;
      wc.width = event->width;
      wc.height = event->height;
      JXConfigureWindow(display, event->window, event->value_mask, &wc);

   }
}

/** Process a configure notify event. */
char HandleConfigureNotify(const XConfigureEvent *event)
{
   if(event->window != rootWindow) {
      return 0;
   }
   if(rootWidth != event->width || rootHeight != event->height) {
      rootWidth = event->width;
      rootHeight = event->height;
      shouldRestart = 1;
      shouldExit = 1;
   }
   return 1;
}

/** True if a window should receive focus if the pointer moves over it. */
char ClientCanReceiveSloppyFocus(const ClientNode *np)
{
   /* Not if the window is already active */
   if(np->state.status & STAT_ACTIVE) {
      return 0;
   }
   /* Only when running in a sloppy focus mode */
   if(! (settings.focusModel == FOCUS_SLOPPY
         || settings.focusModel == FOCUS_SLOPPY_TITLE)) {
      return 0;
   }
   /* The user will probably understand desktop windows as a kind of
    * empty space. If they took focus based just on mouse motion over
    * them, sloppy focus would feel broken. They can still be given
    * focus with a click. */
   if((np->state.windowType == WINDOW_TYPE_DESKTOP)
      || (np->state.layer == LAYER_DESKTOP)) {
      return 0;
   }
   return 1;
}

/** Process an enter notify event. */
void HandleEnterNotify(const XCrossingEvent *event)
{
   ClientNode *np;
   Cursor cur;
   np = FindClient(event->window);
   if(np) {
      if(ClientCanReceiveSloppyFocus(np)) {
         FocusClient(np);
      }
      if(np->parent == event->window) {
         np->mouseContext = GetBorderContext(np, event->x, event->y);
         cur = GetFrameCursor(np->mouseContext);
         JXDefineCursor(display, np->parent, cur);
      } else if(np->mouseContext != MC_NONE) {
         SetDefaultCursor(np->parent);
         np->mouseContext = MC_NONE;
      }
   }

}

/** Handle an expose event. */
char HandleExpose(const XExposeEvent *event)
{
   ClientNode *np;
   np = FindClientByParent(event->window);
   if(np) {
      if(event->count == 0) {
         DrawBorder(np);
      }
      return 1;
   } else {
      np = FindClientByWindow(event->window);
      if(np) {
         if(np->state.status & STAT_WMDIALOG) {

            /* Dialog expose events are handled elsewhere. */
            return 0;

         } else {

            /* Ignore other expose events for client windows. */
            return 1;

         }
      }
      return event->count ? 1 : 0;
   }
}

/** Handle a property notify event. */
char HandlePropertyNotify(const XPropertyEvent *event)
{
   ClientNode *np = FindClientByWindow(event->window);
   if(np) {
      char changed = 0;
      switch(event->atom) {
      case XA_WM_NAME:
         ReadWMName(np);
         changed = 1;
         break;
      case XA_WM_NORMAL_HINTS:
         ReadWMNormalHints(np);
         if(ConstrainSize(np)) {
            ResetBorder(np);
         }
         changed = 1;
         break;
      case XA_WM_HINTS:
         if(np->state.status & STAT_URGENT) {
            UnregisterCallback(SignalUrgent, np);
         }
         ReadWMHints(np->window, &np->state, 1);
         if(np->state.status & STAT_URGENT) {
            RegisterCallback(URGENCY_DELAY, SignalUrgent, np);
         }
         WriteState(np);
         break;
      case XA_WM_TRANSIENT_FOR:
         JXGetTransientForHint(display, np->window, &np->owner);
         break;
      case XA_WM_ICON_NAME:
         break;
      case XA_WM_CLIENT_MACHINE:
         ReadWMMachine(np);
         changed = 1;
         break;
      default:
         if(event->atom == atoms[ATOM_WM_COLORMAP_WINDOWS]) {
            ReadWMColormaps(np);
            UpdateClientColormap(np);
         } else if(event->atom == atoms[ATOM_WM_PROTOCOLS]) {
            ReadWMProtocols(np->window, &np->state);
         } else if(event->atom == atoms[ATOM_NET_WM_ICON]) {
            LoadIcon(np);
            changed = 1;
         } else if(event->atom == atoms[ATOM_NET_WM_NAME]) {
            ReadWMName(np);
            changed = 1;
         } else if(event->atom == atoms[ATOM_NET_WM_STRUT_PARTIAL]) {
            ReadClientStrut(np);
         } else if(event->atom == atoms[ATOM_NET_WM_STRUT]) {
            ReadClientStrut(np);
         } else if(event->atom == atoms[ATOM_MOTIF_WM_HINTS]) {
            UpdateState(np);
            WriteState(np);
            ResetBorder(np);
            changed = 1;
         } else if(event->atom == atoms[ATOM_NET_WM_WINDOW_OPACITY]) {
            ReadWMOpacity(np->window, &np->state.opacity);
            if(np->parent != None) {
               SetOpacity(np, np->state.opacity, 1);
            }
         }
         break;
      }

      if(changed) {
         DrawBorder(np);
         RequireTaskUpdate();
         RequirePagerUpdate();
      }
      if(np->state.status & STAT_WMDIALOG) {
         return 0;
      } else {
         return 1;
      }
   }

   return 1;
}

/** Handle a client message. */
void HandleClientMessage(const XClientMessageEvent *event)
{

   ClientNode *np;
#ifdef DEBUG
   char *atomName;
#endif

   np = FindClientByWindow(event->window);
   if(np) {
      if(event->message_type == atoms[ATOM_WM_CHANGE_STATE]) {

         if(np->controller) {
            (np->controller)(0);
         }

         switch(event->data.l[0]) {
         case WithdrawnState:
            SetClientWithdrawn(np);
            break;
         case IconicState:
            MinimizeClient(np, 1);
            break;
         case NormalState:
            RestoreClient(np, 1);
            break;
         default:
            break;
         }

      } else if(event->message_type == atoms[ATOM_NET_ACTIVE_WINDOW]) {

         RestoreClient(np, 1);
         UnshadeClient(np);
         FocusClient(np);

      } else if(event->message_type == atoms[ATOM_NET_WM_DESKTOP]) {

         if(event->data.l[0] == ~0L) {
            SetClientSticky(np, 1);
         } else {

            if(np->controller) {
               (np->controller)(0);
            }

            if(   event->data.l[0] >= 0
               && event->data.l[0] < (long)settings.desktopCount) {
               np->state.status &= ~STAT_STICKY;
               SetClientDesktop(np, event->data.l[0]);
            }
         }

      } else if(event->message_type == atoms[ATOM_NET_CLOSE_WINDOW]) {

         DeleteClient(np);

      } else if(event->message_type == atoms[ATOM_NET_MOVERESIZE_WINDOW]) {

         HandleNetMoveResize(event, np);

      } else if(event->message_type == atoms[ATOM_NET_WM_MOVERESIZE]) {

         HandleNetWMMoveResize(event, np);

      } else if(event->message_type == atoms[ATOM_NET_RESTACK_WINDOW]) {

         HandleNetRestack(event, np);

      } else if(event->message_type == atoms[ATOM_NET_WM_STATE]) {

         HandleNetWMState(event, np);

      } else {

#ifdef DEBUG
         atomName = JXGetAtomName(display, event->message_type);
         Debug("Unknown ClientMessage to client: %s", atomName);
         JXFree(atomName);
#endif

      }

   } else if(event->window == rootWindow) {

      if(event->message_type == atoms[ATOM_JWM_RESTART]) {
         Restart();
      } else if(event->message_type == atoms[ATOM_JWM_EXIT]) {
         Exit(0);
      } else if(event->message_type == atoms[ATOM_JWM_RELOAD]) {
         ReloadMenu();
      } else if(event->message_type == atoms[ATOM_NET_CURRENT_DESKTOP]) {
         ChangeDesktop(event->data.l[0]);
      } else if(event->message_type == atoms[ATOM_NET_SHOWING_DESKTOP]) {
         ShowDesktop();
      } else {
#ifdef DEBUG
         atomName = JXGetAtomName(display, event->message_type);
         Debug("Unknown ClientMessage to root: %s", atomName);
         JXFree(atomName);
#endif
      }

   } else if(event->message_type == atoms[ATOM_NET_REQUEST_FRAME_EXTENTS]) {

      HandleFrameExtentsRequest(event);

   } else if(event->message_type == atoms[ATOM_NET_SYSTEM_TRAY_OPCODE]) {

      HandleDockEvent(event);

   } else {
#ifdef DEBUG
         atomName = JXGetAtomName(display, event->message_type);
         Debug("ClientMessage to unknown window (0x%x): %s",
               event->window, atomName);
         JXFree(atomName);
#endif
   }

}

/** Handle a _NET_MOVERESIZE_WINDOW request. */
void HandleNetMoveResize(const XClientMessageEvent *event, ClientNode *np)
{

   long flags;
   int gravity;
   int deltax, deltay;

   Assert(event);
   Assert(np);

   flags = event->data.l[0] >> 8;
   gravity = event->data.l[0] & 0xFF;
   if(gravity == 0) {
      gravity = np->gravity;
   }
   GetGravityDelta(np, gravity, &deltax, &deltay);

   if(flags & (1 << 2)) {
      const long width = event->data.l[3];
      switch(gravity) {
      case EastGravity:
      case NorthEastGravity:
      case SouthEastGravity:
         /* Right side should not move. */
         np->x -= width - np->width;
         break;
      case WestGravity:
      case NorthWestGravity:
      case SouthWestGravity:
         /* Left side should not move. */
         break;
      case CenterGravity:
         /* Center of the window should not move. */
         np->x -= (width - np->width) / 2;
         break;
      default:
         break;
      }
      np->width = width;
   }
   if(flags & (1 << 3)) {
      const long height = event->data.l[4];
      switch(gravity) {
      case NorthGravity:
      case NorthEastGravity:
      case NorthWestGravity:
         /* Top should not move. */
         break;
      case SouthGravity:
      case SouthEastGravity:
      case SouthWestGravity:
         /* Bottom should not move. */
         np->y -= height - np->height;
         break;
      case CenterGravity:
         /* Center of the window should not move. */
         np->y -= (height - np->height) / 2;
         break;
      default:
         break;
      }
      np->height = height;
   }
   if(flags & (1 << 0)) {
      np->x = event->data.l[1] - deltax;
   }
   if(flags & (1 << 1)) {
      np->y = event->data.l[2] - deltay;
   }

   /* Don't let maximized clients be moved or resized. */
   if(JUNLIKELY(np->state.status & STAT_FULLSCREEN)) {
      SetClientFullScreen(np, 0);
   }
   if(JUNLIKELY(np->state.maxFlags)) {
      MaximizeClient(np, MAX_NONE);
   }

   ConstrainSize(np);
   ResetBorder(np);
   SendConfigureEvent(np);
   RequirePagerUpdate();

}

/** Handle a _NET_WM_MOVERESIZE request. */
void HandleNetWMMoveResize(const XClientMessageEvent *event, ClientNode *np)
{

   long x = event->data.l[0] - np->x;
   long y = event->data.l[1] - np->y;
   const long direction = event->data.l[2];
   int deltax, deltay;

   GetGravityDelta(np, np->gravity, &deltax, &deltay);
   x -= deltax;
   y -= deltay;

   switch(direction) {
   case 0:  /* top-left */
      ResizeClient(np, MC_BORDER | MC_BORDER_N | MC_BORDER_W, x, y);
      break;
   case 1:  /* top */
      ResizeClient(np, MC_BORDER | MC_BORDER_N, x, y);
      break;
   case 2:  /* top-right */
      ResizeClient(np, MC_BORDER | MC_BORDER_N | MC_BORDER_E, x, y);
      break;
   case 3:  /* right */
      ResizeClient(np, MC_BORDER | MC_BORDER_E, x, y);
      break;
   case 4:  /* bottom-right */
      ResizeClient(np, MC_BORDER | MC_BORDER_S | MC_BORDER_E, x, y);
      break;
   case 5:  /* bottom */
      ResizeClient(np, MC_BORDER | MC_BORDER_S, x, y);
      break;
   case 6:  /* bottom-left */
      ResizeClient(np, MC_BORDER | MC_BORDER_S | MC_BORDER_W, x, y);
      break;
   case 7:  /* left */
      ResizeClient(np, MC_BORDER | MC_BORDER_W, x, y);
      break;
   case 8:  /* move */
      MoveClient(np, x, y);
      break;
   case 9:  /* resize-keyboard */
      ResizeClientKeyboard(np, MC_BORDER | MC_BORDER_S | MC_BORDER_E);
      break;
   case 10: /* move-keyboard */
      MoveClientKeyboard(np);
      break;
   case 11: /* cancel */
      if(np->controller) {
         (np->controller)(0);
      }
      break;
   default:
      break;
   }

}

/** Handle a _NET_RESTACK_WINDOW request. */
void HandleNetRestack(const XClientMessageEvent *event, ClientNode *np)
{
   const Window sibling = event->data.l[1];
   const int detail = event->data.l[2];
   RestackClient(np, sibling, detail);
}

/** Handle a _NET_WM_STATE request. */
void HandleNetWMState(const XClientMessageEvent *event, ClientNode *np)
{

   unsigned int x;
   MaxFlags maxFlags;
   char actionStick;
   char actionShade;
   char actionFullScreen;
   char actionMinimize;
   char actionNolist;
   char actionNopager;
   char actionBelow;
   char actionAbove;

   /* Up to two actions to be applied together. */
   maxFlags = MAX_NONE;
   actionStick = 0;
   actionShade = 0;
   actionFullScreen = 0;
   actionMinimize = 0;
   actionNolist = 0;
   actionNopager = 0;
   actionBelow = 0;
   actionAbove = 0;

   for(x = 1; x <= 2; x++) {
      if(event->data.l[x]
         == (long)atoms[ATOM_NET_WM_STATE_STICKY]) {
         actionStick = 1;
      } else if(event->data.l[x]
         == (long)atoms[ATOM_NET_WM_STATE_MAXIMIZED_VERT]) {
         maxFlags |= MAX_VERT;
      } else if(event->data.l[x]
         == (long)atoms[ATOM_NET_WM_STATE_MAXIMIZED_HORZ]) {
         maxFlags |= MAX_HORIZ;
      } else if(event->data.l[x]
         == (long)atoms[ATOM_NET_WM_STATE_SHADED]) {
         actionShade = 1;
      } else if(event->data.l[x]
         == (long)atoms[ATOM_NET_WM_STATE_FULLSCREEN]) {
         actionFullScreen = 1;
      } else if(event->data.l[x]
         == (long)atoms[ATOM_NET_WM_STATE_HIDDEN]) {
         actionMinimize = 1;
      } else if(event->data.l[x]
         == (long)atoms[ATOM_NET_WM_STATE_SKIP_TASKBAR]) {
         actionNolist = 1;
      } else if(event->data.l[x]
         == (long)atoms[ATOM_NET_WM_STATE_SKIP_PAGER]) {
         actionNopager = 1;
      } else if(event->data.l[x]
         == (long)atoms[ATOM_NET_WM_STATE_BELOW]) {
         actionBelow = 1;
      } else if(event->data.l[x]
         == (long)atoms[ATOM_NET_WM_STATE_ABOVE]) {
         actionAbove = 1;
      }
   }

   switch(event->data.l[0]) {
   case 0: /* Remove */
      if(actionStick) {
         SetClientSticky(np, 0);
      }
      if(maxFlags != MAX_NONE && np->state.maxFlags) {
         MaximizeClient(np, np->state.maxFlags & ~maxFlags);
      }
      if(actionShade) {
         UnshadeClient(np);
      }
      if(actionFullScreen) {
         SetClientFullScreen(np, 0);
      }
      if(actionMinimize) {
         RestoreClient(np, 0);
      }
      if(actionNolist && !(np->state.status & STAT_ILIST)) {
         np->state.status &= ~STAT_NOLIST;
         RequireTaskUpdate();
      }
      if(actionNopager && !(np->state.status & STAT_IPAGER)) {
         np->state.status &= ~STAT_NOPAGER;
         RequirePagerUpdate();
      }
      if(actionBelow && np->state.layer == LAYER_BELOW) {
         SetClientLayer(np, np->state.defaultLayer);
      }
      if(actionAbove && np->state.layer == LAYER_ABOVE) {
         SetClientLayer(np, np->state.defaultLayer);
      }
      break;
   case 1: /* Add */
      if(actionStick) {
         SetClientSticky(np, 1);
      }
      if(maxFlags != MAX_NONE) {
         MaximizeClient(np, np->state.maxFlags | maxFlags);
      }
      if(actionShade) {
         ShadeClient(np);
      }
      if(actionFullScreen) {
         SetClientFullScreen(np, 1);
      }
      if(actionMinimize) {
         MinimizeClient(np, 1);
      }
      if(actionNolist && !(np->state.status & STAT_ILIST)) {
         np->state.status |= STAT_NOLIST;
         RequireTaskUpdate();
      }
      if(actionNopager && !(np->state.status & STAT_IPAGER)) {
         np->state.status |= STAT_NOPAGER;
         RequirePagerUpdate();
      }
      if(actionBelow) {
         SetClientLayer(np, LAYER_BELOW);
      }
      if(actionAbove) {
         SetClientLayer(np, LAYER_ABOVE);
      }
      break;
   case 2: /* Toggle */
      if(actionStick) {
         if(np->state.status & STAT_STICKY) {
            SetClientSticky(np, 0);
         } else {
            SetClientSticky(np, 1);
         }
      }
      if(maxFlags) {
         MaximizeClient(np, np->state.maxFlags ^ maxFlags);
      }
      if(actionShade) {
         if(np->state.status & STAT_SHADED) {
            UnshadeClient(np);
         } else {
            ShadeClient(np);
         }
      }
      if(actionFullScreen) {
         if(np->state.status & STAT_FULLSCREEN) {
            SetClientFullScreen(np, 0);
         } else {
            SetClientFullScreen(np, 1);
         }
      }
      if(actionBelow) {
         if(np->state.layer == LAYER_BELOW) {
            SetClientLayer(np, np->state.defaultLayer);
         } else {
            SetClientLayer(np, LAYER_BELOW);
         }
      }
      if(actionAbove) {
         if(np->state.layer == LAYER_ABOVE) {
            SetClientLayer(np, np->state.defaultLayer);
         } else {
            SetClientLayer(np, LAYER_ABOVE);
         }
      }
      /* Note that we don't handle toggling of hidden per EWMH
       * recommendations. */
      if(actionNolist && !(np->state.status & STAT_ILIST)) {
         np->state.status ^= STAT_NOLIST;
         RequireTaskUpdate();
      }
      if(actionNopager && !(np->state.status & STAT_IPAGER)) {
         np->state.status ^= STAT_NOPAGER;
         RequirePagerUpdate();
      }
      break;
   default:
      Debug("bad _NET_WM_STATE action: %ld", event->data.l[0]);
      break;
   }

   /* Update _NET_WM_STATE if needed.
    * The state update is handled elsewhere for the other actions.
    */
   if(actionNolist | actionNopager | actionAbove | actionBelow) {
      WriteState(np);
   }

}

/** Handle a _NET_REQUEST_FRAME_EXTENTS request. */
void HandleFrameExtentsRequest(const XClientMessageEvent *event)
{
   ClientState state;
   state = ReadWindowState(event->window, 0);
   WriteFrameExtents(event->window, &state);
}

/** Handle a motion notify event. */
void HandleMotionNotify(const XMotionEvent *event)
{

   ClientNode *np;
   Cursor cur;

   if(event->is_hint) {
      return;
   }

   np = FindClientByParent(event->window);
   if(np) {
      const MouseContextType context = GetBorderContext(np, event->x, event->y);
      if(np->mouseContext != context) {
         np->mouseContext = context;
         cur = GetFrameCursor(context);
         JXDefineCursor(display, np->parent, cur);
      }
   }

}

/** Handle a shape event. */
#ifdef USE_SHAPE
void HandleShapeEvent(const XShapeEvent *event)
{
   ClientNode *np;
   np = FindClientByWindow(event->window);
   if(np) {
      np->state.status |= STAT_SHAPED;
      ResetBorder(np);
   }
}
#endif /* USE_SHAPE */

/** Handle a colormap event. */
void HandleColormapChange(const XColormapEvent *event)
{
   ClientNode *np;
   if(event->new == True) {
      np = FindClientByWindow(event->window);
      if(np) {
         np->cmap = event->colormap;
         UpdateClientColormap(np);
      }
   }
}

/** Handle a map request. */
void HandleMapRequest(const XMapEvent *event)
{
   ClientNode *np;
   Assert(event);
   if(CheckSwallowMap(event->window)) {
      return;
   }
   np = FindClientByWindow(event->window);
   if(!np) {
      GrabServer();
      np = AddClientWindow(event->window, 0, 1);
      if(np) {
         if(!(np->state.status & STAT_NOFOCUS)) {
            FocusClient(np);
         }
      } else {
         JXMapWindow(display, event->window);
      }
      UngrabServer();
   } else {
      if(!(np->state.status & STAT_MAPPED)) {
         UpdateState(np);
         np->state.status |= STAT_MAPPED;
         XMapWindow(display, np->window);
         if(np->parent != None) {
            XMapWindow(display, np->parent);
         }
         if(!(np->state.status & STAT_STICKY)) {
            np->state.desktop = currentDesktop;
         }
         if(!(np->state.status & STAT_NOFOCUS)) {
            FocusClient(np);
            RaiseClient(np);
         }
         WriteState(np);
         RequireTaskUpdate();
         RequirePagerUpdate();
      }
   }
   RequireRestack();
}

/** Handle an unmap notify event. */
void HandleUnmapNotify(const XUnmapEvent *event)
{
   ClientNode *np;
   XEvent e;

   Assert(event);

   if(event->window != event->event) {
      /* Allow ICCCM synthetic UnmapNotify events through. */
      if (event->event != rootWindow || !event->send_event) {
         return;
      }
   }

   np = FindClientByWindow(event->window);
   if(np) {

      /* Grab the server to prevent the client from destroying the
       * window after we check for a DestroyNotify. */
      GrabServer();

      if(np->controller) {
         (np->controller)(1);
      }

      if(JXCheckTypedWindowEvent(display, np->window, DestroyNotify, &e)) {
         UpdateTime(&e);
         RemoveClient(np);
      } else if((np->state.status & STAT_MAPPED) || event->send_event) {
         if(!(np->state.status & STAT_HIDDEN)) {
            np->state.status &= ~(STAT_MAPPED | STAT_MINIMIZED | STAT_SHADED);
            JXUngrabButton(display, AnyButton, AnyModifier, np->window);
            GravitateClient(np, 1);
            JXReparentWindow(display, np->window, rootWindow, np->x, np->y);
            WriteState(np);
            JXRemoveFromSaveSet(display, np->window);
            RemoveClient(np);
         }
      }
      UngrabServer();

   }
}

/** Handle a destroy notify event. */
char HandleDestroyNotify(const XDestroyWindowEvent *event)
{
   ClientNode *np;
   np = FindClientByWindow(event->window);
   if(np) {
      if(np->controller) {
         (np->controller)(1);
      }
      RemoveClient(np);
      return 1;
   } else {
      return HandleDockDestroy(event->window);
   }
}

/** Update window state information. */
void UpdateState(ClientNode *np)
{
   const char alreadyMapped = (np->state.status & STAT_MAPPED) ? 1 : 0;
   const char active = (np->state.status & STAT_ACTIVE) ? 1 : 0;

   /* Remove from the layer list. */
   if(np->prev != NULL) {
      np->prev->next = np->next;
   } else {
      Assert(nodes[np->state.layer] == np);
      nodes[np->state.layer] = np->next;
   }
   if(np->next != NULL) {
      np->next->prev = np->prev;
   } else {
      Assert(nodeTail[np->state.layer] == np);
      nodeTail[np->state.layer] = np->prev;
   }

   /* Read the state (and new layer). */
   if(np->state.status & STAT_URGENT) {
      UnregisterCallback(SignalUrgent, np);
   }
   np->state = ReadWindowState(np->window, alreadyMapped);
   if(np->state.status & STAT_URGENT) {
      RegisterCallback(URGENCY_DELAY, SignalUrgent, np);
   }

   /* We don't handle mapping the window, so restore its mapped state. */
   if(!alreadyMapped) {
      np->state.status &= ~STAT_MAPPED;
   }

   /* Add to the layer list. */
   np->prev = NULL;
   np->next = nodes[np->state.layer];
   if(np->next == NULL) {
      nodeTail[np->state.layer] = np;
   } else {
      np->next->prev = np;
   }
   nodes[np->state.layer] = np;

   if(active) {
      FocusClient(np);
   }

}

/** Update the last event time. */
void UpdateTime(const XEvent *event)
{
   Time t = CurrentTime;
   Assert(event);
   switch(event->type) {
   case KeyPress:
   case KeyRelease:
      t = event->xkey.time;
      break;
   case ButtonPress:
   case ButtonRelease:
      t = event->xbutton.time;
      break;
   case MotionNotify:
      t = event->xmotion.time;
      break;
   case EnterNotify:
   case LeaveNotify:
      t = event->xcrossing.time;
      break;
   case PropertyNotify:
      t = event->xproperty.time;
      break;
   case SelectionClear:
      t = event->xselectionclear.time;
      break;
   case SelectionRequest:
      t = event->xselectionrequest.time;
      break;
   case SelectionNotify:
      t = event->xselection.time;
      break;
   default:
      break;
   }
   if(t != CurrentTime) {
      if(t > eventTime || t < eventTime - 60000) {
         eventTime = t;
      }
   }
}

/** Register a callback. */
void RegisterCallback(int freq, SignalCallback callback, void *data)
{
   CallbackNode *cp;
   cp = Allocate(sizeof(CallbackNode));
   cp->last.seconds = 0;
   cp->last.ms = 0;
   cp->freq = freq;
   cp->callback = callback;
   cp->data = data;
   cp->next = callbacks;
   callbacks = cp;
}

/** Unregister a callback. */
void UnregisterCallback(SignalCallback callback, void *data)
{
   CallbackNode **cp;
   for(cp = &callbacks; *cp; cp = &(*cp)->next) {
      if((*cp)->callback == callback && (*cp)->data == data) {
         CallbackNode *temp = *cp;
         *cp = (*cp)->next;
         Release(temp);
         return;
      }
   }
   Assert(0);
}

/** Restack clients before waiting for an event. */
void RequireRestack()
{
   restack_pending = 1;
}

/** Update the task bar before waiting for an event. */
void RequireTaskUpdate()
{
   task_update_pending = 1;
}

/** Update the pager before waiting for an event. */
void RequirePagerUpdate()
{
   pager_update_pending = 1;
}
