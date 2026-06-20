/**
 * @file parse.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief JWM configuration parser.
 *
 */

#include "jwm.h"
#include "parse.h"
#include "lex.h"
#include "settings.h"
#include "menu.h"
#include "root.h"
#include "tray.h"
#include "group.h"
#include "misc.h"
#include "swallow.h"
#include "pager.h"
#include "error.h"
#include "binding.h"
#include "main.h"
#include "font.h"
#include "icon.h"
#include "command.h"
#include "taskbar.h"
#include "traybutton.h"
#include "clock.h"
#include "dock.h"
#include "background.h"
#include "spacer.h"
#include "desktop.h"
#include "border.h"
#include "default.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

/** Mapping of action names to values.
 * Note that this mapping must be sorted.
 */
static const StringMappingType ACTION_MAP[] = {
   { "at#",                   ACTION_AT             },
   { "center",                ACTION_CENTER         },
   { "close",                 ACTION_CLOSE          },
   { "ddesktop",              ACTION_DDESKTOP       },
   { "desktop#",              ACTION_DESKTOP        },
   { "down",                  ACTION_DOWN           },
   { "escape",                ACTION_ESC            },
   { "exit",                  ACTION_EXIT           },
   { "fullscreen",            ACTION_FULLSCREEN     },
   { "kill",                  ACTION_KILL           },
   { "ldesktop",              ACTION_LDESKTOP       },
   { "left",                  ACTION_LEFT           },
   { "maxbottom",             ACTION_MAXBOTTOM      },
   { "maxbottomleft",         ACTION_MAXBOTTOMLEFT  },
   { "maxbottomright",        ACTION_MAXBOTTOMRIGHT },
   { "maxh",                  ACTION_MAXH           },
   { "maximize",              ACTION_MAX            },
   { "maxleft",               ACTION_MAXLEFT        },
   { "maxright",              ACTION_MAXRIGHT       },
   { "maxtop",                ACTION_MAXTOP         },
   { "maxtopleft",            ACTION_MAXTOPLEFT     },
   { "maxtopright",           ACTION_MAXTOPRIGHT    },
   { "maxv",                  ACTION_MAXV           },
   { "minimize",              ACTION_MIN            },
   { "move",                  ACTION_MOVE           },
   { "next",                  ACTION_NEXT           },
   { "nextstacked",           ACTION_NEXTSTACK      },
   { "none",                  ACTION_NONE           },
   { "prev",                  ACTION_PREV           },
   { "prevstacked",           ACTION_PREVSTACK      },
   { "rdesktop",              ACTION_RDESKTOP       },
   { "resize",                ACTION_RESIZE         },
   { "restart",               ACTION_RESTART        },
   { "restore",               ACTION_RESTORE        },
   { "right",                 ACTION_RIGHT          },
   { "select",                ACTION_ENTER          },
   { "send#",                 ACTION_SEND           },
   { "sendd",                 ACTION_SENDD          },
   { "sendl",                 ACTION_SENDL          },
   { "sendr",                 ACTION_SENDR          },
   { "sendu",                 ACTION_SENDU          },
   { "shade",                 ACTION_SHADE          },
   { "showdesktop",           ACTION_SHOWDESK       },
   { "showtray",              ACTION_SHOWTRAY       },
   { "stick",                 ACTION_STICK          },
   { "udesktop",              ACTION_UDESKTOP       },
   { "up",                    ACTION_UP             },
   { "window",                ACTION_WIN            }
};
static const unsigned int ACTION_MAP_COUNT = ARRAY_LENGTH(ACTION_MAP);

/** Mapping of key names to key types.
 * Note that this mapping must be sorted.
 */
static const StringMappingType MC_MAP[] = {
   { "border",                MC_BORDER   },
   { "close",                 MC_CLOSE    },
   { "icon",                  MC_ICON     },
   { "maximize",              MC_MAXIMIZE },
   { "minimize",              MC_MINIMIZE },
   { "root",                  MC_ROOT     },
   { "title",                 MC_MOVE     }
};
static const unsigned int MC_MAP_COUNT = ARRAY_LENGTH(MC_MAP);

/** Mapping of names to group options.
 * Note that this mapping must be sorted.
 */
static const StringMappingType OPTION_MAP[] = {
   { "aerosnap",           OPTION_AEROSNAP      },
   { "border",             OPTION_BORDER        },
   { "centered",           OPTION_CENTERED      },
   { "constrain",          OPTION_CONSTRAIN     },
   { "drag",               OPTION_DRAG          },
   { "fixed",              OPTION_FIXED         },
   { "fullscreen",         OPTION_FULLSCREEN    },
   { "hmax",               OPTION_MAX_H         },
   { "iignore",            OPTION_IIGNORE       },
   { "ilist",              OPTION_ILIST         },
   { "ipager",             OPTION_IPAGER        },
   { "maximized",          OPTION_MAXIMIZED     },
   { "minimized",          OPTION_MINIMIZED     },
   { "noborder",           OPTION_NOBORDER      },
   { "noclose",            OPTION_NOCLOSE       },
   { "nodrag",             OPTION_NODRAG        },
   { "nofocus",            OPTION_NOFOCUS       },
   { "nofullscreen",       OPTION_NOFULLSCREEN  },
   { "nolist",             OPTION_NOLIST        },
   { "nomax",              OPTION_NOMAX         },
   { "nomaxborder",        OPTION_NOMAXBORDER   },
   { "nomaxtitle",         OPTION_NOMAXTITLE    },
   { "nomin",              OPTION_NOMIN         },
   { "nomove",             OPTION_NOMOVE        },
   { "nopager",            OPTION_NOPAGER       },
   { "noresize",           OPTION_NORESIZE      },
   { "noshade",            OPTION_NOSHADE       },
   { "notitle",            OPTION_NOTITLE       },
   { "noturgent",          OPTION_NOTURGENT     },
   { "pignore",            OPTION_PIGNORE       },
   { "shaded",             OPTION_SHADED        },
   { "sticky",             OPTION_STICKY        },
   { "tiled",              OPTION_TILED         },
   { "title",              OPTION_TITLE         },
   { "vmax",               OPTION_MAX_V         }
};
static const unsigned int OPTION_MAP_COUNT = ARRAY_LENGTH(OPTION_MAP);

static const char *DEFAULT_TITLE = "JWM";
static const char *LABEL_ATTRIBUTE = "label";
static const char *ICON_ATTRIBUTE = "icon";
static const char *TOOLTIP_ATTRIBUTE = "tooltip";
static const char *CONFIRM_ATTRIBUTE = "confirm";
static const char *LABELED_ATTRIBUTE = "labeled";
static const char *ONROOT_ATTRIBUTE = "onroot";
static const char *X_ATTRIBUTE = "x";
static const char *Y_ATTRIBUTE = "y";
static const char *WIDTH_ATTRIBUTE = "width";
static const char *HEIGHT_ATTRIBUTE = "height";
static const char *SCREEN_ATTRIBUTE = "screen";
static const char *DYNAMIC_ATTRIBUTE = "dynamic";
static const char *SPACING_ATTRIBUTE = "spacing";
static const char *TIMEOUT_ATTRIBUTE = "timeout";
static const char *POPUP_ATTRIBUTE = "popup";
static const char *CLIENTNAME_ATTRIBUTE = "showclient";
static const char *CN_DELIMITERS_ATTRIBUTE = "delimiters";
static const char *KILL_MENU_ATTRIBUTE = "showkill";

static const char *FALSE_VALUE = "false";
static const char *TRUE_VALUE = "true";

static const char * const CONFIG_FILES[] = {
  "$XDG_CONFIG_HOME/jwm/jwmrc",
  "$HOME/.config/jwm/jwmrc",
  "$HOME/.jwmrc",
  SYSTEM_CONFIG
};
static const unsigned CONFIG_FILE_COUNT = ARRAY_LENGTH(CONFIG_FILES);

static void ParseInternal(const char *config);
static char ParseFile(const char *fileName, int depth);
static TokenNode *TokenizeFile(const char *fileName);
static TokenNode *TokenizePipe(const char *command, unsigned timeout_ms);

/* Misc. */
static void Parse(const TokenNode *start, int depth);
static void ParseInclude(const TokenNode *tp, int depth);
static void ParseDesktops(const TokenNode *tp);
static void ParseDesktop(int desktop, const TokenNode *tp);
static void ParseDesktopBackground(int desktop, const TokenNode *tp);

/* Menus. */
static Menu *ParseMenu(const TokenNode *start);
static void ParseRootMenu(const TokenNode *start);
static MenuItem *ParseMenuItem(const TokenNode *start, Menu *menu,
                               MenuItem *last);
static MenuItem *InsertMenuItem(MenuItem *last);
static MenuItem *ParseMenuInclude(const TokenNode *tp, Menu *menu,
                                  MenuItem *last);
static TokenNode *ParseMenuIncludeHelper(const TokenNode *tp,
                                         unsigned timeout_ms,
                                         const char *command);

/* Tray. */
typedef void (*AddTrayActionFunc)(TrayComponentType*, const char*, int);
static void ParseTray(const TokenNode *tp);
static void ParsePager(const TokenNode *tp, TrayType *tray);
static void ParseTaskList(const TokenNode *tp, TrayType *tray);
static void ParseSwallow(const TokenNode *tp, TrayType *tray);
static void ParseTrayButton(const TokenNode *tp, TrayType *tray);
static void ParseClock(const TokenNode *tp, TrayType *tray);
static void ParseTrayComponentActions(const TokenNode *tp,
                                      TrayComponentType *cp,
                                      AddTrayActionFunc func);
static void ParseDock(const TokenNode *tp, TrayType *tray);
static void ParseSpacer(const TokenNode *tp, TrayType *tray);

/* Groups. */
static void ParseGroup(const TokenNode *tp);
static void ParseGroupOption(const TokenNode *tp,
                             struct GroupType *group,
                             const char *option);

/* Style. */
static void ParseWindowStyle(const TokenNode *tp);
static void ParseActiveWindowStyle(const TokenNode *tp);
static void ParseTrayStyle(const TokenNode *tp,
                           FontType font, ColorType baseColor);
static void ParseActive(const TokenNode *tp, ColorType fg,
                        ColorType bg1, ColorType bg2,
                        ColorType up, ColorType down);
static void ParseMinimized(const TokenNode *tp, ColorType fg,
                        ColorType bg1, ColorType bg2,
                        ColorType up, ColorType down);
static void ParsePagerStyle(const TokenNode *tp);
static void ParseClockStyle(const TokenNode *tp);
static void ParseMenuStyle(const TokenNode *tp);
static void ParsePopupStyle(const TokenNode *tp);

/* Feel. */
static ActionType ParseAction(const char *str, const char **command);
static void ParseKey(const TokenNode *tp);
static void ParseMouse(const TokenNode *tp);
static void ParseSnapMode(const TokenNode *tp);
static void ParseMoveMode(const TokenNode *tp);
static void ParseResizeMode(const TokenNode *tp);
static void ParseFocusModel(const TokenNode *tp);

static AlignmentType ParseTextAlignment(const TokenNode *tp);
static void ParseDecorations(const TokenNode *tp, DecorationsType *deco);
static void ParseGradient(const char *value, ColorType a, ColorType b);
static char *FindAttribute(AttributeNode *ap, const char *name);
static int ParseTokenValue(const StringMappingType *mapping, int count,
                           const TokenNode *tp, int def);
static int ParseAttribute(const StringMappingType *mapping, int count,
                          const TokenNode *tp, const char *attr,
                          int def);
static int ParseSigned(const TokenNode *tp, const char *str);
static unsigned ParseUnsigned(const TokenNode *tp, const char *str);
static unsigned ParseTimeout(const TokenNode *tp, unsigned timeout_ms);
static unsigned int ParseOpacity(const TokenNode *tp, const char *str);
static WinLayerType ParseLayer(const TokenNode *tp, const char *str);
static StatusWindowType ParseStatusWindowType(const TokenNode *tp);
static void InvalidTag(const TokenNode *tp, TokenType parent);
static void ParseError(const TokenNode *tp, const char *str, ...);

/** Parse the default configuration. */
void ParseInternal(const char *config)
{
   TokenNode *tokens = Tokenize(config, "");
   Parse(tokens, 0);
   ReleaseTokens(tokens);
}

/** Parse the JWM configuration. */
void ParseConfig(const char *fileName)
{
   ParseInternal(BASE_CONFIG);
   if(fileName) {
      if(!ParseFile(fileName, 0)) {
         ParseError(NULL, _("could not open %s"), fileName);
         ParseInternal(DEFAULT_CONFIG);
      }
   } else {
      unsigned i;
      for(i = 0; i < CONFIG_FILE_COUNT; i++) {
         if(ParseFile(CONFIG_FILES[i], 0)) {
            goto ConfigFileFound;
         }
      }
      ParseError(NULL, _("could not open %s or %s"),
                 CONFIG_FILES[0], SYSTEM_CONFIG);
      ParseInternal(DEFAULT_CONFIG);
   }
ConfigFileFound:
   ValidateTrayButtons();
   ValidateKeys();
}

/**
 * Parse a specific file.
 * @return 1 on success and 0 on failure.
 */
char ParseFile(const char *fileName, int depth)
{
   TokenNode *tokens;

   depth += 1;
   if(JUNLIKELY(depth > MAX_INCLUDE_DEPTH)) {
      ParseError(NULL, _("include depth (%d) exceeded"),
                 MAX_INCLUDE_DEPTH);
      return 0;
   }

   tokens = TokenizeFile(fileName);
   if(!tokens) {
      return 0;
   }

   Parse(tokens, depth);
   ReleaseTokens(tokens);

   return 1;
}

/** Parse a token list. */
void Parse(const TokenNode *start, int depth)
{

   TokenNode *tp;

   if(!start) {
      return;
   }

   if(JLIKELY(start->type == TOK_JWM)) {
      for(tp = start->subnodeHead; tp; tp = tp->next) {
         if(shouldReload) {
            switch(tp->type) {
            case TOK_ROOTMENU:
               ParseRootMenu(tp);
               break;
            case TOK_INCLUDE:
               ParseInclude(tp, depth);
               break;
            default:
               break;
            }
         } else {
            switch(tp->type) {
            case TOK_DESKTOPS:
               ParseDesktops(tp);
               break;
            case TOK_DOUBLECLICKSPEED:
               settings.doubleClickSpeed = ParseUnsigned(tp, tp->value);
               break;
            case TOK_DOUBLECLICKDELTA:
               settings.doubleClickDelta = ParseUnsigned(tp, tp->value);
               break;
            case TOK_FOCUSMODEL:
               ParseFocusModel(tp);
               break;
            case TOK_GROUP:
               ParseGroup(tp);
               break;
            case TOK_ICONPATH:
               AddIconPath(tp->value);
               break;
            case TOK_INCLUDE:
               ParseInclude(tp, depth);
               break;
            case TOK_KEY:
               ParseKey(tp);
               break;
            case TOK_MOUSE:
               ParseMouse(tp);
               break;
            case TOK_MENUSTYLE:
               ParseMenuStyle(tp);
               break;
            case TOK_MOVEMODE:
               ParseMoveMode(tp);
               break;
            case TOK_PAGERSTYLE:
               ParsePagerStyle(tp);
               break;
            case TOK_POPUPSTYLE:
               ParsePopupStyle(tp);
               break;
            case TOK_RESIZEMODE:
               ParseResizeMode(tp);
               break;
            case TOK_RESTARTCOMMAND:
               AddRestartCommand(tp->value);
               break;
            case TOK_ROOTMENU:
               ParseRootMenu(tp);
               break;
            case TOK_SHUTDOWNCOMMAND:
               AddShutdownCommand(tp->value);
               break;
            case TOK_SNAPMODE:
               ParseSnapMode(tp);
               break;
            case TOK_STARTUPCOMMAND:
               AddStartupCommand(tp->value);
               break;
            case TOK_TRAY:
               ParseTray(tp);
               break;
            case TOK_TRAYSTYLE:
               ParseTrayStyle(tp, FONT_TRAY, COLOR_TRAY_FG);
               break;
            case TOK_TASKLISTSTYLE:
               ParseTrayStyle(tp, FONT_TASKLIST, COLOR_TASKLIST_FG);
               break;
            case TOK_TRAYBUTTONSTYLE:
               ParseTrayStyle(tp, FONT_TRAYBUTTON, COLOR_TRAYBUTTON_FG);
               break;
            case TOK_CLOCKSTYLE:
               ParseClockStyle(tp);
               break;
            case TOK_WINDOWSTYLE:
               ParseWindowStyle(tp);
               break;
            case TOK_BUTTONCLOSE:
               SetBorderIcon(BI_CLOSE, tp->value);
               break;
            case TOK_BUTTONCLOSEFOCUS:
               SetBorderIcon(BI_CLOSE_FOCUS, tp->value);
               break;
            case TOK_BUTTONMAX:
               SetBorderIcon(BI_MAX, tp->value);
               break;
            case TOK_BUTTONMAXACTIVE:
               SetBorderIcon(BI_MAX_ACTIVE, tp->value);
               break;
            case TOK_BUTTONMAXACTIVEFOCUS:
               SetBorderIcon(BI_MAX_ACTIVE_FOCUS, tp->value);
               break;
            case TOK_BUTTONMAXFOCUS:
               SetBorderIcon(BI_MAX_FOCUS, tp->value);
               break;
            case TOK_BUTTONMIN:
               SetBorderIcon(BI_MIN, tp->value);
               break;
            case TOK_BUTTONMINFOCUS:
               SetBorderIcon(BI_MIN_FOCUS, tp->value);
               break;
            case TOK_BUTTONMENU:
               SetBorderIcon(BI_MENU, tp->value);
               break;
            case TOK_BUTTONMENUFOCUS:
               SetBorderIcon(BI_MENU_FOCUS, tp->value);
               break;
            case TOK_DEFAULTICON:
               SetDefaultIcon(tp->value);
               break;
            case TOK_TITLEBUTTONORDER:
               SetTitleButtonOrder(tp->value);
               break;
            default:
               InvalidTag(tp, TOK_JWM);
               break;
            }
         }
      }
   } else {
      ParseError(start, _("invalid start tag: %s"), GetTokenName(start));
   }

}

/** Parse focus model. */
void ParseFocusModel(const TokenNode *tp) {
   static const StringMappingType mapping[] = {
      { "click",        FOCUS_CLICK          },
      { "clicktitle",   FOCUS_CLICK_TITLE    },
      { "sloppy",       FOCUS_SLOPPY         },
      { "sloppytitle",  FOCUS_SLOPPY_TITLE   }
   };
   settings.focusModel = ParseTokenValue(mapping, ARRAY_LENGTH(mapping), tp,
                                         settings.focusModel);
}

/** Parse snap mode for moving windows. */
void ParseSnapMode(const TokenNode *tp)
{
   const char *distance;
   static const StringMappingType mapping[] = {
      { "border", SNAP_BORDER },
      { "none",   SNAP_NONE   },
      { "screen", SNAP_SCREEN }
   };

   distance = FindAttribute(tp->attributes, "distance");
   if(distance) {
      settings.snapDistance = ParseUnsigned(tp, distance);
   }
   settings.snapMode = ParseTokenValue(mapping, ARRAY_LENGTH(mapping), tp,
                                       settings.snapMode);
}

/** Parse move mode. */
void ParseMoveMode(const TokenNode *tp)
{
   const char *str;
   static const StringMappingType mapping[] = {
      { "opaque",    MOVE_OPAQUE    },
      { "outline",   MOVE_OUTLINE   }
   };

   str = FindAttribute(tp->attributes, "delay");
   if(str) {
      settings.desktopDelay = ParseUnsigned(tp, str);
   }
   str = FindAttribute(tp->attributes, "mask");
   if(str && *str) {
      settings.moveMask = ParseModifierString(str);
   }

   settings.moveStatusType = ParseStatusWindowType(tp);
   settings.moveMode = ParseTokenValue(mapping, ARRAY_LENGTH(mapping), tp,
                                       settings.moveMode);
}

/** Parse resize mode. */
void ParseResizeMode(const TokenNode *tp)
{
   static const StringMappingType mapping[] = {
      { "opaque",    RESIZE_OPAQUE  },
      { "outline",   RESIZE_OUTLINE }
   };
   settings.resizeStatusType = ParseStatusWindowType(tp);
   settings.resizeMode = ParseTokenValue(mapping, ARRAY_LENGTH(mapping), tp,
                                         settings.resizeMode);
}

/** Parse a menu. */
Menu *ParseMenu(const TokenNode *start)
{
   const char *value;
   Menu *menu;

   menu = CreateMenu();

   value = FindAttribute(start->attributes, HEIGHT_ATTRIBUTE);
   if(value) {
      menu->itemHeight = ParseUnsigned(start, value);
   } else {
      menu->itemHeight = 0;
   }

   value = FindAttribute(start->attributes, LABELED_ATTRIBUTE);
   if(value && !strcmp(value, TRUE_VALUE)) {
      value = FindAttribute(start->attributes, LABEL_ATTRIBUTE);
      if(!value) {
         value = DEFAULT_TITLE;
      }
      menu->label = CopyString(value);
   } else {
      menu->label = NULL;
   }

   menu->items = NULL;
   ParseMenuItem(start->subnodeHead, menu, NULL);

   return menu;

}

/** Parse a root menu. */
void ParseRootMenu(const TokenNode *start)
{
   Menu *menu;
   char *onroot;
   const char *value;

   menu = ParseMenu(start);

   onroot = FindAttribute(start->attributes, ONROOT_ATTRIBUTE);
   if(!onroot) {
      onroot = "123";
   }

   value = FindAttribute(start->attributes, DYNAMIC_ATTRIBUTE);
   menu->dynamic = CopyString(value);
   menu->timeout_ms = ParseTimeout(start, MENU_TIMEOUT_MS);

   SetRootMenu(onroot, menu);
}

/** Insert a new menu item into a menu. */
MenuItem *InsertMenuItem(MenuItem *last)
{
   MenuItem *item = CreateMenuItem(MENU_ITEM_NORMAL);
   if(last) {
      last->next = item;
   }
   return item;
}

/** Parse a menu item. */
MenuItem *ParseMenuItem(const TokenNode *start, Menu *menu, MenuItem *last)
{

   Menu *child;
   const char *value;

   Assert(menu);

   menu->offsets = NULL;
   while(start) {
      switch(start->type) {
      case TOK_DYNAMIC:

         last = InsertMenuItem(last);
         if(!menu->items) {
            menu->items = last;
         }

         value = FindAttribute(start->attributes, LABEL_ATTRIBUTE);
         last->name = CopyString(value);

         value = FindAttribute(start->attributes, ICON_ATTRIBUTE);
         last->iconName = CopyString(value);

         value = FindAttribute(start->attributes, TOOLTIP_ATTRIBUTE);
         last->tooltip = CopyString(value);

         last->action.type = MA_DYNAMIC;
         last->action.str = CopyString(start->value);
         last->action.timeout_ms = ParseTimeout(start, MENU_TIMEOUT_MS);

         value = FindAttribute(start->attributes, HEIGHT_ATTRIBUTE);
         if(value) {
            last->action.value = ParseUnsigned(start, value);
         } else {
            last->action.value = menu->itemHeight;
         }

         break;
      case TOK_MENU:

         last = InsertMenuItem(last);
         last->type = MENU_ITEM_SUBMENU;
         if(!menu->items) {
            menu->items = last;
         }

         value = FindAttribute(start->attributes, LABEL_ATTRIBUTE);
         last->name = CopyString(value);

         value = FindAttribute(start->attributes, ICON_ATTRIBUTE);
         last->iconName = CopyString(value);

         value = FindAttribute(start->attributes, TOOLTIP_ATTRIBUTE);
         last->tooltip = CopyString(value);

         last->submenu = CreateMenu();
         child = last->submenu;

         value = FindAttribute(start->attributes, HEIGHT_ATTRIBUTE);
         if(value) {
            child->itemHeight = ParseUnsigned(start, value);
         } else {
            child->itemHeight = menu->itemHeight;
         }

         value = FindAttribute(start->attributes, LABELED_ATTRIBUTE);
         if(value && !strcmp(value, TRUE_VALUE)) {
            if(last->name) {
               child->label = CopyString(last->name);
            } else {
               child->label = CopyString(DEFAULT_TITLE);
            }
         } else {
            child->label = NULL;
         }

         last->submenu->items = NULL;
         ParseMenuItem(start->subnodeHead, last->submenu, NULL);

         break;
      case TOK_PROGRAM:

         last = InsertMenuItem(last);
         if(!menu->items) {
            menu->items = last;
         }

         value = FindAttribute(start->attributes, LABEL_ATTRIBUTE);
         if(value) {
            last->name = CopyString(value);
         } else if(start->value) {
            last->name = CopyString(start->value);
         }

         value = FindAttribute(start->attributes, TOOLTIP_ATTRIBUTE);
         last->tooltip = CopyString(value);

         value = FindAttribute(start->attributes, ICON_ATTRIBUTE);
         last->iconName = CopyString(value);

         last->action.type = MA_EXECUTE;
         last->action.str = CopyString(start->value);

         break;
      case TOK_SEPARATOR:

         last = InsertMenuItem(last);
         last->type = MENU_ITEM_SEPARATOR;
         if(!menu->items) {
            menu->items = last;
         }

         break;
      case TOK_INCLUDE:
         last = ParseMenuInclude(start, menu, last);
         break;
      case TOK_DESKTOPS:
      case TOK_STICK:
      case TOK_MAXIMIZE:
      case TOK_MINIMIZE:
      case TOK_SHADE:
      case TOK_MOVE:
      case TOK_RESIZE:
      case TOK_KILL:
      case TOK_CLOSE:
      case TOK_SENDTO:

         last = InsertMenuItem(last);
         if(!menu->items) {
            menu->items = last;
         }

         value = FindAttribute(start->attributes, LABEL_ATTRIBUTE);
         if(!value) {
            value = GetTokenName(start);
         }
         last->name = CopyString(value);

         value = FindAttribute(start->attributes, TOOLTIP_ATTRIBUTE);
         last->tooltip = CopyString(value);

         value = FindAttribute(start->attributes, ICON_ATTRIBUTE);
         last->iconName = CopyString(value);

         switch(start->type) {
         case TOK_DESKTOPS:
            last->action.type = MA_DESKTOP_MENU;
            break;
         case TOK_STICK:
            last->action.type = MA_STICK;
            break;
         case TOK_MAXIMIZE:
            last->action.type = MA_MAXIMIZE;
            break;
         case TOK_MINIMIZE:
            last->action.type = MA_MINIMIZE;
            break;
         case TOK_SHADE:
            last->action.type = MA_SHADE;
            break;
         case TOK_MOVE:
            last->action.type = MA_MOVE;
            break;
         case TOK_RESIZE:
            last->action.type = MA_RESIZE;
            break;
         case TOK_KILL:
            last->action.type = MA_KILL;
            break;
         case TOK_CLOSE:
            last->action.type = MA_CLOSE;
            break;
         case TOK_SENDTO:
            last->action.type = MA_SENDTO_MENU;
            break;
         default:
            break;
         }

         break;
      case TOK_EXIT:

         last = InsertMenuItem(last);
         if(!menu->items) {
            menu->items = last;
         }

         value = FindAttribute(start->attributes, LABEL_ATTRIBUTE);
         if(!value) {
            value = GetTokenName(start);
         }
         last->name = CopyString(value);

         value = FindAttribute(start->attributes, TOOLTIP_ATTRIBUTE);
         last->tooltip = CopyString(value);

         value = FindAttribute(start->attributes, ICON_ATTRIBUTE);
         last->iconName = CopyString(value);

         last->action.type = MA_EXIT;
         last->action.str = CopyString(start->value);
         value = FindAttribute(start->attributes, CONFIRM_ATTRIBUTE);
         if(value && !strcmp(value, FALSE_VALUE)) {
            last->action.value = 0;
         } else {
            last->action.value = 1;
         }

         break;
      case TOK_RESTART:

         last = InsertMenuItem(last);
         if(!menu->items) {
            menu->items = last;
         }

         value = FindAttribute(start->attributes, LABEL_ATTRIBUTE);
         if(!value) {
            value = GetTokenName(start);
         }
         last->name = CopyString(value);

         value = FindAttribute(start->attributes, TOOLTIP_ATTRIBUTE);
         last->tooltip = CopyString(value);

         value = FindAttribute(start->attributes, ICON_ATTRIBUTE);
         last->iconName = CopyString(value);

         last->action.type = MA_RESTART;

         break;
      default:
         InvalidTag(start, TOK_MENU);
         break;
      }
      start = start->next;
   }

   return last;

}

/** Get tokens from a menu include (either dynamic or static). */
TokenNode *ParseMenuIncludeHelper(const TokenNode *tp,
                                  unsigned timeout_ms,
                                  const char *command)
{
   TokenNode *start;
   if(!strncmp(command, "exec:", 5)) {
      start = TokenizePipe(&command[5], timeout_ms);
   } else {
      start = TokenizeFile(command);
   }

   if(JUNLIKELY(!start || start->type != TOK_JWM))
   {
      ParseError(tp, _("invalid include: %s"), command);
      ReleaseTokens(start);
      return NULL;
   }

   return start;
}

/** Parse a menu include. */
MenuItem *ParseMenuInclude(const TokenNode *tp, Menu *menu,
                           MenuItem *last)
{
   TokenNode *start;
   const unsigned timeout_ms = ParseTimeout(tp, INCLUDE_TIMEOUT_MS);

   start = ParseMenuIncludeHelper(tp, timeout_ms, tp->value);
   if(JLIKELY(start)) {
      last = ParseMenuItem(start->subnodeHead, menu, last);
      ReleaseTokens(start);
   }
   return last;
}

/** Parse a dynamic menu (called from menu code). */
Menu *ParseDynamicMenu(unsigned timeout_ms, const char *command)
{
   Menu *menu = NULL;
   TokenNode *start = ParseMenuIncludeHelper(NULL, timeout_ms, command);
   if(JLIKELY(start)) {
      menu = ParseMenu(start);
      ReleaseTokens(start);
   }
   return menu;
}

/** Parse an action. */
ActionType ParseAction(const char *str, const char **command)
{
   ActionType action;
   action.action = ACTION_INVALID;
   action.extra = 0;
   *command = NULL;
   if(!strncmp(str, "exec:", 5)) {
      action.action = ACTION_EXEC;
      *command = str + 5;
   } else if(!strncmp(str, "root:", 5)) {
      action.action = ACTION_ROOT;
      *command = str + 5;
   } else if(!strncmp(str, "resize:", 7)) {
      int i;
      action.action = ACTION_RESIZE;
      for(i = 7; str[i]; i++) {
         switch(str[i]) {
         case 'n': action.extra |= ACTION_RESIZE_N; break;
         case 's': action.extra |= ACTION_RESIZE_S; break;
         case 'e': action.extra |= ACTION_RESIZE_E; break;
         case 'w': action.extra |= ACTION_RESIZE_W; break;
         default: break;
         }
      }
   } else {
      /* Look up the option in the key map using binary search. */
      const int x = FindValue(ACTION_MAP, ACTION_MAP_COUNT, str);
      if(x >= 0) {
         action.action = (unsigned char)x;
      }
   }
   return action;
}

/** Parse a key binding. */
void ParseKey(const TokenNode *tp)
{
   const char *key;
   const char *code;
   const char *mask;
   const char *action;
   const char *command;
   ActionType k;

   mask = FindAttribute(tp->attributes, "mask");
   key = FindAttribute(tp->attributes, "key");
   code = FindAttribute(tp->attributes, "keycode");

   action = tp->value;
   if(JUNLIKELY(action == NULL)) {
      ParseError(tp, _("no action specified for Key"));
      return;
   }

   /* Insert the binding if it's valid. */
   k = ParseAction(action, &command);
   if(JUNLIKELY(k.action == ACTION_INVALID)) {
      ParseError(tp, _("invalid Key action: \"%s\""), action);
   } else {
      InsertBinding(k, mask, key, code, command);
   }
}

/** Parse a mouse binding. */
void ParseMouse(const TokenNode *tp)
{
   const char *mask;
   const char *context;
   const char *action;
   const char *command;
   int button;
   int x;
   ActionType key;

   button = ParseSigned(tp, FindAttribute(tp->attributes, "button"));
   if(JUNLIKELY(button == 0)) {
      return;
   }
   mask = FindAttribute(tp->attributes, "mask");
   context = FindAttribute(tp->attributes, "context");

   action = tp->value;
   if(JUNLIKELY(action == NULL)) {
      ParseError(tp, _("no action specified for Mouse"));
      return;
   }
   key = ParseAction(action, &command);
   if(JUNLIKELY(key.action == ACTION_INVALID)) {
      ParseError(tp, _("invalid Mouse action: \"%s\""), action);
      return;
   }

   x = FindValue(MC_MAP, MC_MAP_COUNT, context);
   if(JUNLIKELY(x < 0)) {
      ParseError(tp, _("invalid Mouse context: \"%s\""), context);
   } else {
      const MouseContextType mc = (MouseContextType)x;
      InsertMouseBinding(button, mask, mc, key, command);
   }
}

/** Parse text alignment. */
AlignmentType ParseTextAlignment(const TokenNode *tp)
{
   static const StringMappingType mapping[] = {
      {"center",  ALIGN_CENTER },
      {"left",    ALIGN_LEFT   },
      {"right",   ALIGN_RIGHT  }
   };
   const char *attr= FindAttribute(tp->attributes, "align");
   if(attr) {
      const int x = FindValue(mapping, ARRAY_LENGTH(mapping), attr);
      if(JLIKELY(x >= 0)) {
         return x;
      } else {
         Warning(_("invalid text alignment: \"%s\""), attr);
      }
   }

   return ALIGN_LEFT;
}

/** Parse window style. */
void ParseWindowStyle(const TokenNode *tp)
{
   const TokenNode *np;

   const char *temp;
   temp = FindAttribute(tp->attributes, CLIENTNAME_ATTRIBUTE);
   if(temp){
        settings.showClientName = !strcmp(temp,TRUE_VALUE);
   }

   temp = FindAttribute(tp->attributes, CN_DELIMITERS_ATTRIBUTE);
   if(temp && strlen(temp)>=2) {
      memcpy(settings.clientNameDelimiters, temp,
          sizeof(settings.clientNameDelimiters));
   }

   ParseDecorations(tp, &settings.windowDecorations);
   for(np = tp->subnodeHead; np; np = np->next) {
      switch(np->type) {
      case TOK_FONT:
         SetFont(FONT_BORDER, np->value);
         settings.titleTextAlignment = ParseTextAlignment(np);
         break;
      case TOK_WIDTH:
         settings.borderWidth = ParseUnsigned(np, np->value);
         break;
      case TOK_HEIGHT:
         settings.titleHeight = ParseUnsigned(np, np->value);
         break;
      case TOK_CORNER:
         settings.cornerRadius = ParseUnsigned(np, np->value);
         break;
      case TOK_ACTIVE:
         ParseActiveWindowStyle(np);
         break;
      case TOK_FOREGROUND:
         SetColor(COLOR_TITLE_FG, np->value);
         break;
      case TOK_BACKGROUND:
         ParseGradient(np->value, COLOR_TITLE_BG1, COLOR_TITLE_BG2);
         break;
      case TOK_OUTLINE:
         ParseGradient(np->value, COLOR_TITLE_DOWN, COLOR_TITLE_UP);
         break;
      case TOK_OPACITY:
         settings.inactiveClientOpacity = ParseOpacity(tp, np->value);
         break;
      default:
         InvalidTag(np, TOK_WINDOWSTYLE);
         break;
      }
   }
}

/** Parse active window style information. */
void ParseActiveWindowStyle(const TokenNode *tp)
{

   const TokenNode *np;

   Assert(tp);

   for(np = tp->subnodeHead; np; np = np->next) {
      switch(np->type) {
      case TOK_FOREGROUND:
         SetColor(COLOR_TITLE_ACTIVE_FG, np->value);
         break;
      case TOK_BACKGROUND:
         ParseGradient(np->value, COLOR_TITLE_ACTIVE_BG1,
                       COLOR_TITLE_ACTIVE_BG2);
         break;
      case TOK_OUTLINE:
         ParseGradient(np->value, COLOR_TITLE_ACTIVE_DOWN,
                       COLOR_TITLE_ACTIVE_UP);
         break;
      case TOK_OPACITY:
         settings.activeClientOpacity = ParseOpacity(np, np->value);
         break;
      default:
         InvalidTag(np, TOK_ACTIVE);
         break;
      }
   }

}

/** Parse an include. */
void ParseInclude(const TokenNode *tp, int depth)
{
   unsigned timeout_ms;

   if(JUNLIKELY(!tp->value)) {
      ParseError(tp, _("no include file specified"));
      return;
   }

   timeout_ms = ParseTimeout(tp, INCLUDE_TIMEOUT_MS);
   if(!strncmp(tp->value, "exec:", 5)) {
      TokenNode *tokens = TokenizePipe(&tp->value[5], timeout_ms);
      if(JLIKELY(tokens)) {
         Parse(tokens, 0);
         ReleaseTokens(tokens);
      } else {
         ParseError(tp, _("could not process include: %s"), &tp->value[5]);
      }
   } else {
      if(JUNLIKELY(!ParseFile(tp->value, depth))) {
         ParseError(tp, _("could not open included file: %s"), tp->value);
      }
   }

}

/** Parse desktop configuration. */
void ParseDesktops(const TokenNode *tp) {
   
   static const StringMappingType mapping[] = {
      { "off",       DBACKANDFORTH_OFF },
      { "on",        DBACKANDFORTH_ON  }
   };
   TokenNode *np;
   const char *width;
   const char *height;
   int desktop;
   DesktopBackAndForthType backandforth;

   Assert(tp);

   backandforth = ParseAttribute(mapping, ARRAY_LENGTH(mapping), tp,
                                 "backandforth", DBACKANDFORTH_OFF);
   settings.desktopBackAndForth = backandforth;

   width = FindAttribute(tp->attributes, WIDTH_ATTRIBUTE);
   if(width != NULL) {
      settings.desktopWidth = ParseUnsigned(tp, width);
   }
   height = FindAttribute(tp->attributes, HEIGHT_ATTRIBUTE);
   if(height != NULL) {
      settings.desktopHeight = ParseUnsigned(tp, height);
   }
   settings.desktopCount = settings.desktopWidth * settings.desktopHeight;

   desktop = 0;
   for(np = tp->subnodeHead; np; np = np->next) {
      if(desktop >= settings.desktopCount) {
         break;
      }
      switch(np->type) {
      case TOK_BACKGROUND:
         ParseDesktopBackground(-1, np);
         break;
      case TOK_DESKTOP:
         ParseDesktop(desktop, np);
         desktop += 1;
         break;
      default:
         InvalidTag(np, TOK_DESKTOPS);
         break;
      }
   }

}

/** Parse a configuration for a specific desktop. */
void ParseDesktop(int desktop, const TokenNode *tp) {

   TokenNode *np;
   const char *attr;

   attr = FindAttribute(tp->attributes, "name");
   if(attr) {
      SetDesktopName(desktop, attr);
   }

   for(np = tp->subnodeHead; np; np = np->next) {
      switch(np->type) {
      case TOK_BACKGROUND:
         ParseDesktopBackground(desktop, np);
         break;
      default:
         InvalidTag(np, TOK_DESKTOP);
         break;
      }
   }

}

/** Parse a background for a desktop. */
void ParseDesktopBackground(int desktop, const TokenNode *tp)
{
   const char *type = FindAttribute(tp->attributes, "type");
   SetBackground(desktop, type, tp->value);
}

/** Parse tray style. */
void ParseTrayStyle(const TokenNode *tp, FontType font, ColorType fg)
{
   const TokenNode *np;
   const ColorType bg1 = fg + COLOR_TRAY_BG1 - COLOR_TRAY_FG;
   const ColorType bg2 = fg + COLOR_TRAY_BG2 - COLOR_TRAY_FG;
   const ColorType activeFg = fg + COLOR_TRAY_ACTIVE_FG - COLOR_TRAY_FG;
   const ColorType activeBg1 = fg + COLOR_TRAY_ACTIVE_BG1 - COLOR_TRAY_FG;
   const ColorType activeBg2 = fg + COLOR_TRAY_ACTIVE_BG2 - COLOR_TRAY_FG;
   const ColorType minimizedFg = fg + COLOR_TASKLIST_MINIMIZED_FG - COLOR_TASKLIST_FG;
   const ColorType minimizedBg1 = fg + COLOR_TASKLIST_MINIMIZED_BG1 - COLOR_TASKLIST_FG;
   const ColorType minimizedBg2 = fg + COLOR_TASKLIST_MINIMIZED_BG2 - COLOR_TASKLIST_FG;
   const ColorType up = fg + COLOR_TRAY_UP - COLOR_TRAY_FG;
   const ColorType down = fg + COLOR_TRAY_DOWN - COLOR_TRAY_FG;
   const ColorType activeUp = fg + COLOR_TRAY_ACTIVE_UP - COLOR_TRAY_FG;
   const ColorType activeDown = fg + COLOR_TRAY_ACTIVE_DOWN - COLOR_TRAY_FG;
   const ColorType minimizedUp = fg + COLOR_TASKLIST_MINIMIZED_UP - COLOR_TASKLIST_FG;
   const ColorType minimizedDown = fg + COLOR_TASKLIST_MINIMIZED_DOWN - COLOR_TASKLIST_FG;

   /* TaskListStyle has extra attributes. */
   if(tp->type == TOK_TASKLISTSTYLE) {
      const char *temp;
      ParseDecorations(tp, &settings.taskListDecorations);
      temp = FindAttribute(tp->attributes, "group");
      if(temp) {
         settings.groupTasks = !strcmp(temp, TRUE_VALUE);
      }

      temp = FindAttribute(tp->attributes, "list");
      if(temp) {
         settings.listAllTasks = !strcmp(temp, "all");
      }

      temp = FindAttribute(tp->attributes, KILL_MENU_ATTRIBUTE);
      if(temp) {
         settings.showKillMenuItem = !strcmp(temp, TRUE_VALUE);
      }

   } else if(tp->type == TOK_TRAYSTYLE) {
      ParseDecorations(tp, &settings.trayDecorations);
   }

   for(np = tp->subnodeHead; np; np = np->next) {
      switch(np->type) {
      case TOK_FONT:
         SetFont(font, np->value);
         break;
      case TOK_ACTIVE:
         ParseActive(np, activeFg, activeBg1, activeBg2, activeUp, activeDown);
         break;
      case TOK_MINIMIZED:
         ParseMinimized(np, minimizedFg, minimizedBg1, minimizedBg2,
                        minimizedUp, minimizedDown);
         break;
      case TOK_BACKGROUND:
         ParseGradient(np->value, bg1, bg2);
         break;
      case TOK_FOREGROUND:
         SetColor(fg, np->value);
         break;
      case TOK_OUTLINE:
         ParseGradient(np->value, down, up);
         break;
      case TOK_OPACITY:
         if(tp->type == TOK_TRAYSTYLE) {
            settings.trayOpacity = ParseOpacity(np, np->value);
            break;
         }
         /* fall through */
      default:
         InvalidTag(np, tp->type);
         break;
      }
   }

}

/** Parse active tray style. */
void ParseActive(const TokenNode *tp, ColorType fg,
                 ColorType bg1, ColorType bg2,
                 ColorType up, ColorType down)
{
   const TokenNode *np;

   for(np = tp->subnodeHead; np; np = np->next) {
      switch(np->type) {
      case TOK_BACKGROUND:
         if(bg1 == bg2) {
            SetColor(bg1, np->value);
         } else {
            ParseGradient(np->value, bg1, bg2);
         }
         break;
      case TOK_FOREGROUND:
         SetColor(fg, np->value);
         break;
      case TOK_OUTLINE:
         if(up != COLOR_COUNT) {
            ParseGradient(np->value, down, up);
            break;
         }
         /* fall through */
      default:
         InvalidTag(np, TOK_ACTIVE);
         break;
      }
   }
}

/** Parse minimized tray style. */
void ParseMinimized(const TokenNode *tp, ColorType fg,
                 ColorType bg1, ColorType bg2,
                 ColorType up, ColorType down)
{
   const TokenNode *np;

   for(np = tp->subnodeHead; np; np = np->next) {
      switch(np->type) {
      case TOK_BACKGROUND:
         if(bg1 == bg2) {
            SetColor(bg1, np->value);
         } else {
            ParseGradient(np->value, bg1, bg2);
         }
         break;
      case TOK_FOREGROUND:
         SetColor(fg, np->value);
         break;
      case TOK_OUTLINE:
         if(up != COLOR_COUNT) {
            ParseGradient(np->value, down, up);
            break;
         }
         /* fall through */
      default:
         InvalidTag(np, TOK_MINIMIZED);
         break;
      }
   }
}

/** Parse tray. */
void ParseTray(const TokenNode *tp)
{
   static const StringMappingType mapping[] = {
      { "bottom",    THIDE_BOTTOM      },
      { "invisible", THIDE_INVISIBLE   },
      { "left",      THIDE_LEFT        },
      { "off",       THIDE_OFF         },
      { "on",        THIDE_ON          },
      { "right",     THIDE_RIGHT       },
      { "top",       THIDE_TOP         }
   };
   const TokenNode *np;
   const char *attr;
   unsigned delay = 0;
   TrayType *tray;
   TrayAutoHideType autohide;

   Assert(tp);

   tray = CreateTray();

   autohide = ParseAttribute(mapping, ARRAY_LENGTH(mapping), tp,
                             "autohide", THIDE_OFF);
   attr = FindAttribute(tp->attributes, "delay");
   if(attr) {
      delay = ParseUnsigned(tp, attr);
   }
   SetAutoHideTray(tray, autohide, delay);

   attr = FindAttribute(tp->attributes, X_ATTRIBUTE);
   if(attr) {
      SetTrayX(tray, attr);
   }

   attr = FindAttribute(tp->attributes, Y_ATTRIBUTE);
   if(attr) {
      SetTrayY(tray, attr);
   }

   attr = FindAttribute(tp->attributes, WIDTH_ATTRIBUTE);
   if(attr) {
      SetTrayWidth(tray, attr);
   }

   attr = FindAttribute(tp->attributes, HEIGHT_ATTRIBUTE);
   if(attr) {
      SetTrayHeight(tray, attr);
   }

   attr = FindAttribute(tp->attributes, SCREEN_ATTRIBUTE);
   if(attr) {
      SetTrayScreen(tray, attr);
   }

   attr = FindAttribute(tp->attributes, "valign");
   SetTrayVerticalAlignment(tray, attr);

   attr = FindAttribute(tp->attributes, "halign");
   SetTrayHorizontalAlignment(tray, attr);

   attr = FindAttribute(tp->attributes, "layout");
   SetTrayLayout(tray, attr);

   attr = FindAttribute(tp->attributes, "layer");
   if(attr) {
      SetTrayLayer(tray, ParseLayer(tp, attr));
   }

   for(np = tp->subnodeHead; np; np = np->next) {
      switch(np->type) {
      case TOK_PAGER:
         ParsePager(np, tray);
         break;
      case TOK_TASKLIST:
         ParseTaskList(np, tray);
         break;
      case TOK_SWALLOW:
         ParseSwallow(np, tray);
         break;
      case TOK_TRAYBUTTON:
         ParseTrayButton(np, tray);
         break;
      case TOK_CLOCK:
         ParseClock(np, tray);
         break;
      case TOK_DOCK:
         ParseDock(np, tray);
         break;
      case TOK_SPACER:
         ParseSpacer(np, tray);
         break;
      default:
         InvalidTag(np, TOK_TRAY);
         break;
      }
   }

}

/** Parse a pager tray component. */
void ParsePager(const TokenNode *tp, TrayType *tray)
{

   TrayComponentType *cp;
   const char *temp;
   int labeled;

   Assert(tp);
   Assert(tray);

   labeled = 0;
   temp = FindAttribute(tp->attributes, LABELED_ATTRIBUTE);
   if(temp && !strcmp(temp, TRUE_VALUE)) {
      labeled = 1;
   }
   cp = CreatePager(labeled);
   AddTrayComponent(tray, cp);

}

/** Parse a task list tray component. */
void ParseTaskList(const TokenNode *tp, TrayType *tray)
{
   TrayComponentType *cp;
   const char *temp;

   Assert(tp);
   Assert(tray);

   cp = CreateTaskBar();
   AddTrayComponent(tray, cp);

   temp = FindAttribute(tp->attributes, "maxwidth");
   if(temp) {
      SetMaxTaskBarItemWidth(cp, temp);
   }

   temp = FindAttribute(tp->attributes, "height");
   if(temp) {
      SetTaskBarHeight(cp, temp);
   }

   temp = FindAttribute(tp->attributes, "labeled");
   if(temp && !strcmp(temp, FALSE_VALUE)) {
      SetTaskBarLabeled(cp, 0);
   }

   temp = FindAttribute(tp->attributes, "labelpos");
   if(temp) {
      SetTaskBarLabelPosition(cp, temp);
   }

}

/** Parse the tray button style. */
void ParseTrayButtonStyle(const TokenNode *tp)
{
   const TokenNode *np;
   for(np = tp->subnodeHead; np; np = np->next) {
      switch(np->type) {
      case TOK_FONT:
         SetFont(FONT_TRAYBUTTON, np->value);
         break;
      case TOK_ACTIVE:
         ParseActive(np, COLOR_TRAYBUTTON_ACTIVE_FG,
                     COLOR_TRAYBUTTON_ACTIVE_BG1, COLOR_TRAYBUTTON_ACTIVE_BG2,
                     COLOR_TRAYBUTTON_ACTIVE_UP, COLOR_TRAYBUTTON_ACTIVE_DOWN);
         break;
      case TOK_BACKGROUND:
         ParseGradient(np->value, COLOR_TRAYBUTTON_BG1, COLOR_TRAYBUTTON_BG2);
         break;
      case TOK_FOREGROUND:
         SetColor(COLOR_TRAYBUTTON_FG, np->value);
         break;
      case TOK_OUTLINE:
         ParseGradient(np->value, COLOR_TRAYBUTTON_DOWN, COLOR_TRAYBUTTON_UP);
         break;
      default:
         InvalidTag(np, TOK_TRAYBUTTONSTYLE);
         break;
      }
   }
}

/** Parse a swallow tray component. */
void ParseSwallow(const TokenNode *tp, TrayType *tray)
{

   TrayComponentType *cp;
   const char *name;
   const char *temp;
   int width, height;

   Assert(tp);
   Assert(tray);

   name = FindAttribute(tp->attributes, "name");
   if(name == NULL) {
      name = tp->value;
   }

   temp = FindAttribute(tp->attributes, WIDTH_ATTRIBUTE);
   if(temp) {
      width = ParseUnsigned(tp, temp);
   } else {
      width = 0;
   }

   temp = FindAttribute(tp->attributes, HEIGHT_ATTRIBUTE);
   if(temp) {
      height = ParseUnsigned(tp, temp);
   } else {
      height = 0;
   }

   cp = CreateSwallow(name, tp->value, width, height);
   if(cp) {
      AddTrayComponent(tray, cp);
   }

}

/** Parse a button tray component. */
void ParseTrayButton(const TokenNode *tp, TrayType *tray)
{

   TrayComponentType *cp;
   const char *icon;
   const char *label;
   const char *popup;
   const char *temp;
   unsigned int width, height;

   Assert(tp);
   Assert(tray);

   icon = FindAttribute(tp->attributes, ICON_ATTRIBUTE);
   label = FindAttribute(tp->attributes, LABEL_ATTRIBUTE);
   popup = FindAttribute(tp->attributes, POPUP_ATTRIBUTE);

   temp = FindAttribute(tp->attributes, WIDTH_ATTRIBUTE);
   if(temp) {
      width = ParseUnsigned(tp, temp);
   } else {
      width = 0;
   }

   temp = FindAttribute(tp->attributes, HEIGHT_ATTRIBUTE);
   if(temp) {
      height = ParseUnsigned(tp, temp);
   } else {
      height = 0;
   }

   cp = CreateTrayButton(icon, label, popup, width, height);
   if(JLIKELY(cp)) {
      AddTrayComponent(tray, cp);
      ParseTrayComponentActions(tp, cp, AddTrayButtonAction);
   }

}

/** Parse a clock tray component. */
void ParseClock(const TokenNode *tp, TrayType *tray)
{
   TrayComponentType *cp;
   const char *format;
   const char *zone;
   const char *temp;
   int width, height;

   Assert(tp);
   Assert(tray);

   format = FindAttribute(tp->attributes, "format");
   zone = FindAttribute(tp->attributes, "zone");

   temp = FindAttribute(tp->attributes, WIDTH_ATTRIBUTE);
   if(temp) {
      width = ParseUnsigned(tp, temp);
   } else {
      width = 0;
   }

   temp = FindAttribute(tp->attributes, HEIGHT_ATTRIBUTE);
   if(temp) {
      height = ParseUnsigned(tp, temp);
   } else {
      height = 0;
   }

   cp = CreateClock(format, zone, width, height);
   if(JLIKELY(cp)) {
      ParseTrayComponentActions(tp, cp, AddClockAction);
      AddTrayComponent(tray, cp);
   }

}

/** Parse tray component actions. */
void ParseTrayComponentActions(const TokenNode *tp, TrayComponentType *cp,
                               AddTrayActionFunc func)
{
   const TokenNode *np;
   const char *mask_str;
   const int default_mask = (1 << 1) | (1 << 2) | (1 << 3);
   int mask;

   if(tp->value) {
      (func)(cp, tp->value, default_mask);
   }

   for(np = tp->subnodeHead; np; np = np->next) {
      switch(np->type) {
      case TOK_BUTTON:
         mask_str = FindAttribute(np->attributes, "mask");
         if(mask_str) {
            int i;
            mask = 0;
            for(i = 0; mask_str[i]; i++) {
               mask |= 1 << (mask_str[i] - '0');
            }
         } else {
            mask = default_mask;
         }
         (func)(cp, np->value, mask);
         break;
      default:
         InvalidTag(np, tp->type);
         break;
      }
   }
}


/** Parse a dock tray component. */
void ParseDock(const TokenNode *tp, TrayType *tray) {

   TrayComponentType *cp;
   int width;
   char *str;

   Assert(tp);
   Assert(tray);

   str = FindAttribute(tp->attributes, WIDTH_ATTRIBUTE);
   if(str) {
      width = ParseUnsigned(tp, str);
   } else {
      width = 0;
   }

   str = FindAttribute(tp->attributes, SPACING_ATTRIBUTE);
   if(str) {
      settings.dockSpacing = ParseUnsigned(tp, str);
   }

   cp = CreateDock(width);
   if(JLIKELY(cp)) {
      AddTrayComponent(tray, cp);
   }

}

/** Parse a spacer tray component. */
void ParseSpacer(const TokenNode *tp, TrayType *tray) {

   TrayComponentType *cp;
   int width;
   int height;
   char *str;

   Assert(tp);
   Assert(tray);

   /* Get the width. */
   str = FindAttribute(tp->attributes, WIDTH_ATTRIBUTE);
   if(str) {
      width = ParseUnsigned(tp, str);
   } else {
      width = 0;
   }

   /* Get the height. */
   str = FindAttribute(tp->attributes, HEIGHT_ATTRIBUTE);
   if(str) {
      height = ParseUnsigned(tp, str);
   } else {
      height = 0;
   }

   /* Create the spacer. */
   cp = CreateSpacer(width, height);
   if(JLIKELY(cp)) {
      AddTrayComponent(tray, cp);
   }

}

/** Parse pager style. */
void ParsePagerStyle(const TokenNode *tp)
{

   const TokenNode *np;

   Assert(tp);

   for(np = tp->subnodeHead; np; np = np->next) {
      switch(np->type) {
      case TOK_OUTLINE:
         SetColor(COLOR_PAGER_OUTLINE, np->value);
         break;
      case TOK_FOREGROUND:
         SetColor(COLOR_PAGER_FG, np->value);
         break;
      case TOK_BACKGROUND:
         SetColor(COLOR_PAGER_BG, np->value);
         break;
      case TOK_ACTIVE:
         ParseActive(np, COLOR_PAGER_ACTIVE_FG,
                     COLOR_PAGER_ACTIVE_BG, COLOR_PAGER_ACTIVE_BG,
                     COLOR_COUNT, COLOR_COUNT);
         break;
      case TOK_FONT:
         SetFont(FONT_PAGER, np->value);
         break;
      case TOK_TEXT:
         SetColor(COLOR_PAGER_TEXT, np->value);
         break;
      default:
         InvalidTag(np, TOK_PAGERSTYLE);
         break;
      }
   }

}

/** Parse clock style. */
void ParseClockStyle(const TokenNode *tp)
{
   const TokenNode *np;
   for(np = tp->subnodeHead; np; np = np->next) {
      switch(np->type) {
      case TOK_FOREGROUND:
         SetColor(COLOR_CLOCK_FG, np->value);
         break;
      case TOK_BACKGROUND:
         ParseGradient(np->value, COLOR_CLOCK_BG1, COLOR_CLOCK_BG2);
         break;
      case TOK_FONT:
         SetFont(FONT_CLOCK, np->value);
         break;
      default:
         InvalidTag(np, TOK_CLOCKSTYLE);
         break;
      }
   }
}

/** Parse popup style. */
void ParsePopupStyle(const TokenNode *tp)
{
   static const StringMappingType enable_mapping[] = {
      { "button", POPUP_BUTTON   },
      { "clock",  POPUP_CLOCK    },
      { "false",  POPUP_NONE     },
      { "menu",   POPUP_MENU     },
      { "pager",  POPUP_PAGER    },
      { "task",   POPUP_TASK     },
      { "true",   POPUP_ALL      }
   };
   const TokenNode *np;
   const char *str;
   char *tok;

   tok = FindAttribute(tp->attributes, "enabled");
   if(tok) {
      settings.popupMask = POPUP_NONE;
      tok = strtok(tok, ",");
      while(tok) {
         const int x = FindValue(enable_mapping,
                                 ARRAY_LENGTH(enable_mapping), tok);
         if(JLIKELY(x >= 0)) {
            settings.popupMask |= x;
         } else {
            ParseError(tp, _("invalid value for 'enabled': \"%s\""), tok);
         }
         tok = strtok(NULL, ",");
      }
   }

   str = FindAttribute(tp->attributes, "delay");
   if(str) {
      settings.popupDelay = ParseUnsigned(tp, str);
   }

   for(np = tp->subnodeHead; np; np = np->next) {
      switch(np->type) {
      case TOK_FONT:
         SetFont(FONT_POPUP, np->value);
         break;
      case TOK_FOREGROUND:
         SetColor(COLOR_POPUP_FG, np->value);
         break;
      case TOK_BACKGROUND:
         SetColor(COLOR_POPUP_BG, np->value);
         break;
      case TOK_OUTLINE:
         SetColor(COLOR_POPUP_OUTLINE, np->value);
         break;
      default:
         InvalidTag(np, TOK_POPUPSTYLE);
         break;
      }
   }

}

/** Parse menu style. */
void ParseMenuStyle(const TokenNode *tp)
{
   const TokenNode *np;

   ParseDecorations(tp, &settings.menuDecorations);
   for(np = tp->subnodeHead; np; np = np->next) {
      switch(np->type) {
      case TOK_FONT:
         SetFont(FONT_MENU, np->value);
         break;
      case TOK_FOREGROUND:
         SetColor(COLOR_MENU_FG, np->value);
         break;
      case TOK_BACKGROUND:
         SetColor(COLOR_MENU_BG, np->value);
         break;
      case TOK_ACTIVE:
         ParseActive(np, COLOR_MENU_ACTIVE_FG,
                     COLOR_MENU_ACTIVE_BG1, COLOR_MENU_ACTIVE_BG2,
                     COLOR_MENU_ACTIVE_UP, COLOR_MENU_ACTIVE_DOWN);
         break;
      case TOK_OUTLINE:
         ParseGradient(np->value, COLOR_MENU_DOWN, COLOR_MENU_UP);
         break;
      case TOK_OPACITY:
         settings.menuOpacity = ParseOpacity(np, np->value);
         break;
      default:
         InvalidTag(np, TOK_MENUSTYLE);
         break;
      }
   }

}

/** Parse an option group. */
void ParseGroup(const TokenNode *tp)
{

   const TokenNode *np;
   struct GroupType *group;

   Assert(tp);

   group = CreateGroup();

   for(np = tp->subnodeHead; np; np = np->next) {
      switch(np->type) {
      case TOK_CLASS:
         AddGroupClass(group, np->value);
         break;
      case TOK_NAME:
         AddGroupName(group, np->value);
         break;
      case TOK_TITLE:
         AddGroupTitle(group, np->value);
         break;
      case TOK_TYPE:
         AddGroupType(group, np->value);
         break;
      case TOK_MACHINE:
         AddGroupMachine(group, np->value);
         break;
      case TOK_OPTION:
         ParseGroupOption(np, group, np->value);
         break;
      default:
         InvalidTag(np, TOK_GROUP);
         break;
      }
   }

}

/** Parse a option group option. */
void ParseGroupOption(const TokenNode *tp, struct GroupType *group,
                      const char *option)
{
   int x;

   if(!option) {
      return;
   }

   /* Look up the option in the option map using binary search. */
   x = FindValue(OPTION_MAP, OPTION_MAP_COUNT, option);
   if(x >= 0) {
      AddGroupOption(group, (OptionType)x);
      return;
   }

   /* These options have arguments and so we handled them separately. */
   if(!strncmp(option, "layer:", 6)) {
      const WinLayerType layer = ParseLayer(tp, option + 6);
      AddGroupOptionUnsigned(group, OPTION_LAYER, layer);
   } else if(!strncmp(option, "desktop:", 8)) {
      const unsigned desktop = (unsigned)atoi(option + 8);
      AddGroupOptionUnsigned(group, OPTION_DESKTOP, desktop);
   } else if(!strncmp(option, "icon:", 5)) {
      AddGroupOptionString(group, OPTION_ICON, option + 5);
   } else if(!strncmp(option, "opacity:", 8)) {
      const unsigned opacity = ParseOpacity(tp, option + 8);
      AddGroupOptionUnsigned(group, OPTION_OPACITY, opacity);
   } else if(!strncmp(option, "x:", 2)) {
      const int x = atoi(option + 2);
      AddGroupOptionSigned(group, OPTION_X, x);
   } else if(!strncmp(option, "y:", 2)) {
      const int y = atoi(option + 2);
      AddGroupOptionSigned(group, OPTION_Y, y);
   } else if(!strncmp(option, "width:", 6)) {
      const unsigned width = (unsigned)atoi(option + 6);
      AddGroupOptionUnsigned(group, OPTION_WIDTH, width);
   } else if(!strncmp(option, "height:", 7)) {
      const unsigned height = (unsigned)atoi(option + 7);
      AddGroupOptionUnsigned(group, OPTION_HEIGHT, height);
   } else {
      ParseError(tp, _("invalid Group Option: %s"), option);
   }

}

/** Parse decorations type. */
void ParseDecorations(const TokenNode *tp, DecorationsType *deco)
{
   const char *str = FindAttribute(tp->attributes, "decorations");
   if(str) {
      if(!strcmp(str, "motif")) {
         *deco = DECO_MOTIF;
      } else if(!strcmp(str, "flat")) {
         *deco = DECO_FLAT;
      } else {
         ParseError(tp, _("invalid decorations: %s"), str);
      }
   }
}

/** Parse a color which may be a gradient. */
void ParseGradient(const char *value, ColorType a, ColorType b)
{

   const char *sep;
   char *temp;

   /* Find the separator. */
   sep = strchr(value, ':');
   if(sep) {
      SetGradient(a, b, GRADIENT_VERTICAL);
   } else {
      sep = strchr(value, ';');
      SetGradient(a, b, GRADIENT_HORIZONTAL);
   }
   
   if(!sep) {

      /* Only one color given - use the same color for both. */
      SetColor(a, value);
      SetColor(b, value);

   } else {

      /* Two colors. */

      /* Get the first color. */
      int len = (int)(sep - value);
      temp = AllocateStack(len + 1);
      memcpy(temp, value, len);
      temp[len] = 0;
      SetColor(a, temp);
      ReleaseStack(temp);

      /* Get the second color. */
      len = strlen(sep + 1);
      temp = AllocateStack(len + 1);
      memcpy(temp, sep + 1, len);
      temp[len] = 0;
      SetColor(b, temp);
      ReleaseStack(temp);

   }

}

/** Find an attribute in a list of attributes. */
char *FindAttribute(AttributeNode *ap, const char *name)
{
   while(ap) {
      if(!strcmp(name, ap->name)) {
         return ap->value;
      }
      ap = ap->next;
   }
   return NULL;
}

/** Parse a token value using a string mapping. */
int ParseTokenValue(const StringMappingType *mapping, int count,
                    const TokenNode *tp, int def)
{
   if(JUNLIKELY(tp->value == NULL)) {
      ParseError(tp, _("%s is empty"), GetTokenName(tp));
      return def;
   } else {
      const int x = FindValue(mapping, count, tp->value);
      if(JLIKELY(x >= 0)) {
         return x;
      } else {
         ParseError(tp, _("invalid %s: \"%s\""), GetTokenName(tp), tp->value);
         return def;
      }
   }
}

/** Parse a string using a string mapping. */
int ParseAttribute(const StringMappingType *mapping, int count,
                   const TokenNode *tp, const char *attr, int def)
{
   const char *str = FindAttribute(tp->attributes, attr);
   if(str == NULL) {
      return def;
   } else {
      const int x = FindValue(mapping, count, str);
      if(JLIKELY(x >= 0)) {
         return x;
      } else {
         ParseError(tp, _("invalid value for %s: \"%s\""), attr, str);
         return def;
      }
   }
}

/** Tokenize a file by memory mapping it. */
TokenNode *TokenizeFile(const char *fileName)
{
   struct stat sbuf;
   TokenNode *tokens;
   char *path;
   char *buffer;
   ssize_t offset;

   path = CopyString(fileName);
   ExpandPath(&path);

   int fd = open(path, O_RDONLY);
   Release(path);

   if(fd < 0) {
      return NULL;
   }
   if(JUNLIKELY(fstat(fd, &sbuf) == -1)) {
      close(fd);
      return NULL;
   }
   buffer = Allocate(sbuf.st_size + 1);
   offset = 0;
   while(offset < sbuf.st_size) {
      const ssize_t rc = read(fd, &buffer[offset], sbuf.st_size - offset);
      if(rc <= 0) {
         break;
      }
      offset += rc;
   }
   buffer[offset] = 0;
   tokens = Tokenize(buffer, fileName);
   Release(buffer);
   close(fd);
   return tokens;
}

/** Tokenize the output of a command. */
TokenNode *TokenizePipe(const char *command, unsigned timeout_ms)
{
   TokenNode *tokens;
   char *path;
   char *buffer;

   path = CopyString(command);
   ExpandPath(&path);

   buffer = ReadFromProcess(path, timeout_ms);
   Release(path);
   if(JUNLIKELY(!buffer)) {
      return NULL;
   }

   tokens = Tokenize(buffer, command);
   Release(buffer);
   return tokens;
}

/** Parse a signed integer. */
int ParseSigned(const TokenNode *tp, const char *str)
{
   long value;
   if(JUNLIKELY(!str)) {
      ParseError(tp, _("no value specified"));
      return 0;
   }
   value = strtol(str, NULL, 0);
   if(JUNLIKELY(value < INT_MIN || value > INT_MAX)) {
      ParseError(tp, _("invalid setting: %s"), str);
      return 0;
   } else {
      return (int)value;
   }
}

/** Parse an unsigned integer. */
unsigned ParseUnsigned(const TokenNode *tp, const char *str)
{
   long value;
   if(JUNLIKELY(!str)) {
      ParseError(tp, _("no value specified"));
      return 0;
   }
   value = strtol(str, NULL, 0);
   if(JUNLIKELY(value < 0 || value > UINT_MAX)) {
      ParseError(tp, _("invalid setting: %s"), str);
      return 0;
   } else {
      return (unsigned)value;
   }
}

/** Parse a timeout attribute. */
unsigned ParseTimeout(const TokenNode *tp, unsigned timeout_ms)
{
   char *temp = FindAttribute(tp->attributes, TIMEOUT_ATTRIBUTE);
   if(temp) {
      const unsigned ms = ParseUnsigned(tp, temp);
      timeout_ms = ms == 0 ? timeout_ms : ms;
   }
   return timeout_ms;
}

/** Parse opacity (a float between 0.0 and 1.0). */
unsigned ParseOpacity(const TokenNode *tp, const char *str)
{
   double value;
   if(JUNLIKELY(!str)) {
      ParseError(tp, _("no value specified"));
      return UINT_MAX;
   }
   value = ParseFloat(str);
   if(JUNLIKELY(value <= 0.0 || value > 1.0)) {
      ParseError(tp, _("invalid opacity: %s"), str);
      return UINT_MAX;
   } else if(value == 1.0) {
      return UINT_MAX;
   } else {
      return (unsigned int)(value * UINT_MAX);
   }
}

/** Parse layer. */
WinLayerType ParseLayer(const TokenNode *tp, const char *str)
{
   static const StringMappingType mapping[] = {
      { "above",  LAYER_ABOVE    },
      { "below",  LAYER_BELOW    },
      { "normal", LAYER_NORMAL   }
   };
   const int x = FindValue(mapping, ARRAY_LENGTH(mapping), str);
   if(JLIKELY(x >= 0)) {
      return x;
   }  else {
      ParseError(tp, _("invalid layer: %s"), str);
      return LAYER_NORMAL;
   }
}

/** Parse status window type. */
StatusWindowType ParseStatusWindowType(const TokenNode *tp)
{
   static const StringMappingType mapping[] = {
      { "corner",    SW_CORNER   },
      { "off",       SW_OFF      },
      { "screen",    SW_SCREEN   },
      { "window",    SW_WINDOW   }
   };
   return ParseAttribute(mapping, ARRAY_LENGTH(mapping), tp,
                         "coordinates", SW_SCREEN);
}

/** Display an invalid tag error message. */
void InvalidTag(const TokenNode *tp, TokenType parent)
{
   ParseError(tp, _("invalid tag in %s: %s"),
              GetTokenTypeName(parent), GetTokenName(tp));
}

/** Display a parser error. */
void ParseError(const TokenNode *tp, const char *str, ...)
{

   va_list ap;

   static const char FILE_MESSAGE[] = "%s[%u]";

   char *msg;

   va_start(ap, str);

   if(tp) {
      const size_t msg_len = sizeof(FILE_MESSAGE) + strlen(tp->fileName) + 1;
      msg = Allocate(msg_len);
      snprintf(msg, msg_len, FILE_MESSAGE, tp->fileName, tp->line);
   } else {
      msg = CopyString(_("configuration error"));
   }

   WarningVA(msg, str, ap);

   Release(msg);

   va_end(ap);

}
