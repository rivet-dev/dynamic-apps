/*
 * Copyright (C) 2019-2021 Graeme Gott <graeme@gottcode.org>
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

#include "launcher-icon-view.h"

#include "icon-renderer.h"
#include "settings.h"
#include "slot.h"

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

LauncherIconView::LauncherIconView() :
	m_icon_renderer(nullptr),
	m_icon_size(-1)
{
	// Create the view
	m_view = GTK_ICON_VIEW(gtk_icon_view_new());

	m_icon_renderer = whiskermenu_icon_renderer_new();
	g_object_set(m_icon_renderer,
			"stretch", true,
			"xalign", 0.5,
			"yalign", 1.0,
			nullptr);
	GtkCellLayout* cell_layout = GTK_CELL_LAYOUT(m_view);
	gtk_cell_layout_pack_start(cell_layout, m_icon_renderer, false);
	gtk_cell_layout_set_attributes(cell_layout, m_icon_renderer, "gicon", COLUMN_ICON, "launcher", COLUMN_LAUNCHER, nullptr);

	gtk_icon_view_set_markup_column(m_view, COLUMN_TEXT);

	reload_icon_size();

	// Use single clicks to activate items
	gtk_icon_view_set_activate_on_single_click(m_view, true);

	// Only allow up to one selected item
	gtk_icon_view_set_selection_mode(m_view, GTK_SELECTION_SINGLE);

	g_object_ref_sink(m_view);

	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_view)), "launchers");

	// Handle hover selection
	gtk_widget_add_events(GTK_WIDGET(m_view), GDK_SCROLL_MASK);

	connect(m_view, "leave-notify-event",
		[this](GtkWidget*, GdkEvent*) -> gboolean
		{
			clear_selection();
			return GDK_EVENT_PROPAGATE;
		});

	connect(m_view, "motion-notify-event",
		[this](GtkWidget*, GdkEvent* event) -> gboolean
		{
			GdkEventMotion* motion_event = reinterpret_cast<GdkEventMotion*>(event);
			select_path_at_pos(motion_event->x, motion_event->y);
			return GDK_EVENT_PROPAGATE;
		});

	connect(m_view, "scroll-event",
		[this](GtkWidget*, GdkEvent* event) -> gboolean
		{
			GdkEventScroll* scroll_event = reinterpret_cast<GdkEventScroll*>(event);
			select_path_at_pos(scroll_event->x, scroll_event->y);
			return GDK_EVENT_PROPAGATE;
		});
}

//-----------------------------------------------------------------------------

LauncherIconView::~LauncherIconView()
{
	gtk_widget_destroy(GTK_WIDGET(m_view));
	g_object_unref(m_view);
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherIconView::get_cursor() const
{
	GtkTreePath* path = nullptr;
	gtk_icon_view_get_cursor(m_view, &path, nullptr);
	return path;
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherIconView::get_path_at_pos(int x, int y) const
{
	return gtk_icon_view_get_path_at_pos(m_view, x, y);
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherIconView::get_selected_path() const
{
	GtkTreePath* path = nullptr;
	GList* selection = gtk_icon_view_get_selected_items(m_view);
	if (selection)
	{
		path = gtk_tree_path_copy(static_cast<GtkTreePath*>(selection->data));
	}
	g_list_free_full(selection, reinterpret_cast<GDestroyNotify>(&gtk_tree_path_free));
	return path;
}

//-----------------------------------------------------------------------------

void LauncherIconView::activate_path(GtkTreePath* path)
{
	gtk_icon_view_item_activated(m_view, path);
}

//-----------------------------------------------------------------------------

void LauncherIconView::scroll_to_path(GtkTreePath* path)
{
	gtk_icon_view_scroll_to_path(m_view, path, true, 0.5f, 0.5f);
}

//-----------------------------------------------------------------------------

void LauncherIconView::select_path(GtkTreePath* path)
{
	gtk_icon_view_select_path(m_view, path);
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_cursor(GtkTreePath* path)
{
	gtk_icon_view_set_cursor(m_view,path, nullptr, false);
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_fixed_height_mode(bool)
{
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_selection_mode(GtkSelectionMode mode)
{
	gtk_icon_view_set_selection_mode(m_view, mode);
}

//-----------------------------------------------------------------------------

void LauncherIconView::hide_tooltips()
{
	gtk_icon_view_set_tooltip_column(m_view, -1);
}

//-----------------------------------------------------------------------------

void LauncherIconView::show_tooltips()
{
	gtk_icon_view_set_tooltip_column(m_view, COLUMN_TOOLTIP);
}

//-----------------------------------------------------------------------------

void LauncherIconView::clear_selection()
{
	gtk_icon_view_unselect_all(m_view);
}

//-----------------------------------------------------------------------------

void LauncherIconView::collapse_all()
{
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_model(GtkTreeModel* model)
{
	m_model = model;
	gtk_icon_view_set_model(m_view, model);
}

//-----------------------------------------------------------------------------

void LauncherIconView::unset_model()
{
	m_model = nullptr;
	gtk_icon_view_set_model(m_view, nullptr);
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_drag_source(GdkModifierType start_button_mask, const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions)
{
	gtk_icon_view_enable_model_drag_source(m_view, start_button_mask, targets, n_targets, actions);
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_drag_dest(const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions)
{
	gtk_icon_view_enable_model_drag_dest(m_view, targets, n_targets, actions);
}

//-----------------------------------------------------------------------------

void LauncherIconView::unset_drag_source()
{
	gtk_icon_view_unset_model_drag_source(m_view);
}

//-----------------------------------------------------------------------------

void LauncherIconView::unset_drag_dest()
{
	gtk_icon_view_unset_model_drag_dest(m_view);
}

//-----------------------------------------------------------------------------

void LauncherIconView::reload_icon_size()
{
	// Fetch icon size
	if (m_icon_size == wm_settings->launcher_icon_size.get_size())
	{
		return;
	}
	m_icon_size = wm_settings->launcher_icon_size.get_size();

	// Configure icon renderer
	if (m_icon_size > 1)
	{
		g_object_set(m_icon_renderer, "size", m_icon_size, "visible", true, nullptr);
	}
	else
	{
		g_object_set(m_icon_renderer, "visible", false, nullptr);
	}

	// Reset padding to fix icon clipping
	gtk_icon_view_set_item_padding(m_view, 0);

	// Adjust item size
	int padding = 2;
	switch (wm_settings->launcher_icon_size)
	{
	case IconSize::Smallest:
	case IconSize::Smaller:
		padding = 2;
		break;

	case IconSize::Small:
	case IconSize::Normal:
	case IconSize::Large:
		padding = 4;
		break;

	case IconSize::Larger:
	case IconSize::Largest:
		padding = 6;
		break;

	default:
		break;
	}
	gtk_icon_view_set_item_padding(m_view, padding);
}

//-----------------------------------------------------------------------------

void LauncherIconView::select_path_at_pos(int x, int y)
{
	GtkTreePath* path = get_path_at_pos(x, y);
	if (!path)
	{
		clear_selection();
	}
	else if (!gtk_icon_view_path_is_selected(m_view, path))
	{
		select_path(path);
	}
	gtk_tree_path_free(path);
}

//-----------------------------------------------------------------------------
