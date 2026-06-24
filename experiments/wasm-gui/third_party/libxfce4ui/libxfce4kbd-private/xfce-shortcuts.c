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

#include <glib.h>

#include <X11/Xlib.h>

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <xfconf/xfconf.h>

#include <libxfce4kbd-private/xfce-shortcuts.h>
#include <libxfce4kbd-private/xfce-shortcuts-xfwm4.h>



typedef struct
{
  const gchar *owner_name;
  const gchar *other_name;
  const gchar *message;
  const gchar *owner_button_text;
  const gchar *other_button_text;
} XfceShortcutConflictMessage;



static XfceShortcutConflictMessage conflict_messages[] = {
  { "xfwm4", "xfwm4",
    N_("This shortcut is already being used for the action '%s'. Which action do you want to use?"),
    N_("Use '%s'"), N_("Keep '%s'") },
  { "xfwm4", "commands",
    N_("This shortcut is already being used for the command '%s'. Which action do you want to use?"),
    N_("Use '%s'"), N_("Keep '%s'") },
  { "commands","commands",
    N_("This shortcut is already being used for the command '%s'. Which action do you want to use?"),
    N_("Use '%s'"), N_("Keep '%s'") },
  { "commands", "xfwm4",
    N_("This shortcut is already being used by the action '%s'. Which action do you want to use?"),
    N_("Use '%s'"), N_("Keep '%s'") },
  { 0, 0, NULL, NULL, NULL },
};



gint
xfce_shortcut_conflict_dialog (GtkWindow   *parent,
                               const gchar *owner,
                               const gchar *other,
                               const gchar *shortcut,
                               const gchar *owner_action,
                               const gchar *other_action,
                               gboolean     ignore_same_provider)
{
  GdkModifierType  modifiers;
  gboolean         handled;
  gchar           *other_action_name;
  gchar           *other_button_text;
  gchar           *owner_action_name;
  gchar           *owner_button_text;
  gchar           *secondary_text;
  gchar           *shortcut_label;
  gchar           *title;
  guint            keyval;
  gint             response;
  gint             i;

  /* Default values */
  response = GTK_RESPONSE_ACCEPT;
  handled = FALSE;

  /* Make sure to use the translations from libxfce4ui */
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  if (g_utf8_collate (owner, other) == 0 && ignore_same_provider)
    return GTK_RESPONSE_ACCEPT;

  /* Get the shortcut label */
  gtk_accelerator_parse (shortcut, &keyval, &modifiers);
  shortcut_label = gtk_accelerator_get_label (keyval, modifiers);

  if (g_utf8_collate (owner, other) == 0 && g_utf8_collate (owner_action, other_action) == 0)
    {
      /* This shortcut already exists in the provider, we don't want it twice */

      /* Warn the user */
      xfce_dialog_show_warning (parent, _("Please use another key combination."),
                                _("%s already triggers this action."), shortcut_label);

      return GTK_RESPONSE_REJECT;
    }

  title = g_strdup_printf (_("Conflicting actions for %s"), shortcut_label);

  for (i = 0; conflict_messages[i].message != NULL; ++i)
    if (g_utf8_collate (conflict_messages[i].owner_name, owner) == 0 &&
        g_utf8_collate (conflict_messages[i].other_name, other) == 0)
      {
        if (owner_action == NULL)
          owner_action_name = NULL;
        else if (g_utf8_collate (owner, "xfwm4") == 0)
          {
            DBG ("Owner action %s is an xfwm4 action, get the string", owner_action);

            /* We need to get the human readable string of the action name */
            owner_action_name =
              g_strdup (xfce_shortcuts_xfwm4_get_feature_name (owner_action));

          }
        else
          owner_action_name = g_strdup(owner_action);

        DBG ("Owner action name: %s", owner_action_name);

        if (other_action == NULL)
          other_action_name = NULL;
        else if (g_utf8_collate (other, "xfwm4") == 0)
          {
            /* We need to get the human readable string of the action name */

            other_action_name =
              g_strdup (xfce_shortcuts_xfwm4_get_feature_name (other_action));

          }
        else
          other_action_name = g_strdup (other_action);

        secondary_text = g_strdup_printf (_(conflict_messages[i].message), other_action_name);

        owner_button_text = g_strdup_printf (_(conflict_messages[i].owner_button_text), owner_action_name);
        other_button_text = g_strdup_printf (_(conflict_messages[i].other_button_text), other_action_name);

        response = xfce_message_dialog (parent, title,
                                        "dialog-question",
                                        title, secondary_text,
                                        XFCE_BUTTON_TYPE_MIXED, NULL, owner_button_text, GTK_RESPONSE_ACCEPT,
                                        XFCE_BUTTON_TYPE_MIXED, NULL, other_button_text, GTK_RESPONSE_REJECT,
                                        NULL);

        g_free (other_button_text);
        g_free (owner_button_text);
        g_free (secondary_text);
        g_free (other_action_name);
        g_free (owner_action_name);
        g_free (shortcut_label);

        handled = TRUE;
        break;
      }

  if (G_UNLIKELY (!handled))
    {
      xfce_message_dialog (parent, title,
                           "dialog-error",
                           title, _("This shortcut is already being used for something else."),
                           "window-close-symbolic",
                           GTK_RESPONSE_CLOSE, NULL);
      response = GTK_RESPONSE_REJECT;
    }

  g_free (title);

  return response;
}
