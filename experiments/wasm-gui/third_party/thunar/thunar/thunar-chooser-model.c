/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#include <thunar/thunar-chooser-model.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-private.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CONTENT_TYPE,
};




static void     thunar_chooser_model_constructed    (GObject                  *object);
static void     thunar_chooser_model_finalize       (GObject                  *object);
static void     thunar_chooser_model_get_property   (GObject                  *object,
                                                     guint                     prop_id,
                                                     GValue                   *value,
                                                     GParamSpec               *pspec);
static void     thunar_chooser_model_set_property   (GObject                  *object,
                                                     guint                     prop_id,
                                                     const GValue             *value,
                                                     GParamSpec               *pspec);
static void     thunar_chooser_model_reload         (ThunarChooserModel       *model);



struct _ThunarChooserModelClass
{
  GtkTreeStoreClass __parent__;
};

struct _ThunarChooserModel
{
  GtkTreeStore __parent__;

  gchar *content_type;
};



G_DEFINE_TYPE (ThunarChooserModel, thunar_chooser_model, GTK_TYPE_TREE_STORE)



static void
thunar_chooser_model_class_init (ThunarChooserModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_chooser_model_finalize;
  gobject_class->constructed = thunar_chooser_model_constructed;
  gobject_class->get_property = thunar_chooser_model_get_property;
  gobject_class->set_property = thunar_chooser_model_set_property;

  /**
   * ThunarChooserModel::content-type:
   *
   * The content type for the model.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CONTENT_TYPE,
                                   g_param_spec_string ("content-type",
                                                        "content-type",
                                                        "content-type",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_chooser_model_init (ThunarChooserModel *model)
{
  /* allocate the types array for the columns */
  GType column_types[THUNAR_CHOOSER_MODEL_N_COLUMNS] =
  {
    G_TYPE_STRING,
    G_TYPE_ICON,
    G_TYPE_APP_INFO,
    PANGO_TYPE_STYLE,
    PANGO_TYPE_WEIGHT,
  };

  /* register the column types */
  gtk_tree_store_set_column_types (GTK_TREE_STORE (model),
                                   G_N_ELEMENTS (column_types),
                                   column_types);
}



static void
thunar_chooser_model_constructed (GObject *object)
{
  ThunarChooserModel *model = THUNAR_CHOOSER_MODEL (object);

  /* start to load the applications installed on the system */
  thunar_chooser_model_reload (model);
}



static void
thunar_chooser_model_finalize (GObject *object)
{
  ThunarChooserModel *model = THUNAR_CHOOSER_MODEL (object);

  /* free the content type */
  g_free (model->content_type);

  (*G_OBJECT_CLASS (thunar_chooser_model_parent_class)->finalize) (object);
}



static void
thunar_chooser_model_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarChooserModel *model = THUNAR_CHOOSER_MODEL (object);

  switch (prop_id)
    {
    case PROP_CONTENT_TYPE:
      g_value_set_string (value, thunar_chooser_model_get_content_type (model));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_chooser_model_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ThunarChooserModel *model = THUNAR_CHOOSER_MODEL (object);

  switch (prop_id)
    {
    case PROP_CONTENT_TYPE:
      model->content_type = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_chooser_model_append (ThunarChooserModel *model,
                             const gchar        *title,
                             const gchar        *icon_name,
                             GList              *app_infos)
{
  GtkTreeIter child_iter;
  GtkTreeIter parent_iter;
  GIcon      *icon;
  GList      *lp;
  gboolean    inserted_infos = FALSE;

  _thunar_return_if_fail (THUNAR_IS_CHOOSER_MODEL (model));
  _thunar_return_if_fail (title != NULL);
  _thunar_return_if_fail (icon_name != NULL);

  icon = g_themed_icon_new (icon_name);

  gtk_tree_store_append (GTK_TREE_STORE (model), &parent_iter, NULL);
  gtk_tree_store_set (GTK_TREE_STORE (model), &parent_iter,
                      THUNAR_CHOOSER_MODEL_COLUMN_NAME, title,
                      THUNAR_CHOOSER_MODEL_COLUMN_ICON, icon,
                      THUNAR_CHOOSER_MODEL_COLUMN_WEIGHT, PANGO_WEIGHT_BOLD,
                      -1);

  g_object_unref (icon);

  if (G_LIKELY (app_infos != NULL))
    {
      /* insert the program items */
      for (lp = app_infos; lp != NULL; lp = lp->next)
        {
          if (!thunar_g_app_info_should_show (lp->data))
            continue;

          /* append the tree row with the program data */
          gtk_tree_store_append (GTK_TREE_STORE (model), &child_iter, &parent_iter);
          gtk_tree_store_set (GTK_TREE_STORE (model), &child_iter,
                              THUNAR_CHOOSER_MODEL_COLUMN_NAME, g_app_info_get_name (lp->data),
                              THUNAR_CHOOSER_MODEL_COLUMN_ICON, g_app_info_get_icon (lp->data),
                              THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, lp->data,
                              THUNAR_CHOOSER_MODEL_COLUMN_WEIGHT, PANGO_WEIGHT_NORMAL,
                              -1);

          inserted_infos = TRUE;
        }
    }

  if (!inserted_infos)
    {
      /* tell the user that we don't have any applications for this category */
      gtk_tree_store_append (GTK_TREE_STORE (model), &child_iter, &parent_iter);
      gtk_tree_store_set (GTK_TREE_STORE (model), &child_iter,
                          THUNAR_CHOOSER_MODEL_COLUMN_NAME, _("None available"),
                          THUNAR_CHOOSER_MODEL_COLUMN_STYLE, PANGO_STYLE_ITALIC,
                          THUNAR_CHOOSER_MODEL_COLUMN_WEIGHT, PANGO_WEIGHT_NORMAL,
                          -1);
    }
}



static gint
compare_app_infos (gconstpointer a,
                   gconstpointer b)
{
  return g_app_info_equal (G_APP_INFO (a), G_APP_INFO (b)) ? 0 : 1;
}



static gint
sort_app_infos (gconstpointer a,
                gconstpointer b)
{
  return g_utf8_collate (g_app_info_get_name (G_APP_INFO (a)),
                         g_app_info_get_name (G_APP_INFO (b)));
}



static void
thunar_chooser_model_reload (ThunarChooserModel *model)
{
  GList *all;
  GList *lp;
  GList *other = NULL;
  GList *recommended;
  GList *default_app = NULL;

  _thunar_return_if_fail (THUNAR_IS_CHOOSER_MODEL (model));
  _thunar_return_if_fail (model->content_type != NULL);

  gtk_tree_store_clear (GTK_TREE_STORE (model));

  /* get default application for this type and append it in @default_app */
  default_app = g_list_prepend (default_app, g_app_info_get_default_for_type (model->content_type, FALSE));

  /* If default application was already selected, then display it in Treeview */
  if (default_app->data)
    {
      thunar_chooser_model_append (model,
                                   _("Default Application"),
                                   "org.xfce.settings.default-applications",
                                   default_app);
    }

  /* check if we have any applications for this type */
  recommended = g_app_info_get_all_for_type (model->content_type);

  /* append them as recommended */
  recommended = g_list_sort (recommended, sort_app_infos);
  thunar_chooser_model_append (model,
                               _("Recommended Applications"),
                               "org.xfce.settings.default-applications",
                               recommended);

  all = g_app_info_get_all ();
  for (lp = all; lp != NULL; lp = lp->next)
    {
      if (g_list_find_custom (recommended,
                              lp->data,
                              compare_app_infos) == NULL)
        {
          other = g_list_prepend (other, lp->data);
        }
    }

  /* append the other applications */
  other = g_list_sort (other, sort_app_infos);
  thunar_chooser_model_append (model,
                               _("Other Applications"),
                               "gnome-applications",
                               other);

  if (default_app->data != NULL)
    g_object_unref (default_app->data);
  g_list_free (default_app);

  g_list_free_full (recommended, g_object_unref);
  g_list_free_full (all, g_object_unref);

  g_list_free (other);
}



/**
 * thunar_chooser_model_new:
 * @content_type : the content type for this model.
 *
 * Allocates a new #ThunarChooserModel for @content_type.
 *
 * Return value: the newly allocated #ThunarChooserModel.
 **/
ThunarChooserModel *
thunar_chooser_model_new (const gchar *content_type)
{
  return g_object_new (THUNAR_TYPE_CHOOSER_MODEL,
                       "content-type", content_type,
                       NULL);
}



/**
 * thunar_chooser_model_get_content_type:
 * @model : a #ThunarChooserModel.
 *
 * Returns the content type for @model.
 *
 * Return value: the content type for @model.
 **/
const gchar *
thunar_chooser_model_get_content_type (ThunarChooserModel *model)
{
  _thunar_return_val_if_fail (THUNAR_IS_CHOOSER_MODEL (model), NULL);
  return model->content_type;
}



/**
 * thunar_chooser_model_remove:
 * @model  : a #ThunarChooserModel.
 * @iter   : the #GtkTreeIter for the application to remove.
 * @delete : whether delete or just dissociate the application.
 * @error  : return location for errors or %NULL.
 *
 * Tries to remove or dissociate the application at the specified
 * @iter from the systems application database.
 *
 * Return value: %TRUE on success, %FALSE otherwise.
 **/
gboolean
thunar_chooser_model_remove (ThunarChooserModel *model,
                             GtkTreeIter        *iter,
                             gboolean            delete,
                             GError            **error)
{
  GAppInfo *app_info;
  gboolean  succeed;

  _thunar_return_val_if_fail (THUNAR_IS_CHOOSER_MODEL (model), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_return_val_if_fail (gtk_tree_store_iter_is_valid (GTK_TREE_STORE (model), iter), FALSE);

  /* determine the app info for the iter */
  gtk_tree_model_get (GTK_TREE_MODEL (model),
                      iter,
                      THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, &app_info,
                      -1);

  if (G_UNLIKELY (app_info == NULL))
    return TRUE;

  /* try to remove support for this content type */
  succeed = g_app_info_remove_supports_type (app_info,
                                             model->content_type,
                                             error);

  /* try to delete the file */
  if (delete && succeed && g_app_info_delete (app_info))
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   _("Failed to remove \"%s\"."),
                   g_app_info_get_id (app_info));
    }

  /* clean up */
  g_object_unref (app_info);

  /* if the removal was successfull, delete the row from the model */
  if (G_LIKELY (succeed))
    gtk_tree_store_remove (GTK_TREE_STORE (model), iter);

  return succeed;
}


