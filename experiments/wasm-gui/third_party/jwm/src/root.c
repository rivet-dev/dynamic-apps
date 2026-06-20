/**
 * @file root.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Root menu functions.
 *
 */

#include "jwm.h"
#include "root.h"
#include "menu.h"
#include "client.h"
#include "error.h"
#include "confirm.h"
#include "misc.h"
#include "winmenu.h"
#include "command.h"
#include "parse.h"
#include "settings.h"
#include "desktop.h"

/** Number of root menus to support. */
#define ROOT_MENU_COUNT 36

static Menu *rootMenu[ROOT_MENU_COUNT];

static void ExitHandler(ClientNode *np);

static void RunRootCommand(MenuAction *action, unsigned button);

/** Initialize root menu data. */
void InitializeRootMenu(void)
{
   unsigned int x;
   for(x = 0; x < ROOT_MENU_COUNT; x++) {
      rootMenu[x] = NULL;
   }
}

/** Startup root menus. */
void StartupRootMenu(void)
{

   unsigned int x, y;
   char found;

   for(x = 0; x < ROOT_MENU_COUNT; x++) {
      if(rootMenu[x]) {
         found = 0;
         for(y = 0; y < x; y++) {
            if(rootMenu[y] == rootMenu[x]) {
               found = 1;
               break;
            }
         }
         if(!found) {
            InitializeMenu(rootMenu[x]);
         }
      }
   }

}

/** Destroy root menu data. */
void DestroyRootMenu(void)
{

   unsigned int x, y;

   for(x = 0; x < ROOT_MENU_COUNT; x++) {
      if(rootMenu[x]) {
         DestroyMenu(rootMenu[x]);
         for(y = x + 1; y < ROOT_MENU_COUNT; y++) {
            if(rootMenu[x] == rootMenu[y]) {
               rootMenu[y] = NULL;
            }
         }
         rootMenu[x] = NULL;
      }
   }

}

/** Get the index for a root menu character. */
int GetRootMenuIndex(char ch)
{
   if(ch >= '0' && ch <= '9') {
      return ch - '0';
   } else if(ch >= 'A' && ch <= 'Z') {
      return ch - 'A' + 10;
   } else if(ch >= 'a' && ch <= 'z') {
      return ch - 'a' + 10;
   } else {
      return -1;
   }
}

/** Get the index for a root menu string. */
int GetRootMenuIndexFromString(const char *str)
{
   unsigned int temp = 0;
   while(*str && IsSpace(*str, &temp)) {
      str += 1;
   }
   if(JUNLIKELY(!*str)) {
      return -1;
   }
   const int result = GetRootMenuIndex(*str);
   str += 1;
   while(*str && IsSpace(*str, &temp)) {
      str += 1;
   }
   return *str ? -1 : result;
}

/** Set a root menu. */
void SetRootMenu(const char *indexes, Menu *m)
{

   unsigned x;

   /* Loop over each index to consider. */
   for(x = 0; indexes[x]; x++) {

      /* Get the index and make sure it's in range. */
      const int index = GetRootMenuIndex(indexes[x]);
      if(JUNLIKELY(index < 0)) {
         Warning(_("invalid root menu specified: \"%c\""), indexes[x]);
         continue;
      }

      if(rootMenu[index] && rootMenu[index] != m) {

         /* See if replacing this value will cause an orphan. */
         unsigned y;
         char found = 0;
         for(y = 0; y < ROOT_MENU_COUNT; y++) {
            if(index != y && rootMenu[y] == rootMenu[index]) {
               found = 1;
               break;
            }
         }

         /* If we have an orphan, destroy it. */
         if(!found) {
            DestroyMenu(rootMenu[index]);
         }

      }

      rootMenu[index] = m;

   }

}

/** Determine if the specified root menu is defined. */
char IsRootMenuDefined(int index)
{
   if(index >= 0 && index < ROOT_MENU_COUNT && rootMenu[index]) {
      return 1;
   } else {
      return 0;
   }
}

/** Determine the size of a root menu. */
void GetRootMenuSize(int index, int *width, int *height)
{

   if(!rootMenu[index]) {
      *width = 0;
      *height = 0;
      return;
   }
   *width = rootMenu[index]->width;
   *height = rootMenu[index]->height;

}

/** Show a root menu. */
char ShowRootMenu(int index, int x, int y, char keyboard)
{
   if(!rootMenu[index]) {
      return 0;
   }
   if(menuShown) {
      return 1;
   }
   if(rootMenu[index]->dynamic) {
      Menu *menu = rootMenu[index];
      menu = ParseDynamicMenu(menu->timeout_ms, menu->dynamic);
      if(menu) {
         InitializeMenu(menu);
         ShowMenu(menu, RunRootCommand, x, y, keyboard);
         DestroyMenu(menu);
         return 1;
      }
   }
   ShowMenu(rootMenu[index], RunRootCommand, x, y, keyboard);
   return 1;
}

/** Exit callback for the exit menu item. */
void ExitHandler(ClientNode *np)
{
   shouldExit = 1;
}

/** Restart callback for the restart menu item. */
void Restart(void)
{
   shouldRestart = 1;
   shouldExit = 1;
}

/** Exit with optional confirmation. */
void Exit(char confirm)
{
   if(confirm) {
      ShowConfirmDialog(NULL, ExitHandler,
                        _("Exit JWM"),
                        _("Are you sure?"),
                        NULL);
   } else {
      ExitHandler(NULL);
   }
}

/** Reload the menu. */
void ReloadMenu(void)
{
   shouldReload = 1;
   if(!menuShown) {
      ShutdownRootMenu();
      DestroyRootMenu();
      InitializeRootMenu();
      ParseConfig(configPath);
      StartupRootMenu();
      shouldReload = 0;
   }
}

/** Root menu callback. */
void RunRootCommand(MenuAction *action, unsigned button)
{

   switch(action->type) {
   case MA_EXECUTE:
      RunCommand(action->str);
      break;
   case MA_RESTART:
      Restart();
      break;
   case MA_EXIT:
      if(exitCommand) {
         Release(exitCommand);
      }
      exitCommand = CopyString(action->str);
      Exit(action->value);
      break;
   case MA_DESKTOP:
      ChangeDesktop(action->value);
      break;

   case MA_SENDTO:
   case MA_LAYER:
   case MA_MAXIMIZE:
   case MA_MINIMIZE:
   case MA_RESTORE:
   case MA_SHADE:
   case MA_MOVE:
   case MA_RESIZE:
   case MA_KILL:
   case MA_CLOSE:
      ChooseWindow(action);
      break;

   default:
      break;
   }

}

