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

#ifndef WHISKERMENU_LAUNCHER_TREE_VIEW_H
#define WHISKERMENU_LAUNCHER_TREE_VIEW_H

#include "launcher-view.h"

namespace WhiskerMenu
{

class LauncherTreeView : public LauncherView
{
public:
	LauncherTreeView();
	~LauncherTreeView();

	GtkWidget* get_widget() const override
	{
		return GTK_WIDGET(m_view);
	}

	GtkTreePath* get_cursor() const override;
	GtkTreePath* get_path_at_pos(int x, int y) const override;
	GtkTreePath* get_selected_path() const override;
	void activate_path(GtkTreePath* path) override;
	void scroll_to_path(GtkTreePath* path) override;
	void select_path(GtkTreePath* path) override;
	void set_cursor(GtkTreePath* path) override;

	void set_fixed_height_mode(bool fixed_height) override;
	void set_selection_mode(GtkSelectionMode mode) override;

	void hide_tooltips() override;
	void show_tooltips() override;

	void clear_selection() override;
	void collapse_all() override;

	void set_model(GtkTreeModel* model) override;
	void unset_model() override;

	void set_drag_source(GdkModifierType start_button_mask, const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions) override;
	void set_drag_dest(const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions) override;
	void unset_drag_source() override;
	void unset_drag_dest() override;

	void reload_icon_size() override;

private:
	void create_column();

private:
	GtkTreeView* m_view;
	GtkTreeViewColumn* m_column;
	int m_icon_size;
};

}

#endif // WHISKERMENU_LAUNCHER_TREE_VIEW_H
