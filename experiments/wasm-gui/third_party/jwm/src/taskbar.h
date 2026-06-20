/**
 * @file taskbar.h
 * @author Joe Wingbermuehle
 * @date 2005-2006
 *
 * @brief Task list tray component.
 *
 */

#ifndef TASKBAR_H
#define TASKBAR_H

struct ClientNode;
struct TimeType;

/*@{*/
void InitializeTaskBar(void);
#define StartupTaskBar()   (void)(0)
void ShutdownTaskBar(void);
void DestroyTaskBar(void);
/*@}*/

/** Create a new task bar tray component. */
struct TrayComponentType *CreateTaskBar();

/** Add a client to the task bar(s).
 * @param np The client to add.
 */
void AddClientToTaskBar(struct ClientNode *np);

/** Remove a client from the task bar(s).
 * @param np The client to remove.
 */
void RemoveClientFromTaskBar(struct ClientNode *np);

/** Update all task bars. */
void UpdateTaskBar(void);

/** Focus the client in the task bar.
 * @param n The window position in the taskbar.
 */
void FocusAt(char n);

/** Focus the next client in the task bar. */
void FocusNext(void);

/** Focus the previous client in the task bar. */
void FocusPrevious(void);

/** Set the maximum width of task bar items.
 * @param cp The task bar component.
 * @param value The maximum width.
 */
void SetMaxTaskBarItemWidth(struct TrayComponentType *cp, const char *value);

/** Set the preferred height of task bar items.
 * @param cp The task bar component.
 * @param value The height.
 */
void SetTaskBarHeight(struct TrayComponentType *cp, const char *value);

/** Set whether labels should be displayed (or icon only).
 * @param cp The task bar component.
 * @param value 1 if labeled, 0 if no label is to be shown.
 */
void SetTaskBarLabeled(struct TrayComponentType *cp, char value);

/** Set where labels should be positioned in relation to the icon.
 * @param cp The task bar component.
 * @param value 0 for right of the icon, 1 for above the icon, 2 for below the icon.
 */
void SetTaskBarLabelPosition(struct TrayComponentType *cp, const char *value);

/** Update the _NET_CLIENT_LIST property. */
void UpdateNetClientList(void);

#endif /* TASKBAR_H */
