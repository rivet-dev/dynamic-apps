/**
 * @file resize.h
 * @author Joe Wingbermuehle
 * @date 2005-2006
 *
 * @brief Header for client window resize functions.
 *
 */

#ifndef RESIZE_H
#define RESIZE_H

#include "border.h"

struct ClientNode;

/** Resize a client window.
 * @param np The client to resize.
 * @param context The mouse context.
 * @param startx The starting mouse x-coordinate (window relative).
 * @param starty The starting mouse y-coordinate (window relative).
 */
void ResizeClient(struct ClientNode *np, MouseContextType context,
                  int startx, int starty);

/** Resize a client window using the keyboard (mouse optional).
 * @param np The client to resize.
 * @param context The mouse context.
 */
void ResizeClientKeyboard(struct ClientNode *np, MouseContextType context);

#endif /* RESIZE_H */

