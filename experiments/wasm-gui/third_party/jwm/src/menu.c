/**
 * @file menu.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Menu display and handling functions.
 *
 */

#include "jwm.h"
#include "menu.h"
#include "font.h"
#include "client.h"
#include "icon.h"
#include "cursor.h"
#include "binding.h"
#include "button.h"
#include "event.h"
#include "root.h"
#include "settings.h"
#include "desktop.h"
#include "parse.h"
#include "winmenu.h"
#include "screen.h"
#include "hint.h"
#include "misc.h"
#include "popup.h"

#define BASE_ICON_OFFSET   3
#define MENU_BORDER_SIZE   1

typedef unsigned char MenuSelectionType;
#define MENU_NOSELECTION   0
#define MENU_LEAVE         1
#define MENU_SUBSELECT     2

static char ShowSubmenu(Menu *menu, Menu *parent,
                        RunMenuCommandType runner,
                        int x, int y, char keyboard);

static void PatchMenu(Menu *menu);
static void UnpatchMenu(Menu *menu);
static void MapMenu(Menu *menu, int x, int y, char keyboard);
static void HideMenu(Menu *menu);
static void DrawMenu(Menu *menu);

static char MenuLoop(Menu *menu, RunMenuCommandType runner);
static void MenuCallback(const TimeType *now, int x, int y,
                         Window w, void *data);
static MenuSelectionType UpdateMotion(Menu *menu,
                                      RunMenuCommandType runner,
                                      XEvent *event);

static void UpdateMenu(Menu *menu);
static void DrawMenuItem(Menu *menu, MenuItem *item, int index);
static MenuItem *GetMenuItem(Menu *menu, int index);
static int GetNextMenuIndex(Menu *menu);
static int GetPreviousMenuIndex(Menu *menu);
static int GetMenuIndex(Menu *menu, int index);
static void SetPosition(Menu *tp, int index);
static char IsMenuValid(const Menu *menu);

int menuShown = 0;

/** Allocate an empty menu. */
Menu *CreateMenu()
{
   Menu *menu = Allocate(sizeof(Menu));
   menu->itemHeight = 0;
   menu->items = NULL;
   menu->label = NULL;
   menu->dynamic = NULL;
   menu->timeout_ms = MENU_TIMEOUT_MS;
   return menu;
}

/** Create an empty menu item. */
MenuItem *CreateMenuItem(MenuItemType type)
{
   MenuItem *item = Allocate(sizeof(MenuItem));
   memset(item, 0, sizeof(MenuItem));
   item->type = type;
   return item;
}

/** Initialize a menu. */
void InitializeMenu(Menu *menu)
{

   MenuItem *np;
   int index, temp;
   int userHeight;
   int hasSubmenu;
   char hasIcon;

   menu->textOffset = 0;
   menu->itemCount = 0;

   /* Compute the max size needed */
   hasIcon = 0;
   userHeight = menu->itemHeight;
   if(userHeight < 0) {
      userHeight = 0;
   }
   menu->itemHeight = GetStringHeight(FONT_MENU);
   for(np = menu->items; np; np = np->next) {
      if(np->iconName) {
         np->icon = LoadNamedIcon(np->iconName, 1, 1);
         if(np->icon) {
            hasIcon = 1;
         }
      } else if(np->icon) {
         hasIcon = 1;
      }
      menu->itemCount += 1;
   }
   menu->itemHeight += BASE_ICON_OFFSET * 2;

   if(userHeight) {
      menu->itemHeight = userHeight + BASE_ICON_OFFSET * 2;
   }
   if(hasIcon) {
      menu->textOffset = menu->itemHeight + BASE_ICON_OFFSET * 2;
   }

   menu->width = 5;
   menu->parent = NULL;
   menu->parentOffset = 0;

   /* Make sure the menu is wide enough for a label if it is labeled. */
   if(menu->label) {
      temp = GetStringWidth(FONT_MENU, menu->label);
      if(temp > menu->width) {
         menu->width = temp;
      }
   }

   menu->height = MENU_BORDER_SIZE;
   if(menu->label) {
      menu->height += menu->itemHeight;
   }

   /* Nothing else to do if there is nothing in the menu. */
   if(JUNLIKELY(menu->itemCount == 0)) {
      return;
   }

   menu->offsets = Allocate(sizeof(int) * menu->itemCount);

   hasSubmenu = 0;
   index = 0;
   for(np = menu->items; np; np = np->next) {
      menu->offsets[index++] = menu->height;
      if(np->type == MENU_ITEM_SEPARATOR) {
         menu->height += 6;
      } else {
         menu->height += menu->itemHeight;
      }
      if(np->name) {
         temp = GetStringWidth(FONT_MENU, np->name);
         if(temp > menu->width) {
            menu->width = temp;
         }
      }
      if(hasIcon && !np->icon) {
         np->icon = &emptyIcon;
      }
      if(np->submenu) {
         hasSubmenu = (menu->itemHeight + 3) / 4;
         InitializeMenu(np->submenu);
      }
   }
   menu->width += hasSubmenu + menu->textOffset;
   menu->width += 7 + 2 * MENU_BORDER_SIZE;
   menu->height += MENU_BORDER_SIZE;
   menu->mousex = -1;
   menu->mousey = -1;

}

/** Show a menu. */
char ShowMenu(Menu *menu, RunMenuCommandType runner,
              int x, int y, char keyboard)
{
   /* Don't show the menu if there isn't anything to show. */
   if(JUNLIKELY(!IsMenuValid(menu))) {
      /* Return 1 if there is an invalid menu.
       * This allows empty root menus to be defined to disable
       * scrolling on the root window.
       */
      return menu ? 1 : 0;
   }
   if(JUNLIKELY(shouldExit)) {
      return 0;
   }

   if(x < 0 && y < 0) {
      Window w;
      GetMousePosition(&x, &y, &w);
      x -= menu->itemHeight / 2;
      y -= menu->itemHeight / 2 + menu->offsets[0];
   }

   if(!GrabMouse(rootWindow)) {
      return 0;
   }
   if(JXGrabKeyboard(display, rootWindow, False, GrabModeAsync,
                     GrabModeAsync, CurrentTime) != GrabSuccess) {
      JXUngrabPointer(display, CurrentTime);
      return 0;
   }

   RegisterCallback(settings.popupDelay, MenuCallback, menu);
   ShowSubmenu(menu, NULL, runner, x, y, keyboard);
   UnregisterCallback(MenuCallback, menu);
   UnpatchMenu(menu);

   JXUngrabKeyboard(display, CurrentTime);
   JXUngrabPointer(display, CurrentTime);
   RefocusClient();

   if(shouldReload) {
      ReloadMenu();
   }

   return 1;
}

/** Hide a menu. */
void HideMenu(Menu *menu)
{
   Menu *mp;
   for(mp = menu; mp; mp = mp->parent) {
      JXUnmapWindow(display, mp->window);
   }
}

/** Destroy a menu. */
void DestroyMenu(Menu *menu)
{
   MenuItem *np;
   if(menu) {
      while(menu->items) {
         np = menu->items->next;
         if(menu->items->name) {
            Release(menu->items->name);
         }
         if(menu->items->tooltip) {
            Release(menu->items->tooltip);
         }
         switch(menu->items->action.type & MA_ACTION_MASK) {
         case MA_EXECUTE:
         case MA_EXIT:
         case MA_DYNAMIC:
            if(menu->items->action.str) {
               Release(menu->items->action.str);
            }
            break;
         default:
            break;
         }
         if(menu->items->iconName) {
            Release(menu->items->iconName);
         }
         if(menu->items->submenu) {
            DestroyMenu(menu->items->submenu);
         }
         Release(menu->items);
         menu->items = np;
      }
      if(menu->label) {
         Release(menu->label);
      }
      if(menu->dynamic) {
         Release(menu->dynamic);
      }
      if(menu->offsets) {
         Release(menu->offsets);
      }
      Release(menu);
   }
}

/** Show a submenu. */
char ShowSubmenu(Menu *menu, Menu *parent,
                 RunMenuCommandType runner,
                 int x, int y, char keyboard)
{

   char status;

   PatchMenu(menu);
   menu->parent = parent;
   MapMenu(menu, x, y, keyboard);

   menuShown += 1;
   status = MenuLoop(menu, runner);
   menuShown -= 1;

   JXDestroyWindow(display, menu->window);
   JXFreePixmap(display, menu->pixmap);

   return status;

}

/** Prepare a menu to be shown. */
void PatchMenu(Menu *menu)
{
   MenuItem *item;
   for(item = menu->items; item; item = item->next) {
      Menu *submenu = NULL;
      switch(item->action.type & MA_ACTION_MASK) {
      case MA_DESKTOP_MENU:
         submenu = CreateDesktopMenu(1 << currentDesktop,
                                     item->action.context);
         break;
      case MA_SENDTO_MENU:
         submenu = CreateSendtoMenu(
            item->action.type & ~MA_ACTION_MASK,
            item->action.context);
         break;
      case MA_WINDOW_MENU:
         submenu = CreateWindowMenu(item->action.context);
         break;
      case MA_DYNAMIC:
         if(!item->submenu) {
            submenu = ParseDynamicMenu(
               item->action.timeout_ms,
               item->action.str);
            if(JLIKELY(submenu)) {
               submenu->itemHeight = item->action.value;
            }
         }
         break;
      default:
         break;
      }
      if(submenu) {
         InitializeMenu(submenu);
         item->submenu = submenu;
      }
   }
}

/** Remove temporary items from a menu. */
void UnpatchMenu(Menu *menu)
{
   MenuItem *item;
   for(item = menu->items; item; item = item->next) {
      if(item->submenu) {
         UnpatchMenu(item->submenu);
         switch(item->action.type & MA_ACTION_MASK) {
         case MA_DESKTOP_MENU:
         case MA_SENDTO_MENU:
         case MA_WINDOW_MENU:
         case MA_DYNAMIC:
            DestroyMenu(item->submenu);
            item->submenu = NULL;
            break;
         default:
            break;
         }
      }
   }
}

/** Menu process loop.
 * Returns 0 if no selection was made or 1 if a selection was made.
 */
char MenuLoop(Menu *menu, RunMenuCommandType runner)
{

   XEvent event;
   MenuItem *ip;
   Window pressw;
   int pressx, pressy;
   char hadMotion;

   hadMotion = 0;

   GetMousePosition(&pressx, &pressy, &pressw);

   for(;;) {

      WaitForEvent(&event);

      switch(event.type) {
      case Expose:
         if(event.xexpose.count == 0) {
            Menu *mp = menu;
            while(mp) {
               if(mp->window == event.xexpose.window) {
                  DrawMenu(mp);
                  break;
               }
               mp = mp->parent;
            }
         }
         break;

      case ButtonPress:

         pressx = -100;
         pressy = -100;

      case KeyPress:
      case MotionNotify:
         hadMotion = 1;
         switch(UpdateMotion(menu, runner, &event)) {
         case MENU_NOSELECTION: /* no selection */
            break;
         case MENU_LEAVE: /* mouse left the menu */
            JXPutBackEvent(display, &event);
            return 0;
         case MENU_SUBSELECT: /* selection made */
            return 1;
         }
         break;

      case ButtonRelease:

         if(event.xbutton.button == Button4) {
            break;
         }
         if(event.xbutton.button == Button5) {
            break;
         }
         if(!hadMotion) {
            break;
         }
         if(abs(event.xbutton.x_root - pressx) < settings.doubleClickDelta) {
            if(abs(event.xbutton.y_root - pressy) < settings.doubleClickDelta) {
               break;
            }
         }

         if(menu->parent && menu->currentIndex < 0) {
            ip = GetMenuItem(menu->parent, menu->parent->currentIndex);
         } else {
            ip = GetMenuItem(menu, menu->currentIndex);
         }
         if(ip != NULL) {
            if(ip->type == MENU_ITEM_NORMAL) {
               HideMenu(menu);
               (runner)(&ip->action, event.xbutton.button);
            } else if(ip->type == MENU_ITEM_SUBMENU) {
               const Menu *parent = menu->parent;
               if(event.xbutton.x >= menu->x &&
                  event.xbutton.x < menu->x + menu->width &&
                  event.xbutton.y >= menu->y &&
                  event.xbutton.y < menu->y + menu->height) {
                  break;
               } else if(parent &&
                         event.xbutton.x >= parent->x &&
                         event.xbutton.x < parent->x + parent->width &&
                         event.xbutton.y >= parent->y &&
                         event.xbutton.y < parent->y + parent->height) {
                  break;
               }
            }
         }
         return 1;
      default:
         break;
      }

   }
}

/** Signal for showing popups. */
void MenuCallback(const TimeType *now, int x, int y, Window w, void *data)
{
   Menu *menu = data;
   MenuItem *item;
   int i;

   /* Check if the mouse moved (and reset if it did). */
   if(   abs(menu->mousex - x) > settings.doubleClickDelta
      || abs(menu->mousey - y) > settings.doubleClickDelta) {
      menu->mousex = x;
      menu->mousey = y;
      menu->lastTime = *now;
      return;
   }

   /* See if enough time has passed. */
   const unsigned long diff = GetTimeDifference(now, &menu->lastTime);
   if(diff < settings.popupDelay) {
      return;
   }

   /* Locate the active menu item. */
   while(menu) {
      if(x > menu->x && x < menu->x + menu->width) {
         if(y > menu->y && y < menu->y + menu->height) {
            break;
         }
      }
      if(menu->currentIndex < 0) {
         return;
      }
      item = menu->items;
      for(i = 0; i < menu->currentIndex; i++) {
         item = item->next;
      }
      if(item->type != MENU_ITEM_SUBMENU) {
         return;
      }
      menu = item->submenu;
   }
   if(!menu) {
      return;
   }
   if(menu->currentIndex < 0) {
      return;
   }
   item = menu->items;
   for(i = 0; i < menu->currentIndex; i++) {
      item = item->next;
   }
   if(item->tooltip) {
      ShowPopup(x, y, item->tooltip, POPUP_MENU);
   }

}

/** Create and map a menu. */
void MapMenu(Menu *menu, int x, int y, char keyboard)
{
   XSetWindowAttributes attr;
   unsigned long attrMask;
   int temp;

   if(menu->parent) {
      menu->screen = menu->parent->screen;
   } else {
      menu->screen = GetCurrentScreen(x, y);
   }
   if(x + menu->width > menu->screen->x + menu->screen->width) {
      if(menu->parent) {
         x = menu->parent->x - menu->width;
      } else {
         x = menu->screen->x + menu->screen->width - menu->width;
      }
   }
   temp = y;
   if(y + menu->height > menu->screen->y + menu->screen->height) {
      y = menu->screen->y + menu->screen->height - menu->height;
   }
   if(y < 0) {
      y = 0;
   }

   menu->x = x;
   menu->y = y;
   menu->parentOffset = temp - y;

   attrMask = 0;

   attrMask |= CWEventMask;
   attr.event_mask = ExposureMask;

   attrMask |= CWBackPixel;
   attr.background_pixel = colors[COLOR_MENU_BG];

   attrMask |= CWSaveUnder;
   attr.save_under = True;

   menu->window = JXCreateWindow(display, rootWindow, x, y,
                                 menu->width, menu->height, 0,
                                 CopyFromParent, InputOutput,
                                 CopyFromParent, attrMask, &attr);
   SetAtomAtom(menu->window, ATOM_NET_WM_WINDOW_TYPE,
               ATOM_NET_WM_WINDOW_TYPE_MENU);
   menu->pixmap = JXCreatePixmap(display, menu->window,
                                 menu->width, menu->height, rootDepth);

   if(settings.menuOpacity < UINT_MAX) {
      SetCardinalAtom(menu->window, ATOM_NET_WM_WINDOW_OPACITY,
                      settings.menuOpacity);
   }

   JXMapRaised(display, menu->window); 

   if(keyboard && menu->itemCount != 0) {
      const int y = menu->offsets[0] + menu->itemHeight / 2;
      menu->lastIndex = 0;
      menu->currentIndex = 0;
      MoveMouse(menu->window, menu->itemHeight / 2, y);
   } else {
      menu->lastIndex = -1;
      menu->currentIndex = -1;
   }

}

/** Draw a menu. */
void DrawMenu(Menu *menu)
{

   MenuItem *np;
   int x;

   JXSetForeground(display, rootGC, colors[COLOR_MENU_BG]);
   JXFillRectangle(display, menu->pixmap, rootGC, 0, 0,
                   menu->width, menu->height);

   if(settings.menuDecorations == DECO_MOTIF) {
      JXSetForeground(display, rootGC, colors[COLOR_MENU_UP]);
      JXDrawLine(display, menu->pixmap, rootGC,
                 0, 0, menu->width, 0);
      JXDrawLine(display, menu->pixmap, rootGC,
                 0, 0, 0, menu->height);

      JXSetForeground(display, rootGC, colors[COLOR_MENU_DOWN]);
      JXDrawLine(display, menu->pixmap, rootGC,
                 0, menu->height - 1, menu->width, menu->height - 1);
      JXDrawLine(display, menu->pixmap, rootGC,
                 menu->width - 1, 0, menu->width - 1, menu->height);
   } else {
      JXSetForeground(display, rootGC, colors[COLOR_MENU_DOWN]);
      JXDrawRectangle(display, menu->pixmap, rootGC,
                      0, 0, menu->width - 1, menu->height - 1);
   }

   if(menu->label) {
      DrawMenuItem(menu, NULL, -1);
   }

   x = 0;
   for(np = menu->items; np; np = np->next) {
      DrawMenuItem(menu, np, x);
      ++x;
   }
   JXCopyArea(display, menu->pixmap, menu->window, rootGC,
              0, 0, menu->width, menu->height, 0, 0);

}

/** Determine the action to take given an event. */
MenuSelectionType UpdateMotion(Menu *menu,
                               RunMenuCommandType runner,
                               XEvent *event)
{
   MenuItem *ip;
   Menu *tp;
   Window subwindow;
   int x, y;

   if(event->type == MotionNotify) {

      SetMousePosition(event->xmotion.x_root, event->xmotion.y_root,
                       event->xmotion.window);
      DiscardMotionEvents(event, menu->window);

      x = event->xmotion.x_root - menu->x;
      y = event->xmotion.y_root - menu->y;
      subwindow = event->xmotion.subwindow;

   } else if(event->type == ButtonPress) {

      if(menu->currentIndex >= 0 || !menu->parent) {
         tp = menu;
      } else {
         tp = menu->parent;
      }

      y = -1;
      if(event->xbutton.button == Button4) {
         y = GetPreviousMenuIndex(tp);
      } else if(event->xbutton.button == Button5) {
         y = GetNextMenuIndex(tp);
      }

      if(y >= 0) {
         SetPosition(tp, y);
      }

      return MENU_NOSELECTION;

   } else if(event->type == KeyPress) {
      ActionType action;

      if(menu->currentIndex >= 0 || !menu->parent) {
         tp = menu;
      } else {
         tp = menu->parent;
      }

      y = -1;
      action = GetKey(MC_NONE, event->xkey.state, event->xkey.keycode);
      switch(action.action) {
      case ACTION_UP:
         y = GetPreviousMenuIndex(tp);
         break;
      case ACTION_DOWN:
         y = GetNextMenuIndex(tp);
         break;
      case ACTION_RIGHT:
         tp = menu;
         y = 0;
         break;
      case ACTION_LEFT:
         if(tp->parent) {
            tp = tp->parent;
            if(tp->currentIndex >= 0) {
               y = tp->currentIndex;
            } else {
               y = 0;
            }
         }
         break;
      case ACTION_ESC:
         return MENU_SUBSELECT;
      case ACTION_ENTER:
         ip = GetMenuItem(tp, tp->currentIndex);
         if(ip != NULL) {
            HideMenu(menu);
            (runner)(&ip->action, 0);
         }
         return MENU_SUBSELECT;
      default:
         break;
      }

      if(y >= 0) {
         SetPosition(tp, y);
      }

      return MENU_NOSELECTION;

   } else {
      Debug("invalid event type in menu.c:UpdateMotion");
      return MENU_SUBSELECT;
   }

   /* Update the selection on the current menu */
   if(x > 0 && y > 0 && x < menu->width && y < menu->height) {
      menu->currentIndex = GetMenuIndex(menu, y);
   } else if(menu->parent && subwindow != menu->parent->window) {

      /* Leave if over a menu window. */
      for(tp = menu->parent->parent; tp; tp = tp->parent) {
         if(tp->window == subwindow) {
            return MENU_LEAVE;
         }
      }
      menu->currentIndex = -1;

   } else {

      /* Leave if over the parent, but not on this selection. */
      tp = menu->parent;
      if(tp && subwindow == tp->window) {
         if(y < menu->parentOffset
            || y > tp->itemHeight + menu->parentOffset) {
            return MENU_LEAVE;
         }
      }

      menu->currentIndex = -1;

   }

   /* Move the menu if needed. */
   if(menu->height > menu->screen->height && menu->currentIndex >= 0) {

      /* If near the top, shift down. */
      if(y + menu->y <= 0) {
         if(menu->currentIndex > 0) {
            menu->currentIndex -= 1;
            SetPosition(menu, menu->currentIndex);
         }
      }

      /* If near the bottom, shift up. */
      if(y + menu->y + menu->itemHeight / 2
            >= menu->screen->y + menu->screen->height) {
         if(menu->currentIndex + 1 < menu->itemCount) {
            menu->currentIndex += 1;
            SetPosition(menu, menu->currentIndex);
         }
      }

   }

   if(menu->lastIndex != menu->currentIndex) {
      UpdateMenu(menu);
      menu->lastIndex = menu->currentIndex;
   }

   /* If the selected item is a submenu, show it. */
   ip = GetMenuItem(menu, menu->currentIndex);
   if(ip && IsMenuValid(ip->submenu)) {
      const int x = menu->x + menu->width
                  - (settings.menuDecorations == DECO_MOTIF ? 0 : 1);
      const int y = menu->y + menu->offsets[menu->currentIndex] - 1;
      if(ShowSubmenu(ip->submenu, menu, runner, x, y, 0)) {

         /* Item selected; destroy the menu tree. */
         return MENU_SUBSELECT;

      } else {

         /* No selection made. */
         UpdateMenu(menu);

      }
   }

   return MENU_NOSELECTION;

}

/** Update the menu selection. */
void UpdateMenu(Menu *menu)
{

   MenuItem *ip;

   /* Clear the old selection. */
   ip = GetMenuItem(menu, menu->lastIndex);
   DrawMenuItem(menu, ip, menu->lastIndex);

   /* Highlight the new selection. */
   ip = GetMenuItem(menu, menu->currentIndex);
   if(ip != NULL) {
      DrawMenuItem(menu, ip, menu->currentIndex);
   }

   JXCopyArea(display, menu->pixmap, menu->window, rootGC,
              0, 0, menu->width, menu->height, 0, 0);

}

/** Draw a menu item. */
void DrawMenuItem(Menu *menu, MenuItem *item, int index)
{

   ButtonNode button;

   Assert(menu);

   if(!item) {
      if(index == -1 && menu->label) {
         ResetButton(&button, menu->pixmap);
         button.x = MENU_BORDER_SIZE;
         button.y = MENU_BORDER_SIZE;
         button.width = menu->width - MENU_BORDER_SIZE * 2;
         button.height = menu->itemHeight - 1;
         button.font = FONT_MENU;
         button.type = BUTTON_LABEL;
         button.text = menu->label;
         button.alignment = ALIGN_CENTER;
         button.labelPos = LABEL_POSITION_RIGHT;
         DrawButton(&button);
      }
      return;
   }

   if(item->type != MENU_ITEM_SEPARATOR) {
      ColorType fg;

      ResetButton(&button, menu->pixmap);
      if(menu->currentIndex == index) {
         button.type = BUTTON_MENU_ACTIVE;
         fg = COLOR_MENU_ACTIVE_FG;
      } else {
         button.type = BUTTON_LABEL;
         fg = COLOR_MENU_FG;
      }

      button.x = MENU_BORDER_SIZE;
      button.y = menu->offsets[index];
      button.font = FONT_MENU;
      button.width = menu->width - MENU_BORDER_SIZE * 2;
      button.height = menu->itemHeight;
      button.text = item->name;
      button.icon = item->icon;
      DrawButton(&button);

      if(item->submenu) {

         const int asize = (menu->itemHeight + 7) / 8;
         const int y = menu->offsets[index] + (menu->itemHeight + 1) / 2;
         int x = menu->width - 2 * asize - 1;
         int i;

         JXSetForeground(display, rootGC, colors[fg]);
         for(i = 0; i < asize; i++) {
            const int y1 = y - asize + i;
            const int y2 = y + asize - i;
            JXDrawLine(display, menu->pixmap, rootGC, x, y1, x, y2);
            x += 1;
         }
         JXDrawPoint(display, menu->pixmap, rootGC, x, y);

      }

   } else {
      if(settings.menuDecorations == DECO_MOTIF) {
         JXSetForeground(display, rootGC, colors[COLOR_MENU_DOWN]);
         JXDrawLine(display, menu->pixmap, rootGC, 4,
                    menu->offsets[index] + 2, menu->width - 6,
                    menu->offsets[index] + 2);
         JXSetForeground(display, rootGC, colors[COLOR_MENU_UP]);
         JXDrawLine(display, menu->pixmap, rootGC, 4,
                    menu->offsets[index] + 3, menu->width - 6,
                    menu->offsets[index] + 3);
      } else {
         JXSetForeground(display, rootGC, colors[COLOR_MENU_FG]);
         JXDrawLine(display, menu->pixmap, rootGC, 4,
                    menu->offsets[index] + 2, menu->width - 6,
                    menu->offsets[index] + 2);
      }
   }

}

/** Get the next item in the menu. */
int GetNextMenuIndex(Menu *menu)
{
   MenuItem *item;
   int x;

   /* Move to the next non-separator in the menu. */
   for(x = menu->currentIndex + 1; x < menu->itemCount; x++) {
      item = GetMenuItem(menu, x);
      if(item->type != MENU_ITEM_SEPARATOR) {
         return x;
      }
   }

   /* Wrap around. */
   for(x = 0; x < menu->currentIndex; x++) {
      item = GetMenuItem(menu, x);
      if(item->type != MENU_ITEM_SEPARATOR) {
         return x;
      }
   }

   /* Nothing in the menu, stay at the current location. */
   return menu->currentIndex;
}

/** Get the previous item in the menu. */
int GetPreviousMenuIndex(Menu *menu)
{
   MenuItem *item;
   int x;

   /* Move to the previous non-separator in the menu. */
   for(x = menu->currentIndex - 1; x >= 0; x--) {
      item = GetMenuItem(menu, x);
      if(item->type != MENU_ITEM_SEPARATOR) {
         return x;
      }
   }

   /* Wrap around. */
   for(x = menu->itemCount - 1; x > menu->currentIndex; x--) {
      item = GetMenuItem(menu, x);
      if(item->type != MENU_ITEM_SEPARATOR) {
         return x;
      }
   }

   /* Nothing in the menu. */
   return menu->currentIndex;
}

/** Get the item in the menu given a y-coordinate. */
int GetMenuIndex(Menu *menu, int y)
{

   int x;

   if(y < menu->offsets[0]) {
      return -1;
   }
   for(x = 0; x < menu->itemCount - 1; x++) {
      if(y >= menu->offsets[x] && y < menu->offsets[x + 1]) {
         return x;
      }
   }
   return x;

}

/** Get the menu item associated with an index. */
MenuItem *GetMenuItem(Menu *menu, int index)
{

   MenuItem *ip;

   if(index >= 0) {
      for(ip = menu->items; ip; ip = ip->next) {
         if(!index) {
            return ip;
         }
         --index;
      }
   } else {
      ip = NULL;
   }

   return ip;

}

/** Set the active menu item. */
void SetPosition(Menu *tp, int index)
{
   int y = tp->offsets[index] + tp->itemHeight / 2;
   if(tp->height > tp->screen->height) {

      int updated = 0;
      while(y + tp->y < tp->itemHeight / 2) {
         tp->y += tp->itemHeight;
         updated = tp->itemHeight;
      }
      while(y + tp->y >= tp->screen->y + tp->screen->height) {
         tp->y -= tp->itemHeight;
         updated = -tp->itemHeight;
      }
      if(updated) {
         JXMoveWindow(display, tp->window, tp->x, tp->y);
         y += updated;
      }

   }

   /* We need to do this twice so the event gets registered
    * on the submenu if one exists. */
   MoveMouse(tp->window, tp->itemHeight / 2, y);
   MoveMouse(tp->window, tp->itemHeight / 2, y);

}

/** Determine if a menu is valid (and can be shown). */
char IsMenuValid(const Menu *menu)
{
   if(menu) {
      MenuItem *ip;
      for(ip = menu->items; ip; ip = ip->next) {
         if(ip->type != MENU_ITEM_SEPARATOR) {
            return 1;
         }
      }
   }
   return 0;
}

