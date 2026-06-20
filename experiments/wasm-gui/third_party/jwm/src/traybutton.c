/**
 * @file traybutton.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Button tray component.
 *
 */

#include "jwm.h"
#include "traybutton.h"
#include "tray.h"
#include "icon.h"
#include "image.h"
#include "error.h"
#include "root.h"
#include "main.h"
#include "font.h"
#include "button.h"
#include "misc.h"
#include "screen.h"
#include "desktop.h"
#include "popup.h"
#include "timing.h"
#include "command.h"
#include "cursor.h"
#include "settings.h"
#include "event.h"
#include "action.h"

typedef struct TrayButtonType {

   TrayComponentType *cp;

   char *label;
   char *popup;
   char *iconName;
   IconNode *icon;

   int mousex;
   int mousey;
   TimeType mouseTime;

   struct ActionNode *actions;
   struct TrayButtonType *next;

} TrayButtonType;

static TrayButtonType *buttons = NULL;

static void Create(TrayComponentType *cp);
static void Destroy(TrayComponentType *cp);
static void SetSize(TrayComponentType *cp, int width, int height);
static void Resize(TrayComponentType *cp);
static void Draw(TrayComponentType *cp);

static void ProcessButtonPress(TrayComponentType *cp,
                               int x, int y, int button);
static void ProcessButtonRelease(TrayComponentType *cp,
                                 int x, int y, int button);
static void ProcessMotionEvent(TrayComponentType *cp,
                               int x, int y, int mask);
static void SignalTrayButton(const TimeType *now,
                             int x, int y, Window w, void *data);

/** Startup tray buttons. */
void StartupTrayButtons(void)
{
   TrayButtonType *bp;
   for(bp = buttons; bp; bp = bp->next) {
      if(bp->label) {
         bp->cp->requestedWidth = BUTTON_BORDER * 2;
         bp->cp->requestedWidth += GetStringWidth(FONT_TRAY, bp->label);
         bp->cp->requestedHeight = BUTTON_BORDER * 2;
         bp->cp->requestedHeight += GetStringHeight(FONT_TRAY);
      } else {
         bp->cp->requestedWidth = 0;
         bp->cp->requestedHeight = 0;
      }
      if(bp->iconName) {
         bp->icon = LoadNamedIcon(bp->iconName, 1, 1);
         if(JLIKELY(bp->icon)) {
            bp->cp->requestedWidth = Max(
              bp->cp->requestedWidth + BUTTON_BORDER,
              BUTTON_BORDER * 2);
            bp->cp->requestedWidth += bp->icon->width;
            bp->cp->requestedHeight = Max(
              bp->icon->height + BUTTON_BORDER * 2,
              bp->cp->requestedHeight);
         } else {
            Warning(_("could not load tray icon: \"%s\""), bp->iconName);
         }
      }
   }
}

/** Release tray button data. */
void DestroyTrayButtons(void)
{
   TrayButtonType *bp;
   while(buttons) {
      bp = buttons->next;
      UnregisterCallback(SignalTrayButton, buttons);
      if(buttons->label) {
         Release(buttons->label);
      }
      if(buttons->iconName) {
         Release(buttons->iconName);
      }
      DestroyActions(buttons->actions);
      if(buttons->popup) {
         Release(buttons->popup);
      }
      Release(buttons);
      buttons = bp;
   }
}

/** Create a button tray component. */
TrayComponentType *CreateTrayButton(const char *iconName,
                                    const char *label,
                                    const char *popup,
                                    unsigned int width,
                                    unsigned int height)
{

   TrayButtonType *bp;
   TrayComponentType *cp;

   if(JUNLIKELY((label == NULL || strlen(label) == 0)
      && (iconName == NULL || strlen(iconName) == 0))) {
      Warning(_("no icon or label for TrayButton"));
      return NULL;
   }

   bp = Allocate(sizeof(TrayButtonType));
   bp->next = buttons;
   buttons = bp;

   bp->icon = NULL;
   bp->iconName = CopyString(iconName);
   bp->label = CopyString(label);
   bp->actions = NULL;
   bp->popup = CopyString(popup);

   cp = CreateTrayComponent();
   cp->object = bp;
   bp->cp = cp;
   cp->requestedWidth = width;
   cp->requestedHeight = height;

   bp->mousex = -settings.doubleClickDelta;
   bp->mousey = -settings.doubleClickDelta;

   cp->Create = Create;
   cp->Destroy = Destroy;
   cp->SetSize = SetSize;
   cp->Resize = Resize;
   cp->Redraw = Draw;

   cp->ProcessButtonPress = ProcessButtonPress;
   cp->ProcessButtonRelease = ProcessButtonRelease;
   if(popup || label) {
      cp->ProcessMotionEvent = ProcessMotionEvent;
   }

   RegisterCallback(settings.popupDelay / 2, SignalTrayButton, bp);

   return cp;

}

/** Add a action to a tray button. */
void AddTrayButtonAction(TrayComponentType *cp,
                         const char *action,
                         int mask)
{
   TrayButtonType *bp = (TrayButtonType*)cp->object;
   AddAction(&bp->actions, action, mask);
}

/** Set the size of a button tray component. */
void SetSize(TrayComponentType *cp, int width, int height)
{
   TrayButtonType *bp;
   int labelWidth, labelHeight;
   int iconWidth, iconHeight;

   bp = (TrayButtonType*)cp->object;

   if(bp->label) {
      labelWidth = GetStringWidth(FONT_TRAY, bp->label);
      labelHeight = GetStringHeight(FONT_TRAY);
      if(bp->icon) {
         /* Padding on the left if there is an icon. */
         labelWidth += BUTTON_BORDER;
      }
   } else {
      labelWidth = 0;
      labelHeight = 0;
   }

   if(bp->icon && bp->icon->width && bp->icon->height) {
      /* With an icon. */
      int ratio;

      iconWidth = bp->icon->width;
      iconHeight = bp->icon->height;

      /* Fixed point with 16 bit fraction. */
      ratio = (iconWidth << 16) / iconHeight;

      if(width > 0) {

         /* Compute height from width. */
         iconWidth = width - labelWidth - BUTTON_BORDER * 2;
         iconHeight = (iconWidth << 16) / ratio;
         height = Max(labelHeight, iconHeight) + BUTTON_BORDER * 2;

      } else if(height > 0) {

         /* Compute width from height. */
         iconHeight = height - BUTTON_BORDER * 2;
         iconWidth = (iconHeight * ratio) >> 16;
         width = iconWidth + labelWidth + BUTTON_BORDER * 2;

      } else {

         /* Use best size. */
         height = Max(labelHeight, iconHeight) + BUTTON_BORDER * 2;
         iconWidth = ((height - BUTTON_BORDER * 2) * ratio) >> 16;
         width = iconWidth + labelWidth + BUTTON_BORDER * 2;

      }

   } else {
      /* No icon. */
      if(width > 0) {
         height = labelHeight + BUTTON_BORDER * 2;
      } else if(height > 0) {
         width = labelWidth + BUTTON_BORDER * 2;
      } else {
         height = labelHeight + BUTTON_BORDER * 2;
         width = labelWidth + BUTTON_BORDER * 2;
      }
   }

   cp->width = width;
   cp->height = height;

}

/** Initialize a button tray component. */
void Create(TrayComponentType *cp)
{
   cp->pixmap = JXCreatePixmap(display, rootWindow,
                               cp->width, cp->height, rootDepth);
   Draw(cp);
}

/** Resize a button tray component. */
void Resize(TrayComponentType *cp)
{
   Destroy(cp);
   Create(cp);
}

/** Destroy a button tray component. */
void Destroy(TrayComponentType *cp)
{
   if(cp->pixmap != None) {
      JXFreePixmap(display, cp->pixmap);
   }
}

/** Draw a tray button. */
void Draw(TrayComponentType *cp)
{

   ButtonNode button;
   TrayButtonType *bp;

   bp = (TrayButtonType*)cp->object;

   ClearTrayDrawable(cp);
   ResetButton(&button, cp->pixmap);
   if(cp->grabbed) {
      button.type = BUTTON_TRAY_ACTIVE;
   } else {
      button.type = BUTTON_TRAY;
   }
   button.width = cp->width;
   button.height = cp->height;
   button.border = settings.trayDecorations == DECO_MOTIF;
   button.x = 0;
   button.y = 0;
   button.font = FONT_TRAY;
   button.text = bp->label;
   button.labelPos = LABEL_POSITION_RIGHT;
   button.icon = bp->icon;
   DrawButton(&button);

}

/** Process a button press. */
void ProcessButtonPress(TrayComponentType *cp, int x, int y, int button)
{
   const TrayButtonType *bp = (TrayButtonType*)cp->object;
   ProcessActionPress(bp->actions, cp, x, y, button);
}

/** Process a button release. */
void ProcessButtonRelease(TrayComponentType *cp, int x, int y, int button)
{
   const TrayButtonType *bp = (TrayButtonType*)cp->object;
   ProcessActionRelease(bp->actions, cp, x, y, button);
}

/** Process a motion event. */
void ProcessMotionEvent(TrayComponentType *cp, int x, int y, int mask)
{
   TrayButtonType *bp = (TrayButtonType*)cp->object;
   bp->mousex = cp->screenx + x;
   bp->mousey = cp->screeny + y;
   GetCurrentTime(&bp->mouseTime);
}

/** Signal (needed for popups). */
void SignalTrayButton(const TimeType *now, int x, int y, Window w, void *data)
{
   TrayButtonType *bp = (TrayButtonType*)data;
   const char *popup;

   if(bp->popup) {
      popup = bp->popup;
   } else if(bp->label) {
      popup = bp->label;
   } else {
      return;
   }
   if(bp->cp->tray->window == w &&
      abs(bp->mousex - x) < settings.doubleClickDelta &&
      abs(bp->mousey - y) < settings.doubleClickDelta) {
      if(GetTimeDifference(now, &bp->mouseTime) >= settings.popupDelay) {
         ShowPopup(x, y, popup, POPUP_BUTTON);
      }
   }
}

/** Validate tray buttons. */
void ValidateTrayButtons(void)
{
   const TrayButtonType *bp;
   for(bp = buttons; bp; bp = bp->next) {
      ValidateActions(bp->actions);
   }
}
