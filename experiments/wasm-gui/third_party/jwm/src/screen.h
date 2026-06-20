/**
 * @file screen.h
 * @author Joe Wingbermuehle
 * @date 2005-2006
 *
 * @brief Header for screen functions.
 *
 * Note that screen here refers to physical monitors. Screens are
 * determined using the xinerama extension (if available). There will
 * always be at least one screen.
 *
 */

#ifndef SCREEN_H
#define SCREEN_H

/** Structure to contain information about a screen. */
typedef struct ScreenType {
   int index;           /**< The index of this screen. */
   int x, y;            /**< The location of this screen. */
   int width, height;   /**< The size of this screen. */
} ScreenType;

/*@{*/
#define InitializeScreens()   (void)(0)
void StartupScreens(void);
void ShutdownScreens(void);
#define DestroyScreens()      (void)(0)
/*@}*/

/** Get the screen of the specified coordinates.
 * @param x The x-coordinate.
 * @param y The y-coordinate.
 * @return The screen.
 */
const ScreenType *GetCurrentScreen(int x, int y);

/** Get the screen containing the mouse.
 * @return The screen containing the mouse.
 */
const ScreenType *GetMouseScreen(void);

/** Get the screen of the specified index.
 * @param index The screen index (0 based).
 * @return The screen.
 */
const ScreenType *GetScreen(int index);

/** Get the number of screens.
 * @return The number of screens.
 */
int GetScreenCount(void);

#endif /* SCREEN_H */

