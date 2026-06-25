/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>.
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-file-monitor.h>
#include <thunar/thunar-folder.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-tree-model.h>
#include <thunar/thunar-device-monitor.h>
#include <thunar/thunar-util.h>



/* convenience macros */
#define G_NODE(node)                 ((GNode *) (node))
#define THUNAR_TREE_MODEL_ITEM(item) ((ThunarTreeModelItem *) (item))
#define G_NODE_HAS_DUMMY(node)       (node->children != NULL \
                                      && node->children->data == NULL \
                                      && node->children->next == NULL)



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CASE_SENSITIVE,
};



typedef struct _ThunarTreeModelItem ThunarTreeModelItem;



static void                 thunar_tree_model_tree_model_init         (GtkTreeModelIface      *iface);
static void                 thunar_tree_model_finalize                (GObject                *object);
static void                 thunar_tree_model_get_property            (GObject                *object,
                                                                       guint                   prop_id,
                                                                       GValue                 *value,
                                                                       GParamSpec             *pspec);
static void                 thunar_tree_model_set_property            (GObject                *object,
                                                                       guint                   prop_id,
                                                                       const GValue           *value,
                                                                       GParamSpec             *pspec);
static GtkTreeModelFlags    thunar_tree_model_get_flags               (GtkTreeModel           *tree_model);
static gint                 thunar_tree_model_get_n_columns           (GtkTreeModel           *tree_model);
static GType                thunar_tree_model_get_column_type         (GtkTreeModel           *tree_model,
                                                                       gint                    column);
static gboolean             thunar_tree_model_get_iter                (GtkTreeModel           *tree_model,
                                                                       GtkTreeIter            *iter,
                                                                       GtkTreePath            *path);
static GtkTreePath         *thunar_tree_model_get_path                (GtkTreeModel           *tree_model,
                                                                       GtkTreeIter            *iter);
static void                 thunar_tree_model_get_value               (GtkTreeModel           *tree_model,
                                                                       GtkTreeIter            *iter,
                                                                       gint                    column,
                                                                       GValue                 *value);
static gboolean             thunar_tree_model_iter_next               (GtkTreeModel           *tree_model,
                                                                       GtkTreeIter            *iter);
static gboolean             thunar_tree_model_iter_children           (GtkTreeModel           *tree_model,
                                                                       GtkTreeIter            *iter,
                                                                       GtkTreeIter            *parent);
static gboolean             thunar_tree_model_iter_has_child          (GtkTreeModel           *tree_model,
                                                                       GtkTreeIter            *iter);
static gint                 thunar_tree_model_iter_n_children         (GtkTreeModel           *tree_model,
                                                                       GtkTreeIter            *iter);
static gboolean             thunar_tree_model_iter_nth_child          (GtkTreeModel           *tree_model,
                                                                       GtkTreeIter            *iter,
                                                                       GtkTreeIter            *parent,
                                                                       gint                    n);
static gboolean             thunar_tree_model_iter_parent             (GtkTreeModel           *tree_model,
                                                                       GtkTreeIter            *iter,
                                                                       GtkTreeIter            *child);
static void                 thunar_tree_model_ref_node                (GtkTreeModel           *tree_model,
                                                                       GtkTreeIter            *iter);
static void                 thunar_tree_model_unref_node              (GtkTreeModel           *tree_model,
                                                                       GtkTreeIter            *iter);
static gint                 thunar_tree_model_cmp_array               (gconstpointer           a,
                                                                       gconstpointer           b,
                                                                       gpointer                user_data);
static void                 thunar_tree_model_sort                    (ThunarTreeModel        *model,
                                                                       GNode                  *node);
static gboolean             thunar_tree_model_cleanup_idle            (gpointer                user_data);
static void                 thunar_tree_model_cleanup_idle_destroy    (gpointer                user_data);
static void                 thunar_tree_model_file_changed            (ThunarFileMonitor      *file_monitor,
                                                                       ThunarFile             *file,
                                                                       ThunarTreeModel        *model);
static void                 thunar_tree_model_device_added            (ThunarDeviceMonitor    *device_monitor,
                                                                       ThunarDevice           *device,
                                                                       ThunarTreeModel        *model);
static void                 thunar_tree_model_device_pre_unmount      (ThunarDeviceMonitor    *device_monitor,
                                                                       ThunarDevice           *device,
                                                                       GFile                  *root_file,
                                                                       ThunarTreeModel        *model);
static void                 thunar_tree_model_device_removed          (ThunarDeviceMonitor    *device_monitor,
                                                                       ThunarDevice           *device,
                                                                       ThunarTreeModel        *model);
static void                 thunar_tree_model_device_changed          (ThunarDeviceMonitor    *device_monitor,
                                                                       ThunarDevice           *device,
                                                                       ThunarTreeModel        *model);
static ThunarTreeModelItem *thunar_tree_model_item_new_with_file      (ThunarTreeModel        *model,
                                                                       ThunarFile             *file) G_GNUC_MALLOC;
static ThunarTreeModelItem *thunar_tree_model_item_new_with_device    (ThunarTreeModel        *model,
                                                                       ThunarDevice           *device) G_GNUC_MALLOC;
static void                 thunar_tree_model_item_free               (ThunarTreeModelItem    *item);
static void                 thunar_tree_model_item_reset              (ThunarTreeModelItem    *item);
static void                 thunar_tree_model_item_load_folder        (ThunarTreeModelItem    *item);
static void                 thunar_tree_model_item_files_added        (ThunarTreeModelItem    *item,
                                                                       GList                  *files,
                                                                       ThunarFolder           *folder);
static void                 thunar_tree_model_item_files_removed      (ThunarTreeModelItem    *item,
                                                                       GList                  *files,
                                                                       ThunarFolder           *folder);
static gboolean             thunar_tree_model_item_load_idle          (gpointer                user_data);
static void                 thunar_tree_model_item_load_idle_destroy  (gpointer                user_data);
static void                 thunar_tree_model_item_notify_loading     (ThunarTreeModelItem    *item,
                                                                       GParamSpec             *pspec,
                                                                       ThunarFolder           *folder);
static void                 thunar_tree_model_node_insert_dummy       (GNode                  *parent,
                                                                       ThunarTreeModel        *model);
static void                 thunar_tree_model_node_drop_dummy         (GNode                  *node,
                                                                       ThunarTreeModel        *model);
static gboolean             thunar_tree_model_node_traverse_cleanup   (GNode                  *node,
                                                                       gpointer                user_data);
static gboolean             thunar_tree_model_node_traverse_changed   (GNode                  *node,
                                                                       gpointer                user_data);
static gboolean             thunar_tree_model_node_traverse_remove    (GNode                  *node,
                                                                       gpointer                user_data);
static gboolean             thunar_tree_model_node_traverse_sort      (GNode                  *node,
                                                                       gpointer                user_data);
static gboolean             thunar_tree_model_node_traverse_free      (GNode                  *node,
                                                                       gpointer                user_data);
static gboolean             thunar_tree_model_node_traverse_visible   (GNode                  *node,
                                                                       gpointer                user_data);
static gboolean             thunar_tree_model_get_case_sensitive      (ThunarTreeModel        *model);
static void                 thunar_tree_model_set_case_sensitive      (ThunarTreeModel        *model,
                                                                       gboolean                case_sensitive);
static gboolean             thunar_tree_model_default_visiblity       (ThunarTreeModel *model,
                                                                       ThunarFile      *file,
                                                                       gpointer         data);



struct _ThunarTreeModelClass
{
  GObjectClass __parent__;
};

struct _ThunarTreeModel
{
  GObject                     __parent__;

  /* the model stamp is only used when debugging is
   * enabled, to make sure we don't accept iterators
   * generated by another model.
   */
#ifndef NDEBUG
  gint                        stamp;
#endif

  /* removable devices */
  ThunarDeviceMonitor        *device_monitor;

  ThunarFileMonitor          *file_monitor;

  gboolean                    sort_case_sensitive;

  ThunarTreeModelVisibleFunc  visible_func;
  gpointer                    visible_data;

  GNode                      *root;
  GNode                      *file_system;
  GNode                      *network;

  guint                       cleanup_idle_id;
};

struct _ThunarTreeModelItem
{
  gint             ref_count;
  guint            load_idle_id;
  ThunarFile      *file;
  ThunarFolder    *folder;
  ThunarDevice    *device;
  ThunarTreeModel *model;

  /* list of children of this node that are
   * not visible in the treeview */
  GSList          *invisible_children;
};

typedef struct
{
  gint   offset;
  GNode *node;
} SortTuple;



G_DEFINE_TYPE_WITH_CODE (ThunarTreeModel, thunar_tree_model, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, thunar_tree_model_tree_model_init))



static void
thunar_tree_model_class_init (ThunarTreeModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_tree_model_finalize;
  gobject_class->get_property = thunar_tree_model_get_property;
  gobject_class->set_property = thunar_tree_model_set_property;

  /**
   * ThunarTreeModel:case-sensitive:
   *
   * Whether the sorting of the folder items will be done
   * in a case-sensitive manner.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CASE_SENSITIVE,
                                   g_param_spec_boolean ("case-sensitive",
                                                         "case-sensitive",
                                                         "case-sensitive",
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_tree_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = thunar_tree_model_get_flags;
  iface->get_n_columns = thunar_tree_model_get_n_columns;
  iface->get_column_type = thunar_tree_model_get_column_type;
  iface->get_iter = thunar_tree_model_get_iter;
  iface->get_path = thunar_tree_model_get_path;
  iface->get_value = thunar_tree_model_get_value;
  iface->iter_next = thunar_tree_model_iter_next;
  iface->iter_children = thunar_tree_model_iter_children;
  iface->iter_has_child = thunar_tree_model_iter_has_child;
  iface->iter_n_children = thunar_tree_model_iter_n_children;
  iface->iter_nth_child = thunar_tree_model_iter_nth_child;
  iface->iter_parent = thunar_tree_model_iter_parent;
  iface->ref_node = thunar_tree_model_ref_node;
  iface->unref_node = thunar_tree_model_unref_node;
}



static void
thunar_tree_model_init (ThunarTreeModel *model)
{
  ThunarTreeModelItem *item;
  ThunarFile          *file;
  GFile               *home;
  GList               *system_paths = NULL;
  GList               *devices;
  GList               *lp;
  GNode               *node;

  /* generate a unique stamp if we're in debug mode */
#ifndef NDEBUG
  model->stamp = g_random_int ();
#endif

  /* initialize the model data */
  model->sort_case_sensitive = TRUE;
  model->visible_func = thunar_tree_model_default_visiblity;
  model->visible_data = NULL;
  model->cleanup_idle_id = 0;

  /* connect to the file monitor */
  model->file_monitor = thunar_file_monitor_get_default ();
  g_signal_connect (G_OBJECT (model->file_monitor), "file-changed", G_CALLBACK (thunar_tree_model_file_changed), model);

  /* allocate the "virtual root node" */
  model->root = g_node_new (NULL);

  /* initialize references to certain toplevel nodes */
  model->file_system = NULL;
  model->network = NULL;

  /* connect to the volume monitor */
  model->device_monitor = thunar_device_monitor_get ();
  g_signal_connect (model->device_monitor, "device-added", G_CALLBACK (thunar_tree_model_device_added), model);
  g_signal_connect (model->device_monitor, "device-pre-unmount", G_CALLBACK (thunar_tree_model_device_pre_unmount), model);
  g_signal_connect (model->device_monitor, "device-removed", G_CALLBACK (thunar_tree_model_device_removed), model);
  g_signal_connect (model->device_monitor, "device-changed", G_CALLBACK (thunar_tree_model_device_changed), model);

  /* append the computer icon if browsing the computer is supported */
  if (thunar_g_vfs_is_uri_scheme_supported ("computer"))
    system_paths = g_list_append (system_paths, thunar_g_file_new_for_computer());

  /* add the home folder to the system paths */
  home = thunar_g_file_new_for_home ();
  system_paths = g_list_append (system_paths, g_object_ref (home));

  if (thunar_g_vfs_is_uri_scheme_supported ("recent"))
    system_paths = g_list_append (system_paths, thunar_g_file_new_for_recent());

  /* append the trash icon if the trash is supported */
  if (thunar_g_vfs_is_uri_scheme_supported ("trash"))
    system_paths = g_list_append (system_paths, thunar_g_file_new_for_trash ());

  /* append the root file system */
  system_paths = g_list_append (system_paths, thunar_g_file_new_for_root ());

  /* append the network icon if browsing the network is supported */
  if (thunar_g_vfs_is_uri_scheme_supported ("network"))
    system_paths = g_list_append (system_paths, thunar_g_file_new_for_network());

  /* append the system defined nodes ('Computer', 'Home', 'Trash', 'File System', 'Network') */
  for (lp = system_paths; lp != NULL; lp = lp->next)
    {
      /* determine the file for the path */
      file = thunar_file_get (lp->data, NULL);
      if (G_LIKELY (file != NULL))
        {
          /* watch the trash for changes */
          if (thunar_file_is_trash (file))
            thunar_file_watch (file);

          /* create and append the new node */
          item = thunar_tree_model_item_new_with_file (model, file);
          node = g_node_append_data (model->root, item);

          /* store reference to the "File System" node */
          if (thunar_file_has_uri_scheme (file, "file") && thunar_file_is_root (file))
            model->file_system = node;

          /* store reference to the "Network" node */
          if (thunar_file_has_uri_scheme (file, "network"))
            model->network = node;

          g_object_unref (G_OBJECT (file));

          /* add the dummy node */
          g_node_append_data (node, NULL);
        }

      /* release the system defined path */
      g_object_unref (lp->data);
    }

  g_list_free (system_paths);
  g_object_unref (home);

  /* setup the initial devices */
  devices = thunar_device_monitor_get_devices (model->device_monitor);
  for (lp = devices; lp != NULL; lp = lp->next)
    {
      thunar_tree_model_device_added (model->device_monitor, lp->data, model);
      g_object_unref (lp->data);
    }
  g_list_free (devices);
}



static void
thunar_tree_model_finalize (GObject *object)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (object);

  /* remove the cleanup idle */
  if (model->cleanup_idle_id != 0)
    g_source_remove (model->cleanup_idle_id);

  /* disconnect from the file monitor */
  g_signal_handlers_disconnect_by_func (model->file_monitor, thunar_tree_model_file_changed, model);
  g_object_unref (model->file_monitor);

  /* release all resources allocated to the model */
  g_node_traverse (model->root, G_POST_ORDER, G_TRAVERSE_ALL, -1, thunar_tree_model_node_traverse_free, NULL);
  g_node_destroy (model->root);

  /* disconnect from the volume monitor */
  g_signal_handlers_disconnect_matched (model->device_monitor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);
  g_object_unref (model->device_monitor);

  (*G_OBJECT_CLASS (thunar_tree_model_parent_class)->finalize) (object);
}



static void
thunar_tree_model_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (object);

  switch (prop_id)
    {
    case PROP_CASE_SENSITIVE:
      g_value_set_boolean (value, thunar_tree_model_get_case_sensitive (model));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_tree_model_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (object);

  switch (prop_id)
    {
    case PROP_CASE_SENSITIVE:
      thunar_tree_model_set_case_sensitive (model, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static GtkTreeModelFlags
thunar_tree_model_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST;
}



static gint
thunar_tree_model_get_n_columns (GtkTreeModel *tree_model)
{
  return THUNAR_TREE_MODEL_N_COLUMNS;
}



static GType
thunar_tree_model_get_column_type (GtkTreeModel *tree_model,
                                   gint          column)
{
  switch (column)
    {
    case THUNAR_TREE_MODEL_COLUMN_FILE:
      return THUNAR_TYPE_FILE;

    case THUNAR_TREE_MODEL_COLUMN_NAME:
      return G_TYPE_STRING;

    case THUNAR_TREE_MODEL_COLUMN_ATTR:
      return PANGO_TYPE_ATTR_LIST;

    case THUNAR_TREE_MODEL_COLUMN_DEVICE:
      return THUNAR_TYPE_DEVICE;

    default:
      _thunar_assert_not_reached ();
      return G_TYPE_INVALID;
    }
}



static gboolean
thunar_tree_model_get_iter (GtkTreeModel *tree_model,
                            GtkTreeIter  *iter,
                            GtkTreePath  *path)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (tree_model);
  GtkTreeIter      parent;
  const gint      *indices;
  gint             depth;
  gint             n;

  _thunar_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  /* determine the path depth */
  depth = gtk_tree_path_get_depth (path);

  /* determine the path indices */
  indices = gtk_tree_path_get_indices (path);

  /* initialize the parent iterator with the root element */
  GTK_TREE_ITER_INIT (parent, model->stamp, model->root);
  if (!gtk_tree_model_iter_nth_child (tree_model, iter, &parent, indices[0]))
    return FALSE;

  for (n = 1; n < depth; ++n)
    {
      parent = *iter;
      if (!gtk_tree_model_iter_nth_child (tree_model, iter, &parent, indices[n]))
        return FALSE;
    }

  return TRUE;
}



static GtkTreePath*
thunar_tree_model_get_path (GtkTreeModel *tree_model,
                            GtkTreeIter  *iter)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (tree_model);
  GtkTreePath     *path;
  GtkTreeIter      tmp_iter;
  GNode           *tmp_node;
  GNode           *node;
  gint             n;

  _thunar_return_val_if_fail (iter->user_data != NULL, NULL);
  _thunar_return_val_if_fail (iter->stamp == model->stamp, NULL);

  /* determine the node for the iterator */
  node = iter->user_data;

  /* check if the iter refers to the "virtual root node" */
  if (node->parent == NULL && node == model->root)
    return gtk_tree_path_new ();
  else if (G_UNLIKELY (node->parent == NULL))
    return NULL;

  if (node->parent == model->root)
    {
      path = gtk_tree_path_new ();
      tmp_node = g_node_first_child (model->root);
    }
  else
    {
      /* determine the iterator for the parent node */
      GTK_TREE_ITER_INIT (tmp_iter, model->stamp, node->parent);

      /* determine the path for the parent node */
      path = gtk_tree_model_get_path (tree_model, &tmp_iter);

      /* and the node for the parent's children */
      tmp_node = g_node_first_child (node->parent);
    }

  /* check if we have a valid path */
  if (G_LIKELY (path != NULL))
    {
      /* lookup our index in the child list */
      for (n = 0; tmp_node != NULL; ++n, tmp_node = tmp_node->next)
        if (tmp_node == node)
          break;

      /* check if we have found the node */
      if (G_UNLIKELY (tmp_node == NULL))
        {
          gtk_tree_path_free (path);
          return NULL;
        }

      /* append the index to the parent path */
      gtk_tree_path_append_index (path, n);
    }

  return path;
}



static void
thunar_tree_model_get_value (GtkTreeModel *tree_model,
                             GtkTreeIter  *iter,
                             gint          column,
                             GValue       *value)
{
  ThunarTreeModelItem *item;
  ThunarTreeModel     *model = THUNAR_TREE_MODEL (tree_model);
  GNode               *node = iter->user_data;

  _thunar_return_if_fail (iter->user_data != NULL);
  _thunar_return_if_fail (iter->stamp == model->stamp);
  _thunar_return_if_fail (column < THUNAR_TREE_MODEL_N_COLUMNS);

  /* determine the item for the node */
  item = node->data;

  switch (column)
    {
    case THUNAR_TREE_MODEL_COLUMN_FILE:
      g_value_init (value, THUNAR_TYPE_FILE);
      g_value_set_object (value, (item != NULL) ? item->file : NULL);
      break;

    case THUNAR_TREE_MODEL_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      if (G_LIKELY (item != NULL && item->device != NULL))
        g_value_take_string (value, thunar_device_get_name (item->device));
      else if (G_LIKELY (item != NULL && item->file != NULL))
        g_value_set_static_string (value, thunar_file_get_display_name (item->file));
      else
        g_value_set_static_string (value, _("Loading..."));
      break;

    case THUNAR_TREE_MODEL_COLUMN_ATTR:
      g_value_init (value, PANGO_TYPE_ATTR_LIST);
      if (G_UNLIKELY (node->parent == model->root))
        g_value_set_boxed (value, thunar_pango_attr_list_bold ());
      else if (G_UNLIKELY (item == NULL))
        g_value_set_boxed (value, thunar_pango_attr_list_italic ());
      break;

    case THUNAR_TREE_MODEL_COLUMN_DEVICE:
      g_value_init (value, THUNAR_TYPE_DEVICE);
      g_value_set_object (value, (item != NULL) ? item->device : NULL);
      break;

    default:
      _thunar_assert_not_reached ();
      break;
    }
}



static gboolean
thunar_tree_model_iter_next (GtkTreeModel *tree_model,
                             GtkTreeIter  *iter)
{
  _thunar_return_val_if_fail (iter->stamp == THUNAR_TREE_MODEL (tree_model)->stamp, FALSE);
  _thunar_return_val_if_fail (iter->user_data != NULL, FALSE);

  /* check if we have any further nodes in this row */
  if (g_node_next_sibling (iter->user_data) != NULL)
    {
      iter->user_data = g_node_next_sibling (iter->user_data);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_tree_model_iter_children (GtkTreeModel *tree_model,
                                 GtkTreeIter  *iter,
                                 GtkTreeIter  *parent)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (tree_model);
  GNode           *children;

  _thunar_return_val_if_fail (parent == NULL || parent->user_data != NULL, FALSE);
  _thunar_return_val_if_fail (parent == NULL || parent->stamp == model->stamp, FALSE);

  if (G_LIKELY (parent != NULL))
    children = g_node_first_child (parent->user_data);
  else
    children = g_node_first_child (model->root);

  if (G_LIKELY (children != NULL))
    {
      GTK_TREE_ITER_INIT (*iter, model->stamp, children);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_tree_model_iter_has_child (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter)
{
  _thunar_return_val_if_fail (iter->stamp == THUNAR_TREE_MODEL (tree_model)->stamp, FALSE);
  _thunar_return_val_if_fail (iter->user_data != NULL, FALSE);

  return (g_node_first_child (iter->user_data) != NULL);
}



static gint
thunar_tree_model_iter_n_children (GtkTreeModel *tree_model,
                                   GtkTreeIter  *iter)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (tree_model);

  _thunar_return_val_if_fail (iter == NULL || iter->user_data != NULL, 0);
  _thunar_return_val_if_fail (iter == NULL || iter->stamp == model->stamp, 0);

  return g_node_n_children ((iter == NULL) ? model->root : iter->user_data);
}



static gboolean
thunar_tree_model_iter_nth_child (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter,
                                  GtkTreeIter  *parent,
                                  gint          n)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (tree_model);
  GNode           *child;

  _thunar_return_val_if_fail (parent == NULL || parent->user_data != NULL, FALSE);
  _thunar_return_val_if_fail (parent == NULL || parent->stamp == model->stamp, FALSE);

  child = g_node_nth_child ((parent != NULL) ? parent->user_data : model->root, n);
  if (G_LIKELY (child != NULL))
    {
      GTK_TREE_ITER_INIT (*iter, model->stamp, child);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_tree_model_iter_parent (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter,
                               GtkTreeIter  *child)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (tree_model);
  GNode           *parent;

  _thunar_return_val_if_fail (iter != NULL, FALSE);
  _thunar_return_val_if_fail (child->user_data != NULL, FALSE);
  _thunar_return_val_if_fail (child->stamp == model->stamp, FALSE);

  /* check if we have a parent for iter */
  parent = G_NODE (child->user_data)->parent;
  if (G_LIKELY (parent != model->root))
    {
      GTK_TREE_ITER_INIT (*iter, model->stamp, parent);
      return TRUE;
    }
  else
    {
      /* no "real parent" for this node */
      return FALSE;
    }
}



static void
thunar_tree_model_ref_node (GtkTreeModel *tree_model,
                            GtkTreeIter  *iter)
{
  ThunarTreeModelItem *item;
  ThunarTreeModel     *model = THUNAR_TREE_MODEL (tree_model);
  GNode               *node;

  _thunar_return_if_fail (iter->user_data != NULL);
  _thunar_return_if_fail (iter->stamp == model->stamp);

  /* determine the node for the iterator */
  node = G_NODE (iter->user_data);
  if (G_UNLIKELY (node == model->root))
    return;

  /* check if we have a dummy item here */
  item = node->data;
  if (G_UNLIKELY (item == NULL))
    {
      /* tell the parent to load the folder */
      thunar_tree_model_item_load_folder (node->parent->data);
    }
  else
    {
      /* do not load `Recent` to avoid slowing down the view */
      if (item->file != NULL && thunar_file_is_recent (item->file))
        return;

      /* schedule a reload of the folder if it is cleaned earlier */
      if (G_UNLIKELY (item->ref_count == 0))
        thunar_tree_model_item_load_folder (item);

      /* increment the reference count */
      item->ref_count += 1;
    }
}



static void
thunar_tree_model_unref_node (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter)
{
  ThunarTreeModelItem *item;
  ThunarTreeModel     *model = THUNAR_TREE_MODEL (tree_model);
  GNode               *node;

  _thunar_return_if_fail (iter->user_data != NULL);
  _thunar_return_if_fail (iter->stamp == model->stamp);

  /* determine the node for the iterator */
  node = G_NODE (iter->user_data);
  if (G_UNLIKELY (node == model->root))
    return;

  /* check if this a non-dummy item, if so, decrement the reference count */
  item = node->data;
  if (G_LIKELY (item != NULL))
    item->ref_count -= 1;

  /* NOTE: we don't cleanup nodes when the item ref count is zero,
   * because GtkTreeView also does a lot of reffing when scrolling the
   * tree, which results in all sorts for glitches */
}



static gint
thunar_tree_model_cmp_array (gconstpointer a,
                             gconstpointer b,
                             gpointer      user_data)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_MODEL (user_data), 0);

  /* just sort by name (case-sensitive) */
  return thunar_file_compare_by_name (THUNAR_TREE_MODEL_ITEM (((const SortTuple *) a)->node->data)->file,
                                      THUNAR_TREE_MODEL_ITEM (((const SortTuple *) b)->node->data)->file,
                                      THUNAR_TREE_MODEL (user_data)->sort_case_sensitive);
}



static void
thunar_tree_model_sort (ThunarTreeModel *model,
                        GNode           *node)
{
  GtkTreePath *path;
  GtkTreeIter  iter;
  SortTuple   *sort_array;
  GNode       *child_node;
  guint        n_children;
  gint        *new_order;
  guint        n;

  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (model));

  /* determine the number of children of the node */
  n_children = g_node_n_children (node);
  if (G_UNLIKELY (n_children <= 1))
    return;

  /* be sure to not overuse the stack */
  if (G_LIKELY (n_children < 500))
    sort_array = g_newa (SortTuple, n_children);
  else
    sort_array = g_new (SortTuple, n_children);

  /* generate the sort array of tuples */
  for (child_node = g_node_first_child (node), n = 0; n < n_children; child_node = g_node_next_sibling (child_node), ++n)
    {
      _thunar_return_if_fail (child_node != NULL);
      _thunar_return_if_fail (child_node->data != NULL);

      sort_array[n].node = child_node;
      sort_array[n].offset = n;
    }

  /* sort the array using QuickSort */
  g_qsort_with_data (sort_array, n_children, sizeof (SortTuple), thunar_tree_model_cmp_array, model);

  /* start out with an empty child list */
  node->children = NULL;

  /* update our internals and generate the new order */
  new_order = g_newa (gint, n_children);
  for (n = 0; n < n_children; ++n)
    {
      /* yeppa, there's the new offset */
      new_order[n] = sort_array[n].offset;

      /* unlink and reinsert */
      sort_array[n].node->next = NULL;
      sort_array[n].node->prev = NULL;
      sort_array[n].node->parent = NULL;
      g_node_append (node, sort_array[n].node);
    }

  /* determine the iterator for the parent node */
  GTK_TREE_ITER_INIT (iter, model->stamp, node);

  /* tell the view about the new item order */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
  gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model), path, &iter, new_order);
  gtk_tree_path_free (path);

  /* cleanup if we used the heap */
  if (G_UNLIKELY (n_children >= 500))
    g_free (sort_array);
}



static gboolean
thunar_tree_model_cleanup_idle (gpointer user_data)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (user_data);

THUNAR_THREADS_ENTER

  /* walk through the tree and release all the nodes with a ref count of 0 */
  g_node_traverse (model->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
                   thunar_tree_model_node_traverse_cleanup, model);

THUNAR_THREADS_LEAVE

  return FALSE;
}



static void
thunar_tree_model_cleanup_idle_destroy (gpointer user_data)
{
  THUNAR_TREE_MODEL (user_data)->cleanup_idle_id = 0;
}



static void
thunar_tree_model_file_changed (ThunarFileMonitor *file_monitor,
                                ThunarFile        *file,
                                ThunarTreeModel   *model)
{
  _thunar_return_if_fail (THUNAR_IS_FILE_MONITOR (file_monitor));
  _thunar_return_if_fail (model->file_monitor == file_monitor);
  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (model));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* traverse the model and emit "row-changed" for the file's nodes */
  if (thunar_file_is_directory (file))
    g_node_traverse (model->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, thunar_tree_model_node_traverse_changed, file);
}



static void
thunar_tree_model_device_changed (ThunarDeviceMonitor *device_monitor,
                                  ThunarDevice        *device,
                                  ThunarTreeModel     *model)
{
  ThunarTreeModelItem *item = NULL;
  GtkTreePath         *path;
  GtkTreeIter          iter;
  GFile               *mount_point;
  GNode               *node;

  _thunar_return_if_fail (THUNAR_IS_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (model->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (model));

  /* lookup the volume in the item list */
  for (node = model->root->children; node != NULL; node = node->next)
    {
      item = THUNAR_TREE_MODEL_ITEM (node->data);
      if (item->device == device)
        break;
    }

  /* verify that we actually found the item */
  _thunar_assert (item != NULL);
  _thunar_assert (item->device == device);

  /* check if the volume is mounted and we don't have a file yet */
  if (thunar_device_is_mounted (device) && item->file == NULL)
    {
      mount_point = thunar_device_get_root (device);
      if (mount_point != NULL)
        {
          /* try to determine the file for the mount point */
          item->file = thunar_file_get (mount_point, NULL);

          /* because the volume node is already reffed, we need to load the folder manually here */
          thunar_tree_model_item_load_folder (item);

          g_object_unref (mount_point);
        }
    }
  else if (!thunar_device_is_mounted (device) && item->file != NULL)
    {
      /* reset the item for the node */
      thunar_tree_model_item_reset (item);

      /* release all child nodes */
      while (node->children != NULL)
        g_node_traverse (node->children, G_POST_ORDER, G_TRAVERSE_ALL, -1, thunar_tree_model_node_traverse_remove, model);

      /* append the dummy node */
      thunar_tree_model_node_insert_dummy (node, model);
    }

  /* generate an iterator for the item */
  GTK_TREE_ITER_INIT (iter, model->stamp, node);

  /* tell the view that the volume has changed in some way */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
  gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
  gtk_tree_path_free (path);
}



static void
thunar_tree_model_device_pre_unmount (ThunarDeviceMonitor *device_monitor,
                                      ThunarDevice        *device,
                                      GFile               *root_file,
                                      ThunarTreeModel     *model)
{
  GNode *node;

  _thunar_return_if_fail (THUNAR_IS_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (model->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (G_IS_FILE (root_file));
  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (model));

  /* lookup the node for the volume (if visible) */
  for (node = model->root->children; node != NULL; node = node->next)
    if (THUNAR_TREE_MODEL_ITEM (node->data)->device == device)
      break;

  /* check if we have a node */
  if (G_UNLIKELY (node == NULL))
    return;

  /* reset the item for the node */
  thunar_tree_model_item_reset (node->data);

  /* remove all child nodes */
  while (node->children != NULL)
    g_node_traverse (node->children, G_POST_ORDER, G_TRAVERSE_ALL, -1, thunar_tree_model_node_traverse_remove, model);

  /* add the dummy node */
  thunar_tree_model_node_insert_dummy (node, model);
}



static void
thunar_tree_model_device_added (ThunarDeviceMonitor *device_monitor,
                                ThunarDevice        *device,
                                ThunarTreeModel     *model)
{
  ThunarTreeModelItem *item;
  GtkTreePath         *path;
  GtkTreeIter          iter;
  GNode               *node;

  _thunar_return_if_fail (THUNAR_IS_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (model->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (model));

  /* check if the new node should be inserted after "File System" or "Network" */
  if (model->network && thunar_device_get_kind (device) == THUNAR_DEVICE_KIND_MOUNT_REMOTE)
    node = model->network;
  else
    node = model->file_system;

  /* fallback to the last child of the root node */
  if (node == NULL)
    node = g_node_last_child (model->root);

  /* determine the position for the new node in the item list */
  for (; node->next != NULL; node = node->next)
    {
      item = THUNAR_TREE_MODEL_ITEM (node->next->data);
      if (item->device == NULL)
        break;

      if (thunar_device_compare_by_name (item->device, device) > 0)
        break;
    }

  /* allocate a new item for the volume */
  item = thunar_tree_model_item_new_with_device (model, device);

  /* insert the new node */
  node = g_node_insert_data_after (model->root, node, item);

  /* determine the iterator for the new node */
  GTK_TREE_ITER_INIT (iter, model->stamp, node);

  /* tell the view about the new node */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
  gtk_tree_path_free (path);

  /* add the dummy node */
  thunar_tree_model_node_insert_dummy (node, model);
}



static void
thunar_tree_model_device_removed (ThunarDeviceMonitor *device_monitor,
                                  ThunarDevice        *device,
                                  ThunarTreeModel     *model)
{
  GNode *node;

  _thunar_return_if_fail (THUNAR_IS_DEVICE_MONITOR (device_monitor));
  _thunar_return_if_fail (model->device_monitor == device_monitor);
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (model));

  /* find the device */
  for (node = model->root->children; node != NULL; node = node->next)
    if (THUNAR_TREE_MODEL_ITEM (node->data)->device == device)
      break;

  /* something is broken if we don't have an item here */
  _thunar_assert (node != NULL);
  _thunar_assert (THUNAR_TREE_MODEL_ITEM (node->data)->device == device);

  /* drop the node from the model */
  g_node_traverse (node, G_POST_ORDER, G_TRAVERSE_ALL, -1, thunar_tree_model_node_traverse_remove, model);
}



static ThunarTreeModelItem*
thunar_tree_model_item_new_with_file (ThunarTreeModel *model,
                                      ThunarFile      *file)
{
  ThunarTreeModelItem *item;

  item = g_slice_new0 (ThunarTreeModelItem);
  item->file = THUNAR_FILE (g_object_ref (G_OBJECT (file)));
  item->model = model;

  return item;
}



static ThunarTreeModelItem*
thunar_tree_model_item_new_with_device (ThunarTreeModel *model,
                                        ThunarDevice    *device)
{
  ThunarTreeModelItem *item;
  GFile               *mount_point;

  item = g_slice_new0 (ThunarTreeModelItem);
  item->device = THUNAR_DEVICE (g_object_ref (G_OBJECT (device)));
  item->model = model;

  /* check if the volume is mounted */
  if (thunar_device_is_mounted (device))
    {
      mount_point = thunar_device_get_root (device);
      if (G_LIKELY (mount_point != NULL))
        {
          /* try to determine the file for the mount point */
          item->file = thunar_file_get (mount_point, NULL);
          g_object_unref (mount_point);
        }
    }

  return item;
}



static void
thunar_tree_model_item_free (ThunarTreeModelItem *item)
{
  /* disconnect from the volume */
  if (G_UNLIKELY (item->device != NULL))
    g_object_unref (item->device);

  /* reset the remaining resources */
  thunar_tree_model_item_reset (item);

  /* release the item */
  g_slice_free (ThunarTreeModelItem, item);
}



static void
thunar_tree_model_item_reset (ThunarTreeModelItem *item)
{
  /* cancel any pending load idle source */
  if (G_UNLIKELY (item->load_idle_id != 0))
    g_source_remove (item->load_idle_id);

  /* disconnect from the folder */
  if (G_LIKELY (item->folder != NULL))
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (item->folder), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, item);
      g_object_unref (G_OBJECT (item->folder));
      item->folder = NULL;
    }

  /* free all the invisible children */
  if (item->invisible_children != NULL)
    {
      g_slist_free_full (item->invisible_children, g_object_unref);
      item->invisible_children = NULL;
    }

  /* disconnect from the file */
  if (G_LIKELY (item->file != NULL))
    {
      /* unwatch the trash */
      if (thunar_file_is_trash (item->file))
        thunar_file_unwatch (item->file);

      /* release and reset the file */
      g_object_unref (G_OBJECT (item->file));
      item->file = NULL;
    }
}



static void
thunar_tree_model_item_load_folder (ThunarTreeModelItem *item)
{
  _thunar_return_if_fail (THUNAR_IS_FILE (item->file) || THUNAR_IS_DEVICE (item->device));

  /* schedule the "load" idle source (if not already done) */
  if (G_LIKELY (item->load_idle_id == 0 && item->folder == NULL))
    {
      item->load_idle_id = g_idle_add_full (G_PRIORITY_HIGH, thunar_tree_model_item_load_idle,
                                            item, thunar_tree_model_item_load_idle_destroy);
    }
}



static void
thunar_tree_model_item_files_added (ThunarTreeModelItem *item,
                                    GList               *files,
                                    ThunarFolder        *folder)
{
  ThunarTreeModel     *model = THUNAR_TREE_MODEL (item->model);
  ThunarFile          *file;
  GNode               *node = NULL;
  GList               *lp;

  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (item->folder == folder);
  _thunar_return_if_fail (model->visible_func != NULL);

  /* process all specified files */
  for (lp = files; lp != NULL; lp = lp->next)
    {
      /* we don't care for anything except folders */
      file = THUNAR_FILE (lp->data);
      if (!thunar_file_is_directory (file))
        continue;

      /* if this file should be visible */
      if (!model->visible_func (model, file, model->visible_data))
        {
          /* file is invisible, insert it in the invisible list and continue */
          item->invisible_children = g_slist_prepend (item->invisible_children,
                                                      g_object_ref (G_OBJECT (file)));
          continue;
        }

      /* lookup the node for the item (on-demand) */
      if (G_UNLIKELY (node == NULL))
        node = g_node_find (model->root, G_POST_ORDER, G_TRAVERSE_ALL, item);
      _thunar_return_if_fail (node != NULL);

      thunar_tree_model_add_child (model, node, file);
    }

  /* sort the folders if any new ones were added */
  if (G_LIKELY (node != NULL))
    thunar_tree_model_sort (model, node);
}



static void
thunar_tree_model_item_files_removed (ThunarTreeModelItem *item,
                                      GList               *files,
                                      ThunarFolder        *folder)
{
  ThunarTreeModel *model = item->model;
  GtkTreePath     *path;
  GtkTreeIter      iter;
  GNode           *child_node;
  GNode           *node;
  GList           *lp;
  GSList          *inv_link;

  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (item->folder == folder);

  /* determine the node for the folder */
  node = g_node_find (model->root, G_POST_ORDER, G_TRAVERSE_ALL, item);
  _thunar_return_if_fail (node != NULL);

  /* check if the node has any visible children */
  if (G_LIKELY (node->children != NULL))
    {
      /* process all files */
      for (lp = files; lp != NULL; lp = lp->next)
        {
          /* find the child node for the file */
          for (child_node = g_node_first_child (node); child_node != NULL; child_node = g_node_next_sibling (child_node))
            if (child_node->data != NULL && THUNAR_TREE_MODEL_ITEM (child_node->data)->file == lp->data)
              break;

          /* drop the child node (and all descendant nodes) from the model */
          if (G_LIKELY (child_node != NULL))
            g_node_traverse (child_node, G_POST_ORDER, G_TRAVERSE_ALL, -1, thunar_tree_model_node_traverse_remove, model);
        }

      /* check if all children of the node where dropped */
      if (G_UNLIKELY (node->children == NULL))
        {
          /* determine the iterator for the folder node */
          GTK_TREE_ITER_INIT (iter, model->stamp, node);

          /* emit "row-has-child-toggled" for the folder node */
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
          gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
        }
    }

  /* we also need to release all the invisible folders */
  if (item->invisible_children != NULL)
    {
      for (lp = files; lp != NULL; lp = lp->next)
        {
          /* find the file in the hidden list */
          inv_link = g_slist_find (item->invisible_children, lp->data);
          if (inv_link != NULL)
            {
              /* release the file */
              g_object_unref (G_OBJECT (lp->data));

              /* remove from the list */
              item->invisible_children = g_slist_delete_link (item->invisible_children, inv_link);
            }
        }
    }
}



static void
thunar_tree_model_item_notify_loading (ThunarTreeModelItem *item,
                                       GParamSpec          *pspec,
                                       ThunarFolder        *folder)
{
  GNode *node;

  _thunar_return_if_fail (THUNAR_IS_FOLDER (folder));
  _thunar_return_if_fail (item->folder == folder);
  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (item->model));

  /* be sure to drop the dummy child node once the folder is loaded */
  if (G_LIKELY (!thunar_folder_get_loading (folder)))
    {
      /* lookup the node for the item... */
      node = g_node_find (item->model->root, G_POST_ORDER, G_TRAVERSE_ALL, item);
      _thunar_return_if_fail (node != NULL);

      /* ...and drop the dummy for the node */
      if (G_NODE_HAS_DUMMY (node))
        thunar_tree_model_node_drop_dummy (node, item->model);
    }
}



static gboolean
thunar_tree_model_item_load_idle (gpointer user_data)
{
  ThunarTreeModelItem *item = user_data;
  GFile               *mount_point;
  GList               *files;
#ifndef NDEBUG
  GNode               *node;
#endif

  _thunar_return_val_if_fail (item->folder == NULL, FALSE);

#ifndef NDEBUG
      /* find the node in the tree */
      node = g_node_find (item->model->root, G_POST_ORDER, G_TRAVERSE_ALL, item);

      /* debug check to make sure the node is empty or contains a dummy node.
       * if this is not true, the node already contains sub folders which means
       * something went wrong. */
      _thunar_return_val_if_fail (node->children == NULL || G_NODE_HAS_DUMMY (node), FALSE);
#endif

THUNAR_THREADS_ENTER

  /* check if we don't have a file yet and this is a mounted volume */
  if (item->file == NULL && item->device != NULL && thunar_device_is_mounted (item->device))
    {
      mount_point = thunar_device_get_root (item->device);
      if (G_LIKELY (mount_point != NULL))
        {
          /* try to determine the file for the mount point */
          item->file = thunar_file_get (mount_point, NULL);
          g_object_unref (mount_point);
        }
    }

  /* verify that we have a file */
  if (G_LIKELY (item->file != NULL))
    {
      /* open the folder for the item */
      item->folder = thunar_folder_get_for_file (item->file);
      if (G_LIKELY (item->folder != NULL))
        {
          /* connect signals */
          g_signal_connect_swapped (G_OBJECT (item->folder), "files-added", G_CALLBACK (thunar_tree_model_item_files_added), item);
          g_signal_connect_swapped (G_OBJECT (item->folder), "files-removed", G_CALLBACK (thunar_tree_model_item_files_removed), item);
          g_signal_connect_swapped (G_OBJECT (item->folder), "notify::loading", G_CALLBACK (thunar_tree_model_item_notify_loading), item);

          /* load the initial set of files (if any) */
          files = thunar_folder_get_files (item->folder);
          if (G_UNLIKELY (files != NULL))
            thunar_tree_model_item_files_added (item, files, item->folder);

          /* notify for "loading" if already loaded */
          if (!thunar_folder_get_loading (item->folder))
            g_object_notify (G_OBJECT (item->folder), "loading");
        }
    }

THUNAR_THREADS_LEAVE

  return FALSE;
}



static void
thunar_tree_model_item_load_idle_destroy (gpointer user_data)
{
  THUNAR_TREE_MODEL_ITEM (user_data)->load_idle_id = 0;
}



static void
thunar_tree_model_node_insert_dummy (GNode           *parent,
                                     ThunarTreeModel *model)
{
  GNode       *node;
  GtkTreeIter  iter;
  GtkTreePath *path;

  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (model));
  _thunar_return_if_fail (g_node_n_children (parent) == 0);

  /* add the dummy node */
  node = g_node_append_data (parent, NULL);

  /* determine the iterator for the dummy node */
  GTK_TREE_ITER_INIT (iter, model->stamp, node);

  /* tell the view about the dummy node */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
  gtk_tree_path_free (path);
}



static void
thunar_tree_model_node_drop_dummy (GNode           *node,
                                   ThunarTreeModel *model)
{
  GtkTreePath *path;
  GtkTreeIter  iter;

  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (model));
  _thunar_return_if_fail (G_NODE_HAS_DUMMY (node) && g_node_n_children (node) == 1);

  /* determine the iterator for the dummy */
  GTK_TREE_ITER_INIT (iter, model->stamp, node->children);

  /* determine the path for the iterator */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
  if (G_LIKELY (path != NULL))
    {
      /* notify the view */
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);

      /* drop the dummy from the model */
      g_node_destroy (node->children);

      /* determine the iter to the parent node */
      GTK_TREE_ITER_INIT (iter, model->stamp, node);

      /* determine the path to the parent node */
      gtk_tree_path_up (path);

      /* emit a "row-has-child-toggled" for the parent */
      gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &iter);

      /* release the path */
      gtk_tree_path_free (path);
    }
}



static gboolean
thunar_tree_model_node_traverse_cleanup (GNode    *node,
                                         gpointer  user_data)
{
  ThunarTreeModelItem *item = node->data;
  ThunarTreeModel     *model = THUNAR_TREE_MODEL (user_data);

  if (item && item->folder != NULL && item->ref_count == 0)
    {
      /* disconnect from the folder */
      g_signal_handlers_disconnect_matched (G_OBJECT (item->folder), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, item);
      g_object_unref (G_OBJECT (item->folder));
      item->folder = NULL;

      /* remove all the children of the node */
      while (node->children)
        g_node_traverse (node->children, G_POST_ORDER, G_TRAVERSE_ALL, -1,
                         thunar_tree_model_node_traverse_remove, model);

      /* insert a dummy node */
      thunar_tree_model_node_insert_dummy (node, model);
    }

  return FALSE;
}



static gboolean
thunar_tree_model_node_traverse_changed (GNode   *node,
                                         gpointer user_data)
{
  ThunarTreeModel     *model;
  GtkTreePath         *path;
  GtkTreeIter          iter;
  ThunarFile          *file = THUNAR_FILE (user_data);
  ThunarTreeModelItem *item = THUNAR_TREE_MODEL_ITEM (node->data);

  /* check if the node's file is the file that changed */
  if (G_UNLIKELY (item != NULL && item->file == file))
    {
      /* determine the tree model from the item */
      model = THUNAR_TREE_MODEL_ITEM (node->data)->model;

      /* determine the iterator for the node */
      GTK_TREE_ITER_INIT (iter, model->stamp, node);

      /* check if the changed node is not one of the root nodes */
      if (G_LIKELY (node->parent != model->root))
        {
          /* need to re-sort as the name of the file may have changed */
          thunar_tree_model_sort (model, node->parent);
        }

      /* determine the path for the node */
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
      if (G_LIKELY (path != NULL))
        {
          /* emit "row-changed" */
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
        }

      /* stop traversing */
      return TRUE;
    }

  /* continue traversing */
  return FALSE;
}



static gboolean
thunar_tree_model_node_traverse_remove (GNode   *node,
                                        gpointer user_data)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (user_data);
  GtkTreeIter      iter;
  GtkTreePath     *path;

  _thunar_return_val_if_fail (node->children == NULL, FALSE);

  /* determine the iterator for the node */
  GTK_TREE_ITER_INIT (iter, model->stamp, node);

  /* determine the path for the node */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
  if (G_LIKELY (path != NULL))
    {
      /* emit a "row-deleted" */
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);

      /* release the item for the node */
      thunar_tree_model_node_traverse_free (node, user_data);

      /* remove the node from the tree */
      g_node_destroy (node);

      /* release the path */
      gtk_tree_path_free (path);
    }

  return FALSE;
}



static gboolean
thunar_tree_model_node_traverse_sort (GNode   *node,
                                      gpointer user_data)
{
  ThunarTreeModel *model = THUNAR_TREE_MODEL (user_data);

  /* we don't want to sort the children of the root node */
  if (G_LIKELY (node != model->root))
    thunar_tree_model_sort (model, node);

  return FALSE;
}



static gboolean
thunar_tree_model_node_traverse_free (GNode   *node,
                                      gpointer user_data)
{
  if (G_LIKELY (node->data != NULL))
    thunar_tree_model_item_free (node->data);
  return FALSE;
}



static gboolean
thunar_tree_model_node_traverse_visible (GNode    *node,
                                         gpointer  user_data)
{
  ThunarTreeModelItem *item = node->data;
  ThunarTreeModel     *model = THUNAR_TREE_MODEL (user_data);
  GtkTreePath         *path;
  GtkTreeIter          iter;
  GNode               *child_node;
  GSList              *lp, *lnext;
  ThunarTreeModelItem *parent, *child;
  ThunarFile          *file;

  _thunar_return_val_if_fail (model->visible_func != NULL, FALSE);
  _thunar_return_val_if_fail (item == NULL || item->file == NULL || THUNAR_IS_FILE (item->file), FALSE);

  if (G_LIKELY (item != NULL && item->file != NULL))
    {
      /* check if this file should be visibily in the treeview */
      if (!model->visible_func (model, item->file, model->visible_data))
        {
          /* delete all the children of the node */
          while (node->children)
            g_node_traverse (node->children, G_POST_ORDER, G_TRAVERSE_ALL, -1,
                             thunar_tree_model_node_traverse_remove, model);

          /* generate an iterator for the item */
          GTK_TREE_ITER_INIT (iter, model->stamp, node);

          /* remove this item from the tree */
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
          gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
          gtk_tree_path_free (path);

          /* insert the file in the invisible list of the parent */
          parent = node->parent->data;
          if (G_LIKELY (parent))
            parent->invisible_children = g_slist_prepend (parent->invisible_children,
                                                          g_object_ref (G_OBJECT (item->file)));

          /* free the item and destroy the node */
          thunar_tree_model_item_free (item);
          g_node_destroy (node);
        }
      else if (!G_NODE_HAS_DUMMY (node))
        {
          /* this node should be visible. check if the node has invisible
           * files that should be visible too */
          for (lp = item->invisible_children, child_node = NULL; lp != NULL; lp = lnext)
            {
              lnext = lp->next;
              file = THUNAR_FILE (lp->data);

              _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

              if (model->visible_func (model, file, model->visible_data))
                {
                  /* allocate a new item for the file */
                  child = thunar_tree_model_item_new_with_file (model, file);

                  /* insert a new node for the child */
                  child_node = g_node_append_data (node, child);

                  /* determine the tree iter for the child */
                  GTK_TREE_ITER_INIT (iter, model->stamp, child_node);

                  /* emit a "row-inserted" for the new node */
                  path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
                  gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
                  gtk_tree_path_free (path);

                  /* release the reference on the file hold by the invisible list */
                  g_object_unref (G_OBJECT (file));

                  /* delete the file in the list */
                  item->invisible_children = g_slist_delete_link (item->invisible_children, lp);

                  /* insert dummy */
                  thunar_tree_model_node_insert_dummy (child_node, model);
                }
            }

          /* sort this node if one of new children have been added */
          if (child_node != NULL)
            thunar_tree_model_sort (model, node);
        }
    }

  return FALSE;
}



/**
 * thunar_tree_model_get_case_sensitive:
 * @model : a #ThunarTreeModel.
 *
 * Returns %TRUE if the sorting for @model is done
 * in a case-sensitive manner.
 *
 * Return value: %TRUE if @model is sorted case-sensitive.
 **/
static gboolean
thunar_tree_model_get_case_sensitive (ThunarTreeModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_MODEL (model), FALSE);
  return model->sort_case_sensitive;
}



/**
 * thunar_tree_model_set_case_sensitive:
 * @model          : a #ThunarTreeModel.
 * @case_sensitive : %TRUE to sort @model case-sensitive.
 *
 * If @case_sensitive is %TRUE the @model will be sorted
 * in a case-sensitive manner.
 **/
static void
thunar_tree_model_set_case_sensitive (ThunarTreeModel *model,
                                      gboolean         case_sensitive)
{
  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (model));

  /* normalize the setting */
  case_sensitive = !!case_sensitive;

  /* check if we have a new setting */
  if (model->sort_case_sensitive != case_sensitive)
    {
      /* apply the new setting */
      model->sort_case_sensitive = case_sensitive;

      /* resort the model with the new setting */
      g_node_traverse (model->root, G_POST_ORDER, G_TRAVERSE_NON_LEAVES, -1, thunar_tree_model_node_traverse_sort, model);

      /* notify listeners */
      g_object_notify (G_OBJECT (model), "case-sensitive");
    }
}



/**
 * thunar_tree_model_set_visible_func:
 * @model : a #ThunarTreeModel.
 * @func  : a #ThunarTreeModelVisibleFunc, the visible function.
 * @data  : User data to pass to the visible function, or %NULL.
 *
 * Sets the visible function used when filtering the #ThunarTreeModel.
 * The function should return %TRUE if the given row should be visible
 * and %FALSE otherwise.
 **/
void
thunar_tree_model_set_visible_func (ThunarTreeModel            *model,
                                    ThunarTreeModelVisibleFunc  func,
                                    gpointer                    data)
{
  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (model));
  _thunar_return_if_fail (func != NULL);

  /* set the new visiblity function and user data */
  model->visible_func = func;
  model->visible_data = data;
}



/**
 * thunar_tree_model_refilter:
 * @model : a #ThunarTreeModel.
 *
 * Walks all the folders in the #ThunarTreeModel and updates their
 * visibility.
 **/
void
thunar_tree_model_refilter (ThunarTreeModel *model)
{
  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (model));

  /* traverse all nodes to update their visibility */
  g_node_traverse (model->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
                   thunar_tree_model_node_traverse_visible, model);
}



/**
 * thunar_tree_model_cleanup:
 * @model : a #ThunarTreeModel.
 *
 * Walks all the folders in the #ThunarTreeModel and release them when
 * they are unused by the treeview.
 **/
void
thunar_tree_model_cleanup (ThunarTreeModel *model)
{
  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (model));

  /* schedule an idle cleanup, if not already done */
  if (model->cleanup_idle_id == 0)
    {
      model->cleanup_idle_id = g_timeout_add_full (G_PRIORITY_LOW, 500, thunar_tree_model_cleanup_idle,
                                                   model, thunar_tree_model_cleanup_idle_destroy);
    }
}



/**
 * thunar_tree_model_node_has_dummy:
 * @model : a #ThunarTreeModel.
 * @node : GNode to check
 *
 * Checks if node is a dummy node ( if it only has a dummy item )
 *
 * Return value: %TRUE if @node has a dummy item
 **/
gboolean
thunar_tree_model_node_has_dummy (ThunarTreeModel *model,
                                  GNode           *node)
{
  _thunar_return_val_if_fail (THUNAR_IS_TREE_MODEL (model), TRUE);
  return G_NODE_HAS_DUMMY(node);
}



/**
 * thunar_tree_model_add_child:
 * @model : a #ThunarTreeModel.
 * @node : GNode to add a child
 * @file : #ThunarFile to be added
 *
 * Creates a new #ThunarTreeModelItem as a child of @node and stores a reference to the passed @file
 * Automatically creates/removes dummy items if required
 **/
void
thunar_tree_model_add_child (ThunarTreeModel *model,
                             GNode           *node,
                             ThunarFile      *file)
{
  ThunarTreeModelItem *child_item;
  GNode               *child_node;
  GtkTreeIter          child_iter;
  GtkTreePath         *child_path;

  _thunar_return_if_fail (THUNAR_IS_TREE_MODEL (model));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* allocate a new item for the file */
  child_item = thunar_tree_model_item_new_with_file (model, file);

  /* check if the node has only the dummy child */
  if (G_UNLIKELY (G_NODE_HAS_DUMMY (node)))
    {
      /* replace the dummy node with the new node */
      child_node = g_node_first_child (node);
      child_node->data = child_item;

      /* determine the tree iter for the child */
      GTK_TREE_ITER_INIT (child_iter, model->stamp, child_node);

      /* emit a "row-changed" for the new node */
      child_path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &child_iter);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (model), child_path, &child_iter);
      gtk_tree_path_free (child_path);
    }
  else
    {
      /* insert a new item for the child */
      child_node = g_node_append_data (node, child_item);

      /* determine the tree iter for the child */
      GTK_TREE_ITER_INIT (child_iter, model->stamp, child_node);

      /* emit a "row-inserted" for the new node */
      child_path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &child_iter);
      gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), child_path, &child_iter);
      gtk_tree_path_free (child_path);
    }

  /* add a dummy to the new child */
  thunar_tree_model_node_insert_dummy (child_node, model);
}



static gboolean
thunar_tree_model_default_visiblity (ThunarTreeModel *model,
                                     ThunarFile      *file,
                                     gpointer         data)
{
  return TRUE;
}
