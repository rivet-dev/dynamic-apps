/*
 * Copyright (C) 2020 Graeme Gott <graeme@gottcode.org>
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

#include "icon-renderer.h"

#include <gio/gio.h>

//-----------------------------------------------------------------------------

struct _WhiskerMenuIconRenderer
{
	GtkCellRenderer parent;

	gpointer launcher;
	GIcon* gicon;
	gint size;
	bool stretch;
};

#define WHISKERMENU_TYPE_ICON_RENDERER whiskermenu_icon_renderer_get_type()
G_DECLARE_FINAL_TYPE(WhiskerMenuIconRenderer, whiskermenu_icon_renderer, WHISKERMENU, ICON_RENDERER, GtkCellRenderer)

G_DEFINE_TYPE(WhiskerMenuIconRenderer, whiskermenu_icon_renderer, GTK_TYPE_CELL_RENDERER)

enum
{
	PROP_0,
	PROP_LAUNCHER,
	PROP_GICON,
	PROP_SIZE,
	PROP_STRETCH
};

//-----------------------------------------------------------------------------

static void whiskermenu_icon_renderer_get_preferred_width(GtkCellRenderer* renderer, GtkWidget*, gint* minimum, gint* natural)
{
	WhiskerMenuIconRenderer* icon_renderer = WHISKERMENU_ICON_RENDERER(renderer);

	gint pad;
	gtk_cell_renderer_get_padding(renderer, &pad, nullptr);
	gint width = (pad * 2) + icon_renderer->size;

	if (icon_renderer->stretch)
	{
		width += 76 - (icon_renderer->size / 4);
		if (natural)
		{
			*natural = (width * 2) - 1;
		}
	}
	else if (natural)
	{
		*natural = width;
	}

	if (minimum)
	{
		*minimum = width;
	}
}

//-----------------------------------------------------------------------------

static void whiskermenu_icon_renderer_get_preferred_height(GtkCellRenderer* renderer, GtkWidget*, gint* minimum, gint* natural)
{
	WhiskerMenuIconRenderer* icon_renderer = WHISKERMENU_ICON_RENDERER(renderer);

	gint pad;
	gtk_cell_renderer_get_padding(renderer, nullptr, &pad);
	gint height = (pad * 2) + icon_renderer->size;

	if (minimum)
	{
		*minimum = height;
	}
	if (natural)
	{
		*natural = height;
	}
}

//-----------------------------------------------------------------------------

static void whiskermenu_icon_renderer_render(GtkCellRenderer* renderer, cairo_t* cr, GtkWidget* widget,
		const GdkRectangle*, const GdkRectangle* cell_area, GtkCellRendererState)
{
	WhiskerMenuIconRenderer* icon_renderer = WHISKERMENU_ICON_RENDERER(renderer);

	if (!icon_renderer->gicon)
	{
		return;
	}

	GdkRectangle clip_area;
	if (!gdk_cairo_get_clip_rectangle(cr, &clip_area))
	{
		return;
	}

	const gint scale = gtk_widget_get_scale_factor(widget);
	GtkIconTheme* icon_theme = gtk_icon_theme_get_for_screen(gtk_widget_get_screen(widget));
	GdkWindow* window = gtk_widget_get_window(widget);

	cairo_surface_t* surface = nullptr;

	GtkIconInfo* icon_info = gtk_icon_theme_lookup_by_gicon_for_scale(icon_theme,
			icon_renderer->gicon,
			icon_renderer->size,
			scale,
			GtkIconLookupFlags(GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_FORCE_SIZE));
	if (icon_info)
	{
		surface = gtk_icon_info_load_surface(icon_info, window, nullptr);
		g_object_unref(icon_info);
	}

	if (!surface)
	{
		icon_info = gtk_icon_theme_lookup_icon_for_scale(icon_theme,
				icon_renderer->launcher ? "application-x-executable" : "applications-other",
				icon_renderer->size,
				scale,
				GtkIconLookupFlags(GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_FORCE_SIZE));
		if (icon_info)
		{
			surface = gtk_icon_info_load_surface(icon_info, window, nullptr);
			g_object_unref(icon_info);
		}
	}

	if (!surface)
	{
		return;
	}

	GdkRectangle icon_area;
	icon_area.width = cairo_image_surface_get_width(surface) / scale;
	icon_area.height = cairo_image_surface_get_height(surface) / scale;
	icon_area.x = cell_area->x + (cell_area->width - icon_area.width) / 2;
	icon_area.y = cell_area->y + (cell_area->height - icon_area.height) / 2;

	GdkRectangle draw_area;
	if (gdk_rectangle_intersect(&clip_area, &icon_area, &draw_area))
	{
		cairo_set_source_surface(cr, surface, icon_area.x, icon_area.y);
		cairo_rectangle(cr, draw_area.x, draw_area.y, draw_area.width, draw_area.height);
		cairo_fill(cr);
	}

	cairo_surface_destroy(surface);
}

//-----------------------------------------------------------------------------

static void whiskermenu_icon_renderer_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
	WhiskerMenuIconRenderer* icon_renderer = WHISKERMENU_ICON_RENDERER(object);

	switch(prop_id)
	{
	case PROP_LAUNCHER:
		g_value_set_pointer(value, icon_renderer->launcher);
		break;

	case PROP_GICON:
		g_value_set_object(value, icon_renderer->gicon);
		break;

	case PROP_SIZE:
		g_value_set_int(value, icon_renderer->size);
		break;

	case PROP_STRETCH:
		g_value_set_boolean(value, icon_renderer->stretch);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

//-----------------------------------------------------------------------------

static void whiskermenu_icon_renderer_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
	WhiskerMenuIconRenderer* icon_renderer = WHISKERMENU_ICON_RENDERER(object);

	switch(prop_id)
	{
	case PROP_LAUNCHER:
		icon_renderer->launcher = g_value_get_pointer(value);
		break;

	case PROP_GICON:
		if (icon_renderer->gicon)
		{
			g_object_unref(icon_renderer->gicon);
		}
		icon_renderer->gicon = static_cast<GIcon*>(g_value_dup_object(value));
		break;

	case PROP_SIZE:
		icon_renderer->size = g_value_get_int(value);
		break;

	case PROP_STRETCH:
		icon_renderer->stretch = g_value_get_boolean(value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

//-----------------------------------------------------------------------------

static void whiskermenu_icon_renderer_finalize(GObject* object)
{
	WhiskerMenuIconRenderer* icon_renderer = WHISKERMENU_ICON_RENDERER(object);

	if (icon_renderer->gicon)
	{
		g_object_unref(icon_renderer->gicon);
	}

	G_OBJECT_CLASS(whiskermenu_icon_renderer_parent_class)->finalize(object);
}

//-----------------------------------------------------------------------------

static void whiskermenu_icon_renderer_class_init(WhiskerMenuIconRendererClass* klass)
{
	GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
	gobject_class->finalize = &whiskermenu_icon_renderer_finalize;
	gobject_class->get_property = &whiskermenu_icon_renderer_get_property;
	gobject_class->set_property = &whiskermenu_icon_renderer_set_property;

	GtkCellRendererClass* renderer_class = GTK_CELL_RENDERER_CLASS(klass);
	renderer_class->get_preferred_width = &whiskermenu_icon_renderer_get_preferred_width;
	renderer_class->get_preferred_height = &whiskermenu_icon_renderer_get_preferred_height;
	renderer_class->render = &whiskermenu_icon_renderer_render;

	g_object_class_install_property(gobject_class,
			PROP_LAUNCHER,
			g_param_spec_pointer("launcher", "launcher", "launcher",
					GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

	g_object_class_install_property(gobject_class,
			PROP_GICON,
			g_param_spec_object("gicon", "gicon", "gicon",
					G_TYPE_ICON,
					GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

	g_object_class_install_property(gobject_class,
			PROP_SIZE,
			g_param_spec_int("size", "size", "size",
					1, G_MAXINT, 48,
					GParamFlags(G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

	g_object_class_install_property(gobject_class,
			PROP_STRETCH,
			g_param_spec_boolean("stretch", "stretch", "stretch",
					false,
					GParamFlags(G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

//-----------------------------------------------------------------------------

static void whiskermenu_icon_renderer_init(WhiskerMenuIconRenderer*)
{
}

//-----------------------------------------------------------------------------

GtkCellRenderer* whiskermenu_icon_renderer_new()
{
	return GTK_CELL_RENDERER(g_object_new(WHISKERMENU_TYPE_ICON_RENDERER, nullptr));
}

//-----------------------------------------------------------------------------
