/**
 * @file spacer.h
 * @author Joe Wingbermuehle
 * @date 2011
 *
 * @brief Spacer tray component.
 *
 */

#include "jwm.h"
#include "main.h"
#include "spacer.h"
#include "tray.h"

static void Create(TrayComponentType *cp);
static void Destroy(TrayComponentType *cp);
static void SetSize(TrayComponentType *cp, int width, int height);
static void Resize(TrayComponentType *cp);

/** Create a spacer tray component. */
TrayComponentType *CreateSpacer(int width, int height)
{

   TrayComponentType *cp;

   if(JUNLIKELY(width < 0)) {
      width = 0;
   }
   if(JUNLIKELY(height < 0)) {
      height = 0;
   }

   cp = CreateTrayComponent();
   cp->requestedWidth = width;
   cp->requestedHeight = height;

   cp->Create = Create;
   cp->Destroy = Destroy;
   cp->SetSize = SetSize;
   cp->Resize = Resize;

   return cp;

}

/** Set the size. */
void SetSize(TrayComponentType *cp, int width, int height)
{
   if(width == 0) {
      cp->width = cp->requestedWidth;
      cp->height = height;
   } else {
      cp->width = width;
      cp->height = cp->requestedHeight;
   }
}

/** Initialize. */
void Create(TrayComponentType *cp)
{
   cp->pixmap = JXCreatePixmap(display, rootWindow, cp->width, cp->height,
                               rootDepth);
   ClearTrayDrawable(cp);
}

/** Resize. */
void Resize(TrayComponentType *cp)
{
   if(cp->pixmap != None) {
      JXFreePixmap(display, cp->pixmap);
   }
   cp->pixmap = JXCreatePixmap(display, rootWindow, cp->width, cp->height,
                               rootDepth);
   ClearTrayDrawable(cp);
}

/** Destroy. */
void Destroy(TrayComponentType *cp)
{
   if(cp->pixmap != None) {
      JXFreePixmap(display, cp->pixmap);
   }
}

