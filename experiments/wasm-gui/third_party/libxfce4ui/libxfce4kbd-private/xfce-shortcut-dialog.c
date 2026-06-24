/* vi:set expandtab sw=2 sts=2: */
/*
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <libxfce4kbd-private/xfce-shortcuts.h>
#include <libxfce4kbd-private/xfce-shortcut-dialog.h>



static void     xfce_shortcut_dialog_finalize         (GObject                 *object);
static void     xfce_shortcut_dialog_create_contents  (XfceShortcutDialog      *dialog,
                                                       const gchar             *provider,
                                                       const gchar             *action_name,
                                                       const gchar             *action);
static gboolean xfce_shortcut_dialog_key_pressed      (XfceShortcutDialog      *dialog,
                                                       GdkEventKey             *event);
static gboolean xfce_shortcut_dialog_key_released     (XfceShortcutDialog      *dialog,
                                                       GdkEventKey             *event);
static void     xfce_shortcut_dialog_prepare_grab     (GdkSeat                 *seat,
                                                       GdkWindow               *window,
                                                       gpointer                 user_data);


struct _XfceShortcutDialogClass
{
  XfceTitledDialogClass __parent__;

  gboolean (*validate_shortcut) (XfceShortcutDialog *dialog,
                                 const gchar        *shortcut,
                                 gpointer            user_data);

  gint validate_shortcut_signal;
};

struct _XfceShortcutDialog
{
  XfceTitledDialog __parent__;

  GtkWidget *shortcut_label;
  GtkWidget *key_box;

  gchar     *action_name;
  gchar     *action;
  gchar     *shortcut;
};



G_DEFINE_TYPE (XfceShortcutDialog, xfce_shortcut_dialog, XFCE_TYPE_TITLED_DIALOG)



/**
 * Taken from GTK+ (_gtk_marshal_BOOLEAN__SRING). Credits go out to the
 * GTK+ devs for this.
 */
static void
marshal_BOOLEAN__STRING (GClosure     *closure,
                         GValue       *return_value G_GNUC_UNUSED,
                         guint         n_param_values,
                         const GValue *param_values,
                         gpointer      invocation_hint G_GNUC_UNUSED,
                         gpointer      marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__STRING) (gpointer data1,
                                                    gpointer arg_1,
                                                    gpointer data2);
  register GMarshalFunc_BOOLEAN__STRING callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  gboolean v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }

  callback = (GMarshalFunc_BOOLEAN__STRING) (marshal_data ? marshal_data : cc->callback);

  #define g_marshal_value_peek_string(v) (char*) g_value_get_string (v)
  v_return = callback (data1, g_marshal_value_peek_string (param_values + 1), data2);

  g_value_set_boolean (return_value, v_return);
}




static void
xfce_shortcut_dialog_class_init (XfceShortcutDialogClass *klass)
{
  GObjectClass *gobject_class;

  /* Make sure to use the translations from libxfce4ui */
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_shortcut_dialog_finalize;

  klass->validate_shortcut = NULL;

  /* Create 'validate-shortcut' signal */
  klass->validate_shortcut_signal = g_signal_new ("validate-shortcut",
                                                  G_TYPE_FROM_CLASS (klass),
                                                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                                  G_STRUCT_OFFSET (XfceShortcutDialogClass, validate_shortcut),
                                                  NULL,
                                                  NULL,
                                                  marshal_BOOLEAN__STRING,
                                                  G_TYPE_BOOLEAN,
                                                  1,
                                                  G_TYPE_STRING);
}



static void
xfce_shortcut_dialog_init (XfceShortcutDialog *dialog)
{
  dialog->shortcut = NULL;
}



static void
xfce_shortcut_dialog_finalize (GObject *object)
{
  XfceShortcutDialog *dialog = XFCE_SHORTCUT_DIALOG (object);

  g_free (dialog->action_name);
  g_free (dialog->action);
  g_free (dialog->shortcut);

  (*G_OBJECT_CLASS (xfce_shortcut_dialog_parent_class)->finalize) (object);
}



GtkWidget*
xfce_shortcut_dialog_new (const gchar *provider,
                          const gchar *action_name,
                          const gchar *action)
{
  XfceShortcutDialog *dialog;

  dialog = g_object_new (XFCE_TYPE_SHORTCUT_DIALOG, NULL);
  dialog->action_name = g_strdup (action_name);
  dialog->action = g_strdup (action);

  xfce_shortcut_dialog_create_contents (dialog, provider, action_name, action);

  return GTK_WIDGET (dialog);
}



static void
xfce_shortcut_dialog_create_contents (XfceShortcutDialog *dialog,
                                      const gchar        *provider,
                                      const gchar        *action_name,
                                      const gchar        *action)
{
  GtkWidget   *content_box;
  GtkWidget   *box;
  GtkWidget   *button;
  GtkWidget   *label;
  const gchar *action_type;
  const gchar *title;
  const gchar *icon_name;
  const gchar *text;
  const gchar *format;
  gchar       *explanation_label;
  gchar       *markup;

  if (g_utf8_collate (provider, "xfwm4") == 0)
    {
      title = _("Window Manager Action Shortcut");
      /* TRANSLATORS: this string will be used to create an explanation for
       * the user in a following string */
      action_type = _("action");
      icon_name = "org.xfce.xfwm4";
    }
  else if (g_utf8_collate (provider, "commands") == 0)
    {
      title = _("Command Shortcut");
      /* TRANSLATORS: this string will be used to create an explanation for
       * the user in a following string */
      action_type = _("command");
      icon_name = "org.xfce.settings.keyboard";
    }
  else
    {
      title = _("Shortcut");
      /* TRANSLATORS: this string will be used to create an explanation for
       * the user in a following string */
      action_type = _("action");
      icon_name = "input-keyboard";
    }

  /* Set dialog title */
  gtk_window_set_title (GTK_WINDOW (dialog), title);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

  xfce_titled_dialog_create_action_area (XFCE_TITLED_DIALOG (dialog));

  /* Create clear button for xfwm4 */
  if (g_utf8_collate (provider, "xfwm4") == 0)
    {
      button = gtk_button_new_from_icon_name ("edit-clear-symbolic", GTK_ICON_SIZE_BUTTON);
      gtk_button_set_label (GTK_BUTTON (button), _("Clear"));
      xfce_titled_dialog_add_action_widget (XFCE_TITLED_DIALOG (dialog), button, GTK_RESPONSE_REJECT);
      gtk_widget_show (button);
    }

  /* Create cancel button */
  button = gtk_button_new_with_mnemonic (_("_Cancel"));
  xfce_titled_dialog_add_action_widget (XFCE_TITLED_DIALOG (dialog), button, GTK_RESPONSE_CANCEL);
  gtk_widget_show (button);

  content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
  gtk_widget_set_valign (content_box, GTK_ALIGN_CENTER);
  gtk_widget_set_vexpand (content_box, TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (content_box), 6);
  gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                     content_box);
  gtk_widget_show (content_box);

  /* TRANSLATORS: this creates the explanation for the user. The first %s is replaced
   * by the action type which you translated earlier, the second %s is replaced by the
   * action name which comes from somewhere else.
   * THE ORDER MUSTN'T BE REVERSED! */
  explanation_label =
    g_strdup_printf (_("Press keyboard keys to trigger the %s '%s'."),
                     action_type, action_name);

  label = gtk_label_new (explanation_label);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_container_add (GTK_CONTAINER (content_box), label);
  gtk_widget_show (label);
  g_free (explanation_label);

  /* Box and labels to display the shortcut currently being grabbed.
   * It will be updated to key-press events. */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_add (GTK_CONTAINER (content_box), box);
  gtk_widget_show (box);

  dialog->key_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_halign (dialog->key_box, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (dialog->key_box, TRUE);
  gtk_container_add (GTK_CONTAINER (box), dialog->key_box);

  dialog->shortcut_label = gtk_label_new (NULL);
  text = _("Please press a key");
  format = "<span size='x-large'><b>%s</b></span>";
  markup = g_markup_printf_escaped (format, text);
  gtk_label_set_markup (GTK_LABEL (dialog->shortcut_label), markup);
  gtk_label_set_xalign (GTK_LABEL (dialog->shortcut_label), 0.5);
  gtk_label_set_yalign (GTK_LABEL (dialog->shortcut_label), 0.5);
  gtk_widget_set_hexpand (dialog->shortcut_label, TRUE);
  gtk_container_add (GTK_CONTAINER (box), dialog->shortcut_label);
  gtk_widget_show (dialog->shortcut_label);
  g_free (markup);

  /* Connect to key release signal for determining the new shortcut */
  g_signal_connect_swapped (dialog, "key-press-event", G_CALLBACK (xfce_shortcut_dialog_key_pressed), dialog);
  g_signal_connect_swapped (dialog, "key-release-event", G_CALLBACK (xfce_shortcut_dialog_key_released), dialog);
}


gint
xfce_shortcut_dialog_run (XfceShortcutDialog *dialog,
                          GtkWidget          *parent)
{
  GdkDisplay       *display;
  GdkSeat          *seat;
  gint              response = GTK_RESPONSE_CANCEL;

  g_return_val_if_fail (XFCE_IS_SHORTCUT_DIALOG (dialog), GTK_RESPONSE_CANCEL);

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  display = gtk_widget_get_display (GTK_WIDGET (dialog));
  seat = gdk_display_get_default_seat (display);

  /* Take control on the keyboard */
  if (gdk_seat_grab (seat,
                 parent != NULL ? gtk_widget_get_window (parent)
                                : gdk_screen_get_root_window (gdk_display_get_default_screen (display)),
                 GDK_SEAT_CAPABILITY_KEYBOARD, TRUE, NULL, NULL,
                 xfce_shortcut_dialog_prepare_grab, NULL) == GDK_GRAB_SUCCESS)
    {
      /* Run the dialog and wait for the user to enter a valid shortcut */
      response = gtk_dialog_run (GTK_DIALOG (dialog));

      /* Clear shortcut if requested by the user */
      if (G_UNLIKELY (response == GTK_RESPONSE_NO))
        {
          g_free (dialog->shortcut);
          dialog->shortcut = g_strdup ("");
        }

      /* Release keyboard */
      gdk_seat_ungrab (seat);
    }
  else
    {
      g_warning (_("Could not grab the keyboard."));
    }

  /* Return the response ID */
  return response;
}



static gboolean
xfce_shortcut_dialog_key_pressed (XfceShortcutDialog *dialog,
                                  GdkEventKey        *event)
{
  GdkDisplay      *display;
  GdkKeymap       *keymap;
  GdkModifierType  consumed, modifiers;
  guint            keyval, mod_mask;
  gchar           *text;
  gchar           *escaped_label;
  gchar           *label;
  gchar          **keys;
  GtkWidget       *key_label;
  GtkStyleContext *context;
  guint            i;

  /* Clean up */
  g_free (dialog->shortcut);
  gtk_container_foreach (GTK_CONTAINER (dialog->key_box),
      (GtkCallback) (void (*)(void)) gtk_widget_destroy, NULL);
  gtk_widget_hide (dialog->shortcut_label);
  gtk_widget_show (dialog->key_box);

  /* Get the keyboard state */
  mod_mask = gtk_accelerator_get_default_mod_mask ();
  display = gdk_display_get_default ();
  keymap = gdk_keymap_get_for_display (display);
  modifiers = event->state;

  gdk_keymap_translate_keyboard_state (keymap, event->hardware_keycode,
                                       modifiers, event->group,
                                       &keyval, NULL, NULL, &consumed);

  /* We want Alt + Print to be Alt + Print not SysReq. See bug #7897 */
  if (keyval == GDK_KEY_Sys_Req && (modifiers & GDK_MOD1_MASK) != 0)
    {
      consumed = 0;
      keyval = GDK_KEY_Print;
    }

  /* Get the modifiers */

  /* If Shift was used when translating the keyboard state, we remove it
   * from the consumed bit because gtk_accelerator_{name,parse} fail to
   * handle this correctly. This allows us to have shortcuts with Shift
   * as a modifier key (see bug #8744). */
  if ((modifiers & GDK_SHIFT_MASK) && (consumed & GDK_SHIFT_MASK))
    consumed &= ~GDK_SHIFT_MASK;

  modifiers &= ~consumed;
  modifiers &= mod_mask;

  /* Get and store the pressed shortcut */
  dialog->shortcut = gtk_accelerator_name (keyval, modifiers);

  label = gtk_accelerator_get_label (keyval, modifiers);
  escaped_label = g_markup_escape_text (label, -1);
  keys = g_strsplit (escaped_label, "+", -1);

  /* Show each key as individual label with the .keycap style class */
  for (i = 0; i < g_strv_length (keys); i++)
  {
    text = g_strdup_printf ("<span size='large'>%s</span>",
                            keys[i]);
    key_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (key_label), text);
    context = gtk_widget_get_style_context (key_label);
    gtk_style_context_add_class (context, "keycap");
    gtk_widget_show (key_label);
    gtk_container_add (GTK_CONTAINER (dialog->key_box), key_label);
    g_free (text);
  }

  g_free (label);
  g_free (escaped_label);
  g_strfreev (keys);

  return TRUE;
}



static gboolean
xfce_shortcut_dialog_key_released (XfceShortcutDialog *dialog,
                                   GdkEventKey        *event)
{
  gboolean    shortcut_accepted = FALSE;
  GdkWindow  *window = event->window;
  GdkDisplay *display;
  GdkSeat    *seat;

  display = gdk_window_get_display (window);
  seat = gdk_display_get_default_seat (display);

  /* Let 'validate-shortcut' listeners decide whether this shortcut is ok or not */
  g_signal_emit_by_name (dialog, "validate-shortcut", dialog->shortcut, &shortcut_accepted);

  /* Check if the shortcut was accepted */
  if (G_LIKELY (shortcut_accepted))
    {
      /* Release keyboard */
      if (gdk_seat_grab (seat, window,
                         GDK_SEAT_CAPABILITY_KEYBOARD,
                         TRUE, NULL, NULL, NULL, NULL))
        {
          gdk_seat_ungrab (seat);
        }

      /* Exit dialog with positive response */
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
    }
  else
    {
      /* Hide keys, show info label */
      gtk_widget_show (dialog->shortcut_label);
      gtk_widget_hide (dialog->key_box);
    }

  return TRUE;
}



const gchar*
xfce_shortcut_dialog_get_shortcut (XfceShortcutDialog *dialog)
{
  g_return_val_if_fail (XFCE_IS_SHORTCUT_DIALOG (dialog), NULL);
  return dialog->shortcut;
}



const gchar *
xfce_shortcut_dialog_get_action (XfceShortcutDialog *dialog)
{
  g_return_val_if_fail (XFCE_IS_SHORTCUT_DIALOG (dialog), NULL);
  return dialog->action;
}



const gchar *
xfce_shortcut_dialog_get_action_name (XfceShortcutDialog *dialog)
{
  g_return_val_if_fail (XFCE_IS_SHORTCUT_DIALOG (dialog), NULL);
  return dialog->action_name;
}



static void
xfce_shortcut_dialog_prepare_grab (GdkSeat   *seat,
                                   GdkWindow *window,
                                   gpointer   user_data)
{
  gdk_window_show_unraised (window);
}
