/*
 * Copyright (C) 2011-2013 Nick Schermer <nick@xfce.org>
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

#include <glib/gstdio.h>
#include <cairo/cairo-gobject.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <src/appfinder-model.h>
#include <src/appfinder-private.h>



#define OLD_HISTORY_PATH "xfce4/xfce4-appfinder/history"
#define NEW_HISTORY_PATH "xfce4/appfinder/history"
#define BOOKMARKS_PATH   "xfce4/appfinder/bookmarks"
#define FRECENCY_PATH    "xfce4/appfinder/frecency"


static void               xfce_appfinder_model_tree_model_init        (GtkTreeModelIface        *iface);
static void               xfce_appfinder_model_get_property           (GObject                  *object,
                                                                       guint                     prop_id,
                                                                       GValue                   *value,
                                                                       GParamSpec               *pspec);
static void               xfce_appfinder_model_set_property           (GObject                  *object,
                                                                       guint                     prop_id,
                                                                       const GValue             *value,
                                                                       GParamSpec               *pspec);
static void               xfce_appfinder_model_finalize               (GObject                  *object);
static GtkTreeModelFlags  xfce_appfinder_model_get_flags              (GtkTreeModel             *tree_model);
static gint               xfce_appfinder_model_get_n_columns          (GtkTreeModel             *tree_model);
static GType              xfce_appfinder_model_get_column_type        (GtkTreeModel             *tree_model,
                                                                       gint                      column);
static gboolean           xfce_appfinder_model_get_iter               (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter,
                                                                       GtkTreePath              *path);
static GtkTreePath       *xfce_appfinder_model_get_path               (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter);
static void               xfce_appfinder_model_get_value              (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter,
                                                                       gint                      column,
                                                                       GValue                   *value);
static gboolean           xfce_appfinder_model_iter_next              (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter);
static gboolean           xfce_appfinder_model_iter_children          (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter,
                                                                       GtkTreeIter              *parent);
static gboolean           xfce_appfinder_model_iter_has_child         (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter);
static gint               xfce_appfinder_model_iter_n_children        (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter);
static gboolean           xfce_appfinder_model_iter_nth_child         (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter,
                                                                       GtkTreeIter              *parent,
                                                                       gint                      n);
static gboolean           xfce_appfinder_model_iter_parent            (GtkTreeModel             *tree_model,
                                                                       GtkTreeIter              *iter,
                                                                       GtkTreeIter              *child);
static void               xfce_appfinder_model_menu_changed           (GarconMenu               *menu,
                                                                       XfceAppfinderModel       *model);
static gpointer           xfce_appfinder_model_collect_thread         (gpointer                  user_data);
static void               xfce_appfinder_model_item_changed           (GarconMenuItem           *menu_item,
                                                                       XfceAppfinderModel       *model);
static void               xfce_appfinder_model_item_free              (gpointer                  data,
                                                                       XfceAppfinderModel       *model);
static void               xfce_appfinder_model_history_changed        (GFileMonitor             *monitor,
                                                                       GFile                    *file,
                                                                       GFile                    *other_file,
                                                                       GFileMonitorEvent         event_type,
                                                                       XfceAppfinderModel       *model);
static void               xfce_appfinder_model_history_monitor_stop   (XfceAppfinderModel       *model);
static void               xfce_appfinder_model_history_monitor        (XfceAppfinderModel       *model,
                                                                       const gchar              *path);
static void               xfce_appfinder_model_bookmarks_changed      (GFileMonitor             *monitor,
                                                                       GFile                    *file,
                                                                       GFile                    *other_file,
                                                                       GFileMonitorEvent         event_type,
                                                                       XfceAppfinderModel       *model);
static void               xfce_appfinder_model_bookmarks_monitor_stop (XfceAppfinderModel       *model);
static void               xfce_appfinder_model_bookmarks_monitor      (XfceAppfinderModel       *model,
                                                                       const gchar              *path);

static gboolean           xfce_appfinder_model_fuzzy_match            (const gchar              *source,
                                                                       const gchar              *token);
static gint               xfce_appfinder_model_item_compare_frecency  (gconstpointer             a,
                                                                       gconstpointer             b,
                                                                       gpointer                  data);
static void               xfce_appfinder_model_frecency_collect       (XfceAppfinderModel       *model,
                                                                       GMappedFile              *mmap);
static void               xfce_appfinder_model_frecency_free          (gpointer                  data);
static gchar*             xfce_appfinder_model_unescape_entry_value   (const gchar              *value);
static void               xfce_appfinder_model_migrate_history_file   (void);

struct _XfceAppfinderModelClass
{
  GObjectClass __parent__;
};

struct _XfceAppfinderModel
{
  GObject                __parent__;

  gint                   stamp;

  GSList                *items;
  GHashTable            *items_hash;

  GHashTable            *bookmarks_hash;
  GHashTable            *frecencies_hash;

  GFileMonitor          *bookmarks_monitor;
  GFile                 *bookmarks_file;
  guint64                bookmarks_mtime;

  GarconMenu            *menu;
  guint                  menu_changed_idle_id;

  GarconMenuDirectory   *command_category;

  GSList                *categories;
  guint                  categories_changed_idle_id;

  guint                  collect_idle_id;
  GSList                *collect_items;
  GSList                *collect_categories;
  GThread               *collect_thread;
  GCancellable          *collect_cancelled;

  GFileMonitor          *history_monitor;
  GFile                 *history_file;
  guint64                history_mtime;

  gboolean               sort_by_frecency;

  XfceAppfinderIconSize  icon_size;

  gint                   scale_factor;
};

typedef struct
{
  guint   frequency;
  guint64 recency;
}
Frecency;

typedef struct
{
  GarconMenuItem  *item;
  gchar           *key;
  gchar           *abstract;
  GPtrArray       *categories;
  gchar           *command;
  gchar           *tooltip;
  guint            not_visible : 1;
  guint            is_bookmark : 1;

  Frecency        *frecency; /* owned by frecencies_hash */

  GdkPixbuf       *icon;
  cairo_surface_t *surface;
  cairo_surface_t *surface_large;
}
ModelItem;

typedef struct
{
  GSList              *items;
  GarconMenuDirectory *category;
  GHashTable          *desktop_ids;
}
CollectContext;

enum
{
  CATEGORIES_CHANGED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_ICON_SIZE,
  PROP_SCALE_FACTOR,
  PROP_SORT_BY_FRECENCY
};



static guint model_signals[LAST_SIGNAL];



G_DEFINE_TYPE_WITH_CODE (XfceAppfinderModel, xfce_appfinder_model, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, xfce_appfinder_model_tree_model_init))



static void
xfce_appfinder_model_class_init (XfceAppfinderModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = xfce_appfinder_model_get_property;
  gobject_class->set_property = xfce_appfinder_model_set_property;
  gobject_class->finalize = xfce_appfinder_model_finalize;

  g_object_class_install_property (gobject_class,
                                   PROP_ICON_SIZE,
                                   g_param_spec_uint ("icon-size", NULL, NULL,
                                                      XFCE_APPFINDER_ICON_SIZE_SMALLEST,
                                                      XFCE_APPFINDER_ICON_SIZE_LARGEST,
                                                      XFCE_APPFINDER_ICON_SIZE_DEFAULT_ITEM,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SCALE_FACTOR,
                                   g_param_spec_uint ("scale-factor", NULL, NULL,
                                                      1, G_MAXINT, 1,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SORT_BY_FRECENCY,
                                   g_param_spec_boolean ("sort-by-frecency", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  model_signals[CATEGORIES_CHANGED] =
    g_signal_new (g_intern_static_string ("categories-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
xfce_appfinder_model_init (XfceAppfinderModel *model)
{
  /* generate a unique stamp */
  model->stamp = g_random_int ();
  model->items_hash = g_hash_table_new (g_str_hash, g_str_equal);
  model->bookmarks_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  model->frecencies_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, xfce_appfinder_model_frecency_free);
  model->icon_size = XFCE_APPFINDER_ICON_SIZE_DEFAULT_ITEM;
  model->command_category = xfce_appfinder_model_get_command_category ();
  model->collect_cancelled = g_cancellable_new ();

  model->menu = garcon_menu_new_applications ();
  appfinder_refcount_debug_add (G_OBJECT (model->menu), "main menu");
}



static void
xfce_appfinder_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = xfce_appfinder_model_get_flags;
  iface->get_n_columns = xfce_appfinder_model_get_n_columns;
  iface->get_column_type = xfce_appfinder_model_get_column_type;
  iface->get_iter = xfce_appfinder_model_get_iter;
  iface->get_path = xfce_appfinder_model_get_path;
  iface->get_value = xfce_appfinder_model_get_value;
  iface->iter_next = xfce_appfinder_model_iter_next;
  iface->iter_children = xfce_appfinder_model_iter_children;
  iface->iter_has_child = xfce_appfinder_model_iter_has_child;
  iface->iter_n_children = xfce_appfinder_model_iter_n_children;
  iface->iter_nth_child = xfce_appfinder_model_iter_nth_child;
  iface->iter_parent = xfce_appfinder_model_iter_parent;
}



static void
xfce_appfinder_model_get_property (GObject      *object,
                                   guint         prop_id,
                                   GValue       *value,
                                   GParamSpec   *pspec)
{
  XfceAppfinderModel *model = XFCE_APPFINDER_MODEL (object);

  switch (prop_id)
    {
    case PROP_ICON_SIZE:
      g_value_set_uint (value, model->icon_size);
      break;

    case PROP_SCALE_FACTOR:
      g_value_set_uint (value, model->scale_factor);
      break;

    case PROP_SORT_BY_FRECENCY:
      g_value_set_boolean (value, model->sort_by_frecency);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_appfinder_model_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  XfceAppfinderModel    *model = XFCE_APPFINDER_MODEL (object);
  XfceAppfinderIconSize  icon_size;
  gint                   scale_factor;

  switch (prop_id)
    {
    case PROP_ICON_SIZE:
      icon_size = g_value_get_uint (value);
      if (model->icon_size != icon_size)
        {
          model->icon_size = icon_size;

          /* trigger a theme change to reload icons */
          xfce_appfinder_model_icon_theme_changed (model);
        }
      break;

    case PROP_SCALE_FACTOR:
      scale_factor = g_value_get_uint (value);
      if (model->scale_factor != scale_factor)
        model->scale_factor = scale_factor;
      break;

    case PROP_SORT_BY_FRECENCY:
      model->sort_by_frecency = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_appfinder_model_finalize (GObject *object)
{
  XfceAppfinderModel *model = XFCE_APPFINDER_MODEL (object);

  /* join the collector thread */
  g_cancellable_cancel (model->collect_cancelled);
  g_thread_join (model->collect_thread);
  g_object_unref (G_OBJECT (model->collect_cancelled));

  /* cancel any pending collect idle source */
  if (G_UNLIKELY (model->collect_idle_id != 0))
    g_source_remove (model->collect_idle_id);
  g_slist_free (model->collect_categories);

  if (G_UNLIKELY (model->categories_changed_idle_id != 0))
    g_source_remove (model->categories_changed_idle_id);

  if (G_UNLIKELY (model->menu_changed_idle_id != 0))
    g_source_remove (model->menu_changed_idle_id);

  /* stop history file monitoring */
  xfce_appfinder_model_history_monitor_stop (model);

  /* stop monitoring bookmarks file */
  xfce_appfinder_model_bookmarks_monitor_stop (model);

  g_signal_handlers_disconnect_by_func (G_OBJECT (model->menu),
      G_CALLBACK (xfce_appfinder_model_menu_changed), model);
  g_object_unref (G_OBJECT (model->menu));

  g_slist_foreach (model->collect_items, (GFunc) xfce_appfinder_model_item_free, model);
  g_slist_free (model->collect_items);
  g_slist_foreach (model->items, (GFunc) xfce_appfinder_model_item_free, model);
  g_slist_free (model->items);

  g_slist_foreach (model->collect_categories, (GFunc) (void (*)(void)) g_object_unref, NULL);
  g_slist_free (model->collect_categories);
  g_slist_foreach (model->categories, (GFunc) (void (*)(void)) g_object_unref, NULL);
  g_slist_free (model->categories);

  g_hash_table_destroy (model->items_hash);
  g_hash_table_destroy (model->bookmarks_hash);
  g_hash_table_destroy (model->frecencies_hash);

  g_object_unref (G_OBJECT (model->command_category));

  APPFINDER_DEBUG ("model finalized");

  (*G_OBJECT_CLASS (xfce_appfinder_model_parent_class)->finalize) (object);
}



static GtkTreeModelFlags
xfce_appfinder_model_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}



static gint
xfce_appfinder_model_get_n_columns (GtkTreeModel *tree_model)
{
  return XFCE_APPFINDER_MODEL_N_COLUMNS;
}



static GType
xfce_appfinder_model_get_column_type (GtkTreeModel *tree_model,
                                      gint          column)
{
  switch (column)
    {
    case XFCE_APPFINDER_MODEL_COLUMN_ABSTRACT:
    case XFCE_APPFINDER_MODEL_COLUMN_TITLE:
    case XFCE_APPFINDER_MODEL_COLUMN_URI:
    case XFCE_APPFINDER_MODEL_COLUMN_COMMAND:
    case XFCE_APPFINDER_MODEL_COLUMN_TOOLTIP:
      return G_TYPE_STRING;

    case XFCE_APPFINDER_MODEL_COLUMN_ICON:
    case XFCE_APPFINDER_MODEL_COLUMN_ICON_LARGE:
      return CAIRO_GOBJECT_TYPE_SURFACE;

    case XFCE_APPFINDER_MODEL_COLUMN_PIXBUF:
      return GDK_TYPE_PIXBUF;

    case XFCE_APPFINDER_MODEL_COLUMN_BOOKMARK:
      return G_TYPE_BOOLEAN;

    case XFCE_APPFINDER_MODEL_COLUMN_FREQUENCY:
      return G_TYPE_UINT;

    case XFCE_APPFINDER_MODEL_COLUMN_RECENCY:
      return G_TYPE_UINT64;

    default:
      g_assert_not_reached ();
      return G_TYPE_INVALID;
    }
}



static gboolean
xfce_appfinder_model_get_iter (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter,
                               GtkTreePath  *path)
{
  XfceAppfinderModel *model = XFCE_APPFINDER_MODEL (tree_model);

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), FALSE);
  appfinder_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  iter->stamp = model->stamp;
  iter->user_data = g_slist_nth (model->items, gtk_tree_path_get_indices (path)[0]);

  return (iter->user_data != NULL);
}



static GtkTreePath*
xfce_appfinder_model_get_path (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter)
{
  XfceAppfinderModel *model = XFCE_APPFINDER_MODEL (tree_model);
  gint                idx;

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), NULL);
  appfinder_return_val_if_fail (iter->stamp == model->stamp, NULL);

  /* determine the index of the iter */
  idx = g_slist_position (model->items, iter->user_data);
  if (G_UNLIKELY (idx < 0))
    return NULL;

  return gtk_tree_path_new_from_indices (idx, -1);
}



static void
xfce_appfinder_model_get_value (GtkTreeModel *tree_model,
                                GtkTreeIter  *iter,
                                gint          column,
                                GValue       *value)
{
  XfceAppfinderModel  *model = XFCE_APPFINDER_MODEL (tree_model);
  ModelItem           *item;
  const gchar         *name;
  const gchar         *comment;
  GFile               *file;
  gchar               *parse_name;
  GList               *categories, *li;
  gchar              **cat_arr;
  gchar               *cat_str;
  guint                i;
  GdkPixbuf           *pixbuf;

  appfinder_return_if_fail (XFCE_IS_APPFINDER_MODEL (model));
  appfinder_return_if_fail (iter->stamp == model->stamp);

  item = ITER_GET_DATA (iter);
  appfinder_return_if_fail ((item->item == NULL && item->command != NULL)
                    || (item->item != NULL && GARCON_IS_MENU_ITEM (item->item)));

  switch (column)
    {
    case XFCE_APPFINDER_MODEL_COLUMN_ABSTRACT:
      if (item->abstract == NULL)
        {
          if (item->item != NULL)
            {
              name = garcon_menu_item_get_name (item->item);
              comment = garcon_menu_item_get_comment (item->item);

              if (comment != NULL)
                {
                  if (model->icon_size < XFCE_APPFINDER_ICON_SIZE_SMALL)
                    item->abstract = g_markup_printf_escaped ("<b>%s</b> &#8212; %s", name, comment);
                  else
                    item->abstract = g_markup_printf_escaped ("<b>%s</b>\n%s", name, comment);
                }
              else
                item->abstract = g_markup_printf_escaped ("<b>%s</b>", name);
            }
          else if (item->command != NULL)
            {
              item->abstract = g_markup_escape_text (item->command, -1);
            }
        }

      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, item->abstract);
      break;

    case XFCE_APPFINDER_MODEL_COLUMN_TITLE:
      g_value_init (value, G_TYPE_STRING);
      if (item->item != NULL)
        g_value_set_static_string (value, garcon_menu_item_get_name (item->item));
      else if (item->command != NULL)
        g_value_set_static_string (value, item->command);
      break;

    case XFCE_APPFINDER_MODEL_COLUMN_COMMAND:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, item->command);
      break;

    case XFCE_APPFINDER_MODEL_COLUMN_ICON:
      if (item->icon == NULL && item->item != NULL)
        {
          name = garcon_menu_item_get_icon_name (item->item);
          item->icon = xfce_appfinder_model_load_pixbuf (name, model->icon_size, model->scale_factor);
        }

      if (item->surface == NULL && item->icon != NULL)
        item->surface = gdk_cairo_surface_create_from_pixbuf (item->icon, model->scale_factor, NULL);

      g_value_init (value, CAIRO_GOBJECT_TYPE_SURFACE);
      g_value_set_boxed (value, item->surface);
      break;

    case XFCE_APPFINDER_MODEL_COLUMN_ICON_LARGE:
      if (item->surface_large == NULL && item->item != NULL)
        {
          name = garcon_menu_item_get_icon_name (item->item);
          pixbuf = xfce_appfinder_model_load_pixbuf (name, XFCE_APPFINDER_ICON_SIZE_48, model->scale_factor);
          if (pixbuf != NULL)
            {
              item->surface_large = gdk_cairo_surface_create_from_pixbuf (pixbuf, model->scale_factor, NULL);
              g_object_unref (G_OBJECT (pixbuf));
            }
        }

      g_value_init (value, CAIRO_GOBJECT_TYPE_SURFACE);
      g_value_set_boxed (value, item->surface_large);
      break;

    case XFCE_APPFINDER_MODEL_COLUMN_PIXBUF:
      if (item->icon == NULL && item->item != NULL)
        {
          name = garcon_menu_item_get_icon_name (item->item);
          item->icon = xfce_appfinder_model_load_pixbuf (name, model->icon_size, model->scale_factor);
        }

      g_value_init (value, GDK_TYPE_PIXBUF);
      g_value_set_object (value, item->icon);
      break;

    case XFCE_APPFINDER_MODEL_COLUMN_TOOLTIP:
      if (item->item != NULL && item->tooltip == NULL)
        {
          file = garcon_menu_item_get_file (item->item);
          parse_name = g_file_get_parse_name (file);
          g_object_unref (G_OBJECT (file));

          /* create nice category string */
          categories = garcon_menu_item_get_categories (item->item);
          i = g_list_length (categories);
          cat_arr = g_new0 (gchar *, i + 1);
          for (li = categories; li != NULL; li = li->next)
            cat_arr[--i] = li->data;
          cat_str = g_strjoinv ("; ", cat_arr);
          g_free (cat_arr);

          comment = garcon_menu_item_get_comment (item->item);
          if (comment == NULL)
            comment = "";

          item->tooltip = g_markup_printf_escaped ("<b>%s:</b> %s\n"
                                                   "<b>%s:</b> %s\n"
                                                   "<b>%s:</b> %s\n"
                                                   "<b>%s:</b> %s\n"
                                                   "<b>%s:</b> %s",
                                                   _("Name"), garcon_menu_item_get_name (item->item),
                                                   _("Comment"), comment,
                                                   _("Command"), garcon_menu_item_get_command (item->item),
                                                   _("Categories"), cat_str,
                                                   _("Filename"), parse_name);

          g_free (parse_name);
          g_free (cat_str);
        }

      g_value_init (value, G_TYPE_STRING);
      g_value_set_string (value, item->tooltip);
      break;

    case XFCE_APPFINDER_MODEL_COLUMN_URI:
      g_value_init (value, G_TYPE_STRING);
      if (item->item != NULL)
        g_value_take_string (value, garcon_menu_item_get_uri (item->item));
      break;

    case XFCE_APPFINDER_MODEL_COLUMN_BOOKMARK:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, item->is_bookmark);
      break;

    case XFCE_APPFINDER_MODEL_COLUMN_FREQUENCY:
      g_value_init (value, G_TYPE_UINT);
      g_value_set_uint (value, item->frecency ? item->frecency->frequency : 0);
      break;

    case XFCE_APPFINDER_MODEL_COLUMN_RECENCY:
      g_value_init (value, G_TYPE_UINT64);
      g_value_set_uint64 (value, item->frecency ? item->frecency->recency : 0);
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}



static gboolean
xfce_appfinder_model_iter_next (GtkTreeModel *tree_model,
                                GtkTreeIter  *iter)
{
  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (tree_model), FALSE);
  appfinder_return_val_if_fail (iter->stamp == XFCE_APPFINDER_MODEL (tree_model)->stamp, FALSE);

  iter->user_data = g_slist_next (iter->user_data);
  return (iter->user_data != NULL);
}



static gboolean
xfce_appfinder_model_iter_children (GtkTreeModel *tree_model,
                                    GtkTreeIter  *iter,
                                    GtkTreeIter  *parent)
{
  XfceAppfinderModel *model = XFCE_APPFINDER_MODEL (tree_model);

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), FALSE);

  if (G_LIKELY (parent == NULL && model->items != NULL))
    {
      iter->stamp = model->stamp;
      iter->user_data = model->items;
      return TRUE;
    }

  return FALSE;
}



static gboolean
xfce_appfinder_model_iter_has_child (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter)
{
  return FALSE;
}



static gint
xfce_appfinder_model_iter_n_children (GtkTreeModel *tree_model,
                                      GtkTreeIter  *iter)
{
  XfceAppfinderModel *model = XFCE_APPFINDER_MODEL (tree_model);

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), 0);

  return (iter == NULL) ? g_slist_length (model->items) : 0;
}



static gboolean
xfce_appfinder_model_iter_nth_child (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter,
                                     GtkTreeIter  *parent,
                                     gint          n)
{
  XfceAppfinderModel *model = XFCE_APPFINDER_MODEL (tree_model);

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), FALSE);

  if (G_LIKELY (parent != NULL))
    {
      iter->stamp = model->stamp;
      iter->user_data = g_slist_nth (model->items, n);
      return (iter->user_data != NULL);
    }

  return FALSE;
}



static gboolean
xfce_appfinder_model_iter_parent (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter,
                                  GtkTreeIter  *child)
{
  return FALSE;
}



static gboolean
xfce_appfinder_model_categories_changed_idle (gpointer data)
{
  XfceAppfinderModel *model = XFCE_APPFINDER_MODEL (data);

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), FALSE);

  model->categories_changed_idle_id = 0;

  g_signal_emit (G_OBJECT (model), model_signals[CATEGORIES_CHANGED], 0);

  return FALSE;
}



static void
xfce_appfinder_model_categories_changed (XfceAppfinderModel *model)
{
  if (model->categories_changed_idle_id == 0)
    {
      model->categories_changed_idle_id =
        g_idle_add_full (G_PRIORITY_HIGH_IDLE, xfce_appfinder_model_categories_changed_idle, model, NULL);
    }
}



static gboolean
xfce_appfinder_model_collect_idle (gpointer user_data)
{
  XfceAppfinderModel *model = XFCE_APPFINDER_MODEL (user_data);
  GtkTreePath        *path;
  GtkTreeIter         iter;
  GSList             *li, *lnext;
  GSList             *tmp;
  ModelItem          *item;
  const gchar        *desktop_id;

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), FALSE);
  appfinder_return_val_if_fail (model->items == NULL, FALSE);

  APPFINDER_DEBUG ("insert idle start");

  /* move the collected items "online" */
  model->items = model->collect_items;
  model->collect_items = NULL;

  /* emit notifications for all new items */
  path = gtk_tree_path_new_first ();
  for (li = model->items; li != NULL; li = li->next)
    {
      /* remember the next item */
      lnext = li->next;
      li->next = NULL;

      /* generate the iterator */
      ITER_INIT (iter, model->stamp, li);

      /* emit the "row-inserted" signal */
      gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);

      /* advance the path */
      gtk_tree_path_next (path);

      /* reset the next item */
      li->next = lnext;

      /* watch changes */
      item = li->data;
      if (item->item != NULL)
        {
          g_signal_connect (G_OBJECT (item->item), "changed",
                            G_CALLBACK (xfce_appfinder_model_item_changed), model);

          desktop_id = garcon_menu_item_get_desktop_id (item->item);
          item->is_bookmark = g_hash_table_lookup (model->bookmarks_hash, desktop_id) != NULL;
          item->frecency = g_hash_table_lookup (model->frecencies_hash, desktop_id);
        }

      /* insert in hash table */
      if (G_LIKELY (item->command != NULL))
        g_hash_table_insert (model->items_hash, item->command, item);
    }
  gtk_tree_path_free (path);

  /* signal new categories */
  if (model->collect_categories != NULL)
    {
      tmp = model->categories;
      model->categories = model->collect_categories;
      model->collect_categories = NULL;

      xfce_appfinder_model_categories_changed (model);

      g_slist_foreach (tmp, (GFunc) (void (*)(void)) g_object_unref, NULL);
      g_slist_free (tmp);
    }

  APPFINDER_DEBUG ("insert idle end");

  return FALSE;
}



static void
xfce_appfinder_model_collect_idle_destroy (gpointer user_data)
{
  XFCE_APPFINDER_MODEL (user_data)->collect_idle_id = 0;
}



static gint
xfce_appfinder_model_item_compare (gconstpointer a,
                                   gconstpointer b)
{
  const ModelItem *item_a = a, *item_b = b;
  const gchar     *name_a, *name_b;

  /* sort custom commands before desktop files */
  if ((item_a->item != NULL) != (item_b->item != NULL))
    return (item_a->item != NULL) ? 1 : -1;

  /* sort desktop entries */
  if (item_a->item != NULL)
    {
      name_a = garcon_menu_item_get_name (item_a->item);
      name_b = garcon_menu_item_get_name (item_b->item);
      return g_utf8_collate (name_a, name_b);
    }

  /* sort custom commands */
  return g_utf8_collate (item_a->command, item_b->command);
}



static gint
xfce_appfinder_model_item_compare_command (gconstpointer a,
                                           gconstpointer b)
{
  const ModelItem *item_a = a, *item_b = b;

  return g_utf8_collate (item_a->command, item_b->command);
}



static gchar *
xfce_appfinder_model_item_key (GarconMenuItem *item)
{
  const gchar *value, *keyword;
  GList       *keywords, *li;
  GString     *str;
  gchar       *normalized;
  gchar       *casefold;
  gchar       *p;

  str = g_string_sized_new (128);

  value = garcon_menu_item_get_name (item);
  if (value != NULL)
    g_string_append (str, value);
  g_string_append_c (str, '\n');

  value = garcon_menu_item_get_command (item);
  if (value != NULL)
    {
      /* only the non-expanding command items */
      p = strchr (value, '%');
      g_string_append_len (str, value, p != NULL ? p - value : -1);
    }
  g_string_append_c (str, '\n');

  value = garcon_menu_item_get_comment (item);
  if (value != NULL)
    g_string_append (str, value);

  value = garcon_menu_item_get_generic_name (item);
  if (value != NULL)
    g_string_append (str, value);

  keywords = garcon_menu_item_get_keywords (item);
  for (li = keywords; li != NULL; li = li->next)
    {
      keyword = li->data;
      if (keyword != NULL) {
        g_string_append (str, keyword);
        g_string_append_c (str, '\n');
      }
    }

  normalized = g_utf8_normalize (str->str, str->len, G_NORMALIZE_ALL);
  casefold = g_utf8_casefold (normalized, -1);
  g_free (normalized);

  g_string_free (str, TRUE);

  return casefold;
}



static ModelItem *
xfce_appfinder_model_item_new (GarconMenuItem *menu_item)
{
  ModelItem   *item;
  const gchar *command, *p;

  appfinder_return_val_if_fail (GARCON_IS_MENU_ITEM (menu_item), NULL);

  item = g_slice_new0 (ModelItem);
  item->item = GARCON_MENU_ITEM (g_object_ref (G_OBJECT (menu_item)));

  appfinder_refcount_debug_add (G_OBJECT (menu_item),
     garcon_menu_item_get_desktop_id (menu_item));

  command = garcon_menu_item_get_command (menu_item);
  if (G_LIKELY (command != NULL))
    {
      p = strstr (command, " %");
      if (p != NULL)
        item->command = g_strndup (command, p - command);
      else
        item->command = g_strdup (command);
    }

  item->key = xfce_appfinder_model_item_key (menu_item);
  item->not_visible = !garcon_menu_element_get_visible (GARCON_MENU_ELEMENT (menu_item));

  return item;
}



static void
xfce_appfinder_model_item_changed (GarconMenuItem     *menu_item,
                                   XfceAppfinderModel *model)
{
  GSList      *li;
  ModelItem   *item;
  gint         idx;
  GtkTreeIter  iter;
  GtkTreePath *path;
  GPtrArray   *categories;
  gboolean     old_not_visible;
  const gchar *desktop_id;

  /* lookup the item in the list */
  for (li = model->items, idx = 0; li != NULL; li = li->next, idx++)
    {
      item = li->data;
      if (item->item == menu_item)
        {
          categories = g_ptr_array_ref (item->categories);
          g_object_ref (G_OBJECT (menu_item));

          APPFINDER_DEBUG ("update item %s", garcon_menu_item_get_desktop_id (menu_item));

          old_not_visible = item->not_visible;

          xfce_appfinder_model_item_free (item, model);

          item = xfce_appfinder_model_item_new (menu_item);
          item->categories = categories;
          li->data = item;

          /* check if the item should be a bookmark */
          desktop_id = garcon_menu_item_get_desktop_id (menu_item);
          if (desktop_id != NULL)
            {
              item->is_bookmark = g_hash_table_lookup (model->bookmarks_hash, desktop_id) != NULL;
              item->frecency = g_hash_table_lookup (model->frecencies_hash, desktop_id);
            }
          else
            item->is_bookmark = FALSE;

          if (G_LIKELY (item->command != NULL))
            g_hash_table_insert (model->items_hash, item->command, item);

          g_signal_connect (G_OBJECT (menu_item), "changed",
                            G_CALLBACK (xfce_appfinder_model_item_changed), model);

          g_object_unref (G_OBJECT (menu_item));

          path = gtk_tree_path_new_from_indices (idx, -1);
          ITER_INIT (iter, model->stamp, li);
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);

          /* update the categories if the visibility changed */
          if (old_not_visible != item->not_visible)
            xfce_appfinder_model_categories_changed (model);

          break;
        }
    }
}



static void
xfce_appfinder_model_item_free (gpointer            data,
                                XfceAppfinderModel *model)
{
  ModelItem *item = data;

  if (item->item != NULL)
    {
      if (model != NULL)
        g_signal_handlers_disconnect_by_func (G_OBJECT (item->item),
            G_CALLBACK (xfce_appfinder_model_item_changed), model);
      g_object_unref (G_OBJECT (item->item));
    }
  if (model != NULL && item->command != NULL)
    g_hash_table_remove (model->items_hash, item->command);
  if (item->icon != NULL)
    g_object_unref (G_OBJECT (item->icon));
  if (item->surface != NULL)
    cairo_surface_destroy (item->surface);
  if (item->surface_large != NULL)
    cairo_surface_destroy (item->surface_large);
  if (item->categories != NULL)
    g_ptr_array_unref (item->categories);
  g_free (item->abstract);
  g_free (item->key);
  g_free (item->command);
  g_free (item->tooltip);
  g_slice_free (ModelItem, item);
}



static void
xfce_appfinder_model_history_remove_items (XfceAppfinderModel *model)
{
  ModelItem   *item;
  GSList      *li, *lnext;
  gint         idx;
  GtkTreePath *path;

  /* no need to remove the items if the stamp is already zero */
  if (model->history_mtime == 0)
    return;

  APPFINDER_DEBUG ("clear history items");

  for (li = model->items, idx = 0; li != NULL; li = lnext, idx++)
    {
      lnext = li->next;
      item = li->data;
      if (item->item != NULL)
        continue;

      model->items = g_slist_delete_link (model->items, li);

      path = gtk_tree_path_new_from_indices (idx--, -1);
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
      gtk_tree_path_free (path);

      xfce_appfinder_model_item_free (item, model);
    }

  /* unset stamp */
  model->history_mtime = 0;
}



static guint64
xfce_appfinder_model_file_get_mtime (GFile *file)
{
  GFileInfo *info;
  guint64    mtime = 0;

  if (G_LIKELY (file != NULL))
    {
      info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                G_FILE_QUERY_INFO_NONE, NULL, NULL);
      if (G_LIKELY (info != NULL))
        {
          mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
          g_object_unref (G_OBJECT (info));
        }
    }

  /* never return 1, because we use that for an empty file */
  return MAX (mtime, 1);
}



static gboolean
xfce_appfinder_model_history_insert (XfceAppfinderModel *model,
                                     const gchar        *command)
{
  ModelItem    *item;
  GtkTreeIter   iter;
  GtkTreePath  *path;
  GSList       *lp;
  guint         idx;

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), FALSE);
  appfinder_return_val_if_fail (IS_STRING (command), FALSE);

  /* add new command */
  item = g_slice_new0 (ModelItem);
  item->command = g_strdup (command);
  if (g_slist_find_custom (model->items, item, xfce_appfinder_model_item_compare_command) != NULL)
    {
       APPFINDER_DEBUG ("Skip adding %s to the model as it's already contained.", command);
       g_slice_free (ModelItem, item);
       return FALSE;
    }
  model->items = g_slist_insert_sorted (model->items, item, xfce_appfinder_model_item_compare);

  /* find the item and the position */
  for (lp = model->items, idx = 0; lp != NULL; lp = lp->next, idx++)
    if (lp->data == item)
      break;
  g_assert (lp != NULL);

  APPFINDER_DEBUG ("insert %s in the model", command);

  /* emit the insert-row signal */
  path = gtk_tree_path_new_from_indices (idx, -1);
  ITER_INIT (iter, model->stamp, lp);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
  gtk_tree_path_free (path);

  g_hash_table_insert (model->items_hash, item->command, item);
  return TRUE;
}



static void
xfce_appfinder_model_history_changed (GFileMonitor       *monitor,
                                      GFile              *file,
                                      GFile              *other_file,
                                      GFileMonitorEvent   event_type,
                                      XfceAppfinderModel *model)
{
  guint64      mtime;
  gchar       *path;
  GError      *error = NULL;
  gchar       *command;
  gchar       *contents;
  gchar       *end;
  GMappedFile *history;

  appfinder_return_if_fail (XFCE_IS_APPFINDER_MODEL (model));
  appfinder_return_if_fail (model->history_monitor == monitor);
  appfinder_return_if_fail (G_IS_FILE_MONITOR (monitor));
  appfinder_return_if_fail (G_IS_FILE (model->history_file));

  switch (event_type)
    {
    case G_FILE_MONITOR_EVENT_DELETED:
      xfce_appfinder_model_history_remove_items (model);
      break;

    case G_FILE_MONITOR_EVENT_CREATED:
      mtime = xfce_appfinder_model_file_get_mtime (model->history_file);
      if (mtime > model->history_mtime)
        {
          /* read the new file and update the commands */
          path = g_file_get_path (file);
          history = g_mapped_file_new (path, FALSE, &error);
          if (G_LIKELY (history != NULL))
            {
              contents = g_mapped_file_get_contents (history);
              if (G_LIKELY (contents != NULL))
                {
                  /* walk the file */
                  for (;;)
                    {
                      end = strchr (contents, '\n');
                      if (G_UNLIKELY (end == NULL))
                        break;

                      if (end != contents)
                        {
                          /* look for new commands */
                          command = g_strndup (contents, end - contents);
                          xfce_appfinder_model_history_insert (model, command);
                          g_free (command);
                        }
                      contents = end + 1;
                    }
                }

              g_mapped_file_unref (history);
            }
          else
            {
              g_warning ("Failed to open history file: %s", error->message);
              g_clear_error (&error);
            }

          model->history_mtime = mtime;
        }
      break;

    default:
      break;
    }
}



static void
xfce_appfinder_model_history_monitor_stop (XfceAppfinderModel *model)
{
  if (model->history_monitor != NULL)
    {
      g_signal_handlers_disconnect_by_func (model->history_monitor,
          G_CALLBACK (xfce_appfinder_model_history_changed), model);

      g_object_unref (G_OBJECT (model->history_monitor));
      model->history_monitor = NULL;
    }

  if (model->history_file != NULL)
    {
      g_object_unref (G_OBJECT (model->history_file));
      model->history_file = NULL;
    }
}



static void
xfce_appfinder_model_history_monitor (XfceAppfinderModel *model,
                                      const gchar        *path)
{
  GFile  *file;
  GError *error = NULL;

  file = g_file_new_for_path (path);

  if (model->history_file == NULL
      || model->history_monitor == NULL
      || !g_file_equal (file, model->history_file))
    {
      xfce_appfinder_model_history_monitor_stop (model);

      /* monitor the file for changes */
      model->history_monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, model->collect_cancelled, &error);
      appfinder_refcount_debug_add (G_OBJECT (model->history_monitor), "history file monitor");
      if (model->history_monitor != NULL)
        {
          APPFINDER_DEBUG ("monitor history file %s", path);

          model->history_file = G_FILE (g_object_ref (G_OBJECT (file)));
          g_signal_connect (G_OBJECT (model->history_monitor), "changed",
              G_CALLBACK (xfce_appfinder_model_history_changed), model);
        }
      else
        {
          g_warning ("Failed to setup a monitor for %s: %s", path, error->message);
          g_error_free (error);
        }
    }

  model->history_mtime = xfce_appfinder_model_file_get_mtime (file);

  g_object_unref (G_OBJECT (file));
}


static void
xfce_appfinder_model_bookmarks_collect (XfceAppfinderModel *model,
                                        GMappedFile        *mmap)
{
  gchar *line;
  gchar *end;
  gchar *contents;

  /* empty the database */
  g_hash_table_remove_all (model->bookmarks_hash);

  contents = g_mapped_file_get_contents (mmap);
  if (contents == NULL)
    return;

  /* walk the file */
  for (;!g_cancellable_is_cancelled (model->collect_cancelled);)
    {
      end = strchr (contents, '\n');
      if (G_UNLIKELY (end == NULL))
        break;

      if (end != contents)
        {
          /* look for new commands */
          line = g_strndup (contents, end - contents);
          g_hash_table_insert (model->bookmarks_hash, line, GUINT_TO_POINTER (1));
        }
      contents = end + 1;
    }
}



static void
xfce_appfinder_model_bookmarks_changed (GFileMonitor       *monitor,
                                        GFile              *file,
                                        GFile              *other_file,
                                        GFileMonitorEvent   event_type,
                                        XfceAppfinderModel *model)
{
  guint64      mtime;
  gchar       *filename;
  GError      *error = NULL;
  GMappedFile *mmap;
  gboolean     is_bookmark;
  ModelItem   *item;
  GSList      *li;
  const gchar *desktop_id;
  gint         idx;
  GtkTreePath *path;
  GtkTreeIter  iter;

  appfinder_return_if_fail (XFCE_IS_APPFINDER_MODEL (model));
  appfinder_return_if_fail (model->bookmarks_monitor == monitor);
  appfinder_return_if_fail (G_IS_FILE_MONITOR (monitor));
  appfinder_return_if_fail (G_IS_FILE (model->bookmarks_file));

  switch (event_type)
    {
    case G_FILE_MONITOR_EVENT_DELETED:
      /* TODO */
      break;

    case G_FILE_MONITOR_EVENT_CREATED:
      mtime = xfce_appfinder_model_file_get_mtime (model->bookmarks_file);
      if (mtime > model->bookmarks_mtime)
        {
          APPFINDER_DEBUG ("bookmarks file changed");

          /* read the new file and update the commands */
          filename = g_file_get_path (file);
          mmap = g_mapped_file_new (filename, FALSE, &error);
          g_free (filename);

          if (G_LIKELY (mmap != NULL))
            {
              xfce_appfinder_model_bookmarks_collect (model, mmap);
              g_mapped_file_unref (mmap);

              /* update the model items */
              for (idx = 0, li = model->items; li != NULL; li = li->next, idx++)
                {
                  item = li->data;
                  if (item->item == NULL)
                    continue;

                  /* check if the item should be a bookmark */
                  desktop_id = garcon_menu_item_get_desktop_id (item->item);
                  if (desktop_id != NULL)
                    is_bookmark = g_hash_table_lookup (model->bookmarks_hash, desktop_id) != NULL;
                  else
                    is_bookmark = FALSE;

                  if (item->is_bookmark != is_bookmark)
                    {
                      APPFINDER_DEBUG ("bookmark %s changed", desktop_id);

                      item->is_bookmark = is_bookmark;

                      /* let model know what happened */
                      path = gtk_tree_path_new_from_indices (idx, -1);
                      ITER_INIT (iter, model->stamp, li);
                      gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
                      gtk_tree_path_free (path);
                    }
                }
            }
        }
      break;

    default:
      break;
    }
}



static void
xfce_appfinder_model_bookmarks_monitor_stop (XfceAppfinderModel *model)
{
  if (model->bookmarks_monitor != NULL)
    {
      g_signal_handlers_disconnect_by_func (model->bookmarks_monitor,
          G_CALLBACK (xfce_appfinder_model_bookmarks_changed), model);

      g_object_unref (G_OBJECT (model->bookmarks_monitor));
      model->bookmarks_monitor = NULL;
    }

  if (model->bookmarks_file != NULL)
    {
      g_object_unref (G_OBJECT (model->bookmarks_file));
      model->bookmarks_file = NULL;
    }
}



static void
xfce_appfinder_model_bookmarks_monitor (XfceAppfinderModel *model,
                                        const gchar        *path)
{
  GFile  *file;
  GError *error = NULL;

  file = g_file_new_for_path (path);

  if (model->bookmarks_file == NULL
      || model->bookmarks_monitor == NULL
      || !g_file_equal (file, model->bookmarks_file))
    {
      xfce_appfinder_model_bookmarks_monitor_stop (model);

      /* monitor the file for changes */
      model->bookmarks_monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, model->collect_cancelled, &error);
      appfinder_refcount_debug_add (G_OBJECT (model->bookmarks_monitor), "bookmarks file monitor");
      if (model->bookmarks_monitor != NULL)
        {
          APPFINDER_DEBUG ("monitor bookmarks file %s", path);

          model->bookmarks_file = G_FILE (g_object_ref (G_OBJECT (file)));
          g_signal_connect (G_OBJECT (model->bookmarks_monitor), "changed",
              G_CALLBACK (xfce_appfinder_model_bookmarks_changed), model);
        }
      else
        {
          g_warning ("Failed to setup a monitor for %s: %s", path, error->message);
          g_error_free (error);
        }
    }

  model->bookmarks_mtime = xfce_appfinder_model_file_get_mtime (file);

  g_object_unref (G_OBJECT (file));
}



static gint
xfce_appfinder_model_category_compare (gconstpointer a,
                                       gconstpointer b)
{
  appfinder_return_val_if_fail (GARCON_IS_MENU_DIRECTORY (a), 0);
  appfinder_return_val_if_fail (GARCON_IS_MENU_DIRECTORY (b), 0);

  return g_utf8_collate (garcon_menu_directory_get_name (GARCON_MENU_DIRECTORY (a)),
                         garcon_menu_directory_get_name (GARCON_MENU_DIRECTORY (b)));
}



static inline gboolean
xfce_appfinder_model_ptr_array_find (GPtrArray     *array,
                                     gconstpointer  data)
{
  guint i;

  if (array != NULL && data != NULL)
    for (i = 0; i < array->len; i++)
      if (g_ptr_array_index (array, i) == data)
        return TRUE;

  return FALSE;
}



static void
xfce_appfinder_model_collect_history (XfceAppfinderModel *model,
                                      GMappedFile        *history)
{
  gchar     *contents;
  gchar     *end;
  ModelItem *item;

  contents = g_mapped_file_get_contents (history);
  if (contents == NULL)
    return;

  for (;!g_cancellable_is_cancelled (model->collect_cancelled);)
    {
      end = strchr (contents, '\n');
      if (G_UNLIKELY (end == NULL))
        break;

      if (end != contents)
        {
          item = g_slice_new0 (ModelItem);
          item->command = g_strndup (contents, end - contents);
          model->collect_items = g_slist_prepend (model->collect_items, item);
        }

      contents = end + 1;
    }
}



static void
xfce_appfinder_model_collect_item (const gchar    *desktop_id,
                                   GarconMenuItem *menu_item,
                                   CollectContext *context)
{
  ModelItem *item;

  appfinder_return_if_fail (GARCON_IS_MENU_ITEM (menu_item));
  appfinder_return_if_fail (desktop_id != NULL);

  /* check if we alread have the item */
  item = g_hash_table_lookup (context->desktop_ids, desktop_id);
  if (G_LIKELY (item == NULL))
    {
      /* ignore menu items without name */
      if (garcon_menu_item_get_name (menu_item) == NULL)
        return;

      item = xfce_appfinder_model_item_new (menu_item);

      item->categories = g_ptr_array_new_with_free_func (g_object_unref);
      if (context->category != NULL)
        {
          g_ptr_array_add (item->categories,
                           g_object_ref (G_OBJECT (context->category)));
        }

      context->items = g_slist_prepend (context->items, item);
      g_hash_table_insert (context->desktop_ids, (gchar *) desktop_id, item);
    }
  else if (context->category != NULL
           && !xfce_appfinder_model_ptr_array_find (item->categories, context->category))
    {
      /* add category to existing item */
      g_ptr_array_add (item->categories, g_object_ref (G_OBJECT (context->category)));
      APPFINDER_DEBUG ("%s is in %d categories", desktop_id, item->categories->len);
    }
}



static void
xfce_appfinder_model_directory_changed (GarconMenu *menu)
{
  GarconMenu *parent;

  appfinder_return_if_fail (GARCON_IS_MENU (menu));

  while (menu != NULL)
    {
      parent = garcon_menu_get_parent (menu);
      if (parent == NULL)
        g_signal_emit_by_name (menu, "reload-required");
      menu = parent;
    }
}



static gboolean
xfce_appfinder_model_collect_items (GarconMenu           *menu,
                                    GCancellable         *cancelled,
                                    GarconMenuDirectory  *category,
                                    GSList              **items,
                                    GSList              **categories,
                                    GHashTable           *desktop_ids)
{
  GList               *menus, *li;
  GarconMenuDirectory *directory;
  gboolean             has_items = FALSE;
  GarconMenuItemPool  *pool;
  CollectContext      *context;

  appfinder_return_val_if_fail (GARCON_IS_MENU (menu), FALSE);
  appfinder_return_val_if_fail (cancelled == NULL || G_IS_CANCELLABLE (cancelled), FALSE);
  appfinder_return_val_if_fail (category == NULL || GARCON_IS_MENU_DIRECTORY (category), FALSE);
  appfinder_return_val_if_fail (items != NULL, FALSE);
  appfinder_return_val_if_fail (categories != NULL, FALSE);
  appfinder_return_val_if_fail (desktop_ids != NULL, FALSE);

  if (g_cancellable_is_cancelled (cancelled))
    return FALSE;

  directory = garcon_menu_get_directory (menu);
  if (directory != NULL)
    {
      appfinder_refcount_debug_add (G_OBJECT (directory),
          garcon_menu_directory_get_name (directory));

      if (!garcon_menu_directory_get_visible (directory))
        return FALSE;

      /* this way we only have 1 level of categories, but
       * skip the directory of the root menu because in some
       * menu files (gnome-menus for example) this puts
       * everything under "Applications" */
      if (category == NULL
          && garcon_menu_get_parent (menu) != NULL)
        category = directory;
    }

  g_signal_connect (G_OBJECT (menu), "directory-changed",
      G_CALLBACK (xfce_appfinder_model_directory_changed), NULL);

  /* collect all the items in the menu's pool. we walk
   * the pool directory to avoid the sorting of items
   * that is done within garcon for _get_elements and
   * _get_items */
  pool = garcon_menu_get_item_pool (menu);
  if (G_LIKELY (pool != NULL))
    {
      context = g_slice_new (CollectContext);
      context->desktop_ids = desktop_ids;
      context->category = category;
      context->items = NULL;

      garcon_menu_item_pool_foreach (pool, (GHFunc)
          xfce_appfinder_model_collect_item, context);

      if (context->items != NULL)
        {
          has_items = TRUE;
          *items = g_slist_concat (*items, context->items);
        }

      g_slice_free (CollectContext, context);
    }


  /* also collect items in the submenus */
  menus = garcon_menu_get_menus (menu);
  for (li = menus; li != NULL; li = li->next)
    {
      appfinder_refcount_debug_add (G_OBJECT (li->data),
          garcon_menu_element_get_name (li->data));

      if (xfce_appfinder_model_collect_items (li->data, cancelled, category, items,
                                              categories, desktop_ids))
        {
          has_items = TRUE;
        }
    }
  g_list_free (menus);

  if (directory != NULL && has_items)
    *categories = g_slist_prepend (*categories, g_object_ref (G_OBJECT (directory)));

  return has_items;
}



static void
xfce_appfinder_model_collect_menu (GarconMenu    *menu,
                                   GCancellable  *cancelled,
                                   GSList       **items,
                                   GSList       **categories)
{
  GHashTable *desktop_ids;

  desktop_ids = g_hash_table_new (g_str_hash, g_str_equal);
  xfce_appfinder_model_collect_items (menu, cancelled, NULL, items,
                                      categories, desktop_ids);
  g_hash_table_destroy (desktop_ids);
}



static void
xfce_appfinder_model_menu_changed_remove (gpointer key,
                                          gpointer value,
                                          gpointer user_data)
{
  XfceAppfinderModel *model = XFCE_APPFINDER_MODEL (user_data);
  GSList             *li = value;
  GtkTreePath        *path;
  gint                position;

  APPFINDER_DEBUG ("remove %s from the model", (gchar *) key);

  position = g_slist_position (model->items, li);
  xfce_appfinder_model_item_free (li->data, model);
  model->items = g_slist_delete_link (model->items, li);

  path = gtk_tree_path_new_from_indices (position, -1);
  gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
  gtk_tree_path_free (path);
}



static gboolean
xfce_appfinder_model_menu_changed_idle (gpointer data)
{
  XfceAppfinderModel *model = XFCE_APPFINDER_MODEL (data);
  GarconMenu         *menu = model->menu;
  GSList             *li, *lp;
  GHashTable         *old_items;
  ModelItem          *item;
  ModelItem          *old_item;
  const gchar        *desktop_id;
  GSList             *collect_items = NULL;
  GSList             *collect_categories = NULL;
  GSList             *tmp;
  guint               idx;
  GtkTreeIter         iter;
  GtkTreePath        *path;
  GSList             *old_li;
  GError             *error = NULL;

  appfinder_return_val_if_fail (GARCON_IS_MENU (menu), FALSE);
  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), FALSE);

  APPFINDER_DEBUG ("menu changed");

  /* add all the old items in a garcon database */
  old_items = g_hash_table_new (g_str_hash, g_str_equal);
  for (li = model->items; li != NULL; li = li->next)
    {
      item = li->data;
      if (item->item != NULL)
        {
          desktop_id = garcon_menu_item_get_desktop_id (item->item);
          g_hash_table_insert (old_items, (gchar *) desktop_id, li);
        }
    }

  if (!garcon_menu_load (model->menu, NULL, &error))
    {
      g_warning ("Failed to reload the root menu: %s", error->message);
      g_error_free (error);

      model->menu_changed_idle_id = 0;

      return FALSE;
    }

  xfce_appfinder_model_collect_menu (menu, NULL, &collect_items, &collect_categories);

  for (li = collect_items; li != NULL; li = li->next)
    {
      item = li->data;

      g_assert (GARCON_IS_MENU_ITEM (item->item));

      desktop_id = garcon_menu_item_get_desktop_id (item->item);

      /* check if desktop id already exists */
      old_li = g_hash_table_lookup (old_items, desktop_id);
      if (old_li != NULL)
        {
          /* check if the file location is still equal */
          old_item = old_li->data;
          if (!garcon_menu_element_equal (GARCON_MENU_ELEMENT (old_item->item),
                                          GARCON_MENU_ELEMENT (item->item)))
            {
              APPFINDER_DEBUG ("%s file location changed", desktop_id);

              /* do not remove from the hash table and insert the new item */
              goto insert;
            }

          /* remove from the table */
          g_hash_table_remove (old_items, desktop_id);

          /* steal the categories */
          g_ptr_array_unref (old_item->categories);
          old_item->categories = item->categories;
          item->categories = NULL;

          /* this is not interesting, since those items are also updated
           * by the GarconMenuItem::changed signal, so don't touch the
           * item in the model */
          xfce_appfinder_model_item_free (item, NULL);
        }
      else
        {
          insert:

          /* insert new item in the list */
          model->items = g_slist_insert_sorted (model->items, item, xfce_appfinder_model_item_compare);

          /* find the item and the position */
          for (lp = model->items, idx = 0; lp != NULL; lp = lp->next, idx++)
            if (lp->data == item)
              break;
          g_assert (lp != NULL);

          APPFINDER_DEBUG ("insert %s in the model", desktop_id);

          /* emit the insert-row signal */
          path = gtk_tree_path_new_from_indices (idx, -1);
          ITER_INIT (iter, model->stamp, lp);
          gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);

          /* watch and add the command */
          g_signal_connect (G_OBJECT (item->item), "changed",
              G_CALLBACK (xfce_appfinder_model_item_changed), model);

          if (G_LIKELY (item->command != NULL))
              g_hash_table_insert (model->items_hash, item->command, item);
        }
    }

  g_slist_free (collect_items);

  /* remove remaining items from the model */
  g_hash_table_foreach (old_items, xfce_appfinder_model_menu_changed_remove, model);
  g_hash_table_destroy (old_items);

  /* update the new categories */
  collect_categories = g_slist_sort (collect_categories, xfce_appfinder_model_category_compare);

  /* swap lists */
  tmp = model->categories;
  model->categories = collect_categories;

  /* always update the categories, because the pointers changed */
  xfce_appfinder_model_categories_changed (model);

  g_slist_foreach (tmp, (GFunc) (void (*)(void)) g_object_unref, NULL);
  g_slist_free (tmp);

  model->menu_changed_idle_id = 0;

  return FALSE;
}



static void
xfce_appfinder_model_menu_changed (GarconMenu         *menu,
                                   XfceAppfinderModel *model)
{
  appfinder_return_if_fail (GARCON_IS_MENU (menu));
  appfinder_return_if_fail (XFCE_IS_APPFINDER_MODEL (model));
  appfinder_return_if_fail (model->menu == menu);

  if (model->menu_changed_idle_id == 0)
    model->menu_changed_idle_id = g_timeout_add (1000, xfce_appfinder_model_menu_changed_idle, model);
}



static gpointer
xfce_appfinder_model_collect_thread (gpointer user_data)
{
  XfceAppfinderModel *model = XFCE_APPFINDER_MODEL (user_data);
  GError             *error = NULL;
  gchar              *filename;
  GMappedFile        *mmap;

  appfinder_return_val_if_fail (GARCON_IS_MENU (model->menu), NULL);
  appfinder_return_val_if_fail (model->collect_items == NULL, NULL);
  appfinder_return_val_if_fail (model->collect_categories == NULL, NULL);

  APPFINDER_DEBUG ("collect thread start");

  /* load menu data */
  if (G_LIKELY (model->menu != NULL))
    {
      if (garcon_menu_load (model->menu, model->collect_cancelled, &error))
        {
          xfce_appfinder_model_collect_menu (model->menu,
                                             model->collect_cancelled,
                                             &model->collect_items,
                                             &model->collect_categories);
        }
      else
        {
          g_warning ("Failed to load the root menu: %s", error->message);
          g_clear_error (&error);
        }
    }

  xfce_appfinder_model_migrate_history_file ();

  /* load command history */
  filename = xfce_resource_lookup (XFCE_RESOURCE_CACHE, NEW_HISTORY_PATH);
  if (G_LIKELY (filename != NULL))
    {
      APPFINDER_DEBUG ("load commands from %s", filename);

      mmap = g_mapped_file_new (filename, FALSE, &error);
      if (G_LIKELY (mmap != NULL))
        {
          xfce_appfinder_model_collect_history (model, mmap);
          g_mapped_file_unref (mmap);
        }
      else
        {
          g_warning ("Failed to open history file: %s", error->message);
          g_clear_error (&error);
        }

      /* start monitoring and update mtime */
      xfce_appfinder_model_history_monitor (model, filename);

      g_free (filename);
    }

  /* load bookmarks */
  filename = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, BOOKMARKS_PATH);
  if (G_LIKELY (filename != NULL))
    {
      APPFINDER_DEBUG ("load bookmarks from %s", filename);

      mmap = g_mapped_file_new (filename, FALSE, &error);
      if (G_LIKELY (mmap != NULL))
        {
          xfce_appfinder_model_bookmarks_collect (model, mmap);
          g_mapped_file_unref (mmap);
        }
      else
        {
          g_warning ("Failed to open bookmarks file: %s", error->message);
          g_clear_error (&error);
        }

      /* start monitoring and update mtime */
      xfce_appfinder_model_bookmarks_monitor (model, filename);

      g_free (filename);
    }

  /* load frecencies */
  filename = xfce_resource_lookup (XFCE_RESOURCE_CACHE, FRECENCY_PATH);
  if (G_LIKELY (filename != NULL))
    {
      APPFINDER_DEBUG ("load frecencies from %s", filename);

      mmap = g_mapped_file_new (filename, FALSE, &error);
      if (G_LIKELY (mmap != NULL))
        {
          xfce_appfinder_model_frecency_collect (model, mmap);
          g_mapped_file_unref (mmap);
        }
      else
        {
          g_warning ("Failed to open frecencies file: %s", error->message);
          g_clear_error (&error);
        }

      g_free (filename);
    }

  if (model->collect_items != NULL
      && !g_cancellable_is_cancelled (model->collect_cancelled))
    {
      if (model->sort_by_frecency)
        model->collect_items = g_slist_sort_with_data (model->collect_items, xfce_appfinder_model_item_compare_frecency, model);
      else
        model->collect_items = g_slist_sort (model->collect_items, xfce_appfinder_model_item_compare);

      model->collect_categories = g_slist_sort (model->collect_categories, xfce_appfinder_model_category_compare);

      model->collect_idle_id = gdk_threads_add_idle_full (G_PRIORITY_LOW, xfce_appfinder_model_collect_idle,
                                                model, xfce_appfinder_model_collect_idle_destroy);
    }

  if (G_LIKELY (model->menu != NULL))
    {
      g_signal_connect (G_OBJECT (model->menu), "reload-required",
                        G_CALLBACK (xfce_appfinder_model_menu_changed), model);
    }

  APPFINDER_DEBUG ("collect thread end");

  return NULL;
}



static gint
xfce_appfinder_model_item_compare_frecency (gconstpointer a,
                                            gconstpointer b,
                                            gpointer      data)
{
  const ModelItem    *item_a = a, *item_b = b;
  const gchar        *desktop_id_a, *desktop_id_b;
  XfceAppfinderModel *model;
  Frecency           *frecency_a, *frecency_b;
  guint               freq_a, freq_b, res_a, res_b;
  guint64             rec_a, rec_b;

  if (item_a->item != NULL && item_b->item != NULL)
    {
      desktop_id_a = garcon_menu_item_get_desktop_id (item_a->item);
      desktop_id_b = garcon_menu_item_get_desktop_id (item_b->item);

      if (desktop_id_a != NULL && desktop_id_b != NULL)
        {
          model = (XfceAppfinderModel *) data;

          frecency_a = g_hash_table_lookup (model->frecencies_hash, desktop_id_a);
          frecency_b = g_hash_table_lookup (model->frecencies_hash, desktop_id_b);

          freq_a = frecency_a ? frecency_a->frequency : 0;
          freq_b = frecency_b ? frecency_b->frequency : 0;

          rec_a = frecency_a ? frecency_a->recency : 0;
          rec_b = frecency_b ? frecency_b->recency : 0;

          res_a = xfce_appfinder_model_calculate_frecency (freq_a, rec_a);
          res_b = xfce_appfinder_model_calculate_frecency (freq_b, rec_b);

          if (res_b - res_a != 0)
            return res_b - res_a;
        }
    }

  /* fallback to alphabetical order if desktop files are invalid
   * or items have the same frecency */
  return xfce_appfinder_model_item_compare (a, b);
}



static void
xfce_appfinder_model_frecency_collect (XfceAppfinderModel  *model,
                                       GMappedFile         *mmap)
{
  gchar       *line;
  gchar      **tokens;
  gchar       *end;
  gchar       *contents;
  Frecency    *frecency;

  contents = g_mapped_file_get_contents (mmap);
  if (contents == NULL)
    return;

  /* walk the file */
  for (; !g_cancellable_is_cancelled (model->collect_cancelled);)
    {
      end = strchr (contents, '\n');
      if (G_UNLIKELY (end == NULL))
        break;

      if (end != contents)
        {
          line = g_strndup (contents, end - contents);
          tokens = g_strsplit (line, ":", 3);
          if (g_strv_length (tokens) == 3 && tokens[0] != NULL && tokens[1] != NULL && tokens[2] != NULL)
            {
              frecency = g_slice_new0 (Frecency);
              frecency->frequency = g_ascii_strtoull (tokens[1], NULL, 0);
              frecency->recency = g_ascii_strtoull (tokens[2], NULL, 0);
              g_hash_table_insert (model->frecencies_hash, g_strdup (tokens[0]), frecency);
            }
          g_free (line);
          g_strfreev (tokens);
        }
      contents = end + 1;
    }
}



static void
xfce_appfinder_model_frecency_free (gpointer data)
{
  g_slice_free (Frecency, data);
}



/* Remove this function when libxfce4util minimal required version is bumped to 4.18 */
static gchar*
xfce_appfinder_model_unescape_entry_value (const gchar *value)
{
  const gchar *p;
  GString     *string;

  if (G_UNLIKELY (value == NULL))
    return NULL;

  string = g_string_sized_new (strlen (value));

  for (p = value; *p != '\0'; ++p)
    {
      if (G_UNLIKELY (p[0] == '\\' && p[1] != '\0'))
        {
          switch (*++p)
            {
            case 's':
              g_string_append_c (string, ' ');
              break;

            case '\\':
              g_string_append_c (string, '\\');
              break;
            }
        }
      else
        {
          g_string_append_c (string, *p);
        }
    }

  return g_string_free (string, FALSE);
}



/* Migration for history cache file, can be removed during the 4.20 cycle */
static void
xfce_appfinder_model_migrate_history_file (void)
{
  gchar  *filename;
  GFile  *old_file, *new_file, *dir;
  GError *error = NULL;

  filename = xfce_resource_lookup (XFCE_RESOURCE_CACHE, OLD_HISTORY_PATH);

  if (filename == NULL)
    return;

  old_file = g_file_new_for_path (filename);
  g_free (filename);

  new_file = g_file_resolve_relative_path (old_file, "../../../" NEW_HISTORY_PATH);

  dir = g_file_get_parent (new_file);
  g_file_make_directory_with_parents (dir, NULL, NULL);
  g_object_unref (dir);

  if (g_file_query_exists (new_file, NULL))
    g_warning ("Old and new history files exist, migration aborted");
  else if (g_file_move (old_file, new_file, G_FILE_COPY_NONE, NULL, NULL, NULL, &error))
    g_message ("Migration of history file was successful");
  else
    {
      g_warning ("Failed to migrate history file: %s", error->message);
      g_clear_error (&error);
    }

  g_object_unref (old_file);
  g_object_unref (new_file);
}



guint
xfce_appfinder_model_calculate_frecency (guint   frequency,
                                         guint64 recency)
{
  GDateTime *date_time_now;
  guint64    unix_time_now;
  guint64    diff;

  /* get the current timestamp */
  date_time_now = g_date_time_new_now_local ();
  unix_time_now = g_date_time_to_unix (date_time_now);
  g_date_time_unref (date_time_now);

  diff = unix_time_now - recency;

  /* return frecency according to the frequency and how recent the item is */
  if (diff < 3600) return frequency * 4;
  if (diff < 86400) return frequency * 2;
  if (diff < 604800) return frequency / 2;
  return frequency / 4;
}



void
xfce_appfinder_model_update_frecency (XfceAppfinderModel *model,
                                      const gchar        *desktop_id,
                                      GError            **error)
{
  ModelItem    *item;
  GSList       *li;
  const gchar  *item_desktop_id;
  GString      *frecency_contents;
  gchar        *filename;
  GtkTreePath  *path;
  gint          idx;
  GtkTreeIter   iter;
  GDateTime    *now;

  appfinder_return_if_fail (XFCE_IS_APPFINDER_MODEL (model));
  appfinder_return_if_fail (error == NULL || *error == NULL);
  appfinder_return_if_fail (desktop_id != NULL);

  frecency_contents = g_string_sized_new (0);

  /* update the model items */
  for (idx = 0, li = model->items; li != NULL; li = li->next, idx++)
    {
      item = li->data;
      if (item->item == NULL)
        continue;

      item_desktop_id = garcon_menu_item_get_desktop_id (item->item);
      if (item_desktop_id == NULL)
        continue;

      if (item->frecency == NULL)
        item->frecency = g_hash_table_lookup (model->frecencies_hash, item_desktop_id);

      if (item->frecency == NULL)
        {
          item->frecency = g_slice_new0 (Frecency);
          item->frecency->frequency = 0;
          item->frecency->recency = 0;
          g_hash_table_insert (model->frecencies_hash, g_strdup (item_desktop_id), item->frecency);
        }

      /* find the item we're trying to add/remove */
      if (desktop_id != NULL && g_strcmp0 (item_desktop_id, desktop_id) == 0)
        {
          /* update frequency */
          item->frecency->frequency++;

          /* update recency */
          now = g_date_time_new_now_local ();
          item->frecency->recency = g_date_time_to_unix (now);
          g_date_time_unref (now);

          /* stop searching, continue collecting */
          desktop_id = NULL;

          /* update model */
          path = gtk_tree_path_new_from_indices (idx, -1);
          ITER_INIT (iter, model->stamp, li);
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
        }

      /* collect items' frecency */
      g_string_append (frecency_contents, g_strdup_printf (
        "%s:%" G_GUINT32_FORMAT ":%" G_GUINT64_FORMAT,
        item_desktop_id,
        item->frecency->frequency,
        item->frecency->recency));
      g_string_append_c (frecency_contents, '\n');
    }

  APPFINDER_DEBUG ("saving frecencies");

  /* write new frecencies */
  filename = xfce_resource_save_location (XFCE_RESOURCE_CACHE, FRECENCY_PATH, TRUE);
  if (G_LIKELY (filename != NULL))
    g_file_set_contents (filename, frecency_contents->str, frecency_contents->len, NULL);
  else
    g_set_error_literal (error, 0, 0, "Unable to create frecencies file");

  g_free (filename);
  g_string_free (frecency_contents, TRUE);
}



XfceAppfinderModel *
xfce_appfinder_model_get (gboolean sort_by_frecency,
                          gint     scale_factor)
{
  static XfceAppfinderModel *model = NULL;

  if (G_LIKELY (model != NULL))
    {
      g_object_ref (G_OBJECT (model));
    }
  else
    {
      model = g_object_new (XFCE_TYPE_APPFINDER_MODEL,
                            "sort-by-frecency", sort_by_frecency,
                            "scale-factor", scale_factor,
                            NULL);

      xfce_appfinder_model_icon_theme_changed (model);

      /* only start loading data once model is fully initialized */
      model->collect_thread = g_thread_new ("Collector", xfce_appfinder_model_collect_thread, model);

      g_object_add_weak_pointer (G_OBJECT (model), (gpointer) &model);
      appfinder_refcount_debug_add (G_OBJECT (model), "appfinder-model");
      APPFINDER_DEBUG ("allocate new model");
    }

  return model;
}



GSList *
xfce_appfinder_model_get_categories (XfceAppfinderModel *model)
{
  GSList              *categories = NULL;
  GSList              *li, *lp;
  gboolean             visible;
  ModelItem           *item;
  GarconMenuDirectory *category;

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), NULL);

  for (li = model->categories; li != NULL; li = li->next)
    {
      category = li->data;
      appfinder_assert (GARCON_IS_MENU_DIRECTORY (category));
      visible = FALSE;

      /* check if the category has a visible item */
      for (lp = model->items; lp != NULL; lp = lp->next)
        {
          item = lp->data;
          if (!item->not_visible
              && xfce_appfinder_model_ptr_array_find (item->categories, category))
            {
              visible = TRUE;
              break;
            }
        }

      if (!visible)
        continue;

      categories = g_slist_prepend (categories, category);
    }

  /* we return in reversed order here */
  return categories;
}



gboolean
xfce_appfinder_model_get_visible (XfceAppfinderModel        *model,
                                  const GtkTreeIter         *iter,
                                  const GarconMenuDirectory *category,
                                  const gchar               *string,
                                  const gchar               *string_casefold)
{
  ModelItem *item;
  GarconMenuDirectory *bookmarks;
  gboolean             in_category;

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), FALSE);
  appfinder_return_val_if_fail (iter->stamp == model->stamp, FALSE);
  appfinder_return_val_if_fail (category == NULL || GARCON_IS_MENU_DIRECTORY (category), FALSE);
  appfinder_return_val_if_fail (GARCON_IS_MENU_DIRECTORY (model->command_category), FALSE);

  item = ITER_GET_DATA (iter);

  if (item->item != NULL)
    {
      appfinder_return_val_if_fail (GARCON_IS_MENU_ITEM (item->item), FALSE);

      if (item->not_visible)
        return FALSE;

      if (category != NULL)
        {
          if (!xfce_appfinder_model_ptr_array_find (item->categories, category))
            {
              in_category = FALSE;
              if (item->is_bookmark)
                {
                  bookmarks = xfce_appfinder_model_get_bookmarks_category ();
                  in_category = (bookmarks == category);
                  g_object_unref (G_OBJECT (bookmarks));
                }

              if (!in_category)
                return FALSE;
            }
        }

      if (string_casefold != NULL && item->key != NULL)
        return xfce_appfinder_model_fuzzy_match (item->key, string_casefold);
    }
  else /* command item */
    {
      appfinder_return_val_if_fail (item->command != NULL, FALSE);

      if (category != model->command_category)
        return FALSE;

      if (string != NULL)
        return xfce_appfinder_model_fuzzy_match (item->command, string);
    }

  return TRUE;
}



gboolean
xfce_appfinder_model_get_visible_command (XfceAppfinderModel *model,
                                          const GtkTreeIter  *iter,
                                          const gchar        *string)
{
  ModelItem *item;

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), FALSE);
  appfinder_return_val_if_fail (iter->stamp == model->stamp, FALSE);

  item = ITER_GET_DATA (iter);

  if (item->item != NULL)
    {
      appfinder_return_val_if_fail (GARCON_IS_MENU_ITEM (item->item), FALSE);

      if (item->not_visible)
        return FALSE;
    }

  if (item->command != NULL && string != NULL)
    return xfce_appfinder_model_fuzzy_match (item->command, string);

  return FALSE;
}



gboolean
xfce_appfinder_model_execute (XfceAppfinderModel  *model,
                              const GtkTreeIter   *iter,
                              GdkScreen           *screen,
                              gboolean            *is_regular_command,
                              GError             **error)
{
  const gchar     *icon;
  gchar           *command, *escaped_command, *uri;
  GarconMenuItem  *item;
  ModelItem       *mitem;
  gboolean         succeed = FALSE;
  gchar          **argv, **envp = NULL;

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), FALSE);
  appfinder_return_val_if_fail (iter->stamp == model->stamp, FALSE);
  appfinder_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  mitem = ITER_GET_DATA (iter);
  item = mitem->item;

  /* leave if this is not a menu item */
  *is_regular_command = (item == NULL);
  if (item == NULL)
    return FALSE;

  appfinder_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);

  command = (gchar*) garcon_menu_item_get_command (item);
  if (!IS_STRING (command))
    {
      g_set_error_literal (error, 0, 0, _("Application has no command"));
      return FALSE;
    }

  /* expand the field codes */
  icon = garcon_menu_item_get_icon_name (item);
  uri = garcon_menu_item_get_uri (item);
  command = xfce_expand_desktop_entry_field_codes (command, NULL, icon,
                                                   garcon_menu_item_get_name (item),
                                                   uri,
                                                   garcon_menu_item_requires_terminal (item));
  g_free (uri);

#if LIBXFCE4UTIL_CHECK_VERSION (4, 18, 0)
  escaped_command = xfce_unescape_desktop_entry_value (command);
#else
  escaped_command = xfce_appfinder_model_unescape_entry_value (command);
#endif
  g_free (command);
  command = escaped_command;
  escaped_command = NULL;

#if GARCON_CHECK_VERSION (4, 17, 0)
  if (garcon_menu_item_get_prefers_non_default_gpu (item))
    {
      envp = g_get_environ ();
      envp = g_environ_setenv (envp, "__NV_PRIME_RENDER_OFFLOAD", "1", TRUE);
      envp = g_environ_setenv (envp, "__GLX_VENDOR_LIBRARY_NAME", "nvidia", TRUE);
      envp = g_environ_setenv (envp, "__VK_LAYER_NV_optimus", "NVIDIA_only", TRUE);
    }
#endif

  if (g_shell_parse_argv (command, NULL, &argv, error))
    {
#if LIBXFCE4UI_CHECK_VERSION (4, 15, 6)
      succeed = xfce_spawn (screen,
                            garcon_menu_item_get_path (item),
                            argv, envp, G_SPAWN_SEARCH_PATH,
                            garcon_menu_item_supports_startup_notification (item),
                            gtk_get_current_event_time (),
                            icon, TRUE, error);
#else
      succeed = xfce_spawn_on_screen (screen,
                                      garcon_menu_item_get_path (item),
                                      argv, envp, G_SPAWN_SEARCH_PATH,
                                      garcon_menu_item_supports_startup_notification (item),
                                      gtk_get_current_event_time (),
                                      icon, error);
#endif

      g_strfreev (argv);
    }

  g_free (command);
  g_strfreev (envp);

  return succeed;
}



GdkPixbuf *
xfce_appfinder_model_load_pixbuf (const gchar           *icon_name,
                                  XfceAppfinderIconSize  icon_size,
                                  gint                   scale_factor)
{
  GdkPixbuf    *pixbuf = NULL;
  GdkPixbuf    *scaled;
  gchar        *p, *name;
  GtkIconTheme *icon_theme;
  gint          size;

  switch (icon_size)
    {
    case XFCE_APPFINDER_ICON_SIZE_SMALLEST: size = 16;  break;
    case XFCE_APPFINDER_ICON_SIZE_SMALLER:  size = 24;  break;
    case XFCE_APPFINDER_ICON_SIZE_SMALL:    size = 36;  break;
    case XFCE_APPFINDER_ICON_SIZE_NORMAL:   size = 48;  break;
    case XFCE_APPFINDER_ICON_SIZE_LARGE:    size = 64;  break;
    case XFCE_APPFINDER_ICON_SIZE_LARGER:   size = 96;  break;
    case XFCE_APPFINDER_ICON_SIZE_LARGEST:  size = 128; break;
    default: return NULL;
    }

  APPFINDER_DEBUG ("load icon %s at %dpx and scale %dx", icon_name, size, scale_factor);

  size *= scale_factor;

  if (icon_name != NULL)
    {
      if (g_path_is_absolute (icon_name))
        {
          pixbuf = gdk_pixbuf_new_from_file_at_scale (icon_name, size, size, TRUE, NULL);
        }
      else
        {
          icon_theme = gtk_icon_theme_get_default ();
          pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name, size,
                                             GTK_ICON_LOOKUP_FORCE_SIZE, NULL);

          if (pixbuf == NULL)
            {
              p = strrchr (icon_name, '.');
              if (p != NULL)
                {
                  /* strip extension and try for theme icon */
                  name = g_strndup (icon_name, p - icon_name);
                  APPFINDER_DEBUG ("  try icon as %s", name);
                  pixbuf = gtk_icon_theme_load_icon (icon_theme, name, size, 0, NULL);
                  g_free (name);

                  if (pixbuf == NULL)
                    {
                      /* maybe the . means a file in the pixmaps folder */
                      p = g_build_filename ("pixmaps", icon_name, NULL);
                      name = xfce_resource_lookup (XFCE_RESOURCE_DATA, p);
                      g_free (p);

                      if (name != NULL)
                        {
                          APPFINDER_DEBUG ("  try icon as %s", name);
                          pixbuf = gdk_pixbuf_new_from_file_at_scale (name, size, size, TRUE, NULL);
                          g_free (name);
                        }
                    }
                }
            }
        }
    }

  if (G_UNLIKELY (pixbuf == NULL))
    {
      pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                         "applications-other",
                                         size, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
    }

  if (pixbuf != NULL
      && (gdk_pixbuf_get_width (pixbuf) > size
          || gdk_pixbuf_get_height (pixbuf) > size))
    {
      scaled = gdk_pixbuf_scale_simple (pixbuf, size, size, GDK_INTERP_BILINEAR);
      g_object_unref (G_OBJECT (pixbuf));
      pixbuf = scaled;
    }

  appfinder_refcount_debug_add (G_OBJECT (pixbuf), icon_name);

  return pixbuf;
}



gboolean
xfce_appfinder_model_save_command (XfceAppfinderModel  *model,
                                   const gchar         *command,
                                   GError             **error)
{
  GSList       *li;
  GString      *contents;
  gboolean      succeed = FALSE;
  gchar        *filename;
  ModelItem    *item;
  static gsize  old_len = 0;

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), FALSE);
  appfinder_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!IS_STRING (command))
    return TRUE;

  /* add command to the model history if unique, else just return */
  if (!xfce_appfinder_model_history_insert (model, command))
    return TRUE;

  /* add to the hashtable */
  APPFINDER_DEBUG ("saving history");

  /* store all the custom commands */
  contents = g_string_sized_new (old_len + strlen (command) + 1);
  for (li = model->items; li != NULL; li = li->next)
    {
      item = li->data;
      if (item->item != NULL
          || item->command == NULL)
        continue;

      g_string_append (contents, item->command);
      g_string_append_c (contents, '\n');
    }

  filename = xfce_resource_save_location (XFCE_RESOURCE_CACHE, NEW_HISTORY_PATH, TRUE);
  if (G_LIKELY (filename != NULL))
    succeed = g_file_set_contents (filename, contents->str, contents->len, error);
  else
    g_set_error_literal (error, 0, 0, "Unable to create history cache file");

  if (succeed)
    {
      /* possible restart monitoring and update mtime */
      xfce_appfinder_model_history_monitor (model, filename);
    }

  /* optimization for next run */
  old_len = contents->allocated_len;

  g_free (filename);
  g_string_free (contents, TRUE);

  return succeed;
}



cairo_surface_t*
xfce_appfinder_model_get_icon_for_command (XfceAppfinderModel *model,
                                           const gchar        *command)
{
  ModelItem   *item;
  const gchar *icon_name;
  GdkPixbuf   *pixbuf;

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), NULL);

  if (IS_STRING (command))
    {
      item = g_hash_table_lookup (model->items_hash, command);
      if (G_LIKELY (item != NULL))
        {
          if (item->surface_large == NULL && item->item != NULL)
            {
              icon_name = garcon_menu_item_get_icon_name (item->item);
              pixbuf = xfce_appfinder_model_load_pixbuf (icon_name, XFCE_APPFINDER_ICON_SIZE_48, model->scale_factor);
              if (pixbuf != NULL)
              {
                item->surface_large = gdk_cairo_surface_create_from_pixbuf (pixbuf, model->scale_factor, NULL);
                g_object_unref (G_OBJECT (pixbuf));
              }
            }

          return cairo_surface_reference (item->surface_large);
        }
    }

  return NULL;
}



void
xfce_appfinder_model_icon_theme_changed (XfceAppfinderModel *model)
{
  ModelItem   *item;
  GtkTreeIter  iter;
  GtkTreePath *path;
  gint         idx;
  gboolean     item_changed;
  GSList      *li;

  appfinder_return_if_fail (XFCE_IS_APPFINDER_MODEL (model));

  APPFINDER_DEBUG ("icon theme or size changed, updating %d items",
                   g_slist_length (model->items));

  /* update the model items */
  for (li = model->items, idx = 0; li != NULL; li = li->next, idx++)
    {
      item = li->data;
      item_changed = FALSE;

      if (item->icon != NULL)
        {
          g_object_unref (G_OBJECT (item->icon));
          item->icon = NULL;
          item_changed = TRUE;
        }
      if (item->surface != NULL)
        {
          cairo_surface_destroy (item->surface);
          item->surface = NULL;
          item_changed = TRUE;
        }
      if (item->surface_large != NULL)
        {
          cairo_surface_destroy (item->surface_large);
          item->surface_large = NULL;
          item_changed = TRUE;
        }
      if (item->abstract != NULL)
        {
          g_free (item->abstract);
          item->abstract = NULL;
          item_changed = TRUE;
        }

      if (item_changed)
        {
          path = gtk_tree_path_new_from_indices (idx, -1);
          ITER_INIT (iter, model->stamp, li);
          gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
        }
    }
}



void
xfce_appfinder_model_history_clear (XfceAppfinderModel *model)
{
  gchar *filename;

  appfinder_return_if_fail (XFCE_IS_APPFINDER_MODEL (model));

  /* remove items from model */
  xfce_appfinder_model_history_remove_items (model);

  /* remove the history file */
  filename = xfce_resource_save_location (XFCE_RESOURCE_CACHE, NEW_HISTORY_PATH, FALSE);
  if (filename != NULL)
    g_unlink (filename);
  g_free (filename);
}



gboolean
xfce_appfinder_model_bookmark_toggle (XfceAppfinderModel  *model,
                                      const gchar         *desktop_id,
                                      GError             **error)
{
  ModelItem    *item;
  GSList       *li;
  const gchar  *desktop_id2;
  static gsize  old_len = 0;
  GString      *contents;
  gchar        *filename;
  gboolean      succeed = FALSE;
  GtkTreePath  *path;
  gint          idx;
  GtkTreeIter   iter;

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_MODEL (model), FALSE);
  appfinder_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  appfinder_return_val_if_fail (desktop_id != NULL, FALSE);

  if (g_hash_table_lookup (model->bookmarks_hash, desktop_id) == NULL)
    g_hash_table_insert (model->bookmarks_hash, g_strdup (desktop_id), GUINT_TO_POINTER (1));
  else
    g_hash_table_remove (model->bookmarks_hash, desktop_id);

  /* string to store custom commands */
  contents = g_string_sized_new (old_len);

  /* update the model items */
  for (idx = 0, li = model->items; li != NULL; li = li->next, idx++)
    {
      item = li->data;
      if (item->item == NULL)
        continue;

      /* find the item we're trying to add/remove */
      if (desktop_id != NULL)
        {
          desktop_id2 = garcon_menu_item_get_desktop_id (item->item);
          if (desktop_id2 != NULL
              && strcmp (desktop_id2, desktop_id) == 0)
            {
              /* toggle state */
              item->is_bookmark = !item->is_bookmark;

              /* stop searching, continue collecting */
              desktop_id = NULL;

              /* update model */
              path = gtk_tree_path_new_from_indices (idx, -1);
              ITER_INIT (iter, model->stamp, li);
              gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
              gtk_tree_path_free (path);
            }
        }

      /* collect bookmarked items */
      if (item->is_bookmark)
        {
          desktop_id2 = garcon_menu_item_get_desktop_id (item->item);
          if (G_LIKELY (desktop_id2 != NULL))
            {
              g_string_append (contents, desktop_id2);
              g_string_append_c (contents, '\n');
            }
        }
    }

  APPFINDER_DEBUG ("saving bookmarks");

  /* write new bookmarks */
  filename = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, BOOKMARKS_PATH, TRUE);
  if (G_LIKELY (filename != NULL))
    succeed = g_file_set_contents (filename, contents->str, contents->len, error);
  else
    g_set_error_literal (error, 0, 0, "Unable to create bookmarks file");

  if (succeed)
    {
      /* possible restart monitoring and update mtime */
      xfce_appfinder_model_bookmarks_monitor (model, filename);
    }

  /* optimization for next run */
  old_len = contents->allocated_len;

  g_free (filename);
  g_string_free (contents, TRUE);

  return succeed;
}



GarconMenuDirectory *
xfce_appfinder_model_get_command_category (void)
{
  static GarconMenuDirectory *category = NULL;

  if (G_LIKELY (category != NULL))
    {
      g_object_ref (G_OBJECT (category));
    }
  else
    {
      category = g_object_new (GARCON_TYPE_MENU_DIRECTORY,
                               "name", _("Commands History"),
                               "icon-name", XFCE_APPFINDER_ICON_NAME_EXECUTE,
                               NULL);
      appfinder_refcount_debug_add (G_OBJECT (category), "commands");
      g_object_add_weak_pointer (G_OBJECT (category), (gpointer) &category);
    }

  return category;
}



GarconMenuDirectory *
xfce_appfinder_model_get_bookmarks_category (void)
{
  static GarconMenuDirectory *category = NULL;

  if (G_LIKELY (category != NULL))
    {
      g_object_ref (G_OBJECT (category));
    }
  else
    {
      category = g_object_new (GARCON_TYPE_MENU_DIRECTORY,
                               "name", _("Bookmarks"),
                               "icon-name", "user-bookmarks",
                               NULL);
      appfinder_refcount_debug_add (G_OBJECT (category), "bookmarks");
      g_object_add_weak_pointer (G_OBJECT (category), (gpointer) &category);
    }

  return category;
}



gboolean
xfce_appfinder_model_fuzzy_match (const gchar *source,
                                  const gchar *token)
{
    const guint token_size = strlen (token);
    const guint cmd_part_size = token_size + 1;
    guint index;
    gboolean match = FALSE;
    gboolean contain_uppercase = FALSE;

    // length is chosen because of ".*<part1> *.*(?i)<part2>.*" pattern format
    // "(?i)" is optional part
    gchar pattern [token_size + 14];
    gchar cmd_part [cmd_part_size];
    gchar *param_part;

    if (strcmp (token,"") == 0)
        return TRUE;

    for (index=1; !match && (index <= token_size); index++)
      {
        memset (cmd_part, 0, cmd_part_size);
        strncpy (cmd_part, token, index);
        cmd_part [index + 1] = '\0';

        contain_uppercase = FALSE;
        param_part = (gchar*) token + index;

        while (!contain_uppercase && (*param_part != '\0'))
          {
            contain_uppercase = g_ascii_isupper (*param_part);
            param_part++;
          }

        memset (pattern, 0, sizeof (pattern));
        g_sprintf (pattern, ".*%s *.*%s%s.*", cmd_part,
                   (contain_uppercase) ? "(?-i)" : "(?i)",
                   token + index);
        match = g_regex_match_simple (pattern, source, 0, 0);
        if (match)
          {
            APPFINDER_DEBUG ("Fuzzy match: regexp=%s ; source=%s", pattern, source);
          }
      }

    return match;
}
