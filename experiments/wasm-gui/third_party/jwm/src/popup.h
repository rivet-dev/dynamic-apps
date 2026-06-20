/**
 * @file popup.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Header for popup functions.
 *
 */

#ifndef POPUP_H
#define POPUP_H

#include "settings.h"

/*@{*/
#define InitializePopup()  (void)(0)
void StartupPopup(void);
void ShutdownPopup(void);
#define DestroyPopup()     (void)(0)
/*@}*/

/** Show a popup window.
 * @param x The x coordinate of the left edge of the popup window.
 * @param y The y coordinate of the bottom edge of the popup window.
 * @param text The text to display in the popup.
 * @param context The popup context.
 */
void ShowPopup(int x, int y, const char *text,
               const PopupMaskType context);

/** Process a popup event.
 * @param event The event to process.
 * @return 1 if handled, 0 otherwise.
 */
char ProcessPopupEvent(const XEvent *event);

#endif /* POPUP_H */

