/*
 * Copyright (C) 2021-2023 Graeme Gott <graeme@gottcode.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <string>
#include <vector>

#include <gio/gio.h>
#include <glib/gi18n.h>

#include <xfconf/xfconf.h>

//-----------------------------------------------------------------------------

static int listInstances()
{
	// Connect to Xfconf
	if (!xfconf_init(nullptr))
	{
		return EXIT_FAILURE;
	}
	XfconfChannel* channel = xfconf_channel_get("xfce4-panel");

	// Find all plugins in each panel
	std::vector<std::string> panels;
	GPtrArray* values = xfconf_channel_get_arrayv(channel, "/panels");
	const guint size = (values) ? values->len : xfconf_channel_get_uint(channel, "/panels", 0);
	for (guint i = 0; i < size; ++i)
	{
		const guint id = (values) ? g_value_get_int(static_cast<GValue*>(g_ptr_array_index(values, i))) : i;

		gchar* panel = g_strdup_printf("/panels/panel-%d/plugin-ids", id);
		panels.push_back(panel);
		g_free(panel);
	}
	xfconf_array_free(values);

	// Find Whisker Menu plugins
	for (const std::string& panel : panels)
	{
		values = xfconf_channel_get_arrayv(channel, panel.c_str());
		if (!values)
		{
			continue;
		}

		for (guint i = 0; i < values->len; ++i)
		{
			const GValue* id = static_cast<GValue*>(g_ptr_array_index(values, i));
			if (!G_VALUE_HOLDS_INT(id))
			{
				continue;
			}
			const int instance = g_value_get_int(id);

			gchar* plugin = g_strdup_printf("/plugins/plugin-%d", instance);
			gchar* name = xfconf_channel_get_string(channel, plugin, nullptr);
			if (g_strcmp0(name, "whiskermenu") == 0)
			{
				std::cout << instance << std::endl;
			}
			g_free(name);
			g_free(plugin);
		}
		xfconf_array_free(values);
	}

	// Disconnect from Xfconf
	xfconf_shutdown();

	return EXIT_SUCCESS;
}

//-----------------------------------------------------------------------------

int main(int argc, char** argv)
{
	// Set up gettext
	setlocale(LC_ALL, "");
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	// Parse commandline options
	gboolean at_pointer = FALSE;
	gboolean at_center = FALSE;
	gboolean print_version = FALSE;
	gboolean list_instances = FALSE;
	gint instance_number = 0;
	const GOptionEntry entries[] =
	{
		{ "pointer", 'p', 0, G_OPTION_ARG_NONE, &at_pointer, _("Popup menu at current mouse position"), nullptr },
		{ "center", 'c', 0, G_OPTION_ARG_NONE, &at_center, _("Popup menu at center of screen"), nullptr },
		{ "list", 'l', 0, G_OPTION_ARG_NONE, &list_instances, _("Print available menu instance IDs"), nullptr },
		{ "instance", 'i', 0, G_OPTION_ARG_INT, &instance_number, _("Choose which menu to popup by instance ID"), nullptr },
		{ "version", 'V', 0, G_OPTION_ARG_NONE, &print_version, _("Print version information and exit"), nullptr },
		{ nullptr, '\0', 0, G_OPTION_ARG_NONE, nullptr, nullptr, nullptr }
	};

	GError* error = nullptr;
	GOptionContext* context = g_option_context_new(nullptr);
	g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse(context, &argc, &argv, &error))
	{
		std::cerr << PACKAGE_NAME << ": " << error->message << std::endl;
		g_error_free(error);
		return EXIT_FAILURE;
	}
	g_option_context_free(context);

	if (print_version)
	{
		std::cout << PACKAGE_NAME << " " << PACKAGE_VERSION << "\n"
				<< _("Copyright \302\251 2013-2024 Graeme Gott") << std::endl;
		return EXIT_SUCCESS;
	}

	if (list_instances)
	{
		return listInstances();
	}

	// Connect to Xfce panel through D-Bus
	error = nullptr;
	GDBusProxy* proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
			G_DBUS_PROXY_FLAGS_NONE,
			nullptr,
			"org.xfce.Panel",
			"/org/xfce/Panel",
			"org.xfce.Panel",
			nullptr,
			&error);
	if (!proxy)
	{
		std::cerr << PACKAGE_NAME << ": " << error->message << std::endl;
		g_error_free(error);
		return EXIT_FAILURE;
	}

	// Tell panel to call remote-event on Whisker Menu
	const std::string instance = !instance_number ? "whiskermenu" : ("whiskermenu-" + std::to_string(instance_number));

	int result = EXIT_SUCCESS;
	GVariant* variant = g_variant_new_variant(g_variant_new_int32(at_pointer + (at_center * 2)));
	GVariant* ret = g_dbus_proxy_call_sync(proxy,
			"PluginEvent",
			g_variant_new("(ss@v)", instance.c_str(), "popup", variant),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			nullptr,
			&error);
	if (!ret)
	{
		std::cerr << PACKAGE_NAME << ": " << error->message << std::endl;
		g_error_free(error);
		result = EXIT_FAILURE;
	}
	else
	{
		g_variant_unref(ret);
	}

	// Disconnect from D-Bus
	g_object_unref(G_OBJECT(proxy));

	return result;
}

//-----------------------------------------------------------------------------
