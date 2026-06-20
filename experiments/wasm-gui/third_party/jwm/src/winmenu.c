/**
 * @file winmenu.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Functions for handling window menus.
 *
 */

#include "jwm.h"
#include "winmenu.h"
#include "client.h"
#include "desktop.h"
#include "move.h"
#include "resize.h"
#include "event.h"
#include "cursor.h"
#include "misc.h"
#include "root.h"
#include "settings.h"

static void CreateWindowLayerMenu(Menu *menu, ClientNode *np);
static void CreateWindowSendToMenu(Menu *menu, ClientNode *np);
static void AddWindowMenuItem(Menu *menu, const char *name,
                              MenuActionType type,
                              ClientNode *np, int value);

/** Show a window menu. */
void ShowWindowMenu(ClientNode *np, int x, int y, char keyboard)
{
   Menu *menu = CreateWindowMenu(np);
   InitializeMenu(menu);
   ShowMenu(menu, RunWindowCommand, x, y, keyboard);
   DestroyMenu(menu);
}

/** Create a new window menu. */
Menu *CreateWindowMenu(ClientNode *np)
{

   Menu *menu;

   menu = CreateMenu();

   /* Note that items are added in reverse order of display. */

   if(!(np->state.status & STAT_WMDIALOG)) {
      AddWindowMenuItem(menu, _("Close"), MA_CLOSE, np, 0);
      if (settings.showKillMenuItem){
          AddWindowMenuItem(menu, _("Kill"), MA_KILL, np, 0);
      }
      AddWindowMenuItem(menu, NULL, MA_NONE, np, 0);
   }

   if(!(np->state.status & (STAT_FULLSCREEN | STAT_MINIMIZED)
        || np->state.maxFlags)) {
      if(np->state.status & (STAT_MAPPED | STAT_SHADED)) {
         if(np->state.border & BORDER_RESIZE) {
            AddWindowMenuItem(menu, _("Resize"), MA_RESIZE, np, 0);
         }
         if(np->state.border & BORDER_MOVE) {
            AddWindowMenuItem(menu, _("Move"), MA_MOVE, np, 0);
         }
      }
   }

   if((np->state.border & BORDER_MIN) && !(np->state.status & STAT_MINIMIZED)) {
      AddWindowMenuItem(menu, _("Minimize"), MA_MINIMIZE, np, 0);
   }

   if(!(np->state.status & STAT_FULLSCREEN)) {
      if(!(np->state.status & STAT_MINIMIZED)) {
         if(np->state.status & STAT_SHADED) {
            AddWindowMenuItem(menu, _("Unshade"), MA_SHADE, np, 0);
         } else if(np->state.border & BORDER_SHADE) {
            AddWindowMenuItem(menu, _("Shade"), MA_SHADE, np, 0);
         }
      }
      if(np->state.border & BORDER_MAX) {
         if(np->state.status & STAT_MINIMIZED
            || !(np->state.maxFlags & MAX_VERT)
            || np->state.maxFlags & MAX_HORIZ) {
            AddWindowMenuItem(menu, _("Maximize-y"), MA_MAXIMIZE_V, np, 0);
         }
         if(np->state.status & STAT_MINIMIZED
            || !(np->state.maxFlags & MAX_HORIZ)
            || np->state.maxFlags & MAX_VERT) {
            AddWindowMenuItem(menu, _("Maximize-x"), MA_MAXIMIZE_H, np, 0);
         }
         if(np->state.status & STAT_MINIMIZED
            || !(np->state.maxFlags & (MAX_VERT | MAX_HORIZ))) {
            AddWindowMenuItem(menu, _("Maximize"), MA_MAXIMIZE, np, 0);
         }
         if(!(np->state.status & STAT_MINIMIZED)) {
            if((np->state.maxFlags & MAX_HORIZ)
               && (np->state.maxFlags & MAX_VERT)) {
               AddWindowMenuItem(menu, _("Restore"), MA_MAXIMIZE, np, 0);
            } else if(np->state.maxFlags & MAX_VERT) {
               AddWindowMenuItem(menu, _("Restore"), MA_MAXIMIZE_V, np, 0);
            } else if(np->state.maxFlags & MAX_HORIZ) {
               AddWindowMenuItem(menu, _("Restore"), MA_MAXIMIZE_H, np, 0);
            }
         }
      }
   }

   if(np->state.status & STAT_MINIMIZED) {
      AddWindowMenuItem(menu, _("Restore"), MA_RESTORE, np, 0);
   }

   if(!(np->state.status & STAT_WMDIALOG)) {
      if(settings.desktopCount > 1) {
         if(np->state.status & STAT_STICKY) {
            AddWindowMenuItem(menu, _("Unstick"), MA_STICK, np, 0);
         } else {
            AddWindowMenuItem(menu, _("Stick"), MA_STICK, np, 0);
         }
      }

      CreateWindowLayerMenu(menu, np);

      if(settings.desktopCount > 1) {
         if(!(np->state.status & STAT_STICKY)) {
            CreateWindowSendToMenu(menu, np);
         }
      }

   }

   return menu;
}

/** Create a window layer submenu. */
void CreateWindowLayerMenu(Menu *menu, ClientNode *np)
{

   Menu *submenu;
   MenuItem *item;

   item = CreateMenuItem(MENU_ITEM_SUBMENU);
   item->name = CopyString(_("Layer"));
   item->action.type = MA_NONE;
   item->next = menu->items;
   menu->items = item;

   submenu = CreateMenu();
   item->submenu = submenu;

   if(np->state.layer == LAYER_ABOVE) {
      AddWindowMenuItem(submenu, _("[Above]"), MA_LAYER, np, LAYER_ABOVE);
   } else {
      AddWindowMenuItem(submenu, _("Above"), MA_LAYER, np, LAYER_ABOVE);
   }
   if(np->state.layer == LAYER_NORMAL) {
      AddWindowMenuItem(submenu, _("[Normal]"), MA_LAYER, np, LAYER_NORMAL);
   } else {
      AddWindowMenuItem(submenu, _("Normal"), MA_LAYER, np, LAYER_NORMAL);
   }
   if(np->state.layer == LAYER_BELOW) {
      AddWindowMenuItem(submenu, _("[Below]"), MA_LAYER, np, LAYER_BELOW);
   } else {
      AddWindowMenuItem(submenu, _("Below"), MA_LAYER, np, LAYER_BELOW);
   }

}

/** Create a send to submenu. */
void CreateWindowSendToMenu(Menu *menu, ClientNode *np)
{

   unsigned int mask;
   unsigned int x;

   mask = 0;
   for(x = 0; x < settings.desktopCount; x++) {
      if(np->state.desktop == x || (np->state.status & STAT_STICKY)) {
         mask |= 1 << x;
      }
   }

   AddWindowMenuItem(menu, _("Send To"), MA_NONE, np, 0);

   /* Now the first item in the menu is for the desktop list. */
   menu->items->submenu = CreateDesktopMenu(mask, np);

}

/** Add an item to the current window menu. */
void AddWindowMenuItem(Menu *menu, const char *name, MenuActionType type,
                       ClientNode *np, int value)
{

   MenuItem *item;

   item = CreateMenuItem(name ? MENU_ITEM_NORMAL : MENU_ITEM_SEPARATOR);
   item->name = CopyString(name);
   item->action.type = type;
   item->action.context = np;
   item->action.value = value;
   item->next = menu->items;
   menu->items = item;

}

/** Select a window for performing an action. */
void ChooseWindow(MenuAction *action)
{

   XEvent event;
   ClientNode *np;

   if(!GrabMouseForChoose()) {
      return;
   }

   for(;;) {

      WaitForEvent(&event);

      if(event.type == ButtonPress) {
         if(event.xbutton.button == Button1) {
            np = FindClient(event.xbutton.subwindow);
            if(np) {
               action->context = np;
               RunWindowCommand(action, event.xbutton.button);
            }
         }
         break;
      } else if(event.type == KeyPress) {
         break;
      }

   }

   JXUngrabPointer(display, CurrentTime);

}

/** Window menu action callback. */
void RunWindowCommand(MenuAction *action, unsigned button)
{
   ClientNode *client = action->context;
   switch(action->type) {
   case MA_STICK:
      if(client->state.status & STAT_STICKY) {
         SetClientSticky(client, 0);
      } else {
         SetClientSticky(client, 1);
      }
      break;
   case MA_MAXIMIZE:
      if((client->state.maxFlags & MAX_HORIZ)
         && (client->state.maxFlags & MAX_VERT)
         && !(client->state.status & STAT_MINIMIZED)) {
         MaximizeClient(client, MAX_NONE);
      } else {
         MaximizeClient(client, MAX_VERT | MAX_HORIZ);
      }
      break;
   case MA_MAXIMIZE_H:
      if((client->state.maxFlags & MAX_HORIZ)
         && !(client->state.maxFlags & MAX_VERT)
         && !(client->state.status & STAT_MINIMIZED)) {
         MaximizeClient(client, MAX_NONE);
      } else {
         MaximizeClient(client, MAX_HORIZ);
      }
      break;
   case MA_MAXIMIZE_V:
      if((client->state.maxFlags & MAX_VERT)
         && !(client->state.maxFlags & MAX_HORIZ)
         && !(client->state.status & STAT_MINIMIZED)) {
         MaximizeClient(client, MAX_NONE);
      } else {
         MaximizeClient(client, MAX_VERT);
      }
      break;
   case MA_MINIMIZE:
      MinimizeClient(client, 1);
      break;
   case MA_RESTORE:
      RestoreClient(client, 1);
      break;
   case MA_CLOSE:
      DeleteClient(client);
      break;
   case MA_SENDTO:
   case MA_DESKTOP:
      SetClientDesktop(client, action->value);
      break;
   case MA_SHADE:
      if(client->state.status & STAT_SHADED) {
         UnshadeClient(client);
      } else {
         ShadeClient(client);
      }
      break;
   case MA_MOVE:
      MoveClientKeyboard(client);
      break;
   case MA_RESIZE:
      ResizeClientKeyboard(client, MC_BORDER | MC_BORDER_S | MC_BORDER_E);
      break;
   case MA_KILL:
      KillClient(client);
      break;
   case MA_LAYER:
      SetClientLayer(client, action->value);
      break;
   default:
      break;
   }

}
