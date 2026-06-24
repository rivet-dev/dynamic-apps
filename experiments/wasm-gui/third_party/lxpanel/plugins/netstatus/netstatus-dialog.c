/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2004 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include <glib/gi18n.h>
#include "netstatus-dialog.h"
#include <libfm/fm-gtk.h>

#include <string.h>
/* #include <gconf/gconf-client.h> */

#include "netstatus-icon.h"
#include "netstatus-util.h"

#define NETWORK_CONFIG_TOOL_DIR "/apps/netstatus_applet"
#define NETWORK_CONFIG_TOOL_KEY NETWORK_CONFIG_TOOL_DIR "/config_tool"

#if 0 /* stripped-down version does nothing to configurators. */
static const char *network_config_tools[] = {
  "network-admin --configure %i",
  "redhat-config-network",
  "system-control-network"
};
#endif

#define UNKNOWN_STR(t,s) G_STMT_START {       \
	if (!((t) = (s))) (t) = _("Unknown"); \
} G_STMT_END

typedef struct
{
  GtkBuilder* builder;
  GtkWidget      *dialog;

  NetstatusIface *iface;
  NetstatusIcon  *icon;

  /*  GConfClient    *client; */
  char           *config_tool;
  /* guint           listener; */

  GtkWidget      *name;
  GtkWidget      *name_entry; /* child GtkEntry of name (GtkComboBoxEntry) */
  GtkWidget      *status;
  GtkWidget      *received;
  GtkWidget      *sent;
  GtkWidget      *signal_strength_frame;
  GtkWidget      *signal_strength_bar;
  GtkWidget      *signal_strength_label;
  GtkWidget      *configure_button;

  GtkWidget      *inet4_frame;
  GtkWidget      *inet4_table;
  GtkWidget      *inet4_addr;
  GtkWidget      *inet4_addr_title;
  GtkWidget      *inet4_dest;
  GtkWidget      *inet4_dest_title;
  GtkWidget      *inet4_bcast;
  GtkWidget      *inet4_bcast_title;
  GtkWidget      *inet4_mask;
  GtkWidget      *inet4_mask_title;

  GtkWidget      *dev_frame;
  GtkWidget      *dev_type;
  GtkWidget      *dev_addr;

  guint           iface_list_monitor;
  int             n_ifaces;
} NetstatusDialogData;


static void
netstatus_dialog_update_state (NetstatusDialogData *data)
{
  NetstatusState state;

  state = netstatus_iface_get_state (data->iface);
  gtk_label_set_text (GTK_LABEL (data->status),
		      netstatus_get_state_string (state));
}

static void
netstatus_dialog_update_name (NetstatusDialogData *data)
{
  const char *iface_name;
  const char *text;

  iface_name = netstatus_iface_get_name (data->iface);
  if (!iface_name)
    {
      gtk_window_set_title (GTK_WINDOW (data->dialog),
			    _("Connection Properties"));
    }
  else
    {
      char *p;

      p = g_strdup_printf (_("Connection Properties: %s"), iface_name);
      gtk_window_set_title (GTK_WINDOW (data->dialog), p);
      g_free (p);
    }

  UNKNOWN_STR (text, iface_name);
  gtk_entry_set_text (GTK_ENTRY (data->name_entry), text);
}

static inline void
print_packets_string (GString *str,
		      gulong   packets)
{
  g_string_printf (str, ngettext ("%lu packet", "%lu packets", packets), packets);
}

static inline void
print_bytes_string (GString *str,
		    guint64  bytes)
{
  if (bytes > 1 << 30)
    {
      bytes = (bytes * 10) / (1 << 30);

      g_string_append_printf (str, " (%"G_GUINT64_FORMAT".%"G_GUINT64_FORMAT" GiB)",
                              bytes / 10, bytes % 10);
    }
  else if (bytes > 1 << 20)
    {
      bytes = (bytes * 10) / (1 << 20);

      g_string_append_printf (str, " (%"G_GUINT64_FORMAT".%"G_GUINT64_FORMAT" MiB)",
                              bytes / 10, bytes % 10);
    }
  else if (bytes > 1 << 10)
    {
      bytes = (bytes * 10) / (1 << 10);

      g_string_append_printf (str, " (%"G_GUINT64_FORMAT".%"G_GUINT64_FORMAT" KiB)",
                              bytes / 10, bytes % 10);
    }
  else if (bytes > 0)
    {
      g_string_append_printf (str, " (%"G_GUINT64_FORMAT" B)", bytes);
    }
}

static void
netstatus_dialog_update_activity (NetstatusDialogData *data)
{
  NetstatusStats  stats = { 0, };
  GString        *str;

  netstatus_iface_get_statistics (data->iface, &stats);

  str = g_string_new (NULL);

  print_packets_string (str, stats.out_packets);
  print_bytes_string (str, stats.out_bytes);
  gtk_label_set_text (GTK_LABEL (data->sent), str->str);

  print_packets_string (str, stats.in_packets);
  print_bytes_string (str, stats.in_bytes);
  gtk_label_set_text (GTK_LABEL (data->received), str->str);

  g_string_free (str, TRUE);
}

static void
netstatus_dialog_update_signal_strength (NetstatusDialogData *data)
{
  if (netstatus_iface_get_is_wireless (data->iface))
    {
      int   signal_strength;
      char *str;

      gtk_widget_show (data->signal_strength_frame);

      signal_strength = netstatus_iface_get_signal_strength (data->iface);

      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (data->signal_strength_bar),
				     signal_strength == 0 ? 0.0 : signal_strength / 100.0);

      str = g_strdup_printf ("%d%%\n", signal_strength);
      gtk_label_set_text (GTK_LABEL (data->signal_strength_label), str);
      g_free (str);
    }
  else
    {
      gtk_widget_hide (data->signal_strength_frame);
    }
}

static void
netstatus_dialog_update_inet4_support (NetstatusDialogData *data)
{
  char *addr = NULL;
  char *dest = NULL;
  char *bcast = NULL;
  char *mask = NULL;

  if (netstatus_iface_get_inet4_details (data->iface, &addr, &dest, &bcast, &mask))
    {
      gtk_widget_show (data->inet4_frame);

      /* Address */
      if (addr)
	{
	  gtk_table_set_row_spacing (GTK_TABLE (data->inet4_table), 0, 6);
	  gtk_label_set_text (GTK_LABEL (data->inet4_addr), addr);
	  gtk_widget_show (data->inet4_addr);
	  gtk_widget_show (data->inet4_addr_title);
	}
      else
	{
	  gtk_table_set_row_spacing (GTK_TABLE (data->inet4_table), 0, 0);
	  gtk_widget_hide (data->inet4_addr);
	  gtk_widget_hide (data->inet4_addr_title);
	}

      /* Destination */
      if (dest)
	{
	  gtk_table_set_row_spacing (GTK_TABLE (data->inet4_table), 1, 6);
	  gtk_label_set_text (GTK_LABEL (data->inet4_dest), dest);
	  gtk_widget_show (data->inet4_dest);
	  gtk_widget_show (data->inet4_dest_title);
	}
      else
	{
	  gtk_table_set_row_spacing (GTK_TABLE (data->inet4_table), 1, 0);
	  gtk_widget_hide (data->inet4_dest);
	  gtk_widget_hide (data->inet4_dest_title);
	}

      /* Broadcast */
      if (bcast)
	{
	  gtk_table_set_row_spacing (GTK_TABLE (data->inet4_table), 2, 6);
	  gtk_label_set_text (GTK_LABEL (data->inet4_bcast), bcast);
	  gtk_widget_show (data->inet4_bcast);
	  gtk_widget_show (data->inet4_bcast_title);
	}
      else
	{
	  gtk_table_set_row_spacing (GTK_TABLE (data->inet4_table), 2, 0);
	  gtk_widget_hide (data->inet4_bcast);
	  gtk_widget_hide (data->inet4_bcast_title);
	}

      /* Subnet Mask */
      if (mask)
	{
	  gtk_label_set_text (GTK_LABEL (data->inet4_mask), mask);
	  gtk_widget_show (data->inet4_mask);
	  gtk_widget_show (data->inet4_mask_title);
	}
      else
	{
	  gtk_widget_hide (data->inet4_mask);
	  gtk_widget_hide (data->inet4_mask_title);
	}

      g_free (addr);
      g_free (dest);
      g_free (bcast);
      g_free (mask);
    }
  else
    {
      gtk_widget_hide (data->inet4_frame);
    }
}

static void
netstatus_dialog_update_device_support (NetstatusDialogData *data)
{
  const char *type = NULL;
  char        *addr = NULL;

  if (netstatus_iface_get_device_details (data->iface, &type, &addr))
    {
      g_assert (type != NULL);

      gtk_widget_show (data->dev_frame);

      /* Device type */
      gtk_label_set_text (GTK_LABEL (data->dev_type), type);

      /* Address */
      if (addr)
	gtk_label_set_text (GTK_LABEL (data->dev_addr), addr);
      else
	gtk_label_set_text (GTK_LABEL (data->dev_addr), _("Unknown"));

      g_free (addr);
    }
  else
    {
      gtk_widget_hide (data->dev_frame);
    }
}

static void
netstatus_dialog_iface_state_changed (NetstatusIface      *iface,
				      GParamSpec          *pspec,
				      NetstatusDialogData *data)
{
  netstatus_dialog_update_state (data);
  netstatus_dialog_update_inet4_support (data);
  netstatus_dialog_update_device_support (data);
}

static void
netstatus_dialog_iface_stats_changed (NetstatusIface      *iface,
				      GParamSpec          *pspec,
				      NetstatusDialogData *data)
{
  netstatus_dialog_update_activity (data);
}

static void
netstatus_dialog_iface_signal_strength_changed (NetstatusIface      *iface,
						GParamSpec          *pspec,
						NetstatusDialogData *data)
{
  netstatus_dialog_update_signal_strength (data);
}

static void
netstatus_dialog_iface_name_changed (NetstatusIface      *iface,
				     GParamSpec          *pspec,
				     NetstatusDialogData *data)
{
  netstatus_dialog_update_name (data);
  netstatus_dialog_update_signal_strength (data);
}

static void
netstatus_dialog_set_iface_name (NetstatusDialogData *data,
				 GtkEntry            *entry)
{
  const char *iface_name;

  iface_name = gtk_entry_get_text (entry);
  if (iface_name && iface_name [0])
    {
      netstatus_iface_set_name (data->iface, iface_name);

      gtk_widget_set_sensitive (data->configure_button,
				!netstatus_iface_get_is_loopback (data->iface));
    }
}

static void
netstatus_dialog_response (GtkWidget *dialog,
			   int        response)
{
  switch (response)
    {
    case GTK_RESPONSE_HELP:
      {
	GError    *error = NULL;
	GdkScreen *screen;

	screen = gtk_window_get_screen (GTK_WINDOW (dialog));

	// gnome_help_display_on_screen ("gnome-netstatus", "gnome-netstatus-props", screen, &error);

	if (error)
	  {
	    GtkWidget *message_dialog;

	    message_dialog = gtk_message_dialog_new (GTK_WINDOW (dialog),
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_MESSAGE_ERROR,
						     GTK_BUTTONS_CLOSE,
						     _("There was an error displaying help:\n%s"),
						     error->message);

	    gtk_window_set_resizable (GTK_WINDOW (message_dialog), FALSE);
	    gtk_window_set_screen (GTK_WINDOW (message_dialog), screen);

	    g_signal_connect (message_dialog, "response",
			      G_CALLBACK (gtk_widget_destroy),
			      NULL);

	    gtk_widget_show (message_dialog);

	    g_error_free (error);
	  }
      }
      break;
    default:
      gtk_widget_hide (dialog);
      break;
    }
}

static void
netstatus_dialog_destroy (GtkWidget *dialog)
{
  NetstatusDialogData *data;

  data = g_object_get_data (G_OBJECT (dialog), "netstatus-dialog-data");
  if (!data)
    return;

  if (data->config_tool)
    g_free (data->config_tool);
  data->config_tool = NULL;

  /*
  if (data->listener)
    {
      g_assert (data->client != NULL);

      gconf_client_notify_remove (data->client, data->listener);
      data->listener = 0;

      gconf_client_remove_dir (data->client, NETWORK_CONFIG_TOOL_DIR, NULL);
    }

  if (data->client)
    g_object_unref (data->client);
  data->client = NULL;
  */

  if (data->iface_list_monitor)
    g_source_remove (data->iface_list_monitor);
  data->iface_list_monitor = 0;

  if (data->iface)
    g_object_unref (data->iface);
  data->iface = NULL;

  g_free (data);
}

static gboolean
netstatus_dialog_check_config_tool (NetstatusDialogData *dialog_data,
				    const char          *config_tool)
{
  char     **argv = NULL;
  gboolean   found = FALSE;

  if (config_tool && g_shell_parse_argv (config_tool, NULL, &argv, NULL))
    {
      char *path;

      g_assert (argv != NULL);

      if ((path = g_find_program_in_path (argv [0])))
	{
	  if (dialog_data->config_tool)
	    g_free (dialog_data->config_tool);
	  dialog_data->config_tool = g_strdup (config_tool);
	  found = TRUE;
	}

      g_free (path);
      g_strfreev (argv);
    }

  return found;
}

/*
static void
netstatus_dialog_config_tool_notify (GConfClient         *client,
				     guint                cnx_id,
				     GConfEntry          *entry,
				     NetstatusDialogData *dialog_data)
{
  if (!entry->value || !entry->value->type == GCONF_VALUE_STRING)
    return;

  netstatus_dialog_check_config_tool (dialog_data,
				      gconf_value_get_string (entry->value));
}
*/

static void
netstatus_dialog_detect_configuration_tool (NetstatusDialogData *dialog_data)
{
  /*
  char *config_tool;
  int   i;

  dialog_data->client = gconf_client_get_default ();

  gconf_client_add_dir (dialog_data->client,
			NETWORK_CONFIG_TOOL_DIR,
			GCONF_CLIENT_PRELOAD_NONE,
			NULL);

  dialog_data->listener =
    gconf_client_notify_add (dialog_data->client,
			     NETWORK_CONFIG_TOOL_KEY,
			     (GConfClientNotifyFunc) netstatus_dialog_config_tool_notify,
			     dialog_data, NULL, NULL);

  config_tool = gconf_client_get_string (dialog_data->client,
					 NETWORK_CONFIG_TOOL_KEY,
					 NULL);
  if (netstatus_dialog_check_config_tool (dialog_data, config_tool))
    {
      g_free (config_tool);
      return;
    }

  for (i = 0; i < G_N_ELEMENTS (network_config_tools); i++)
    {
      if (netstatus_dialog_check_config_tool (dialog_data, network_config_tools [i]))
	break;
    }
  */
}

static void
netstatus_iface_configure (GtkWidget           *configure_button,
			   NetstatusDialogData *dialog_data)
{
  GString    *command;
  char      **argv = NULL;
  int         i;

  g_return_if_fail (dialog_data != NULL);
  g_return_if_fail (dialog_data->config_tool != NULL);

  g_shell_parse_argv (dialog_data->config_tool, NULL, &argv, NULL);

  /* We've already verified config_tool as valid */
  g_assert (argv != NULL);

  command = g_string_new (argv [0]);

  for (i = 1; argv [i]; i++)
    {
      command = g_string_append_c (command, ' ');

      if (!strcmp (argv [i], "%i"))
	command = g_string_append (command,
				   netstatus_iface_get_name (dialog_data->iface));
      else
	command = g_string_append (command, argv [i]);
    }

  fm_launch_command_simple(GTK_WINDOW (dialog_data->dialog), NULL,
                           G_APP_INFO_CREATE_NONE, command->str, NULL);

  g_string_free (command, TRUE);
  g_strfreev (argv);
}

static void
netstatus_dialog_setup_configure_button (NetstatusDialogData *data)
{
  data->configure_button = (GtkWidget*)gtk_builder_get_object(data->builder, "configure_button");

  g_signal_connect (data->configure_button, "clicked",
		    G_CALLBACK (netstatus_iface_configure), data);

  netstatus_dialog_detect_configuration_tool (data);

  if (!data->config_tool)
    gtk_widget_hide (data->configure_button);

  gtk_widget_set_sensitive (data->configure_button,
			    !netstatus_iface_get_is_loopback (data->iface));
}

#if !GTK_CHECK_VERSION(2, 24, 0)
static void _combo_box_entry_active_changed (GtkComboBox *combo_box,
                                             gpointer     user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *str = NULL;

  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
      GtkEntry *entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo_box)));

      if (entry)
        {
          model = gtk_combo_box_get_model (combo_box);

          gtk_tree_model_get (model, &iter, 0, &str, -1);
          gtk_entry_set_text (entry, str);
          g_free (str);
        }
    }
}
#endif

static void
netstatus_dialog_setup_connection (NetstatusDialogData *data)
{
  GtkWidget *hbox;
  GtkWidget *icon;
  GtkListStore* model;

  hbox = (GtkWidget*)gtk_builder_get_object(data->builder, "connection_hbox");

  icon = netstatus_icon_new (data->iface);
  netstatus_icon_set_tooltips_enabled (NETSTATUS_ICON (icon), FALSE);
  netstatus_icon_set_show_signal (NETSTATUS_ICON (icon), FALSE);
  gtk_box_pack_end (GTK_BOX (hbox), icon, FALSE, TRUE, 4);
  gtk_widget_show (icon);

  data->icon = NETSTATUS_ICON (icon);

  data->name   = (GtkWidget*)gtk_builder_get_object(data->builder, "name_combo");
  data->name_entry = gtk_bin_get_child((GtkBin*)data->name);
  model = gtk_list_store_new(1, G_TYPE_STRING);
  gtk_combo_box_set_model(GTK_COMBO_BOX(data->name), GTK_TREE_MODEL(model));
#if GTK_CHECK_VERSION(2, 24, 0)
  gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(data->name), 0);
#else
  /* emulate 2.24/GtkComboBoxEntry behavior */
  GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (data->name),
                              text_renderer, TRUE);

  gtk_combo_box_set_active (GTK_COMBO_BOX (data->name), -1);

  g_signal_connect (data->name, "changed",
                    G_CALLBACK (_combo_box_entry_active_changed), NULL);

  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (data->name),
                                  text_renderer, "text", 0, NULL);
#endif
  g_object_unref(model);

  data->status = (GtkWidget*)gtk_builder_get_object(data->builder, "status_label");

  netstatus_dialog_update_name (data);
  netstatus_dialog_update_state (data);
}

static void
netstatus_dialog_setup_activity (NetstatusDialogData *data)
{
  data->sent     = (GtkWidget*)gtk_builder_get_object(data->builder, "sent_label");
  data->received = (GtkWidget*)gtk_builder_get_object(data->builder, "received_label");

  netstatus_dialog_update_activity (data);
}

static void
netstatus_dialog_setup_signal_strength_details (NetstatusDialogData *data)
{
  data->signal_strength_frame = (GtkWidget*)gtk_builder_get_object(data->builder, "signal_strength_frame");
  data->signal_strength_bar   = (GtkWidget*)gtk_builder_get_object(data->builder, "signal_strength_bar");
  data->signal_strength_label = (GtkWidget*)gtk_builder_get_object(data->builder, "signal_strength_label");

  netstatus_dialog_update_signal_strength (data);
}

static void
netstatus_dialog_setup_inet4_support (NetstatusDialogData *data)
{
  data->inet4_frame       = (GtkWidget*)gtk_builder_get_object(data->builder, "inet4_frame");
  data->inet4_table       = (GtkWidget*)gtk_builder_get_object(data->builder, "inet4_table");
  data->inet4_addr        = (GtkWidget*)gtk_builder_get_object(data->builder, "inet4_addr_label");
  data->inet4_addr_title  = (GtkWidget*)gtk_builder_get_object(data->builder, "inet4_addr_title");
  data->inet4_dest        = (GtkWidget*)gtk_builder_get_object(data->builder, "inet4_dest_label");
  data->inet4_dest_title  = (GtkWidget*)gtk_builder_get_object(data->builder, "inet4_dest_title");
  data->inet4_bcast       = (GtkWidget*)gtk_builder_get_object(data->builder, "inet4_bcast_label");
  data->inet4_bcast_title = (GtkWidget*)gtk_builder_get_object(data->builder, "inet4_bcast_title");
  data->inet4_mask        = (GtkWidget*)gtk_builder_get_object(data->builder, "inet4_mask_label");
  data->inet4_mask_title  = (GtkWidget*)gtk_builder_get_object(data->builder, "inet4_mask_title");

  netstatus_dialog_update_inet4_support (data);
}
static void
netstatus_dialog_setup_device_support (NetstatusDialogData *data)
{
  data->dev_frame = (GtkWidget*)gtk_builder_get_object(data->builder, "dev_frame");
  data->dev_type  = (GtkWidget*)gtk_builder_get_object(data->builder, "dev_type_label");
  data->dev_addr  = (GtkWidget*)gtk_builder_get_object(data->builder, "dev_addr_label");

  netstatus_dialog_update_device_support (data);
}

static void
netstatus_dialog_set_icon (GtkWidget *dialog)
{
  GtkIconTheme *icon_theme;
  GtkIconInfo  *icon_info;

  icon_theme = gtk_icon_theme_get_for_screen (gtk_window_get_screen (GTK_WINDOW (dialog)));

  if ((icon_info = gtk_icon_theme_lookup_icon (icon_theme, "gnome-netstatus-tx", 48, 0)))
    {
      gtk_window_set_icon_from_file (GTK_WINDOW (dialog),
				     gtk_icon_info_get_filename (icon_info),
				     NULL);
      gtk_icon_info_free (icon_info);
    }
}

static gboolean
_netstatus_dialog_iface_list_monitor (NetstatusDialogData *data)
{
  GList *iface_names, *l;
  int    n_ifaces;
  GtkListStore* model;

  iface_names = netstatus_list_interface_names (NULL);

  n_ifaces = g_list_length (iface_names);

  if (data->n_ifaces != n_ifaces)
    {
      model = (GtkListStore*)gtk_combo_box_get_model(GTK_COMBO_BOX(data->name));
      gtk_list_store_clear(model);
      g_signal_handlers_block_by_func (data->name_entry,
				       G_CALLBACK (netstatus_dialog_set_iface_name), data);

      for (l = iface_names; l; l = l->next)
        {
          GtkTreeIter it;
          gtk_list_store_append(model, &it);
          gtk_list_store_set(model, &it, 0, (char*)l->data, -1);
        }
      /* GtkCombo is deprecated. */
      /* gtk_combo_set_popdown_strings (GTK_COMBO (data->name), iface_names); */
      netstatus_dialog_update_name (data);

      g_signal_handlers_unblock_by_func (data->name_entry,
					 G_CALLBACK (netstatus_dialog_set_iface_name), data);
    }

  data->n_ifaces = n_ifaces;

  for (l = iface_names; l; l = l->next)
    g_free (l->data);
  g_list_free (iface_names);

  return TRUE;
}

static gboolean
netstatus_dialog_iface_list_monitor (gpointer data)
{
    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    return _netstatus_dialog_iface_list_monitor (data);
}

GtkWidget *
netstatus_dialog_new (NetstatusIface *iface)
{
  NetstatusDialogData *data;
  data = g_new0 (NetstatusDialogData, 1);

  data->builder = gtk_builder_new();
  gtk_builder_add_from_file(data->builder, PACKAGE_UI_DIR "/netstatus.ui", NULL);
  data->dialog = (GtkWidget*)gtk_builder_get_object(data->builder, "network_status_dialog");

  g_object_set_data (G_OBJECT (data->dialog), "netstatus-dialog-data", data);

  netstatus_dialog_set_icon (data->dialog);

  data->iface = g_object_ref (iface);
  netstatus_connect_signal_while_alive (data->iface,
					"notify::state",
					G_CALLBACK (netstatus_dialog_iface_state_changed),
					data,
					data->dialog);

  netstatus_connect_signal_while_alive (data->iface,
					"notify::stats",
					G_CALLBACK (netstatus_dialog_iface_stats_changed),
					data,
					data->dialog);

  netstatus_connect_signal_while_alive (data->iface,
					"notify::name",
					G_CALLBACK (netstatus_dialog_iface_name_changed),
					data,
					data->dialog);

  netstatus_connect_signal_while_alive (data->iface,
					"notify::wireless",
					G_CALLBACK (netstatus_dialog_iface_signal_strength_changed),
					data,
					data->dialog);

  netstatus_connect_signal_while_alive (data->iface,
					"notify::signal-strength",
					G_CALLBACK (netstatus_dialog_iface_signal_strength_changed),
					data,
					data->dialog);

  g_signal_connect (data->dialog, "response",
		    G_CALLBACK (netstatus_dialog_response), NULL);

  g_signal_connect (data->dialog, "destroy",
		    G_CALLBACK (netstatus_dialog_destroy), NULL);

  netstatus_dialog_setup_connection (data);
  netstatus_dialog_setup_activity (data);
  netstatus_dialog_setup_signal_strength_details (data);
  netstatus_dialog_setup_inet4_support (data);
  netstatus_dialog_setup_device_support (data);
  netstatus_dialog_setup_configure_button (data);

  data->iface_list_monitor = g_timeout_add (2 * 1000,
					    netstatus_dialog_iface_list_monitor,
					    data);
  _netstatus_dialog_iface_list_monitor (data);

  g_signal_connect_swapped (data->name_entry, "changed",
			    G_CALLBACK (netstatus_dialog_set_iface_name),
			    data);

  g_object_unref(data->builder);
  data->builder = NULL;

  return data->dialog;
}

/* 2006.10.08 Add by Hong Jen Yee (PCMan) to be used in lxpanel plugin */
void netstatus_dialog_set_configuration_tool( GtkWidget* dialog, const char* tool )
{
    NetstatusDialogData *data;
    data = g_object_get_data (G_OBJECT (dialog), "netstatus-dialog-data");

    netstatus_dialog_check_config_tool( data, tool );

    if (data->config_tool)
        gtk_widget_show (data->configure_button);
    else
        gtk_widget_hide (data->configure_button);
}

const char* netstatus_dialog_get_configuration_tool( GtkWidget* dialog )
{
    NetstatusDialogData *data;
    data = g_object_get_data (G_OBJECT (dialog), "netstatus-dialog-data");
    return data->config_tool;
}

/* 2009.05.10 Add by Hong Jen Yee (PCMan) to be used in lxpanel plugin */
const char* netstatus_dialog_get_iface_name( GtkWidget* dialog )
{
    NetstatusDialogData *data;
    data = g_object_get_data (G_OBJECT (dialog), "netstatus-dialog-data");
    return netstatus_iface_get_name (data->iface);
}
