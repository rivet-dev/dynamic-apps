/*
 * Copyright (C) 2011 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <gdk/gdkkeysyms.h>
#include <xfconf/xfconf.h>
#include <glib/gstdio.h>

#include <src/appfinder-window.h>
#include <src/appfinder-model.h>
#include <src/appfinder-category-model.h>
#include <src/appfinder-preferences.h>
#include <src/appfinder-actions.h>
#include <src/appfinder-private.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#define APPFINDER_WIDGET_XID(widget) ((guint) GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (widget))))
#else
#define APPFINDER_WIDGET_XID(widget) (0)
#endif



#define DEFAULT_WINDOW_WIDTH   580
#define DEFAULT_WINDOW_HEIGHT  422
#define DEFAULT_PANED_POSITION 180

#define XFCE_APPFINDER_LOCAL_PREFIX "file://"


static void       xfce_appfinder_window_finalize                      (GObject                     *object);
static void       xfce_appfinder_window_unmap                         (GtkWidget                   *widget);
static gboolean   xfce_appfinder_window_key_press_event               (GtkWidget                   *widget,
                                                                       GdkEventKey                 *event);
static gboolean   xfce_appfinder_window_window_state_event            (GtkWidget                   *widget,
                                                                       GdkEventWindowState         *event);
static void       xfce_appfinder_window_view                          (XfceAppfinderWindow         *window);
static gboolean   xfce_appfinder_window_popup_menu                    (GtkWidget                   *view,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_set_padding                   (GtkWidget                   *entry,
                                                                       GtkWidget                   *align);
static gboolean   xfce_appfinder_window_completion_match_func         (GtkEntryCompletion          *completion,
                                                                       const gchar                 *key,
                                                                       GtkTreeIter                 *iter,
                                                                       gpointer                     data);
static void       xfce_appfinder_window_entry_changed                 (XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_entry_activate                (GtkEditable                 *entry,
                                                                       XfceAppfinderWindow         *window);
static gboolean   xfce_appfinder_window_entry_key_press_event         (GtkWidget                   *entry,
                                                                       GdkEventKey                 *event,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_entry_icon_released           (GtkEntry                    *entry,
                                                                       GtkEntryIconPosition         icon_pos,
                                                                       GdkEvent                    *event,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_drag_begin                    (GtkWidget                   *widget,
                                                                       GdkDragContext              *drag_context,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_drag_data_get                 (GtkWidget                   *widget,
                                                                       GdkDragContext              *drag_context,
                                                                       GtkSelectionData            *data,
                                                                       guint                        info,
                                                                       guint                        drag_time,
                                                                       XfceAppfinderWindow         *window);
static gboolean   xfce_appfinder_window_treeview_key_press_event      (GtkWidget                   *widget,
                                                                       GdkEventKey                 *event,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_category_changed              (GtkTreeSelection            *selection,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_category_set_categories       (XfceAppfinderModel          *model,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_preferences                   (GtkWidget                   *button,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_property_changed              (XfconfChannel               *channel,
                                                                       const gchar                 *prop,
                                                                       const GValue                *value,
                                                                       XfceAppfinderWindow         *window);
static gboolean   xfce_appfinder_window_item_visible                  (GtkTreeModel                *model,
                                                                       GtkTreeIter                 *iter,
                                                                       gpointer                     data);
static void       xfce_appfinder_window_item_changed                  (XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_row_activated                 (XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_icon_theme_changed            (XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_launch_clicked                (XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_execute                       (XfceAppfinderWindow         *window,
                                                                       gboolean                     close_on_succeed);
static gint       xfce_appfinder_window_sort_items                    (GtkTreeModel                *model,
                                                                       GtkTreeIter                 *a,
                                                                       GtkTreeIter                 *b,
                                                                       gpointer                     data);
static gboolean   xfce_appfinder_should_sort_icon_view                (void);
static void       xfce_appfinder_window_update_frecency               (XfceAppfinderWindow         *window,
                                                                       GtkTreeModel                *model,
                                                                       const gchar                 *uri);
static gint       xfce_appfinder_window_sort_items_frecency           (GtkTreeModel                *model,
                                                                       GtkTreeIter                 *a,
                                                                       GtkTreeIter                 *b,
                                                                       gpointer                     data);
static gchar    **xfce_appfinder_parse_envp                           (gchar                      **cmd);

struct _XfceAppfinderWindowClass
{
  GtkWindowClass __parent__;
};

struct _XfceAppfinderWindow
{
  GtkWindow __parent__;

  XfceAppfinderModel         *model;

  GtkTreeModel               *sort_model;
  GtkTreeModel               *filter_model;

  XfceAppfinderCategoryModel *category_model;

  XfceAppfinderActions       *actions;

  GtkIconTheme               *icon_theme;

  GtkEntryCompletion         *completion;

  XfconfChannel              *channel;

  GtkWidget                  *paned;
  GtkWidget                  *entry;
  GtkWidget                  *image;
  GtkWidget                  *view;
  GtkWidget                  *viewscroll;
  GtkWidget                  *sidepane;

  GdkPixbuf                  *icon_find;

  GtkWidget                  *button_launch;

  GarconMenuDirectory        *filter_category;
  gchar                      *filter_text;

  guint                       idle_entry_changed_id;

  gint                        last_window_height;

  gulong                      property_watch_id;
  gulong                      categories_changed_id;
};

static const GtkTargetEntry target_list[] =
{
  { "text/uri-list", 0, 0 }
};



G_DEFINE_TYPE (XfceAppfinderWindow, xfce_appfinder_window, GTK_TYPE_WINDOW)



static void
xfce_appfinder_window_class_init (XfceAppfinderWindowClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_appfinder_window_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->unmap = xfce_appfinder_window_unmap;
  gtkwidget_class->key_press_event = xfce_appfinder_window_key_press_event;
  gtkwidget_class->window_state_event = xfce_appfinder_window_window_state_event;
}



static void
xfce_appfinder_window_init (XfceAppfinderWindow *window)
{
  GtkWidget          *vbox;
  GtkWidget          *entry;
  GtkWidget          *pane;
  GtkWidget          *scroll;
  GtkWidget          *sidepane;
  GtkWidget          *image;
  GtkWidget          *hbox;
  GtkTreeViewColumn  *column;
  GtkCellRenderer    *renderer;
  GtkTreeSelection   *selection;
  GtkWidget          *button;
  GtkEntryCompletion *completion;
  GtkCellRenderer    *cell;
  gint                integer;
  gboolean            sort_by_frecency;
  gint                scale_factor;

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (window));

  window->channel = xfconf_channel_get ("xfce4-appfinder");
  window->last_window_height = xfconf_channel_get_int (window->channel, "/last/window-height", DEFAULT_WINDOW_HEIGHT);

  window->category_model = xfce_appfinder_category_model_new (scale_factor);
  xfconf_g_property_bind (window->channel, "/category-icon-size", G_TYPE_UINT,
                          G_OBJECT (window->category_model), "icon-size");

  sort_by_frecency = xfconf_channel_get_bool (window->channel, "/sort-by-frecency", FALSE);
  window->model = xfce_appfinder_model_get (sort_by_frecency, scale_factor);
  xfconf_g_property_bind (window->channel, "/item-icon-size", G_TYPE_UINT,
                          G_OBJECT (window->model), "icon-size");

  gtk_window_set_title (GTK_WINDOW (window), _("Application Finder"));
  integer = xfconf_channel_get_int (window->channel, "/last/window-width", DEFAULT_WINDOW_WIDTH);
  gtk_window_set_default_size (GTK_WINDOW (window), integer, -1);
  gtk_window_set_icon_name (GTK_WINDOW (window), "org.xfce.appfinder");

  if (xfconf_channel_get_bool (window->channel, "/always-center", TRUE))
    gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);

  if (xfconf_channel_get_bool (window->channel, "/hide-window-decorations", FALSE))
    gtk_window_set_decorated (GTK_WINDOW (window), FALSE);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_widget_show (vbox);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  window->icon_find = xfce_appfinder_model_load_pixbuf (XFCE_APPFINDER_ICON_NAME_FIND,
                                                        XFCE_APPFINDER_ICON_SIZE_DEFAULT_ITEM,
                                                        scale_factor);
  window->image = image = gtk_image_new_from_pixbuf (window->icon_find);
  gtk_widget_set_size_request (image, 48, 48);
  gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (hbox), image);
  gtk_widget_show (image);
  
  window->entry = entry = gtk_entry_new ();
  gtk_widget_set_halign(entry, GTK_ALIGN_FILL);
  gtk_widget_set_valign (entry, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), entry);
  g_signal_connect (G_OBJECT (entry), "icon-release",
      G_CALLBACK (xfce_appfinder_window_entry_icon_released), window);
  g_signal_connect (G_OBJECT (entry), "realize",
      G_CALLBACK (xfce_appfinder_window_set_padding), entry);
  g_signal_connect_swapped (G_OBJECT (entry), "changed",
      G_CALLBACK (xfce_appfinder_window_entry_changed), window);
  g_signal_connect (G_OBJECT (entry), "activate",
      G_CALLBACK (xfce_appfinder_window_entry_activate), window);
  g_signal_connect (G_OBJECT (entry), "key-press-event",
      G_CALLBACK (xfce_appfinder_window_entry_key_press_event), window);
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     XFCE_APPFINDER_ICON_NAME_GO_DOWN);
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
                                   GTK_ENTRY_ICON_SECONDARY,
                                   _("Toggle view mode"));
  gtk_widget_show (entry);

  window->completion = completion = gtk_entry_completion_new ();
  gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (window->model));
  gtk_entry_completion_set_match_func (completion, xfce_appfinder_window_completion_match_func, window, NULL);
  g_object_set (G_OBJECT (completion), "text-column", XFCE_APPFINDER_MODEL_COLUMN_COMMAND, NULL);
  gtk_entry_completion_set_popup_single_match (completion, TRUE);
  gtk_entry_completion_set_inline_completion (completion, TRUE);

  cell = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (cell), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_cell_renderer_set_fixed_size (cell, 1, -1);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), cell, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (completion), cell, "text", XFCE_APPFINDER_MODEL_COLUMN_COMMAND);

  if (xfconf_channel_get_bool (window->channel, "/disable-completion", FALSE))
    gtk_entry_completion_set_popup_completion (completion, FALSE);

  window->paned = pane = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start (GTK_BOX (vbox), pane, TRUE, TRUE, 0);
  integer = xfconf_channel_get_int (window->channel, "/last/pane-position", DEFAULT_PANED_POSITION);
  gtk_paned_set_position (GTK_PANED (pane), integer);
  g_object_set (G_OBJECT (pane), "position-set", TRUE, NULL);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_paned_add1 (GTK_PANED (pane), scroll);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_show (scroll);

  if (xfconf_channel_get_bool (window->channel, "/hide-category-pane", TRUE))
    gtk_widget_set_visible (scroll, FALSE);

  sidepane = window->sidepane = gtk_tree_view_new_with_model (GTK_TREE_MODEL (window->category_model));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (sidepane), FALSE);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (sidepane), FALSE);
  g_signal_connect_swapped (G_OBJECT (sidepane), "start-interactive-search",
      G_CALLBACK (gtk_widget_grab_focus), entry);
  g_signal_connect (G_OBJECT (sidepane), "key-press-event",
      G_CALLBACK (xfce_appfinder_window_treeview_key_press_event), window);
  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (sidepane),
      xfce_appfinder_category_model_row_separator_func, NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroll), sidepane);
  gtk_widget_show (sidepane);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (sidepane));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
  g_signal_connect (G_OBJECT (selection), "changed",
      G_CALLBACK (xfce_appfinder_window_category_changed), window);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (sidepane), GTK_TREE_VIEW_COLUMN (column));

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, FALSE);
  gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (column), renderer,
                                       "surface", XFCE_APPFINDER_CATEGORY_MODEL_COLUMN_ICON, NULL);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, TRUE);
  gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (column), renderer,
                                       "text", XFCE_APPFINDER_CATEGORY_MODEL_COLUMN_NAME, NULL);

  window->viewscroll = scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_paned_add2 (GTK_PANED (pane), scroll);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_show (scroll);

  /* set the icon or tree view */
  xfce_appfinder_window_view (window);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  button = gtk_button_new_with_mnemonic (_("_Preferences"));
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
      G_CALLBACK (xfce_appfinder_window_preferences), window);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_button_set_always_show_image(GTK_BUTTON(button), TRUE);
  gtk_widget_show (button);

  image = gtk_image_new_from_icon_name (XFCE_APPFINDER_ICON_NAME_PREFERENCES, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);

  window->button_launch = button = gtk_button_new_with_mnemonic (_("La_unch"));
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
      G_CALLBACK (xfce_appfinder_window_launch_clicked), window);
  gtk_widget_set_sensitive (button, FALSE);
  gtk_button_set_always_show_image(GTK_BUTTON(button), TRUE);
  gtk_widget_show (button);

  image = gtk_image_new_from_icon_name (XFCE_APPFINDER_ICON_NAME_EXECUTE, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);

  /* update icons when icon theme changes */
  window->icon_theme = gtk_icon_theme_get_for_screen (gtk_window_get_screen (GTK_WINDOW (window)));
  g_signal_connect_swapped (G_OBJECT (window->icon_theme), "changed",
      G_CALLBACK (xfce_appfinder_window_icon_theme_changed), window);
  g_object_ref (G_OBJECT (window->icon_theme));

  /* update icons when scale factor changes */
  g_signal_connect (G_OBJECT (window), "notify::scale-factor",
      G_CALLBACK (xfce_appfinder_window_icon_theme_changed), NULL);

  /* load categories in the model */
  xfce_appfinder_window_category_set_categories (NULL, window);
  window->categories_changed_id =
      g_signal_connect (G_OBJECT (window->model), "categories-changed",
                        G_CALLBACK (xfce_appfinder_window_category_set_categories),
                        window);

  /* monitor xfconf property changes */
  window->property_watch_id =
    g_signal_connect (G_OBJECT (window->channel), "property-changed",
        G_CALLBACK (xfce_appfinder_window_property_changed), window);

  APPFINDER_DEBUG ("constructed window");
}



static void
xfce_appfinder_window_finalize (GObject *object)
{
  XfceAppfinderWindow *window = XFCE_APPFINDER_WINDOW (object);

  if (window->idle_entry_changed_id != 0)
    g_source_remove (window->idle_entry_changed_id);

  g_signal_handler_disconnect (window->channel, window->property_watch_id);
  g_signal_handler_disconnect (window->model, window->categories_changed_id);

  g_signal_handlers_disconnect_by_func (G_OBJECT (window),
      xfce_appfinder_window_icon_theme_changed, NULL);

  /* release our reference on the icon theme */
  g_signal_handlers_disconnect_by_func (G_OBJECT (window->icon_theme),
      xfce_appfinder_window_icon_theme_changed, window);
  g_object_unref (G_OBJECT (window->icon_theme));

  g_object_unref (G_OBJECT (window->model));
  g_object_unref (G_OBJECT (window->category_model));
  g_object_unref (G_OBJECT (window->filter_model));
  g_object_unref (G_OBJECT (window->sort_model));
  g_object_unref (G_OBJECT (window->completion));
  g_object_unref (G_OBJECT (window->icon_find));

  if (window->actions != NULL)
    g_object_unref (G_OBJECT (window->actions));

  if (window->filter_category != NULL)
    g_object_unref (G_OBJECT (window->filter_category));
  g_free (window->filter_text);

  (*G_OBJECT_CLASS (xfce_appfinder_window_parent_class)->finalize) (object);
}



static void
xfce_appfinder_window_unmap (GtkWidget *widget)
{
  XfceAppfinderWindow *window = XFCE_APPFINDER_WINDOW (widget);
  gint                 width, height;
  gint                 position;

  position = gtk_paned_get_position (GTK_PANED (window->paned));
  gtk_window_get_size (GTK_WINDOW (window), &width, &height);
  if (!gtk_widget_get_visible (window->paned))
    height = window->last_window_height;

  (*GTK_WIDGET_CLASS (xfce_appfinder_window_parent_class)->unmap) (widget);

  xfconf_channel_set_int (window->channel, "/last/window-height", height);
  xfconf_channel_set_int (window->channel, "/last/window-width", width);
  xfconf_channel_set_int (window->channel, "/last/pane-position", position);

  return (*GTK_WIDGET_CLASS (xfce_appfinder_window_parent_class)->unmap) (widget);
}



static gboolean
xfce_appfinder_window_key_press_event (GtkWidget   *widget,
                                       GdkEventKey *event)
{
  XfceAppfinderWindow   *window = XFCE_APPFINDER_WINDOW (widget);
  GtkWidget             *entry;
  XfceAppfinderIconSize  icon_size = XFCE_APPFINDER_ICON_SIZE_DEFAULT_ITEM;

  if (event->keyval == GDK_KEY_Escape)
    {
      gtk_widget_destroy (widget);
      return TRUE;
    }

  if ((event->state & GDK_CONTROL_MASK) != 0)
    {
      switch (event->keyval)
        {
        case GDK_KEY_l:
          entry = XFCE_APPFINDER_WINDOW (widget)->entry;

          gtk_widget_grab_focus (entry);
          gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);

          return TRUE;

        case GDK_KEY_1:
        case GDK_KEY_2:
          /* toggle between icon and tree view */
          xfconf_channel_set_bool (window->channel, "/icon-view",
                                   event->keyval == GDK_KEY_1);
          return TRUE;

        case GDK_KEY_plus:
        case GDK_KEY_minus:
        case GDK_KEY_KP_Add:
        case GDK_KEY_KP_Subtract:
          g_object_get (G_OBJECT (window->model), "icon-size", &icon_size, NULL);
          if ((event->keyval == GDK_KEY_plus || event->keyval == GDK_KEY_KP_Add))
            {
              if (icon_size < XFCE_APPFINDER_ICON_SIZE_LARGEST)
                icon_size++;
            }
          else if (icon_size > XFCE_APPFINDER_ICON_SIZE_SMALLEST)
            {
              icon_size--;
            }
          g_object_set (G_OBJECT (window->model), "icon-size", icon_size, NULL);
          return TRUE;

        case GDK_KEY_0:
        case GDK_KEY_KP_0:
          g_object_set (G_OBJECT (window->model), "icon-size", icon_size, NULL);
          return TRUE;
        }
    }

  return  (*GTK_WIDGET_CLASS (xfce_appfinder_window_parent_class)->key_press_event) (widget, event);
}



static gboolean
xfce_appfinder_window_window_state_event (GtkWidget           *widget,
                                          GdkEventWindowState *event)
{
  XfceAppfinderWindow *window = XFCE_APPFINDER_WINDOW (widget);
  gint                 width;

  if (!gtk_widget_get_visible (window->paned))
    {
      if ((event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0)
        {
          gtk_window_unmaximize (GTK_WINDOW (widget));

          /* set sensible width instead of taking entire width */
          width = xfconf_channel_get_int (window->channel, "/last/window-width", DEFAULT_WINDOW_WIDTH);
          gtk_window_resize (GTK_WINDOW (widget), width, 80 /* should be corrected by wm */);
        }

      if ((event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0)
        gtk_window_unfullscreen (GTK_WINDOW (widget));
    }

  if ((*GTK_WIDGET_CLASS (xfce_appfinder_window_parent_class)->window_state_event) != NULL)
    return (*GTK_WIDGET_CLASS (xfce_appfinder_window_parent_class)->window_state_event) (widget, event);

  return FALSE;
}



static void
xfce_appfinder_window_set_item_width (XfceAppfinderWindow *window)
{
  gint                   width = 0, padding = 0;
  gint                   text_column_idx, column_idx = 0;
  XfceAppfinderIconSize  icon_size;
  GtkOrientation         item_orientation = GTK_ORIENTATION_VERTICAL;
  GList                 *renderers;
  GList                 *li;

  appfinder_return_if_fail (GTK_IS_ICON_VIEW (window->view));

  g_object_get (G_OBJECT (window->model), "icon-size", &icon_size, NULL);

  /* some hard-coded values for the cell size that seem to work fine */
  switch (icon_size)
    {
    case XFCE_APPFINDER_ICON_SIZE_SMALLEST:
      padding = 2;
      width = 16 * 3.75;
      break;

    case XFCE_APPFINDER_ICON_SIZE_SMALLER:
      padding = 2;
      width = 24 * 3;
      break;

    case XFCE_APPFINDER_ICON_SIZE_SMALL:
      padding = 4;
      width = 36 * 2.5;
      break;

    case XFCE_APPFINDER_ICON_SIZE_NORMAL:
      padding = 4;
      width = 48 * 2;
      break;

    case XFCE_APPFINDER_ICON_SIZE_LARGE:
      padding = 6;
      width = 64 * 1.5;
      break;

    case XFCE_APPFINDER_ICON_SIZE_LARGER:
      padding = 6;
      width = 96 * 1.75;
      break;

    case XFCE_APPFINDER_ICON_SIZE_LARGEST:
      padding = 6;
      width = 128 * 1.25;
      break;
    }

  if (xfconf_channel_get_bool (window->channel, "/text-beside-icons", FALSE))
    {
      item_orientation = GTK_ORIENTATION_HORIZONTAL;
      width *= 2;
    }

  gtk_icon_view_set_item_orientation (GTK_ICON_VIEW (window->view), item_orientation);
  gtk_icon_view_set_item_padding (GTK_ICON_VIEW (window->view), padding);
  gtk_icon_view_set_item_width (GTK_ICON_VIEW (window->view), width);

  renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (window->view));
  text_column_idx = gtk_icon_view_get_text_column (GTK_ICON_VIEW (window->view));

  for (li = renderers; li != NULL; li = li->next, column_idx++) {
    /* work around the yalign = 0.0 gtk uses */
    if (item_orientation == GTK_ORIENTATION_HORIZONTAL) {
      g_object_set (li->data, "yalign", 0.5, NULL);
    }

    /* do not wrap when text beside icons is enabled */
    if (column_idx == text_column_idx) {
      g_object_set (li->data, "ellipsize",
                    item_orientation == GTK_ORIENTATION_HORIZONTAL ?
                    PANGO_ELLIPSIZE_END : PANGO_ELLIPSIZE_NONE, NULL);
    }
  }
  g_list_free (renderers);
}



static gboolean
xfce_appfinder_window_view_button_press_event (GtkWidget           *widget,
                                               GdkEventButton      *event,
                                               XfceAppfinderWindow *window)
{
  gint         x, y;
  GtkTreePath *path;
  gboolean     have_selection = FALSE;

  if (event->button == 3
      && event->type == GDK_BUTTON_PRESS)
    {
      if (GTK_IS_TREE_VIEW (widget))
        {
          gtk_tree_view_convert_widget_to_bin_window_coords (GTK_TREE_VIEW (widget),
                                                             event->x, event->y, &x, &y);

          if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget), x, y, &path, NULL, NULL, NULL))
            {
              gtk_tree_view_set_cursor (GTK_TREE_VIEW (widget), path, NULL, FALSE);
              gtk_tree_path_free (path);
              have_selection = TRUE;
            }
        }
      else
        {
          path = gtk_icon_view_get_path_at_pos (GTK_ICON_VIEW (widget), event->x, event->y);
          if (path != NULL)
            {
              gtk_icon_view_select_path (GTK_ICON_VIEW (widget), path);
              gtk_icon_view_set_cursor (GTK_ICON_VIEW (widget), path, NULL, FALSE);
              gtk_tree_path_free (path);
              have_selection = TRUE;
            }
        }

      if (have_selection)
        return xfce_appfinder_window_popup_menu (widget, window);
    }

  return FALSE;
}



static void
xfce_appfinder_window_view (XfceAppfinderWindow *window)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkTreeSelection  *selection;
  GtkWidget         *view;
  gboolean           icon_view;

  icon_view = xfconf_channel_get_bool (window->channel, "/icon-view", FALSE);
  if (window->view != NULL)
    {
      if (icon_view && GTK_IS_ICON_VIEW (window->view))
        return;
      gtk_widget_destroy (window->view);
    }

  if (window->filter_model == NULL)
    {
      window->filter_model = gtk_tree_model_filter_new (GTK_TREE_MODEL (window->model), NULL);
      gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (window->filter_model), xfce_appfinder_window_item_visible, window, NULL);
    }

  if (window->sort_model == NULL)
    {
      window->sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (window->filter_model));
      if (xfconf_channel_get_bool (window->channel, "/sort-by-frecency", FALSE))
        gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (window->sort_model), xfce_appfinder_window_sort_items_frecency, window->entry, NULL);
      else
        gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (window->sort_model), xfce_appfinder_window_sort_items, window->entry, NULL);
    }

  if (icon_view)
    {
      window->view = view = gtk_icon_view_new_with_model (
        xfce_appfinder_should_sort_icon_view () ?
          window->sort_model :
          window->filter_model);

      gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (view), GTK_SELECTION_SINGLE);
      gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (view), XFCE_APPFINDER_MODEL_COLUMN_PIXBUF);
      gtk_icon_view_set_text_column (GTK_ICON_VIEW (view), XFCE_APPFINDER_MODEL_COLUMN_TITLE);
      gtk_icon_view_set_tooltip_column (GTK_ICON_VIEW (view), XFCE_APPFINDER_MODEL_COLUMN_TOOLTIP);
      gtk_icon_view_set_row_spacing (GTK_ICON_VIEW (view), 0);
      xfce_appfinder_window_set_item_width (window);

      g_signal_connect_swapped (G_OBJECT (view), "selection-changed",
          G_CALLBACK (xfce_appfinder_window_item_changed), window);
      g_signal_connect_swapped (G_OBJECT (view), "item-activated",
          G_CALLBACK (xfce_appfinder_window_row_activated), window);
    }
  else
    {
      window->view = view = gtk_tree_view_new_with_model (window->sort_model);
      gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);
      gtk_tree_view_set_enable_search (GTK_TREE_VIEW (view), FALSE);
      gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (view), XFCE_APPFINDER_MODEL_COLUMN_TOOLTIP);
      g_signal_connect_swapped (G_OBJECT (view), "row-activated",
          G_CALLBACK (xfce_appfinder_window_row_activated), window);
      g_signal_connect_swapped (G_OBJECT (view), "start-interactive-search",
          G_CALLBACK (gtk_widget_grab_focus), window->entry);
      g_signal_connect (G_OBJECT (view), "key-press-event",
           G_CALLBACK (xfce_appfinder_window_treeview_key_press_event), window);

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
      gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
      g_signal_connect_swapped (G_OBJECT (selection), "changed",
          G_CALLBACK (xfce_appfinder_window_item_changed), window);

      column = gtk_tree_view_column_new ();
      gtk_tree_view_column_set_spacing (column, 2);
      gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
      gtk_tree_view_append_column (GTK_TREE_VIEW (view), GTK_TREE_VIEW_COLUMN (column));

      renderer = gtk_cell_renderer_pixbuf_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, FALSE);
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer,
                                      "surface", XFCE_APPFINDER_MODEL_COLUMN_ICON, NULL);

      renderer = gtk_cell_renderer_text_new ();
      g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, TRUE);
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer,
                                      "markup", XFCE_APPFINDER_MODEL_COLUMN_ABSTRACT, NULL);
    }

  gtk_drag_source_set (view, GDK_BUTTON1_MASK, target_list,
                       G_N_ELEMENTS (target_list), GDK_ACTION_COPY);
  g_signal_connect (G_OBJECT (view), "drag-begin",
      G_CALLBACK (xfce_appfinder_window_drag_begin), window);
  g_signal_connect (G_OBJECT (view), "drag-data-get",
      G_CALLBACK (xfce_appfinder_window_drag_data_get), window);
  g_signal_connect (G_OBJECT (view), "popup-menu",
      G_CALLBACK (xfce_appfinder_window_popup_menu), window);
  g_signal_connect (G_OBJECT (view), "button-press-event",
      G_CALLBACK (xfce_appfinder_window_view_button_press_event), window);
  gtk_container_add (GTK_CONTAINER (window->viewscroll), view);
  gtk_widget_show (view);
}



static gboolean
xfce_appfinder_window_view_get_selected (XfceAppfinderWindow  *window,
                                         GtkTreeModel        **model,
                                         GtkTreeIter          *iter)
{
  GtkTreeSelection *selection;
  gboolean          have_iter;
  GList            *items;

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_WINDOW (window), FALSE);
  appfinder_return_val_if_fail (model != NULL, FALSE);
  appfinder_return_val_if_fail (iter != NULL, FALSE);

  if (GTK_IS_TREE_VIEW (window->view))
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->view));
      have_iter = gtk_tree_selection_get_selected (selection, model, iter);
    }
  else
    {
      items = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (window->view));
      appfinder_assert (g_list_length (items) <= 1);
      if (items != NULL)
        {
          *model = gtk_icon_view_get_model (GTK_ICON_VIEW (window->view));
          have_iter = gtk_tree_model_get_iter (*model, iter, items->data);

          gtk_tree_path_free (items->data);
          g_list_free (items);
        }
      else
        {
          have_iter = FALSE;
        }
    }

  return have_iter;
}



static gboolean
xfce_appfinder_window_view_get_selected_path (XfceAppfinderWindow  *window,
                                              GtkTreePath         **path)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;

  if (!xfce_appfinder_window_view_get_selected (window, &model, &iter))
    return FALSE;

  *path = gtk_tree_model_get_path (model, &iter);

  return (*path != NULL);
}



static void
xfce_appfinder_window_popup_menu_toggle_bookmark (GtkWidget           *mi,
                                                  XfceAppfinderWindow *window)
{
  const gchar  *uri;
  GFile        *gfile;
  gchar        *desktop_id;
  GtkWidget    *menu = gtk_widget_get_parent (mi);
  GtkTreeModel *model;
  GError       *error = NULL;

  uri = g_object_get_data (G_OBJECT (menu), "uri");
  if (uri != NULL)
    {
      gfile = g_file_new_for_uri (uri);
      desktop_id = g_file_get_basename (gfile);
      g_object_unref (G_OBJECT (gfile));

      /* toggle bookmarks */
      model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (window->filter_model));
      xfce_appfinder_model_bookmark_toggle (XFCE_APPFINDER_MODEL (model), desktop_id, &error);

      g_free (desktop_id);

      if (G_UNLIKELY (error != NULL))
        {
          g_printerr ("%s: failed to save bookmarks: %s\n", G_LOG_DOMAIN, error->message);
          g_error_free (error);
        }
    }
}



static void
xfce_appfinder_window_popup_menu_execute (GtkWidget           *mi,
                                          XfceAppfinderWindow *window)
{
  xfce_appfinder_window_execute (window, FALSE);
}



static void
xfce_appfinder_window_popup_menu_edit (GtkWidget           *mi,
                                       XfceAppfinderWindow *window)
{
  GError      *error = NULL;
  gchar       *cmd;
  const gchar *uri;
  GtkWidget   *menu = gtk_widget_get_parent (mi);

  appfinder_return_if_fail (GTK_IS_MENU (menu));
  appfinder_return_if_fail (XFCE_IS_APPFINDER_WINDOW (window));

  uri = g_object_get_data (G_OBJECT (menu), "uri");
  if (G_LIKELY (uri != NULL))
    {
      cmd = g_strdup_printf ("exo-desktop-item-edit --xid=0x%x '%s'",
                             APPFINDER_WIDGET_XID (window), uri);
      if (!g_spawn_command_line_async (cmd, &error))
        {
          xfce_dialog_show_error (GTK_WINDOW (window), error,
                                  _("Failed to launch desktop item editor"));
          g_error_free (error);
        }
      g_free (cmd);
    }
}



static void
xfce_appfinder_window_popup_menu_revert (GtkWidget           *mi,
                                         XfceAppfinderWindow *window)
{
  const gchar *uri;
  const gchar *name;
  GError      *error;
  GtkWidget   *menu = gtk_widget_get_parent (mi);

  appfinder_return_if_fail (GTK_IS_MENU (menu));
  appfinder_return_if_fail (XFCE_IS_APPFINDER_WINDOW (window));

  name = g_object_get_data (G_OBJECT (menu), "name");
  if (name == NULL)
    return;

  if (xfce_dialog_confirm (GTK_WINDOW (window), XFCE_APPFINDER_ICON_NAME_REVERT, NULL,
          _("This will permanently remove the custom desktop file from your home directory."),
          _("Are you sure you want to revert \"%s\"?"), name))
    {
      uri = g_object_get_data (G_OBJECT (menu), "uri");
      if (uri != NULL)
        {
          if (g_unlink (uri + 7) == -1)
            {
              error = g_error_new (G_FILE_ERROR, g_file_error_from_errno (errno),
                                   "%s: %s", uri + 7, g_strerror (errno));
              xfce_dialog_show_error (GTK_WINDOW (window), error,
                                      _("Failed to remove desktop file"));
              g_error_free (error);
            }
        }
    }
}



static void
xfce_appfinder_window_popup_menu_hide (GtkWidget           *mi,
                                       XfceAppfinderWindow *window)
{
  const gchar  *uri;
  const gchar  *name;
  GtkWidget    *menu = gtk_widget_get_parent (mi);
  gchar        *path;
  gchar        *message;
  gchar       **dirs;
  guint         i;
  const gchar  *relpath;
  XfceRc       *rc;

  appfinder_return_if_fail (GTK_IS_MENU (menu));
  appfinder_return_if_fail (XFCE_IS_APPFINDER_WINDOW (window));

  name = g_object_get_data (G_OBJECT (menu), "name");
  if (name == NULL)
    return;

  path = xfce_resource_save_location (XFCE_RESOURCE_DATA, "applications/", FALSE);
  /* I18N: the first %s will be replace with users' applications directory, the
   * second with Hidden=true */
  message = g_strdup_printf (_("To unhide the item you have to manually "
                               "remove the desktop file from \"%s\" or "
                               "open the file in the same directory and "
                               "remove the line \"%s\"."), path, "Hidden=true");

  if (xfce_dialog_confirm (GTK_WINDOW (window), NULL, _("_Hide"), message,
          _("Are you sure you want to hide \"%s\"?"), name))
    {
      uri = g_object_get_data (G_OBJECT (menu), "uri");
      if (uri != NULL)
        {
          /* lookup the correct relative path */
          dirs = xfce_resource_lookup_all (XFCE_RESOURCE_DATA, "applications/");
          for (i = 0; dirs[i] != NULL; i++)
            {
              if (g_str_has_prefix (uri + 7, dirs[i]))
                {
                  /* relative path to XFCE_RESOURCE_DATA */
                  relpath = uri + 7 + strlen (dirs[i]) - 13;

                  /* xfcerc can handle everything else */
                  rc = xfce_rc_config_open (XFCE_RESOURCE_DATA, relpath, FALSE);
                  xfce_rc_set_group (rc, G_KEY_FILE_DESKTOP_GROUP);
                  xfce_rc_write_bool_entry (rc, G_KEY_FILE_DESKTOP_KEY_HIDDEN, TRUE);
                  xfce_rc_close (rc);

                  break;
                }
            }
          g_strfreev (dirs);
        }
    }

  g_free (path);
  g_free (message);
}



static gboolean
xfce_appfinder_window_popup_menu (GtkWidget           *view,
                                  XfceAppfinderWindow *window)
{
  GtkWidget    *menu;
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gchar        *title;
  gchar        *uri;
  GtkWidget    *mi;
  GtkWidget    *image;
  GtkWidget    *box;
  GtkWidget    *label;
  gchar        *path;
  gboolean      uri_is_local;
  gboolean      is_bookmark;

  if (xfce_appfinder_window_view_get_selected (window, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          XFCE_APPFINDER_MODEL_COLUMN_TITLE, &title,
                          XFCE_APPFINDER_MODEL_COLUMN_URI, &uri,
                          XFCE_APPFINDER_MODEL_COLUMN_BOOKMARK, &is_bookmark,
                          -1);

      /* custom command don't have an uri */
      if (uri == NULL)
        {
          g_free (title);
          return FALSE;
        }

      uri_is_local = g_str_has_prefix (uri, XFCE_APPFINDER_LOCAL_PREFIX);

      menu = gtk_menu_new ();
      gtk_menu_set_reserve_toggle_size (GTK_MENU (menu), FALSE);
      g_object_set_data_full (G_OBJECT (menu), "uri", uri, g_free);
      g_object_set_data_full (G_OBJECT (menu), "name", title, g_free);
      g_object_set_data (G_OBJECT (menu), "model", model);
      g_signal_connect (G_OBJECT (menu), "selection-done",
          G_CALLBACK (gtk_widget_destroy), NULL);

      /* Title */
      mi = gtk_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_set_sensitive (mi, FALSE);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      label = gtk_label_new (title);
      image = gtk_image_new_from_icon_name ("", GTK_ICON_SIZE_MENU);
      gtk_container_add (GTK_CONTAINER (box), image);
      gtk_container_add (GTK_CONTAINER (box), label);
      gtk_container_add (GTK_CONTAINER (mi), box);
      gtk_widget_show_all (mi);

      /* Separator */
      mi = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_show (mi);

      /* Add/Remove from Bookmarks */
      mi = gtk_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      g_signal_connect (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_appfinder_window_popup_menu_toggle_bookmark), window);

      if (is_bookmark)
        image = gtk_image_new_from_icon_name (XFCE_APPFINDER_ICON_NAME_REMOVE, GTK_ICON_SIZE_MENU);
      else
        image = gtk_image_new_from_icon_name (XFCE_APPFINDER_ICON_NAME_BOOKMARK_NEW, GTK_ICON_SIZE_MENU);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      label = gtk_label_new (is_bookmark ? _("Remove From Bookmarks") : _("Add to Bookmarks"));
      gtk_container_add (GTK_CONTAINER (box), image);
      gtk_container_add (GTK_CONTAINER (box), label);
      gtk_container_add (GTK_CONTAINER (mi), box);
      gtk_widget_show_all (mi);

      /* Separator */
      mi = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_show (mi);

      /* Launch */
      mi = gtk_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      g_signal_connect (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_appfinder_window_popup_menu_execute), window);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      label = gtk_label_new_with_mnemonic (_("La_unch"));
      image = gtk_image_new_from_icon_name (XFCE_APPFINDER_ICON_NAME_EXECUTE, GTK_ICON_SIZE_MENU);
      gtk_container_add (GTK_CONTAINER (box), image);
      gtk_container_add (GTK_CONTAINER (box), label);
      gtk_container_add (GTK_CONTAINER (mi), box);
      gtk_widget_show_all (mi);

      /* Edit */
      mi = gtk_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      g_signal_connect (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_appfinder_window_popup_menu_edit), window);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      label = gtk_label_new_with_mnemonic (_("_Edit"));
      image = gtk_image_new_from_icon_name (XFCE_APPFINDER_ICON_NAME_EDIT, GTK_ICON_SIZE_MENU);
      gtk_container_add (GTK_CONTAINER (box), image);
      gtk_container_add (GTK_CONTAINER (box), label);
      gtk_container_add (GTK_CONTAINER (mi), box);
      gtk_widget_show_all (mi);

      /* Revert */
      mi = gtk_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      g_signal_connect (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_appfinder_window_popup_menu_revert), window);
      path = xfce_resource_save_location (XFCE_RESOURCE_DATA, "applications/", FALSE);
      gtk_widget_set_sensitive (mi, uri_is_local && g_str_has_prefix (uri + 7, path));
      g_free (path);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      label = gtk_label_new_with_mnemonic (_("_Revert"));
      image = gtk_image_new_from_icon_name (XFCE_APPFINDER_ICON_NAME_REVERT, GTK_ICON_SIZE_MENU);
      gtk_container_add (GTK_CONTAINER (box), image);
      gtk_container_add (GTK_CONTAINER (box), label);
      gtk_container_add (GTK_CONTAINER (mi), box);
      gtk_widget_show_all (mi);

      /* Hide */
      mi = gtk_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_set_sensitive (mi, uri_is_local);
      g_signal_connect (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_appfinder_window_popup_menu_hide), window);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      label = gtk_label_new_with_mnemonic (_("_Hide"));
      image = gtk_image_new_from_icon_name ("", GTK_ICON_SIZE_MENU);
      gtk_container_add (GTK_CONTAINER (box), image);
      gtk_container_add (GTK_CONTAINER (box), label);
      gtk_container_add (GTK_CONTAINER (mi), box);
      gtk_widget_show_all (mi);

      gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);

      return TRUE;
    }

  return FALSE;
}



static void
xfce_appfinder_window_update_image (XfceAppfinderWindow *window,
                                    cairo_surface_t     *surface)
{
  gint     scale_factor;
  gboolean free_surface = FALSE;

  if (surface == NULL)
    {
      scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (window));
      surface = gdk_cairo_surface_create_from_pixbuf (window->icon_find, scale_factor,
                                                      gtk_widget_get_window (GTK_WIDGET (window)));
      free_surface = TRUE;
    }

    gtk_image_set_from_surface (GTK_IMAGE (window->image), surface);

    if (free_surface)
      cairo_surface_destroy (surface);
}



static void
xfce_appfinder_window_set_padding (GtkWidget *entry,
                                   GtkWidget *align)
{
  gint          padding;
  GtkAllocation alloc;

  /* 48 is the icon size of XFCE_APPFINDER_ICON_SIZE_48 */
  gtk_widget_get_allocation (entry, &alloc);
  padding = (48 - alloc.height) / 2;
  // gtk_alignment_set_padding (GTK_ALIGNMENT (align), MAX (0, padding), 0, 0, 0);
  gtk_widget_set_margin_top (align, MAX (0, padding));
}



static gboolean
xfce_appfinder_window_completion_match_func (GtkEntryCompletion *completion,
                                             const gchar        *key,
                                             GtkTreeIter        *iter,
                                             gpointer            data)
{
  const gchar *text;

  XfceAppfinderWindow *window = XFCE_APPFINDER_WINDOW (data);

  appfinder_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), FALSE);
  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_WINDOW (data), FALSE);
  appfinder_return_val_if_fail (GTK_TREE_MODEL (window->model)
      == gtk_entry_completion_get_model (completion), FALSE);

  /* don't use the casefolded key generated by gtk */
  text = gtk_entry_get_text (GTK_ENTRY (window->entry));

  return xfce_appfinder_model_get_visible_command (window->model, iter, text);
}



static gboolean
xfce_appfinder_window_entry_changed_idle (gpointer data)
{
  XfceAppfinderWindow *window = XFCE_APPFINDER_WINDOW (data);
  const gchar         *text;
  cairo_surface_t     *surface;
  gchar               *normalized;
  GtkTreePath         *path;
  GtkTreeSelection    *selection;

  text = gtk_entry_get_text (GTK_ENTRY (window->entry));

  if (gtk_widget_get_visible (window->paned))
    {
      g_free (window->filter_text);

      if (IS_STRING (text))
        {
          normalized = g_utf8_normalize (text, -1, G_NORMALIZE_ALL);
          window->filter_text = g_utf8_casefold (normalized, -1);
          g_free (normalized);
        }
      else
        {
          window->filter_text = NULL;
        }

      APPFINDER_DEBUG ("refilter entry");

      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (window->filter_model));
      APPFINDER_DEBUG ("FILTER TEXT: %s\n", window->filter_text);

      if (GTK_IS_TREE_VIEW (window->view))
        {
          gtk_tree_view_scroll_to_point (GTK_TREE_VIEW (window->view), 0, 0);

          if (window->filter_text == NULL)
            {
              /* if filter is empty, clear selection */
              selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->view));
              gtk_tree_selection_unselect_all (selection);

              /* reset cursor */
              path = gtk_tree_path_new_first ();
              gtk_tree_view_set_cursor (GTK_TREE_VIEW (window->view), path, NULL, FALSE);
              gtk_tree_path_free (path);
            }
          else if (gtk_tree_view_get_visible_range (GTK_TREE_VIEW (window->view), &path, NULL))
            {
              /* otherwise select the first match */
              gtk_tree_view_set_cursor (GTK_TREE_VIEW (window->view), path, NULL, FALSE);
              gtk_tree_path_free (path);
            }
        }
      else
        {
          if (window->filter_text == NULL)
            {
              /* if filter is empty, clear selection */
              gtk_icon_view_unselect_all (GTK_ICON_VIEW (window->view));
            }
          else if (gtk_icon_view_get_visible_range (GTK_ICON_VIEW (window->view), &path, NULL))
            {
              /* otherwise select the first match */
              gtk_icon_view_select_path (GTK_ICON_VIEW (window->view), path);
              gtk_icon_view_set_cursor (GTK_ICON_VIEW (window->view), path, NULL, FALSE);
              gtk_tree_path_free (path);
            }
        }
    }
  else
    {
      gtk_widget_set_sensitive (window->button_launch, IS_STRING (text));

      /* Clear entry's error icon and tooltip */
      gtk_entry_set_icon_from_icon_name (GTK_ENTRY (window->entry), GTK_ENTRY_ICON_PRIMARY, NULL);
      gtk_entry_set_icon_tooltip_text (GTK_ENTRY (window->entry), GTK_ENTRY_ICON_PRIMARY, NULL);

      surface = xfce_appfinder_model_get_icon_for_command (window->model, text);
      xfce_appfinder_window_update_image (window, surface);
      if (surface != NULL)
        cairo_surface_destroy (surface);
    }


  return FALSE;
}



static void
xfce_appfinder_window_entry_changed_idle_destroyed (gpointer data)
{
  XFCE_APPFINDER_WINDOW (data)->idle_entry_changed_id = 0;
}



static void
xfce_appfinder_window_entry_changed (XfceAppfinderWindow *window)
{
  if (window->idle_entry_changed_id != 0)
    g_source_remove (window->idle_entry_changed_id);

  window->idle_entry_changed_id =
      gdk_threads_add_idle_full (G_PRIORITY_DEFAULT, xfce_appfinder_window_entry_changed_idle,
                       window, xfce_appfinder_window_entry_changed_idle_destroyed);
}



static void
xfce_appfinder_window_entry_activate (GtkEditable         *entry,
                                      XfceAppfinderWindow *window)
{
  GList            *selected_items;
  GtkTreeSelection *selection;
  gboolean          has_selection = FALSE;

  if (gtk_widget_get_visible (window->paned))
    {
      if (GTK_IS_TREE_VIEW (window->view))
        {
          selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->view));
          has_selection = gtk_tree_selection_count_selected_rows (selection) > 0;
        }
      else
        {
          selected_items = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (window->view));
          has_selection = (selected_items != NULL);
          g_list_free_full (selected_items, (GDestroyNotify) gtk_tree_path_free);
        }

      if (has_selection)
        xfce_appfinder_window_execute (window, TRUE);
    }
  else if (gtk_widget_get_sensitive (window->button_launch))
    {
      gtk_button_clicked (GTK_BUTTON (window->button_launch));
    }
}



static gboolean
xfce_appfinder_window_pointer_is_grabbed (GtkWidget *widget)
{
  GdkSeat          *seat;
  GdkDevice        *pointer;
  GdkDisplay       *display;

  display = gtk_widget_get_display (widget);
  seat = gdk_display_get_default_seat (display);
  pointer = gdk_seat_get_pointer (seat);
  return gdk_display_device_is_grabbed (display, pointer);
}



static gboolean
xfce_appfinder_window_entry_key_press_event (GtkWidget           *entry,
                                             GdkEventKey         *event,
                                             XfceAppfinderWindow *window)
{
  gboolean          expand;
  gboolean          is_expanded;
  GtkTreePath      *path;

  if (event->keyval == GDK_KEY_Tab &&
      !gtk_widget_get_visible (window->paned) &&
      xfce_appfinder_window_pointer_is_grabbed (entry))
    {
      /* don't tab to the close button */
      return TRUE;
    }

  if (event->keyval == GDK_KEY_Up ||
      event->keyval == GDK_KEY_Down)
    {
      expand = (event->keyval == GDK_KEY_Down);
      is_expanded = gtk_widget_get_visible (window->paned);

      if (is_expanded != expand)
        {
          /* don't break entry completion navigation in collapsed mode */
          if (!is_expanded && xfce_appfinder_window_pointer_is_grabbed (entry))
            {
              /* window is still collapsed and the pointer is grabbed
               * by the popup menu, do nothing with the event */
              return FALSE;
            }

          xfce_appfinder_window_set_expanded (window, expand);
          return TRUE;
        }

      /* The first item is usually selected, so we should jump to 2nd one
       * when down key is pressed */
      if (is_expanded && expand &&
          GTK_IS_TREE_VIEW (window->view) && /* does not make sense for icon view */
          xfce_appfinder_window_view_get_selected_path (window, &path))
        {
          gtk_tree_path_next (path);
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (window->view), path, NULL, FALSE);
          gtk_tree_path_free (path);
        }
    }

  return FALSE;
}



static void
xfce_appfinder_window_drag_begin (GtkWidget           *widget,
                                  GdkDragContext      *drag_context,
                                  XfceAppfinderWindow *window)
{
  GtkTreeModel    *model;
  GtkTreeIter      iter;
  cairo_surface_t *surface;

  if (xfce_appfinder_window_view_get_selected (window, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, XFCE_APPFINDER_MODEL_COLUMN_ICON_LARGE, &surface, -1);
      if (G_LIKELY (surface != NULL))
        {
          gtk_drag_set_icon_surface (drag_context, surface);
          cairo_surface_destroy (surface);
        }
    }
  else
    {
      gtk_drag_set_icon_name (drag_context, XFCE_APPFINDER_ICON_NAME_DIALOG_ERROR, 0, 0);
    }
}



static void
xfce_appfinder_window_drag_data_get (GtkWidget           *widget,
                                     GdkDragContext      *drag_context,
                                     GtkSelectionData    *data,
                                     guint                info,
                                     guint                drag_time,
                                     XfceAppfinderWindow *window)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gchar        *uris[2];

  if (xfce_appfinder_window_view_get_selected (window, &model, &iter))
    {
      uris[1] = NULL;
      gtk_tree_model_get (model, &iter, XFCE_APPFINDER_MODEL_COLUMN_URI, &uris[0], -1);
      gtk_selection_data_set_uris (data, uris);
      g_free (uris[0]);
    }
}



static gboolean
xfce_appfinder_window_treeview_key_press_event (GtkWidget           *widget,
                                                GdkEventKey         *event,
                                                XfceAppfinderWindow *window)
{
  GdkEvent     ev;
  GtkTreePath *path;

  if (widget == window->view)
    {
      if (event->keyval == GDK_KEY_Control_L ||
          event->keyval == GDK_KEY_Control_R ||
          event->keyval == GDK_KEY_Shift_L ||
          event->keyval == GDK_KEY_Shift_R ||
          event->keyval == GDK_KEY_Alt_L ||
          event->keyval == GDK_KEY_Alt_R ||
          event->keyval == GDK_KEY_Right ||
          event->keyval == GDK_KEY_Down)
          return FALSE;

      if (event->keyval == GDK_KEY_Left)
        {
          if (gtk_widget_get_realized (window->sidepane))
            gtk_widget_grab_focus (window->sidepane);
          return TRUE;
        }

      if (event->keyval == GDK_KEY_Up)
        {
          if (xfce_appfinder_window_view_get_selected_path (window, &path))
            {
              if (!gtk_tree_path_prev (path))
                gtk_widget_grab_focus (window->entry);
              gtk_tree_path_free (path);
            }

          return FALSE;
        }

      gtk_widget_grab_focus (window->entry);

      ev.key = *event;
      gtk_propagate_event (window->entry, &ev);

      return TRUE;
    }

  if (widget == window->sidepane)
    {
      if (event->keyval == GDK_KEY_Right)
        {
          gtk_widget_grab_focus (window->view);
          return TRUE;
        }
    }

  return FALSE;
}



static void
xfce_appfinder_window_entry_icon_released (GtkEntry             *entry,
                                           GtkEntryIconPosition  icon_pos,
                                           GdkEvent             *event,
                                           XfceAppfinderWindow  *window)
{
  if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
    xfce_appfinder_window_set_expanded (window, !gtk_widget_get_visible (window->paned));
}



static void
xfce_appfinder_window_category_changed (GtkTreeSelection    *selection,
                                        XfceAppfinderWindow *window)
{
  GtkTreeIter          iter;
  GtkTreeModel        *model;
  GarconMenuDirectory *category;
  gchar               *name;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          XFCE_APPFINDER_CATEGORY_MODEL_COLUMN_DIRECTORY, &category,
                          XFCE_APPFINDER_CATEGORY_MODEL_COLUMN_NAME, &name, -1);

      if (window->filter_category != category)
        {
          if (window->filter_category != NULL)
            g_object_unref (G_OBJECT (window->filter_category));

          if (category == NULL)
            window->filter_category = NULL;
          else
            window->filter_category = GARCON_MENU_DIRECTORY (g_object_ref (G_OBJECT (category)));

          APPFINDER_DEBUG ("refilter category");

          /* update visible items */
          if (GTK_IS_TREE_VIEW (window->view))
            model = gtk_tree_view_get_model (GTK_TREE_VIEW (window->view));
          else
            model = gtk_icon_view_get_model (GTK_ICON_VIEW (window->view));
          gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (window->filter_model));

          /* store last category */
          if (xfconf_channel_get_bool (window->channel, "/remember-category", FALSE))
            xfconf_channel_set_string (window->channel, "/last/category", name);
        }

      g_free (name);
      if (category != NULL)
        g_object_unref (G_OBJECT (category));
    }
}



static void
xfce_appfinder_window_category_set_categories (XfceAppfinderModel  *signal_from_model,
                                               XfceAppfinderWindow *window)
{
  GSList           *categories;
  GtkTreePath      *path;
  gchar            *name = NULL;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GtkTreeSelection *selection;

  appfinder_return_if_fail (GTK_IS_TREE_VIEW (window->sidepane));

  if (signal_from_model != NULL)
    {
      /* reload from the model, make sure we restore the selected category */
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->sidepane));
      if (gtk_tree_selection_get_selected (selection, &model, &iter))
        gtk_tree_model_get (model, &iter, XFCE_APPFINDER_CATEGORY_MODEL_COLUMN_NAME, &name, -1);
    }

  /* update the categories */
  categories = xfce_appfinder_model_get_categories (window->model);
  if (categories != NULL)
    xfce_appfinder_category_model_set_categories (window->category_model, categories);
  g_slist_free (categories);

  if (name == NULL && xfconf_channel_get_bool (window->channel, "/remember-category", FALSE))
    name = xfconf_channel_get_string (window->channel, "/last/category", NULL);

  path = xfce_appfinder_category_model_find_category (window->category_model, name);
  if (path != NULL)
    {
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (window->sidepane), path, NULL, FALSE);
      gtk_tree_path_free (path);
    }

  g_free (name);
}



static void
xfce_appfinder_window_preferences (GtkWidget           *button,
                                   XfceAppfinderWindow *window)
{
  appfinder_return_if_fail (GTK_IS_WIDGET (button));

  /* preload the actions, to make sure there are default values */
  if (window->actions == NULL)
    window->actions = xfce_appfinder_actions_get ();

  xfce_appfinder_preferences_show (gtk_widget_get_screen (button));
}



static void
xfce_appfinder_window_property_changed (XfconfChannel       *channel,
                                        const gchar         *prop,
                                        const GValue        *value,
                                        XfceAppfinderWindow *window)
{
  appfinder_return_if_fail (XFCE_IS_APPFINDER_WINDOW (window));
  appfinder_return_if_fail (window->channel == channel);

  if (g_strcmp0 (prop, "/icon-view") == 0)
    {
      xfce_appfinder_window_view (window);
    }
  else if (g_strcmp0 (prop, "/item-icon-size") == 0)
    {
      if (GTK_IS_ICON_VIEW (window->view))
        xfce_appfinder_window_set_item_width (window);
    }
  else if (g_strcmp0 (prop, "/text-beside-icons") == 0)
    {
      if (GTK_IS_ICON_VIEW (window->view))
        xfce_appfinder_window_set_item_width (window);
    }
  else if (g_strcmp0 (prop, "/hide-category-pane") == 0)
    {
      gboolean hide = g_value_get_boolean (value);
      gtk_widget_set_visible (gtk_widget_get_parent (window->sidepane), !hide);

      if (hide) {
        GtkTreePath *path = gtk_tree_path_new_from_string ("0");
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (window->sidepane), path, NULL, FALSE);
        gtk_tree_path_free (path);
      }
    }
  else if (g_strcmp0 (prop, "/hide-window-decorations") == 0)
    {
      gboolean hide = g_value_get_boolean (value);
      gtk_window_set_decorated (GTK_WINDOW (window), !hide);
    }
}



static gboolean
xfce_appfinder_window_item_visible (GtkTreeModel *model,
                                    GtkTreeIter  *iter,
                                    gpointer      data)
{
  XfceAppfinderWindow *window = XFCE_APPFINDER_WINDOW (data);
  const gchar *filter_string = gtk_entry_get_text (GTK_ENTRY (window->entry));

  return xfce_appfinder_model_get_visible (XFCE_APPFINDER_MODEL (model),
                                           iter,
                                           window->filter_category,
                                           filter_string,
                                           window->filter_text);
}



static void
xfce_appfinder_window_item_changed (XfceAppfinderWindow *window)
{
  GtkTreeIter       iter;
  GtkTreeModel     *model;
  gboolean          can_launch;
  cairo_surface_t  *surface;

  if (gtk_widget_get_visible (window->paned))
    {
      can_launch = xfce_appfinder_window_view_get_selected (window, &model, &iter);
      gtk_widget_set_sensitive (window->button_launch, can_launch);

      if (can_launch)
        {
          gtk_tree_model_get (model, &iter, XFCE_APPFINDER_MODEL_COLUMN_ICON_LARGE, &surface, -1);
          if (G_LIKELY (surface != NULL))
            {
              xfce_appfinder_window_update_image (window, surface);
              cairo_surface_destroy (surface);
            }
        }
      else
        {
          xfce_appfinder_window_update_image (window, NULL);
        }
    }
}



static void
xfce_appfinder_window_row_activated (XfceAppfinderWindow *window)
{
  if (gtk_widget_get_sensitive (window->button_launch))
    gtk_button_clicked (GTK_BUTTON (window->button_launch));
}



static void
xfce_appfinder_window_icon_theme_changed (XfceAppfinderWindow *window)
{
  gint scale_factor;

  appfinder_return_if_fail (XFCE_IS_APPFINDER_WINDOW (window));

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (window));

  if (window->icon_find != NULL)
    g_object_unref (G_OBJECT (window->icon_find));
  window->icon_find = xfce_appfinder_model_load_pixbuf (XFCE_APPFINDER_ICON_NAME_FIND,
                                                        XFCE_APPFINDER_ICON_SIZE_DEFAULT_ITEM,
                                                        scale_factor);

  /* drop cached pixbufs and surfaces */
  if (G_LIKELY (window->model != NULL))
    {
      g_object_set (window->model, "scale-factor", scale_factor, NULL);
      xfce_appfinder_model_icon_theme_changed (window->model);
    }

  if (G_LIKELY (window->category_model != NULL))
    {
      g_object_set (window->category_model, "scale-factor", scale_factor, NULL);
      xfce_appfinder_category_model_icon_theme_changed (window->category_model);
    }

  /* update state */
  xfce_appfinder_window_entry_changed (window);
  xfce_appfinder_window_item_changed (window);
}



static gboolean
xfce_appfinder_window_execute_command (const gchar          *text,
                                       GdkScreen            *screen,
                                       XfceAppfinderWindow  *window,
                                       gboolean              only_custom_cmd,
                                       gboolean             *save_cmd,
                                       GError              **error)
{
  gboolean  succeed = FALSE;
  gchar    *action_cmd = NULL;
  gchar    *expanded;

  appfinder_return_val_if_fail (error != NULL && *error == NULL, FALSE);
  appfinder_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  if (!IS_STRING (text))
    return TRUE;

  if (window->actions == NULL)
    window->actions = xfce_appfinder_actions_get ();

  /* try to match a custom action */
  action_cmd = xfce_appfinder_actions_execute (window->actions, text, save_cmd, error);
  if (*error != NULL)
    return FALSE;
  else if (action_cmd != NULL)
    text = action_cmd;
  else if (only_custom_cmd)
    return FALSE;

  if (IS_STRING (text))
    {
      gchar **argv = NULL;
      gchar **envp;

      /* expand variables */
      expanded = xfce_expand_variables (text, NULL);

      /* spawn the command */
      APPFINDER_DEBUG ("spawn \"%s\"", expanded);

      envp = xfce_appfinder_parse_envp (&expanded);

      if (G_LIKELY (g_shell_parse_argv (expanded, NULL, &argv, error)))
        {
#if LIBXFCE4UI_CHECK_VERSION (4, 15, 6)
          succeed = xfce_spawn (screen, NULL, argv, envp,
                                G_SPAWN_SEARCH_PATH, FALSE,
                                gtk_get_current_event_time (), NULL,
                                TRUE, error);
#else
          succeed = xfce_spawn_on_screen (screen, NULL, argv, envp,
                                          G_SPAWN_SEARCH_PATH, FALSE,
                                          gtk_get_current_event_time (), NULL,
                                          error);
#endif
        }

      g_strfreev (argv);
      g_strfreev (envp);

      g_free (expanded);
    }

  g_free (action_cmd);

  return succeed;
}



static void
xfce_appfinder_window_launch_clicked (XfceAppfinderWindow *window)
{
  xfce_appfinder_window_execute (window, TRUE);
}



static void
xfce_appfinder_window_execute (XfceAppfinderWindow *window,
                               gboolean             close_on_succeed)
{
  GtkTreeModel *model, *child_model;
  GtkTreeIter   iter, child_iter;
  GError       *error = NULL;
  gboolean      result = FALSE;
  GdkScreen    *screen;
  const gchar  *text;
  gchar        *cmd = NULL;
  gboolean      regular_command = FALSE;
  gboolean      save_cmd;
  gboolean      only_custom_cmd = FALSE;
  const gchar  *uri;

  screen = gtk_window_get_screen (GTK_WINDOW (window));
  if (gtk_widget_get_visible (window->paned))
    {
      if (!gtk_widget_get_sensitive (window->button_launch))
        {
          only_custom_cmd = TRUE;
          goto entry_execute;
        }

      if (xfce_appfinder_window_view_get_selected (window, &model, &iter))
        {
          child_model = model;
          gtk_tree_model_get (model, &iter, XFCE_APPFINDER_MODEL_COLUMN_URI, &uri, -1);

          if (GTK_IS_TREE_MODEL_SORT (model) || xfce_appfinder_should_sort_icon_view ())
            {
              gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model), &child_iter, &iter);
              iter = child_iter;
              child_model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (model));
            }

          gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (child_model), &child_iter, &iter);
          result = xfce_appfinder_model_execute (window->model, &child_iter, screen, &regular_command, &error);

          xfce_appfinder_window_update_frecency (window, model, uri);
          if (!result && regular_command)
            {
              gtk_tree_model_get (model, &child_iter, XFCE_APPFINDER_MODEL_COLUMN_COMMAND, &cmd, -1);
              result = xfce_appfinder_window_execute_command (cmd, screen, window, FALSE, NULL, &error);
              g_free (cmd);
            }
        }
    }
  else
    {
      if (!gtk_widget_get_sensitive (window->button_launch))
        return;

      entry_execute:

      text = gtk_entry_get_text (GTK_ENTRY (window->entry));
      save_cmd = TRUE;

      if (xfce_appfinder_window_execute_command (text, screen, window, only_custom_cmd, &save_cmd, &error))
        {
          if (save_cmd)
            result = xfce_appfinder_model_save_command (window->model, text, &error);
          else
            result = TRUE;
        }
    }

  if (!only_custom_cmd)
    {
      gtk_entry_set_icon_from_icon_name (GTK_ENTRY (window->entry), GTK_ENTRY_ICON_PRIMARY,
                                     result ? NULL : XFCE_APPFINDER_ICON_NAME_DIALOG_ERROR);
      gtk_entry_set_icon_tooltip_text (GTK_ENTRY (window->entry), GTK_ENTRY_ICON_PRIMARY,
                                       error != NULL ? error->message : NULL);
    }

  if (error != NULL)
    {
      g_printerr ("%s: failed to execute: %s\n", G_LOG_DOMAIN, error->message);
      g_error_free (error);
    }

  if (result && close_on_succeed)
    gtk_widget_destroy (GTK_WIDGET (window));
}



static void
xfce_appfinder_window_update_frecency (XfceAppfinderWindow *window,
                                        GtkTreeModel        *model,
                                        const gchar         *uri)
{
  GFile      *gfile;
  gchar      *desktop_id;
  GError     *error = NULL;

  if (uri != NULL)
    {
      gfile = g_file_new_for_uri (uri);
      desktop_id = g_file_get_basename (gfile);
      g_object_unref (G_OBJECT (gfile));
      APPFINDER_DEBUG ("desktop : %s", desktop_id);

      model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (window->filter_model));
      xfce_appfinder_model_update_frecency (XFCE_APPFINDER_MODEL (model), desktop_id, &error);

      g_free (desktop_id);
      g_free (error);
    }
}



void
xfce_appfinder_window_set_expanded (XfceAppfinderWindow *window,
                                    gboolean             expanded)
{
  gint                width;
  GtkEntryCompletion *completion;

  APPFINDER_DEBUG ("set expand = %s", expanded ? "true" : "false");

  /* update window geometry */
  if (expanded)
    {
      gtk_window_get_size (GTK_WINDOW (window), &width, NULL);
      gtk_window_resize (GTK_WINDOW (window), width, window->last_window_height);
    }
  else
    {
      if (gtk_widget_get_visible (GTK_WIDGET (window)))
        gtk_window_get_size (GTK_WINDOW (window), &width, &window->last_window_height);
      else
        width = xfconf_channel_get_int (window->channel, "/last/window-width", DEFAULT_WINDOW_WIDTH);

      gtk_window_resize (GTK_WINDOW (window), width, 80 /* should be corrected by wm */);
    }

  /* show/hide pane with treeviews */
  gtk_widget_set_visible (window->paned, expanded);

  /* toggle icon */
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (window->entry), GTK_ENTRY_ICON_SECONDARY,
                                     expanded ? XFCE_APPFINDER_ICON_NAME_GO_UP : XFCE_APPFINDER_ICON_NAME_GO_DOWN);
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (window->entry), GTK_ENTRY_ICON_PRIMARY, NULL);

  /* update completion (remove completed text of restart completion) */
  completion = gtk_entry_get_completion (GTK_ENTRY (window->entry));
  if (completion != NULL)
    gtk_editable_delete_selection (GTK_EDITABLE (window->entry));
  gtk_entry_set_completion (GTK_ENTRY (window->entry), expanded ? NULL : window->completion);
  if (!expanded && gtk_entry_get_text_length (GTK_ENTRY (window->entry)) > 0)
    gtk_entry_completion_insert_prefix (window->completion);

  /* update state */
  xfce_appfinder_window_entry_changed (window);
  xfce_appfinder_window_item_changed (window);
}



static gint
xfce_appfinder_window_sort_items (GtkTreeModel *model,
                                  GtkTreeIter  *a,
                                  GtkTreeIter  *b,
                                  gpointer      data)
{
  gchar        *normalized, *casefold, *title_a, *title_b, *found;
  GtkWidget    *entry = GTK_WIDGET (data);
  gint          result = -1;

  appfinder_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  normalized = g_utf8_normalize (gtk_entry_get_text (GTK_ENTRY (entry)), -1, G_NORMALIZE_ALL);
  casefold = g_utf8_casefold (normalized, -1);
  g_free (normalized);

  gtk_tree_model_get (model, a, XFCE_APPFINDER_MODEL_COLUMN_TITLE, &title_a, -1);
  normalized = g_utf8_normalize (title_a, -1, G_NORMALIZE_ALL);
  g_free (title_a);
  title_a = g_utf8_casefold (normalized, -1);
  g_free (normalized);

  gtk_tree_model_get (model, b, XFCE_APPFINDER_MODEL_COLUMN_TITLE, &title_b, -1);
  normalized = g_utf8_normalize (title_b, -1, G_NORMALIZE_ALL);
  g_free (title_b);
  title_b = g_utf8_casefold (normalized, -1);
  g_free (normalized);

  if (g_strcmp0 (casefold, "") == 0)
    result = g_strcmp0 (title_a, title_b);
  else
    {
      found = g_strrstr (title_a, casefold);
      if (found)
        result -= (G_MAXINT - (found - title_a));

      found = g_strrstr (title_b, casefold);
      if (found)
        result += (G_MAXINT - (found - title_b));
    }

  g_free (casefold);
  g_free (title_a);
  g_free (title_b);
  return result;
}



static gint
xfce_appfinder_window_sort_items_frecency  (GtkTreeModel *model,
                                            GtkTreeIter  *a,
                                            GtkTreeIter  *b,
                                            gpointer      data)
{
  guint        a_freq, b_freq;
  guint64      a_rec, b_rec;
  guint        a_res, b_res;

  gtk_tree_model_get (model, a, XFCE_APPFINDER_MODEL_COLUMN_FREQUENCY, &a_freq, -1);
  gtk_tree_model_get (model, b, XFCE_APPFINDER_MODEL_COLUMN_FREQUENCY, &b_freq, -1);

  gtk_tree_model_get (model, a, XFCE_APPFINDER_MODEL_COLUMN_RECENCY, &a_rec, -1);
  gtk_tree_model_get (model, b, XFCE_APPFINDER_MODEL_COLUMN_RECENCY, &b_rec, -1);

  a_res = xfce_appfinder_model_calculate_frecency (a_freq, a_rec);
  b_res = xfce_appfinder_model_calculate_frecency (b_freq, b_rec);

  /* Sort according the frecency */
  if (b_res - a_res != 0)
    return b_res - a_res;

  /* If they have the same frecency, fallback to the alphabetical order */
  return xfce_appfinder_window_sort_items (model, a, b, data);
}



/*
 * Checks for gtk => 3.22.27 during runtime.
 * This is necessary because sort model for icon view did not work as expected.
 */
static gboolean
xfce_appfinder_should_sort_icon_view (void)
{
  return gtk_get_major_version () >= 3  &&
         gtk_get_minor_version () >= 22 &&
         gtk_get_micro_version () >= 27;
}



static gchar**
xfce_appfinder_parse_envp (gchar **cmd)
{
  gchar **vars;
  gchar **envp;
  gint    offset = 0;

  vars = g_strsplit (*cmd, " ", -1);
  envp = g_get_environ ();

  for (gint n = 0; vars[n] != NULL; ++n)
    {
      gchar *var, *val;
      gchar *index = g_strrstr (vars[n], "=");

      if (index == NULL)
        break;

      offset += strlen (vars[n]);

      var = g_strndup (vars[n], index - vars[n]);
      val = g_strdup (index + 1);
      envp = g_environ_setenv (envp, var, val, TRUE);

      g_free (var);
      g_free (val);
    }

  if (offset > 0)
    {
      gchar *temp = g_strdup (*cmd + offset + 1);
      g_free (*cmd);
      *cmd = temp;
    }

  g_strfreev (vars);

  return envp;
}
