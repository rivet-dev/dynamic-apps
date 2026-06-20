/**
 * @file taskbar.c
 * @author Joe Wingbermuehle
 * @date 2005-2006
 *
 * @brief Task list tray component.
 *
 */

#include "jwm.h"
#include "taskbar.h"
#include "tray.h"
#include "timing.h"
#include "main.h"
#include "client.h"
#include "clientlist.h"
#include "color.h"
#include "popup.h"
#include "button.h"
#include "cursor.h"
#include "icon.h"
#include "error.h"
#include "winmenu.h"
#include "screen.h"
#include "settings.h"
#include "event.h"
#include "misc.h"
#include "desktop.h"

typedef struct TaskBarType {

   TrayComponentType *cp;
   struct TaskBarType *next;

   int maxItemWidth;
   int userHeight;
   int itemHeight;
   int itemWidth;
   LayoutType layout;
   char labeled;
   LabelPosition labelPos;

   Pixmap buffer;

   TimeType mouseTime;
   int mousex, mousey;

} TaskBarType;

typedef struct ClientEntry {
   ClientNode *client;
   struct ClientEntry *next;
   struct ClientEntry *prev;
} ClientEntry;

typedef struct TaskEntry {
   ClientEntry *clients;
   struct TaskEntry *next;
   struct TaskEntry *prev;
} TaskEntry;

static TaskBarType *bars;
static TaskEntry *taskEntries;
static TaskEntry *taskEntriesTail;

static unsigned TallyVisibleItems(void);
static void ComputeItemSize(TaskBarType *tp);
static char ShouldShowEntry(const TaskEntry *tp);
static char ShouldFocusEntry(const TaskEntry *tp);
static TaskEntry *GetEntry(TaskBarType *bar, int x, int y);
static void Render(const TaskBarType *bp);
static void ShowClientList(TaskBarType *bar, TaskEntry *tp);
static void RunTaskBarCommand(MenuAction *action, unsigned button);

static void SetSize(TrayComponentType *cp, int width, int height);
static void Create(TrayComponentType *cp);
static void Resize(TrayComponentType *cp);
static void ProcessTaskButtonEvent(TrayComponentType *cp,
                                   int x, int y, int mask);
static void MinimizeGroup(const TaskEntry *tp);
static void FocusGroup(const TaskEntry *tp);
static char IsGroupOnTop(const TaskEntry *entry);
static void ProcessTaskMotionEvent(TrayComponentType *cp,
                                   int x, int y, int mask);
static void SignalTaskbar(const TimeType *now, int x, int y, Window w,
                          void *data);

/** Initialize task bar data. */
void InitializeTaskBar(void)
{
   bars = NULL;
   taskEntries = NULL;
   taskEntriesTail = NULL;
}

/** Shutdown the task bar. */
void ShutdownTaskBar(void)
{
   TaskBarType *bp;
   for(bp = bars; bp; bp = bp->next) {
      JXFreePixmap(display, bp->buffer);
   }
}

/** Destroy task bar data. */
void DestroyTaskBar(void)
{
   TaskBarType *bp;
   while(bars) {
      bp = bars->next;
      UnregisterCallback(SignalTaskbar, bars);
      Release(bars);
      bars = bp;
   }
}

/** Create a new task bar tray component. */
TrayComponentType *CreateTaskBar()
{

   TrayComponentType *cp;
   TaskBarType *tp;

   tp = Allocate(sizeof(TaskBarType));
   tp->next = bars;
   bars = tp;
   tp->itemHeight = 0;
   tp->itemWidth = 0;
   tp->userHeight = 0;
   tp->maxItemWidth = 0;
   tp->layout = LAYOUT_HORIZONTAL;
   tp->labeled = 1;
   tp->labelPos = LABEL_POSITION_RIGHT;
   tp->mousex = -settings.doubleClickDelta;
   tp->mousey = -settings.doubleClickDelta;
   tp->mouseTime.seconds = 0;
   tp->mouseTime.ms = 0;

   cp = CreateTrayComponent();
   cp->object = tp;
   tp->cp = cp;

   cp->SetSize = SetSize;
   cp->Create = Create;
   cp->Resize = Resize;
   cp->ProcessButtonPress = ProcessTaskButtonEvent;
   cp->ProcessMotionEvent = ProcessTaskMotionEvent;

   RegisterCallback(settings.popupDelay / 2, SignalTaskbar, tp);

   return cp;

}

/** Set the size of a task bar tray component. */
void SetSize(TrayComponentType *cp, int width, int height)
{
   TaskBarType *tp = (TaskBarType*)cp->object;
   if(width == 0) {
      tp->layout = LAYOUT_HORIZONTAL;
   } else if(height == 0) {
      tp->layout = LAYOUT_VERTICAL;
   } else if(width > height) {
      tp->layout = LAYOUT_HORIZONTAL;
   } else {
      tp->layout = LAYOUT_VERTICAL;
   }
}

/** Initialize a task bar tray component. */
void Create(TrayComponentType *cp)
{
   TaskBarType *tp = (TaskBarType*)cp->object;
   cp->pixmap = JXCreatePixmap(display, rootWindow, cp->width, cp->height,
                               rootDepth);
   tp->buffer = cp->pixmap;
   ClearTrayDrawable(cp);
}

/** Resize a task bar tray component. */
void Resize(TrayComponentType *cp)
{
   TaskBarType *tp = (TaskBarType*)cp->object;
   if(tp->buffer != None) {
      JXFreePixmap(display, tp->buffer);
   }
   cp->pixmap = JXCreatePixmap(display, rootWindow, cp->width, cp->height,
                               rootDepth);
   tp->buffer = cp->pixmap;
   ClearTrayDrawable(cp);
}

/** Count the number of items that should be shown in the task bar. */
unsigned TallyVisibleItems(void)
{
   TaskEntry *ep;
   unsigned count = 0;
   for(ep = taskEntries; ep; ep = ep->next) {
      if(ShouldShowEntry(ep)) {
         count += 1;
      }
   }
   return count;
}

/** Determine the size of items in the task bar. */
void ComputeItemSize(TaskBarType *tp)
{
   TrayComponentType *cp = tp->cp;

   if(tp->layout == LAYOUT_VERTICAL) {
      if(tp->labelPos > LABEL_POSITION_RIGHT) {
         unsigned itemCount = TallyVisibleItems();
         if(itemCount == 0) {
            return;
         }

         tp->itemWidth = cp->width;
         tp->itemHeight = Max(1, cp->height / itemCount);

         if(!tp->labeled) {
            tp->itemHeight = Min(tp->itemWidth, tp->itemHeight);
         } else {
            tp->itemHeight = Min(tp->itemWidth + GetStringHeight(FONT_TASKLIST), tp->itemHeight);
         }

         if(tp->maxItemWidth > 0) {
            tp->itemHeight = Min(tp->maxItemWidth, tp->itemHeight);
         }
      } else {
         tp->itemHeight = tp->userHeight > 0 ? tp->userHeight : GetStringHeight(FONT_TASKLIST) + 12;
         tp->itemWidth = cp->width;
      }
   } else {
      unsigned itemCount = TallyVisibleItems();
      if(itemCount == 0) {
         return;
      }

      tp->itemHeight = cp->height;
      tp->itemWidth = Max(1, cp->width / itemCount);

      if(!tp->labeled) {
         tp->itemWidth = Min(tp->itemHeight, tp->itemWidth);
      }

      if(tp->maxItemWidth > 0) {
         tp->itemWidth = Min(tp->maxItemWidth, tp->itemWidth);
      }
   }
}

/** Check if all clients in this grou are on the top of their layer. */
char IsGroupOnTop(const TaskEntry *entry)
{
   ClientEntry *cp;
   int layer;

   for(layer = FIRST_LAYER; layer <= LAST_LAYER; layer++) {
      ClientNode *np;
      char foundOther = 0;
      for(np = nodes[layer]; np; np = np->next) {
         char found = 0;
         if(!IsClientOnCurrentDesktop(np)) {
            continue;
         }
         for(cp = entry->clients; cp; cp = cp->next) {
            if(np == cp->client) {
               if(foundOther) {
                  return 0;
               }
               found = 1;
               break;
            }
         }
         foundOther = !found;
      }
   }
   return 1;
}

/** Process a task list button event. */
void ProcessTaskButtonEvent(TrayComponentType *cp, int x, int y, int mask)
{

   TaskBarType *bar = (TaskBarType*)cp->object;
   TaskEntry *entry = GetEntry(bar, x, y);

   if(entry) {
      ClientEntry *cp;
      ClientNode *focused = NULL;
      char hasActive = 0;
      char allTop;

      switch(mask) {
      case Button1:  /* Raise or minimize items in this group. */

         allTop = IsGroupOnTop(entry);
         if(allTop) {
            for(cp = entry->clients; cp; cp = cp->next) {
               int layer;
               if(cp->client->state.status & STAT_MINIMIZED) {
                  continue;
               } else if(!ShouldFocus(cp->client, 0)) {
                  continue;
               }
               for(layer = LAST_LAYER; layer >= FIRST_LAYER; layer--) {
                  ClientNode *np;
                  for(np = nodes[layer]; np; np = np->next) {
                     if(np->state.status & STAT_MINIMIZED) {
                        continue;
                     } else if(!ShouldFocus(np, 0)) {
                        continue;
                     }
                     if(np == cp->client) {
                        const char isActive = (np->state.status & STAT_ACTIVE)
                                            && IsClientOnCurrentDesktop(np);
                        if(isActive) {
                           focused = np;
                        }
                        if(!(cp->client->state.status
                              & (STAT_CANFOCUS | STAT_TAKEFOCUS))
                           || isActive) {
                           hasActive = 1;
                        }
                     }
                     if(hasActive) {
                        goto FoundActiveAndTop;
                     }
                  }
               }
            }
         }
FoundActiveAndTop:
         if(hasActive && allTop) {
            /* The client is already active.
             * Here we switch desktops if the client is on another
             * desktop too. Otherwise we minimize the client.
             */
            ClientNode *nextClient = NULL;
            int i;

            /* Try to find a client on a different desktop. */
            for(i = 0; i < settings.desktopCount - 1; i++) {
               const int target = (currentDesktop + i + 1)
                                % settings.desktopCount;
               for(cp = entry->clients; cp; cp = cp->next) {
                  ClientNode *np = cp->client;
                  if(!ShouldFocus(np, 0)) {
                     continue;
                  } else if(np->state.status & STAT_STICKY) {
                     continue;
                  } else if(np->state.desktop == target) {
                     if(!nextClient || np->state.status & STAT_ACTIVE) {
                        nextClient = np;
                     }
                  }
               }
               if(nextClient) {
                  break;
               }
            }
            /* Focus the next client or minimize the current group. */
            if(nextClient) {
               ChangeDesktop(nextClient->state.desktop);
               RestoreClient(nextClient, 1);
            } else {
               MinimizeGroup(entry);
            }
         } else {
            /* The group was not currently on top, raise the group. */
            FocusGroup(entry);
            if(focused) {
               /* If the group already contained the active client,
                * ensure that the same client remains active.
                */
               FocusClient(focused);
            }
         }
         break;
      case Button2:
         for(cp = entry->clients; cp; cp = cp->next) {
      	    DeleteClient(cp->client);
         }
         break;
      case Button3:
         ShowClientList(bar, entry);
         break;
      case Button4:
         FocusPrevious();
         break;
      case Button5:
         FocusNext();
         break;
      default:
         break;
      }
   }

}

/** Minimize all clients in a group. */
void MinimizeGroup(const TaskEntry *tp)
{
   ClientEntry *cp;
   for(cp = tp->clients; cp; cp = cp->next) {
      if(ShouldFocus(cp->client, 1) && (cp->client->state.border & BORDER_MIN)) {
         MinimizeClient(cp->client, 0);
      }
   }
}

/** Raise all clients in a group and focus the top-most. */
void FocusGroup(const TaskEntry *tp)
{
   const char *className = tp->clients->client->className;
   ClientNode **toRestore;
   const ClientEntry *cp;
   unsigned restoreCount;
   int i;
   char shouldSwitch;

   /* If there is no class name, then there will only be one client. */
   if(!className || !settings.groupTasks) {
      if(!(tp->clients->client->state.status & STAT_STICKY)) {
         ChangeDesktop(tp->clients->client->state.desktop);
      }
      RestoreClient(tp->clients->client, 1);
      FocusClient(tp->clients->client);
      return;
   }

   /* If there is a client in the group on this desktop,
    * then we remain on the same desktop. */
   shouldSwitch = 1;
   for(cp = tp->clients; cp; cp = cp->next) {
      if(IsClientOnCurrentDesktop(cp->client)) {
         shouldSwitch = 0;
         break;
      }
   }

   /* Switch to the desktop of the top-most client in the group. */
   if(shouldSwitch) {
      for(i = 0; i < LAYER_COUNT; i++) {
         ClientNode *np;
         for(np = nodes[i]; np; np = np->next) {
            if(np->className && !strcmp(np->className, className)) {
               if(ShouldFocus(np, 0)) {
                  if(!(np->state.status & STAT_STICKY)) {
                     ChangeDesktop(np->state.desktop);
                  }
                  break;
               }
            }
         }
      }
   }

   /* Build up the list of clients to restore in correct order. */
   toRestore = AllocateStack(sizeof(ClientNode*) * clientCount);
   restoreCount = 0;
   for(i = 0; i < LAYER_COUNT; i++) {
      ClientNode *np;
      for(np = nodes[i]; np; np = np->next) {
         if(!ShouldFocus(np, 1)) {
            continue;
         }
         if(np->className && !strcmp(np->className, className)) {
            toRestore[restoreCount] = np;
            restoreCount += 1;
         }
      }
   }
   Assert(restoreCount <= clientCount);
   for(i = restoreCount - 1; i >= 0; i--) {
      RestoreClient(toRestore[i], 1);
   }
   for(i = 0; i < restoreCount; i++) {
      if(toRestore[i]->state.status & (STAT_CANFOCUS | STAT_TAKEFOCUS)) {
         FocusClient(toRestore[i]);
         break;
      }
   }
   ReleaseStack(toRestore);
}

/** Process a task list motion event. */
void ProcessTaskMotionEvent(TrayComponentType *cp, int x, int y, int mask)
{
   TaskBarType *bp = (TaskBarType*)cp->object;
   bp->mousex = cp->screenx + x;
   bp->mousey = cp->screeny + y;
   GetCurrentTime(&bp->mouseTime);
}

/** Show the menu associated with a task list item. */
void ShowClientList(TaskBarType *bar, TaskEntry *tp)
{
   Menu *menu;
   MenuItem *item;
   ClientEntry *cp;

   const ScreenType *sp;
   int x, y;
   Window w;

   if(settings.groupTasks) {

      menu = CreateMenu();

      item = CreateMenuItem(MENU_ITEM_NORMAL);
      item->name = CopyString(_("Close"));
      item->action.type = MA_CLOSE | MA_GROUP_MASK;
      item->action.context = tp;
      item->next = menu->items;
      menu->items = item;

      item = CreateMenuItem(MENU_ITEM_NORMAL);
      item->name = CopyString(_("Minimize"));
      item->action.type = MA_MINIMIZE | MA_GROUP_MASK;
      item->action.context = tp;
      item->next = menu->items;
      menu->items = item;

      item = CreateMenuItem(MENU_ITEM_NORMAL);
      item->name = CopyString(_("Restore"));
      item->action.type = MA_RESTORE | MA_GROUP_MASK;
      item->action.context = tp;
      item->next = menu->items;
      menu->items = item;

      item = CreateMenuItem(MENU_ITEM_SUBMENU);
      item->name = CopyString(_("Send To"));
      item->action.type = MA_SENDTO_MENU | MA_GROUP_MASK;
      item->action.context = tp;
      item->next = menu->items;
      menu->items = item;

      /* Load the separator and group actions. */
      item = CreateMenuItem(MENU_ITEM_SEPARATOR);
      item->next = menu->items;
      menu->items = item;

      /* Load the clients into the menu. */
      for(cp = tp->clients; cp; cp = cp->next) {
         if(!ShouldFocus(cp->client, 0)) {
            continue;
         }
         item = CreateMenuItem(MENU_ITEM_NORMAL);
         if(cp->client->state.status & STAT_MINIMIZED) {
            size_t len = 0;
            if(cp->client->name) {
               len = strlen(cp->client->name);
            }
            item->name = Allocate(len + 3);
            item->name[0] = '[';
            memcpy(&item->name[1], cp->client->name, len);
            item->name[len + 1] = ']';
            item->name[len + 2] = 0;
         } else {
            item->name = CopyString(cp->client->name);
         }
         item->icon = cp->client->icon ? cp->client->icon : GetDefaultIcon();
         item->action.type = MA_EXECUTE;
         item->action.context = cp->client;
         item->next = menu->items;
         menu->items = item;
      }
   } else {
      /* Not grouping clients. */
      menu = CreateWindowMenu(tp->clients->client);
   }

   /* Initialize and position the menu. */
   InitializeMenu(menu);
   sp = GetCurrentScreen(bar->cp->screenx, bar->cp->screeny);
   GetMousePosition(&x, &y, &w);
   if(bar->layout == LAYOUT_HORIZONTAL) {
      if(bar->cp->screeny + bar->cp->height / 2 < sp->y + sp->height / 2) {
         /* Bottom of the screen: menus go up. */
         y = bar->cp->screeny + bar->cp->height;
      } else {
         /* Top of the screen: menus go down. */
         y = bar->cp->screeny - menu->height;
      }
      x -= menu->width / 2;
      x = Max(x, sp->x);
   } else {
      if(bar->cp->screenx + bar->cp->width / 2 < sp->x + sp->width / 2) {
         /* Left side: menus go right. */
         x = bar->cp->screenx + bar->cp->width;
      } else {
         /* Right side: menus go left. */
         x = bar->cp->screenx - menu->width;
      }
      y -= menu->height / 2;
      y = Max(y, sp->y);
   }

   ShowMenu(menu, RunTaskBarCommand, x, y, 0);

   DestroyMenu(menu);

}

/** Run a menu action. */
void RunTaskBarCommand(MenuAction *action, unsigned button)
{
   ClientEntry *cp;

   if(action->type & MA_GROUP_MASK) {
      TaskEntry *tp = action->context;
      for(cp = tp->clients; cp; cp = cp->next) {
         if(!ShouldFocus(cp->client, 0)) {
            continue;
         }
         switch(action->type & ~MA_GROUP_MASK) {
         case MA_SENDTO:
            SetClientDesktop(cp->client, action->value);
            break;
         case MA_CLOSE:
            DeleteClient(cp->client);
            break;
         case MA_RESTORE:
            RestoreClient(cp->client, 0);
            break;
         case MA_MINIMIZE:
            MinimizeClient(cp->client, 0);
            break;
         default:
            break;
         }
      }
   } else if(action->type == MA_EXECUTE) {
      if(button == Button3) {
         Window w;
         int x, y;
         GetMousePosition(&x, &y, &w);
         ShowWindowMenu(action->context, x, y, 0);
      } else {
         ClientNode *np = action->context;
         RestoreClient(np, 1);
         FocusClient(np);
         MoveMouse(np->window, np->width / 2, np->height / 2);
      }
   } else {
      RunWindowCommand(action, button);
   }
}

/** Add a client to the task bar. */
void AddClientToTaskBar(ClientNode *np)
{
   TaskEntry *tp = NULL;
   ClientEntry *cp = Allocate(sizeof(ClientEntry));
   cp->client = np;

   if(np->className && settings.groupTasks) {
      for(tp = taskEntries; tp; tp = tp->next) {
         const char *className = tp->clients->client->className;
         if(className && !strcmp(np->className, className)) {
            break;
         }
      }
   }
   if(tp == NULL) {
      tp = Allocate(sizeof(TaskEntry));
      tp->clients = NULL;
      tp->next = NULL;
      tp->prev = taskEntriesTail;
      if(taskEntriesTail) {
         taskEntriesTail->next = tp;
      } else {
         taskEntries = tp;
      }
      taskEntriesTail = tp;
   }

   cp->next = tp->clients;
   if(tp->clients) {
      tp->clients->prev = cp;
   }
   cp->prev = NULL;
   tp->clients = cp;

   RequireTaskUpdate();
   UpdateNetClientList();

}

/** Remove a client from the task bar. */
void RemoveClientFromTaskBar(ClientNode *np)
{
   TaskEntry *tp;
   for(tp = taskEntries; tp; tp = tp->next) {
      ClientEntry *cp;
      for(cp = tp->clients; cp; cp = cp->next) {
         if(cp->client == np) {
            if(cp->prev) {
               cp->prev->next = cp->next;
            } else {
               tp->clients = cp->next;
            }
            if(cp->next) {
               cp->next->prev = cp->prev;
            }
            Release(cp);
            if(!tp->clients) {
               if(tp->prev) {
                  tp->prev->next = tp->next;
               } else {
                  taskEntries = tp->next;
               }
               if(tp->next) {
                  tp->next->prev = tp->prev;
               } else {
                  taskEntriesTail = tp->prev;
               }
               Release(tp);
            }
            RequireTaskUpdate();
            UpdateNetClientList();
            return;
         }
      }
   }
}

/** Update all task bars. */
void UpdateTaskBar(void)
{
   TaskBarType *bp;
   int lastHeight = -1;

   if(JUNLIKELY(shouldExit)) {
      return;
   }

   for(bp = bars; bp; bp = bp->next) {
      if(bp->layout == LAYOUT_VERTICAL && bp->labelPos < LABEL_POSITION_TOP) {
         TaskEntry *tp;
         lastHeight = bp->cp->requestedHeight;
         if(bp->userHeight > 0) {
            bp->itemHeight = bp->userHeight;
         } else {
            bp->itemHeight = GetStringHeight(FONT_TASKLIST) + 12;
         }
         bp->cp->requestedHeight = 0;
         for(tp = taskEntries; tp; tp = tp->next) {
            if(ShouldShowEntry(tp)) {
               bp->cp->requestedHeight += bp->itemHeight;
            }
         }
         bp->cp->requestedHeight = Max(1, bp->cp->requestedHeight);
         if(lastHeight != bp->cp->requestedHeight) {
            ResizeTray(bp->cp->tray);
         }
      }
      ComputeItemSize(bp);
      Render(bp);
   }
}

/** Signal task bar (for popups). */
void SignalTaskbar(const TimeType *now, int x, int y, Window w, void *data)
{

   TaskBarType *bp = (TaskBarType*)data;
   TaskEntry *ep;

   if(w == bp->cp->tray->window &&
      abs(bp->mousex - x) < settings.doubleClickDelta &&
      abs(bp->mousey - y) < settings.doubleClickDelta) {
      if(GetTimeDifference(now, &bp->mouseTime) >= settings.popupDelay) {
         ep = GetEntry(bp, x - bp->cp->screenx, y - bp->cp->screeny);
         if(settings.groupTasks) {
            if(ep && ep->clients->client->className) {
               ShowPopup(x, y, ep->clients->client->className, POPUP_TASK);
            }
         } else {
            if(ep && ep->clients->client->name) {
               ShowPopup(x, y, ep->clients->client->name, POPUP_TASK);
            }
         }
      }
   }

}

/** Draw a specific task bar. */
void Render(const TaskBarType *bp)
{
   TaskEntry *tp;
   char *displayName;
   ButtonNode button;
   int x, y;

   if(JUNLIKELY(shouldExit)) {
      return;
   }

   ClearTrayDrawable(bp->cp);
   if(!taskEntries) {
      UpdateSpecificTray(bp->cp->tray, bp->cp);
      return;
   }

   ResetButton(&button, bp->cp->pixmap);
   button.border = settings.taskListDecorations == DECO_MOTIF;
   button.font = FONT_TASKLIST;
   button.height = bp->itemHeight;
   button.width = bp->itemWidth;
   button.labelPos = bp->labelPos;
   button.text = NULL;

   x = 0;
   y = 0;
   for(tp = taskEntries; tp; tp = tp->next) {

      if(!ShouldShowEntry(tp)) {
         continue;
      }

      /* Check for an active or urgent window and count clients. */
      ClientEntry *cp;
      unsigned clientCount = 0;
      button.type = BUTTON_TASK;
      for(cp = tp->clients; cp; cp = cp->next) {
         if(ShouldFocus(cp->client, 0)) {
            const char flash = (cp->client->state.status & STAT_FLASH) != 0;
            const char active = (cp->client->state.status & STAT_ACTIVE)
               && IsClientOnCurrentDesktop(cp->client);
            const char minimized = (cp->client->state.status & STAT_MINIMIZED);
            if(flash || active) {
               if(button.type != BUTTON_TASK_ACTIVE) {
                  button.type = BUTTON_TASK_ACTIVE;
               } else if(minimized) {
                  button.type = BUTTON_TASK_MINIMIZED;
               } else {
                  button.type = BUTTON_TASK;
               }
            } else if(minimized) {
               button.type = BUTTON_TASK_MINIMIZED;
            }
            clientCount += 1;
         }
      }
      button.x = x;
      button.y = y;
      if(!tp->clients->client || !tp->clients->client->icon) {
         button.icon = GetDefaultIcon();
      } else {
         button.icon = tp->clients->client->icon;
      }
      displayName = NULL;
      if(bp->labeled) {
         if(tp->clients->client->className && settings.groupTasks) {
            if(clientCount != 1) {
               const size_t len = strlen(tp->clients->client->className) + 16;
               displayName = Allocate(len);
               snprintf(displayName, len, "%s (%u)",
                        tp->clients->client->className, clientCount);
               button.text = displayName;
            } else {
               button.text = tp->clients->client->className;
            }
         } else {
            button.text = tp->clients->client->name;
         }
      }
      DrawButton(&button);

      if(displayName) {
         Release(displayName);
      }

      if(bp->layout == LAYOUT_HORIZONTAL) {
         x += bp->itemWidth;
      } else {
         y += bp->itemHeight;
      }
   }

   UpdateSpecificTray(bp->cp->tray, bp->cp);

}

/** Focus the next client in the task bar. */
void FocusNext(void)
{
   TaskEntry *tp;

   /* Find the current entry. */
   for(tp = taskEntries; tp; tp = tp->next) {
      ClientEntry *cp;
      for(cp = tp->clients; cp; cp = cp->next) {
         if(cp->client->state.status & (STAT_CANFOCUS | STAT_TAKEFOCUS)) {
            if(ShouldFocus(cp->client, 1)) {
               if(cp->client->state.status & STAT_ACTIVE) {
                  goto ClientFound;
               }
            }
         }
      }
   }
ClientFound:

   /* Move to the next group. */
   if(tp) {
      do {
         tp = tp->next;
      } while(tp && !ShouldFocusEntry(tp));
   }
   if(!tp) {
      /* Wrap around; start at the beginning. */
      for(tp = taskEntries; tp; tp = tp->next) {
         if(ShouldFocusEntry(tp)) {
            break;
         }
      }
   }

   /* Focus the group if one exists. */
   if(tp) {
      FocusGroup(tp);
   }
}

/** Focus the previous client in the task bar. */
void FocusPrevious(void)
{
   TaskEntry *tp;

   /* Find the current entry. */
   for(tp = taskEntries; tp; tp = tp->next) {
      ClientEntry *cp;
      for(cp = tp->clients; cp; cp = cp->next) {
         if(cp->client->state.status & (STAT_CANFOCUS | STAT_TAKEFOCUS)) {
            if(ShouldFocus(cp->client, 1)) {
               if(cp->client->state.status & STAT_ACTIVE) {
                  cp = cp->next;
                  goto ClientFound;
               }
            }
         }
      }
   }
ClientFound:

   /* Move to the previous group. */
   if(tp) {
      do {
         tp = tp->prev;
      } while(tp && !ShouldFocusEntry(tp));
   }
   if(!tp) {
      /* Wrap around; start at the end. */
      for(tp = taskEntriesTail; tp; tp = tp->prev) {
         if(ShouldFocusEntry(tp)) {
            break;
         }
      }
   }

   /* Focus the group if one exists. */
   if(tp) {
      FocusGroup(tp);
   }
}

/** Focus the client in the task bar. */
void FocusAt(char n)
{
   TaskEntry *tp;
   char window = 0;
      
   for(tp = taskEntries; tp; tp = tp->next) {
       if(ShouldFocusEntry(tp)) {
          if(window == n) {
             FocusGroup(tp);
             break;
          }
          ++window;
       }
   }
}

/** Determine if there is anything to show for the specified entry. */
char ShouldShowEntry(const TaskEntry *tp)
{
   const ClientEntry *cp;
   for(cp = tp->clients; cp; cp = cp->next) {
      if(ShouldFocus(cp->client, 0)) {
         return 1;
      }
   }
   return 0;
}

/** Determine if we should attempt to focus an entry. */
char ShouldFocusEntry(const TaskEntry *tp)
{
   const ClientEntry *cp;
   for(cp = tp->clients; cp; cp = cp->next) {
      if(cp->client->state.status & (STAT_CANFOCUS | STAT_TAKEFOCUS)) {
         if(ShouldFocus(cp->client, 1)) {
            return 1;
         }
      }
   }
   return 0;
}

/** Get the item associated with a coordinate on the task bar. */
TaskEntry *GetEntry(TaskBarType *bar, int x, int y)
{
   TaskEntry *tp;
   int offset;

   offset = 0;
   for(tp = taskEntries; tp; tp = tp->next) {
      if(!ShouldShowEntry(tp)) {
         continue;
      }
      if(bar->layout == LAYOUT_HORIZONTAL) {
         offset += bar->itemWidth;
         if(x < offset) {
            return tp;
         }
      } else {
         offset += bar->itemHeight;
         if(y < offset) {
            return tp;
         }
      }
   }

   return NULL;
}

/** Set the maximum width of an item in the task bar. */
void SetMaxTaskBarItemWidth(TrayComponentType *cp, const char *value)
{
   TaskBarType *bp = (TaskBarType*)cp->object;
   int temp;

   Assert(cp);
   Assert(value);

   temp = atoi(value);
   if(JUNLIKELY(temp < 0)) {
      Warning(_("invalid maxwidth for TaskList: %s"), value);
      return;
   }
   bp->maxItemWidth = temp;
}

/** Set the preferred height of the specified task bar. */
void SetTaskBarHeight(TrayComponentType *cp, const char *value)
{
   TaskBarType *bp = (TaskBarType*)cp->object;
   int temp;

   temp = atoi(value);
   if(JUNLIKELY(temp < 0)) {
      Warning(_("invalid height for TaskList: %s"), value);
      return;
   }
   bp->userHeight = temp;
}

/** Set whether the label should be displayed. */
void SetTaskBarLabeled(TrayComponentType *cp, char labeled)
{
   TaskBarType *bp = (TaskBarType*)cp->object;
   bp->labeled = labeled;
}

/** Set the label's postion. */
void SetTaskBarLabelPosition(TrayComponentType *cp, const char *value)
{
   TaskBarType *bp = (TaskBarType*)cp->object;

   Assert(cp);
   Assert(value);

   if(!strcmp(value, "right")) {
      bp->labelPos = LABEL_POSITION_RIGHT;
   } else if(!strcmp(value, "top")) {
      bp->labelPos = LABEL_POSITION_TOP;
   } else if(!strcmp(value, "bottom")) {
      bp->labelPos = LABEL_POSITION_BOTTOM;
   } else {
      Warning(_("invalid labelpos for TaskList: %s"), value);
   }
}

/** Maintain the _NET_CLIENT_LIST[_STACKING] properties on the root. */
void UpdateNetClientList(void)
{
   TaskEntry *tp;
   ClientNode *client;
   Window *windows;
   unsigned int count;
   int layer;

   /* Determine how much we need to allocate. */
   if(clientCount == 0) {
      windows = NULL;
   } else {
      windows = AllocateStack(clientCount * sizeof(Window));
   }

   /* Set _NET_CLIENT_LIST */
   count = 0;
   for(tp = taskEntries; tp; tp = tp->next) {
      ClientEntry *cp;
      for(cp = tp->clients; cp; cp = cp->next) {
         windows[count] = cp->client->window;
         count += 1;
      }
   }
   Assert(count <= clientCount);
   JXChangeProperty(display, rootWindow, atoms[ATOM_NET_CLIENT_LIST],
                    XA_WINDOW, 32, PropModeReplace,
                    (unsigned char*)windows, count);

   /* Set _NET_CLIENT_LIST_STACKING */
   count = 0;
   for(layer = FIRST_LAYER; layer <= LAST_LAYER; layer++) {
      for(client = nodes[layer]; client; client = client->next) {
         windows[count] = client->window;
         count += 1;
      }
   }
   JXChangeProperty(display, rootWindow, atoms[ATOM_NET_CLIENT_LIST_STACKING],
                    XA_WINDOW, 32, PropModeReplace,
                    (unsigned char*)windows, count);

   if(windows != NULL) {
      ReleaseStack(windows);
   }
   
}
