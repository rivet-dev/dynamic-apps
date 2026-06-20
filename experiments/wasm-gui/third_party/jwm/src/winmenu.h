/**
 * @file winmenu.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Functions for handling window menus.
 *
 */

#ifndef WINMENU_H
#define WINMENU_H

#include "menu.h"

struct ClientNode;

/** Create a window menu. */
Menu *CreateWindowMenu(struct ClientNode *np);

/** Show a window menu.
 * @param np The client for the window menu.
 * @param x The x-coordinate of the menu (root relative).
 * @param y The y-coordinate of the menu (root relative).
 * @param keyboard Set if this request came from a key binding.
 */
void ShowWindowMenu(struct ClientNode *np, int x, int y, char keyboard);

/** Grab the mouse to select a window.
 * @param action The action to perform when a window is selected.
 */
void ChooseWindow(MenuAction *action);

/** Run a menu action for selected client. */
void RunWindowCommand(MenuAction *action, unsigned button);

#endif /* WINMENU_H */
