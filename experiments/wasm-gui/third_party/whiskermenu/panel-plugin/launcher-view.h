/*
 * Copyright (C) 2013-2020 Graeme Gott <graeme@gottcode.org>
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

#ifndef WHISKERMENU_LAUNCHER_VIEW_H
#define WHISKERMENU_LAUNCHER_VIEW_H

#include <gtk/gtk.h>

namespace WhiskerMenu
{

class LauncherView
{
public:
	virtual ~LauncherView() = default;

	LauncherView(const LauncherView&) = delete;
	LauncherView(LauncherView&&) = delete;
	LauncherView& operator=(const LauncherView&) = delete;
	LauncherView& operator=(LauncherView&&) = delete;

	virtual GtkWidget* get_widget() const=0;

	virtual GtkTreePath* get_cursor() const=0;
	virtual GtkTreePath* get_path_at_pos(int x, int y) const=0;
	virtual GtkTreePath* get_selected_path() const=0;
	virtual void activate_path(GtkTreePath* path)=0;
	virtual void scroll_to_path(GtkTreePath* path)=0;
	virtual void select_path(GtkTreePath* path)=0;
	virtual void set_cursor(GtkTreePath* path)=0;

	virtual void set_fixed_height_mode(bool fixed_height)=0;
	virtual void set_selection_mode(GtkSelectionMode mode)=0;

	virtual void hide_tooltips()=0;
	virtual void show_tooltips()=0;

	virtual void clear_selection()=0;
	virtual void collapse_all()=0;

	GtkTreeModel* get_model() const
	{
		return m_model;
	}

	virtual void set_model(GtkTreeModel* model)=0;
	virtual void unset_model()=0;

	virtual void set_drag_source(GdkModifierType start_button_mask, const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions)=0;
	virtual void set_drag_dest(const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions)=0;
	virtual void unset_drag_source()=0;
	virtual void unset_drag_dest()=0;

	virtual void reload_icon_size()=0;

	enum Columns
	{
		COLUMN_ICON = 0,
		COLUMN_TEXT,
		COLUMN_TOOLTIP,
		COLUMN_LAUNCHER,
		N_COLUMNS
	};

protected:
	LauncherView() = default;

	GtkTreeModel* m_model = nullptr;
};

}

#endif // WHISKERMENU_LAUNCHER_VIEW_H
