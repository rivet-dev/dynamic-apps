/*
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
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
 * SECTION:xfce-titled-dialog
 * @title: XfceTitledDialog
 * @short_description: A titled dialog window
 * @stability: Stable
 * @include: libxfce4ui/libxfce4ui.h
 *
 * #XfceTitledDialog is a titled dialog window supporting an optional
 * subtitle and mixed or pixbuf buttons.
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <libxfce4util/libxfce4util.h>

#include <libxfce4ui/xfce-titled-dialog.h>
#include <libxfce4ui/libxfce4ui-private.h>
#include <libxfce4ui/libxfce4ui-alias.h>
#include <libxfce4ui/xfce-gtk-extensions.h>


#define XFCE_TITLED_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), XFCE_TYPE_TITLED_DIALOG, XfceTitledDialogPrivate))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_SUBTITLE,
};


static GObject *xfce_titled_dialog_constructor    (GType                   type,
                                                   guint                   n_construct_params,
                                                   GObjectConstructParam  *construct_params);
static void     xfce_titled_dialog_finalize       (GObject                *object);
static void     xfce_titled_dialog_get_property   (GObject                *object,
                                                   guint                   prop_id,
                                                   GValue                 *value,
                                                   GParamSpec             *pspec);
static void     xfce_titled_dialog_set_property   (GObject                *object,
                                                   guint                   prop_id,
                                                   const GValue           *value,
                                                   GParamSpec             *pspec);
static void     xfce_titled_dialog_close          (GtkDialog              *dialog);
static void     xfce_titled_dialog_update_window  (XfceTitledDialog       *titled_dialog);
static void     xfce_titled_dialog_update_icon    (XfceTitledDialog       *titled_dialog);



struct _XfceTitledDialogPrivate
{
  GtkWidget *headerbar;
  GtkWidget *icon;
  GtkWidget *action_area;
  GtkWidget *subtitle_label;
  GtkWidget *subtitle_separator;
  GdkPixbuf *pixbuf;
  gchar     *subtitle;
  gboolean   use_header;
};

typedef struct _ResponseData ResponseData;

struct _ResponseData
{
  gint response_id;
};



G_DEFINE_TYPE_WITH_PRIVATE (XfceTitledDialog, xfce_titled_dialog, GTK_TYPE_DIALOG)



static void
xfce_titled_dialog_class_init (XfceTitledDialogClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GtkBindingSet  *binding_set;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructor = xfce_titled_dialog_constructor;
  gobject_class->get_property = xfce_titled_dialog_get_property;
  gobject_class->set_property = xfce_titled_dialog_set_property;
  gobject_class->finalize = xfce_titled_dialog_finalize;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->close = xfce_titled_dialog_close;

  /**
   * XfceTitledDialog:subtitle:
   *
   * The subtitle displayed below the main dialog title.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SUBTITLE,
                                   g_param_spec_string ("subtitle",
                                                        "subtitle",
                                                        "subtitle",
                                                        NULL,
                                                        G_PARAM_READWRITE
                                                        | G_PARAM_STATIC_STRINGS));

  /* connect additional key bindings to the GtkDialog::close action signal */
  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_w, GDK_CONTROL_MASK, "close", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_W, GDK_CONTROL_MASK, "close", 0);
}



static GObject *
xfce_titled_dialog_constructor (GType                  type,
                                guint                  n_construct_params,
                                GObjectConstructParam *construct_params)
{
  GObject *object;

  object = G_OBJECT_CLASS (xfce_titled_dialog_parent_class)->constructor (type,
                                                           n_construct_params,
                                                           construct_params);

  return object;
}



static void
xfce_titled_dialog_init (XfceTitledDialog *titled_dialog)
{
  GtkSettings *settings;

  /* connect the private data */
  titled_dialog->priv = xfce_titled_dialog_get_instance_private (titled_dialog);

  settings = gtk_settings_get_default ();
  g_object_get (settings, "gtk-dialogs-use-header", &titled_dialog->priv->use_header, NULL);

  if (titled_dialog->priv->use_header)
    {
      g_object_set (G_OBJECT (titled_dialog), "use-header-bar", TRUE, NULL);

      /* Get the headerbar of the dialog */
      titled_dialog->priv->headerbar = gtk_dialog_get_header_bar (GTK_DIALOG (titled_dialog));
      g_return_if_fail (GTK_IS_HEADER_BAR (titled_dialog->priv->headerbar));

      /* Don't reserve vertical space for subtitles */
      gtk_header_bar_set_has_subtitle (GTK_HEADER_BAR (titled_dialog->priv->headerbar), FALSE);

      /* Pack the window icon into the headerbar */
      titled_dialog->priv->icon = gtk_image_new ();
      gtk_header_bar_pack_start (GTK_HEADER_BAR (titled_dialog->priv->headerbar), titled_dialog->priv->icon);
      gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (titled_dialog->priv->headerbar), TRUE);
      gtk_widget_show (titled_dialog->priv->icon);
      titled_dialog->priv->pixbuf = NULL;

      /* Adjust window buttons and window placement */
      g_signal_connect (G_OBJECT (titled_dialog), "notify::window", G_CALLBACK (xfce_titled_dialog_update_window), NULL);

      /* Make sure to update the icon whenever one of the relevant window properties changes */
      g_signal_connect (G_OBJECT (titled_dialog), "notify::icon", G_CALLBACK (xfce_titled_dialog_update_icon), NULL);
      g_signal_connect (G_OBJECT (titled_dialog), "notify::icon-name", G_CALLBACK (xfce_titled_dialog_update_icon), NULL);
    }
  else
    {
      GtkWidget *vbox, *widget, *content_area;

      /* remove the main dialog box from the window */
      content_area = gtk_dialog_get_content_area (GTK_DIALOG (titled_dialog));
      g_object_ref (G_OBJECT (content_area));
      gtk_container_remove (GTK_CONTAINER (titled_dialog), content_area);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (titled_dialog), vbox);
      gtk_widget_show (vbox);

      widget = titled_dialog->priv->subtitle_label = gtk_label_new (NULL);
      gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
      gtk_widget_set_no_show_all (widget, TRUE);
      gtk_style_context_add_class (gtk_widget_get_style_context (widget), "xfce-titled-dialog-subtitle");
      gtk_widget_set_margin_start (widget, 8);
      gtk_widget_set_margin_end (widget, 8);

      widget = titled_dialog->priv->subtitle_separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
      gtk_widget_set_no_show_all (widget, TRUE);
      gtk_style_context_add_class (gtk_widget_get_style_context (widget), "xfce-titled-dialog-separator");

      gtk_box_pack_start (GTK_BOX (vbox), content_area, TRUE, TRUE, 0);
      g_object_unref (G_OBJECT(content_area));
    }
}



static void
xfce_titled_dialog_finalize (GObject *object)
{
  XfceTitledDialog *titled_dialog = XFCE_TITLED_DIALOG (object);

  /* release the subtitle */
  g_free (titled_dialog->priv->subtitle);

  /* release the pixbuf */
  if (titled_dialog->priv->pixbuf)
    {
      g_object_unref (titled_dialog->priv->pixbuf);
      titled_dialog->priv->pixbuf = NULL;
    }

  (*G_OBJECT_CLASS (xfce_titled_dialog_parent_class)->finalize) (object);
}



static void
xfce_titled_dialog_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  XfceTitledDialog *titled_dialog = XFCE_TITLED_DIALOG (object);

  switch (prop_id)
    {
    case PROP_SUBTITLE:
      g_value_set_string (value, xfce_titled_dialog_get_subtitle (titled_dialog));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_titled_dialog_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  XfceTitledDialog *titled_dialog = XFCE_TITLED_DIALOG (object);

  switch (prop_id)
    {
    case PROP_SUBTITLE:
      xfce_titled_dialog_set_subtitle (titled_dialog, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_titled_dialog_close (GtkDialog *dialog)
{
  GdkEvent *event;

  /* verify that the dialog is realized */
  if (G_LIKELY (gtk_widget_get_realized (GTK_WIDGET (dialog))))
    {
      /* send a delete event to the dialog */
      event = gdk_event_new (GDK_DELETE);
      event->any.window = g_object_ref (gtk_widget_get_window(GTK_WIDGET (dialog)));
      event->any.send_event = TRUE;
      gtk_main_do_event (event);
      gdk_event_free (event);
    }
}



static void
xfce_titled_dialog_update_window (XfceTitledDialog *titled_dialog)
{
  /* skip if the dialog is a normal window by design (e.g. Settings Editor) */
  if (gtk_window_get_type_hint (GTK_WINDOW (titled_dialog)) == GDK_WINDOW_TYPE_HINT_NORMAL)
    return;

  /* set type-hint to normal to show min, max and close buttons */
  gtk_window_set_type_hint (GTK_WINDOW (titled_dialog), GDK_WINDOW_TYPE_HINT_NORMAL);

  /* center window on the active screen */
  xfce_gtk_window_center_on_active_screen (GTK_WINDOW (titled_dialog));
}



static void
xfce_titled_dialog_update_icon (XfceTitledDialog *titled_dialog)
{
  const gchar *icon_name;

  icon_name = gtk_window_get_icon_name (GTK_WINDOW (titled_dialog));

  if (icon_name)
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (titled_dialog->priv->icon), icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR);
      gtk_image_set_pixel_size (GTK_IMAGE (titled_dialog->priv->icon), 24);
    }
  else
    {
      if (titled_dialog->priv->pixbuf)
          g_object_unref (titled_dialog->priv->pixbuf);
      titled_dialog->priv->pixbuf = gtk_window_get_icon (GTK_WINDOW (titled_dialog));
      gtk_image_set_from_pixbuf (GTK_IMAGE (titled_dialog->priv->icon), titled_dialog->priv->pixbuf);
    }
}


/* Borrowed from gtkdialog.c */
static void
response_data_free (gpointer data)
{
  g_slice_free (ResponseData, data);
}



static ResponseData *
get_response_data (GtkWidget *widget,
                   gboolean   create)
{
  ResponseData *ad = g_object_get_data (G_OBJECT (widget),
                                        "gtk-dialog-response-data");

  if (ad == NULL && create)
    {
      ad = g_slice_new (ResponseData);

      g_object_set_data_full (G_OBJECT (widget),
                              I_("gtk-dialog-response-data"),
                              ad,
                              response_data_free);
    }

  return ad;
}



static void
action_widget_activated (GtkWidget *widget, GtkDialog *dialog)
{
  gint response_id;

  response_id = gtk_dialog_get_response_for_widget (dialog, widget);

  gtk_dialog_response (dialog, response_id);
}



static void
add_response_data (GtkDialog *dialog,
                   GtkWidget *child,
                   gint       response_id)
{
  ResponseData *ad;
  guint signal_id;

  ad = get_response_data (child, TRUE);
  ad->response_id = response_id;

  if (GTK_IS_BUTTON (child))
    signal_id = g_signal_lookup ("clicked", GTK_TYPE_BUTTON);
  else
    signal_id = GTK_WIDGET_GET_CLASS (child)->activate_signal;

  if (signal_id)
    {
      GClosure *closure;

      closure = g_cclosure_new_object (G_CALLBACK (action_widget_activated),
                                       G_OBJECT (dialog));
      g_signal_connect_closure_by_id (child, signal_id, 0, closure, FALSE);
    }
  else
    g_warning ("Only 'activatable' widgets can be packed into the action area of a GtkDialog");
}
/* End: Borrowed from gtkdialog.c */



/* Repack all the buttons that would normally end up in the headerbar to the action area */
static void
xfce_titled_dialog_repack_dialog (GtkWidget *action_area,
                                  GtkWidget *headerbar,
                                  GtkWidget *button,
                                  gint       response_id)
{
  g_return_if_fail (GTK_IS_WIDGET (action_area));
  g_return_if_fail (GTK_IS_WIDGET (headerbar));
  g_return_if_fail (GTK_IS_WIDGET (button));

  g_object_ref (G_OBJECT (button));
  gtk_container_remove (GTK_CONTAINER (headerbar), button);
  gtk_container_add (GTK_CONTAINER (action_area), button);
  g_object_unref (G_OBJECT (button));
  /* always add help buttons as secondary */
  if (response_id == GTK_RESPONSE_HELP)
    gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (action_area), button, TRUE);
}



/**
 * xfce_titled_dialog_new:
 *
 * Allocates a new #XfceTitledDialog instance.
 *
 * Return value: the newly allocated #XfceTitledDialog.
 **/
GtkWidget*
xfce_titled_dialog_new (void)
{
  return g_object_new (XFCE_TYPE_TITLED_DIALOG, NULL);
}



/**
 * xfce_titled_dialog_new_with_buttons:
 * @title             : (nullable): title of the dialog, or %NULL.
 * @parent            : (nullable): transient parent window of the dialog, or %NULL.
 * @flags             : from #GtkDialogFlags.
 * @first_button_text : (nullable): stock ID or text to go in first, or %NULL.
 * @...               : response ID for the first button, then additional buttons, ending with %NULL.
 *
 * See the documentation of gtk_dialog_new_with_buttons() for details about the
 * parameters and the returned dialog.
 *
 * Return value: the newly allocated #XfceTitledDialog.
 *
 * Deprecated: 4.16: Use #xfce_titled_dialog_new_with_mixed_buttons instead.
 **/
GtkWidget*
xfce_titled_dialog_new_with_buttons (const gchar    *title,
                                     GtkWindow      *parent,
                                     GtkDialogFlags  flags,
                                     const gchar    *first_button_text,
                                     ...)
{
  const gchar *button_text;
  GtkWidget   *dialog;
  GtkWidget   *button;
  GtkWidget   *headerbar;
  GtkWidget   *action_area;
  va_list      args;
  gint         response_id;

  /* allocate the dialog */
  dialog = g_object_new (XFCE_TYPE_TITLED_DIALOG,
                         "destroy-with-parent", ((flags & GTK_DIALOG_DESTROY_WITH_PARENT) != 0),
                         "modal", ((flags & GTK_DIALOG_MODAL) != 0),
                         "title", title,
                         NULL);

  /* set the transient parent (if any) */
  if (G_LIKELY (parent != NULL))
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  action_area = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
G_GNUC_END_IGNORE_DEPRECATIONS
  headerbar = gtk_dialog_get_header_bar (GTK_DIALOG (dialog));

  /* add all additional buttons */
  va_start (args, first_button_text);
  for (button_text = first_button_text; button_text != NULL; )
    {
      response_id = va_arg (args, gint);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      button = gtk_button_new_from_stock (button_text);
G_GNUC_END_IGNORE_DEPRECATIONS
      gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, response_id);
      if (XFCE_TITLED_DIALOG (dialog)->priv->use_header)
        xfce_titled_dialog_repack_dialog (action_area, headerbar, button, response_id);
      button_text = va_arg (args, const gchar *);
    }
  va_end (args);

  return dialog;
}



/**
 * xfce_titled_dialog_new_with_mixed_buttons:
 * @title                  : (nullable): title of the dialog, or %NULL.
 * @parent                 : (nullable): transient parent window of the dialog, or %NULL.
 * @flags                  : from #GtkDialogFlags.
 * @first_button_icon_name : icon name to go in first, or "" for no icon.
 * @first_button_text      : (nullable): text to go in first, or %NULL.
 * @...                    : response ID for the first button, then additional buttons, ending with %NULL.
 *
 * Creates an #XfceTitledDialog using xfce_gtk_button_new_mixed. This allows
 * the buttons to use an optional named or stock icon.
 *
 * Return value: the newly allocated #XfceTitledDialog.
 *
 * Since: 4.14
 *
 **/
GtkWidget*
xfce_titled_dialog_new_with_mixed_buttons (const gchar    *title,
                                           GtkWindow      *parent,
                                           GtkDialogFlags  flags,
                                           const gchar    *first_button_icon_name,
                                           const gchar    *first_button_text,
                                           ...)
{
  const gchar *icon_name;
  const gchar *button_text;
  GtkWidget   *dialog;
  GtkWidget   *headerbar;
  GtkWidget   *action_area;
  va_list      args;
  gint         response_id;

  /* allocate the dialog */
  dialog = g_object_new (XFCE_TYPE_TITLED_DIALOG,
                         "destroy-with-parent", ((flags & GTK_DIALOG_DESTROY_WITH_PARENT) != 0),
                         "modal", ((flags & GTK_DIALOG_MODAL) != 0),
                         "title", title,
                         NULL);

  /* set the transient parent (if any) */
  if (G_LIKELY (parent != NULL))
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  action_area = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
G_GNUC_END_IGNORE_DEPRECATIONS
  headerbar = gtk_dialog_get_header_bar (GTK_DIALOG (dialog));

  /* add all additional buttons */
  icon_name = first_button_icon_name;
  button_text = first_button_text;
  va_start (args, first_button_text);

  while (icon_name != NULL)
    {
      GtkWidget *button;

      /* response id comes after button text */
      response_id = va_arg (args, gint);

      /* build our button and add it */
      button = xfce_gtk_button_new_mixed (icon_name, button_text);
      gtk_widget_set_can_default (button, TRUE);

      gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, response_id);
      if (XFCE_TITLED_DIALOG (dialog)->priv->use_header)
        xfce_titled_dialog_repack_dialog (action_area, headerbar, button, response_id);
      gtk_widget_show (button);

      /* this is to pickup for the next button.
       * The pattern is icon_name, button text
       */
      icon_name = va_arg (args, const gchar *);
      if (icon_name)
        {
          button_text = va_arg (args, const gchar *);
        }
    }
  va_end (args);

  /* since we just removed all buttons from the headerbar we have to show the close button again explicitly */
  if (XFCE_TITLED_DIALOG (dialog)->priv->use_header)
    gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (headerbar), TRUE);

  return dialog;
}



/**
 * xfce_titled_dialog_create_action_area:
 * @titled_dialog : a #XfceTitledDialog.
 *
 * This function creates a custom action area (of type #GtkButtonBox) and has to
 * be used in combination with #xfce_titled_dialog_add_action_widget.
 *
 * When using the XfceTitledDialogClass directly to create dialogs this function is
 * useful to keep action widgets out of the #GtkHeaderBar in which they would
 * normally end up by calling #gtk_dialog_add_action_widget.
 *
 * Since: 4.16
 *
 **/
void
xfce_titled_dialog_create_action_area (XfceTitledDialog *titled_dialog)
{
  GtkWidget *dialog_vbox;

  g_return_if_fail (XFCE_IS_TITLED_DIALOG (titled_dialog));

  /* Create new buttonbox to act as custom action area for the dialog */
  titled_dialog->priv->action_area = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (titled_dialog->priv->action_area), GTK_BUTTONBOX_END);
  gtk_box_set_spacing (GTK_BOX (titled_dialog->priv->action_area), 6);

  /* Pack the buttonbox in the dialog's main vbox */
  dialog_vbox = gtk_bin_get_child (GTK_BIN (titled_dialog));
  gtk_box_pack_end (GTK_BOX (dialog_vbox), titled_dialog->priv->action_area, FALSE, TRUE, 0);
  gtk_box_reorder_child (GTK_BOX (dialog_vbox), titled_dialog->priv->action_area, 0);
  gtk_widget_show (titled_dialog->priv->action_area);
}



/**
 * xfce_titled_dialog_add_button:
 * @titled_dialog : a #XfceTitledDialog.
 * @button_text   : text of button.
 * @response_id   : response ID for @child.
 *
 * This function is a replacement for #gtk_dialog_add_button and assumes that
 * you have called #xfce_titled_dialog_create_action_area before.
 *
 * Buttons with #GTK_RESPONSE_HELP will be added to the secondary group of children
 * (see #gtk_button_box_set_child_secondary for reference).
 *
 * Return value: (transfer none): the GtkButton widget that was added.
 *
 * Since: 4.16
 *
 **/
GtkWidget *
xfce_titled_dialog_add_button (XfceTitledDialog *titled_dialog,
                               const gchar      *button_text,
                               gint              response_id)
{
  GtkWidget *button;

  g_return_val_if_fail (XFCE_IS_TITLED_DIALOG (titled_dialog), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (titled_dialog->priv->action_area), NULL);
  g_return_val_if_fail (button_text != NULL, NULL);

  button = gtk_button_new_with_label (button_text);
  gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);

  xfce_titled_dialog_add_action_widget (titled_dialog, button, response_id);

  return button;
}



/**
 * xfce_titled_dialog_add_action_widget:
 * @titled_dialog : a #XfceTitledDialog.
 * @child         : an activatable widget.
 * @response_id   : response ID for @child.
 *
 * This function is a replacement for #gtk_dialog_add_action_widget and assumes that
 * you have called #xfce_titled_dialog_create_action_area before.
 *
 * Children with #GTK_RESPONSE_HELP will be added to the secondary group of children
 * (see #gtk_button_box_set_child_secondary for reference).
 *
 * Since: 4.16
 *
 **/
void
xfce_titled_dialog_add_action_widget (XfceTitledDialog *titled_dialog,
                                      GtkWidget        *child,
                                      gint              response_id)
{
  g_return_if_fail (XFCE_IS_TITLED_DIALOG (titled_dialog));
  g_return_if_fail (GTK_IS_WIDGET (titled_dialog->priv->action_area));
  g_return_if_fail (GTK_IS_WIDGET (child));

  add_response_data (GTK_DIALOG (titled_dialog), child, response_id);

  gtk_box_pack_start (GTK_BOX (titled_dialog->priv->action_area), child, FALSE, TRUE, 0);
  gtk_widget_show (child);

  if (response_id == GTK_RESPONSE_HELP)
    gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (titled_dialog->priv->action_area), child, TRUE);
}



/**
 * xfce_titled_dialog_set_default_response:
 * @titled_dialog : a #XfceTitledDialog.
 * @response_id   : a response ID
 *
 * Sets the last widget in the dialog’s action area with the given @response_id
 * as the default widget for the dialog. Pressing “Enter” normally activates
 * the default widget.
 *
 * This function is a replacement for #gtk_dialog_set_default_response, which does
 * not work with #XfceTitledDialog.
 *
 * Since: 4.16
 *
 **/
void
xfce_titled_dialog_set_default_response (XfceTitledDialog *titled_dialog,
                                         gint              response_id)
{
  GtkWidget *action_area;
  GList     *children;
  GList     *tmp_list;

  g_return_if_fail (XFCE_IS_TITLED_DIALOG (titled_dialog));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  action_area = gtk_dialog_get_action_area (GTK_DIALOG (titled_dialog));
G_GNUC_END_IGNORE_DEPRECATIONS

  children = gtk_container_get_children (GTK_CONTAINER (action_area));
  tmp_list = children;
  while (tmp_list != NULL)
    {
      GtkWidget *widget = tmp_list->data;
      ResponseData *rd = get_response_data (widget, FALSE);

      if (rd && rd->response_id == response_id)
        gtk_window_set_default (GTK_WINDOW (titled_dialog), widget);

      tmp_list = tmp_list->next;
    }

  g_list_free (children);
}



/**
 * xfce_titled_dialog_get_subtitle:
 * @titled_dialog : a #XfceTitledDialog.
 *
 * Returns the subtitle of the @titled_dialog, or %NULL
 * if no subtitle is displayed in the @titled_dialog.
 * This is just a convenience function around #gtk_header_bar_get_subtitle.
 *
 * Return value: the subtitle of @titled_dialog, or %NULL.
 **/
const gchar*
xfce_titled_dialog_get_subtitle (XfceTitledDialog *titled_dialog)
{
  g_return_val_if_fail (XFCE_IS_TITLED_DIALOG (titled_dialog), NULL);
  return titled_dialog->priv->subtitle;
}



/**
 * xfce_titled_dialog_set_subtitle:
 * @titled_dialog : a #XfceTitledDialog.
 * @subtitle: (nullable): the new subtitle for the @titled_dialog, or %NULL.
 *
 * Sets the subtitle displayed by @titled_dialog to @subtitle; if
 * @subtitle is %NULL no subtitle will be displayed by the @titled_dialog.
 * This is just a convenience function around #gtk_header_bar_set_subtitle
 * when dialogs use header bars. Otherwise a simple label and separator are
 * shown at the top of dialog.
 **/
void
xfce_titled_dialog_set_subtitle (XfceTitledDialog *titled_dialog,
                                 const gchar      *subtitle)
{
  g_return_if_fail (XFCE_IS_TITLED_DIALOG (titled_dialog));
  g_return_if_fail (subtitle == NULL || g_utf8_validate (subtitle, -1, NULL));

  /* release the previous subtitle */
  g_free (titled_dialog->priv->subtitle);

  /* activate the new subtitle */
  titled_dialog->priv->subtitle = g_strdup (subtitle);

  if (titled_dialog->priv->use_header)
    {
      /* update the subtitle of the headerbar */
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (titled_dialog->priv->headerbar),
                                   titled_dialog->priv->subtitle);
    }
  else
    {
      gtk_label_set_label (GTK_LABEL (titled_dialog->priv->subtitle_label), subtitle);
      gtk_widget_show (titled_dialog->priv->subtitle_label);
      gtk_widget_show (titled_dialog->priv->subtitle_separator);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (titled_dialog), "subtitle");
}



#define __XFCE_TITLED_DIALOG_C__
#include <libxfce4ui/libxfce4ui-aliasdef.c>
