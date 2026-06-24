/**
 * Copyright (c) 2012-2014 Piotr Sipika; see the AUTHORS file for more.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 * See the COPYRIGHT file for more information.
 */

/* Declarations for the custom weather widget */

#ifndef __WEATHERWIDGET_H__
#define __WEATHERWIDGET_H__

#include "providers.h"

#include <gtk/gtk.h>
#include <glib.h>

G_BEGIN_DECLS

#define GTK_WEATHER_TYPE            (gtk_weather_get_type())
#define GTK_WEATHER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                     GTK_WEATHER_TYPE, GtkWeather))
#define GTK_WEATHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                     GTK_WEATHER_TYPE, GtkWeatherClass))
#define IS_GTK_WEATHER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                     GTK_WEATHER_TYPE))
#define IS_GTK_WEATHER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                     GTK_WEATHER_TYPE))

typedef struct _GtkWeather      GtkWeather;
typedef struct _GtkWeatherClass GtkWeatherClass;

struct _GtkWeather
{
  GtkEventBox widget;
};

struct _GtkWeatherClass
{
  GtkEventBoxClass parent_class;
  
  void (* location_changed) (GtkWeather * weather, const gpointer location);
  void (* forecast_changed) (GtkWeather * weather, const gpointer lorecast);
};

GType       gtk_weather_get_type(void) G_GNUC_CONST;
GtkWeather * gtk_weather_new(void);
#ifdef USE_STANDALONE
void        gtk_weather_run_preferences_dialog(GtkWeather * weather);
void        gtk_weather_run_popup_menu(GtkWeather * weather);
#endif
void        gtk_weather_run_conditions_dialog(GtkWeather * weather);
gchar *     gtk_weather_get_tooltip_text(GtkWeather * weather);
GtkWidget * gtk_weather_create_preferences_dialog(GtkWeather * weather, provider_callback_info ** list);
int         gtk_weather_set_provider(GtkWeather * weather, provider_callback_info * provider);
provider_callback_info * gtk_weather_get_provider(GtkWeather * weather);

/* if USE_STANDALONE is used then application should provide these functions */
void weather_save_configuration(GtkWidget * pWeather, LocationInfo * pLocation);
void weather_set_label_text(GtkWidget * pWeather, GtkWidget * label,
                            const gchar * text);

void gtk_weather_render        (GtkWeather * weather);

G_END_DECLS

#endif
