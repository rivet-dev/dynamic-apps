/**
 * @file settings.c
 * @author Joe Wingbermuehle
 * @date 2012
 *
 * @brief JWM settings.
 *
 */

#include "jwm.h"
#include "settings.h"
#include "misc.h"

Settings settings;

static const MouseContextType DEFAULT_TITLE_BAR_LAYOUT[TBC_COUNT + 1] = {
   MC_ICON,
   MC_MOVE,
   MC_MINIMIZE,
   MC_MAXIMIZE,
   MC_CLOSE,
   MC_NONE
};

/** Default characters for client machine name delimiters. */
static const char DEFAULT_CLIENT_NAME_DELIMITERS[2] = {'(',')'};

static void FixRange(unsigned int *value,
                     unsigned int min_value,
                     unsigned int max_value,
                     unsigned int def_value);

/** Initialize settings. */
void InitializeSettings(void)
{
   settings.moveMask = (1 << Mod1MapIndex);
   settings.doubleClickSpeed = 400;
   settings.doubleClickDelta = 2;
   settings.snapMode = SNAP_BORDER;
   settings.snapDistance = 5;
   settings.moveMode = MOVE_OPAQUE;
   settings.moveStatusType = SW_SCREEN;
   settings.resizeStatusType = SW_SCREEN;
   settings.focusModel = FOCUS_SLOPPY;
   settings.resizeMode = RESIZE_OPAQUE;
   settings.popupDelay = 600;
   settings.desktopDelay = 1000;
   settings.trayOpacity = UINT_MAX;
   settings.popupMask = POPUP_ALL;
   settings.activeClientOpacity = UINT_MAX;
   settings.inactiveClientOpacity = (unsigned int)(0.75 * UINT_MAX);
   settings.borderWidth = 5;
   settings.titleHeight = 0;
   settings.titleTextAlignment = ALIGN_LEFT;
   settings.desktopWidth = 4;
   settings.desktopHeight = 1;
   settings.desktopBackAndForth = DBACKANDFORTH_OFF;
   settings.menuOpacity = UINT_MAX;
   settings.windowDecorations = DECO_FLAT;
   settings.trayDecorations = DECO_FLAT;
   settings.taskListDecorations = DECO_UNSET;
   settings.menuDecorations = DECO_FLAT;
   settings.cornerRadius = 4;
   settings.groupTasks = 0;
   settings.listAllTasks = 0;
   settings.dockSpacing = 0;
   settings.showClientName = 0;
   memcpy(settings.clientNameDelimiters, DEFAULT_CLIENT_NAME_DELIMITERS,
      sizeof(settings.clientNameDelimiters));
   settings.showKillMenuItem = 1;
   memcpy(settings.titleBarLayout, DEFAULT_TITLE_BAR_LAYOUT,
      sizeof(settings.titleBarLayout));
}

/** Make sure settings are reasonable. */
void StartupSettings(void)
{

   FixRange(&settings.cornerRadius, 0, 5, 4);

   FixRange(&settings.borderWidth, 1, 128, 4);
   FixRange(&settings.titleHeight, 0, 256, 0);

   FixRange(&settings.doubleClickDelta, 0, 64, 2);
   FixRange(&settings.doubleClickSpeed, 1, 2000, 400);

   FixRange(&settings.desktopWidth, 1, 64, 4);
   FixRange(&settings.desktopHeight, 1, 64, 1);
   settings.desktopCount = settings.desktopWidth * settings.desktopHeight;

   if(settings.taskListDecorations == DECO_UNSET) {
      settings.taskListDecorations = settings.trayDecorations;
   }

   FixRange(&settings.dockSpacing, 0, 64, 0);
}

/** Update a string setting. */
void SetPathString(char **dest, const char *src)
{
   if(*dest) {
      Release(*dest);
   }
   *dest = CopyString(src);
   if(JLIKELY(*dest)) {
      ExpandPath(dest);
   }
}

/** Make sure a value is in range. */
void FixRange(unsigned int *value,
              unsigned int min_value,
              unsigned int max_value,
              unsigned int def_value)
{
   if(JUNLIKELY(*value < min_value || *value > max_value)) {
      *value = def_value;
   }
}

/** Set the title button order. */
void SetTitleButtonOrder(const char *order)
{
   unsigned i = 0;
   memset(settings.titleBarLayout, 0, sizeof(settings.titleBarLayout));
   while(*order && i < TBC_COUNT) {
      switch(tolower(*order)) {
      case 'x':   /* Close */
         settings.titleBarLayout[i++] = MC_CLOSE;
         break;
      case 'i':   /* Minimize (iconify) */
         settings.titleBarLayout[i++] = MC_MINIMIZE;
         break;
      case 't':   /* Title */
         settings.titleBarLayout[i++] = MC_MOVE;
         break;
      case 'm':   /* Maximize */
         settings.titleBarLayout[i++] = MC_MAXIMIZE;
         break;
      case 'w':   /* Window */
         settings.titleBarLayout[i++] = MC_ICON;
         break;
      default:
         break;
      }
      order += 1;
   }
}
