/**
 * @file icon.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Header file for icon functions.
 *
 */

#ifndef ICON_H
#define ICON_H

struct ClientNode;

/** Structure to hold a scaled icon. */
typedef struct ScaledIconNode {

   int width;   /**< The scaled width of the icon. */
   int height;  /**< The scaled height of the icon. */
   long fg;     /**< Foreground color for bitmaps. */

   XID image;
   XID mask;

   struct ScaledIconNode *next;

} ScaledIconNode;

/** Structure to hold an icon. */
typedef struct IconNode {

   char *name;                    /**< The name of the icon. */
   struct ImageNode *images;      /**< Images associated with this icon. */
   struct ScaledIconNode *nodes;  /**< Scaled icons. */
   int width;                     /**< Natural width. */
   int height;                    /**< Natural height. */

   struct IconNode *next;         /**< The next icon in the list. */
   struct IconNode *prev;         /**< The previous icon in the list. */

   char preserveAspect;           /**< Set to preserve the aspect ratio
                                   *   of the icon when scaling. */
   char bitmap;                   /**< Set if this is a bitmap. */
   char transient;                /**< Set if this icon is transient. */
#ifdef USE_XRENDER
   char render;                   /**< Set to use render. */
#endif

} IconNode;

extern IconNode emptyIcon;

#ifdef USE_ICONS

/*@{*/
void InitializeIcons(void);
void StartupIcons(void);
void ShutdownIcons(void);
void DestroyIcons(void);
/*@}*/

/** Add an icon path.
 * This adds a path to the list of icon search paths.
 * @param path The icon path to add.
 */
void AddIconPath(char *path);

/** Render an icon.
 * This will scale an icon if necessary to fit the requested size. The
 * aspect ratio of the icon is preserved.
 * @param icon The icon to render.
 * @param d The drawable on which to place the icon.
 * @param fg The foreground color.
 * @param x The x offset on the drawable to render the icon.
 * @param y The y offset on the drawable to render the icon.
 * @param width The width of the icon to display.
 * @param height The height of the icon to display.
 */
void PutIcon(IconNode *icon, Drawable d,
             long fg, int x, int y, int width, int height);

/** Load an icon for a client.
 * @param np The client.
 */
void LoadIcon(struct ClientNode *np);

/** Load an icon.
 * @param name The name of the icon to load.
 * @param save Set if this icon should be saved in the icon hash.
 * @param preserveAspect Set to preserve the aspect ratio when scaling.
 * @return A pointer to the icon (NULL if not found).
 */
IconNode *LoadNamedIcon(const char *name, char save, char preserveAspect);

/** Load the default icon.
 * @return The default icon.
 */
IconNode *GetDefaultIcon(void);

/** Destroy an icon.
 * @param icon The icon to destroy.
 */
void DestroyIcon(IconNode *icon);

/** Set the default icon. */
void SetDefaultIcon(const char *name);

#else

#define ICON_DUMMY_FUNCTION ((void)0)

#define InitializeIcons()                  ICON_DUMMY_FUNCTION
#define StartupIcons()                     ICON_DUMMY_FUNCTION
#define ShutdownIcons()                    ICON_DUMMY_FUNCTION
#define DestroyIcons()                     ICON_DUMMY_FUNCTION
#define AddIconPath( a )                   ICON_DUMMY_FUNCTION
#define PutIcon( a, b, c, d, e, f, g )     ICON_DUMMY_FUNCTION
#define LoadIcon( a )                      ICON_DUMMY_FUNCTION
#define GetDefaultIcon()                   NULL
#define LoadNamedIcon( a, b, c )           NULL
#define DestroyIcon( a )                   ICON_DUMMY_FUNCTION
#define SetDefaultIcon( a )                ICON_DUMMY_FUNCTION

#endif /* USE_ICONS */

#endif /* ICON_H */

