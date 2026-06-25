/*
 * Copyright (C) 2006-2007 Jasper Huijsmans <jasper@xfce.org>
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>

#include <common/panel-private.h>
#include <libxfce4panel/xfce-panel-macros.h>
#include <libxfce4panel/xfce-arrow-button.h>
#include <libxfce4panel/libxfce4panel-alias.h>



/**
 * SECTION: xfce-arrow-button
 * @title: XfceArrowButton
 * @short_description: Toggle button with arrow
 * @include: libxfce4panel/libxfce4panel.h
 *
 * Toggle button with (optional) arrow. The arrow direction will be
 * inverted when the button is toggled.
 * Since 4.8 it is also possible to make the button blink and pack additional
 * widgets in the button, using gtk_container_add().
 **/



#define ARROW_WIDTH        (8)
#define MAX_BLINKING_COUNT (16)

enum
{
  ARROW_TYPE_CHANGED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_ARROW_TYPE
};

static void     xfce_arrow_button_set_property         (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void     xfce_arrow_button_get_property         (GObject               *object,
                                                        guint                  prop_id,
                                                        GValue                *value,
                                                        GParamSpec            *pspec);
static void     xfce_arrow_button_dispose              (GObject               *object);
static gboolean xfce_arrow_button_draw                 (GtkWidget             *widget,
                                                        cairo_t               *cr);
static void     xfce_arrow_button_get_preferred_width  (GtkWidget             *widget,
                                                        gint                  *minimum_width,
                                                        gint                  *natural_width);
static void     xfce_arrow_button_get_preferred_height (GtkWidget             *widget,
                                                        gint                  *minimum_height,
                                                        gint                  *natural_height);
static void     xfce_arrow_button_size_allocate        (GtkWidget             *widget,
                                                        GtkAllocation         *allocation);



struct _XfceArrowButtonPrivate
{
  /* arrow type of the button */
  GtkArrowType   arrow_type;

  /* blinking timeout id */
  guint          blinking_timeout_id;

  /* counter to make the blinking stop when
   * MAX_BLINKING_COUNT is reached */
  guint          blinking_counter;

  /* button style overridden when blinking */
  GtkReliefStyle style;
};



static guint arrow_button_signals[LAST_SIGNAL];



G_DEFINE_TYPE_WITH_PRIVATE (XfceArrowButton, xfce_arrow_button, GTK_TYPE_TOGGLE_BUTTON)



static void
xfce_arrow_button_class_init (XfceArrowButtonClass * klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = xfce_arrow_button_get_property;
  gobject_class->set_property = xfce_arrow_button_set_property;
  gobject_class->dispose = xfce_arrow_button_dispose;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->draw = xfce_arrow_button_draw;
  gtkwidget_class->get_preferred_width = xfce_arrow_button_get_preferred_width;
  gtkwidget_class->get_preferred_height = xfce_arrow_button_get_preferred_height;
  gtkwidget_class->size_allocate = xfce_arrow_button_size_allocate;

  /**
   * XfceArrowButton::arrow-type-changed
   * @button: the object which emitted the signal
   * @type: the new #GtkArrowType of the button
   *
   * Emitted when the arrow direction of the menu button changes.
   * This value also determines the direction of the popup menu.
   **/
  arrow_button_signals[ARROW_TYPE_CHANGED] =
      g_signal_new (g_intern_static_string ("arrow-type-changed"),
                    G_OBJECT_CLASS_TYPE (klass),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (XfceArrowButtonClass, arrow_type_changed),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__ENUM,
                    G_TYPE_NONE, 1, GTK_TYPE_ARROW_TYPE);

  /**
   * XfceArrowButton:arrow-type
   *
   * The arrow type of the button. This value also determines the direction
   * of the popup menu.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ARROW_TYPE,
                                   g_param_spec_enum ("arrow-type",
                                                      "Arrow type",
                                                      "The arrow type of the menu button",
                                                      GTK_TYPE_ARROW_TYPE,
                                                      GTK_ARROW_UP,
                                                      G_PARAM_READWRITE
                                                      | G_PARAM_STATIC_STRINGS));
}



static void
xfce_arrow_button_init (XfceArrowButton *button)
{
  GtkStyleContext *context;
  GtkCssProvider  *provider;

  button->priv = xfce_arrow_button_get_instance_private (button);

  /* initialize button values */
  button->priv->arrow_type = GTK_ARROW_UP;
  button->priv->blinking_timeout_id = 0;
  button->priv->blinking_counter = 0;
  button->priv->style = GTK_RELIEF_NORMAL;

  /* set some widget properties */
  gtk_widget_set_has_window (GTK_WIDGET (button), FALSE);
  gtk_widget_set_can_default (GTK_WIDGET (button), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (button), FALSE);
  gtk_widget_set_focus_on_click (GTK_WIDGET (button), FALSE);
  /* Make sure themes like Adwaita, which set excessive padding, don't cause the
     launcher buttons to overlap when panels have a fairly normal size */
  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, ".xfce4-panel button { padding: 0; }", -1, NULL);
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);
}



static void
xfce_arrow_button_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  XfceArrowButton *button = XFCE_ARROW_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ARROW_TYPE:
      xfce_arrow_button_set_arrow_type (button, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_arrow_button_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  XfceArrowButton *button = XFCE_ARROW_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ARROW_TYPE:
      g_value_set_enum (value, xfce_arrow_button_get_arrow_type (button));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_arrow_button_dispose (GObject *object)
{
  XfceArrowButton *button = XFCE_ARROW_BUTTON (object);

  if (button->priv->blinking_timeout_id != 0)
    {
      g_source_remove (button->priv->blinking_timeout_id);
      button->priv->blinking_timeout_id = 0;
    }

  (*G_OBJECT_CLASS (xfce_arrow_button_parent_class)->dispose) (object);
}



static gboolean
xfce_arrow_button_draw (GtkWidget *widget,
                        cairo_t   *cr)
{
  XfceArrowButton *button = XFCE_ARROW_BUTTON (widget);
  GtkWidget       *child;
  gdouble          x, y, width;
  GtkAllocation    alloc;
  gdouble          angle;
  GtkStyleContext *context;
  GtkBorder        padding, border;
  GdkRGBA          fg_rgba;

  /* draw the button */
  (*GTK_WIDGET_CLASS (xfce_arrow_button_parent_class)->draw) (widget, cr);

  if (button->priv->arrow_type != GTK_ARROW_NONE
      && gtk_widget_is_drawable (widget))
    {
      gtk_widget_get_allocation (widget, &alloc);
      child = gtk_bin_get_child (GTK_BIN (widget));
      context = gtk_widget_get_style_context (widget);
      gtk_style_context_get_padding (context, gtk_widget_get_state_flags (widget), &padding);
      gtk_style_context_get_border (context, gtk_widget_get_state_flags (widget), &border);

      if (child != NULL
          && gtk_widget_get_visible (child))
        {
          if (button->priv->arrow_type == GTK_ARROW_UP
              || button->priv->arrow_type == GTK_ARROW_DOWN)
            {
              width = (gdouble) ARROW_WIDTH;
              x = (gdouble) padding.left + border.left;
              y = (gdouble) (alloc.height - width) / 2.0;
            }
          else
            {
              width = (gdouble) ARROW_WIDTH;
              x = (gdouble) (alloc.width - width) / 2.0;
              y = (gdouble) padding.top + border.top;
            }
        }
      else
        {
          width = (gdouble) MIN (alloc.height - padding.top - padding.bottom - border.top - border.bottom,
                                 alloc.width  - padding.left - padding.right - border.left - border.right);
          width = (gdouble) CLAMP (width, 0.0, (gdouble) ARROW_WIDTH);

          x = (gdouble) (alloc.width - width) / 2.0;
          y = (gdouble) (alloc.height - width) / 2.0;
        }

      switch (button->priv->arrow_type)
        {
        case GTK_ARROW_DOWN:  angle = G_PI;
          break;
        case GTK_ARROW_LEFT:  angle = G_PI / 2.0 + G_PI;
          break;
        case GTK_ARROW_RIGHT: angle = G_PI / 2.0;
          break;
        default:              angle = 0.0;
        }
      gtk_style_context_get_color (context, gtk_widget_get_state_flags (widget), &fg_rgba);
      gdk_cairo_set_source_rgba (cr, &fg_rgba);
      if (width > 0)
        gtk_render_arrow (context, cr, angle, x, y, width);
    }

  return FALSE;
}



static void
xfce_arrow_button_get_preferred_width (GtkWidget *widget,
                                       gint      *minimum_width,
                                       gint      *natural_width)
{
  XfceArrowButton *button = XFCE_ARROW_BUTTON (widget);
  GtkWidget       *child;
  gint             minimum_child_width, natural_child_width;
  GtkStyleContext *context;
  GtkBorder        padding, border;

  minimum_child_width = natural_child_width = 0;
  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child != NULL
      && gtk_widget_get_visible (child))
    {
      /* use gtk for the widget size */
      (*GTK_WIDGET_CLASS (xfce_arrow_button_parent_class)->get_preferred_width) (widget, &minimum_child_width, &natural_child_width);

      /* reserve space for the arrow */
      switch (button->priv->arrow_type)
        {
        case GTK_ARROW_UP:
        case GTK_ARROW_DOWN:
          natural_child_width += ARROW_WIDTH;
          break;

        default:
          break;
        }
    }
  else if (button->priv->arrow_type != GTK_ARROW_NONE)
    {
      /* style thickness */
      context = gtk_widget_get_style_context (widget);
      gtk_style_context_get_padding (context, gtk_widget_get_state_flags (widget), &padding);
      gtk_style_context_get_border (context, gtk_widget_get_state_flags (widget), &border);
      minimum_child_width = natural_child_width = (ARROW_WIDTH + padding.left + padding.right + border.left + border.right);
    }

  if (minimum_width != NULL)
    *minimum_width = minimum_child_width;

  if (natural_width != NULL)
    *natural_width = natural_child_width;
}



static void
xfce_arrow_button_get_preferred_height (GtkWidget *widget,
                                        gint      *minimum_height,
                                        gint      *natural_height)
{
  XfceArrowButton *button = XFCE_ARROW_BUTTON (widget);
  GtkWidget       *child;
  gint             minimum_child_height, natural_child_height;
  GtkStyleContext *context;
  GtkBorder        padding, border;

  minimum_child_height = natural_child_height = 0;
  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child != NULL
      && gtk_widget_get_visible (child))
    {
      /* use gtk for the widget size */
      (*GTK_WIDGET_CLASS (xfce_arrow_button_parent_class)->get_preferred_height) (widget, &minimum_child_height, &natural_child_height);

      /* reserve space for the arrow */
      switch (button->priv->arrow_type)
        {
        case GTK_ARROW_LEFT:
        case GTK_ARROW_RIGHT:
          natural_child_height += ARROW_WIDTH;
          break;

        default:
          break;
        }
    }
  else if (button->priv->arrow_type != GTK_ARROW_NONE)
    {
      /* style thickness */
      context = gtk_widget_get_style_context (widget);
      gtk_style_context_get_padding (context, gtk_widget_get_state_flags (widget), &padding);
      gtk_style_context_get_border (context, gtk_widget_get_state_flags (widget), &border);
      minimum_child_height = natural_child_height = (ARROW_WIDTH + padding.top + padding.bottom + border.top + border.bottom);
    }


  if (minimum_height != NULL)
    *minimum_height = minimum_child_height;

  if (natural_height != NULL)
    *natural_height = natural_child_height;
}



static void
xfce_arrow_button_size_allocate (GtkWidget     *widget,
                                 GtkAllocation *allocation)
{
  XfceArrowButton *button = XFCE_ARROW_BUTTON (widget);
  GtkWidget       *child;
  GtkAllocation    child_allocation;

  /* allocate the button */
  (*GTK_WIDGET_CLASS (xfce_arrow_button_parent_class)->size_allocate) (widget, allocation);

  if (button->priv->arrow_type != GTK_ARROW_NONE)
    {
      child = gtk_bin_get_child (GTK_BIN (widget));
      if (child != NULL
          && gtk_widget_get_visible (child))
        {
          /* copy the child allocation */
          gtk_widget_get_allocation (child, &child_allocation);

          /* update the allocation to make space for the arrow */
          switch (button->priv->arrow_type)
            {
            case GTK_ARROW_LEFT:
            case GTK_ARROW_RIGHT:
              child_allocation.height -= ARROW_WIDTH;
              child_allocation.y += ARROW_WIDTH;
              break;

            default:
              child_allocation.width -= ARROW_WIDTH;
              child_allocation.x += ARROW_WIDTH;
              break;
            }

          /* set the child allocation again */
          gtk_widget_size_allocate (child, &child_allocation);
        }
    }
}



static gboolean
xfce_arrow_button_blinking_timeout (gpointer user_data)
{
  XfceArrowButton *button = XFCE_ARROW_BUTTON (user_data);
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  if (gtk_style_context_has_class (context, GTK_STYLE_CLASS_SUGGESTED_ACTION))
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_SUGGESTED_ACTION);
  else
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_SUGGESTED_ACTION);

  return (button->priv->blinking_counter++ < MAX_BLINKING_COUNT);
}



static void
xfce_arrow_button_blinking_timeout_destroyed (gpointer user_data)
{
  XfceArrowButton *button = XFCE_ARROW_BUTTON (user_data);
  GtkWidget       *plugin;

  button->priv->blinking_timeout_id = 0;
  button->priv->blinking_counter = 0;

  /* release our lock */
  plugin = gtk_widget_get_ancestor (GTK_WIDGET (button), XFCE_TYPE_PANEL_PLUGIN);
  xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (plugin), FALSE);
}



/**
 * xfce_arrow_button_new:
 * @arrow_type : #GtkArrowType for the arrow button
 *
 * Creates a new #XfceArrowButton widget.
 *
 * Returns: The newly created #XfceArrowButton widget.
 **/
GtkWidget *
xfce_arrow_button_new (GtkArrowType arrow_type)
{
  return g_object_new (XFCE_TYPE_ARROW_BUTTON,
                       "arrow-type", arrow_type,
                       NULL);
}



/**
 * xfce_arrow_button_get_arrow_type:
 * @button : a #XfceArrowButton
 *
 * Returns the value of the ::arrow-type property.
 *
 * Returns: the #GtkArrowType of @button.
 **/
GtkArrowType
xfce_arrow_button_get_arrow_type (XfceArrowButton *button)
{
  g_return_val_if_fail (XFCE_IS_ARROW_BUTTON (button), GTK_ARROW_UP);
  return button->priv->arrow_type;
}



/**
 * xfce_arrow_button_set_arrow_type:
 * @button     : a #XfceArrowButton
 * @arrow_type : a valid  #GtkArrowType
 *
 * Sets the arrow type for @button.
 **/
void
xfce_arrow_button_set_arrow_type (XfceArrowButton *button,
                                  GtkArrowType     arrow_type)
{
  g_return_if_fail (XFCE_IS_ARROW_BUTTON (button));

  if (G_LIKELY (button->priv->arrow_type != arrow_type))
    {
      /* store the new arrow type */
      button->priv->arrow_type = arrow_type;

      /* emit signal */
      g_signal_emit (G_OBJECT (button),
                     arrow_button_signals[ARROW_TYPE_CHANGED],
                     0, arrow_type);

      /* notify property change */
      g_object_notify (G_OBJECT (button), "arrow-type");

      /* redraw the arrow button */
      gtk_widget_queue_resize (GTK_WIDGET (button));
    }
}



/**
 * xfce_arrow_button_get_blinking:
 * @button : a #XfceArrowButton
 *
 * Whether the button is blinking. If the blink timeout is finished
 * and the button is still highlighted, this functions returns %TRUE.
 *
 * Returns: %TRUE when @button is blinking.
 *
 * Since: 4.8
 **/
gboolean
xfce_arrow_button_get_blinking (XfceArrowButton *button)
{
  g_return_val_if_fail (XFCE_IS_ARROW_BUTTON (button), FALSE);
  return button->priv->blinking_timeout_id != 0
         || gtk_style_context_has_class (gtk_widget_get_style_context (GTK_WIDGET (button)),
                                         GTK_STYLE_CLASS_SUGGESTED_ACTION);
}



/**
 * xfce_arrow_button_set_blinking:
 * @button   : a #XfceArrowButton
 * @blinking : %TRUE when the button should start blinking, %FALSE to
 *             stop the blinking.
 *
 * Make the button blink.
 *
 * Since: 4.8
 **/
void
xfce_arrow_button_set_blinking (XfceArrowButton *button,
                                gboolean         blinking)
{
  g_return_if_fail (XFCE_IS_ARROW_BUTTON (button));

  if (blinking)
    {
      if (! xfce_arrow_button_get_blinking (button))
        {
          GtkWidget *plugin;

          /* raise the panel if needed, or keep it raised */
          plugin = gtk_widget_get_ancestor (GTK_WIDGET (button), XFCE_TYPE_PANEL_PLUGIN);
          xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (plugin), TRUE);

          /* override button style so blinking is visible */
          button->priv->style = gtk_button_get_relief (GTK_BUTTON (button));
          gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NORMAL);

          /* start blinking timeout */
          button->priv->blinking_timeout_id =
              gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT_IDLE, 500,
                                            xfce_arrow_button_blinking_timeout, button,
                                            xfce_arrow_button_blinking_timeout_destroyed);
          xfce_arrow_button_blinking_timeout (button);
        }
    }
  else
    {
      GtkStyleContext *context;

      /* stop the blinking timeout */
      if (button->priv->blinking_timeout_id != 0)
        g_source_remove (button->priv->blinking_timeout_id);

      /* remove css class */
      context = gtk_widget_get_style_context (GTK_WIDGET (button));
      if (gtk_style_context_has_class (context, GTK_STYLE_CLASS_SUGGESTED_ACTION))
        gtk_style_context_remove_class (context, GTK_STYLE_CLASS_SUGGESTED_ACTION);

      /* restore button style */
      gtk_button_set_relief (GTK_BUTTON (button), button->priv->style);
    }
}



#define __XFCE_ARROW_BUTTON_C__
#include <libxfce4panel/libxfce4panel-aliasdef.c>
