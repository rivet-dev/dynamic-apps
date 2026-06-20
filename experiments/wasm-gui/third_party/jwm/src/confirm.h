/**
 * @file confirm.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Header for the confirm dialog functions.
 *
 */

#ifndef CONFIRM_H
#define CONFIRM_H

struct ClientNode;

/*@{*/
#define InitializeDialogs()   (void)(0)
#define StartupDialogs()      (void)(0)
void ShutdownDialogs(void);
#define DestroyDialogs()      (void)(0)
/*@}*/

/** Handle an event on a dialog window.
 * @param event The event.
 * @return 1 if handled, 0 if not handled.
 */
char ProcessDialogEvent(const XEvent *event);

/** Show a confirm dialog.
 * @param np A client window associated with the dialog.
 * @param action A callback to run if "OK" is clicked.
 */
void ShowConfirmDialog(struct ClientNode *np,
                       void (*action)(struct ClientNode*), ...);

#endif /* CONFIRM_H */

