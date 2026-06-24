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

#include <libxfce4ui/libxfce4ui.h>
#include <xfce-mime-helper-chooser.h>
#include <xfce-mime-helper-enum-types.h>
#include <xfce-mime-helper-launcher-dialog.h>
#include <gtk/gtkx.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CATEGORY,
};



static void xfce_mime_helper_launcher_dialog_get_property     (GObject                      *object,
                                                               guint                         prop_id,
                                                               GValue                       *value,
                                                               GParamSpec                   *pspec);
static void xfce_mime_helper_launcher_dialog_set_property     (GObject                      *object,
                                                               guint                         prop_id,
                                                               const GValue                 *value,
                                                               GParamSpec                   *pspec);
static void xfce_mime_helper_launcher_dialog_notify_is_valid  (XfceMimeHelperChooser        *chooser,
                                                               GParamSpec                   *pspec,
                                                               XfceMimeHelperLauncherDialog *launcher_dialog);



struct _XfceMimeHelperLauncherDialogClass
{
  GtkDialogClass __parent__;
};

struct _XfceMimeHelperLauncherDialog
{
  GtkDialog              __parent__;
  GtkWidget             *label;
  XfceMimeHelperCategory category;
};



G_DEFINE_TYPE (XfceMimeHelperLauncherDialog, xfce_mime_helper_launcher_dialog, GTK_TYPE_DIALOG)



static void
xfce_mime_helper_launcher_dialog_class_init (XfceMimeHelperLauncherDialogClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = xfce_mime_helper_launcher_dialog_get_property;
  gobject_class->set_property = xfce_mime_helper_launcher_dialog_set_property;

  /**
   * XfceMimeHelperLauncherDialog:category:
   *
   * The #XfceMimeHelperCategory for which a launcher dialog will
   * be displayed.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CATEGORY,
                                   g_param_spec_enum ("category",
                                                      "Category",
                                                      "Category",
                                                      XFCE_MIME_TYPE_MIME_HELPER_CATEGORY,
                                                      XFCE_MIME_HELPER_WEBBROWSER,
                                                      EXO_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}



static void
xfce_mime_helper_launcher_dialog_init (XfceMimeHelperLauncherDialog *launcher_dialog)
{
  GtkWidget      *chooser;
  GtkWidget      *image;
  GtkWidget      *hbox;
  GtkWidget      *vbox;

  gtk_dialog_add_button (GTK_DIALOG (launcher_dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (launcher_dialog), _("_OK"), GTK_RESPONSE_OK);
  gtk_dialog_set_default_response (GTK_DIALOG (launcher_dialog), GTK_RESPONSE_OK);
  gtk_window_set_resizable (GTK_WINDOW (launcher_dialog), FALSE);
  gtk_window_set_title (GTK_WINDOW (launcher_dialog), _("Choose Preferred Application"));

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (launcher_dialog))), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  image = gtk_image_new_from_icon_name ("org.xfce.settings.default-applications", GTK_ICON_SIZE_DIALOG);
  g_object_set (image, "halign", GTK_ALIGN_CENTER, "valign", GTK_ALIGN_START, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  launcher_dialog->label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (vbox), launcher_dialog->label, FALSE, FALSE, 0);
  gtk_widget_show (launcher_dialog->label);

  chooser = g_object_new (XFCE_MIME_TYPE_HELPER_CHOOSER, NULL);
  g_signal_connect (G_OBJECT (chooser), "notify::is-valid", G_CALLBACK (xfce_mime_helper_launcher_dialog_notify_is_valid), launcher_dialog);
  g_object_bind_property (G_OBJECT (launcher_dialog), "category", G_OBJECT (chooser), "category", G_BINDING_SYNC_CREATE);
  gtk_box_pack_start (GTK_BOX (vbox), chooser, FALSE, FALSE, 0);
  gtk_widget_show (chooser);

  xfce_gtk_label_set_a11y_relation (GTK_LABEL (launcher_dialog->label), GTK_WIDGET (chooser));
}



static void
xfce_mime_helper_launcher_dialog_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
  XfceMimeHelperLauncherDialog *launcher_dialog = XFCE_MIME_HELPER_LAUNCHER_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      g_value_set_enum (value, xfce_mime_helper_launcher_dialog_get_category (launcher_dialog));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_mime_helper_launcher_dialog_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  XfceMimeHelperLauncherDialog *launcher_dialog = XFCE_MIME_HELPER_LAUNCHER_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      xfce_mime_helper_launcher_dialog_set_category (launcher_dialog, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_mime_helper_launcher_dialog_notify_is_valid (XfceMimeHelperChooser        *chooser,
                                                  GParamSpec                   *pspec,
                                                  XfceMimeHelperLauncherDialog *launcher_dialog)
{
  g_return_if_fail (XFCE_MIME_IS_HELPER_CHOOSER (chooser));
  g_return_if_fail (XFCE_MIME_IS_HELPER_LAUNCHER_DIALOG (launcher_dialog));

  gtk_dialog_set_response_sensitive (GTK_DIALOG (launcher_dialog), GTK_RESPONSE_OK,
                                     xfce_mime_helper_chooser_get_is_valid (chooser));
}



/**
 * xfce_mime_helper_launcher_dialog_new:
 * @category : a #XfceMimeHelperCategory.
 *
 * Allocates a new #XfceMimeHelperLauncherDialog for the
 * specified @category.
 *
 * Return value: the newly allocated #XfceMimeHelperLauncherDialog.
 **/
GtkWidget*
xfce_mime_helper_launcher_dialog_new (XfceMimeHelperCategory category)
{
  g_return_val_if_fail (category < XFCE_MIME_HELPER_N_CATEGORIES, NULL);
  return g_object_new (XFCE_MIME_TYPE_HELPER_LAUNCHER_DIALOG, "category", category, NULL);
}



/**
 * xfce_mime_helper_launcher_dialog_get_category:
 * @launcher_dialog : a #XfceMimeHelperLauncherDialog.
 *
 * Returns the #XfceMimeHelperCategory for @launcher_dialog.
 *
 * Return value: the #XfceMimeHelperCategory for @launcher_dialog.
 **/
XfceMimeHelperCategory
xfce_mime_helper_launcher_dialog_get_category (const XfceMimeHelperLauncherDialog *launcher_dialog)
{
  g_return_val_if_fail (XFCE_MIME_IS_HELPER_LAUNCHER_DIALOG (launcher_dialog), XFCE_MIME_HELPER_WEBBROWSER);
  return launcher_dialog->category;
}



/**
 * xfce_mime_helper_launcher_dialog_set_category:
 * @launcher_dialog : a #XfceMimeHelperLauncherDialog.
 * @category        : a #XfceMimeHelperCategory.
 *
 * Sets the #XfceMimeHelperCategory for @launcher_dialog to
 * @category.
 **/
void
xfce_mime_helper_launcher_dialog_set_category (XfceMimeHelperLauncherDialog *launcher_dialog,
                                               XfceMimeHelperCategory        category)
{
  static const gchar *MESSAGES[] =
  {
    N_("Please choose your preferred Web\n"
       "Browser now and click OK to proceed."),
    N_("Please choose your preferred Mail Reader\n"
       "now and click OK to proceed."),
    N_("Please choose your preferred File Manager\n"
       "now and click OK to proceed."),
    N_("Please choose your preferred Terminal\n"
       "Emulator now and click OK to proceed."),
  };

  /* sanity check the categories */
  g_assert (XFCE_MIME_HELPER_N_CATEGORIES == G_N_ELEMENTS (MESSAGES));

  g_return_if_fail (XFCE_MIME_IS_HELPER_LAUNCHER_DIALOG (launcher_dialog));
  g_return_if_fail (category < XFCE_MIME_HELPER_N_CATEGORIES);

  /* setup the new label text */
  gtk_label_set_text (GTK_LABEL (launcher_dialog->label), gettext (MESSAGES[category]));

  /* apply the new category */
  launcher_dialog->category = category;
  g_object_notify (G_OBJECT (launcher_dialog), "category");
}


