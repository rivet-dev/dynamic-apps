/**
 * @file client.h
 * @author Joe Wingbermuehle
 * @date 2004-2007
 *
 * @brief Client window functions.
 *
 */

#ifndef CLIENT_H
#define CLIENT_H

#include "main.h"
#include "border.h"
#include "hint.h"

struct TimeType;

/** Window border flags.
 * We use an unsigned short for storing these, so we get at least 16
 * on reasonable architectures.
 */
typedef unsigned short BorderFlags;
#define BORDER_NONE        0
#define BORDER_OUTLINE     (1 << 0)    /**< Window has a border. */
#define BORDER_TITLE       (1 << 1)    /**< Window has a title bar. */
#define BORDER_MIN         (1 << 2)    /**< Window supports minimize. */
#define BORDER_MAX         (1 << 3)    /**< Window supports maximize. */
#define BORDER_CLOSE       (1 << 4)    /**< Window supports close. */
#define BORDER_RESIZE      (1 << 5)    /**< Window supports resizing. */
#define BORDER_MOVE        (1 << 6)    /**< Window supports moving. */
#define BORDER_MAX_V       (1 << 7)    /**< Maximize vertically. */
#define BORDER_MAX_H       (1 << 8)    /**< Maximize horizontally. */
#define BORDER_SHADE       (1 << 9)    /**< Allow shading. */
#define BORDER_CONSTRAIN   (1 << 10)   /**< Constrain to the screen. */
#define BORDER_FULLSCREEN  (1 << 11)   /**< Allow fullscreen. */
#define BORDER_NOMAX       (1 << 12)   /**< No border on maximize. */
#define TITLE_NOMAX        (1 << 13)   /**< No title on maximize. */

/** The default border flags. */
#define BORDER_DEFAULT (   \
        BORDER_OUTLINE     \
      | BORDER_TITLE       \
      | BORDER_MIN         \
      | BORDER_MAX         \
      | BORDER_CLOSE       \
      | BORDER_RESIZE      \
      | BORDER_MOVE        \
      | BORDER_MAX_V       \
      | BORDER_MAX_H       \
      | BORDER_SHADE       \
      | BORDER_FULLSCREEN  )

/** Window status flags.
 * We use an unsigned int for storing these, so we get 32 on
 * reasonable architectures.
 */
typedef unsigned int StatusFlags;
#define STAT_NONE       0
#define STAT_ACTIVE     (1 << 0)    /**< Has focus. */
#define STAT_MAPPED     (1 << 1)    /**< Shown (on some desktop). */
#define STAT_HIDDEN     (1 << 2)    /**< Not on the current desktop. */
#define STAT_STICKY     (1 << 3)    /**< This client is on all desktops. */
#define STAT_NOLIST     (1 << 4)    /**< Skip this client in the task list. */
#define STAT_MINIMIZED  (1 << 5)    /**< Minimized. */
#define STAT_SHADED     (1 << 6)    /**< Shaded. */
#define STAT_WMDIALOG   (1 << 7)    /**< This is a JWM dialog window. */
#define STAT_PIGNORE    (1 << 8)    /**< Ignore the program-position. */
#define STAT_SDESKTOP   (1 << 9)    /**< Minimized to show desktop. */
#define STAT_FULLSCREEN (1 << 10)   /**< Full screen. */
#define STAT_OPACITY    (1 << 11)   /**< Fixed opacity. */
#define STAT_NOFOCUS    (1 << 12)   /**< Don't focus on map. */
#define STAT_CANFOCUS   (1 << 13)   /**< Client accepts input focus. */
#define STAT_DELETE     (1 << 14)   /**< Client accepts WM_DELETE. */
#define STAT_TAKEFOCUS  (1 << 15)   /**< Client uses WM_TAKE_FOCUS. */
#define STAT_URGENT     (1 << 16)   /**< Urgency hint is set. */
#define STAT_NOTURGENT  (1 << 17)   /**< Ignore the urgency hint. */
#define STAT_CENTERED   (1 << 18)   /**< Use centered window placement. */
#define STAT_TILED      (1 << 19)   /**< Use tiled window placement. */
#define STAT_IIGNORE    (1 << 20)   /**< Ignore increment when maximized. */
#define STAT_NOPAGER    (1 << 21)   /**< Don't show in pager. */
#define STAT_SHAPED     (1 << 22)   /**< This window is shaped. */
#define STAT_FLASH      (1 << 23)   /**< Flashing for urgency. */
#define STAT_DRAG       (1 << 24)   /**< Pass mouse events to JWM. */
#define STAT_ILIST      (1 << 25)   /**< Ignore program-specified list. */
#define STAT_IPAGER     (1 << 26)   /**< Ignore program-specified pager. */
#define STAT_FIXED      (1 << 27)   /**< Keep on the specified desktop. */
#define STAT_AEROSNAP   (1 << 28)   /**< Enable Aero Snap. */
#define STAT_NODRAG     (1 << 29)   /**< Disable mod1+drag/resize. */
#define STAT_POSITION   (1 << 30)   /**< Config-specified position. */

/** Maximization flags. */
typedef unsigned char MaxFlags;
#define MAX_NONE     0           /**< Don't maximize. */
#define MAX_HORIZ    (1 << 0)    /**< Horizontal maximization. */
#define MAX_VERT     (1 << 1)    /**< Vertical maximization. */
#define MAX_LEFT     (1 << 2)    /**< Maximize on left. */
#define MAX_RIGHT    (1 << 3)    /**< Maximize on right. */
#define MAX_TOP      (1 << 4)    /**< Maximize on top. */
#define MAX_BOTTOM   (1 << 5)    /**< Maximize on bottom. */

/** Colormap window linked list. */
typedef struct ColormapNode {
   Window window;             /**< A window containing a colormap. */
   struct ColormapNode *next; /**< Next value in the linked list. */
} ColormapNode;

/** The aspect ratio of a window. */
typedef struct AspectRatio {
   int minx;   /**< The x component of the minimum aspect ratio. */
   int miny;   /**< The y component of the minimum aspect ratio. */
   int maxx;   /**< The x component of the maximum aspect ratio. */
   int maxy;   /**< The y component of the maximum aspect ratio. */
} AspectRatio;

/** Struture to store information about a client window. */
typedef struct ClientNode {

   Window window;             /**< The client window. */
   Window parent;             /**< The frame window. */

   Window owner;              /**< The owner window (for transients). */

   int x, y;                  /**< The location of the window. */
   int width;                 /**< The width of the window. */
   int height;                /**< The height of the window. */
   int oldx;                  /**< The old x coordinate (for maximize). */
   int oldy;                  /**< The old y coordinate (for maximize). */
   int oldWidth;              /**< The old width (for maximize). */
   int oldHeight;             /**< The old height (for maximize). */

   long sizeFlags;            /**< Size flags from XGetWMNormalHints. */
   int baseWidth;             /**< Base width for resizing. */
   int baseHeight;            /**< Base height for resizing. */
   int minWidth;              /**< Minimum width of this window. */
   int minHeight;             /**< Minimum height of this window. */
   int maxWidth;              /**< Maximum width of this window. */
   int maxHeight;             /**< Maximum height of this window. */
   int xinc;                  /**< Resize x increment. */
   int yinc;                  /**< Resize y increment. */
   AspectRatio aspect;        /**< Aspect ratio. */
   int gravity;               /**< Gravity for reparenting. */

   Colormap cmap;             /**< This window's colormap. */
   ColormapNode *colormaps;   /**< Colormaps assigned to this window. */

   char *name;                /**< Name of this window for display. */
   char *instanceName;        /**< Name of this window for properties. */
   char *className;           /**< Name of the window class. */
   char *clientName;         /**< Name of the client machine. */

   ClientState state;         /**< Window state. */

   MouseContextType mouseContext;

   struct IconNode *icon;     /**< Icon assigned to this window. */

   /** Callback to stop move/resize. */
   void (*controller)(int wasDestroyed);

   struct ClientNode *prev;   /**< The previous client in this layer. */
   struct ClientNode *next;   /**< The next client in this layer. */

} ClientNode;

/** The number of clients (maintained in client.c). */
extern unsigned int clientCount;

/** Find a client by window or parent window.
 * @param w The window.
 * @return The client (NULL if not found).
 */
ClientNode *FindClient(Window w);

/** Find a client by window.
 * @param w The window.
 * @return The client (NULL if not found).
 */
ClientNode *FindClientByWindow(Window w);

/** Find a client by its parent window.
 * @param p The parent window.
 * @return The client (NULL if not found).
 */
ClientNode *FindClientByParent(Window p);

/** Get the active client.
 * @return The active client (NULL if no client is active).
 */
ClientNode *GetActiveClient(void);

/*@{*/
#define InitializeClients()   (void)(0)
void StartupClients(void);
void ShutdownClients(void);
#define DestroyClients()      (void)(0)
/*@}*/

/** Add a window to management.
 * @param w The client window.
 * @param alreadyMapped 1 if the window is mapped, 0 if not.
 * @param notOwner 1 if JWM doesn't own this window, 0 if JWM is the owner.
 * @return The client window data.
 */
ClientNode *AddClientWindow(Window w, char alreadyMapped, char notOwner);

/** Remove a client from management.
 * @param np The client to remove.
 */
void RemoveClient(ClientNode *np);

/** Minimize a client.
 * @param np The client to minimize.
 * @param lower Set to lower the client in the stacking order.
 */
void MinimizeClient(ClientNode *np, char lower);

/** Shade a client.
 * @param np The client to shade.
 */
void ShadeClient(ClientNode *np);

/** Unshade a client.
 * @param np The client to unshade.
 */
void UnshadeClient(ClientNode *np);

/** Set a client's status to withdrawn.
 * A withdrawn client is a client that is not visible in any way to the
 * user. This may be a window that an application keeps around so that
 * it can be reused at a later time.
 * @param np The client whose status to change.
 */
void SetClientWithdrawn(ClientNode *np);

/** Restore a client from minimized state.
 * @param np The client to restore.
 * @param raise 1 to raise the client, 0 to leave stacking unchanged.
 */
void RestoreClient(ClientNode *np, char raise);

/** Maximize a client.
 * @param np The client to maximize (NULL is allowed).
 * @param flags The type of maximization to perform.
 */
void MaximizeClient(ClientNode *np, MaxFlags flags);

/** Maximize a client using the default maximize settings.
 * @param np The client to maximize.
 */
void MaximizeClientDefault(ClientNode *np);

/** Set the full screen status of a client.
 * @param np The client.
 * @param fullScreen 1 to make full screen, 0 to make not full screen.
 */
void SetClientFullScreen(ClientNode *np, char fullScreen);

/** Set the keyboard focus to a client.
 * @param np The client to focus.
 */
void FocusClient(ClientNode *np);

/** Set the keyboard focus back to the active client. */
void RefocusClient(void);

/** Tell a client to exit.
 * @param np The client to delete.
 */
void DeleteClient(ClientNode *np);

/** Force a client to exit.
 * @param np The client to kill.
 */
void KillClient(ClientNode *np);

/** Raise a client to the top of its layer.
 * @param np The client to raise.
 */
void RaiseClient(ClientNode *np);

/** Restack a client.
 * @param np The client to restack.
 * @param above A reference window (or None).
 * @param detail The stack mode (Above, Below, etc).
 */
void RestackClient(ClientNode *np, Window above, int detail);

/** Restack the clients.
 * This is used when a client is mapped so that the stacking order
 * remains consistent.
 */
void RestackClients(void);

/** Set the layer of a client.
 * @param np The client whose layer to set.
 * @param layer the layer to assign to the client.
 */
void SetClientLayer(ClientNode *np, unsigned int layer);

/** Set the desktop for a client.
 * @param np The client.
 * @param desktop The desktop to be assigned to the client.
 */
void SetClientDesktop(ClientNode *np, unsigned int desktop);

/** Set the sticky status of a client.
 * A sticky client will appear on all desktops.
 * @param np The client.
 * @param isSticky 1 to make the client sticky, 0 to make it not sticky.
 */
void SetClientSticky(ClientNode *np, char isSticky);

/** Hide a client.
 * This is used for changing desktops.
 * @param np The client to hide.
 */
void HideClient(ClientNode *np);

/** Show a client.
 * This is used for changing desktops.
 * @param np The client to show.
 */
void ShowClient(ClientNode *np);

/** Update a client's colormap.
 * @param np The client.
 */
void UpdateClientColormap(ClientNode *np);

/** Reparent a client.
 * This will create a window for a frame (or destroy it) depending on
 * whether a client needs a frame.
 * @param np The client.
 */
void ReparentClient(ClientNode *np);

/** Send a configure event to a client.
 * This will send updated location and size information to a client.
 * @param np The client to get the event.
 */
void SendConfigureEvent(ClientNode *np);

/** Send a message to a client.
 * @param w The client window.
 * @param type The type of message to send.
 * @param message The message to send.
 */
void SendClientMessage(Window w, AtomType type, AtomType message);

/** Update callback for clients with the urgency hint set. */
void SignalUrgent(const struct TimeType *now, int x, int y, Window w,
                  void *data);

#endif /* CLIENT_H */

