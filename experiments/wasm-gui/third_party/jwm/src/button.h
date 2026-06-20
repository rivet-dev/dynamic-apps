/**
 * @file button.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Functions for rendering buttons.
 *
 */

#ifndef BUTTON_H
#define BUTTON_H

#include "font.h"
#include "settings.h"

/** Number of pixels used for a border. */
#define BUTTON_BORDER 3

struct IconNode;

/** Button types. */
typedef unsigned char ButtonType;
#define BUTTON_LABEL          0  /**< Label. */
#define BUTTON_MENU           1  /**< Menu item. */
#define BUTTON_MENU_ACTIVE    2  /**< Active menu item. */
#define BUTTON_TRAY           3  /**< Inactive tray button. */
#define BUTTON_TRAY_ACTIVE    4  /**< Active tray button. */
#define BUTTON_TASK           5  /**< Item in the task list. */
#define BUTTON_TASK_ACTIVE    6  /**< Active item in the task list. */
#define BUTTON_TASK_MINIMIZED 7  /**< Minimized item in the task list. */

/** Enumeration of button label positions. */
typedef unsigned char LabelPosition;
#define LABEL_POSITION_RIGHT  0  /**< Right of the button icon. */
#define LABEL_POSITION_TOP    1  /**< Above of the button icon. */
#define LABEL_POSITION_BOTTOM 2  /**< Below the button icon.. */

/** Data used for drawing a button. */
typedef struct {

   ButtonType type;           /**< The type of button to draw. */
   AlignmentType alignment;   /**< Alignment of the button content. */
   LabelPosition labelPos;    /**< Position of the button label. */
   FontType font;             /**< The font for button text. */
   char fill;                 /**< Determine if we should fill. */
   char border;               /**< Determine if we should draw a border. */

   Drawable drawable;         /**< The place to put the button. */

   int x, y;                  /**< The coordinates to render the button. */
   int width, height;         /**< The size of the button. */

   struct IconNode *icon;     /**< Icon used in the button. */
   const char *text;          /**< Text used in the button. */

} ButtonNode;

/** Draw a button.
 * @param bp The button to draw.
 */
void DrawButton(ButtonNode *bp);

/** Reset the contents of a ButtonNode structure.
 * @param bp The structure to reset.
 * @param d The drawable to use.
 */
void ResetButton(ButtonNode *bp, Drawable d);

#endif /* BUTTON_H */
