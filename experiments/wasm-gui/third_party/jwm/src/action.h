/**
 * @file action.h
 * @author Joe Wingbermuehle
 *
 * @brief Tray component actions.
 *
 */

#ifndef ACTION_H
#define ACTION_H

struct ActionNode;
struct TrayComponentType;

/** Enumeration of actions.
 * Note that we use the high bits to store additional information
 * for some key types (for example the desktop number).
 */
typedef struct {
   unsigned char action;
   unsigned char extra;
} ActionType;
#define ACTION_NONE           0
#define ACTION_UP             1
#define ACTION_DOWN           2
#define ACTION_RIGHT          3
#define ACTION_LEFT           4
#define ACTION_ESC            5
#define ACTION_ENTER          6
#define ACTION_NEXT           7
#define ACTION_NEXTSTACK      8
#define ACTION_PREV           9
#define ACTION_PREVSTACK      10
#define ACTION_CLOSE          11
#define ACTION_MIN            12
#define ACTION_MAX            13
#define ACTION_SHADE          14
#define ACTION_STICK          15
#define ACTION_MOVE           16
#define ACTION_RESIZE         17
#define ACTION_ROOT           18
#define ACTION_WIN            19
#define ACTION_DESKTOP        20
#define ACTION_RDESKTOP       21
#define ACTION_LDESKTOP       22
#define ACTION_UDESKTOP       23
#define ACTION_DDESKTOP       24
#define ACTION_SHOWDESK       25
#define ACTION_SHOWTRAY       26
#define ACTION_EXEC           27
#define ACTION_RESTART        28
#define ACTION_EXIT           29
#define ACTION_FULLSCREEN     30
#define ACTION_SEND           31
#define ACTION_SENDR          32
#define ACTION_SENDL          33
#define ACTION_SENDU          34
#define ACTION_SENDD          35
#define ACTION_MAXTOP         36
#define ACTION_MAXTOPLEFT     37
#define ACTION_MAXTOPRIGHT    38
#define ACTION_MAXBOTTOM      39
#define ACTION_MAXBOTTOMLEFT  40
#define ACTION_MAXBOTTOMRIGHT 41
#define ACTION_MAXLEFT        42
#define ACTION_MAXRIGHT       43
#define ACTION_MAXV           44
#define ACTION_MAXH           45
#define ACTION_RESTORE        46
#define ACTION_CENTER         47
#define ACTION_AT             48
#define ACTION_KILL           49
#define ACTION_INVALID        255
#define ACTION_RESIZE_N       1  /* Extra value mask for resize north. */
#define ACTION_RESIZE_S       2  /* Extra value mask for resize south. */
#define ACTION_RESIZE_E       4  /* Extra value mask for resize east. */
#define ACTION_RESIZE_W       8  /* Extra value mask for resize west. */

/** Add an action to a list of actions.
 * @param actions The action list to update.
 * @param action The action to add to the list.
 * @param mask The mouse button mask.
 */
void AddAction(struct ActionNode **actions, const char *action, int mask);

/** Destroy a list of actions. */
void DestroyActions(struct ActionNode *actions);

/** Process a button press event.
 * @param actions The action list.
 * @param cp The tray component.
 * @param x The mouse x-coordinate.
 * @param y The mouse y-coordinate.
 * @param button The mouse button.
 */
void ProcessActionPress(struct ActionNode *actions,
                        struct TrayComponentType *cp,
                        int x, int y, int button);

/** Process a button release event.
 * @param actions The action list.
 * @param cp The tray component.
 * @param x The mouse x-coordinate.
 * @param y The mouse y-coordinate.
 * @param button The mouse button.
 */
void ProcessActionRelease(struct ActionNode *actions,
                          struct TrayComponentType *cp,
                          int x, int y, int button);

/** Validate actions.
 * @param actions The action list to validate.
 */
void ValidateActions(const struct ActionNode *actions);

#endif /* ACTION_H */
