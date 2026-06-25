/*
 * Copyright (C) 2007-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <common/panel-private.h>
#include <common/panel-xfconf.h>

#include "clock.h"
#include "clock-time.h"
#include "clock-digital.h"



static void     xfce_clock_digital_set_property  (GObject               *object,
                                                  guint                  prop_id,
                                                  const GValue          *value,
                                                  GParamSpec            *pspec);
static void     xfce_clock_digital_get_property  (GObject               *object,
                                                  guint                  prop_id,
                                                  GValue                *value,
                                                  GParamSpec            *pspec);
static void     xfce_clock_digital_finalize      (GObject               *object);
static void     xfce_clock_digital_update        (XfceClockDigital      *digital,
                                                  ClockTime             *time);
static void     xfce_clock_digital_update_layout (XfceClockDigital      *digital);




enum
{
  PROP_0,
  PROP_DIGITAL_LAYOUT,
  PROP_DIGITAL_TIME_FORMAT,
  PROP_DIGITAL_TIME_FONT,
  PROP_DIGITAL_DATE_FORMAT,
  PROP_DIGITAL_DATE_FONT,
  PROP_SIZE_RATIO,
  PROP_ORIENTATION,
};

struct _XfceClockDigitalClass
{
  GtkBoxClass __parent__;
};

struct _XfceClockDigital
{
  GtkBox __parent__;

  GtkWidget *vbox;
  GtkWidget *time_label;
  GtkWidget *date_label;

  ClockTime          *time;
  ClockTimeTimeout   *timeout;

  ClockPluginDigitalFormat layout;

  gchar *date_format;
  gchar *date_font;
  gchar *time_format;
  gchar *time_font;
};



#define DEFAULT_FONT "Sans Regular 8"



XFCE_PANEL_DEFINE_TYPE (XfceClockDigital, xfce_clock_digital, GTK_TYPE_BOX)



static void
xfce_clock_digital_class_init (XfceClockDigitalClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_clock_digital_finalize;
  gobject_class->set_property = xfce_clock_digital_set_property;
  gobject_class->get_property = xfce_clock_digital_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_SIZE_RATIO,
                                   g_param_spec_double ("size-ratio", NULL, NULL,
                                                        -1, G_MAXDOUBLE, 0.0,
                                                        G_PARAM_READABLE
                                                        | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_DIGITAL_LAYOUT,
                                   g_param_spec_uint ("digital-layout",
                                                      NULL, NULL,
                                                      CLOCK_PLUGIN_DIGITAL_FORMAT_MIN,
                                                      CLOCK_PLUGIN_DIGITAL_FORMAT_MAX,
                                                      CLOCK_PLUGIN_DIGITAL_FORMAT_DEFAULT,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_ORIENTATION,
                                   g_param_spec_enum ("orientation", NULL, NULL,
                                                      GTK_TYPE_ORIENTATION,
                                                      GTK_ORIENTATION_HORIZONTAL,
                                                      G_PARAM_WRITABLE
                                                      | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_DIGITAL_DATE_FONT,
                                   g_param_spec_string ("digital-date-font", NULL, NULL,
                                                        DEFAULT_FONT,
                                                        G_PARAM_READWRITE
                                                        | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_DIGITAL_DATE_FORMAT,
                                   g_param_spec_string ("digital-date-format", NULL, NULL,
                                                        DEFAULT_DIGITAL_DATE_FORMAT,
                                                        G_PARAM_READWRITE
                                                        | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_DIGITAL_TIME_FONT,
                                   g_param_spec_string ("digital-time-font", NULL, NULL,
                                                        DEFAULT_FONT,
                                                        G_PARAM_READWRITE
                                                        | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_DIGITAL_TIME_FORMAT,
                                   g_param_spec_string ("digital-time-format", NULL, NULL,
                                                        DEFAULT_DIGITAL_TIME_FORMAT,
                                                        G_PARAM_READWRITE
                                                        | G_PARAM_STATIC_STRINGS));
}



static void
xfce_clock_digital_init (XfceClockDigital *digital)
{
  digital->date_font = g_strdup (DEFAULT_FONT);
  digital->date_format = g_strdup (DEFAULT_DIGITAL_DATE_FORMAT);
  digital->time_font = g_strdup (DEFAULT_FONT);
  digital->time_format = g_strdup (DEFAULT_DIGITAL_TIME_FORMAT);

  digital->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (digital), digital->vbox, TRUE, TRUE, 0);
  gtk_box_set_homogeneous (GTK_BOX (digital->vbox), TRUE);

  digital->time_label = gtk_label_new (NULL);
  digital->date_label = gtk_label_new (NULL);

  gtk_label_set_justify (GTK_LABEL (digital->time_label), GTK_JUSTIFY_CENTER);
  gtk_label_set_justify (GTK_LABEL (digital->date_label), GTK_JUSTIFY_CENTER);

  gtk_box_pack_start (GTK_BOX (digital->vbox), digital->time_label, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (digital->vbox), digital->date_label, TRUE, FALSE, 0);

  gtk_widget_show_all (digital->vbox);
}



static void
xfce_clock_digital_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  XfceClockDigital *digital = XFCE_CLOCK_DIGITAL (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_label_set_angle (GTK_LABEL (digital->time_label),
          g_value_get_enum (value) == GTK_ORIENTATION_HORIZONTAL ?
          0 : 270);
      gtk_label_set_angle (GTK_LABEL (digital->date_label),
          g_value_get_enum (value) == GTK_ORIENTATION_HORIZONTAL ?
          0 : 270);
      break;

    case PROP_DIGITAL_LAYOUT:
      digital->layout = g_value_get_uint (value);
      xfce_clock_digital_update_layout (digital);
      break;

    case PROP_DIGITAL_DATE_FONT:
      g_free (digital->date_font);
      digital->date_font = g_value_dup_string (value);
      break;

    case PROP_DIGITAL_DATE_FORMAT:
      g_free (digital->date_format);
      digital->date_format = g_value_dup_string (value);
      break;

    case PROP_DIGITAL_TIME_FONT:
      g_free (digital->time_font);
      digital->time_font = g_value_dup_string (value);
      break;

    case PROP_DIGITAL_TIME_FORMAT:
      g_free (digital->time_format);
      digital->time_format = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

  /* reschedule the timeout and redraw */
  clock_time_timeout_set_interval (digital->timeout,
      clock_time_interval_from_format (digital->time_format));
  xfce_clock_digital_update (digital, digital->time);
}



static void
xfce_clock_digital_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  XfceClockDigital *digital = XFCE_CLOCK_DIGITAL (object);

  switch (prop_id)
    {
    case PROP_DIGITAL_LAYOUT:
        g_value_set_uint (value, digital->layout);
        break;

    case PROP_DIGITAL_DATE_FORMAT:
        g_value_set_string (value, digital->date_format);
        break;

    case PROP_DIGITAL_DATE_FONT:
      g_value_set_string (value, digital->date_font);
      break;

    case PROP_DIGITAL_TIME_FORMAT:
      g_value_set_string (value, digital->time_format);
      break;

    case PROP_DIGITAL_TIME_FONT:
      g_value_set_string (value, digital->time_font);
      break;

    case PROP_SIZE_RATIO:
      g_value_set_double (value, -1.0);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_clock_digital_finalize (GObject *object)
{
  XfceClockDigital *digital = XFCE_CLOCK_DIGITAL (object);

  /* stop the timeout */
  clock_time_timeout_free (digital->timeout);

  g_free (digital->date_font);
  g_free (digital->date_format);
  g_free (digital->time_font);
  g_free (digital->time_format);

  (*G_OBJECT_CLASS (xfce_clock_digital_parent_class)->finalize) (object);
}



static void
xfce_clock_digital_update (XfceClockDigital *digital,
                           ClockTime        *time)
{
  PangoAttrList *attr_list;
  PangoAttribute *attr;
  PangoFontDescription *font_desc;
  gchar *markup, *stripped;

  panel_return_if_fail (XFCE_CLOCK_IS_DIGITAL (digital));
  panel_return_if_fail (XFCE_IS_CLOCK_TIME (time));

  /* set time label */
  markup = clock_time_strdup_strftime (digital->time, digital->time_format);
  if (markup != NULL && pango_parse_markup (markup, -1, 0, &attr_list, &stripped, NULL, NULL))
    {
      font_desc = pango_font_description_from_string (digital->time_font);
      attr = pango_attr_font_desc_new (font_desc);
      pango_attr_list_insert_before (attr_list, attr);
      gtk_label_set_text (GTK_LABEL (digital->time_label), stripped);
      gtk_label_set_attributes (GTK_LABEL (digital->time_label), attr_list);
      pango_font_description_free (font_desc);
      pango_attr_list_unref (attr_list);
      g_free (stripped);
    }
  g_free (markup);

  /* set date label */
  markup = clock_time_strdup_strftime (digital->time, digital->date_format);
  if (markup != NULL && pango_parse_markup (markup, -1, 0, &attr_list, &stripped, NULL, NULL))
    {
      font_desc = pango_font_description_from_string (digital->date_font);
      attr = pango_attr_font_desc_new (font_desc);
      pango_attr_list_insert_before (attr_list, attr);
      gtk_label_set_text (GTK_LABEL (digital->date_label), stripped);
      gtk_label_set_attributes (GTK_LABEL (digital->date_label), attr_list);
      pango_font_description_free (font_desc);
      pango_attr_list_unref (attr_list);
      g_free (stripped);
    }
  g_free (markup);
}



static void
xfce_clock_digital_update_layout (XfceClockDigital *digital)
{
  gtk_widget_hide (digital->date_label);
  gtk_widget_hide (digital->time_label);
  if (digital->layout == CLOCK_PLUGIN_DIGITAL_FORMAT_DATE)
    {
      gtk_widget_show (digital->date_label);
    }
  else if (digital->layout == CLOCK_PLUGIN_DIGITAL_FORMAT_TIME)
    {
      gtk_widget_show (digital->time_label);
    }
  else if (digital->layout == CLOCK_PLUGIN_DIGITAL_FORMAT_DATE_TIME)
    {
      gtk_widget_show (digital->time_label);
      gtk_widget_show (digital->date_label);
      gtk_box_reorder_child (GTK_BOX (digital->vbox), digital->date_label, 0);
      gtk_box_reorder_child (GTK_BOX (digital->vbox), digital->time_label, 1);
    }
  else
    {
      gtk_widget_show (digital->time_label);
      gtk_widget_show (digital->date_label);
      gtk_box_reorder_child (GTK_BOX (digital->vbox), digital->date_label, 1);
      gtk_box_reorder_child (GTK_BOX (digital->vbox), digital->time_label, 0);
    }
}



static void
xfce_clock_digital_anchored (XfceClockDigital *digital)
{
  XfconfChannel *channel;
  GtkWidget     *plugin;
  gchar         *prop, *format;
  const gchar   *prop_base;
  gboolean       has_prop;
  const gchar   *props[] = { "digital-layout", "digital-time-font", "digital-time-format",
                             "digital-date-font", "digital-date-format" };

  g_signal_handlers_disconnect_by_func (digital, xfce_clock_digital_anchored, NULL);

  plugin = gtk_widget_get_ancestor (GTK_WIDGET (digital), XFCE_TYPE_PANEL_PLUGIN);
  channel = panel_properties_get_channel (G_OBJECT (plugin));
  prop_base = xfce_panel_plugin_get_property_base (XFCE_PANEL_PLUGIN (plugin));

  /* see if any of the new properties are set */
  for (guint n = 0; n < G_N_ELEMENTS (props); n++)
    {
      prop = g_strdup_printf ("%s/%s", prop_base, props[n]);
      has_prop = xfconf_channel_has_property (channel, prop);
      g_free (prop);
      if (has_prop)
        return;
    }

  /* new user, see if he has an old format */
  prop = g_strdup_printf ("%s/%s", prop_base, "digital-format");
  has_prop = xfconf_channel_has_property (channel, prop);
  if (! has_prop)
    {
      g_free (prop);
      return;
    }

  /* set layout to time only and time format to old format, i.e. the best we can do */
  format = xfconf_channel_get_string (channel, prop, DEFAULT_DIGITAL_TIME_FORMAT);
  g_object_set (G_OBJECT (digital),
                "digital-layout", CLOCK_PLUGIN_DIGITAL_FORMAT_TIME,
                "digital-time-format", format,
                NULL);
  g_free (format);
  g_free (prop);
}



GtkWidget *
xfce_clock_digital_new (ClockTime *time,
                        ClockSleepMonitor *sleep_monitor)
{
  XfceClockDigital *digital = g_object_new (XFCE_CLOCK_TYPE_DIGITAL, NULL);

  digital->time = time;
  digital->timeout = clock_time_timeout_new (clock_time_interval_from_format (digital->time_format),
                                             digital->time,
                                             sleep_monitor,
                                             G_CALLBACK (xfce_clock_digital_update), digital);
  xfce_clock_digital_update_layout (digital);

  /* backward compatibility */
  g_signal_connect (digital, "hierarchy-changed", G_CALLBACK (xfce_clock_digital_anchored), NULL);

  return GTK_WIDGET (digital);
}
