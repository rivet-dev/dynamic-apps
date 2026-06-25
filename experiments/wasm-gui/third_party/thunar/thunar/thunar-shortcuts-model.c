/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2012      Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>

#include <thunar/thunar-file-monitor.h>
#include <thunar/thunar-file.h>
#include <thunar/thunar-shortcuts-model.h>
#include <thunar/thunar-device-monitor.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-util.h>
#include <thunar/thunar-private.h>

#define SPINNER_CYCLE_DURATION 1000
#define SPINNER_NUM_STEPS      12



#define THUNAR_SHORTCUT(obj) ((ThunarShortcut *) (obj))



typedef struct _ThunarShortcut ThunarShortcut;



enum
{
  PROP_0,
  PROP_HIDDEN_BOOKMARKS,
  PROP_FILE_SIZE_BINARY
};



static void               thunar_shortcuts_model_tree_model_init    (GtkTreeModelIface         *iface);
static void               thunar_shortcuts_model_drag_source_init   (GtkTreeDragSourceIface    *iface);
static void               thunar_shortcuts_model_get_property       (GObject                   *object,
                                                                     guint                      prop_id,
                                                                     GValue                    *value,
                                                                     GParamSpec                *pspec);
static void               thunar_shortcuts_model_set_property       (GObject                   *object,
                                                                     guint                      prop_id,
                                                                     const GValue              *value,
                                                                     GParamSpec                *pspec);
static void               thunar_shortcuts_model_finalize           (GObject                   *object);
static GtkTreeModelFlags  thunar_shortcuts_model_get_flags          (GtkTreeModel              *tree_model);
static gint               thunar_shortcuts_model_get_n_columns      (GtkTreeModel              *tree_model);
static GType              thunar_shortcuts_model_get_column_type    (GtkTreeModel              *tree_model,
                                                                     gint                       idx);
static gboolean           thunar_shortcuts_model_get_iter           (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter,
                                                                     GtkTreePath               *path);
static GtkTreePath       *thunar_shortcuts_model_get_path           (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter);
static void               thunar_shortcuts_model_get_value          (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter,
                                                                     gint                       column,
                                                                     GValue                    *value);
static gboolean           thunar_shortcuts_model_iter_next          (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter);
static gboolean           thunar_shortcuts_model_iter_children      (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter,
                                                                     GtkTreeIter               *parent);
static gboolean           thunar_shortcuts_model_iter_has_child     (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter);
static gint               thunar_shortcuts_model_iter_n_children    (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter);
static gboolean           thunar_shortcuts_model_iter_nth_child     (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter,
                                                                     GtkTreeIter               *parent,
                                                                     gint                       n);
static gboolean           thunar_shortcuts_model_iter_parent        (GtkTreeModel              *tree_model,
                                                                     GtkTreeIter               *iter,
                                                                     GtkTreeIter               *child);
static gboolean           thunar_shortcuts_model_row_draggable      (GtkTreeDragSource         *source,
                                                                     GtkTreePath               *path);
static gboolean           thunar_shortcuts_model_drag_data_get      (GtkTreeDragSource         *source,
                                                                     GtkTreePath               *path,
                                                                     GtkSelectionData          *selection_data);
static gboolean           thunar_shortcuts_model_drag_data_delete   (GtkTreeDragSource         *source,
                                                                     GtkTreePath               *path);
static void               thunar_shortcuts_model_header_visibility  (ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_shortcut_devices   (ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_shortcut_places    (ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_shortcut_network   (ThunarShortcutsModel      *model);
static gboolean           thunar_shortcuts_model_get_hidden         (ThunarShortcutsModel      *model,
                                                                     ThunarShortcut            *shortcut);
static void               thunar_shortcuts_model_add_shortcut       (ThunarShortcutsModel      *model,
                                                                     ThunarShortcut            *shortcut);
static void               thunar_shortcuts_model_remove_shortcut    (ThunarShortcutsModel      *model,
                                                                     ThunarShortcut            *shortcut);
static gboolean           thunar_shortcuts_model_load               (gpointer                   data);
static void               thunar_shortcuts_model_save               (ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_monitor            (GFileMonitor              *monitor,
                                                                     GFile                     *file,
                                                                     GFile                     *other_file,
                                                                     GFileMonitorEvent          event_type,
                                                                     gpointer                   user_data);
static void               thunar_shortcuts_model_device_added       (ThunarDeviceMonitor       *device_monitor,
                                                                     ThunarDevice              *device,
                                                                     ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_device_removed     (ThunarDeviceMonitor       *device_monitor,
                                                                     ThunarDevice              *device,
                                                                     ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_device_changed     (ThunarDeviceMonitor       *device_monitor,
                                                                     ThunarDevice              *device,
                                                                     ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_file_changed       (ThunarFileMonitor         *file_monitor,
                                                                     ThunarFile                *file,
                                                                     ThunarShortcutsModel      *model);
static void               thunar_shortcuts_model_file_destroyed     (ThunarFileMonitor         *file_monitor,
                                                                     ThunarFile                *file,
                                                                     ThunarShortcutsModel      *model);
static void               thunar_shortcut_free                      (ThunarShortcut            *shortcut,
                                                                     ThunarShortcutsModel      *model);



struct _ThunarShortcutsModelClass
{
  GObjectClass __parent__;
};

struct _ThunarShortcutsModel
{
  GObject         __parent__;

  /* the model stamp is only used when debugging is
   * enabled, to make sure we don't accept iterators
   * generated by another model.
   */
#ifndef NDEBUG
  gint                  stamp;
#endif

  GList                *shortcuts;

  ThunarPreferences    *preferences;
  gchar               **hidden_bookmarks;
  gboolean              file_size_binary;

  ThunarFileMonitor    *file_monitor;
  ThunarDeviceMonitor  *device_monitor;

  gint64                bookmarks_time;
  GFile                *bookmarks_file;
  GFileMonitor         *bookmarks_monitor;
  guint                 bookmarks_idle_id;

  guint                 busy_timeout_id;
};

struct _ThunarShortcut
{
  ThunarShortcutGroup  group;

  gchar               *name;
  GIcon               *gicon;
  gchar               *tooltip;
  gint                 sort_id;

  guint                busy : 1;
  guint                busy_pulse;

  GFile               *location;
  ThunarFile          *file;
  ThunarDevice        *device;

  guint                hidden : 1;
};



G_DEFINE_TYPE_WITH_CODE (ThunarShortcutsModel, thunar_shortcuts_model, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, thunar_shortcuts_model_tree_model_init)
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE, thunar_shortcuts_model_drag_source_init))



static void
thunar_shortcuts_model_class_init (ThunarShortcutsModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_shortcuts_model_finalize;
  gobject_class->get_property = thunar_shortcuts_model_get_property;
  gobject_class->set_property = thunar_shortcuts_model_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_HIDDEN_BOOKMARKS,
                                   g_param_spec_boxed ("hidden-bookmarks",
                                                       NULL,
                                                       NULL,
                                                       G_TYPE_STRV,
                                                       EXO_PARAM_READWRITE));

  /**
   * ThunarPropertiesDialog:file_size_binary:
   *
   * Whether the file size should be shown in binary or decimal.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE_SIZE_BINARY,
                                   g_param_spec_boolean ("file-size-binary",
                                                         "FileSizeBinary",
                                                         NULL,
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_shortcuts_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = thunar_shortcuts_model_get_flags;
  iface->get_n_columns = thunar_shortcuts_model_get_n_columns;
  iface->get_column_type = thunar_shortcuts_model_get_column_type;
  iface->get_iter = thunar_shortcuts_model_get_iter;
  iface->get_path = thunar_shortcuts_model_get_path;
  iface->get_value = thunar_shortcuts_model_get_value;
  iface->iter_next = thunar_shortcuts_model_iter_next;
  iface->iter_children = thunar_shortcuts_model_iter_children;
  iface->iter_has_child = thunar_shortcuts_model_iter_has_child;
  iface->iter_n_children = thunar_shortcuts_model_iter_n_children;
  iface->iter_nth_child = thunar_shortcuts_model_iter_nth_child;
  iface->iter_parent = thunar_shortcuts_model_iter_parent;
}



static void
thunar_shortcuts_model_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = thunar_shortcuts_model_row_draggable;
  iface->drag_data_get = thunar_shortcuts_model_drag_data_get;
  iface->drag_data_delete = thunar_shortcuts_model_drag_data_delete;
}



static void
thunar_shortcuts_model_init (ThunarShortcutsModel *model)
{
#ifndef NDEBUG
  model->stamp = g_random_int ();
#endif

  /* hidden bookmarks */
  model->preferences = thunar_preferences_get ();
  g_object_bind_property (model->preferences, "hidden-bookmarks",
                          model,              "hidden-bookmarks",
                          G_BINDING_SYNC_CREATE);

  /* binary file size */
  g_object_bind_property (model->preferences, "misc-file-size-binary",
                          model,              "file-size-binary",
                          G_BINDING_SYNC_CREATE);

  /* load volumes */
  thunar_shortcuts_model_shortcut_devices (model);

  /* add network */
  thunar_shortcuts_model_shortcut_network (model);

  /* add bookmarks */
  thunar_shortcuts_model_shortcut_places (model);

  /* connect to the file monitor */
  model->file_monitor = thunar_file_monitor_get_default ();
  g_signal_connect (G_OBJECT (model->file_monitor), "file-changed", G_CALLBACK (thunar_shortcuts_model_file_changed), model);
  g_signal_connect (G_OBJECT (model->file_monitor), "file-destroyed", G_CALLBACK (thunar_shortcuts_model_file_destroyed), model);
}



static void
thunar_shortcuts_model_finalize (GObject *object)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (object);

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  /* stop the busy timeout */
  if (model->busy_timeout_id != 0)
    g_source_remove (model->busy_timeout_id);

  /* stop bookmark load idle */
  if (model->bookmarks_idle_id != 0)
    g_source_remove (model->bookmarks_idle_id);

  /* free all shortcuts */
  g_list_foreach (model->shortcuts, (GFunc) (void (*)(void)) thunar_shortcut_free, model);
  g_list_free (model->shortcuts);

  /* disconnect from the preferences */
  g_object_unref (model->preferences);

  /* free hidden list */
  g_strfreev (model->hidden_bookmarks);

  /* disconnect from the file monitor */
  g_signal_handlers_disconnect_by_func (model->file_monitor, thunar_shortcuts_model_file_changed, model);
  g_signal_handlers_disconnect_by_func (model->file_monitor, thunar_shortcuts_model_file_destroyed, model);
  g_object_unref (model->file_monitor);

  /* detach from the file monitor */
  if (model->bookmarks_monitor != NULL)
    {
      g_file_monitor_cancel (model->bookmarks_monitor);
      g_object_unref (model->bookmarks_monitor);
    }

  if (model->bookmarks_file != NULL)
    g_object_unref (model->bookmarks_file);

  /* unlink from the device monitor */
  g_signal_handlers_disconnect_matched (model->device_monitor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);
  g_object_unref (model->device_monitor);

  (*G_OBJECT_CLASS (thunar_shortcuts_model_parent_class)->finalize) (object);
}



static void
thunar_shortcuts_model_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (object);

  switch (prop_id)
    {
    case PROP_HIDDEN_BOOKMARKS:
      g_value_set_boxed (value, model->hidden_bookmarks);
      break;

    case PROP_FILE_SIZE_BINARY:
      g_value_set_boolean (value, model->file_size_binary);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_shortcuts_model_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (object);
  GList                *lp;
  ThunarShortcut       *shortcut;
  gboolean              hidden;
  GtkTreeIter           iter;
  GtkTreePath          *path;
  guint                 idx;

  switch (prop_id)
    {
    case PROP_HIDDEN_BOOKMARKS:
      g_strfreev (model->hidden_bookmarks);
      model->hidden_bookmarks = g_value_dup_boxed (value);

      /* update shortcuts */
      for (lp = model->shortcuts, idx = 0; lp != NULL; lp = lp->next, idx++)
        {
          shortcut = lp->data;

          /* skip devices and headers*/
          if (shortcut->device != NULL
              || (shortcut->file == NULL && shortcut->location == NULL))
            continue;

          /* update state if required */
          hidden = thunar_shortcuts_model_get_hidden (model, shortcut);
          if (shortcut->hidden != hidden)
            {
              shortcut->hidden = hidden;

              GTK_TREE_ITER_INIT (iter, model->stamp, lp);

              path = gtk_tree_path_new_from_indices (idx, -1);
              gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
              gtk_tree_path_free (path);
            }
        }

      /* update header visibility */
      thunar_shortcuts_model_header_visibility (model);
      break;

    case PROP_FILE_SIZE_BINARY:
      model->file_size_binary = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static GtkTreeModelFlags
thunar_shortcuts_model_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}



static gint
thunar_shortcuts_model_get_n_columns (GtkTreeModel *tree_model)
{
  return THUNAR_SHORTCUTS_MODEL_N_COLUMNS;
}



static GType
thunar_shortcuts_model_get_column_type (GtkTreeModel *tree_model,
                                        gint          idx)
{
  switch (idx)
    {
    case THUNAR_SHORTCUTS_MODEL_COLUMN_IS_HEADER:
      return G_TYPE_BOOLEAN;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_IS_ITEM:
      return G_TYPE_BOOLEAN;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_VISIBLE:
      return G_TYPE_BOOLEAN;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_NAME:
      return G_TYPE_STRING;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_TOOLTIP:
      return G_TYPE_STRING;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_FILE:
      return THUNAR_TYPE_FILE;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_LOCATION:
      return G_TYPE_FILE;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_GICON:
      return G_TYPE_ICON;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_DEVICE:
      return THUNAR_TYPE_DEVICE;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_MUTABLE:
      return G_TYPE_BOOLEAN;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_CAN_EJECT:
      return G_TYPE_BOOLEAN;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_GROUP:
      return G_TYPE_UINT;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_BUSY:
      return G_TYPE_BOOLEAN;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_BUSY_PULSE:
      return G_TYPE_UINT;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_HIDDEN:
      return G_TYPE_BOOLEAN;
    }

  _thunar_assert_not_reached ();
  return G_TYPE_INVALID;
}



static gboolean
thunar_shortcuts_model_get_iter (GtkTreeModel *tree_model,
                                 GtkTreeIter  *iter,
                                 GtkTreePath  *path)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (tree_model);
  GList                 *lp;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);
  _thunar_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* determine the list item for the path */
  lp = g_list_nth (model->shortcuts, gtk_tree_path_get_indices (path)[0]);
  if (G_LIKELY (lp != NULL))
    {
      GTK_TREE_ITER_INIT (*iter, model->stamp, lp);
      return TRUE;
    }

  return FALSE;
}



static GtkTreePath*
thunar_shortcuts_model_get_path (GtkTreeModel *tree_model,
                                 GtkTreeIter  *iter)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (tree_model);
  gint                  idx;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), NULL);
  _thunar_return_val_if_fail (iter->stamp == model->stamp, NULL);

  /* lookup the list item in the shortcuts list */
  idx = g_list_position (model->shortcuts, iter->user_data);
  if (G_LIKELY (idx >= 0))
    return gtk_tree_path_new_from_indices (idx, -1);

  return NULL;
}



static void
thunar_shortcuts_model_get_value (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter,
                                  gint          column,
                                  GValue       *value)
{
  ThunarShortcut *shortcut;
  GFile          *file;
  gboolean        can_eject;
  gboolean        file_size_binary;
  gchar          *disk_usage;
  gchar          *device_name;
  gchar          *device_id;
  gchar          *location;
  gchar          *tooltip;
  guint32         trash_items;
  gchar          *parse_name;

  _thunar_return_if_fail (iter->stamp == THUNAR_SHORTCUTS_MODEL (tree_model)->stamp);
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (tree_model));

  /* determine the shortcut for the list item */
  shortcut = THUNAR_SHORTCUT (((GList *) iter->user_data)->data);
  if (shortcut == NULL)
    return;

  switch (column)
    {
    case THUNAR_SHORTCUTS_MODEL_COLUMN_IS_HEADER:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, (shortcut->group & THUNAR_SHORTCUT_GROUP_HEADER) != 0);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_IS_ITEM:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, (shortcut->group & THUNAR_SHORTCUT_GROUP_HEADER) == 0);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_VISIBLE:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, !shortcut->hidden);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      if (G_UNLIKELY (shortcut->device != NULL))
        g_value_take_string (value, thunar_device_get_name (shortcut->device));
      else if (shortcut->name != NULL)
        g_value_set_static_string (value, shortcut->name);
      else if (shortcut->file != NULL)
        g_value_set_static_string (value, thunar_file_get_display_name (shortcut->file));
      else if (shortcut->location != NULL)
        g_value_take_string (value, thunar_g_file_get_display_name_remote (shortcut->location));
      else
        g_value_set_static_string (value, "");
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_TOOLTIP:
      g_value_init (value, G_TYPE_STRING);
      if ((shortcut->group & THUNAR_SHORTCUT_GROUP_DEVICES) != 0)
        {
          /* get Unix device path */
          if (shortcut->device != NULL)
            device_id = thunar_device_get_identifier_unix (shortcut->device);
          else
            device_id = NULL;

          /* get device root directory */
          if (shortcut->device != NULL)
            file = thunar_device_get_root (shortcut->device);
          else if (shortcut->file != NULL)
            file = g_object_ref (thunar_file_get_file (shortcut->file));
          else
            file = NULL;

          if (file != NULL)
            {
              if (g_file_has_uri_scheme (file, "file") && thunar_g_file_is_root (file))
                location = g_strdup (_("Browse the file system"));
              else if (!g_file_has_uri_scheme (file, "file"))
                location = g_file_get_uri (file);
              else
                location = thunar_g_file_get_location (file);

              if (device_id != NULL)
                {
                  gchar *tmp = g_strdup_printf ("%s (%s)", location, device_id);
                  g_free (location);
                  g_free (device_id);
                  location = tmp;
                }

              file_size_binary = THUNAR_SHORTCUTS_MODEL (tree_model)->file_size_binary;
              disk_usage = thunar_g_file_get_free_space_string (file, file_size_binary);

              if (disk_usage != NULL)
                tooltip = g_strdup_printf ("%s\n%s", location, disk_usage);
              else
                tooltip = g_strdup_printf ("%s", location);

              g_value_take_string (value, tooltip);

              g_free (location);
              g_free (disk_usage);

              g_object_unref (file);
            }
          else
            {
              if (device_id != NULL)
                {
                  device_name = thunar_device_get_name (shortcut->device);
                  tooltip = g_strdup_printf (_("Mount and open %s (%s)"), device_name, device_id);
                  g_value_take_string (value, tooltip);
                  g_free (device_name);
                  g_free (device_id);
                }
            }
          break;
        }
      else if ((shortcut->group & THUNAR_SHORTCUT_GROUP_PLACES_TRASH) != 0)
        {
          trash_items = thunar_file_get_item_count (shortcut->file);
          if (trash_items == 0)
            {
              g_value_set_static_string (value, _("Trash is empty"));
            }
          else
            {
              tooltip = g_strdup_printf (ngettext ("Trash contains %d file",
                                                   "Trash contains %d files",
                                                   trash_items), trash_items);
              g_value_take_string (value, tooltip);
            }
          break;
        }
      else if (shortcut->tooltip == NULL)
        {
          if (shortcut->device != NULL)
            {
              file = thunar_device_get_root (shortcut->device);
              if (G_LIKELY (file != NULL))
                {
                  shortcut->tooltip = g_file_get_uri (file);
                  g_object_unref (file);
                }
            }
          else
            {
              if (shortcut->file != NULL)
                file = thunar_file_get_file (shortcut->file);
              else if (shortcut->location != NULL)
                file = shortcut->location;
              else
                file = NULL;

              if (G_LIKELY (file != NULL))
                {
                  parse_name = g_file_get_parse_name (file);
                  shortcut->tooltip = g_markup_escape_text (parse_name, -1);
                  g_free (parse_name);
                }
            }
        }

      g_value_set_static_string (value, shortcut->tooltip);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_FILE:
      g_value_init (value, THUNAR_TYPE_FILE);
      g_value_set_object (value, shortcut->file);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_GICON:
      g_value_init (value, G_TYPE_ICON);
      g_value_set_object (value, shortcut->gicon);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_DEVICE:
      g_value_init (value, THUNAR_TYPE_DEVICE);
      g_value_set_object (value, shortcut->device);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_LOCATION:
      g_value_init (value, G_TYPE_FILE);
      if (shortcut->location != NULL)
        g_value_set_object (value, shortcut->location);
      else if (shortcut->file != NULL)
        g_value_set_object (value, thunar_file_get_file (shortcut->file));
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_MUTABLE:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value,
                           shortcut->group == THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_CAN_EJECT:
      if (shortcut->device != NULL)
        can_eject = thunar_device_can_eject (shortcut->device);
      else
        can_eject = FALSE;

      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, can_eject);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_GROUP:
      g_value_init (value, G_TYPE_UINT);
      g_value_set_uint (value, shortcut->group);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_BUSY:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, shortcut->busy);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_BUSY_PULSE:
      g_value_init (value, G_TYPE_UINT);
      g_value_set_uint (value, shortcut->busy_pulse);
      break;

    case THUNAR_SHORTCUTS_MODEL_COLUMN_HIDDEN:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, FALSE);
      break;

    default:
      _thunar_assert_not_reached ();
    }
}



static gboolean
thunar_shortcuts_model_iter_next (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter)
{
  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (tree_model), FALSE);
  _thunar_return_val_if_fail (iter->stamp == THUNAR_SHORTCUTS_MODEL (tree_model)->stamp, FALSE);

  iter->user_data = g_list_next (iter->user_data);
  return (iter->user_data != NULL);
}



static gboolean
thunar_shortcuts_model_iter_children (GtkTreeModel *tree_model,
                                      GtkTreeIter  *iter,
                                      GtkTreeIter  *parent)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (tree_model);

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);

  if (G_LIKELY (parent == NULL && model->shortcuts != NULL))
    {
      GTK_TREE_ITER_INIT (*iter, model->stamp, model->shortcuts);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_shortcuts_model_iter_has_child (GtkTreeModel *tree_model,
                                       GtkTreeIter  *iter)
{
  return FALSE;
}



static gint
thunar_shortcuts_model_iter_n_children (GtkTreeModel *tree_model,
                                        GtkTreeIter  *iter)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (tree_model);

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), 0);

  return (iter == NULL) ? g_list_length (model->shortcuts) : 0;
}



static gboolean
thunar_shortcuts_model_iter_nth_child (GtkTreeModel *tree_model,
                                       GtkTreeIter  *iter,
                                       GtkTreeIter  *parent,
                                       gint          n)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (tree_model);

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);

  if (G_LIKELY (parent == NULL))
    {
      GTK_TREE_ITER_INIT (*iter, model->stamp, g_list_nth (model->shortcuts, n));
      return (iter->user_data != NULL);
    }

  return FALSE;
}



static gboolean
thunar_shortcuts_model_iter_parent (GtkTreeModel *tree_model,
                                    GtkTreeIter  *iter,
                                    GtkTreeIter  *child)
{
  return FALSE;
}



static gboolean
thunar_shortcuts_model_row_draggable (GtkTreeDragSource *source,
                                      GtkTreePath       *path)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (source);
  ThunarShortcut       *shortcut;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);
  _thunar_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* lookup the ThunarShortcut for the path */
  shortcut = g_list_nth_data (model->shortcuts, gtk_tree_path_get_indices (path)[0]);

  /* special shortcuts cannot be reordered */
  return (shortcut != NULL && shortcut->group == THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS);
}



static gboolean
thunar_shortcuts_model_drag_data_get (GtkTreeDragSource *source,
                                      GtkTreePath       *path,
                                      GtkSelectionData  *selection_data)
{
  /* we simply return FALSE here, as the drag handling is done in
   * the ThunarShortcutsView class.
   */
  return FALSE;
}



static gboolean
thunar_shortcuts_model_drag_data_delete (GtkTreeDragSource *source,
                                         GtkTreePath       *path)
{
  /* we simply return FALSE here, as this function can only be
   * called if the user is re-arranging shortcuts within the
   * model, which will be handle by the exchange method.
   */
  return FALSE;
}



static void
thunar_shortcuts_model_header_visibility (ThunarShortcutsModel *model)
{
  GList          *lp;
  ThunarShortcut *shortcut;
  ThunarShortcut *header;
  guint           n_children = 0;
  GList          *lp_header = NULL;
  gint            idx_header = 0;
  gint            i;
  GtkTreePath    *path;
  GtkTreeIter     iter;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  for (lp = model->shortcuts, i = 0; lp != NULL; lp = lp->next, i++)
    {
      shortcut = lp->data;

      if ((shortcut->group & THUNAR_SHORTCUT_GROUP_HEADER) != 0)
        {
          if (lp_header != NULL)
            {
              update_header:

              header = lp_header->data;
              if (header->hidden != (n_children == 0))
                {
                  /* set new visibility */
                  header->hidden = (n_children == 0);

                  /* notify view */
                  path = gtk_tree_path_new_from_indices (idx_header, -1);
                  GTK_TREE_ITER_INIT (iter, model->stamp, lp_header);
                  gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
                  gtk_tree_path_free (path);
                }

              if (lp == NULL)
                return;
            }

          /* reset for new */
          idx_header = i;
          lp_header = lp;
          n_children = 0;
        }
      else if (!shortcut->hidden)
        {
          n_children++;
        }
    }

  _thunar_assert (lp == NULL);
  if (lp_header != NULL)
    goto update_header;
}



static void
thunar_shortcuts_model_shortcut_devices (ThunarShortcutsModel *model)
{
  ThunarShortcut *shortcut;
  GList          *devices;
  GList          *lp;

  /* add the devices heading */
  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->group = THUNAR_SHORTCUT_GROUP_DEVICES_HEADER;
  shortcut->name = g_strdup (_("Devices"));
  thunar_shortcuts_model_add_shortcut (model, shortcut);

  /* the filesystem entry */
  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->group = THUNAR_SHORTCUT_GROUP_DEVICES_FILESYSTEM;
  shortcut->name = g_strdup (_("File System"));
  shortcut->tooltip = g_strdup (_("Browse the file system"));
  shortcut->file = thunar_file_get_for_uri ("file:///", NULL);
  shortcut->gicon = g_themed_icon_new ("drive-harddisk");
  shortcut->hidden = thunar_shortcuts_model_get_hidden (model, shortcut);
  thunar_shortcuts_model_add_shortcut (model, shortcut);

  /* connect to the device monitor */
  model->device_monitor = thunar_device_monitor_get ();

  /* get a list of all devices available */
  devices = thunar_device_monitor_get_devices (model->device_monitor);
  for (lp = devices; lp != NULL; lp = lp->next)
    {
      thunar_shortcuts_model_device_added (NULL, lp->data, model);
      g_object_unref (G_OBJECT (lp->data));
    }
  g_list_free (devices);

  /* monitor for changes */
  g_signal_connect (model->device_monitor, "device-added", G_CALLBACK (thunar_shortcuts_model_device_added), model);
  g_signal_connect (model->device_monitor, "device-removed", G_CALLBACK (thunar_shortcuts_model_device_removed), model);
  g_signal_connect (model->device_monitor, "device-changed", G_CALLBACK (thunar_shortcuts_model_device_changed), model);

  thunar_shortcuts_model_header_visibility (model);
}



static void
thunar_shortcuts_model_shortcut_network (ThunarShortcutsModel *model)
{
  ThunarShortcut *shortcut;

  /* add the network heading */
  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->group = THUNAR_SHORTCUT_GROUP_NETWORK_HEADER;
  shortcut->name = g_strdup (_("Network"));
  thunar_shortcuts_model_add_shortcut (model, shortcut);

  /* append the browse network entry if it is supported */
  if (thunar_g_vfs_is_uri_scheme_supported ("network"))
    {
      shortcut = g_slice_new0 (ThunarShortcut);
      shortcut->group = THUNAR_SHORTCUT_GROUP_NETWORK_DEFAULT;
      shortcut->name = g_strdup (_("Browse Network"));
      shortcut->tooltip = g_strdup (_("Browse local network connections"));
      shortcut->location = thunar_g_file_new_for_network();
      shortcut->gicon = g_themed_icon_new ("network-workgroup");
      shortcut->hidden = thunar_shortcuts_model_get_hidden (model, shortcut);
      thunar_shortcuts_model_add_shortcut (model, shortcut);
    }
}



static void
thunar_shortcuts_model_shortcut_places (ThunarShortcutsModel *model)
{
  ThunarShortcut *shortcut;
  GFile          *home;
  GFile          *desktop;
  GFile          *trash;
  ThunarFile     *file;

  /* add the places heading */
  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->group = THUNAR_SHORTCUT_GROUP_PLACES_HEADER;
  shortcut->name = g_strdup (_("Places"));
  thunar_shortcuts_model_add_shortcut (model, shortcut);

  /* get home path */
  home = thunar_g_file_new_for_home ();

  /* add home entry */
  file = thunar_file_get (home, NULL);
  if (file != NULL)
    {
      shortcut = g_slice_new0 (ThunarShortcut);
      shortcut->group = THUNAR_SHORTCUT_GROUP_PLACES_DEFAULT;
      shortcut->tooltip = g_strdup (_("Open the home folder"));
      shortcut->file = file;
      shortcut->gicon = g_themed_icon_new ("go-home");
      shortcut->sort_id = 0;
      shortcut->hidden = thunar_shortcuts_model_get_hidden (model, shortcut);
      thunar_shortcuts_model_add_shortcut (model, shortcut);
    }

  /* add desktop entry */
  desktop = thunar_g_file_new_for_desktop ();
  if (!g_file_equal (desktop, home))
    {
      file = thunar_file_get (desktop, NULL);
      if (file != NULL)
        {
          shortcut = g_slice_new0 (ThunarShortcut);
          shortcut->group = THUNAR_SHORTCUT_GROUP_PLACES_DEFAULT;
          shortcut->tooltip = g_strdup (_("Open the desktop folder"));
          shortcut->file = file;
          shortcut->sort_id =  1;
          shortcut->hidden = thunar_shortcuts_model_get_hidden (model, shortcut);
          thunar_shortcuts_model_add_shortcut (model, shortcut);
        }
    }
  g_object_unref (desktop);
  g_object_unref (home);

  /* append the trash icon if the trash is supported */
  if (thunar_g_vfs_is_uri_scheme_supported ("trash"))
    {
      trash = thunar_g_file_new_for_trash ();
      file = thunar_file_get (trash, NULL);
      g_object_unref (trash);

      if (file != NULL)
        {
          shortcut = g_slice_new0 (ThunarShortcut);
          shortcut->group = THUNAR_SHORTCUT_GROUP_PLACES_TRASH;
          shortcut->name = g_strdup (_("Trash"));
          shortcut->file = file;
          shortcut->hidden = thunar_shortcuts_model_get_hidden (model, shortcut);
          thunar_shortcuts_model_add_shortcut (model, shortcut);
        }
    }

  /* determine the URI to the Gtk+ bookmarks file */
  model->bookmarks_file = thunar_g_file_new_for_bookmarks ();

  /* register with the alteration monitor for the bookmarks file */
  model->bookmarks_monitor = g_file_monitor_file (model->bookmarks_file, G_FILE_MONITOR_NONE, NULL, NULL);
  if (G_LIKELY (model->bookmarks_monitor != NULL))
    {
      g_signal_connect (model->bookmarks_monitor, "changed",
        G_CALLBACK (thunar_shortcuts_model_monitor), model);
    }

  /* read the Gtk+ bookmarks file */
  model->bookmarks_idle_id = g_idle_add_full (G_PRIORITY_DEFAULT, thunar_shortcuts_model_load, model, NULL);

  /* append the computer icon if browsing the computer is supported */
  if (thunar_g_vfs_is_uri_scheme_supported ("computer"))
    {
      shortcut = g_slice_new0 (ThunarShortcut);
      shortcut->group = THUNAR_SHORTCUT_GROUP_PLACES_COMPUTER;
      shortcut->name = g_strdup (_("Computer"));
      shortcut->tooltip = g_strdup (_("Browse the computer"));
      shortcut->location = thunar_g_file_new_for_computer();
      shortcut->gicon = g_themed_icon_new ("computer");
      shortcut->hidden = thunar_shortcuts_model_get_hidden (model, shortcut);
      thunar_shortcuts_model_add_shortcut (model, shortcut);
    }

  /* append the recent icon if browsing recent is supported */
  if (thunar_g_vfs_is_uri_scheme_supported ("recent"))
    {
      shortcut = g_slice_new0 (ThunarShortcut);
      shortcut->group = THUNAR_SHORTCUT_GROUP_PLACES_RECENT;
      shortcut->name = g_strdup (_("Recent"));
      shortcut->tooltip = g_strdup (_("Browse recently used files"));
      shortcut->location = thunar_g_file_new_for_recent();
      shortcut->gicon = g_themed_icon_new ("document-open-recent");
      shortcut->hidden = thunar_shortcuts_model_get_hidden (model, shortcut);
      thunar_shortcuts_model_add_shortcut (model, shortcut);
    }
}



static gboolean
thunar_shortcuts_model_get_hidden (ThunarShortcutsModel *model,
                                   ThunarShortcut       *shortcut)
{
  gchar    *uri;
  guint     n;
  gboolean  hidden = FALSE;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);
  _thunar_return_val_if_fail (shortcut->device == NULL, FALSE);

  if (model->hidden_bookmarks == NULL)
    return FALSE;

  /* get the uri */
  if (shortcut->file != NULL)
    uri = thunar_file_dup_uri (shortcut->file);
  else if (shortcut->location != NULL)
    uri = g_file_get_uri (shortcut->location);
  else
    {
      g_warn_if_reached ();
      return FALSE;
    }

  if (uri == NULL)
    return FALSE;

  for (n = 0; !hidden && model->hidden_bookmarks[n] != NULL; n++)
    hidden = (g_strcmp0 (model->hidden_bookmarks[n], uri) == 0);

  g_free (uri);

  return hidden;
}



static gint
thunar_shortcuts_model_sort_func (gconstpointer shortcut_a,
                                  gconstpointer shortcut_b)
{
  const ThunarShortcut *a = shortcut_a;
  const ThunarShortcut *b = shortcut_b;

  /* sort groups */
  if (a->group != b->group)
    return a->group - b->group;

  /* use sort order */
  if (a->sort_id != b->sort_id)
    return a->sort_id > b->sort_id ? 1 : -1;

  if (a->device != NULL && b->device != NULL)
    return thunar_device_compare_by_name (a->device, b->device);

  return g_strcmp0 (a->name, b->name);
}



static void
thunar_shortcuts_model_add_shortcut_with_path (ThunarShortcutsModel *model,
                                               ThunarShortcut       *shortcut,
                                               GtkTreePath          *path)
{
  GtkTreeIter  iter;
  GtkTreePath *sorted_path = NULL;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (shortcut->file == NULL || THUNAR_IS_FILE (shortcut->file));

  if (G_LIKELY (shortcut->file != NULL))
    {
      /* watch the trash for changes */
      if (thunar_file_is_trash (shortcut->file))
        thunar_file_watch (shortcut->file);
    }

  if (path == NULL)
    {
      /* insert the new shortcut to the shortcuts list */
      model->shortcuts = g_list_insert_sorted (model->shortcuts, shortcut, thunar_shortcuts_model_sort_func);
      sorted_path = gtk_tree_path_new_from_indices (g_list_index (model->shortcuts, shortcut), -1);
      path = sorted_path;
    }
  else
    {
      model->shortcuts = g_list_insert (model->shortcuts, shortcut, gtk_tree_path_get_indices (path)[0]);
    }

  /* tell everybody that we have a new shortcut */
  gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);

  if (sorted_path)
    gtk_tree_path_free (sorted_path);
}



static void
thunar_shortcuts_model_add_shortcut (ThunarShortcutsModel *model,
                                     ThunarShortcut       *shortcut)
{
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (shortcut->file == NULL || THUNAR_IS_FILE (shortcut->file));

  thunar_shortcuts_model_add_shortcut_with_path (model, shortcut, NULL);
}



static void
thunar_shortcuts_model_remove_shortcut (ThunarShortcutsModel *model,
                                        ThunarShortcut       *shortcut)
{
  GtkTreePath *path;
  gint         idx;
  gboolean     needs_save;

  /* determine the index of the shortcut */
  idx = g_list_index (model->shortcuts, shortcut);
  if (G_LIKELY (idx >= 0))
    {
      /* unlink the shortcut from the model */
      model->shortcuts = g_list_remove (model->shortcuts, shortcut);

      /* tell everybody that we have lost a shortcut */
      path = gtk_tree_path_new_from_indices (idx, -1);
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
      gtk_tree_path_free (path);

      /* check if we need to save */
      needs_save = (shortcut->group == THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS);

      /* actually free the shortcut */
      thunar_shortcut_free (shortcut, model);

      /* the shortcuts list was changed, so write the gtk bookmarks file */
      if (needs_save)
        thunar_shortcuts_model_save (model);

      /* update header visibility */
      thunar_shortcuts_model_header_visibility (model);
    }
}



static gboolean
thunar_shortcuts_model_local_file (GFile *gfile)
{
  _thunar_return_val_if_fail (G_IS_FILE (gfile), FALSE);

  /* schemes we'd like to have as ThunarFiles in the model */
  if (g_file_has_uri_scheme (gfile, "file")
      || g_file_has_uri_scheme (gfile, "computer")
      || g_file_has_uri_scheme (gfile, "recent"))
    return TRUE;

  return FALSE;
}



static void
thunar_shortcuts_model_load_line (GFile       *file_path,
                                  const gchar *name,
                                  gint         row_num,
                                  gpointer     user_data)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (user_data);
  ThunarShortcut       *shortcut;
  ThunarFile           *file;

  _thunar_return_if_fail (G_IS_FILE (file_path));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (name == NULL || g_utf8_validate (name, -1, NULL));

  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->group = THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS;
  shortcut->location = g_object_ref (file_path);

  /* handle local and remote files differently */
  /* If we dont have a thunar-file, we need to set the gicon manually */
  if (thunar_shortcuts_model_local_file (file_path))
    {
      /* try to open the file corresponding to the uri */
      file = thunar_file_get (file_path, NULL);
      if (G_UNLIKELY (file == NULL))
          shortcut->gicon = g_themed_icon_new ("folder");
      else
          shortcut->file = file;
    }
  else
    {
      shortcut->gicon = g_themed_icon_new ("folder-remote");
    }

  shortcut->sort_id = row_num;
  shortcut->hidden = thunar_shortcuts_model_get_hidden (model, shortcut);
  shortcut->name = g_strdup (name);

  /* append the shortcut to the list */
  thunar_shortcuts_model_add_shortcut (model, shortcut);
}



static gboolean
thunar_shortcuts_model_load (gpointer data)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (data);

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);

THUNAR_THREADS_ENTER

  /* parse the bookmarks */
  thunar_util_load_bookmarks (model->bookmarks_file,
                              thunar_shortcuts_model_load_line,
                              model);

  /* update the visibility */
  thunar_shortcuts_model_header_visibility (model);

THUNAR_THREADS_LEAVE

  model->bookmarks_idle_id = 0;

  return FALSE;
}



static gboolean
thunar_shortcuts_model_reload (gpointer data)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (data);
  ThunarShortcut       *shortcut;
  GtkTreePath          *path;
  GList                *lp;
  gint                  idx;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);

THUNAR_THREADS_ENTER

  /* drop all existing user-defined shortcuts from the model */
  for (idx = 0, lp = model->shortcuts; lp != NULL; )
    {
      /* grab the shortcut */
      shortcut = THUNAR_SHORTCUT (lp->data);

      /* advance to the next list item */
      lp = g_list_next (lp);

      /* drop the shortcut if it is user-defined */
      if (shortcut->group == THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS)
        {
          /* unlink the shortcut from the model */
          model->shortcuts = g_list_remove (model->shortcuts, shortcut);

          /* tell everybody that we have lost a shortcut */
          path = gtk_tree_path_new_from_indices (idx, -1);
          gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
          gtk_tree_path_free (path);

          /* actually free the shortcut */
          thunar_shortcut_free (shortcut, model);
        }
      else
        {
          ++idx;
        }
    }

THUNAR_THREADS_LEAVE

  /* load new bookmarks */
  return thunar_shortcuts_model_load (data);
}



static void
thunar_shortcuts_model_monitor (GFileMonitor     *monitor,
                                GFile            *file,
                                GFile            *other_file,
                                GFileMonitorEvent event_type,
                                gpointer          user_data)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (user_data);

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (model->bookmarks_monitor == monitor);

  /* leave if we saved less than 2 seconds ago */
  if (model->bookmarks_time + 2 * G_USEC_PER_SEC > g_get_real_time ())
    return;

  /* reload the shortcuts model */
  if (model->bookmarks_idle_id == 0)
    model->bookmarks_idle_id = g_idle_add (thunar_shortcuts_model_reload, model);
}



static void
thunar_shortcuts_model_save (ThunarShortcutsModel *model)
{
  GString        *contents;
  ThunarShortcut *shortcut;
  gchar          *bookmarks_path;
  gchar          *uri;
  GList          *lp;
  GError         *err = NULL;
  GFile          *parent = NULL;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  contents = g_string_new (NULL);

  for (lp = model->shortcuts; lp != NULL; lp = lp->next)
    {
      shortcut = THUNAR_SHORTCUT (lp->data);
      if (shortcut->group == THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS)
        {
          if (shortcut->file != NULL)
            uri = thunar_file_dup_uri (shortcut->file);
          else if (shortcut->location != NULL)
            uri = g_file_get_uri (shortcut->location);
          else
            continue;

          if (G_LIKELY (shortcut->name != NULL))
            g_string_append_printf (contents, "%s %s\n", uri, shortcut->name);
          else
            g_string_append_printf (contents, "%s\n", uri);
          g_free (uri);
        }
    }

  /* create folder if it does not exist */
  parent = g_file_get_parent (model->bookmarks_file);
  if (!g_file_make_directory_with_parents (parent, NULL, &err))
    {
       if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_EXISTS))
         {
           g_clear_error (&err);
         }
       else
         {
           g_warning ("Failed to create bookmarks folder: %s", err->message);
           g_error_free (err);
         }
    }
  g_clear_object (&parent);

  /* write data to the disk */
  bookmarks_path = g_file_get_path (model->bookmarks_file);
  if (!g_file_set_contents (bookmarks_path, contents->str, contents->len, &err))
    {
      g_warning ("Failed to write \"%s\": %s", bookmarks_path, err->message);
      g_error_free (err);
    }
  g_free (bookmarks_path);
  g_string_free (contents, TRUE);

  /* store the save time */
  model->bookmarks_time = g_get_real_time ();
}



static void
thunar_shortcuts_model_device_added (ThunarDeviceMonitor  *device_monitor,
                                     ThunarDevice         *device,
                                     ThunarShortcutsModel *model)
{
  ThunarShortcut *shortcut;
  GFile          *mount_point;

  _thunar_return_if_fail (device_monitor == NULL || THUNAR_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (device_monitor == NULL || model->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  /* allocate a new shortcut */
  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->device = g_object_ref (device);
  shortcut->hidden = thunar_device_get_hidden (device);

  mount_point = thunar_device_get_root (device);
  if (mount_point != NULL)
    {
      shortcut->file = thunar_file_get (mount_point, NULL);
      g_object_unref (mount_point);
    }

  switch (thunar_device_get_kind (device))
    {
    case THUNAR_DEVICE_KIND_VOLUME:
      shortcut->group = THUNAR_SHORTCUT_GROUP_DEVICES_VOLUMES;
      break;

    case THUNAR_DEVICE_KIND_MOUNT_LOCAL:
      shortcut->group = THUNAR_SHORTCUT_GROUP_DEVICES_MOUNTS;
      break;

    case THUNAR_DEVICE_KIND_MOUNT_REMOTE:
      shortcut->group = THUNAR_SHORTCUT_GROUP_NETWORK_MOUNTS;
      break;
    }

  /* insert in the model */
  thunar_shortcuts_model_add_shortcut (model, shortcut);

  /* header visibility if call is from monitor */
  if (device_monitor != NULL
      && !shortcut->hidden)
    thunar_shortcuts_model_header_visibility (model);
}



static void
thunar_shortcuts_model_device_removed (ThunarDeviceMonitor  *device_monitor,
                                       ThunarDevice         *device,
                                       ThunarShortcutsModel *model)
{
  GList *lp;

  _thunar_return_if_fail (THUNAR_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (model->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  /* find the shortcut */
  for (lp = model->shortcuts; lp != NULL; lp = lp->next)
    if (THUNAR_SHORTCUT (lp->data)->device == device)
      break;

  /* something is broken if we don't have a shortcut here */
  _thunar_assert (lp != NULL);
  _thunar_assert (THUNAR_SHORTCUT (lp->data)->device == device);

  /* drop the shortcut from the model */
  if (G_LIKELY (lp != NULL))
    thunar_shortcuts_model_remove_shortcut (model, lp->data);
}



static void
thunar_shortcuts_model_device_changed (ThunarDeviceMonitor  *device_monitor,
                                       ThunarDevice         *device,
                                       ThunarShortcutsModel *model)
{
  GtkTreeIter     iter;
  GList          *lp;
  gint            idx;
  GtkTreePath    *path;
  ThunarShortcut *shortcut;
  GFile          *mount_point;
  gboolean        update_header = FALSE;

  _thunar_return_if_fail (THUNAR_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (model->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));

  /* find the shortcut */
  for (lp = model->shortcuts, idx = 0; lp != NULL; lp = lp->next, idx++)
    if (THUNAR_SHORTCUT (lp->data)->device == device)
      break;

  /* something is broken if we don't have a shortcut here */
  _thunar_assert (lp != NULL);
  _thunar_assert (THUNAR_SHORTCUT (lp->data)->device == device);

  if (G_LIKELY (lp != NULL))
    {
      shortcut = lp->data;

      /* cleanup tooltip */
      g_free (shortcut->tooltip);
      shortcut->tooltip = NULL;

      if (shortcut->file == NULL)
        {
          mount_point = thunar_device_get_root (device);
          if (mount_point != NULL)
            {
              shortcut->file = thunar_file_get (mount_point, NULL);
              g_object_unref (mount_point);
            }
        }

      /* hidden state */
      if (shortcut->hidden != thunar_device_get_hidden (device))
        {
          shortcut->hidden = thunar_device_get_hidden (device);
          update_header = TRUE;
        }

      /* generate an iterator for the path */
      GTK_TREE_ITER_INIT (iter, model->stamp, lp);

      /* tell the view that the volume has changed in some way */
      path = gtk_tree_path_new_from_indices (idx, -1);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
      gtk_tree_path_free (path);
    }

  /* header visibility */
  if (update_header)
    thunar_shortcuts_model_header_visibility (model);
}



static void
thunar_shortcuts_model_file_destroyed (ThunarFileMonitor      *file_monitor,
                                       ThunarFile             *file,
                                       ThunarShortcutsModel   *model)
{
  GtkTreeIter     iter;
  GList          *lp;
  gint            idx;
  GtkTreePath    *path;

    /* find the shortcut */
  for (lp = model->shortcuts, idx = 0; lp != NULL; lp = lp->next, idx++)
    if (THUNAR_SHORTCUT (lp->data)->file == file)
      break;

  if (G_LIKELY (lp != NULL))
    {
      /* remove the thunar file from the list and set the g_file instead */
      GFile *g_file = thunar_file_get_file (file);
      THUNAR_SHORTCUT (lp->data)->file = NULL;
      THUNAR_SHORTCUT (lp->data)->location = g_file;
      if (thunar_shortcuts_model_local_file (g_file))
        THUNAR_SHORTCUT (lp->data)->gicon = g_themed_icon_new ("folder");
      else
        THUNAR_SHORTCUT (lp->data)->gicon = g_themed_icon_new ("folder-remote");

      /* generate an iterator for the path */
      GTK_TREE_ITER_INIT (iter, model->stamp, lp);

      /* tell the view that the file has changed in some way */
      path = gtk_tree_path_new_from_indices (idx, -1);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
      gtk_tree_path_free (path);
    }
}



static void
thunar_shortcuts_model_file_changed (ThunarFileMonitor      *file_monitor,
                                     ThunarFile             *file,
                                     ThunarShortcutsModel   *model)
{
  GtkTreeIter     iter;
  GList          *lp;
  gint            idx;
  GtkTreePath    *path;

  _thunar_return_if_fail (THUNAR_IS_FILE_MONITOR (file_monitor));
  _thunar_return_if_fail (model->file_monitor == file_monitor);
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

    /* find the shortcut */
  for (lp = model->shortcuts, idx = 0; lp != NULL; lp = lp->next, idx++)
    if (THUNAR_SHORTCUT (lp->data)->file == file)
      break;

  if (G_LIKELY (lp != NULL))
    {

      GFile *g_file = thunar_file_get_file (file);

      /* Only update the shortcut if trash has changed or the related g_file has changed (rename) */
      if (thunar_file_is_trash (file) ||
          (THUNAR_SHORTCUT (lp->data)->location != NULL && g_file_equal (g_file, THUNAR_SHORTCUT (lp->data)->location) == FALSE))
        {
          /* generate an iterator for the path */
          GTK_TREE_ITER_INIT (iter, model->stamp, lp);

          /* tell the view that the file has changed in some way */
          path = gtk_tree_path_new_from_indices (idx, -1);
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);

          /* Store the changed shortcut */
          thunar_shortcuts_model_save (model);
        }
    }
}



static void
thunar_shortcut_free (ThunarShortcut       *shortcut,
                      ThunarShortcutsModel *model)
{
  if (G_LIKELY (shortcut->file != NULL))
    {
      /* drop the file watch on trash */
      if (thunar_file_is_trash (shortcut->file))
        thunar_file_unwatch (shortcut->file);

      g_object_unref (shortcut->file);
    }

  if (G_LIKELY (shortcut->device != NULL))
    g_object_unref (shortcut->device);

  if (G_LIKELY (shortcut->gicon != NULL))
    g_object_unref (shortcut->gicon);

  if (G_LIKELY (shortcut->location != NULL))
    g_object_unref (shortcut->location);

  g_free (shortcut->name);
  g_free (shortcut->tooltip);

  /* release the shortcut itself */
  g_slice_free (ThunarShortcut, shortcut);
}



static gboolean
thunar_shortcuts_model_busy_timeout (gpointer data)
{
  ThunarShortcutsModel *model = THUNAR_SHORTCUTS_MODEL (data);
  gboolean              keep_running = FALSE;
  ThunarShortcut       *shortcut;
  GtkTreePath          *path;
  guint                 idx;
  GtkTreeIter           iter;
  GList                *lp;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (data), FALSE);

  for (lp = model->shortcuts, idx = 0; lp != NULL; lp = lp->next, idx++)
    {
      shortcut = lp->data;
      if (!shortcut->busy)
        continue;

      /* loop the pulse of the shortcut */
      shortcut->busy_pulse++;
      if (shortcut->busy_pulse >= SPINNER_NUM_STEPS)
        shortcut->busy_pulse = 0;

      /* generate an iterator for the path */
      GTK_TREE_ITER_INIT (iter, model->stamp, lp);

      /* notify the views about the change */
      path = gtk_tree_path_new_from_indices (idx, -1);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
      gtk_tree_path_free (path);

      /* keep the timeout running */
      keep_running = TRUE;
    }

  return keep_running;
}



static void
thunar_shortcuts_model_busy_timeout_destroyed (gpointer data)
{
  THUNAR_SHORTCUTS_MODEL (data)->busy_timeout_id = 0;
}



/**
 * thunar_shortcuts_model_get_default:
 *
 * Returns the default #ThunarShortcutsModel instance shared by
 * all #ThunarShortcutsView instances.
 *
 * Call #g_object_unref() on the returned object when you
 * don't need it any longer.
 *
 * Return value: the default #ThunarShortcutsModel instance.
 **/
ThunarShortcutsModel*
thunar_shortcuts_model_get_default (void)
{
  static ThunarShortcutsModel *model = NULL;

  if (G_UNLIKELY (model == NULL))
    {
      model = g_object_new (THUNAR_TYPE_SHORTCUTS_MODEL, NULL);
      g_object_add_weak_pointer (G_OBJECT (model), (gpointer) &model);
    }
  else
    {
      g_object_ref (G_OBJECT (model));
    }

  return model;
}



/**
 * thunar_shortcuts_model_has_bookmark:
 * @model : a #ThunarShortcutsModel instance.
 * @file  : a #ThuanrFile instance.
 *
 * Returns %TRUE if there is a bookmark (not a mount or volume) with
 * @file as destination.
 *
 * Return value: %TRUE if @file was found, else %FALSE.
 **/
gboolean
thunar_shortcuts_model_has_bookmark (ThunarShortcutsModel *model,
                                     GFile                *file)
{
  GList          *lp;
  ThunarShortcut *shortcut;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);
  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);

  for (lp = model->shortcuts; lp != NULL; lp = lp->next)
    {
      shortcut = lp->data;

      /* only check bookmarks */
      if (shortcut->group != THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS)
        continue;

      if (shortcut->file != NULL
          && g_file_equal (thunar_file_get_file (shortcut->file), file))
        return TRUE;

      if (shortcut->location != NULL
          && g_file_equal (shortcut->location, file))
        return TRUE;
    }

  return FALSE;
}



/**
 * thunar_shortcuts_model_iter_for_file:
 * @model : a #ThunarShortcutsModel instance.
 * @file  : a #ThuanrFile instance.
 * @iter  : pointer to a #GtkTreeIter.
 *
 * Tries to lookup the #GtkTreeIter, that belongs to a shortcut, which
 * refers to @file and stores it to @iter. If no such #GtkTreeIter was
 * found, %FALSE will be returned and @iter won't be changed. Else
 * %TRUE will be returned and @iter will be set appropriately.
 *
 * Return value: %TRUE if @file was found, else %FALSE.
 **/
gboolean
thunar_shortcuts_model_iter_for_file (ThunarShortcutsModel *model,
                                      ThunarFile           *file,
                                      GtkTreeIter          *iter)
{
  GFile          *mount_point;
  GList          *lp;
  ThunarShortcut *shortcut;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (iter != NULL, FALSE);

  for (lp = model->shortcuts; lp != NULL; lp = lp->next)
    {
      shortcut = lp->data;

      /* check if we have a location that matches */
      if (shortcut->file == file)
        {
          GTK_TREE_ITER_INIT (*iter, model->stamp, lp);
          return TRUE;
        }

      if (shortcut->location != NULL
          && g_file_equal (shortcut->location, thunar_file_get_file (file)))
        {
          GTK_TREE_ITER_INIT (*iter, model->stamp, lp);
          return TRUE;
        }

      /* but maybe we have a mounted(!) volume with a matching mount point */
      if (shortcut->device != NULL
          && !thunar_device_get_hidden (shortcut->device))
        {
          mount_point = thunar_device_get_root (shortcut->device);
          if (mount_point != NULL)
            {
              if (g_file_equal (mount_point, thunar_file_get_file (file)))
                {
                  GTK_TREE_ITER_INIT (*iter, model->stamp, lp);
                  g_object_unref (mount_point);
                  return TRUE;
                }

              g_object_unref (mount_point);
            }
        }
    }

  return FALSE;
}



/**
 * thunar_shortcuts_model_drop_possible:
 * @model : a #ThunarShortcutstModel.
 * @path  : a #GtkTreePath.
 *
 * Determines whether a drop is possible before the given @path, at the same depth
 * as @path. I.e., can we drop data at that location. @path does not have to exist;
 * the return value will almost certainly be FALSE if the parent of @path doesn't
 * exist, though.
 *
 * Return value: %TRUE if it's possible to drop data before @path, else %FALSE.
 **/
gboolean
thunar_shortcuts_model_drop_possible (ThunarShortcutsModel *model,
                                      GtkTreePath          *path)
{
  ThunarShortcut *shortcut;

  _thunar_return_val_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model), FALSE);
  _thunar_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* determine the list item for the path */
  shortcut = g_list_nth_data (model->shortcuts, gtk_tree_path_get_indices (path)[0]);

  /* append to the list is not possible */
  if (G_LIKELY (shortcut == NULL))
    return FALSE;

  /* cannot drop before special shortcuts! */
  if (shortcut->group == THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS)
    return TRUE;

  /* we can drop at the end of the bookmarks (before devices header) */
  if (shortcut->group == THUNAR_SHORTCUT_GROUP_DEVICES_HEADER)
    return TRUE;

  return FALSE;
}



/**
 * thunar_shortcuts_model_add:
 * @model    : a #ThunarShortcutsModel.
 * @dst_path : the destination path.
 * @file     : the #ThunarFile that should be added to the shortcuts list.
 *
 * Adds the shortcut @file to the @model at @dst_path, unless @file is
 * already present in @model in which case no action is performed.
 **/
void
thunar_shortcuts_model_add (ThunarShortcutsModel *model,
                            GtkTreePath          *dst_path,
                            gpointer              file)
{
  ThunarShortcut *shortcut;
  GFile          *location;
  GList          *lp;
  guint           position = 0;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (dst_path == NULL || gtk_tree_path_get_depth (dst_path) > 0);
  _thunar_return_if_fail (dst_path == NULL || gtk_tree_path_get_indices (dst_path)[0] >= 0);
  _thunar_return_if_fail (dst_path == NULL || gtk_tree_path_get_indices (dst_path)[0] <= (gint) g_list_length (model->shortcuts));
  _thunar_return_if_fail (THUNAR_IS_FILE (file) || G_IS_FILE (file));

  location = G_IS_FILE (file) ? file : thunar_file_get_file (file);

  /* verify that the file is not already in use as shortcut */
  if (thunar_shortcuts_model_has_bookmark (model, location))
    return;

  /* create the new shortcut that will be inserted */
  shortcut = g_slice_new0 (ThunarShortcut);
  shortcut->group = THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS;
  shortcut->location = G_FILE (g_object_ref (G_OBJECT (location)));

  if (thunar_shortcuts_model_local_file (location))
      shortcut->file = thunar_file_get (location, NULL);
  else
      shortcut->gicon = g_themed_icon_new ("folder-remote");

  /* if no position was given, place the shortcut at the bottom */
  if (dst_path == NULL)
    {
      for (lp = model->shortcuts; lp != NULL; lp = lp->next)
        if (THUNAR_SHORTCUT (lp->data)->group == THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS)
          position++;

      shortcut->sort_id = ++position;
    }

  /* add the shortcut to the list at the given position */
  thunar_shortcuts_model_add_shortcut_with_path (model, shortcut, dst_path);

  /* the shortcuts list was changed, so write the gtk bookmarks file */
  thunar_shortcuts_model_save (model);
}



/**
 * thunar_shortcuts_model_move:
 * @model    : a #ThunarShortcutsModel.
 * @src_path : the source path.
 * @dst_path : the destination path.
 *
 * Moves the shortcut at @src_path to @dst_path, adjusting other
 * shortcut's positions as required.
 **/
void
thunar_shortcuts_model_move (ThunarShortcutsModel *model,
                             GtkTreePath          *src_path,
                             GtkTreePath          *dst_path)
{
  ThunarShortcut *shortcut;
  GtkTreePath    *path;
  GList          *lp;
  gint           *order;
  gint            index_src;
  gint            index_dst;
  gint            idx;
  gint            n_shortcuts;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (gtk_tree_path_get_depth (src_path) > 0);
  _thunar_return_if_fail (gtk_tree_path_get_depth (dst_path) > 0);
  _thunar_return_if_fail (gtk_tree_path_get_indices (src_path)[0] >= 0);
  _thunar_return_if_fail (gtk_tree_path_get_indices (src_path)[0] < (gint) g_list_length (model->shortcuts));
  _thunar_return_if_fail (gtk_tree_path_get_indices (dst_path)[0] > 0);

  index_src = gtk_tree_path_get_indices (src_path)[0];
  index_dst = gtk_tree_path_get_indices (dst_path)[0];

  if (G_UNLIKELY (index_src == index_dst))
    return;

  /* generate the order for the rows prior the dst/src rows */
  n_shortcuts = g_list_length (model->shortcuts);
  order = g_newa (gint, n_shortcuts);
  for (idx = 0, lp = model->shortcuts; idx < index_src && idx < index_dst; ++idx, lp = lp->next)
    order[idx] = idx;

  if (idx == index_src)
    {
      shortcut = THUNAR_SHORTCUT (lp->data);

      for (; idx < index_dst; ++idx, lp = lp->next)
        {
          if (lp->next == NULL)
            break;
          lp->data = lp->next->data;
          order[idx] = idx + 1;
        }

      lp->data = shortcut;
      order[idx++] = index_src;
    }
  else
    {
      for (; idx < index_src; ++idx, lp = lp->next)
        ;

      _thunar_assert (idx == index_src);

      shortcut = THUNAR_SHORTCUT (lp->data);

      for (; idx > index_dst; --idx, lp = lp->prev)
        {
          lp->data = lp->prev->data;
          order[idx] = idx - 1;
        }

      _thunar_assert (idx == index_dst);

      lp->data = shortcut;
      order[idx] = index_src;
      idx = index_src + 1;
    }

  /* generate the remaining order */
  for (; idx < n_shortcuts; ++idx)
    order[idx] = idx;

  /* tell all listeners about the reordering just performed */
  path = gtk_tree_path_new_first ();
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model), path, NULL, order);
  gtk_tree_path_free (path);

  /* the shortcuts list was changed, so write the gtk bookmarks file */
  thunar_shortcuts_model_save (model);
}



/**
 * thunar_shortcuts_model_remove:
 * @model : a #ThunarShortcutsModel.
 * @path  : the #GtkTreePath of the shortcut to remove.
 *
 * Removes the shortcut at @path from the @model and syncs to
 * on-disk storage. @path must refer to a valid, user-defined
 * shortcut, as you cannot remove system-defined entities (they
 * are managed internally).
 **/
void
thunar_shortcuts_model_remove (ThunarShortcutsModel *model,
                               GtkTreePath          *path)
{
  ThunarShortcut *shortcut;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (gtk_tree_path_get_depth (path) > 0);
  _thunar_return_if_fail (gtk_tree_path_get_indices (path)[0] >= 0);
  _thunar_return_if_fail (gtk_tree_path_get_indices (path)[0] < (gint) g_list_length (model->shortcuts));

  /* lookup the shortcut for the given path */
  shortcut = g_list_nth_data (model->shortcuts, gtk_tree_path_get_indices (path)[0]);

  /* verify that the shortcut is removable */
  _thunar_assert (shortcut->group == THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS);

  /* remove the shortcut (using the file destroy handler) */
  thunar_shortcuts_model_remove_shortcut (model, shortcut);
}



/**
 * thunar_shortcuts_model_rename:
 * @model : a #ThunarShortcutsModel.
 * @iter  : the #GtkTreeIter which refers to the shortcut that
 *          should be renamed to @name.
 * @name  : the new name for the shortcut at @path or %NULL to
 *          return to the default name.
 *
 * Renames the shortcut at @iter to the new @name in @model.
 *
 * @name may be %NULL or an empty to reset the shortcut to
 * its default name.
 **/
void
thunar_shortcuts_model_rename (ThunarShortcutsModel *model,
                               GtkTreeIter          *iter,
                               const gchar          *name)
{
  ThunarShortcut *shortcut;
  GtkTreePath    *path;

  _thunar_return_if_fail (name == NULL || g_utf8_validate (name, -1, NULL));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (iter->stamp == model->stamp);
  _thunar_return_if_fail (iter->user_data != NULL);

  /* lookup the shortcut for the given path */
  shortcut = THUNAR_SHORTCUT (((GList *) iter->user_data)->data);

  /* verify the shortcut */
  _thunar_assert (shortcut->group == THUNAR_SHORTCUT_GROUP_PLACES_BOOKMARKS);

  /* perform the rename */
  if (G_UNLIKELY (shortcut->name != NULL))
    g_free (shortcut->name);
  if (G_UNLIKELY (name == NULL || *name == '\0'))
    shortcut->name = NULL;
  else
    shortcut->name = g_strdup (name);

  /* notify the views about the change */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
  gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, iter);
  gtk_tree_path_free (path);

  /* save the changes to the model */
  thunar_shortcuts_model_save (model);
}



void
thunar_shortcuts_model_set_busy (ThunarShortcutsModel *model,
                                 ThunarDevice         *device,
                                 gboolean              busy)
{
  ThunarShortcut *shortcut;
  GList          *lp;
  guint           idx;
  GtkTreeIter     iter;
  GtkTreePath    *path;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));

  /* get the device */
  for (lp = model->shortcuts, idx = 0; lp != NULL; lp = lp->next, idx++)
    if (THUNAR_SHORTCUT (lp->data)->device == device)
      break;

  if (lp == NULL)
    return;

  shortcut = lp->data;
  _thunar_assert (shortcut->device == device);

  if (G_LIKELY (shortcut->busy != busy))
    {
      shortcut->busy = busy;

      if (busy && model->busy_timeout_id == 0)
        {
          /* start the global cycle timeout */
          model->busy_timeout_id =
            gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT,
                                          SPINNER_CYCLE_DURATION / SPINNER_NUM_STEPS,
                                          thunar_shortcuts_model_busy_timeout, model,
                                          thunar_shortcuts_model_busy_timeout_destroyed);
        }
      else if (!busy)
        {
          /* generate an iterator for the path */
          GTK_TREE_ITER_INIT (iter, model->stamp, lp);

          /* notify the views about the change */
          path = gtk_tree_path_new_from_indices (idx, -1);
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
        }
    }
}




void
thunar_shortcuts_model_set_hidden (ThunarShortcutsModel *model,
                                   GtkTreePath          *path,
                                   gboolean              hidden)
{
  ThunarShortcut  *shortcut;
  guint            length;
  gchar          **bookmarks;
  guint            pos;
  gchar           *uri;
  guint            n;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_MODEL (model));
  _thunar_return_if_fail (gtk_tree_path_get_depth (path) > 0);
  _thunar_return_if_fail (gtk_tree_path_get_indices (path)[0] >= 0);
  _thunar_return_if_fail (gtk_tree_path_get_indices (path)[0] < (gint) g_list_length (model->shortcuts));

  /* lookup the shortcut for the given path */
  shortcut = g_list_nth_data (model->shortcuts, gtk_tree_path_get_indices (path)[0]);
  _thunar_return_if_fail (shortcut != NULL);

  if (shortcut->device != NULL)
    {
      /* if this is a device, store in the device monitor */
      thunar_device_monitor_set_hidden (model->device_monitor, shortcut->device, hidden);
      return;
    }

  /* get the uri */
  if (shortcut->file != NULL)
    uri = thunar_file_dup_uri (shortcut->file);
  else if (shortcut->location != NULL)
    uri = g_file_get_uri (shortcut->location);
  else
    {
      g_warn_if_reached ();
      return;
    }

  /* prepare array */
  length = model->hidden_bookmarks != NULL ? g_strv_length (model->hidden_bookmarks) : 0;
  bookmarks = g_new0 (gchar *, length + 2);
  pos = 0;

  /* copy other uuid in the new list */
  if (model->hidden_bookmarks != NULL)
    {
      for (n = 0; model->hidden_bookmarks[n] != NULL; n++)
        if (g_strcmp0 (model->hidden_bookmarks[n], uri) != 0)
          bookmarks[pos++] = g_strdup (model->hidden_bookmarks[n]);
    }

  /* add the new uri if it should hide */
  if (hidden)
    bookmarks[pos++] = uri;
  else
    g_free (uri);

  /* store new list */
  g_object_set (G_OBJECT (model->preferences), "hidden-bookmarks", bookmarks, NULL);
  g_strfreev (bookmarks);

  /* header visibility */
  thunar_shortcuts_model_header_visibility (model);
}
