/*
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

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-application.h>
#include <mousepad/mousepad-prefs-dialog.h>
#include <mousepad/mousepad-util.h>

#include <shortcuts-plugin/shortcuts-plugin.h>

/* prevent I18N macros redefinition */
#ifndef __G_I18N_LIB_H__
#define __G_I18N_LIB_H__
#endif
#ifdef I_
#undef I_
#endif
#include <libxfce4kbd-private-3/libxfce4kbd-private/xfce-shortcuts-editor.h>



/* GObject virtual functions */
static void shortcuts_plugin_constructed  (GObject        *object);
static void shortcuts_plugin_finalize     (GObject        *object);

/* MousepadPlugin virtual functions */
static void shortcuts_plugin_enable       (MousepadPlugin *mplugin);
static void shortcuts_plugin_disable      (MousepadPlugin *mplugin);

/* ShortcutsPlugin own functions */
static void shortcuts_plugin_build_editor (ShortcutsPlugin *plugin);



struct _ShortcutsPlugin
{
  MousepadPlugin __parent__;

  XfceShortcutsEditorSection *menubar_sections, *prefs_dialog_sections;
  guint                       n_menubar_sections, n_prefs_dialog_sections;
  XfceGtkActionEntry         *misc_entries;
  guint                       n_misc_entries;
  GtkWidget                  *menubar_editor, *prefs_dialog_editor, *misc_editor;

  GtkWidget                  *dialog;
};



G_DEFINE_DYNAMIC_TYPE (ShortcutsPlugin, shortcuts_plugin, MOUSEPAD_TYPE_PLUGIN);



void
shortcuts_plugin_register (MousepadPluginProvider *plugin)
{
  shortcuts_plugin_register_type (G_TYPE_MODULE (plugin));
}



static void
shortcuts_plugin_class_init (ShortcutsPluginClass *klass)
{
  GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);
  MousepadPluginClass *mplugin_class = MOUSEPAD_PLUGIN_CLASS (klass);

  gobject_class->constructed = shortcuts_plugin_constructed;
  gobject_class->finalize = shortcuts_plugin_finalize;

  mplugin_class->enable = shortcuts_plugin_enable;
  mplugin_class->disable = shortcuts_plugin_disable;
}



static void
shortcuts_plugin_class_finalize (ShortcutsPluginClass *klass)
{
}



static void
shortcuts_plugin_init (ShortcutsPlugin *plugin)
{
  /* initialization */
  plugin->menubar_sections = NULL;
  plugin->prefs_dialog_sections = NULL;
  plugin->misc_entries = NULL;
  plugin->menubar_editor = NULL;
  plugin->prefs_dialog_editor = NULL;
  plugin->misc_editor = NULL;
  plugin->dialog = NULL;

  shortcuts_plugin_enable (MOUSEPAD_PLUGIN (plugin));
}



static void
shortcuts_plugin_setting_box_packed (GObject         *object,
                                     GParamSpec      *pspec,
                                     ShortcutsPlugin *plugin)
{
  GtkWidget *parent;

  g_object_get (object, "parent", &parent, NULL);
  if (! GTK_IS_POPOVER (parent))
    return;

  /* do the main work now if not already done */
  if (plugin->menubar_sections == NULL)
    shortcuts_plugin_build_editor (plugin);

  /* we need to make the parent popover expand in that case */
  gtk_widget_set_hexpand (parent, TRUE);
  gtk_widget_set_vexpand (parent, TRUE);
}



static void
shortcuts_plugin_constructed (GObject *object)
{
  MousepadPluginProvider *provider;
  GtkWidget              *vbox;

  /* request the creation of the plugin setting box */
  g_object_get (object, "provider", &provider, NULL);
  vbox = mousepad_plugin_provider_create_setting_box (provider);
  g_signal_connect (vbox, "notify::parent",
                    G_CALLBACK (shortcuts_plugin_setting_box_packed), object);

  /* chain-up to MousepadPlugin */
  G_OBJECT_CLASS (shortcuts_plugin_parent_class)->constructed (object);
}



static void
shortcuts_plugin_finalize (GObject *object)
{
  ShortcutsPlugin *plugin = SHORTCUTS_PLUGIN (object);

  shortcuts_plugin_disable (MOUSEPAD_PLUGIN (plugin));

  G_OBJECT_CLASS (shortcuts_plugin_parent_class)->finalize (object);
}



static void
shortcuts_plugin_disable_scrolled_window (GtkWidget *widget)
{
  if (GTK_IS_SCROLLED_WINDOW (widget))
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget),
                                    GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  else if (GTK_IS_CONTAINER (widget))
    {
      GList *list;

      list = gtk_container_get_children (GTK_CONTAINER (widget));
      shortcuts_plugin_disable_scrolled_window (list->data);
      g_list_free (list);
    }
}



static void
shortcuts_plugin_pack_frame (GtkWidget   *editor,
                             GtkWidget   *sbox,
                             const gchar *title)
{
  GtkWidget *frame, *fbox, *label;
  gchar     *str;

  /* prevent markup syntax mistakes in translations for the label */
  str = g_strdup_printf ("<b>%s</b>", title);
  label = gtk_label_new (str);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_free (str);

  /* pack the frame */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_box_pack_start (GTK_BOX (sbox), frame, TRUE, TRUE, 0);
  fbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (frame), fbox);

  /* pack shortcut sections by disabling their scrolled window */
  shortcuts_plugin_disable_scrolled_window (editor);
  gtk_box_pack_start (GTK_BOX (fbox), editor, TRUE, TRUE, 0);
  gtk_widget_set_margin_bottom (editor, 6);
}



static void
shortcuts_plugin_enable_action (GObject       *object,
                                GParamSpec    *pspec,
                                GSimpleAction *action)
{
  GtkWidget *parent;

  g_object_get (object, "parent", &parent, NULL);
  g_simple_action_set_enabled (action, parent == NULL);
}



static void
shortcuts_plugin_set_setting_box (ShortcutsPlugin *plugin)
{
  MousepadPluginProvider *provider;
  GtkWidget              *vbox, *swin, *sbox;
  GAction                *action;

  /* get the plugin setting box */
  g_object_get (plugin, "provider", &provider, NULL);
  vbox = mousepad_plugin_provider_get_setting_box (provider);

  /* disable action when the setting box is already packed in the prefs dialog popover */
  action = g_action_map_lookup_action (G_ACTION_MAP (g_application_get_default ()), "shortcuts");
  g_signal_connect (vbox, "notify::parent", G_CALLBACK (shortcuts_plugin_enable_action), action);

  /* pack a scrolled window with a reasonable minimum size, which is necessary in particular
   * for the popover to expand properly */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (swin), 400);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (swin), 600);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), swin, TRUE, TRUE, 0);
  sbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_add (GTK_CONTAINER (swin), sbox);

  /* pack frames */
  shortcuts_plugin_pack_frame (plugin->menubar_editor, sbox, _("Menubar"));
  shortcuts_plugin_pack_frame (plugin->prefs_dialog_editor, sbox, _("Preferences Dialog"));
  shortcuts_plugin_pack_frame (plugin->misc_editor, sbox, _("Miscellaneous"));

  /* show all widgets in the setting box */
  gtk_widget_show_all (vbox);
}



static void
shortcuts_plugin_fake_callback (void)
{
}



static void
shortcuts_plugin_get_misc_paths (gpointer data,
                                 const gchar *accel_path,
                                 guint accel_key,
                                 GdkModifierType accel_mods,
                                 gboolean changed)
{
  GList **list = data;

  if (mousepad_object_get_data (gtk_accel_map_get (), accel_path))
    return;

  *list = g_list_prepend (*list, (gpointer) accel_path);
}



static const gchar *
shortcuts_plugin_search_child_label (GtkWidget *widget)
{
  GList       *list, *lp;
  const gchar *label = NULL;

  if (GTK_IS_LABEL (widget))
    label = gtk_label_get_label (GTK_LABEL (widget));
  else if (GTK_IS_CONTAINER (widget))
    {
      list = gtk_container_get_children (GTK_CONTAINER (widget));
      for (lp = list; lp != NULL; lp = lp->next)
        {
          label = shortcuts_plugin_search_child_label (lp->data);
          if (label != NULL)
            break;
        }

      g_list_free (list);
    }

  return label;
}



static void
shortcuts_plugin_get_tab_entries (GtkAccelMap        *map,
                                  GtkWidget          *widget,
                                  XfceGtkActionEntry *entries,
                                  guint              *size)
{
  GList       *list, *lp;
  gchar       *path;
  const gchar *action, *label, *accel;

  if (GTK_IS_CHECK_BUTTON (widget))
    {
      action = gtk_actionable_get_action_name (GTK_ACTIONABLE (widget));
      path = g_strconcat ("<Actions>/", action, NULL);
      if (! gtk_accel_map_lookup_entry (path, NULL) || mousepad_object_get_data (map, path))
        {
          g_free (path);
          return;
        }

      /* try to look into the check button if no native label is set */
      label = gtk_button_get_label (GTK_BUTTON (widget));
      if (label == NULL)
        label = shortcuts_plugin_search_child_label (widget);

      mousepad_object_set_data (map, g_intern_string (path), GINT_TO_POINTER (TRUE));
      accel = mousepad_object_get_data (map, path + 10);
      entries[*size].menu_item_label_text = g_strdup (label != NULL ? label : action);
      entries[*size].accel_path = path;
      entries[*size].default_accelerator = g_strdup (accel != NULL ? accel : "");
      entries[*size].callback = shortcuts_plugin_fake_callback;

      (*size)++;
    }
  else if (GTK_IS_CONTAINER (widget))
    {
      list = gtk_container_get_children (GTK_CONTAINER (widget));
      for (lp = list; lp != NULL; lp = lp->next)
        shortcuts_plugin_get_tab_entries (map, lp->data, entries, size);

      g_list_free (list);
    }
}



static void
shortcuts_plugin_get_menu_entries (GtkAccelMap        *map,
                                   GMenuModel         *menu,
                                   XfceGtkActionEntry *entries,
                                   guint              *size)
{
  GMenuModel *submenu;
  GVariant   *action, *target, *label;
  gchar      *path, *temp, *target_str;
  guint       n_items;

  n_items = g_menu_model_get_n_items (menu);
  for (guint n = 0; n < n_items; n++)
    {
      /* section or submenu GMenuItem: go ahead recursively */
      if ((submenu = g_menu_model_get_item_link (menu, n, G_MENU_LINK_SECTION)) != NULL
          || (submenu = g_menu_model_get_item_link (menu, n, G_MENU_LINK_SUBMENU)) != NULL)
        shortcuts_plugin_get_menu_entries (map, submenu, entries, size);
      /* real GMenuItem */
      else if ((action = g_menu_model_get_item_attribute_value (menu, n, "action",
                                                                G_VARIANT_TYPE_STRING)) != NULL)
        {
          /* build path from action name and target */
          path = g_strconcat ("<Actions>/", g_variant_get_string (action, NULL), NULL);
          g_variant_unref (action);
          target = g_menu_model_get_item_attribute_value (menu, n, "target", NULL);
          if (target != NULL)
            {
              target_str = g_variant_print (target, TRUE);
              temp = path;
              path = g_strdup_printf ("%s(%s)", path, target_str);
              g_free (temp);
              g_free (target_str);
              g_variant_unref (target);
            }

          if (! gtk_accel_map_lookup_entry (path, NULL))
            {
              g_free (path);
              continue;
            }

          mousepad_object_set_data (map, g_intern_string (path), GINT_TO_POINTER (TRUE));
          label = g_menu_model_get_item_attribute_value (menu, n, "label", G_VARIANT_TYPE_STRING);
          temp = mousepad_object_get_data (map, path + 10);
          entries[*size].menu_item_label_text = g_strdup (g_variant_get_string (label, NULL));
          entries[*size].accel_path = path;
          entries[*size].default_accelerator = g_strdup (temp != NULL ? temp : "");
          entries[*size].callback = shortcuts_plugin_fake_callback;

          g_variant_unref (label);
          (*size)++;
        }
    }
}



static void
shortcuts_plugin_count_accels (gpointer data,
                               const gchar *accel_path,
                               guint accel_key,
                               GdkModifierType accel_mods,
                               gboolean changed)
{
  (*((guint *) data))++;
}



static void
shortcuts_plugin_build_editor (ShortcutsPlugin *plugin)
{
  XfceGtkActionEntry *entries;
  GtkAccelMap        *map;
  GtkApplication     *application;
  GtkNotebook        *notebook;
  GtkWidget          *dialog, *widget;
  GList              *list, *lp;
  GMenuModel         *menubar, *menu;
  GVariant           *label;
  GStrv               strv;
  guint               n_accels = 0, size, n, n_sections;

  /* get the mousepad application */
  application = GTK_APPLICATION (g_application_get_default ());

  /* get the accel map size */
  map = gtk_accel_map_get ();
  gtk_accel_map_foreach (&n_accels, shortcuts_plugin_count_accels);

  /* allocate the XfceShortcutsEditorSection array */
  menubar = G_MENU_MODEL (gtk_application_get_menu_by_id (application, "menubar"));
  n_sections = g_menu_model_get_n_items (menubar);
  plugin->menubar_sections = g_new (XfceShortcutsEditorSection, n_sections);
  plugin->n_menubar_sections = n_sections;

  /* walk the menubar */
  for (n = 0; n < n_sections; n++)
    {
      /* allocate a sufficiently large XfceGtkActionEntry array */
      entries = g_new (XfceGtkActionEntry, n_accels);

      /* get action entries for this menu */
      menu = g_menu_model_get_item_link (menubar, n, G_MENU_LINK_SUBMENU);
      size = 0;
      shortcuts_plugin_get_menu_entries (map, menu, entries, &size);
      plugin->menubar_sections[n].entries = g_renew (XfceGtkActionEntry, entries, size);
      plugin->menubar_sections[n].size = size;

      /* get section name for this menu */
      label = g_menu_model_get_item_attribute_value (menubar, n, "label", G_VARIANT_TYPE_STRING);
      strv = g_strsplit (g_variant_get_string (label, NULL), "_", 0);
      plugin->menubar_sections[n].section_name = g_strjoinv (NULL, strv);

      /* cleanup */
      g_variant_unref (label);
      g_object_unref (menu);
      g_strfreev (strv);
    }

  /* get shortcuts editor for the menubar */
  plugin->menubar_editor = xfce_shortcuts_editor_new_array (plugin->menubar_sections, n_sections);

  /* get the prefs dialog */
  dialog = mousepad_application_get_prefs_dialog (MOUSEPAD_APPLICATION (application));
  if (dialog == NULL)
    dialog = mousepad_prefs_dialog_new ();

  /* get the notebook (switch to the plugin tab to trigger its filling if needed) */
  widget = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  list = gtk_container_get_children (GTK_CONTAINER (widget));
  notebook = list->data;
  g_list_free (list);
  n_sections = gtk_notebook_get_n_pages (notebook);
  if (! gtk_widget_get_visible (dialog))
    gtk_notebook_set_current_page (notebook, n_sections - 1);

  /* allocate the XfceShortcutsEditorSection array */
  plugin->prefs_dialog_sections = g_new (XfceShortcutsEditorSection, n_sections);
  plugin->n_prefs_dialog_sections = n_sections;

  /* walk the prefs dialog */
  for (n = 0; n < n_sections; n++)
    {
      /* allocate a sufficiently large XfceGtkActionEntry array */
      entries = g_new (XfceGtkActionEntry, n_accels);

      /* get action entries for this tab */
      widget = gtk_notebook_get_nth_page (notebook, n);
      size = 0;
      shortcuts_plugin_get_tab_entries (map, widget, entries, &size);
      plugin->prefs_dialog_sections[n].entries = g_renew (XfceGtkActionEntry, entries, size);
      plugin->prefs_dialog_sections[n].size = size;

      /* get section name for this tab */
      plugin->prefs_dialog_sections[n].section_name =
        g_strdup (gtk_notebook_get_tab_label_text (notebook, widget));
    }

  if (! gtk_widget_get_visible (dialog))
    gtk_widget_destroy (dialog);

  /* get shortcuts editor for the prefs dialog */
  plugin->prefs_dialog_editor =
    xfce_shortcuts_editor_new_array (plugin->prefs_dialog_sections, n_sections);

  /* remaining accel map entries not treated above (off-menu, hidden, ...) */
  entries = g_new (XfceGtkActionEntry, n_accels);
  list = NULL;
  gtk_accel_map_foreach (&list, shortcuts_plugin_get_misc_paths);
  list = g_list_sort (list, (GCompareFunc) g_strcmp0);
  for (lp = list, size = 0; lp != NULL; lp = lp->next, size++)
    {
      const gchar *path = lp->data, *detailed_name = path + 10, *accel;

      accel = mousepad_object_get_data (map, detailed_name);
      entries[size].menu_item_label_text = g_strdup (detailed_name);
      entries[size].accel_path = g_strdup (path);
      entries[size].default_accelerator = g_strdup (accel != NULL ? accel : "");
      entries[size].callback = shortcuts_plugin_fake_callback;
    }

  plugin->misc_entries = g_renew (XfceGtkActionEntry, entries, size);
  plugin->n_misc_entries = size;
  plugin->misc_editor = xfce_shortcuts_editor_new (4, NULL, plugin->misc_entries, size);
  g_list_free (list);

  shortcuts_plugin_set_setting_box (plugin);
}



static void
shortcuts_plugin_remove_setting_box (GtkWidget *dialog,
                                     GtkWidget *sbox)
{
  GtkWidget *dbox;

  /* preserve the setting box from destruction */
  dbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_remove (GTK_CONTAINER (dbox), sbox);
}



static void
shortcuts_plugin_show_dialog (GSimpleAction   *action,
                              GVariant        *parameter,
                              ShortcutsPlugin *plugin)
{
  MousepadPluginProvider *provider;
  GtkApplication         *application;
  GtkWidget              *dbox, *sbox;
  GtkWindow              *dialog, *parent;

  /* get the mousepad application */
  application = GTK_APPLICATION (g_application_get_default ());

  /* set the dialog */
  plugin->dialog = gtk_dialog_new ();
  dialog = GTK_WINDOW (plugin->dialog);
  parent = gtk_application_get_active_window (application);
  gtk_window_set_transient_for (dialog, parent);
  gtk_window_set_title (dialog, _("Mousepad Shortcuts"));
  mousepad_util_set_titlebar (dialog);
  gtk_window_set_default_size (dialog, 500, -1);

  /* do the main work now if not already done */
  if (plugin->menubar_sections == NULL)
    shortcuts_plugin_build_editor (plugin);

  /* pack the plugin setting box into the dialog */
  dbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  g_object_get (plugin, "provider", &provider, NULL);
  sbox = mousepad_plugin_provider_get_setting_box (provider);
  gtk_box_pack_start (GTK_BOX (dbox), sbox, TRUE, TRUE, 0);

  /* show the dialog */
  g_signal_connect (dialog, "destroy", G_CALLBACK (shortcuts_plugin_remove_setting_box), sbox);
  g_signal_connect (dialog, "destroy", G_CALLBACK (gtk_widget_destroyed), &plugin->dialog);
  gtk_window_present (dialog);
}



static void
shortcuts_plugin_set_menubar_entry (ShortcutsPlugin *plugin)
{
  GtkApplication *application;
  GMenu          *menu;
  GMenuItem      *item;
  GSimpleAction  *action;
  const gchar    *path;

  /* get the mousepad application */
  application = GTK_APPLICATION (g_application_get_default ());

  /* add shortcuts action to the application */
  action = g_simple_action_new ("shortcuts", NULL);
  g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action));
  g_object_unref (action);
  g_signal_connect (action, "activate", G_CALLBACK (shortcuts_plugin_show_dialog), plugin);

  /* add accel map entry */
  path = "<Actions>/app.shortcuts";
  if (! gtk_accel_map_lookup_entry (path, NULL))
    gtk_accel_map_add_entry (path, 0, 0);

  /* get the menubar section to which to add the shortcuts menu entry */
  menu = gtk_application_get_menu_by_id (application, "edit.preferences");

  /* add the shortcuts menu entry */
  item = g_menu_item_new (_("Shortcuts..."), "app.shortcuts");
  g_menu_item_set_attribute_value (item, "icon",
                                   g_variant_new_string ("input-keyboard"));
  g_menu_item_set_attribute_value (item, "tooltip",
                                   g_variant_new_string (_("Show the shortcuts dialog")));
  g_menu_append_item (menu, item);
  g_object_unref (item);
}



static void
shortcuts_plugin_enable (MousepadPlugin *mplugin)
{
  ShortcutsPlugin *plugin = SHORTCUTS_PLUGIN (mplugin);

  /*
   * First add the menubar entry so it is parsed by the plugin afterwards.
   * Also, in order not to slow down Mousepad unnecessarily at startup, the main work will
   * be done later, when the user actually requests the display of the shortcuts editor.
   */
  shortcuts_plugin_set_menubar_entry (plugin);
}



static void
shortcuts_plugin_free_entry_array (XfceGtkActionEntry *entries,
                                   guint               n_entries)
{
  for (guint n = 0; n < n_entries; n++)
    {
      g_free (entries[n].menu_item_label_text);
      g_free ((gchar *) entries[n].accel_path);
      g_free ((gchar *) entries[n].default_accelerator);
    }

  g_free (entries);
}



static void
shortcuts_plugin_free_section_array (XfceShortcutsEditorSection *sections,
                                     guint                       n_sections)
{
  for (guint n = 0; n < n_sections; n++)
    {
      g_free (sections[n].section_name);
      shortcuts_plugin_free_entry_array (sections[n].entries, sections[n].size);
    }

  g_free (sections);
}



static void
shortcuts_plugin_disable (MousepadPlugin *mplugin)
{
  ShortcutsPlugin *plugin = SHORTCUTS_PLUGIN (mplugin);
  GtkApplication  *application;
  GMenu           *menu;

  /* remove the shortcuts menu entry from the menubar */
  application = GTK_APPLICATION (g_application_get_default ());
  menu = gtk_application_get_menu_by_id (application, "edit.preferences");
  g_menu_remove (menu, g_menu_model_get_n_items (G_MENU_MODEL (menu)) - 1);

  /* destroy the dialog if present */
  if (plugin->dialog != NULL)
    gtk_widget_destroy (plugin->dialog);

  /* main cleanup */
  if (plugin->menubar_sections != NULL)
    {
      shortcuts_plugin_free_section_array (plugin->menubar_sections, plugin->n_menubar_sections);
      shortcuts_plugin_free_section_array (plugin->prefs_dialog_sections,
                                           plugin->n_prefs_dialog_sections);
      shortcuts_plugin_free_entry_array (plugin->misc_entries, plugin->n_misc_entries);

      gtk_widget_destroy (plugin->menubar_editor);
      gtk_widget_destroy (plugin->prefs_dialog_editor);
      gtk_widget_destroy (plugin->misc_editor);
    }
}
