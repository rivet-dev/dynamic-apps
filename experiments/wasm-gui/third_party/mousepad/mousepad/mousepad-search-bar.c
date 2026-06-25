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
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-search-bar.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-window.h>
#include <mousepad/mousepad-history.h>



static void      mousepad_search_bar_finalize                   (GObject                 *object);
static void      mousepad_search_bar_find_string                (MousepadSearchBar       *bar,
                                                                 MousepadSearchFlags      flags);
static void      mousepad_search_bar_search_completed           (MousepadSearchBar       *bar,
                                                                 gint                     cur_match,
                                                                 gint                     n_matches,
                                                                 const gchar             *search_string,
                                                                 MousepadSearchFlags      flags);
static void      mousepad_search_bar_hide_clicked               (MousepadSearchBar       *bar);
static void      mousepad_search_bar_entry_activate             (MousepadSearchBar       *bar);
static void      mousepad_search_bar_entry_activate_backward    (MousepadSearchBar       *bar);
static void      mousepad_search_bar_entry_changed              (MousepadSearchBar       *bar);
static void      mousepad_search_bar_setting_changed            (MousepadSearchBar       *bar);



enum
{
  HIDE_BAR,
  SEARCH,
  LAST_SIGNAL
};

struct _MousepadSearchBar
{
  GtkToolbar      __parent__;

  /* bar widgets */
  GtkWidget *box;
  GtkWidget *entry;
  GtkWidget *hits_label;
  GtkWidget *spinner;
};



static guint search_bar_signals[LAST_SIGNAL];



GtkWidget *
mousepad_search_bar_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_SEARCH_BAR,
                       "toolbar-style", GTK_TOOLBAR_BOTH_HORIZ,
                       "icon-size", GTK_ICON_SIZE_MENU,
                       NULL);
}



G_DEFINE_TYPE (MousepadSearchBar, mousepad_search_bar, GTK_TYPE_TOOLBAR)



static void
mousepad_search_bar_class_init (MousepadSearchBarClass *klass)
{
  GObjectClass  *gobject_class;
  GObjectClass  *entry_class;
  GtkBindingSet *binding_set;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_search_bar_finalize;

  /* signals */
  search_bar_signals[HIDE_BAR] =
    g_signal_new (I_("hide-bar"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  search_bar_signals[SEARCH] =
    g_signal_new (I_("search"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _mousepad_marshal_VOID__FLAGS_STRING_STRING,
                  G_TYPE_NONE, 3,
                  MOUSEPAD_TYPE_SEARCH_FLAGS,
                  G_TYPE_STRING, G_TYPE_STRING);

  /* setup key bindings for the search bar */
  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "hide-bar", 0);

  /* add an activate-backwards signal to GtkEntry */
  entry_class = g_type_class_ref (GTK_TYPE_ENTRY);
  binding_set = gtk_binding_set_by_class (entry_class);
  if (G_LIKELY (g_signal_lookup ("activate-backward", GTK_TYPE_ENTRY) == 0))
    {
      g_signal_new ("activate-backward", GTK_TYPE_ENTRY, G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return,
                                    GDK_SHIFT_MASK, "activate-backward", 0);
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter,
                                    GDK_SHIFT_MASK, "activate-backward", 0);
    }

  /* add a select-all signal to GtkEntry */
  if (G_LIKELY (g_signal_lookup ("select-all", GTK_TYPE_ENTRY) == 0))
    {
      g_signal_new ("select-all", GTK_TYPE_ENTRY, G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK, "select-all", 0);
    }

  /* cleanup */
  g_type_class_unref (entry_class);
}



static void
mousepad_search_bar_entry_select_all (GtkEntry *entry)
{
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
}



static void
mousepad_search_bar_hide_box_button (GtkWidget *widget,
                                     gpointer   data)
{
  if (GTK_IS_BOX (widget))
    gtk_container_forall (GTK_CONTAINER (widget), mousepad_search_bar_hide_box_button, NULL);
  else if (GTK_IS_BUTTON (widget))
    gtk_widget_hide (widget);
}



static void
mousepad_search_bar_post_init (MousepadSearchBar *bar)
{
  GtkApplication   *application;
  GtkWidget        *window;
  GtkBindingSet    *binding_set;
  GdkModifierType   accel_mods;
  guint             n, accel_key;
  gchar           **accels;
  const gchar      *actions[] = { "win.edit.cut", "win.edit.copy", "win.edit.paste",
                                  "win.edit.select-all" };
  const gchar      *signals[] = { "cut-clipboard", "copy-clipboard", "paste-clipboard",
                                  "select-all" };

  /* disconnect this handler */
  mousepad_disconnect_by_func (bar, mousepad_search_bar_post_init, NULL);

  /* get the ancestor MousepadWindow and the application */
  window = gtk_widget_get_ancestor (GTK_WIDGET (bar), MOUSEPAD_TYPE_WINDOW);
  application = gtk_window_get_application (GTK_WINDOW (window));

  /* connect to the "search-completed" window signal */
  g_signal_connect_object (window, "search-completed",
                           G_CALLBACK (mousepad_search_bar_search_completed),
                           bar, G_CONNECT_SWAPPED);

  /* make search entry keybindings consistent with those of the text view */
  binding_set = gtk_binding_set_by_class (g_type_class_peek (GTK_TYPE_ENTRY));

  accels = gtk_application_get_accels_for_action (application, "win.edit.delete-selection");
  if (accels[0] != NULL)
    {
      gtk_accelerator_parse (accels[0], &accel_key, &accel_mods);
      gtk_binding_entry_remove (binding_set, accel_key, accel_mods);
      gtk_binding_entry_add_signal (binding_set, accel_key, accel_mods, "delete-from-cursor",
                                    2, GTK_TYPE_DELETE_TYPE, GTK_DELETE_CHARS, G_TYPE_INT, 1);
    }

  g_strfreev (accels);

  for (n = 0; n < G_N_ELEMENTS (actions); n++)
    {
      accels = gtk_application_get_accels_for_action (application, actions[n]);
      if (accels[0] != NULL)
        {
          gtk_accelerator_parse (accels[0], &accel_key, &accel_mods);
          gtk_binding_entry_remove (binding_set, accel_key, accel_mods);
          gtk_binding_entry_add_signal (binding_set, accel_key, accel_mods, signals[n], 0);
        }

      g_strfreev (accels);
    }
}



static void
mousepad_search_bar_init (MousepadSearchBar *bar)
{
  GtkWidget      *widget, *box, *menu_item;
  GtkToolItem    *item;
  GtkCssProvider *provider;
  const gchar    *css_string;

  /* we will complete initialization when the bar is anchored */
  g_signal_connect (bar, "hierarchy-changed", G_CALLBACK (mousepad_search_bar_post_init), NULL);

  /* the close button */
  widget = gtk_button_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU);
  gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
  g_signal_connect_swapped (widget, "clicked", G_CALLBACK (mousepad_search_bar_hide_clicked), bar);

  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (item), widget);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);

  /* box for the search entry and its buttons */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_LINKED);
  gtk_widget_set_margin_end (box, 6);

  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (item), box);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);

  /* the entry field */
  bar->box = gtk_combo_box_text_new_with_entry ();
  gtk_container_forall (GTK_CONTAINER (bar->box), mousepad_search_bar_hide_box_button, NULL);
  gtk_box_pack_start (GTK_BOX (box), bar->box, FALSE, FALSE, 0);

  bar->entry = gtk_bin_get_child (GTK_BIN (bar->box));
  g_signal_connect_swapped (bar->entry, "changed",
                            G_CALLBACK (mousepad_search_bar_entry_changed), bar);
  g_signal_connect_swapped (bar->entry, "activate",
                            G_CALLBACK (mousepad_search_bar_entry_activate), bar);
  g_signal_connect_swapped (bar->entry, "activate-backward",
                            G_CALLBACK (mousepad_search_bar_entry_activate_backward), bar);
  g_signal_connect (bar->entry, "select-all",
                    G_CALLBACK (mousepad_search_bar_entry_select_all), NULL);

  /* recover entry shape after hiding the combo box button */
  provider = gtk_css_provider_new ();
  css_string = "entry { border-top-right-radius: 0; border-bottom-right-radius: 0; }";
  gtk_css_provider_load_from_data (provider, css_string, -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (bar->entry),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  /* previous button */
  widget = gtk_button_new_from_icon_name ("go-up-symbolic", GTK_ICON_SIZE_MENU);
  gtk_widget_set_can_focus (widget, FALSE);
  g_signal_connect_swapped (widget, "clicked", G_CALLBACK (mousepad_search_bar_find_previous), bar);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

  /* next button */
  widget = gtk_button_new_from_icon_name ("go-down-symbolic", GTK_ICON_SIZE_MENU);
  gtk_widget_set_can_focus (widget, FALSE);
  g_signal_connect_swapped (widget, "clicked", G_CALLBACK (mousepad_search_bar_find_next), bar);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

  /* check button for case sensitive, including the proxy menu item */
  widget = gtk_check_button_new_with_mnemonic (_("Match _case"));
  MOUSEPAD_SETTING_BIND (SEARCH_MATCH_CASE, widget, "active", G_SETTINGS_BIND_DEFAULT);
  g_signal_connect_swapped (widget, "toggled", G_CALLBACK (mousepad_search_bar_setting_changed), bar);

  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (item), widget);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);

  menu_item = gtk_check_menu_item_new_with_mnemonic (_("Match _case"));
  gtk_tool_item_set_proxy_menu_item (item, "case-sensitive", menu_item);
  g_object_bind_property (widget, "active", menu_item, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  /* check button for matching whole words, including the proxy menu item */
  widget = gtk_check_button_new_with_mnemonic (_("_Match whole word"));
  MOUSEPAD_SETTING_BIND (SEARCH_MATCH_WHOLE_WORD, widget, "active", G_SETTINGS_BIND_DEFAULT);
  g_signal_connect_swapped (widget, "toggled", G_CALLBACK (mousepad_search_bar_setting_changed), bar);

  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (item), widget);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);

  menu_item = gtk_check_menu_item_new_with_mnemonic (_("_Match whole word"));
  gtk_tool_item_set_proxy_menu_item (item, "match-whole-word", menu_item);
  g_object_bind_property (widget, "active", menu_item, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  /* check button for enabling regex, including the proxy menu item */
  widget = gtk_check_button_new_with_mnemonic (_("Regular e_xpression"));
  MOUSEPAD_SETTING_BIND (SEARCH_ENABLE_REGEX, widget, "active", G_SETTINGS_BIND_DEFAULT);
  g_signal_connect_swapped (widget, "toggled", G_CALLBACK (mousepad_search_bar_setting_changed), bar);

  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (item), widget);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);

  menu_item = gtk_check_menu_item_new_with_mnemonic (_("Regular e_xpression"));
  gtk_tool_item_set_proxy_menu_item (item, "enable-regex", menu_item);
  g_object_bind_property (widget, "active", menu_item, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  /* the occurrences label */
  bar->hits_label = gtk_label_new (NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (bar->hits_label),
                               GTK_STYLE_CLASS_DIM_LABEL);

  item = gtk_tool_item_new ();
  gtk_widget_set_margin_start (GTK_WIDGET (item), 6);
  gtk_container_add (GTK_CONTAINER (item), bar->hits_label);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);

  /* the spinner */
  bar->spinner = gtk_spinner_new ();

  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (item), bar->spinner);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);

  /* overflow menu item for the spinner and the occurrences label */
  menu_item = gtk_menu_item_new ();
  gtk_tool_item_set_proxy_menu_item (item, "hits-label", menu_item);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (menu_item), box);

  widget = gtk_label_new (NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (widget), GTK_STYLE_CLASS_DIM_LABEL);
  g_object_bind_property (bar->hits_label, "label", widget, "label", G_BINDING_DEFAULT);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

  widget = gtk_spinner_new ();
  g_object_bind_property (bar->spinner, "active", widget, "active", G_BINDING_DEFAULT);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

  /* show all widgets but the search bar */
  gtk_widget_show_all (GTK_WIDGET (bar));
  gtk_widget_hide (GTK_WIDGET (bar));

  /* reset search box history on show/hide */
  g_signal_connect_swapped (bar, "show",
                            G_CALLBACK (mousepad_history_search_fill_search_box), bar->box);
  g_signal_connect_swapped (bar, "hide", G_CALLBACK (gtk_combo_box_text_remove_all), bar->box);
}



static void
mousepad_search_bar_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (mousepad_search_bar_parent_class)->finalize) (object);
}



static void
mousepad_search_bar_reset_display (MousepadSearchBar *bar)
{
  const gchar *string;

  /* reset occurrences label */
  gtk_label_set_text (GTK_LABEL (bar->hits_label), NULL);

  /* start the spinner or reset entry color */
  string = gtk_entry_get_text (GTK_ENTRY (bar->entry));
  if (string != NULL && *string != '\0')
    gtk_spinner_start (GTK_SPINNER (bar->spinner));
  else
    mousepad_util_entry_error (bar->entry, FALSE);
}



static void
mousepad_search_bar_find_string (MousepadSearchBar   *bar,
                                 MousepadSearchFlags  flags)
{
  const gchar *string;
  guint        idx;

  /* always true when using the search bar */
  flags |= MOUSEPAD_SEARCH_FLAGS_WRAP_AROUND;

  /* always select unless it is a silent search */
  if (! (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_NONE))
    flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT;

  /* get the entry string */
  string = gtk_entry_get_text (GTK_ENTRY (bar->entry));

  /* update search history in case of an explicit search */
  if (! (flags & MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START)
      || ! (flags & MOUSEPAD_SEARCH_FLAGS_DIR_FORWARD))
    {
      GtkComboBoxText *box = GTK_COMBO_BOX_TEXT (bar->box);

      idx = mousepad_history_search_insert_search_text (string);
      if (idx > 0)
        {
          gtk_combo_box_text_prepend_text (box, string);
          gtk_combo_box_text_remove (box, idx);

          /* always be in box: avoid `idx == -1` and `idx == history_size` */
          gtk_combo_box_set_active (GTK_COMBO_BOX (box), 0);
        }
    }

  /* reset display widgets */
  mousepad_search_bar_reset_display (bar);

  /* emit signal */
  g_signal_emit (bar, search_bar_signals[SEARCH], 0, flags, string, NULL);
}



static void
mousepad_search_bar_search_completed (MousepadSearchBar   *bar,
                                      gint                 cur_match,
                                      gint                 n_matches,
                                      const gchar         *search_string,
                                      MousepadSearchFlags  flags)
{
  gchar       *message;
  const gchar *string;

  /* stop the spinner */
  gtk_spinner_stop (GTK_SPINNER (bar->spinner));

  /* get the entry string */
  string = gtk_entry_get_text (GTK_ENTRY (bar->entry));

  /* leave the search bar unchanged if the search was launched from the replace dialog
   * for a different string or irrelevant settings for the search bar*/
  if (g_strcmp0 (string, search_string) != 0
      || (flags & MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS)
      || (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION))
    return;

  if (string != NULL && *string != '\0')
    {
      /* update entry color */
      mousepad_util_entry_error (bar->entry, n_matches == 0);

      /* update counter */
      if (cur_match != 0)
        message = g_strdup_printf (ngettext ("%d of %d match", "%d of %d matches", n_matches),
                                   cur_match, n_matches);
      else
        message = g_strdup_printf (ngettext ("%d match", "%d matches", n_matches),
                                   n_matches);
      gtk_label_set_markup (GTK_LABEL (bar->hits_label), message);
      g_free (message);
    }
}



static void
mousepad_search_bar_hide_clicked (MousepadSearchBar *bar)
{
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* emit the signal */
  g_signal_emit (bar, search_bar_signals[HIDE_BAR], 0);
}



static void
mousepad_search_bar_entry_activate (MousepadSearchBar *bar)
{
  mousepad_search_bar_find_next (bar);
}



static void
mousepad_search_bar_entry_activate_backward (MousepadSearchBar *bar)
{
  mousepad_search_bar_find_previous (bar);
}



static void
mousepad_search_bar_entry_changed (MousepadSearchBar *bar)
{
  MousepadSearchFlags flags;

  /* set the search flags */
  flags = MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START
          | MOUSEPAD_SEARCH_FLAGS_DIR_FORWARD;

  if (! MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_INCREMENTAL))
    flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_NONE;

  /* find */
  mousepad_search_bar_find_string (bar, flags);
}



static gboolean
mousepad_search_bar_setting_changed_idle (gpointer bar)
{
  mousepad_search_bar_entry_changed (bar);

  return FALSE;
}



static void
mousepad_search_bar_setting_changed (MousepadSearchBar *bar)
{
  /* allow time for the search context settings to synchronize with those of Mousepad */
  g_idle_add (mousepad_search_bar_setting_changed_idle, mousepad_util_source_autoremove (bar));
}



void
mousepad_search_bar_focus (MousepadSearchBar *bar)
{
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* focus the entry field */
  gtk_widget_grab_focus (bar->entry);

  /* select the entire entry */
  gtk_editable_select_region (GTK_EDITABLE (bar->entry), 0, -1);
}



void
mousepad_search_bar_find_next (MousepadSearchBar *bar)
{
  MousepadSearchFlags flags;

  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* set search flags */
  flags = MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END
          | MOUSEPAD_SEARCH_FLAGS_DIR_FORWARD;

  /* search */
  mousepad_search_bar_find_string (bar, flags);
}



void
mousepad_search_bar_find_previous (MousepadSearchBar *bar)
{
  MousepadSearchFlags flags;

  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* set search flags */
  flags = MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START
          | MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD;

  /* search */
  mousepad_search_bar_find_string (bar, flags);
}



void
mousepad_search_bar_page_switched (MousepadSearchBar *bar,
                                   GtkTextBuffer     *old_buffer,
                                   GtkTextBuffer     *new_buffer,
                                   gboolean           search)
{
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* disconnect from old buffer signals */
  if (old_buffer != NULL)
    mousepad_disconnect_by_func (old_buffer, mousepad_search_bar_reset_display, bar);

  /* connect to new buffer signals to update display widgets on change */
  g_signal_connect_object (new_buffer, "insert-text",
                           G_CALLBACK (mousepad_search_bar_reset_display),
                           bar, G_CONNECT_SWAPPED);
  g_signal_connect_object (new_buffer, "delete-range",
                           G_CALLBACK (mousepad_search_bar_reset_display),
                           bar, G_CONNECT_SWAPPED);

  /* run a silent search */
  if (search)
    mousepad_search_bar_find_string (bar, MOUSEPAD_SEARCH_FLAGS_ACTION_NONE);
}



void
mousepad_search_bar_set_text (MousepadSearchBar *bar,
                              const gchar       *text)
{
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  gtk_entry_set_text (GTK_ENTRY (bar->entry), text);
}
