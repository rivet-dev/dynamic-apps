/**
 * @file hint.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Header for reading and writing X properties.
 *
 */

#ifndef HINT_H
#define HINT_H

struct ClientNode;

/** Enumeration of atoms. */
typedef enum {

   /* Misc */
   ATOM_COMPOUND_TEXT,
   ATOM_UTF8_STRING,
   ATOM_XROOTPMAP_ID,
   ATOM_MANAGER,

   /* Standard atoms */
   ATOM_WM_STATE,
   ATOM_WM_PROTOCOLS,
   ATOM_WM_DELETE_WINDOW,
   ATOM_WM_TAKE_FOCUS,
   ATOM_WM_CHANGE_STATE,
   ATOM_WM_COLORMAP_WINDOWS,

   /* WM Spec atoms */
   ATOM_NET_SUPPORTED,
   ATOM_NET_NUMBER_OF_DESKTOPS,
   ATOM_NET_DESKTOP_NAMES,
   ATOM_NET_DESKTOP_GEOMETRY,
   ATOM_NET_DESKTOP_VIEWPORT,
   ATOM_NET_CURRENT_DESKTOP,
   ATOM_NET_ACTIVE_WINDOW,
   ATOM_NET_WORKAREA,
   ATOM_NET_SUPPORTING_WM_CHECK,
   ATOM_NET_SHOWING_DESKTOP,
   ATOM_NET_FRAME_EXTENTS,
   ATOM_NET_WM_DESKTOP,

   ATOM_NET_WM_STATE,
   ATOM_NET_WM_STATE_STICKY,
   ATOM_NET_WM_STATE_MAXIMIZED_VERT,
   ATOM_NET_WM_STATE_MAXIMIZED_HORZ,
   ATOM_NET_WM_STATE_SHADED,
   ATOM_NET_WM_STATE_FULLSCREEN,
   ATOM_NET_WM_STATE_HIDDEN,
   ATOM_NET_WM_STATE_SKIP_TASKBAR,
   ATOM_NET_WM_STATE_SKIP_PAGER,
   ATOM_NET_WM_STATE_BELOW,
   ATOM_NET_WM_STATE_ABOVE,
   ATOM_NET_WM_STATE_DEMANDS_ATTENTION,
   ATOM_NET_WM_STATE_FOCUSED,

   ATOM_NET_WM_ALLOWED_ACTIONS,
   ATOM_NET_WM_ACTION_MOVE,
   ATOM_NET_WM_ACTION_RESIZE,
   ATOM_NET_WM_ACTION_MINIMIZE,
   ATOM_NET_WM_ACTION_SHADE,
   ATOM_NET_WM_ACTION_STICK,
   ATOM_NET_WM_ACTION_FULLSCREEN,
   ATOM_NET_WM_ACTION_MAXIMIZE_HORZ,
   ATOM_NET_WM_ACTION_MAXIMIZE_VERT,
   ATOM_NET_WM_ACTION_CHANGE_DESKTOP,
   ATOM_NET_WM_ACTION_CLOSE,
   ATOM_NET_WM_ACTION_BELOW,
   ATOM_NET_WM_ACTION_ABOVE,

   ATOM_NET_CLOSE_WINDOW,
   ATOM_NET_MOVERESIZE_WINDOW,
   ATOM_NET_RESTACK_WINDOW,
   ATOM_NET_REQUEST_FRAME_EXTENTS,

   ATOM_NET_WM_PID,
   ATOM_NET_WM_NAME,
   ATOM_NET_WM_VISIBLE_NAME,
   ATOM_NET_WM_HANDLED_ICONS,
   ATOM_NET_WM_ICON,
   ATOM_NET_WM_ICON_NAME,
   ATOM_NET_WM_USER_TIME,
   ATOM_NET_WM_USER_TIME_WINDOW,
   ATOM_NET_WM_VISIBLE_ICON_NAME,
   ATOM_NET_WM_WINDOW_TYPE,
   ATOM_NET_WM_WINDOW_TYPE_DESKTOP,
   ATOM_NET_WM_WINDOW_TYPE_DOCK,
   ATOM_NET_WM_WINDOW_TYPE_SPLASH,
   ATOM_NET_WM_WINDOW_TYPE_DIALOG,
   ATOM_NET_WM_WINDOW_TYPE_NORMAL,
   ATOM_NET_WM_WINDOW_TYPE_MENU,
   ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION,
   ATOM_NET_WM_WINDOW_TYPE_TOOLBAR,
   ATOM_NET_WM_WINDOW_TYPE_UTILITY,

   ATOM_NET_CLIENT_LIST,
   ATOM_NET_CLIENT_LIST_STACKING,

   ATOM_NET_WM_STRUT_PARTIAL,
   ATOM_NET_WM_WINDOW_OPACITY,
   ATOM_NET_WM_STRUT,
   ATOM_NET_WM_MOVERESIZE,

   ATOM_NET_SYSTEM_TRAY_OPCODE,
   ATOM_NET_SYSTEM_TRAY_ORIENTATION,

   /* MWM atoms */
   ATOM_MOTIF_WM_HINTS,

   /* JWM-specific atoms. */
   ATOM_JWM_RESTART,
   ATOM_JWM_EXIT,
   ATOM_JWM_RELOAD,
   ATOM_JWM_WM_STATE_MAXIMIZED_TOP,
   ATOM_JWM_WM_STATE_MAXIMIZED_BOTTOM,
   ATOM_JWM_WM_STATE_MAXIMIZED_LEFT,
   ATOM_JWM_WM_STATE_MAXIMIZED_RIGHT,

   ATOM_COUNT
} AtomType;

extern const char jwmRestart[];
extern const char jwmExit[];
extern const char jwmReload[];
extern const char managerProperty[];

#define FIRST_NET_ATOM ATOM_NET_SUPPORTED
#define LAST_NET_ATOM  ATOM_NET_WM_STRUT

#define FIRST_MWM_ATOM ATOM_MOTIF_WM_HINTS
#define LAST_MWM_ATOM  ATOM_MOTIF_WM_HINTS

/** Enumeration of window layers. */
typedef unsigned char WinLayerType;
#define LAYER_DESKTOP   0
#define LAYER_BELOW     1
#define LAYER_NORMAL    2
#define LAYER_ABOVE     3
#define LAYER_COUNT     4

#define FIRST_LAYER        LAYER_DESKTOP
#define LAST_LAYER         LAYER_ABOVE
#define DEFAULT_TRAY_LAYER LAYER_ABOVE

/** Enumeration of window type. */
typedef unsigned char WindowType;
#define WINDOW_TYPE_DESKTOP        0
#define WINDOW_TYPE_DOCK           1
#define WINDOW_TYPE_SPLASH         2
#define WINDOW_TYPE_DIALOG         3
#define WINDOW_TYPE_NORMAL         4
#define WINDOW_TYPE_MENU           5
#define WINDOW_TYPE_NOTIFICATION   6
#define WINDOW_TYPE_TOOLBAR        7
#define WINDOW_TYPE_UTILITY        8
#define WINDOW_TYPE_COUNT          9

/** Client state information. */
typedef struct ClientState {
   unsigned int status;          /**< Status bit mask. */
   unsigned int opacity;         /**< Opacity (0 - 0xFFFFFFFF). */
   unsigned short border;        /**< Border bit mask. */
   unsigned short desktop;       /**< Desktop. */
   unsigned char maxFlags;       /**< Maximization status. */
   unsigned char layer;          /**< Current window layer. */
   unsigned char defaultLayer;   /**< Default window layer. */
   unsigned char windowType;     /**< Window type. */
} ClientState;  /* !! Keep in sync: defaults go in ReadWindowState() */

extern Atom atoms[ATOM_COUNT];

/*@{*/
#define InitializeHints()  (void)(0)
void StartupHints(void);
#define ShutdownHints()    (void)(0)
#define DestroyHints()     (void)(0)
/*@}*/

/** Determine the current desktop. */
void ReadCurrentDesktop(void);

/** Read client info.
 * @param np The client.
 * @param alreadyMapped Set if the client is already mapped.
 */
void ReadClientInfo(struct ClientNode *np, char alreadyMapped);

/** Read a client's name.
 * @param np The client.
 */
void ReadWMName(struct ClientNode *np);

/** Read a client's machine.
 * @param np The client.
 */
void ReadWMMachine(struct ClientNode *np);

/** Read a client's class.
 * @param np The client.
 */
void ReadWMClass(struct ClientNode *np);

/** Read normal hints for a client.
 * @param np The client.
 */
void ReadWMNormalHints(struct ClientNode *np);

/** Read the WM_PROTOCOLS property for a window.
 * @param w The window.
 * @param state The client state to update.
 */
void ReadWMProtocols(Window w, ClientState *state);

/** Read colormap information for a client.
 * @param np The client.
 */
void ReadWMColormaps(struct ClientNode *np);

/** Determine the layer of a client.
 * @param np The client.
 */
void ReadWinLayer(struct ClientNode *np);

/** Read the current state of a window.
 * @param win The window.
 * @param alreadyMapped Set if the window is already mapped.
 * @return The window state.
 */
ClientState ReadWindowState(Window win, char alreadyMapped);

/** Read WM hints.
 * @param win The window.
 * @param state The state hints to update.
 * @param alreadyMapped Set if the window is already mapped.
 */
void ReadWMHints(Window win, ClientState *state, char alreadyMapped);

/** Read opacity.
 * @param win The window.
 * @param opacity The opacity to update.
 */
void ReadWMOpacity(Window win, unsigned *opacity);

/** Set the state of a client window.
 * Note that this will call WriteNetState.
 * @param np The client.
 */
void WriteState(struct ClientNode *np);

/** Set _NET_WM_STATE.
 * @param np The client.
 */
void WriteNetState(struct ClientNode *np);

/** Set the opacity of a client window.
 * @param np The client.
 * @param opacity The opacity to set.
 * @param force Set the opacity even if it hasn't changed.
 */
void SetOpacity(struct ClientNode *np, unsigned int opacity, char force);

/** Set the frame extents of a window.
 * @param win The window.
 * @param state The client state.
 */
void WriteFrameExtents(Window win, const ClientState *state);

/** Read a cardinal atom.
 * @param window The window.
 * @param atom The atom to read.
 * @param value A pointer to the location to save the atom.
 * @return 1 on success, 0 on failure.
 */
char GetCardinalAtom(Window window, AtomType atom, unsigned long *value);

/** Set a cardinal atom.
 * @param window The window.
 * @param atom The atom to set.
 * @param value The value.
 */
void SetCardinalAtom(Window window, AtomType atom, unsigned long value);

/** Read a window atom.
 * @param window The window.
 * @param atom The atom to read.
 * @param value A pointer to the location to save the atom.
 * @return 1 on success, 0 on failure.
 */
char GetWindowAtom(Window window, AtomType atom, Window *value);

/** Set a window atom.
 * @param window The window.
 * @param atom The atom to set.
 * @param value The value.
 */
void SetWindowAtom(Window window, AtomType atom, unsigned long value);

/** Set a pixmap atom.
 * @param window The window.
 * @param atom The atom to set.
 * @param value The value.
 */
void SetPixmapAtom(Window window, AtomType atom, Pixmap value);

/** Set an atom atom.
 * @param window The window.
 * @param atom The atom to set.
 * @param value The value.
 */
void SetAtomAtom(Window window, AtomType atom, AtomType value);

#endif /* HINT_H */

