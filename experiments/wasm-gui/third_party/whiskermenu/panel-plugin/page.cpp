/*
 * Copyright (C) 2013-2021 Graeme Gott <graeme@gottcode.org>
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

#include "page.h"

#include "category-button.h"
#include "favorites-page.h"
#include "image-menu-item.h"
#include "launcher.h"
#include "launcher-icon-view.h"
#include "launcher-tree-view.h"
#include "recent-page.h"
#include "settings.h"
#include "slot.h"
#include "window.h"

#include <libxfce4ui/libxfce4ui.h>

#include <glib/gstdio.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

Page::Page(Window* window, const gchar* icon, const gchar* text) :
	m_window(window),
	m_button(nullptr),
	m_selected_launcher(nullptr),
	m_drag_enabled(true),
	m_launcher_dragged(false),
	m_reorderable(false)
{
	// Create button
	if (icon && text)
	{
		GIcon* gicon = g_themed_icon_new(icon);
		m_button = new CategoryButton(gicon, text);
		g_object_unref(gicon);
	}

	// Create view
	create_view();

	// Add scrolling to view
	m_widget = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_widget), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(m_widget), GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(m_widget), m_view->get_widget());
	g_object_ref_sink(m_widget);

	gtk_style_context_add_class(gtk_widget_get_style_context(m_widget), "launchers-pane");
}

//-----------------------------------------------------------------------------

Page::~Page()
{
	delete m_button;
	delete m_view;
	gtk_widget_destroy(m_widget);
	g_object_unref(m_widget);
}

//-----------------------------------------------------------------------------

void Page::reset_selection()
{
	m_view->collapse_all();

	// Set keyboard focus on first item and scroll to top
	select_first();

	// Clear selection
	m_view->clear_selection();
}

//-----------------------------------------------------------------------------

void Page::select_first()
{
	// Select and set keyboard focus on first item
	GtkTreeModel* model = m_view->get_model();
	GtkTreeIter iter;
	if (model && gtk_tree_model_get_iter_first(model, &iter))
	{
		GtkTreePath* path = gtk_tree_model_get_path(model, &iter);
		m_view->set_cursor(path);
		m_view->select_path(path);
		m_view->scroll_to_path(path);
		gtk_tree_path_free(path);
	}

	// Scroll to top
	GtkAdjustment* adjustment = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(m_view->get_widget()));
	gtk_adjustment_set_value(adjustment, gtk_adjustment_get_lower(adjustment));
}

//-----------------------------------------------------------------------------

void Page::update_view()
{
	if ( ((wm_settings->view_mode == Settings::ViewAsIcons) && dynamic_cast<LauncherIconView*>(m_view))
			|| ((wm_settings->view_mode != Settings::ViewAsIcons) && dynamic_cast<LauncherTreeView*>(m_view)) )
	{
		return;
	}

	g_assert(m_view);
	LauncherView* view = m_view;
	create_view();
	m_view->set_model(view->get_model());
	delete view;

	gtk_container_add(GTK_CONTAINER(m_widget), m_view->get_widget());
	gtk_widget_show_all(m_widget);

	view_created();
}

//-----------------------------------------------------------------------------

void Page::set_reorderable(bool reorderable)
{
	m_reorderable = reorderable;
	if (m_reorderable)
	{
		const GtkTargetEntry row_targets[] = {
			{ g_strdup("GTK_TREE_MODEL_ROW"), GTK_TARGET_SAME_WIDGET, 0 },
			{ g_strdup("text/uri-list"), GTK_TARGET_OTHER_APP, 1 }
		};

		m_view->set_drag_source(GDK_BUTTON1_MASK,
				row_targets, 2,
				GdkDragAction(GDK_ACTION_MOVE | GDK_ACTION_COPY));

		m_view->set_drag_dest(row_targets, 1,
				GDK_ACTION_MOVE);

		g_free(row_targets[0].target);
		g_free(row_targets[1].target);
	}
	else
	{
		const GtkTargetEntry row_targets[] = {
			{ g_strdup("text/uri-list"), GTK_TARGET_OTHER_APP, 1 }
		};

		m_view->set_drag_source(GDK_BUTTON1_MASK,
				row_targets, 1,
				GDK_ACTION_COPY);

		m_view->unset_drag_dest();

		g_free(row_targets[0].target);
	}
}

//-----------------------------------------------------------------------------

void Page::create_view()
{
	if (wm_settings->view_mode == Settings::ViewAsIcons)
	{
		m_view = new LauncherIconView();
		connect(m_view->get_widget(), "item-activated",
			[this](GtkIconView*, GtkTreePath* path)
			{
				launcher_activated(path);
			});
	}
	else
	{
		m_view = new LauncherTreeView();
		connect(m_view->get_widget(), "row-activated",
			[this](GtkTreeView*, GtkTreePath* path, GtkTreeViewColumn*)
			{
				launcher_activated(path);
			});
	}

	connect(m_view->get_widget(), "button-press-event",
		[this](GtkWidget*, GdkEvent* event) -> gboolean
		{
			return view_button_press_event(event);
		});

	connect(m_view->get_widget(), "button-release-event",
		[this](GtkWidget*, GdkEvent* event) -> gboolean
		{
			return view_button_release_event(event);
		});

	connect(m_view->get_widget(), "drag-data-get",
		[this](GtkWidget*, GdkDragContext*, GtkSelectionData* data, guint info, guint)
		{
			view_drag_data_get(data, info);
		});

	connect(m_view->get_widget(), "drag-end",
		[this](GtkWidget*, GdkDragContext*)
		{
			view_drag_end();
		});

	connect(m_view->get_widget(), "popup-menu",
		[this](GtkWidget*) -> gboolean
		{
			return view_popup_menu_event();
		});

	set_reorderable(m_reorderable);
}

//-----------------------------------------------------------------------------

bool Page::remember_launcher(Launcher*)
{
	return true;
}

//-----------------------------------------------------------------------------

void Page::launcher_activated(GtkTreePath* path)
{
	GtkTreeIter iter;
	GtkTreeModel* model = m_view->get_model();
	gtk_tree_model_get_iter(model, &iter, path);

	// Find element
	Element* element = nullptr;
	gtk_tree_model_get(model, &iter, LauncherView::COLUMN_LAUNCHER, &element, -1);
	if (!element)
	{
		return;
	}

	// Add to recent
	if (Launcher* launcher = dynamic_cast<Launcher*>(element))
	{
		if (remember_launcher(launcher))
		{
			m_window->get_recent()->add(launcher);
		}
	}

	// Hide window
	m_window->hide();

	// Execute app
	element->run(gtk_widget_get_screen(m_widget));
}

//-----------------------------------------------------------------------------

void Page::launcher_action_activated(GtkMenuItem* menuitem, DesktopAction* action)
{
	g_assert(m_selected_launcher);

	// Add to recent
	if (remember_launcher(m_selected_launcher))
	{
		m_window->get_recent()->add(m_selected_launcher);
	}

	// Hide window
	m_window->hide();

	// Execute app
	m_selected_launcher->run(gtk_widget_get_screen(GTK_WIDGET(menuitem)), action);
}

//-----------------------------------------------------------------------------

gboolean Page::view_button_press_event(GdkEvent* event)
{
	GdkEventButton* button_event = reinterpret_cast<GdkEventButton*>(event);

	m_launcher_dragged = false;

	GtkTreePath* path = m_view->get_path_at_pos(button_event->x, button_event->y);
	if (!path)
	{
		return GDK_EVENT_PROPAGATE;
	}

	if (gdk_event_triggers_context_menu(event))
	{
		create_context_menu(path, event);
		return GDK_EVENT_STOP;
	}
	else if (button_event->button != GDK_BUTTON_PRIMARY)
	{
		gtk_tree_path_free(path);
		return GDK_EVENT_PROPAGATE;
	}

	Element* element = nullptr;
	GtkTreeModel* model = m_view->get_model();
	GtkTreeIter iter;
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(model, &iter, LauncherView::COLUMN_LAUNCHER, &element, -1);
	if (!(m_selected_launcher = dynamic_cast<Launcher*>(element)))
	{
		m_drag_enabled = false;
		m_view->unset_drag_source();
		m_view->unset_drag_dest();
	}
	else if (!m_drag_enabled)
	{
		m_drag_enabled = true;
		set_reorderable(m_reorderable);
	}

	m_window->set_child_has_focus();

	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

gboolean Page::view_button_release_event(GdkEvent* event)
{
	GdkEventButton* button_event = reinterpret_cast<GdkEventButton*>(event);
	if (button_event->button != 1)
	{
		return GDK_EVENT_PROPAGATE;
	}

	if (m_launcher_dragged)
	{
		m_window->hide();
		m_launcher_dragged = false;
	}

	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

void Page::view_drag_data_get(GtkSelectionData* data, guint info)
{
	if ((info != 1) || !m_selected_launcher)
	{
		return;
	}

	gchar* uris[2] = { m_selected_launcher->get_uri(), nullptr };
	if (uris[0])
	{
		gtk_selection_data_set_uris(data, uris);
		g_free(uris[0]);
	}

	m_launcher_dragged = true;
}

//-----------------------------------------------------------------------------

void Page::view_drag_end()
{
	if (m_launcher_dragged)
	{
		m_window->hide();
		m_launcher_dragged = false;
	}
}

//-----------------------------------------------------------------------------

gboolean Page::view_popup_menu_event()
{
	GtkTreePath* path = m_view->get_cursor();
	if (!path)
	{
		return GDK_EVENT_PROPAGATE;
	}

	create_context_menu(path, nullptr);

	return GDK_EVENT_STOP;
}

//-----------------------------------------------------------------------------

void Page::create_context_menu(GtkTreePath* path, GdkEvent* event)
{
	// Get selected launcher
	Element* element = nullptr;
	GtkTreeModel* model = m_view->get_model();
	GtkTreeIter iter;
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, LauncherView::COLUMN_LAUNCHER, &element, -1);
	if (!(m_selected_launcher = dynamic_cast<Launcher*>(element)))
	{
		gtk_tree_path_free(path);
		return;
	}

	// Create context menu
	GtkWidget* menu = gtk_menu_new();
	connect(menu, "selection-done",
		[this](GtkMenuShell* menu)
		{
			m_selected_launcher = nullptr;
			gtk_widget_destroy(GTK_WIDGET(menu));
		});

	// Add menu items
	GtkWidget* menuitem = gtk_menu_item_new_with_label(m_selected_launcher->get_display_name());
	gtk_widget_set_sensitive(menuitem, false);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	const auto actions = m_selected_launcher->get_actions();
	if (!actions.empty())
	{
		for (auto action : actions)
		{
			menuitem = whiskermenu_image_menu_item_new(action->get_icon(), action->get_name());
			connect(menuitem, "activate",
				[this, action](GtkMenuItem* menuitem)
				{
					launcher_action_activated(menuitem, action);
				});
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		}

		menuitem = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}

	if (!m_window->get_favorites()->contains(m_selected_launcher))
	{
		menuitem = whiskermenu_image_menu_item_new("bookmark-new", _("Add to Favorites"));
		connect(menuitem, "activate",
			[this](GtkMenuItem*)
			{
				g_assert(m_selected_launcher);
				m_window->get_favorites()->add(m_selected_launcher);
			});
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}
	else
	{
		menuitem = whiskermenu_image_menu_item_new("list-remove", _("Remove from Favorites"));
		connect(menuitem, "activate",
			[this](GtkMenuItem*)
			{
				g_assert(m_selected_launcher);
				m_window->get_favorites()->remove(m_selected_launcher);
			});
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}

	menuitem = whiskermenu_image_menu_item_new("list-add", _("Add to Desktop"));
	connect(menuitem, "activate",
		[this](GtkMenuItem*)
		{
			add_selected_to_desktop();
		});
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = whiskermenu_image_menu_item_new("list-add", _("Add to Panel"));
	connect(menuitem, "activate",
		[this](GtkMenuItem*)
		{
			add_selected_to_panel();
		});
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = whiskermenu_image_menu_item_new("gtk-edit", _("Edit Application..."));
	connect(menuitem, "activate",
		[this](GtkMenuItem*)
		{
			edit_selected();
		});
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = whiskermenu_image_menu_item_new("edit-delete", _("Hide Application"));
	connect(menuitem, "activate",
		[this](GtkMenuItem*)
		{
			g_assert(m_selected_launcher);
			m_window->hide();
			m_selected_launcher->hide();
		});
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	extend_context_menu(menu);

	gtk_widget_show_all(menu);

	// Show context menu
	m_window->set_child_has_focus();
	gtk_menu_attach_to_widget(GTK_MENU(menu), m_view->get_widget(), nullptr);
	gtk_menu_popup_at_pointer(GTK_MENU(menu), event);

	// Keep selection
	m_view->select_path(path);
	gtk_tree_path_free(path);
}

//-----------------------------------------------------------------------------

void Page::extend_context_menu(GtkWidget*)
{
}

//-----------------------------------------------------------------------------

void Page::add_selected_to_desktop()
{
	// Fetch desktop folder
	const gchar* desktop_path = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
	GFile* desktop_folder = g_file_new_for_path(desktop_path);

	// Fetch launcher source
	g_assert(m_selected_launcher);
	GFile* source_file = m_selected_launcher->get_file();

	// Fetch launcher destination
	gchar* basename = g_file_get_basename(source_file);
	GFile* destination_file = g_file_get_child(desktop_folder, basename);
	g_free(basename);

	// Copy launcher to desktop folder
	GError* error = nullptr;
	if (g_file_copy(source_file, destination_file, G_FILE_COPY_NONE, nullptr, nullptr, nullptr, &error))
	{
		// Make launcher executable
		gchar* path = g_file_get_path(destination_file);
		g_chmod(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
		g_free(path);

#if LIBXFCE4UTIL_CHECK_VERSION(4,17,0)
		// Make launcher trusted
		xfce_g_file_set_trusted(destination_file, true, nullptr, nullptr);
#endif
	}
	else
	{
		xfce_dialog_show_error(nullptr, error, _("Unable to add launcher to desktop."));
		g_error_free(error);
	}

	g_object_unref(destination_file);
	g_object_unref(source_file);
	g_object_unref(desktop_folder);
}

//-----------------------------------------------------------------------------

void Page::add_selected_to_panel()
{
	// Connect to Xfce panel through D-Bus
	GError* error = nullptr;
	GDBusProxy* proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
			G_DBUS_PROXY_FLAGS_NONE,
			nullptr,
			"org.xfce.Panel",
			"/org/xfce/Panel",
			"org.xfce.Panel",
			nullptr,
			&error);
	if (proxy)
	{
		// Fetch launcher desktop ID
		g_assert(m_selected_launcher);
		const gchar* parameters[] = { m_selected_launcher->get_desktop_id(), nullptr };

		// Tell panel to add item
		GVariant* result = g_dbus_proxy_call_sync(proxy,
				"AddNewItem",
				g_variant_new("(s^as)", "launcher", parameters),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				nullptr,
				&error);

		if (!result)
		{
			xfce_dialog_show_error(nullptr, error, _("Unable to add launcher to panel."));
			g_error_free(error);
		}
		else
		{
			g_variant_unref(result);
		}

		// Disconnect from D-Bus
		g_object_unref(proxy);
	}
	else
	{
		xfce_dialog_show_error(nullptr, error, _("Unable to add launcher to panel."));
		g_error_free(error);
	}
}

//-----------------------------------------------------------------------------

void Page::edit_selected()
{
	g_assert(m_selected_launcher);

	m_window->hide();

	gchar* uri = m_selected_launcher->get_uri();
	gchar* command = g_strdup_printf("exo-desktop-item-edit '%s'", uri);
	g_free(uri);

	GError* error = nullptr;
	if (!g_spawn_command_line_async(command, &error))
	{
		xfce_dialog_show_error(nullptr, error, _("Unable to edit launcher."));
		g_error_free(error);
	}
	g_free(command);
}

//-----------------------------------------------------------------------------
