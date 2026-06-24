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

#ifndef USE_STANDALONE

#include "location.h"
#include "weatherwidget.h"
#include "yahooutil.h"
#include "logutil.h"
#include "providers.h"
#include "openweathermap.h"

#include "plugin.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

static provider_callback_info *providersList[] = {
/*  &YahooCallbacks, -- does not work anymore */
  &OpenWeatherMapCallbacks,
  NULL
};

/* Need to maintain count for bookkeeping */
static gint g_iCount = 0;

typedef struct
{
  gint iMyId_;
  GtkWeather *pWeather_;
  config_setting_t *pConfig_;
  LXPanel *pPanel_;
} WeatherPluginPrivate;


/**
 * Weather Plugin destructor.
 *
 * @param pData Pointer to the plugin data (private).
 */
static void
weather_destructor(gpointer pData)
{
  WeatherPluginPrivate * pPriv = (WeatherPluginPrivate *) pData;

  LXW_LOG(LXW_DEBUG, "weather_destructor(%d): %d", pPriv->iMyId_, g_iCount);

  g_free(pPriv);

  --g_iCount;

  if (g_iCount == 0)
    {
      cleanupLogUtil();
    }
}

/**
 * Weather Plugin constructor
 *
 * @param pPlugin Pointer to the PluginClass wrapper instance.
 * @param setting Pointer to the configuration settings for this plugin.
 *
 * @return Pointer to a new weather widget.
 */
static GtkWidget *
weather_constructor(LXPanel *pPanel, config_setting_t *pConfig)
{
  WeatherPluginPrivate *pPriv;
  const char *pczDummy;
  int iDummyVal;
  int locSet = 0;
  provider_callback_info **pProvider;

  pPriv = g_new0(WeatherPluginPrivate, 1);

  pPriv->pConfig_ = pConfig;
  pPriv->pPanel_ = pPanel;

  /* There is one more now... */
  ++g_iCount;

  pPriv->iMyId_ = g_iCount;

  if (g_iCount == 1)
    {
      initializeLogUtil("syslog");
      
      setMaxLogLevel(LXW_ERROR);
    }

  LXW_LOG(LXW_DEBUG, "weather_constructor()");
  
  GtkWeather * pWidg = gtk_weather_new();

  pPriv->pWeather_ = pWidg;

  /* Try to get a provider */
  if (config_setting_lookup_string(pConfig, "provider", &pczDummy))
    {
      for (pProvider = providersList; *pProvider; pProvider++)
        {
          if (strcmp((*pProvider)->name, pczDummy) == 0)
            {
              locSet = gtk_weather_set_provider(pWidg, *pProvider);
              break;
            }
        }
    }

  /* No working provider selected, let try some */
  if (!locSet)
    {
      for (pProvider = providersList; *pProvider; pProvider++)
        {
          locSet = gtk_weather_set_provider(pWidg, *pProvider);
          if (locSet)
            break;
        }
    }

  /* No working provider found, retreat */
  if (!locSet)
  {
    gtk_widget_destroy(GTK_WIDGET(pWidg));
    g_free(pPriv);
    --g_iCount;
    if (g_iCount == 0)
      cleanupLogUtil();
    return NULL;
  }

  GtkWidget * pEventBox = gtk_event_box_new();

  lxpanel_plugin_set_data(pEventBox, pPriv, weather_destructor);
  gtk_container_add(GTK_CONTAINER(pEventBox), GTK_WIDGET(pWidg));

  gtk_widget_set_has_window(pEventBox, FALSE);

  gtk_widget_show_all(pEventBox);

  /* use config settings */
  LocationInfo * pLocation = g_new0(LocationInfo, 1);

  if (config_setting_lookup_string(pConfig, "alias", &pczDummy))
    {
      pLocation->pcAlias_ = g_strndup(pczDummy, (pczDummy) ? strlen(pczDummy) : 0);
    }
  else if (config_setting_lookup_int(pConfig, "alias", &iDummyVal))
    {
      pLocation->pcAlias_ = g_strdup_printf("%d", iDummyVal);
    }
  else
    {
      LXW_LOG(LXW_ERROR, "Weather: could not lookup alias in config.");
    }

  if (config_setting_lookup_string(pConfig, "city", &pczDummy))
    {
      pLocation->pcCity_ = g_strndup(pczDummy, (pczDummy) ? strlen(pczDummy) : 0);
    }
  else
    {
      LXW_LOG(LXW_ERROR, "Weather: could not lookup city in config.");
    }

  if (config_setting_lookup_string(pConfig, "state", &pczDummy))
    {
      pLocation->pcState_ = g_strndup(pczDummy, (pczDummy) ? strlen(pczDummy) : 0);
    }
  else
    {
      LXW_LOG(LXW_ERROR, "Weather: could not lookup state in config.");
    }

  if (config_setting_lookup_string(pConfig, "country", &pczDummy))
    {
      pLocation->pcCountry_ = g_strndup(pczDummy, (pczDummy) ? strlen(pczDummy) : 0);
    }
  else
    {
      LXW_LOG(LXW_ERROR, "Weather: could not lookup country in config.");
    }

  iDummyVal = 0;
  locSet = 0;
  pLocation->dLongitude_ = 360.0; /* invalid value */
  pLocation->dLatitude_ = 360.0;
  if (config_setting_lookup_string(pConfig, "longitude", &pczDummy))
    {
      pLocation->dLongitude_ = g_strtod(pczDummy, NULL);
      iDummyVal++;
    }
  if (iDummyVal && config_setting_lookup_string(pConfig, "latitude", &pczDummy))
    {
      pLocation->dLatitude_ = g_strtod(pczDummy, NULL);
      locSet = 1;
    }
  /* no coords found, let try woeid if provider works with it though */
  else if ((*pProvider)->supports_woeid &&
           config_setting_lookup_string(pConfig, "woeid", &pczDummy))
    {
      pLocation->pcWOEID_ = g_strndup(pczDummy, (pczDummy) ? strlen(pczDummy) : 0);
      locSet = 1;
    }
  else if ((*pProvider)->supports_woeid &&
           config_setting_lookup_int(pConfig, "woeid", &iDummyVal))
    {
      pLocation->pcWOEID_ = g_strdup_printf("%d", iDummyVal);
      locSet = 1;
    }
  else
    {
      LXW_LOG(LXW_ERROR, "Weather: could not lookup woeid in config.");
    }

  if (config_setting_lookup_string(pConfig, "units", &pczDummy))
    {
      pLocation->cUnits_ = pczDummy[0];
    }
  else
    {
      LXW_LOG(LXW_ERROR, "Weather: could not lookup units in config.");
    }

  if (config_setting_lookup_int(pConfig, "interval", &iDummyVal))
    {
      if (iDummyVal < 20) /* Minimum 20 minutes */
        iDummyVal = 60; /* Set to default 60 minutes */
      pLocation->uiInterval_ = (guint)iDummyVal;
    }
  else
    {
      LXW_LOG(LXW_ERROR, "Weather: could not lookup interval in config.");
    }

  if (config_setting_lookup_int(pConfig, "enabled", &iDummyVal))
    {
      pLocation->bEnabled_ = (gint)iDummyVal;
    }
  else
    {
      LXW_LOG(LXW_ERROR, "Weather: could not lookup enabled flag in config.");
    }

  if (pLocation->pcAlias_ && locSet)
    {
      GValue locationValue = G_VALUE_INIT;

      g_value_init(&locationValue, G_TYPE_POINTER);

      /* location is copied by the widget */
      g_value_set_pointer(&locationValue, pLocation);

      g_object_set_property(G_OBJECT(pWidg),
                            "location",
                            &locationValue);
    }

  freeLocation(pLocation);

  return pEventBox;
}

/**
 * Weather Plugin callback to save configuration
 *
 * @param pWidget Pointer to this widget.
 */
void weather_save_configuration(GtkWidget * pWeather, LocationInfo * pLocation)
{
  GtkWidget * pWidget = gtk_widget_get_parent(pWeather);
  WeatherPluginPrivate * pPriv = NULL;
  provider_callback_info * pProvider;

  if (pWidget)
    {
      pPriv = (WeatherPluginPrivate *) lxpanel_plugin_get_data(pWidget);
    }
  if (pPriv == NULL)
    {
      LXW_LOG(LXW_ERROR, "Weather: weather_save_configuration() for invalid widget");
      return;
    }

  LXW_LOG(LXW_DEBUG, "weather_save_configuration(%d)", pPriv->iMyId_);

  if (pLocation)
    {
      /* save configuration */
      config_group_set_string(pPriv->pConfig_, "alias", pLocation->pcAlias_);
      config_group_set_string(pPriv->pConfig_, "city", pLocation->pcCity_);
      config_group_set_string(pPriv->pConfig_, "state", pLocation->pcState_);
      config_group_set_string(pPriv->pConfig_, "country", pLocation->pcCountry_);
      config_group_set_string(pPriv->pConfig_, "woeid", pLocation->pcWOEID_);

      char buff[16];
      if (snprintf(buff, 2, "%c", pLocation->cUnits_) > 0)
        {
          config_group_set_string(pPriv->pConfig_, "units", buff);
        }

      if (pLocation->dLatitude_ < 360.0)
        {
          snprintf(buff, sizeof(buff), "%.6f", pLocation->dLatitude_);
          config_group_set_string(pPriv->pConfig_, "latitude", buff);
        }
      if (pLocation->dLongitude_ < 360.0)
        {
          snprintf(buff, sizeof(buff), "%.6f", pLocation->dLongitude_);
          config_group_set_string(pPriv->pConfig_, "longitude", buff);
        }

      config_group_set_int(pPriv->pConfig_, "interval", (int) pLocation->uiInterval_);
      config_group_set_int(pPriv->pConfig_, "enabled", (int) pLocation->bEnabled_);
    }

  pProvider = gtk_weather_get_provider(GTK_WEATHER(pWeather));
  if (pProvider)
    {
      config_group_set_string(pPriv->pConfig_, "provider", pProvider->name);
    }
}

void weather_set_label_text(GtkWidget * pWeather, GtkWidget * label, const gchar * text)
{
  GtkWidget * pWidget = gtk_widget_get_parent(pWeather);
  WeatherPluginPrivate * pPriv = NULL;

  if (pWidget)
    {
      pPriv = (WeatherPluginPrivate *) lxpanel_plugin_get_data(pWidget);
    }
  if (pPriv == NULL)
    {
      LXW_LOG(LXW_ERROR, "Weather: weather_set_label_text() for invalid widget");
      return;
    }

  lxpanel_draw_label_text(pPriv->pPanel_, label, text, TRUE, 1, TRUE);
}

/**
 * Weather Plugin configuration change callback.
 *
 * @param pPanel  Pointer to the panel instance.
 * @param pPlugin Pointer to the PluginClass wrapper instance.
 */
static void
weather_configuration_changed(LXPanel *pPanel, GtkWidget *pWidget)
{
  LXW_LOG(LXW_DEBUG, "weather_configuration_changed()");

  if (pPanel && pWidget)
    {
      LXW_LOG(LXW_DEBUG, 
             "   orientation: %s, width: %d, height: %d, icon size: %d\n", 
              (panel_get_orientation(pPanel) == GTK_ORIENTATION_HORIZONTAL)?"HORIZONTAL":
              (panel_get_orientation(pPanel) == GTK_ORIENTATION_VERTICAL)?"VERTICAL":"NONE",
              pPanel->width, panel_get_height(pPanel), 
              panel_get_icon_size(pPanel));

      WeatherPluginPrivate * pPriv = (WeatherPluginPrivate *) lxpanel_plugin_get_data(pWidget);
      gtk_weather_render(pPriv->pWeather_);
    }
}

/**
 * Weather Plugin configuration dialog callback.
 *
 * @param pPanel  Pointer to the panel instance.
 * @param pWidget Pointer to the Plugin widget instance.
 * @param pParent Pointer to the GtkWindow parent.
 *
 * @return Instance of the widget.
 */
static GtkWidget *
weather_configure(LXPanel *pPanel G_GNUC_UNUSED, GtkWidget *pWidget)
{
  LXW_LOG(LXW_DEBUG, "weather_configure()");

  WeatherPluginPrivate * pPriv = (WeatherPluginPrivate *) lxpanel_plugin_get_data(pWidget);

  GtkWidget * pDialog = gtk_weather_create_preferences_dialog(pPriv->pWeather_, providersList);

  return pDialog;
}

FM_DEFINE_MODULE(lxpanel_gtk, weather)

/**
 * Definition of the weather plugin module
 */
LXPanelPluginInit fm_module_init_lxpanel_gtk =
  {
    .name = N_("Weather Plugin"),
    .description = N_("Show weather conditions for a location."),

    // API functions
    .new_instance = weather_constructor,
    .config = weather_configure,
    .reconfigure = weather_configuration_changed
  };
#endif /* USE_STANDALONE */
