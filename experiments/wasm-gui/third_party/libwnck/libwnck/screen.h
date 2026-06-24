/* screen object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2006-2007 Vincent Untz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__LIBWNCK_H_INSIDE__) && !defined (WNCK_COMPILATION)
#error "Only <libwnck/libwnck.h> can be included directly."
#endif

#ifndef WNCK_SCREEN_H
#define WNCK_SCREEN_H

#include <glib-object.h>

G_BEGIN_DECLS

/* forward decls */
typedef struct _WnckApplication WnckApplication;
typedef struct _WnckClassGroup  WnckClassGroup;
typedef struct _WnckWindow      WnckWindow;
typedef struct _WnckWorkspace   WnckWorkspace;

/* Screen */

#define WNCK_TYPE_SCREEN              (wnck_screen_get_type ())
#define WNCK_SCREEN(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WNCK_TYPE_SCREEN, WnckScreen))
#define WNCK_SCREEN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WNCK_TYPE_SCREEN, WnckScreenClass))
#define WNCK_IS_SCREEN(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WNCK_TYPE_SCREEN))
#define WNCK_IS_SCREEN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WNCK_TYPE_SCREEN))
#define WNCK_SCREEN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WNCK_TYPE_SCREEN, WnckScreenClass))

typedef struct _WnckScreen        WnckScreen;
typedef struct _WnckScreenClass   WnckScreenClass;
typedef struct _WnckScreenPrivate WnckScreenPrivate;

/**
 * WnckScreen:
 *
 * The #WnckScreen struct contains only private fields and should not be
 * directly accessed.
 */
struct _WnckScreen
{
  GObject parent_instance;

  WnckScreenPrivate *priv;
};

struct _WnckScreenClass
{
  GObjectClass parent_class;

  /* focused window changed */
  void (* active_window_changed)    (WnckScreen *screen,
                                     WnckWindow *previous_window);
  /* current workspace changed */
  void (* active_workspace_changed) (WnckScreen *screen,
                                     WnckWorkspace *previous_workspace);
  /* stacking order changed */
  void (* window_stacking_changed)  (WnckScreen *screen);
  /* window added */
  void (* window_opened)            (WnckScreen *screen,
                                     WnckWindow *window);
  /* window removed */
  void (* window_closed)            (WnckScreen *screen,
                                     WnckWindow *window);
  /* new workspace */
  void (* workspace_created)        (WnckScreen *screen,
                                     WnckWorkspace *space);
  /* workspace gone */
  void (* workspace_destroyed)      (WnckScreen *screen,
                                     WnckWorkspace *space);
  /* new app */
  void (* application_opened)       (WnckScreen      *screen,
                                     WnckApplication *app);
  /* app gone */
  void (* application_closed)       (WnckScreen      *screen,
                                     WnckApplication *app);

  /* New background */
  void (* background_changed)       (WnckScreen      *screen);

  /* new class group */
  void (* class_group_opened)       (WnckScreen     *screen,
                                     WnckClassGroup *class_group);
  /* class group gone */
  void (* class_group_closed)       (WnckScreen     *screen,
                                     WnckClassGroup *class_group);
  /* Toggle showing desktop */
  void (* showing_desktop_changed)  (WnckScreen      *screen);

  /* Viewport stuff changed */
  void (* viewports_changed)        (WnckScreen      *screen);
  
  /* Window manager changed */
  void (* window_manager_changed)   (WnckScreen      *screen);

  /* Padding for future expansion */
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
  void (* pad5) (void);
  void (* pad6) (void);
};

#ifndef WNCK_DISABLE_DEPRECATED
typedef struct _WnckWorkspaceLayout WnckWorkspaceLayout;

/**
 * WnckWorkspaceLayout:
 * @rows: number of rows in the layout grid.
 * @cols: number of columns in the layout grid.
 * @grid: array of size @grid_area containing the index (starting from 0) of
 * the #WnckWorkspace for each position in the layout grid, or -1 if the
 * position does not correspond to any #WnckWorkspace.
 * @grid_area: size of the grid containing all #WnckWorkspace. This can be
 * bigger than the number of #WnckWorkspace because the grid might not be
 * filled.
 * @current_row: row of the specific #WnckWorkspace, starting from 0.
 * @current_col: column of the specific #WnckWorkspace, starting from 0.
 *
 * The #WnckWorkspaceLayout struct contains information about the layout of
 * #WnckWorkspace on a #WnckScreen, and the exact position of a specific
 * #WnckWorkspace.
 *
 * Since: 2.12
 * Deprecated:2.20:
 */
struct _WnckWorkspaceLayout
{
  int rows;
  int cols;
  int *grid;
  int grid_area;
  int current_row;
  int current_col;
};
#endif /* WNCK_DISABLE_DEPRECATED */

/**
 * WnckLayoutOrientation:
 * @WNCK_LAYOUT_ORIENTATION_HORIZONTAL: the #WnckWorkspace are laid out in
 * rows, with the first #WnckWorkspace in the defined #WnckLayoutCorner.
 * @WNCK_LAYOUT_ORIENTATION_VERTICAL: the #WnckWorkspace are laid out in
 * columns, with the first #WnckWorkspace in the defined #WnckLayoutCorner.
 *
 * Type defining the orientation of the layout of #WnckWorkspace. See
 * wnck_pager_set_orientation() for more information.
 */
typedef enum
{
  WNCK_LAYOUT_ORIENTATION_HORIZONTAL,
  WNCK_LAYOUT_ORIENTATION_VERTICAL
} _WnckLayoutOrientation;

/**
 * WnckLayoutCorner:
 * @WNCK_LAYOUT_CORNER_TOPLEFT: the first #WnckWorkspace is in the top left
 * corner of the layout.
 * @WNCK_LAYOUT_CORNER_TOPRIGHT: the first #WnckWorkspace is in the top right
 * corner of the layout.
 * @WNCK_LAYOUT_CORNER_BOTTOMRIGHT: the first #WnckWorkspace is in the bottom
 * right corner of the layout.
 * @WNCK_LAYOUT_CORNER_BOTTOMLEFT: the first #WnckWorkspace is in the bottom
 * left corner of the layout.
 *
 * Type defining the starting corner of the layout of #WnckWorkspace, i.e. the
 * corner containing the first #WnckWorkspace.
 */
typedef enum
{
  WNCK_LAYOUT_CORNER_TOPLEFT,
  WNCK_LAYOUT_CORNER_TOPRIGHT,
  WNCK_LAYOUT_CORNER_BOTTOMRIGHT,
  WNCK_LAYOUT_CORNER_BOTTOMLEFT
} _WnckLayoutCorner;

GType wnck_screen_get_type (void) G_GNUC_CONST;

WnckScreen*    wnck_screen_get_default              (void);
WnckScreen*    wnck_screen_get                      (int         index);
WnckScreen*    wnck_screen_get_for_root             (gulong      root_window_id);
int            wnck_screen_get_number               (WnckScreen *screen);
WnckWorkspace* wnck_screen_get_workspace            (WnckScreen *screen,
                                                     int         workspace);
WnckWorkspace* wnck_screen_get_active_workspace     (WnckScreen *screen);
GList*         wnck_screen_get_workspaces           (WnckScreen *screen);
WnckWindow*    wnck_screen_get_active_window        (WnckScreen *screen);
WnckWindow*    wnck_screen_get_previously_active_window (WnckScreen *screen);
GList*         wnck_screen_get_windows              (WnckScreen *screen);
GList*         wnck_screen_get_windows_stacked      (WnckScreen *screen);
void           wnck_screen_force_update             (WnckScreen *screen);
int            wnck_screen_get_workspace_count      (WnckScreen *screen);
void           wnck_screen_change_workspace_count   (WnckScreen *screen,
                                                     int         count);
const char*    wnck_screen_get_window_manager_name  (WnckScreen *screen);
gboolean       wnck_screen_net_wm_supports          (WnckScreen *screen,
                                                     const char *atom);
gulong         wnck_screen_get_background_pixmap    (WnckScreen *screen);
int            wnck_screen_get_width                (WnckScreen *screen);
int            wnck_screen_get_height               (WnckScreen *screen);
gboolean       wnck_screen_get_showing_desktop      (WnckScreen *screen);
void           wnck_screen_toggle_showing_desktop   (WnckScreen *screen,
                                                     gboolean    show);
void           wnck_screen_move_viewport            (WnckScreen *screen,
                                                     int         x,
                                                     int         y);
void           _wnck_screen_get_workspace_layout     (WnckScreen             *screen,
                                                      _WnckLayoutOrientation *orientation,
                                                      int                    *rows,
                                                      int                    *columns,
                                                      _WnckLayoutCorner      *starting_corner);
int            wnck_screen_try_set_workspace_layout (WnckScreen *screen,
                                                     int         current_token,
                                                     int         rows,
                                                     int         columns);
void           wnck_screen_release_workspace_layout (WnckScreen *screen,
                                                     int         current_token);
#ifndef WNCK_DISABLE_DEPRECATED
G_DEPRECATED
void           wnck_screen_calc_workspace_layout    (WnckScreen          *screen,
                                                     int                  num_workspaces,
                                                     int                  space_index,
                                                     WnckWorkspaceLayout *layout);
G_DEPRECATED
void           wnck_screen_free_workspace_layout (WnckWorkspaceLayout *layout);
#endif /* WNCK_DISABLE_DEPRECATED */


G_END_DECLS

#endif /* WNCK_SCREEN_H */
