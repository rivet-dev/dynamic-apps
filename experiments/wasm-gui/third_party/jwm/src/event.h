/**
 * @file event.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Header for the event functions.
 *
 */

#ifndef EVENT_H
#define EVENT_H

struct TimeType;

typedef void (*SignalCallback)(const struct TimeType *now,
                               int x, int y,
                               Window w,
                               void *data);

/** Last event time. */
extern Time eventTime;

/** Wait for an event and process it.
 * @return 1 if there is an event to process, 0 otherwise.
 */
char WaitForEvent(XEvent *event);

/** Process an event.
 * @param event The event to process.
 */
void ProcessEvent(XEvent *event);

/** Descard excess button events. */
void DiscardButtonEvents(void);

/** Discard excess motion events.
 * @param event The event to return.
 * @param w The window whose events to discard.
 */
void DiscardMotionEvents(XEvent *event, Window w);

/** Discard excess key events.
 * @param event The event to return.
 * @param w The window whose events to discard.
 */
void DiscardKeyEvents(XEvent *event, Window w);

/** Update the last event time.
 * @param event The event containing the time to use.
 */
void UpdateTime(const XEvent *event);

/** Register a callback.
 * @param freq The frequency in milliseconds.
 * @param callback The callback function.
 * @param data Data to pass to the callback.
 */
void RegisterCallback(int freq, SignalCallback callback, void *data);

/** Unregister a callback.
 * @param callback The callback to remove.
 * @param data The data passed to the register function.
 */
void UnregisterCallback(SignalCallback callback, void *data);

/** Restack clients before waiting for an event. */
void RequireRestack();

/** Update the task bar before waiting for an event. */
void RequireTaskUpdate();

/** Update the pager before waiting for an event. */
void RequirePagerUpdate();

#endif /* EVENT_H */

