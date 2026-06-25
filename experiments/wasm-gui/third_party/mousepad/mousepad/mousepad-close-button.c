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
#include <mousepad/mousepad-close-button.h>



#define ICON_NAME_MODIFIED "media-record-symbolic"
#define ICON_NAME_UNMODIFIED "window-close"

#define mousepad_close_button_set_icon_name(button, icon_name) \
  gtk_image_set_from_icon_name (GTK_IMAGE (gtk_button_get_image (GTK_BUTTON (button))), \
                                icon_name, GTK_ICON_SIZE_MENU);

/* GObject virtual functions */
static void     mousepad_close_button_finalize           (GObject          *object);

/* GtkWidget virtual functions */
static gboolean mousepad_close_button_enter_notify_event (GtkWidget        *widget,
                                                          GdkEventCrossing *event);
static gboolean mousepad_close_button_leave_notify_event (GtkWidget        *widget,
                                                          GdkEventCrossing *event);



struct _MousepadCloseButton
{
  GtkButton parent;

  GtkTextBuffer *buffer;
};



G_DEFINE_TYPE (MousepadCloseButton, mousepad_close_button, GTK_TYPE_BUTTON)



static void
mousepad_close_button_class_init (MousepadCloseButtonClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = mousepad_close_button_finalize;

  widget_class->enter_notify_event = mousepad_close_button_enter_notify_event;
  widget_class->leave_notify_event = mousepad_close_button_leave_notify_event;
}



static void
mousepad_close_button_init (MousepadCloseButton *button)
{
  GtkWidget *image;
  GtkCssProvider  *css_provider;
  GtkStyleContext *context;

  static const gchar *button_style =
    "* {\n"
    "  outline-width: 0;\n"
    "  outline-offset: 0;\n"
    "  padding: 0;\n"
    "}\n";

  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css_provider, button_style, -1, NULL);

  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css_provider);

  image = gtk_image_new_from_icon_name (NULL, GTK_ICON_SIZE_MENU);
  g_object_set (button, "relief", GTK_RELIEF_NONE, "focus-on-click", FALSE,
                "always-show-image", TRUE, "image", image, NULL);
}



static void
mousepad_close_button_finalize (GObject *object)
{
  MousepadCloseButton *button = MOUSEPAD_CLOSE_BUTTON (object);

  g_object_unref (button->buffer);

  G_OBJECT_CLASS (mousepad_close_button_parent_class)->finalize (object);
}



static gboolean
mousepad_close_button_enter_notify_event (GtkWidget        *widget,
                                          GdkEventCrossing *event)
{
  MousepadCloseButton *button = MOUSEPAD_CLOSE_BUTTON (widget);

  if (gtk_text_buffer_get_modified (button->buffer))
    mousepad_close_button_set_icon_name (button, ICON_NAME_UNMODIFIED);

  return GTK_WIDGET_CLASS (mousepad_close_button_parent_class)->enter_notify_event (widget, event);
}



static gboolean
mousepad_close_button_leave_notify_event (GtkWidget        *widget,
                                          GdkEventCrossing *event)
{
  MousepadCloseButton *button = MOUSEPAD_CLOSE_BUTTON (widget);

  if (gtk_text_buffer_get_modified (button->buffer))
    mousepad_close_button_set_icon_name (button, ICON_NAME_MODIFIED);

  return GTK_WIDGET_CLASS (mousepad_close_button_parent_class)->leave_notify_event (widget, event);
}



static void
mousepad_close_button_modified_changed (GtkTextBuffer       *buffer,
                                        MousepadCloseButton *button)
{
  const gchar *icon_name;

  icon_name = gtk_text_buffer_get_modified (buffer) ? ICON_NAME_MODIFIED : ICON_NAME_UNMODIFIED;
  mousepad_close_button_set_icon_name (button, icon_name);
}



GtkWidget *
mousepad_close_button_new (GtkTextBuffer *buffer)
{
  MousepadCloseButton *button;

  button = g_object_new (MOUSEPAD_TYPE_CLOSE_BUTTON, NULL);
  button->buffer = g_object_ref (buffer);
  mousepad_close_button_modified_changed (buffer, button);
  g_signal_connect_object (buffer, "modified-changed",
                           G_CALLBACK (mousepad_close_button_modified_changed), button, 0);

  return GTK_WIDGET (button);
}
