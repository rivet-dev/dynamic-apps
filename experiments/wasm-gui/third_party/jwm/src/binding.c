/**
 * @file binding.c
 * @author Joe Wingbermuehle
 * @date 2004-20017
 *
 * @brief Key/mouse binding functions.
 *
 */

#include "jwm.h"
#include "binding.h"

#include "client.h"
#include "clientlist.h"
#include "command.h"
#include "error.h"
#include "misc.h"
#include "root.h"
#include "tray.h"

#define MASK_NONE    0
#define MASK_SHIFT   (1 << ShiftMapIndex)
#define MASK_LOCK    (1 << LockMapIndex)
#define MASK_CTRL    (1 << ControlMapIndex)
#define MASK_MOD1    (1 << Mod1MapIndex)
#define MASK_MOD2    (1 << Mod2MapIndex)
#define MASK_MOD3    (1 << Mod3MapIndex)
#define MASK_MOD4    (1 << Mod4MapIndex)
#define MASK_MOD5    (1 << Mod5MapIndex)

typedef struct ModifierNode {
   char           name;
   unsigned int   mask;
} ModifierNode;

static const ModifierNode MODIFIERS[] = {
   { 'C',   MASK_CTRL      },
   { 'S',   MASK_SHIFT     },
   { 'A',   MASK_MOD1      },
   { '1',   MASK_MOD1      },
   { '2',   MASK_MOD2      },
   { '3',   MASK_MOD3      },
   { '4',   MASK_MOD4      },
   { '5',   MASK_MOD5      }
};
static const unsigned MODIFIER_COUNT = ARRAY_LENGTH(MODIFIERS);

typedef struct KeyNode {

   /* These are filled in when the configuration file is parsed */
   ActionType action;
   MouseContextType context;
   unsigned int state;
   KeySym symbol;
   char *command;
   struct KeyNode *next;

   /* This is filled in by StartupKeys if it isn't already set. */
   int code;

} KeyNode;

typedef struct LockNode {
   KeySym symbol;
   unsigned int mask;
} LockNode;

static LockNode lockMods[] = {
   { XK_Caps_Lock,   0 },
   { XK_Num_Lock,    0 }
};

static KeyNode *bindings[MC_COUNT];
unsigned lockMask;

static unsigned int GetModifierMask(XModifierKeymap *modmap, KeySym key);
static KeySym ParseKeyString(const char *str);
static char ShouldGrab(ActionType key);
static void GrabKey(KeyNode *np, Window win);

/** Initialize binding data. */
void InitializeBindings(void)
{
   memset(bindings, 0, sizeof(bindings));
   lockMask = 0;
}

/** Startup bindings. */
void StartupBindings(void)
{

   XModifierKeymap *modmap;
   KeyNode *np;
   TrayType *tp;
   int x;

   /* Get the keys that we don't care about (num lock, etc). */
   modmap = JXGetModifierMapping(display);
   for(x = 0; x < ARRAY_LENGTH(lockMods); x++) {
      lockMods[x].mask = GetModifierMask(modmap, lockMods[x].symbol);
      lockMask |= lockMods[x].mask;
   }
   JXFreeModifiermap(modmap);
   lockMask |= Button1Mask | Button2Mask | Button3Mask
            | Button4Mask | Button5Mask | (1<<13) | (1<<14);

   /* Look up and grab the keys. */
   for(np = bindings[MC_NONE]; np; np = np->next) {

      /* Determine the key code. */
      if(!np->code) {
         np->code = JXKeysymToKeycode(display, np->symbol);
      }

      /* Grab the key if needed. */
      if(ShouldGrab(np->action)) {

         /* Grab on the root. */
         GrabKey(np, rootWindow);

         /* Grab on the trays. */
         for(tp = GetTrays(); tp; tp = tp->next) {
            GrabKey(np, tp->window);
         }

      }

   }
}

/** Shutdown bindings. */
void ShutdownBindings(void)
{
   ClientNode *np;
   TrayType *tp;
   unsigned int layer;

   /* Ungrab keys on client windows. */
   for(layer = 0; layer < LAYER_COUNT; layer++) {
      for(np = nodes[layer]; np; np = np->next) {
         JXUngrabKey(display, AnyKey, AnyModifier, np->window);
      }
   }

   /* Ungrab keys on trays, only really needed if we are restarting. */
   for(tp = GetTrays(); tp; tp = tp->next) {
      JXUngrabKey(display, AnyKey, AnyModifier, tp->window);
   }

   /* Ungrab keys on the root. */
   JXUngrabKey(display, AnyKey, AnyModifier, rootWindow);
}

/** Destroy key data. */
void DestroyBindings(void)
{
   unsigned i;
   for(i = 0; i < MC_COUNT; i++) {
      while(bindings[i]) {
         KeyNode *np = bindings[i]->next;
         if(bindings[i]->command) {
            Release(bindings[i]->command);
         }
         Release(bindings[i]);
         bindings[i] = np;
      }
   }
}

/** Grab a key. */
void GrabKey(KeyNode *np, Window win)
{
   unsigned int x;
   unsigned int index, maxIndex;
   unsigned int mask;

   /* Don't attempt to grab if there is nothing to grab. */
   if(!np->code) {
      return;
   }

   /* Grab for each lock modifier. */
   maxIndex = 1 << ARRAY_LENGTH(lockMods);
   for(index = 0; index < maxIndex; index++) {

      /* Compute the modifier mask. */
      mask = 0;
      for(x = 0; x < ARRAY_LENGTH(lockMods); x++) {
         if(index & (1 << x)) {
            mask |= lockMods[x].mask;
         }
      }
      mask |= np->state;

      /* Grab the key. */
      JXGrabKey(display, np->code, mask, win,
         True, GrabModeAsync, GrabModeAsync);

   }

}

/** Get the key action from an event. */
ActionType GetKey(MouseContextType context, unsigned state, int code)
{
   KeyNode *np;
   ActionType result;

   /* Remove modifiers we don't care about from the state. */
   state &= ~lockMask;

   /* Mask off flags. */
   context &= MC_MASK;

   /* Loop looking for a matching key binding. */
   for(np = bindings[context]; np; np = np->next) {
      if(np->state == state && np->code == code) {
         return np->action;
      }
   }

   result.action = ACTION_NONE;
   result.extra = 0;
   return result;
}

/** Run a command invoked from a key binding. */
void RunKeyCommand(MouseContextType context, unsigned state, int code)
{
   KeyNode *np;

   /* Remove the lock key modifiers. */
   state &= ~lockMask;

   /* Mask off flags. */
   context &= MC_MASK;

   for(np = bindings[context]; np; np = np->next) {
      if(np->state == state && np->code == code) {
         RunCommand(np->command);
         return;
      }
   }
}

/** Show a root menu caused by a key binding. */
void ShowKeyMenu(MouseContextType context, unsigned state, int code)
{
   KeyNode *np;

   /* Remove the lock key modifiers. */
   state &= ~lockMask;

   /* Mask off flags. */
   context &= MC_MASK;

   for(np = bindings[context]; np; np = np->next) {
      if(np->state == state && np->code == code) {
         const int button = GetRootMenuIndexFromString(np->command);
         if(JLIKELY(button >= 0)) {
            ShowRootMenu(button, -1, -1, 1);
         }
         return;
      }
   }
}

/** Determine if a key should be grabbed on client windows. */
char ShouldGrab(ActionType action)
{
   switch(action.action) {
   case ACTION_NEXT:
   case ACTION_NEXTSTACK:
   case ACTION_PREV:
   case ACTION_PREVSTACK:
   case ACTION_CLOSE:
   case ACTION_MIN:
   case ACTION_MAX:
   case ACTION_SHADE:
   case ACTION_STICK:
   case ACTION_MOVE:
   case ACTION_RESIZE:
   case ACTION_ROOT:
   case ACTION_WIN:
   case ACTION_DESKTOP:
   case ACTION_RDESKTOP:
   case ACTION_LDESKTOP:
   case ACTION_DDESKTOP:
   case ACTION_UDESKTOP:
   case ACTION_SHOWDESK:
   case ACTION_SHOWTRAY:
   case ACTION_EXEC:
   case ACTION_RESTART:
   case ACTION_EXIT:
   case ACTION_FULLSCREEN:
   case ACTION_SEND:
   case ACTION_SENDR:
   case ACTION_SENDL:
   case ACTION_SENDU:
   case ACTION_SENDD:
   case ACTION_MAXTOP:
   case ACTION_MAXTOPLEFT:
   case ACTION_MAXTOPRIGHT:
   case ACTION_MAXBOTTOM:
   case ACTION_MAXBOTTOMLEFT:
   case ACTION_MAXBOTTOMRIGHT:
   case ACTION_MAXLEFT:
   case ACTION_MAXRIGHT:
   case ACTION_MAXV:
   case ACTION_MAXH:
   case ACTION_RESTORE:
   case ACTION_CENTER:
   case ACTION_AT:
   case ACTION_KILL:
      return 1;
   default:
      return 0;
   }
}

/** Get the modifier mask for a key. */
unsigned GetModifierMask(XModifierKeymap *modmap, KeySym key) {

   KeyCode temp;
   int x;

   temp = JXKeysymToKeycode(display, key);
   if(JUNLIKELY(temp == 0)) {
      Warning(_("Specified KeySym is not defined for any KeyCode"));
   }
   for(x = 0; x < 8 * modmap->max_keypermod; x++) {
      if(modmap->modifiermap[x] == temp) {
         return 1 << (x / modmap->max_keypermod);
      }
   }

   Warning(_("modifier not found for keysym 0x%0x"), key);

   return 0;

}

/** Parse a modifier mask string. */
unsigned int ParseModifierString(const char *str)
{
   unsigned mask = MASK_NONE;
   unsigned x;

   if(!str) {
      return mask;
   }

   for(x = 0; str[x]; x++) {
      unsigned y;
      char found = 0;
      for(y = 0; y < MODIFIER_COUNT; y++) {
         if(MODIFIERS[y].name == str[x]) {
            mask |= MODIFIERS[y].mask;
            found = 1;
            break;
         }
      }

      if(JUNLIKELY(!found)) {
         Warning(_("invalid modifier: \"%c\""), str[x]);
      }
   }

   return mask;

}

/** Parse a key string. */
KeySym ParseKeyString(const char *str)
{
   KeySym symbol;
   symbol = JXStringToKeysym(str);
   if(JUNLIKELY(symbol == NoSymbol)) {
      Warning(_("invalid key symbol: \"%s\""), str);
   }
   return symbol;
}

/** Remove a binding. */
void RemoveDuplicates(KeyNode *bp)
{
   KeyNode **npp = &bindings[bp->context];
   while(*npp) {
      KeyNode *np = *npp;
      if(   np != bp
         && np->symbol == bp->symbol
         && np->state == bp->state
         && np->code == bp->code) {
         *npp = np->next;
         if(np->command) {
            Release(np->command);
         }
         Release(np);
      } else {
         npp = &np->next;
      }
   }
}

/** Insert a key binding. */
void InsertBinding(ActionType action, const char *modifiers,
                   const char *stroke, const char *code,
                   const char *command)
{

   KeyNode *np;
   unsigned int mask;
   char *temp;
   KeySym sym;

   mask = ParseModifierString(modifiers);

   if(stroke && strlen(stroke) > 0) {
      int offset;

      for(offset = 0; stroke[offset]; offset++) {
         if(stroke[offset] == '#') {

            temp = CopyString(stroke);

            for(temp[offset] = '1'; temp[offset] <= '9'; temp[offset]++) {

               sym = ParseKeyString(temp);
               if(sym == NoSymbol) {
                  Release(temp);
                  return;
               }

               np = Allocate(sizeof(KeyNode));
               np->next = bindings[MC_NONE];
               bindings[MC_NONE] = np;

               np->context = MC_NONE;
               np->action = action;
               np->action.extra = temp[offset] - '1';
               np->state = mask;
               np->symbol = sym;
               np->command = NULL;
               np->code = 0;
               RemoveDuplicates(np);

            }

            Release(temp);

            return;
         }
      }

      sym = ParseKeyString(stroke);
      if(sym == NoSymbol) {
         return;
      }

      np = Allocate(sizeof(KeyNode));
      np->next = bindings[MC_NONE];
      bindings[MC_NONE] = np;

      np->context = MC_NONE;
      np->action = action;
      np->state = mask;
      np->symbol = sym;
      np->command = CopyString(command);
      np->code = 0;
      RemoveDuplicates(np);

   } else if(code && strlen(code) > 0) {

      np = Allocate(sizeof(KeyNode));
      np->next = bindings[MC_NONE];
      bindings[MC_NONE] = np;

      np->context = MC_NONE;
      np->action = action;
      np->state = mask;
      np->symbol = NoSymbol;
      np->command = CopyString(command);
      np->code = atoi(code);
      RemoveDuplicates(np);

   } else {

      Warning(_("neither key nor keycode specified for Key"));
      np = NULL;

   }
}

/** Insert a mouse binding. */
void InsertMouseBinding(
   int button,
   const char *mask,
   MouseContextType context,
   ActionType action,
   const char *command)
{
   KeyNode *np = Allocate(sizeof(KeyNode));
   np->next = bindings[context];
   bindings[context] = np;

   np->command = CopyString(command);
   np->action = action;
   np->state = ParseModifierString(mask);
   np->symbol = NoSymbol;
   np->code = button;
   np->context = context;
   RemoveDuplicates(np);
}

/** Validate key bindings. */
void ValidateKeys(void)
{
   KeyNode *kp;
   unsigned i;

   for(i = 0; i < MC_COUNT; i++) {
      for(kp = bindings[i]; kp; kp = kp->next) {
         if(kp->action.action == ACTION_ROOT && kp->command) {
            const int bindex = GetRootMenuIndexFromString(kp->command);
            if(JUNLIKELY(!IsRootMenuDefined(bindex))) {
               Warning(_("key binding: root menu \"%s\" not defined"),
                       kp->command);
            }
         }
      }
   }
}

