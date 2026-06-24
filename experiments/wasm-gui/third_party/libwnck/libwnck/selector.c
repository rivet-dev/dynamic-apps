/* selector */
/* vim: set sw=2 et: */
/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2001 Free Software Foundation, Inc.
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2005-2007 Vincent Untz
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
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 *      George Lebl <jirka@5z.com>
 *      Jacob Berkman <jacob@helixcode.com>
 */

#include <config.h>

#include <gtk/gtk.h>

#include <glib/gi18n-lib.h>
#include "selector.h"
#include "inlinepixbufs.h"
#include "libwnck.h"
#include "screen.h"
#include "private.h"

/**
 * SECTION:selector
 * @short_description: a window selector widget, showing the list of windows as
 * a menu.
 * @see_also: #WnckTasklist
 * @stability: Unstable
 *
 * The #WnckSelector represents client windows on a screen as a menu, where
 * menu items are labelled with the window titles and icons. Activating a menu
 * item activates the represented window.
 *
 * The #WnckSelector will automatically detect the screen it is on, and will
 * represent windows of this screen only.
 */

typedef struct
{
  GtkWidget *item;
  GtkWidget *label;
} window_hash_item;

struct _WnckSelectorPrivate {
  GtkWidget  *image;
  WnckWindow *icon_window;

  /* those have the same lifecycle as the menu */
  GtkWidget  *menu;
  GtkWidget  *no_windows_item;
  GHashTable *window_hash;

  int size;
};

G_DEFINE_TYPE (WnckSelector, wnck_selector, GTK_TYPE_MENU_BAR);
#define WNCK_SELECTOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), WNCK_TYPE_SELECTOR, WnckSelectorPrivate))

static GObject *wnck_selector_constructor (GType                  type,
                                           guint                  n_construct_properties,
                                           GObjectConstructParam *construct_properties);
static void wnck_selector_dispose           (GObject           *object);
static void wnck_selector_finalize          (GObject           *object);
static void wnck_selector_realize           (GtkWidget *widget);
static void wnck_selector_unrealize         (GtkWidget *widget);
static gboolean wnck_selector_scroll_event  (GtkWidget        *widget,
                                             GdkEventScroll   *event);
static void wnck_selector_connect_to_window (WnckSelector      *selector,
                                             WnckWindow        *window);

static void wnck_selector_insert_window (WnckSelector *selector,
                                         WnckWindow   *window);
static void wnck_selector_append_window (WnckSelector *selector,
                                         WnckWindow   *window);

static gint
wnck_selector_windows_compare (gconstpointer  a,
                               gconstpointer  b)
{
  int posa;
  int posb;

  posa = wnck_window_get_sort_order (WNCK_WINDOW (a));
  posb = wnck_window_get_sort_order (WNCK_WINDOW (b));

  return (posa - posb);
}

static void
wncklet_connect_while_alive (gpointer object,
                             const char *signal,
                             GCallback func,
                             gpointer func_data, gpointer alive_object)
{
  GClosure *closure;

  closure = g_cclosure_new (func, func_data, NULL);
  g_object_watch_closure (G_OBJECT (alive_object), closure);
  g_signal_connect_closure_by_id (object,
                                  g_signal_lookup (signal,
                                                   G_OBJECT_TYPE (object)), 0,
                                  closure, FALSE);
}

static WnckScreen *
wnck_selector_get_screen (WnckSelector *selector)
{
  GdkScreen *screen;

  g_assert (gtk_widget_has_screen (GTK_WIDGET (selector)));

  screen = gtk_widget_get_screen (GTK_WIDGET (selector));

  return wnck_screen_get (gdk_screen_get_number (screen));
}

static GdkPixbuf *
wnck_selector_get_default_window_icon (void)
{
  static GdkPixbuf *retval = NULL;

  if (retval)
    return retval;

  retval = gdk_pixbuf_new_from_inline (-1, default_icon_data, FALSE, NULL);

  g_assert (retval);

  return retval;
}

static GdkPixbuf *
wnck_selector_dimm_icon (GdkPixbuf *pixbuf)
{
  int x, y, pixel_stride, row_stride;
  guchar *row, *pixels;
  int w, h;
  GdkPixbuf *dimmed;

  w = gdk_pixbuf_get_width (pixbuf);
  h = gdk_pixbuf_get_height (pixbuf);

  if (gdk_pixbuf_get_has_alpha (pixbuf))
    dimmed = gdk_pixbuf_copy (pixbuf);
  else
    dimmed = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);

  pixel_stride = 4;

  row = gdk_pixbuf_get_pixels (dimmed);
  row_stride = gdk_pixbuf_get_rowstride (dimmed);

  for (y = 0; y < h; y++)
    {
      pixels = row;
      for (x = 0; x < w; x++)
        {
          pixels[3] /= 2;
          pixels += pixel_stride;
        }
      row += row_stride;
    }

  return dimmed;
}

static void
wnck_selector_set_window_icon (WnckSelector *selector,
                               GtkWidget *image,
                               WnckWindow *window, gboolean use_icon_size)
{
  GdkPixbuf *pixbuf, *freeme, *freeme2;
  int width, height;
  int icon_size = -1;

  pixbuf = NULL;
  freeme = NULL;
  freeme2 = NULL;

  if (window)
    pixbuf = wnck_window_get_mini_icon (window);

  if (!pixbuf)
    pixbuf = wnck_selector_get_default_window_icon ();

  if (!use_icon_size && selector->priv->size > 1)
    icon_size = selector->priv->size;

  if (icon_size == -1)
    gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, NULL, &icon_size);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  if (icon_size != -1 && (width > icon_size || height > icon_size))
    {
      double scale;

      scale = ((double) icon_size) / MAX (width, height);

      pixbuf = gdk_pixbuf_scale_simple (pixbuf, width * scale,
                                        height * scale, GDK_INTERP_BILINEAR);
      freeme = pixbuf;
    }

  if (window && wnck_window_is_minimized (window))
    {
      pixbuf = wnck_selector_dimm_icon (pixbuf);
      freeme2 = pixbuf;
    }

  gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);

  if (freeme)
    g_object_unref (freeme);
  if (freeme2)
    g_object_unref (freeme2);
}

static void
wnck_selector_set_active_window (WnckSelector *selector, WnckWindow *window)
{
  wnck_selector_set_window_icon (selector, selector->priv->image,
		  		 window, FALSE);
  selector->priv->icon_window = window;
}

static void
wnck_selector_make_menu_consistent (WnckSelector *selector)
{
  GList     *l, *children;
  int        workspace_n;
  GtkWidget *workspace_item;
  GtkWidget *separator;
  gboolean   separator_is_first;
  gboolean   separator_is_last;
  gboolean   visible_window;

  workspace_n = -1;
  workspace_item = NULL;

  separator = NULL;
  separator_is_first = FALSE;
  separator_is_last = FALSE;

  visible_window = FALSE;

  children = gtk_container_get_children (GTK_CONTAINER (selector->priv->menu));

  for (l = children; l; l = l->next)
    {
      int i;

      i = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (l->data),
                                              "wnck-selector-workspace-n"));

      if (i > 0)
        {
          workspace_n = i - 1;

          /* we have two consecutive workspace items => hide the first */
          if (workspace_item)
            gtk_widget_hide (workspace_item);

          workspace_item = GTK_WIDGET (l->data);
        }
      else if (GTK_IS_SEPARATOR_MENU_ITEM (l->data))
        {
          if (!visible_window)
            separator_is_first = TRUE;
          separator_is_last = TRUE;
          separator = GTK_WIDGET (l->data);
        }
      else if (gtk_widget_get_visible (l->data) &&
               l->data != selector->priv->no_windows_item)
        {
          separator_is_last = FALSE;
          visible_window = TRUE;

          /* if we know of a workspace item that was not shown */
          if (workspace_item)
            {
              WnckWindow    *window;
              WnckWorkspace *workspace;

              window = g_object_get_data (G_OBJECT (l->data),
                                          "wnck-selector-window");

              if (window)
                {
                  workspace = wnck_window_get_workspace (window);
                  if (workspace &&
                      workspace_n == wnck_workspace_get_number (workspace))
                    {
                      gtk_widget_show (workspace_item);
                      workspace_n = -1;
                      workspace_item = NULL;
                    }
                }
            }
        } /* end if (normal item) */
    }

  g_list_free (children);

  /* do we have a trailing workspace item to be hidden? */
  if (workspace_item)
    gtk_widget_hide (workspace_item);

  if (separator)
    {
      if (separator_is_first || separator_is_last)
        gtk_widget_hide (separator);
      else
        gtk_widget_show (separator);
    }

  if (visible_window)
    gtk_widget_hide (selector->priv->no_windows_item);
  else
    gtk_widget_show (selector->priv->no_windows_item);
}

static void
wnck_selector_window_icon_changed (WnckWindow *window,
                                   WnckSelector *selector)
{
  window_hash_item *item;
  GtkWidget *image;

  if (selector->priv->icon_window == window)
    wnck_selector_set_active_window (selector, window);

  item = NULL;

  if (!selector->priv->window_hash)
	  return;

  item = g_hash_table_lookup (selector->priv->window_hash, window);
  if (item != NULL)
    {
      image = gtk_image_new ();
      wnck_selector_set_window_icon (selector, image, window, TRUE);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item->item),
                                     GTK_WIDGET (image));
      gtk_widget_show (image);
    }
}

static void
wnck_selector_window_name_changed (WnckWindow *window,
                                   WnckSelector *selector)
{
  window_hash_item *item;
  char *window_name;

  item = NULL;
  window_name = NULL;

  if (!selector->priv->window_hash)
	  return;

  item = g_hash_table_lookup (selector->priv->window_hash, window);
  if (item != NULL)
    {
      window_name = _wnck_window_get_name_for_display (window, FALSE, TRUE);
      gtk_label_set_text (GTK_LABEL (item->label), window_name);
      g_free (window_name);
    }
}

static void
wnck_selector_window_state_changed (WnckWindow *window,
                                    WnckWindowState changed_mask,
                                    WnckWindowState new_state,
                                    WnckSelector *selector)
{
  window_hash_item *item;
  char *window_name;

  if (!
      (changed_mask &
       (WNCK_WINDOW_STATE_MINIMIZED | WNCK_WINDOW_STATE_SHADED |
        WNCK_WINDOW_STATE_SKIP_TASKLIST |
        WNCK_WINDOW_STATE_DEMANDS_ATTENTION |
        WNCK_WINDOW_STATE_URGENT)))
    return;

  item = NULL;
  window_name = NULL;

  if (!selector->priv->window_hash)
	  return;

  item = g_hash_table_lookup (selector->priv->window_hash, window);
  if (item == NULL)
    return;

  if (changed_mask & WNCK_WINDOW_STATE_SKIP_TASKLIST)
    {
      if (wnck_window_is_skip_tasklist (window))
        gtk_widget_hide (item->item);
      else
        gtk_widget_show (item->item);

      wnck_selector_make_menu_consistent (selector);

      gtk_menu_reposition (GTK_MENU (selector->priv->menu));
    }

  if (changed_mask &
      (WNCK_WINDOW_STATE_DEMANDS_ATTENTION | WNCK_WINDOW_STATE_URGENT))
    {
      if (wnck_window_or_transient_needs_attention (window))
	_make_gtk_label_bold (GTK_LABEL (item->label));
      else
	_make_gtk_label_normal (GTK_LABEL (item->label));
    }

  if (changed_mask &
      (WNCK_WINDOW_STATE_MINIMIZED | WNCK_WINDOW_STATE_SHADED))
    {
      window_name = _wnck_window_get_name_for_display (window, FALSE, TRUE);
      gtk_label_set_text (GTK_LABEL (item->label), window_name);
      g_free (window_name);
    }
}


static void
wnck_selector_window_workspace_changed (WnckWindow   *window,
                                        WnckSelector *selector)
{
  window_hash_item *item;

  item = NULL;

  if (!selector->priv->menu || !gtk_widget_get_visible (selector->priv->menu))
    return;

  if (!selector->priv->window_hash)
    return;

  item = g_hash_table_lookup (selector->priv->window_hash, window);
  if (!item)
    return;

  /* destroy the item and recreate one so it's at the right position */
  gtk_widget_destroy (item->item);
  g_hash_table_remove (selector->priv->window_hash, window);

  wnck_selector_insert_window (selector, window);
  wnck_selector_make_menu_consistent (selector);

  gtk_menu_reposition (GTK_MENU (selector->priv->menu));
}

static void
wnck_selector_active_window_changed (WnckScreen   *screen,
                                     WnckWindow   *previous_window,
                                     WnckSelector *selector)
{
  WnckWindow *window;

  window = wnck_screen_get_active_window (screen);

  if (selector->priv->icon_window != window)
    wnck_selector_set_active_window (selector, window);
}

static void
wnck_selector_activate_window (WnckWindow *window)
{
  WnckWorkspace *workspace;
  guint32 timestamp;

  /* We're in an activate callback, so gtk_get_current_time() works... */
  timestamp = gtk_get_current_event_time ();

  /* FIXME: THIS IS SICK AND WRONG AND BUGGY.  See the end of
   * http://mail.gnome.org/archives/wm-spec-list/2005-July/msg00032.html
   * There should only be *one* activate call.
   */
  workspace = wnck_window_get_workspace (window);
  if (workspace)
    wnck_workspace_activate (workspace, timestamp);

  wnck_window_activate (window, timestamp);
}

#define SELECTOR_MAX_WIDTH 50   /* maximum width in characters */

static gint
wnck_selector_get_width (GtkWidget *widget, const char *text)
{
  GtkStyleContext *style_context;
  GtkStateFlags state;
  PangoContext *context;
  PangoFontMetrics *metrics;
  PangoFontDescription *description;
  gint char_width;
  PangoLayout *layout;
  PangoRectangle natural;
  gint max_width;
  gint screen_width;
  gint width;

  state = gtk_widget_get_state_flags (widget);
  style_context = gtk_widget_get_style_context (widget);
  gtk_style_context_get (style_context, state, GTK_STYLE_PROPERTY_FONT, &description, NULL);

  context = gtk_widget_get_pango_context (widget);
  metrics = pango_context_get_metrics (context, description,
                                       pango_context_get_language (context));
  char_width = pango_font_metrics_get_approximate_char_width (metrics);
  pango_font_metrics_unref (metrics);
  max_width = PANGO_PIXELS (SELECTOR_MAX_WIDTH * char_width);

  layout = gtk_widget_create_pango_layout (widget, text);
  pango_layout_get_pixel_extents (layout, NULL, &natural);
  g_object_unref (G_OBJECT (layout));

  screen_width = gdk_screen_get_width (gtk_widget_get_screen (widget));

  width = MIN (natural.width, max_width);
  width = MIN (width, 3 * (screen_width / 4));

  return width;
}

static void
wnck_selector_drag_begin (GtkWidget          *widget,
			  GdkDragContext     *context,
			  WnckWindow         *window)
{
  while (widget)
    {
      if (WNCK_IS_SELECTOR (widget))
        break;

      if (GTK_IS_MENU (widget))
        widget = gtk_menu_get_attach_widget (GTK_MENU (widget));
      else
        widget = gtk_widget_get_parent (widget);
    }

  if (widget)
    _wnck_window_set_as_drag_icon (window, context, widget);
}

static void
wnck_selector_drag_data_get (GtkWidget          *widget,
			     GdkDragContext     *context,
			     GtkSelectionData   *selection_data,
			     guint               info,
			     guint               time,
			     WnckWindow         *window)
{
  gulong xid;

  xid = wnck_window_get_xid (window);
  gtk_selection_data_set (selection_data,
                          gtk_selection_data_get_target (selection_data),
			  8, (guchar *)&xid, sizeof (gulong));
}

static GtkWidget *
wnck_selector_item_new (WnckSelector *selector,
                        const gchar *label, WnckWindow *window)
{
  GtkWidget *item;
  GtkWidget *ellipsizing_label;
  window_hash_item *hash_item;
  static const GtkTargetEntry targets[] = {
    { "application/x-wnck-window-id", 0, 0 }
  };

  item = gtk_image_menu_item_new ();
  gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (item), TRUE);

  ellipsizing_label = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (ellipsizing_label), 0.0, 0.5);
  gtk_label_set_ellipsize (GTK_LABEL (ellipsizing_label),
                           PANGO_ELLIPSIZE_END);

  if (window != NULL)
    {
      /* if window demands attention, bold the label */
      if (wnck_window_or_transient_needs_attention (window))
	_make_gtk_label_bold (GTK_LABEL (ellipsizing_label));

      hash_item = g_new0 (window_hash_item, 1);
      hash_item->item = item;
      hash_item->label = ellipsizing_label;
      g_hash_table_insert (selector->priv->window_hash, window, hash_item);
    }

  gtk_container_add (GTK_CONTAINER (item), ellipsizing_label);

  gtk_widget_show (ellipsizing_label);

  gtk_widget_set_size_request (ellipsizing_label,
                               wnck_selector_get_width (GTK_WIDGET (selector),
                                                        label), -1);

  if (window != NULL)
    {
      gtk_drag_source_set (item,
                           GDK_BUTTON1_MASK,
                           targets, 1,
                           GDK_ACTION_MOVE);

      g_signal_connect_object (item, "drag_data_get",
                               G_CALLBACK (wnck_selector_drag_data_get),
                               G_OBJECT (window),
                               0);

      g_signal_connect_object (item, "drag_begin",
                               G_CALLBACK (wnck_selector_drag_begin),
                               G_OBJECT (window),
                               0);
    }

  return item;
}

static void
wnck_selector_workspace_name_changed (WnckWorkspace *workspace,
                                      GtkLabel      *label)
{
  GtkStyleContext *context;
  GdkRGBA          color;
  char            *name;
  char            *markup;

  context = gtk_widget_get_style_context (GTK_WIDGET (label));
  gtk_style_context_get_color (context, GTK_STATE_FLAG_INSENSITIVE, &color);

  name = g_markup_escape_text (wnck_workspace_get_name (workspace), -1);
  markup = g_strdup_printf ("<span size=\"x-small\" style=\"italic\" foreground=\"#%.2x%.2x%.2x\">%s</span>",
                            (int)(color.red * 65535 + 0.5),
                            (int)(color.green * 65535 + 0.5),
                            (int)(color.blue * 65535 + 0.5), name);
  g_free (name);

  gtk_label_set_markup (label, markup);
  g_free (markup);
}

static void
wnck_selector_workspace_label_style_updated (GtkLabel      *label,
                                             WnckWorkspace *workspace)
{
  wnck_selector_workspace_name_changed (workspace, label);
}

static void
wnck_selector_add_workspace (WnckSelector *selector,
                             WnckScreen   *screen,
                             int           workspace_n)
{
  WnckWorkspace *workspace;
  GtkWidget     *item;
  GtkWidget     *label;

  workspace = wnck_screen_get_workspace (screen, workspace_n);

  /* We use a separator in which we add a label. This makes the menu item not
   * selectable without any hack. */
  item = gtk_separator_menu_item_new ();

  label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  gtk_widget_show (label);
  /* the handler will also take care of setting the name for the first time,
   * and we'll be able to adapt to theme changes */
  g_signal_connect (G_OBJECT (label), "style-updated",
                    G_CALLBACK (wnck_selector_workspace_label_style_updated),
		    workspace);
  wncklet_connect_while_alive (workspace, "name_changed",
                               G_CALLBACK (wnck_selector_workspace_name_changed),
                                label, label);

  gtk_container_add (GTK_CONTAINER (item), label);

  gtk_menu_shell_append (GTK_MENU_SHELL (selector->priv->menu), item);

  g_object_set_data (G_OBJECT (item), "wnck-selector-workspace-n",
                     GINT_TO_POINTER (workspace_n + 1));
}

static GtkWidget *
wnck_selector_create_window (WnckSelector *selector, WnckWindow *window)
{
  GtkWidget *item;
  GtkWidget *image;
  char *name;

  name = _wnck_window_get_name_for_display (window, FALSE, TRUE);

  item = wnck_selector_item_new (selector, name, window);

  g_free (name);

  image = gtk_image_new ();

  wnck_selector_set_window_icon (selector, image, window, TRUE);

  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                 GTK_WIDGET (image));
  gtk_widget_show (image);

  g_signal_connect_swapped (item, "activate",
                            G_CALLBACK (wnck_selector_activate_window),
                            window);

  if (!wnck_window_is_skip_tasklist (window))
    gtk_widget_show (item);

  g_object_set_data (G_OBJECT (item), "wnck-selector-window", window);

  return item;
}

static void
wnck_selector_insert_window (WnckSelector *selector, WnckWindow *window)
{
  GtkWidget     *item;
  WnckScreen    *screen;
  WnckWorkspace *workspace;
  int            workspace_n;
  int            i;

  screen = wnck_selector_get_screen (selector);
  workspace = wnck_window_get_workspace (window);

  if (!workspace && !wnck_window_is_pinned (window))
    return;

  item = wnck_selector_create_window (selector, window);

  if (!workspace || workspace == wnck_screen_get_active_workspace (screen))
    {
      /* window is pinned or in the current workspace
       * => insert before the separator */
      GList *l, *children;

      i = 0;

      children = gtk_container_get_children (GTK_CONTAINER (selector->priv->menu));
      for (l = children; l; l = l->next)
        {
          if (GTK_IS_SEPARATOR_MENU_ITEM (l->data))
            break;
          i++;
        }
      g_list_free (children);

      gtk_menu_shell_insert (GTK_MENU_SHELL (selector->priv->menu),
                             item, i);
    }
  else
    {
      workspace_n = wnck_workspace_get_number (workspace);

      if (workspace_n == wnck_screen_get_workspace_count (screen) - 1)
        /* window is in last workspace => just append */
        gtk_menu_shell_append (GTK_MENU_SHELL (selector->priv->menu), item);
      else
        {
          /* insert just before the next workspace item */
          GList *l, *children;

          i = 0;

          children = gtk_container_get_children (GTK_CONTAINER (selector->priv->menu));
          for (l = children; l; l = l->next)
            {
              int j;
              j = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (l->data),
                                                      "wnck-selector-workspace-n"));
              if (j - 1 == workspace_n + 1)
                break;
              i++;
            }
          g_list_free (children);

          gtk_menu_shell_insert (GTK_MENU_SHELL (selector->priv->menu),
                                 item, i);
        }
    }
}

static void
wnck_selector_append_window (WnckSelector *selector, WnckWindow *window)
{
  GtkWidget *item;

  item = wnck_selector_create_window (selector, window);
  gtk_menu_shell_append (GTK_MENU_SHELL (selector->priv->menu), item);
}

static void
wnck_selector_window_opened (WnckScreen *screen,
                             WnckWindow *window, WnckSelector *selector)
{
  wnck_selector_connect_to_window (selector, window);

  if (!selector->priv->menu || !gtk_widget_get_visible (selector->priv->menu))
    return;

  if (!selector->priv->window_hash)
    return;

  wnck_selector_insert_window (selector, window);
  wnck_selector_make_menu_consistent (selector);

  gtk_menu_reposition (GTK_MENU (selector->priv->menu));
}

static void
wnck_selector_window_closed (WnckScreen *screen,
                             WnckWindow *window, WnckSelector *selector)
{
  window_hash_item *item;

  if (window == selector->priv->icon_window)
    wnck_selector_set_active_window (selector, NULL);

  if (!selector->priv->menu || !gtk_widget_get_visible (selector->priv->menu))
    return;

  if (!selector->priv->window_hash)
    return;

  item = g_hash_table_lookup (selector->priv->window_hash, window);
  if (!item)
    return;

  g_object_set_data (G_OBJECT (item->item), "wnck-selector-window", NULL);

  gtk_widget_hide (item->item);
  wnck_selector_make_menu_consistent (selector);

  gtk_menu_reposition (GTK_MENU (selector->priv->menu));
}

static void
wnck_selector_workspace_created (WnckScreen    *screen,
                                 WnckWorkspace *workspace,
                                 WnckSelector  *selector)
{
  if (!selector->priv->menu || !gtk_widget_get_visible (selector->priv->menu))
    return;

  /* this is assuming that the new workspace will have a higher number
   * than all the old workspaces, which is okay since the old workspaces
   * didn't disappear in the meantime */
  wnck_selector_add_workspace (selector, screen,
                               wnck_workspace_get_number (workspace));

  wnck_selector_make_menu_consistent (selector);

  gtk_menu_reposition (GTK_MENU (selector->priv->menu));
}

static void
wnck_selector_workspace_destroyed (WnckScreen    *screen,
                                   WnckWorkspace *workspace,
                                   WnckSelector  *selector)
{
  GList     *l, *children;
  GtkWidget *destroy;
  int        i;

  if (!selector->priv->menu || !gtk_widget_get_visible (selector->priv->menu))
    return;

  destroy = NULL;

  i = wnck_workspace_get_number (workspace);

  /* search for the item of this workspace so that we destroy it */
  children = gtk_container_get_children (GTK_CONTAINER (selector->priv->menu));

  for (l = children; l; l = l->next)
    {
      int j;

      j = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (l->data),
                           "wnck-selector-workspace-n"));


      if (j - 1 == i)
        destroy = GTK_WIDGET (l->data);
      else if (j - 1 > i)
        /* shift the following workspaces */
        g_object_set_data (G_OBJECT (l->data), "wnck-selector-workspace-n",
                           GINT_TO_POINTER (j - 1));
    }

  g_list_free (children);

  if (destroy)
    gtk_widget_destroy (destroy);

  wnck_selector_make_menu_consistent (selector);

  gtk_menu_reposition (GTK_MENU (selector->priv->menu));
}

static void
wnck_selector_connect_to_window (WnckSelector *selector, WnckWindow *window)
{
  wncklet_connect_while_alive (window, "icon_changed",
                               G_CALLBACK (wnck_selector_window_icon_changed),
                               selector, selector);
  wncklet_connect_while_alive (window, "name_changed",
                               G_CALLBACK (wnck_selector_window_name_changed),
                               selector, selector);
  wncklet_connect_while_alive (window, "state_changed",
                               G_CALLBACK (wnck_selector_window_state_changed),
                               selector, selector);
  wncklet_connect_while_alive (window, "workspace_changed",
                               G_CALLBACK (wnck_selector_window_workspace_changed),
                               selector, selector);
}

static void
wnck_selector_disconnect_from_window (WnckSelector *selector,
                                      WnckWindow   *window)
{
  g_signal_handlers_disconnect_by_func (window,
                                        wnck_selector_window_icon_changed,
                                        selector);
  g_signal_handlers_disconnect_by_func (window,
                                        wnck_selector_window_name_changed,
                                        selector);
  g_signal_handlers_disconnect_by_func (window,
                                        wnck_selector_window_state_changed,
                                        selector);
  g_signal_handlers_disconnect_by_func (window,
                                        wnck_selector_window_workspace_changed,
                                        selector);
}

static void
wnck_selector_connect_to_screen (WnckSelector *selector, WnckScreen *screen)
{
  wncklet_connect_while_alive (screen, "active_window_changed",
                               G_CALLBACK
                               (wnck_selector_active_window_changed),
                               selector, selector);

  wncklet_connect_while_alive (screen, "window_opened",
                               G_CALLBACK (wnck_selector_window_opened),
                               selector, selector);

  wncklet_connect_while_alive (screen, "window_closed",
                               G_CALLBACK (wnck_selector_window_closed),
                               selector, selector);

  wncklet_connect_while_alive (screen, "workspace_created",
                               G_CALLBACK (wnck_selector_workspace_created),
                               selector, selector);

  wncklet_connect_while_alive (screen, "workspace_destroyed",
                               G_CALLBACK (wnck_selector_workspace_destroyed),
                               selector, selector);
}

static void
wnck_selector_disconnect_from_screen (WnckSelector *selector,
                                      WnckScreen   *screen)
{
  g_signal_handlers_disconnect_by_func (screen,
                                        wnck_selector_active_window_changed,
                                        selector);
  g_signal_handlers_disconnect_by_func (screen,
                                        wnck_selector_window_opened,
                                        selector);
  g_signal_handlers_disconnect_by_func (screen,
                                        wnck_selector_window_closed,
                                        selector);
  g_signal_handlers_disconnect_by_func (screen,
                                        wnck_selector_workspace_created,
                                        selector);
  g_signal_handlers_disconnect_by_func (screen,
                                        wnck_selector_workspace_destroyed,
                                        selector);
}

static void
wnck_selector_destroy_menu (GtkWidget *widget, WnckSelector *selector)
{
  selector->priv->menu = NULL;

  if (selector->priv->window_hash)
    g_hash_table_destroy (selector->priv->window_hash);
  selector->priv->window_hash = NULL;

  selector->priv->no_windows_item = NULL;
}

static gboolean
wnck_selector_scroll_event (GtkWidget      *widget,
                            GdkEventScroll *event)
{
  WnckSelector *selector;
  WnckScreen *screen;
  WnckWorkspace *workspace;
  GList *windows_list;
  GList *l;
  WnckWindow *window;
  WnckWindow *previous_window;
  gboolean should_activate_next_window;

  selector = WNCK_SELECTOR (widget);

  screen = wnck_selector_get_screen (selector);
  workspace = wnck_screen_get_active_workspace (screen);
  windows_list = wnck_screen_get_windows (screen);
  windows_list = g_list_sort (windows_list, wnck_selector_windows_compare);

  /* Walk through the list of windows until we find the active one
   * (considering only those windows on the same workspace).
   * Then, depending on whether we're scrolling up or down, activate the next
   * window in the list (if it exists), or the previous one.
   */
  previous_window = NULL;
  should_activate_next_window = FALSE;
  for (l = windows_list; l; l = l->next)
    {
      window = WNCK_WINDOW (l->data);

      if (wnck_window_is_skip_tasklist (window))
        continue;

      if (workspace && !wnck_window_is_pinned (window) &&
          wnck_window_get_workspace (window) != workspace)
        continue;

      if (should_activate_next_window)
        {
          wnck_window_activate_transient (window, event->time);
          return TRUE;
        }

      if (wnck_window_is_active (window))
        {
          switch (event->direction)
            {
              case GDK_SCROLL_UP:
                if (previous_window != NULL)
                  {
                    wnck_window_activate_transient (previous_window,
                                                    event->time);
                    return TRUE;
                  }
              break;

              case GDK_SCROLL_DOWN:
                should_activate_next_window = TRUE;
              break;

              case GDK_SCROLL_LEFT:
              case GDK_SCROLL_RIGHT:
                /* We ignore LEFT and RIGHT scroll events. */
              break;

              case GDK_SCROLL_SMOOTH:
              break;

              default:
                g_assert_not_reached ();
            }
        }

      previous_window = window;
    }

  return TRUE;
}

static void
wnck_selector_menu_hidden (GtkWidget *menu, WnckSelector *selector)
{
  gtk_widget_set_state (GTK_WIDGET (selector), GTK_STATE_NORMAL);
}

static void
wnck_selector_on_show (GtkWidget *widget, WnckSelector *selector)
{
  GtkWidget *separator;
  WnckScreen *screen;
  WnckWorkspace *workspace;
  int nb_workspace;
  int i;
  GList **windows_per_workspace;
  GList *windows;
  GList *l, *children;

  /* Remove existing items */
  children = gtk_container_get_children (GTK_CONTAINER (selector->priv->menu));
  for (l = children; l; l = l->next)
    gtk_container_remove (GTK_CONTAINER (selector->priv->menu), l->data);
  g_list_free (children);

  if (selector->priv->window_hash)
    g_hash_table_destroy (selector->priv->window_hash);
  selector->priv->window_hash = g_hash_table_new_full (g_direct_hash,
                                                 g_direct_equal,
                                                 NULL, g_free);

  screen = wnck_selector_get_screen (selector);

  nb_workspace = wnck_screen_get_workspace_count (screen);
  windows_per_workspace = g_malloc0 (nb_workspace * sizeof (GList *));

  /* Get windows ordered by workspaces */
  windows = wnck_screen_get_windows (screen);
  windows = g_list_sort (windows, wnck_selector_windows_compare);

  for (l = windows; l; l = l->next)
    {
      workspace = wnck_window_get_workspace (l->data);
      if (!workspace && wnck_window_is_pinned (l->data))
        workspace = wnck_screen_get_active_workspace (screen);
      if (!workspace)
        continue;
      i = wnck_workspace_get_number (workspace);
      windows_per_workspace[i] = g_list_prepend (windows_per_workspace[i],
                                                 l->data);
    }

  /* Add windows from the current workspace */
  workspace = wnck_screen_get_active_workspace (screen);
  if (workspace)
    {
      i = wnck_workspace_get_number (workspace);

      windows_per_workspace[i] = g_list_reverse (windows_per_workspace[i]);
      for (l = windows_per_workspace[i]; l; l = l->next)
        wnck_selector_append_window (selector, l->data);
      g_list_free (windows_per_workspace[i]);
      windows_per_workspace[i] = NULL;
    }

  /* Add separator */
  separator = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (selector->priv->menu), separator);

  /* Add windows from other workspaces */
  for (i = 0; i < nb_workspace; i++)
    {
      wnck_selector_add_workspace (selector, screen, i);
      windows_per_workspace[i] = g_list_reverse (windows_per_workspace[i]);
      for (l = windows_per_workspace[i]; l; l = l->next)
        wnck_selector_append_window (selector, l->data);
      g_list_free (windows_per_workspace[i]);
      windows_per_workspace[i] = NULL;
    }
  g_free (windows_per_workspace);

  selector->priv->no_windows_item = wnck_selector_item_new (selector,
		  					    _("No Windows Open"),
							    NULL);
  gtk_widget_set_sensitive (selector->priv->no_windows_item, FALSE);
  gtk_menu_shell_append (GTK_MENU_SHELL (selector->priv->menu),
                         selector->priv->no_windows_item);

  wnck_selector_make_menu_consistent (selector);
}

static void
wnck_selector_fill (WnckSelector *selector)
{
  GtkWidget      *menu_item;
  GtkCssProvider *provider;

  menu_item = gtk_menu_item_new ();
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (selector), menu_item);

  selector->priv->image = gtk_image_new ();
  gtk_widget_show (selector->priv->image);
  gtk_container_add (GTK_CONTAINER (menu_item), selector->priv->image);

  selector->priv->menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item),
                             selector->priv->menu);
  g_signal_connect (selector->priv->menu, "hide",
                    G_CALLBACK (wnck_selector_menu_hidden), selector);
  g_signal_connect (selector->priv->menu, "destroy",
                    G_CALLBACK (wnck_selector_destroy_menu), selector);
  g_signal_connect (selector->priv->menu, "show",
                    G_CALLBACK (wnck_selector_on_show), selector);

  gtk_widget_set_name (GTK_WIDGET (selector),
                       "gnome-panel-window-menu-menu-bar");

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider,
                                   "#gnome-panel-window-menu-menu-bar {\n"
                                   " border-width: 0px;\n"
                                   "}",
                                   -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (selector)),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  gtk_widget_show (GTK_WIDGET (selector));
}

static void
wnck_selector_init (WnckSelector *selector)
{
  AtkObject *atk_obj;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (selector));
  atk_object_set_name (atk_obj, _("Window Selector"));
  atk_object_set_description (atk_obj, _("Tool to switch between windows"));

  selector->priv = WNCK_SELECTOR_GET_PRIVATE (selector);

  selector->priv->size = -1;

  gtk_widget_add_events (GTK_WIDGET (selector), GDK_SCROLL_MASK);
}

static void
wnck_selector_class_init (WnckSelectorClass *klass)
{
  GObjectClass   *object_class     = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class     = GTK_WIDGET_CLASS (klass);

  g_type_class_add_private (klass, sizeof (WnckSelectorPrivate));

  object_class->constructor = wnck_selector_constructor;
  object_class->dispose = wnck_selector_dispose;
  object_class->finalize = wnck_selector_finalize;

  widget_class->realize   = wnck_selector_realize;
  widget_class->unrealize = wnck_selector_unrealize;
  widget_class->scroll_event = wnck_selector_scroll_event;
}

static GObject *
wnck_selector_constructor (GType                  type,
                           guint                  n_construct_properties,
                           GObjectConstructParam *construct_properties)
{
  GObject *obj;

  obj = G_OBJECT_CLASS (wnck_selector_parent_class)->constructor (
                                                      type,
                                                      n_construct_properties,
                                                      construct_properties);

  wnck_selector_fill (WNCK_SELECTOR (obj));

  return obj;
}

static void
wnck_selector_finalize (GObject *object)
{
  WnckSelector *selector;

  selector = WNCK_SELECTOR (object);

  if (selector->priv->window_hash)
    g_hash_table_destroy (selector->priv->window_hash);
  selector->priv->window_hash = NULL;

  G_OBJECT_CLASS (wnck_selector_parent_class)->finalize (object);
}

static void
wnck_selector_dispose (GObject *object)
{
  WnckSelector *selector;

  selector = WNCK_SELECTOR (object);

  if (selector->priv->menu)
    gtk_widget_destroy (selector->priv->menu);
  selector->priv->menu = NULL;

  selector->priv->image       = NULL;
  selector->priv->icon_window = NULL;

  G_OBJECT_CLASS (wnck_selector_parent_class)->dispose (object);
}

static void
wnck_selector_realize (GtkWidget *widget)
{
  WnckSelector *selector;
  WnckScreen   *screen;
  WnckWindow   *window;
  GList        *l;

  GTK_WIDGET_CLASS (wnck_selector_parent_class)->realize (widget);

  selector = WNCK_SELECTOR (widget);
  screen = wnck_selector_get_screen (selector);

  window = wnck_screen_get_active_window (screen);
  wnck_selector_set_active_window (selector, window);

  for (l = wnck_screen_get_windows (screen); l; l = l->next)
    wnck_selector_connect_to_window (selector, l->data);

  wnck_selector_connect_to_screen (selector, screen);
}

static void
wnck_selector_unrealize (GtkWidget *widget)
{
  WnckSelector *selector;
  WnckScreen   *screen;
  GList        *l;

  selector = WNCK_SELECTOR (widget);
  screen = wnck_selector_get_screen (selector);

  wnck_selector_disconnect_from_screen (selector, screen);

  for (l = wnck_screen_get_windows (screen); l; l = l->next)
    wnck_selector_disconnect_from_window (selector, l->data);

  GTK_WIDGET_CLASS (wnck_selector_parent_class)->unrealize (widget);
}

/**
 * wnck_selector_new:
 *
 * Creates a new #WnckSelector. The #WnckSelector will list #WnckWindow of the
 * #WnckScreen it is on.
 *
 * Return value: a newly created #WnckSelector.
 *
 * Since: 2.10
 */
GtkWidget *
wnck_selector_new (void)
{
  WnckSelector *selector;

  selector = g_object_new (WNCK_TYPE_SELECTOR, NULL);

  return GTK_WIDGET (selector);
}
