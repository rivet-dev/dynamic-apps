/**
 * @file border.c
 * @author Joe Wingbermuehle
 * @date 2004-2015
 *
 * @brief Functions for handling window borders.
 * 
 */

#include "jwm.h"
#include "border.h"
#include "client.h"
#include "clientlist.h"
#include "color.h"
#include "icon.h"
#include "font.h"
#include "misc.h"
#include "settings.h"
#include "grab.h"

static char *buttonNames[BI_COUNT];
static IconNode *buttonIcons[BI_COUNT];

static char IsContextEnabled(MouseContextType context, const ClientNode *np);
static void DrawBorderHelper(const ClientNode *np);
static void DrawBorderHandles(const ClientNode *np,
                              Pixmap canvas, GC gc);
static void DrawBorderButton(const ClientNode *np, MouseContextType context,
                             int x, int y, Pixmap canvas, GC gc, long fg);
static void DrawButtonBorder(const ClientNode *np, int x,
                             Pixmap canvas, GC gc);
static void DrawLeftButton(const ClientNode *np, MouseContextType context,
                           int x, int y, Pixmap canvas, GC gc, long fg);
static void DrawRightButton(const ClientNode *np, MouseContextType context,
                            int x, int y, Pixmap canvas, GC gc, long fg);
static XPoint DrawBorderButtons(const ClientNode *np, Pixmap canvas, GC gc);
static char DrawBorderIcon(BorderIconType t,
                           unsigned xoffset, unsigned yoffset,
                           Pixmap canvas, long fg);
static void DrawIconButton(const ClientNode *np, int x, int y,
                           Pixmap canvas, GC gc, long fg, char active);
static void DrawCloseButton(unsigned xoffset, unsigned yoffset,
                            Pixmap canvas, GC gc, long fg, char active);
static void DrawMaxIButton(unsigned xoffset, unsigned yoffset,
                           Pixmap canvas, GC gc, long fg, char active);
static void DrawMaxAButton(unsigned xoffset, unsigned yoffset,
                           Pixmap canvas, GC gc, long fg, char active);
static void DrawMinButton(unsigned xoffset, unsigned yoffset,
                          Pixmap canvas, GC gc, long fg, char active);

#ifdef USE_SHAPE
static void FillRoundedRectangle(Drawable d, GC gc, int x, int y,
                                 int width, int height, int radius);
#endif

/** Initialize structures. */
void InitializeBorders(void)
{
   memset(buttonNames, 0, sizeof(buttonNames));
}

/** Initialize server resources. */
void StartupBorders(void)
{
   unsigned int i;

   for(i = 0; i < BI_COUNT; i++) {
      if(buttonNames[i]) {
         buttonIcons[i] = LoadNamedIcon(buttonNames[i], 1, 1);
         Release(buttonNames[i]);
         buttonNames[i] = NULL;
      } else {
         buttonIcons[i] = NULL;
      }
   }

   /* Always load a menu icon for windows without one. */
   if(buttonIcons[BI_MENU] == NULL) {
      buttonIcons[BI_MENU] = GetDefaultIcon();
   }
}

/** Destroy structures. */
void DestroyBorders(void)
{
   unsigned i;
   for(i = 0; i < BI_COUNT; i++)
   {
      if(buttonNames[i]) {
         Release(buttonNames[i]);
         buttonNames[i] = NULL;
      }
   }
}

/** Get the size of the icon to display on a window. */
int GetBorderIconSize(void)
{
   const unsigned height = GetTitleHeight();
   if(settings.windowDecorations == DECO_MOTIF) {
      return Max((int)height - 4, 0);
   } else {
      return Max((int)height - 6, 0);
   }
}

/** Determine if the specified context is available for a client. */
char IsContextEnabled(MouseContextType context, const ClientNode *np)
{
   const BorderFlags flags = np->state.border;
   switch(context) {
   case MC_BORDER:   return (flags & BORDER_RESIZE) != 0;
   case MC_MOVE:     return (flags & BORDER_MOVE) != 0;
   case MC_CLOSE:    return (flags & BORDER_CLOSE) != 0;
   case MC_MAXIMIZE: return (flags & BORDER_MAX) != 0;
   case MC_MINIMIZE: return (flags & BORDER_MIN) != 0;
   case MC_ICON:     return 1;
   default:          return 0;
   }
}

/** Determine the border action to take given coordinates. */
MouseContextType GetBorderContext(const ClientNode *np, int x, int y)
{
   int north, south, east, west;
   unsigned resizeMask;
   const unsigned titleHeight = GetTitleHeight();

   GetBorderSize(&np->state, &north, &south, &east, &west);

   /* Check title bar actions. */
   if((np->state.border & BORDER_TITLE) &&
      titleHeight > settings.borderWidth) {
      int rightOffset = np->width + west;
      int leftOffset = west;

      if(y >= south && y <= titleHeight + south) {
         int index = 0;
         int titleIndex;

         /* Check button(s) to the left of the title. */
         while(settings.titleBarLayout[index]) {
            const int nextOffset = leftOffset + titleHeight + 1;
            const MouseContextType context = settings.titleBarLayout[index];
            if(context == MC_MOVE) {
               /* At the title. */
               break;
            }
            if(nextOffset >= np->width - west) {
               /* Past the end of the window. */
               break;
            }

            if(IsContextEnabled(context, np)) {
               if(x >= leftOffset && x < nextOffset) {
                  return context;
               }
               leftOffset = nextOffset;
            }

            index += 1;
         }

         /* Seek to the last title bar component. */
         titleIndex = index;
         while(settings.titleBarLayout[index]) index += 1;
         index -= 1;

         /* Check button(s) on to right of the title. */
         while(index > titleIndex) {
            const int nextOffset = rightOffset - titleHeight - 1;
            const MouseContextType context = settings.titleBarLayout[index];
            if(context == MC_MOVE) {
               /* Hit the title bar from the right. */
               break;
            }
            if(nextOffset < leftOffset) {
               /* No more room. */
               break;
            }

            if(IsContextEnabled(context, np)) {
               if(x >= nextOffset && x < rightOffset) {
                  return context;
               }
               rightOffset = nextOffset;
            }

            index -= 1;
         }
      }

      /* Check for move. */
      if(y >= south && y <= titleHeight + south) {
         if(x >= leftOffset && x < rightOffset) {
            if(np->state.border & BORDER_MOVE) {
               return MC_MOVE;
            } else {
               return MC_NONE;
            }
         }
      }

   }

   /* Now we check resize actions.
    * There is no need to go further if resizing isn't allowed. */
   if(!(np->state.border & BORDER_RESIZE)) {
      return MC_NONE;
   }

   resizeMask = MC_BORDER_S | MC_BORDER_N
              | MC_BORDER_E | MC_BORDER_W
              | MC_BORDER;
   if(np->state.status & STAT_SHADED) {
      resizeMask &= ~(MC_BORDER_N | MC_BORDER_S);
   }

   /* Check south east/west and north east/west resizing. */
   if(y > np->height + north - titleHeight) {
      if(x < titleHeight) {
         return (MC_BORDER_S | MC_BORDER_W | MC_BORDER) & resizeMask;
      } else if(x > np->width + west - titleHeight) {
         return (MC_BORDER_S | MC_BORDER_E | MC_BORDER) & resizeMask;
      }
   } else if(y < titleHeight) {
      if(x < titleHeight) {
         return (MC_BORDER_N | MC_BORDER_W | MC_BORDER) & resizeMask;
      } else if(x > np->width + west - titleHeight) {
         return (MC_BORDER_N | MC_BORDER_E | MC_BORDER) & resizeMask;
      }
   }

   /* Check east, west, north, and south resizing. */
   if(x <= west) {
      return (MC_BORDER_W | MC_BORDER) & resizeMask;
   } else if(x >= np->width + west) {
      return (MC_BORDER_E | MC_BORDER) & resizeMask;
   } else if(y >= np->height + north) {
      return (MC_BORDER_S | MC_BORDER) & resizeMask;
   } else if(y <= south) {
      return (MC_BORDER_N | MC_BORDER) & resizeMask;
   } else {
      return MC_NONE;
   }

}

/** Reset the shape of a window border. */
void ResetBorder(const ClientNode *np)
{
#ifdef USE_SHAPE
   Pixmap shapePixmap;
   GC shapeGC;
#endif

   int north, south, east, west;
   int width, height;

   if(np->parent == None) {
      JXMoveResizeWindow(display, np->window, np->x, np->y,
                         np->width, np->height);
      return;
   }

   GrabServer();

   /* Determine the size of the window. */
   GetBorderSize(&np->state, &north, &south, &east, &west);
   width = np->width + east + west;
   if(np->state.status & STAT_SHADED) {
      height = north + south;
   } else {
      height = np->height + north + south;
   }

   /** Set the window size. */
   if(!(np->state.status & STAT_SHADED)) {
      JXMoveResizeWindow(display, np->window, west, north,
                         np->width, np->height);
   }
   JXMoveResizeWindow(display, np->parent, np->x - west, np->y - north,
                      width, height);

#ifdef USE_SHAPE
   if(settings.cornerRadius > 0 || (np->state.status & STAT_SHAPED)) {

      /* First set the shape to the window border. */
      shapePixmap = JXCreatePixmap(display, np->parent, width, height, 1);
      shapeGC = JXCreateGC(display, shapePixmap, 0, NULL);

      /* Make the whole area transparent. */
      JXSetForeground(display, shapeGC, 0);
      JXFillRectangle(display, shapePixmap, shapeGC, 0, 0, width, height);

      /* Draw the window area without the corners. */
      /* Corner bound radius -1 to allow slightly better outline drawing */
      JXSetForeground(display, shapeGC, 1);
      if((np->state.status & STAT_FULLSCREEN) &&
         !(np->state.status & STAT_SHADED)) {
         JXFillRectangle(display, shapePixmap, shapeGC, 0, 0, width, height);
      } else {
         const int radius
            = (np->state.maxFlags && (np->state.border & BORDER_NOMAX))
            ? 0 : (settings.cornerRadius - 1);
         FillRoundedRectangle(shapePixmap, shapeGC, 0, 0, width, height,
                              radius);
      }

      /* Apply the client window. */
      if(!(np->state.status & STAT_SHADED) &&
          (np->state.status & STAT_SHAPED)) {

         XRectangle *rects;
         int count;
         int ordering;

         /* Cut out an area for the client window. */
         JXSetForeground(display, shapeGC, 0);
         JXFillRectangle(display, shapePixmap, shapeGC, west, north,
                         np->width, np->height);

         /* Fill in the visible area. */
         rects = JXShapeGetRectangles(display, np->window, ShapeBounding,
                                      &count, &ordering);
         if(JLIKELY(rects)) {
            int i;
            for(i = 0; i < count; i++) {
               rects[i].x += east;
               rects[i].y += north;
            }
            JXSetForeground(display, shapeGC, 1);
            JXFillRectangles(display, shapePixmap, shapeGC, rects, count);
            JXFree(rects);
         }

      }

      /* Set the shape. */
      JXShapeCombineMask(display, np->parent, ShapeBounding, 0, 0,
                         shapePixmap, ShapeSet);

      JXFreeGC(display, shapeGC);
      JXFreePixmap(display, shapePixmap);
   }
#endif

   UngrabServer();

}

/** Draw a client border. */
void DrawBorder(ClientNode *np)
{

   Assert(np);

   /* Don't draw any more if we are shutting down. */
   if(JUNLIKELY(shouldExit)) {
      return;
   }

   /* Must be either mapped or shaded to have a border. */
   if(!(np->state.status & (STAT_MAPPED | STAT_SHADED))) {
      return;
   }

   /* Hidden and fullscreen windows don't get borders. */
   if(np->state.status & (STAT_HIDDEN | STAT_FULLSCREEN)) {
      return;
   }

   /* Create the frame if needed. */
   ReparentClient(np);

   /* Return if there is no border. */
   if(np->parent == None) {
      return;
   }

   /* Do the actual drawing. */
   DrawBorderHelper(np);

}

/** Helper method for drawing borders. */
void DrawBorderHelper(const ClientNode *np)
{
   ColorType borderTextColor;

   long titleColor1, titleColor2;
   long outlineColor;
   GradientDirection gradient;

   int north, south, east, west;
   unsigned int width, height;
   const int titleHeight = GetTitleHeight();

   Pixmap canvas;
   GC gc;

   Assert(np);

   GetBorderSize(&np->state, &north, &south, &east, &west);
   width = np->width + east + west;
   height = np->height + north + south;

   /* Determine the colors and gradients to use. */
   if(np->state.status & (STAT_ACTIVE | STAT_FLASH)) {
      borderTextColor = COLOR_TITLE_ACTIVE_FG;
      titleColor1 = colors[COLOR_TITLE_ACTIVE_BG1];
      titleColor2 = colors[COLOR_TITLE_ACTIVE_BG2];
      outlineColor = colors[COLOR_TITLE_ACTIVE_DOWN];
      gradient = gradients[COLOR_TITLE_ACTIVE_BG1];
   } else {
      borderTextColor = COLOR_TITLE_FG;
      titleColor1 = colors[COLOR_TITLE_BG1];
      titleColor2 = colors[COLOR_TITLE_BG2];
      outlineColor = colors[COLOR_TITLE_DOWN];
      gradient = gradients[COLOR_TITLE_BG1];
   }

   /* Set parent background to reduce flicker. */
   JXSetWindowBackground(display, np->parent, titleColor2);

   canvas = JXCreatePixmap(display, np->parent, width, north, rootDepth);
   gc = JXCreateGC(display, canvas, 0, NULL);

   /* Clear the window with the right color. */
   JXSetForeground(display, gc, titleColor2);
   JXFillRectangle(display, canvas, gc, 0, 0, width, north);

   /* Draw the top part (either a title or north border). */
   if((np->state.border & BORDER_TITLE) &&
      !(np->state.maxFlags && (np->state.border & TITLE_NOMAX)) &&
      titleHeight > settings.borderWidth) {

      XPoint point;
      const unsigned gradientHeight
        = settings.windowDecorations == DECO_MOTIF
        ? titleHeight + settings.borderWidth : titleHeight;

      /* Draw a title bar. */
      DrawGradient(canvas, gc, titleColor1, titleColor2,
                   0, 0, width, gradientHeight, gradient);

      /* Draw the buttons.
       * This returns the start and end positions of the title as `x` and `y`.
       */
      point = DrawBorderButtons(np, canvas, gc);

      /* Draw the title. */
      if(np->name && np->name[0] && point.x < point.y) {
         unsigned titleWidth = point.y - point.x;
         const int sheight = GetStringHeight(FONT_BORDER);
         char *titleBuffer;
         if (settings.showClientName && np->clientName && np->clientName[0]){
             /* Space for 2 delimiters, space, terminator, and strings */
             const size_t buffSize = strlen(np->name) + strlen(np->clientName) + 4;
             titleBuffer = Allocate(buffSize);
             sprintf(titleBuffer,"%s %c%s%c",np->name,
                     settings.clientNameDelimiters[0],np->clientName,
                     settings.clientNameDelimiters[1]);
         } else {
             titleBuffer = CopyString(np->name);
         }
         const int textWidth = GetStringWidth(FONT_BORDER,titleBuffer);
         unsigned titlex, titley;
         int xoffset = 0;

         switch (settings.titleTextAlignment) {
         case ALIGN_CENTER:
            xoffset = (int)(titleWidth - textWidth) / 2;
            break;
         case ALIGN_RIGHT:
            xoffset = (titleWidth - textWidth);
            break;
         }
         xoffset = Max(xoffset, 0);
         titlex = point.x + xoffset;
         titlex = Min(Max(titlex, point.x), point.y);

         titleWidth = Min(titleWidth, point.y - titlex);

         titley = (titleHeight - sheight) / 2;
         if(settings.windowDecorations == DECO_MOTIF) {
            titley += south - 1;
         }
         RenderString(canvas, FONT_BORDER, borderTextColor,
                      titlex, titley, titleWidth, titleBuffer);
         Release(titleBuffer);
      }

   }

   /* Copy the pixmap for the title bar and clear the part of
    * the window to be drawn directly. */
   if(settings.windowDecorations == DECO_MOTIF) {
      const int off = 2;
      JXCopyArea(display, canvas, np->parent, gc, off, off,
         width - 2 * off, north - off, off, off);
      JXClearArea(display, np->parent,
         off, north, width - 2 * off, height - north - off, False);
   } else {
      JXCopyArea(display, canvas, np->parent, gc, 1, 1,
         width - 2, north - 1, 1, 1);
      JXClearArea(display, np->parent,
         1, north, width - 2, height - north - 1, False);
   }

   /* Window outline. */
   if(settings.windowDecorations == DECO_MOTIF) {
      DrawBorderHandles(np, np->parent, gc);
   } else {
      const int radius
         = (np->state.maxFlags && (np->state.border & BORDER_NOMAX))
         ? 0 : settings.cornerRadius;
      JXSetForeground(display, gc, outlineColor);
      if(np->state.status & STAT_SHADED) {
         DrawRoundedRectangle(np->parent, gc, 0, 0, width - 1, north - 1,
                              radius);
      } else {
         DrawRoundedRectangle(np->parent, gc, 0, 0, width - 1, height - 1,
                              radius);
      }
   }

   JXFreePixmap(display, canvas);
   JXFreeGC(display, gc);

}

/** Draw window handles. */
void DrawBorderHandles(const ClientNode *np, Pixmap canvas, GC gc)
{
   XSegment segments[9];
   long pixelUp, pixelDown;
   int width, height;
   int north, south, east, west;
   unsigned offset = 0;
   unsigned starty = 0;
   unsigned titleHeight;

   if(np->state.maxFlags && (np->state.border & BORDER_NOMAX)) {
      return;
   }

   /* Determine the window size. */
   GetBorderSize(&np->state, &north, &south, &east, &west);
   titleHeight = GetTitleHeight();
   width = np->width + east + west;
   if(np->state.status & STAT_SHADED) {
      height = north + south;
   } else {
      height = np->height + north + south;
   }

   /* Determine the y-offset to start drawing. */
   starty = settings.borderWidth;

   /* Determine the colors to use. */
   if(np->state.status & (STAT_ACTIVE | STAT_FLASH)) {
      pixelUp = colors[COLOR_TITLE_ACTIVE_UP];
      pixelDown = colors[COLOR_TITLE_ACTIVE_DOWN];
   } else {
      pixelUp = colors[COLOR_TITLE_UP];
      pixelDown = colors[COLOR_TITLE_DOWN];
   }

   /* Top title border. */
   segments[offset].x1 = west;
   segments[offset].y1 = settings.borderWidth;
   segments[offset].x2 = width - east - 1;
   segments[offset].y2 = settings.borderWidth;
   offset += 1;

   /* Inside bottom border. */
   segments[offset].x1 = west;
   segments[offset].y1 = height - south;
   segments[offset].x2 = width - east;
   segments[offset].y2 = height - south;
   offset += 1;

   /* Top border. */
   segments[offset].x1 = 1;
   segments[offset].y1 = 0;
   segments[offset].x2 = width - 1;
   segments[offset].y2 = 0;
   offset += 1;
   segments[offset].x1 = 1;
   segments[offset].y1 = 1;
   segments[offset].x2 = width - 2;
   segments[offset].y2 = 1;
   offset += 1;

   /* Right title border. */
   segments[offset].x1 = west;
   segments[offset].y1 = starty + 1;
   segments[offset].x2 = east;
   segments[offset].y2 = titleHeight + south - 1;
   offset += 1;

   /* Inside right border. */
   segments[offset].x1 = width - east;
   segments[offset].y1 = starty;
   segments[offset].x2 = width - east;
   segments[offset].y2 = height - south;
   offset += 1;

   /* Inside left border. */
   segments[offset].x1 = west;
   segments[offset].y1 = starty;
   segments[offset].x2 = west;
   segments[offset].y2 = starty + titleHeight;
   offset += 1;

   /* Left border. */
   segments[offset].x1 = 0;
   segments[offset].y1 = 0;
   segments[offset].x2 = 0;
   segments[offset].y2 = height - 1;
   offset += 1;
   segments[offset].x1 = 1;
   segments[offset].y1 = 1;
   segments[offset].x2 = 1;
   segments[offset].y2 = height - 2;
   offset += 1;

   /* Draw pixel-up segments. */
   JXSetForeground(display, gc, pixelUp);
   JXDrawSegments(display, canvas, gc, segments, offset);
   offset = 0;

   /* Bottom title border. */
   segments[offset].x1 = west + 1;
   segments[offset].y1 = north - 1;
   segments[offset].x2 = width - east - 1;
   segments[offset].y2 = north - 1;
   offset += 1;

   /* Inside top border. */
   segments[offset].x1 = west - 1;
   segments[offset].y1 = settings.borderWidth - 1;
   segments[offset].x2 = width - east;
   segments[offset].y2 = settings.borderWidth - 1;
   offset += 1;

   /* Bottom border. */
   segments[offset].x1 = 0;
   segments[offset].y1 = height - 1;
   segments[offset].x2 = width;
   segments[offset].y2 = height - 1;
   offset += 1;
   segments[offset].x1 = 1;
   segments[offset].y1 = height - 2;
   segments[offset].x2 = width - 1;
   segments[offset].y2 = height - 2;
   offset += 1;

   /* Right title border. */
   segments[offset].x1 = width - east - 1;
   segments[offset].y1 = starty + 1;
   segments[offset].x2 = width - east - 1;
   segments[offset].y2 = north - 1;
   offset += 1;

   /* Inside left border. */
   segments[offset].x1 = west - 1;
   segments[offset].y1 = starty;
   segments[offset].x2 = west - 1;
   segments[offset].y2 = height - starty;
   offset += 1;

   /* Right border. */
   segments[offset].x1 = width - 1;
   segments[offset].y1 = 0;
   segments[offset].x2 = width - 1;
   segments[offset].y2 = height - 1;
   offset += 1;
   segments[offset].x1 = width - 2;
   segments[offset].y1 = 1;
   segments[offset].x2 = width - 2;
   segments[offset].y2 = height - 2;
   offset += 1;

   /* Draw pixel-down segments. */
   JXSetForeground(display, gc, pixelDown);
   JXDrawSegments(display, canvas, gc, segments, offset);
   offset = 0;

   /* Draw marks */
   if(    (np->state.border & BORDER_RESIZE)
      && !(np->state.status & STAT_SHADED)) {

      /* Upper left */
      segments[0].x1 = titleHeight + settings.borderWidth - 1;
      segments[0].y1 = 0;
      segments[0].x2 = titleHeight + settings.borderWidth - 1;
      segments[0].y2 = settings.borderWidth;
      segments[1].x1 = 0;
      segments[1].y1 = titleHeight + settings.borderWidth - 1;
      segments[1].x2 = settings.borderWidth;
      segments[1].y2 = titleHeight + settings.borderWidth - 1;

      /* Upper right. */
      segments[2].x1 = width - settings.borderWidth;
      segments[2].y1 = titleHeight + settings.borderWidth - 1;
      segments[2].x2 = width;
      segments[2].y2 = titleHeight + settings.borderWidth - 1;
      segments[3].x1 = width - titleHeight - settings.borderWidth - 1;
      segments[3].y1 = 0;
      segments[3].x2 = width - titleHeight - settings.borderWidth - 1;
      segments[3].y2 = settings.borderWidth;

      /* Lower left */
      segments[4].x1 = 0;
      segments[4].y1 = height - titleHeight - settings.borderWidth - 1;
      segments[4].x2 = settings.borderWidth;
      segments[4].y2 = height - titleHeight - settings.borderWidth - 1;
      segments[5].x1 = titleHeight + settings.borderWidth - 1;
      segments[5].y1 = height - settings.borderWidth;
      segments[5].x2 = titleHeight + settings.borderWidth - 1;
      segments[5].y2 = height;

      /* Lower right */
      segments[6].x1 = width - settings.borderWidth;
      segments[6].y1 = height - titleHeight - settings.borderWidth - 1;
      segments[6].x2 = width;
      segments[6].y2 = height - titleHeight - settings.borderWidth - 1;
      segments[7].x1 = width - titleHeight - settings.borderWidth - 1;
      segments[7].y1 = height - settings.borderWidth;
      segments[7].x2 = width - titleHeight - settings.borderWidth - 1;
      segments[7].y2 = height;

      /* Draw pixel-down segments. */
      JXSetForeground(display, gc, pixelDown);
      JXDrawSegments(display, canvas, gc, segments, 8);

      /* Upper left */
      segments[0].x1 = titleHeight + settings.borderWidth;
      segments[0].y1 = 0;
      segments[0].x2 = titleHeight + settings.borderWidth;
      segments[0].y2 = settings.borderWidth;
      segments[1].x1 = 0;
      segments[1].y1 = titleHeight + settings.borderWidth;
      segments[1].x2 = settings.borderWidth;
      segments[1].y2 = titleHeight + settings.borderWidth;

      /* Upper right */
      segments[2].x1 = width - titleHeight - settings.borderWidth;
      segments[2].y1 = 0;
      segments[2].x2 = width - titleHeight - settings.borderWidth;
      segments[2].y2 = settings.borderWidth;
      segments[3].x1 = width - settings.borderWidth;
      segments[3].y1 = titleHeight + settings.borderWidth;
      segments[3].x2 = width;
      segments[3].y2 = titleHeight + settings.borderWidth;

      /* Lower left */
      segments[4].x1 = 0;
      segments[4].y1 = height - titleHeight - settings.borderWidth;
      segments[4].x2 = settings.borderWidth;
      segments[4].y2 = height - titleHeight - settings.borderWidth;
      segments[5].x1 = titleHeight + settings.borderWidth;
      segments[5].y1 = height - settings.borderWidth;
      segments[5].x2 = titleHeight + settings.borderWidth;
      segments[5].y2 = height;

      /* Lower right */
      segments[6].x1 = width - settings.borderWidth;
      segments[6].y1 = height - titleHeight - settings.borderWidth;
      segments[6].x2 = width;
      segments[6].y2 = height - titleHeight - settings.borderWidth;
      segments[7].x1 = width - titleHeight - settings.borderWidth;
      segments[7].y1 = height - settings.borderWidth;
      segments[7].x2 = width - titleHeight - settings.borderWidth;
      segments[7].y2 = height;

      /* Draw pixel-up segments. */
      JXSetForeground(display, gc, pixelUp);
      JXDrawSegments(display, canvas, gc, segments, 8);
   }
}

/** Draw draw a border button (with the border). */
void DrawBorderButton(const ClientNode *np, MouseContextType context,
                      int x, int y, Pixmap canvas, GC gc, long fg)
{
  const char isActive = (np->state.status & STAT_ACTIVE)
                      && IsClientOnCurrentDesktop(np);
   JXSetForeground(display, gc, fg);
   switch(context) {
   case MC_CLOSE:
      DrawCloseButton(x, y, canvas, gc, fg, isActive);
      break;
   case MC_MINIMIZE:
      DrawMinButton(x, y, canvas, gc, fg, isActive);
      break;
   case MC_MAXIMIZE:
      if(np->state.maxFlags) {
         DrawMaxAButton(x, y, canvas, gc, fg, isActive);
      } else {
         DrawMaxIButton(x, y, canvas, gc, fg, isActive);
      }
      break;
   case MC_ICON:
      DrawIconButton(np, x, y, canvas, gc, fg, isActive);
      break;
   default:
      Assert(0);
      break;
   }
}

/** Draw a button border. */
void DrawButtonBorder(const ClientNode *np, int x, Pixmap canvas, GC gc)
{
   long pixelUp, pixelDown;
   const unsigned y1 = settings.borderWidth - 1;
   const unsigned y2 = y1 + GetTitleHeight();
   int north, south, east, west;

   /* Only draw borders for motif decorations. */
   if(settings.windowDecorations != DECO_MOTIF) {
      return;
   }

   /* Determine the colors to use. */
   if(np->state.status & (STAT_ACTIVE | STAT_FLASH)) {
      pixelUp = colors[COLOR_TITLE_ACTIVE_UP];
      pixelDown = colors[COLOR_TITLE_ACTIVE_DOWN];
   } else {
      pixelUp = colors[COLOR_TITLE_UP];
      pixelDown = colors[COLOR_TITLE_DOWN];
   }

   GetBorderSize(&np->state, &north, &south, &east, &west);

   JXSetForeground(display, gc, pixelDown);
   JXDrawLine(display, canvas, gc, x, y1, x, y2);
   JXSetForeground(display, gc, pixelUp);
   JXDrawLine(display, canvas, gc, x + 1, y1, x + 1, y2);
}

/** Draw a button on the left side of the title (with border). */
void DrawLeftButton(const ClientNode *np, MouseContextType context,
                    int x, int y, Pixmap canvas, GC gc, long fg)
{
   DrawButtonBorder(np, x, canvas, gc);
   DrawBorderButton(np, context, x, y, canvas, gc, fg);
}

/** Draw a button on the right side of the title (with border). */
void DrawRightButton(const ClientNode *np, MouseContextType context,
                     int x, int y, Pixmap canvas, GC gc, long fg)
{
   DrawButtonBorder(np, x + GetTitleHeight() - 1, canvas, gc);
   DrawBorderButton(np, context, x, y, canvas, gc, fg);
}

/** Draw the buttons on a client frame. */
XPoint DrawBorderButtons(const ClientNode *np, Pixmap canvas, GC gc)
{
   long fg;
   XPoint point;
   const unsigned titleHeight = GetTitleHeight();
   int titleIndex, index;
   int leftOffset, rightOffset;
   int north, south, east, west;
   int yoffset;

   /* Determine the foreground color to use. */
   if(np->state.status & (STAT_ACTIVE | STAT_FLASH)) {
      fg = colors[COLOR_TITLE_ACTIVE_FG];
   } else {
      fg = colors[COLOR_TITLE_FG];
   }

   GetBorderSize(&np->state, &north, &south, &east, &west);
   if(settings.windowDecorations == DECO_MOTIF) {
      yoffset = south - 1;
   } else {
      yoffset = 0;
   }

   /* Draw buttons to the left of the title. */
   index = 0;
   leftOffset = west;
   while(settings.titleBarLayout[index]) {
      const MouseContextType context = settings.titleBarLayout[index];
      const int nextOffset = leftOffset + titleHeight;
      if(context == MC_MOVE) {
         /* Hit the window title. */
         break;
      }
      if(nextOffset >= np->width - west) {
         /* Past the end of the window. */
         break;
      }

      /* Draw the button only if it's enabled. */
      if(IsContextEnabled(context, np)) {
         DrawRightButton(np, context, leftOffset, yoffset, canvas, gc, fg);
         leftOffset = nextOffset;
      }

      index += 1;
   }

   /* Seek to the last title bar component. */
   titleIndex = index;
   while(settings.titleBarLayout[index]) index += 1;
   index -= 1;

   /* Draw buttons to the right of the title. */
   rightOffset = np->width + west;
   while(index > titleIndex) {
      const int nextOffset = rightOffset - titleHeight;
      const MouseContextType context = settings.titleBarLayout[index];
      if(context == MC_MOVE) {
         /* Hit the title bar from the right. */
         break;
      }
      if(nextOffset < leftOffset) {
         /* No more room. */
         break;
      }

      if(IsContextEnabled(context, np)) {
         rightOffset = nextOffset;
         DrawLeftButton(np, context, rightOffset - 1, yoffset, canvas, gc, fg);
      }

      index -= 1;
   }

   point.x = leftOffset;
   point.y = rightOffset;
   if(settings.windowDecorations == DECO_MOTIF) {
      point.x += 4;
      point.y -= 4;
   }
   return point;
}

/** Attempt to draw a border icon. */
char DrawBorderIcon(BorderIconType t,
                    unsigned xoffset, unsigned yoffset,
                    Pixmap canvas, long fg)
{
   if(buttonIcons[t]) {
#ifdef USE_ICONS
      const unsigned titleHeight = GetTitleHeight();
      PutIcon(buttonIcons[t], canvas, fg, xoffset + 2, yoffset + 2,
              titleHeight - 2, titleHeight - 2);
#endif
      return 1;
   } else {
      return 0;
   }
}

/** Draw a close button. */
void DrawCloseButton(unsigned xoffset, unsigned yoffset,
                     Pixmap canvas, GC gc, long fg, char active)
{
   XSegment segments[2];
   const unsigned titleHeight = GetTitleHeight();
   unsigned size;
   unsigned x1, y1;
   unsigned x2, y2;

   if(active && DrawBorderIcon(BI_CLOSE_FOCUS, xoffset, yoffset, canvas, fg)) {
      return;
   } else if(DrawBorderIcon(BI_CLOSE, xoffset, yoffset, canvas, fg)) {
      return;
   }

   size = (titleHeight + 2) / 3;
   x1 = xoffset + titleHeight / 2 - size / 2;
   y1 = yoffset + titleHeight / 2 - size / 2;
   x2 = x1 + size;
   y2 = y1 + size;

   segments[0].x1 = x1;
   segments[0].y1 = y1;
   segments[0].x2 = x2;
   segments[0].y2 = y2;

   segments[1].x1 = x2;
   segments[1].y1 = y1;
   segments[1].x2 = x1;
   segments[1].y2 = y2;

   JXSetLineAttributes(display, gc, 2, LineSolid,
                       CapProjecting, JoinBevel);
   JXDrawSegments(display, canvas, gc, segments, 2);
   JXSetLineAttributes(display, gc, 1, LineSolid,
                       CapNotLast, JoinMiter);

}

/** Draw an inactive maximize button. */
void DrawMaxIButton(unsigned xoffset, unsigned yoffset,
                    Pixmap canvas, GC gc, long fg, char active)
{

   XSegment segments[5];
   const unsigned titleHeight = GetTitleHeight();
   unsigned int size;
   unsigned int x1, y1;
   unsigned int x2, y2;

   if(active && DrawBorderIcon(BI_MAX_FOCUS, xoffset, yoffset, canvas, fg)) {
      return;
   } else if(DrawBorderIcon(BI_MAX, xoffset, yoffset, canvas, fg)) {
      return;
   }

   size = 2 + (titleHeight + 2) / 3;
   x1 = xoffset + titleHeight / 2 - size / 2;
   y1 = yoffset + titleHeight / 2 - size / 2;
   x2 = x1 + size;
   y2 = y1 + size;

   segments[0].x1 = x1;
   segments[0].y1 = y1;
   segments[0].x2 = x1 + size;
   segments[0].y2 = y1;

   segments[1].x1 = x1;
   segments[1].y1 = y1 + 1;
   segments[1].x2 = x1 + size;
   segments[1].y2 = y1 + 1;

   segments[2].x1 = x1;
   segments[2].y1 = y1;
   segments[2].x2 = x1;
   segments[2].y2 = y2;

   segments[3].x1 = x2;
   segments[3].y1 = y1;
   segments[3].x2 = x2;
   segments[3].y2 = y2;

   segments[4].x1 = x1;
   segments[4].y1 = y2;
   segments[4].x2 = x2;
   segments[4].y2 = y2;

   JXSetLineAttributes(display, gc, 1, LineSolid,
                       CapProjecting, JoinMiter);
   JXDrawSegments(display, canvas, gc, segments, 5);
   JXSetLineAttributes(display, gc, 1, LineSolid,
                       CapButt, JoinMiter);

}

/** Draw an active maximize button. */
void DrawMaxAButton(unsigned xoffset, unsigned yoffset,
                    Pixmap canvas, GC gc, long fg, char active)
{
   XSegment segments[8];
   unsigned titleHeight;
   unsigned size;
   unsigned x1, y1;
   unsigned x2, y2;
   unsigned x3, y3;

   if(active && DrawBorderIcon(BI_MAX_ACTIVE_FOCUS, xoffset, yoffset, canvas, fg)) {
      return;
   } else if(DrawBorderIcon(BI_MAX_ACTIVE, xoffset, yoffset, canvas, fg)) {
      return;
   }

   titleHeight = GetTitleHeight();
   size = 2 + (titleHeight + 2) / 3;
   x1 = xoffset + titleHeight / 2 - size / 2;
   y1 = yoffset + titleHeight / 2 - size / 2;
   x2 = x1 + size;
   y2 = y1 + size;
   x3 = x1 + size / 2;
   y3 = y1 + size / 2;

   segments[0].x1 = x1;
   segments[0].y1 = y1;
   segments[0].x2 = x2;
   segments[0].y2 = y1;

   segments[1].x1 = x1;
   segments[1].y1 = y1 + 1;
   segments[1].x2 = x2;
   segments[1].y2 = y1 + 1;

   segments[2].x1 = x1;
   segments[2].y1 = y1;
   segments[2].x2 = x1;
   segments[2].y2 = y2;

   segments[3].x1 = x2;
   segments[3].y1 = y1;
   segments[3].x2 = x2;
   segments[3].y2 = y2;

   segments[4].x1 = x1;
   segments[4].y1 = y2;
   segments[4].x2 = x2;
   segments[4].y2 = y2;

   segments[5].x1 = x1;
   segments[5].y1 = y3;
   segments[5].x2 = x3;
   segments[5].y2 = y3;

   segments[6].x1 = x1;
   segments[6].y1 = y3 + 1;
   segments[6].x2 = x3;
   segments[6].y2 = y3 + 1;

   segments[7].x1 = x3;
   segments[7].y1 = y3;
   segments[7].x2 = x3;
   segments[7].y2 = y2;

   JXSetLineAttributes(display, gc, 1, LineSolid,
                       CapProjecting, JoinMiter);
   JXDrawSegments(display, canvas, gc, segments, 8);
   JXSetLineAttributes(display, gc, 1, LineSolid,
                       CapButt, JoinMiter);
}

/** Draw a minimize button. */
void DrawMinButton(unsigned xoffset, unsigned yoffset,
                   Pixmap canvas, GC gc, long fg, char active)
{
   unsigned titleHeight;
   unsigned size;
   unsigned x1, y1;
   unsigned x2, y2;

   if(active && DrawBorderIcon(BI_MIN_FOCUS, xoffset, yoffset, canvas, fg)) {
      return;
   } else if(DrawBorderIcon(BI_MIN, xoffset, yoffset, canvas, fg)) {
      return;
   }

   titleHeight = GetTitleHeight();
   size = (titleHeight + 2) / 3;
   x1 = xoffset + titleHeight / 2 - size / 2;
   y1 = yoffset + titleHeight / 2 - size / 2;
   x2 = x1 + size;
   y2 = y1 + size;
   JXSetLineAttributes(display, gc, 2, LineSolid,
                       CapProjecting, JoinMiter);
   JXDrawLine(display, canvas, gc, x1, y2, x2, y2);
   JXSetLineAttributes(display, gc, 1, LineSolid, CapButt, JoinMiter);

}


/* Draw the title icon. */
void DrawIconButton(const ClientNode *np, int x, int y,
                    Pixmap canvas, GC gc, long fg, char active)
{
#ifdef USE_ICONS
   const char hasIcon = np->icon ? 1 : 0;
   const int titleHeight = GetTitleHeight();
   const int iconSize = hasIcon ? GetBorderIconSize() : Max((int)titleHeight - 2, 0);
   const int iconXOffset = (titleHeight - iconSize) / 2;
   const int iconYOffset = hasIcon ? iconXOffset : iconXOffset + 1;
   IconNode *icon;
   if(hasIcon) {
      icon = np->icon;
   } else if(active && (buttonIcons[BI_MENU_FOCUS] != NULL)) {
      icon = buttonIcons[BI_MENU_FOCUS];
   } else {
      icon = buttonIcons[BI_MENU];
   }
   PutIcon(icon, canvas, fg,
           x + iconXOffset,
           y + iconYOffset,
           iconSize, iconSize);
#endif
}

/** Redraw the borders on the current desktop.
 * This should be done after loading clients since the stacking order
 * may cause borders on the current desktop to become visible after moving
 * clients to their assigned desktops.
 */
void ExposeCurrentDesktop(void)
{
   ClientNode *np;
   int layer;

   for(layer = 0; layer < LAYER_COUNT; layer++) {
      for(np = nodes[layer]; np; np = np->next) {
         if(!(np->state.status & (STAT_HIDDEN | STAT_MINIMIZED))) {
            DrawBorder(np);
         }
      }
   }
}

/** Get the height of a window title bar. */
unsigned GetTitleHeight(void)
{
   if(JUNLIKELY(settings.titleHeight == 0)) {
      settings.titleHeight = GetStringHeight(FONT_BORDER) + 4;
   }
   return settings.titleHeight;
}

/** Get the size of the borders for a client. */
void GetBorderSize(const ClientState *state,
                   int *north, int *south, int *east, int *west)
{
   Assert(state);
   Assert(north);
   Assert(south);
   Assert(east);
   Assert(west);

   /* Full screen is a special case. */
   if(state->status & STAT_FULLSCREEN) {
      *north = 0;
      *south = 0;
      *east = 0;
      *west = 0;
      return;
   }

   if(state->border & BORDER_OUTLINE) {
      const char show_border =
         !state->maxFlags || !(state->border & BORDER_NOMAX);

      if((state->border & BORDER_TITLE) &&
         !(state->maxFlags && (state->border & TITLE_NOMAX))) {
         *north = GetTitleHeight();
      } else if(settings.windowDecorations == DECO_MOTIF) {
         *north = 0;
      } else {
         *north = show_border ? settings.borderWidth : 0;
      }
      if(settings.windowDecorations == DECO_MOTIF) {
         *north += show_border ? settings.borderWidth : 0;
         *south = show_border ? settings.borderWidth : 0;
      } else {
         if(state->status & STAT_SHADED) {
            *south = 0;
         } else {
            *south = show_border ? settings.borderWidth : 0;
         }
      }

      if(show_border) {
         *west = settings.borderWidth;
         *east = settings.borderWidth;
      } else {
         *west = 0;
         *east = 0;
      }

   } else {

      *north = 0;
      *south = 0;
      *east = 0;
      *west = 0;

   }
}

/** Draw a rounded rectangle. */
void DrawRoundedRectangle(Drawable d, GC gc, int x, int y,
                          int width, int height, int radius)
{
#ifdef USE_SHAPE
#ifdef USE_XMU

   if(radius > 0) {
      XmuDrawRoundedRectangle(display, d, gc, x, y, width, height,
                              radius, radius);
   } else {
      JXDrawRectangle(display, d, gc, x, y, width, height);
   }

#else

   if(radius > 0) {
      XSegment segments[4];
      XArc     arcs[4];

      segments[0].x1 = x + radius;         segments[0].y1 = y;
      segments[0].x2 = x + width - radius; segments[0].y2 = y;
      segments[1].x1 = x + radius;         segments[1].y1 = y + height;
      segments[1].x2 = x + width - radius; segments[1].y2 = y + height;
      segments[2].x1 = x;                  segments[2].y1 = y + radius;
      segments[2].x2 = x;                  segments[2].y2 = y + height - radius;
      segments[3].x1 = x + width;          segments[3].y1 = y + radius;
      segments[3].x2 = x + width;          segments[3].y2 = y + height - radius;
      JXDrawSegments(display, d, gc, segments, 4);

      arcs[0].x = x;
      arcs[0].y = y;
      arcs[0].width = radius * 2;
      arcs[0].height = radius * 2;
      arcs[0].angle1 = 90 * 64;
      arcs[0].angle2 = 90 * 64;
      arcs[1].x = x + width - radius * 2;
      arcs[1].y = y;
      arcs[1].width  = radius * 2;
      arcs[1].height = radius * 2;
      arcs[1].angle1 = 0 * 64;
      arcs[1].angle2 = 90 * 64;
      arcs[2].x = x;
      arcs[2].y = y + height - radius * 2;
      arcs[2].width  = radius * 2;
      arcs[2].height = radius * 2;
      arcs[2].angle1 = 180 * 64;
      arcs[2].angle2 = 90 * 64;
      arcs[3].x = x + width - radius * 2;
      arcs[3].y = y + height - radius * 2;
      arcs[3].width  = radius * 2;
      arcs[3].height = radius * 2;
      arcs[3].angle1 = 270 * 64;
      arcs[3].angle2 = 90 * 64;
      JXDrawArcs(display, d, gc, arcs, 4);
   } else {
      JXDrawRectangle(display, d, gc, x, y, width, height);
   }

#endif
#else

   JXDrawRectangle(display, d, gc, x, y, width, height);
   
#endif
}

/** Fill a rounded rectangle. */
#ifdef USE_SHAPE
void FillRoundedRectangle(Drawable d, GC gc, int x, int y,
                          int width, int height, int radius)
{

#ifdef USE_XMU

   XmuFillRoundedRectangle(display, d, gc, x, y, width, height,
                           radius, radius);

#else

   XRectangle  rects[3];
   XArc        arcs[4];

   rects[0].x = x + radius;
   rects[0].y = y;
   rects[0].width = width - radius * 2;
   rects[0].height = radius;
   rects[1].x = x;
   rects[1].y = radius;
   rects[1].width = width;
   rects[1].height = height - radius * 2;
   rects[2].x = x + radius;
   rects[2].y = y + height - radius;
   rects[2].width = width - radius * 2;
   rects[2].height = radius;
   JXFillRectangles(display, d, gc, rects, 3);

   arcs[0].x = x;
   arcs[0].y = y;
   arcs[0].width = radius * 2;
   arcs[0].height = radius * 2;
   arcs[0].angle1 = 90 * 64;
   arcs[0].angle2 = 90 * 64;
   arcs[1].x = x + width - radius * 2 - 1;
   arcs[1].y = y;
   arcs[1].width  = radius * 2;
   arcs[1].height = radius * 2;
   arcs[1].angle1 = 0 * 64;
   arcs[1].angle2 = 90 * 64;
   arcs[2].x = x;
   arcs[2].y = y + height - radius * 2 - 1;
   arcs[2].width  = radius * 2;
   arcs[2].height = radius * 2;
   arcs[2].angle1 = 180 * 64;
   arcs[2].angle2 = 90 * 64;
   arcs[3].x = x + width - radius * 2 - 1;
   arcs[3].y = y + height - radius * 2 -1;
   arcs[3].width  = radius * 2;
   arcs[3].height = radius * 2;
   arcs[3].angle1 = 270 * 64;
   arcs[3].angle2 = 90 * 64;
   JXFillArcs(display, d, gc, arcs, 4);

#endif

}
#endif

/** Set the icon to use for a button. */
void SetBorderIcon(BorderIconType t, const char *name)
{
   if(buttonNames[t]) {
      Release(buttonNames[t]);
   }
   buttonNames[t] = CopyString(name);
}
 
