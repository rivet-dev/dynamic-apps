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

#include "window.h"

#include "applications-page.h"
#include "category-button.h"
#include "command.h"
#include "favorites-page.h"
#include "launcher-view.h"
#include "plugin.h"
#include "profile.h"
#include "recent-page.h"
#include "search-page.h"
#include "settings.h"
#include "slot.h"
#include "util.h"

#include <gdk/gdkkeysyms.h>
#include <libxfce4ui/libxfce4ui.h>

#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell.h>
#endif

#include <ctime>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

WhiskerMenu::Window::Window(Plugin* plugin) :
	m_plugin(plugin),
	m_window(nullptr),
	m_sidebar_size_group(nullptr),
	m_geometry{0,0,1,1},
	m_layout_ltr(true),
	m_layout_categories_horizontal(false),
	m_layout_categories_alternate(false),
	m_layout_search_alternate(false),
	m_layout_commands_alternate(false),
	m_layout_profile_alternate(false),
	m_profile_shape(0),
	m_supports_alpha(false),
	m_child_has_focus(false)
{
	// Create the window
	m_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_widget_set_name(GTK_WIDGET(m_window), "whiskermenu-window");
	// Untranslated window title to allow window managers to identify it; not visible to users.
	gtk_window_set_title(m_window, "Whisker Menu");
#ifdef HAVE_GTK_LAYER_SHELL
	if (!gtk_layer_is_supported())
#endif
	{
		gtk_window_set_modal(m_window, true);
	}
	gtk_window_set_decorated(m_window, false);
	gtk_window_set_skip_taskbar_hint(m_window, true);
	gtk_window_set_skip_pager_hint(m_window, true);
	gtk_window_set_type_hint(m_window, GDK_WINDOW_TYPE_HINT_MENU);
	gtk_window_stick(m_window);
	gtk_widget_add_events(GTK_WIDGET(m_window), GDK_FOCUS_CHANGE_MASK | GDK_STRUCTURE_MASK);

#ifdef HAVE_GTK_LAYER_SHELL
	if (gtk_layer_is_supported())
	{
		gtk_layer_init_for_window(m_window);

		// Position from top left, and excludes other windows
		gtk_layer_set_exclusive_zone(m_window, -1);
		gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_TOP, true);
		gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_BOTTOM, false);
		gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_LEFT, true);
		gtk_layer_set_anchor(m_window, GTK_LAYER_SHELL_EDGE_RIGHT, false);

		// Grab keyboard focus when shown
		gtk_layer_set_keyboard_mode(m_window, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

		// Position menu above other windows
		gtk_layer_set_layer(m_window, GTK_LAYER_SHELL_LAYER_OVERLAY);
	}
#endif

	connect(m_window, "enter-notify-event",
		[this](GtkWidget*, GdkEvent*) -> gboolean
		{
			m_child_has_focus = false;
			return GDK_EVENT_PROPAGATE;
		});

	connect(m_window, "focus-in-event",
		[this](GtkWidget*, GdkEvent*) -> gboolean
		{
			m_child_has_focus = false;
			return GDK_EVENT_PROPAGATE;
		});

	connect(m_window, "focus-out-event",
		[this](GtkWidget* widget, GdkEvent*) -> gboolean
		{
			if (wm_settings->stay_on_focus_out || m_child_has_focus || !gtk_widget_get_visible(widget))
			{
				return GDK_EVENT_PROPAGATE;
			}

			// Needed to make focus out event happen after button press event,
			// otherwise it is impossible to toggle panel button.
			g_idle_add(
				+[](gpointer user_data) -> gboolean
				{
					static_cast<Window*>(user_data)->hide(true);
					return G_SOURCE_REMOVE;
				},
			this);

			return GDK_EVENT_PROPAGATE;
		});

	connect(m_window, "key-press-event",
		[this](GtkWidget* widget, GdkEvent* event) -> gboolean
		{
			return on_key_press_event(widget, reinterpret_cast<GdkEventKey*>(event));
		});

	connect(m_window, "key-press-event",
		[this](GtkWidget* widget, GdkEvent* event) -> gboolean
		{
			return on_key_press_event_after(widget, reinterpret_cast<GdkEventKey*>(event));
		},
		Connect::After);

	connect(m_window, "map-event",
		[this](GtkWidget*, GdkEvent*) -> gboolean
		{
			return on_map_event();
		});

	connect(m_window, "state-flags-changed",
		[this](GtkWidget* widget, GtkStateFlags)
		{
			on_state_flags_changed(widget);
		});

	g_signal_connect(G_OBJECT(m_window), "delete-event", G_CALLBACK(&gtk_widget_hide_on_delete), nullptr);

	// Create the border of the window
	GtkWidget* frame = gtk_frame_new(nullptr);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(m_window), frame);

	// Create window contents stack
	m_window_stack = GTK_STACK(gtk_stack_new());
	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(m_window_stack));

	// Create loading message
	m_window_load_spinner = GTK_SPINNER(gtk_spinner_new());
	gtk_widget_set_halign(GTK_WIDGET(m_window_load_spinner), GTK_ALIGN_CENTER);
	gtk_widget_set_valign(GTK_WIDGET(m_window_load_spinner), GTK_ALIGN_CENTER);
	gtk_stack_add_named(m_window_stack, GTK_WIDGET(m_window_load_spinner), "load");

	// Create the profile picture and username label
	m_profile = new Profile(this);

	// Create action buttons
	for (int i = 0; i < 9; ++i)
	{
		m_commands_button[i] = wm_settings->command[i]->get_button();
		m_command_slots[i] = connect(m_commands_button[i], "clicked",
			[this](GtkButton*)
			{
				hide();
			});
	}

	// Create search entry
	m_search_entry = GTK_ENTRY(gtk_search_entry_new());
	gtk_window_set_focus(m_window, GTK_WIDGET(m_search_entry));

	connect(m_search_entry, "changed",
		[this](GtkEditable*)
		{
			search();
		});

	connect(m_search_entry, "populate-popup",
		[this](GtkEntry*, GtkWidget*)
		{
			set_child_has_focus();
		});

	// Create favorites
	m_favorites = new FavoritesPage(this);

	CategoryButton* favorites_button = m_favorites->get_button();
	connect(favorites_button->get_widget(), "toggled",
		[this](GtkToggleButton*)
		{
			favorites_toggled();
		});

	// Create recent
	m_recent = new RecentPage(this);

	CategoryButton* recent_button = m_recent->get_button();
	recent_button->join_group(favorites_button);
	connect(recent_button->get_widget(), "toggled",
		[this](GtkToggleButton*)
		{
			recent_toggled();
		});

	// Create applications
	m_applications = new ApplicationsPage(this);

	CategoryButton* applications_button = m_applications->get_button();
	applications_button->join_group(recent_button);
	connect(applications_button->get_widget(), "toggled",
		[this](GtkToggleButton*)
		{
			category_toggled();
		});

	// Create search results
	m_search_results = new SearchPage(this);

	// Create box for packing children
	m_vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
	gtk_container_set_border_width(GTK_CONTAINER(m_vbox), 6);
	gtk_stack_add_named(m_window_stack, GTK_WIDGET(m_vbox), "contents");

	// Create box for packing commands
	m_commands_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
	m_commands_spacer = gtk_label_new(nullptr);
	gtk_box_pack_start(m_commands_box, m_commands_spacer, true, true, 0);
	for (auto command : m_commands_button)
	{
		gtk_box_pack_start(m_commands_box, command, false, false, 0);
	}

	// Create box for packing username and commands
	m_title_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6));
	gtk_box_pack_start(m_vbox, GTK_WIDGET(m_title_box), false, false, 0);
	gtk_box_pack_start(m_title_box, m_profile->get_picture(), false, false, 0);
	gtk_box_pack_start(m_title_box, m_profile->get_username(), true, true, 0);
	gtk_box_pack_start(m_title_box, GTK_WIDGET(m_commands_box), false, false, 0);

	// Add search to layout
	m_search_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6));
	gtk_box_pack_start(m_vbox, GTK_WIDGET(m_search_box), false, true, 0);
	gtk_box_pack_start(m_search_box, GTK_WIDGET(m_search_entry), true, true, 0);

	// Create box for packing launcher pages and sidebar
	m_contents_stack = GTK_STACK(gtk_stack_new());
	m_contents_box = GTK_GRID(gtk_grid_new());
	gtk_grid_set_column_spacing(m_contents_box, 6);
	gtk_grid_set_row_spacing(m_contents_box, 0);
	gtk_stack_add_named(m_contents_stack, GTK_WIDGET(m_contents_box), "contents");
	gtk_stack_add_named(m_contents_stack, m_search_results->get_widget(), "search");
	gtk_box_pack_start(m_vbox, GTK_WIDGET(m_contents_stack), true, true, 0);

	// Create box for packing categories horizontally
	m_categories_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
	gtk_grid_attach(m_contents_box, GTK_WIDGET(m_categories_box), 0, 0, 2, 1);

	// Create box for packing launcher pages
	m_panels_stack = GTK_STACK(gtk_stack_new());
	gtk_grid_attach(m_contents_box, GTK_WIDGET(m_panels_stack), 0, 1, 1, 1);
	gtk_widget_set_hexpand(GTK_WIDGET(m_panels_stack), true);
	gtk_widget_set_vexpand(GTK_WIDGET(m_panels_stack), true);
	gtk_stack_add_named(m_panels_stack, m_favorites->get_widget(), "favorites");
	gtk_stack_add_named(m_panels_stack, m_recent->get_widget(), "recent");
	gtk_stack_add_named(m_panels_stack, m_applications->get_widget(), "applications");

	// Create box for packing sidebar
	m_category_buttons = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_pack_start(m_category_buttons, favorites_button->get_widget(), false, false, 0);
	gtk_box_pack_start(m_category_buttons, recent_button->get_widget(), false, false, 0);
	gtk_box_pack_start(m_category_buttons, applications_button->get_widget(), false, false, 0);
	gtk_box_pack_start(m_category_buttons, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), false, false, 4);

	m_sidebar = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
	gtk_grid_attach(m_contents_box, GTK_WIDGET(m_sidebar), 1, 1, 1, 1);
	gtk_scrolled_window_set_propagate_natural_height(m_sidebar, true);
	gtk_scrolled_window_set_shadow_type(m_sidebar, GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy(m_sidebar, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(m_sidebar), GTK_WIDGET(m_category_buttons));

	// Handle default page
	reset_default_button();

	// Add CSS classes
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_window)), "whiskermenu");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_search_box)), "search-area");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_title_box)), "title-area");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_commands_box)), "commands-area");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_contents_stack)), "contents");

	GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(m_category_buttons));
	gtk_style_context_add_class(context, "categories");
	gtk_style_context_add_class(context, "right");

	// Show widgets
	gtk_widget_show_all(frame);
	m_default_button->set_active(true);

	// Handle transparency
	gtk_widget_set_app_paintable(GTK_WIDGET(m_window), true);

	connect(m_window, "draw",
		[this](GtkWidget* widget, cairo_t* cr) -> gboolean
		{
			return on_draw_event(widget, cr);
		});

	connect(m_window, "screen-changed",
		[this](GtkWidget* widget, GdkScreen*)
		{
			on_screen_changed(widget);
		});
	on_screen_changed(GTK_WIDGET(m_window));

	// Load applications
	m_applications->load();

	g_object_ref_sink(m_window);
}

//-----------------------------------------------------------------------------

WhiskerMenu::Window::~Window()
{
	for (int i = 0; i < 9; ++i)
	{
		g_signal_handler_disconnect(m_commands_button[i], m_command_slots[i]);
		gtk_container_remove(GTK_CONTAINER(m_commands_box), m_commands_button[i]);
	}

	delete m_applications;
	delete m_search_results;
	delete m_recent;
	delete m_favorites;

	delete m_profile;

	gtk_widget_destroy(GTK_WIDGET(m_window));
	g_object_unref(m_window);
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::hide(bool lost_focus)
{
	// Save settings
	wm_settings->favorites.save();
	wm_settings->recent.save();

	// Scroll categories to top
	GtkAdjustment* adjustment = gtk_scrolled_window_get_vadjustment(m_sidebar);
	gtk_adjustment_set_value(adjustment, gtk_adjustment_get_lower(adjustment));

	// Hide command buttons to remove active border
	for (auto command : m_commands_button)
	{
		gtk_widget_set_visible(command, false);
	}

	// Hide window
	gtk_widget_hide(GTK_WIDGET(m_window));

	// Switch back to default page
	show_default_page();

	// Inform plugin that window is hidden
	if (!lost_focus)
	{
		m_plugin->menu_hidden();
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::show(const Position position)
{
	// Handle switching view types
	m_search_results->update_view();
	m_favorites->update_view();
	m_recent->update_view();
	m_applications->update_view();

	// Handle showing tooltips
	if (wm_settings->launcher_show_tooltip)
	{
		m_search_results->get_view()->show_tooltips();
		m_favorites->get_view()->show_tooltips();
		m_recent->get_view()->show_tooltips();
		m_applications->get_view()->show_tooltips();
	}
	else
	{
		m_search_results->get_view()->hide_tooltips();
		m_favorites->get_view()->hide_tooltips();
		m_recent->get_view()->hide_tooltips();
		m_applications->get_view()->hide_tooltips();
	}
	m_profile->reset_tooltip();

	// Make sure commands are valid and visible
	for (auto command : wm_settings->command)
	{
		command->check();
	}

	// Make sure recent item count is within max
	m_recent->enforce_item_count();

	// Make sure recent button is only visible when tracked
	gtk_widget_set_visible(m_recent->get_button()->get_widget(), wm_settings->recent_items_max);

	// Make sure applications list is current; does nothing unless list has changed
	if (m_applications->load())
	{
		set_loaded();
	}
	else
	{
		m_plugin->set_loaded(false);
		gtk_stack_set_visible_child_name(m_window_stack, "load");
		gtk_spinner_start(m_window_load_spinner);
	}

	// Update default page
	reset_default_button();
	show_default_page();

	// Clear any previous selection
	m_favorites->reset_selection();
	m_recent->reset_selection();
	m_applications->reset_selection();

	// Make sure icon sizes are correct
	m_favorites->get_button()->reload_icon_size();
	m_recent->get_button()->reload_icon_size();
	m_applications->get_button()->reload_icon_size();

	m_applications->reload_category_icon_size();

	m_search_results->get_view()->reload_icon_size();
	m_favorites->get_view()->reload_icon_size();
	m_recent->get_view()->reload_icon_size();
	m_applications->get_view()->reload_icon_size();

	if (position == PositionAtButton)
	{
		// Wait up to half a second for auto-hidden panels to be shown
		int parent_x = 0, parent_y = 0;
		clock_t end = clock() + (CLOCKS_PER_SEC / 2);
		GtkWidget* parent = m_plugin->get_button();
		GtkWindow* parent_window = GTK_WINDOW(gtk_widget_get_toplevel(parent));
		gtk_window_get_position(parent_window, &parent_x, &parent_y);
		while ((parent_x == -9999) && (parent_y == -9999) && (clock() < end))
		{
			while (gtk_events_pending())
			{
				gtk_main_iteration();
			}
			gtk_window_get_position(parent_window, &parent_x, &parent_y);
		}

		// Fetch position
		m_plugin->get_menu_position(&m_geometry.x, &m_geometry.y);
	}
	else
	{
		// Fetch cursor position
		GdkDisplay* display = gdk_display_get_default();
		GdkSeat* seat = gdk_display_get_default_seat(display);
		GdkDevice* device = gdk_seat_get_pointer(seat);
		gdk_device_get_position(device, nullptr, &m_geometry.x, &m_geometry.y);
	}

	// Fetch screen geomtry
	GdkRectangle monitor;
	GdkMonitor* monitor_gdk = gdk_display_get_monitor_at_point(gdk_display_get_default(), m_geometry.x, m_geometry.y);
	gdk_monitor_get_geometry(monitor_gdk, &monitor);

	// Resize window if necessary, and also prevent it from being larger than screen
	const int width = std::min(static_cast<int>(wm_settings->menu_width), monitor.width);
	const int height = std::min(static_cast<int>(wm_settings->menu_height), monitor.height);
	bool resized = false;
	if ((m_geometry.width != width) || (m_geometry.height != height))
	{
		m_geometry.width = width;
		m_geometry.height = height;
		gtk_widget_set_size_request(GTK_WIDGET(m_window), m_geometry.width, m_geometry.height);
		gtk_window_resize(m_window, 1, 1);
		resized = true;
	}

	// Center window if requested
	if (position == PositionAtCenter)
	{
		m_geometry.x = (monitor.width - m_geometry.width) / 2;
		m_geometry.y = (monitor.height - m_geometry.height) / 2;
	}

	// Move window
	move_window(monitor);

	// Relayout window if necessary
	const bool layout_ltr = gtk_widget_get_default_direction() != GTK_TEXT_DIR_RTL;
	if ((m_layout_ltr != layout_ltr)
			|| (m_layout_categories_horizontal != wm_settings->position_categories_horizontal)
			|| (m_layout_categories_alternate != wm_settings->position_categories_alternate)
			|| (m_layout_search_alternate != wm_settings->position_search_alternate)
			|| (m_layout_commands_alternate != wm_settings->position_commands_alternate)
			|| (m_layout_profile_alternate != wm_settings->position_profile_alternate)
			|| (m_profile_shape != wm_settings->profile_shape))
	{
		m_layout_ltr = layout_ltr;
		m_layout_categories_horizontal = wm_settings->position_categories_horizontal;
		m_layout_categories_alternate = wm_settings->position_categories_alternate;
		m_layout_search_alternate = wm_settings->position_search_alternate;
		m_layout_commands_alternate = wm_settings->position_commands_alternate;
		m_layout_profile_alternate = wm_settings->position_profile_alternate;
		m_profile->update_picture();
		m_profile_shape = wm_settings->profile_shape;
		update_layout();
	}

	// Show window
	gtk_window_present(m_window);

	if (resized)
	{
		check_scrollbar_needed();
	}

	// Fetch position again to make sure window does not overlap panel
	if (position == PositionAtButton)
	{
		m_plugin->get_menu_position(&m_geometry.x, &m_geometry.y);
	}

	// Move window
	move_window(monitor);
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::set_child_has_focus()
{
	m_child_has_focus = true;
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::set_categories(const std::vector<CategoryButton*>& categories)
{
	CategoryButton* last_button = m_applications->get_button();
	for (auto button : categories)
	{
		button->join_group(last_button);
		last_button = button;
		gtk_box_pack_start(m_category_buttons, button->get_widget(), false, false, 0);
		connect(button->get_widget(), "toggled",
			[this](GtkToggleButton*)
			{
				category_toggled();
			});
	}

	show_default_page();
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::set_items()
{
	m_search_results->set_menu_items();
	m_favorites->set_menu_items();
	m_recent->set_menu_items();

	// Handle switching to favorites are added
	connect(m_favorites->get_view()->get_model(), "row-inserted",
		[this](GtkTreeModel*, GtkTreePath*, GtkTreeIter*)
		{
			show_favorites();
		});
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::set_loaded()
{
	// Hide loading spinner
	gtk_spinner_stop(m_window_load_spinner);
	gtk_stack_set_visible_child_name(m_window_stack, "contents");

	// Focus search entry
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));

	// Show panel button
	m_plugin->set_loaded(true);

	// Check in case of plugin reload
	check_scrollbar_needed();
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::unset_items()
{
	m_search_results->unset_menu_items();
	m_favorites->unset_menu_items();
	m_recent->unset_menu_items();
}

//-----------------------------------------------------------------------------

gboolean WhiskerMenu::Window::on_key_press_event(GtkWidget* widget, GdkEventKey* key_event)
{
	if (key_event->keyval == GDK_KEY_Escape)
	{
		// Hide if escape is pressed and there is no text in search entry
		if (xfce_str_is_empty(gtk_entry_get_text(m_search_entry)))
		{
			hide();
		}
		// Clear search entry of text if escape is pressed
		else
		{
			gtk_entry_set_text(m_search_entry, "");
		}
		return GDK_EVENT_STOP;
	}

	Page* page = nullptr;
	if (gtk_stack_get_visible_child(m_contents_stack) == m_search_results->get_widget())
	{
		page = m_search_results;
	}
	else if (m_favorites->get_button()->get_active())
	{
		page = m_favorites;
	}
	else if (m_recent->get_button()->get_active())
	{
		page = m_recent;
	}
	else
	{
		page = m_applications;
	}
	GtkWidget* view = page->get_view()->get_widget();
	GtkWidget* search = GTK_WIDGET(m_search_entry);

	if ((key_event->keyval == GDK_KEY_Left) || (key_event->keyval == GDK_KEY_Right))
	{
		// Allow keyboard navigation out of treeview
		if (GTK_IS_TREE_VIEW(view) && ((widget == view) || (gtk_window_get_focus(m_window) == view)))
		{
			gtk_widget_grab_focus(m_default_button->get_widget());
			page->reset_selection();
		}
		// Allow keyboard navigation out of search into iconview
		else if (GTK_IS_ICON_VIEW(view) && ((widget == search) || (gtk_window_get_focus(m_window) == search)))
		{
			const auto length = gtk_entry_get_text_length(m_search_entry);
			const bool at_end = length && (gtk_editable_get_position(GTK_EDITABLE(m_search_entry)) == length);
			const bool move_next = (gtk_widget_get_default_direction() != GTK_TEXT_DIR_RTL)
					? (key_event->keyval == GDK_KEY_Right) : (key_event->keyval == GDK_KEY_Left);
			if (at_end && move_next)
			{
				gtk_widget_grab_focus(view);
			}
		}
	}

	// Make up and down keys scroll current list of applications from search
	if ((key_event->keyval == GDK_KEY_Up) || (key_event->keyval == GDK_KEY_Down))
	{
		// Determine if there is a selected item
		bool reset = page != m_search_results;
		if (reset)
		{
			GtkTreePath* path = page->get_view()->get_selected_path();
			if (path)
			{
				reset = false;
				gtk_tree_path_free(path);
			}
		}
		// Allow keyboard navigation out of search into view
		if ((widget == search) || (gtk_window_get_focus(m_window) == search))
		{
			gtk_widget_grab_focus(view);
		}
		// Only select first item if there is no selected item
		if ((gtk_window_get_focus(m_window) == view) && reset)
		{
			page->select_first();
			return GDK_EVENT_STOP;
		}
	}

	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

gboolean WhiskerMenu::Window::on_key_press_event_after(GtkWidget* widget, GdkEventKey* key_event)
{
	// Pass unhandled key presses to search entry
	GtkWidget* search_entry = GTK_WIDGET(m_search_entry);
	if ((widget != search_entry) && (gtk_window_get_focus(m_window) != search_entry))
	{
		if (key_event->is_modifier)
		{
			return GDK_EVENT_PROPAGATE;
		}
		gtk_widget_grab_focus(search_entry);
		gtk_window_propagate_key_event(m_window, key_event);
		return GDK_EVENT_STOP;
	}
	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

gboolean WhiskerMenu::Window::on_map_event()
{
	gtk_window_set_keep_above(m_window, true);

	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::on_state_flags_changed(GtkWidget* widget)
{
	// Refocus and raise window if visible
	if (gtk_widget_get_visible(widget))
	{
		gtk_window_present(m_window);
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::on_screen_changed(GtkWidget* widget)
{
	GdkScreen* screen = gtk_widget_get_screen(widget);
	GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
	if (!visual || (wm_settings->menu_opacity == 100))
	{
		visual = gdk_screen_get_system_visual(screen);
		m_supports_alpha = false;
	}
	else
	{
		m_supports_alpha = true;
	}
	gtk_widget_set_visual(widget, visual);
}

//-----------------------------------------------------------------------------

gboolean WhiskerMenu::Window::on_draw_event(GtkWidget* widget, cairo_t* cr)
{
	if (!gtk_widget_get_realized(widget))
	{
		gtk_widget_realize(widget);
	}

	GtkStyleContext* context = gtk_widget_get_style_context(widget);
	const double width = gtk_widget_get_allocated_width(widget);
	const double height = gtk_widget_get_allocated_height(widget);

	GdkScreen* screen = gtk_widget_get_screen(widget);
	const bool enabled = gdk_screen_is_composited(screen);

	if (enabled && m_supports_alpha)
	{
		cairo_surface_t* background = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
		cairo_t* cr_background = cairo_create(background);
		cairo_set_operator(cr_background, CAIRO_OPERATOR_SOURCE);
		gtk_render_background(context, cr_background, 0.0, 0.0, width, height);
		cairo_destroy(cr_background);

		cairo_set_source_surface(cr, background, 0.0, 0.0);
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		cairo_paint_with_alpha(cr, wm_settings->menu_opacity / 100.0);

		cairo_surface_destroy(background);
	}
	else
	{
		gtk_render_background(context, cr, 0.0, 0.0, width, height);
	}

	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::check_scrollbar_needed()
{
	// Find height of sidebar and buttons
	int buttons_height = 0;
	gtk_widget_get_preferred_height(GTK_WIDGET(m_category_buttons), nullptr, &buttons_height);

	int sidebar_height = 0;
	gtk_widget_get_preferred_height(GTK_WIDGET(m_sidebar), nullptr, &sidebar_height);

	// Always show scrollbar if sidebar is shorter than buttons
	if (sidebar_height >= buttons_height)
	{
		gtk_scrolled_window_set_policy(m_sidebar, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	}
	else
	{
		gtk_scrolled_window_set_policy(m_sidebar, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::favorites_toggled()
{
	m_favorites->reset_selection();
	gtk_stack_set_visible_child_name(m_panels_stack, "favorites");
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::recent_toggled()
{
	m_recent->reset_selection();
	gtk_stack_set_visible_child_name(m_panels_stack, "recent");
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::category_toggled()
{
	m_applications->reset_selection();
	gtk_stack_set_visible_child_name(m_panels_stack, "applications");
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::move_window(const GdkRectangle& monitor)
{
	// Prevent window from leaving screen
	m_geometry.x = CLAMP(m_geometry.x, monitor.x, monitor.x + monitor.width - m_geometry.width);
	m_geometry.y = CLAMP(m_geometry.y, monitor.y, monitor.y + monitor.height - m_geometry.height);

	// Move window
#ifdef HAVE_GTK_LAYER_SHELL
	if (gtk_layer_is_supported())
	{
		gtk_layer_set_margin(m_window, GTK_LAYER_SHELL_EDGE_LEFT, m_geometry.x);
		gtk_layer_set_margin(m_window, GTK_LAYER_SHELL_EDGE_TOP, m_geometry.y);
	}
	else
#endif
	{
		gtk_window_move(m_window, m_geometry.x, m_geometry.y);
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::reset_default_button()
{
	switch (wm_settings->default_category)
	{
	case Settings::CategoryRecent:
		m_default_button = m_recent->get_button();
		gtk_box_reorder_child(m_category_buttons, m_recent->get_button()->get_widget(), 0);
		gtk_box_reorder_child(m_category_buttons, m_favorites->get_button()->get_widget(), 1);
		gtk_box_reorder_child(m_category_buttons, m_applications->get_button()->get_widget(), 2);
		break;

	case Settings::CategoryAll:
		m_default_button = m_applications->get_button();
		gtk_box_reorder_child(m_category_buttons, m_applications->get_button()->get_widget(), 0);
		gtk_box_reorder_child(m_category_buttons, m_favorites->get_button()->get_widget(), 1);
		gtk_box_reorder_child(m_category_buttons, m_recent->get_button()->get_widget(), 2);
		break;

	default:
		m_default_button = m_favorites->get_button();
		gtk_box_reorder_child(m_category_buttons, m_favorites->get_button()->get_widget(), 0);
		gtk_box_reorder_child(m_category_buttons, m_recent->get_button()->get_widget(), 1);
		gtk_box_reorder_child(m_category_buttons, m_applications->get_button()->get_widget(), 2);
		break;
	}
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::show_favorites()
{
	// Switch to favorites panel
	m_favorites->get_button()->set_active(true);

	// Clear search entry
	gtk_entry_set_text(m_search_entry, "");
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::show_default_page()
{
	// Switch to favorites panel
	m_default_button->set_active(true);

	// Clear search entry
	gtk_entry_set_text(m_search_entry, "");
	gtk_widget_grab_focus(GTK_WIDGET(m_search_entry));
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::search()
{
	// Fetch search string
	const gchar* text = gtk_entry_get_text(m_search_entry);
	if (xfce_str_is_empty(text))
	{
		text = nullptr;
	}

	if (text)
	{
		// Show search results
		gtk_stack_set_visible_child_name(m_contents_stack, "search");
	}
	else
	{
		// Show active panel
		gtk_stack_set_visible_child_name(m_contents_stack, "contents");
	}

	// Apply filter
	m_search_results->set_filter(text);
}

//-----------------------------------------------------------------------------

void WhiskerMenu::Window::update_layout()
{
	// Set vertical position of commands
	g_object_ref(m_commands_box);
	gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(GTK_WIDGET(m_commands_box))), GTK_WIDGET(m_commands_box));
	if (m_layout_commands_alternate)
	{
		gtk_box_pack_start(m_search_box, GTK_WIDGET(m_commands_box), false, false, 0);

		if (!m_layout_categories_horizontal)
		{
			if (m_layout_ltr == m_layout_categories_alternate)
			{
				gtk_box_reorder_child(m_search_box, GTK_WIDGET(m_commands_box), 0);
				gtk_box_reorder_child(m_search_box, GTK_WIDGET(m_search_entry), 1);
			}
		}
		else
		{
			if (!m_layout_ltr)
			{
				gtk_box_reorder_child(m_search_box, GTK_WIDGET(m_commands_box), 0);
				gtk_box_reorder_child(m_search_box, GTK_WIDGET(m_search_entry), 1);
			}
		}
	}
	else
	{
		gtk_box_pack_start(m_title_box, GTK_WIDGET(m_commands_box), false, false, 0);
	}
	g_object_unref(m_commands_box);

	// Set horizontal position of categories
	g_object_ref(m_category_buttons);
	if (m_layout_categories_horizontal)
	{
		if (gtk_orientable_get_orientation(GTK_ORIENTABLE(m_category_buttons)) == GTK_ORIENTATION_VERTICAL)
		{
			gtk_orientable_set_orientation(GTK_ORIENTABLE(m_category_buttons), GTK_ORIENTATION_HORIZONTAL);
			gtk_container_remove(GTK_CONTAINER(m_sidebar), GTK_WIDGET(m_category_buttons));
			gtk_widget_set_visible(GTK_WIDGET(m_sidebar), false);
			gtk_widget_set_visible(GTK_WIDGET(m_categories_box), true);
			gtk_box_set_center_widget(m_categories_box, GTK_WIDGET(m_category_buttons));
		}
	}
	else
	{
		if (gtk_orientable_get_orientation(GTK_ORIENTABLE(m_category_buttons)) == GTK_ORIENTATION_HORIZONTAL)
		{
			gtk_orientable_set_orientation(GTK_ORIENTABLE(m_category_buttons), GTK_ORIENTATION_VERTICAL);
			gtk_container_remove(GTK_CONTAINER(m_categories_box), GTK_WIDGET(m_category_buttons));
			gtk_widget_set_visible(GTK_WIDGET(m_categories_box), false);
			gtk_widget_set_visible(GTK_WIDGET(m_sidebar), true);
			gtk_container_add(GTK_CONTAINER(m_sidebar), GTK_WIDGET(m_category_buttons));
		}
	}
	g_object_unref(m_category_buttons);

	// Handle showing username and profile
	if (m_profile_shape != Settings::ProfileHidden)
	{
		gtk_widget_set_visible(m_profile->get_picture(), true);
		gtk_widget_set_visible(m_profile->get_username(), true);
		gtk_widget_set_visible(GTK_WIDGET(m_title_box), true);
	}
	else
	{
		gtk_widget_set_visible(m_profile->get_picture(), false);
		gtk_widget_set_visible(m_profile->get_username(), false);
		gtk_widget_set_visible(GTK_WIDGET(m_title_box), !m_layout_categories_alternate);
	}

	// Arrange horizontal order of profile picture, username, and commands
	if (m_layout_ltr && m_layout_commands_alternate)
	{
		gtk_widget_set_halign(m_profile->get_username(), GTK_ALIGN_START);

		gtk_box_reorder_child(m_title_box, m_profile->get_picture(), 0);
		gtk_box_reorder_child(m_title_box, m_profile->get_username(), 1);

		for (int i = 0; i < 9; ++i)
		{
			gtk_box_reorder_child(m_commands_box, m_commands_button[i], i);
		}
	}
	else if (m_layout_commands_alternate)
	{
		gtk_widget_set_halign(m_profile->get_username(), GTK_ALIGN_END);

		gtk_box_reorder_child(m_title_box, m_profile->get_picture(), 1);
		gtk_box_reorder_child(m_title_box, m_profile->get_username(), 0);

		for (int i = 0; i < 9; ++i)
		{
			gtk_box_reorder_child(m_commands_box, m_commands_button[i], 8 - i);
		}
	}
	else if (m_layout_ltr)
	{
		gtk_widget_set_halign(m_profile->get_username(), GTK_ALIGN_START);

		gtk_box_reorder_child(m_title_box, m_profile->get_picture(), 0);
		gtk_box_reorder_child(m_title_box, m_profile->get_username(), 1);
		gtk_box_reorder_child(m_title_box, GTK_WIDGET(m_commands_box), 2);

		for (int i = 0; i < 9; ++i)
		{
			gtk_box_reorder_child(m_commands_box, m_commands_button[i], i);
		}
	}
	else
	{
		gtk_widget_set_halign(m_profile->get_username(), GTK_ALIGN_END);

		gtk_box_reorder_child(m_title_box, m_profile->get_picture(), 2);
		gtk_box_reorder_child(m_title_box, m_profile->get_username(), 1);
		gtk_box_reorder_child(m_title_box, GTK_WIDGET(m_commands_box), 0);

		for (int i = 0; i < 9; ++i)
		{
			gtk_box_reorder_child(m_commands_box, m_commands_button[i], 8 - i);
		}
	}

	// Arrange horizontal order of applications and sidebar
	g_object_ref(m_categories_box);
	g_object_ref(m_panels_stack);
	g_object_ref(m_sidebar);

	GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(m_category_buttons));
	if (gtk_style_context_has_class(context, "left"))
	{
		gtk_style_context_remove_class(context, "left");
	}
	else if (gtk_style_context_has_class(context, "right"))
	{
		gtk_style_context_remove_class(context, "right");
	}
	else if (gtk_style_context_has_class(context, "top"))
	{
		gtk_style_context_remove_class(context, "top");
	}
	else if (gtk_style_context_has_class(context, "bottom"))
	{
		gtk_style_context_remove_class(context, "bottom");
	}

	gtk_grid_remove_row(m_contents_box, 1);
	gtk_grid_remove_row(m_contents_box, 0);
	if (m_layout_categories_horizontal)
	{
		gtk_grid_set_column_spacing(m_contents_box, 0);
		gtk_grid_set_row_spacing(m_contents_box, 6);

		gtk_style_context_add_class(context, m_layout_categories_alternate ? "bottom" : "top");
	}
	else
	{
		gtk_grid_set_column_spacing(m_contents_box, 6);
		gtk_grid_set_row_spacing(m_contents_box, 0);

		gtk_style_context_add_class(context, (m_layout_ltr == m_layout_categories_alternate) ? "left" : "right");
	}

	if (m_layout_ltr != m_layout_categories_alternate)
	{
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_panels_stack), 0, 0, 1, 1);
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_sidebar), 1, 0, 1, 1);

		gtk_box_reorder_child(m_commands_box, m_commands_spacer, 0);
	}
	else
	{
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_sidebar), 0, 0, 1, 1);
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_panels_stack), 1, 0, 1, 1);

		gtk_box_reorder_child(m_commands_box, m_commands_spacer, 9);
	}

	if (!m_layout_categories_alternate)
	{
		gtk_grid_insert_row(m_contents_box, 0);
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_categories_box), 0, 0, 2, 1);
	}
	else
	{
		gtk_grid_attach(m_contents_box, GTK_WIDGET(m_categories_box), 0, 1, 2, 1);
	}

	g_object_unref(m_sidebar);
	g_object_unref(m_panels_stack);
	g_object_unref(m_categories_box);

	// Arrange vertical order of header, applications, and search
	if (m_layout_search_alternate && m_layout_profile_alternate)
	{
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_contents_stack), 0);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_search_box), 1);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_title_box), 2);
	}
	else if (m_layout_profile_alternate)
	{
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_search_box), 0);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_contents_stack), 1);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_title_box), 2);
	}
	else if (m_layout_search_alternate)
	{
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_title_box), 0);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_contents_stack), 1);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_search_box), 2);
	}
	else
	{
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_title_box), 0);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_search_box), 1);
		gtk_box_reorder_child(m_vbox, GTK_WIDGET(m_contents_stack), 2);
	}

	// Handle size group to category buttons
	const bool category_show_name = wm_settings->category_show_name && !wm_settings->position_categories_horizontal;
	if (!m_sidebar_size_group && m_layout_commands_alternate && category_show_name)
	{
		m_sidebar_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
		gtk_size_group_add_widget(m_sidebar_size_group, GTK_WIDGET(m_sidebar));
		gtk_size_group_add_widget(m_sidebar_size_group, GTK_WIDGET(m_commands_box));
	}
	else if (m_sidebar_size_group && (!m_layout_commands_alternate || !category_show_name))
	{
		gtk_size_group_remove_widget(m_sidebar_size_group, GTK_WIDGET(m_sidebar));
		gtk_size_group_remove_widget(m_sidebar_size_group, GTK_WIDGET(m_commands_box));
		g_object_unref(m_sidebar_size_group);
		m_sidebar_size_group = nullptr;
	}
}

//-----------------------------------------------------------------------------
