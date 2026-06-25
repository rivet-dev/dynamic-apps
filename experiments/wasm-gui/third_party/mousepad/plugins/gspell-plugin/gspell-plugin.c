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
#include <mousepad/mousepad-window.h>
#include <mousepad/mousepad-view.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-settings.h>

#include <gspell-plugin/gspell-plugin.h>

#include <gspell/gspell.h>



/* for GSettings */
#define DEFAULT_LANGUAGE "plugins.gspell.preferences.default-language"



typedef struct _GspellPluginView GspellPluginView;

/* GObject virtual functions */
static void gspell_plugin_constructed          (GObject          *object);
static void gspell_plugin_finalize             (GObject          *object);

/* MousepadPlugin virtual functions */
static void gspell_plugin_enable               (MousepadPlugin   *mplugin);
static void gspell_plugin_disable              (MousepadPlugin   *mplugin);

/* GspellPlugin own functions */
static void gspell_plugin_set_state            (GspellPlugin     *plugin,
                                                gboolean          enabled,
                                                gboolean          external,
                                                GspellPluginView *view);
static void gspell_plugin_view_menu_populate   (GspellPlugin     *plugin,
                                                GtkWidget        *menu,
                                                GtkTextView      *mousepad_view);
static void gspell_plugin_view_menu_show       (GspellPlugin     *plugin,
                                                GtkWidget        *menu);
static void gspell_plugin_view_menu_deactivate (GspellPlugin     *plugin,
                                                GtkMenuShell     *menu);



struct _GspellPlugin
{
  MousepadPlugin __parent__;

  /* the plugin views list */
  GList *views;

  /* gspell and mousepad menus, retrieved from the "populate-popup" menu */
  GtkWidget *gspell_menu, *mousepad_menu;

  /* no extra padding in menus (not supposed to change at runtime) */
  gboolean old_style_menu;
};

struct _GspellPluginView
{
  /* the mousepad view the gspell objects are attached to */
  MousepadView *mousepad_view;

  /* gspell objects */
  GspellTextView   *gspell_view;
  GspellChecker    *gspell_checker;
  GspellTextBuffer *gspell_buffer;
};



G_DEFINE_DYNAMIC_TYPE (GspellPlugin, gspell_plugin, MOUSEPAD_TYPE_PLUGIN);



void
gspell_plugin_register (MousepadPluginProvider *plugin)
{
  gspell_plugin_register_type (G_TYPE_MODULE (plugin));
}



static void
gspell_plugin_class_init (GspellPluginClass *klass)
{
  GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);
  MousepadPluginClass *mplugin_class = MOUSEPAD_PLUGIN_CLASS (klass);

  gobject_class->constructed = gspell_plugin_constructed;
  gobject_class->finalize = gspell_plugin_finalize;

  mplugin_class->enable = gspell_plugin_enable;
  mplugin_class->disable = gspell_plugin_disable;
}



static void
gspell_plugin_class_finalize (GspellPluginClass *klass)
{
}



static void
gspell_plugin_init (GspellPlugin *plugin)
{
  /* initialization */
  plugin->views = NULL;
  plugin->gspell_menu = gtk_menu_new ();
  plugin->mousepad_menu = gtk_menu_new ();
  plugin->old_style_menu = MOUSEPAD_SETTING_GET_BOOLEAN (OLD_STYLE_MENU);

  /* connect signal handlers and set spell checking on all views */
  gspell_plugin_enable (MOUSEPAD_PLUGIN (plugin));
}



static void
gspell_plugin_constructed (GObject *object)
{
  const GspellLanguage   *language;
  MousepadPluginProvider *provider;
  GtkWidget              *vbox, *hbox, *widget;
  GtkListStore           *list;
  GtkCellRenderer        *cell;
  const GList            *item;
  gint                    n;

  /* request the creation of the plugin setting box */
  g_object_get (object, "provider", &provider, NULL);
  vbox = mousepad_plugin_provider_create_setting_box (provider);

  /* default language box */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

  /* combo box label */
  widget = gtk_label_new (_("Default language:"));
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, TRUE, 0);

  /* combo box model */
  list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  for (item = gspell_language_get_available (), n = 0; item != NULL; item = item->next, n++)
    {
      language = item->data;
      gtk_list_store_insert_with_values (list, NULL, n,
                                         0, gspell_language_get_name (language),
                                         1, gspell_language_get_code (language), -1);
    }

  /* create combo box */
  widget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (list));
  gtk_combo_box_set_id_column (GTK_COMBO_BOX (widget), 1);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, TRUE, 0);

  /* set combo box cell renderer */
  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), cell, "text", 0, NULL);

  /* initialize language and keep in sync with the settings */
  language = gspell_language_get_default ();
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget),
                               gspell_language_get_code (language));
  mousepad_setting_bind (DEFAULT_LANGUAGE, widget, "active-id", G_SETTINGS_BIND_DEFAULT);

  /* show all widgets in the setting box */
  gtk_widget_show_all (vbox);

  /* chain-up to MousepadPlugin */
  G_OBJECT_CLASS (gspell_plugin_parent_class)->constructed (object);
}



static void
gspell_plugin_release_view (gpointer data)
{
  GspellPluginView *view = data;

  g_object_unref (view->gspell_checker);
  g_free (view);
}



static void
gspell_plugin_finalize (GObject *object)
{
  GspellPlugin *plugin = GSPELL_PLUGIN (object);

  gspell_plugin_disable (MOUSEPAD_PLUGIN (plugin));
  g_list_free_full (plugin->views, gspell_plugin_release_view);
  gtk_widget_destroy (plugin->gspell_menu);
  gtk_widget_destroy (plugin->mousepad_menu);

  G_OBJECT_CLASS (gspell_plugin_parent_class)->finalize (object);
}



static gint
gspell_plugin_compare_view (gconstpointer item_data,
                            gconstpointer user_data)
{
  GspellPluginView *view = (GspellPluginView *) item_data;

  return view->mousepad_view != user_data;
}



static void
gspell_plugin_view_destroy (GspellPlugin *plugin,
                            GtkWidget    *mousepad_view)
{
  GList *item;

  /* retrieve the plugin view to release */
  item = g_list_find_custom (plugin->views, mousepad_view, gspell_plugin_compare_view);

  /* release data */
  gspell_plugin_release_view (item->data);
  plugin->views = g_list_delete_link (plugin->views, item);
}



static void
gspell_plugin_document_added (GspellPlugin *plugin,
                              GtkWidget    *widget)
{
  GspellPluginView     *view;
  const GspellLanguage *language;
  MousepadDocument     *document = MOUSEPAD_DOCUMENT (widget);
  GtkTextView          *text_view;
  GtkTextBuffer        *buffer;
  GList                *item = NULL;
  gchar                *language_code;

  /* (re)connect to the mousepad view "populate-popup" signal to rearrange the context menu */
  g_signal_connect_object (document->textview, "populate-popup",
                           G_CALLBACK (gspell_plugin_view_menu_populate),
                           plugin, G_CONNECT_SWAPPED);

  /* see if we have already treated this view (tab dnd, plugin re-enabled, ...) */
  if (plugin->views != NULL)
    item = g_list_find_custom (plugin->views, document->textview, gspell_plugin_compare_view);

  if (item == NULL)
    {
      /* allocate and fill in a new plugin view */
      view = g_new (GspellPluginView, 1);
      view->mousepad_view = document->textview;
      text_view = GTK_TEXT_VIEW (document->textview);
      view->gspell_view = gspell_text_view_get_from_gtk_text_view (text_view);
      buffer = gtk_text_view_get_buffer (text_view);
      view->gspell_buffer = gspell_text_buffer_get_from_gtk_text_buffer (buffer);
      view->gspell_checker = gspell_checker_new (NULL);
      plugin->views = g_list_prepend (plugin->views, view);

      /* connect to the mousepad view "destroy" signal to release our data at that time */
      g_signal_connect_object (document->textview, "destroy",
                               G_CALLBACK (gspell_plugin_view_destroy),
                               plugin, G_CONNECT_SWAPPED);

      /* initialize language from the settings */
      language_code = mousepad_setting_get_string (DEFAULT_LANGUAGE);
      if ((language = gspell_language_lookup (language_code)) != NULL)
        gspell_checker_set_language (view->gspell_checker, language);

      g_free (language_code);
    }
  else
    view = item->data;

  /* set spell checking on this view */
  gspell_plugin_set_state (plugin, TRUE, TRUE, view);
}



static void
gspell_plugin_window_added (GspellPlugin   *plugin,
                            MousepadWindow *window)
{
  GtkNotebook *notebook;
  gint         n_docs, n;

  /* get the notebook */
  notebook = GTK_NOTEBOOK (mousepad_window_get_notebook (window));

  /* connect to the "page-added" signal to be aware of future views added this way */
  g_signal_connect_object (notebook, "page-added", G_CALLBACK (gspell_plugin_document_added),
                           plugin, G_CONNECT_SWAPPED);

  /* walk the current documents list */
  n_docs = gtk_notebook_get_n_pages (notebook);
  for (n = 0; n < n_docs; n++)
    gspell_plugin_document_added (plugin, gtk_notebook_get_nth_page (notebook, n));
}



static void
gspell_plugin_enable (MousepadPlugin *mplugin)
{
  GspellPlugin        *plugin = GSPELL_PLUGIN (mplugin);
  MousepadApplication *application;
  GList               *windows, *window;

  /* get the mousepad application */
  application = MOUSEPAD_APPLICATION (g_application_get_default ());

  /* connect to the "window-added" signal to be aware of future views added this way */
  g_signal_connect_object (application, "window-added", G_CALLBACK (gspell_plugin_window_added),
                           plugin, G_CONNECT_SWAPPED);

  /* walk the current windows list */
  windows = gtk_application_get_windows (GTK_APPLICATION (application));
  for (window = windows; window != NULL; window = window->next)
    gspell_plugin_window_added (plugin, window->data);
}



static void
gspell_plugin_disable (MousepadPlugin *mplugin)
{
  GspellPlugin        *plugin = GSPELL_PLUGIN (mplugin);
  GspellPluginView    *view;
  MousepadApplication *application;
  GList               *list, *item;

  /* get the mousepad application */
  application = MOUSEPAD_APPLICATION (g_application_get_default ());

  /* disconnect signal handlers */
  mousepad_disconnect_by_func (application, gspell_plugin_window_added, plugin);

  list = gtk_application_get_windows (GTK_APPLICATION (application));
  for (item = list; item != NULL; item = item->next)
    mousepad_disconnect_by_func (mousepad_window_get_notebook (item->data),
                                 gspell_plugin_document_added, plugin);

  for (item = plugin->views; item != NULL; item = item->next)
    {
      /* don't disconnect from "destroy": cleanup must be done in all cases */
      view = item->data;
      mousepad_disconnect_by_func (view->mousepad_view,
                                   gspell_plugin_view_menu_populate, plugin);

      /* unset spell checking on this view */
      gspell_plugin_set_state (plugin, FALSE, TRUE, view);
    }
}



static void
gspell_plugin_set_state (GspellPlugin     *plugin,
                         gboolean          enabled,
                         gboolean          external,
                         GspellPluginView *view)
{
  /* the function call comes from a change in mousepad settings, not from
   * gspell_plugin_view_menu_show() to retrieve the gspell menu */
  if (external)
    gspell_text_view_set_inline_spell_checking (view->gspell_view, enabled);

  gspell_text_buffer_set_spell_checker (view->gspell_buffer, enabled ? view->gspell_checker : NULL);
  gspell_text_view_set_enable_language_menu (view->gspell_view, enabled);
}



static void
gspell_plugin_view_menu_populate (GspellPlugin *plugin,
                                  GtkWidget    *menu,
                                  GtkTextView  *mousepad_view)
{
  GtkWidget *window;
  guint      signal_id;

  /* connect to the context menu "show" signal to get it after mousepad replaced it */
  g_signal_connect_object (menu, "show", G_CALLBACK (gspell_plugin_view_menu_show),
                           plugin, G_CONNECT_SWAPPED);

  /* connect to the context menu "deactivate" signal to store the gspell menu at that time */
  g_signal_connect_object (menu, "deactivate", G_CALLBACK (gspell_plugin_view_menu_deactivate),
                           plugin, G_CONNECT_SWAPPED);

  /* block mousepad signal handlers connected to the context menu "deactivate" signal,
   * so that we have time to store the gspell menu before the context menu is cleared */
  signal_id = g_signal_lookup ("deactivate", GTK_TYPE_MENU_SHELL);
  window = gtk_widget_get_ancestor (GTK_WIDGET (mousepad_view), MOUSEPAD_TYPE_WINDOW);
  g_signal_handlers_block_matched (menu, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                   signal_id, 0, NULL, NULL, window);
}



/* Subtract menu_2 from menu_1 */
static void
gspell_plugin_view_menu_subtract (GtkMenu *menu_1,
                                  GtkMenu *menu_2)
{
  GList       *list_1, *list_2, *iter_1, *iter_2;
  const gchar *label;

  g_return_if_fail (GTK_IS_MENU (menu_1));
  g_return_if_fail (GTK_IS_MENU (menu_2));

  list_1 = gtk_container_get_children (GTK_CONTAINER (menu_1));
  list_2 = gtk_container_get_children (GTK_CONTAINER (menu_2));

  for (iter_1 = list_1; iter_1 != NULL; iter_1 = iter_1->next)
    {
      label = gtk_menu_item_get_label (iter_1->data);
      for (iter_2 = list_2; iter_2 != NULL; iter_2 = iter_2->next)
        {
          if (g_strcmp0 (label, gtk_menu_item_get_label (iter_2->data)) == 0)
            {
              gtk_container_remove (GTK_CONTAINER (menu_1), iter_1->data);
              break;
            }
        }
    }

  g_list_free (list_1);
  g_list_free (list_2);
}



static void
gspell_plugin_view_menu_show (GspellPlugin *plugin,
                              GtkWidget    *menu)
{
  GspellPluginView *view;
  GtkWidget        *mousepad_view, *window, *item;
  GList            *children, *child;
  guint             signal_id, index;

  /* disconnect this handler and the "populate-popup" handler */
  mousepad_disconnect_by_func (menu, gspell_plugin_view_menu_show, plugin);
  mousepad_view = gtk_menu_get_attach_widget (GTK_MENU (menu));
  mousepad_disconnect_by_func (mousepad_view, gspell_plugin_view_menu_populate, plugin);

  /* block mousepad signal handlers connected to this view "populate-popup" signal, so that
   * they don't interfer while we play with this signal to retrieve the gspell menu */
  signal_id = g_signal_lookup ("populate-popup", GTK_TYPE_TEXT_VIEW);
  window = gtk_widget_get_ancestor (mousepad_view, MOUSEPAD_TYPE_WINDOW);
  g_signal_handlers_block_matched (mousepad_view, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                   signal_id, 0, NULL, NULL, window);

  /* retrieve the gspell menu by comparison: send the "populate-popup" signal with gspell
   * enabled / disabled and subtract the resulting menus */

  /* store the mousepad context menu */
  mousepad_util_container_move_children (GTK_CONTAINER (menu),
                                         GTK_CONTAINER (plugin->mousepad_menu));

  /* first populate the context menu with spell checking enabled */
  g_signal_emit (mousepad_view, signal_id, 0, menu);

  /* clear old gspell menu (must be done after the new one is created by gspell above) */
  mousepad_util_container_clear (GTK_CONTAINER (plugin->gspell_menu));

  /* then populate the plugin menu with spell checking disabled */
  view = g_list_find_custom (plugin->views, mousepad_view, gspell_plugin_compare_view)->data;
  gspell_plugin_set_state (plugin, FALSE, FALSE, view);
  g_signal_emit (mousepad_view, signal_id, 0, plugin->gspell_menu);
  gspell_plugin_set_state (plugin, TRUE, FALSE, view);

  /* finally substract the second menu from the first one to keep only the gspell menu */
  gspell_plugin_view_menu_subtract (GTK_MENU (menu), GTK_MENU (plugin->gspell_menu));
  mousepad_util_container_clear (GTK_CONTAINER (plugin->gspell_menu));

  /* realign menu items */
  if (plugin->old_style_menu)
    {
      children = gtk_container_get_children (GTK_CONTAINER (menu));
      for (child = children, index = 0; child != NULL; child = child->next, index++)
        mousepad_window_menu_item_realign (MOUSEPAD_WINDOW (window),
                                           child->data, NULL, menu, index);

      g_list_free (children);
    }

  /* append the mousepad context menu to the context menu to be shown */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);
  mousepad_util_container_move_children (GTK_CONTAINER (plugin->mousepad_menu),
                                         GTK_CONTAINER (menu));

  /* unblock mousepad "populate-popup" handlers and reconnect the plugin handler */
  g_signal_handlers_unblock_matched (mousepad_view, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                     signal_id, 0, NULL, NULL, window);
  g_signal_connect_object (mousepad_view, "populate-popup",
                           G_CALLBACK (gspell_plugin_view_menu_populate),
                           plugin, G_CONNECT_SWAPPED);
}



static void
gspell_plugin_view_menu_move_sections (GtkMenu *source,
                                       GtkMenu *destination,
                                       gint     sections)
{
  GtkWidget *tmp;
  GList     *list, *iter;

  g_return_if_fail (GTK_IS_MENU (source));
  g_return_if_fail (GTK_IS_MENU (destination));

  list = gtk_container_get_children (GTK_CONTAINER (source));

  for (iter = list; iter != NULL; iter = iter->next)
    {
      tmp = g_object_ref (iter->data);

      gtk_container_remove (GTK_CONTAINER (source), tmp);
      gtk_container_add (GTK_CONTAINER (destination), tmp);
      g_object_unref (tmp);

      if (GTK_IS_SEPARATOR_MENU_ITEM (iter->data) && ! --sections)
        break;
    }

  g_list_free (list);
}



static void
gspell_plugin_view_menu_deactivate (GspellPlugin *plugin,
                                    GtkMenuShell *menu)
{
  GtkWidget *mousepad_view, *window;
  guint      signal_id;

  /* disconnect this handler */
  mousepad_disconnect_by_func (menu, gspell_plugin_view_menu_deactivate, plugin);

  /* store the gspell section of the context menu until next "populate-popup" signal */
  gspell_plugin_view_menu_move_sections (GTK_MENU (menu), GTK_MENU (plugin->gspell_menu), 1);

  /* unblock mousepad signal handlers connected to the context menu "deactivate" signal */
  signal_id = g_signal_lookup ("deactivate", GTK_TYPE_MENU_SHELL);
  mousepad_view = gtk_menu_get_attach_widget (GTK_MENU (menu));
  window = gtk_widget_get_ancestor (mousepad_view, MOUSEPAD_TYPE_WINDOW);
  g_signal_handlers_unblock_matched (menu, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                     signal_id, 0, NULL, NULL, window);

  /* re-emit the signal for these now unblocked handlers */
  g_signal_emit (menu, signal_id, 0);
}
