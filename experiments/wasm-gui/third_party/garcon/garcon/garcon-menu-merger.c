/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include <garcon/garcon-menu-node.h>
#include <garcon/garcon-menu-tree-provider.h>
#include <garcon/garcon-menu-parser.h>
#include <garcon/garcon-menu-merger.h>
#include <garcon/garcon-private.h>



typedef struct _GarconMenuMergerContext GarconMenuMergerContext;



/* Property identifiers */
enum
{
  PROP_0,
  PROP_TREE_PROVIDER,
};



static void     garcon_menu_merger_provider_init           (GarconMenuTreeProviderIface *iface);
static void     garcon_menu_merger_constructed             (GObject                     *object);
static void     garcon_menu_merger_finalize                (GObject                     *object);
static void     garcon_menu_merger_get_property            (GObject                     *object,
                                                            guint                        prop_id,
                                                            GValue                      *value,
                                                            GParamSpec                  *pspec);
static void     garcon_menu_merger_set_property            (GObject                     *object,
                                                            guint                        prop_id,
                                                            const GValue                *value,
                                                            GParamSpec                  *pspec);
static GNode   *garcon_menu_merger_get_tree                (GarconMenuTreeProvider      *provider);
static GFile   *garcon_menu_merger_get_file                (GarconMenuTreeProvider      *provider);
static gboolean garcon_menu_merger_resolve_default_dirs    (GNode                       *node,
                                                            GarconMenuMergerContext     *context);
static gboolean garcon_menu_merger_resolve_relative_paths  (GNode                       *node,
                                                            GarconMenuMergerContext     *context);
static void     garcon_menu_merger_remove_duplicate_paths  (GNode                       *node,
                                                            GarconMenuNodeType           type);
static void     garcon_menu_merger_consolidate_child_menus (GNode                       *node);
static gboolean garcon_menu_merger_resolve_merge_dirs      (GNode                       *node,
                                                            GarconMenuMergerContext     *context);
static gboolean garcon_menu_merger_process_merge_files     (GNode                       *node,
                                                            GarconMenuMergerContext     *context);
static void     garcon_menu_merger_clean_up_elements       (GNode                       *node,
                                                            GarconMenuNodeType           type);
static void     garcon_menu_merger_resolve_moves           (GNode                       *node);
static void     garcon_menu_merger_prepend_default_layout  (GNode                       *node);



struct _GarconMenuMergerPrivate
{
  GarconMenuTreeProvider *tree_provider;
  GNode                  *menu;
  GList                  *file_stack;
};

struct _GarconMenuMergerContext
{
  GarconMenuNodeType node_type;
  GarconMenuMerger  *merger;
  GCancellable      *cancellable;
  GError           **error;
  gboolean           success;
  GList             *file_stack;
  GList            **merge_files;
  GList            **merge_dirs;
};



G_DEFINE_TYPE_WITH_CODE (GarconMenuMerger, garcon_menu_merger,
                         G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GarconMenuMerger)
                         G_IMPLEMENT_INTERFACE (GARCON_TYPE_MENU_TREE_PROVIDER,
                                                garcon_menu_merger_provider_init))



static void
garcon_menu_merger_class_init (GarconMenuMergerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = garcon_menu_merger_finalize;
  gobject_class->constructed = garcon_menu_merger_constructed;
  gobject_class->get_property = garcon_menu_merger_get_property;
  gobject_class->set_property = garcon_menu_merger_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_TREE_PROVIDER,
                                   g_param_spec_object ("tree-provider",
                                                        "tree-provider",
                                                        "tree-provider",
                                                        GARCON_TYPE_MENU_TREE_PROVIDER,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS |
                                                        G_PARAM_CONSTRUCT_ONLY));
}



static void
garcon_menu_merger_provider_init (GarconMenuTreeProviderIface *iface)
{
  iface->get_tree = garcon_menu_merger_get_tree;
  iface->get_file = garcon_menu_merger_get_file;
}



static void
garcon_menu_merger_init (GarconMenuMerger *merger)
{
  merger->priv = garcon_menu_merger_get_instance_private (merger);
  merger->priv->tree_provider = NULL;
  merger->priv->menu = NULL;
  merger->priv->file_stack = NULL;
}



static void
garcon_menu_merger_constructed (GObject *object)
{
  GarconMenuMerger *merger = GARCON_MENU_MERGER (object);

  merger->priv->menu = garcon_menu_tree_provider_get_tree (merger->priv->tree_provider);
}



static void
garcon_menu_merger_finalize (GObject *object)
{
  GarconMenuMerger *merger = GARCON_MENU_MERGER (object);

  garcon_menu_node_tree_free (merger->priv->menu);

  g_object_unref (merger->priv->tree_provider);

  (*G_OBJECT_CLASS (garcon_menu_merger_parent_class)->finalize) (object);
}



static void
garcon_menu_merger_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GarconMenuMerger *merger = GARCON_MENU_MERGER (object);

  switch (prop_id)
    {
    case PROP_TREE_PROVIDER:
      g_value_set_object (value, merger->priv->tree_provider);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
garcon_menu_merger_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GarconMenuMerger *merger = GARCON_MENU_MERGER (object);

  switch (prop_id)
    {
    case PROP_TREE_PROVIDER:
      merger->priv->tree_provider = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



GarconMenuMerger *
garcon_menu_merger_new (GarconMenuTreeProvider *provider)
{
  g_return_val_if_fail (GARCON_IS_MENU_TREE_PROVIDER (provider), NULL);
  return g_object_new (GARCON_TYPE_MENU_MERGER, "tree-provider", provider, NULL);
}



static gboolean
garcon_menu_merger_prepare_merging (GarconMenuMerger        *merger,
                                    GNode                   *tree,
                                    GarconMenuMergerContext *context)
{
  g_return_val_if_fail (GARCON_IS_MENU_MERGER (merger), FALSE);
  g_return_val_if_fail (context != NULL, FALSE);

  context->node_type = GARCON_MENU_NODE_TYPE_INVALID;
  g_node_traverse (tree, G_IN_ORDER, G_TRAVERSE_ALL, -1,
                   (GNodeTraverseFunc) garcon_menu_merger_resolve_default_dirs,
                   context);

  context->node_type = GARCON_MENU_NODE_TYPE_MERGE_DIR;
  g_node_traverse (tree, G_IN_ORDER, G_TRAVERSE_ALL, -1,
                   (GNodeTraverseFunc) garcon_menu_merger_resolve_relative_paths,
                   context);

  context->node_type = GARCON_MENU_NODE_TYPE_MERGE_FILE;
  g_node_traverse (tree, G_IN_ORDER, G_TRAVERSE_ALL, -1,
                   (GNodeTraverseFunc) garcon_menu_merger_resolve_relative_paths,
                   context);

  garcon_menu_merger_remove_duplicate_paths (tree, GARCON_MENU_NODE_TYPE_MERGE_DIR);

  g_node_traverse (tree, G_IN_ORDER, G_TRAVERSE_ALL, -1,
                   (GNodeTraverseFunc) garcon_menu_merger_resolve_merge_dirs,
                   context);

  garcon_menu_merger_remove_duplicate_paths (tree, GARCON_MENU_NODE_TYPE_MERGE_FILE);

  return context->success;
}


/**
 * garcon_menu_merger_run:
 * @merger: a #GarconMenuMerger
 * @merge_files: (element-type utf8): list of files to merge
 * @merge_dirs: (element-type utf8): list of menu directories to merge
 * @cancellable:
 * @error:
 *
 */
gboolean
garcon_menu_merger_run (GarconMenuMerger *merger,
                        GList           **merge_files,
                        GList           **merge_dirs,
                        GCancellable     *cancellable,
                        GError          **error)
{
  GarconMenuMergerContext context;
  GFile                  *file;

  g_return_val_if_fail (GARCON_IS_MENU_MERGER (merger), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  context.merger = merger;
  context.cancellable = cancellable;
  context.error = error;
  context.success = TRUE;
  context.file_stack = NULL;
  context.merge_files = merge_files;
  context.merge_dirs = merge_dirs;

  file = garcon_menu_tree_provider_get_file (GARCON_MENU_TREE_PROVIDER (merger));
  context.file_stack = g_list_concat (context.file_stack, merger->priv->file_stack);
  context.file_stack = g_list_prepend (context.file_stack, file);

  garcon_menu_merger_prepare_merging (merger, merger->priv->menu, &context);

  g_node_traverse (merger->priv->menu, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
                   (GNodeTraverseFunc) garcon_menu_merger_process_merge_files,
                   &context);

  garcon_menu_merger_consolidate_child_menus (merger->priv->menu);

  context.node_type = GARCON_MENU_NODE_TYPE_DEFAULT_APP_DIRS;
  g_node_traverse (merger->priv->menu, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
                   (GNodeTraverseFunc) garcon_menu_merger_resolve_default_dirs,
                   &context);

  context.node_type = GARCON_MENU_NODE_TYPE_APP_DIR;
  g_node_traverse (merger->priv->menu, G_IN_ORDER, G_TRAVERSE_ALL, -1,
                   (GNodeTraverseFunc) garcon_menu_merger_resolve_relative_paths,
                   &context);

  garcon_menu_merger_remove_duplicate_paths (merger->priv->menu, GARCON_MENU_NODE_TYPE_APP_DIR);

  context.node_type = GARCON_MENU_NODE_TYPE_DEFAULT_DIRECTORY_DIRS;
  g_node_traverse (merger->priv->menu, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
                   (GNodeTraverseFunc) garcon_menu_merger_resolve_default_dirs,
                   &context);

  context.node_type = GARCON_MENU_NODE_TYPE_DIRECTORY_DIR;
  g_node_traverse (merger->priv->menu, G_IN_ORDER, G_TRAVERSE_ALL, -1,
                   (GNodeTraverseFunc) garcon_menu_merger_resolve_relative_paths,
                   &context);

  garcon_menu_merger_remove_duplicate_paths (merger->priv->menu, GARCON_MENU_NODE_TYPE_DIRECTORY_DIR);
  garcon_menu_merger_remove_duplicate_paths (merger->priv->menu, GARCON_MENU_NODE_TYPE_DIRECTORY);

  garcon_menu_merger_resolve_moves (merger->priv->menu);

  garcon_menu_merger_consolidate_child_menus (merger->priv->menu);

  garcon_menu_merger_clean_up_elements (merger->priv->menu, GARCON_MENU_NODE_TYPE_DELETED);
  garcon_menu_merger_clean_up_elements (merger->priv->menu, GARCON_MENU_NODE_TYPE_ONLY_UNALLOCATED);

  garcon_menu_merger_prepend_default_layout (merger->priv->menu);
  garcon_menu_merger_clean_up_elements (merger->priv->menu, GARCON_MENU_NODE_TYPE_DEFAULT_LAYOUT);
  garcon_menu_merger_clean_up_elements (merger->priv->menu, GARCON_MENU_NODE_TYPE_LAYOUT);

  _garcon_g_list_free_full (context.file_stack, g_object_unref);

  return context.success;
}



static GNode *
garcon_menu_merger_get_tree (GarconMenuTreeProvider *provider)
{
  g_return_val_if_fail (GARCON_IS_MENU_MERGER (provider), NULL);
  return garcon_menu_node_tree_copy (GARCON_MENU_MERGER (provider)->priv->menu);
}



static GFile *
garcon_menu_merger_get_file (GarconMenuTreeProvider *provider)
{
  GarconMenuTreeProvider *provider_;

  g_return_val_if_fail (GARCON_IS_MENU_MERGER (provider), NULL);

  provider_ = GARCON_MENU_MERGER (provider)->priv->tree_provider;
  return garcon_menu_tree_provider_get_file (provider_);
}



static void
garcon_menu_merger_insert_default_dirs (GNode *parent,
                                        GNode *defaults_node)
{
  GarconMenuNodeType   type;
  GNode               *node;
  GNode               *prev_node;
  const gchar * const *dirs;
  const gchar         *kde_dir;
  int                  i;
  gchar               *path;
  gchar               *kde_data_dir;
  const gchar         *base_name;

  g_return_if_fail (parent != NULL);
  g_return_if_fail (defaults_node != NULL);

  prev_node = defaults_node;

  if  (garcon_menu_node_tree_get_node_type (defaults_node) == GARCON_MENU_NODE_TYPE_DEFAULT_DIRECTORY_DIRS)
    {
      base_name = "desktop-directories";
      type = GARCON_MENU_NODE_TYPE_DIRECTORY_DIR;
    }
  else
    {
      base_name = "applications";
      type = GARCON_MENU_NODE_TYPE_APP_DIR;
    }

  /* Append $KDEDIR/share/desktop-directories and $KDEDIR/share/applications
   * as a workaround for distributions not installing KDE menu files
   * properly into $XDG_DATA_DIRS */

  /* Get KDEDIR environment variable */
  kde_dir = g_getenv ("KDEDIR");

  /* Check if this variable is set */
  if (G_UNLIKELY (kde_dir != NULL))
    {
      /* Build KDE data dir */
      kde_data_dir = g_build_filename (kde_dir, "share", base_name, NULL);

      /* Add it as a directory dir */
      node = g_node_new (garcon_menu_node_create (type, kde_data_dir));
      prev_node = g_node_insert_after (parent, prev_node, node);

      /* Free the KDE data dir */
      g_free (kde_data_dir);
    }

  /* The $KDEDIR workaround ends here */

  /* Append system-wide data dirs, respecting the priority */
  dirs = g_get_system_data_dirs ();
  for (i = 0; dirs[i] != NULL; i++)
    {
      path = g_build_path (G_DIR_SEPARATOR_S, dirs[i], base_name, NULL);
      node = g_node_new (garcon_menu_node_create (type, path));
      g_node_insert_after (parent, prev_node, node);
      g_free (path);
    }

  /* Append user data dir */
  path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (), base_name, NULL);
  node = g_node_new (garcon_menu_node_create (type, path));
  g_node_append (parent, node);
  g_free (path);
}



static gint
compare_files (GFile *file,
               GFile *other_file)
{
  return g_file_equal (file, other_file) ? 0 : 1;
}




static void
garcon_menu_merger_insert_default_merge_dirs (GNode       *parent,
                                              GNode       *defaults_node,
                                              const gchar *merge_dir_basename)
{
  GNode               *node;
  GNode               *prev_node;
  const gchar * const *dirs;
  int                  i;
  gchar               *path;

  g_return_if_fail (parent != NULL);
  g_return_if_fail (defaults_node != NULL);
  g_return_if_fail (merge_dir_basename != NULL && *merge_dir_basename != '\0');

  prev_node = defaults_node;

  /* Append system-wide config dirs, respecting the priority */
  dirs = g_get_system_config_dirs ();
  for (i = 0; dirs[i] != NULL; i++)
    {
      path = g_build_path (G_DIR_SEPARATOR_S, dirs[i], "menus", 
                           merge_dir_basename, NULL);
      node = g_node_new (garcon_menu_node_create (GARCON_MENU_NODE_TYPE_MERGE_DIR, path));
      g_node_insert_after (parent, prev_node, node);
      g_free (path);
    }

  /* Append user config dir */
  path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (), "menus",
                       merge_dir_basename, NULL);
  node = g_node_new (garcon_menu_node_create (GARCON_MENU_NODE_TYPE_MERGE_DIR, path));
  g_node_append (parent, node);
  g_free (path);
}



static gboolean
garcon_menu_merger_resolve_default_dirs (GNode                   *node,
                                         GarconMenuMergerContext *context)
{
  const gchar *prefix;
  gboolean     is_application_menu = FALSE;
  GFile       *file;
  gchar       *applications_filename;
  gchar       *extension;
  gchar       *filename;
  gchar       *menu_name;
  gchar       *merge_dir_basename = NULL;

  g_return_val_if_fail (context != NULL, FALSE);

  if (garcon_menu_node_tree_get_node_type (node) == context->node_type)
    {
      garcon_menu_merger_insert_default_dirs (node->parent, node);
      garcon_menu_node_tree_free (node);
    }
  else if (garcon_menu_node_tree_get_node_type (node) == GARCON_MENU_NODE_TYPE_DEFAULT_MERGE_DIRS)
    {
      /* determine the .menu file basename */
      file = g_list_first (context->file_stack)->data;
      filename = g_file_get_basename (file);

      /* check if we have an application menu file */
      prefix = g_getenv ("XDG_MENU_PREFIX");
      applications_filename = g_strconcat (prefix != NULL ? prefix : _GARCON_DEFAULT_MENU_PREFIX,
                                           "applications.menu", NULL);
      if (g_strcmp0 (filename, applications_filename) == 0)
        is_application_menu = TRUE;
      g_free (applications_filename);

      /* derive the basename of default merge dirs from the menu filename */
      if (!is_application_menu)
        {
          /* find the extension */
          extension = g_strstr_len (filename, -1, ".");

          /* derive the merge dir name by stripping the extension */
          if (extension != NULL && extension > filename)
            {
              menu_name = g_strndup (filename, extension - filename);
              merge_dir_basename = g_strconcat (menu_name, "-merged", NULL);
              g_free (menu_name);
            }
        }

      /* free the filename */
      g_free (filename);

      /* use "applications-merged" as the fallback dir */
      if (merge_dir_basename == NULL)
        merge_dir_basename = g_strdup ("applications-merged");

      garcon_menu_merger_insert_default_merge_dirs (node->parent, node, 
                                                    merge_dir_basename);

      /* free the merge dir name */
      g_free (merge_dir_basename);

      garcon_menu_node_tree_free (node);
    }

  return FALSE;
}



static gboolean
garcon_menu_merger_resolve_relative_paths (GNode                   *node,
                                           GarconMenuMergerContext *context)
{
  GFile               *source_file;
  const gchar * const *system_config_dirs;
  const gchar        **config_dirs;
  gchar               *absolute_path = NULL;
  gchar               *relative_path = NULL;
  guint                i;

  g_return_val_if_fail (context != NULL, FALSE);

  if (garcon_menu_node_tree_get_node_type (node) != context->node_type)
    return FALSE;

  source_file = g_list_first (context->file_stack)->data;

  if (garcon_menu_node_tree_get_node_type (node) == GARCON_MENU_NODE_TYPE_APP_DIR ||
      garcon_menu_node_tree_get_node_type (node) == GARCON_MENU_NODE_TYPE_DIRECTORY_DIR ||
      garcon_menu_node_tree_get_node_type (node) == GARCON_MENU_NODE_TYPE_MERGE_DIR)
    {
      relative_path = (gchar *)garcon_menu_node_tree_get_string (node);
      absolute_path = _garcon_file_get_uri_relative_to_file (relative_path, source_file);
      garcon_menu_node_tree_set_string (node, absolute_path);
      g_free (absolute_path);
    }
  else if (garcon_menu_node_tree_get_node_type (node) == GARCON_MENU_NODE_TYPE_MERGE_FILE)
    {
      if (garcon_menu_node_tree_get_merge_file_type (node) == GARCON_MENU_MERGE_FILE_PATH)
        {
          relative_path = (gchar *)garcon_menu_node_tree_get_merge_file_filename (node);
          absolute_path = _garcon_file_get_uri_relative_to_file (relative_path, source_file);
          garcon_menu_node_tree_set_merge_file_filename (node, absolute_path);
          g_free (absolute_path);
        }
      else
        {
          system_config_dirs = g_get_system_config_dirs ();

          config_dirs = g_new0 (const gchar *, 2 + g_strv_length ((gchar **)system_config_dirs));

          config_dirs[0] = g_get_user_config_dir ();
          config_dirs[1 + g_strv_length ((gchar **)system_config_dirs)] = NULL;

          for (i = 0; system_config_dirs[i] != NULL; ++i)
            config_dirs[i+1] = system_config_dirs[i];

          /* Find the parent XDG_CONFIG_DIRS entry for the current menu file */
          for (i = 0; relative_path == NULL && config_dirs[i] != NULL; ++i)
            {
              GFile *config_dir = _garcon_file_new_for_unknown_input (config_dirs[i], NULL);
              relative_path = g_file_get_relative_path (config_dir, source_file);
              g_object_unref (config_dir);
            }

          /* Look for the same relative path in the XDG_CONFIG_DIRS entries after the parent
           * of the current menu file */
          for (; relative_path != NULL && config_dirs[i] != NULL; ++i)
            {
              GFile *config_dir = _garcon_file_new_for_unknown_input (config_dirs[i], NULL);
              GFile *absolute = g_file_resolve_relative_path (config_dir, relative_path);

              if (G_LIKELY (absolute != NULL))
                {
                  if (G_UNLIKELY (g_file_query_exists (absolute, NULL)))
                    {
                      absolute_path = g_file_get_uri (absolute);

                      /* Destroy the MenuFile type="parent" information */
                      garcon_menu_node_tree_free_data (node);

                      /* Replace it with a MergeFile type="path" element */
                      node->data = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_MERGE_FILE,
                                                            GUINT_TO_POINTER (GARCON_MENU_MERGE_FILE_PATH));
                      garcon_menu_node_tree_set_merge_file_filename (node, absolute_path);
                      break;
                    }
                  g_object_unref (absolute);
                }
              g_object_unref (config_dir);
            }

          /* No file with the same relative filename found in XDG_CONFIG_DIRS */
          if (absolute_path == NULL || i >= g_strv_length ((gchar **)config_dirs))
            {
              /* Remove the MenuFile type="parent" node */
              garcon_menu_node_tree_free (node);
            }

          g_free (absolute_path);
          g_free (relative_path);
          g_free (config_dirs);
        }
    }

  return FALSE;
}



static void
garcon_menu_merger_remove_duplicate_paths (GNode             *node,
                                           GarconMenuNodeType type)
{
  GSList *destroy_nodes = NULL;
  GSList *remaining_nodes = NULL;
  GNode  *child;

  g_return_if_fail (node != NULL);

  if (garcon_menu_node_tree_get_node_type (node) != GARCON_MENU_NODE_TYPE_MENU)
    return;

  for (child = g_node_last_child (node); child != NULL; child = g_node_prev_sibling (child))
    {
      if (garcon_menu_node_tree_get_node_type (child) == GARCON_MENU_NODE_TYPE_MENU)
        {
          garcon_menu_merger_remove_duplicate_paths (child, type);
          continue;
        }

      if (garcon_menu_node_tree_get_node_type (child) != type)
        continue;

      if (G_LIKELY (g_slist_find_custom (remaining_nodes, child,
                                         (GCompareFunc) garcon_menu_node_tree_compare) == NULL))
        {
          remaining_nodes = g_slist_prepend (remaining_nodes, child);
        }
      else
        {
          destroy_nodes = g_slist_prepend (destroy_nodes, child);
        }
    }

  _garcon_g_slist_free_full (destroy_nodes, garcon_menu_node_tree_free);
  g_slist_free (remaining_nodes);
}



static gboolean
collect_name (GNode        *node,
              const gchar **name)
{
  if (garcon_menu_node_tree_get_node_type (node) == GARCON_MENU_NODE_TYPE_NAME)
    {
      *name = garcon_menu_node_tree_get_string (node);
      return TRUE;
    }
  else
    return FALSE;
}



static void
garcon_menu_merger_move_nodes (GNode *source,
                               GNode *target,
                               GNode *position)
{
  GNode *child;

  for (child = g_node_first_child (source); child != NULL; child = g_node_next_sibling (child))
    if (garcon_menu_node_tree_get_node_type (child) != GARCON_MENU_NODE_TYPE_NAME)
      g_node_insert_before (target, position, g_node_copy (child));
}



static void
garcon_menu_merger_consolidate_child_menus (GNode *node)
{
  GHashTable  *table;
  GNode       *child;
  GNode       *next_child;
  GNode       *target;
  const gchar *name;

  if (garcon_menu_node_tree_get_node_type (node) != GARCON_MENU_NODE_TYPE_MENU)
    return;

  table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  /* Determine the last child menu for each child menu name */
  for (child = g_node_last_child (node); child != NULL; child = g_node_prev_sibling (child))
    {
      if (garcon_menu_node_tree_get_node_type (child) != GARCON_MENU_NODE_TYPE_MENU)
        continue;

      name = NULL;
      g_node_traverse (child, G_IN_ORDER, G_TRAVERSE_ALL, 2,
                       (GNodeTraverseFunc) collect_name, &name);

      if (G_UNLIKELY (g_hash_table_lookup (table, name) == NULL))
        g_hash_table_insert (table, g_strdup (name), child);
    }

  for (child = g_node_first_child (node); child != NULL; )
    {
      if (garcon_menu_node_tree_get_node_type (child) == GARCON_MENU_NODE_TYPE_MENU)
        {
          name = NULL;
          g_node_traverse (child, G_IN_ORDER, G_TRAVERSE_ALL, 2,
                           (GNodeTraverseFunc) collect_name, &name);

          target = g_hash_table_lookup (table, name);

          if (G_LIKELY (target != NULL && child != target))
            {
              garcon_menu_merger_move_nodes (child, target, g_node_first_child (target));

              next_child = g_node_next_sibling (child);
              g_node_destroy (child);
              child = g_node_prev_sibling (next_child);
            }
        }

        child = g_node_next_sibling (child);
    }

  for (child = g_node_first_child (node); child != NULL; child = g_node_next_sibling (child))
    garcon_menu_merger_consolidate_child_menus (child);

  g_hash_table_unref (table);
}



static gboolean
garcon_menu_merger_resolve_merge_dirs (GNode                   *node,
                                       GarconMenuMergerContext *context)
{
  GFileEnumerator *enumerator;
  GFileInfo       *file_info;
  GFile           *file;
  GFile           *dir;
  GNode           *file_node;
  gchar           *uri;

  g_return_val_if_fail (context != NULL, FALSE);

  /* Skip elements that are not MergeDirs */
  if (garcon_menu_node_tree_get_node_type (node) != GARCON_MENU_NODE_TYPE_MERGE_DIR)
    return FALSE;

  dir = _garcon_file_new_for_unknown_input (garcon_menu_node_tree_get_string (node), NULL);

  if (dir == NULL)
    return FALSE;

  enumerator = g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE, NULL, NULL);

  if (G_UNLIKELY (enumerator != NULL))
    {
      /* Add merge dir to the list */
      if (context->merge_dirs != NULL 
          && !g_list_find_custom (*context->merge_dirs, dir, (GCompareFunc) compare_files))
        {
          *context->merge_dirs = g_list_prepend (*context->merge_dirs, g_object_ref (dir));
        }

      while (TRUE)
        {
          file_info = g_file_enumerator_next_file (enumerator, NULL, NULL);

          if (G_UNLIKELY (file_info == NULL))
            break;

          if (G_LIKELY (g_str_has_suffix (g_file_info_get_name (file_info), ".menu")))
            {
              file_node = g_node_new (garcon_menu_node_create (GARCON_MENU_NODE_TYPE_MERGE_FILE,
                                                               GUINT_TO_POINTER (GARCON_MENU_MERGE_FILE_PATH)));

              file = g_file_resolve_relative_path (dir, g_file_info_get_name (file_info));
              uri = g_file_get_uri (file);

              garcon_menu_node_tree_set_merge_file_filename (file_node, uri);

              g_free (uri);
              g_object_unref (file);

              g_node_insert_after (node->parent, node, file_node);
            }

          g_object_unref (file_info);
        }

      g_object_unref (enumerator);
    }

  garcon_menu_node_tree_free (node);

  g_object_unref (dir);

  return FALSE;
}



static gboolean
garcon_menu_parser_insert_elements (GNode *node,
                                    GNode *origin)
{
  if (node == origin)
    return FALSE;

  if (garcon_menu_node_tree_get_node_type (node) == GARCON_MENU_NODE_TYPE_NAME)
    {
      garcon_menu_node_tree_free (node);
      return FALSE;
    }

  g_node_insert_before (origin->parent, origin, g_node_copy (node));

  return FALSE;
}



static void
garcon_menu_merger_object_ref (gpointer data,
                               gpointer user_data)
{
  g_object_ref (G_OBJECT (data));
}



static gboolean
garcon_menu_merger_process_merge_files (GNode                   *node,
                                        GarconMenuMergerContext *context)
{
  GarconMenuMerger *merger;
  GarconMenuParser *parser;
  GFile            *file;
  GNode            *tree;

  g_return_val_if_fail (context != NULL, FALSE);

  if (garcon_menu_node_tree_get_node_type (node) != GARCON_MENU_NODE_TYPE_MERGE_FILE ||
      garcon_menu_node_tree_get_merge_file_type (node) != GARCON_MENU_MERGE_FILE_PATH)
    {
      return FALSE;
    }

  file = g_file_new_for_uri (garcon_menu_node_tree_get_merge_file_filename (node));

  if (G_UNLIKELY (g_list_find_custom (context->file_stack, file,
                                      (GCompareFunc) compare_files) != NULL))
    {
      g_object_unref (file);
      return FALSE;
    }

  parser = garcon_menu_parser_new (file);

  if (G_LIKELY (garcon_menu_parser_run (parser, NULL, NULL)))
    {
      merger = garcon_menu_merger_new (GARCON_MENU_TREE_PROVIDER (parser));
      g_object_unref (parser);

      merger->priv->file_stack = g_list_copy (context->file_stack);
      g_list_foreach (merger->priv->file_stack, garcon_menu_merger_object_ref, NULL);

      if (G_LIKELY (garcon_menu_merger_run (merger, 
                                            context->merge_files, 
                                            context->merge_dirs, 
                                            context->cancellable, NULL)))
        {
          tree = garcon_menu_tree_provider_get_tree (GARCON_MENU_TREE_PROVIDER (merger));
          g_object_unref (merger);

          g_node_insert_after (node->parent, node, tree);
          g_node_traverse (tree, G_IN_ORDER, G_TRAVERSE_ALL, 2,
                           (GNodeTraverseFunc) garcon_menu_parser_insert_elements, tree);
          g_node_destroy (tree);

          /* Add merged file to the list */
          if (context->merge_files != NULL
              && !g_list_find_custom (*context->merge_files, file, (GCompareFunc) compare_files))
            {
              *context->merge_files = g_list_prepend (*context->merge_files, g_object_ref (file));
            }
        }
    }

  garcon_menu_node_tree_free (node);

  g_object_unref (file);

  return FALSE;
}



static void
garcon_menu_merger_clean_up_elements (GNode             *node,
                                      GarconMenuNodeType type)
{
  GarconMenuNode *node_;
  GNode          *child;
  GNode          *remaining_node = NULL;
  GList          *destroy_list = NULL;

  for (child = g_node_last_child (node); child != NULL; child = g_node_prev_sibling (child))
    {
      if (garcon_menu_node_tree_get_node_type (child) == GARCON_MENU_NODE_TYPE_MENU)
        {
          garcon_menu_merger_clean_up_elements (child, type);
          continue;
        }

      if (type == GARCON_MENU_NODE_TYPE_DELETED
          && garcon_menu_node_tree_get_node_type (node) != GARCON_MENU_NODE_TYPE_DELETED
          && garcon_menu_node_tree_get_node_type (node) != GARCON_MENU_NODE_TYPE_NOT_DELETED)
        {
          continue;
        }

      if (type == GARCON_MENU_NODE_TYPE_ONLY_UNALLOCATED
          && garcon_menu_node_tree_get_node_type (node) != GARCON_MENU_NODE_TYPE_ONLY_UNALLOCATED
          && garcon_menu_node_tree_get_node_type (node) != GARCON_MENU_NODE_TYPE_NOT_ONLY_UNALLOCATED)
        {
          continue;
        }

      if (type == GARCON_MENU_NODE_TYPE_LAYOUT
          && garcon_menu_node_tree_get_node_type (child) != type)
        {
          continue;
        }

      if (type == GARCON_MENU_NODE_TYPE_DEFAULT_LAYOUT
          && garcon_menu_node_tree_get_node_type (child) != type)
        {
          continue;
        }

      if (remaining_node != NULL)
        destroy_list = g_list_prepend (destroy_list, child);
      else
        remaining_node = child;
    }

  _garcon_g_list_free_full (destroy_list, garcon_menu_node_tree_free);

  if (type == GARCON_MENU_NODE_TYPE_LAYOUT 
      && remaining_node != NULL
      && G_NODE_IS_LEAF (remaining_node))
    {
      garcon_menu_node_tree_free (remaining_node);
    }

  if (type == GARCON_MENU_NODE_TYPE_DEFAULT_LAYOUT
      && remaining_node != NULL
      && G_NODE_IS_LEAF (remaining_node))
    {
      /* FIXME Fix empty <DefaultLayout> elements created due to a bug in
       * alacarte. See http://bugzilla.xfce.org/show_bug.cgi?id=6882#c2
       * for more information */
      node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_MERGE, 
                                       GUINT_TO_POINTER (GARCON_MENU_LAYOUT_MERGE_MENUS));
      g_node_append_data (remaining_node, node_);
      node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_MERGE, 
                                       GUINT_TO_POINTER (GARCON_MENU_LAYOUT_MERGE_FILES));
      g_node_append_data (remaining_node, node_);
    }
}



static gboolean
collect_moves (GNode  *node,
               GList **list)
{
  if (garcon_menu_node_tree_get_node_type (node) == GARCON_MENU_NODE_TYPE_MOVE)
    *list = g_list_append (*list, node);

  return FALSE;
}



static gboolean
remove_moves (GNode   *node,
              gpointer data)
{
  if (garcon_menu_node_tree_get_node_type (node) == GARCON_MENU_NODE_TYPE_MOVE)
    garcon_menu_node_tree_free (node);

  return FALSE;
}



static gboolean
collect_old_new (GNode  *node,
                 GList **list)
{
  if (garcon_menu_node_tree_get_node_type (node) == GARCON_MENU_NODE_TYPE_OLD)
    *list = g_list_append (*list, g_strdup (garcon_menu_node_tree_get_string (node)));
  else if (garcon_menu_node_tree_get_node_type (node) == GARCON_MENU_NODE_TYPE_NEW)
    *list = g_list_append (*list, g_strdup (garcon_menu_node_tree_get_string (node)));

  return FALSE;
}



static GNode *
garcon_menu_merger_find_menu_with_name (GNode       *node,
                                        const gchar *name)
{
  GNode       *result = NULL;
  GNode       *child;
  const gchar *child_name;

  for (child = g_node_first_child (node); child != NULL; child = g_node_next_sibling (child))
    if (garcon_menu_node_tree_get_node_type (child) == GARCON_MENU_NODE_TYPE_MENU)
      {
        child_name = NULL;
        g_node_traverse (child, G_IN_ORDER, G_TRAVERSE_ALL, 2,
                         (GNodeTraverseFunc) collect_name, &child_name);

        if (g_str_equal (child_name, name))
          {
            result = child;
            break;
          }
      }

  return result;
}



static GNode *
garcon_menu_merger_find_menu (GNode  *node,
                              gchar **path,
                              gint    position,
                              gint    depth,
                              GNode **parent)
{
  GNode *result = NULL;
  GNode *child;

  g_return_val_if_fail (position <= depth, NULL);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  /* Make sure to set parent to NULL unless the path is valid */
  if (parent != NULL)
    *parent = NULL;

  /* Search for the direct child which has the name equal to the current
   * path componenent */
  child = garcon_menu_merger_find_menu_with_name (node, path[position]);

  if (G_UNLIKELY (child == NULL))
    return NULL;

  if (G_LIKELY (position == depth))
    {
      result = child;

      if (parent != NULL)
        *parent = node;
    }
  else
    result = garcon_menu_merger_find_menu (child, path, position+1, depth, parent);

  return result;
}



static GNode *
garcon_menu_merger_create_menu (GNode  *node,
                                gchar **path,
                                gint    position,
                                gint    depth)
{
  GNode *result = NULL;
  GNode *child;

  g_return_val_if_fail (position <= depth, NULL);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  /* Search for the direct child which has the name equal to the current
   * path componenent */
  child = garcon_menu_merger_find_menu_with_name (node, path[position]);

  /* Create it if it doesn't exist */
  if (G_LIKELY (child == NULL))
    {
      child = g_node_append_data (node, NULL);
      g_node_append_data (child, garcon_menu_node_create (GARCON_MENU_NODE_TYPE_NAME,
                                                          path[position]));
    }

  if (G_LIKELY (position == depth))
    result = child;
  else
    result = garcon_menu_merger_create_menu (child, path, position+1, depth);

  return result;
}



static void
garcon_menu_merger_resolve_moves (GNode *node)
{
  GNode  *child;
  GNode  *old_node;
  GNode  *new_node;
  GList  *moves = NULL;
  GList  *pairs = NULL;
  GList  *iter;
  gchar **old_path;
  gchar **new_path;

  if (garcon_menu_node_tree_get_node_type (node) != GARCON_MENU_NODE_TYPE_MENU)
    return;

  for (child = g_node_first_child (node); child != NULL; child = g_node_next_sibling (child))
    garcon_menu_merger_resolve_moves (child);

  g_node_traverse (node, G_IN_ORDER, G_TRAVERSE_ALL, 2,
                   (GNodeTraverseFunc) collect_moves, &moves);

  for (iter = moves; iter != NULL; iter = g_list_next (iter))
    {
      g_node_traverse (iter->data, G_IN_ORDER, G_TRAVERSE_ALL, 2,
                       (GNodeTraverseFunc) collect_old_new, &pairs);
    }

  g_node_traverse (node, G_IN_ORDER, G_TRAVERSE_ALL, 2,
                   (GNodeTraverseFunc) remove_moves, &moves);

  g_list_free (moves);

  for (iter = pairs; iter != NULL; iter = g_list_next (iter))
    {
      if (g_list_length (iter) < 2)
        break;

      old_path = g_strsplit (iter->data, "/", -1);
      iter = g_list_next (iter);
      new_path = g_strsplit (iter->data, "/", -1);

      old_node = garcon_menu_merger_find_menu (node, old_path, 0,
                                               g_strv_length (old_path)-1, NULL);
      new_node = garcon_menu_merger_find_menu (node, new_path, 0,
                                               g_strv_length (new_path)-1, NULL);

      if (G_LIKELY (old_node != NULL && old_node != new_node))
        {
          if (G_LIKELY (new_node == NULL))
            {
              new_node = garcon_menu_merger_create_menu (node, new_path, 0,
                                                         g_strv_length (new_path)-1);

              garcon_menu_merger_move_nodes (old_node, new_node, NULL);
            }
          else
            garcon_menu_merger_move_nodes (old_node, new_node,
                                           g_node_first_child (new_node));

          g_node_destroy (old_node);
        }

      g_strfreev (old_path);
      g_strfreev (new_path);
    }

  _garcon_g_list_free_full (pairs, g_free);
}



static void
garcon_menu_merger_prepend_default_layout (GNode *node)
{
  GarconMenuNode *node_;
  GNode        *layout;

  if (garcon_menu_node_tree_get_node_type (node) == GARCON_MENU_NODE_TYPE_MENU)
    {
      node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_DEFAULT_LAYOUT, NULL);
      layout = g_node_prepend_data (node, node_);

      node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_MERGE,
                                       GUINT_TO_POINTER (GARCON_MENU_LAYOUT_MERGE_MENUS));
      g_node_append_data (layout, node_);

      node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_MERGE,
                                       GUINT_TO_POINTER (GARCON_MENU_LAYOUT_MERGE_FILES));
      g_node_append_data (layout, node_);
    }
}

