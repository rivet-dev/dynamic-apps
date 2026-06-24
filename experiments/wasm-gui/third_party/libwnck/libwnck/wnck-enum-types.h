


#if !defined (__LIBWNCK_H_INSIDE__) && !defined (WNCK_COMPILATION)
#error "Only <libwnck/libwnck.h> can be included directly."
#endif

#ifndef __WNCK_ENUM_TYPES_H__
#define __WNCK_ENUM_TYPES_H__

G_BEGIN_DECLS


/* enumerations from "pager.h" */
#define WNCK_TYPE_PAGER_DISPLAY_MODE wnck_pager_display_mode_get_type()
GType wnck_pager_display_mode_get_type (void);

/* enumerations from "screen.h" */
#define WNCK_TYPE___WNCK_LAYOUT_ORIENTATION _wncklayoutorientation___wnck_layout_orientation_get_type()
GType _wncklayoutorientation___wnck_layout_orientation_get_type (void);
#define WNCK_TYPE___WNCK_LAYOUT_CORNER _wncklayoutcorner___wnck_layout_corner_get_type()
GType _wncklayoutcorner___wnck_layout_corner_get_type (void);

/* enumerations from "tasklist.h" */
#define WNCK_TYPE_TASKLIST_GROUPING_TYPE wnck_tasklist_grouping_type_get_type()
GType wnck_tasklist_grouping_type_get_type (void);

/* enumerations from "util.h" */
#define WNCK_TYPE_CLIENT_TYPE wnck_client_type_get_type()
GType wnck_client_type_get_type (void);

/* enumerations from "window.h" */
#define WNCK_TYPE_WINDOW_STATE wnck_window_state_get_type()
GType wnck_window_state_get_type (void);
#define WNCK_TYPE_WINDOW_ACTIONS wnck_window_actions_get_type()
GType wnck_window_actions_get_type (void);
#define WNCK_TYPE_WINDOW_TYPE wnck_window_type_get_type()
GType wnck_window_type_get_type (void);
#define WNCK_TYPE_WINDOW_GRAVITY wnck_window_gravity_get_type()
GType wnck_window_gravity_get_type (void);
#define WNCK_TYPE_WINDOW_MOVE_RESIZE_MASK wnck_window_move_resize_mask_get_type()
GType wnck_window_move_resize_mask_get_type (void);

/* enumerations from "workspace.h" */
#define WNCK_TYPE_MOTION_DIRECTION wnck_motion_direction_get_type()
GType wnck_motion_direction_get_type (void);
G_END_DECLS

#endif /* __WNCK_ENUM_TYPES_H__ */



