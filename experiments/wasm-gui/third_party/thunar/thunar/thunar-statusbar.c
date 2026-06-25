/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <exo/exo.h>
#include <libxfce4ui/libxfce4ui.h>

#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-statusbar.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-application.h>
#include <thunar/thunar-window.h>



enum
{
  PROP_0,
  PROP_TEXT,
};



static void         thunar_statusbar_finalize                   (GObject              *object);
static void         thunar_statusbar_set_property               (GObject              *object,
                                                                 guint                 prop_id,
                                                                 const GValue         *value,
                                                                 GParamSpec           *pspec);
static void         thunar_statusbar_set_text                   (ThunarStatusbar      *statusbar,
                                                                 const gchar          *text);
static GtkWidget*   thunar_statusbar_context_menu               (ThunarStatusbar      *statusbar);
static gboolean     thunar_statusbar_button_press_event         (GtkWidget            *widget,
                                                                 GdkEventButton       *event,
                                                                 ThunarStatusbar      *statusbar);
static gboolean     thunar_statusbar_action_show_size           (ThunarStatusbar      *statusbar);
static gboolean     thunar_statusbar_action_show_size_bytes     (ThunarStatusbar      *statusbar);
static gboolean     thunar_statusbar_action_show_filetype       (ThunarStatusbar      *statusbar);
static gboolean     thunar_statusbar_action_show_display_name   (ThunarStatusbar      *statusbar);
static gboolean     thunar_statusbar_action_show_last_modified  (ThunarStatusbar      *statusbar);
static void         thunar_statusbar_update_all                 (void);


struct _ThunarStatusbarClass
{
  GtkStatusbarClass __parent__;
};

struct _ThunarStatusbar
{
  GtkStatusbar       __parent__;
  guint              context_id;

  ThunarPreferences *preferences;
};

static XfceGtkActionEntry thunar_status_bar_action_entries[] =
    {
        { THUNAR_STATUS_BAR_ACTION_TOGGLE_SIZE,           "<Actions>/ThunarStatusBar/toggle-size",             "", XFCE_GTK_CHECK_MENU_ITEM,       N_ ("Size"),          N_ ("Show size"),               NULL, G_CALLBACK (thunar_statusbar_action_show_size),            },
        { THUNAR_STATUS_BAR_ACTION_TOGGLE_SIZE_IN_BYTES,  "<Actions>/ThunarStatusBar/toggle-size-in-bytes",    "", XFCE_GTK_CHECK_MENU_ITEM,       N_ ("Size in bytes"), N_ ("Show size in bytes"),      NULL, G_CALLBACK (thunar_statusbar_action_show_size_bytes),      },
        { THUNAR_STATUS_BAR_ACTION_TOGGLE_FILETYPE,       "<Actions>/ThunarStatusBar/toggle-filetype",         "", XFCE_GTK_CHECK_MENU_ITEM,       N_ ("Filetype"),      N_ ("Show filetype"),           NULL, G_CALLBACK (thunar_statusbar_action_show_filetype),        },
        { THUNAR_STATUS_BAR_ACTION_TOGGLE_DISPLAY_NAME,   "<Actions>/ThunarStatusBar/toggle-display-name",     "", XFCE_GTK_CHECK_MENU_ITEM,       N_ ("Display Name"),  N_ ("Show display name"),       NULL, G_CALLBACK (thunar_statusbar_action_show_display_name),    },
        { THUNAR_STATUS_BAR_ACTION_TOGGLE_LAST_MODIFIED,  "<Actions>/ThunarStatusBar/toggle-last-modified",    "", XFCE_GTK_CHECK_MENU_ITEM,       N_ ("Last Modified"), N_ ("Show last modified"),      NULL, G_CALLBACK (thunar_statusbar_action_show_last_modified),   },
    };

#define get_action_entry(id) xfce_gtk_get_action_entry_by_id (thunar_status_bar_action_entries, G_N_ELEMENTS (thunar_status_bar_action_entries) ,id)


G_DEFINE_TYPE (ThunarStatusbar, thunar_statusbar, GTK_TYPE_STATUSBAR)



static void
thunar_statusbar_class_init (ThunarStatusbarClass *klass)
{
  static gboolean style_initialized = FALSE;

  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = thunar_statusbar_set_property;
  gobject_class->finalize = thunar_statusbar_finalize;

  /**
   * ThunarStatusbar:text:
   *
   * The main text to be displayed in the statusbar. This property
   * can only be written.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        "text",
                                                        "text",
                                                        NULL,
                                                        EXO_PARAM_WRITABLE));

  if (!style_initialized)
    {
        gtk_widget_class_install_style_property (GTK_WIDGET_CLASS (gobject_class),
          g_param_spec_enum (
            "shadow-type",                //name
            "shadow-type",                //nick
            "type of shadow",             //blurb
            gtk_shadow_type_get_type(),   //type
            GTK_SHADOW_NONE,              //default
            G_PARAM_READWRITE ));         //flags
    }

  xfce_gtk_translate_action_entries (thunar_status_bar_action_entries, G_N_ELEMENTS (thunar_status_bar_action_entries));
}



static void
thunar_statusbar_init (ThunarStatusbar *statusbar)
{
  statusbar->context_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "Main text");

  /* make the status thinner */
  gtk_widget_set_margin_top (GTK_WIDGET (statusbar), 0);
  gtk_widget_set_margin_bottom (GTK_WIDGET (statusbar), 0);

  statusbar->preferences = thunar_preferences_get ();
}



static void
thunar_statusbar_finalize (GObject *object)
{
  ThunarStatusbar *statusbar = THUNAR_STATUSBAR (object);

  g_object_unref (statusbar->preferences);
}



/**
 * thunar_statusbar_setup_event:
 * @statusbar : a #ThunarStatusbar instance.
 * @event_box    : a #GtkWidget instance (actually a GtkEventBox passed as a GtkWidget).
 *
 * Sets the statusbar's GtkEventBox and uses it to receive signals.
 **/
void
thunar_statusbar_setup_event (ThunarStatusbar *statusbar,
                              GtkWidget       *event_box)
{
  g_signal_connect (G_OBJECT (event_box), "button-press-event", G_CALLBACK (thunar_statusbar_button_press_event), G_OBJECT (statusbar));
}



static gboolean
thunar_statusbar_button_press_event (GtkWidget        *widget,
                                     GdkEventButton   *event,
                                     ThunarStatusbar  *statusbar)
{
  if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
      thunar_gtk_menu_run (GTK_MENU (thunar_statusbar_context_menu (statusbar)));
      return TRUE;
    }

  return FALSE;
}



static void
thunar_statusbar_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ThunarStatusbar *statusbar = THUNAR_STATUSBAR (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      thunar_statusbar_set_text (statusbar, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/**
 * thunar_statusbar_set_text:
 * @statusbar : a #ThunarStatusbar instance.
 * @text      : the main text to be displayed in @statusbar.
 *
 * Sets up a new main text for @statusbar.
 **/
static void
thunar_statusbar_set_text (ThunarStatusbar *statusbar,
                           const gchar     *text)
{
  _thunar_return_if_fail (THUNAR_IS_STATUSBAR (statusbar));
  _thunar_return_if_fail (text != NULL);

  gtk_statusbar_pop (GTK_STATUSBAR (statusbar), statusbar->context_id);
  gtk_statusbar_push (GTK_STATUSBAR (statusbar), statusbar->context_id, text);
}



static GtkWidget*
thunar_statusbar_context_menu (ThunarStatusbar *statusbar)
{
  GtkWidget *context_menu = gtk_menu_new();
  GtkWidget *widget;
  guint      active;
  gboolean   show_size, show_size_in_bytes, show_filetype, show_display_name, show_last_modified;

  g_object_get (G_OBJECT (statusbar->preferences), "misc-status-bar-active-info", &active, NULL);
  show_size = active & THUNAR_STATUS_BAR_INFO_SIZE;
  show_size_in_bytes = active & THUNAR_STATUS_BAR_INFO_SIZE_IN_BYTES;
  show_filetype = active & THUNAR_STATUS_BAR_INFO_FILETYPE;
  show_display_name = active & THUNAR_STATUS_BAR_INFO_DISPLAY_NAME;
  show_last_modified = active & THUNAR_STATUS_BAR_INFO_LAST_MODIFIED;

  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_STATUS_BAR_ACTION_TOGGLE_SIZE), G_OBJECT (statusbar), show_size, GTK_MENU_SHELL (context_menu));
  widget = xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_STATUS_BAR_ACTION_TOGGLE_SIZE_IN_BYTES), G_OBJECT (statusbar), show_size_in_bytes, GTK_MENU_SHELL (context_menu));
  if (show_size == FALSE)
    gtk_widget_set_sensitive (widget, FALSE);
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_STATUS_BAR_ACTION_TOGGLE_FILETYPE), G_OBJECT (statusbar), show_filetype, GTK_MENU_SHELL (context_menu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_STATUS_BAR_ACTION_TOGGLE_DISPLAY_NAME), G_OBJECT (statusbar), show_display_name, GTK_MENU_SHELL (context_menu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_STATUS_BAR_ACTION_TOGGLE_LAST_MODIFIED), G_OBJECT (statusbar), show_last_modified, GTK_MENU_SHELL (context_menu));

  gtk_widget_show_all (GTK_WIDGET (context_menu));

  return context_menu;
}



static gboolean
thunar_statusbar_action_show_size (ThunarStatusbar *statusbar)
{
  guint active;

  g_object_get (G_OBJECT (statusbar->preferences), "misc-status-bar-active-info", &active, NULL);
  g_object_set (G_OBJECT (statusbar->preferences), "misc-status-bar-active-info", thunar_status_bar_info_toggle_bit (active, THUNAR_STATUS_BAR_INFO_SIZE), NULL);
  thunar_statusbar_update_all ();

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_statusbar_action_show_size_bytes (ThunarStatusbar *statusbar)
{
  guint active;

  g_object_get (G_OBJECT (statusbar->preferences), "misc-status-bar-active-info", &active, NULL);
  g_object_set (G_OBJECT (statusbar->preferences), "misc-status-bar-active-info", thunar_status_bar_info_toggle_bit (active, THUNAR_STATUS_BAR_INFO_SIZE_IN_BYTES), NULL);
  thunar_statusbar_update_all ();

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_statusbar_action_show_filetype (ThunarStatusbar *statusbar)
{
  guint active;

  g_object_get (G_OBJECT (statusbar->preferences), "misc-status-bar-active-info", &active, NULL);
  g_object_set (G_OBJECT (statusbar->preferences), "misc-status-bar-active-info", thunar_status_bar_info_toggle_bit (active, THUNAR_STATUS_BAR_INFO_FILETYPE), NULL);
  thunar_statusbar_update_all ();

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_statusbar_action_show_display_name (ThunarStatusbar *statusbar)
{
  guint active;

  g_object_get (G_OBJECT (statusbar->preferences), "misc-status-bar-active-info", &active, NULL);
  g_object_set (G_OBJECT (statusbar->preferences), "misc-status-bar-active-info", thunar_status_bar_info_toggle_bit (active, THUNAR_STATUS_BAR_INFO_DISPLAY_NAME), NULL);
  thunar_statusbar_update_all ();

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static gboolean
thunar_statusbar_action_show_last_modified (ThunarStatusbar *statusbar)
{
  guint active;

  g_object_get (G_OBJECT (statusbar->preferences), "misc-status-bar-active-info", &active, NULL);
  g_object_set (G_OBJECT (statusbar->preferences), "misc-status-bar-active-info", thunar_status_bar_info_toggle_bit (active, THUNAR_STATUS_BAR_INFO_LAST_MODIFIED), NULL);
  thunar_statusbar_update_all ();

  /* required in case of shortcut activation, in order to signal that the accel key got handled */
  return TRUE;
}



static void
thunar_statusbar_update_all (void)
{
  ThunarApplication *app     = thunar_application_get ();
  GList             *windows = thunar_application_get_windows (app);
  GList             *lp;

  for (lp = windows; lp != NULL; lp = lp->next)
    thunar_window_update_statusbar (THUNAR_WINDOW (lp->data));

  g_list_free (windows);
  g_object_unref (app);
}



/**
 * thunar_statusbar_new:
 *
 * Allocates a new #ThunarStatusbar instance with no
 * text set.
 *
 * Return value: the newly allocated #ThunarStatusbar instance.
 **/
GtkWidget*
thunar_statusbar_new (void)
{
  return g_object_new (THUNAR_TYPE_STATUSBAR, NULL);
}



XfceGtkActionEntry*
thunar_statusbar_get_action_entries (void)
{
  return thunar_status_bar_action_entries;
}



/**
 * thunar_statusbar_append_accelerators:
 * @statusbar    : a #ThunarStatusbar.
 * @accel_group : a #GtkAccelGroup to be used used for new menu items
 *
 * Connects all accelerators and corresponding default keys of this widget to the global accelerator list
 **/
void thunar_statusbar_append_accelerators (ThunarStatusbar *statusbar,
                                           GtkAccelGroup   *accel_group)
{
  _thunar_return_if_fail (THUNAR_IS_STATUSBAR (statusbar));

  xfce_gtk_accel_map_add_entries (thunar_status_bar_action_entries, G_N_ELEMENTS (thunar_status_bar_action_entries));
  xfce_gtk_accel_group_connect_action_entries (accel_group,
                                               thunar_status_bar_action_entries,
                                               G_N_ELEMENTS (thunar_status_bar_action_entries),
                                               statusbar);
}
