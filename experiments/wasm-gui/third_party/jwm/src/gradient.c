/**
 * @file gradient.c
 * @author Joe Wingbermuehle
 * @date 2006
 *
 * @brief Gradient fill functions.
 *
 */

#include "jwm.h"
#include "gradient.h"
#include "main.h"

/** Draw a gradient. */
void DrawGradient(Drawable d, GC g,
                  long fromColor, long toColor,
                  int x, int y,
                  unsigned width, unsigned height,
                  GradientDirection gd)
{

   unsigned i;
   unsigned limit;
   XColor colors[2];
   float red, green, blue;
   float ared, agreen, ablue;
   float bred, bgreen, bblue;
   float redStep, greenStep, blueStep;

   /* Return if there's nothing to do or if the background was filled elsewhere. */
   if((width == 0 || height == 0) || (fromColor == toColor)) {
      return;
   }

   /* Query the from/to colors. */
   colors[0].pixel = fromColor;
   colors[1].pixel = toColor;
   JXQueryColors(display, rootColormap, colors, 2);

   /* Set the "from" color. */
   ared = colors[0].red;
   agreen = colors[0].green;
   ablue = colors[0].blue;

   /* Set the "to" color. */
   bred = colors[1].red;
   bgreen = colors[1].green;
   bblue = colors[1].blue;

   /* Determine the step. */
   if(gd == GRADIENT_VERTICAL) {
      redStep = (bred - ared) / height;
      greenStep = (bgreen - agreen) / height;
      blueStep = (bblue - ablue) / height;
      limit = height;
   } else {
      redStep = (bred - ared) / width;
      greenStep = (bgreen - agreen) / width;
      blueStep = (bblue - ablue) / width;
      limit = width;
   }

   /* Loop over each line or column. */
   red = ared;
   blue = ablue;
   green = agreen;
   for(i = 0; i < limit; i++) {

      /* Determine the color for this line. */
      colors[0].red = (unsigned short)red;
      colors[0].green = (unsigned short)green;
      colors[0].blue = (unsigned short)blue;

      GetColor(&colors[0]);

      /* Draw the line. */
      JXSetForeground(display, g, colors[0].pixel);
      if(gd == GRADIENT_VERTICAL) {
         JXDrawLine(display, d, g, x, y + i, x + width - 1, y + i);
      } else {
         JXDrawLine(display, d, g, x + i, y, x + i + 1, y + height - 1);
      }
      
      red += redStep;
      green += greenStep;
      blue += blueStep;
   }
}
