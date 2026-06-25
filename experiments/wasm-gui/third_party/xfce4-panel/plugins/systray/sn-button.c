/*
 *  Copyright (c) 2012-2013 Andrzej Radecki <andrzejr@xfce.org>
 *  Copyright (c) 2017      Viktor Odintsev <ninetls@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gdk/gdkx.h>

#include <libxfce4panel/libxfce4panel.h>

#include "sn-button.h"
#include "sn-icon-box.h"
#include "sn-util.h"



static void                  sn_button_finalize                      (GObject                 *object);

static gboolean              sn_button_button_press                  (GtkWidget               *widget,
                                                                      GdkEventButton          *event);

static gboolean              sn_button_button_release                (GtkWidget               *widget,
                                                                      GdkEventButton          *event);

static gboolean              sn_button_scroll_event                  (GtkWidget               *widget,
                                                                      GdkEventScroll          *event);

static void                  sn_button_unrealize                     (GtkWidget               *widget);

static void                  sn_button_menu_changed                  (GtkWidget               *widget,
                                                                      SnItem                  *item);

static gboolean              sn_button_query_tooltip                 (GtkWidget               *widget,
                                                                      gint                     x,
                                                                      gint                     y,
                                                                      gboolean                 keyboard_mode,
                                                                      GtkTooltip              *tooltip,
                                                                      gpointer                 user_data);



struct _SnButtonClass
{
  GtkButtonClass       __parent__;
};

struct _SnButton
{
  GtkButton            __parent__;

  SnItem              *item;
  SnPlugin            *plugin;
  SnConfig            *config;

  GtkWidget           *menu;
  gboolean             menu_only;

  GtkWidget           *box;

  guint                menu_deactivate_handler;
  guint                menu_size_allocate_handler;
  guint                menu_size_allocate_idle_handler;
};

G_DEFINE_TYPE (SnButton, sn_button, GTK_TYPE_BUTTON)



static void
sn_button_class_init (SnButtonClass *klass)
{
  GObjectClass   *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = sn_button_finalize;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->button_press_event = sn_button_button_press;
  widget_class->button_release_event = sn_button_button_release;
  widget_class->scroll_event = sn_button_scroll_event;
  widget_class->unrealize = sn_button_unrealize;
}



static void
sn_button_init (SnButton *button)
{
  GtkCssProvider *css_provider;
  GdkEventMask    event_mask = GDK_SCROLL_MASK;
  const gchar    *wm_name;

  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

  gtk_widget_set_name (GTK_WIDGET (button), "sn-button");
  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css_provider,
                                   "#sn-button {"
                                     "padding: 0px;"
                                     "border-width: 0px;"
                                   "}", -1, NULL);
  gtk_style_context_add_provider (GTK_STYLE_CONTEXT (gtk_widget_get_style_context (GTK_WIDGET (button))),
                                  GTK_STYLE_PROVIDER (css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css_provider);

  /* see https://gitlab.xfce.org/xfce/xfwm4/-/issues/641 */
  wm_name = gdk_x11_screen_get_window_manager_name (gtk_widget_get_screen (GTK_WIDGET (button)));
  if (g_strcmp0 (wm_name, "Xfwm4") != 0 && g_strcmp0 (wm_name, "unknown") != 0)
    event_mask |= GDK_SMOOTH_SCROLL_MASK;

  gtk_widget_add_events (GTK_WIDGET (button), event_mask);

  button->item = NULL;
  button->plugin = NULL;
  button->config = NULL;

  button->menu = NULL;
  button->menu_only = FALSE;

  button->box = NULL;

  button->menu_deactivate_handler = 0;
  button->menu_size_allocate_handler = 0;
  button->menu_size_allocate_idle_handler = 0;

  gtk_widget_set_halign (GTK_WIDGET (button), GTK_ALIGN_FILL);
  gtk_widget_set_valign (GTK_WIDGET (button), GTK_ALIGN_FILL);
}



SnItem *
sn_button_get_item (SnButton *button)
{
  g_return_val_if_fail (XFCE_IS_SN_BUTTON (button), NULL);

  return button->item;
}



const gchar *
sn_button_get_name (SnButton *button)
{
  g_return_val_if_fail (XFCE_IS_SN_BUTTON (button), NULL);

  return sn_item_get_name (button->item);
}



GtkWidget *
sn_button_new (SnItem   *item,
               SnPlugin *plugin,
               SnConfig *config)
{
  SnButton *button = g_object_new (XFCE_TYPE_SN_BUTTON, NULL);

  g_return_val_if_fail (XFCE_IS_SN_ITEM (item), NULL);
  g_return_val_if_fail (XFCE_IS_SN_CONFIG (config), NULL);

  button->item = item;
  button->plugin = plugin;
  button->config = config;

  button->box = sn_icon_box_new (item, config);
  gtk_container_add (GTK_CONTAINER (button), button->box);
  gtk_widget_show (button->box);

  g_object_set (G_OBJECT (button), "has-tooltip", TRUE, NULL);
  g_signal_connect (button, "query-tooltip",
                    G_CALLBACK (sn_button_query_tooltip), NULL);
  sn_signal_connect_weak_swapped (item, "tooltip-changed",
                                  G_CALLBACK (gtk_widget_trigger_tooltip_query), button);
  sn_signal_connect_weak_swapped (item, "menu-changed",
                                  G_CALLBACK (sn_button_menu_changed), button);
  sn_button_menu_changed (GTK_WIDGET (button), item);

  return GTK_WIDGET (button);
}



static void
sn_button_finalize (GObject *object)
{
  SnButton *button = XFCE_SN_BUTTON (object);

  if (button->menu_deactivate_handler != 0)
    g_signal_handler_disconnect (button->menu, button->menu_deactivate_handler);

  if (button->menu_size_allocate_handler != 0)
    g_signal_handler_disconnect (button->menu, button->menu_size_allocate_handler);

  if (button->menu_size_allocate_idle_handler != 0)
    g_source_remove (button->menu_size_allocate_idle_handler);

  G_OBJECT_CLASS (sn_button_parent_class)->finalize (object);
}



static void
sn_button_menu_deactivate (GtkWidget *widget,
                           GtkMenu   *menu)
{
  SnButton *button = XFCE_SN_BUTTON (widget);

  if (button->menu_deactivate_handler != 0)
  {
    g_signal_handler_disconnect (menu, button->menu_deactivate_handler);
    button->menu_deactivate_handler = 0;
  }

  gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_ACTIVE);
}



static gboolean
sn_button_button_press (GtkWidget      *widget,
                        GdkEventButton *event)
{
  SnButton *button = XFCE_SN_BUTTON (widget);
  gboolean  menu_is_primary;

  menu_is_primary = sn_config_get_menu_is_primary (button->config);

  if (event->button == 3 && (event->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
    {
      /* ctrl + right click is used to show plugin's menu */
      return FALSE;
    }

  if (event->button == 3 && menu_is_primary)
    {
      /* menu is available by left click, so show the panel menu instead */
      return FALSE;
    }

  if ((event->button == 1 && (button->menu_only || menu_is_primary)) || event->button == 3)
    {
      if (button->menu != NULL && sn_container_has_children (button->menu))
        {
          button->menu_deactivate_handler = 
            g_signal_connect_swapped (G_OBJECT (button->menu), "deactivate",
                                      G_CALLBACK (sn_button_menu_deactivate), button);

          xfce_panel_plugin_popup_menu (XFCE_PANEL_PLUGIN (button->plugin),
                                        GTK_MENU (button->menu), widget, (GdkEvent *) event);

          gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_ACTIVE, FALSE);
          return TRUE;
        }
      else if (event->button == 3)
        {
          /* dispay panel menu */
          return FALSE;
        }
    }

  /* process animations */
  GTK_WIDGET_CLASS (sn_button_parent_class)->button_press_event (widget, event);

  return TRUE;
}



static gboolean
sn_button_button_release (GtkWidget      *widget,
                          GdkEventButton *event)
{
  SnButton *button = XFCE_SN_BUTTON (widget);
  gboolean  menu_is_primary;

  /* leave if button release happens outside of systray item */
  if (event->x < 0 || event->x > gdk_window_get_width (event->window) ||
      event->y < 0 || event->y > gdk_window_get_height (event->window))
    return FALSE;

  menu_is_primary = sn_config_get_menu_is_primary (button->config);

  if (event->button == 1)
    {
      /* menu could be handled in button-press-event, check this */
      if (button->menu == NULL || !(button->menu_only || menu_is_primary))
        sn_item_activate (button->item, (gint) event->x_root, (gint) event->y_root);
    }
  else if (event->button == 2)
    {
      if (menu_is_primary && !button->menu_only)
        sn_item_activate (button->item, (gint) event->x_root, (gint) event->y_root);
      else
        sn_item_secondary_activate (button->item, (gint) event->x_root, (gint) event->y_root);
    }

  /* process animations */
  GTK_WIDGET_CLASS (sn_button_parent_class)->button_release_event (widget, event);

  return TRUE;
}



static gboolean
sn_button_scroll_event (GtkWidget      *widget,
                        GdkEventScroll *event)
{
  SnButton *button = XFCE_SN_BUTTON (widget);
  gdouble   delta_x, delta_y;

  /* see reference to https://gitlab.xfce.org/xfce/xfwm4/-/issues/641 in init() */
  if (gtk_widget_get_events (widget) & GDK_SMOOTH_SCROLL_MASK)
    {
      if (!gdk_event_get_scroll_deltas ((GdkEvent *)event, &delta_x, &delta_y))
        {
          delta_x = event->delta_x;
          delta_y = event->delta_y;
        }
    }
  else
    {
      delta_x = delta_y = 0;
      switch (event->direction)
        {
          case GDK_SCROLL_UP:
            delta_y = -1;
            break;
          case GDK_SCROLL_DOWN:
            delta_y = 1;
            break;
          case GDK_SCROLL_RIGHT:
            delta_x = -1;
            break;
          case GDK_SCROLL_LEFT:
            delta_x = 1;
            break;
          default:
            break;
        }
    }

  if (delta_x != 0 || delta_y != 0)
    {
      delta_x = (delta_x == 0 ? 0 : delta_x > 0 ? 1 : -1) *
                MAX (ABS (delta_x) + 0.5, 1);
      delta_y = (delta_y == 0 ? 0 : delta_y > 0 ? 1 : -1) *
                MAX (ABS (delta_y) + 0.5, 1);
      sn_item_scroll (button->item, (gint) delta_x, (gint) delta_y);
    }

  return TRUE;
}



static void
sn_button_unrealize (GtkWidget *widget)
{
  SnButton *button = XFCE_SN_BUTTON (widget);

  /*
   * The button could be hidden without being destroyed, as Blueman does for example when
   * the bluetooth service is stopped (see issue #391). As the menu is attached to the
   * button, care must be taken that it does not remain shown while the button is hidden.
   * This also triggers the "deactivate" signal handler to cleanly end the menu display.
   */
  if (button->menu != NULL && gtk_widget_get_visible (button->menu))
    gtk_menu_shell_deactivate (GTK_MENU_SHELL (button->menu));

  GTK_WIDGET_CLASS (sn_button_parent_class)->unrealize (widget);
}



static gboolean
sn_button_menu_size_changed_idle (gpointer user_data)
{
  SnButton *button = user_data;

  gtk_menu_reposition (GTK_MENU (button->menu));
  button->menu_size_allocate_idle_handler = 0;

  return G_SOURCE_REMOVE;
}



static void
sn_button_menu_size_changed (GtkWidget *widget)
{
  SnButton *button = XFCE_SN_BUTTON (widget);

  /* defer gtk_menu_reposition call since it may not work in size event handler */
  if (button->menu_size_allocate_idle_handler == 0)
    {
      button->menu_size_allocate_idle_handler =
        g_idle_add (sn_button_menu_size_changed_idle, button);
    }
}



static void
sn_button_menu_changed (GtkWidget *widget,
                        SnItem    *item)
{
  SnButton *button = XFCE_SN_BUTTON (widget);

  if (button->menu != NULL)
    {
      if (button->menu_deactivate_handler != 0)
        {
          g_signal_handler_disconnect (button->menu, button->menu_deactivate_handler);
          button->menu_deactivate_handler = 0;

          gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_ACTIVE);
          gtk_menu_popdown (GTK_MENU (button->menu));
        }

      if (button->menu_size_allocate_handler != 0)
        {
          g_signal_handler_disconnect (button->menu, button->menu_size_allocate_handler);
          button->menu_size_allocate_handler = 0;
        }

      if (button->menu_size_allocate_idle_handler != 0)
        {
          g_source_remove (button->menu_size_allocate_idle_handler);
          button->menu_size_allocate_idle_handler = 0;
        }

      gtk_menu_detach (GTK_MENU (button->menu));
    }

  button->menu_only = sn_item_is_menu_only (item);
  button->menu = sn_item_get_menu (item);

  if (button->menu != NULL)
    {
      gtk_menu_attach_to_widget (GTK_MENU (button->menu), GTK_WIDGET (button), NULL);
      /* restore menu position to its corner if size was changed */
      button->menu_size_allocate_handler =
        g_signal_connect_swapped (button->menu, "size-allocate",
                                  G_CALLBACK (sn_button_menu_size_changed), button);
    }
}



static gboolean
sn_button_query_tooltip (GtkWidget  *widget,
                         gint        x,
                         gint        y,
                         gboolean    keyboard_mode,
                         GtkTooltip *tooltip,
                         gpointer    user_data)
{
  SnButton    *button = XFCE_SN_BUTTON (widget);
  const gchar *tooltip_title;
  const gchar *tooltip_subtitle;
  gchar       *tooltip_title_escaped;
  gchar       *full;

  sn_item_get_tooltip (button->item, &tooltip_title, &tooltip_subtitle);

  if (tooltip_title != NULL)
    {
      if (tooltip_subtitle != NULL)
        {
          tooltip_title_escaped = g_markup_escape_text (tooltip_title, -1);
          full = g_strdup_printf ("<b>%s</b>\n%s", tooltip_title_escaped, tooltip_subtitle);
          gtk_tooltip_set_markup (tooltip, full);
          g_free (full);
          g_free (tooltip_title_escaped);
        }
      else
        {
          gtk_tooltip_set_text (tooltip, tooltip_title);
        }

      return TRUE;
    }

  return FALSE;
}
