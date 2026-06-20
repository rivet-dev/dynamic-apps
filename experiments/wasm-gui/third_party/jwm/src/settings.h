/**
 * @file settings.h
 * @author Joe Wingbermuehle
 * @date 2012
 *
 * @brief JWM settings.
 *
 */

#ifndef SETTINGS_H
#define SETTINGS_H

/** Window snap modes. */
typedef unsigned char SnapModeType;
#define SNAP_NONE    0  /**< Don't snap. */
#define SNAP_SCREEN  1  /**< Snap to the edges of the screen. */
#define SNAP_BORDER  2  /**< Snap to all borders. */

/** Window move modes. */
typedef unsigned char MoveModeType;
#define MOVE_OPAQUE     0  /**< Show window contents while moving. */
#define MOVE_OUTLINE    1  /**< Show an outline while moving. */

/** Window resize modes. */
typedef unsigned char ResizeModeType;
#define RESIZE_OPAQUE   0  /**< Show window contents while resizing. */
#define RESIZE_OUTLINE  1  /**< Show an outline while resizing. */

/** Status window types. */
typedef unsigned char StatusWindowType;
#define SW_OFF       0  /**< No status window. */
#define SW_SCREEN    1  /**< Centered on screen. */
#define SW_WINDOW    2  /**< Centered on window. */
#define SW_CORNER    3  /**< Upper-left corner. */

/** Focus models. */
typedef unsigned char FocusModelType;
#define FOCUS_SLOPPY       0 /**< Sloppy focus, click to raise. */
#define FOCUS_CLICK        1 /**< Click to focus, click to raise. */
#define FOCUS_SLOPPY_TITLE 2 /**< Sloppy focus, title to raise. */
#define FOCUS_CLICK_TITLE  3 /**< Click to focus, title to raise. */

/** Decorations. */
typedef unsigned char DecorationsType;
#define DECO_UNSET   0
#define DECO_FLAT    1
#define DECO_MOTIF   2

/** Popup mask. */
typedef unsigned char PopupMaskType;
#define POPUP_NONE   0
#define POPUP_TASK   1
#define POPUP_PAGER  2
#define POPUP_BUTTON 4
#define POPUP_CLOCK  8
#define POPUP_MENU   16
#define POPUP_ALL    255

/** Text alignment. */
typedef unsigned char AlignmentType;
#define ALIGN_LEFT      0
#define ALIGN_CENTER    1
#define ALIGN_RIGHT     2

/** Mouse binding contexts. */
typedef unsigned char MouseContextType;
#define MC_NONE            0     /**< Keyboard/none. */
#define MC_ROOT            1     /**< Root window. */
#define MC_BORDER          2     /**< Resize handle. */
#define MC_MOVE            3     /**< Move handle. */
#define MC_CLOSE           4     /**< Close button. */
#define MC_MAXIMIZE        5     /**< Maximize button. */
#define MC_MINIMIZE        6     /**< Minimize button. */
#define MC_ICON            7     /**< Window menu button. */
#define MC_COUNT           8     /**< Number of contexts. */
#define MC_MASK            0x0F  /**< Context type mask. */
#define MC_BORDER_N        0x10  /**< North border. */
#define MC_BORDER_S        0x20  /**< South border. */
#define MC_BORDER_E        0x40  /**< East border. */
#define MC_BORDER_W        0x80  /**< West border. */

/** Enumeration of desktop back and forth values. */
typedef unsigned char DesktopBackAndForthType;
#define DBACKANDFORTH_OFF 0 /**< No back and forth */
#define DBACKANDFORTH_ON  1 /**< Enable back and forth */

/** Maximum number of title bar components
 * For now, we allow each component to be used twice. */
#define TBC_COUNT       9


/** Settings. */
typedef struct {
   unsigned doubleClickSpeed;
   unsigned doubleClickDelta;
   unsigned snapDistance;
   unsigned popupDelay;
   unsigned trayOpacity;
   unsigned activeClientOpacity;
   unsigned inactiveClientOpacity;
   unsigned borderWidth;
   unsigned titleHeight;
   unsigned desktopWidth;
   unsigned desktopHeight;
   unsigned desktopCount;
   unsigned menuOpacity;
   unsigned desktopDelay;
   unsigned cornerRadius;
   unsigned moveMask;
   unsigned dockSpacing;
   AlignmentType titleTextAlignment;
   SnapModeType snapMode;
   MoveModeType moveMode;
   StatusWindowType moveStatusType;
   StatusWindowType resizeStatusType;
   FocusModelType focusModel;
   ResizeModeType resizeMode;
   DecorationsType windowDecorations;
   DecorationsType trayDecorations;
   DecorationsType taskListDecorations;
   DecorationsType menuDecorations;
   PopupMaskType popupMask;
   MouseContextType titleBarLayout[TBC_COUNT + 1];
   char groupTasks;
   char listAllTasks;
   char showClientName;
   char clientNameDelimiters[2];
   char showKillMenuItem;
   DesktopBackAndForthType desktopBackAndForth;
} Settings;

extern Settings settings;

/*@{*/
void InitializeSettings(void);
void StartupSettings(void);
#define ShutdownSettings()    (void)(0)
#define DestroySettings()     (void)(0)
/*@}*/

/** Update a string setting. */
void SetPathString(char **dest, const char *src);

/** Set the title button order. */
void SetTitleButtonOrder(const char *order);

#endif
