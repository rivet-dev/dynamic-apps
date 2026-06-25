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

#include "launcher-tree-view.h"

#include "category.h"
#include "icon-renderer.h"
#include "settings.h"
#include "slot.h"
#include "util.h"

#include <gdk/gdkkeysyms.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

static gboolean is_separator(GtkTreeModel* model, GtkTreeIter* iter, gpointer)
{
	const gchar* text;
	gtk_tree_model_get(model, iter, LauncherView::COLUMN_TEXT, &text, -1);
	return xfce_str_is_empty(text);
}

//-----------------------------------------------------------------------------

LauncherTreeView::LauncherTreeView() :
	m_icon_size(0)
{
	// Create the view
	m_view = GTK_TREE_VIEW(gtk_tree_view_new());
	gtk_tree_view_set_activate_on_single_click(m_view, true);
	gtk_tree_view_set_headers_visible(m_view, false);
	gtk_tree_view_set_enable_tree_lines(m_view, false);
	gtk_tree_view_set_hover_selection(m_view, true);
	gtk_tree_view_set_enable_search(m_view, false);
	gtk_tree_view_set_fixed_height_mode(m_view, true);
	gtk_tree_view_set_row_separator_func(m_view, &is_separator, nullptr, nullptr);
	create_column();

	connect(m_view, "key-press-event",
		[this](GtkWidget*, GdkEvent* event) -> gboolean
		{
			GdkEventKey* key_event = reinterpret_cast<GdkEventKey*>(event);
			if ((key_event->keyval == GDK_KEY_Up) || (key_event->keyval == GDK_KEY_Down))
			{
				gtk_tree_view_set_hover_selection(m_view, false);
			}
			return GDK_EVENT_PROPAGATE;
		});

	connect(m_view, "key-release-event",
		[this](GtkWidget*, GdkEvent* event) -> gboolean
		{
			GdkEventKey* key_event = reinterpret_cast<GdkEventKey*>(event);
			if ((key_event->keyval == GDK_KEY_Up) || (key_event->keyval == GDK_KEY_Down))
			{
				gtk_tree_view_set_hover_selection(m_view, true);
			}
			return GDK_EVENT_PROPAGATE;
		});

	// Only allow up to one selected item
	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_view);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

	g_object_ref_sink(m_view);

	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_view)), "launchers");

	// Expand on click
	connect(m_view, "row-activated",
		[this](GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn*)
		{
			Element* element = nullptr;
			GtkTreeIter iter;
			gtk_tree_model_get_iter(m_model, &iter, path);
			gtk_tree_model_get(m_model, &iter, COLUMN_LAUNCHER, &element, -1);
			if (element && !dynamic_cast<Category*>(element))
			{
				return;
			}

			if (gtk_tree_view_row_expanded(tree_view, path))
			{
				gtk_tree_view_collapse_row(tree_view, path);
			}
			else
			{
				gtk_tree_view_expand_row(tree_view, path, false);
			}
		});
}

//-----------------------------------------------------------------------------

LauncherTreeView::~LauncherTreeView()
{
	gtk_widget_destroy(GTK_WIDGET(m_view));
	g_object_unref(m_view);
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherTreeView::get_cursor() const
{
	GtkTreePath* path = nullptr;
	gtk_tree_view_get_cursor(m_view, &path, nullptr);
	return path;
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherTreeView::get_path_at_pos(int x, int y) const
{
	GtkTreePath* path = nullptr;
	gtk_tree_view_get_path_at_pos(m_view, x, y, &path, nullptr, nullptr, nullptr);
	return path;
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherTreeView::get_selected_path() const
{
	GtkTreePath* path = nullptr;
	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_view);
	GtkTreeIter iter;
	if (gtk_tree_selection_get_selected(selection, nullptr, &iter))
	{
		path = gtk_tree_model_get_path(m_model, &iter);
	}
	return path;
}

//-----------------------------------------------------------------------------

void LauncherTreeView::activate_path(GtkTreePath* path)
{
	GtkTreeViewColumn* column = gtk_tree_view_get_column(m_view, 0);
	gtk_tree_view_row_activated(m_view, path, column);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::scroll_to_path(GtkTreePath* path)
{
	gtk_tree_view_scroll_to_cell(m_view, path, nullptr, true, 0.5f, 0.5f);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::select_path(GtkTreePath* path)
{
	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_view);
	gtk_tree_selection_select_path(selection, path);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::set_cursor(GtkTreePath* path)
{
	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_view);
	GtkSelectionMode mode = gtk_tree_selection_get_mode(selection);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);
	gtk_tree_view_set_cursor(m_view, path, nullptr, false);
	gtk_tree_selection_set_mode(selection, mode);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::set_fixed_height_mode(bool fixed_height)
{
	gtk_tree_view_set_fixed_height_mode(m_view, fixed_height);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::set_selection_mode(GtkSelectionMode mode)
{
	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_view);
	gtk_tree_selection_set_mode(selection, mode);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::hide_tooltips()
{
	gtk_tree_view_set_tooltip_column(m_view, -1);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::show_tooltips()
{
	gtk_tree_view_set_tooltip_column(m_view, COLUMN_TOOLTIP);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::clear_selection()
{
	GtkTreeSelection* selection = gtk_tree_view_get_selection(m_view);
	gtk_tree_selection_unselect_all(selection);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::collapse_all()
{
	gtk_tree_view_collapse_all(m_view);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::set_model(GtkTreeModel* model)
{
	m_model = model;
	gtk_tree_view_set_model(m_view, model);
	gtk_tree_view_set_search_column(m_view, -1);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::unset_model()
{
	m_model = nullptr;
	gtk_tree_view_set_model(m_view, nullptr);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::set_drag_source(GdkModifierType start_button_mask, const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions)
{
	gtk_tree_view_enable_model_drag_source(m_view, start_button_mask, targets, n_targets, actions);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::set_drag_dest(const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions)
{
	gtk_tree_view_enable_model_drag_dest(m_view, targets, n_targets, actions);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::unset_drag_source()
{
	gtk_tree_view_unset_rows_drag_source(m_view);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::unset_drag_dest()
{
	gtk_tree_view_unset_rows_drag_dest(m_view);
}

//-----------------------------------------------------------------------------

void LauncherTreeView::reload_icon_size()
{
	// Force exo to reload SVG icons
	if (m_icon_size != wm_settings->launcher_icon_size.get_size())
	{
		gtk_tree_view_remove_column(m_view, m_column);
		create_column();
	}
}

//-----------------------------------------------------------------------------

void LauncherTreeView::create_column()
{
	m_icon_size = wm_settings->launcher_icon_size.get_size();

	m_column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_expand(m_column, true);
	gtk_tree_view_column_set_visible(m_column, true);

	if (m_icon_size > 1)
	{
		GtkCellRenderer* icon_renderer = whiskermenu_icon_renderer_new();
		g_object_set(icon_renderer, "size", m_icon_size, nullptr);
		gtk_tree_view_column_pack_start(m_column, icon_renderer, false);
		gtk_tree_view_column_set_attributes(m_column, icon_renderer, "gicon", COLUMN_ICON, "launcher", COLUMN_LAUNCHER, nullptr);
	}

	GtkCellRenderer* text_renderer = gtk_cell_renderer_text_new();
	g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_END, nullptr);
	gtk_tree_view_column_pack_start(m_column, text_renderer, true);
	gtk_tree_view_column_add_attribute(m_column, text_renderer, "markup", COLUMN_TEXT);

	gtk_tree_view_column_set_sizing(m_column, GTK_TREE_VIEW_COLUMN_FIXED);

	gtk_tree_view_append_column(m_view, m_column);
}

//-----------------------------------------------------------------------------
