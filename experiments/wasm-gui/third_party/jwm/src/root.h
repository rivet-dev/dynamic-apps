/**
 * @file root.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Root menu functions.
 *
 */

#ifndef ROOT_H
#define ROOT_H

struct Menu;

/*@{*/
void InitializeRootMenu(void);
void StartupRootMenu(void);
#define ShutdownRootMenu() (void)(0)
void DestroyRootMenu(void);
/*@}*/

/** Set the root menu to be used for the specified indexes.
 * @param indexes The indexes (ASCII string of '0' to '9').
 * @param m The menu to use for the specified indexes.
 */
void SetRootMenu(const char *indexes, struct Menu *m);

/** Get the index for a root menu character.
 * @return The menu index, -1 if not found.
 */
int GetRootMenuIndex(char ch);

/** Get the index for a root menu string.
 * @return The menu index, -1 if not found.
 */
int GetRootMenuIndexFromString(const char *str);

/** Determine if a root menu is defined for the specified index.
 * @return 1 if it is defined, 0 if not.
 */
char IsRootMenuDefined(int index);

/** Get the size of a root menu.
 * @param index The root menu index.
 * @param width The width output.
 * @param height The height output.
 */
void GetRootMenuSize(int index, int *width, int *height);

/** Show a root menu.
 * @param index The root menu index.
 * @param x The x-coordinate.
 * @param y The y-coordinate.
 * @param keyboard Set if this request came from a key binding.
 * @return 1 if a menu was displayed, 0 if not.
 */
char ShowRootMenu(int index, int x, int y, char keyboard);

/** Restart the window manager. */
void Restart(void);

/** Exit the window manager.
 * @param confirm 1 to confirm exit, 0 for immediate exit.
 */
void Exit(char confirm);

/** Reload the menu. */
void ReloadMenu(void);

#endif /* ROOT_H */

