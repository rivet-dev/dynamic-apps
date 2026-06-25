/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2007 -2009 Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
 * Copyright (C) 2009 - Jesse van den Kieboom <jessevdk@gnome.org>
 * Copyright (C) 2012 - Sébastien Wilmet <swilmet@gnome.org>
 *
 * GtkSourceView is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GtkSourceView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:completioninfo
 * @title: GtkSourceCompletionInfo
 * @short_description: Calltips object
 *
 * This object can be used to show a calltip or help for the
 * current completion proposal.
 *
 * The info window has always the same size as the natural size of its child
 * widget, added with gtk_container_add().  If you want a fixed size instead, a
 * possibility is to use a scrolled window, as the following example
 * demonstrates.
 *
 * <example>
 *   <title>Fixed size with a scrolled window.</title>
 *   <programlisting>
 * GtkSourceCompletionInfo *info;
 * GtkWidget *your_widget;
 * GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
 *
 * gtk_widget_set_size_request (scrolled_window, 300, 200);
 * gtk_container_add (GTK_CONTAINER (scrolled_window), your_widget);
 * gtk_container_add (GTK_CONTAINER (info), scrolled_window);
 *   </programlisting>
 * </example>
 *
 * If the calltip is displayed on top of a certain widget, say a #GtkTextView,
 * you should attach the calltip window to the #GtkTextView with
 * gtk_window_set_attached_to().  By doing this, the calltip will be hidden when
 * the #GtkWidget::focus-out-event signal is emitted by the #GtkTextView. You
 * may also be interested by the #GtkTextBuffer:cursor-position property (when
 * its value is modified). If you use the #GtkSourceCompletionInfo through the
 * #GtkSourceCompletion machinery, you don't need to worry about this.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gtksourcecompletioninfo.h"
#include <glib/gi18n-lib.h>

struct _GtkSourceCompletionInfoPrivate
{
	guint idle_resize;

	GtkWidget *attached_to;
	gulong focus_out_event_handler;

	gint xoffset;

	guint transient_set : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceCompletionInfo, gtk_source_completion_info, GTK_TYPE_WINDOW);

/* Resize the window */

static gboolean
idle_resize (GtkSourceCompletionInfo *info)
{
	GtkWidget *child = gtk_bin_get_child (GTK_BIN (info));
	GtkRequisition nat_size;
	guint border_width;
	gint window_width;
	gint window_height;
	gint cur_window_width;
	gint cur_window_height;

	info->priv->idle_resize = 0;

	if (child == NULL)
	{
		return G_SOURCE_REMOVE;
	}

	gtk_widget_get_preferred_size (child, NULL, &nat_size);

	border_width = gtk_container_get_border_width (GTK_CONTAINER (info));

	window_width = nat_size.width + 2 * border_width;
	window_height = nat_size.height + 2 * border_width;

	gtk_window_get_size (GTK_WINDOW (info), &cur_window_width, &cur_window_height);

	/* Avoid an infinite loop */
	if (cur_window_width != window_width || cur_window_height != window_height)
	{
		gtk_window_resize (GTK_WINDOW (info),
				   MAX (1, window_width),
				   MAX (1, window_height));
	}

	return G_SOURCE_REMOVE;
}

static void
queue_resize (GtkSourceCompletionInfo *info)
{
	if (info->priv->idle_resize == 0)
	{
		info->priv->idle_resize = g_idle_add ((GSourceFunc)idle_resize, info);
	}
}

static void
gtk_source_completion_info_check_resize (GtkContainer *container)
{
	GtkSourceCompletionInfo *info = GTK_SOURCE_COMPLETION_INFO (container);
	queue_resize (info);

	GTK_CONTAINER_CLASS (gtk_source_completion_info_parent_class)->check_resize (container);
}

/* Geometry management */

static GtkSizeRequestMode
gtk_source_completion_info_get_request_mode (GtkWidget *widget)
{
	return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_source_completion_info_get_preferred_width (GtkWidget *widget,
						gint	  *min_width,
						gint	  *nat_width)
{
	GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));
	gint width = 0;

	if (child != NULL)
	{
		GtkRequisition nat_size;
		gtk_widget_get_preferred_size (child, NULL, &nat_size);
		width = nat_size.width;
	}

	if (min_width != NULL)
	{
		*min_width = width;
	}

	if (nat_width != NULL)
	{
		*nat_width = width;
	}
}

static void
gtk_source_completion_info_get_preferred_height (GtkWidget *widget,
						 gint	   *min_height,
						 gint	   *nat_height)
{
	GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));
	gint height = 0;

	if (child != NULL)
	{
		GtkRequisition nat_size;
		gtk_widget_get_preferred_size (child, NULL, &nat_size);
		height = nat_size.height;
	}

	if (min_height != NULL)
	{
		*min_height = height;
	}

	if (nat_height != NULL)
	{
		*nat_height = height;
	}
}

/* Init, dispose, finalize, ... */

static gboolean
focus_out_event_cb (GtkSourceCompletionInfo *info)
{
	gtk_widget_hide (GTK_WIDGET (info));
	return FALSE;
}

static void
set_attached_to (GtkSourceCompletionInfo *info,
		 GtkWidget               *attached_to)
{
	if (info->priv->attached_to != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (info->priv->attached_to),
					      (gpointer *) &info->priv->attached_to);

		if (info->priv->focus_out_event_handler != 0)
		{
			g_signal_handler_disconnect (info->priv->attached_to,
						     info->priv->focus_out_event_handler);

			info->priv->focus_out_event_handler = 0;
		}
	}

	info->priv->attached_to = attached_to;

	if (attached_to == NULL)
	{
		return;
	}

	g_object_add_weak_pointer (G_OBJECT (attached_to),
				   (gpointer *) &info->priv->attached_to);

	info->priv->focus_out_event_handler =
		g_signal_connect_swapped (attached_to,
					  "focus-out-event",
					  G_CALLBACK (focus_out_event_cb),
					  info);

	info->priv->transient_set = FALSE;
}

static void
update_attached_to (GtkSourceCompletionInfo *info)
{
	set_attached_to (info, gtk_window_get_attached_to (GTK_WINDOW (info)));
}

static void
gtk_source_completion_info_init (GtkSourceCompletionInfo *info)
{
	info->priv = gtk_source_completion_info_get_instance_private (info);

	g_signal_connect (info,
			  "notify::attached-to",
			  G_CALLBACK (update_attached_to),
			  NULL);

	update_attached_to (info);

	/* Tooltip style */
	gtk_window_set_title (GTK_WINDOW (info), _("Completion Info"));
	gtk_widget_set_name (GTK_WIDGET (info), "gtk-tooltip");

	gtk_window_set_type_hint (GTK_WINDOW (info),
	                          GDK_WINDOW_TYPE_HINT_COMBO);

	gtk_container_set_border_width (GTK_CONTAINER (info), 1);
}

static void
gtk_source_completion_info_dispose (GObject *object)
{
	GtkSourceCompletionInfo *info = GTK_SOURCE_COMPLETION_INFO (object);

	if (info->priv->idle_resize != 0)
	{
		g_source_remove (info->priv->idle_resize);
		info->priv->idle_resize = 0;
	}

	set_attached_to (info, NULL);

	G_OBJECT_CLASS (gtk_source_completion_info_parent_class)->dispose (object);
}

static void
gtk_source_completion_info_show (GtkWidget *widget)
{
	GtkSourceCompletionInfo *info = GTK_SOURCE_COMPLETION_INFO (widget);

	if (info->priv->attached_to != NULL && !info->priv->transient_set)
	{
		GtkWidget *toplevel;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (info->priv->attached_to));
		if (gtk_widget_is_toplevel (toplevel))
		{
			gtk_window_set_transient_for (GTK_WINDOW (info),
						      GTK_WINDOW (toplevel));
			info->priv->transient_set = TRUE;
		}
	}

	GTK_WIDGET_CLASS (gtk_source_completion_info_parent_class)->show (widget);
}

static gboolean
gtk_source_completion_info_draw (GtkWidget *widget,
                                 cairo_t   *cr)
{
	GTK_WIDGET_CLASS (gtk_source_completion_info_parent_class)->draw (widget, cr);

	gtk_render_frame (gtk_widget_get_style_context (widget),
	                  cr,
	                  0, 0,
	                  gtk_widget_get_allocated_width (widget),
	                  gtk_widget_get_allocated_height (widget));

	return FALSE;
}

static void
gtk_source_completion_info_class_init (GtkSourceCompletionInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

	object_class->dispose = gtk_source_completion_info_dispose;

	widget_class->show = gtk_source_completion_info_show;
	widget_class->draw = gtk_source_completion_info_draw;
	widget_class->get_request_mode = gtk_source_completion_info_get_request_mode;
	widget_class->get_preferred_width = gtk_source_completion_info_get_preferred_width;
	widget_class->get_preferred_height = gtk_source_completion_info_get_preferred_height;

	container_class->check_resize = gtk_source_completion_info_check_resize;
}

void
_gtk_source_completion_info_set_xoffset (GtkSourceCompletionInfo *window,
					 gint                     xoffset)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_INFO (window));

	window->priv->xoffset = xoffset;
}

/* Move to iter */

static void
get_iter_pos (GtkTextView *text_view,
              GtkTextIter *iter,
              gint        *x,
              gint        *y,
              gint        *height)
{
	GdkRectangle location;

	gtk_text_view_get_iter_location (text_view, iter, &location);

	gtk_text_view_buffer_to_window_coords (text_view,
					       GTK_TEXT_WINDOW_WIDGET,
					       location.x,
					       location.y,
					       x,
					       y);

	*height = location.height;
}

static void
move_to_iter (GtkSourceCompletionInfo *window,
              GtkTextView             *view,
              GtkTextIter             *iter)
{
	GdkRectangle position;
	GdkWindow *gdk_window;
	gint x, y;
	gint line_height;

	gdk_window = gtk_widget_get_window (GTK_WIDGET (window));

	if (gdk_window == NULL)
	{
		return;
	}

	get_iter_pos (view, iter, &x, &y, &line_height);

	gtk_widget_translate_coordinates (GTK_WIDGET (view),
	                                  gtk_widget_get_toplevel (GTK_WIDGET (view)),
					  x, y, &x, &y);

	position.x = x;
	position.y = y;
	position.height = line_height;
	position.width = 0;

	gdk_window_move_to_rect (gdk_window,
				 &position,
				 GDK_GRAVITY_SOUTH_WEST,
				 GDK_GRAVITY_NORTH_WEST,
				 GDK_ANCHOR_SLIDE | GDK_ANCHOR_FLIP_Y | GDK_ANCHOR_RESIZE,
				 window->priv->xoffset, 0);
}

static void
move_to_cursor (GtkSourceCompletionInfo *window,
		GtkTextView             *view)
{
	GtkTextBuffer *buffer;
	GtkTextIter insert;

	buffer = gtk_text_view_get_buffer (view);
	gtk_text_buffer_get_iter_at_mark (buffer, &insert, gtk_text_buffer_get_insert (buffer));

	move_to_iter (window, view, &insert);
}

/* Public functions */

/**
 * gtk_source_completion_info_new:
 *
 * Returns: a new GtkSourceCompletionInfo.
 */
GtkSourceCompletionInfo *
gtk_source_completion_info_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_INFO,
	                     "type", GTK_WINDOW_POPUP,
			     "border-width", 3,
	                     NULL);
}

/**
 * gtk_source_completion_info_move_to_iter:
 * @info: a #GtkSourceCompletionInfo.
 * @view: a #GtkTextView on which the info window should be positioned.
 * @iter: (nullable): a #GtkTextIter.
 *
 * Moves the #GtkSourceCompletionInfo to @iter. If @iter is %NULL @info is
 * moved to the cursor position. Moving will respect the #GdkGravity setting
 * of the info window and will ensure the line at @iter is not occluded by
 * the window.
 */
void
gtk_source_completion_info_move_to_iter (GtkSourceCompletionInfo *info,
                                         GtkTextView             *view,
                                         GtkTextIter             *iter)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_INFO (info));
	g_return_if_fail (GTK_IS_TEXT_VIEW (view));

	if (iter == NULL)
	{
		move_to_cursor (info, view);
	}
	else
	{
		move_to_iter (info, view, iter);
	}
}
