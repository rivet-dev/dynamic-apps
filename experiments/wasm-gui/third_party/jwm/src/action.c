/**
 * @file action.c
 * @author Joe Wingbermuehle
 *
 * @brief Tray component actions.
 */

#include "jwm.h"
#include "action.h"
#include "tray.h"
#include "root.h"
#include "screen.h"
#include "misc.h"
#include "error.h"
#include "cursor.h"
#include "command.h"
#include "desktop.h"
#include "menu.h"

typedef struct ActionNode {
   char *action;
   struct ActionNode *next;
   int mask;
} ActionNode;

/** Add an action. */
void AddAction(ActionNode **actions, const char *action, int mask)
{
   ActionNode *ap;

   /* Make sure we actually have an action. */
   if(action == NULL || action[0] == 0 || mask == 0) {
      /* Valid (root menu 1). */
   } else if(!strncmp(action, "exec:", 5)) {
      /* Valid. */
   } else if(!strncmp(action, "root:", 5)) {
      /* Valid. However, the specified root menu may not exist.
       * This case is handled in ValidateTrayButtons.
       */
   } else if(!strcmp(action, "showdesktop")) {
      /* Valid. */
   } else {
      /* Invalid; don't add the action. */
      Warning(_("invalid action: \"%s\""), action);
      return;
   }

   ap = Allocate(sizeof(ActionNode));
   ap->action = CopyString(action);
   ap->mask = mask;
   ap->next = *actions;
   *actions = ap;
}

/** Destroy an action list. */
void DestroyActions(ActionNode *actions)
{
   while(actions) {
      ActionNode *next = actions->next;
      Release(actions->action);
      Release(actions);
      actions = next;
   }
}

/** Process a button press. */
void ProcessActionPress(struct ActionNode *actions,
                        struct TrayComponentType *cp,
                        int x, int y, int button)
{
   const ScreenType *sp;
   const ActionNode *ap;
   const int mask = 1 << button;
   int mwidth, mheight;
   int menu;

   if(JUNLIKELY(menuShown)) {
      return;
   }
   if (x < -1 || x > cp->width) {
      return;
   }
   if (y < -1 || y > cp->height) {
      return;
   }

   menu = -1;
   for(ap = actions; ap; ap = ap->next) {
      if(ap->mask & mask) {
         if(ap->action && ap->action[0]) {
            if(strncmp(ap->action, "root:", 5) != 0) {
               if(button == Button4 || button == Button5) {

                  /* Don't wait for a release if the scroll wheel is used. */
                  if(!strncmp(ap->action, "exec:", 5)) {
                     RunCommand(ap->action + 5);
                  } else if(!strcmp(ap->action, "showdesktop")) {
                     ShowDesktop();
                  }

               } else {

                  if(!GrabMouse(cp->tray->window)) {
                     return;
                  }

                  /* Show the button being pressed. */
                  cp->grabbed = 1;
                  if(cp->Redraw) {
                     (cp->Redraw)(cp);
                     UpdateSpecificTray(cp->tray, cp);
                  }
               }
               return;

            } else {
               menu = GetRootMenuIndexFromString(&ap->action[5]);
            }
         } else {
            menu = 1;
         }
         break;
      }
   }
   if(menu < 0) {
      return;
   }

   GetRootMenuSize(menu, &mwidth, &mheight);
   sp = GetCurrentScreen(cp->screenx, cp->screeny);
   if(cp->tray->layout == LAYOUT_HORIZONTAL) {
      x = cp->screenx - 1;
      if(cp->screeny + cp->height / 2 < sp->y + sp->height / 2) {
         y = cp->screeny + cp->height;
      } else {
         y = cp->screeny - mheight;
      }
   } else {
      y = cp->screeny - 1;
      if(cp->screenx + cp->width / 2 < sp->x + sp->width / 2) {
         x = cp->screenx + cp->width;
      } else {
         x = cp->screenx - mwidth;
      }
   }

   cp->grabbed = 1;
   if(cp->Redraw) {
      (cp->Redraw)(cp);
      UpdateSpecificTray(cp->tray, cp);
   }
   if(ShowRootMenu(menu, x, y, 0)) {
      cp->grabbed = 0;
      if(cp->Redraw) {
         (cp->Redraw)(cp);
         UpdateSpecificTray(cp->tray, cp);
      }
   }
}

/** Process a button release. */
void ProcessActionRelease(struct ActionNode *actions,
                          struct TrayComponentType *cp,
                          int x, int y, int button)
{
   const ActionNode *ap;
   const int mask = 1 << button;

   if(JUNLIKELY(menuShown)) {
      return;
   }

   cp->grabbed = 0;
   if(cp->Redraw) {
      (cp->Redraw)(cp);
      UpdateSpecificTray(cp->tray, cp);
   }

   /* Since we grab the mouse, make sure the mouse is actually over
    * the button. */
   if(x < -1 || x > cp->width) {
      return;
   }
   if(y < -1 || y > cp->height) {
      return;
   }

   /* Run the action (if any). */
   for(ap = actions; ap; ap = ap->next) {
      if(ap->mask & mask) {
         if(ap->action && strlen(ap->action) > 0) {
            if(!strncmp(ap->action, "exec:", 5)) {
               RunCommand(ap->action + 5);
            } else if(!strcmp(ap->action, "showdesktop")) {
               ShowDesktop();
            }
         }
         return;
      }
   }
}

/** Validate actions. */
void ValidateActions(const ActionNode *actions)
{
   const ActionNode *ap;
   for(ap = actions; ap; ap = ap->next) {
      if(ap->action && !strncmp(ap->action, "root:", 5)) {
         const int bindex = GetRootMenuIndexFromString(&ap->action[5]);
         if(JUNLIKELY(!IsRootMenuDefined(bindex))) {
            Warning(_("action: root menu \"%s\" not defined"),
                    &ap->action[5]);
         }
      }
   }
}
