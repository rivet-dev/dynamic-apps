/*
 * Copyright (C) 2012-2014 Piotr Sipika.
 * Copyright (C) 2019 Andriy Grytsenko <andrej@rep.kiev.ua>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "httputil.h"
#include "location.h"
#include "forecast.h"
#include "logutil.h"
#include "openweathermap.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xmlstring.h>
#include <libxml/uri.h>
#include <libxml/xpathInternals.h>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <time.h>
#include <sys/utsname.h>

#define XMLCHAR_P(x) (xmlChar *)(x)
#define CONSTXMLCHAR_P(x) (const xmlChar *)(x)
#define CONSTCHAR_P(x) (const char *)(x)
#define CHAR_P(x) (char *)(x)

#define WIND_DIRECTION(x) ( \
  ((x>=350 && x<=360) || (x>=0 && x<=11 ))?_("N"): \
  (x>11   && x<=33 )?_("NNE"): \
  (x>33   && x<=57 )?_("NE"):  \
  (x>57   && x<=79 )?_("ENE"): \
  (x>79   && x<=101)?_("E"):   \
  (x>101  && x<=123)?_("ESE"): \
  (x>123  && x<=147)?_("SE"):  \
  (x>147  && x<=169)?_("SSE"): \
  (x>169  && x<=192)?_("S"):   \
  (x>192  && x<=214)?_("SSW"): \
  (x>214  && x<=236)?_("SW"):  \
  (x>236  && x<=258)?_("WSW"): \
  (x>258  && x<=282)?_("W"):   \
  (x>282  && x<=304)?_("WNW"): \
  (x>304  && x<=326)?_("NW"):  \
  (x>326  && x<=349)?_("NNW"):"")

static gint g_iInitialized = 0;

struct ProviderInfo {
    char *wLang;
};


/**
 * Generates the forecast query string
 *
 * @param cQuery Buffer to contain the query
 * @param pczWOEID WOEID string
 * @param czUnits Units character (length of 1)
 *
 * @return 0 on success, -1 on failure
 */
static gchar *
getForecastQuery(gdouble latitude, gdouble longitude, const gchar czUnits, const gchar *lang)
{
    return g_strdup_printf("http://api.openweathermap.org/data/2.5/weather?"
                           "lat=%.4f&lon=%.4f&APPID=" WEATHER_APPID "&mode=xml&"
                           "units=%s%s%s", latitude, longitude,
                           czUnits == 'c' ? "metric" : "imperial",
                           lang ? "&lang=" : "", lang ? lang : "");
}

/**
 * Converts the passed-in string from UTF-8 to ASCII for http transmisison.
 *
 * @param pczInString String to convert
 *
 * @return The converted string which MUST BE FREED BY THE CALLER.
 */
static gchar *
convertToASCII(const gchar *pczInString)
{
  // for UTF-8 to ASCII conversions
  setlocale(LC_CTYPE, "en_US");

  GError * pError = NULL;

  gsize szBytesRead = 0;
  gsize szBytesWritten = 0;

  gchar * pcConvertedString = g_convert(pczInString,
                                        strlen(pczInString),
                                        "ASCII//TRANSLIT",
                                        "UTF-8",
                                        &szBytesRead,
                                        &szBytesWritten,
                                        &pError);

  if (pError)
    {
      LXW_LOG(LXW_ERROR, "openweathermap::convertToASCII(%s): Error: %s",
              pczInString, pError->message);

      g_error_free(pError);

      pcConvertedString = g_strndup(pczInString, strlen(pczInString));
    }

  // now escape space, if any
  xmlChar * pxEscapedString = xmlURIEscapeStr((const xmlChar *)pcConvertedString, NULL);

  if (pxEscapedString)
    {
      // release ConvertedString, reset it, then release EscapedString.
      // I know it's confusing, but keeps everything as a gchar and g_free
      g_free(pcConvertedString);

      pcConvertedString = g_strndup((const gchar *)pxEscapedString,
                                    strlen((const gchar *)pxEscapedString));

      xmlFree(pxEscapedString);
    }

  // restore locale to default
  setlocale(LC_CTYPE, "");

  return pcConvertedString;
}

/**
 * Compares two strings and then sets the storage variable to the second
 * value if the two do not match. The storage variable is cleared first.
 *
 * @param pcStorage Pointer to the storage location with the first value.
 * @param pczString2 The second string.
 * @param szString2 The length of the second string.
 *
 * @return 0 on succes, -1 on failure.
 */
static gint
setStringIfDifferent(gchar ** pcStorage,
                     const gchar * pczString2,
                     const gsize szString2)
{
  // if diffrent, clear and set
  if (g_strcmp0(*pcStorage, pczString2))
    {
      g_free(*pcStorage);

      *pcStorage = g_strndup(pczString2, szString2);
    }

  return 0;
}

/**
 * Compares the URL of an image to the 'new' value. If the two
 * are different, the image at the 'new' URL is retrieved and replaces
 * the old one. The old one is freed.
 *
 * @param pcStorage Pointer to the storage location with the first value.
 * @param pImage Pointer to the image storage.
 * @param pczNewURL The new url.
 * @param szURLLength The length of the new URL.
 *
 * @return 0 on succes, -1 on failure.
 */
static gint
setImageIfDifferent(gchar ** pcStorage,
                    GdkPixbuf ** pImage,
                    const gchar * pczNewURL,
                    const gsize szURLLength)
{
  int err = 0;

  // if diffrent, clear and set
  if (g_strcmp0(*pcStorage, pczNewURL))
    {
      g_free(*pcStorage);

      *pcStorage = g_strndup(pczNewURL, szURLLength);

      if (*pImage)
        {
          g_object_unref(*pImage);

          *pImage = NULL;
        }

      // retrieve the URL and create the new image
      CURLcode iRetCode = 0;
      gint iDataSize = 0;
      char * pResponse = NULL;

      iRetCode = getURL(pczNewURL, &pResponse, &iDataSize, NULL);

      if (!pResponse || iRetCode != CURLE_OK)
        {
          LXW_LOG(LXW_ERROR, "openweathermap::setImageIfDifferent(): Failed to get URL (%d, %d)",
                  iRetCode, iDataSize);
          g_free(pResponse);

          return -1;
        }

      GInputStream * pInputStream = g_memory_input_stream_new_from_data(pResponse,
                                                                        iDataSize,
                                                                        g_free);

      GError * pError = NULL;

      *pImage = gdk_pixbuf_new_from_stream(pInputStream,
                                           NULL,
                                           &pError);

      if (!*pImage)
        {
          LXW_LOG(LXW_ERROR, "openweathermap::setImageIfDifferent(): PixBuff allocation failed: %s",
                  pError->message);

          g_error_free(pError);

          err = -1;
        }

      if (!g_input_stream_close(pInputStream, NULL, &pError))
        {
          LXW_LOG(LXW_ERROR, "openweathermap::setImageIfDifferent(): InputStream closure failed: %s",
                  pError->message);

          g_error_free(pError);

          err = -1;
        }

    }

  return err;
}

/**
 * Compares an integer to a converted string and then sets the storage variable
 * to the second value if the two do not match.
 *
 * @param piStorage Pointer to the storage location with the first value.
 * @param pczString2 The second string.
 *
 * @return 0 on succes, -1 on failure.
 */
static gint
setIntIfDifferent(gint * piStorage, const gchar * pczString2)
{
  gint iValue = (gint)g_ascii_strtoll((pczString2)?pczString2:"0", NULL, 10);

  // if diffrent, set
  if (*piStorage != iValue)
    {
      *piStorage = iValue;
    }

  return 0;
}

static gint
setTimeIfDifferent(gchar ** pcStorage, const gchar * pczString)
{
  int hour, min, sec;
  char * setTime = pczString ? strchr(pczString, 'T') : NULL;
  char adjTime[16];

  if (setTime && sscanf(setTime, "T%2u:%2u:%2u", &hour, &min, &sec) == 3)
    {
      sec -= timezone % 60;
      min -= (timezone / 60) % 60;
      hour -= timezone / 3600;
      if (sec < 0)
        {
          sec += 60;
          min--;
        }
      min += sec / 60;
      sec %= 60;
      if (min < 0)
        {
          min += 60;
          hour--;
        }
      hour += min / 60;
      min %= 60;
      if (hour < 0)
        {
          hour += 24;
        }
      hour %= 24;
      snprintf(adjTime, sizeof(adjTime), "%02d:%02d:%02d", hour, min, sec); // FIXME: AM/PM
      setTime = adjTime;
    }
  else
   setTime = NULL;

  return setStringIfDifferent(pcStorage, setTime, setTime ? strlen(setTime) : 0);
}

static void
processCityNode(ForecastInfo * pEntry, xmlNodePtr pNode)
{
    xmlNodePtr pCurr;

    for (pCurr = pNode->children; pCurr; pCurr = pCurr->next)
    {
        if (pCurr->type == XML_ELEMENT_NODE)
        {
            if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("sun"))) // rise="2019-02-16T05:06:50" set="2019-02-16T15:17:50"
            {
                char * rise = CHAR_P(xmlGetProp(pCurr, XMLCHAR_P("rise")));
                char * set = CHAR_P(xmlGetProp(pCurr, XMLCHAR_P("set")));

                setTimeIfDifferent(&pEntry->pcSunrise_, rise);
                setTimeIfDifferent(&pEntry->pcSunset_, set);
                xmlFree(rise);
                xmlFree(set);
            }
        }
    }
}

static void
processWindNode(ForecastInfo * pEntry, xmlNodePtr pNode, const gchar czUnits)
{
    xmlNodePtr pCurr;

    for (pCurr = pNode->children; pCurr; pCurr = pCurr->next)
    {
        if (pCurr->type == XML_ELEMENT_NODE)
        {
            if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("speed"))) // value="5" name="Gentle Breeze"
            {
                char * value = CHAR_P(xmlGetProp(pCurr, XMLCHAR_P("value")));
                const char * units = (czUnits == 'f') ? _("Mph") : _("m/s");

                setIntIfDifferent(&pEntry->iWindSpeed_, value);
                setStringIfDifferent(&pEntry->units_.pcSpeed_, units, strlen(units));
                xmlFree(value);
            }
            else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("direction"))) // value="270" code="W" name="West"
            {
                char * code = CHAR_P(xmlGetProp(pCurr, XMLCHAR_P("code")));
                const char * name = (code && *code) ? _(code) : NULL;
                gsize nlen;

                if (!name)
                {
                    xmlFree(code);
                    code = CHAR_P(xmlGetProp(pCurr, XMLCHAR_P("value")));
                    if (code)
                    {
                        gint degree = atoi(code);
                        name = WIND_DIRECTION(degree);
                    }
                }
                nlen = (name)?strlen(name):0;
                setStringIfDifferent(&pEntry->pcWindDirection_, name, nlen);
                xmlFree(code);
            }
        }
    }
}

/**
 * Parses the response and fills in the supplied list with entries (if any)
 *
 * @param pResponse Pointer to the response received.
 * @param pList Pointer to the pointer to the list to populate.
 * @param pForecast Pointer to the pointer to the forecast to retrieve.
 *
 * @return 0 on success, -1 on failure
 *
 * @note If the pList pointer is NULL or the pForecast pointer is NULL,
 *       nothing is done and failure is returned. Otherwise, the appropriate
 *       pointer is set based on the name of the XML element:
 *       'Result' for GList (pList)
 *       'channel' for Forecast (pForecast)
 */
static gint
parseResponse(const char * pResponse, GList ** pList, ForecastInfo ** pForecast, const gchar czUnits)
{
  xmlDocPtr pDoc = xmlReadMemory(pResponse,
                                 strlen(pResponse),
                                 "",
                                 NULL,
                                 0);

  if (!pDoc)
    {
      // failed
      LXW_LOG(LXW_ERROR, "openweathermap::parseResponse(): Failed to parse response %s",
              pResponse);

      return -1;
    }

  xmlNodePtr pRoot = xmlDocGetRootElement(pDoc);

  // the second part of the if can be broken out
  if (!pRoot || !xmlStrEqual(pRoot->name, CONSTXMLCHAR_P("current")))
    {
      // failed
      LXW_LOG(LXW_ERROR, "openweathermap::parseResponse(): Failed to retrieve root %s",
              pResponse);

      xmlFreeDoc(pDoc);

      return -1;
    }

  // have some results...
  xmlNodePtr pNode = pRoot->children;

  ForecastInfo * pEntry = NULL;

  if (pForecast)
    {
      if (*pForecast)
        {
          pEntry = *pForecast;
        }
      else
        {
          pEntry = (ForecastInfo *)g_try_new0(ForecastInfo, 1);
        }
    }

  if (!pEntry)
    {
      xmlFreeDoc(pDoc);
      return -1;
    }

  for (pNode = pRoot->children; pNode; pNode = pNode->next)
    {
      if (pNode && pNode->type == XML_ELEMENT_NODE)
        {
          if (xmlStrEqual(pNode->name, CONSTXMLCHAR_P("city")))
            {
              processCityNode(pEntry, pNode);
            }
          else if (xmlStrEqual(pNode->name, CONSTXMLCHAR_P("temperature"))) // value="3" min="3" max="3" unit="metric"
            {
              char * value = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("value")));
//              char * min = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("min")));
//              char * max = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("max")));
              char * unit = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("unit")));

              setIntIfDifferent(&pEntry->iTemperature_, value);
//              setIntIfDifferent(&pEntry->today_.iLow_, min);
//              setIntIfDifferent(&pEntry->today_.iHigh_, max);
              switch (unit[0])
                {
                  case 'c': case 'C': /* Celsius */
                  case 'm': /* metric */
                    setStringIfDifferent(&pEntry->units_.pcTemperature_, "C", 1);
                    break;
                  case 'f': case 'F': /* Fahrengeith */
                  case 'i': /* imperial */
                    setStringIfDifferent(&pEntry->units_.pcTemperature_, "F", 1);
                    break;
                  default: /* Kelvin */
                    setStringIfDifferent(&pEntry->units_.pcTemperature_, "K", 1);
                    break;
                }
              xmlFree(value);
//              xmlFree(min);
//              xmlFree(max);
              xmlFree(unit);
            }
          else if (xmlStrEqual(pNode->name, CONSTXMLCHAR_P("humidity"))) // value="93" unit="%"
            {
              char * value = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("value")));

              setIntIfDifferent(&pEntry->iHumidity_, value);
              xmlFree(value);
            }
          else if (xmlStrEqual(pNode->name, CONSTXMLCHAR_P("pressure"))) // value="1022" unit="hPa"
            {
              char * value = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("value")));
              char * unit = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("unit")));
              gsize ulen = (value)?strlen(value):0;

              pEntry->dPressure_ = g_strtod((value)?value:"0", NULL);
              setStringIfDifferent(&pEntry->units_.pcPressure_, unit, ulen);
              xmlFree(value);
              xmlFree(unit);
            }
          else if (xmlStrEqual(pNode->name, CONSTXMLCHAR_P("wind")))
            {
              processWindNode(pEntry, pNode, czUnits);
            }
          else if (xmlStrEqual(pNode->name, CONSTXMLCHAR_P("clouds"))) // value="40" name="scattered clouds"
            {
              char * value = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("name")));
              gsize vlen = (value)?strlen(value):0;

              setStringIfDifferent(&pEntry->pcClouds_, value, vlen);
              xmlFree(value);
            }
          else if (xmlStrEqual(pNode->name, CONSTXMLCHAR_P("visibility"))) // value="7000"
            {
              char * value = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("value")));
              const char * units = _("m");

              pEntry->dVisibility_ = g_strtod((value)?value:"0", NULL);
              setStringIfDifferent(&pEntry->units_.pcDistance_, units, strlen(units));
              xmlFree(value);
            }
          else if (xmlStrEqual(pNode->name, CONSTXMLCHAR_P("precipitation"))) // mode="no" // value="0.025" mode="rain"
            {
            }
          else if (xmlStrEqual(pNode->name, CONSTXMLCHAR_P("weather"))) // number="701" value="mist" icon="50n"
            {
              char * value = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("value")));
              char * icon = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("icon")));
              char * number = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("number")));
              char * pcImageURL = NULL;
              gsize vlen = (value)?strlen(value):0;

              if (icon)
                pcImageURL = g_strdup_printf("http://openweathermap.org/img/w/%s.png", icon);
              setImageIfDifferent(&pEntry->pcImageURL_,
                                  &pEntry->pImage_,
                                  pcImageURL,
                                  strlen(pcImageURL));

              if (number && *number && atoi(number) < 800) /* not clear */
                {
                  setStringIfDifferent(&pEntry->pcConditions_, value, vlen);
                }
              else
                {
                  g_free(pEntry->pcConditions_);
                  pEntry->pcConditions_ = NULL;
                }

              xmlFree(value);
              xmlFree(icon);
              xmlFree(number);
              g_free(pcImageURL);
            }
          else if (xmlStrEqual(pNode->name, CONSTXMLCHAR_P("lastupdate"))) // value="2019-02-16T18:00:00"
            {
              char * value = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("value")));

              setTimeIfDifferent(&pEntry->pcTime_, value);
              xmlFree(value);
            }
        }// end if element
    }// end for pRoot children

  *pForecast = pEntry;

  xmlFreeDoc(pDoc);

  return 0;
}

/**
 * pairs: first is ISO code, second is code on site
 */
static const char *localeTranslations[] = {
    "cs", "cz", /* Checz */
    "ko", "kr", /* Korean */
    "lv", "la", /* Latvian */
    "sv", "se", /* Swedish */
    "uk", "ua", /* Ukrainian */
    "zh_CN", "zh_cn",
    "zh_TW", "zh_tw",
    NULL
};

/**
 * Initializes the internals: XML
 *
 */
static ProviderInfo *initOWM(void)
{
    ProviderInfo *info = g_malloc(sizeof(ProviderInfo));

    if (!info)
        /* out of memory! */
        return info;

    if (!g_iInitialized)
    {
        xmlInitParser();
        g_iInitialized = 1;
    }

    const char *locale = setlocale(LC_MESSAGES, NULL); /* query locale */
    const char **localeTranslation = localeTranslations;

    tzset();
    info->wLang = g_strndup(locale, 2);
    if (locale)
    {
        for (; *localeTranslation; localeTranslation += 2)
        {
            if (strncmp(localeTranslation[0], locale,
                        strlen(localeTranslation[0])) == 0)
            {
                g_free(info->wLang);
                info->wLang = g_strdup(localeTranslation[1]);
                break;
            }
        }
    }

    //g_debug("%s: %p",__func__,info);
    return info;
}

/**
 * Cleans up the internals: XML
 *
 */
static void freeOWM(ProviderInfo *instance)
{
    //g_debug("%s: %p",__func__,instance);
    g_free(instance->wLang);
    g_free(instance);
}

static int processOSMPlace(LocationInfo *info, xmlNodePtr pNode)
{
/*
type=".....":
display_name="Дуда, Харгита, 537302, Румунія" 1 2 4
display_name="Дуда, Рытанский сельский Совет, Островецький район, Гродненська область, Білорусь" 1 2+ 5
display_name="Duda, Bali, Індонезія" 1 2 3
display_name="Dudar, Muzaffargarh District, Пенджаб, Пакистан" 1 2+ 3
display_name="Дударків, Бориспільський район, Київська область, 08330, Україна" 1 2+ 5
display_name="Berlin, Hartford County, Коннектикут, 06037, Сполучені Штати Америки" 1 2+ 5
type="city":
display_name="Київ, Шевченківський район, Київ, 1001, Україна" 1 3 5
display_name="Київ, Україна" 1 - 2
display_name="Житомир, Житомирська міська територіальна громада, Житомирська область, 10000-10499, Україна" 1 3 5
display_name="Житомир, Житомирська міська територіальна громада, Житомирська область, Україна" 1 3 4
display_name="Берлін, 10117, Німеччина" 1 - 3
display_name="Berlin, Coös County, Нью-Гемпшир, 03570, Сполучені Штати Америки" 1 3 5
display_name="City of Berlin, Green Lake County, Вісконсин, Сполучені Штати Америки" 1 3 4
 */
    char *value = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("class")));
    char *type;
    xmlNodePtr pCurr;
    int res;

    if (!value) /* no class property */
        goto _fail;

    res = strcmp(value, "place");
    xmlFree(value);
    if (res != 0) /* ignore other than class="place" */
        goto _fail;

    value = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("lon")));
    if (!value) /* no longitude */
        goto _fail;
    info->dLongitude_ = g_strtod(value, NULL);
    xmlFree(value);

    value = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("lat")));
    if (!value) /* no latitude */
        goto _fail;
    info->dLatitude_ = g_strtod(value, NULL);
    xmlFree(value);

    type = CHAR_P(xmlGetProp(pNode, XMLCHAR_P("type")));

    for (pCurr = pNode->children; pCurr; pCurr = pCurr->next)
    {
        if (pCurr && pCurr->type == XML_ELEMENT_NODE)
        {
            value = CHAR_P(xmlNodeListGetString(pCurr->doc, pCurr->xmlChildrenNode, 1));
            if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P(type ? type : "city")))
                info->pcCity_ = g_strdup(value);
            else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("state")))
                info->pcState_ = g_strdup(value);
/*
            else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("county")))
                info->pcCounty_ = g_strdup(value);
*/
            else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("country")))
                info->pcCountry_ = g_strdup(value);
            xmlFree(value);
        }
    }

    xmlFree(type);

    return 1;

_fail:
    freeLocation(info);
    return 0;
}

static GList *parseOSMResponse(const gchar *pResponse, const gchar *locale)
{
/*
<searchresults timestamp="Sun, 17 Feb 19 01:59:27 +0000" attribution="Data © OpenStreetMap contributors, ODbL 1.0. http://www.openstreetmap.org/copyright" querystring="Дударків" polygon="false" exclude_place_ids="1537043" more_url="https://nominatim.openstreetmap.org/search.php?q=%D0%94%D1%83%D0%B4%D0%B0%D1%80%D0%BA%D1%96%D0%B2&exclude_place_ids=1537043&format=xml&accept-language=uk%2Cen%3Bq%3D0.9%2Cen-US%3Bq%3D0.8%2Cru%3Bq%3D0.7">
<place place_id="1537043" osm_type="node" osm_id="337521620" place_rank="19" boundingbox="50.429219,50.469219,30.93158,30.97158" lat="50.449219" lon="30.95158" display_name="Дударків, Бориспільський район, Київська область, 08330, Україна" class="place" type="village" importance="0.43621598500338" icon="https://nominatim.openstreetmap.org/images/mapicons/poi_place_village.p.20.png"/>
<place place_id="240722518" osm_type="relation" osm_id="8759567" place_rank="19" boundingbox="47.8622784,47.8705346,31.012428,31.024226" lat="47.8671228" lon="31.0179572" display_name="Київ, Доманівський район, Миколаївська область, Україна" class="place" type="hamlet" importance="0.275" icon="https://nominatim.openstreetmap.org/images/mapicons/poi_place_village.p.20.png"/>
<place place_id="127538" osm_type="node" osm_id="26150422" place_rank="15" boundingbox="50.2900644,50.6100644,30.3641037,30.6841037" lat="50.4500644" lon="30.5241037" display_name="Київ, Шевченківський район, Київ, 1001, Україна" class="place" type="city" importance="0.74145054816511" icon="https://nominatim.openstreetmap.org/images/mapicons/poi_place_city.p.20.png"/>
<place place_id="197890553" osm_type="relation" osm_id="421866" place_rank="16" boundingbox="50.2132422,50.590833,30.2363911,30.8276549" lat="50.4020865" lon="30.6146803128848" display_name="Київ, Україна" class="place" type="city" importance="0.74145054816511" icon="https://nominatim.openstreetmap.org/images/mapicons/poi_place_city.p.20.png"/>
</searchresults>
 */
    GList *list = NULL;
    xmlDocPtr pDoc = xmlReadMemory(pResponse, strlen(pResponse), "", NULL, 0);
    xmlNodePtr pRoot;
    xmlNodePtr pNode;
    char cUnits;

    if (!pDoc)
    {
        // failed
        LXW_LOG(LXW_ERROR, "openweathermap::parseOSMResponse(): Failed to parse response %s",
                pResponse);

        return NULL;
    }

    pRoot = xmlDocGetRootElement(pDoc);

    // the second part of the if can be broken out
    if (!pRoot || !xmlStrEqual(pRoot->name, CONSTXMLCHAR_P("searchresults")))
    {
        // failed
        LXW_LOG(LXW_ERROR, "openweathermap::parseResponse(): Failed to retrieve root %s",
                pResponse);

        xmlFreeDoc(pDoc);

        return NULL;
    }

    /* guess units by locale */
    if (strncmp(locale, "en", 2) == 0 || strncmp(locale, "my", 2) == 0)
        cUnits = 'f';
    else
        cUnits = 'c';

    // have some results...
    for (pNode = pRoot->children; pNode; pNode = pNode->next)
    {
        if (pNode && pNode->type == XML_ELEMENT_NODE)
        {
            if (xmlStrEqual(pNode->name, CONSTXMLCHAR_P("place")))
            {
                LocationInfo *pInfo = g_new0(LocationInfo, 1);

                /* preset units by locale */
                pInfo->cUnits_ = cUnits;
                /* validate and process all fields */
                if (processOSMPlace(pInfo, pNode))
                    list = g_list_prepend(list, pInfo);
            }
        }// end if element
    }// end for pRoot children

    xmlFreeDoc(pDoc);

    return g_list_reverse(list);
}

/**
 * Retrieves the details for the specified location from OpenStreetMap server
 *
 * @param pczLocation The string containing the name/code of the location
 *
 * @return A pointer to a list of LocationInfo entries, possibly empty,
 *         if no details were found. Caller is responsible for freeing the list.
 */
GList *
getOSMLocationInfo(ProviderInfo * instance, const gchar * pczLocation)
{
    GList * pList = NULL;
    gchar * pcEscapedLocation = convertToASCII(pczLocation);
    gchar * cQuery = g_strdup_printf("https://nominatim.openstreetmap.org/search?"
                                     "q=%s&addressdetails=1&format=xml",
                                     pcEscapedLocation);
    const gchar * locale;
    struct utsname uts;
    gchar * pResponse = NULL;
    CURLcode iRetCode;
    gint iDataSize = 0;
    char userAgentHeader[256];
    char languageHeader[32];
    const char *headers[] = { userAgentHeader, languageHeader, NULL };

    /* parse and search */
    g_free(pcEscapedLocation);
    locale = setlocale(LC_MESSAGES, NULL);
    if (!locale)
        locale = "en";
    uname(&uts);

    snprintf(languageHeader, sizeof(languageHeader), "Accept-Language: %.2s,en",
             locale);
    snprintf(userAgentHeader, sizeof(userAgentHeader), "User-Agent: " PACKAGE "/" VERSION "(%s %s)",
             uts.sysname, uts.machine);

    //g_debug("cQuery %s",cQuery);
    //g_debug("userAgentHeader %s",userAgentHeader);

    LXW_LOG(LXW_DEBUG, "openweathermap::getLocationInfo(%s): query[%d]: %s",
            pczLocation, iRet, cQuery);

    iRetCode = getURL(cQuery, &pResponse, &iDataSize, headers);

    //g_debug("pResponse %s",pResponse);
    g_free(cQuery);

    if (!pResponse || iRetCode != CURLE_OK)
    {
        LXW_LOG(LXW_ERROR, "openweathermap::getLocationInfo(%s): Failed with error code %d",
                pczLocation, iRetCode);
    }
    else
    {
        LXW_LOG(LXW_DEBUG, "openweathermap::getLocationInfo(%s): Response code: %d, size: %d",
                pczLocation, iRetCode, iDataSize);

        LXW_LOG(LXW_VERBOSE, "openweathermap::getLocation(%s): Contents: %s",
                pczLocation, (const char *)pResponse);

        pList = parseOSMResponse(pResponse, locale);
    }

    g_free(pResponse);

    return pList;
}

/**
 * Retrieves the forecast for the specified location WOEID
 *
 * @param pczWOEID The string containing the WOEID of the location
 * @param czUnits The character containing the units for the forecast (c|f)
 * @param pForecast The pointer to the forecast to be filled. If set to NULL,
 *                  a new one will be allocated.
 *
 */
static ForecastInfo *getForecastInfo(ProviderInfo *instance,
                                     LocationInfo *location,
                                     ForecastInfo *lastForecast)
{
  CURLcode iRetCode = 0;
  gint iDataSize = 0;
  gint iRet;
  gchar * cQueryBuffer = getForecastQuery(location->dLatitude_,
                                          location->dLongitude_,
                                          location->cUnits_, instance->wLang);
  ForecastInfo *pForecast = lastForecast;
  char * pResponse = NULL;

  LXW_LOG(LXW_DEBUG, "openweathermap::getForecastInfo(%s): query[%d]: %s",
          pczWOEID, iRet, cQueryBuffer);
//g_debug("query: %s",cQueryBuffer);

  iRetCode = getURL(cQueryBuffer, &pResponse, &iDataSize, NULL);
//g_debug("response: %s",pResponse);

  if (!pResponse || iRetCode != CURLE_OK)
    {
      LXW_LOG(LXW_ERROR, "openweathermap::getForecastInfo(%s): Failed with error code %d",
              pczWOEID, iRetCode);
    }
  else
    {
      LXW_LOG(LXW_DEBUG, "openweathermap::getForecastInfo(%s): Response code: %d, size: %d",
              pczWOEID, iRetCode, iDataSize);

      LXW_LOG(LXW_VERBOSE, "openweathermap::getForecastInfo(%s): Contents: %s",
              pczWOEID, (const char *)pResponse);

      iRet = parseResponse(pResponse, NULL, &pForecast, location->cUnits_);

      LXW_LOG(LXW_DEBUG, "openweathermap::getForecastInfo(%s): Response parsing returned %d",
              pczWOEID, iRet);

      if (iRet)
        {
          freeForecast(pForecast);
          pForecast = NULL;
        }
      else
        pForecast->iWindChill_ = -1000; /* set it to invalid value */
    }

  g_free(cQueryBuffer);
  g_free(pResponse);

  return pForecast;
}

provider_callback_info OpenWeatherMapCallbacks = {
  .name = "openweathermap",
  .description = N_("OpenWeatherMap"),
  .initProvider = initOWM,
  .freeProvider = freeOWM,
  .getLocationInfo = getOSMLocationInfo,
  .getForecastInfo = getForecastInfo,
  .supports_woeid = FALSE
};
