/*
 * Copyright (c) 2006-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2019      The Xfce Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

/**
 * SECTION:xfce-dialogs
 * @title: XfceDialogs
 * @short_description: A collection of helper dialogs
 * @stability: Stable
 * @include: libxfce4ui/libxfce4ui.h
 *
 * Xfce-dialogs are a collection of helper dialogs to display
 * the help dialog with link to the docs website, warning, info, and
 * error dialogs and more.
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include <libxfce4ui/xfce-dialogs.h>
#include <libxfce4ui/xfce-gtk-extensions.h>
#include <libxfce4ui/xfce-gdk-extensions.h>
#include <libxfce4ui/xfce-spawn.h>
#include <libxfce4ui/libxfce4ui-private.h>
#include <libxfce4ui/libxfce4ui-alias.h>

#include "libxfce4ui-resources.h"

static void
xfce_dialog_show_help_auto_toggled (GtkWidget *button)
{
  XfceRc   *rc;
  gboolean  active = FALSE;

  if (button != NULL)
    active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "xfce4/help.rc", FALSE);
  if (rc != NULL)
    {
      xfce_rc_write_bool_entry (rc, "auto-online", active);
      xfce_rc_close (rc);
    }
}



static void
xfce_dialog_show_help_uri (GdkScreen *screen,
                           GtkWindow *parent,
                           GString   *uri)
{
  GError   *error = NULL;
  gchar    *path;
  gchar    *cmd;
  gboolean  result;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

  path = g_find_program_in_path ("exo-open");
  if (G_LIKELY (path != NULL))
    {
      cmd = g_strdup_printf ("%s --launch WebBrowser '%s'", path, uri->str);

      result = xfce_spawn_command_line_on_screen (screen, cmd, FALSE, TRUE, &error);

      g_free (path);
      g_free (cmd);
    }
  else
    {
      /* not very likely to happen, but it is possible exo is not installed */
      result = gtk_show_uri_on_window (parent, uri->str, gtk_get_current_event_time (), &error);
    }

  if (!result)
    {
      xfce_dialog_show_error (parent, error,
          _("Failed to open web browser for online documentation"));
      g_error_free (error);
    }
}



static void
xfce_dialog_show_help_response (GtkWidget *dialog,
                                gint       response_id,
                                GString   *uri)
{
  gtk_widget_hide (dialog);

  if (response_id == GTK_RESPONSE_YES)
    {
      xfce_dialog_show_help_uri (gtk_widget_get_screen (dialog),
                                 gtk_window_get_transient_for (GTK_WINDOW (dialog)),
                                 uri);
    }
  else
    {
      /* unset auto */
      xfce_dialog_show_help_auto_toggled (NULL);
    }

  g_string_free (uri, TRUE);
  gtk_widget_destroy (dialog);
}



/**
 * xfce_dialog_show_help:
 * @parent    : (nullable): transient parent of the dialog, or %NULL.
 * @component : (nullable): name of the component opening the help page or %NULL. If the
 *              value is %NULL the target will be the main page of the
 *              documentation website.
 * @page      : (nullable): subpage of the @component on the website or %NULL.
 * @offset    : (nullable): anchor offset in @page or %NULL.
 *
 * Asks the user to visit the online documentation. If confirmed, it will open
 * the webbrowser and redirect the user to the correct location.
 *
 * Appart from the @component, @page and @offset the following information
 * is also send to the server: user language and the xfce_version_string().
 *
 * See also: xfce_dialog_show_help_with_version().
 *
 * Since: 4.10
 */
void
xfce_dialog_show_help (GtkWindow   *parent,
                       const gchar *component,
                       const gchar *page,
                       const gchar *offset)
{
  xfce_dialog_show_help_with_version (parent, component, page, offset, NULL);
}



/**
 * xfce_dialog_show_help_with_version:
 * @parent    : (nullable): transient parent of the dialog, or %NULL.
 * @component : (nullable): name of the component opening the help page or %NULL. If the
 *              value is %NULL the target will be the main page of the
 *              documentation website.
 * @page      : (nullable): subpage of the @component on the website or %NULL.
 * @offset    : (nullable): anchor offset in @page or %NULL.
 * @version   : (nullable): alternative version, or %NULL to use xfce_version_string().
 *
 * Asks the user to visit the online documentation. If confirmed, it will open
 * the webbrowser and redirect the user to the correct location.
 *
 * Apart from the @component, @page and @offset the following information
 * is also sent to the server: user language and the xfce_version_string()
 * or @version if set.
 *
 * See also: xfce_dialog_show_help().
 *
 * Since: 4.12
 *
 */
void
xfce_dialog_show_help_with_version (GtkWindow   *parent,
                                    const gchar *component,
                                    const gchar *page,
                                    const gchar *offset,
                                    const gchar *version)
{
  GtkWidget   *dialog;
  const gchar *name;
  gchar       *primary;
  GString     *uri;
  gchar       *locale;
  GtkWidget   *message_box;
  GtkWidget   *button;
  XfceRc      *rc;
  gboolean     auto_online;
  GdkScreen   *screen;

  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

  /* get the user's locale without encoding */
  locale = g_strdup (setlocale (LC_MESSAGES, NULL));
  if (G_LIKELY (locale != NULL))
    locale = g_strdelimit (locale, ".", '\0');
  else
    locale = g_strdup ("C");

  /* use desktop version if none is set */
  if (version == NULL)
    version = xfce_version_string ();

  /* build the redirect uri */
  uri = g_string_new (MANUAL_WEBSITE);
  g_string_append_printf (uri, "?version=%s&locale=%s", version, locale);
  g_free (locale);

  if (component != NULL)
    g_string_append_printf (uri, "&component=%s", component);
  if (page != NULL)
    g_string_append_printf (uri, "&page=%s", page);
  if (offset != NULL)
    g_string_append_printf (uri, "&offset=%s", offset);

  /* check if we should automatically go online */
  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "xfce4/help.rc", TRUE);
  if (rc != NULL)
    {
      auto_online = xfce_rc_read_bool_entry (rc, "auto-online", FALSE);
      xfce_rc_close (rc);

      if (auto_online)
        {
          if (parent != NULL)
            screen = gtk_window_get_screen (GTK_WINDOW (parent));
          else
            screen = xfce_gdk_screen_get_active (NULL);

          xfce_dialog_show_help_uri (screen, parent, uri);
          g_string_free (uri, TRUE);

          return;
        }
    }

  /* try to get a translated name of the application */
  name = g_get_application_name ();
  if (g_strcmp0 (name, g_get_prgname ()) == 0)
    name = NULL;

  /* try to get a decent primary text */
  if (name != NULL)
    primary = g_strdup_printf (_("Do you want to read the %s manual online?"), name);
  else
    primary = g_strdup (_("Do you want to read the manual online?"));

  dialog = xfce_message_dialog_new (parent,
                                    NULL,
                                    "dialog-question",
                                    primary,
                                    _("You will be redirected to the documentation website "
                                      "where the help pages are maintained and translated."),
                                    _("_Cancel"),
                                    GTK_RESPONSE_NO,
                                    _("_Read Online"),
                                    GTK_RESPONSE_YES,
                                    NULL);
  g_free (primary);


  message_box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  g_return_if_fail (GTK_IS_BOX (message_box));

  button = gtk_check_button_new_with_mnemonic (_("_Always go directly to the online documentation"));
  gtk_box_pack_end (GTK_BOX (message_box), button, FALSE, TRUE, 0);
  g_object_set (G_OBJECT (button), "halign", GTK_ALIGN_END, "margin-start", 6, "margin-end", 6, NULL);
  gtk_widget_set_hexpand (button, TRUE);
  g_signal_connect (G_OBJECT (button), "toggled",
      G_CALLBACK (xfce_dialog_show_help_auto_toggled), NULL);
  gtk_widget_show (button);

  /* don't focus the checkbutton */
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
  button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
  gtk_widget_grab_focus (button);

  /* show the dialog without locking the mainloop */
  gtk_window_set_modal (GTK_WINDOW (dialog), parent != NULL);
  g_signal_connect (G_OBJECT (dialog), "response",
      G_CALLBACK (xfce_dialog_show_help_response), uri);
  gtk_window_present (GTK_WINDOW (dialog));
}



/**
 * xfce_dialog_show_info:
 * @parent         : (nullable): transient parent of the dialog, or %NULL.
 * @secondary_text : (nullable): secondary text of the dialog or %NULL.
 * @primary_format : the printf()-style format for the primary problem description.
 * @...            : argument list for the @format.
 *
 * Displays an information dialog on @parent using the @primary_format as message.
 */
void
xfce_dialog_show_info (GtkWindow   *parent,
                       const gchar *secondary_text,
                       const gchar *primary_format,
                       ...)
{
  va_list  args;
  gchar   *primary_text;

  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

  va_start (args, primary_format);
  primary_text = g_strdup_vprintf (primary_format, args);
  va_end (args);

  xfce_message_dialog (parent, NULL,
                       "dialog-information",
                       primary_text, secondary_text,
                       XFCE_BUTTON_TYPE_MIXED, "window-close-symbolic", _("Close"),
                       GTK_RESPONSE_CLOSE, NULL);

  g_free (primary_text);
}



/**
 * xfce_dialog_show_warning:
 * @parent         : (nullable): transient parent of the dialog, or %NULL.
 * @secondary_text : (nullable): secondary text of the dialog or %NULL.
 * @primary_format : the printf()-style format for the primary problem description.
 * @...            : argument list for the @format.
 *
 * Displays a warning dialog on @parent using the @primary_format as message.
 */
void
xfce_dialog_show_warning (GtkWindow   *parent,
                          const gchar *secondary_text,
                          const gchar *primary_format,
                          ...)
{
  va_list  args;
  gchar   *primary_text;

  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

  va_start (args, primary_format);
  primary_text = g_strdup_vprintf (primary_format, args);
  va_end (args);

  xfce_message_dialog (parent, NULL,
                       "dialog-warning",
                       primary_text, secondary_text,
                       XFCE_BUTTON_TYPE_MIXED, "window-close-symbolic", _("Close"),
                       GTK_RESPONSE_CLOSE, NULL);

  g_free (primary_text);
}



/**
 * xfce_dialog_show_error:
 * @parent         : (nullable): transient parent of the dialog, or %NULL.
 * @error          : (nullable): a #GError, which gives a more precise description of the problem or %NULL.
 * @primary_format : the printf()-style format for the primary problem description.
 * @...            : argument list for the @primary_format.
 *
 * Displays an error dialog on @parent using the @primary_format as primary message and optionally
 * displaying @error as secondary error text.
 */
void
xfce_dialog_show_error (GtkWindow    *parent,
                        const GError *error,
                        const gchar  *primary_format,
                        ...)
{
  va_list  args;
  gchar   *primary_text;

  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

  va_start (args, primary_format);
  primary_text = g_strdup_vprintf (primary_format, args);
  va_end (args);

  xfce_message_dialog (parent, NULL,
                       "dialog-error",
                       primary_text, error ? error->message : NULL,
                       XFCE_BUTTON_TYPE_MIXED, "window-close-symbolic", _("Close"),
                       GTK_RESPONSE_CLOSE, NULL);

  g_free (primary_text);
}



/**
 * xfce_dialog_confirm:
 * @parent         : (nullable): transient parent of the dialog, or %NULL.
 * @stock_id       : the stock name of the confirm button, for example #GTK_STOCK_YES or #GTK_STOCK_CLEAR.
 * @confirm_label  : (nullable): if non-%NULL, this text is used on the confirm button together with the @stock_id icon.
 * @secondary_text : (nullable): secondary text in the dialog.
 * @primary_format : (nullable): the printf()-style format for the dialog question.
 * @...            : (nullable): argument list for the @primary_format.
 *
 * Runs a questions dialog that has a 'Cancel' and a 'Confirm' button. The 'Confirm'
 * button text can be set by @action if given.
 *
 * If @stock_id is equal to #GTK_STOCK_YES, the 'Cancel' button becomes a 'No' button.
 *
 * Return value: TRUE if the user confirms, else FALSE.
 */
gboolean
xfce_dialog_confirm (GtkWindow   *parent,
                     const gchar *stock_id,
                     const gchar *confirm_label,
                     const gchar *secondary_text,
                     const gchar *primary_format,
                     ...)
{
  va_list      args;
  gchar       *primary_text;
  const gchar *no_stock_id;
  gint         response_id;

  g_return_val_if_fail (stock_id != NULL || confirm_label != NULL, FALSE);
  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), FALSE);

  va_start (args, primary_format);
  primary_text = g_strdup_vprintf (primary_format, args);
  va_end (args);

  /* whether this will be a yes/no dialog */
  if (stock_id != NULL && (strcmp (stock_id, "gtk-yes") == 0 || strcmp (stock_id, "yes") == 0))
    {
      no_stock_id = _("No");
      if (confirm_label == NULL)
        confirm_label = _("Yes");
    }
  else
    {
      no_stock_id = _("Cancel");
    }

  response_id = xfce_message_dialog (parent, NULL,
                                     "dialog-question",
                                     primary_text,
                                     secondary_text,
                                     no_stock_id, GTK_RESPONSE_NO,
                                     XFCE_BUTTON_TYPE_MIXED, stock_id, confirm_label, GTK_RESPONSE_YES,
                                     NULL);

  g_free (primary_text);

  return (response_id == GTK_RESPONSE_YES);
}



/**
 * xfce_dialog_confirm_close_tabs:
 * @parent              : (nullable): transient parent of the dialog, or %NULL.
 * @num_tabs            : the number of open tabs for display to user
 * @show_confirm_box    : whether to ask the user if this confirmation shall be shown in the future
 * @confirm_box_checked : (nullable): state of confirmation checkbox
 *
 * Runs a dialog to ask the user whether they want to close the whole window,
 * close the current tab, or cancel.
 *
 * If @num_tabs is non-negative, the message to the user will state that there
 * are @num_tabs open tabs. If @num_tabs is negative, then the message to the
 * user will state simply that there are "multiple open tabs".
 *
 * If @show_confirm_box is %TRUE a checkbox is added to the dialog to allow
 * the user to set whether they wish to see this dialog in future. The initial
 * state of the checkbox is determined by the value stored at @confirm_box_checked
 * and the value at @confirm_box_checked after returning records the state of the
 * checkbox. If @show_confirm_box is %FALSE, @confirm_box_checked is ignored and
 * may be %NULL.
 *
 * Return value: #GTK_RESPONSE_CANCEL if cancelled, #GTK_RESPONSE_YES if the user
 * wants to close the window, #GTK_RESPONSE_CLOSE if the user wants to close the tab,
 * and #GTK_RESPONSE_NONE for an error.
 *
 * Since: 4.16
 */
gint
xfce_dialog_confirm_close_tabs (GtkWindow *parent,
                                gint       num_tabs,
                                gboolean   show_confirm_box,
                                gboolean  *confirm_box_checked)
{
  GtkWidget *dialog, *checkbutton, *vbox;
  const gchar *primary_text, *warning_icon;
  gchar *secondary_text;
  gint response;

  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), GTK_RESPONSE_NONE);
  g_return_val_if_fail (!show_confirm_box || confirm_box_checked != NULL, GTK_RESPONSE_NONE);

  primary_text = _("Close window with multiple tabs?");
  if (num_tabs < 0)
    secondary_text = g_strdup (_("This window has multiple tabs open. Closing this window "
                                "will also close all its tabs."));
  else
    secondary_text = g_strdup_printf (_("This window has %d tabs open. Closing this window "
                               "will also close all its tabs."), num_tabs);

  warning_icon = "dialog-warning";

  dialog = xfce_message_dialog_new (parent,
                                    _("Warning"),
                                    warning_icon,
                                    primary_text,
                                    secondary_text,
                                    XFCE_BUTTON_TYPE_MIXED, NULL, _("_Cancel"), GTK_RESPONSE_CANCEL,
                                    XFCE_BUTTON_TYPE_MIXED, NULL, _("Close T_ab"), GTK_RESPONSE_CLOSE,
                                    XFCE_BUTTON_TYPE_MIXED, NULL, _("Close _Window"), GTK_RESPONSE_YES,
                                    NULL);

  if (show_confirm_box)
    {
      checkbutton = gtk_check_button_new_with_mnemonic (_("Do _not ask me again"));
      vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
      gtk_box_pack_end (GTK_BOX (vbox), checkbutton, FALSE, FALSE, 5);
      g_object_set (G_OBJECT (checkbutton), "halign", GTK_ALIGN_END, "margin-start", 6, "margin-end", 6, NULL);
      gtk_widget_set_hexpand (checkbutton, TRUE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), *confirm_box_checked);
    }

  gtk_widget_show_all (dialog);
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (show_confirm_box)
    *confirm_box_checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton));

  gtk_widget_destroy (dialog);
  g_free (secondary_text);
  return response;
}



/**
 * xfce_message_dialog_new_valist:
 * @parent            : (nullable): transient parent of the dialog, or %NULL.
 * @title             : (nullable): title of the dialog, or %NULL.
 * @icon_stock_id     : gtk stock icon name to show in the dialog.
 * @primary_text      : primary text shown in large bold font.
 * @secondary_text    : secondary text shown in normal font.
 * @first_button_text : text for the first button.
 * @args              : argument list.
 *
 * See xfce_message_dialog_new(), this version takes a va_list for
 * language bindings to use.
 *
 * Returns: (transfer none): A new #GtkMessageDialog.
 **/
GtkWidget *
xfce_message_dialog_new_valist (GtkWindow   *parent,
                                const gchar *title,
                                const gchar *icon_stock_id,
                                const gchar *primary_text,
                                const gchar *secondary_text,
                                const gchar *first_button_text,
                                va_list      args)
{
  GtkBuilder  *gxml;
  GtkWidget   *dialog;
  GtkWidget   *dialog_image;
  GtkWidget   *image;
  GtkWidget   *button;
  GtkWidget   *content_area;
  GtkWidget   *label_box;
  const gchar *text = first_button_text;
  const gchar *label;
  const gchar *stock_id;
  gint         response;
  GdkPixbuf   *pixbuf, *scaled;
  gint         w, h;
  GList       *children;

  g_return_val_if_fail (primary_text != NULL || secondary_text != NULL, NULL);
  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);


  gxml = gtk_builder_new_from_resource ("/org/xfce/libxfce4ui/libxfce4ui-dialog-ui.ui");

  dialog = GTK_WIDGET(gtk_builder_get_object(gxml, "xfce4ui-dialog"));
  label_box = GTK_WIDGET(gtk_builder_get_object(gxml, "label-box"));
  dialog_image = GTK_WIDGET(gtk_builder_get_object(gxml, "icon_stock_id"));
  gtk_window_set_default_size (GTK_WINDOW (dialog), 500, -1);

  /* Remove the original message area of the GtkMessageDialog as we add our own */
  content_area = GTK_WIDGET (gtk_dialog_get_content_area (GTK_DIALOG (dialog)));
  children = gtk_container_get_children (GTK_CONTAINER (content_area));
  gtk_container_remove (GTK_CONTAINER (content_area), GTK_WIDGET ((g_list_nth (children, 1))->data));
  g_list_free (children);

  if (parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
    }

  if (primary_text != NULL)
    {
      /* escape ampersand symbols, etc. (bug #12715) */
      gchar *escaped_text = g_markup_escape_text (primary_text, -1);

      /* Add a top line of large bold text */
      GtkWidget *primary_label = gtk_label_new (NULL);
      gchar *markedup_text = g_strdup_printf ("<span weight='bold' size='large'>%s</span>", escaped_text);

      gtk_label_set_markup (GTK_LABEL (primary_label), markedup_text);
      gtk_label_set_xalign (GTK_LABEL (primary_label), 0);
      gtk_widget_set_vexpand (GTK_WIDGET (primary_label), TRUE);
      gtk_widget_set_valign (GTK_WIDGET (primary_label), 1.0);

      gtk_container_add (GTK_CONTAINER (label_box), primary_label);
      gtk_widget_show (primary_label);

      g_free (escaped_text);
      g_free (markedup_text);
    }

  if (secondary_text != NULL)
    {
      /* Add the secondary text, no special formatting done */
      GtkWidget *secondary_label = gtk_label_new (secondary_text);
      gtk_label_set_line_wrap (GTK_LABEL (secondary_label), TRUE);
      gtk_label_set_xalign (GTK_LABEL (secondary_label), 0.0);
      gtk_label_set_max_width_chars (GTK_LABEL (secondary_label), 80);
      gtk_widget_set_vexpand (GTK_WIDGET (secondary_label), TRUE);
      gtk_widget_set_valign (GTK_WIDGET (secondary_label), 0.0);

      gtk_container_add (GTK_CONTAINER (label_box), secondary_label);
      gtk_widget_show (secondary_label);
    }

  if (title != NULL)
    gtk_window_set_title (GTK_WINDOW (dialog), title);

  /* put the dialog on the active screen if no parent is defined */
  if (parent == NULL)
    xfce_gtk_window_center_on_active_screen (GTK_WINDOW (dialog));

  /* add buttons */
  while (text != NULL)
    {
      if (strcmp (text, XFCE_BUTTON_TYPE_MIXED) == 0)
        {
          /* get arguments */
          stock_id = va_arg (args, const gchar *);
          label = va_arg (args, const gchar *);
          response = va_arg (args, gint);

          /* add a mixed button to the dialog */
          button = xfce_gtk_button_new_mixed (stock_id, label);
          gtk_widget_set_can_default (button, TRUE);
          gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, response);
          gtk_widget_show (button);
        }
      else if (strcmp (text, XFCE_BUTTON_TYPE_PIXBUF) == 0)
        {
          /* get arguments */
          pixbuf = va_arg (args, GdkPixbuf *);
          label = va_arg (args, const gchar *);
          response = va_arg (args, gint);

          /* lookup real icons size for button icons */
          gtk_icon_size_lookup (GTK_ICON_SIZE_BUTTON, &w, &h);

          /* scale the pixbuf if needed */
          if (gdk_pixbuf_get_width (pixbuf) != w || gdk_pixbuf_get_height (pixbuf) != h)
            scaled = gdk_pixbuf_scale_simple (pixbuf, w, h, GDK_INTERP_BILINEAR);
          else
            scaled = NULL;

          image = gtk_image_new_from_pixbuf (scaled ? scaled : pixbuf);

          /* release scaled image */
          if (scaled != NULL)
            g_object_unref (G_OBJECT (scaled));

          /* create button and add it to the dialog */
          button = gtk_button_new_with_label (label);
          gtk_button_set_image (GTK_BUTTON (button), image);
          gtk_widget_set_can_default (button, TRUE);
          gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, response);
          gtk_widget_show (button);
        }
      else /* stock button */
        {
          /* get arguments */
          stock_id = text;
          response = va_arg (args, gint);

          /* add a stock button to the dialog */
          gtk_dialog_add_button (GTK_DIALOG (dialog), stock_id, response);
        }

      /* get the next argument */
      text = va_arg (args, const gchar *);
    }

  if (icon_stock_id != NULL)
    {
      /* set dialog and window icon */
      gtk_image_set_from_icon_name (GTK_IMAGE (dialog_image), icon_stock_id, GTK_ICON_SIZE_DIALOG);

      gtk_widget_show (dialog_image);
      gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_stock_id);
    }

  g_object_unref (gxml);

  return dialog;
}



/**
 * xfce_message_dialog_new:
 * @parent            : (nullable): transient parent of the dialog, or %NULL.
 * @title             : (nullable): title of the dialog, or %NULL.
 * @stock_id          : gtk stock icon name to show in the dialog.
 * @primary_text      : primary text shown in large bold font.
 * @secondary_text    : secondary text shown in normal font.
 * @first_button_text : text for the first button.
 * @...               : %NULL terminated list of parameters.
 *
 * xfce_message_dialog_new() allows you to easily create Gtk+ message dialogs.
 * It accepts GTK+ stock buttons (deprecated), mixed buttons (using XFCE_BUTTON_TYPE_MIXED)
 * or buttons with a #GdkPixbuf (using XFCE_BUTTON_TYPE_PIXBUF).
 *
 * The buttons are defined by @first_button_text and the next arguments in the
 * following format @type, @param1[, @param2, @param3].
 *
 * <variablelist>
 *   <varlistentry>
 *     <term><varname>XFCE_BUTTON_TYPE_MIXED</varname></term>
 *     <listitem>
 *       <para>
 *         This allows you to easily create mixed buttons in a dialog.
 *         @param1 is used for the icon name, @param2 for the label and
 *         @param3 for the response_id. See also xfce_gtk_button_new_mixed().
 *       </para>
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><varname>XFCE_BUTTON_TYPE_PIXBUF</varname></term>
 *     <listitem>
 *       <para>
 *         Creates a button with the #GdkPixbuf as button icon.
 *         @param1 is the #GdkPixbuf, @param2 for the label and
 *         @param3 for the response_id.
 *       </para>
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>Stock Buttons</term>
 *     <listitem>
 *       <para>
 *         When the variables above were not matched, the button type
 *         will be a stock button. @type will be the stock id, @param1
 *         is used for the response_id.
 *       </para>
 *     </listitem>
 *   </varlistentry>
 * </variablelist>
 *
 * To clarify this behaviour see the example below. We create a dialog with
 * two stock buttons, a #GdkPixbuf button and a mixed button.
 *
 * <example>
 * <title>Creating a Xfce Message Dialog</title>
 * <programlisting>
 * GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (filename, 24, 24);
 *
 * GtkWidget *dialog = xfce_message_dialog (parent, "Question",
 *                                          GTK_STOCK_DIALOG_QUESTION,
 *                                          "There are unsaved modifications",
 *                                          "The menu has been modified, do you want to save it before quitting?",
 *                                          GTK_STOCK_SAVE, GTK_RESPONSE_YES,
 *                                          XFCE_BUTTON_TYPE_MIXED, "edit-delete", _("Forget modifications"), GTK_RESPONSE_APPLY,
 *                                          XFCE_BUTTON_TYPE_PIXBUF, pixbuf, "Quit", GTK_RESPONSE_NO,
 *                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
 *                                          NULL);
 *
 * g_object_unref (G_OBJECT (pixbuf));
 * </programlisting>
 * </example>
 *
 * The caller is responsible for destroying the dialog with gtk_widget_destroy()
 * when it is no longer required.
 *
 * Return value: (transfer none): A new #GtkMessageDialog.
 **/
GtkWidget *
xfce_message_dialog_new (GtkWindow   *parent,
                         const gchar *title,
                         const gchar *stock_id,
                         const gchar *primary_text,
                         const gchar *secondary_text,
                         const gchar *first_button_text,
                         ...)
{
  va_list    args;
  GtkWidget *dialog;

  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);

  va_start (args, first_button_text);
  dialog = xfce_message_dialog_new_valist (parent, title, stock_id, primary_text,
                                           secondary_text, first_button_text, args);
  va_end (args);

  return dialog;
}



/**
 * xfce_message_dialog:
 * @parent            : (nullable): transient parent of the dialog, or %NULL.
 * @title             : (nullable): title of the dialog, or %NULL.
 * @stock_id          : gtk stock icon name to show in the dialog.
 * @primary_text      : primary text shown in large bold font.
 * @secondary_text    : secondary text shown in normal font.
 * @first_button_text : text for the first button.
 * @...               : %NULL ended list of parameters.
 *
 * Create a new dialog as in xfce_message_dialog_new(), then runs the
 * dialog using #gtk_dialog_run and return the response id selected by the user.
 *
 * See xfce_message_dialog_new() for more information.
 *
 * Returns: the selected response id.
 **/
gint
xfce_message_dialog (GtkWindow   *parent,
                     const gchar *title,
                     const gchar *stock_id,
                     const gchar *primary_text,
                     const gchar *secondary_text,
                     const gchar *first_button_text,
                     ...)
{
  va_list    args;
  GtkWidget *dialog;
  gint       response_id;

  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), 0);

  va_start (args, first_button_text);
  dialog = xfce_message_dialog_new_valist (parent, title, stock_id, primary_text,
                                           secondary_text, first_button_text, args);
  va_end (args);

  response_id = gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  return response_id;
}



#define __XFCE_DIALOGS_C__
#include <libxfce4ui/libxfce4ui-aliasdef.c>
