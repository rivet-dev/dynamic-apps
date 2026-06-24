/*
 * Copyright (c) 2012 Jérôme Guelfucci <jeromeg@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4kbd-private/xfce-shortcuts-xfwm4.h>

typedef struct _ShortcutTemplate ShortcutTemplate;

struct _ShortcutTemplate
{
  const gchar *name;
  const gchar *feature;
};

const ShortcutTemplate xfwm4_shortcut_values[] = {
  { N_("Window operations menu"), "popup_menu_key" },
  { N_("Up"), "up_key" },
  { N_("Down"), "down_key" },
  { N_("Left"), "left_key" },
  { N_("Right"), "right_key" },
  { N_("Cancel"), "cancel_key" },
  { N_("Cycle windows"), "cycle_windows_key" },
  { N_("Cycle windows (Reverse)"), "cycle_reverse_windows_key" },
  { N_("Switch window for same application"), "switch_window_key" },
  { N_("Switch application"), "switch_application_key" },
  { N_("Close window"), "close_window_key" },
  { N_("Maximize window horizontally"), "maximize_horiz_key" },
  { N_("Maximize window vertically"), "maximize_vert_key" },
  { N_("Maximize window"), "maximize_window_key" },
  { N_("Hide window"), "hide_window_key" },
  { N_("Move window"), "move_window_key" },
  { N_("Resize window"), "resize_window_key" },
  { N_("Shade window"), "shade_window_key" },
  { N_("Stick window"), "stick_window_key" },
  { N_("Raise window"), "raise_window_key" },
  { N_("Lower window"), "lower_window_key" },
  { N_("Raise or lower window"), "raiselower_window_key" },
  { N_("Fill window"), "fill_window_key" },
  { N_("Fill window horizontally"), "fill_horiz_key" },
  { N_("Fill window vertically"), "fill_vert_key" },
  { N_("Toggle above"), "above_key" },
  { N_("Toggle fullscreen"), "fullscreen_key" },
  { N_("Move window to lower monitor"), "move_window_to_monitor_down_key" },
  { N_("Move window to left monitor"), "move_window_to_monitor_left_key" },
  { N_("Move window to right monitor"), "move_window_to_monitor_right_key" },
  { N_("Move window to upper monitor"), "move_window_to_monitor_up_key" },
  { N_("Move window to upper workspace"), "move_window_up_workspace_key" },
  { N_("Move window to bottom workspace"), "move_window_down_workspace_key" },
  { N_("Move window to left workspace"), "move_window_left_workspace_key" },
  { N_("Move window to right workspace"), "move_window_right_workspace_key" },
  { N_("Move window to previous workspace"), "move_window_prev_workspace_key" },
  { N_("Move window to next workspace"), "move_window_next_workspace_key" },
  { N_("Move window to workspace 1"), "move_window_workspace_1_key" },
  { N_("Move window to workspace 2"), "move_window_workspace_2_key" },
  { N_("Move window to workspace 3"), "move_window_workspace_3_key" },
  { N_("Move window to workspace 4"), "move_window_workspace_4_key" },
  { N_("Move window to workspace 5"), "move_window_workspace_5_key" },
  { N_("Move window to workspace 6"), "move_window_workspace_6_key" },
  { N_("Move window to workspace 7"), "move_window_workspace_7_key" },
  { N_("Move window to workspace 8"), "move_window_workspace_8_key" },
  { N_("Move window to workspace 9"), "move_window_workspace_9_key" },
  { N_("Move window to workspace 10"), "move_window_workspace_10_key" },
  { N_("Move window to workspace 11"), "move_window_workspace_11_key" },
  { N_("Move window to workspace 12"), "move_window_workspace_12_key" },
  { N_("Tile window to the top"), "tile_up_key" },
  { N_("Tile window to the bottom"), "tile_down_key" },
  { N_("Tile window to the left"), "tile_left_key" },
  { N_("Tile window to the right"), "tile_right_key" },
  { N_("Tile window to the top-left"), "tile_up_left_key" },
  { N_("Tile window to the top-right"), "tile_up_right_key" },
  { N_("Tile window to the bottom-left"), "tile_down_left_key" },
  { N_("Tile window to the bottom-right"), "tile_down_right_key" },
  { N_("Show desktop"), "show_desktop_key" },
  { N_("Upper workspace"), "up_workspace_key" },
  { N_("Bottom workspace"), "down_workspace_key" },
  { N_("Left workspace"), "left_workspace_key" },
  { N_("Right workspace"), "right_workspace_key" },
  { N_("Previous workspace"), "prev_workspace_key" },
  { N_("Next workspace"), "next_workspace_key" },
  { N_("Workspace 1"), "workspace_1_key" },
  { N_("Workspace 2"), "workspace_2_key" },
  { N_("Workspace 3"), "workspace_3_key" },
  { N_("Workspace 4"), "workspace_4_key" },
  { N_("Workspace 5"), "workspace_5_key" },
  { N_("Workspace 6"), "workspace_6_key" },
  { N_("Workspace 7"), "workspace_7_key" },
  { N_("Workspace 8"), "workspace_8_key" },
  { N_("Workspace 9"), "workspace_9_key" },
  { N_("Workspace 10"), "workspace_10_key" },
  { N_("Workspace 11"), "workspace_11_key" },
  { N_("Workspace 12"), "workspace_12_key" },
  { N_("Add workspace"), "add_workspace_key" },
  { N_("Add adjacent workspace"), "add_adjacent_workspace_key" },
  { N_("Delete last workspace"), "del_workspace_key" },
  { N_("Delete active workspace"), "del_active_workspace_key" },
  { NULL, NULL }
};



const gchar *
xfce_shortcuts_xfwm4_get_feature_name (const gchar *feature)
{
  guint i;

  /* Make sure to use the translations from libxfce4ui */
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  /* We need to get the human readable string of the action name */
  for (i = 0; xfwm4_shortcut_values[i].name != NULL; ++i)
    {
      if (G_UNLIKELY (g_str_equal (xfwm4_shortcut_values[i].feature, feature)))
        {
          return _(xfwm4_shortcut_values[i].name);
        }
    }

  /* Not a valid feature */
  return NULL;
}

GList *
xfce_shortcuts_xfwm4_get_feature_list (void)
{
  GList *list;
  guint  i;

  list = NULL;

  for (i = 0; xfwm4_shortcut_values[i].name != NULL; ++i)
    list = g_list_prepend (list, (gpointer) xfwm4_shortcut_values[i].feature);

  return g_list_reverse (list);
}
