/*
Copyright 2010 Julien Lavergne <gilir@ubuntu.com>

Based on indicator-applet :
Copyright 2009 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3.0 as published by the Free Software Foundation.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License version 3.0 for more details.

You should have received a copy of the GNU General Public
License along with this library. If not, see
<http://www.gnu.org/licenses/>.

TODO Check also http://bazaar.launchpad.net/~unity-team/unity/trunk/view/head:/services/panel-service.c

TODO ? : add hotkey support (r348 + r352)

TODO : vertical support (r354)

*/

#include "plugin.h"

#include <stdlib.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <libindicator/indicator-object.h>

static gchar * indicator_order[][2] = {
  {"libappmenu.so", NULL},
  {"libapplication.so", NULL},
  {"libapplication.so", "gst-keyboard-xkb"},
  {"libmessaging.so", NULL},
  {"libpower.so", NULL},
  {"libapplication.so", "bluetooth-manager"},
  {"libnetwork.so", NULL},
  {"libnetworkmenu.so", NULL},
  {"libapplication.so", "nm-applet"},
  {"libsoundmenu.so", NULL},
  {"libdatetime.so", NULL},
  {"libsession.so", NULL},
  {NULL, NULL}
};

#define  MENU_DATA_BOX               "box"
#define  MENU_DATA_INDICATOR_OBJECT  "indicator-object"
#define  MENU_DATA_INDICATOR_ENTRY   "indicator-entry"
#define  MENU_DATA_IN_MENUITEM       "in-menuitem"
#define  MENU_DATA_MENUITEM_PRESSED  "menuitem-pressed"

#define  IO_DATA_NAME                "indicator-name"
#define  IO_DATA_ORDER_NUMBER        "indicator-order-number"

#define LOG_FILE_NAME  "lxpanel-indicator-plugin.log"

GOutputStream * log_file = NULL;

typedef struct {
    LXPanel *panel;
    config_setting_t *settings;

    IndicatorObject *io;		/* Indicators applets */

    GList *images;				/* List of images of applets */
    GList *menus;				/* List of menus of applets */

    GtkWidget * menubar;		/* Displayed menubar */

    gboolean applications;      /* Support for differents indicators */
    gboolean datetime;
    gboolean me;
    gboolean messages;
    gboolean network;
    gboolean session;
    gboolean sound;
    /* gboolean appmenu; */


} IndicatorPlugin;

static const gchar * indicator_env[] = {
  "indicator-applet",
  NULL
};

static void indicator_destructor(gpointer user_data);

static gint
name2order (const gchar * name, const gchar * hint) {
  int i;

  for (i = 0; indicator_order[i][0] != NULL; i++) {
    if (g_strcmp0(name, indicator_order[i][0]) == 0 &&
        g_strcmp0(hint, indicator_order[i][1]) == 0) {
      return i;
    }
  }

  return -1;
}

typedef struct _incoming_position_t incoming_position_t;
struct _incoming_position_t {
        gint objposition;
        gint entryposition;
        gint menupos;
        gboolean found;
};

/* This function helps by determining where in the menu list
   this new entry should be placed.  It compares the objects
   that they're on, and then the individual entries.  Each
   is progressively more expensive. */
static void
place_in_menu_cb (GtkWidget * widget, gpointer user_data)
{
  incoming_position_t * position = (incoming_position_t *)user_data;
  if (position->found) {
    /* We've already been placed, just finish the foreach */
    return;
  }

  IndicatorObject * io = INDICATOR_OBJECT(g_object_get_data(G_OBJECT(widget), MENU_DATA_INDICATOR_OBJECT));
  g_return_if_fail(INDICATOR_IS_OBJECT(io));

  gint objposition = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(io), IO_DATA_ORDER_NUMBER));
  /* We've already passed it, well, then this is where
     we should be be.  Stop! */
  if (objposition > position->objposition) {
    position->found = TRUE;
    return;
  }

  /* The objects don't match yet, keep looking */
  if (objposition < position->objposition) {
    position->menupos++;
    return;
  }

  /* The objects are the same, let's start looking at entries. */
  IndicatorObjectEntry * entry = (IndicatorObjectEntry *)g_object_get_data(G_OBJECT(widget), MENU_DATA_INDICATOR_ENTRY);
  gint entryposition = indicator_object_get_location(io, entry);

  if (entryposition > position->entryposition) {
    position->found = TRUE;
    return;
  }

  if (entryposition < position->entryposition) {
    position->menupos++;
    return;
  }

  /* We've got the same object and the same entry.  Well,
     let's just put it right here then. */
  position->found = TRUE;
  return;
}

/* Position the entry */
static void
place_in_menu (GtkWidget *menubar,
               GtkWidget *menuitem,
               IndicatorObject *io,
               IndicatorObjectEntry *entry)
{
  incoming_position_t position;

  /* Start with the default position for this indicator object */
  gint io_position = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(io), IO_DATA_ORDER_NUMBER));

  /* If name-hint is set, try to find the entry's position */
  if (entry->name_hint != NULL) {
    const gchar *name = (const gchar *)g_object_get_data(G_OBJECT(io), IO_DATA_NAME);
    gint entry_position = name2order(name, entry->name_hint);

    /* If we don't find the entry, fall back to the indicator object's position */
    if (entry_position > -1)
      io_position = entry_position;
  }

  position.objposition = io_position;
  position.entryposition = indicator_object_get_location(io, entry);
  position.menupos = 0;
  position.found = FALSE;

  gtk_container_foreach(GTK_CONTAINER(menubar), place_in_menu_cb, &position);

  gtk_menu_shell_insert(GTK_MENU_SHELL(menubar), menuitem, position.menupos);
}

static void
something_shown (GtkWidget * widget, gpointer user_data)
{
        GtkWidget * menuitem = GTK_WIDGET(user_data);
        gtk_widget_show(menuitem);
}

static void
something_hidden (GtkWidget * widget, gpointer user_data)
{
        GtkWidget * menuitem = GTK_WIDGET(user_data);
        gtk_widget_hide(menuitem);
}

static void
sensitive_cb (GObject * obj, GParamSpec * pspec, gpointer user_data)
{
    g_return_if_fail(GTK_IS_WIDGET(obj));
    g_return_if_fail(GTK_IS_WIDGET(user_data));

    gtk_widget_set_sensitive(GTK_WIDGET(user_data), gtk_widget_get_sensitive(GTK_WIDGET(obj)));
    return;
}

static void
entry_activated (GtkWidget * widget, gpointer user_data)
{
  g_return_if_fail(GTK_IS_WIDGET(widget));

  IndicatorObject *io = g_object_get_data (G_OBJECT (widget), MENU_DATA_INDICATOR_OBJECT);
  IndicatorObjectEntry *entry = g_object_get_data (G_OBJECT (widget), MENU_DATA_INDICATOR_ENTRY);

  g_return_if_fail(INDICATOR_IS_OBJECT(io));

  return indicator_object_entry_activate(io, entry, gtk_get_current_event_time());
}

static gboolean
entry_secondary_activated (GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
  g_return_val_if_fail(GTK_IS_WIDGET(widget), FALSE);

  switch (event->type) {
    case GDK_ENTER_NOTIFY:
      g_object_set_data(G_OBJECT(widget), MENU_DATA_IN_MENUITEM, GINT_TO_POINTER(TRUE));
      break;

    case GDK_LEAVE_NOTIFY:
      g_object_set_data(G_OBJECT(widget), MENU_DATA_IN_MENUITEM, GINT_TO_POINTER(FALSE));
      g_object_set_data(G_OBJECT(widget), MENU_DATA_MENUITEM_PRESSED, GINT_TO_POINTER(FALSE));
      break;

    case GDK_BUTTON_PRESS:
      if (event->button.button == 2) {
        g_object_set_data(G_OBJECT(widget), MENU_DATA_MENUITEM_PRESSED, GINT_TO_POINTER(TRUE));
      }
      break;

    case GDK_BUTTON_RELEASE:
      if (event->button.button == 2) {
        gboolean in_menuitem = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), MENU_DATA_IN_MENUITEM));
        gboolean menuitem_pressed = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), MENU_DATA_MENUITEM_PRESSED));

        if (in_menuitem && menuitem_pressed) {
          g_object_set_data(G_OBJECT(widget), MENU_DATA_MENUITEM_PRESSED, GINT_TO_POINTER(FALSE));

          IndicatorObject *io = g_object_get_data(G_OBJECT(widget), MENU_DATA_INDICATOR_OBJECT);
          IndicatorObjectEntry *entry = g_object_get_data(G_OBJECT(widget), MENU_DATA_INDICATOR_ENTRY);

          g_return_val_if_fail(INDICATOR_IS_OBJECT(io), FALSE);

          g_signal_emit_by_name(io, INDICATOR_OBJECT_SIGNAL_SECONDARY_ACTIVATE,
              entry, event->button.time);
        }
      }
      break;
    default: ;
  }

  return FALSE;
}

static gboolean
entry_scrolled (GtkWidget *menuitem, GdkEventScroll *event, gpointer data)
{
  g_return_val_if_fail(GTK_IS_WIDGET(menuitem), FALSE);

  IndicatorObject *io = g_object_get_data (G_OBJECT (menuitem), MENU_DATA_INDICATOR_OBJECT);
  IndicatorObjectEntry *entry = g_object_get_data (G_OBJECT (menuitem), MENU_DATA_INDICATOR_ENTRY);

  g_return_val_if_fail(INDICATOR_IS_OBJECT(io), FALSE);

  g_signal_emit_by_name (io, INDICATOR_OBJECT_SIGNAL_ENTRY_SCROLLED, entry, 1, event->direction);

  return FALSE;
}

static void
entry_added (IndicatorObject * io, IndicatorObjectEntry * entry, GtkWidget * menubar)
{
    const char *indicator_name = (const gchar *)g_object_get_data(G_OBJECT(io), IO_DATA_NAME);
    g_debug("Signal: Entry Added from %s", indicator_name);
    gboolean something_visible = FALSE;
    gboolean something_sensitive = FALSE;

    GtkWidget * menuitem = gtk_menu_item_new();
    GtkWidget * hbox = gtk_hbox_new(FALSE, 3);

    g_object_set_data (G_OBJECT (menuitem), MENU_DATA_BOX, hbox);
    g_object_set_data(G_OBJECT(menuitem), MENU_DATA_INDICATOR_OBJECT, io);
    g_object_set_data(G_OBJECT(menuitem), MENU_DATA_INDICATOR_ENTRY,  entry);

    g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(entry_activated), NULL);
    g_signal_connect(G_OBJECT(menuitem), "button-press-event", G_CALLBACK(entry_secondary_activated), NULL);
    g_signal_connect(G_OBJECT(menuitem), "button-release-event", G_CALLBACK(entry_secondary_activated), NULL);
    g_signal_connect(G_OBJECT(menuitem), "enter-notify-event", G_CALLBACK(entry_secondary_activated), NULL);
    g_signal_connect(G_OBJECT(menuitem), "leave-notify-event", G_CALLBACK(entry_secondary_activated), NULL);
    g_signal_connect(G_OBJECT(menuitem), "scroll-event", G_CALLBACK(entry_scrolled), NULL);

    if (entry->image != NULL)
    {
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(entry->image), FALSE, FALSE, 1);
        if (gtk_widget_get_visible(GTK_WIDGET(entry->image))) {
                something_visible = TRUE;
        }

        if (gtk_widget_get_sensitive(GTK_WIDGET(entry->image))) {
            something_sensitive = TRUE;
        }

        g_signal_connect(G_OBJECT(entry->image), "show", G_CALLBACK(something_shown), menuitem);
        g_signal_connect(G_OBJECT(entry->image), "hide", G_CALLBACK(something_hidden), menuitem);
        g_signal_connect(G_OBJECT(entry->image), "notify::sensitive", G_CALLBACK(sensitive_cb), menuitem);
    }
    if (entry->label != NULL)
    {
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(entry->label), FALSE, FALSE, 1);

        if (gtk_widget_get_visible(GTK_WIDGET(entry->label))) {
                something_visible = TRUE;
        }

        if (gtk_widget_get_sensitive(GTK_WIDGET(entry->label))) {

            something_sensitive = TRUE;
        }

        g_signal_connect(G_OBJECT(entry->label), "show", G_CALLBACK(something_shown), menuitem);
        g_signal_connect(G_OBJECT(entry->label), "hide", G_CALLBACK(something_hidden), menuitem);
        g_signal_connect(G_OBJECT(entry->label), "notify::sensitive", G_CALLBACK(sensitive_cb), menuitem);

    }
    gtk_container_add(GTK_CONTAINER(menuitem), hbox);
    gtk_widget_show(hbox);

    if (entry->menu != NULL)
    {
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), GTK_WIDGET(entry->menu));
    }

    place_in_menu(menubar, menuitem, io, entry);

    if (something_visible) {
        gtk_widget_show(menuitem);
    }
    gtk_widget_set_sensitive(menuitem, something_sensitive);

    return;
}

static void
entry_removed_cb (GtkWidget * widget, gpointer userdata)
{
    gpointer data = g_object_get_data(G_OBJECT(widget), MENU_DATA_INDICATOR_ENTRY);

    if (data != userdata)
    {
        return;
    }

    IndicatorObjectEntry * entry = (IndicatorObjectEntry *)data;
    if (entry->label != NULL) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(entry->label), G_CALLBACK(something_shown), widget);
        g_signal_handlers_disconnect_by_func(G_OBJECT(entry->label), G_CALLBACK(something_hidden), widget);
        g_signal_handlers_disconnect_by_func(G_OBJECT(entry->label), G_CALLBACK(sensitive_cb), widget);
    }
    if (entry->image != NULL) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(entry->image), G_CALLBACK(something_shown), widget);
        g_signal_handlers_disconnect_by_func(G_OBJECT(entry->image), G_CALLBACK(something_hidden), widget);
        g_signal_handlers_disconnect_by_func(G_OBJECT(entry->image), G_CALLBACK(sensitive_cb), widget);
    }

    gtk_widget_destroy(widget);
    return;
}

static void
entry_moved_find_cb (GtkWidget * widget, gpointer userdata)
{
    gpointer * array = (gpointer *)userdata;
    if (array[1] != NULL) {
        return;
    }

    gpointer data = g_object_get_data(G_OBJECT(widget), MENU_DATA_INDICATOR_ENTRY);

    if (data != array[0]) {
        return;
    }

    array[1] = widget;
    return;
}

/* Gets called when an entry for an object was moved. */
static void
entry_moved (IndicatorObject * io, IndicatorObjectEntry * entry,
             gint old G_GNUC_UNUSED, gint new G_GNUC_UNUSED, gpointer user_data)
{
    GtkWidget * menubar = GTK_WIDGET(user_data);

    gpointer array[2];
    array[0] = entry;
    array[1] = NULL;

    gtk_container_foreach(GTK_CONTAINER(menubar), entry_moved_find_cb, array);
    if (array[1] == NULL) {
        g_warning("Moving an entry that isn't in our menus.");
        return;
    }

    GtkWidget * mi = GTK_WIDGET(array[1]);
    g_object_ref(G_OBJECT(mi));
    gtk_container_remove(GTK_CONTAINER(menubar), mi);
    place_in_menu(menubar, mi, io, entry);
    g_object_unref(G_OBJECT(mi));

    return;
}

static void
entry_removed (IndicatorObject * io G_GNUC_UNUSED, IndicatorObjectEntry * entry,
               gpointer user_data)
{
    g_debug("Signal: Entry Removed");

    gtk_container_foreach(GTK_CONTAINER(user_data), entry_removed_cb, entry);

    return;
}

static void
menu_show (IndicatorObject * io, IndicatorObjectEntry * entry,
           guint32 timestamp, gpointer user_data)
{
  GtkWidget * menubar = GTK_WIDGET(user_data);

  if (entry == NULL) {
    /* Close any open menus instead of opening one */
    GList * entries = indicator_object_get_entries(io);
    GList * entry = NULL;
    for (entry = entries; entry != NULL; entry = g_list_next(entry)) {
      IndicatorObjectEntry * entrydata = (IndicatorObjectEntry *)entry->data;
      gtk_menu_popdown(entrydata->menu);
    }
    g_list_free(entries);

    /* And tell the menubar to exit activation mode too */
    gtk_menu_shell_cancel(GTK_MENU_SHELL(menubar));
    return;
  }

  // TODO: do something sensible here
}

static gboolean
load_module (const gchar * name, GtkWidget * menubar)
{
    g_debug("Looking at Module: %s", name);
    g_return_val_if_fail(name != NULL, FALSE);

    if (!g_str_has_suffix(name, G_MODULE_SUFFIX))
    {
        return FALSE;
    }

    g_debug("Loading Module: %s", name);

    /* Build the object for the module */
    gchar *fullpath = g_build_filename(INDICATOR_DIR, name, NULL);
    g_debug("Full path: %s", fullpath);
    IndicatorObject * io = indicator_object_new_from_file(fullpath);
    g_free(fullpath);

    /* Set the environment it's in */
    indicator_object_set_environment(io, (const GStrv)indicator_env);

    /* Attach the 'name' to the object */
    g_object_set_data_full(G_OBJECT(io), IO_DATA_NAME, g_strdup(name), g_free);
    g_object_set_data(G_OBJECT(io), IO_DATA_ORDER_NUMBER, GINT_TO_POINTER(name2order(name, NULL)));

    /* Connect to it's signals */
    g_signal_connect(G_OBJECT(io), INDICATOR_OBJECT_SIGNAL_ENTRY_ADDED,   G_CALLBACK(entry_added),    menubar);
    g_signal_connect(G_OBJECT(io), INDICATOR_OBJECT_SIGNAL_ENTRY_REMOVED, G_CALLBACK(entry_removed),  menubar);
    g_signal_connect(G_OBJECT(io), INDICATOR_OBJECT_SIGNAL_ENTRY_MOVED,   G_CALLBACK(entry_moved),    menubar);
    g_signal_connect(G_OBJECT(io), INDICATOR_OBJECT_SIGNAL_MENU_SHOW,     G_CALLBACK(menu_show),      menubar);

    /* Work on the entries */
    GList * entries = indicator_object_get_entries(io);
    GList * entry = NULL;

    for (entry = entries; entry != NULL; entry = g_list_next(entry))
    {
        IndicatorObjectEntry * entrydata = (IndicatorObjectEntry *)entry->data;
        entry_added(io, entrydata, menubar);
    }

    g_list_free(entries);

    return TRUE;
}

#if 0
static void
log_to_file_cb (GObject * source_obj G_GNUC_UNUSED,
                GAsyncResult * result G_GNUC_UNUSED, gpointer user_data)
{
    g_free(user_data);
    return;
}

static void
log_to_file (const gchar * domain G_GNUC_UNUSED,
             GLogLevelFlags level G_GNUC_UNUSED,
             const gchar * message,
             gpointer data G_GNUC_UNUSED)
{
    if (log_file == NULL) {
        GError * error = NULL;
        gchar * filename = g_build_filename(g_get_user_cache_dir(), LOG_FILE_NAME, NULL);
        GFile * file = g_file_new_for_path(filename);
        g_free(filename);

        if (!g_file_test(g_get_user_cache_dir(), G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
            GFile * cachedir = g_file_new_for_path(g_get_user_cache_dir());
            g_file_make_directory_with_parents(cachedir, NULL, &error);

            if (error != NULL) {
                g_error("Unable to make directory '%s' for log file: %s", g_get_user_cache_dir(), error->message);
                return;
            }
        }

        g_file_delete(file, NULL, NULL);

        GFileIOStream * io = g_file_create_readwrite(file,
                                G_FILE_CREATE_REPLACE_DESTINATION, /* flags */
                                NULL, /* cancelable */
                                &error); /* error */
        if (error != NULL) {
            g_error("Unable to replace file: %s", error->message);
            return;
        }

        log_file = g_io_stream_get_output_stream(G_IO_STREAM(io));
    }

    gchar * outputstring = g_strdup_printf("%s\n", message);
    g_output_stream_write_async(log_file,
                                outputstring, /* data */
                                strlen(outputstring), /* length */
                                G_PRIORITY_LOW, /* priority */
                                NULL, /* cancelable */
                                log_to_file_cb, /* callback */
                                outputstring); /* data */

    return;
}
#endif

static gboolean
menubar_press (GtkWidget * widget,
                    GdkEventButton *event,
                    gpointer data G_GNUC_UNUSED)

{
    if (event->button != 1) {
        g_signal_stop_emission_by_name(widget, "button-press-event");
    }

    return FALSE;

}

static gboolean
menubar_scroll (GtkWidget      *widget G_GNUC_UNUSED,
                GdkEventScroll *event,
                gpointer        data G_GNUC_UNUSED)
{

    GtkWidget *menuitem;

    menuitem = gtk_get_event_widget ((GdkEvent *)event);

    IndicatorObject *io = g_object_get_data (G_OBJECT (menuitem), "indicator");
    g_signal_emit_by_name (io, "scroll", 1, event->direction);

    return FALSE;

}

static gboolean
menubar_on_expose (GtkWidget * widget,
                    GdkEventExpose *event G_GNUC_UNUSED,
                    GtkWidget * menubar)
{
    if (GTK_WIDGET_HAS_FOCUS(menubar))
        gtk_paint_focus(widget->style, widget->window, GTK_WIDGET_STATE(menubar),
                        NULL, widget, "menubar-applet", 0, 0, -1, -1);

    return FALSE;
}

static void indicator_load_modules(LXPanel *panel, GtkWidget *p)
{

    gint indicators_loaded = 0;
    IndicatorPlugin * indicator = lxpanel_plugin_get_data(p);

    gtk_container_forall(GTK_CONTAINER(indicator->menubar),
                         (GtkCallback)gtk_widget_destroy, NULL);

    if (g_file_test(INDICATOR_DIR, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
    {
        GDir *dir = g_dir_open(INDICATOR_DIR, 0, NULL);

        const gchar *name;
        while ((name = g_dir_read_name(dir)) != NULL)
        {

            if (g_strcmp0(name, "libsession.so")== 0) {
                if (indicator->session == 1){
                    load_module(name, indicator->menubar);
                    indicators_loaded++;
                }
            }
            else if (g_strcmp0(name, "libapplication.so")== 0) {
                if (indicator->applications == 1){
                    load_module(name, indicator->menubar);
                    indicators_loaded++;
                }
            }
            else if (g_strcmp0(name, "libdatetime.so")== 0) {
                if (indicator->datetime == 1) {
                    load_module(name, indicator->menubar);
                    indicators_loaded++;
                }
            }
            else if (g_strcmp0(name, "libmessaging.so")== 0) {
                if (indicator->messages == 1) {
                    load_module(name, indicator->menubar);
                    indicators_loaded++;
                }
            }
            else if (g_strcmp0(name, "libnetworkmenu.so")== 0) {
                if (indicator->network == 1) {
                    load_module(name, indicator->menubar);
                    indicators_loaded++;
                }
            }
            else if (g_strcmp0(name, "libsoundmenu.so")== 0) {
                if (indicator->sound == 1) {
                    load_module(name, indicator->menubar);
                    indicators_loaded++;
                }
            }
            /* else if (g_strcmp0(name, "libappmenu.so") == 0) {
                if (indicator->appmenu == 1) {
                    load_module(name, indicator->menubar);
                    indicators_loaded++;
                }
            }*/
        }
        g_dir_close (dir);
    }

    /* Update the plugin container contents */
    if (gtk_bin_get_child(GTK_BIN(p)))
        gtk_container_remove(GTK_CONTAINER(p), gtk_bin_get_child(GTK_BIN(p)));
    if (indicators_loaded == 0)
    {
        /* A label to allow for click through */
        GtkWidget *label = gtk_label_new(_("No Indicators"));
        gtk_widget_show(label);
        gtk_container_add(GTK_CONTAINER(p), label);
    }
    else
    {
        gtk_container_add(GTK_CONTAINER(p), indicator->menubar);
        /* Enforce background */
        plugin_widget_set_background(indicator->menubar, panel);
    }
}

/* Plugin constructor. */
static GtkWidget *indicator_constructor(LXPanel *panel, config_setting_t *settings)
{
    /* Allocate and initialize plugin context and set into Plugin private data pointer. */
    IndicatorPlugin * indicator = g_new0(IndicatorPlugin, 1);
    GtkWidget *p;
    int tmp_int;

    indicator->panel = panel;
    indicator->settings = settings;

    /* Default support for indicators */
    indicator->applications = TRUE;
    indicator->datetime     = FALSE;
    indicator->messages     = FALSE;
    indicator->network      = FALSE;
    indicator->session      = FALSE;
    indicator->sound        = FALSE;
    /* indicator->appmenu      = FALSE; */

    /* Load parameters from the configuration file. */
    if (config_setting_lookup_int(settings, "applications", &tmp_int))
        indicator->applications = tmp_int != 0;
    if (config_setting_lookup_int(settings, "datetime", &tmp_int))
        indicator->datetime = tmp_int != 0;
    if (config_setting_lookup_int(settings, "messages", &tmp_int))
        indicator->messages = tmp_int != 0;
    if (config_setting_lookup_int(settings, "network", &tmp_int))
        indicator->network = tmp_int != 0;
    if (config_setting_lookup_int(settings, "session", &tmp_int))
        indicator->session = tmp_int != 0;
    if (config_setting_lookup_int(settings, "sound", &tmp_int))
        indicator->sound = tmp_int != 0;
    /* if (config_setting_lookup_int(settings, "appmenu", &tmp_int))
        indicator->appmenu = tmp_int != 0;*/

    /* Allocate top level widget and set into Plugin widget pointer. */
    p = gtk_event_box_new();
    lxpanel_plugin_set_data(p, indicator, indicator_destructor);

    gtk_rc_parse_string (
        "style \"indicator-applet-style\"\n"
        "{\n"
        "    GtkMenuBar::shadow-type = none\n"
        "    GtkMenuBar::internal-padding = 0\n"
        "    GtkWidget::focus-line-width = 0\n"
        "    GtkWidget::focus-padding = 0\n"
        "}\n"
        "style \"indicator-applet-menubar-style\"\n"
        "{\n"
        "    GtkMenuBar::shadow-type = none\n"
        "    GtkMenuBar::internal-padding = 0\n"
        "    GtkWidget::focus-line-width = 0\n"
        "    GtkWidget::focus-padding = 0\n"
        "    GtkMenuItem::horizontal-padding = 0\n"
        "}\n"
        "style \"indicator-applet-menuitem-style\"\n"
        "{\n"
        "    GtkWidget::focus-line-width = 0\n"
        "    GtkWidget::focus-padding = 0\n"
        "    GtkMenuItem::horizontal-padding = 0\n"
        "}\n"
        "widget \"*.fast-user-switch-applet\" style \"indicator-applet-style\""
        "widget \"*.fast-user-switch-menuitem\" style \"indicator-applet-menuitem-style\""
        "widget \"*.fast-user-switch-menubar\" style \"indicator-applet-menubar-style\"");

    gtk_widget_set_name(p, "fast-user-switch-applet");

    /* Connect signals for container */
    //g_log_set_default_handler(log_to_file, NULL);

    /* Allocate icon as a child of top level. */
    indicator->menubar = g_object_ref_sink(gtk_menu_bar_new());
    gtk_widget_show(indicator->menubar);
    gtk_widget_set_can_focus(indicator->menubar, TRUE);

    /* Init some theme/icon stuff */
    gtk_icon_theme_append_search_path(panel_get_icon_theme(panel),
                                    INDICATOR_ICONS_DIR);
    g_debug("Icons directory: %s", INDICATOR_ICONS_DIR);

    gtk_widget_set_name(indicator->menubar, "fast-user-switch-menubar");

    /* Connect signals. */
    g_signal_connect(indicator->menubar, "button-press-event", G_CALLBACK(menubar_press), NULL);
    g_signal_connect(indicator->menubar, "scroll-event", G_CALLBACK (menubar_scroll), NULL);
    g_signal_connect_after(indicator->menubar, "expose-event", G_CALLBACK(menubar_on_expose), indicator->menubar);

    gtk_container_set_border_width(GTK_CONTAINER(indicator->menubar), 0);

    return p;
}

/* Plugin destructor. */
static void indicator_destructor(gpointer user_data)
{
    IndicatorPlugin * indicator = (IndicatorPlugin *) user_data;

    /* Deallocate all memory. */
    g_object_unref(indicator->menubar);
    g_free(indicator);
}

/* Callback when panel configuration changes. */
static void indicator_panel_configuration_changed(LXPanel *panel, GtkWidget *p)
{
    /*
    Update when configuration changed
    */

    /* load 'em */
    indicator_load_modules(panel, p);

    /* Determine if the orientation changed in a way that requires action. */
    /*
    GtkWidget * sep = gtk_bin_get_child(GTK_BIN(p->pwid));
    if (GTK_IS_VSEPARATOR(sep))
    {
        if (p->panel->orientation == GTK_ORIENTATION_HORIZONTAL)
        return;
    }
    else
    {
        if (p->panel->orientation == GTK_ORIENTATION_VERTICAL)
            return;
    }
*/
}

/* Callback when the configuration dialog has recorded a configuration change. */
static gboolean indicator_apply_configuration(gpointer user_data)
{
    IndicatorPlugin * indicator = lxpanel_plugin_get_data(user_data);

    /* load 'em */
    indicator_load_modules(indicator->panel, user_data);

    config_group_set_int(indicator->settings, "applications", indicator->applications);
    config_group_set_int(indicator->settings, "datetime", indicator->datetime);
    config_group_set_int(indicator->settings, "messages", indicator->messages);
    config_group_set_int(indicator->settings, "network", indicator->network);
    config_group_set_int(indicator->settings, "session", indicator->session);
    config_group_set_int(indicator->settings, "sound", indicator->sound);
    /* Apply settings. */
/*
    if (p->panel->orientation == ORIENT_HORIZ)
        gtk_widget_set_size_request(p->pwid, sp->size, 2);
    else
        gtk_widget_set_size_request(p->pwid, 2, sp->size);
*/
    return FALSE;
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *indicator_configure(LXPanel *panel, GtkWidget *p)
{
    IndicatorPlugin * indicator = lxpanel_plugin_get_data(p);
    GtkWidget * dlg = lxpanel_generic_config_dlg(_("Indicator applets"),
        panel, indicator_apply_configuration, p,
        _("Indicator Applications"), &indicator->applications, CONF_TYPE_BOOL,
        _("Clock Indicator"), &indicator->datetime, CONF_TYPE_BOOL,
        _("Messaging Menu"), &indicator->messages, CONF_TYPE_BOOL,
        _("Network Menu"), &indicator->network, CONF_TYPE_BOOL,
        _("Session Menu"), &indicator->session, CONF_TYPE_BOOL,
        _("Sound Menu"), &indicator->sound, CONF_TYPE_BOOL,
        /* _("Applications menus"), &indicator->appmenu, CONF_TYPE_BOOL,*/
        NULL);
    gtk_widget_set_size_request(GTK_WIDGET(dlg), 300, -1);
    return dlg;
}

FM_DEFINE_MODULE(lxpanel_gtk, indicator)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Indicator applets"),
    .description = N_("Add indicator applets to the panel"),

    .new_instance = indicator_constructor,
    .config = indicator_configure,
    .reconfigure = indicator_panel_configuration_changed
};
