/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#if !defined (_GARCON_INSIDE_GARCON_H) && !defined (GARCON_COMPILATION)
#error "Only <garcon/garcon.h> can be included directly. This file may disappear or change contents."
#endif

#ifndef __GARCON_MENU_NODE_H__
#define __GARCON_MENU_NODE_H__

#include <garcon/garcon.h>

G_BEGIN_DECLS

/* Types for the menu nodes */
typedef enum
{
  GARCON_MENU_NODE_TYPE_INVALID,
  GARCON_MENU_NODE_TYPE_MENU,
  GARCON_MENU_NODE_TYPE_NAME,
  GARCON_MENU_NODE_TYPE_DIRECTORY,
  GARCON_MENU_NODE_TYPE_DIRECTORY_DIR,
  GARCON_MENU_NODE_TYPE_DEFAULT_DIRECTORY_DIRS,
  GARCON_MENU_NODE_TYPE_APP_DIR,
  GARCON_MENU_NODE_TYPE_DEFAULT_APP_DIRS,
  GARCON_MENU_NODE_TYPE_ONLY_UNALLOCATED,
  GARCON_MENU_NODE_TYPE_NOT_ONLY_UNALLOCATED,
  GARCON_MENU_NODE_TYPE_DELETED,
  GARCON_MENU_NODE_TYPE_NOT_DELETED,
  GARCON_MENU_NODE_TYPE_INCLUDE,
  GARCON_MENU_NODE_TYPE_EXCLUDE,
  GARCON_MENU_NODE_TYPE_ALL,
  GARCON_MENU_NODE_TYPE_FILENAME,
  GARCON_MENU_NODE_TYPE_CATEGORY,
  GARCON_MENU_NODE_TYPE_OR,
  GARCON_MENU_NODE_TYPE_AND,
  GARCON_MENU_NODE_TYPE_NOT,
  GARCON_MENU_NODE_TYPE_MOVE,
  GARCON_MENU_NODE_TYPE_OLD,
  GARCON_MENU_NODE_TYPE_NEW,
  GARCON_MENU_NODE_TYPE_DEFAULT_LAYOUT,
  GARCON_MENU_NODE_TYPE_LAYOUT,
  GARCON_MENU_NODE_TYPE_MENUNAME,
  GARCON_MENU_NODE_TYPE_SEPARATOR,
  GARCON_MENU_NODE_TYPE_MERGE,
  GARCON_MENU_NODE_TYPE_MERGE_FILE,
  GARCON_MENU_NODE_TYPE_MERGE_DIR,
  GARCON_MENU_NODE_TYPE_DEFAULT_MERGE_DIRS,
} GarconMenuNodeType;



typedef enum
{
  GARCON_MENU_LAYOUT_MERGE_MENUS,
  GARCON_MENU_LAYOUT_MERGE_FILES,
  GARCON_MENU_LAYOUT_MERGE_ALL,
} GarconMenuLayoutMergeType;

typedef enum
{
  GARCON_MENU_MERGE_FILE_PATH,
  GARCON_MENU_MERGE_FILE_PARENT,
} GarconMenuMergeFileType;



typedef union  _GarconMenuNodeData  GarconMenuNodeData;
typedef struct _GarconMenuNodeClass GarconMenuNodeClass;
typedef struct _GarconMenuNode      GarconMenuNode;

#define GARCON_TYPE_MENU_NODE            (garcon_menu_node_get_type ())
#define GARCON_MENU_NODE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GARCON_TYPE_MENU_NODE, GarconMenuNode))
#define GARCON_MENU_NODE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GARCON_TYPE_MENU_NODE, GarconMenuNodeClass))
#define GARCON_IS_MENU_NODE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GARCON_TYPE_MENU_NODE))
#define GARCON_IS_MENU_NODE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GARCON_TYPE_MENU_NODE)
#define GARCON_MENU_NODE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GARCON_TYPE_MENU_NODE, GarconMenuNodeClass))

GType                     garcon_menu_node_type_get_type                (void);
GType                     garcon_menu_node_get_type                     (void) G_GNUC_CONST;

GarconMenuNode           *garcon_menu_node_new                          (GarconMenuNodeType      node_type) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
GarconMenuNodeType        garcon_menu_node_get_node_type                (GarconMenuNode         *node);
GarconMenuNode           *garcon_menu_node_create                       (GarconMenuNodeType      node_type,
                                                                         gpointer                first_value,
                                                                         ...) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
GarconMenuNode           *garcon_menu_node_copy                         (GarconMenuNode         *node,
                                                                         gpointer                data) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
const gchar              *garcon_menu_node_get_string                   (GarconMenuNode         *node);
void                      garcon_menu_node_set_string                   (GarconMenuNode         *node,
                                                                         const gchar            *value);
GarconMenuMergeFileType   garcon_menu_node_get_merge_file_type          (GarconMenuNode         *node);
void                      garcon_menu_node_set_merge_file_type          (GarconMenuNode         *node,
                                                                         GarconMenuMergeFileType type);
const gchar              *garcon_menu_node_get_merge_file_filename      (GarconMenuNode         *node);
void                      garcon_menu_node_set_merge_file_filename      (GarconMenuNode         *node,
                                                                         const gchar            *filename);

GNode                    *garcon_menu_node_tree_get_child_node          (GNode                  *tree,
                                                                         GarconMenuNodeType      type,
                                                                         gboolean                reverse);
GList                    *garcon_menu_node_tree_get_child_nodes         (GNode                  *tree,
                                                                         GarconMenuNodeType      type,
                                                                         gboolean                reverse);
GList                    *garcon_menu_node_tree_get_string_children     (GNode                  *tree,
                                                                         GarconMenuNodeType      type,
                                                                         gboolean                reverse);
gboolean                  garcon_menu_node_tree_get_boolean_child       (GNode                  *tree,
                                                                         GarconMenuNodeType      type);
const gchar              *garcon_menu_node_tree_get_string_child        (GNode                  *tree,
                                                                         GarconMenuNodeType      type);
gboolean                  garcon_menu_node_tree_rule_matches            (GNode                  *tree,
                                                                         GarconMenuItem         *item);
GarconMenuNodeType        garcon_menu_node_tree_get_node_type           (GNode                  *tree);
const gchar              *garcon_menu_node_tree_get_string              (GNode                  *tree);
void                      garcon_menu_node_tree_set_string              (GNode                  *tree,
                                                                         const gchar            *value);
GarconMenuLayoutMergeType garcon_menu_node_tree_get_layout_merge_type   (GNode                  *tree);
GarconMenuMergeFileType   garcon_menu_node_tree_get_merge_file_type     (GNode                  *tree);
const gchar              *garcon_menu_node_tree_get_merge_file_filename (GNode                  *tree);
void                      garcon_menu_node_tree_set_merge_file_filename (GNode                  *tree,
                                                                         const gchar            *filename);
gint                      garcon_menu_node_tree_compare                 (GNode                  *tree,
                                                                         GNode                  *other_tree);
GNode                    *garcon_menu_node_tree_copy                    (GNode                  *tree);
void                      garcon_menu_node_tree_free                    (GNode                  *tree);
void                      garcon_menu_node_tree_free_data               (GNode                  *tree);


G_END_DECLS

#endif /* !__GARCON_MENU_NODE_H__ */
