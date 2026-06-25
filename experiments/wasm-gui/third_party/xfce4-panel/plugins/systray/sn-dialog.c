/*
 *  Copyright (c) 2012-2013 Andrzej Radecki <andrzejr@xfce.org>
 *  Copyright (c) 2017      Viktor Odintsev <ninetls@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/libxfce4panel.h>

#include "sn-dialog.h"
#include "sn-dialog-ui.h"



#define DEFAULT_ICON_SIZE          22



static gboolean              sn_dialog_build                         (SnDialog                *dialog);

static void                  sn_dialog_finalize                      (GObject                 *object);



struct _SnDialogClass
{
  GObjectClass         __parent__;
};

struct _SnDialog
{
  GObject              __parent__;

  GtkBuilder          *builder;
  GtkWidget           *dialog;
  GtkWidget           *auto_size;
  GtkWidget           *size_spinbutton;
  GtkWidget           *size_revealer;
  GObject             *store;
  GObject             *legacy_store;
  SnConfig            *config;
};

G_DEFINE_TYPE (SnDialog, sn_dialog, G_TYPE_OBJECT)



enum
{
  COLUMN_ICON,
  COLUMN_TITLE,
  COLUMN_HIDDEN,
  COLUMN_TIP
};



/* known applications to improve the icon and name */
static const gchar *known_applications[][3] =
{
  /* application name, icon-name, understandable name */
  { "blueman", "blueman", "Blueman Applet" },
  { "nm-applet", "network-workgroup", "Network Manager Applet" },
  { "Skype1", "skypeforlinux", "Skype" },
  { "chrome_status_icon_1", "google-chrome", "Google Chrome" },
  { "Telegram Desktop", "telegram", "Telegram Desktop" },
  { "redshift", "redshift", "Redshift" },
  { "vlc", "vlc", "VLC Player" },
  { "zoom", "Zoom", "Zoom" },
};

static const gchar *known_legacy_applications[][3] =
{
  /* application name, icon-name, understandable name */
  { "audacious2", "audacious", "Audacious" },
  { "drop-down terminal", "utilities-terminal", "Xfce Dropdown Terminal" },
  { "networkmanager applet", "network-workgroup", "Network Manager Applet" },
  { "parole", "parole", "Parole Media Player" },
  { "task manager", "org.xfce.taskmanager", "Xfce Taskmanager" },
  { "thunar", "Thunar", "Thunar Progress Dialog" },
  { "wicd-client.py", "wicd-gtk", "Wicd" },
  { "workrave tray icon", NULL, "Workrave Applet" },
  { "workrave", NULL, "Workrave" },
  { "xfce terminal", "utilities-terminal", "Xfce Terminal" },
  { "xfce4-power-manager", "xfpm-ac-adapter", "Xfce Power Manager" },
  { "redshift-gtk", "redshift", "Redshift" },
  { "skypeforlinux", "skypeforlinux", "Skype" },
  { "blueman-applet", "blueman", "Blueman Applet" },
  { "system-config-printer", "printer", "Printing Service" },
  { "network", "network-workgroup", "Network Manager Applet" },
};



static void
sn_dialog_class_init (SnDialogClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = sn_dialog_finalize;
}



static void
sn_dialog_init (SnDialog *dialog)
{
  dialog->builder = NULL;
  dialog->dialog = NULL;
  dialog->store  = NULL;
  dialog->legacy_store  = NULL;
  dialog->config = NULL;
}



static void
sn_dialog_add_item (SnDialog   *dialog,
                    GIcon       *icon,
                    const gchar *name,
                    const gchar *title,
                    gboolean     hidden)
{
  GtkTreeIter iter;

  g_return_if_fail (XFCE_IS_SN_DIALOG (dialog));
  g_return_if_fail (GTK_IS_LIST_STORE (dialog->store));
  g_return_if_fail (name == NULL || g_utf8_validate (name, -1, NULL));

  /* insert in the store */
  gtk_list_store_append (GTK_LIST_STORE (dialog->store), &iter);
  gtk_list_store_set (GTK_LIST_STORE (dialog->store), &iter,
                      COLUMN_ICON,    icon,
                      COLUMN_TITLE,   title,
                      COLUMN_HIDDEN,  hidden,
                      COLUMN_TIP,     name,
                      -1);
}



static void
sn_dialog_add_legacy_item(SnDialog *dialog,
                   GIcon *icon,
                   const gchar *name,
                   const gchar *title,
                   gboolean hidden)
{
  GtkTreeIter iter;

  g_return_if_fail(XFCE_IS_SN_DIALOG(dialog));
  g_return_if_fail(GTK_IS_LIST_STORE(dialog->legacy_store));
  g_return_if_fail(name == NULL || g_utf8_validate(name, -1, NULL));

  /* insert in the store */
  gtk_list_store_append(GTK_LIST_STORE(dialog->legacy_store), &iter);
  gtk_list_store_set(GTK_LIST_STORE(dialog->legacy_store), &iter,
                     COLUMN_ICON, icon,
                     COLUMN_TITLE, title,
                     COLUMN_HIDDEN, hidden,
                     COLUMN_TIP, name,
                     -1);
}



static void
sn_dialog_update_names (SnDialog *dialog)
{
  GList       *li;
  const gchar *name;
  const gchar *title;
  const gchar *icon_name;
  GIcon       *icon;
  guint        i;

  g_return_if_fail (XFCE_IS_SN_DIALOG (dialog));
  g_return_if_fail (XFCE_IS_SN_CONFIG (dialog->config));
  g_return_if_fail (GTK_IS_LIST_STORE (dialog->store));

  for (li = sn_config_get_known_items (dialog->config); li != NULL; li = li->next)
    {
      name = li->data;
      title = name;
      icon_name = name;
      icon = NULL;

      /* check if we have a better name for the application */
      for (i = 0; i < G_N_ELEMENTS (known_applications); i++)
        {
          if (strcmp (name, known_applications[i][0]) == 0)
            {
              icon_name = known_applications[i][1];
              title = known_applications[i][2];
              break;
            }
        }

      if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), icon_name))
        icon = g_themed_icon_new (icon_name);

      /* insert item in the store */
      sn_dialog_add_item (dialog, icon, name, title,
                          sn_config_is_hidden (dialog->config, name));

      if (icon != NULL)
        g_object_unref (G_OBJECT (icon));
    }
}



static void
sn_dialog_update_legacy_names(SnDialog *dialog)
{
  GList *li;
  const gchar *name;
  const gchar *title;
  const gchar *icon_name;
  GIcon *icon;
  guint i;

  g_return_if_fail(XFCE_IS_SN_DIALOG(dialog));
  g_return_if_fail(XFCE_IS_SN_CONFIG(dialog->config));
  g_return_if_fail(GTK_IS_LIST_STORE(dialog->legacy_store));

  for (li = sn_config_get_known_legacy_items(dialog->config); li != NULL; li = li->next)
  {
    name = li->data;
    title = name;
    icon_name = name;
    icon = NULL;

    /* check if we have a better name for the application */
    for (i = 0; i < G_N_ELEMENTS(known_legacy_applications); i++)
    {
      if (strcmp(name, known_legacy_applications[i][0]) == 0)
      {
        icon_name = known_legacy_applications[i][1];
        title = known_legacy_applications[i][2];
        break;
      }
    }

    if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), icon_name))
      icon = g_themed_icon_new(icon_name);

    /* insert item in the store */
    sn_dialog_add_legacy_item(dialog, icon, name, title,
                       sn_config_is_legacy_hidden(dialog->config, name));

    if (icon != NULL)
      g_object_unref(G_OBJECT(icon));
  }
}



static void
sn_dialog_selection_changed (GtkTreeSelection *selection,
                             SnDialog         *dialog)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GtkTreePath  *path;
  gint         *indices;
  gint          count = 0, position = -1, depth;
  gboolean      item_up_sensitive, item_down_sensitive;
  GObject      *object;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      path = gtk_tree_model_get_path (model, &iter);
      indices = gtk_tree_path_get_indices_with_depth (path, &depth);

      if (indices != NULL && depth > 0)
        position = indices[0];

      count = gtk_tree_model_iter_n_children (model, NULL);

      gtk_tree_path_free (path);
    }

  item_up_sensitive = position > 0;
  item_down_sensitive = position + 1 < count;

  object = gtk_builder_get_object (dialog->builder, "item-up");
  if (GTK_IS_BUTTON (object))
    gtk_widget_set_sensitive (GTK_WIDGET (object), item_up_sensitive);

  object = gtk_builder_get_object (dialog->builder, "item-down");
  if (GTK_IS_BUTTON (object))
    gtk_widget_set_sensitive (GTK_WIDGET (object), item_down_sensitive);
}



static void
sn_dialog_legacy_selection_changed (GtkTreeSelection *selection,
                                    SnDialog         *dialog)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GtkTreePath  *path;
  gint         *indices;
  gint          count = 0, position = -1, depth;
  gboolean      item_up_sensitive, item_down_sensitive;
  GObject      *object;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      path = gtk_tree_model_get_path (model, &iter);
      indices = gtk_tree_path_get_indices_with_depth (path, &depth);

      if (indices != NULL && depth > 0)
        position = indices[0];

      count = gtk_tree_model_iter_n_children (model, NULL);

      gtk_tree_path_free (path);
    }

  item_up_sensitive = position > 0;
  item_down_sensitive = position + 1 < count;

  object = gtk_builder_get_object (dialog->builder, "item-up");
  if (GTK_IS_BUTTON (object))
    gtk_widget_set_sensitive (GTK_WIDGET (object), item_up_sensitive);

  object = gtk_builder_get_object (dialog->builder, "item-down");
  if (GTK_IS_BUTTON (object))
    gtk_widget_set_sensitive (GTK_WIDGET (object), item_down_sensitive);
}



static void
sn_dialog_hidden_toggled (GtkCellRendererToggle *renderer,
                          const gchar           *path_string,
                          SnDialog              *dialog)
{
  GtkTreeIter  iter;
  gboolean     hidden;
  gchar       *name;

  g_return_if_fail (XFCE_IS_SN_DIALOG (dialog));
  g_return_if_fail (XFCE_IS_SN_CONFIG (dialog->config));
  g_return_if_fail (GTK_IS_LIST_STORE (dialog->store));

  if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (dialog->store), &iter, path_string))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (dialog->store), &iter,
                          COLUMN_HIDDEN, &hidden,
                          COLUMN_TIP, &name, -1);

      /* insert value (we need to update it) */
      hidden = !hidden;

      /* update box and store with new state */
      sn_config_set_hidden (dialog->config, name, hidden);
      gtk_list_store_set (GTK_LIST_STORE (dialog->store), &iter, COLUMN_HIDDEN, hidden, -1);

      g_free (name);
    }
}



static void
sn_dialog_legacy_hidden_toggled (GtkCellRendererToggle *renderer,
                                 const gchar           *path_string,
                                 SnDialog              *dialog)
{
  GtkTreeIter  iter;
  gboolean     hidden;
  gchar       *name;

  g_return_if_fail (XFCE_IS_SN_DIALOG (dialog));
  g_return_if_fail (XFCE_IS_SN_CONFIG (dialog->config));
  g_return_if_fail (GTK_IS_LIST_STORE (dialog->store));

  if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (dialog->legacy_store), &iter, path_string))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (dialog->legacy_store), &iter,
                          COLUMN_HIDDEN, &hidden,
                          COLUMN_TIP, &name, -1);

      /* insert value (we need to update it) */
      hidden = !hidden;

      /* update box and store with new state */
      sn_config_set_legacy_hidden (dialog->config, name, hidden);
      gtk_list_store_set (GTK_LIST_STORE (dialog->legacy_store), &iter, COLUMN_HIDDEN, hidden, -1);

      g_free (name);
    }
}



static void
sn_dialog_swap_rows (SnDialog   *dialog,
                     GtkTreeIter *iter_prev,
                     GtkTreeIter *iter)
{
  GIcon     *icon1, *icon2;
  gchar     *title1, *title2;
  gboolean   hidden1, hidden2;
  gchar     *tip1, *tip2;

  g_return_if_fail (XFCE_IS_SN_DIALOG (dialog));
  g_return_if_fail (XFCE_IS_SN_CONFIG (dialog->config));
  g_return_if_fail (GTK_IS_LIST_STORE (dialog->store));

  gtk_tree_model_get (GTK_TREE_MODEL (dialog->store), iter_prev,
                      COLUMN_ICON,    &icon1,
                      COLUMN_TITLE,   &title1,
                      COLUMN_HIDDEN,  &hidden1,
                      COLUMN_TIP,     &tip1, -1);
  gtk_tree_model_get (GTK_TREE_MODEL (dialog->store), iter,
                      COLUMN_ICON,    &icon2,
                      COLUMN_TITLE,   &title2,
                      COLUMN_HIDDEN,  &hidden2,
                      COLUMN_TIP,     &tip2, -1);
  gtk_list_store_set (GTK_LIST_STORE (dialog->store), iter_prev,
                      COLUMN_ICON,    icon2,
                      COLUMN_TITLE,   title2,
                      COLUMN_HIDDEN,  hidden2,
                      COLUMN_TIP,     tip2, -1);
  gtk_list_store_set (GTK_LIST_STORE (dialog->store), iter,
                      COLUMN_ICON,    icon1,
                      COLUMN_TITLE,   title1,
                      COLUMN_HIDDEN,  hidden1,
                      COLUMN_TIP,     tip1, -1);

  /* do a matching operation on SnConfig */
  sn_config_swap_known_items (dialog->config, tip1, tip2);
}



static void
sn_dialog_legacy_swap_rows(SnDialog *dialog,
                           GtkTreeIter *iter_prev,
                           GtkTreeIter *iter)
{
  GIcon *icon1, *icon2;
  gchar *title1, *title2;
  gboolean hidden1, hidden2;
  gchar *tip1, *tip2;

  g_return_if_fail(XFCE_IS_SN_DIALOG(dialog));
  g_return_if_fail(XFCE_IS_SN_CONFIG(dialog->config));
  g_return_if_fail(GTK_IS_LIST_STORE(dialog->legacy_store));

  gtk_tree_model_get(GTK_TREE_MODEL(dialog->legacy_store), iter_prev,
                     COLUMN_ICON, &icon1,
                     COLUMN_TITLE, &title1,
                     COLUMN_HIDDEN, &hidden1,
                     COLUMN_TIP, &tip1, -1);
  gtk_tree_model_get(GTK_TREE_MODEL(dialog->legacy_store), iter,
                     COLUMN_ICON, &icon2,
                     COLUMN_TITLE, &title2,
                     COLUMN_HIDDEN, &hidden2,
                     COLUMN_TIP, &tip2, -1);
  gtk_list_store_set(GTK_LIST_STORE(dialog->legacy_store), iter_prev,
                     COLUMN_ICON, icon2,
                     COLUMN_TITLE, title2,
                     COLUMN_HIDDEN, hidden2,
                     COLUMN_TIP, tip2, -1);
  gtk_list_store_set(GTK_LIST_STORE(dialog->legacy_store), iter,
                     COLUMN_ICON, icon1,
                     COLUMN_TITLE, title1,
                     COLUMN_HIDDEN, hidden1,
                     COLUMN_TIP, tip1, -1);

  /* do a matching operation on SnConfig */
  sn_config_swap_known_legacy_items(dialog->config, tip1, tip2);
}



static gboolean
sn_dialog_iter_equal (GtkTreeIter *iter1,
                      GtkTreeIter *iter2)
{
  return (iter1->user_data  == iter2->user_data  &&
          iter1->user_data2 == iter2->user_data2 &&
          iter1->user_data3 == iter2->user_data3);
}



static void
sn_dialog_item_up_clicked (GtkWidget *button,
                           SnDialog *dialog)
{
  GObject          *treeview;
  GtkTreeSelection *selection;
  GtkTreeIter       iter, iter_prev, iter_tmp;

  g_return_if_fail (XFCE_IS_SN_DIALOG (dialog));
  g_return_if_fail (GTK_IS_LIST_STORE (dialog->store));

  treeview = gtk_builder_get_object (dialog->builder, "items-treeview");
  g_return_if_fail (GTK_IS_TREE_VIEW (treeview));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  /* gtk_tree_model_iter_previous available from Gtk3 */
  /* so we have to search for it starting from the first iter */
  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (dialog->store), &iter_prev))
    return;

  iter_tmp = iter_prev;
  while (!sn_dialog_iter_equal (&iter_tmp, &iter))
    {
      iter_prev = iter_tmp;
      if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (dialog->store), &iter_tmp))
        return;
    }

  sn_dialog_swap_rows (dialog, &iter_prev, &iter);
  gtk_tree_selection_select_iter (selection, &iter_prev);
}



static void
sn_dialog_legacy_item_up_clicked(GtkWidget *button,
                                 SnDialog *dialog)
{
  GObject *treeview;
  GtkTreeSelection *selection;
  GtkTreeIter iter, iter_prev, iter_tmp;

  g_return_if_fail(XFCE_IS_SN_DIALOG(dialog));
  g_return_if_fail(GTK_IS_LIST_STORE(dialog->legacy_store));

  treeview = gtk_builder_get_object(dialog->builder, "legacy-items-treeview");
  g_return_if_fail(GTK_IS_TREE_VIEW(treeview));

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
  if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
    return;

  /* gtk_tree_model_iter_previous available from Gtk3 */
  /* so we have to search for it starting from the first iter */
  if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(dialog->legacy_store), &iter_prev))
    return;

  iter_tmp = iter_prev;
  while (!sn_dialog_iter_equal(&iter_tmp, &iter))
  {
    iter_prev = iter_tmp;
    if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(dialog->legacy_store), &iter_tmp))
      return;
  }

  sn_dialog_legacy_swap_rows(dialog, &iter_prev, &iter);
  gtk_tree_selection_select_iter(selection, &iter_prev);
}



static void
sn_dialog_item_down_clicked (GtkWidget *button,
                             SnDialog *dialog)
{
  GObject          *treeview;
  GtkTreeSelection *selection;
  GtkTreeIter       iter, iter_next;

  g_return_if_fail (XFCE_IS_SN_DIALOG (dialog));

  treeview = gtk_builder_get_object (dialog->builder, "items-treeview");
  g_return_if_fail (GTK_IS_TREE_VIEW (treeview));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  iter_next = iter;
  if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (dialog->store), &iter_next))
    return;

  sn_dialog_swap_rows (dialog, &iter, &iter_next);
  gtk_tree_selection_select_iter (selection, &iter_next);
}



static void
sn_dialog_legacy_item_down_clicked(GtkWidget *button,
                            SnDialog *dialog)
{
  GObject *treeview;
  GtkTreeSelection *selection;
  GtkTreeIter iter, iter_next;

  g_return_if_fail(XFCE_IS_SN_DIALOG(dialog));

  treeview = gtk_builder_get_object(dialog->builder, "legacy-items-treeview");
  g_return_if_fail(GTK_IS_TREE_VIEW(treeview));

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
  if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
    return;

  iter_next = iter;
  if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(dialog->legacy_store), &iter_next))
    return;

  sn_dialog_legacy_swap_rows(dialog, &iter, &iter_next);
  gtk_tree_selection_select_iter(selection, &iter_next);
}



static void
sn_dialog_clear_clicked (GtkWidget *button,
                         SnDialog *dialog)
{
  g_return_if_fail (XFCE_IS_SN_DIALOG (dialog));

  if (xfce_dialog_confirm (GTK_WINDOW (gtk_widget_get_toplevel (button)),
                           "edit-clear", _("Clear"), NULL,
                           _("Are you sure you want to clear the list of "
                             "known items?")))
    {
      if (sn_config_items_clear (dialog->config))
        {
          gtk_list_store_clear (GTK_LIST_STORE (dialog->store));
          sn_dialog_update_names (dialog);
        }
      if (sn_config_legacy_items_clear (dialog->config))
        {
          gtk_list_store_clear (GTK_LIST_STORE (dialog->legacy_store));
          sn_dialog_update_legacy_names (dialog);
        }
    }
}



static void
sn_dialog_dialog_unref (gpointer  data,
                        GObject  *where_the_object_was)
{
  SnDialog *dialog = data;

  dialog->dialog = NULL;
  g_object_unref (dialog);
}



static void
reveal_icon_size (GtkWidget  *widget,
                  GParamSpec *pspec,
                  SnDialog   *dialog)
{
  gboolean active;
  gint     icon_size;

  g_return_if_fail (XFCE_IS_SN_DIALOG (dialog));

  active = gtk_switch_get_active (GTK_SWITCH (widget));

  if (active)
    icon_size = 0;
  else
    icon_size = DEFAULT_ICON_SIZE;

  gtk_revealer_set_reveal_child (GTK_REVEALER (dialog->size_revealer), !active);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->size_spinbutton), icon_size);
}



static gboolean
sn_dialog_build (SnDialog *dialog)
{
  GObject          *object;
  GError           *error = NULL;
  GtkTreeSelection *selection;

  if (xfce_titled_dialog_get_type () == 0)
    return FALSE;

  dialog->builder = gtk_builder_new ();

  /* load the builder data into the object */
  if (gtk_builder_add_from_string (dialog->builder, sn_dialog_ui,
                                   sn_dialog_ui_length, &error))
    {
      object = gtk_builder_get_object (dialog->builder, "dialog");
      g_return_val_if_fail (XFCE_IS_TITLED_DIALOG (object), FALSE);
      dialog->dialog = GTK_WIDGET (object);

      object = gtk_builder_get_object (dialog->builder, "close-button");
      g_return_val_if_fail (GTK_IS_BUTTON (object), FALSE);
      g_signal_connect_swapped (G_OBJECT (object), "clicked",
                                G_CALLBACK (gtk_widget_destroy),
                                dialog->dialog);

      object = gtk_builder_get_object (dialog->builder, "switch-auto-size");
      g_return_val_if_fail (GTK_IS_WIDGET (object), FALSE);
      dialog->auto_size = GTK_WIDGET (object);

      object = gtk_builder_get_object (dialog->builder, "spinbutton-icon-size");
      g_return_val_if_fail (GTK_IS_WIDGET (object), FALSE);
      g_object_bind_property (G_OBJECT (dialog->config), "icon-size",
                              G_OBJECT (gtk_spin_button_get_adjustment
                                        (GTK_SPIN_BUTTON (object))), "value",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      dialog->size_spinbutton = GTK_WIDGET (object);

      object = gtk_builder_get_object (dialog->builder, "revealer-icon-size");
      g_return_val_if_fail (GTK_IS_WIDGET (object), FALSE);
      dialog->size_revealer = GTK_WIDGET (object);

      if (sn_config_get_icon_size_is_automatic (dialog->config))
        {
          gtk_switch_set_active (GTK_SWITCH (dialog->auto_size), TRUE);
          gtk_revealer_set_reveal_child (GTK_REVEALER (dialog->size_revealer), FALSE);
        }

      g_signal_connect (G_OBJECT (dialog->auto_size), "notify::active",
                        G_CALLBACK (reveal_icon_size), dialog);

      object = gtk_builder_get_object (dialog->builder, "checkbutton-single-row");
      g_return_val_if_fail (GTK_IS_WIDGET (object), FALSE);
      g_object_bind_property (G_OBJECT (dialog->config), "single-row",
                              G_OBJECT (object), "active",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

      object = gtk_builder_get_object (dialog->builder, "checkbutton-square-icons");
      g_return_val_if_fail (GTK_IS_WIDGET (object), FALSE);
      g_object_bind_property (G_OBJECT (dialog->config), "square-icons",
                              G_OBJECT (object), "active",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

      object = gtk_builder_get_object (dialog->builder, "checkbutton-symbolic-icons");
      g_return_val_if_fail (GTK_IS_WIDGET (object), FALSE);
      g_object_bind_property (G_OBJECT (dialog->config), "symbolic-icons",
                              G_OBJECT (object), "active",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

      object = gtk_builder_get_object (dialog->builder, "checkbutton-menu-is-primary");
      g_return_val_if_fail (GTK_IS_WIDGET (object), FALSE);
      g_object_bind_property (G_OBJECT (dialog->config), "menu-is-primary",
                              G_OBJECT (object), "active",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

      object = gtk_builder_get_object (dialog->builder, "checkbutton-hide-new-items");
      g_return_val_if_fail (GTK_IS_WIDGET (object), FALSE);
      g_object_bind_property (G_OBJECT (dialog->config), "hide-new-items",
                              G_OBJECT (object), "active",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

      dialog->store = gtk_builder_get_object (dialog->builder, "items-store");
      g_return_val_if_fail (GTK_IS_LIST_STORE (dialog->store), FALSE);
      sn_dialog_update_names (dialog);

      object = gtk_builder_get_object (dialog->builder, "items-treeview");
      g_return_val_if_fail (GTK_IS_TREE_VIEW (object), FALSE);
      gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (object), COLUMN_TIP);

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
      g_signal_connect (G_OBJECT (selection), "changed",
                        G_CALLBACK (sn_dialog_selection_changed), dialog);
      sn_dialog_selection_changed (selection, dialog);

      object = gtk_builder_get_object (dialog->builder, "hidden-toggle");
      g_return_val_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (object), FALSE);
      g_signal_connect (G_OBJECT (object), "toggled",
                        G_CALLBACK (sn_dialog_hidden_toggled), dialog);

      object = gtk_builder_get_object (dialog->builder, "item-up");
      g_return_val_if_fail (GTK_IS_BUTTON (object), FALSE);
      g_signal_connect (G_OBJECT (object), "clicked",
                        G_CALLBACK (sn_dialog_item_up_clicked), dialog);

      object = gtk_builder_get_object (dialog->builder, "item-down");
      g_return_val_if_fail (GTK_IS_BUTTON (object), FALSE);
      g_signal_connect (G_OBJECT (object), "clicked",
                        G_CALLBACK (sn_dialog_item_down_clicked), dialog);

      dialog->legacy_store = gtk_builder_get_object (dialog->builder, "legacy-items-store");
      g_return_val_if_fail (GTK_IS_LIST_STORE (dialog->legacy_store), FALSE);
      sn_dialog_update_legacy_names (dialog);

      object = gtk_builder_get_object (dialog->builder, "legacy-items-treeview");
      g_return_val_if_fail (GTK_IS_TREE_VIEW (object), FALSE);
      gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (object), COLUMN_TIP);

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
      g_signal_connect (G_OBJECT (selection), "changed",
                        G_CALLBACK (sn_dialog_legacy_selection_changed), dialog);
      sn_dialog_selection_changed (selection, dialog);

      object = gtk_builder_get_object (dialog->builder, "legacy-hidden-toggle");
      g_return_val_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (object), FALSE);
      g_signal_connect (G_OBJECT (object), "toggled",
                        G_CALLBACK (sn_dialog_legacy_hidden_toggled), dialog);

      object = gtk_builder_get_object (dialog->builder, "legacy-item-up");
      g_return_val_if_fail (GTK_IS_BUTTON (object), FALSE);
      g_signal_connect (G_OBJECT (object), "clicked",
                        G_CALLBACK (sn_dialog_legacy_item_up_clicked), dialog);

      object = gtk_builder_get_object (dialog->builder, "legacy-item-down");
      g_return_val_if_fail (GTK_IS_BUTTON (object), FALSE);
      g_signal_connect (G_OBJECT (object), "clicked",
                        G_CALLBACK (sn_dialog_legacy_item_down_clicked), dialog);

      object = gtk_builder_get_object (dialog->builder, "items-clear");
      g_return_val_if_fail (GTK_IS_BUTTON (object), FALSE);
      g_signal_connect (G_OBJECT (object), "clicked",
                        G_CALLBACK (sn_dialog_clear_clicked), dialog);

#ifndef HAVE_DBUSMENU
      object = gtk_builder_get_object (dialog->builder, "sn_box");
      gtk_widget_hide (GTK_WIDGET (object));
      object = gtk_builder_get_object (dialog->builder, "items_stack_switcher");
      gtk_widget_hide (GTK_WIDGET (object));
#endif

      g_object_weak_ref (G_OBJECT (dialog->dialog), sn_dialog_dialog_unref, dialog);
      return TRUE;
    }
  else
    {
      g_critical ("Failed to construct the builder: %s.",
                  error->message);
      g_error_free (error);
      return FALSE;
    }
}



SnDialog *
sn_dialog_new (SnConfig  *config,
               GdkScreen *screen)
{
  SnDialog *dialog;

  g_return_val_if_fail (XFCE_IS_SN_CONFIG (config), NULL);

  dialog = g_object_new (XFCE_TYPE_SN_DIALOG, NULL);
  dialog->config = config;

  if (sn_dialog_build (dialog))
    {
      gtk_widget_show (dialog->dialog);
      gtk_window_set_screen (GTK_WINDOW (dialog->dialog), screen);
      return dialog;
    }
  else
    {
      g_object_unref (dialog);
      return NULL;
    }
}



static void
sn_dialog_finalize (GObject *object)
{
  SnDialog *dialog = XFCE_SN_DIALOG (object);

  if (dialog->dialog != NULL)
    gtk_widget_destroy (dialog->dialog);

  if (dialog->builder != NULL)
    g_object_unref (dialog->builder);

  G_OBJECT_CLASS (sn_dialog_parent_class)->finalize (object);
}
