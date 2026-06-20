/**
 * @file spacer.h
 * @author Joe Wingbermuehle
 * @date 2011
 *
 * @brief Spacer tray component.
 *
 */
#ifndef SPACER_H
#define SPACER_H

struct TrayComponentType;

/** Create a spacer tray component.
 * @param width Minimum width.
 * @param height Minimum height.
 * @return A new spacer tray component.
 */
struct TrayComponentType *CreateSpacer(int width, int height);

#endif /* SPACER_H */

