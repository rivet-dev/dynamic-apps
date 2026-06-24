/* window action menu (ops on a single window) */
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

#include <config.h>

#include <string.h>
#include <stdio.h>
#include <glib/gi18n-lib.h>

#include "window-action-menu.h"
#include "private.h"

/**
 * SECTION:window-action-menu
 * @short_description: a menu widget, used to manipulate a window.
 * @see_also: #WnckWindow
 * @stability: Unstable
 *
 * A #WnckActionMenu is a menu containing items to manipulate a window.
 * Relevant actions are displayed in the menu, and updated if the window state
 * changes. The content of this menu is synchronized with the similar menu
 * available in Metacity.
 *
 * <note>
 *  <para>
 * If there is only one workspace with a viewport, the #WnckActionMenu will
 * contain items to move the window in the viewport as if the viewport feature
 * was used to create workspaces. This is useful since viewport is generally
 * used as an alternative way to create virtual desktops.
 *  </para>
 *  <para>
 * The #WnckActionMenu does not support moving the window in the viewport if
 * there are multiple workspaces on the screen: those two notions are so
 * similar that having both at the same time would result in a menu which would
 * be confusing to the user.
 *  </para>
 * </note>
 */

typedef enum
{
  CLOSE,
  MINIMIZE,
  MAXIMIZE,
  ABOVE,
  MOVE,
  RESIZE,
  PIN,
  UNPIN,
  LEFT,
  RIGHT,
  UP,
  DOWN,
  MOVE_TO_WORKSPACE
} WindowAction;

struct _WnckActionMenuPrivate
{
  WnckWindow *window;
  GtkWidget *minimize_item;
  GtkWidget *maximize_item;
  GtkWidget *above_item;
  GtkWidget *move_item;
  GtkWidget *resize_item;
  GtkWidget *close_item;
  GtkWidget *workspace_separator;
  GtkWidget *pin_item;
  GtkWidget *unpin_item;
  GtkWidget *left_item;
  GtkWidget *right_item;
  GtkWidget *up_item;
  GtkWidget *down_item;
  GtkWidget *workspace_item;
  guint idle_handler;
};

enum {
	PROP_0,
	PROP_WINDOW
};

G_DEFINE_TYPE (WnckActionMenu, wnck_action_menu, GTK_TYPE_MENU);
#define WNCK_ACTION_MENU_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), WNCK_TYPE_ACTION_MENU, WnckActionMenuPrivate))

static void wnck_action_menu_dispose (GObject *object);

static void window_weak_notify (gpointer data,
                                GObject *window);

static void refill_submenu_workspace (WnckActionMenu *menu);
static void refill_submenu_viewport (WnckActionMenu *menu);

static void
window_weak_notify (gpointer data,
                    GObject *window)
{
  WNCK_ACTION_MENU(data)->priv->window = NULL;
  gtk_widget_destroy (GTK_WIDGET (data));
}

static WnckActionMenu*
get_action_menu (GtkWidget *widget)
{
  while (widget) {
    if (GTK_IS_MENU_ITEM (widget))
      widget = gtk_widget_get_parent (widget);

    if (WNCK_IS_ACTION_MENU (widget))
      return WNCK_ACTION_MENU (widget);

    widget = gtk_menu_get_attach_widget (GTK_MENU (widget));
    if (widget == NULL)
      break;
  }

  return NULL;
}

static void
item_activated_callback (GtkWidget *menu_item,
                         gpointer   data)
{
  WnckActionMenu *menu;
  WnckWindow *window;
  WindowAction action = GPOINTER_TO_INT (data);
  WnckScreen *screen;
  gboolean viewport_mode;

  menu = get_action_menu (menu_item);
  if (menu == NULL)
    return;

  window = menu->priv->window;

  screen = wnck_window_get_screen (window);
  viewport_mode = wnck_screen_get_workspace_count (screen) == 1 &&
                  wnck_workspace_is_virtual (wnck_screen_get_workspace (screen,
                                                                        0));

  switch (action)
    {
    case CLOSE:
      /* In an activate callback, so gtk_get_current_event_time() suffices */
      wnck_window_close (window,
			 gtk_get_current_event_time ());
      break;
    case MINIMIZE:
      if (wnck_window_is_minimized (window))
        wnck_window_unminimize (window,
                                gtk_get_current_event_time ());
      else
        wnck_window_minimize (window);
      break;
    case MAXIMIZE:
      if (wnck_window_is_maximized (window))
        wnck_window_unmaximize (window);
      else
        wnck_window_maximize (window);
      break;
    case ABOVE:
      if (wnck_window_is_above (window))
        wnck_window_unmake_above (window);
      else
        wnck_window_make_above (window);
      break;
    case MOVE:
      wnck_window_keyboard_move (window);
      break;
    case RESIZE:
      wnck_window_keyboard_size (window);
      break;
    case PIN:
      if (!viewport_mode)
        wnck_window_pin (window);
      else
        wnck_window_stick (window);
      break;
    case UNPIN:
      if (!viewport_mode)
        wnck_window_unpin (window);
      else
        wnck_window_unstick (window);
      break;
    case LEFT:
      if (!viewport_mode)
        {
          WnckWorkspace *workspace;
          workspace = wnck_workspace_get_neighbor (wnck_window_get_workspace (window),
                                                   WNCK_MOTION_LEFT);
          wnck_window_move_to_workspace (window, workspace);
        }
      else
        {
          int width, xw, yw, ww, hw;

          width = wnck_screen_get_width (screen);
          wnck_window_get_geometry (window, &xw, &yw, &ww, &hw);
          wnck_window_unstick (window);
          wnck_window_set_geometry (window, 0,
                                    WNCK_WINDOW_CHANGE_X | WNCK_WINDOW_CHANGE_Y,
                                    xw - width, yw,
                                    ww, hw);
        }
      break;
    case RIGHT:
      if (!viewport_mode)
        {
          WnckWorkspace *workspace;
          workspace = wnck_workspace_get_neighbor (wnck_window_get_workspace (window),
                                                   WNCK_MOTION_RIGHT);
          wnck_window_move_to_workspace (window, workspace);
        }
      else
        {
          int width, xw, yw, ww, hw;

          width = wnck_screen_get_width (screen);
          wnck_window_get_geometry (window, &xw, &yw, &ww, &hw);
          wnck_window_unstick (window);
          wnck_window_set_geometry (window, 0,
                                    WNCK_WINDOW_CHANGE_X | WNCK_WINDOW_CHANGE_Y,
                                    xw + width, yw,
                                    ww, hw);
        }
      break;
    case UP:
      if (!viewport_mode)
        {
          WnckWorkspace *workspace;
          workspace = wnck_workspace_get_neighbor (wnck_window_get_workspace (window),
                                                   WNCK_MOTION_UP);
          wnck_window_move_to_workspace (window, workspace);
        }
      else
        {
          int height, xw, yw, ww, hw;

          height = wnck_screen_get_height (screen);
          wnck_window_get_geometry (window, &xw, &yw, &ww, &hw);
          wnck_window_unstick (window);
          wnck_window_set_geometry (window, 0,
                                    WNCK_WINDOW_CHANGE_X | WNCK_WINDOW_CHANGE_Y,
                                    xw, yw - height,
                                    ww, hw);
        }
      break;
    case DOWN:
      if (!viewport_mode)
        {
          WnckWorkspace *workspace;
          workspace = wnck_workspace_get_neighbor (wnck_window_get_workspace (window),
                                                   WNCK_MOTION_DOWN);
          wnck_window_move_to_workspace (window, workspace);
        }
      else
        {
          int height, xw, yw, ww, hw;

          height = wnck_screen_get_height (screen);
          wnck_window_get_geometry (window, &xw, &yw, &ww, &hw);
          wnck_window_unstick (window);
          wnck_window_set_geometry (window, 0,
                                    WNCK_WINDOW_CHANGE_X | WNCK_WINDOW_CHANGE_Y,
                                    xw, yw + height,
                                    ww, hw);
        }
      break;
    case MOVE_TO_WORKSPACE:
      if (!viewport_mode)
        {
          int workspace_index;
          WnckWorkspace *workspace;

          workspace_index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item),
                                                                "workspace"));

          workspace = wnck_screen_get_workspace (screen, workspace_index);
          wnck_window_move_to_workspace (window, workspace);
        }
      else
        {
          WnckWorkspace *workspace;
          int new_viewport_x, new_viewport_y;
          int xw, yw, ww, hw;
          int viewport_x, viewport_y;

          new_viewport_x = GPOINTER_TO_INT (
                                      g_object_get_data (G_OBJECT (menu_item),
                                                         "viewport_x"));
          new_viewport_y = GPOINTER_TO_INT (
                                      g_object_get_data (G_OBJECT (menu_item),
                                                         "viewport_y"));

          workspace = wnck_screen_get_workspace (screen, 0);

          wnck_window_get_geometry (window, &xw, &yw, &ww, &hw);

          viewport_x = wnck_workspace_get_viewport_x (workspace);
          viewport_y = wnck_workspace_get_viewport_y (workspace);

          wnck_window_unstick (window);
          wnck_window_set_geometry (window, 0,
                                    WNCK_WINDOW_CHANGE_X | WNCK_WINDOW_CHANGE_Y,
                                    xw + new_viewport_x - viewport_x,
                                    yw + new_viewport_y - viewport_y,
                                    ww, hw);
        }
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
set_item_text (GtkWidget  *mi,
               const char *text)
{
  GtkLabel *label;

  label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (mi)));
  gtk_label_set_text (label, text);
  gtk_label_set_use_underline (label, TRUE);
}

static void
set_item_stock (GtkWidget  *mi,
                const char *stock_id)
{
  GtkWidget *image;

  image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (mi));

  if (stock_id == NULL)
    {
      if (image != NULL)
        gtk_widget_destroy (image);
      return;
    }

  if (image == NULL)
    {
      image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), image);
    }
  else
    {
      gtk_image_set_from_stock (GTK_IMAGE (image), stock_id,
                                GTK_ICON_SIZE_MENU);
    }
}

static gboolean
update_menu_state (WnckActionMenu *menu)
{
  WnckActionMenuPrivate *priv;
  WnckWindowActions      actions;
  WnckScreen            *screen;
  WnckWorkspace         *workspace;
  gboolean               viewport_mode;
  gboolean               move_workspace_sensitive;

  priv = menu->priv;

  priv->idle_handler = 0;

  actions = wnck_window_get_actions (priv->window);
  screen  = wnck_window_get_screen  (priv->window);

  viewport_mode = wnck_screen_get_workspace_count (screen) == 1 &&
                  wnck_workspace_is_virtual (wnck_screen_get_workspace (screen,
                                                                        0));
  move_workspace_sensitive = viewport_mode ||
                             (actions & WNCK_WINDOW_ACTION_CHANGE_WORKSPACE) != 0;

  if (wnck_window_is_minimized (priv->window))
    {
      set_item_text (priv->minimize_item, _("Unmi_nimize"));
      set_item_stock (priv->minimize_item, NULL);
      gtk_widget_set_sensitive (priv->minimize_item,
                                (actions & WNCK_WINDOW_ACTION_UNMINIMIZE) != 0);
    }
  else
    {
      set_item_text (priv->minimize_item, _("Mi_nimize"));
      set_item_stock (priv->minimize_item, WNCK_STOCK_MINIMIZE);
      gtk_widget_set_sensitive (priv->minimize_item,
                                (actions & WNCK_WINDOW_ACTION_MINIMIZE) != 0);
    }

  if (wnck_window_is_maximized (priv->window))
    {
      set_item_text (priv->maximize_item, _("Unma_ximize"));
      set_item_stock (priv->maximize_item, NULL);
      gtk_widget_set_sensitive (priv->maximize_item,
                                (actions & WNCK_WINDOW_ACTION_UNMAXIMIZE) != 0);
    }
  else
    {
      set_item_text (priv->maximize_item, _("Ma_ximize"));
      set_item_stock (priv->maximize_item, WNCK_STOCK_MAXIMIZE);
      gtk_widget_set_sensitive (priv->maximize_item,
                                (actions & WNCK_WINDOW_ACTION_MAXIMIZE) != 0);
    }

  g_signal_handlers_block_by_func (G_OBJECT (priv->above_item),
                                   item_activated_callback,
                                   GINT_TO_POINTER (ABOVE));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (priv->above_item),
                                  wnck_window_is_above (priv->window));
  g_signal_handlers_unblock_by_func (G_OBJECT (priv->above_item),
                                     item_activated_callback,
                                     GINT_TO_POINTER (ABOVE));

  gtk_widget_set_sensitive (priv->above_item,
                            (actions & WNCK_WINDOW_ACTION_ABOVE) != 0);

  g_signal_handlers_block_by_func (G_OBJECT (priv->pin_item),
                                   item_activated_callback,
                                   GINT_TO_POINTER (PIN));
  g_signal_handlers_block_by_func (G_OBJECT (priv->unpin_item),
                                   item_activated_callback,
                                   GINT_TO_POINTER (UNPIN));
  if ((viewport_mode  && wnck_window_is_sticky (priv->window)) ||
      (!viewport_mode && wnck_window_is_pinned (priv->window)))
          gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (priv->pin_item),
                                          TRUE);
  else
          gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (priv->unpin_item),
                                          TRUE);
  g_signal_handlers_unblock_by_func (G_OBJECT (priv->pin_item),
                                     item_activated_callback,
                                     GINT_TO_POINTER (PIN));
  g_signal_handlers_unblock_by_func (G_OBJECT (priv->unpin_item),
                                     item_activated_callback,
                                     GINT_TO_POINTER (UNPIN));

  gtk_widget_set_sensitive (priv->pin_item,
                            move_workspace_sensitive);

  gtk_widget_set_sensitive (priv->unpin_item,
                            move_workspace_sensitive);

  gtk_widget_set_sensitive (priv->close_item,
                            (actions & WNCK_WINDOW_ACTION_CLOSE) != 0);

  gtk_widget_set_sensitive (priv->move_item,
                            (actions & WNCK_WINDOW_ACTION_MOVE) != 0);

  gtk_widget_set_sensitive (priv->resize_item,
                            (actions & WNCK_WINDOW_ACTION_RESIZE) != 0);

  gtk_widget_set_sensitive (priv->workspace_item,
                            move_workspace_sensitive);

  gtk_widget_set_sensitive (priv->left_item,
                            move_workspace_sensitive);
  gtk_widget_set_sensitive (priv->right_item,
                            move_workspace_sensitive);
  gtk_widget_set_sensitive (priv->up_item,
                            move_workspace_sensitive);
  gtk_widget_set_sensitive (priv->down_item,
                            move_workspace_sensitive);

  workspace = wnck_window_get_workspace (priv->window);

  if (viewport_mode && !wnck_window_is_sticky (priv->window))
    {
      int window_x, window_y;
      int viewport_x, viewport_y;
      int viewport_width, viewport_height;
      int screen_width, screen_height;

      if (!workspace)
        workspace = wnck_screen_get_workspace (screen, 0);

      wnck_window_get_geometry (priv->window, &window_x, &window_y, NULL, NULL);

      viewport_x = wnck_workspace_get_viewport_x (workspace);
      viewport_y = wnck_workspace_get_viewport_y (workspace);

      window_x += viewport_x;
      window_y += viewport_y;

      viewport_width = wnck_workspace_get_width (workspace);
      viewport_height = wnck_workspace_get_height (workspace);

      screen_width = wnck_screen_get_width (screen);
      screen_height = wnck_screen_get_height (screen);

      if (window_x >= screen_width)
        gtk_widget_show (priv->left_item);
      else
        gtk_widget_hide (priv->left_item);

      if (window_x < viewport_width - screen_width)
        gtk_widget_show (priv->right_item);
      else
        gtk_widget_hide (priv->right_item);

      if (window_y >= screen_height)
        gtk_widget_show (priv->up_item);
      else
        gtk_widget_hide (priv->up_item);

      if (window_y < viewport_height - screen_height)
        gtk_widget_show (priv->down_item);
      else
        gtk_widget_hide (priv->down_item);
    }
  else if (!viewport_mode && workspace && !wnck_window_is_pinned (priv->window))
    {
      if (wnck_workspace_get_neighbor (workspace, WNCK_MOTION_LEFT))
        gtk_widget_show (priv->left_item);
      else
        gtk_widget_hide (priv->left_item);

      if (wnck_workspace_get_neighbor (workspace, WNCK_MOTION_RIGHT))
        gtk_widget_show (priv->right_item);
      else
        gtk_widget_hide (priv->right_item);

      if (wnck_workspace_get_neighbor (workspace, WNCK_MOTION_UP))
        gtk_widget_show (priv->up_item);
      else
        gtk_widget_hide (priv->up_item);

      if (wnck_workspace_get_neighbor (workspace, WNCK_MOTION_DOWN))
        gtk_widget_show (priv->down_item);
      else
        gtk_widget_hide (priv->down_item);
    }
  else
    {
      gtk_widget_hide (priv->left_item);
      gtk_widget_hide (priv->right_item);
      gtk_widget_hide (priv->up_item);
      gtk_widget_hide (priv->down_item);
    }

  if (viewport_mode)
    {
      int viewport_width, viewport_height;
      int screen_width, screen_height;

      viewport_width = wnck_workspace_get_width (workspace);
      viewport_height = wnck_workspace_get_height (workspace);

      screen_width = wnck_screen_get_width (screen);
      screen_height = wnck_screen_get_height (screen);

      gtk_widget_show (priv->workspace_separator);
      gtk_widget_show (priv->pin_item);
      gtk_widget_show (priv->unpin_item);
      if (viewport_width  >= 2 * screen_width ||
          viewport_height >= 2 * screen_height)
        {
          gtk_widget_show (priv->workspace_item);
          refill_submenu_viewport (menu);
        }
      else
        {
          gtk_widget_hide (priv->workspace_item);
          gtk_menu_popdown (GTK_MENU (gtk_menu_item_get_submenu (GTK_MENU_ITEM (priv->workspace_item))));
        }
    }
  else if (wnck_screen_get_workspace_count (screen) > 1)
    {
      gtk_widget_show (priv->workspace_separator);
      gtk_widget_show (priv->pin_item);
      gtk_widget_show (priv->unpin_item);
      gtk_widget_show (priv->workspace_item);
      refill_submenu_workspace (menu);
    }
  else
    {
      gtk_widget_hide (priv->workspace_separator);
      gtk_widget_hide (priv->pin_item);
      gtk_widget_hide (priv->unpin_item);
      gtk_widget_hide (priv->workspace_item);
      gtk_menu_popdown (GTK_MENU (gtk_menu_item_get_submenu (GTK_MENU_ITEM (priv->workspace_item))));
    }

  gtk_menu_reposition (GTK_MENU (menu));

  return FALSE;
}

static void
queue_update (WnckActionMenu *menu)
{
  if (menu->priv->idle_handler == 0)
    menu->priv->idle_handler = g_idle_add ((GSourceFunc)update_menu_state,
                                           menu);
}

static void
state_changed_callback (WnckWindow     *window,
                        WnckWindowState changed_mask,
                        WnckWindowState new_state,
                        gpointer        data)
{
  queue_update (WNCK_ACTION_MENU (data));
}

static void
actions_changed_callback (WnckWindow       *window,
                          WnckWindowActions changed_mask,
                          WnckWindowActions new_actions,
                          gpointer          data)
{
  queue_update (WNCK_ACTION_MENU (data));
}

static void
workspace_changed_callback (WnckWindow *window,
                            gpointer    data)
{
  queue_update (WNCK_ACTION_MENU (data));
}

static void
screen_workspace_callback (WnckWindow    *window,
                           WnckWorkspace *space,
                           gpointer       data)
{
  queue_update (WNCK_ACTION_MENU (data));
}

static void
viewports_changed_callback (WnckWindow *window,
                            gpointer    data)
{
  queue_update (WNCK_ACTION_MENU (data));
}

static GtkWidget*
make_radio_menu_item (WindowAction   action,
                      GSList       **group,
                      const gchar   *mnemonic_text)
{
  GtkWidget *mi;

  mi = gtk_radio_menu_item_new_with_mnemonic (*group, mnemonic_text);
  *group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (mi));

  g_signal_connect (G_OBJECT (mi), "activate",
                    G_CALLBACK (item_activated_callback),
                    GINT_TO_POINTER (action));

  gtk_widget_show (mi);

  return mi;
}

static GtkWidget*
make_check_menu_item (WindowAction  action,
                      const gchar  *mnemonic_text)
{
  GtkWidget *mi;

  mi = gtk_check_menu_item_new_with_mnemonic (mnemonic_text);

  g_signal_connect (G_OBJECT (mi), "activate",
                    G_CALLBACK (item_activated_callback),
                    GINT_TO_POINTER (action));

  gtk_widget_show (mi);

  return mi;
}

static GtkWidget*
make_menu_item (WindowAction action)
{
  GtkWidget *mi;

  mi = gtk_image_menu_item_new_with_label ("");

  g_signal_connect (G_OBJECT (mi), "activate",
                    G_CALLBACK (item_activated_callback),
                    GINT_TO_POINTER (action));

  gtk_widget_show (mi);

  return mi;
}

static char *
get_workspace_name_with_accel (WnckWindow *window,
			       int index)
{
  const char *name;
  int number;

  name = wnck_workspace_get_name (wnck_screen_get_workspace (wnck_window_get_screen (window),
				  index));

  g_assert (name != NULL);

  /*
   * If the name is of the form "Workspace x" where x is an unsigned
   * integer, insert a '_' before the number if it is less than 10 and
   * return it
   */
  number = 0;
  if (sscanf (name, _("Workspace %d"), &number) == 1) {
      /* Keep this in sync with what is in refill_submenu_viewport() */
      char *new_name;

      /*
       * Above name is a pointer into the Workspace struct. Here we make
       * a copy copy so we can have our wicked way with it.
       */
      if (number == 10)
        new_name = g_strdup_printf (_("Workspace 1_0"));
      else
        new_name = g_strdup_printf (_("Workspace %s%d"),
                                    number < 10 ? "_" : "",
                                    number);
      return new_name;
  }
  else {
      /*
       * Otherwise this is just a normal name. Escape any _ characters so that
       * the user's workspace names do not get mangled.  If the number is less
       * than 10 we provide an accelerator.
       */
      char *new_name;
      const char *source;
      char *dest;

      /*
       * Assume the worst case, that every character is a _.  We also
       * provide memory for " (_#)"
       */
      new_name = g_malloc0 (strlen (name) * 2 + 6 + 1);

      /*
       * Now iterate down the strings, adding '_' to escape as we go
       */
      dest = new_name;
      source = name;
      while (*source != '\0') {
          if (*source == '_')
            *dest++ = '_';
          *dest++ = *source++;
      }

      /* People don't start at workstation 0, but workstation 1 */
      if (index < 9) {
          g_snprintf (dest, 6, " (_%d)", index + 1);
      }
      else if (index == 9) {
          g_snprintf (dest, 6, " (_0)");
      }

      return new_name;
  }
}

static void
refill_submenu_workspace (WnckActionMenu *menu)
{
  GtkWidget *submenu;
  GList *children;
  GList *l;
  int num_workspaces, window_space, i;
  WnckWorkspace *workspace;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menu->priv->workspace_item));

  /* Remove existing items */
  children = gtk_container_get_children (GTK_CONTAINER (submenu));
  for (l = children; l; l = l->next)
    gtk_container_remove (GTK_CONTAINER (submenu), l->data);
  g_list_free (children);

  workspace = wnck_window_get_workspace (menu->priv->window);

  num_workspaces = wnck_screen_get_workspace_count (wnck_window_get_screen (menu->priv->window));

  if (workspace)
    window_space = wnck_workspace_get_number (workspace);
  else
    window_space = -1;

  for (i = 0; i < num_workspaces; i++)
    {
      char      *name;
      GtkWidget *item;

      name = get_workspace_name_with_accel (menu->priv->window, i);

      item = make_menu_item (MOVE_TO_WORKSPACE);
      g_object_set_data (G_OBJECT (item), "workspace", GINT_TO_POINTER (i));

      if (i == window_space)
        gtk_widget_set_sensitive (item, FALSE);

      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
      set_item_text (item, name);
      set_item_stock (item, NULL);

      g_free (name);
    }

  gtk_menu_reposition (GTK_MENU (submenu));
}

static void
refill_submenu_viewport (WnckActionMenu *menu)
{
  GtkWidget *submenu;
  GList *children;
  GList *l;
  WnckScreen *screen;
  WnckWorkspace *workspace;
  int window_x, window_y;
  int viewport_x, viewport_y;
  int viewport_width, viewport_height;
  int screen_width, screen_height;
  int x, y;
  int number;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menu->priv->workspace_item));

  /* Remove existing items */
  children = gtk_container_get_children (GTK_CONTAINER (submenu));
  for (l = children; l; l = l->next)
    gtk_container_remove (GTK_CONTAINER (submenu), l->data);
  g_list_free (children);

  screen = wnck_window_get_screen (menu->priv->window);
  workspace = wnck_screen_get_workspace (screen, 0);

  wnck_window_get_geometry (menu->priv->window,
                            &window_x, &window_y, NULL, NULL);

  viewport_x = wnck_workspace_get_viewport_x (workspace);
  viewport_y = wnck_workspace_get_viewport_y (workspace);

  window_x += viewport_x;
  window_y += viewport_y;

  viewport_width = wnck_workspace_get_width (workspace);
  viewport_height = wnck_workspace_get_height (workspace);

  screen_width = wnck_screen_get_width (screen);
  screen_height = wnck_screen_get_height (screen);

  number = 1;
  for (y = 0; y < viewport_height; y += screen_height)
    {
      char      *label;
      GtkWidget *item;

      for (x = 0; x < viewport_width; x += screen_width)
        {
          /* Keep this in sync with what is in get_workspace_name_with_accel()
           */
          if (number == 10)
            label = g_strdup_printf (_("Workspace 1_0"));
          else
            label = g_strdup_printf (_("Workspace %s%d"),
                                     number < 10 ? "_" : "",
                                     number);
          number++;

          item = make_menu_item (MOVE_TO_WORKSPACE);
          g_object_set_data (G_OBJECT (item), "viewport_x",
                             GINT_TO_POINTER (x));
          g_object_set_data (G_OBJECT (item), "viewport_y",
                             GINT_TO_POINTER (y));

          if (window_x >= x && window_x < x + screen_width &&
              window_y >= y && window_y < y + screen_height)
            gtk_widget_set_sensitive (item, FALSE);

          gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
          set_item_text (item, label);
          set_item_stock (item, NULL);

          g_free (label);
        }
    }

  gtk_menu_reposition (GTK_MENU (submenu));
}

static void
wnck_action_menu_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  WnckActionMenu *menu;

  g_return_if_fail (WNCK_IS_ACTION_MENU (object));

  menu = WNCK_ACTION_MENU (object);

  switch (prop_id)
    {
      case PROP_WINDOW:
        g_value_set_pointer (value, menu->priv->window);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}


static void
wnck_action_menu_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  WnckActionMenu *menu;

  g_return_if_fail (WNCK_IS_ACTION_MENU (object));

  menu = WNCK_ACTION_MENU (object);

  switch (prop_id)
    {
      case PROP_WINDOW:
        g_return_if_fail (WNCK_IS_WINDOW (g_value_get_pointer (value)));

        menu->priv->window = g_value_get_pointer (value);
        g_object_notify (G_OBJECT (menu), "window");
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
wnck_action_menu_init (WnckActionMenu *menu)
{
  menu->priv = WNCK_ACTION_MENU_GET_PRIVATE (menu);

  menu->priv->window = NULL;
  menu->priv->minimize_item = NULL;
  menu->priv->maximize_item = NULL;
  menu->priv->above_item = NULL;
  menu->priv->move_item = NULL;
  menu->priv->resize_item = NULL;
  menu->priv->close_item = NULL;
  menu->priv->workspace_separator = NULL;
  menu->priv->pin_item = NULL;
  menu->priv->unpin_item = NULL;
  menu->priv->left_item = NULL;
  menu->priv->right_item = NULL;
  menu->priv->up_item = NULL;
  menu->priv->down_item = NULL;
  menu->priv->workspace_item = NULL;
  menu->priv->idle_handler = 0;
}

static GObject *
wnck_action_menu_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_properties)
{
  GObject               *obj;
  WnckActionMenu        *menu;
  WnckActionMenuPrivate *priv;
  GtkWidget             *submenu;
  GtkWidget             *separator;
  GSList                *pin_group;
  WnckScreen            *screen;


  obj = G_OBJECT_CLASS (wnck_action_menu_parent_class)->constructor (type,
                                                                     n_construct_properties,
                                                                     construct_properties);

  menu = WNCK_ACTION_MENU (obj);
  priv = menu->priv;

  if (priv->window == NULL)
    {
      g_warning ("No window specified during creation of the action menu");
      return obj;
    }

  g_object_weak_ref (G_OBJECT (priv->window), window_weak_notify, menu);

  priv->minimize_item = make_menu_item (MINIMIZE);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         priv->minimize_item);

  priv->maximize_item = make_menu_item (MAXIMIZE);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         priv->maximize_item);

  priv->move_item = make_menu_item (MOVE);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         priv->move_item);

  set_item_text (priv->move_item, _("_Move"));
  set_item_stock (priv->move_item, NULL);

  priv->resize_item = make_menu_item (RESIZE);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         priv->resize_item);

  set_item_text (priv->resize_item, _("_Resize"));
  set_item_stock (priv->move_item, NULL);

  priv->workspace_separator = separator = gtk_separator_menu_item_new ();
  gtk_widget_show (separator);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         separator);

  priv->above_item = make_check_menu_item (ABOVE,
                                          _("Always On _Top"));

  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         priv->above_item);

  pin_group = NULL;

  priv->pin_item = make_radio_menu_item (PIN, &pin_group,
                                        _("_Always on Visible Workspace"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         priv->pin_item);

  priv->unpin_item = make_radio_menu_item (UNPIN, &pin_group,
                                          _("_Only on This Workspace"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         priv->unpin_item);

  priv->left_item = make_menu_item (LEFT);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         priv->left_item);
  set_item_text (priv->left_item, _("Move to Workspace _Left"));
  set_item_stock (priv->left_item, NULL);

  priv->right_item = make_menu_item (RIGHT);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         priv->right_item);
  set_item_text (priv->right_item, _("Move to Workspace R_ight"));
  set_item_stock (priv->right_item, NULL);

  priv->up_item = make_menu_item (UP);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         priv->up_item);
  set_item_text (priv->up_item, _("Move to Workspace _Up"));
  set_item_stock (priv->up_item, NULL);

  priv->down_item = make_menu_item (DOWN);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         priv->down_item);
  set_item_text (priv->down_item, _("Move to Workspace _Down"));
  set_item_stock (priv->down_item, NULL);

  priv->workspace_item = gtk_menu_item_new_with_mnemonic (_("Move to Another _Workspace"));
  gtk_widget_show (priv->workspace_item);

  submenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->workspace_item),
                             submenu);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         priv->workspace_item);

  separator = gtk_separator_menu_item_new ();
  gtk_widget_show (separator);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         separator);

  priv->close_item = make_menu_item (CLOSE);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         priv->close_item);

  set_item_text (priv->close_item, _("_Close"));
  set_item_stock (priv->close_item, WNCK_STOCK_DELETE);

  g_signal_connect_object (G_OBJECT (priv->window),
                           "state_changed",
                           G_CALLBACK (state_changed_callback),
                           G_OBJECT (menu),
                           0);

  g_signal_connect_object (G_OBJECT (priv->window),
                           "actions_changed",
                           G_CALLBACK (actions_changed_callback),
                           G_OBJECT (menu),
                           0);

  g_signal_connect_object (G_OBJECT (priv->window),
                           "workspace_changed",
                           G_CALLBACK (workspace_changed_callback),
                           G_OBJECT (menu),
                           0);

  screen = wnck_window_get_screen (priv->window);

  g_signal_connect_object (G_OBJECT (screen),
                           "workspace_created",
                           G_CALLBACK (screen_workspace_callback),
                           G_OBJECT (menu),
                           0);

  g_signal_connect_object (G_OBJECT (screen),
                           "workspace_destroyed",
                           G_CALLBACK (screen_workspace_callback),
                           G_OBJECT (menu),
                           0);

  g_signal_connect_object (G_OBJECT (screen),
                           "viewports_changed",
                           G_CALLBACK (viewports_changed_callback),
                           G_OBJECT (menu),
                           0);

  update_menu_state (menu);

  return obj;
}

static void
wnck_action_menu_class_init (WnckActionMenuClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  _wnck_stock_icons_init ();

  g_type_class_add_private (klass, sizeof (WnckActionMenuPrivate));

  object_class->constructor = wnck_action_menu_constructor;
  object_class->get_property = wnck_action_menu_get_property;
  object_class->set_property = wnck_action_menu_set_property;
  object_class->dispose = wnck_action_menu_dispose;

  g_object_class_install_property (object_class,
                                   PROP_WINDOW,
                                   g_param_spec_pointer ("window",
                                                         "Window",
                                                         "The window that will be manipulated through this menu",
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
wnck_action_menu_dispose (GObject *object)
{
  WnckActionMenu *menu;

  menu = WNCK_ACTION_MENU (object);

  if (menu->priv->idle_handler)
    {
      g_source_remove (menu->priv->idle_handler);
      menu->priv->idle_handler = 0;
    }

  if (WNCK_IS_WINDOW (menu->priv->window))
    {
      g_object_weak_unref (G_OBJECT (menu->priv->window), window_weak_notify, menu);
      g_signal_handlers_disconnect_by_data (menu->priv->window, menu);

      WnckScreen *screen = wnck_window_get_screen (menu->priv->window);
      g_signal_handlers_disconnect_by_data (screen, menu);

      menu->priv->window = NULL;
    }

  G_OBJECT_CLASS (wnck_action_menu_parent_class)->dispose (object);
}

/**
 * wnck_action_menu_new:
 * @window: the #WnckWindow for which a menu will be created.
 *
 * Creates a new #WnckActionMenu. The #WnckActionMenu will be filled with menu
 * items for window operations on @window.
 *
 * Return value: a newly created #WnckActionMenu.
 *
 * Since: 2.22
 **/
GtkWidget*
wnck_action_menu_new (WnckWindow *window)
{
  g_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);

  return g_object_new (WNCK_TYPE_ACTION_MENU,
                       "window", window,
                       NULL);
}
