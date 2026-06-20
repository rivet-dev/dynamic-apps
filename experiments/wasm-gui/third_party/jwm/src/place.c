/**
 * @file place.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Client placement functions.
 *
 */

#include "jwm.h"
#include "place.h"
#include "client.h"
#include "border.h"
#include "screen.h"
#include "tray.h"
#include "settings.h"
#include "clientlist.h"
#include "misc.h"

typedef struct Strut {
   ClientNode *client;
   BoundingBox box;
   struct Strut *next;
} Strut;

static Strut *struts = NULL;

/* desktopCount x screenCount */
/* Note that we assume x and y are 0 based for all screens here. */
static int *cascadeOffsets = NULL;

static char DoRemoveClientStrut(ClientNode *np);
static void InsertStrut(const BoundingBox *box, ClientNode *np);
static void CenterClient(const BoundingBox *box, ClientNode *np);
static int IntComparator(const void *a, const void *b);
static int TryTileClient(const BoundingBox *box, ClientNode *np,
                         int x, int y);
static char TileClient(const BoundingBox *box, ClientNode *np);
static void CascadeClient(const BoundingBox *box, ClientNode *np);

static void SubtractStrutBounds(BoundingBox *box, const ClientNode *np);
static void SubtractBounds(const BoundingBox *src, BoundingBox *dest);
static void SubtractTrayBounds(BoundingBox *box, unsigned int layer);
static void SetWorkarea(void);

/** Startup placement. */
void StartupPlacement(void)
{
   const unsigned titleHeight = GetTitleHeight();
   int count;
   int x;

   count = settings.desktopCount * GetScreenCount();
   cascadeOffsets = Allocate(count * sizeof(int));

   for(x = 0; x < count; x++) {
      cascadeOffsets[x] = settings.borderWidth + titleHeight;
   }

   SetWorkarea();
}

/** Shutdown placement. */
void ShutdownPlacement(void)
{

   Strut *sp;

   Release(cascadeOffsets);

   while(struts) {
      sp = struts->next;
      Release(struts);
      struts = sp;
   }

}

/** Remove struts associated with a client. */
void RemoveClientStrut(ClientNode *np)
{
   if(DoRemoveClientStrut(np)) {
      SetWorkarea();
   }
}

/** Remove struts associated with a client. */
char DoRemoveClientStrut(ClientNode *np)
{
   char updated = 0;
   Strut **spp = &struts;
   while(*spp) {
      Strut *sp = *spp;
      if(sp->client == np) {
         *spp = sp->next;
         Release(sp);
         updated = 1;
      } else {
         spp = &sp->next;
      }
   }
   return updated;
}


/** Insert a bounding box to the list of struts. */
void InsertStrut(const BoundingBox *box, ClientNode *np)
{
   if(JLIKELY(box->width > 0 && box->height > 0)) {
      Strut *sp = Allocate(sizeof(Strut));
      sp->client = np;
      sp->box = *box;
      sp->next = struts;
      struts = sp;
   }
}

/** Add client specified struts to our list. */
void ReadClientStrut(ClientNode *np)
{

   BoundingBox box;
   int status;
   Atom actualType;
   int actualFormat;
   unsigned long count;
   unsigned long bytesLeft;
   unsigned char *value;
   long *lvalue;
   long leftWidth, rightWidth, topHeight, bottomHeight;
   char updated;

   updated = DoRemoveClientStrut(np);

   box.x = 0;
   box.y = 0;
   box.width = 0;
   box.height = 0;

   /* First try to read _NET_WM_STRUT_PARTIAL */
   /* Format is:
    *   left_width, right_width, top_width, bottom_width,
    *   left_start_y, left_end_y, right_start_y, right_end_y,
    *   top_start_x, top_end_x, bottom_start_x, bottom_end_x
    */
   count = 0;
   status = JXGetWindowProperty(display, np->window,
                                atoms[ATOM_NET_WM_STRUT_PARTIAL],
                                0, 12, False, XA_CARDINAL, &actualType,
                                &actualFormat, &count, &bytesLeft, &value);
   if(status == Success && actualFormat != 0) {
      if(JLIKELY(count == 12)) {

         long leftStart, leftStop;
         long rightStart, rightStop;
         long topStart, topStop;
         long bottomStart, bottomStop;

         lvalue = (long*)value;
         leftWidth      = lvalue[0];
         rightWidth     = lvalue[1];
         topHeight      = lvalue[2];
         bottomHeight   = lvalue[3];
         leftStart      = lvalue[4];
         leftStop       = lvalue[5];
         rightStart     = lvalue[6];
         rightStop      = lvalue[7];
         topStart       = lvalue[8];
         topStop        = lvalue[9];
         bottomStart    = lvalue[10];
         bottomStop     = lvalue[11];

         if(leftWidth > 0) {
            box.x = 0;
            box.y = leftStart;
            box.height = leftStop - leftStart;
            box.width = leftWidth;
            InsertStrut(&box, np);
         }

         if(rightWidth > 0) {
            box.x = rootWidth - rightWidth;
            box.y = rightStart;
            box.height = rightStop - rightStart;
            box.width = rightWidth;
            InsertStrut(&box, np);
         }

         if(topHeight > 0) {
            box.x = topStart;
            box.y = 0;
            box.height = topHeight;
            box.width = topStop - topStart;
            InsertStrut(&box, np);
         }

         if(bottomHeight > 0) {
            box.x = bottomStart;
            box.y = rootHeight - bottomHeight;
            box.width = bottomStop - bottomStart;
            box.height = bottomHeight;
            InsertStrut(&box, np);
         }

      }
      JXFree(value);
      SetWorkarea();
      return;
   }

   /* Next try to read _NET_WM_STRUT */
   /* Format is: left_width, right_width, top_width, bottom_width */
   count = 0;
   status = JXGetWindowProperty(display, np->window, atoms[ATOM_NET_WM_STRUT],
                                0, 4, False, XA_CARDINAL, &actualType,
                                &actualFormat, &count, &bytesLeft, &value);
   if(status == Success && actualFormat != 0) {
      if(JLIKELY(count == 4)) {
         lvalue = (long*)value;
         leftWidth = lvalue[0];
         rightWidth = lvalue[1];
         topHeight = lvalue[2];
         bottomHeight = lvalue[3];

         if(leftWidth > 0) {
            box.x = 0;
            box.y = 0;
            box.width = leftWidth;
            box.height = rootHeight;
            InsertStrut(&box, np);
         }

         if(rightWidth > 0) {
            box.x = rootWidth - rightWidth;
            box.y = 0;
            box.width = rightWidth;
            box.height = rootHeight;
            InsertStrut(&box, np);
         }

         if(topHeight > 0) {
            box.x = 0;
            box.y = 0;
            box.width = rootWidth;
            box.height = topHeight;
            InsertStrut(&box, np);
         }

         if(bottomHeight > 0) {
            box.x = 0;
            box.y = rootHeight - bottomHeight;
            box.width = rootWidth;
            box.height = bottomHeight;
            InsertStrut(&box, np);
         }

      }
      JXFree(value);
      SetWorkarea();
      return;
   }

   /* Struts were removed. */
   if(updated) {
      SetWorkarea();
   }

}

/** Get the screen bounds. */
void GetScreenBounds(const ScreenType *sp, BoundingBox *box)
{
   box->x = sp->x;
   box->y = sp->y;
   box->width = sp->width;
   box->height = sp->height;
}

/** Shrink dest such that it does not intersect with src. */
void SubtractBounds(const BoundingBox *src, BoundingBox *dest)
{

   BoundingBox boxes[4];

   if(src->x + src->width <= dest->x) {
      return;
   }
   if(src->y + src->height <= dest->y) {
      return;
   }
   if(dest->x + dest->width <= src->x) {
      return;
   }
   if(dest->y + dest->height <= src->y) {
      return;
   }

   /* There are four ways to do this:
    *  0. Increase the x-coordinate and decrease the width of dest.
    *  1. Increase the y-coordinate and decrease the height of dest.
    *  2. Decrease the width of dest.
    *  3. Decrease the height of dest.
    * We will chose the option which leaves the greatest area.
    * Note that negative areas are possible.
    */

   /* 0 */
   boxes[0] = *dest;
   boxes[0].x = src->x + src->width;
   boxes[0].width = dest->x + dest->width - boxes[0].x;

   /* 1 */
   boxes[1] = *dest;
   boxes[1].y = src->y + src->height;
   boxes[1].height = dest->y + dest->height - boxes[1].y;

   /* 2 */
   boxes[2] = *dest;
   boxes[2].width = src->x - dest->x;

   /* 3 */
   boxes[3] = *dest;
   boxes[3].height = src->y - dest->y;

   /* 0 and 1, winner in 0. */
   if(boxes[0].width * boxes[0].height < boxes[1].width * boxes[1].height) {
      boxes[0] = boxes[1];
   }

   /* 2 and 3, winner in 2. */
   if(boxes[2].width * boxes[2].height < boxes[3].width * boxes[3].height) {
      boxes[2] = boxes[3];
   }

   /* 0 and 2, winner in dest. */
   if(boxes[0].width * boxes[0].height < boxes[2].width * boxes[2].height) {
      *dest = boxes[2];
   } else {
      *dest = boxes[0];
   }

}

/** Subtract tray area from the bounding box. */
void SubtractTrayBounds(BoundingBox *box, unsigned int layer)
{
   const TrayType *tp;
   BoundingBox src;
   BoundingBox last;
   for(tp = GetTrays(); tp; tp = tp->next) {

      if(tp->layer > layer && tp->autoHide == THIDE_OFF) {

         src.x = tp->x;
         src.y = tp->y;
         src.width = tp->width;
         src.height = tp->height;
         if(src.x < 0) {
            src.width += src.x;
            src.x = 0;
         }
         if(src.y < 0) {
            src.height += src.y;
            src.y = 0;
         }

         last = *box;
         SubtractBounds(&src, box);
         if(box->width * box->height <= 0) {
            *box = last;
            break;
         }

      }

   }
}

/** Remove struts from the bounding box. */
void SubtractStrutBounds(BoundingBox *box, const ClientNode *np)
{

   Strut *sp;
   BoundingBox last;

   for(sp = struts; sp; sp = sp->next) {
      if(np != NULL && sp->client == np) {
         continue;
      }
      if(IsClientOnCurrentDesktop(sp->client)) {
         last = *box;
         SubtractBounds(&sp->box, box);
         if(box->width * box->height <= 0) {
            *box = last;
            break;
         }
      }
   }

}

/** Centered placement. */
void CenterClient(const BoundingBox *box, ClientNode *np)
{
   np->x = box->x + (box->width / 2) - (np->width / 2);
   np->y = box->y + (box->height / 2) - (np->height / 2);
   ConstrainSize(np);
   ConstrainPosition(np);
}

/** Compare two integers. */
int IntComparator(const void *a, const void *b)
{
   const int ia = *(const int*)a;
   const int ib = *(const int*)b;
   return ia - ib;
}

/** Attempt to place the client at the specified coordinates. */
int TryTileClient(const BoundingBox *box, ClientNode *np, int x, int y)
{
   const ClientNode *tp;
   int layer;
   int north, south, east, west;
   int x1, x2, y1, y2;
   int ox1, ox2, oy1, oy2;
   int overlap;

   /* Set the client position. */
   GetBorderSize(&np->state, &north, &south, &east, &west);
   np->x = x + west;
   np->y = y + north;
   ConstrainSize(np);
   ConstrainPosition(np);

   /* Get the client boundaries. */
   x1 = np->x - west;
   x2 = np->x + np->width + east;
   y1 = np->y - north;
   y2 = np->y + np->height + south;

   overlap = 0;

   /* Return maximum cost for window outside bounding box. */
   if (  x1 < box->x ||
         x2 > box->x + box->width ||
         y1 < box->y ||
         y2 > box->y + box->height) {
       return INT_MAX;
   }

   /* Loop over each client. */
   for(layer = np->state.layer; layer < LAYER_COUNT; layer++) {
      for(tp = nodes[layer]; tp; tp = tp->next) {

         /* Skip clients that aren't visible. */
         if(!IsClientOnCurrentDesktop(tp)) {
            continue;
         }
         if(!(tp->state.status & STAT_MAPPED)) {
            continue;
         }
         if(tp == np) {
            continue;
         }

         /* Get the boundaries of the other client. */
         GetBorderSize(&tp->state, &north, &south, &east, &west);
         ox1 = tp->x - west;
         ox2 = tp->x + tp->width + east;
         oy1 = tp->y - north;
         oy2 = tp->y + tp->height + south;

         /* Check for an overlap. */
         if(x2 <= ox1 || x1 >= ox2) {
            continue;
         }
         if(y2 <= oy1 || y1 >= oy2) {
            continue;
         }
         overlap += (Min(ox2, x2) - Max(ox1, x1))
                  * (Min(oy2, y2) - Max(oy1, y1));
      }
   }

   return overlap;
}

/** Tiled placement. */
char TileClient(const BoundingBox *box, ClientNode *np)
{

   const ClientNode *tp;
   int layer;
   int north, south, east, west;
   int i, j;
   int count;
   int *xs;
   int *ys;
   int leastOverlap;
   int bestx, besty;

   /* Count insertion points, including bounding box edges. */
   count = 2;
   for(layer = np->state.layer; layer < LAYER_COUNT; layer++) {
      for(tp = nodes[layer]; tp; tp = tp->next) {
         if(!IsClientOnCurrentDesktop(tp)) {
            continue;
         }
         if(!(tp->state.status & STAT_MAPPED)) {
            continue;
         }
         if(tp == np) {
            continue;
         }
         count += 2;
      }
   }

   /* Allocate space for the points. */
   xs = AllocateStack(sizeof(int) * count);
   ys = AllocateStack(sizeof(int) * count);

   /* Insert points. */
   xs[0] = box->x;
   ys[0] = box->y;
   count = 1;
   for(layer = np->state.layer; layer < LAYER_COUNT; layer++) {
      for(tp = nodes[layer]; tp; tp = tp->next) {
         if(!IsClientOnCurrentDesktop(tp)) {
            continue;
         }
         if(!(tp->state.status & STAT_MAPPED)) {
            continue;
         }
         if(tp == np) {
            continue;
         }
         GetBorderSize(&tp->state, &north, &south, &east, &west);
         xs[count + 0] = tp->x - west;
         xs[count + 1] = tp->x + tp->width + east;
         ys[count + 0] = tp->y - north;
         ys[count + 1] = tp->y + tp->height + south;
         count += 2;
      }
   }

   /* Try placing at lower right edge of box, too. */
   GetBorderSize(&np->state, &north, &south, &east, &west);
   xs[count] = box->x + box->width - np->width - east - west;
   ys[count] = box->y + box->height - np->height - north - south;
   count += 1;

   /* Sort the points. */
   qsort(xs, count, sizeof(int), IntComparator);
   qsort(ys, count, sizeof(int), IntComparator);

   /* Try all possible positions. */
   leastOverlap = INT_MAX;
   bestx = xs[0];
   besty = ys[0];
   for(i = 0; i < count; i++) {
      for(j = 0; j < count; j++) {
         const int overlap = TryTileClient(box, np, xs[i], ys[j]);
         if(overlap < leastOverlap) {
            leastOverlap = overlap;
            bestx = xs[i];
            besty = ys[j];
            if(overlap == 0) {
               break;
            }
         }
      }
   }

   ReleaseStack(xs);
   ReleaseStack(ys);

   if(leastOverlap < INT_MAX) {
      /* Set the client position. */
      GetBorderSize(&np->state, &north, &south, &east, &west);
      np->x = bestx + west;
      np->y = besty + north;
      ConstrainSize(np);
      ConstrainPosition(np);
      return 1;
   } else {
      /* Tiled placement failed. */
      return 0;
   }
}

/** Cascade placement. */
void CascadeClient(const BoundingBox *box, ClientNode *np)
{
   const ScreenType *sp;
   const unsigned titleHeight = GetTitleHeight();
   int north, south, east, west;
   int cascadeIndex;
   char overflow;

   GetBorderSize(&np->state, &north, &south, &east, &west);
   sp = GetMouseScreen();
   cascadeIndex = sp->index * settings.desktopCount + currentDesktop;

   /* Set the cascaded location. */
   np->x = box->x + west + cascadeOffsets[cascadeIndex];
   np->y = box->y + north + cascadeOffsets[cascadeIndex];
   cascadeOffsets[cascadeIndex] += settings.borderWidth + titleHeight;

   /* Check for cascade overflow. */
   overflow = 0;
   if(np->x + np->width - box->x > box->width) {
      overflow = 1;
   } else if(np->y + np->height - box->y > box->height) {
      overflow = 1;
   }

   if(overflow) {
      cascadeOffsets[cascadeIndex] = settings.borderWidth + titleHeight;
      np->x = box->x + west + cascadeOffsets[cascadeIndex];
      np->y = box->y + north + cascadeOffsets[cascadeIndex];

      /* Check for client overflow and update cascade position. */
      if(np->x + np->width - box->x > box->width) {
         np->x = box->x + west;
      } else if(np->y + np->height - box->y > box->height) {
         np->y = box->y + north;
      } else {
         cascadeOffsets[cascadeIndex] += settings.borderWidth + titleHeight;
      }
   }

   ConstrainSize(np);
   ConstrainPosition(np);

}

/** Place a client on the screen. */
void PlaceClient(ClientNode *np, char alreadyMapped)
{

   BoundingBox box;
   const ScreenType *sp;

   Assert(np);

   if(alreadyMapped
      || (np->state.status & STAT_POSITION)
      || (!(np->state.status & STAT_PIGNORE)
         && (np->sizeFlags & (PPosition | USPosition)))) {

      GravitateClient(np, 0);
      if(!alreadyMapped) {
         ConstrainSize(np);
         ConstrainPosition(np);
      }

   } else {

      sp = GetMouseScreen();
      GetScreenBounds(sp, &box);
      SubtractTrayBounds(&box, np->state.layer);
      SubtractStrutBounds(&box, np);

      /* If tiled is specified, first attempt to use tiled placement. */
      if(np->state.status & STAT_TILED) {
         if(TileClient(&box, np)) {
            return;
         }
      }

      /* Either tiled placement failed or was not specified. */
      if(np->state.status & STAT_CENTERED) {
         CenterClient(&box, np);
      } else {
         CascadeClient(&box, np);
      }

   }

}

/** Constrain the size of the client. */
char ConstrainSize(ClientNode *np)
{

   BoundingBox box;
   const ScreenType *sp;
   int north, south, east, west;
   const int oldWidth = np->width;
   const int oldHeight = np->height;

   /* First we make sure the window isn't larger than the program allows.
    * We do this here to avoid moving the window below.
    */
   np->width = Min(np->width, np->maxWidth);
   np->height = Min(np->height, np->maxHeight);

   /* Constrain the width if necessary. */
   sp = GetCurrentScreen(np->x, np->y);
   GetScreenBounds(sp, &box);
   SubtractTrayBounds(&box, np->state.layer);
   SubtractStrutBounds(&box, np);
   GetBorderSize(&np->state, &north, &south, &east, &west);
   if(np->width + east + west > sp->width) {
      box.x += west;
      box.width -= east + west;
      if(box.width > np->maxWidth) {
         box.width = np->maxWidth;
      }
      if(box.width > np->width) {
         box.width = np->width;
      }
      np->x = box.x;
      np->width = box.width - (box.width % np->xinc);
   }

   /* Constrain the height if necessary. */
   if(np->height + north + south > sp->height) {
      box.y += north;
      box.height -= north + south;
      if(box.height > np->maxHeight) {
         box.height = np->maxHeight;
      }
      if(box.height > np->height) {
         box.height = np->height;
      }
      np->y = box.y;
      np->height = box.height - (box.height % np->yinc);
   }

   /* If the program has a minimum constraint, we apply that here.
    * Note that this could cause the window to overlap something. */
   np->width = Max(np->width, np->minWidth);
   np->height = Max(np->height, np->minHeight);

   /* Fix the aspect ratio. */
   if(np->sizeFlags & PAspect) {
      if(np->width * np->aspect.miny < np->height * np->aspect.minx) {
         np->height = (np->width * np->aspect.miny) / np->aspect.minx;
      }
      if(np->width * np->aspect.maxy > np->height * np->aspect.maxx) {
         np->width = (np->height * np->aspect.maxx) / np->aspect.maxy;
      }
   }

   if(np->width != oldWidth || np->height != oldHeight) {
      return 1;
   } else {
      return 0;
   }

}

/** Constrain the position of a client. */
void ConstrainPosition(ClientNode *np)
{

   BoundingBox box;
   int north, south, east, west;

   /* Get the bounds for placement. */
   box.x = 0;
   box.y = 0;
   box.width = rootWidth;
   box.height = rootHeight;
   SubtractTrayBounds(&box, np->state.layer);
   SubtractStrutBounds(&box, np);

   /* Fix the position. */
   GetBorderSize(&np->state, &north, &south, &east, &west);
   if(np->x + np->width + east + west > box.x + box.width) {
      np->x = box.x + box.width - np->width - east;
   }
   if(np->y + np->height + north + south > box.y + box.height) {
      np->y = box.y + box.height - np->height - south;
   }
   if(np->x < box.x + west) {
      np->x = box.x + west;
   }
   if(np->y < box.y + north) {
      np->y = box.y + north;
   }

}

/** Place a maximized client on the screen. */
void PlaceMaximizedClient(ClientNode *np, MaxFlags flags)
{
   BoundingBox box;
   const ScreenType *sp;
   int north, south, east, west;

   np->oldx = np->x;
   np->oldy = np->y;
   np->oldWidth = np->width;
   np->oldHeight = np->height;
   np->state.maxFlags = flags;

   GetBorderSize(&np->state, &north, &south, &east, &west);

   sp = GetCurrentScreen(np->x + (east + west + np->width) / 2,
                         np->y + (north + south + np->height) / 2);
   GetScreenBounds(sp, &box);
   if(!(flags & (MAX_HORIZ | MAX_LEFT | MAX_RIGHT))) {
      box.x = np->x;
      box.width = np->width;
   }
   if(!(flags & (MAX_VERT | MAX_TOP | MAX_BOTTOM))) {
      box.y = np->y;
      box.height = np->height;
   }
   SubtractTrayBounds(&box, np->state.layer);
   SubtractStrutBounds(&box, np);

   if(box.width > np->maxWidth) {
      box.width = np->maxWidth;
   }
   if(box.height > np->maxHeight) {
      box.height = np->maxHeight;
   }

   if(np->sizeFlags & PAspect) {
      if(box.width * np->aspect.miny < box.height * np->aspect.minx) {
         box.height = (box.width * np->aspect.miny) / np->aspect.minx;
      }
      if(box.width * np->aspect.maxy > box.height * np->aspect.maxx) {
         box.width = (box.height * np->aspect.maxx) / np->aspect.maxy;
      }
   }

   /* If maximizing horizontally, update width. */
   if(flags & MAX_HORIZ) {
      np->x = box.x + west;
      np->width = box.width - east - west;
      if(!(np->state.status & STAT_IIGNORE)) {
         np->width -= ((np->width - np->baseWidth) % np->xinc);
      }
   } else if(flags & MAX_LEFT) {
      np->x = box.x + west;
      np->width = box.width / 2 - east - west;
      if(!(np->state.status & STAT_IIGNORE)) {
         np->width -= ((np->width - np->baseWidth) % np->xinc);
      }
   } else if(flags & MAX_RIGHT) {
      np->x = box.x + box.width / 2 + west;
      np->width = box.width / 2 - east - west;
      if(!(np->state.status & STAT_IIGNORE)) {
         np->width -= ((np->width - np->baseWidth) % np->xinc);
      }
   }

   /* If maximizing vertically, update height. */
   if(flags & MAX_VERT) {
      np->y = box.y + north;
      np->height = box.height - north - south;
      if(!(np->state.status & STAT_IIGNORE)) {
         np->height -= ((np->height - np->baseHeight) % np->yinc);
      }
   } else if(flags & MAX_TOP) {
      np->y = box.y + north;
      np->height = box.height / 2 - north - south;
      if(!(np->state.status & STAT_IIGNORE)) {
         np->height -= ((np->height - np->baseHeight) % np->yinc);
      }
   } else if(flags & MAX_BOTTOM) {
      np->y = box.y + box.height / 2 + north;
      np->height = box.height / 2 - north - south;
      if(!(np->state.status & STAT_IIGNORE)) {
         np->height -= ((np->height - np->baseHeight) % np->yinc);
      }
   }

}

/** Determine which way to move the client for the border. */
void GetGravityDelta(const ClientNode *np, int gravity, int *x, int  *y)
{
   int north, south, east, west;
   GetBorderSize(&np->state, &north, &south, &east, &west);
   switch(gravity) {
   case NorthWestGravity:
      *y = -north;
      *x = -west;
      break;
   case NorthGravity:
      *y = -north;
      *x = (west - east) / 2;
      break;
   case NorthEastGravity:
      *y = -north;
      *x = west;
      break;
   case WestGravity:
      *x = -west;
      *y = (north - south) / 2;
      break;
   case CenterGravity:
      *y = (north - south) / 2;
      *x = (west - east) / 2;
      break;
   case EastGravity:
      *x = west;
      *y = (north - south) / 2;
      break;
   case SouthWestGravity:
      *y = south;
      *x = -west;
      break;
   case SouthGravity:
      *x = (west - east) / 2;
      *y = south;
      break;
   case SouthEastGravity:
      *y = south;
      *x = west;
      break;
   default: /* Static */
      *x = 0;
      *y = 0;
      break;
   }
}

/** Move the window in the specified direction for reparenting. */
void GravitateClient(ClientNode *np, char negate)
{
   int deltax, deltay;
   GetGravityDelta(np, np->gravity, &deltax, &deltay);
   if(negate) {
      np->x += deltax;
      np->y += deltay;
   } else {
      np->x -= deltax;
      np->y -= deltay;
   }
}

/** Set _NET_WORKAREA. */
void SetWorkarea(void)
{
   BoundingBox box;
   unsigned long *array;
   unsigned int count;
   int x;

   count = 4 * settings.desktopCount * sizeof(unsigned long);
   array = (unsigned long*)AllocateStack(count);

   box.x = 0;
   box.y = 0;
   box.width = rootWidth;
   box.height = rootHeight;

   SubtractTrayBounds(&box, LAYER_NORMAL);
   SubtractStrutBounds(&box, NULL);

   for(x = 0; x < settings.desktopCount; x++) {
      array[x * 4 + 0] = box.x;
      array[x * 4 + 1] = box.y;
      array[x * 4 + 2] = box.width;
      array[x * 4 + 3] = box.height;
   }
   JXChangeProperty(display, rootWindow, atoms[ATOM_NET_WORKAREA],
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char*)array, settings.desktopCount * 4);

   ReleaseStack(array);
}
