/**
 * @file swallow.h
 * @author Joe Wingbermuehle
 * @date 2005-2006
 *
 * @brief Swallow tray component.
 *
 */

#ifndef SWALLOW_H
#define SWALLOW_H

/*@{*/
#define InitializeSwallow()   (void)(0)
void StartupSwallow(void);
#define ShutdownSwallow()     (void)(0)
void DestroySwallow(void);
/*@}*/

/** Create a swallowed application tray component.
 * @param name The name of the application to swallow.
 * @param command The command used to start the swallowed application.
 * @param width The width to use (0 for default).
 * @param height the height to use (0 for default).
 */
struct TrayComponentType *CreateSwallow(const char *name,
                                        const char *command,
                                        int width, int height);

/** Determine if a window should be swallowed.
 * @param win The window.
 * @return 1 if this window should be swallowed, 0 if not.
 */
char CheckSwallowMap(Window win);

/** Process an event on a swallowed window.
 * @param event The event to process.
 * @return 1 if the event was for a swallowed window, 0 if not.
 */
char ProcessSwallowEvent(const XEvent *event);

/** Determine if there are swallow processes pending.
 * @return 1 if there are still pending swallow processes, 0 otherwise.
 */
char IsSwallowPending(void);

#endif /* SWALLOW_H */

