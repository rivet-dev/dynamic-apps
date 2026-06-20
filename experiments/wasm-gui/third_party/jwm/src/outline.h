/**
 * @file outline.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Outlines for moving and resizing client windows.
 *
 */

#ifndef OUTLINE_H
#define OUTLINE_H

/** Draw an outline.
 * @param x The x-coordinate.
 * @param y The y-coordinate.
 * @param width The width of the outline.
 * @param height The height of the outline.
 */
void DrawOutline(int x, int y, int width, int height);

/** Clear an outline. */
void ClearOutline(void);

#endif /* OUTLINE_H */

