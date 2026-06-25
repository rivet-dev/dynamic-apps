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

#include "category-button.h"

#include "settings.h"
#include "slot.h"

#include <libxfce4panel/libxfce4panel.h>

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

static gboolean hover_timeout(gpointer user_data)
{
	GtkToggleButton* button = GTK_TOGGLE_BUTTON(user_data);
	if (gtk_widget_get_state_flags(GTK_WIDGET(button)) & GTK_STATE_FLAG_PRELIGHT)
	{
		gtk_toggle_button_set_active(button, true);
	}
	return GDK_EVENT_PROPAGATE;
}

//-----------------------------------------------------------------------------

CategoryButton::CategoryButton(GIcon* icon, const gchar* text)
{
	m_button = GTK_RADIO_BUTTON(gtk_radio_button_new(nullptr));
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(m_button), false);
	gtk_button_set_relief(GTK_BUTTON(m_button), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text(GTK_WIDGET(m_button), text);
	gtk_widget_set_focus_on_click(GTK_WIDGET(m_button), false);

	connect(m_button, "enter-notify-event",
		[](GtkWidget* widget, GdkEvent*) -> gboolean
		{
			GtkToggleButton* button = GTK_TOGGLE_BUTTON(widget);
			if (wm_settings->category_hover_activate && !gtk_toggle_button_get_active(button))
			{
				g_timeout_add(150, &hover_timeout, button);
			}
			return GDK_EVENT_PROPAGATE;
		});

	connect(m_button, "focus-in-event",
		[](GtkWidget* widget, GdkEvent*) -> gboolean
		{
			GtkToggleButton* button = GTK_TOGGLE_BUTTON(widget);
			if (wm_settings->category_hover_activate && !gtk_toggle_button_get_active(button))
			{
				gtk_toggle_button_set_active(button, true);
				gtk_widget_grab_focus(widget);
			}
			return GDK_EVENT_PROPAGATE;
		});

	m_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
	gtk_container_add(GTK_CONTAINER(m_button), GTK_WIDGET(m_box));

	m_icon = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(m_box, m_icon, false, false, 0);

	m_label = gtk_label_new(text);
	gtk_box_pack_start(m_box, m_label, false, true, 0);

	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_button)), "category-button");

	gtk_widget_show_all(GTK_WIDGET(m_button));

	reload_icon_size();
}

//-----------------------------------------------------------------------------

CategoryButton::~CategoryButton()
{
	gtk_widget_destroy(GTK_WIDGET(m_button));
}

//-----------------------------------------------------------------------------

void CategoryButton::reload_icon_size()
{
	int size = wm_settings->category_icon_size.get_size();
	gtk_image_set_pixel_size(GTK_IMAGE(m_icon), size);
	gtk_widget_set_visible(m_icon, size > 1);

	if (wm_settings->category_show_name && !wm_settings->position_categories_horizontal)
	{
		gtk_widget_set_has_tooltip(GTK_WIDGET(m_button), false);
		gtk_box_set_child_packing(m_box, m_icon, false, false, 0, GTK_PACK_START);
		gtk_widget_show(m_label);
	}
	else
	{
		gtk_widget_set_has_tooltip(GTK_WIDGET(m_button), true);
		gtk_widget_hide(m_label);
		gtk_box_set_child_packing(m_box, m_icon, true, true, 0, GTK_PACK_START);
	}
}

//-----------------------------------------------------------------------------
