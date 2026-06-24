/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
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

#include <exo-desktop-item-edit/exo-die-desktop-model.h>



typedef struct _ExoDieDesktopItem ExoDieDesktopItem;



static void               exo_die_desktop_model_tree_model_init       (GtkTreeModelIface        *iface);
static void               exo_die_desktop_model_finalize              (GObject                  *object);
static GtkTreeModelFlags  exo_die_desktop_model_get_flags             (GtkTreeModel             *tree_model);
static gint               exo_die_desktop_model_get_n_columns         (GtkTreeModel             *tree_model);
static GType              exo_die_desktop_model_get_column_type       (GtkTreeModel             *tree_model,
                                                                       gint                      column);
static gboolean           exo_die_desktop_model_get_iter              (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter,
                                                                       GtkTreePath              *path);
static GtkTreePath       *exo_die_desktop_model_get_path              (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter);
static void               exo_die_desktop_model_get_value             (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter,
                                                                       gint                      column,
                                                                       GValue                   *value);
static gboolean           exo_die_desktop_model_iter_next             (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter);
static gboolean           exo_die_desktop_model_iter_children         (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter,
                                                                       GtkTreeIter              *parent);
static gboolean           exo_die_desktop_model_iter_has_child        (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter);
static gint               exo_die_desktop_model_iter_n_children       (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter);
static gboolean           exo_die_desktop_model_iter_nth_child        (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter,
                                                                       GtkTreeIter              *parent,
                                                                       gint                      n);
static gboolean           exo_die_desktop_model_iter_parent           (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter,
                                                                       GtkTreeIter              *child);
static gboolean           exo_die_desktop_model_collect_idle          (gpointer                  user_data);
static void               exo_die_desktop_model_collect_idle_destroy  (gpointer                  user_data);
static GSList            *exo_die_desktop_model_collect_readdir       (ExoDieDesktopModel       *desktop_model,
                                                                       const gchar              *dir_path);
static gpointer           exo_die_desktop_model_collect_thread        (gpointer                  user_data);
static ExoDieDesktopItem *exo_die_desktop_item_new_from_file          (const gchar              *file);
static gint               exo_die_desktop_item_compare                (gconstpointer             desktop_item_a,
                                                                       gconstpointer             desktop_item_b);
static void               exo_die_desktop_item_free                   (ExoDieDesktopItem        *desktop_item);



struct _ExoDieDesktopModelClass
{
  GObjectClass __parent__;
};

struct _ExoDieDesktopModel
{
  GObject           __parent__;
  gint              stamp;
  GSList           *items;

  gint              collect_idle_id;
  GSList           *collect_items;
  GThread          *collect_thread;
  volatile gboolean collect_cancelled;
};

struct _ExoDieDesktopItem
{
  gchar *command;
  gchar *comment;
  gchar *icon;
  gchar *name;
  guint  snotify : 1;
  guint  terminal : 1;
};



G_DEFINE_TYPE_WITH_CODE (ExoDieDesktopModel, exo_die_desktop_model, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, exo_die_desktop_model_tree_model_init))



static void
exo_die_desktop_model_class_init (ExoDieDesktopModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = exo_die_desktop_model_finalize;
}



static void
exo_die_desktop_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = exo_die_desktop_model_get_flags;
  iface->get_n_columns = exo_die_desktop_model_get_n_columns;
  iface->get_column_type = exo_die_desktop_model_get_column_type;
  iface->get_iter = exo_die_desktop_model_get_iter;
  iface->get_path = exo_die_desktop_model_get_path;
  iface->get_value = exo_die_desktop_model_get_value;
  iface->iter_next = exo_die_desktop_model_iter_next;
  iface->iter_children = exo_die_desktop_model_iter_children;
  iface->iter_has_child = exo_die_desktop_model_iter_has_child;
  iface->iter_n_children = exo_die_desktop_model_iter_n_children;
  iface->iter_nth_child = exo_die_desktop_model_iter_nth_child;
  iface->iter_parent = exo_die_desktop_model_iter_parent;
}



static void
exo_die_desktop_model_init (ExoDieDesktopModel *desktop_model)
{
  /* generate a unique stamp */
  desktop_model->stamp = g_random_int ();

  /* spawn the collector thread */
  desktop_model->collect_thread = g_thread_new ("ExoDesktopCollect", exo_die_desktop_model_collect_thread, desktop_model);
}



static void
exo_die_desktop_model_finalize (GObject *object)
{
  ExoDieDesktopModel *desktop_model = EXO_DIE_DESKTOP_MODEL (object);

  /* join the collector thread */
  desktop_model->collect_cancelled = TRUE;
  g_thread_join (desktop_model->collect_thread);

  /* cancel any pending collect idle source */
  if (G_UNLIKELY (desktop_model->collect_idle_id > 0))
    g_source_remove (desktop_model->collect_idle_id);

  /* release collected items (if any) */
  g_slist_foreach (desktop_model->collect_items, (GFunc) (void (*)(void)) exo_die_desktop_item_free, NULL);
  g_slist_free (desktop_model->collect_items);

  /* release all items */
  g_slist_foreach (desktop_model->items, (GFunc) (void (*)(void)) exo_die_desktop_item_free, NULL);
  g_slist_free (desktop_model->items);

  (*G_OBJECT_CLASS (exo_die_desktop_model_parent_class)->finalize) (object);
}



static GtkTreeModelFlags
exo_die_desktop_model_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}



static gint
exo_die_desktop_model_get_n_columns (GtkTreeModel *tree_model)
{
  return EXO_DIE_DESKTOP_MODEL_N_COLUMNS;
}



static GType
exo_die_desktop_model_get_column_type (GtkTreeModel *tree_model,
                                       gint          column)
{
  switch (column)
    {
    case EXO_DIE_DESKTOP_MODEL_COLUMN_ABSTRACT:
    case EXO_DIE_DESKTOP_MODEL_COLUMN_COMMAND:
    case EXO_DIE_DESKTOP_MODEL_COLUMN_COMMENT:
    case EXO_DIE_DESKTOP_MODEL_COLUMN_ICON:
    case EXO_DIE_DESKTOP_MODEL_COLUMN_NAME:
      return G_TYPE_STRING;

    case EXO_DIE_DESKTOP_MODEL_COLUMN_SNOTIFY:
    case EXO_DIE_DESKTOP_MODEL_COLUMN_TERMINAL:
      return G_TYPE_BOOLEAN;

    default:
      g_assert_not_reached ();
      return G_TYPE_INVALID;
    }
}



static gboolean
exo_die_desktop_model_get_iter (GtkTreeModel *tree_model,
                                GtkTreeIter  *iter,
                                GtkTreePath  *path)
{
  ExoDieDesktopModel *desktop_model = EXO_DIE_DESKTOP_MODEL (tree_model);

  g_return_val_if_fail (EXO_DIE_IS_DESKTOP_MODEL (desktop_model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  iter->stamp = desktop_model->stamp;
  iter->user_data = g_slist_nth (desktop_model->items, gtk_tree_path_get_indices (path)[0]);

  return (iter->user_data != NULL);
}



static GtkTreePath*
exo_die_desktop_model_get_path (GtkTreeModel *tree_model,
                                GtkTreeIter  *iter)
{
  ExoDieDesktopModel *desktop_model = EXO_DIE_DESKTOP_MODEL (tree_model);
  gint                idx;

  g_return_val_if_fail (EXO_DIE_IS_DESKTOP_MODEL (desktop_model), NULL);
  g_return_val_if_fail (iter->stamp == desktop_model->stamp, NULL);

  /* determine the index of the iter */
  idx = g_slist_position (desktop_model->items, iter->user_data);
  if (G_UNLIKELY (idx < 0))
    return NULL;

  return gtk_tree_path_new_from_indices (idx, -1);
}



static void
exo_die_desktop_model_get_value (GtkTreeModel *tree_model,
                                 GtkTreeIter  *iter,
                                 gint          column,
                                 GValue       *value)
{
  ExoDieDesktopModel *desktop_model = EXO_DIE_DESKTOP_MODEL (tree_model);
  ExoDieDesktopItem  *desktop_item;
  gchar              *escaped;

  g_return_if_fail (EXO_DIE_IS_DESKTOP_MODEL (desktop_model));
  g_return_if_fail (iter->stamp == desktop_model->stamp);

  desktop_item = g_slist_nth_data (iter->user_data, 0);

  switch (column)
    {
    case EXO_DIE_DESKTOP_MODEL_COLUMN_ABSTRACT:
      g_value_init (value, G_TYPE_STRING);
      escaped = g_markup_escape_text (desktop_item->name, -1);
      g_value_take_string (value, g_strdup_printf (_("Create Launcher <b>%s</b>"), escaped));
      g_free (escaped);
      break;

    case EXO_DIE_DESKTOP_MODEL_COLUMN_COMMAND:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, desktop_item->command);
      break;

    case EXO_DIE_DESKTOP_MODEL_COLUMN_COMMENT:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, desktop_item->comment);
      break;

    case EXO_DIE_DESKTOP_MODEL_COLUMN_ICON:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, desktop_item->icon);
      break;

    case EXO_DIE_DESKTOP_MODEL_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, desktop_item->name);
      break;

    case EXO_DIE_DESKTOP_MODEL_COLUMN_SNOTIFY:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, desktop_item->snotify);
      break;

    case EXO_DIE_DESKTOP_MODEL_COLUMN_TERMINAL:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, desktop_item->terminal);
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}



static gboolean
exo_die_desktop_model_iter_next (GtkTreeModel *tree_model,
                                 GtkTreeIter  *iter)
{
  g_return_val_if_fail (EXO_DIE_IS_DESKTOP_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter->stamp == EXO_DIE_DESKTOP_MODEL (tree_model)->stamp, FALSE);

  iter->user_data = g_slist_next (iter->user_data);
  return (iter->user_data != NULL);
}



static gboolean
exo_die_desktop_model_iter_children (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter,
                                     GtkTreeIter  *parent)
{
  ExoDieDesktopModel *desktop_model = EXO_DIE_DESKTOP_MODEL (tree_model);

  g_return_val_if_fail (EXO_DIE_IS_DESKTOP_MODEL (desktop_model), FALSE);

  if (G_LIKELY (parent == NULL && desktop_model->items != NULL))
    {
      iter->stamp = desktop_model->stamp;
      iter->user_data = desktop_model->items;
      return TRUE;
    }

  return FALSE;
}



static gboolean
exo_die_desktop_model_iter_has_child (GtkTreeModel *tree_model,
                                      GtkTreeIter  *iter)
{
  return FALSE;
}



static gint
exo_die_desktop_model_iter_n_children (GtkTreeModel *tree_model,
                                       GtkTreeIter  *iter)
{
  ExoDieDesktopModel *desktop_model = EXO_DIE_DESKTOP_MODEL (tree_model);

  g_return_val_if_fail (EXO_DIE_IS_DESKTOP_MODEL (desktop_model), 0);

  return (iter == NULL) ? g_slist_length (desktop_model->items) : 0;
}



static gboolean
exo_die_desktop_model_iter_nth_child (GtkTreeModel *tree_model,
                                      GtkTreeIter  *iter,
                                      GtkTreeIter  *parent,
                                      gint          n)
{
  ExoDieDesktopModel *desktop_model = EXO_DIE_DESKTOP_MODEL (tree_model);

  g_return_val_if_fail (EXO_DIE_IS_DESKTOP_MODEL (desktop_model), FALSE);

  if (G_LIKELY (parent != NULL))
    {
      iter->stamp = desktop_model->stamp;
      iter->user_data = g_slist_nth (desktop_model->items, n);
      return (iter->user_data != NULL);
    }

  return FALSE;
}



static gboolean
exo_die_desktop_model_iter_parent (GtkTreeModel *tree_model,
                                   GtkTreeIter  *iter,
                                   GtkTreeIter  *child)
{
  return FALSE;
}



static gboolean
exo_die_desktop_model_collect_idle (gpointer user_data)
{
  ExoDieDesktopModel *desktop_model = EXO_DIE_DESKTOP_MODEL (user_data);
  GtkTreePath        *path;
  GtkTreeIter         iter;
  GSList             *lp;
  GSList             *np;

  g_return_val_if_fail (EXO_DIE_IS_DESKTOP_MODEL (desktop_model), FALSE);
  g_return_val_if_fail (desktop_model->items == NULL, FALSE);

  /* move the collected items "online" */
  desktop_model->items = desktop_model->collect_items;
  desktop_model->collect_items = NULL;

  /* emit notifications for all new items */
  path = gtk_tree_path_new_first ();
  for (lp = desktop_model->items; lp != NULL; lp = lp->next)
    {
      /* remember the next item */
      np = lp->next;
      lp->next = NULL;

      /* generate the iterator */
      iter.stamp = desktop_model->stamp;
      iter.user_data = lp;

      /* emit the "row-inserted" signal */
      gtk_tree_model_row_inserted (GTK_TREE_MODEL (desktop_model), path, &iter);

      /* advance the path */
      gtk_tree_path_next (path);

      /* reset the next item */
      lp->next = np;
    }
  gtk_tree_path_free (path);

  return FALSE;
}



static void
exo_die_desktop_model_collect_idle_destroy (gpointer user_data)
{
  EXO_DIE_DESKTOP_MODEL (user_data)->collect_idle_id = 0;
}



static GSList*
exo_die_desktop_model_collect_readdir (ExoDieDesktopModel *desktop_model,
                                       const gchar        *dir_path)
{
  ExoDieDesktopItem *desktop_item;
  const gchar       *name;
  GSList            *items = NULL;
  gchar             *path;
  GDir              *dp;

  /* try to open the directory */
  dp = g_dir_open (dir_path, 0, NULL);
  if (G_LIKELY (dp != NULL))
    {
      /* process the files within this directory */
      while (!desktop_model->collect_cancelled)
        {
          /* read the next file entry */
          name = g_dir_read_name (dp);
          if (G_UNLIKELY (name == NULL))
            break;

          /* generate the absolute path to the file entry */
          path = g_build_filename (dir_path, name, NULL);

          /* check if we have a directory or a regular file here */
          if (g_file_test (path, G_FILE_TEST_IS_DIR))
            {
              /* recurse for directories */
              items = g_slist_concat (items, exo_die_desktop_model_collect_readdir (desktop_model, path));
            }
          else if (g_file_test (path, G_FILE_TEST_IS_REGULAR) && g_str_has_suffix (name, ".desktop"))
            {
              /* try to parse the .desktop file */
              desktop_item = exo_die_desktop_item_new_from_file (path);
              if (G_LIKELY (desktop_item != NULL))
                items = g_slist_prepend (items, desktop_item);
            }

          /* cleanup */
          g_free (path);
        }

      /* close the directory handle */
      g_dir_close (dp);
    }

  return items;
}



static gpointer
exo_die_desktop_model_collect_thread (gpointer user_data)
{
  ExoDieDesktopModel *desktop_model = EXO_DIE_DESKTOP_MODEL (user_data);
  GSList             *items = NULL;
  gchar             **paths;
  guint               n;

  /* determine the available applications/ directories */
  paths = xfce_resource_lookup_all (XFCE_RESOURCE_DATA, "applications/");
  for (n = 0; !desktop_model->collect_cancelled && paths[n] != NULL; ++n)
    {
      /* collect this directory */
      items = g_slist_concat (items, exo_die_desktop_model_collect_readdir (desktop_model, paths[n]));
    }

  /* check if we're still active */
  if (G_LIKELY (!desktop_model->collect_cancelled && items != NULL))
    {
      /* tell the model about the items (sorting the items by their names) */
      desktop_model->collect_items = g_slist_sort (items, exo_die_desktop_item_compare);

      /* and schedule an idle source */
      desktop_model->collect_idle_id = gdk_threads_add_idle_full (G_PRIORITY_LOW, exo_die_desktop_model_collect_idle,
                                                        desktop_model, exo_die_desktop_model_collect_idle_destroy);
    }
  else
    {
      /* release the collected items */
      g_slist_foreach (items, (GFunc) (void (*)(void)) exo_die_desktop_item_free, NULL);
      g_slist_free (items);
    }

  /* cleanup */
  g_strfreev (paths);

  return NULL;
}



static ExoDieDesktopItem*
exo_die_desktop_item_new_from_file (const gchar *file)
{
  ExoDieDesktopItem *desktop_item = NULL;
  const gchar       *comment;
  const gchar       *command;
  const gchar       *icon;
  const gchar       *name;
  const gchar       *type;
  XfceRc            *rc;
  gint               icon_len;

  /* try to open the file */
  rc = xfce_rc_simple_open (file, TRUE);
  if (G_LIKELY (rc != NULL))
    {
      /* skip hidden items to avoid confusion */
      xfce_rc_set_group (rc, "Desktop Entry");
      if (!xfce_rc_read_bool_entry (rc, "Hidden", FALSE) && !xfce_rc_read_bool_entry (rc, "NoDisplay", FALSE))
        {
          /* determine the attributes from the file */
          command = xfce_rc_read_entry_untranslated (rc, "Exec", NULL);
          comment = xfce_rc_read_entry (rc, "Comment", NULL);
          icon = xfce_rc_read_entry (rc, "Icon", NULL);
          name = xfce_rc_read_entry (rc, "Name", NULL);
          type = xfce_rc_read_entry (rc, "Type", "Application");

          /* check if the required attributes were found (and we have an Application) */
          if (G_LIKELY (strcmp (type, "Application") == 0 && command != NULL && name != NULL))
            {
              /* allocate the desktop item */
              desktop_item = g_new (ExoDieDesktopItem, 1);
              desktop_item->command = g_strdup (command);
              desktop_item->comment = g_strdup (comment);
              desktop_item->icon = g_strdup (icon);
              desktop_item->name = g_strdup (name);

              /* strip off known extensions from the icon */
              if (G_LIKELY (desktop_item->icon != NULL))
                {
                  /* check if ends with ".png" */
                  icon_len = strlen (desktop_item->icon);
                  if (icon_len > 4 && strcmp (desktop_item->icon + (icon_len - 4), ".png") == 0)
                    desktop_item->icon[icon_len - 4] = '\0';
                }

              /* strip the "Xfce 4 " prefix from the names */
              if (strncmp (desktop_item->name, "Xfce 4 ", 7) == 0)
                {
                  /* release the full name */
                  g_free (desktop_item->name);

                  /* use the short name */
                  desktop_item->name = g_strdup (name + 7);
                }

              /* check if startup notification is supported */
              desktop_item->snotify = (xfce_rc_read_bool_entry (rc, "StartupNotify", FALSE) || xfce_rc_read_bool_entry (rc, "X-KDE-StartupNotify", FALSE));

              /* check if should be run in terminal */
              desktop_item->terminal = xfce_rc_read_bool_entry (rc, "Terminal", FALSE) ? TRUE : FALSE;
            }
        }

      /* close the file */
      xfce_rc_close (rc);
    }

  return desktop_item;
}



static gint
exo_die_desktop_item_compare (gconstpointer desktop_item_a,
                              gconstpointer desktop_item_b)
{
  return g_utf8_collate (((ExoDieDesktopItem *) desktop_item_a)->name, ((ExoDieDesktopItem *) desktop_item_b)->name);
}



static void
exo_die_desktop_item_free (ExoDieDesktopItem *desktop_item)
{
  g_free (desktop_item->command);
  g_free (desktop_item->comment);
  g_free (desktop_item->icon);
  g_free (desktop_item->name);
  g_free (desktop_item);
}



/**
 * exo_die_desktop_model_new:
 *
 * Allocates a new #ExoDieDesktopModel instance.
 *
 * Return value: the newly allocated #ExoDieDesktopModel.
 **/
ExoDieDesktopModel*
exo_die_desktop_model_new (void)
{
  return g_object_new (EXO_DIE_TYPE_DESKTOP_MODEL, NULL);
}



/**
 * exo_die_desktop_model_match_func:
 * @completion : a #GtkEntryCompletion.
 * @key        : the text to match.
 * @iter       : a valid #GtkTreeIter for the row to match.
 * @user_data  : a #ExoDieDesktopModel.
 *
 * Convenience function to match the @iter with the specified @key.
 *
 * Return value: %TRUE if @iter is a possible match, %FALSE otherwise.
 **/
gboolean
exo_die_desktop_model_match_func (GtkEntryCompletion *completion,
                                  const gchar        *key,
                                  GtkTreeIter        *iter,
                                  gpointer            user_data)
{
  ExoDieDesktopModel *desktop_model = EXO_DIE_DESKTOP_MODEL (user_data);
  ExoDieDesktopItem  *desktop_item;
  gboolean            matches = FALSE;
  gchar              *casefolded;
  gchar              *normalized;

  g_return_val_if_fail (EXO_DIE_IS_DESKTOP_MODEL (desktop_model), FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), FALSE);
  g_return_val_if_fail (iter->stamp == desktop_model->stamp, FALSE);
  g_return_val_if_fail (g_utf8_validate (key, -1, NULL), FALSE);

  /* determine the item for the iter */
  desktop_item = g_slist_nth_data (iter->user_data, 0);

  /* check if the name matches */
  if (G_LIKELY (desktop_item->name != NULL))
    {
      normalized = g_utf8_normalize (desktop_item->name, -1, G_NORMALIZE_ALL);
      casefolded = g_utf8_casefold (normalized, -1);
      if (G_LIKELY (casefolded != NULL && key != NULL))
        matches = (strstr (casefolded, key) != NULL);
      g_free (casefolded);
      g_free (normalized);
    }

  /* check if no hit yet */
  if (G_LIKELY (!matches && desktop_item->comment != NULL))
    {
      /* also check the comment then */
      normalized = g_utf8_normalize (desktop_item->comment, -1, G_NORMALIZE_ALL);
      casefolded = g_utf8_casefold (normalized, -1);
      if (G_LIKELY (casefolded != NULL && key != NULL))
        matches = (strstr (casefolded, key) != NULL);
      g_free (casefolded);
      g_free (normalized);
    }

  return matches;
}

