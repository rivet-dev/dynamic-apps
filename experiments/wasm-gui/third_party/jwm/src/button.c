/**
 * @file button.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Functions for rendering buttons.
 *
 */

#include "jwm.h"
#include "button.h"
#include "border.h"
#include "main.h"
#include "icon.h"
#include "image.h"
#include "misc.h"
#include "settings.h"

/** Determine the colors to use. */
static void GetButtonColors(ButtonNode *bp, ColorType *fg, long *bg1, long *bg2,
                            long *up, long *down, DecorationsType *decorations,
                            GradientDirection *gd)
{
   switch(bp->type) {
   case BUTTON_LABEL:
      *fg = COLOR_MENU_FG;
      *bg1 = colors[COLOR_MENU_BG];
      *bg2 = colors[COLOR_MENU_BG];
      *up = colors[COLOR_MENU_UP];
      *down = colors[COLOR_MENU_DOWN];
      *decorations = settings.menuDecorations;
      *gd = gradients[COLOR_MENU_BG];
      break;
   case BUTTON_MENU_ACTIVE:
      *fg = COLOR_MENU_ACTIVE_FG;
      *bg1 = colors[COLOR_MENU_ACTIVE_BG1];
      *bg2 = colors[COLOR_MENU_ACTIVE_BG2];
      *down = colors[COLOR_MENU_ACTIVE_UP];
      *up = colors[COLOR_MENU_ACTIVE_DOWN];
      *decorations = settings.menuDecorations;
      *gd = gradients[COLOR_MENU_ACTIVE_BG1];
      break;
   case BUTTON_TRAY:
      *fg = COLOR_TRAYBUTTON_FG;
      *bg1 = colors[COLOR_TRAYBUTTON_BG1];
      *bg2 = colors[COLOR_TRAYBUTTON_BG2];
      *up = colors[COLOR_TRAYBUTTON_UP];
      *down = colors[COLOR_TRAYBUTTON_DOWN];
      *decorations = settings.trayDecorations;
      *gd = gradients[COLOR_TRAYBUTTON_BG1];
      break;
   case BUTTON_TRAY_ACTIVE:
      *fg = COLOR_TRAYBUTTON_ACTIVE_FG;
      *bg1 = colors[COLOR_TRAYBUTTON_ACTIVE_BG1];
      *bg2 = colors[COLOR_TRAYBUTTON_ACTIVE_BG2];
      *down = colors[COLOR_TRAYBUTTON_ACTIVE_UP];
      *up = colors[COLOR_TRAYBUTTON_ACTIVE_DOWN];
      *decorations = settings.trayDecorations;
      *gd = gradients[COLOR_TRAYBUTTON_ACTIVE_BG1];
      break;
   case BUTTON_TASK:
      *fg = COLOR_TASKLIST_FG;
      *bg1 = colors[COLOR_TASKLIST_BG1];
      *bg2 = colors[COLOR_TASKLIST_BG2];
      *up = colors[COLOR_TASKLIST_UP];
      *down = colors[COLOR_TASKLIST_DOWN];
      *decorations = settings.taskListDecorations;
      *gd = gradients[COLOR_TASKLIST_BG1];
      break;
   case BUTTON_TASK_ACTIVE:
      *fg = COLOR_TASKLIST_ACTIVE_FG;
      *bg1 = colors[COLOR_TASKLIST_ACTIVE_BG1];
      *bg2 = colors[COLOR_TASKLIST_ACTIVE_BG2];
      *down = colors[COLOR_TASKLIST_ACTIVE_UP];
      *up = colors[COLOR_TASKLIST_ACTIVE_DOWN];
      *decorations = settings.taskListDecorations;
      *gd = gradients[COLOR_TASKLIST_ACTIVE_BG1];
      break;
   case BUTTON_TASK_MINIMIZED:
      *fg = COLOR_TASKLIST_MINIMIZED_FG;
      *bg1 = colors[COLOR_TASKLIST_MINIMIZED_BG1];
      *bg2 = colors[COLOR_TASKLIST_MINIMIZED_BG2];
      *down = colors[COLOR_TASKLIST_MINIMIZED_UP];
      *up = colors[COLOR_TASKLIST_MINIMIZED_DOWN];
      *decorations = settings.taskListDecorations;
      *gd = gradients[COLOR_TASKLIST_MINIMIZED_BG1];
      break;
   case BUTTON_MENU:
   default:
      *fg = COLOR_MENU_FG;
      *bg1 = colors[COLOR_MENU_BG];
      *bg2 = colors[COLOR_MENU_BG];
      *up = colors[COLOR_MENU_UP];
      *down = colors[COLOR_MENU_DOWN];
      *decorations = settings.menuDecorations;
      *gd = gradients[COLOR_MENU_BG];
      break;
   }

}

/** Determine the size of the icon (if any) to display. */
static void GetButtonIconSize(ButtonNode *bp, int *width, int *height, int *iconWidth, int *iconHeight)
{
   *iconWidth = 0;
   *iconHeight = 0;

   if(!bp->icon)
      return;

   int maxIconWidth = *width - BUTTON_BORDER * 2;
   int maxIconHeight = *height - BUTTON_BORDER * 2;

   if(bp->text) {
      if(bp->labelPos > LABEL_POSITION_RIGHT) {
         /* Make room for the text. */
         maxIconHeight -= GetStringHeight(bp->font) + BUTTON_BORDER;
      } else {
         /* Showing text, keep the icon square. */
         maxIconWidth = Min(*width, *height) - BUTTON_BORDER * 2;
      }
   }
   if(!bp->icon->width || !bp->icon->height) {
      *iconWidth = Min(maxIconWidth, maxIconHeight);
      *iconHeight = *iconWidth;
   } else {
      const int ratio = (bp->icon->width << 16) / bp->icon->height;
      *iconHeight = maxIconHeight;
      *iconWidth = (*iconHeight * ratio) >> 16;
      if(*iconWidth > maxIconWidth) {
         *iconWidth = maxIconWidth;
         *iconHeight = (*iconWidth << 16) / ratio;
      }
    }

}

/** Determine how much room is left for text. */
static void GetTextSpaceRemaining(ButtonNode *bp, int *width, int *height, int *iconWidth,
                                  int *iconHeight, int *textWidth, int *textHeight)
{
   *textWidth = 0;
   *textHeight = 0;

   if(!bp->text) {
      return;
   }

   *textWidth = GetStringWidth(bp->font, bp->text);
   *textHeight = GetStringHeight(bp->font);

   if(bp->labelPos < LABEL_POSITION_TOP) {
      if(*width > *height || !bp->icon) {
         const int borderWidth = BUTTON_BORDER * (bp->icon ? 3 : 2);
         *textWidth = Min(*textWidth, *width - *iconWidth - borderWidth);
      }
   } else {
      *textHeight = Min(*textHeight, *height - *iconHeight - BUTTON_BORDER * 3);
      *textWidth = Min(*textWidth, *width - BUTTON_BORDER * 2);
   }

   *textWidth = Max(*textWidth, 0);
}

void DrawButton(ButtonNode *bp)
{
   ColorType fg;
   long bg1, bg2;
   long up, down;
   DecorationsType decorations;
   GradientDirection gradient;

   Drawable drawable;
   GC gc;
   int x, y;
   int width, height;
   int xoffset, yoffset;

   int iconWidth, iconHeight;
   int textWidth, textHeight;

   Assert(bp);

   drawable = bp->drawable;
   x = bp->x;
   y = bp->y;
   width = bp->width;
   height = bp->height;
   gc = JXCreateGC(display, drawable, 0, NULL);

   /* Determine the colors to use. */
   GetButtonColors(bp, &fg, &bg1, &bg2, &up, &down, &decorations, &gradient);

   /* Draw the background. */
   if(bp->fill) {

      /* Draw the button background. */
      JXSetForeground(display, gc, bg1);
      if(bg1 == bg2) {
         /* single color */
         JXFillRectangle(display, drawable, gc, x, y, width, height);
      } else {
         /* gradient */
         DrawGradient(drawable, gc, bg1, bg2,
                                x, y, width, height, gradient);
      }

   }

   /* Draw the border. */
   if(bp->border) {
      if(decorations == DECO_MOTIF) {
         JXSetForeground(display, gc, up);
         JXDrawLine(display, drawable, gc, x, y, x + width - 1, y);
         JXDrawLine(display, drawable, gc, x, y, x, y + height - 1);
         JXSetForeground(display, gc, down);
         JXDrawLine(display, drawable, gc, x, y + height - 1,
                    x + width - 1, y + height - 1);
         JXDrawLine(display, drawable, gc, x + width - 1, y,
                    x + width - 1, y + height - 1);
      } else {
         JXSetForeground(display, gc, down);
         JXDrawRectangle(display, drawable, gc, x, y, width - 1, height - 1);
      }
   }

   /* Determine the size of the icon (if any) to display. */
   GetButtonIconSize(bp, &width, &height, &iconWidth, &iconHeight);

   /* Determine how much room is left for text. */
   GetTextSpaceRemaining(bp, &width, &height, &iconWidth, &iconHeight,
                         &textWidth, &textHeight);

   if(bp->labelPos == LABEL_POSITION_RIGHT) {
      if(bp->alignment == ALIGN_CENTER || width <= height) {
         xoffset = Max(0, (width - iconWidth - textWidth + 1) / 2);
      } else {
         xoffset = BUTTON_BORDER;
      }

      /* Display the icon. */
      if(bp->icon) {
         yoffset = (height - iconHeight + 1) / 2;
         PutIcon(bp->icon, drawable, colors[fg],
                 x + xoffset, y + yoffset,
                 iconWidth, iconHeight);
         xoffset += iconWidth + BUTTON_BORDER;
      }

      /* Display the label. */
      if(textWidth > 0) {
         yoffset = (height - textHeight + 1) / 2;
         RenderString(drawable, bp->font, fg,
                      x + xoffset, y + yoffset,
                      textWidth, bp->text);
      }
   } else if (bp->labelPos == LABEL_POSITION_TOP) {
      const int ycenter = (height - textHeight - iconHeight - BUTTON_BORDER + 1) / 2;
      yoffset = (height - iconHeight + 1) / 2;

      /* Display the label before the icon. */
      if(textHeight > 0 && textWidth > 0) {
         xoffset = (width - textWidth + 1) / 2;
         yoffset = width <= height ? Max(BUTTON_BORDER, ycenter) : BUTTON_BORDER;

         RenderString(drawable, bp->font, fg,
                      x + xoffset, y + yoffset,
                      textWidth, bp->text);
         yoffset += textHeight + BUTTON_BORDER;
      }

      /* Display the icon. */
      if(bp->icon) {
         xoffset = (width - iconWidth + 1) / 2;
         PutIcon(bp->icon, drawable, colors[fg],
                 x + xoffset, y + yoffset,
                 iconWidth, iconHeight);
      }
   } else if (bp->labelPos == LABEL_POSITION_BOTTOM) {
      const int ycenter = (height - iconHeight - textHeight - BUTTON_BORDER + 1) / 2;
      yoffset = width <= height ? Max(BUTTON_BORDER, ycenter) : BUTTON_BORDER;

      /* Display the icon. */
      if(bp->icon) {
         xoffset = (width - iconWidth + 1) / 2;
         PutIcon(bp->icon, drawable, colors[fg],
                 x + xoffset, y + yoffset,
                 iconWidth, iconHeight);
         yoffset += iconHeight + BUTTON_BORDER;
      }

      /* Display the label. */
      if(textHeight > 0 && textWidth > 0) {
         xoffset = (width - textWidth + 1) / 2;
         RenderString(drawable, bp->font, fg,
                      x + xoffset, y + yoffset,
                      textWidth, bp->text);
      }
   }

   JXFreeGC(display, gc);

}

/** Reset a button node with default values. */
void ResetButton(ButtonNode *bp, Drawable d)
{

   Assert(bp);

   bp->type = BUTTON_MENU;
   bp->drawable = d;
   bp->font = FONT_TRAY;
   bp->alignment = ALIGN_LEFT;
   bp->labelPos = LABEL_POSITION_RIGHT;
   bp->x = 0;
   bp->y = 0;
   bp->width = 1;
   bp->height = 1;
   bp->icon = NULL;
   bp->text = NULL;
   bp->fill = 1;
   bp->border = 0;

}
