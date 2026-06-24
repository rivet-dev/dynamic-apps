/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "netstatus-icon.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "netstatus-util.h"
#include "netstatus-enums.h"
#include "netstatus-fallback-pixbuf.h"

#include "gtk-compat.h"

typedef enum
{
  NETSTATUS_SIGNAL_0_24 = 0,
  NETSTATUS_SIGNAL_25_49,
  NETSTATUS_SIGNAL_50_74,
  NETSTATUS_SIGNAL_75_100,
  NETSTATUS_SIGNAL_LAST
} NetstatusSignal;

struct _NetstatusIconPrivate
{
  GtkWidget      *image;
  GtkWidget      *signal_image;
  GtkWidget      *error_dialog;

  NetstatusIface *iface;
  NetstatusState  state;
  NetstatusSignal signal_strength;

  GtkIconTheme   *icon_theme;
  GdkPixbuf      *icons [NETSTATUS_STATE_LAST];
  GdkPixbuf      *scaled_icons [NETSTATUS_STATE_LAST];

  GdkPixbuf      *signal_icons [NETSTATUS_SIGNAL_LAST];
  GdkPixbuf      *rotated_signal_icons [NETSTATUS_SIGNAL_LAST];
  GdkPixbuf      *scaled_signal_icons [NETSTATUS_SIGNAL_LAST];

  GtkOrientation  orientation;
  int             size;

  GtkTooltip    *tooltips;

  gulong          state_changed_id;
  gulong          name_changed_id;
  gulong          wireless_changed_id;
  gulong          signal_changed_id;

  guint           tooltips_enabled : 1;
  guint           show_signal : 1;
};

enum {
  INVOKED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_IFACE,
  PROP_ORIENTATION,
  PROP_TOOLTIPS_ENABLED,
  PROP_SHOW_SIGNAL
};

static void netstatus_icon_init_pixbufs (NetstatusIcon *icon);
static void netstatus_icon_scale_icons  (NetstatusIcon *icon,
					 int            height);

static GObjectClass *parent_class;
static guint icon_signals [LAST_SIGNAL] = { 0 };

static GdkPixbuf *
netstatus_icon_get_default_pixbuf (NetstatusIcon *icon __attribute__((unused)))
{
  static GdkPixbuf *fallback_pixbuf = NULL;

  if (!fallback_pixbuf)
    {
      fallback_pixbuf = gdk_pixbuf_new_from_inline (-1,
						    fallback_icon_data,
						    FALSE,
						    NULL);
      g_object_add_weak_pointer (G_OBJECT (fallback_pixbuf),
				 (gpointer) &fallback_pixbuf);

      return fallback_pixbuf;
   }

  return g_object_ref (fallback_pixbuf);
}

static void
netstatus_icon_theme_changed (NetstatusIcon *icon,
			      GtkIconTheme  *icon_theme __attribute__((unused)))
{
  int i;

  for (i = 0; i < NETSTATUS_STATE_LAST; i++)
    {
      g_object_unref (icon->priv->scaled_icons [i]);
      icon->priv->scaled_icons [i] = NULL;

      g_object_unref (icon->priv->icons [i]);
      icon->priv->icons [i] = NULL;
    }

  for (i = 0; i < NETSTATUS_SIGNAL_LAST; i++)
    {
      g_object_unref (icon->priv->scaled_signal_icons [i]);
      icon->priv->scaled_signal_icons [i] = NULL;

      g_object_unref (icon->priv->signal_icons [i]);
      icon->priv->signal_icons [i] = NULL;
    }

  netstatus_icon_init_pixbufs (icon);

  if (icon->priv->size)
    {
      netstatus_icon_scale_icons (icon, icon->priv->size);
    }
}

static GtkIconTheme *
netstatus_icon_get_icon_theme (NetstatusIcon *icon)
{
  if (!icon->priv->icon_theme)
    {
      GdkScreen *screen;

      screen = gtk_widget_get_screen (GTK_WIDGET (icon));
      icon->priv->icon_theme = gtk_icon_theme_get_for_screen (screen);

      g_signal_connect_object (icon->priv->icon_theme, "changed",
			       G_CALLBACK (netstatus_icon_theme_changed),
			       icon,
			       G_CONNECT_SWAPPED);
    }

  return icon->priv->icon_theme;
}

static char *
netstatus_icon_lookup_icon_theme (NetstatusIcon *icon,
				  const char    *icon_name)
{
  GtkIconTheme *icon_theme;
  GtkIconInfo  *icon_info;
  char         *filename = NULL;

  icon_theme = netstatus_icon_get_icon_theme (icon);
  if ((icon_info = gtk_icon_theme_lookup_icon (icon_theme, icon_name, 1000, 0)))
    {
      filename = g_strdup (gtk_icon_info_get_filename (icon_info));

      gtk_icon_info_free (icon_info);
    }

  return filename;
}

static void
netstatus_icon_init_pixbuf (NetstatusIcon  *icon,
			    GdkPixbuf     **pixbuf,
			    const char     *icon_name)
{
  char *filename;

  g_assert (*pixbuf == NULL);

  if ((filename = netstatus_icon_lookup_icon_theme (icon, icon_name)))
    {
      GError *error;

      error = NULL;
      *pixbuf = gdk_pixbuf_new_from_file (filename, &error);
      if (error)
	{
	  netstatus_adopt_error (error, NETSTATUS_ERROR_ICONS);
	  netstatus_iface_set_error (icon->priv->iface, error);
	  g_error_free (error);

	}

      g_free (filename);
    }

  if (!*pixbuf)
    {
      *pixbuf = netstatus_icon_get_default_pixbuf (icon);
      g_assert (*pixbuf != NULL);
    }
}

static GdkPixbuf *
rotate_pixbuf (GdkPixbuf *pixbuf)
{
  GdkPixbuf *freeme;
  GdkPixbuf *retval;
  guint32   *dest;
  guint32   *src;
  int        width;
  int        height;
  int        x;
  int        y;

  freeme = NULL;
  if (!gdk_pixbuf_get_has_alpha (pixbuf))
    pixbuf = freeme = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);

  width  = gdk_pixbuf_get_width  (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  retval = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, height, width);

  dest = (guint32 *) gdk_pixbuf_get_pixels (retval);
  src  = (guint32 *) gdk_pixbuf_get_pixels (pixbuf);

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      dest [(height - y - 1) + height * (width - x - 1)] = src [y * width + x];

  if (freeme)
    g_object_unref (freeme);

  return retval;
}

static void
netstatus_icon_rotate_signal_icons (NetstatusIcon  *icon,
				    GtkOrientation  orientation)
{
  int i;

  if (!icon->priv->signal_icons [0])
    return;

  for (i = 0; i < NETSTATUS_SIGNAL_LAST; i++)
    {
      GdkPixbuf *pixbuf;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  pixbuf = g_object_ref (icon->priv->signal_icons [i]);
	}
      else /* if (orientation == GTK_ORIENTATION_VERTICAL) */
	{
	  pixbuf = rotate_pixbuf (icon->priv->signal_icons [i]);
	}

      if (icon->priv->rotated_signal_icons [i])
	g_object_unref (icon->priv->rotated_signal_icons [i]);
      icon->priv->rotated_signal_icons [i] = pixbuf;

      if (icon->priv->scaled_signal_icons [i])
	g_object_unref (icon->priv->scaled_signal_icons [i]);
      icon->priv->scaled_signal_icons [i] = NULL;
    }
}

static void
netstatus_icon_init_pixbufs (NetstatusIcon *icon)
{
  netstatus_icon_init_pixbuf (icon,
			      &icon->priv->icons [NETSTATUS_STATE_DISCONNECTED],
			      "gnome-netstatus-disconn");
  icon->priv->scaled_icons [NETSTATUS_STATE_DISCONNECTED] = NULL;

  netstatus_icon_init_pixbuf (icon,
			      &icon->priv->icons [NETSTATUS_STATE_IDLE],
			      "gnome-netstatus-idle");
  icon->priv->scaled_icons [NETSTATUS_STATE_IDLE] = NULL;

  netstatus_icon_init_pixbuf (icon,
			      &icon->priv->icons [NETSTATUS_STATE_TX],
			      "gnome-netstatus-tx");
  icon->priv->scaled_icons [NETSTATUS_STATE_TX] = NULL;

  netstatus_icon_init_pixbuf (icon,
			      &icon->priv->icons [NETSTATUS_STATE_RX],
			      "gnome-netstatus-rx");
  icon->priv->scaled_icons [NETSTATUS_STATE_RX] = NULL;

  netstatus_icon_init_pixbuf (icon,
			      &icon->priv->icons [NETSTATUS_STATE_TX_RX],
			      "gnome-netstatus-txrx");
  icon->priv->scaled_icons [NETSTATUS_STATE_TX_RX] = NULL;

  netstatus_icon_init_pixbuf (icon,
			      &icon->priv->icons [NETSTATUS_STATE_ERROR],
			      "gnome-netstatus-error");
  icon->priv->scaled_icons [NETSTATUS_STATE_ERROR] = NULL;

  netstatus_icon_init_pixbuf (icon,
			      &icon->priv->signal_icons [NETSTATUS_SIGNAL_0_24],
			      "gnome-netstatus-0-24");
  icon->priv->rotated_signal_icons [NETSTATUS_SIGNAL_0_24] = NULL;
  icon->priv->scaled_signal_icons  [NETSTATUS_SIGNAL_0_24] = NULL;

  netstatus_icon_init_pixbuf (icon,
			      &icon->priv->signal_icons [NETSTATUS_SIGNAL_25_49],
			      "gnome-netstatus-25-49");
  icon->priv->rotated_signal_icons [NETSTATUS_SIGNAL_25_49] = NULL;
  icon->priv->scaled_signal_icons  [NETSTATUS_SIGNAL_25_49] = NULL;

  netstatus_icon_init_pixbuf (icon,
			      &icon->priv->signal_icons [NETSTATUS_SIGNAL_50_74],
			      "gnome-netstatus-50-74");
  icon->priv->rotated_signal_icons [NETSTATUS_SIGNAL_50_74] = NULL;
  icon->priv->scaled_signal_icons  [NETSTATUS_SIGNAL_50_74] = NULL;

  netstatus_icon_init_pixbuf (icon,
			      &icon->priv->signal_icons [NETSTATUS_SIGNAL_75_100],
			      "gnome-netstatus-75-100");
  icon->priv->rotated_signal_icons [NETSTATUS_SIGNAL_75_100] = NULL;
  icon->priv->scaled_signal_icons  [NETSTATUS_SIGNAL_75_100] = NULL;

  netstatus_icon_rotate_signal_icons (icon, icon->priv->orientation);
}

static void
netstatus_icon_update_image (NetstatusIcon *icon)
{
  GdkPixbuf *pixbuf;

  if (!icon->priv->icons [icon->priv->state])
    netstatus_icon_init_pixbufs (icon);

  pixbuf = icon->priv->scaled_icons [icon->priv->state];
  if (!pixbuf)
    pixbuf = icon->priv->icons [icon->priv->state];

  if (gtk_image_get_pixbuf (GTK_IMAGE (icon->priv->image)) != pixbuf)
    gtk_image_set_from_pixbuf (GTK_IMAGE (icon->priv->image), pixbuf);

  pixbuf = icon->priv->scaled_signal_icons [icon->priv->signal_strength];
  if (!pixbuf)
    pixbuf = icon->priv->rotated_signal_icons [icon->priv->signal_strength];

  if (gtk_image_get_pixbuf (GTK_IMAGE (icon->priv->signal_image)) != pixbuf)
    gtk_image_set_from_pixbuf (GTK_IMAGE (icon->priv->signal_image), pixbuf);
}

static void
netstatus_icon_name_changed (NetstatusIface *iface __attribute__((unused)),
			     GParamSpec     *pspec __attribute__((unused)),
			     NetstatusIcon  *icon)
{
  const gchar *iface_name;
  const gchar *tip;
  gchar       *freeme = NULL;

  iface_name = netstatus_iface_get_name (icon->priv->iface);
  if (iface_name)
    {
      freeme = g_strdup_printf (_("Network Connection: %s"), iface_name);
      tip = freeme;
    }
  else
    {
      tip = _("Network Connection");
    }

  gtk_widget_set_tooltip_text(GTK_WIDGET (icon), tip);

  g_free (freeme);
}

static void
netstatus_icon_state_changed (NetstatusIface *iface,
			      GParamSpec     *pspec __attribute__((unused)),
			      NetstatusIcon  *icon)
{
  NetstatusState state;

  g_return_if_fail (NETSTATUS_IS_ICON (icon));

  state = netstatus_iface_get_state (iface);

  if (icon->priv->state != state)
    {
      icon->priv->state = state;

      netstatus_icon_update_image (icon);
    }
}

static void
netstatus_icon_is_wireless_changed (NetstatusIface *iface,
				    GParamSpec     *pspec __attribute__((unused)),
				    NetstatusIcon  *icon)
{
  if (netstatus_iface_get_is_wireless (iface) && netstatus_icon_get_show_signal(icon))
    gtk_widget_show (icon->priv->signal_image);
  else
    gtk_widget_hide (icon->priv->signal_image);
}

static void
netstatus_icon_signal_changed (NetstatusIface *iface,
			       GParamSpec     *pspec __attribute__((unused)),
			       NetstatusIcon  *icon)
{
  NetstatusSignal signal_strength;
  int             percent;

  percent = netstatus_iface_get_signal_strength (iface);

  if (percent < 25)
    signal_strength = NETSTATUS_SIGNAL_0_24;
  else if (percent < 50)
    signal_strength = NETSTATUS_SIGNAL_25_49;
  else if (percent < 75)
    signal_strength = NETSTATUS_SIGNAL_50_74;
  else
    signal_strength = NETSTATUS_SIGNAL_75_100;

  if (icon->priv->signal_strength != signal_strength)
    {
      icon->priv->signal_strength = signal_strength;

      netstatus_icon_update_image (icon);
    }
}

static void
#if GTK_CHECK_VERSION(3, 0, 0)
netstatus_icon_destroy (GtkWidget *widget)
#else
netstatus_icon_destroy (GtkObject *widget)
#endif
{
  NetstatusIcon *icon = (NetstatusIcon *) widget;

  if (icon->priv->error_dialog)
    gtk_widget_destroy (icon->priv->error_dialog);
  icon->priv->error_dialog = NULL;

  if (icon->priv->state_changed_id)
    {
      g_assert (icon->priv->iface != NULL);
      g_assert (icon->priv->name_changed_id != 0);
      g_signal_handler_disconnect (icon->priv->iface,
				   icon->priv->state_changed_id);
      g_signal_handler_disconnect (icon->priv->iface,
				   icon->priv->name_changed_id);
      g_signal_handler_disconnect (icon->priv->iface,
				   icon->priv->wireless_changed_id);
      g_signal_handler_disconnect (icon->priv->iface,
				   icon->priv->signal_changed_id);
    }
  icon->priv->state_changed_id    = 0;
  icon->priv->name_changed_id     = 0;
  icon->priv->wireless_changed_id = 0;
  icon->priv->signal_changed_id   = 0;

  icon->priv->image = NULL;

#if GTK_CHECK_VERSION(3, 0, 0)
  GTK_WIDGET_CLASS (parent_class)->destroy (widget);
#else
  GTK_OBJECT_CLASS (parent_class)->destroy (widget);
#endif
}

static GdkPixbuf *
scale_pixbuf (GdkPixbuf      *pixbuf,
	      GtkOrientation  orientation,
	      int             size,
	      gboolean        retain_aspect)
{
  GdkPixbuf *retval;
  int        orig_size;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      orig_size = gdk_pixbuf_get_height (pixbuf);
    }
  else /* if (orientation == GTK_ORIENTATION_VERTICAL) */
    {
      orig_size = gdk_pixbuf_get_width (pixbuf);
    }

  if (size >= orig_size)
    {
      retval = g_object_ref (pixbuf);
    }
  else
    {
      int    orig_width;
      int    orig_height;
      int    scaled_width;
      int    scaled_height;
      double scale;

      orig_width  = gdk_pixbuf_get_width  (pixbuf);
      orig_height = gdk_pixbuf_get_height (pixbuf);

      scale = (double) size / orig_size;

      if (retain_aspect)
	{
	  scaled_width  = orig_width  * scale;
	  scaled_height = orig_height * scale;
	}
      else
	{
	  if (orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      scaled_width  = orig_width;
	      scaled_height = orig_height * scale;
	    }
	  else /* if (orientation == GTK_ORIENTATION_VERTICAL) */
	    {
	      scaled_width  = orig_width * scale;
	      scaled_height = orig_height;
	    }
	}

      retval = gdk_pixbuf_scale_simple (pixbuf,
					scaled_width,
					scaled_height,
					GDK_INTERP_BILINEAR);
    }

  return retval;
}

static void
netstatus_icon_scale_icons (NetstatusIcon  *icon,
			    int             size)
{
  int i;

  g_return_if_fail (size > 0);

  if (!icon->priv->icons [0])
    netstatus_icon_init_pixbufs (icon);

  for (i = 0; i < NETSTATUS_STATE_LAST; i++)
    {
      if (icon->priv->scaled_icons [i])
	g_object_unref (icon->priv->scaled_icons [i]);
      icon->priv->scaled_icons [i] = scale_pixbuf (icon->priv->icons [i],
						   icon->priv->orientation,
						   size,
						   TRUE);
    }

  for (i = 0; i < NETSTATUS_SIGNAL_LAST; i++)
    {
      if (icon->priv->scaled_signal_icons [i])
	g_object_unref (icon->priv->scaled_signal_icons [i]);
      icon->priv->scaled_signal_icons [i] = scale_pixbuf (icon->priv->rotated_signal_icons [i],
							  icon->priv->orientation,
							  size,
							  TRUE);
    }

  netstatus_icon_update_image (icon);
}

static inline GObjectClass *
get_box_class (GtkOrientation orientation)
{
  return g_type_class_peek(orientation == GTK_ORIENTATION_HORIZONTAL ? GTK_TYPE_HBOX : GTK_TYPE_VBOX);
}

#if !GTK_CHECK_VERSION(3, 0, 0)
static void
netstatus_icon_size_request (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
  NetstatusIcon *icon = NETSTATUS_ICON (widget);
  GObjectClass  *klass;

  klass = get_box_class (icon->priv->orientation);

  if (GTK_WIDGET_CLASS (klass)->size_request)
    GTK_WIDGET_CLASS (klass)->size_request (widget, requisition);
}
// FIXME: for GTK+ 3.0 make get_preferred_{width,height} callbacks!
#endif

static void
netstatus_icon_size_allocate (GtkWidget     *widget,
			      GtkAllocation *allocation)
{
  NetstatusIcon *icon = (NetstatusIcon *) widget;
  GtkAllocation  child_allocation;
  GObjectClass  *klass;
  int            size;
  GdkWindow     *window = gtk_widget_get_window(widget);
  guint border_width = gtk_container_get_border_width(GTK_CONTAINER (widget));

  if (icon->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    size = allocation->height;
  else
    size = allocation->width;

  if (icon->priv->size != size && size > 1)
    {
      icon->priv->size = size;

      netstatus_icon_scale_icons (icon, size);
    }

  if (gtk_widget_get_realized(widget))
    {
      gdk_window_move_resize (window,
                              allocation->x + border_width,
                              allocation->y + border_width,
                              MAX (allocation->width - border_width * 2, 0),
			                  MAX (allocation->height - border_width * 2, 0));
    }

  klass = get_box_class (icon->priv->orientation);

  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width  = MAX (allocation->width  - border_width * 2, 0);
  child_allocation.height = MAX (allocation->height - border_width * 2, 0);

  if (GTK_WIDGET_CLASS (klass)->size_allocate)
    GTK_WIDGET_CLASS (klass)->size_allocate (widget, &child_allocation);

  gtk_widget_get_allocation(widget, allocation);
}

static void
netstatus_icon_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  int           attributes_mask;
  guint         border_width;
  GtkAllocation allocation;
  GdkWindow    *window;
  GtkStyle     *style;

#if GTK_CHECK_VERSION(2, 20, 0)
  gtk_widget_set_realized(widget, TRUE);
#else
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
#endif

  gtk_widget_get_allocation (widget, &allocation);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  attributes.x = allocation.x + border_width;
  attributes.y = allocation.y + border_width;
  attributes.width = allocation.width - 2 * border_width;
  attributes.height = allocation.height - 2 * border_width;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
#if !GTK_CHECK_VERSION(3, 0, 0)
  attributes.colormap = gtk_widget_get_colormap (widget);
#endif
  attributes.event_mask = gtk_widget_get_events (widget) |
                          GDK_BUTTON_MOTION_MASK         |
                          GDK_BUTTON_PRESS_MASK          |
                          GDK_BUTTON_RELEASE_MASK        |
                          GDK_EXPOSURE_MASK              |
                          GDK_ENTER_NOTIFY_MASK          |
                          GDK_LEAVE_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
#if !GTK_CHECK_VERSION(3, 0, 0)
   attributes_mask |= GDK_WA_COLORMAP;
#endif

  window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gdk_window_set_user_data (window, widget);

  gtk_widget_ensure_style (widget);
  style = gtk_widget_get_style (widget);
  gtk_style_set_background (style, window, GTK_STATE_NORMAL);
}

static gboolean
netstatus_icon_button_press_event (GtkWidget      *widget,
				   GdkEventButton *event)
{
  if (event->button == 1)
    {
      netstatus_icon_invoke (NETSTATUS_ICON (widget));
      return TRUE;
    }

  return FALSE;
}
#if 0
static void
netstatus_icon_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
  NetstatusIcon *icon;

  g_return_if_fail (NETSTATUS_IS_ICON (object));

  icon = NETSTATUS_ICON (object);

  switch (prop_id)
    {
    case PROP_IFACE:
      netstatus_icon_set_iface (icon, g_value_get_object (value));
      break;
    case PROP_ORIENTATION:
      netstatus_icon_set_orientation (icon, g_value_get_enum (value));
      break;
    case PROP_TOOLTIPS_ENABLED:
      netstatus_icon_set_tooltips_enabled (icon, g_value_get_boolean (value));
      break;
    case PROP_SHOW_SIGNAL:
      netstatus_icon_set_show_signal (icon, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
netstatus_icon_get_property (GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
  NetstatusIcon *icon;

  g_return_if_fail (NETSTATUS_IS_ICON (object));

  icon = NETSTATUS_ICON (object);

  switch (prop_id)
    {
    case PROP_IFACE:
      g_value_set_object (value, icon->priv->iface);
      break;
    case PROP_ORIENTATION:
      g_value_set_enum (value, icon->priv->orientation);
      break;
    case PROP_TOOLTIPS_ENABLED:
      g_value_set_boolean (value, icon->priv->tooltips_enabled);
      break;
    case PROP_SHOW_SIGNAL:
      g_value_set_boolean (value, icon->priv->show_signal);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}
#endif

static void
netstatus_icon_finalize (GObject *object)
{
  NetstatusIcon *icon = (NetstatusIcon *) object;
  int            i;

  if (icon->priv->iface)
    g_object_unref (icon->priv->iface);
  icon->priv->iface = NULL;

  for (i = 0; i < NETSTATUS_STATE_LAST; i++)
    {
      if (icon->priv->icons [i])
	g_object_unref (icon->priv->icons [i]);
      icon->priv->icons [i] = NULL;

      if (icon->priv->scaled_icons [i])
	g_object_unref (icon->priv->scaled_icons [i]);
      icon->priv->scaled_icons [i] = NULL;
    }

  g_free (icon->priv);
  icon->priv = NULL;

  parent_class->finalize (object);
}

static void
netstatus_icon_class_init (NetstatusIconClass *klass)
{
  GObjectClass   *gobject_class   = (GObjectClass   *) klass;
#if !GTK_CHECK_VERSION(3, 0, 0)
  GtkObjectClass *gtkobject_class = (GtkObjectClass *) klass;
#endif
  GtkWidgetClass *widget_class    = (GtkWidgetClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  /*
  gobject_class->set_property = netstatus_icon_set_property;
  gobject_class->get_property = netstatus_icon_get_property;
  */

  gobject_class->finalize     = netstatus_icon_finalize;

#if GTK_CHECK_VERSION(3, 0, 0)
  widget_class->destroy = netstatus_icon_destroy;
#else
  gtkobject_class->destroy = netstatus_icon_destroy;
#endif

#if !GTK_CHECK_VERSION(3, 0, 0)
  widget_class->size_request       = netstatus_icon_size_request;
  // FIXME: for GTK+ 3.0 make get_preferred_{width,height} callbacks!
#endif
  widget_class->size_allocate      = netstatus_icon_size_allocate;
  widget_class->realize            = netstatus_icon_realize;
  widget_class->button_press_event = netstatus_icon_button_press_event;
#if 0
  g_object_class_install_property (gobject_class,
				   PROP_IFACE,
				   g_param_spec_object ("iface",
							_("Interface"),
							_("The current interface the icon is monitoring."),
							NETSTATUS_TYPE_IFACE,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
                                   PROP_ORIENTATION,
                                   g_param_spec_enum ("orientation",
                                                      _("Orientation"),
                                                      _("The orientation of the tray."),
                                                      GTK_TYPE_ORIENTATION,
                                                      GTK_ORIENTATION_HORIZONTAL,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
                                   PROP_TOOLTIPS_ENABLED,
                                   g_param_spec_boolean ("tooltips-enabled",
							 _("Tooltips Enabled"),
							 _("Whether or not the icon's tooltips are enabled."),
							 TRUE,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_SIGNAL,
                                   g_param_spec_boolean ("show-signal",
							 _("Show Signal"),
							 _("Whether or not the signal strength should be displayed."),
							 TRUE,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
#endif

  icon_signals [INVOKED] =
    g_signal_new ("invoked",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (NetstatusIconClass, invoked),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
netstatus_icon_instance_init (NetstatusIcon      *icon,
			      NetstatusIconClass *klass __attribute__((unused)))
{
  icon->priv = g_new0 (NetstatusIconPrivate, 1);

  gtk_widget_set_has_window(GTK_WIDGET(icon), TRUE);

  icon->priv->iface            = NULL;
  icon->priv->state            = NETSTATUS_STATE_DISCONNECTED;
  icon->priv->orientation      = GTK_ORIENTATION_HORIZONTAL;
  icon->priv->size             = 0;
  icon->priv->state_changed_id = 0;

  gtk_box_set_spacing (GTK_BOX (icon), 3);

  icon->priv->image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (icon), icon->priv->image);
  gtk_widget_show (icon->priv->image);

  icon->priv->signal_image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (icon), icon->priv->signal_image);
  gtk_widget_hide (icon->priv->signal_image);

  gtk_widget_add_events (GTK_WIDGET (icon),
			 GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
}

GType
netstatus_icon_get_type (void)
{
  static GType type = 0;
  if (!type)
    {
      static const GTypeInfo info =
        {
            sizeof (NetstatusIconClass),
            NULL,
            NULL,
            (GClassInitFunc) netstatus_icon_class_init,
            NULL,
            NULL,
            sizeof (NetstatusIcon),
            0,
            (GInstanceInitFunc) netstatus_icon_instance_init,
            NULL
        };
      type = g_type_register_static (GTK_TYPE_BOX, "NetstatusIcon", &info, 0);
    }
  return type;
}

GtkWidget *
netstatus_icon_new (NetstatusIface *iface)
{
    GtkWidget* obj = g_object_new (NETSTATUS_TYPE_ICON,
                             /*"iface", iface,*/
                             NULL);
    netstatus_icon_set_iface( NETSTATUS_ICON(obj), iface );
    return obj;
}

void
netstatus_icon_invoke (NetstatusIcon *icon)
{
  g_return_if_fail (NETSTATUS_IS_ICON (icon));

  if (netstatus_iface_get_state (icon->priv->iface) != NETSTATUS_STATE_ERROR)
    {
      if (icon->priv->error_dialog)
	gtk_widget_destroy (icon->priv->error_dialog);
      icon->priv->error_dialog = NULL;

      g_signal_emit (icon, icon_signals [INVOKED], 0);
    }
  else
    {
      const GError *error;

      error = netstatus_iface_get_error (icon->priv->iface);
      g_assert (error != NULL);

      if (icon->priv->error_dialog)
        {
	  gtk_window_set_screen (GTK_WINDOW (icon->priv->error_dialog),
				 gtk_widget_get_screen (GTK_WIDGET (icon)));
          gtk_window_present (GTK_WINDOW (icon->priv->error_dialog));
          return;
        }

      icon->priv->error_dialog =
	gtk_message_dialog_new (NULL, 0,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("Please contact your system administrator to resolve the following problem:\n\n%s"),
				error->message);

      gtk_window_set_screen (GTK_WINDOW (icon->priv->error_dialog),
			     gtk_widget_get_screen (GTK_WIDGET (icon)));

      g_signal_connect (icon->priv->error_dialog, "response",
			G_CALLBACK (gtk_widget_destroy), NULL);
      g_signal_connect (icon->priv->error_dialog, "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
			&icon->priv->error_dialog);

      gtk_widget_show (icon->priv->error_dialog);
    }
}

void
netstatus_icon_set_iface (NetstatusIcon  *icon,
			  NetstatusIface *iface)
{

  g_return_if_fail (NETSTATUS_IS_ICON (icon));

  if (icon->priv->iface != iface)
    {
      NetstatusIface *old_iface;

      old_iface = icon->priv->iface;

      if (icon->priv->state_changed_id)
	{
	  g_assert (icon->priv->iface != NULL);
	  g_assert (icon->priv->name_changed_id != 0);
	  g_signal_handler_disconnect (icon->priv->iface,
				       icon->priv->state_changed_id);
	  g_signal_handler_disconnect (icon->priv->iface,
				       icon->priv->name_changed_id);
	  g_signal_handler_disconnect (icon->priv->iface,
				       icon->priv->wireless_changed_id);
	  g_signal_handler_disconnect (icon->priv->iface,
				       icon->priv->signal_changed_id);
	}

      if (iface)
	g_object_ref (iface);
      icon->priv->iface = iface;

      if (old_iface)
	g_object_unref (old_iface);

      icon->priv->state_changed_id     = g_signal_connect (icon->priv->iface, "notify::state",
							   G_CALLBACK (netstatus_icon_state_changed), icon);
      icon->priv->name_changed_id      = g_signal_connect (icon->priv->iface, "notify::name",
							   G_CALLBACK (netstatus_icon_name_changed), icon);
      icon->priv->wireless_changed_id  = g_signal_connect (icon->priv->iface, "notify::wireless",
							   G_CALLBACK (netstatus_icon_is_wireless_changed), icon);
      icon->priv->signal_changed_id    = g_signal_connect (icon->priv->iface, "notify::signal-strength",
							   G_CALLBACK (netstatus_icon_signal_changed), icon);

      netstatus_icon_state_changed       (icon->priv->iface, NULL, icon);
      netstatus_icon_name_changed        (icon->priv->iface, NULL, icon);
      netstatus_icon_is_wireless_changed (icon->priv->iface, NULL, icon);
      netstatus_icon_signal_changed      (icon->priv->iface, NULL, icon);

      /* g_object_notify (G_OBJECT (icon), "iface"); */
    }
}

NetstatusIface *
netstatus_icon_get_iface (NetstatusIcon *icon)
{
  g_return_val_if_fail (NETSTATUS_IS_ICON (icon), 0);

  return icon->priv->iface;
}

void
netstatus_icon_set_orientation (NetstatusIcon  *icon,
				GtkOrientation  orientation)
{
  g_return_if_fail (NETSTATUS_IS_ICON (icon));

  if (icon->priv->orientation != orientation)
    {
      icon->priv->orientation = orientation;

      netstatus_icon_rotate_signal_icons (icon, orientation);
      netstatus_icon_update_image (icon);

      icon->priv->size = -1;

      gtk_widget_queue_resize (GTK_WIDGET (icon));

      g_object_notify (G_OBJECT (icon), "orientation");
    }
}

GtkOrientation
netstatus_icon_get_orientation (NetstatusIcon *icon)
{
  g_return_val_if_fail (NETSTATUS_IS_ICON (icon), GTK_ORIENTATION_HORIZONTAL);

  return icon->priv->orientation;
}

void
netstatus_icon_set_tooltips_enabled (NetstatusIcon *icon,
				     gboolean       enabled)
{
  g_return_if_fail (NETSTATUS_IS_ICON (icon));

  enabled = enabled != FALSE;

  if (icon->priv->tooltips_enabled != enabled)
    {
      icon->priv->tooltips_enabled = enabled;

      g_object_notify (G_OBJECT (icon), "tooltips-enabled");
    }
}

gboolean
netstatus_icon_get_tooltips_enabled (NetstatusIcon *icon)
{
  g_return_val_if_fail (NETSTATUS_ICON (icon), TRUE);

  return icon->priv->tooltips_enabled;
}

void
netstatus_icon_set_show_signal (NetstatusIcon *icon,
				gboolean       show_signal)
{
  g_return_if_fail (NETSTATUS_IS_ICON (icon));

  show_signal = show_signal != FALSE;

  if (icon->priv->show_signal != show_signal)
    {
      icon->priv->show_signal = show_signal;

      if (show_signal && netstatus_iface_get_is_wireless (icon->priv->iface))
	gtk_widget_show (icon->priv->signal_image);
      else
	gtk_widget_hide (icon->priv->signal_image);

      /* g_object_notify (G_OBJECT (icon), "show-signal"); */
    }
}

gboolean
netstatus_icon_get_show_signal (NetstatusIcon *icon)
{
  g_return_val_if_fail (NETSTATUS_ICON (icon), TRUE);

  return icon->priv->show_signal;
}
