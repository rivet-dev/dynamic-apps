/**
 * @file clock.h
 * @author Joe Wingbermuehle
 * @date 2005-2006
 *
 * @brief Clock tray component.
 *
 */

#ifndef CLOCK_H
#define CLOCK_H

struct TrayComponentType;

/*@{*/
void InitializeClock(void);
void StartupClock(void);
#define ShutdownClock() (void)(0)
void DestroyClock(void);
/*@}*/

/** Create a clock component for the tray.
 * @param format The format of the clock.
 * @param zone The timezone of the clock (NULL for local time).
 * @param width The width of the clock (0 for auto).
 * @param height The height of the clock (0 for auto).
 */
struct TrayComponentType *CreateClock(const char *format,
                                      const char *zone,
                                      int width, int height);

/** Add an action to a clock.
 * @param cp The clock.
 * @param action The action to take.
 * @param mask The mouse button mask.
 */
void AddClockAction(struct TrayComponentType *cp,
                    const char *action,
                    int mask);

#endif /* CLOCK_H */
