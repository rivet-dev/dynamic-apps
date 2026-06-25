/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * The icon code is based on ideas from SexyIconEntry, which was written by
 * Christian Hammond <chipx86@chipx86.com>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-icon-renderer.h>
#include <thunar/thunar-list-model.h>
#include <thunar/thunar-path-entry.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>
#include <thunar/thunar-window.h>



#define ICON_MARGIN (2)



enum
{
  PROP_0,
  PROP_CURRENT_FILE,
};



static void     thunar_path_entry_editable_init                 (GtkEditableInterface *iface);
static void     thunar_path_entry_finalize                      (GObject              *object);
static void     thunar_path_entry_get_property                  (GObject              *object,
                                                                 guint                 prop_id,
                                                                 GValue               *value,
                                                                 GParamSpec           *pspec);
static void     thunar_path_entry_set_property                  (GObject              *object,
                                                                 guint                 prop_id,
                                                                 const GValue         *value,
                                                                 GParamSpec           *pspec);
static gboolean thunar_path_entry_focus                         (GtkWidget            *widget,
                                                                 GtkDirectionType      direction);
static void     thunar_path_entry_icon_press_event              (GtkEntry            *entry,
                                                                 GtkEntryIconPosition icon_pos,
                                                                 GdkEventButton      *event,
                                                                 gpointer             userdata);
static void     thunar_path_entry_icon_release_event            (GtkEntry            *entry,
                                                                 GtkEntryIconPosition icon_pos,
                                                                 GdkEventButton      *event,
                                                                 gpointer             user_data);
static void     thunar_path_entry_scale_changed                 (GObject              *object,
                                                                 GParamSpec           *pspec,
                                                                 gpointer              user_data);
static gboolean thunar_path_entry_motion_notify_event           (GtkWidget            *widget,
                                                                 GdkEventMotion       *event);
static gboolean thunar_path_entry_key_press_event               (GtkWidget            *widget,
                                                                 GdkEventKey          *event);
static void     thunar_path_entry_drag_data_get                 (GtkWidget            *widget,
                                                                 GdkDragContext       *context,
                                                                 GtkSelectionData     *selection_data,
                                                                 guint                 info,
                                                                 guint                 timestamp);
static void     thunar_path_entry_activate                      (GtkEntry             *entry);
static void     thunar_path_entry_changed                       (GtkEditable          *editable);
static void     thunar_path_entry_update_icon                   (ThunarPathEntry      *path_entry);
static void     thunar_path_entry_do_insert_text                (GtkEditable          *editable,
                                                                 const gchar          *new_text,
                                                                 gint                  new_text_length,
                                                                 gint                 *position);
static void     thunar_path_entry_clear_completion              (ThunarPathEntry      *path_entry);
static void     thunar_path_entry_common_prefix_append          (ThunarPathEntry      *path_entry,
                                                                 gboolean              highlight);
static void     thunar_path_entry_common_prefix_lookup          (ThunarPathEntry      *path_entry,
                                                                 gchar               **prefix_return,
                                                                 ThunarFile          **file_return);
static gboolean thunar_path_entry_match_func                    (GtkEntryCompletion   *completion,
                                                                 const gchar          *key,
                                                                 GtkTreeIter          *iter,
                                                                 gpointer              user_data);
static gboolean thunar_path_entry_match_selected                (GtkEntryCompletion   *completion,
                                                                 GtkTreeModel         *model,
                                                                 GtkTreeIter          *iter,
                                                                 gpointer              user_data);
static gboolean thunar_path_entry_parse                         (ThunarPathEntry      *path_entry,
                                                                 gchar               **folder_part,
                                                                 gchar               **file_part,
                                                                 GError              **error);
static void     thunar_path_entry_queue_check_completion        (ThunarPathEntry      *path_entry);
static gboolean thunar_path_entry_check_completion_idle         (gpointer              user_data);
static void     thunar_path_entry_check_completion_idle_destroy (gpointer              user_data);



struct _ThunarPathEntryClass
{
  GtkEntryClass __parent__;
};

struct _ThunarPathEntry
{
  GtkEntry __parent__;

  ThunarIconFactory *icon_factory;
  ThunarFile        *current_folder;
  ThunarFile        *current_file;
  GFile             *working_directory;

  guint              drag_button;
  gint               drag_x;
  gint               drag_y;

  /* auto completion support */
  guint              in_change : 1;
  guint              has_completion : 1;
  guint              check_completion_idle_id;

  gboolean           search_mode;
};



static const GtkTargetEntry drag_targets[] =
{
  { "text/uri-list", 0, 0, },
};



static GtkEditableInterface *thunar_path_entry_editable_parent_iface;



G_DEFINE_TYPE_WITH_CODE (ThunarPathEntry, thunar_path_entry, GTK_TYPE_ENTRY,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE, thunar_path_entry_editable_init))



static void
thunar_path_entry_class_init (ThunarPathEntryClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GtkEntryClass  *gtkentry_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_path_entry_finalize;
  gobject_class->get_property = thunar_path_entry_get_property;
  gobject_class->set_property = thunar_path_entry_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->focus = thunar_path_entry_focus;
  gtkwidget_class->motion_notify_event = thunar_path_entry_motion_notify_event;
  gtkwidget_class->drag_data_get = thunar_path_entry_drag_data_get;

  gtkentry_class = GTK_ENTRY_CLASS (klass);
  gtkentry_class->activate = thunar_path_entry_activate;

  /**
   * ThunarPathEntry:current-file:
   *
   * The #ThunarFile currently displayed by the path entry or %NULL.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CURRENT_FILE,
                                   g_param_spec_object ("current-file",
                                                        "current-file",
                                                        "current-file",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPathEntry:icon-size:
   *
   * The preferred size of the icon displayed in the path entry.
   **/
  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("icon-size",
                                                             _("Icon size"),
                                                             _("The icon size for the path entry"),
                                                             1, G_MAXINT, 16, EXO_PARAM_READABLE));
}



static void
thunar_path_entry_editable_init (GtkEditableInterface *iface)
{
  thunar_path_entry_editable_parent_iface = g_type_interface_peek_parent (iface);

  iface->changed = thunar_path_entry_changed;
  iface->do_insert_text = thunar_path_entry_do_insert_text;
}



static void
thunar_path_entry_init (ThunarPathEntry *path_entry)
{
  GtkEntryCompletion *completion;
  GtkCellRenderer    *renderer;
  ThunarListModel    *store;

  path_entry->check_completion_idle_id = 0;
  path_entry->working_directory = NULL;

  /* allocate a new entry completion for the given model */
  completion = gtk_entry_completion_new ();
  gtk_entry_completion_set_popup_single_match (completion, FALSE);
  gtk_entry_completion_set_match_func (completion, thunar_path_entry_match_func, path_entry, NULL);
  g_signal_connect (G_OBJECT (completion), "match-selected", G_CALLBACK (thunar_path_entry_match_selected), path_entry);

  /* add the icon renderer to the entry completion */
  renderer = g_object_new (THUNAR_TYPE_ICON_RENDERER, "size", 16, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), renderer, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (completion), renderer, "file", THUNAR_COLUMN_FILE);

  /* add the text renderer to the entry completion */
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (completion), renderer, "text", THUNAR_COLUMN_NAME);

  /* allocate a new list mode for the completion */
  store = thunar_list_model_new ();
  thunar_list_model_set_show_hidden (store, TRUE);
  thunar_list_model_set_folders_first (store, TRUE);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), THUNAR_COLUMN_FILE_NAME, GTK_SORT_ASCENDING);
  gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
  g_object_unref (G_OBJECT (store));

  /* need to connect the "key-press-event" before the GtkEntry class connects the completion signals, so
   * we get the Tab key before its handled as part of the completion stuff.
   */
  g_signal_connect (G_OBJECT (path_entry), "key-press-event", G_CALLBACK (thunar_path_entry_key_press_event), NULL);

  /* setup the new completion */
  gtk_entry_set_completion (GTK_ENTRY (path_entry), completion);

  /* cleanup */
  g_object_unref (G_OBJECT (completion));

  /* clear the auto completion whenever the cursor is moved manually or the selection is changed manually */
  g_signal_connect (G_OBJECT (path_entry), "notify::cursor-position", G_CALLBACK (thunar_path_entry_clear_completion), NULL);
  g_signal_connect (G_OBJECT (path_entry), "notify::selection-bound", G_CALLBACK (thunar_path_entry_clear_completion), NULL);

  /* connect the icon signals */
  g_signal_connect (G_OBJECT (path_entry), "icon-press", G_CALLBACK (thunar_path_entry_icon_press_event), NULL);
  g_signal_connect (G_OBJECT (path_entry), "icon-release", G_CALLBACK (thunar_path_entry_icon_release_event), NULL);
  g_signal_connect (G_OBJECT (path_entry), "notify::scale-factor", G_CALLBACK (thunar_path_entry_scale_changed), NULL);

  /* disabled initially */
  path_entry->search_mode = FALSE;
}



static void
thunar_path_entry_finalize (GObject *object)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (object);

  /* release factory */
  if (path_entry->icon_factory != NULL)
    g_object_unref (path_entry->icon_factory);

  /* release the current-folder reference */
  if (G_LIKELY (path_entry->current_folder != NULL))
    g_object_unref (G_OBJECT (path_entry->current_folder));

  /* release the current-file reference */
  if (G_LIKELY (path_entry->current_file != NULL))
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (path_entry->current_file), thunar_path_entry_set_current_file, path_entry);
      g_object_unref (G_OBJECT (path_entry->current_file));
    }

  /* release the working directory */
  if (G_LIKELY (path_entry->working_directory != NULL))
    g_object_unref (G_OBJECT (path_entry->working_directory));

  /* drop the check_completion_idle source */
  if (G_UNLIKELY (path_entry->check_completion_idle_id != 0))
    g_source_remove (path_entry->check_completion_idle_id);

  (*G_OBJECT_CLASS (thunar_path_entry_parent_class)->finalize) (object);
}



static void
thunar_path_entry_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (object);

  switch (prop_id)
    {
    case PROP_CURRENT_FILE:
      g_value_set_object (value, thunar_path_entry_get_current_file (path_entry));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_path_entry_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (object);

  switch (prop_id)
    {
    case PROP_CURRENT_FILE:
      thunar_path_entry_set_current_file (path_entry, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_path_entry_focus (GtkWidget       *widget,
                         GtkDirectionType direction)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (widget);
  GdkModifierType  state;
  gboolean         control_pressed;

  /* determine whether control is pressed */
  control_pressed = (gtk_get_current_event_state (&state) && (state & GDK_CONTROL_MASK) != 0);

  /* evil hack, but works for GtkFileChooserEntry, so who cares :-) */
  if ((direction == GTK_DIR_TAB_FORWARD) && (gtk_widget_has_focus (widget)) && !control_pressed)
    {
      /* if we don't have a completion and the cursor is at the end of the line, we just insert the common prefix */
      if (!path_entry->has_completion && gtk_editable_get_position (GTK_EDITABLE (path_entry)) == gtk_entry_get_text_length (GTK_ENTRY (path_entry)))
        thunar_path_entry_common_prefix_append (path_entry, FALSE);

      /* place the cursor at the end */
      gtk_editable_set_position (GTK_EDITABLE (path_entry), gtk_entry_get_text_length (GTK_ENTRY (path_entry)));

      return TRUE;
    }
  else
    return (*GTK_WIDGET_CLASS (thunar_path_entry_parent_class)->focus) (widget, direction);
}



static void
thunar_path_entry_icon_press_event (GtkEntry            *entry,
                                    GtkEntryIconPosition icon_pos,
                                    GdkEventButton      *event,
                                    gpointer             userdata)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (entry);

  if (event->button == 1 && icon_pos == GTK_ENTRY_ICON_PRIMARY)
    {
      /* consume the event */
      path_entry->drag_button = event->button;
      path_entry->drag_x = event->x;
      path_entry->drag_y = event->y;
    }
}



static void
thunar_path_entry_icon_release_event (GtkEntry            *entry,
                                      GtkEntryIconPosition icon_pos,
                                      GdkEventButton      *event,
                                      gpointer             user_data)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (entry);

  if (event->button == path_entry->drag_button && icon_pos == GTK_ENTRY_ICON_PRIMARY)
    {
      /* reset the drag button state */
      path_entry->drag_button = 0;
    }
}



static void
thunar_path_entry_scale_changed (GObject    *object,
                                 GParamSpec *pspec,
                                 gpointer    user_data)
{
  gtk_widget_queue_draw (GTK_WIDGET (object));
}



static gboolean
thunar_path_entry_motion_notify_event (GtkWidget      *widget,
                                       GdkEventMotion *event)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (widget);
  GdkDragContext  *context;
  GtkTargetList   *target_list;
  GdkPixbuf       *icon;
  cairo_surface_t *surface;
  gint             size;
  gint             scale_factor;

  if (path_entry->drag_button > 0
      && path_entry->current_file != NULL
      /*FIXME && event->window == gtk_entry_get_icon_window (GTK_ENTRY (widget), GTK_ENTRY_ICON_PRIMARY)*/
      && gtk_drag_check_threshold (widget, path_entry->drag_x, path_entry->drag_y, event->x, event->y))
    {
      /* create the drag context */
      target_list = gtk_target_list_new (drag_targets, G_N_ELEMENTS (drag_targets));
      context = gtk_drag_begin_with_coordinates (widget, target_list,
                                                 GDK_ACTION_COPY |
                                                 GDK_ACTION_LINK,
                                                 path_entry->drag_button,
                                                 (GdkEvent *) event, -1, -1);
      gtk_target_list_unref (target_list);

      /* setup the drag icon (atleast 24px) */
      gtk_widget_style_get (widget, "icon-size", &size, NULL);
      scale_factor = gtk_widget_get_scale_factor (widget);
      icon = thunar_icon_factory_load_file_icon (path_entry->icon_factory,
                                                 path_entry->current_file,
                                                 THUNAR_FILE_ICON_STATE_DEFAULT,
                                                 MAX (size, 16) * scale_factor);
      if (G_LIKELY (icon != NULL))
        {
          surface = gdk_cairo_surface_create_from_pixbuf (icon, scale_factor, gtk_widget_get_window (widget));
          g_object_unref (G_OBJECT (icon));
          gtk_drag_set_icon_surface (context, surface);
          cairo_surface_destroy (surface);
        }

      /* reset the drag button state */
      path_entry->drag_button = 0;

      return TRUE;
    }

  return (*GTK_WIDGET_CLASS (thunar_path_entry_parent_class)->motion_notify_event) (widget, event);
}



static gboolean
thunar_path_entry_key_press_event (GtkWidget   *widget,
                                   GdkEventKey *event)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (widget);

  /* check if we have a tab key press here and control is not pressed */
  if (G_UNLIKELY (event->keyval == GDK_KEY_Tab && (event->state & GDK_CONTROL_MASK) == 0) && path_entry->search_mode == FALSE)
    {
      /* if we don't have a completion and the cursor is at the end of the line, we just insert the common prefix */
      if (!path_entry->has_completion && gtk_editable_get_position (GTK_EDITABLE (path_entry)) == gtk_entry_get_text_length (GTK_ENTRY (path_entry)))
        thunar_path_entry_common_prefix_append (path_entry, FALSE);

      /* place the cursor at the end */
      gtk_editable_set_position (GTK_EDITABLE (path_entry), gtk_entry_get_text_length (GTK_ENTRY (path_entry)));

      /* emit "changed", so the completion window is popped up */
      g_signal_emit_by_name (G_OBJECT (path_entry), "changed", 0);

      /* we handled the event */
      return TRUE;
    }
  /* cancel search with `Escape` */
  if (G_UNLIKELY (path_entry->search_mode == TRUE && event->keyval == GDK_KEY_Escape && (event->state & GDK_CONTROL_MASK) == 0))
    {
      GtkWidget *window = gtk_widget_get_toplevel (widget);
      thunar_window_action_cancel_search (THUNAR_WINDOW (window));
      return TRUE;
    }

  return FALSE;
}



static void
thunar_path_entry_drag_data_get (GtkWidget        *widget,
                                 GdkDragContext   *context,
                                 GtkSelectionData *selection_data,
                                 guint             info,
                                 guint             timestamp)
{
  ThunarPathEntry  *path_entry = THUNAR_PATH_ENTRY (widget);
  GList             file_list;
  gchar           **uris;

  /* verify that we actually display a path */
  if (G_LIKELY (path_entry->current_file != NULL))
    {
      /* transform the path for the current file into an uri string list */
      file_list.next = file_list.prev = NULL;
      file_list.data = thunar_file_get_file (path_entry->current_file);

      /* setup the uri list for the drag selection */
      uris = thunar_g_file_list_to_stringv (&file_list);
      gtk_selection_data_set_uris (selection_data, uris);
      g_strfreev (uris);
    }
}



static void
thunar_path_entry_activate (GtkEntry *entry)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (entry);

  if (G_LIKELY (path_entry->has_completion))
    {
      /* place cursor at the end of the text if we have completion set */
      gtk_editable_set_position (GTK_EDITABLE (path_entry), -1);
    }

  /* emit the "activate" signal */
  (*GTK_ENTRY_CLASS (thunar_path_entry_parent_class)->activate) (entry);
}



static void
thunar_path_entry_changed (GtkEditable *editable)
{
  GtkEntryCompletion *completion;
  ThunarPathEntry    *path_entry = THUNAR_PATH_ENTRY (editable);
  ThunarFolder       *folder;
  GtkTreeModel       *model;
  const gchar        *text;
  gchar              *scheme;
  ThunarFile         *current_folder;
  ThunarFile         *current_file;
  GFile              *folder_path = NULL;
  GFile              *file_path = NULL;
  gchar              *folder_part = NULL;
  gchar              *file_part = NULL;
  gboolean            update_icon = FALSE;

  /* check if we should ignore this event */
  if (G_UNLIKELY (path_entry->in_change))
    return;

  /* parse the entered string (handling URIs properly) */
  text = gtk_entry_get_text (GTK_ENTRY (path_entry));

  if (G_UNLIKELY (text != NULL && thunar_util_is_a_search_query (text)) == TRUE)
    {
      GtkWidget *window = gtk_widget_get_toplevel (GTK_WIDGET (editable));
      path_entry->search_mode = TRUE;
      update_icon = TRUE;
      thunar_window_update_search (THUNAR_WINDOW (window));
    }
  else
    {
      /* cancel search when part of 'Search: ' is deleted */
      if (path_entry->search_mode == TRUE)
        {
          GtkWidget *window = gtk_widget_get_toplevel (GTK_WIDGET (editable));
          thunar_window_action_cancel_search (THUNAR_WINDOW (window));
        }
      
      /* location/folder-path code */
      scheme = g_uri_parse_scheme (text);
      if (G_UNLIKELY (scheme != NULL))
        {
          /* Text is URI */
          g_free (scheme);
          file_path = g_file_new_for_uri (text);

          /* use the same file if the text assumes we're in a directory */
          if (g_str_has_suffix (text, "/"))
            folder_path = G_FILE (g_object_ref (G_OBJECT (file_path)));
          else
            folder_path = g_file_get_parent (file_path);
        }
      else if (thunar_path_entry_parse (path_entry, &folder_part, &file_part, NULL))
        {
          /* determine the folder path */
          folder_path = g_file_new_for_path (folder_part);

          /* determine the relative file path */
          if (G_LIKELY (*file_part != '\0'))
            file_path = g_file_resolve_relative_path (folder_path, file_part);
          else
            file_path = g_object_ref (folder_path);

          /* cleanup the part strings */
          g_free (folder_part);
          g_free (file_part);
        }
    }

  /* determine new current file/folder from the paths */
  current_folder = (folder_path != NULL) ? thunar_file_get (folder_path, NULL) : NULL;
  current_file = (file_path != NULL) ? thunar_file_get (file_path, NULL) : NULL;

  /* determine the entry completion */
  completion = gtk_entry_get_completion (GTK_ENTRY (path_entry));

  /* update the current folder if required */
  if (current_folder != path_entry->current_folder)
    {
      /* take a reference on the current folder */
      if (G_LIKELY (path_entry->current_folder != NULL))
        g_object_unref (G_OBJECT (path_entry->current_folder));
      path_entry->current_folder = current_folder;
      if (G_LIKELY (current_folder != NULL))
        g_object_ref (G_OBJECT (current_folder));

      /* try to open the current-folder file as folder */
      if (current_folder != NULL && thunar_file_is_directory (current_folder))
        folder = thunar_folder_get_for_file (current_folder);
      else
        folder = NULL;

      /* set the new folder for the completion model, but disconnect the model from the
       * completion first, because GtkEntryCompletion has become very slow recently when
       * updating the model being used (https://bugzilla.xfce.org/show_bug.cgi?id=1681).
       */
      model = gtk_entry_completion_get_model (completion);
      g_object_ref (G_OBJECT (model));
      gtk_entry_completion_set_model (completion, NULL);
      thunar_list_model_set_folder (THUNAR_LIST_MODEL (model), folder, NULL);
      gtk_entry_completion_set_model (completion, model);
      g_object_unref (G_OBJECT (model));

      /* cleanup */
      if (G_LIKELY (folder != NULL))
        g_object_unref (G_OBJECT (folder));

      /* we most likely need a new icon */
      update_icon = TRUE;
    }

  /* update the current file if required */
  if (current_file != path_entry->current_file)
    {
      if (G_UNLIKELY (path_entry->current_file != NULL))
        {
          g_signal_handlers_disconnect_by_func (G_OBJECT (path_entry->current_file), thunar_path_entry_set_current_file, path_entry);
          g_object_unref (G_OBJECT (path_entry->current_file));
        }
      path_entry->current_file = current_file;
      if (G_UNLIKELY (current_file != NULL))
        {
          g_object_ref (G_OBJECT (current_file));
          g_signal_connect_swapped (G_OBJECT (current_file), "changed", G_CALLBACK (thunar_path_entry_set_current_file), path_entry);
        }
      g_object_notify (G_OBJECT (path_entry), "current-file");

      /* we most likely need a new icon */
      update_icon = TRUE;
    }

  if (update_icon)
    thunar_path_entry_update_icon (path_entry);

  /* cleanup */
  if (G_LIKELY (current_folder != NULL))
    g_object_unref (G_OBJECT (current_folder));
  if (G_LIKELY (current_file != NULL))
    g_object_unref (G_OBJECT (current_file));
  if (G_LIKELY (folder_path != NULL))
    g_object_unref (folder_path);
  if (G_LIKELY (file_path != NULL))
    g_object_unref (file_path);
}


static void
thunar_path_entry_update_icon (ThunarPathEntry *path_entry)
{
  GdkPixbuf          *icon = NULL;
  GtkIconTheme       *icon_theme;
  gint                icon_size;
  gint                scale_factor;

  if (path_entry->search_mode == TRUE)
    {
      gtk_entry_set_icon_from_icon_name (GTK_ENTRY (path_entry),
                                         GTK_ENTRY_ICON_PRIMARY,
                                         "system-search");
      return;
    }

  if (path_entry->icon_factory == NULL)
    {
      icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (path_entry)));
      path_entry->icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);
    }

  gtk_widget_style_get (GTK_WIDGET (path_entry), "icon-size", &icon_size, NULL);
  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (path_entry));

  if (G_UNLIKELY (path_entry->current_file != NULL))
    {
      icon = thunar_icon_factory_load_file_icon (path_entry->icon_factory,
                                                 path_entry->current_file,
                                                 THUNAR_FILE_ICON_STATE_DEFAULT,
                                                 icon_size * scale_factor);
    }
  else if (G_LIKELY (path_entry->current_folder != NULL))
    {
      icon = thunar_icon_factory_load_file_icon (path_entry->icon_factory,
                                                 path_entry->current_folder,
                                                 THUNAR_FILE_ICON_STATE_DEFAULT,
                                                 icon_size * scale_factor);
    }

  if (icon != NULL)
    {
      gtk_entry_set_icon_from_pixbuf (GTK_ENTRY (path_entry),
                                      GTK_ENTRY_ICON_PRIMARY,
                                      icon);
      g_object_unref (icon);
    }
  else
    {
      gtk_entry_set_icon_from_icon_name (GTK_ENTRY (path_entry),
                                         GTK_ENTRY_ICON_PRIMARY,
                                         "dialog-error-symbolic");
    }
}



static void
thunar_path_entry_do_insert_text (GtkEditable *editable,
                                  const gchar *new_text,
                                  gint         new_text_length,
                                  gint        *position)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (editable);

  /* let the GtkEntry class handle the insert */
  (*thunar_path_entry_editable_parent_iface->do_insert_text) (editable, new_text, new_text_length, position);

  /* queue a completion check if this insert operation was triggered by the user */
  if (G_LIKELY (!path_entry->in_change))
    thunar_path_entry_queue_check_completion (path_entry);
}



static void
thunar_path_entry_clear_completion (ThunarPathEntry *path_entry)
{
  /* reset the completion and apply the new text */
  if (G_UNLIKELY (path_entry->has_completion))
    {
      path_entry->has_completion = FALSE;
      thunar_path_entry_changed (GTK_EDITABLE (path_entry));
    }
}



static void
thunar_path_entry_common_prefix_append (ThunarPathEntry *path_entry,
                                        gboolean         highlight)
{
  const gchar *last_slash;
  const gchar *text;
  ThunarFile  *file;
  gchar       *prefix;
  gchar       *tmp;
  gint         prefix_length;
  gint         text_length;
  gint         offset;
  gint         base;

  /* determine the common prefix */
  thunar_path_entry_common_prefix_lookup (path_entry, &prefix, &file);

  /* check if we should append a slash to the prefix */
  if (G_LIKELY (file != NULL))
    {
      /* we only append slashes for directories */
      if (thunar_file_is_directory (file) && file != path_entry->current_file)
        {
          tmp = g_strconcat (prefix, G_DIR_SEPARATOR_S, NULL);
          g_free (prefix);
          prefix = tmp;
        }

      /* release the file */
      g_object_unref (G_OBJECT (file));
    }

  /* check if we have a common prefix */
  if (G_LIKELY (prefix != NULL))
    {
      /* determine the UTF-8 length of the entry text */
      text = gtk_entry_get_text (GTK_ENTRY (path_entry));
      last_slash = g_utf8_strrchr (text, -1, G_DIR_SEPARATOR);
      if (G_LIKELY (last_slash != NULL))
        offset = g_utf8_strlen (text, last_slash - text) + 1;
      else
        offset = 0;
      text_length = g_utf8_strlen (text, -1) - offset;

      /* determine the UTF-8 length of the prefix */
      prefix_length = g_utf8_strlen (prefix, -1);

      /* append only if the prefix is longer than the already entered text */
      if (G_LIKELY (prefix_length > text_length))
        {
          /* remember the base offset */
          base = offset;

          /* insert the prefix */
          path_entry->in_change = TRUE;
          gtk_editable_delete_text (GTK_EDITABLE (path_entry), offset, -1);
          gtk_editable_insert_text (GTK_EDITABLE (path_entry), prefix, -1, &offset);
          path_entry->in_change = FALSE;

          /* highlight the prefix if requested */
          if (G_LIKELY (highlight))
            {
              gtk_editable_select_region (GTK_EDITABLE (path_entry), base + text_length, base + prefix_length);
              path_entry->has_completion = TRUE;
            }
        }

      /* cleanup */
      g_free (prefix);
    }
}



static gboolean
thunar_path_entry_has_prefix_casefolded (const gchar *string,
                                         const gchar *prefix)
{
  gchar *string_casefolded;
  gchar *prefix_casefolded;
  gboolean has_prefix;

  if (string == NULL || prefix == NULL)
    return FALSE;

  string_casefolded = g_utf8_casefold (string, -1);
  prefix_casefolded = g_utf8_casefold (prefix, -1);

  has_prefix = g_str_has_prefix (string_casefolded , prefix_casefolded);

  g_free (string_casefolded);
  g_free (prefix_casefolded);

  return has_prefix;
}



static void
thunar_path_entry_common_prefix_lookup (ThunarPathEntry *path_entry,
                                        gchar          **prefix_return,
                                        ThunarFile     **file_return)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  const gchar  *text;
  const gchar  *s;
  gchar        *name;
  gchar        *t;

  *prefix_return = NULL;
  *file_return = NULL;

  /* lookup the last slash character in the entry text */
  text = gtk_entry_get_text (GTK_ENTRY (path_entry));
  s = strrchr (text, G_DIR_SEPARATOR);
  if (G_UNLIKELY (s != NULL && s[1] == '\0'))
    return;
  else if (G_LIKELY (s != NULL))
    text = s + 1;

  /* check all items in the model */
  model = gtk_entry_completion_get_model (gtk_entry_get_completion (GTK_ENTRY (path_entry)));
  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          /* determine the real file name for the iter */
          gtk_tree_model_get (model, &iter, THUNAR_COLUMN_FILE_NAME, &name, -1);

          /* check if we have a valid prefix here */
          if (thunar_path_entry_has_prefix_casefolded (name, text))
            {
              /* check if we're the first to match */
              if (*prefix_return == NULL)
                {
                  /* remember the prefix */
                  *prefix_return = g_strdup (name);

                  /* determine the file for the iter */
                  gtk_tree_model_get (model, &iter, THUNAR_COLUMN_FILE, file_return, -1);
                }
              else
                {
                  /* we already have another prefix, so determine the common part */
                  for (s = name, t = *prefix_return; *s != '\0' && *s == *t; ++s, ++t)
                    ;
                  *t = '\0';

                  /* release the file, since it's not a unique match */
                  if (G_LIKELY (*file_return != NULL))
                    {
                      g_object_unref (G_OBJECT (*file_return));
                      *file_return = NULL;
                    }
                }
            }

          /* cleanup */
          g_free (name);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
}



static gboolean
thunar_path_entry_match_func (GtkEntryCompletion *completion,
                              const gchar        *key,
                              GtkTreeIter        *iter,
                              gpointer            user_data)
{
  GtkTreeModel    *model;
  ThunarPathEntry *path_entry;
  const gchar     *last_slash;
  ThunarFile      *file;
  gboolean         matched;
  gchar           *text_normalized;
  gchar           *name_normalized;
  gchar           *name;

  /* determine the model from the completion */
  model = gtk_entry_completion_get_model (completion);

  /* leave if the model is null, we do this in thunar_path_entry_changed() to speed
   * things up, but that causes https://bugzilla.xfce.org/show_bug.cgi?id=4847. */
  if (G_UNLIKELY (model == NULL))
    return FALSE;

  /* leave if the auto completion highlight was not cleared yet, to prevent
   * https://bugzilla.xfce.org/show_bug.cgi?id=16267. */
  path_entry = THUNAR_PATH_ENTRY (user_data);
  if (G_UNLIKELY (path_entry->has_completion))
    return FALSE;

  /* determine the current text (UTF-8 normalized) */
  text_normalized = g_utf8_normalize (gtk_entry_get_text (GTK_ENTRY (user_data)), -1, G_NORMALIZE_ALL);

  /* lookup the last slash character in the key */
  last_slash = strrchr (text_normalized, G_DIR_SEPARATOR);
  if (G_UNLIKELY (last_slash != NULL && last_slash[1] == '\0'))
    {
      /* check if the file is hidden */
      gtk_tree_model_get (model, iter, THUNAR_COLUMN_FILE, &file, -1);
      matched = !thunar_file_is_hidden (file);
      g_object_unref (G_OBJECT (file));
    }
  else
    {
      if (G_UNLIKELY (last_slash == NULL))
        last_slash = text_normalized;
      else
        last_slash += 1;

      /* determine the real file name for the iter */
      gtk_tree_model_get (model, iter, THUNAR_COLUMN_FILE_NAME, &name, -1);
      name_normalized = g_utf8_normalize (name, -1, G_NORMALIZE_ALL);
      if (G_LIKELY (name_normalized != NULL))
        g_free (name);
      else
        name_normalized = name;

      /* check if we have a match here */
      if (name_normalized != NULL)
        matched = thunar_path_entry_has_prefix_casefolded (name_normalized, last_slash);
      else
        matched = FALSE;

      /* cleanup */
      g_free (name_normalized);
    }

  /* cleanup */
  g_free (text_normalized);

  return matched;
}



static gboolean
thunar_path_entry_match_selected (GtkEntryCompletion *completion,
                                  GtkTreeModel       *model,
                                  GtkTreeIter        *iter,
                                  gpointer            user_data)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (user_data);
  const gchar     *last_slash;
  const gchar     *text;
  ThunarFile      *file;
  gchar           *real_name;
  gchar           *tmp;
  gint             offset;

  /* determine the file for the iterator */
  gtk_tree_model_get (model, iter, THUNAR_COLUMN_FILE, &file, -1);

  /* determine the real name for the file */
  gtk_tree_model_get (model, iter, THUNAR_COLUMN_FILE_NAME, &real_name, -1);

  /* append a slash if we have a folder here */
  if (G_LIKELY (thunar_file_is_directory (file)))
    {
      tmp = g_strconcat (real_name, G_DIR_SEPARATOR_S, NULL);
      g_free (real_name);
      real_name = tmp;
    }

  /* determine the UTF-8 offset of the last slash on the entry text */
  text = gtk_entry_get_text (GTK_ENTRY (path_entry));
  last_slash = g_utf8_strrchr (text, -1, G_DIR_SEPARATOR);
  if (G_LIKELY (last_slash != NULL))
    offset = g_utf8_strlen (text, last_slash - text) + 1;
  else
    offset = 0;

  /* delete the previous text at the specified offset */
  gtk_editable_delete_text (GTK_EDITABLE (path_entry), offset, -1);

  /* insert the new file/folder name */
  gtk_editable_insert_text (GTK_EDITABLE (path_entry), real_name, -1, &offset);

  /* move the cursor to the end of the text entry */
  gtk_editable_set_position (GTK_EDITABLE (path_entry), -1);

  /* cleanup */
  g_object_unref (G_OBJECT (file));
  g_free (real_name);

  return TRUE;
}



static gboolean
thunar_path_entry_parse (ThunarPathEntry *path_entry,
                         gchar          **folder_part,
                         gchar          **file_part,
                         GError         **error)
{
  const gchar *last_slash;
  gchar       *filename;
  gchar       *path;

  _thunar_return_val_if_fail (THUNAR_IS_PATH_ENTRY (path_entry), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_return_val_if_fail (folder_part != NULL, FALSE);
  _thunar_return_val_if_fail (file_part != NULL, FALSE);

  /* expand the filename */
  filename = thunar_util_expand_filename (gtk_entry_get_text (GTK_ENTRY (path_entry)),
                                          path_entry->working_directory,
                                          error);
  if (G_UNLIKELY (filename == NULL))
    return FALSE;

  /* lookup the last slash character in the filename */
  last_slash = strrchr (filename, G_DIR_SEPARATOR);
  if (G_UNLIKELY (last_slash == NULL))
    {
      /* no slash character, it's relative to the home dir */
      *file_part = g_filename_from_utf8 (filename, -1, NULL, NULL, error);
      if (G_LIKELY (*file_part != NULL))
        *folder_part = g_strdup (xfce_get_homedir ());
    }
  else
    {
      if (G_LIKELY (last_slash != filename))
        *folder_part = g_filename_from_utf8 (filename, last_slash - filename, NULL, NULL, error);
      else
        *folder_part = g_strdup ("/");

      if (G_LIKELY (*folder_part != NULL))
        {
          /* if folder_part doesn't start with '/', it's relative to the home dir */
          if (G_UNLIKELY (**folder_part != G_DIR_SEPARATOR))
            {
              path = xfce_get_homefile (*folder_part, NULL);
              g_free (*folder_part);
              *folder_part = path;
            }

          /* determine the file part */
          *file_part = g_filename_from_utf8 (last_slash + 1, -1, NULL, NULL, error);
          if (G_UNLIKELY (*file_part == NULL))
            {
              g_free (*folder_part);
              *folder_part = NULL;
            }
        }
      else
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s", g_strerror (EINVAL));
        }
    }

  /* release the filename */
  g_free (filename);

  return (*folder_part != NULL);
}



static void
thunar_path_entry_queue_check_completion (ThunarPathEntry *path_entry)
{
  if (G_LIKELY (path_entry->check_completion_idle_id == 0))
    {
      path_entry->check_completion_idle_id = g_idle_add_full (G_PRIORITY_HIGH, thunar_path_entry_check_completion_idle,
                                                              path_entry, thunar_path_entry_check_completion_idle_destroy);
    }
}



static gboolean
thunar_path_entry_check_completion_idle (gpointer user_data)
{
  ThunarPathEntry *path_entry = THUNAR_PATH_ENTRY (user_data);
  const gchar     *text;

THUNAR_THREADS_ENTER

  /* check if the user entered at least part of a filename */
  text = gtk_entry_get_text (GTK_ENTRY (path_entry));
  if (*text != '\0' && text[strlen (text) - 1] != '/')
    {
      /* automatically insert the common prefix */
      thunar_path_entry_common_prefix_append (path_entry, TRUE);
    }

THUNAR_THREADS_LEAVE

  return FALSE;
}



static void
thunar_path_entry_check_completion_idle_destroy (gpointer user_data)
{
  THUNAR_PATH_ENTRY (user_data)->check_completion_idle_id = 0;
}



/**
 * thunar_path_entry_new:
 *
 * Allocates a new #ThunarPathEntry instance.
 *
 * Return value: the newly allocated #ThunarPathEntry.
 **/
GtkWidget*
thunar_path_entry_new (void)
{
  return g_object_new (THUNAR_TYPE_PATH_ENTRY, NULL);
}



/**
 * thunar_path_entry_get_current_file:
 * @path_entry : a #ThunarPathEntry.
 *
 * Returns the #ThunarFile currently being displayed by
 * @path_entry or %NULL if @path_entry doesn't contain
 * a valid #ThunarFile.
 *
 * Return value: the #ThunarFile for @path_entry or %NULL.
 **/
ThunarFile*
thunar_path_entry_get_current_file (ThunarPathEntry *path_entry)
{
  _thunar_return_val_if_fail (THUNAR_IS_PATH_ENTRY (path_entry), NULL);
  return path_entry->current_file;
}



/**
 * thunar_path_entry_set_current_file:
 * @path_entry   : a #ThunarPathEntry.
 * @current_file : a #ThunarFile or %NULL.
 *
 * Sets the #ThunarFile that should be displayed by
 * @path_entry to @current_file.
 **/
void
thunar_path_entry_set_current_file (ThunarPathEntry *path_entry,
                                    ThunarFile      *current_file)
{
  GFile    *file;
  gchar    *text;
  gchar    *unescaped;
  gchar    *tmp;
  gboolean  is_uri = FALSE;

  _thunar_return_if_fail (THUNAR_IS_PATH_ENTRY (path_entry));
  _thunar_return_if_fail (current_file == NULL || THUNAR_IS_FILE (current_file));

  file = (current_file != NULL) ? thunar_file_get_file (current_file) : NULL;
  if (G_UNLIKELY (file == NULL))
    {
      /* invalid file */
      text = g_strdup ("");
    }
  else
    {
      /* check if the file is native to the platform */
      if (g_file_is_native (file))
        {
          /* it is, try the local path first */
          text = g_file_get_path (file);

          /* if there is no local path, use the URI (which always works) */
          if (text == NULL)
            {
              text = g_file_get_uri (file);
              is_uri = TRUE;
            }
        }
      else
        {
          /* not a native file, use the URI */
          text = g_file_get_uri (file);
          is_uri = TRUE;
        }

      /* if the file is a directory, end with a / to avoid loading the parent
       * directory which is probably not something the user wants */
      if (thunar_file_is_directory (current_file)
          && !g_str_has_suffix (text, "/"))
        {
          tmp = g_strconcat (text, "/", NULL);
          g_free (text);
          text = tmp;
        }

      // convert filename into valid UTF-8 string for display
      tmp = text;
      text = g_filename_display_name(tmp);
      g_free (tmp);
    }

  if (is_uri)
    unescaped = g_uri_unescape_string (text, NULL);
  else
    unescaped = g_strdup (text);
  g_free (text);

  /* setup the entry text */
  gtk_entry_set_text (GTK_ENTRY (path_entry), unescaped);
  g_free (unescaped);

  /* update the icon */
  thunar_path_entry_update_icon (path_entry);

  gtk_editable_set_position (GTK_EDITABLE (path_entry), -1);

  gtk_widget_queue_draw (GTK_WIDGET (path_entry));
}



/**
 * thunar_path_entry_set_working_directory:
 * @path_entry        : a #ThunarPathEntry.
 * @working_directory : a #ThunarFile or %NULL.
 *
 * Sets the #ThunarFile that should be used as the
 * working directory for @path_entry.
 **/
void
thunar_path_entry_set_working_directory (ThunarPathEntry *path_entry,
                                         ThunarFile      *working_directory)
{
  _thunar_return_if_fail (THUNAR_IS_PATH_ENTRY (path_entry));
  _thunar_return_if_fail (working_directory == NULL || THUNAR_IS_FILE (working_directory));

  if (G_LIKELY (path_entry->working_directory != NULL))
    g_object_unref (path_entry->working_directory);

  path_entry->working_directory = NULL;

  if (THUNAR_IS_FILE (working_directory))
    path_entry->working_directory = g_object_ref (thunar_file_get_file (working_directory));
}



/**
 * thunar_path_entry_get_search_query:
 * @path_entry        : a #ThunarPathEntry.
 *
 * Returns a copy of the search query in the text field of the @path_entry or "" if the path_entry doesn't contain
 * a search query.
 *
 * It's the responsibility of the caller to free the returned string using `g_free`.
 **/
gchar*
thunar_path_entry_get_search_query (ThunarPathEntry *path_entry)
{
  unsigned long search_prefix_length;
  const gchar *text = gtk_entry_get_text (GTK_ENTRY (path_entry));
  search_prefix_length = strlen (SEARCH_PREFIX);

  _thunar_return_val_if_fail (THUNAR_IS_PATH_ENTRY (path_entry), NULL);
  _thunar_return_val_if_fail (strncmp (text, SEARCH_PREFIX, search_prefix_length) == 0, NULL);

  return strlen(text) > search_prefix_length ? g_strdup (&text[search_prefix_length]) : g_strdup ("");
}



void
thunar_path_entry_cancel_search (ThunarPathEntry *path_entry)
{
  _thunar_return_if_fail (THUNAR_IS_PATH_ENTRY (path_entry));

  path_entry->search_mode = FALSE;

  gtk_widget_grab_focus (gtk_widget_get_parent (GTK_WIDGET (path_entry)));
}
