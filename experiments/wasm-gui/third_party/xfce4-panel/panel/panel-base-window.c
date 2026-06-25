/*
 * Copyright (C) 2009-2010 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin-provider.h>
#include <common/panel-private.h>
#include <common/panel-debug.h>
#include <panel/panel-base-window.h>
#include <panel/panel-window.h>
#include <panel/panel-plugin-external.h>



#define PANEL_BASE_CSS        ".xfce4-panel.background { border-style: solid; }"\
                              ".xfce4-panel.background button { background: transparent; padding: 0; }"\
                              ".xfce4-panel.background.marching-ants-dashed { border: 1px dashed #ff0000; }"\
                              ".xfce4-panel.background.marching-ants-dotted { border: 1px dotted #ff0000; }"
#define MARCHING_ANTS_DASHED  "marching-ants-dashed"
#define MARCHING_ANTS_DOTTED  "marching-ants-dotted"


static void     panel_base_window_get_property                (GObject              *object,
                                                               guint                 prop_id,
                                                               GValue               *value,
                                                               GParamSpec           *pspec);
static void     panel_base_window_set_property                (GObject              *object,
                                                               guint                 prop_id,
                                                               const GValue         *value,
                                                               GParamSpec           *pspec);
static void     panel_base_window_finalize                    (GObject              *object);
static void     panel_base_window_screen_changed              (GtkWidget            *widget,
                                                               GdkScreen            *previous_screen);
static void     panel_base_window_composited_changed          (GdkScreen            *screen,
                                                               GtkWidget            *widget);
static gboolean panel_base_window_active_timeout              (gpointer              user_data);
static void     panel_base_window_active_timeout_destroyed    (gpointer              user_data);
static void     panel_base_window_set_background_color_css    (PanelBaseWindow      *window);
static void     panel_base_window_set_background_image_css    (PanelBaseWindow      *window);
static void     panel_base_window_set_background_css          (PanelBaseWindow      *window,
                                                               const gchar          *css_string);
static void     panel_base_window_set_plugin_data             (PanelBaseWindow      *window,
                                                               GtkCallback           func);
static void     panel_base_window_set_plugin_opacity          (GtkWidget            *widget,
                                                               gpointer              user_data,
                                                               gdouble               opacity);
static void     panel_base_window_set_plugin_enter_opacity    (GtkWidget            *widget,
                                                               gpointer              user_data);
static void     panel_base_window_set_plugin_leave_opacity    (GtkWidget            *widget,
                                                               gpointer              user_data);
static void     panel_base_window_set_plugin_background_color (GtkWidget            *widget,
                                                               gpointer              user_data);
static void     panel_base_window_set_plugin_background_image (GtkWidget            *widget,
                                                               gpointer              user_data);



enum
{
  PROP_0,
  PROP_ENTER_OPACITY,
  PROP_LEAVE_OPACITY,
  PROP_BORDERS,
  PROP_ACTIVE,
  PROP_COMPOSITED,
  PROP_BACKGROUND_STYLE,
  PROP_BACKGROUND_RGBA,
  PROP_BACKGROUND_IMAGE
};

struct _PanelBaseWindowPrivate
{
  PanelBorders     borders;

  /* background css style provider */
  GtkCssProvider  *css_provider;

  /* active window timeout id */
  guint            active_timeout_id;
};



G_DEFINE_TYPE_WITH_PRIVATE (PanelBaseWindow, panel_base_window, GTK_TYPE_WINDOW)



static void
panel_base_window_class_init (PanelBaseWindowClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = panel_base_window_get_property;
  gobject_class->set_property = panel_base_window_set_property;
  gobject_class->finalize = panel_base_window_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->screen_changed = panel_base_window_screen_changed;

  g_object_class_install_property (gobject_class,
                                   PROP_ENTER_OPACITY,
                                   g_param_spec_uint ("enter-opacity",
                                                      NULL, NULL,
                                                      0, 100, 100,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_LEAVE_OPACITY,
                                   g_param_spec_uint ("leave-opacity",
                                                      NULL, NULL,
                                                      0, 100, 100,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_BACKGROUND_STYLE,
                                   g_param_spec_uint ("background-style",
                                                      NULL, NULL,
                                                      PANEL_BG_STYLE_NONE,
                                                      PANEL_BG_STYLE_IMAGE,
                                                      PANEL_BG_STYLE_NONE,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_BACKGROUND_RGBA,
                                   g_param_spec_boxed ("background-rgba",
                                                       NULL, NULL,
                                                       GDK_TYPE_RGBA,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_BACKGROUND_IMAGE,
                                   g_param_spec_string ("background-image",
                                                        NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_BORDERS,
                                   g_param_spec_uint ("borders",
                                                      NULL, NULL,
                                                      0, G_MAXUINT,
                                                      PANEL_BORDER_NONE,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_COMPOSITED,
                                   g_param_spec_boolean ("composited",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}



static void
panel_base_window_scale_factor_changed (PanelBaseWindow *window)
{
  if (window->background_style == PANEL_BG_STYLE_IMAGE && window->background_image != NULL)
    panel_base_window_set_background_image_css (window);
}



static void
panel_base_window_init (PanelBaseWindow *window)
{
  GtkStyleContext *context;
  GdkScreen       *screen;

  window->priv = panel_base_window_get_instance_private (window);

  window->is_composited = FALSE;
  window->background_style = PANEL_BG_STYLE_NONE;
  window->background_image = NULL;
  window->background_rgba = NULL;
  window->enter_opacity = 1.00;
  window->leave_opacity = 1.00;
  window->opacity_is_enter = FALSE;

  window->priv->css_provider = gtk_css_provider_new ();
  window->priv->borders = PANEL_BORDER_NONE;
  window->priv->active_timeout_id = 0;

  screen = gtk_widget_get_screen (GTK_WIDGET (window));
  g_signal_connect_object (G_OBJECT (screen), "composited-changed",
                           G_CALLBACK (panel_base_window_composited_changed), window, 0);

  /* some wm require stick to show the window on all workspaces, on xfwm4
   * the type-hint already takes care of that */
  gtk_window_stick (GTK_WINDOW (window));

  /* set colormap */
  panel_base_window_screen_changed (GTK_WIDGET (window), NULL);

  /* set the panel class */
  context = gtk_widget_get_style_context (GTK_WIDGET (window));
  gtk_style_context_add_class (context, "panel");
  gtk_style_context_add_class (context, "xfce4-panel");
  g_signal_connect (window, "notify::scale-factor", G_CALLBACK (panel_base_window_scale_factor_changed), NULL);
}



static void
panel_base_window_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  PanelBaseWindow         *window = PANEL_BASE_WINDOW (object);
  PanelBaseWindowPrivate  *priv = window->priv;
  GdkRGBA                 *rgba;
  GtkStyleContext         *ctx;

  switch (prop_id)
    {
    case PROP_ENTER_OPACITY:
      g_value_set_uint (value, rint (window->enter_opacity * 100.00));
      break;

    case PROP_LEAVE_OPACITY:
      g_value_set_uint (value, rint (window->leave_opacity * 100.00));
      break;

    case PROP_BACKGROUND_STYLE:
      g_value_set_uint (value, window->background_style);
      break;

    case PROP_BACKGROUND_RGBA:
      if (window->background_rgba != NULL)
        g_value_set_boxed (value, window->background_rgba);
      else
        {
          ctx = gtk_widget_get_style_context (GTK_WIDGET (window));
          gtk_style_context_get (ctx, GTK_STATE_FLAG_NORMAL,
                                 GTK_STYLE_PROPERTY_BACKGROUND_COLOR,
                                 &rgba, NULL);
          g_value_set_boxed (value, rgba);
          gdk_rgba_free (rgba);
        }
      break;

    case PROP_BACKGROUND_IMAGE:
      g_value_set_string (value, window->background_image);
      break;

    case PROP_BORDERS:
      g_value_set_uint (value, priv->borders);
      break;

    case PROP_ACTIVE:
      g_value_set_boolean (value, !!(priv->active_timeout_id != 0));
      break;

    case PROP_COMPOSITED:
      g_value_set_boolean (value, window->is_composited);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
panel_base_window_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  PanelBaseWindow        *window = PANEL_BASE_WINDOW (object);
  PanelBaseWindowPrivate *priv = window->priv;
  PanelBgStyle            bg_style;
  GFile                  *file;
  gdouble                 opacity;
  const gchar            *str;

  switch (prop_id)
    {
    case PROP_ENTER_OPACITY:
      opacity = g_value_get_uint (value) / 100.00;
      if (opacity != window->enter_opacity)
        {
          window->enter_opacity = opacity;
          if (window->is_composited && window->opacity_is_enter)
            {
              gtk_widget_set_opacity (GTK_WIDGET (window), window->enter_opacity);
              panel_base_window_set_plugin_data (window, panel_base_window_set_plugin_enter_opacity);
            }
        }
      break;

    case PROP_LEAVE_OPACITY:
      opacity = g_value_get_uint (value) / 100.00;
      if (opacity != window->leave_opacity)
        {
          window->leave_opacity = opacity;
          if (window->is_composited && ! window->opacity_is_enter)
            {
              gtk_widget_set_opacity (GTK_WIDGET (window), window->leave_opacity);
              panel_base_window_set_plugin_data (window, panel_base_window_set_plugin_leave_opacity);
            }
        }
      break;

    case PROP_BACKGROUND_STYLE:
      bg_style = g_value_get_uint (value);
      if (window->background_style != bg_style)
        {
          window->background_style = bg_style;

          /* send information to external plugins */
          if (window->background_style == PANEL_BG_STYLE_IMAGE
              && window->background_image != NULL)
            {
              panel_base_window_set_background_image_css (window);
              panel_base_window_set_plugin_data (window,
                  panel_base_window_set_plugin_background_image);
            }
          else if (window->background_style == PANEL_BG_STYLE_NONE)
            {
              panel_base_window_reset_background_css (window);
              panel_base_window_set_plugin_data (window,
                  panel_base_window_set_plugin_background_color);
            }
          else if (window->background_style == PANEL_BG_STYLE_COLOR
                   && window->background_rgba != NULL)
            {
              panel_base_window_set_background_color_css (window);
              panel_base_window_set_plugin_data (window,
                  panel_base_window_set_plugin_background_color);
            }

          /* resize to update border size too */
          gtk_widget_queue_resize (GTK_WIDGET (window));
        }
      break;

    case PROP_BACKGROUND_RGBA:
      if (window->background_rgba != NULL)
        gdk_rgba_free (window->background_rgba);
      window->background_rgba = g_value_dup_boxed (value);

      if (window->background_style == PANEL_BG_STYLE_COLOR)
        {
          panel_base_window_set_background_color_css (window);
          panel_base_window_set_plugin_data (window,
              panel_base_window_set_plugin_background_color);
        }
      break;

    case PROP_BACKGROUND_IMAGE:
      g_free (window->background_image);
      str = g_value_get_string (value);
      if (str != NULL)
        {
          /* store new uri, built and escaped through a GFile */
          file = g_file_new_for_commandline_arg (str);
          window->background_image = g_file_get_uri (file);
          g_object_unref (file);
        }
      else
        window->background_image = NULL;

      if (window->background_style == PANEL_BG_STYLE_IMAGE)
        {
          panel_base_window_set_background_image_css (window);
          panel_base_window_set_plugin_data (window,
              panel_base_window_set_plugin_background_image);
        }
      break;

    case PROP_BORDERS:
      /* set new window borders and redraw the widget */
      panel_base_window_set_borders (PANEL_BASE_WINDOW (object),
                                     g_value_get_uint (value));
      break;

    case PROP_ACTIVE:
      if (g_value_get_boolean (value))
        {
          if (priv->active_timeout_id == 0)
            {
              /* start timeout for the marching ants selection */
              priv->active_timeout_id = gdk_threads_add_timeout_seconds_full (
                  G_PRIORITY_DEFAULT_IDLE, 1,
                  panel_base_window_active_timeout, object,
                  panel_base_window_active_timeout_destroyed);
            }
        }
      else if (priv->active_timeout_id != 0)
        {
          /* stop timeout */
          g_source_remove (priv->active_timeout_id);
        }

      /* queue a draw for first second */
      gtk_widget_queue_resize (GTK_WIDGET (object));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
panel_base_window_finalize (GObject *object)
{
  PanelBaseWindow *window = PANEL_BASE_WINDOW (object);

  /* stop running marching ants timeout */
  if (window->priv->active_timeout_id != 0)
    g_source_remove (window->priv->active_timeout_id);

  /* release bg colors data */
  g_free (window->background_image);
  if (window->background_rgba != NULL)
    gdk_rgba_free (window->background_rgba);
  g_object_unref (window->priv->css_provider);

  (*G_OBJECT_CLASS (panel_base_window_parent_class)->finalize) (object);
}



static void
panel_base_window_screen_changed (GtkWidget *widget, GdkScreen *previous_screen)
{
  PanelBaseWindow *window = PANEL_BASE_WINDOW (widget);
  GdkVisual       *visual;
  GdkScreen       *screen;

  if (GTK_WIDGET_CLASS (panel_base_window_parent_class)->screen_changed != NULL)
    GTK_WIDGET_CLASS (panel_base_window_parent_class)->screen_changed (widget, previous_screen);

  /* set the rgba colormap if supported by the screen */
  screen = gtk_window_get_screen (GTK_WINDOW (window));
  visual = gdk_screen_get_rgba_visual (screen);
  if (visual != NULL)
    {
      gtk_widget_set_visual (widget, visual);
      window->is_composited = gdk_screen_is_composited (screen);
    }

   panel_debug (PANEL_DEBUG_BASE_WINDOW,
               "%p: rgba visual=%p, compositing=%s", window,
               visual, PANEL_DEBUG_BOOL (window->is_composited));
}



static void
panel_base_window_composited_changed (GdkScreen *screen,
                                      GtkWidget *widget)
{
  PanelBaseWindow *window = PANEL_BASE_WINDOW (widget);
  gboolean         was_composited = window->is_composited;
  GdkWindow       *gdkwindow;
  GtkAllocation    allocation;

  /* set new compositing state */
  window->is_composited = gdk_screen_is_composited (screen);

  if (window->is_composited == was_composited)
    return;

  if (window->is_composited)
    {
      gtk_widget_set_opacity (GTK_WIDGET (widget), window->leave_opacity);
      panel_base_window_set_plugin_data (window,
                                         panel_base_window_set_plugin_leave_opacity);
    }
  else
    {
      /* make sure to always disable the leave opacity without compositing */
      window->opacity_is_enter = FALSE;
      gtk_widget_set_opacity (GTK_WIDGET (widget), 1.0);
      panel_base_window_set_plugin_data (window,
                                         panel_base_window_set_plugin_leave_opacity);
    }
  panel_debug (PANEL_DEBUG_BASE_WINDOW,
               "%p: compositing=%s", window,
               PANEL_DEBUG_BOOL (window->is_composited));

  if (window->is_composited != was_composited)
    g_object_notify (G_OBJECT (widget), "composited");

  /* make sure the entire window is redrawn */
  gdkwindow = gtk_widget_get_window (widget);
  if (gdkwindow != NULL)
    gdk_window_invalidate_rect (gdkwindow, NULL, TRUE);

  /* HACK: invalid the geometry, so the wm notices it */
  gtk_widget_get_allocation (widget, &allocation);
  gtk_window_move (GTK_WINDOW (window),
                   allocation.x,
                   allocation.y);
  gtk_widget_queue_resize (widget);
}



static gboolean
panel_base_window_active_timeout (gpointer user_data)
{
  PanelBaseWindow        *window = PANEL_BASE_WINDOW (user_data);
  GtkStyleContext        *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (window));

  /* Animate the border à la "marching ants" by cycling betwee a dashed and
     dotted border every other second */
  if ((g_get_real_time () / G_USEC_PER_SEC) % 2 == 0)
    {
      if (gtk_style_context_has_class (context, MARCHING_ANTS_DOTTED))
        gtk_style_context_remove_class (context, MARCHING_ANTS_DOTTED);
      gtk_style_context_add_class (context, MARCHING_ANTS_DASHED);
    }
  else
    {
      if (gtk_style_context_has_class (context, MARCHING_ANTS_DASHED))
        gtk_style_context_remove_class (context, MARCHING_ANTS_DASHED);
      gtk_style_context_add_class (context, MARCHING_ANTS_DOTTED);
    }

  gtk_widget_queue_draw (GTK_WIDGET (window));
  return TRUE;
}



static void
panel_base_window_active_timeout_destroyed (gpointer user_data)
{
  PanelBaseWindow        *window = PANEL_BASE_WINDOW (user_data);
  GtkStyleContext        *context;

  window->priv->active_timeout_id = 0;
  context = gtk_widget_get_style_context (GTK_WIDGET (window));

  /* Stop the marching ants */
  if (gtk_style_context_has_class (context, MARCHING_ANTS_DASHED))
    gtk_style_context_remove_class (context, MARCHING_ANTS_DASHED);
  if (gtk_style_context_has_class (context, MARCHING_ANTS_DOTTED))
    gtk_style_context_remove_class (context, MARCHING_ANTS_DOTTED);
}



static void
panel_base_window_set_background_color_css (PanelBaseWindow *window)
{
  gchar *css_string, *str;

  panel_return_if_fail (window->background_rgba != NULL);

  str = gdk_rgba_to_string (window->background_rgba);
  css_string = g_strdup_printf (".xfce4-panel.background { background: %s; "
                                                          "border-color: transparent; } %s",
                                str, PANEL_BASE_CSS);

  panel_base_window_set_background_css (window, css_string);

  g_free (css_string);
  g_free (str);
}



static void
panel_base_window_set_background_image_css (PanelBaseWindow *window)
{
  gchar *css_url, *css_string;
  gint scale_factor;

  panel_return_if_fail (window->background_image != NULL);

  /* do not scale background image with the panel */
  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (window));
  css_url = g_strdup_printf ("url(\"%s\")", window->background_image);
  for (gint i = 1; i < scale_factor; i++)
    {
      gchar *temp = g_strdup_printf ("%s,url(\"%s\")", css_url, window->background_image);
      g_free (css_url);
      css_url = temp;
    }

  css_string = g_strdup_printf (".xfce4-panel.background { background: -gtk-scaled(%s);"
                                                          "border-color: transparent; } %s",
                                css_url, PANEL_BASE_CSS);

  panel_base_window_set_background_css (window, css_string);

  g_free (css_string);
  g_free (css_url);
}



static void
panel_base_window_set_background_css (PanelBaseWindow *window,
                                      const gchar *css_string)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (window));

  /* Reset the css style provider */
  gtk_style_context_remove_provider (context, GTK_STYLE_PROVIDER (window->priv->css_provider));
  gtk_css_provider_load_from_data (window->priv->css_provider, css_string, -1, NULL);
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (window->priv->css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}



void
panel_base_window_reset_background_css (PanelBaseWindow *window)
{
  PanelBaseWindowPrivate  *priv = window->priv;
  GtkStyleContext         *context;
  GdkRGBA                 *background_rgba;
  gchar                   *border_side = NULL;
  gchar                   *base_css;
  gchar                   *color_text;

  context = gtk_widget_get_style_context (GTK_WIDGET (window));
  gtk_style_context_remove_provider (context,
                                     GTK_STYLE_PROVIDER (window->priv->css_provider));
  /* Get the background color of the panel to draw the border */
  gtk_style_context_get (context, GTK_STATE_FLAG_NORMAL,
                         GTK_STYLE_PROPERTY_BACKGROUND_COLOR,
                         &background_rgba, NULL);

  /* Set correct border style depending on panel position and length */
  if (priv->borders != PANEL_BORDER_NONE)
    {
      border_side = g_strdup_printf ("%s %s %s %s",
                                     PANEL_HAS_FLAG (priv->borders, PANEL_BORDER_TOP) ? "solid" : "none",
                                     PANEL_HAS_FLAG (priv->borders, PANEL_BORDER_RIGHT) ? "solid" : "none",
                                     PANEL_HAS_FLAG (priv->borders, PANEL_BORDER_BOTTOM) ? "solid" : "none",
                                     PANEL_HAS_FLAG (priv->borders, PANEL_BORDER_LEFT) ? "solid" : "none");
    }

  if (border_side)
    {
      color_text = gdk_rgba_to_string (background_rgba);
      base_css = g_strdup_printf ("%s .xfce4-panel.background { border-style: %s; border-width: 1px; border-color: shade(%s, 0.7); }",
                                  PANEL_BASE_CSS, border_side, color_text);
      gtk_css_provider_load_from_data (window->priv->css_provider, base_css, -1, NULL);
      g_free (base_css);
      g_free (color_text);
      g_free (border_side);
    }
  else
    gtk_css_provider_load_from_data (window->priv->css_provider, PANEL_BASE_CSS, -1, NULL);

  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (window->priv->css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  gdk_rgba_free (background_rgba);
}



void
panel_base_window_orientation_changed (PanelBaseWindow *window,
                                       gint             mode)
{
  GtkStyleContext         *context = gtk_widget_get_style_context (GTK_WIDGET (window));

  /* Reset all orientation-related style-classes */
  if (gtk_style_context_has_class (context, "horizontal"))
    gtk_style_context_remove_class (context, "horizontal");
  if (gtk_style_context_has_class (context, "vertical"))
    gtk_style_context_remove_class (context, "vertical");
  if (gtk_style_context_has_class (context, "deskbar"))
    gtk_style_context_remove_class (context, "deskbar");

  /* Apply the appropriate style-class */
  if (mode == 0)
    gtk_style_context_add_class (context, "horizontal");
  else if (mode == 1)
    gtk_style_context_add_class (context, "vertical");
  else if (mode == 2)
    gtk_style_context_add_class (context, "deskbar");
}



static void
panel_base_window_set_plugin_data (PanelBaseWindow *window,
                                   GtkCallback      func)
{
  GtkWidget *itembar;

  itembar = gtk_bin_get_child (GTK_BIN (window));
  if (G_LIKELY (itembar != NULL))
    gtk_container_foreach (GTK_CONTAINER (itembar), func, window);
}



static void
panel_base_window_set_plugin_enter_opacity (GtkWidget *widget,
                                            gpointer   user_data)
{
  PanelBaseWindow *window = PANEL_BASE_WINDOW (user_data);

  panel_base_window_set_plugin_opacity (widget, user_data, window->enter_opacity);
}



static void
panel_base_window_set_plugin_leave_opacity (GtkWidget *widget,
                                            gpointer   user_data)
{
  PanelBaseWindow *window = PANEL_BASE_WINDOW (user_data);

  if (window->is_composited)
    panel_base_window_set_plugin_opacity (widget, user_data, window->leave_opacity);
  else
    panel_base_window_set_plugin_opacity (widget, user_data, 1.0);
}



static void
panel_base_window_set_plugin_opacity (GtkWidget *widget,
                                      gpointer   user_data,
                                      gdouble    opacity)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (widget));
  panel_return_if_fail (PANEL_IS_BASE_WINDOW (user_data));

  if (PANEL_IS_PLUGIN_EXTERNAL (widget))
    panel_plugin_external_set_opacity (PANEL_PLUGIN_EXTERNAL (widget), opacity);
}



static void
panel_base_window_set_plugin_background_color (GtkWidget *widget,
                                               gpointer   user_data)
{
  PanelBaseWindow *window = PANEL_BASE_WINDOW (user_data);
  GdkRGBA         *color;

  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (widget));
  panel_return_if_fail (PANEL_IS_BASE_WINDOW (user_data));

  /* send null if the style is not a bg color */
  color = window->background_style == PANEL_BG_STYLE_COLOR ? window->background_rgba : NULL;
  if (PANEL_IS_PLUGIN_EXTERNAL (widget))
    panel_plugin_external_set_background_color (PANEL_PLUGIN_EXTERNAL (widget), color);
}



static void
panel_base_window_set_plugin_background_image (GtkWidget *widget,
                                               gpointer   user_data)
{
  panel_return_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (widget));
  panel_return_if_fail (PANEL_IS_BASE_WINDOW (user_data));

  if (PANEL_IS_PLUGIN_EXTERNAL (widget))
    panel_plugin_external_set_background_image (PANEL_PLUGIN_EXTERNAL (widget),
        PANEL_BASE_WINDOW (user_data)->background_image);
}



void
panel_base_window_move_resize (PanelBaseWindow *window,
                               gint             x,
                               gint             y,
                               gint             width,
                               gint             height)
{
  panel_return_if_fail (PANEL_IS_BASE_WINDOW (window));

  if (width > 0 && height > 0)
    gtk_window_resize (GTK_WINDOW (window), width, height);

  gtk_window_move (GTK_WINDOW (window), x, y);
}



void
panel_base_window_set_borders (PanelBaseWindow *window,
                               PanelBorders     borders)
{
  PanelBaseWindowPrivate *priv = window->priv;

  panel_return_if_fail (PANEL_IS_BASE_WINDOW (window));

  if (priv->borders != borders)
    {
      priv->borders = borders;
      gtk_widget_queue_resize (GTK_WIDGET (window));
      /* Re-draw the borders if system colors are being used */
      if (window->background_style == PANEL_BG_STYLE_NONE)
        panel_base_window_reset_background_css (window);
    }
}



PanelBorders
panel_base_window_get_borders (PanelBaseWindow *window)
{
  PanelBaseWindowPrivate *priv = window->priv;

  panel_return_val_if_fail (PANEL_IS_BASE_WINDOW (window), PANEL_BORDER_NONE);

  /* show all borders for the marching ants */
  if (priv->active_timeout_id != 0)
    return PANEL_BORDER_TOP | PANEL_BORDER_BOTTOM
           | PANEL_BORDER_LEFT | PANEL_BORDER_RIGHT;
  else if (window->background_style != PANEL_BG_STYLE_NONE)
    return PANEL_BORDER_NONE;

  return priv->borders;
}



void
panel_base_window_opacity_enter (PanelBaseWindow *window,
                                 gboolean         enter)
{
  if (! window->is_composited)
    return;

  window->opacity_is_enter = enter;
  if (window->leave_opacity == window->enter_opacity)
    return;

  if (enter)
    {
      gtk_widget_set_opacity (GTK_WIDGET (window), window->enter_opacity);
      panel_base_window_set_plugin_data (window, panel_base_window_set_plugin_enter_opacity);
    }
  else
    {
      gtk_widget_set_opacity (GTK_WIDGET (window), window->leave_opacity);
      panel_base_window_set_plugin_data (window, panel_base_window_set_plugin_leave_opacity);
    }
}
