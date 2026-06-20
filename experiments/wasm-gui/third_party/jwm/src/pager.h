/**
 * @file pager.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Pager tray component.
 *
 */

#ifndef PAGER_H
#define PAGER_H

struct TrayComponentType;

/*@{*/
#define InitializePager()  (void)(0)
#define StartupPager()     (void)(0)
void ShutdownPager(void);
void DestroyPager(void);
/*@}*/

/** Create a pager tray component.
 * @param labeled Set to label the pager.
 * @return A new pager tray component.
 */
struct TrayComponentType *CreatePager(char labeled);

/** Update pagers. */
void UpdatePager(void);

#endif /* PAGER_H */

