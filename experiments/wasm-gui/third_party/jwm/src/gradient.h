/**
 * @file gradient.h
 * @author Joe Wingbermuehle
 * @date 2006
 *
 * @brief Gradient fill functions.
 *
 */

#ifndef GRADIENT_H
#define GRADIENT_H

#include "color.h"

/** Draw a gradient.
 * Note that no action is taken if fromColor == toColor.
 * @param d The drawable on which to draw the gradient.
 * @param g The graphics context to use.
 * @param fromColor The starting color pixel value.
 * @param toColor The ending color pixel value.
 * @param x The x-coordinate.
 * @param y The y-coordinate.
 * @param width The width of the area to fill.
 * @param height The height of the area to fill.
 * @param gd The direction of the gradient.
 */
void DrawGradient(Drawable d, GC g,
                  long fromColor, long toColor,
                  int x, int y,
                  unsigned width, unsigned height,
                  GradientDirection gd);


#endif /* GRADIENT_H */

