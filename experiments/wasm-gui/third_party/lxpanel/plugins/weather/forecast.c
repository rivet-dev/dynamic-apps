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

/* Provides implementation for ForecastInfo-specific functions */

#include "forecast.h" // includes glib.h
#include "logutil.h"

#include <stdio.h>
#include <string.h>

/**
 * Provides the mechanism to free any data associated with 
 * the Forecast structure
 *
 * @param pEntry Entry to free.
 *
 */
static void
freeForecastForecast(Forecast * pEntry)
{
  g_free(pEntry->pcDay_);
  //  g_free(pEntry->iHigh_);
  //  g_free(pEntry->iLow_);
  g_free(pEntry->pcConditions_);
  g_free(pEntry->pcClouds_);
}

/**
 * Provides the mechanism to free any data associated with 
 * the ForecastUnits structure
 *
 * @param pEntry Entry to free.
 *
 */
static void
freeForecastUnits(ForecastUnits * pEntry)
{
  g_free(pEntry->pcDistance_);
  g_free(pEntry->pcPressure_);
  g_free(pEntry->pcSpeed_);
  g_free(pEntry->pcTemperature_);
}

/**
 * Provides the mechanism to free any data associated with 
 * the ForecastInfo structure
 *
 * @param pData Entry to free.
 *
 */
void
freeForecast(ForecastInfo * pEntry)
{
  if (!pEntry)
    {
      return;
    }

  freeForecastUnits(&pEntry->units_);

  freeForecastForecast(&pEntry->today_);
  freeForecastForecast(&pEntry->tomorrow_);

  /*  g_free(pEntry->iWindChill_); */
  g_free(pEntry->pcWindDirection_);
  /*  g_free(pEntry->iWindSpeed_);
  g_free(pEntry->iHumidity_);
  g_free(pEntry->dPressure_);
  g_free(pEntry->dVisibility_);*/
  g_free(pEntry->pcSunrise_);
  g_free(pEntry->pcSunset_);
  g_free(pEntry->pcTime_);
  //  g_free(pEntry->iTemperature_);
  g_free(pEntry->pcConditions_);
  g_free(pEntry->pcClouds_);
  g_free(pEntry->pcImageURL_);
  
  if (pEntry->pImage_)
    {
      g_object_unref(pEntry->pImage_);
    }

  g_free(pEntry);
}

/**
 * Prints the contents of the supplied entry to stdout
 *
 * @param pEntry Entry contents of which to print.
 *
 */
void
printForecast(ForecastInfo * pInfo G_GNUC_UNUSED)
{
#ifdef DEBUG
  if (!pInfo)
    {
      LXW_LOG(LXW_ERROR, "forecast::printForecast(): Entry: NULL");
      
      return;
    }
  
  LXW_LOG(LXW_VERBOSE, "Forecast at %s:", (const char *)pInfo->pcTime_);
  LXW_LOG(LXW_VERBOSE, "\tTemperature: %d%s", 
          pInfo->iTemperature_,
          (const char *)pInfo->units_.pcTemperature_);
  LXW_LOG(LXW_VERBOSE, "\tHumidity: %d%s", pInfo->iHumidity_, "%");
  LXW_LOG(LXW_VERBOSE, "\tWind chill: %d%s, speed: %d%s, direction %s", 
          pInfo->iWindChill_,
          (const char *)pInfo->units_.pcTemperature_,
          pInfo->iWindSpeed_,
          (const char *)pInfo->units_.pcSpeed_,
          pInfo->pcWindDirection_);
  LXW_LOG(LXW_VERBOSE, "\tPressure: %2.02f%s and %s", 
          pInfo->dPressure_,
          (const char *)pInfo->units_.pcPressure_,
          ((pInfo->pressureState_ == STEADY)?"steady":
           (pInfo->pressureState_ == RISING)?"rising":
           (pInfo->pressureState_ == FALLING)?"falling":"?"));
  LXW_LOG(LXW_VERBOSE, "\tConditions: %s", (const char *)pInfo->pcConditions_);
  LXW_LOG(LXW_VERBOSE, "\tClouds: %s", (const char *)pInfo->pcClouds_);
  LXW_LOG(LXW_VERBOSE, "\tVisibility: %3.02f%s", 
          pInfo->dVisibility_,
          (const char *)pInfo->units_.pcDistance_);
  LXW_LOG(LXW_VERBOSE, "\tSunrise: %s", (const char *)pInfo->pcSunrise_);
  LXW_LOG(LXW_VERBOSE, "\tSunset: %s", (const char *)pInfo->pcSunset_);
  LXW_LOG(LXW_VERBOSE, "\tImage URL: %s", pInfo->pcImageURL_);

  LXW_LOG(LXW_VERBOSE, "\tTwo-day forecast:");
  LXW_LOG(LXW_VERBOSE, "\t\t%s: High: %d%s, Low: %d%s, Conditions: %s",
          (const char *)pInfo->today_.pcDay_,
          pInfo->today_.iHigh_,
          (const char *)pInfo->units_.pcTemperature_,
          pInfo->today_.iLow_,
          (const char *)pInfo->units_.pcTemperature_,
          (const char *)pInfo->today_.pcConditions_);
  LXW_LOG(LXW_VERBOSE, "\t\t%s: High: %d%s, Low: %d%s, Conditions: %s",
          (const char *)pInfo->tomorrow_.pcDay_,
          pInfo->tomorrow_.iHigh_,
          (const char *)pInfo->units_.pcTemperature_,
          pInfo->tomorrow_.iLow_,
          (const char *)pInfo->units_.pcTemperature_,
          (const char *)pInfo->tomorrow_.pcConditions_);
#endif
}
