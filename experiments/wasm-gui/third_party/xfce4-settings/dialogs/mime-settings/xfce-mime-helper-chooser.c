/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

#include "xfce-mime-helper-chooser.h"
#include "xfce-mime-helper-enum-types.h"
#include "xfce-mime-helper-utils.h"



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CATEGORY,
  PROP_IS_VALID,
};



static void xfce_mime_helper_chooser_finalize     (GObject                *object);
static void xfce_mime_helper_chooser_get_property (GObject                *object,
                                             guint                   prop_id,
                                             GValue                 *value,
                                             GParamSpec             *pspec);
static void xfce_mime_helper_chooser_set_property (GObject                *object,
                                             guint                   prop_id,
                                             const GValue           *value,
                                             GParamSpec             *pspec);
static void xfce_mime_helper_chooser_update       (XfceMimeHelperChooser       *chooser);
static void xfce_mime_helper_chooser_pressed      (XfceMimeHelperChooser       *chooser,
                                             GtkWidget              *button);



struct _XfceMimeHelperChooserClass
{
  GtkBinClass __parent__;
};

struct _XfceMimeHelperChooser
{
  GtkBin __parent__;

  GtkWidget         *image;
  GtkWidget         *label;

  XfceMimeHelperDatabase *database;
  XfceMimeHelperCategory  category;

  gboolean           is_valid;
};



G_DEFINE_TYPE (XfceMimeHelperChooser, xfce_mime_helper_chooser, GTK_TYPE_BIN)



static void
xfce_mime_helper_chooser_class_init (XfceMimeHelperChooserClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_mime_helper_chooser_finalize;
  gobject_class->get_property = xfce_mime_helper_chooser_get_property;
  gobject_class->set_property = xfce_mime_helper_chooser_set_property;

  /**
   * XfceMimeHelperChooser:category:
   *
   * The #XfceMimeHelperCategory which should be configured by this
   * #XfceMimeHelperChooser. See xfce_mime_helper_chooser_get_category() and
   * xfce_mime_helper_chooser_set_category() for details.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CATEGORY,
                                   g_param_spec_enum ("category",
                                                      "Helper category",
                                                      "Helper category",
                                                      XFCE_MIME_TYPE_MIME_HELPER_CATEGORY,
                                                      XFCE_MIME_HELPER_WEBBROWSER,
                                                      EXO_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * XfceMimeHelperChooser:is-valid:
   *
   * %TRUE if a valid #XfceMimeHelper is selected by
   * this #XfceMimeHelperChooser, else %FALSE.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_IS_VALID,
                                   g_param_spec_boolean ("is-valid",
                                                         "Is valid",
                                                         "Is valid",
                                                         FALSE,
                                                         EXO_PARAM_READABLE));
}



static void
xfce_mime_helper_chooser_init (XfceMimeHelperChooser *chooser)
{
  AtkObject      *object;
  GtkWidget      *separator;
  GtkWidget      *button;
  GtkWidget      *arrow;
  GtkWidget      *hbox;

  chooser->database = xfce_mime_helper_database_get ();

  /*gtk_widget_push_composite_child ();*/

  button = gtk_button_new ();
  g_signal_connect_swapped (G_OBJECT (button), "pressed", G_CALLBACK (xfce_mime_helper_chooser_pressed), chooser);
  gtk_widget_set_tooltip_text (button, _("Press left mouse button to change the selected application."));
  gtk_container_add (GTK_CONTAINER (chooser), button);
  gtk_widget_show (button);

  /* set Atk properties for the button */
  object = gtk_widget_get_accessible (button);
  atk_object_set_name (object, _("Application Chooser Button"));
  atk_object_set_description (object, _("Press left mouse button to change the selected application."));

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_container_add (GTK_CONTAINER (button), hbox);
  gtk_widget_show (hbox);

  chooser->image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (hbox), chooser->image, FALSE, FALSE, 0);
  gtk_widget_show (chooser->image);

  chooser->label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, "yalign", 0.5f, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), chooser->label, TRUE, TRUE, 0);
  gtk_widget_show (chooser->label);

  xfce_gtk_label_set_a11y_relation (GTK_LABEL (chooser->label), GTK_WIDGET (button));

  separator = g_object_new (GTK_TYPE_SEPARATOR, "orientation", GTK_ORIENTATION_VERTICAL, "height-request", 16, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), separator, FALSE, FALSE, 0);
  gtk_widget_show (separator);

  arrow = gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (hbox), arrow, FALSE, FALSE, 0);
  gtk_widget_show (arrow);

  /*gtk_widget_pop_composite_child ();*/
}



static void
xfce_mime_helper_chooser_finalize (GObject *object)
{
  XfceMimeHelperChooser *chooser = XFCE_MIME_HELPER_CHOOSER (object);

  g_object_unref (G_OBJECT (chooser->database));

  (*G_OBJECT_CLASS (xfce_mime_helper_chooser_parent_class)->finalize) (object);
}



static void
xfce_mime_helper_chooser_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  XfceMimeHelperChooser *chooser = XFCE_MIME_HELPER_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      g_value_set_enum (value, xfce_mime_helper_chooser_get_category (chooser));
      break;

    case PROP_IS_VALID:
      g_value_set_boolean (value, xfce_mime_helper_chooser_get_is_valid (chooser));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_mime_helper_chooser_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  XfceMimeHelperChooser *chooser = XFCE_MIME_HELPER_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      xfce_mime_helper_chooser_set_category (chooser, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static cairo_surface_t *
xfce_mime_helper_chooser_load_app_icon (const gchar *icon_name,
                                        gint         scale_factor)
{
    cairo_surface_t *surface = NULL;
    GdkPixbuf       *icon = NULL;
    GtkIconTheme    *icon_theme;
    gint             icon_size;

    icon_theme = gtk_icon_theme_get_default ();
    gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_size, &icon_size);

    if (G_LIKELY (icon_name != NULL))
      {
        /* load the icon */
        if (g_path_is_absolute (icon_name))
          {
            icon = gdk_pixbuf_new_from_file_at_size (icon_name,
                                                     icon_size * scale_factor,
                                                     icon_size * scale_factor,
                                                     NULL);
          }
        else
          {
            GIcon       *gicon;
            GtkIconInfo *icon_info;

            gicon = g_themed_icon_new_with_default_fallbacks (icon_name);
            icon_info = gtk_icon_theme_lookup_by_gicon_for_scale (icon_theme, gicon,
                                                                  icon_size, scale_factor,
                                                                  GTK_ICON_LOOKUP_FORCE_SIZE);
            g_object_unref (gicon);

            if (icon_info != NULL)
              {
                icon = gtk_icon_info_load_icon (icon_info, NULL);
                g_object_unref (icon_info);
              }
         }
      }

    /* fallback to application-x-executable */
    if (G_UNLIKELY (icon == NULL))
      {
        icon = gtk_icon_theme_load_icon_for_scale (icon_theme, "application-x-executable",
                                                   icon_size, scale_factor,
                                                   GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
      }

    /* fallback to gnome-mime-application-x-executable */
    if (G_UNLIKELY (icon == NULL))
      {
        icon = gtk_icon_theme_load_icon_for_scale (icon_theme, "gnome-mime-application-x-executable",
                                                   icon_size, scale_factor,
                                                   GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
      }

    if (G_LIKELY (icon != NULL))
      {
        surface = gdk_cairo_surface_create_from_pixbuf (icon, scale_factor, NULL);
        g_object_unref (icon);
      }

    return surface;
}



static void
xfce_mime_helper_chooser_update (XfceMimeHelperChooser *chooser)
{
  const gchar     *icon_name;
  XfceMimeHelper  *helper;
  cairo_surface_t *surface = NULL;
  gint             scale_factor;

  g_return_if_fail (XFCE_MIME_IS_HELPER_CHOOSER (chooser));

  /* determine the default helper for the category */
  helper = xfce_mime_helper_database_get_default (chooser->database, chooser->category);
  if (G_LIKELY (helper != NULL))
    {
      scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (chooser));

      /* try to load the icon for the helper */
      icon_name = xfce_mime_helper_get_icon (helper);
      surface = xfce_mime_helper_chooser_load_app_icon (icon_name, scale_factor);
      gtk_image_set_from_surface (GTK_IMAGE (chooser->image), surface);
      if (G_LIKELY (surface != NULL))
        cairo_surface_destroy (surface);

      gtk_label_set_text (GTK_LABEL (chooser->label), xfce_mime_helper_get_name (helper));
      g_object_unref (G_OBJECT (helper));
    }
  else
    {
      gtk_image_set_from_surface (GTK_IMAGE (chooser->image), NULL);
      gtk_label_set_text (GTK_LABEL (chooser->label), _("No application selected"));
    }

  /* update the "is-valid" property */
  chooser->is_valid = (helper != NULL);
  g_object_notify (G_OBJECT (chooser), "is-valid");
}



static void
menu_activate (GtkWidget        *item,
               XfceMimeHelperChooser *chooser)
{
  static const gchar *CATEGORY_ERRORS[] =
  {
    N_("Failed to set default Web Browser"),
    N_("Failed to set default Mail Reader"),
    N_("Failed to set default File Manager"),
    N_("Failed to set default Terminal Emulator"),
  };

  XfceMimeHelper *helper;
  GtkWidget *message;
  GError    *error = NULL;

  /* verify helper category values */
  g_assert (XFCE_MIME_HELPER_N_CATEGORIES == G_N_ELEMENTS (CATEGORY_ERRORS));

  g_return_if_fail (GTK_IS_WIDGET (item));
  g_return_if_fail (XFCE_MIME_IS_HELPER_CHOOSER (chooser));

  /* determine the helper for the item */
  helper = g_object_get_data (G_OBJECT (item), I_("exo-helper"));
  if (G_LIKELY (helper != NULL))
    {
      if (!xfce_mime_helper_database_set_default (chooser->database, chooser->category, helper, &error))
        {
          message = gtk_message_dialog_new (GTK_WINDOW (chooser),
                                            GTK_DIALOG_DESTROY_WITH_PARENT
                                            | GTK_DIALOG_MODAL,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            "%s.", _(CATEGORY_ERRORS[chooser->category]));
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s.", error->message);
          gtk_dialog_run (GTK_DIALOG (message));
          gtk_widget_destroy (message);
          g_error_free (error);
        }
      else
        {
          /* update the chooser state */
          xfce_mime_helper_chooser_update (chooser);
        }
    }
}



static void
entry_changed (GtkEditable *editable,
               GtkDialog   *dialog)
{
  gchar *text;

  text = gtk_editable_get_chars (editable, 0, -1);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, *text != '\0');
  g_free (text);
}



static void
browse_clicked (GtkWidget *button,
                GtkWidget *entry)
{
  GtkFileFilter *filter;
  GtkWidget     *toplevel;
  GtkWidget     *chooser;
  gchar         *filename;
  gchar         *text;
  gchar         *s;

  /* determine the toplevel window */
  toplevel = gtk_widget_get_toplevel (entry);
  if (toplevel == NULL || !gtk_widget_is_toplevel (toplevel))
    return;

  /* allocate the chooser */
  chooser = gtk_file_chooser_dialog_new (_("Select application"),
                                         GTK_WINDOW (toplevel),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         _("_Open"), GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);

  /* add filters */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Executable Files"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-executable");
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Perl Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Python Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Ruby Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Shell Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  /* default to bindir */
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser), BINDIR);

  /* preselect the filename */
  filename = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
  if (G_LIKELY (filename != NULL))
    {
      /* use only the first argument */
      s = strchr (filename, ' ');
      if (G_UNLIKELY (s != NULL))
        *s = '\0';

      /* check if we have a filename */
      if (G_LIKELY (*filename != '\0'))
        {
          /* check if the filename is not an absolute path */
          if (G_LIKELY (!g_path_is_absolute (filename)))
            {
              /* try to lookup the filename in $PATH */
              s = g_find_program_in_path (filename);
              if (G_LIKELY (s != NULL))
                {
                  /* use the absolute path instead */
                  g_free (filename);
                  filename = s;
                }
            }

          /* check if we have an absolute path now */
          if (G_LIKELY (g_path_is_absolute (filename)))
            gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), filename);
        }

      /* release the filename */
      g_free (filename);
    }

  /* run the chooser dialog */
  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
    {
      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
      text = g_strconcat (filename, " \"%s\"", NULL);
      gtk_entry_set_text (GTK_ENTRY (entry), text);
      g_free (filename);
      g_free (text);
    }

  /* destroy the chooser */
  gtk_widget_destroy (chooser);
}



static void
menu_activate_other (GtkWidget        *item,
                     XfceMimeHelperChooser *chooser)
{
  static const gchar *BROWSE_TITLES[] =
  {
    N_("Choose a custom Web Browser"),
    N_("Choose a custom Mail Reader"),
    N_("Choose a custom File Manager"),
    N_("Choose a custom Terminal Emulator"),
  };

  static const gchar *BROWSE_MESSAGES[] =
  {
    N_("Specify the application you want to use\nas default Web Browser for Xfce:"),
    N_("Specify the application you want to use\nas default Mail Reader for Xfce:"),
    N_("Specify the application you want to use\nas default File Manager for Xfce:"),
    N_("Specify the application you want to use\nas default Terminal Emulator for Xfce:"),
  };

  const gchar *command;
  XfceMimeHelper   *helper;
  GtkWidget   *toplevel;
  GtkWidget   *dialog;
  GtkWidget   *hbox;
  GtkWidget   *image;
  GtkWidget   *vbox;
  GtkWidget   *label;
  GtkWidget   *entry;
  GtkWidget   *button;

  /* sanity check the category values */
  g_assert (XFCE_MIME_HELPER_N_CATEGORIES == G_N_ELEMENTS (BROWSE_TITLES));
  g_assert (XFCE_MIME_HELPER_N_CATEGORIES == G_N_ELEMENTS (BROWSE_MESSAGES));

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (chooser));

  dialog = gtk_dialog_new_with_buttons (dgettext (GETTEXT_PACKAGE, BROWSE_TITLES[chooser->category]),
                                        GTK_WINDOW (toplevel),
                                        GTK_DIALOG_DESTROY_WITH_PARENT
                                        | GTK_DIALOG_MODAL,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_OK"), GTK_RESPONSE_OK,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 6);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  hbox = g_object_new (GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_HORIZONTAL, "border-width", 5, "spacing", 12, NULL);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  image = gtk_image_new_from_icon_name ("org.xfce.settings.default-applications", GTK_ICON_SIZE_DIALOG);
  g_object_set (image, "halign", GTK_ALIGN_CENTER, "valign", GTK_ALIGN_START, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", _(BROWSE_MESSAGES[chooser->category]),
                        "xalign", 0.0,
                        "yalign", 0.0,
                        NULL);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  entry = g_object_new (GTK_TYPE_ENTRY, "activates-default", TRUE, NULL);
  g_signal_connect (G_OBJECT (entry), "changed", G_CALLBACK (entry_changed), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  gtk_widget_show (entry);

  button = gtk_button_new ();
  gtk_widget_set_tooltip_text (button, _("Browse the file system to choose a custom command."));
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (browse_clicked), entry);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  image = gtk_image_new_from_icon_name ("document-open", GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_widget_show (image);

  /* set the current custom command (if any) */
  helper = xfce_mime_helper_database_get_custom (chooser->database, chooser->category);
  if (G_LIKELY (helper != NULL))
    {
      command = xfce_mime_helper_get_command (helper);
      if (G_LIKELY (command != NULL))
        gtk_entry_set_text (GTK_ENTRY (entry), command);
    }

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      /* change the custom command in the database */
      command = gtk_entry_get_text (GTK_ENTRY (entry));
      xfce_mime_helper_database_set_custom (chooser->database, chooser->category, command);

      /* reload the custom helper */
      helper = xfce_mime_helper_database_get_custom (chooser->database, chooser->category);
      if (G_LIKELY (helper != NULL))
        {
          /* hide the dialog */
          gtk_widget_hide (dialog);

          /* use menu_activate() to set the custom application as default */
          g_object_set_data_full (G_OBJECT (item), I_("exo-helper"), helper, g_object_unref);
          menu_activate (item, chooser);
        }
    }

  gtk_widget_destroy (dialog);
}



static void
xfce_mime_helper_chooser_pressed (XfceMimeHelperChooser *chooser,
                            GtkWidget        *button)
{
  AtkRelationSet *relations;
  AtkRelation    *relation;
  AtkObject      *object;
  const gchar    *icon_name;
  XfceMimeHelper *helper;
  GMainLoop      *loop;
  GdkCursor      *cursor;
  GtkWidget      *image;
  GtkWidget      *menu;
  GtkAllocation   menu_allocation;
  GtkWidget      *item;
  GtkWidget      *item_hbox;
  GtkWidget      *item_label;
  GList          *helpers;
  GList          *lp;
  gint            scale_factor;
  GtkAllocation   chooser_allocation;

  /* Catch button-release-event params and discard */
  GdkEvent       *event;
  gboolean        handled;

  g_return_if_fail (XFCE_MIME_IS_HELPER_CHOOSER (chooser));
  g_return_if_fail (GTK_IS_BUTTON (button));

  /* set a watch cursor while loading the menu */
  if (G_LIKELY (gtk_widget_get_window (button) != NULL))
    {
      cursor = gdk_cursor_new_for_display (gdk_window_get_display (gtk_widget_get_window (button)), GDK_WATCH);
      gdk_window_set_cursor (gtk_widget_get_window (button), cursor);
      g_object_unref (cursor);
      gdk_display_flush (gdk_display_get_default ());
    }

  /* allocate a new menu */
  menu = gtk_menu_new ();
  g_object_ref_sink (G_OBJECT (menu));
  gtk_menu_set_reserve_toggle_size (GTK_MENU (menu), FALSE);
  gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (button));

  /* set Atk popup-window relation for the menu */
  object = gtk_widget_get_accessible (button);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (menu));
  relation = atk_relation_new (&object, 1, ATK_RELATION_POPUP_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  scale_factor = gtk_widget_get_scale_factor (button);

  /* append menu items for all available helpers */
  helpers = xfce_mime_helper_database_get_all (chooser->database, chooser->category);
  for (lp = helpers; lp != NULL; lp = lp->next)
    {
      cairo_surface_t *surface;

      /* determine the helper */
      helper = XFCE_MIME_HELPER (lp->data);

      /* create a menu item for the helper */
      item_label = gtk_label_new (xfce_mime_helper_get_name (helper));
      g_object_set (item_label, "xalign", 0.0f, "yalign", 0.5f, NULL);
      item_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_box_pack_end (GTK_BOX (item_hbox), item_label, TRUE, TRUE, 0);
      item = gtk_menu_item_new ();
      gtk_container_add (GTK_CONTAINER (item), item_hbox);

      /* try to load the icon for the helper */
      icon_name = xfce_mime_helper_get_icon (helper);
      surface = xfce_mime_helper_chooser_load_app_icon (icon_name, scale_factor);
      if (G_LIKELY (surface != NULL))
        {
          image = gtk_image_new_from_surface (surface);
          gtk_box_pack_start (GTK_BOX (item_hbox), image, FALSE, FALSE, 0);
          cairo_surface_destroy (surface);
        }

      /* finish setting up the menu item and add it */
      g_object_set_data_full (G_OBJECT (item), I_("exo-helper"), helper, g_object_unref);
      g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (menu_activate), chooser);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show_all (item);
    }

  if (G_LIKELY (helpers != NULL))
    {
      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  item = gtk_menu_item_new_with_mnemonic (_("_Other..."));
  gtk_widget_set_tooltip_text (item, _("Use a custom application which is not included in the above list."));
  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (menu_activate_other), chooser);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* make sure the menu has atleast the same width as the chooser */
  gtk_widget_get_allocation (menu, &menu_allocation);
  gtk_widget_get_allocation (GTK_WIDGET (chooser), &chooser_allocation);
  if (menu_allocation.width < chooser_allocation.width)
    gtk_widget_set_size_request (menu, chooser_allocation.width, -1);

  /* reset the watch cursor on the chooser */
  if (G_LIKELY (gtk_widget_get_window (button) != NULL))
    gdk_window_set_cursor (gtk_widget_get_window (button), NULL);

  /* allocate a new main loop */
  loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect_swapped (G_OBJECT (menu), "deactivate", G_CALLBACK (g_main_loop_quit), loop);

  /* run the loop for the menu */
  gtk_grab_add (menu);
  gtk_menu_popup_at_widget (GTK_MENU (menu), button, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
  g_main_loop_run (loop);
  gtk_grab_remove (menu);
  g_main_loop_unref (loop);

  g_signal_emit_by_name (button, "button-release-event", &event, &handled);
  g_object_unref (G_OBJECT (menu));
}



/**
 * xfce_mime_helper_chooser_new:
 * @category : the #XfceMimeHelperCategory for the chooser.
 *
 * Allocates a new #XfceMimeHelperChooser for the given
 * @category.
 *
 * Return value: the newly allocated #XfceMimeHelperChooser.
 **/
GtkWidget*
xfce_mime_helper_chooser_new (XfceMimeHelperCategory category)
{
  g_return_val_if_fail (category < XFCE_MIME_HELPER_N_CATEGORIES, NULL);
  return g_object_new (XFCE_MIME_TYPE_HELPER_CHOOSER, "category", category, NULL);
}



/**
 * xfce_mime_helper_chooser_get_category:
 * @chooser : a #XfceMimeHelperChooser.
 *
 * Returns the #XfceMimeHelperCategory which is configured
 * by @chooser.
 *
 * Return value: the category for @chooser.
 **/
XfceMimeHelperCategory
xfce_mime_helper_chooser_get_category (const XfceMimeHelperChooser *chooser)
{
  g_return_val_if_fail (XFCE_MIME_IS_HELPER_CHOOSER (chooser), XFCE_MIME_HELPER_WEBBROWSER);
  return chooser->category;
}



/**
 * xfce_mime_helper_chooser_set_category:
 * @chooser  : a #XfceMimeHelperChooser.
 * @category : a #XfceMimeHelperCategory.
 *
 * Sets the category for @chooser to @category.
 **/
void
xfce_mime_helper_chooser_set_category (XfceMimeHelperChooser *chooser,
                                 XfceMimeHelperCategory category)
{
  g_return_if_fail (XFCE_MIME_IS_HELPER_CHOOSER (chooser));
  g_return_if_fail (category < XFCE_MIME_HELPER_N_CATEGORIES);

  /* apply the new category */
  chooser->category = category;
  xfce_mime_helper_chooser_update (chooser);
  g_object_notify (G_OBJECT (chooser), "category");
}



/**
 * xfce_mime_helper_chooser_get_is_valid:
 * @chooser : a #XfceMimeHelperChooser.
 *
 * Returns %TRUE if a valid helper is selected for
 * @chooser.
 *
 * Return value: %TRUE if a valid helper is selected.
 **/
gboolean
xfce_mime_helper_chooser_get_is_valid (const XfceMimeHelperChooser *chooser)
{
  g_return_val_if_fail (XFCE_MIME_IS_HELPER_CHOOSER (chooser), FALSE);
  return chooser->is_valid;
}
