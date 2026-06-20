/**
 * @file tray.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Tray functions.
 *
 */

#ifndef TRAY_H
#define TRAY_H

#include "hint.h"
#include "timing.h"

/** Enumeration of tray layouts. */
typedef unsigned char LayoutType;
#define LAYOUT_HORIZONTAL  0  /**< Left-to-right. */
#define LAYOUT_VERTICAL    1  /**< Top-to-bottom. */

/** Enumeration of tray alignments. */
typedef unsigned char TrayAlignmentType;
#define TALIGN_FIXED    0 /**< Fixed at user specified x and y coordinates. */
#define TALIGN_LEFT     1 /**< Left aligned. */
#define TALIGN_TOP      2 /**< Top aligned. */
#define TALIGN_CENTER   3 /**< Center aligned. */
#define TALIGN_RIGHT    4 /**< Right aligned. */
#define TALIGN_BOTTOM   5 /**< Bottom aligned. */

/** Enumeration of tray autohide values. */
typedef unsigned char TrayAutoHideType;
#define THIDE_OFF       0 /**< No autohide. */
#define THIDE_LEFT      1 /**< Hide on the left. */
#define THIDE_RIGHT     2 /**< Hide on the right. */
#define THIDE_TOP       3 /**< Hide on the top. */
#define THIDE_BOTTOM    4 /**< Hide on the bottom. */
#define THIDE_ON        5 /**< Auto-select hide location. */
#define THIDE_INVISIBLE 6 /**< Make the tray invisible when hidden. */
#define THIDE_RAISED    8 /**< Mask to indicate the tray is raised. */

/** Structure to hold common tray component data.
 * Sizing is handled as follows:
 *  - The component is created via a factory method. It sets its
 *    requested size (0 for no preference).
 *  - The SetSize callback is issued with size constraints
 *    (0 for no constraint). The component should update
 *    width and height in SetSize.
 *  - The Create callback is issued with finalized size information.
 * Resizing is handled as follows:
 *  - A component determines that it needs to change size. It updates
 *    its requested size (0 for no preference).
 *  - The component calls ResizeTray.
 *  - The SetSize callback is issued with size constraints
 *    (0 for no constraint). The component should update
 *    width and height in SetSize.
 *  - The Resize callback is issued with finalized size information.
 */
typedef struct TrayComponentType {

   /** The tray containing the component.
    * UpdateSpecificTray(TrayType*, TrayComponentType*) should be called
    * when content changes.
    */
   struct TrayType *tray;

   /** Additional information needed for the component. */
   void *object;

   int x;   /**< x-coordinate on the tray (valid only after Create). */
   int y;   /**< y-coordinate on the tray (valid only after Create). */

   int screenx;   /**< x-coordinate on the screen (valid only after Create). */
   int screeny;   /**< y-coordinate on the screen (valid only after Create). */


   int requestedWidth;  /**< Requested width. */
   int requestedHeight; /**< Requested height. */

   int width;     /**< Actual width. */
   int height;    /**< Actual height. */

   char grabbed;     /**< 1 if the mouse was grabbed by this component. */

   Window window;    /**< Content (if a window, otherwise None). */
   Pixmap pixmap;    /**< Content (if a pixmap, otherwise None). */

   /** Callback to create the component. */
   void (*Create)(struct TrayComponentType *cp);

   /** Callback to destroy the component. */
   void (*Destroy)(struct TrayComponentType *cp);

   /** Callback to set the size known so far.
    * This is needed for items that maintain width/height ratios.
    * Either width or height may be zero.
    * This is called before Create.
    */
   void (*SetSize)(struct TrayComponentType *cp, int width, int height);

   /** Callback to resize the component. */
   void (*Resize)(struct TrayComponentType *cp);

   /** Callback for mouse presses. */
   void (*ProcessButtonPress)(struct TrayComponentType *cp,
                              int x, int y, int mask);

   /** Callback for mouse releases. */
   void (*ProcessButtonRelease)(struct TrayComponentType *cp,
                                int x, int y, int mask);

   /** Callback for mouse motion. */
   void (*ProcessMotionEvent)(struct TrayComponentType *cp,
                              int x, int y, int mask);

   /** Callback to redraw the component contents.
    * This is only needed for components that use actions.
    */
   void (*Redraw)(struct TrayComponentType *cp);

   /** The next component in the tray. */
   struct TrayComponentType *next;

} TrayComponentType;

/** Structure to represent a tray. */
typedef struct TrayType {

   int requestedX;      /**< The user-requested x-coordinate of the tray. */
   int requestedY;      /**< The user-requested y-coordinate of the tray. */

   int screen;       /**< The screen index (-1 if not known). */
   int x;            /**< The x-coordinate of the tray. */
   int y;            /**< The y-coordinate of the tray. */

   int requestedWidth;  /**< Total requested width of the tray. */
   int requestedHeight; /**< Total requested height of the tray. */

   int width;     /**< Actual width of the tray. */
   int height;    /**< Actual height of the tray. */

   WinLayerType layer;        /**< Layer. */
   LayoutType layout;         /**< Layout. */
   TrayAlignmentType valign;  /**< Vertical alignment. */
   TrayAlignmentType halign;  /**< Horizontal alignment. */

   TimeType showTime;
   TrayAutoHideType  autoHide;
   unsigned autoHideDelay;
   char hidden;     /**< 1 if hidden (due to autohide), 0 otherwise. */

   Window window; /**< The tray window. */

   /** Start of the tray components. */
   struct TrayComponentType *components;

   /** End of the tray components. */
   struct TrayComponentType *componentsTail;

   struct TrayType *next;  /**< Next tray. */

} TrayType;

void InitializeTray(void);
void StartupTray(void);
void ShutdownTray(void);
void DestroyTray(void);

/** Create a new tray.
 * @return A new, empty tray.
 */
TrayType *CreateTray(void);

/** Create a tray component.
 * @return A new tray component structure.
 */
TrayComponentType *CreateTrayComponent(void);

/** Add a tray component to a tray.
 * @param tp The tray to update.
 * @param cp The tray component to add.
 */
void AddTrayComponent(TrayType *tp, TrayComponentType *cp);

/** Show a tray.
 * @param tp The tray to show.
 */
void ShowTray(TrayType *tp);

/** Show all trays. */
void ShowAllTrays(void);

/** Hide a tray.
 * @param tp The tray to hide.
 */
void HideTray(TrayType *tp);

/** Draw all trays. */
void DrawTray(void);

/** Draw a specific tray.
 * @param tp The tray to draw.
 */
void DrawSpecificTray(const TrayType *tp);

/** Raise tray windows. */
void RaiseTrays(void);

/** Lower tray windows. */
void LowerTrays(void);

/** Update a component on a tray.
 * @param tp The tray containing the component.
 * @param cp The component that needs updating.
 */
void UpdateSpecificTray(const TrayType *tp, const TrayComponentType *cp);

/** Resize a tray.
 * @param tp The tray to resize containing the new requested size information.
 */
void ResizeTray(TrayType *tp);

/** Draw the tray background on a drawable. */
void ClearTrayDrawable(const TrayComponentType *cp);

/** Get a linked list of trays.
 * @return The trays.
 */
TrayType *GetTrays(void);

/** Get the number of trays.
 * @return The number of trays.
 */
unsigned int GetTrayCount(void);

/** Process an event that may be for a tray.
 * @param event The event to process.
 * @return 1 if this event was for a tray, 0 otherwise.
 */
char ProcessTrayEvent(const XEvent *event);

/** Set whether auto-hide is enabled for a tray.
 * @param tp The tray.
 * @param autohide The auto-hide setting.
 * @param delay_ms The auto-hide timeout in milliseconds.
 */
void SetAutoHideTray(TrayType *tp,
                     TrayAutoHideType autohide,
                     unsigned delay_ms);

/** Set the tray x-coordinate.
 * @param tp The tray.
 * @param str The x-coordinate (ASCII, pixels, negative ok).
 */
void SetTrayX(TrayType *tp, const char *str);

/** Set the tray y-coordinate.
 * @param tp The tray.
 * @param str The y-coordinate (ASCII, pixels, negative ok).
 */
void SetTrayY(TrayType *tp, const char *str);

/** Set the tray width.
 * @param tp The tray.
 * @param str The width (ASCII, pixels).
 */
void SetTrayWidth(TrayType *tp, const char *str);

/** Set the tray height.
 * @param tp The tray.
 * @param str The height (ASCII, pixels).
 */
void SetTrayHeight(TrayType *tp, const char *str);

/** Set the tray screen.
 * @param tp The tray.
 * @param str The screen index (ASCII).
 */
void SetTrayScreen(TrayType *tp, const char *str);

/** Set the tray layout.
 * @param tp The tray.
 * @param str A string representation of the layout to use.
 */
void SetTrayLayout(TrayType *tp, const char *str);

/** Set the tray layer.
 * @param tp The tray.
 * @param layer The layer.
 */
void SetTrayLayer(TrayType *tp, WinLayerType layer);

/** Set the tray horizontal alignment.
 * @param tp The tray.
 * @param str The alignment (ASCII).
 */
void SetTrayHorizontalAlignment(TrayType *tp, const char *str);

/** Set the tray vertical alignment.
 * @param tp The tray.
 * @param str The alignment (ASCII).
 */
void SetTrayVerticalAlignment(TrayType *tp, const char *str);

#endif /* TRAY_H */

