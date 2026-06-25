/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2015 Jonas Kümmerlin <rgcjonas@gmail.com>
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

#include <thunar/thunar-location-bar.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-navigator.h>
#include <thunar/thunar-location-entry.h>
#include <thunar/thunar-location-buttons.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-util.h>
#include <thunar/thunar-window.h>



struct _ThunarLocationBarClass
{
  GtkBinClass __parent__;

  /* signals */
  void (*entry_done) (void);
  void (*reload_requested) (void); /* UNUSED */
};

struct _ThunarLocationBar
{
  GtkBin __parent__;

  ThunarPreferences *preferences;

  ThunarFile *current_directory;

  GtkWidget  *locationEntry;
  GtkWidget  *locationButtons;

  gboolean    is_searching;
};



enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
};


static void         thunar_location_bar_navigator_init             (ThunarNavigatorIface *iface);
static void         thunar_location_bar_finalize                   (GObject              *object);
static void         thunar_location_bar_get_property               (GObject              *object,
                                                                    guint                 prop_id,
                                                                    GValue               *value,
                                                                    GParamSpec           *pspec);
static void         thunar_location_bar_set_property               (GObject              *object,
                                                                    guint                 prop_id,
                                                                    const GValue         *value,
                                                                    GParamSpec           *pspec);
static ThunarFile  *thunar_location_bar_get_current_directory      (ThunarNavigator      *navigator);
static void         thunar_location_bar_set_current_directory      (ThunarNavigator      *navigator,
                                                                    ThunarFile           *current_directory);
static GtkWidget   *thunar_location_bar_install_widget             (ThunarLocationBar    *bar,
                                                                    GType                 type);
static void         thunar_location_bar_settings_changed           (ThunarLocationBar    *bar);
static void         thunar_location_bar_on_enry_edit_done          (ThunarLocationEntry  *entry,
                                                                    ThunarLocationBar    *bar);
static void         thunar_location_bar_request_temp_entry         (ThunarLocationBar    *bar,
                                                                    const gchar          *initial_text);


G_DEFINE_TYPE_WITH_CODE (ThunarLocationBar, thunar_location_bar, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR, thunar_location_bar_navigator_init));



GtkWidget *
thunar_location_bar_new (void)
{
  return gtk_widget_new (THUNAR_TYPE_LOCATION_BAR, NULL);
}



static void
thunar_location_bar_class_init (ThunarLocationBarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = thunar_location_bar_get_property;
  gobject_class->set_property = thunar_location_bar_set_property;
  gobject_class->finalize = thunar_location_bar_finalize;

  klass->reload_requested = NULL;

  /* Override ThunarNavigator's properties */
  g_object_class_override_property (gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

  /* install signals */

  /**
   * ThunarLocationBar::entry-done:
   * @location_bar : a #ThunarLocationBar.
   *
   * Emitted by @location_bar exactly once after an entry has been requested using
   * #thunar_location_bar_request_entry and the user has finished editing the entry.
   **/
  g_signal_new ("entry-done",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                G_STRUCT_OFFSET (ThunarLocationBarClass, entry_done),
                NULL, NULL,
                NULL,
                G_TYPE_NONE, 0);
}



static void
thunar_location_bar_init (ThunarLocationBar *bar)
{
  bar->preferences = thunar_preferences_get ();

  bar->current_directory = NULL;
  bar->locationEntry = NULL;
  bar->locationButtons = NULL;

  thunar_location_bar_settings_changed (bar);

  g_signal_connect_swapped (bar->preferences, "notify::last-location-bar", G_CALLBACK (thunar_location_bar_settings_changed), bar);
}



static void
thunar_location_bar_finalize (GObject *object)
{
  ThunarLocationBar *bar = THUNAR_LOCATION_BAR (object);

  _thunar_return_if_fail (THUNAR_IS_LOCATION_BAR (bar));

  /* disconnect from the preferences */
  g_signal_handlers_disconnect_by_func (bar->preferences, thunar_location_bar_settings_changed, bar);
  g_object_unref (bar->preferences);

  if (bar->locationEntry)
    g_object_unref (bar->locationEntry);
  if (bar->locationButtons)
    g_object_unref (bar->locationButtons);

  /* release from the current_directory */
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (bar), NULL);

  (*G_OBJECT_CLASS (thunar_location_bar_parent_class)->finalize) (object);
}



static void
thunar_location_bar_navigator_init (ThunarNavigatorIface *iface)
{
  iface->set_current_directory = thunar_location_bar_set_current_directory;
  iface->get_current_directory = thunar_location_bar_get_current_directory;
}



static void
thunar_location_bar_get_property (GObject              *object,
                                  guint                 prop_id,
                                  GValue               *value,
                                  GParamSpec           *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void thunar_location_bar_set_property   (GObject              *object,
                                                guint                 prop_id,
                                                const GValue         *value,
                                                GParamSpec           *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile *
thunar_location_bar_get_current_directory (ThunarNavigator *navigator)
{
  return THUNAR_LOCATION_BAR (navigator)->current_directory;
}



static void
thunar_location_bar_set_current_directory (ThunarNavigator      *navigator,
                                           ThunarFile           *current_directory)
{
  ThunarLocationBar *bar = THUNAR_LOCATION_BAR (navigator);
  GtkWidget         *child;

  if (bar->current_directory) g_object_unref (bar->current_directory);
  bar->current_directory = current_directory;

  if (current_directory) g_object_ref (current_directory);

  if ((child = gtk_bin_get_child (GTK_BIN (bar))) && THUNAR_IS_NAVIGATOR (child))
    thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (child), current_directory);

  g_object_notify (G_OBJECT (bar), "current-directory");
}



static GtkWidget *
thunar_location_bar_install_widget (ThunarLocationBar    *bar,
                                    GType                 type)
{
  GtkWidget *installedWidget, *child;

  /* check if the the right type is already installed */
  if ((child = gtk_bin_get_child (GTK_BIN (bar))) && G_TYPE_CHECK_INSTANCE_TYPE (child, type))
    return child;

  if (type == THUNAR_TYPE_LOCATION_ENTRY)
    {
      if (bar->locationEntry == NULL)
        {
          bar->locationEntry = gtk_widget_new (THUNAR_TYPE_LOCATION_ENTRY, "current-directory", NULL, NULL);
          g_object_ref (bar->locationEntry);
          g_signal_connect_swapped (bar->locationEntry, "change-directory", G_CALLBACK (thunar_navigator_change_directory), THUNAR_NAVIGATOR (bar));
          g_signal_connect_swapped (bar->locationEntry, "open-new-tab", G_CALLBACK (thunar_navigator_open_new_tab), THUNAR_NAVIGATOR (bar));
        }
      installedWidget = bar->locationEntry;
    }
  else
    {
      if (bar->locationButtons == NULL)
        {
          bar->locationButtons = gtk_widget_new (THUNAR_TYPE_LOCATION_BUTTONS, "current-directory", NULL, NULL);
          g_object_ref (bar->locationButtons);
          g_signal_connect_swapped (bar->locationButtons, "entry-requested", G_CALLBACK (thunar_location_bar_request_temp_entry), bar);
          g_signal_connect_swapped (bar->locationButtons, "change-directory", G_CALLBACK (thunar_navigator_change_directory), THUNAR_NAVIGATOR (bar));
          g_signal_connect_swapped (bar->locationButtons, "open-new-tab", G_CALLBACK (thunar_navigator_open_new_tab), THUNAR_NAVIGATOR (bar));
        }
      installedWidget = bar->locationButtons;
    }

  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (installedWidget), bar->current_directory);

  if ((child = gtk_bin_get_child (GTK_BIN (bar))))
    gtk_container_remove (GTK_CONTAINER (bar), child);

  gtk_container_add (GTK_CONTAINER (bar), installedWidget);
  gtk_widget_show (installedWidget);

  return installedWidget;
}



static void
thunar_location_bar_on_enry_edit_done (ThunarLocationEntry *entry,
                                       ThunarLocationBar   *bar)
{
  g_signal_handlers_disconnect_by_func (entry, thunar_location_bar_on_enry_edit_done, bar);

  thunar_location_bar_settings_changed (bar);
  g_signal_emit_by_name (bar, "entry-done");
}



static void
thunar_location_bar_request_temp_entry (ThunarLocationBar *bar,
                                        const gchar       *initial_text)
{
  thunar_location_bar_request_entry (bar, initial_text, TRUE);
}



/**
 * thunar_location_bar_request_entry
 * @bar          : The #ThunarLocationBar
 * @initial_text : The initial text to be placed inside the entry, or NULL to
 *                 use the path of the current directory.
 * @temporary_till_focus_lost : Revert to the previous location bar style when focus got lost
 *
 * Makes the location bar display an entry with the given text and places the cursor
 * accordingly. If the currently displayed location widget is a path bar, it will be
 * temporarily swapped for an entry widget and swapped back once the user completed
 * (or aborted) the input.
 */
void
thunar_location_bar_request_entry (ThunarLocationBar *bar,
                                   const gchar       *initial_text,
                                   gboolean           temporary_till_focus_lost)
{
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (bar));

  _thunar_return_if_fail (child != NULL && GTK_IS_WIDGET (child));

  if (THUNAR_IS_LOCATION_ENTRY (child))
    {
      /* already have an entry */
      thunar_location_entry_accept_focus (THUNAR_LOCATION_ENTRY (child), initial_text);
    }
  else
    {
      /* not an entry => temporarily replace it */
      child = thunar_location_bar_install_widget (bar, THUNAR_TYPE_LOCATION_ENTRY);
      thunar_location_entry_accept_focus (THUNAR_LOCATION_ENTRY (child), initial_text);
    }

  if (temporary_till_focus_lost)
    {
      thunar_location_entry_enable_edit_done_once (THUNAR_LOCATION_ENTRY (child));
      g_signal_connect (child, "edit-done", G_CALLBACK (thunar_location_bar_on_enry_edit_done), bar);
    }
}



static void
thunar_location_bar_settings_changed (ThunarLocationBar *bar)
{
  gchar *last_location_bar;
  GType  type;

  g_object_get (bar->preferences, "last-location-bar", &last_location_bar, NULL);

  /* validate it */
  if (!strcmp (last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_BUTTONS)))
    type = THUNAR_TYPE_LOCATION_BUTTONS;
  else if (!strcmp (last_location_bar, g_type_name (THUNAR_TYPE_LOCATION_ENTRY)))
    type = THUNAR_TYPE_LOCATION_ENTRY;
  else
    type = THUNAR_TYPE_LOCATION_ENTRY; /* fallback */

  g_free (last_location_bar);

  thunar_location_bar_install_widget (bar, type);
}



/**
 * thunar_location_bar_cancel_search
 * @bar          : The #ThunarLocationBar
 *
 * Cancels the search for the location bar and its children.
 */
void
thunar_location_bar_cancel_search (ThunarLocationBar *bar)
{
  if (bar->locationEntry != NULL)
    thunar_location_entry_cancel_search (THUNAR_LOCATION_ENTRY (bar->locationEntry));
  
  /* Recover the previous location bar style (BUTTONS/ENTRY) after search is done */
  thunar_location_bar_settings_changed (bar);
}



/**
 * thunar_location_bar_get_search_query:
 * @entry        : a #ThunarLocationBar.
 *
 * Returns a copy of the search query in the text field of @entry or "" if the path_entry doesn't contain
 * a search query.
 *
 * It's the responsibility of the caller to free the returned string using `g_free`.
 **/
gchar*
thunar_location_bar_get_search_query (ThunarLocationBar *entry)
{
  return (entry->locationEntry != NULL) ? thunar_location_entry_get_search_query (THUNAR_LOCATION_ENTRY (entry->locationEntry)) : g_strdup ("");
}




