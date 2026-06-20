/**
 * @file group.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Functions for handling window groups.
 *
 */

#include "jwm.h"
#include "group.h"
#include "client.h"
#include "icon.h"
#include "error.h"
#include "match.h"
#include "misc.h"
#include "settings.h"

/** What part of the window to match. */
typedef unsigned int MatchType;
#define MATCH_NAME      0  /**< Match the window name. */
#define MATCH_CLASS     1  /**< Match the window class. */
#define MATCH_TYPE      2  /**< Match the window type. */
#define MATCH_MACHINE   3  /**< Match the window client machine name. */
#define MATCH_TITLE     4  /**< Match the window title. */

/** List of match patterns for a group. */
typedef struct PatternListType {
   char *pattern;
   MatchType match;
   struct PatternListType *next;
} PatternListType;

/** List of options for a group. */
typedef struct OptionListType {
   OptionType option;
   char *str;
   union {
      unsigned u;
      int s;
   } value;
   struct OptionListType *next;
} OptionListType;

/** List of groups. */
typedef struct GroupType {
   PatternListType *patterns;
   OptionListType *options;
   struct GroupType *next;
} GroupType;

static GroupType *groups = NULL;

static void ReleasePatternList(PatternListType *lp);
static void ReleaseOptionList(OptionListType *lp);
static void AddPattern(PatternListType **lp, const char *pattern,
                       MatchType match);
static void ApplyGroup(const GroupType *gp, ClientNode *np);

/** Destroy group data. */
void DestroyGroups(void)
{
   GroupType *gp;
   while(groups) {
      gp = groups->next;
      ReleasePatternList(groups->patterns);
      ReleaseOptionList(groups->options);
      Release(groups);
      groups = gp;
   }
}

/** Release a group pattern list. */
void ReleasePatternList(PatternListType *lp)
{
   PatternListType *tp;
   while(lp) {
      tp = lp->next;
      Release(lp->pattern);
      Release(lp);
      lp = tp;
   }
}

/** Release a group option list. */
void ReleaseOptionList(OptionListType *lp)
{
   OptionListType *tp;
   while(lp) {
      tp = lp->next;
      if(lp->str) {
         Release(lp->str);
      }
      Release(lp);
      lp = tp;
   }
}

/** Create an empty group. */
GroupType *CreateGroup(void)
{
   GroupType *tp;
   tp = Allocate(sizeof(GroupType));
   tp->patterns = NULL;
   tp->options = NULL;
   tp->next = groups;
   groups = tp;
   return tp;
}

/** Add a window class to a group. */
void AddGroupClass(GroupType *gp, const char *pattern)
{
   Assert(gp);
   if(JLIKELY(pattern)) {
      AddPattern(&gp->patterns, pattern, MATCH_CLASS);
   } else {
      Warning(_("invalid group class"));
   }
}

/** Add a window name to a group. */
void AddGroupName(GroupType *gp, const char *pattern)
{
   Assert(gp);
   if(JLIKELY(pattern)) {
      AddPattern(&gp->patterns, pattern, MATCH_NAME);
   } else {
      Warning(_("invalid group name"));
   }
}

/** Add a window title to a group. */
void AddGroupTitle(GroupType *gp, const char *pattern)
{
   Assert(gp);
   if(JLIKELY(pattern)) {
      AddPattern(&gp->patterns, pattern, MATCH_TITLE);
   } else {
      Warning(_("invalid group name"));
   }
}

/** Add a window type to a group. */
void AddGroupType(GroupType *gp, const char *pattern)
{
   Assert(gp);
   if(JLIKELY(pattern)) {
      AddPattern(&gp->patterns, pattern, MATCH_TYPE);
   } else {
      Warning(_("invalid group type"));
   }
}

/** Add a window client machine to a group. */
void AddGroupMachine(GroupType *gp, const char *pattern)
{
   Assert(gp);
   if(JLIKELY(pattern)) {
      AddPattern(&gp->patterns, pattern, MATCH_MACHINE);
   } else {
      Warning(_("invalid group machine"));
   }
}

/** Add a pattern to a pattern list. */
void AddPattern(PatternListType **lp, const char *pattern, MatchType match)
{
   PatternListType *tp;
   Assert(lp);
   Assert(pattern);
   tp = Allocate(sizeof(PatternListType));
   tp->next = *lp;
   *lp = tp;
   tp->pattern = CopyString(pattern);
   tp->match = match;
}

/** Add an option to a group. */
void AddGroupOption(GroupType *gp, OptionType option)
{
   OptionListType *lp;
   lp = Allocate(sizeof(OptionListType));
   lp->option = option;
   lp->str = NULL;
   lp->next = gp->options;
   gp->options = lp;
}

/** Add an option (with a string) to a group. */
void AddGroupOptionString(GroupType *gp, OptionType option,
                          const char *value)
{
   OptionListType *lp;
   Assert(value);
   lp = Allocate(sizeof(OptionListType));
   lp->option = option;
   lp->str = CopyString(value);
   lp->next = gp->options;
   gp->options = lp;
}

/** Add an option (with an unsigned integer) to a group. */
void AddGroupOptionUnsigned(GroupType *gp, OptionType option,
                            unsigned value)
{
   OptionListType *lp;
   Assert(value);
   lp = Allocate(sizeof(OptionListType));
   lp->option = option;
   lp->str = NULL;
   lp->value.u = value;
   lp->next = gp->options;
   gp->options = lp;
}

/** Add an option (with a signed integer) to a group. */
void AddGroupOptionSigned(GroupType *gp, OptionType option,
                          int value)
{
   OptionListType *lp;
   Assert(value);
   lp = Allocate(sizeof(OptionListType));
   lp->option = option;
   lp->str = NULL;
   lp->value.s = value;
   lp->next = gp->options;
   gp->options = lp;
}

/** Apply groups to a client. */
void ApplyGroups(ClientNode *np)
{
   PatternListType *lp;
   GroupType *gp;
   char hasClass;
   char hasName;
   char hasTitle;
   char hasType;
   char hasClient;
   char matchesClass;
   char matchesName;
   char matchesTitle;
   char matchesType;
   char matchesClient;

   static const StringMappingType windowTypeMapping[] = {
      { "desktop",      WINDOW_TYPE_DESKTOP      },
      { "dialog",       WINDOW_TYPE_DIALOG       },
      { "dock",         WINDOW_TYPE_DOCK         },
      { "menu",         WINDOW_TYPE_MENU         },
      { "normal",       WINDOW_TYPE_NORMAL       },
      { "notification", WINDOW_TYPE_NOTIFICATION },
      { "splash",       WINDOW_TYPE_SPLASH       },
      { "toolbar",      WINDOW_TYPE_TOOLBAR      },
      { "utility",      WINDOW_TYPE_UTILITY      }
   };

   Assert(np);
   for(gp = groups; gp; gp = gp->next) {
      hasClass = 0;
      hasName = 0;
      hasType = 0;
      hasTitle = 0;
      hasClient = 0;
      matchesClass = 0;
      matchesName = 0;
      matchesTitle = 0;
      matchesType = 0;
      matchesClient = 0;
      for(lp = gp->patterns; lp; lp = lp->next) {
         if(lp->match == MATCH_CLASS) {
            if(Match(lp->pattern, np->className)) {
               matchesClass = 1;
            }
            hasClass = 1;
         } else if(lp->match == MATCH_NAME) {
            if(Match(lp->pattern, np->instanceName)) {
               matchesName = 1;
            }
            hasName = 1;
         } else if(lp->match == MATCH_TITLE) {
            if(Match(lp->pattern, np->name)) {
               matchesTitle = 1;
            }
            hasTitle = 1;
         } else if(lp->match == MATCH_TYPE) {
             if(FindValue(windowTypeMapping, WINDOW_TYPE_COUNT, lp->pattern)
             == np->state.windowType) {
                matchesType = 1;
             }
             hasType = 1;
         } else if(lp->match == MATCH_MACHINE) {
            if(Match(lp->pattern, np->clientName)) {
               matchesClient = 1;
            }
             hasClient = 1;
         } else {
            Debug("invalid match in ApplyGroups: %d", lp->match);
         }
      }
      if(hasName == matchesName
      && hasClass == matchesClass
      && hasTitle == matchesTitle
      && hasType == matchesType
      && hasClient == matchesClient) {
         ApplyGroup(gp, np);
      }
   }

}

/** Apply a group to a client. */
void ApplyGroup(const GroupType *gp, ClientNode *np)
{
   OptionListType *lp;
   char noPager = 0;
   char noList = 0;

   Assert(gp);
   Assert(np);
   for(lp = gp->options; lp; lp = lp->next) {
      switch(lp->option) {
      case OPTION_STICKY:
         np->state.status |= STAT_STICKY;
         break;
      case OPTION_NOLIST:
         np->state.status |= STAT_NOLIST;
         noList = 1;
         break;
      case OPTION_ILIST:
         np->state.status |= STAT_ILIST;
         if(!noList) {
            np->state.status &= ~STAT_NOLIST;
         }
         break;
      case OPTION_NOPAGER:
         np->state.status |= STAT_NOPAGER;
         noPager = 1;
         break;
      case OPTION_IPAGER:
         np->state.status |= STAT_IPAGER;
         if(!noPager) {
            np->state.status &= ~STAT_NOPAGER;
         }
         break;
      case OPTION_BORDER:
         np->state.border |= BORDER_OUTLINE;
         break;
      case OPTION_NOBORDER:
         np->state.border &= ~BORDER_OUTLINE;
         break;
      case OPTION_TITLE:
         np->state.border |= BORDER_TITLE;
         break;
      case OPTION_NOTITLE:
         np->state.border &= ~BORDER_TITLE;
         np->state.border &= ~BORDER_SHADE;
         break;
      case OPTION_LAYER:
         np->state.layer = lp->value.u;
         break;
      case OPTION_DESKTOP:
         if(JLIKELY(lp->value.u >= 1 && lp->value.u <= settings.desktopCount)) {
            np->state.desktop = lp->value.u - 1;
         } else {
            Warning(_("invalid group desktop: %d"), lp->value.u);
         }
         break;
      case OPTION_ICON:
         DestroyIcon(np->icon);
         np->icon = LoadNamedIcon(lp->str, 1, 1);
         break;
      case OPTION_PIGNORE:
         np->state.status |= STAT_PIGNORE;
         break;
      case OPTION_IIGNORE:
         np->state.status |= STAT_IIGNORE;
         break;
      case OPTION_MAXIMIZED:
         np->state.maxFlags |= MAX_HORIZ | MAX_VERT;
         break;
      case OPTION_MINIMIZED:
         np->state.status |= STAT_MINIMIZED;
         break;
      case OPTION_SHADED:
         np->state.status |= STAT_SHADED;
         break;
      case OPTION_OPACITY:
         np->state.opacity = lp->value.u;
         np->state.status |= STAT_OPACITY;
         break;
      case OPTION_MAX_V:
         np->state.border &= ~BORDER_MAX_H;
         break;
      case OPTION_MAX_H:
         np->state.border &= ~BORDER_MAX_V;
         break;
      case OPTION_NOFOCUS:
         np->state.status |= STAT_NOFOCUS;
         break;
      case OPTION_NOSHADE:
         np->state.border &= ~BORDER_SHADE;
         break;
      case OPTION_NOMIN:
         np->state.border &= ~BORDER_MIN;
         break;
      case OPTION_NOMAX:
         np->state.border &= ~BORDER_MAX;
         break;
      case OPTION_NOCLOSE:
         np->state.border &= ~BORDER_CLOSE;
         break;
      case OPTION_NOMOVE:
         np->state.border &= ~BORDER_MOVE;
         break;
      case OPTION_NORESIZE:
         np->state.border &= ~BORDER_RESIZE;
         break;
      case OPTION_CENTERED:
         np->state.status |= STAT_CENTERED;
         break;
      case OPTION_TILED:
         np->state.status |= STAT_TILED;
         break;
      case OPTION_NOTURGENT:
         np->state.status |= STAT_NOTURGENT;
         break;
      case OPTION_CONSTRAIN:
         np->state.border |= BORDER_CONSTRAIN;
         break;
      case OPTION_FULLSCREEN:
         np->state.status |= STAT_FULLSCREEN;
         break;
      case OPTION_NOFULLSCREEN:
         np->state.border &= ~BORDER_FULLSCREEN;
         break;
      case OPTION_DRAG:
         np->state.status |= STAT_DRAG;
         break;
      case OPTION_FIXED:
         np->state.status |= STAT_FIXED;
         break;
      case OPTION_AEROSNAP:
         np->state.status |= STAT_AEROSNAP;
         break;
      case OPTION_NODRAG:
         np->state.status |= STAT_NODRAG;
         break;
      case OPTION_X:
         if(lp->value.s < 0) {
            int north, south, east, west;
            GetBorderSize(&np->state, &north, &south, &east, &west);
            np->x = rootWidth + lp->value.s - np->width - east - west;
         } else {
            np->x = lp->value.s;
         }
         np->state.status |= STAT_POSITION;
         break;
      case OPTION_Y:
         if(lp->value.s < 0) {
            int north, south, east, west;
            GetBorderSize(&np->state, &north, &south, &east, &west);
            np->y = rootHeight + lp->value.s - np->height - north - south;
         } else {
            np->y = lp->value.s;
         }
         np->state.status |= STAT_POSITION;
         break;
      case OPTION_WIDTH:
         np->width = Max(1, lp->value.u);
         break;
      case OPTION_HEIGHT:
         np->height = Max(1, lp->value.u);
         break;
      case OPTION_NOMAXBORDER:
         np->state.border |= BORDER_NOMAX;
         break;
      case OPTION_NOMAXTITLE:
         np->state.border |= TITLE_NOMAX;
         break;
      default:
         Debug("invalid option: %d", lp->option);
         break;
      }
   }

}

