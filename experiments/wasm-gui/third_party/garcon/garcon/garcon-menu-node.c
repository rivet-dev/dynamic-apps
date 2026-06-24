/* vi:set sw=2 sts=2 ts=2 et ai: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>.
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

#include <garcon/garcon-menu-item.h>
#include <garcon/garcon-menu-node.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_NODE_TYPE,
};



static void garcon_menu_node_finalize     (GObject             *object);
static void garcon_menu_node_get_property (GObject             *object,
                                           guint                prop_id,
                                           GValue              *value,
                                           GParamSpec          *pspec);
static void garcon_menu_node_set_property (GObject             *object,
                                           guint                prop_id,
                                           const GValue        *value,
                                           GParamSpec          *pspec);
static void garcon_menu_node_free_data    (GarconMenuNode      *node);



struct _GarconMenuNodeClass
{
  GObjectClass __parent__;
};

union _GarconMenuNodeData
{
  GarconMenuLayoutMergeType layout_merge_type;
  struct
  {
    GarconMenuMergeFileType type;
    gchar                  *filename;
  } merge_file;
  gchar                    *string;
};

struct _GarconMenuNode
{
  GObject            __parent__;

  GarconMenuNodeType node_type;
  GarconMenuNodeData data;
};



GType
garcon_menu_node_type_get_type (void)
{
  static GType      type = G_TYPE_INVALID;
  static GEnumValue values[] =
  {
    { GARCON_MENU_NODE_TYPE_INVALID, "GARCON_MENU_NODE_TYPE_INVALID", "Invalid" },
    { GARCON_MENU_NODE_TYPE_MENU, "GARCON_MENU_NODE_TYPE_MENU", "Menu" },
    { GARCON_MENU_NODE_TYPE_NAME, "GARCON_MENU_NODE_TYPE_NAME", "Name" },
    { GARCON_MENU_NODE_TYPE_DIRECTORY, "GARCON_MENU_NODE_TYPE_DIRECTORY", "Directory" },
    { GARCON_MENU_NODE_TYPE_DIRECTORY_DIR, "GARCON_MENU_NODE_TYPE_DIRECTORY_DIR", "DirectoryDir" },
    { GARCON_MENU_NODE_TYPE_DEFAULT_DIRECTORY_DIRS, "GARCON_MENU_NODE_TYPE_DEFAULT_DIRECTORY_DIRS", "DefaultDirectoryDirs" },
    { GARCON_MENU_NODE_TYPE_APP_DIR, "GARCON_MENU_NODE_TYPE_APP_DIR", "AppDir" },
    { GARCON_MENU_NODE_TYPE_DEFAULT_APP_DIRS, "GARCON_MENU_NODE_TYPE_DEFAULT_APP_DIRS", "DefaultAppDirs" },
    { GARCON_MENU_NODE_TYPE_ONLY_UNALLOCATED, "GARCON_MENU_NODE_TYPE_ONLY_UNALLOCATED", "OnlyUnallocated" },
    { GARCON_MENU_NODE_TYPE_NOT_ONLY_UNALLOCATED, "GARCON_MENU_NODE_TYPE_NOT_ONLY_UNALLOCATED", "NotOnlyUnallocated" },
    { GARCON_MENU_NODE_TYPE_DELETED, "GARCON_MENU_NODE_TYPE_DELETED", "Deleted" },
    { GARCON_MENU_NODE_TYPE_NOT_DELETED, "GARCON_MENU_NODE_TYPE_NOT_DELETED", "NotDeleted" },
    { GARCON_MENU_NODE_TYPE_INCLUDE, "GARCON_MENU_NODE_TYPE_INCLUDE", "Include" },
    { GARCON_MENU_NODE_TYPE_EXCLUDE, "GARCON_MENU_NODE_TYPE_EXCLUDE", "Exclude" },
    { GARCON_MENU_NODE_TYPE_ALL, "GARCON_MENU_NODE_TYPE_ALL", "All" },
    { GARCON_MENU_NODE_TYPE_FILENAME, "GARCON_MENU_NODE_TYPE_FILENAME", "Filename" },
    { GARCON_MENU_NODE_TYPE_CATEGORY, "GARCON_MENU_NODE_TYPE_CATEGORY", "Category" },
    { GARCON_MENU_NODE_TYPE_OR, "GARCON_MENU_NODE_TYPE_OR", "Or" },
    { GARCON_MENU_NODE_TYPE_AND, "GARCON_MENU_NODE_TYPE_AND", "And" },
    { GARCON_MENU_NODE_TYPE_NOT, "GARCON_MENU_NODE_TYPE_NOT", "Not" },
    { GARCON_MENU_NODE_TYPE_MOVE, "GARCON_MENU_NODE_TYPE_MOVE", "Move" },
    { GARCON_MENU_NODE_TYPE_OLD, "GARCON_MENU_NODE_TYPE_OLD", "Old" },
    { GARCON_MENU_NODE_TYPE_NEW, "GARCON_MENU_NODE_TYPE_NEW", "New" },
    { GARCON_MENU_NODE_TYPE_DEFAULT_LAYOUT, "GARCON_MENU_NODE_TYPE_DEFAULT_LAYOUT", "DefaultLayout" },
    { GARCON_MENU_NODE_TYPE_LAYOUT, "GARCON_MENU_NODE_TYPE_LAYOUT", "Layout" },
    { GARCON_MENU_NODE_TYPE_MENUNAME, "GARCON_MENU_NODE_TYPE_MENUNAME", "Menuname" },
    { GARCON_MENU_NODE_TYPE_SEPARATOR, "GARCON_MENU_NODE_TYPE_SEPARATOR", "Separator" },
    { GARCON_MENU_NODE_TYPE_MERGE, "GARCON_MENU_NODE_TYPE_MERGE", "Merge" },
    { GARCON_MENU_NODE_TYPE_MERGE_FILE, "GARCON_MENU_NODE_TYPE_MERGE_FILE", "MergeFile" },
    { GARCON_MENU_NODE_TYPE_MERGE_DIR, "GARCON_MENU_NODE_TYPE_MERGE_DIR", "MergeDir" },
    { GARCON_MENU_NODE_TYPE_DEFAULT_MERGE_DIRS, "GARCON_MENU_NODE_TYPE_DEFAULT_MERGE_DIRS", "MergeDirs" },
    { 0, NULL, NULL },
  };

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    type = g_enum_register_static ("GarconMenuNodeType", values);

  return type;
}



G_DEFINE_TYPE (GarconMenuNode, garcon_menu_node, G_TYPE_OBJECT)



static void
garcon_menu_node_class_init (GarconMenuNodeClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = garcon_menu_node_finalize;
  gobject_class->get_property = garcon_menu_node_get_property;
  gobject_class->set_property = garcon_menu_node_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_NODE_TYPE,
                                   g_param_spec_enum ("node-type",
                                                      "node-type",
                                                      "node-type",
                                                      garcon_menu_node_type_get_type (),
                                                      GARCON_MENU_NODE_TYPE_MENU,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));
}



static void
garcon_menu_node_init (GarconMenuNode *node)
{
}



static void
garcon_menu_node_finalize (GObject *object)
{
  GarconMenuNode *node = GARCON_MENU_NODE (object);

  garcon_menu_node_free_data (node);

  (*G_OBJECT_CLASS (garcon_menu_node_parent_class)->finalize) (object);
}



static void
garcon_menu_node_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GarconMenuNode *node = GARCON_MENU_NODE (object);

  switch (prop_id)
    {
    case PROP_NODE_TYPE:
      g_value_set_enum (value, node->node_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
garcon_menu_node_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GarconMenuNode *node = GARCON_MENU_NODE (object);

  switch (prop_id)
    {
    case PROP_NODE_TYPE:
      node->node_type = g_value_get_enum (value);
      g_object_notify (G_OBJECT (node), "node-type");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



GarconMenuNode *
garcon_menu_node_new (GarconMenuNodeType node_type)
{
  return g_object_new (GARCON_TYPE_MENU_NODE, "node-type", node_type, NULL);
}



GarconMenuNodeType garcon_menu_node_get_node_type (GarconMenuNode *node)
{
  g_return_val_if_fail (GARCON_IS_MENU_NODE (node), 0);
  return node->node_type;
}


/**
 * garcon_menu_node_create:
 * @node_type: a #GarconMenuNodeType
 * @first_value:
 * @...:
 *
 * Returns: (transfer full): a #GarconMenuNode
 */
GarconMenuNode *
garcon_menu_node_create (GarconMenuNodeType node_type,
                         gpointer           first_value,
                         ...)
{
  GarconMenuNode *node;

  node = garcon_menu_node_new (node_type);

  switch (node_type)
    {
    case GARCON_MENU_NODE_TYPE_NAME:
    case GARCON_MENU_NODE_TYPE_DIRECTORY:
    case GARCON_MENU_NODE_TYPE_DIRECTORY_DIR:
    case GARCON_MENU_NODE_TYPE_APP_DIR:
    case GARCON_MENU_NODE_TYPE_FILENAME:
    case GARCON_MENU_NODE_TYPE_CATEGORY:
    case GARCON_MENU_NODE_TYPE_OLD:
    case GARCON_MENU_NODE_TYPE_NEW:
    case GARCON_MENU_NODE_TYPE_MENUNAME:
    case GARCON_MENU_NODE_TYPE_MERGE_DIR:
      node->data.string = g_strdup (first_value);
      break;

    case GARCON_MENU_NODE_TYPE_MERGE:
      node->data.layout_merge_type = GPOINTER_TO_UINT (first_value);
      break;

    case GARCON_MENU_NODE_TYPE_MERGE_FILE:
      node->data.merge_file.type = GPOINTER_TO_UINT (first_value);
      node->data.merge_file.filename = NULL;
      break;

    default:
      break;
    }

  return node;
}


/**
 * garcon_menu_node_copy:
 * @node: a #GarconMenuNode
 * @data:
 *
 * Returns: (transfer full): a #GarconMenuNode
 */
GarconMenuNode *
garcon_menu_node_copy (GarconMenuNode *node,
                       gpointer        data)
{
  GarconMenuNode *copy;

  if (node == NULL || !GARCON_IS_MENU_NODE (node))
    return NULL;

  copy = garcon_menu_node_new (node->node_type);

  switch (copy->node_type)
    {
    case GARCON_MENU_NODE_TYPE_NAME:
    case GARCON_MENU_NODE_TYPE_DIRECTORY:
    case GARCON_MENU_NODE_TYPE_DIRECTORY_DIR:
    case GARCON_MENU_NODE_TYPE_APP_DIR:
    case GARCON_MENU_NODE_TYPE_FILENAME:
    case GARCON_MENU_NODE_TYPE_CATEGORY:
    case GARCON_MENU_NODE_TYPE_OLD:
    case GARCON_MENU_NODE_TYPE_NEW:
    case GARCON_MENU_NODE_TYPE_MENUNAME:
    case GARCON_MENU_NODE_TYPE_MERGE_DIR:
      copy->data.string = g_strdup (node->data.string);
      break;

    case GARCON_MENU_NODE_TYPE_MERGE:
      copy->data.layout_merge_type = node->data.layout_merge_type;
      break;

    case GARCON_MENU_NODE_TYPE_MERGE_FILE:
      copy->data.merge_file.type = node->data.merge_file.type;
      copy->data.merge_file.filename = g_strdup (node->data.merge_file.filename);
      break;

    default:
      break;
    }

  return copy;
}



static void
garcon_menu_node_free_data (GarconMenuNode *node)
{
  g_return_if_fail (GARCON_IS_MENU_NODE (node));

  switch (node->node_type)
    {
    case GARCON_MENU_NODE_TYPE_NAME:
    case GARCON_MENU_NODE_TYPE_DIRECTORY:
    case GARCON_MENU_NODE_TYPE_DIRECTORY_DIR:
    case GARCON_MENU_NODE_TYPE_APP_DIR:
    case GARCON_MENU_NODE_TYPE_FILENAME:
    case GARCON_MENU_NODE_TYPE_CATEGORY:
    case GARCON_MENU_NODE_TYPE_OLD:
    case GARCON_MENU_NODE_TYPE_NEW:
    case GARCON_MENU_NODE_TYPE_MENUNAME:
    case GARCON_MENU_NODE_TYPE_MERGE_DIR:
      g_free (node->data.string);
      break;

    case GARCON_MENU_NODE_TYPE_MERGE_FILE:
      g_free (node->data.merge_file.filename);
      break;

    default:
      break;
    }
}



const gchar *
garcon_menu_node_get_string (GarconMenuNode *node)
{
  g_return_val_if_fail (GARCON_IS_MENU_NODE (node), NULL);
  return node->data.string;
}



void
garcon_menu_node_set_string (GarconMenuNode *node,
                             const gchar    *value)
{
  g_return_if_fail (GARCON_IS_MENU_NODE (node));
  g_return_if_fail (value != NULL);

  g_free (node->data.string);
  node->data.string = g_strdup (value);
}



GarconMenuMergeFileType
garcon_menu_node_get_merge_file_type (GarconMenuNode *node)
{
  g_return_val_if_fail (GARCON_IS_MENU_NODE (node), 0);
  g_return_val_if_fail (node->node_type == GARCON_MENU_NODE_TYPE_MERGE_FILE, 0);
  return node->data.merge_file.type;
}



void
garcon_menu_node_set_merge_file_type (GarconMenuNode         *node,
                                      GarconMenuMergeFileType type)
{
  g_return_if_fail (GARCON_IS_MENU_NODE (node));
  g_return_if_fail (node->node_type == GARCON_MENU_NODE_TYPE_MERGE_FILE);
  node->data.merge_file.type = type;
}



const gchar *
garcon_menu_node_get_merge_file_filename (GarconMenuNode *node)
{
  g_return_val_if_fail (GARCON_IS_MENU_NODE (node), NULL);
  g_return_val_if_fail (node->node_type == GARCON_MENU_NODE_TYPE_MERGE_FILE, NULL);
  return node->data.merge_file.filename;
}



void
garcon_menu_node_set_merge_file_filename (GarconMenuNode *node,
                                          const gchar    *filename)
{
  g_return_if_fail (GARCON_IS_MENU_NODE (node));
  g_return_if_fail (filename != NULL);
  g_return_if_fail (node->node_type == GARCON_MENU_NODE_TYPE_MERGE_FILE);

  g_free (node->data.merge_file.filename);
  node->data.merge_file.filename = g_strdup (filename);
}



typedef struct
{
  GarconMenuNodeType type;
  GNode             *self;
  gpointer           value;
} Pair;



static gboolean
collect_children (GNode *node,
                  Pair  *pair)
{
  if (node == pair->self)
    return FALSE;

  if (garcon_menu_node_tree_get_node_type (node) == pair->type)
    pair->value = g_list_prepend (pair->value, node);

  return FALSE;
}


/**
 * garcon_menu_node_tree_get_child_node: (skip)
 * @tree: #GNode instance
 * @type: type for the menu nodes
 * @reverse:
 *
 * Returns: a #GNode if @type is valid menu nodes type.
 */
GNode *
garcon_menu_node_tree_get_child_node (GNode             *tree,
                                      GarconMenuNodeType type,
                                      gboolean           reverse)
{
  GNode *child = NULL;

  if (reverse)
    {
      for (child = g_node_last_child (tree); 
           child != NULL;
           child = g_node_prev_sibling (child))
        {
          if (garcon_menu_node_tree_get_node_type (child) == type)
            return child;
        }
    }
  else
    {
      for (child = g_node_first_child (tree);
           child != NULL;
           child = g_node_next_sibling (child))
        {
          if (garcon_menu_node_tree_get_node_type (child) == type)
            return child;
        }
    }

  return child;
}


/**
 * garcon_menu_node_tree_get_child_nodes:
 * @tree: a GNode
 * @type: type for the menu nodes
 * @reverse:
 *
 * Returns: (element-type GNode) (transfer full): list of #GNode
 */
GList *
garcon_menu_node_tree_get_child_nodes (GNode             *tree,
                                       GarconMenuNodeType type,
                                       gboolean           reverse)
{
  Pair pair;

  pair.type = type;
  pair.value = NULL;
  pair.self = tree;

  g_node_traverse (tree, G_IN_ORDER, G_TRAVERSE_ALL, 2,
                   (GNodeTraverseFunc) collect_children, &pair);

  /* Return the list as if we appended */
  if (!reverse && pair.value != NULL)
    pair.value = g_list_reverse (pair.value);

  return (GList *) pair.value;
}



static gboolean
collect_strings (GNode *node,
                 Pair  *pair)
{
  gpointer string;

  if (node == pair->self)
    return FALSE;

  if (garcon_menu_node_tree_get_node_type (node) == pair->type)
    {
      string = (gpointer) garcon_menu_node_tree_get_string (node);
      pair->value = g_list_prepend (pair->value, string);
    }

  return FALSE;
}


/**
 * garcon_menu_node_tree_get_string_children:
 * @tree: a #GNode instance
 * @type: type for the menu nodes
 * @reverse:
 *
 * Returns: (element-type GNode) (transfer container): list of #GNode
 */
GList *
garcon_menu_node_tree_get_string_children (GNode             *tree,
                                           GarconMenuNodeType type,
                                           gboolean           reverse)
{
  Pair pair;

  pair.type = type;
  pair.value = NULL;
  pair.self = tree;

  g_node_traverse (tree, G_IN_ORDER, G_TRAVERSE_ALL, 2,
                   (GNodeTraverseFunc) collect_strings, &pair);

  /* Return the list as if we appended */
  if (!reverse && pair.value != NULL)
    pair.value = g_list_reverse (pair.value);

  return (GList *) pair.value;
}



static gboolean
collect_boolean (GNode *node,
                 Pair  *pair)
{
  if (node == pair->self)
    return FALSE;

  if (garcon_menu_node_tree_get_node_type (node) == pair->type)
    {
      pair->value = GUINT_TO_POINTER (1);
      return TRUE;
    }

  return FALSE;
}



gboolean
garcon_menu_node_tree_get_boolean_child (GNode             *tree,
                                         GarconMenuNodeType type)
{
  Pair pair;

  pair.value = GUINT_TO_POINTER (0);
  pair.self = tree;
  pair.type = type;

  g_node_traverse (tree, G_IN_ORDER, G_TRAVERSE_ALL, 2,
                   (GNodeTraverseFunc) collect_boolean, &pair);

  return !!GPOINTER_TO_UINT (pair.value);
}



static gboolean
collect_string (GNode *node,
                Pair  *pair)
{
  const gchar **string = pair->value;

  if (node == pair->self)
    return FALSE;

  if (garcon_menu_node_tree_get_node_type (node) == pair->type)
    {
      *string = garcon_menu_node_tree_get_string (node);
      return TRUE;
    }

  return FALSE;
}



const gchar *
garcon_menu_node_tree_get_string_child (GNode             *tree,
                                        GarconMenuNodeType type)
{
  Pair         pair;
  const gchar *string = NULL;

  pair.type = type;
  pair.value = &string;
  pair.self = tree;

  g_node_traverse (tree, G_IN_ORDER, G_TRAVERSE_ALL, 2,
                   (GNodeTraverseFunc) collect_string, &pair);

  return string;
}



gboolean
garcon_menu_node_tree_rule_matches (GNode          *node,
                                    GarconMenuItem *item)
{
  GNode   *child;
  gboolean matches = FALSE;
  gboolean child_matches = FALSE;

  switch (garcon_menu_node_tree_get_node_type (node))
    {
    case GARCON_MENU_NODE_TYPE_CATEGORY:
      matches = garcon_menu_item_has_category (item, garcon_menu_node_tree_get_string (node));
      break;

    case GARCON_MENU_NODE_TYPE_INCLUDE:
    case GARCON_MENU_NODE_TYPE_EXCLUDE:
    case GARCON_MENU_NODE_TYPE_OR:
      for (child = g_node_first_child (node); child != NULL; child = g_node_next_sibling (child))
        matches = matches || garcon_menu_node_tree_rule_matches (child, item);
      break;

    case GARCON_MENU_NODE_TYPE_FILENAME:
      matches = g_str_equal (garcon_menu_node_tree_get_string (node),
                             garcon_menu_item_get_desktop_id (item));
      break;

    case GARCON_MENU_NODE_TYPE_AND:
      matches = TRUE;
      for (child = g_node_first_child (node); child != NULL; child = g_node_next_sibling (child))
        matches = matches && garcon_menu_node_tree_rule_matches (child, item);
      break;

    case GARCON_MENU_NODE_TYPE_NOT:
      for (child = g_node_first_child (node); child != NULL; child = g_node_next_sibling (child))
        child_matches = child_matches || garcon_menu_node_tree_rule_matches (child, item);
      matches = !child_matches;
      break;

    case GARCON_MENU_NODE_TYPE_ALL:
      matches = TRUE;
      break;

    default:
      break;
    }

  return matches;
}






GarconMenuNodeType
garcon_menu_node_tree_get_node_type (GNode *tree)
{
  if (tree == NULL)
    return GARCON_MENU_NODE_TYPE_INVALID;

  if (tree->data == NULL)
    return GARCON_MENU_NODE_TYPE_MENU;

  return garcon_menu_node_get_node_type (tree->data);
}



const gchar *
garcon_menu_node_tree_get_string (GNode *tree)
{
  if (tree == NULL || tree->data == NULL)
    return NULL;
  else
    return garcon_menu_node_get_string (tree->data);
}



void
garcon_menu_node_tree_set_string (GNode       *tree,
                                  const gchar *value)
{
  GarconMenuNodeType type;

  type = garcon_menu_node_tree_get_node_type (tree);

  g_return_if_fail (type == GARCON_MENU_NODE_TYPE_NAME ||
                    type == GARCON_MENU_NODE_TYPE_DIRECTORY ||
                    type == GARCON_MENU_NODE_TYPE_DIRECTORY_DIR ||
                    type == GARCON_MENU_NODE_TYPE_APP_DIR ||
                    type == GARCON_MENU_NODE_TYPE_FILENAME ||
                    type == GARCON_MENU_NODE_TYPE_CATEGORY ||
                    type == GARCON_MENU_NODE_TYPE_OLD ||
                    type == GARCON_MENU_NODE_TYPE_NEW ||
                    type == GARCON_MENU_NODE_TYPE_MENUNAME ||
                    type == GARCON_MENU_NODE_TYPE_MERGE_DIR);

  garcon_menu_node_set_string (tree->data, value);
}


GarconMenuLayoutMergeType
garcon_menu_node_tree_get_layout_merge_type (GNode *tree)
{
  g_return_val_if_fail (garcon_menu_node_tree_get_node_type (tree) == GARCON_MENU_NODE_TYPE_MERGE, 0);
  return ((GarconMenuNode *)tree->data)->data.layout_merge_type;
}



GarconMenuMergeFileType
garcon_menu_node_tree_get_merge_file_type (GNode *tree)
{
  g_return_val_if_fail (garcon_menu_node_tree_get_node_type (tree) == GARCON_MENU_NODE_TYPE_MERGE_FILE, 0);
  return garcon_menu_node_get_merge_file_type (tree->data);
}



const gchar *
garcon_menu_node_tree_get_merge_file_filename (GNode *tree)
{
  g_return_val_if_fail (garcon_menu_node_tree_get_node_type (tree) == GARCON_MENU_NODE_TYPE_MERGE_FILE, NULL);
  return garcon_menu_node_get_merge_file_filename (tree->data);
}



void
garcon_menu_node_tree_set_merge_file_filename (GNode       *tree,
                                                  const gchar *filename)
{
  g_return_if_fail (garcon_menu_node_tree_get_node_type (tree) == GARCON_MENU_NODE_TYPE_MERGE_FILE);
  garcon_menu_node_set_merge_file_filename (tree->data, filename);
}



gint
garcon_menu_node_tree_compare (GNode *tree,
                               GNode *other_tree)
{
  GarconMenuNode *node;
  GarconMenuNode *other_node;

  if (tree == NULL || other_tree == NULL)
    return 0;

  node = tree->data;
  other_node = other_tree->data;

  if (node->node_type != other_node->node_type)
    return 0;

  switch (node->node_type)
    {
    case GARCON_MENU_NODE_TYPE_NAME:
    case GARCON_MENU_NODE_TYPE_DIRECTORY:
    case GARCON_MENU_NODE_TYPE_DIRECTORY_DIR:
    case GARCON_MENU_NODE_TYPE_APP_DIR:
    case GARCON_MENU_NODE_TYPE_FILENAME:
    case GARCON_MENU_NODE_TYPE_CATEGORY:
    case GARCON_MENU_NODE_TYPE_OLD:
    case GARCON_MENU_NODE_TYPE_NEW:
    case GARCON_MENU_NODE_TYPE_MENUNAME:
    case GARCON_MENU_NODE_TYPE_MERGE_DIR:
      return g_strcmp0 (node->data.string, other_node->data.string);
      break;

    case GARCON_MENU_NODE_TYPE_MERGE_FILE:
      return g_strcmp0 (node->data.merge_file.filename,
                        other_node->data.merge_file.filename);
      break;

    default:
      return 0;
      break;
    }

  return 0;
}


/**
 * garcon_menu_node_tree_copy: (skip)
 * @tree: a #GNode
 *
 * Recursively copies a #GNode.
 */
GNode *
garcon_menu_node_tree_copy (GNode *tree)
{
  return g_node_copy_deep (tree, (GCopyFunc) garcon_menu_node_copy, NULL);
}



static gboolean
free_children (GNode   *tree,
               gpointer data)
{
  garcon_menu_node_tree_free_data (tree);
  return FALSE;
}



void
garcon_menu_node_tree_free (GNode *tree)
{
  if (tree != NULL)
    {
      g_node_traverse (tree, G_IN_ORDER, G_TRAVERSE_ALL, -1,
                       (GNodeTraverseFunc) free_children, NULL);

      g_node_destroy (tree);
    }
}



void
garcon_menu_node_tree_free_data (GNode *tree)
{
  if (tree != NULL && tree->data != NULL)
    g_object_unref (tree->data);
}

