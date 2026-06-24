


#include <libwnck/libwnck.h>

/* enumerations from "pager.h" */
static const GEnumValue _wnck_pager_display_mode_values[] = {
  { WNCK_PAGER_DISPLAY_NAME, "WNCK_PAGER_DISPLAY_NAME", "name" },
  { WNCK_PAGER_DISPLAY_CONTENT, "WNCK_PAGER_DISPLAY_CONTENT", "content" },
  { 0, NULL, NULL }
};

GType
wnck_pager_display_mode_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_enum_register_static ("WnckPagerDisplayMode", _wnck_pager_display_mode_values);

  return type;
}


/* enumerations from "screen.h" */
static const GEnumValue __wncklayoutorientation___wnck_layout_orientation_values[] = {
  { WNCK_LAYOUT_ORIENTATION_HORIZONTAL, "WNCK_LAYOUT_ORIENTATION_HORIZONTAL", "horizontal" },
  { WNCK_LAYOUT_ORIENTATION_VERTICAL, "WNCK_LAYOUT_ORIENTATION_VERTICAL", "vertical" },
  { 0, NULL, NULL }
};

GType
_wncklayoutorientation___wnck_layout_orientation_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_enum_register_static ("_WnckLayoutOrientation", __wncklayoutorientation___wnck_layout_orientation_values);

  return type;
}

static const GEnumValue __wncklayoutcorner___wnck_layout_corner_values[] = {
  { WNCK_LAYOUT_CORNER_TOPLEFT, "WNCK_LAYOUT_CORNER_TOPLEFT", "topleft" },
  { WNCK_LAYOUT_CORNER_TOPRIGHT, "WNCK_LAYOUT_CORNER_TOPRIGHT", "topright" },
  { WNCK_LAYOUT_CORNER_BOTTOMRIGHT, "WNCK_LAYOUT_CORNER_BOTTOMRIGHT", "bottomright" },
  { WNCK_LAYOUT_CORNER_BOTTOMLEFT, "WNCK_LAYOUT_CORNER_BOTTOMLEFT", "bottomleft" },
  { 0, NULL, NULL }
};

GType
_wncklayoutcorner___wnck_layout_corner_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_enum_register_static ("_WnckLayoutCorner", __wncklayoutcorner___wnck_layout_corner_values);

  return type;
}


/* enumerations from "tasklist.h" */
static const GEnumValue _wnck_tasklist_grouping_type_values[] = {
  { WNCK_TASKLIST_NEVER_GROUP, "WNCK_TASKLIST_NEVER_GROUP", "never-group" },
  { WNCK_TASKLIST_AUTO_GROUP, "WNCK_TASKLIST_AUTO_GROUP", "auto-group" },
  { WNCK_TASKLIST_ALWAYS_GROUP, "WNCK_TASKLIST_ALWAYS_GROUP", "always-group" },
  { 0, NULL, NULL }
};

GType
wnck_tasklist_grouping_type_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_enum_register_static ("WnckTasklistGroupingType", _wnck_tasklist_grouping_type_values);

  return type;
}


/* enumerations from "util.h" */
static const GEnumValue _wnck_client_type_values[] = {
  { WNCK_CLIENT_TYPE_APPLICATION, "WNCK_CLIENT_TYPE_APPLICATION", "application" },
  { WNCK_CLIENT_TYPE_PAGER, "WNCK_CLIENT_TYPE_PAGER", "pager" },
  { 0, NULL, NULL }
};

GType
wnck_client_type_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_enum_register_static ("WnckClientType", _wnck_client_type_values);

  return type;
}


/* enumerations from "window.h" */
static const GFlagsValue _wnck_window_state_values[] = {
  { WNCK_WINDOW_STATE_MINIMIZED, "WNCK_WINDOW_STATE_MINIMIZED", "minimized" },
  { WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY, "WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY", "maximized-horizontally" },
  { WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY, "WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY", "maximized-vertically" },
  { WNCK_WINDOW_STATE_SHADED, "WNCK_WINDOW_STATE_SHADED", "shaded" },
  { WNCK_WINDOW_STATE_SKIP_PAGER, "WNCK_WINDOW_STATE_SKIP_PAGER", "skip-pager" },
  { WNCK_WINDOW_STATE_SKIP_TASKLIST, "WNCK_WINDOW_STATE_SKIP_TASKLIST", "skip-tasklist" },
  { WNCK_WINDOW_STATE_STICKY, "WNCK_WINDOW_STATE_STICKY", "sticky" },
  { WNCK_WINDOW_STATE_HIDDEN, "WNCK_WINDOW_STATE_HIDDEN", "hidden" },
  { WNCK_WINDOW_STATE_FULLSCREEN, "WNCK_WINDOW_STATE_FULLSCREEN", "fullscreen" },
  { WNCK_WINDOW_STATE_DEMANDS_ATTENTION, "WNCK_WINDOW_STATE_DEMANDS_ATTENTION", "demands-attention" },
  { WNCK_WINDOW_STATE_URGENT, "WNCK_WINDOW_STATE_URGENT", "urgent" },
  { WNCK_WINDOW_STATE_ABOVE, "WNCK_WINDOW_STATE_ABOVE", "above" },
  { WNCK_WINDOW_STATE_BELOW, "WNCK_WINDOW_STATE_BELOW", "below" },
  { 0, NULL, NULL }
};

GType
wnck_window_state_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_flags_register_static ("WnckWindowState", _wnck_window_state_values);

  return type;
}

static const GFlagsValue _wnck_window_actions_values[] = {
  { WNCK_WINDOW_ACTION_MOVE, "WNCK_WINDOW_ACTION_MOVE", "move" },
  { WNCK_WINDOW_ACTION_RESIZE, "WNCK_WINDOW_ACTION_RESIZE", "resize" },
  { WNCK_WINDOW_ACTION_SHADE, "WNCK_WINDOW_ACTION_SHADE", "shade" },
  { WNCK_WINDOW_ACTION_STICK, "WNCK_WINDOW_ACTION_STICK", "stick" },
  { WNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY, "WNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY", "maximize-horizontally" },
  { WNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY, "WNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY", "maximize-vertically" },
  { WNCK_WINDOW_ACTION_CHANGE_WORKSPACE, "WNCK_WINDOW_ACTION_CHANGE_WORKSPACE", "change-workspace" },
  { WNCK_WINDOW_ACTION_CLOSE, "WNCK_WINDOW_ACTION_CLOSE", "close" },
  { WNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY, "WNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY", "unmaximize-horizontally" },
  { WNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY, "WNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY", "unmaximize-vertically" },
  { WNCK_WINDOW_ACTION_UNSHADE, "WNCK_WINDOW_ACTION_UNSHADE", "unshade" },
  { WNCK_WINDOW_ACTION_UNSTICK, "WNCK_WINDOW_ACTION_UNSTICK", "unstick" },
  { WNCK_WINDOW_ACTION_MINIMIZE, "WNCK_WINDOW_ACTION_MINIMIZE", "minimize" },
  { WNCK_WINDOW_ACTION_UNMINIMIZE, "WNCK_WINDOW_ACTION_UNMINIMIZE", "unminimize" },
  { WNCK_WINDOW_ACTION_MAXIMIZE, "WNCK_WINDOW_ACTION_MAXIMIZE", "maximize" },
  { WNCK_WINDOW_ACTION_UNMAXIMIZE, "WNCK_WINDOW_ACTION_UNMAXIMIZE", "unmaximize" },
  { WNCK_WINDOW_ACTION_FULLSCREEN, "WNCK_WINDOW_ACTION_FULLSCREEN", "fullscreen" },
  { WNCK_WINDOW_ACTION_ABOVE, "WNCK_WINDOW_ACTION_ABOVE", "above" },
  { WNCK_WINDOW_ACTION_BELOW, "WNCK_WINDOW_ACTION_BELOW", "below" },
  { 0, NULL, NULL }
};

GType
wnck_window_actions_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_flags_register_static ("WnckWindowActions", _wnck_window_actions_values);

  return type;
}

static const GEnumValue _wnck_window_type_values[] = {
  { WNCK_WINDOW_NORMAL, "WNCK_WINDOW_NORMAL", "normal" },
  { WNCK_WINDOW_DESKTOP, "WNCK_WINDOW_DESKTOP", "desktop" },
  { WNCK_WINDOW_DOCK, "WNCK_WINDOW_DOCK", "dock" },
  { WNCK_WINDOW_DIALOG, "WNCK_WINDOW_DIALOG", "dialog" },
  { WNCK_WINDOW_TOOLBAR, "WNCK_WINDOW_TOOLBAR", "toolbar" },
  { WNCK_WINDOW_MENU, "WNCK_WINDOW_MENU", "menu" },
  { WNCK_WINDOW_UTILITY, "WNCK_WINDOW_UTILITY", "utility" },
  { WNCK_WINDOW_SPLASHSCREEN, "WNCK_WINDOW_SPLASHSCREEN", "splashscreen" },
  { 0, NULL, NULL }
};

GType
wnck_window_type_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_enum_register_static ("WnckWindowType", _wnck_window_type_values);

  return type;
}

static const GEnumValue _wnck_window_gravity_values[] = {
  { WNCK_WINDOW_GRAVITY_CURRENT, "WNCK_WINDOW_GRAVITY_CURRENT", "current" },
  { WNCK_WINDOW_GRAVITY_NORTHWEST, "WNCK_WINDOW_GRAVITY_NORTHWEST", "northwest" },
  { WNCK_WINDOW_GRAVITY_NORTH, "WNCK_WINDOW_GRAVITY_NORTH", "north" },
  { WNCK_WINDOW_GRAVITY_NORTHEAST, "WNCK_WINDOW_GRAVITY_NORTHEAST", "northeast" },
  { WNCK_WINDOW_GRAVITY_WEST, "WNCK_WINDOW_GRAVITY_WEST", "west" },
  { WNCK_WINDOW_GRAVITY_CENTER, "WNCK_WINDOW_GRAVITY_CENTER", "center" },
  { WNCK_WINDOW_GRAVITY_EAST, "WNCK_WINDOW_GRAVITY_EAST", "east" },
  { WNCK_WINDOW_GRAVITY_SOUTHWEST, "WNCK_WINDOW_GRAVITY_SOUTHWEST", "southwest" },
  { WNCK_WINDOW_GRAVITY_SOUTH, "WNCK_WINDOW_GRAVITY_SOUTH", "south" },
  { WNCK_WINDOW_GRAVITY_SOUTHEAST, "WNCK_WINDOW_GRAVITY_SOUTHEAST", "southeast" },
  { WNCK_WINDOW_GRAVITY_STATIC, "WNCK_WINDOW_GRAVITY_STATIC", "static" },
  { 0, NULL, NULL }
};

GType
wnck_window_gravity_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_enum_register_static ("WnckWindowGravity", _wnck_window_gravity_values);

  return type;
}

static const GFlagsValue _wnck_window_move_resize_mask_values[] = {
  { WNCK_WINDOW_CHANGE_X, "WNCK_WINDOW_CHANGE_X", "x" },
  { WNCK_WINDOW_CHANGE_Y, "WNCK_WINDOW_CHANGE_Y", "y" },
  { WNCK_WINDOW_CHANGE_WIDTH, "WNCK_WINDOW_CHANGE_WIDTH", "width" },
  { WNCK_WINDOW_CHANGE_HEIGHT, "WNCK_WINDOW_CHANGE_HEIGHT", "height" },
  { 0, NULL, NULL }
};

GType
wnck_window_move_resize_mask_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_flags_register_static ("WnckWindowMoveResizeMask", _wnck_window_move_resize_mask_values);

  return type;
}


/* enumerations from "workspace.h" */
static const GEnumValue _wnck_motion_direction_values[] = {
  { WNCK_MOTION_UP, "WNCK_MOTION_UP", "up" },
  { WNCK_MOTION_DOWN, "WNCK_MOTION_DOWN", "down" },
  { WNCK_MOTION_LEFT, "WNCK_MOTION_LEFT", "left" },
  { WNCK_MOTION_RIGHT, "WNCK_MOTION_RIGHT", "right" },
  { 0, NULL, NULL }
};

GType
wnck_motion_direction_get_type (void)
{
  static GType type = 0;

  if (!type)
    type = g_enum_register_static ("WnckMotionDirection", _wnck_motion_direction_values);

  return type;
}




