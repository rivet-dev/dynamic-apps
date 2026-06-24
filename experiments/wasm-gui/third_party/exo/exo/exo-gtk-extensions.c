/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <exo/exo-gtk-extensions.h>
#include <exo/exo-private.h>
#include <exo/exo-thumbnail-preview.h>
#include <exo/exo-alias.h>

/**
 * SECTION: exo-gtk-extensions
 * @title: Extensions to Gtk+
 * @short_description: Miscelleanous extensions to the Gtk+ library
 * @include: exo/exo.h
 *
 * Various additional functions to the core API provided by the Gtk+ library.
 *
 * For example, exo_gtk_file_chooser_add_thumbnail_preview() is a
 * convenience method to add a thumbnail based preview widget to a
 * #GtkFileChooser, which will display a preview of the selected file if
 * either a thumbnail is available or a thumbnail could be generated using
 * the GdkPixbuf library.
 **/



static gboolean
later_destroy (gpointer object)
{
  gtk_widget_destroy (GTK_WIDGET (object));
  g_object_unref (G_OBJECT (object));
  return FALSE;
}



/**
 * exo_gtk_object_destroy_later:
 * @object : a #GtkObject.
 *
 * Schedules an idle function to destroy the specified @object
 * when the application enters the main loop the next time.
 **/
void
exo_gtk_object_destroy_later (GtkWidget *object)
{
  g_return_if_fail (GTK_IS_WIDGET (object));

  g_idle_add_full (G_PRIORITY_HIGH, later_destroy, object, NULL);
  g_object_ref_sink (object);
}



static void
update_preview (GtkFileChooser      *chooser,
                ExoThumbnailPreview *thumbnail_preview)
{
  gchar *uri;

  _exo_return_if_fail (EXO_IS_THUMBNAIL_PREVIEW (thumbnail_preview));
  _exo_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  /* update the URI for the preview */
  uri = gtk_file_chooser_get_preview_uri (chooser);
  if (G_UNLIKELY (uri == NULL))
    {
      /* gee, why is there a get_preview_uri() method if
       * it doesn't work in several cases? did anybody ever
       * test this method prior to committing it?
       */
      uri = gtk_file_chooser_get_uri (chooser);
    }
  _exo_thumbnail_preview_set_uri (thumbnail_preview, uri);
  g_free (uri);
}



static void
scale_factor_changed (ExoThumbnailPreview *thumbnail_preview,
                      GParamSpec          *spec,
                      GtkFileChooser      *chooser)
{
    update_preview (chooser, thumbnail_preview);
}



/**
 * exo_gtk_file_chooser_add_thumbnail_preview:
 * @chooser : a #GtkFileChooser.
 *
 * This is a convenience function that adds a preview widget to the @chooser,
 * which displays thumbnails for the selected filenames using the thumbnail
 * database. The preview widget is also able to generate thumbnails for all
 * image formats supported by #GdkPixbuf.
 *
 * Use this function whenever you display a #GtkFileChooser to ask the user
 * to select an image file from the file system.
 *
 * The preview widget also supports URIs other than file:-URIs to a certain
 * degree, but this support is rather limited currently, so you may want to
 * use gtk_file_chooser_set_local_only() to ensure that the user can only
 * select files from the local file system.
 *
 * When @chooser is configured to select multiple image files - using the
 * gtk_file_chooser_set_select_multiple() method - the behaviour of the
 * preview widget is currently undefined, in that it is not defined for
 * which of the selected files the preview will be displayed.
 *
 * Since: 0.3.1.9
 **/
void
exo_gtk_file_chooser_add_thumbnail_preview (GtkFileChooser *chooser)
{
  GtkWidget *thumbnail_preview;

  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  /* add the preview to the file chooser */
  thumbnail_preview = _exo_thumbnail_preview_new ();
  gtk_file_chooser_set_preview_widget (chooser, thumbnail_preview);
  gtk_file_chooser_set_preview_widget_active (chooser, TRUE);
  gtk_file_chooser_set_use_preview_label (chooser, FALSE);
  gtk_widget_show (thumbnail_preview);
  g_signal_connect (G_OBJECT (thumbnail_preview), "notify::scale-factor", G_CALLBACK (scale_factor_changed), chooser);

  /* update the preview as necessary */
  g_signal_connect (G_OBJECT (chooser), "update-preview", G_CALLBACK (update_preview), thumbnail_preview);

  /* initially update the preview, in case the file chooser is already setup */
  update_preview (chooser, EXO_THUMBNAIL_PREVIEW (thumbnail_preview));
}



/**
 * exo_gtk_url_about_dialog_hook:
 * @about_dialog : the #GtkAboutDialog in which the user activated a link.
 * @address      : the link, mail or web address, to open.
 * @user_data    : user data that was passed when the function was
 *                 registered with gtk_about_dialog_set_email_hook()
 *                 or gtk_about_dialog_set_url_hook(). This is currently
 *                 unused within the context of this function, so you
 *                 can safely pass %NULL when registering this hook
 *                 with #GtkAboutDialog.
 *
 * This is a convenience function, which can be registered with #GtkAboutDialog,
 * to open links clicked by the user in #GtkAboutDialog<!---->s.
 *
 * All you need to do is to register this hook with gtk_about_dialog_set_url_hook()
 * and gtk_about_dialog_set_email_hook(). This can be done prior to calling
 * gtk_show_about_dialog(), for example:
 *
 * <informalexample><programlisting>
 * static void show_about_dialog (void)
 * {
 *
 *   gtk_show_about_dialog (.....);
 * }
 * </programlisting></informalexample>
 *
 * This function is not needed when you use Gtk 2.18 or later, because from
 * that version this is implemented by default.
 *
 * Since: 0.5.0
 **/
void
exo_gtk_url_about_dialog_hook (GtkAboutDialog *about_dialog,
                               const gchar    *address,
                               gpointer        user_data)
{
  GtkWidget *message;
  GError    *error = NULL;
  gchar     *uri, *escaped;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about_dialog));
  g_return_if_fail (address != NULL);

  /* simple check if this is an email address */
  if (!g_str_has_prefix (address, "mailto:") && strchr (address, '@') != NULL)
    {
      escaped = g_uri_escape_string (address, NULL, FALSE);
      uri = g_strdup_printf ("mailto:%s", escaped);
      g_free (escaped);
    }
  else
    {
      uri = g_strdup (address);
    }

  /* try to open the url on the given screen */
  if (!gtk_show_uri_on_window (GTK_WINDOW(about_dialog), uri, gtk_get_current_event_time (), &error))
    {
      /* make sure to initialize i18n support first,
       * so we'll see a translated message.
       */
      _exo_i18n_init ();

      /* display an error message to tell the user that we were unable to open the link */
      message = gtk_message_dialog_new (GTK_WINDOW (about_dialog),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                        _("Failed to open \"%s\"."), uri);
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s.", error->message);
      gtk_dialog_run (GTK_DIALOG (message));
      gtk_widget_destroy (message);
      g_error_free (error);
    }

  /* cleanup */
  g_free (uri);
}



/**
 * exo_gtk_dialog_get_action_area:
 * @dialog : a #GtkDialog.
 *
 * Returns the action area of a #GtkDialog. The internal function has been
 * deprecated in GTK+, so this wraps and dispels the deprecation warning.
 *
 * Returns: the action area.
 *
 * Since: 0.11.4
 **/
GtkWidget *
exo_gtk_dialog_get_action_area (GtkDialog *dialog)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    return gtk_dialog_get_action_area (dialog);
G_GNUC_END_IGNORE_DEPRECATIONS
}



/**
 * exo_gtk_dialog_add_secondary_button:
 * @dialog : a #GtkDialog.
 * @button : a #GtkButton to add and mark as secondary.
 *
 * Convenience function to add a secondary button to a #GtkDialog.
 *
 * Since: 0.11.4
 **/
void
exo_gtk_dialog_add_secondary_button (GtkDialog *dialog,
                                     GtkWidget *button)
{
    GtkWidget *button_box;

    button_box = exo_gtk_dialog_get_action_area (dialog);
    gtk_box_pack_start (GTK_BOX (button_box), button, FALSE, FALSE, 0);
    gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (button_box), button, TRUE);
}



#define __EXO_GTK_EXTENSIONS_C__
#include <exo/exo-aliasdef.c>
