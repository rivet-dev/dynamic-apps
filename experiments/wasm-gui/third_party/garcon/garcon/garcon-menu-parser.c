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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include <gio/gio.h>

#include <garcon/garcon-menu-node.h>
#include <garcon/garcon-menu-tree-provider.h>
#include <garcon/garcon-menu-parser.h>



/* Menu file parser states */
typedef enum
{
  GARCON_MENU_PARSER_STATE_START,
  GARCON_MENU_PARSER_STATE_ROOT,
  GARCON_MENU_PARSER_STATE_MENU,
  GARCON_MENU_PARSER_STATE_RULE,
  GARCON_MENU_PARSER_STATE_MOVE,
  GARCON_MENU_PARSER_STATE_LAYOUT,
  GARCON_MENU_PARSER_STATE_END,
} GarconMenuParserState;

/* Node types */
typedef enum
{
  GARCON_MENU_PARSER_NODE_TYPE_NONE,
  GARCON_MENU_PARSER_NODE_TYPE_NAME,
  GARCON_MENU_PARSER_NODE_TYPE_DIRECTORY,
  GARCON_MENU_PARSER_NODE_TYPE_DIRECTORY_DIR,
  GARCON_MENU_PARSER_NODE_TYPE_APP_DIR,
  GARCON_MENU_PARSER_NODE_TYPE_FILENAME,
  GARCON_MENU_PARSER_NODE_TYPE_CATEGORY,
  GARCON_MENU_PARSER_NODE_TYPE_OLD,
  GARCON_MENU_PARSER_NODE_TYPE_NEW,
  GARCON_MENU_PARSER_NODE_TYPE_MENUNAME,
  GARCON_MENU_PARSER_NODE_TYPE_MERGE_FILE,
  GARCON_MENU_PARSER_NODE_TYPE_MERGE_DIR,
} GarconMenuParserNodeType;

typedef struct _GarconMenuParserContext GarconMenuParserContext;

struct _GarconMenuParserContext
{
  GarconMenuParserNodeType node_type;
  GarconMenuParserState    state;
  GarconMenuParser        *parser;
  GNode                   *node;
};



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
};



static void   garcon_menu_parser_provider_init (GarconMenuTreeProviderIface *iface);
static void   garcon_menu_parser_finalize      (GObject                     *object);
static void   garcon_menu_parser_get_property  (GObject                     *object,
                                                guint                        prop_id,
                                                GValue                      *value,
                                                GParamSpec                  *pspec);
static void   garcon_menu_parser_set_property  (GObject                     *object,
                                                guint                        prop_id,
                                                const GValue                *value,
                                                GParamSpec                  *pspec);
static void   garcon_menu_parser_start_element (GMarkupParseContext         *context,
                                                const gchar                 *element_name,
                                                const gchar                **attribute_names,
                                                const gchar                **attribute_values,
                                                gpointer                     user_data,
                                                GError                     **error);
static void   garcon_menu_parser_end_element   (GMarkupParseContext         *context,
                                                const gchar                 *element_name,
                                                gpointer                     user_data,
                                                GError                     **error);
static void   garcon_menu_parser_characters    (GMarkupParseContext         *context,
                                                const gchar                 *text,
                                                gsize                        text_len,
                                                gpointer                     user_data,
                                                GError                     **error);
static GNode *garcon_menu_parser_get_tree      (GarconMenuTreeProvider      *provider);
static GFile *garcon_menu_parser_get_file      (GarconMenuTreeProvider      *provider);



struct _GarconMenuParserPrivate
{
  /* .menu file */
  GFile *file;

  /* Root menu node */
  GNode *menu;
};



G_DEFINE_TYPE_WITH_CODE (GarconMenuParser, garcon_menu_parser,
                         G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GarconMenuParser)
                         G_IMPLEMENT_INTERFACE (GARCON_TYPE_MENU_TREE_PROVIDER,
                                                garcon_menu_parser_provider_init))



static void
garcon_menu_parser_class_init (GarconMenuParserClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = garcon_menu_parser_finalize;
  gobject_class->get_property = garcon_menu_parser_get_property;
  gobject_class->set_property = garcon_menu_parser_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        "file",
                                                        "file",
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS |
                                                        G_PARAM_CONSTRUCT_ONLY));
}



static void
garcon_menu_parser_provider_init (GarconMenuTreeProviderIface *iface)
{
  iface->get_tree = garcon_menu_parser_get_tree;
  iface->get_file = garcon_menu_parser_get_file;
}



static void
garcon_menu_parser_init (GarconMenuParser *parser)
{
  parser->priv = garcon_menu_parser_get_instance_private (parser);
  parser->priv->file = NULL;
  parser->priv->menu = NULL;
}



static void
garcon_menu_parser_finalize (GObject *object)
{
  GarconMenuParser *parser = GARCON_MENU_PARSER (object);

  garcon_menu_node_tree_free (parser->priv->menu);

  g_object_unref (parser->priv->file);

  (*G_OBJECT_CLASS (garcon_menu_parser_parent_class)->finalize) (object);
}



static void
garcon_menu_parser_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GarconMenuParser *parser = GARCON_MENU_PARSER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, parser->priv->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
garcon_menu_parser_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GarconMenuParser *parser = GARCON_MENU_PARSER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      parser->priv->file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



GarconMenuParser *
garcon_menu_parser_new (GFile *file)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  return g_object_new (GARCON_TYPE_MENU_PARSER, "file", file, NULL);
}



gboolean
garcon_menu_parser_run (GarconMenuParser *parser,
                        GCancellable     *cancellable,
                        GError          **error)
{
  GarconMenuParserContext parser_context;
  GMarkupParseContext      *context;
  GMarkupParser             markup_parser = {
    garcon_menu_parser_start_element,
    garcon_menu_parser_end_element,
    garcon_menu_parser_characters,
    NULL,
  };
  gboolean                  result = TRUE;
  gchar                    *data;
  gsize                     data_length;
  gchar                    *name;
  GError                   *err = NULL;

  g_return_val_if_fail (GARCON_IS_MENU_PARSER (parser), FALSE);
  g_return_val_if_fail (G_IS_FILE (parser->priv->file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* Try to open and read the file */
  if (G_UNLIKELY (!g_file_load_contents (parser->priv->file, cancellable,
                                         &data, &data_length, NULL, &err)))
    {
      name = g_file_get_parse_name (parser->priv->file);

      if (err != NULL)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_EXIST,
                       _("Could not load menu file data from %s: %s"),
                       name, err->message);
          g_error_free (err);
        }
      else
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_EXIST,
                       _("Could not load menu file data from %s"),
                       name);
        }

      g_free (name);
      return FALSE;
    }

  /* Create parser context */
  parser_context.parser = parser;
  parser_context.node_type = GARCON_MENU_PARSER_NODE_TYPE_NONE;
  parser_context.state = GARCON_MENU_PARSER_STATE_START;
  parser_context.node = NULL;

  /* Create markup parse context */
  context = g_markup_parse_context_new (&markup_parser, 0, &parser_context, NULL);

  /* Try to parse the menu file */
  if (!g_markup_parse_context_parse (context, data, data_length, error) ||
      !g_markup_parse_context_end_parse (context, error))
    {
      result = FALSE;
    }

  g_markup_parse_context_free (context);
  g_free (data);

  return result;
}



static GNode *
garcon_menu_parser_get_tree (GarconMenuTreeProvider *provider)
{
  g_return_val_if_fail (GARCON_IS_MENU_PARSER (provider), NULL);
  return garcon_menu_node_tree_copy (GARCON_MENU_PARSER (provider)->priv->menu);
}



static GFile *
garcon_menu_parser_get_file (GarconMenuTreeProvider *provider)
{
  g_return_val_if_fail (GARCON_IS_MENU_PARSER (provider), NULL);
  return g_object_ref (GARCON_MENU_PARSER (provider)->priv->file);
}



static void
garcon_menu_parser_start_element (GMarkupParseContext *context,
                                  const gchar         *element_name,
                                  const gchar        **attribute_names,
                                  const gchar        **attribute_values,
                                  gpointer             user_data,
                                  GError             **error)
{
  GarconMenuParserContext *parser_context = (GarconMenuParserContext *)user_data;
  GarconMenuNode          *node_;

  switch (parser_context->state)
    {
    case GARCON_MENU_PARSER_STATE_START:
      if (g_str_equal (element_name, "Menu"))
        {
          parser_context->parser->priv->menu = g_node_new (NULL);

          parser_context->state = GARCON_MENU_PARSER_STATE_ROOT;
          parser_context->node = parser_context->parser->priv->menu;
        }
      break;

    case GARCON_MENU_PARSER_STATE_ROOT:
    case GARCON_MENU_PARSER_STATE_MENU:
      if (g_str_equal (element_name, "Name"))
        parser_context->node_type = GARCON_MENU_PARSER_NODE_TYPE_NAME;

      else if (g_str_equal (element_name, "Directory"))
        parser_context->node_type = GARCON_MENU_PARSER_NODE_TYPE_DIRECTORY;
      else if (g_str_equal (element_name, "DirectoryDir"))
        parser_context->node_type = GARCON_MENU_PARSER_NODE_TYPE_DIRECTORY_DIR;
      else if (g_str_equal (element_name, "DefaultDirectoryDirs"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_DEFAULT_DIRECTORY_DIRS, NULL);
          g_node_append_data (parser_context->node, node_);
        }

      else if (g_str_equal (element_name, "AppDir"))
        parser_context->node_type = GARCON_MENU_PARSER_NODE_TYPE_APP_DIR;
      else if (g_str_equal (element_name, "DefaultAppDirs"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_DEFAULT_APP_DIRS, NULL);
          g_node_append_data (parser_context->node, node_);
        }

      else if (g_str_equal (element_name, "Deleted"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_DELETED, NULL);
          g_node_append_data (parser_context->node, node_);
        }
      else if (g_str_equal (element_name, "NotDeleted"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_NOT_DELETED, NULL);
          g_node_append_data (parser_context->node, node_);
        }
      else if (g_str_equal (element_name, "OnlyUnallocated"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_ONLY_UNALLOCATED, NULL);
          g_node_append_data (parser_context->node, node_);
        }
      else if (g_str_equal (element_name, "NotOnlyUnallocated"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_NOT_ONLY_UNALLOCATED, NULL);
          g_node_append_data (parser_context->node, node_);
        }

      else if (g_str_equal (element_name, "Include"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_INCLUDE, NULL);
          parser_context->node = g_node_append_data (parser_context->node, node_);
          parser_context->state = GARCON_MENU_PARSER_STATE_RULE;
        }
      else if (g_str_equal (element_name, "Exclude"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_EXCLUDE, NULL);
          parser_context->node = g_node_append_data (parser_context->node, node_);
          parser_context->state = GARCON_MENU_PARSER_STATE_RULE;
        }

      else if (g_str_equal (element_name, "Menu"))
        {
          parser_context->node = g_node_append_data (parser_context->node, NULL);
          parser_context->state = GARCON_MENU_PARSER_STATE_MENU;
        }

      else if (g_str_equal (element_name, "Move"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_MOVE, NULL);
          parser_context->node = g_node_append_data (parser_context->node, node_);
          parser_context->state = GARCON_MENU_PARSER_STATE_MOVE;
        }

      else if (g_str_equal (element_name, "DefaultLayout"))
        {
          /* TODO Parse attributes */
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_DEFAULT_LAYOUT, NULL);
          parser_context->node = g_node_append_data (parser_context->node, node_);
          parser_context->state = GARCON_MENU_PARSER_STATE_LAYOUT;
        }
      else if (g_str_equal (element_name, "Layout"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_LAYOUT, NULL);
          parser_context->node = g_node_append_data (parser_context->node, node_);
          parser_context->state = GARCON_MENU_PARSER_STATE_LAYOUT;
        }

      else if (g_str_equal (element_name, "MergeFile"))
        {
          GarconMenuMergeFileType type = GARCON_MENU_MERGE_FILE_PATH;

          if (g_strv_length ((gchar **)attribute_names) == 1 &&
              g_str_equal (attribute_names[0], "type"))
            {
              if (g_str_equal (attribute_values[0], "parent"))
                type = GARCON_MENU_MERGE_FILE_PARENT;
            }

          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_MERGE_FILE, GUINT_TO_POINTER (type));
          parser_context->node = g_node_append_data (parser_context->node, node_);
          parser_context->node_type = GARCON_MENU_PARSER_NODE_TYPE_MERGE_FILE;
        }
      else if (g_str_equal (element_name, "MergeDir"))
        parser_context->node_type = GARCON_MENU_PARSER_NODE_TYPE_MERGE_DIR;
      else if (g_str_equal (element_name, "DefaultMergeDirs"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_DEFAULT_MERGE_DIRS, NULL);
          g_node_append_data (parser_context->node, node_);
        }
      break;

    case GARCON_MENU_PARSER_STATE_RULE:
      if (g_str_equal (element_name, "All"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_ALL, NULL);
          g_node_append_data (parser_context->node, node_);
        }
      else if (g_str_equal (element_name, "Filename"))
        parser_context->node_type = GARCON_MENU_PARSER_NODE_TYPE_FILENAME;
      else if (g_str_equal (element_name, "Category"))
        parser_context->node_type = GARCON_MENU_PARSER_NODE_TYPE_CATEGORY;
      else if (g_str_equal (element_name, "Or"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_OR, NULL);
          parser_context->node = g_node_append_data (parser_context->node, node_);
        }
      else if (g_str_equal (element_name, "And"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_AND, NULL);
          parser_context->node = g_node_append_data (parser_context->node, node_);
        }
      else if (g_str_equal (element_name, "Not"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_NOT, NULL);
          parser_context->node = g_node_append_data (parser_context->node, node_);
        }
      break;

    case GARCON_MENU_PARSER_STATE_MOVE:
      if (g_str_equal (element_name, "Old"))
        parser_context->node_type = GARCON_MENU_PARSER_NODE_TYPE_OLD;
      else if (g_str_equal (element_name, "New"))
        parser_context->node_type = GARCON_MENU_PARSER_NODE_TYPE_NEW;
      break;

    case GARCON_MENU_PARSER_STATE_LAYOUT:
      if (g_str_equal (element_name, "Filename"))
        parser_context->node_type = GARCON_MENU_PARSER_NODE_TYPE_FILENAME;
      else if (g_str_equal (element_name, "Menuname"))
        {
          /* TODO Parse attributes */
          parser_context->node_type = GARCON_MENU_PARSER_NODE_TYPE_MENUNAME;
        }
      else if (g_str_equal (element_name, "Separator"))
        {
          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_SEPARATOR, NULL);
          g_node_append_data (parser_context->node, node_);
        }
      else if (g_str_equal (element_name, "Merge"))
        {
          GarconMenuLayoutMergeType type = GARCON_MENU_LAYOUT_MERGE_ALL;

          if (g_strv_length ((gchar **)attribute_names) == 1 &&
              g_str_equal (attribute_names[0], "type"))
            {
              if (g_str_equal (attribute_values[0], "menus"))
                type = GARCON_MENU_LAYOUT_MERGE_MENUS;
              else if (g_str_equal (attribute_values[0], "files"))
                type = GARCON_MENU_LAYOUT_MERGE_FILES;
            }

          node_ = garcon_menu_node_create (GARCON_MENU_NODE_TYPE_MERGE, GUINT_TO_POINTER (type));
          g_node_append_data (parser_context->node, node_);
        }
      break;

    default:
      break;
    }
}



static void
garcon_menu_parser_end_element (GMarkupParseContext *context,
                                const gchar         *element_name,
                                gpointer             user_data,
                                GError             **error)
{
  GarconMenuParserContext *parser_context = (GarconMenuParserContext *)user_data;

  switch (parser_context->state)
    {
    case GARCON_MENU_PARSER_STATE_ROOT:
    case GARCON_MENU_PARSER_STATE_MENU:
      if (g_str_equal (element_name, "Menu"))
        {
          /* We no longer have a menu on the stack */
          parser_context->node = parser_context->node->parent;

          if (parser_context->node == NULL)
            parser_context->state = GARCON_MENU_PARSER_STATE_END;
          else if (parser_context->node->parent == NULL)
            parser_context->state = GARCON_MENU_PARSER_STATE_ROOT;
        }
      else if (g_str_equal (element_name, "MergeFile"))
        {
          parser_context->node = parser_context->node->parent;

          if (parser_context->node->parent == NULL)
            parser_context->state = GARCON_MENU_PARSER_STATE_ROOT;
          else
            parser_context->state = GARCON_MENU_PARSER_STATE_MENU;
        }
      break;

    case GARCON_MENU_PARSER_STATE_RULE:
      if (g_str_equal (element_name, "Include") ||
          g_str_equal (element_name, "Exclude") ||
          g_str_equal (element_name, "Or") ||
          g_str_equal (element_name, "And") ||
          g_str_equal (element_name, "Not"))
        {
          /* Switch to the parent rule or menu */
          parser_context->node = parser_context->node->parent;

          /* Set the parse state according to the parent type */
          if (parser_context->node->data == NULL)
            {
              if (parser_context->node->parent == NULL)
                parser_context->state = GARCON_MENU_PARSER_STATE_ROOT;
              else
                parser_context->state = GARCON_MENU_PARSER_STATE_MENU;
            }
        }
      break;

    case GARCON_MENU_PARSER_STATE_MOVE:
      if (g_str_equal (element_name, "Move"))
        {
          parser_context->node = parser_context->node->parent;

          /* Set the parse state according to the parent type */
          if (parser_context->node->data == NULL)
            {
              if (parser_context->node->parent == NULL)
                parser_context->state = GARCON_MENU_PARSER_STATE_ROOT;
              else
                parser_context->state = GARCON_MENU_PARSER_STATE_MENU;
            }
        }
      break;

    case GARCON_MENU_PARSER_STATE_LAYOUT:
      if (g_str_equal (element_name, "Layout") || g_str_equal (element_name, "DefaultLayout"))
        {
          parser_context->node = parser_context->node->parent;

          /* Set the parse state according to the parent type */
          if (parser_context->node->data == NULL)
            {
              if (parser_context->node->parent == NULL)
                parser_context->state = GARCON_MENU_PARSER_STATE_ROOT;
              else
                parser_context->state = GARCON_MENU_PARSER_STATE_MENU;
            }
        }
      break;

    default:
      break;
    }
}



static void
garcon_menu_parser_characters (GMarkupParseContext *context,
                               const gchar         *text,
                               gsize                text_len,
                               gpointer             user_data,
                               GError             **error)
{
  GarconMenuParserContext *parser_context = (GarconMenuParserContext *)user_data;
  gchar                 *data;

  /* Ignore characters outside the root <Menu> element */
  if (G_UNLIKELY (parser_context->node_type == GARCON_MENU_PARSER_NODE_TYPE_NONE))
    return;

  /* Generate NUL-terminated string */
  data = g_strndup (text, text_len);

  switch (parser_context->node_type)
    {
    case GARCON_MENU_PARSER_NODE_TYPE_NAME:
      g_node_append_data (parser_context->node,
                          garcon_menu_node_create (GARCON_MENU_NODE_TYPE_NAME, data));
      break;

    case GARCON_MENU_PARSER_NODE_TYPE_DIRECTORY:
      g_node_append_data (parser_context->node,
                          garcon_menu_node_create (GARCON_MENU_NODE_TYPE_DIRECTORY, data));
      break;

    case GARCON_MENU_PARSER_NODE_TYPE_DIRECTORY_DIR:
      g_node_append_data (parser_context->node,
                          garcon_menu_node_create (GARCON_MENU_NODE_TYPE_DIRECTORY_DIR, data));
      break;

    case GARCON_MENU_PARSER_NODE_TYPE_APP_DIR:
      g_node_append_data (parser_context->node,
                          garcon_menu_node_create (GARCON_MENU_NODE_TYPE_APP_DIR, data));
      break;

    case GARCON_MENU_PARSER_NODE_TYPE_FILENAME:
      g_node_append_data (parser_context->node,
                          garcon_menu_node_create (GARCON_MENU_NODE_TYPE_FILENAME, data));
      break;

    case GARCON_MENU_PARSER_NODE_TYPE_CATEGORY:
      g_node_append_data (parser_context->node,
                          garcon_menu_node_create (GARCON_MENU_NODE_TYPE_CATEGORY, data));
      break;

    case GARCON_MENU_PARSER_NODE_TYPE_OLD:
      g_node_append_data (parser_context->node,
                          garcon_menu_node_create (GARCON_MENU_NODE_TYPE_OLD, data));
      break;

    case GARCON_MENU_PARSER_NODE_TYPE_NEW:
      g_node_append_data (parser_context->node,
                          garcon_menu_node_create (GARCON_MENU_NODE_TYPE_NEW, data));
      break;

    case GARCON_MENU_PARSER_NODE_TYPE_MENUNAME:
      g_node_append_data (parser_context->node,
                          garcon_menu_node_create (GARCON_MENU_NODE_TYPE_MENUNAME, data));
      break;

    case GARCON_MENU_PARSER_NODE_TYPE_MERGE_FILE:
      if (garcon_menu_node_tree_get_node_type (parser_context->node) == GARCON_MENU_NODE_TYPE_MERGE_FILE)
        garcon_menu_node_set_merge_file_filename (parser_context->node->data, data);
      break;

    case GARCON_MENU_PARSER_NODE_TYPE_MERGE_DIR:
      g_node_append_data (parser_context->node,
                          garcon_menu_node_create (GARCON_MENU_NODE_TYPE_MERGE_DIR, data));
      break;

    default:
      break;
    }

  /* Free NUL-terminated string */
  g_free (data);

  /* Invalidate node type information */
  parser_context->node_type = GARCON_MENU_PARSER_NODE_TYPE_NONE;
}

