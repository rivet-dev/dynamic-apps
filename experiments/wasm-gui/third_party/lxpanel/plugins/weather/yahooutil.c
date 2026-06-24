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

/* Provides utilities to use Yahoo's weather services */

#include "httputil.h"
#include "location.h"
#include "forecast.h"
#include "logutil.h"
#include "yahooutil.h"

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

static gint g_iInitialized;

static const gchar * WOEID_QUERY = "SELECT%20*%20FROM%20geo.places%20WHERE%20text=";
static const gchar * FORECAST_QUERY_P1 = "SELECT%20*%20FROM%20weather.forecast%20WHERE%20woeid=";
static const gchar * FORECAST_QUERY_P2 = "%20and%20u=";
static const gchar * FORECAST_URL = "http://query.yahooapis.com/v1/public/yql?format=xml&q=";

struct ProviderInfo {
};

/**
 * Returns the length for the appropriate WOEID query
 *
 * @param pczLocation Location string to be used inside query
 *
 * @return length of resulting query on success or 0 on failure
 */
static gsize
getWOEIDQueryLength(const gchar * pczLocation)
{
  // len of all strings plus two quotes ('%22') and \0
  return strlen(FORECAST_URL) + 
    strlen(WOEID_QUERY) + 
    strlen(pczLocation) + 7;
}

/**
 * Returns the length for the appropriate Forecast query
 *
 * @param pczWOEID WOEID string to be used inside query
 *
 * @return length of resulting query on success or 0 on failure
 */
static gsize
getForecastQueryLength(const gchar * pczWOEID)
{
  // len of all strings plus four quotes ('%27'), units char and \0
  return strlen(FORECAST_URL) + 
    strlen(FORECAST_QUERY_P1) + 
    strlen(pczWOEID) + 14 +
    strlen(FORECAST_QUERY_P2);
}

/**
 * Generates the WOEID query string
 *
 * @param cQuery Buffer to contain the query
 * @param pczLocation Location string
 *
 * @return 0 on success, -1 on failure
 */
static gint
getWOEIDQuery(gchar * pcQuery, const gchar * pczLocation)
{
  gsize totalLength = getWOEIDQueryLength(pczLocation);
  
  snprintf(pcQuery, totalLength, "%s%s%s%s%s", 
           FORECAST_URL, WOEID_QUERY, "%22", pczLocation, "%22");

  pcQuery[totalLength] = '\0';

  return 0;
}

/**
 * Generates the forecast query string
 *
 * @param cQuery Buffer to contain the query
 * @param pczWOEID WOEID string
 * @param czUnits Units character (length of 1)
 *
 * @return 0 on success, -1 on failure
 */
static gint
getForecastQuery(gchar * pcQuery, const gchar * pczWOEID, const gchar czUnits)
{
  gsize totalLength = getForecastQueryLength(pczWOEID);

  snprintf(pcQuery, totalLength, "%s%s%s%s%s%s%s%c%s", 
           FORECAST_URL, 
           FORECAST_QUERY_P1,
           "%22",
           pczWOEID, 
           "%22",
           FORECAST_QUERY_P2,
           "%22",
           czUnits,
           "%22");

  pcQuery[totalLength] = '\0';

  return 0;
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
      LXW_LOG(LXW_ERROR, "yahooutil::convertToASCII(%s): Error: %s", 
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
      CURLcode iRetCode;
      gint iDataSize = 0;
      gpointer pResponse = NULL;
      iRetCode = getURL(pczNewURL, &pResponse, &iDataSize, NULL);

      if (!pResponse || iRetCode != CURLE_OK)
        {
          LXW_LOG(LXW_ERROR, "yahooutil::setImageIfDifferent(): Failed to get URL (%d, %d)", 
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
          LXW_LOG(LXW_ERROR, "yahooutil::setImageIfDifferent(): PixBuff allocation failed: %s",
                  pError->message);

          g_error_free(pError);
          
          err = -1;
        }

      if (!g_input_stream_close(pInputStream, NULL, &pError))
        {
          LXW_LOG(LXW_ERROR, "yahooutil::setImageIfDifferent(): InputStream closure failed: %s",
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

/**
 * Processes the passed-in node to generate a LocationInfo entry
 *
 * @param pNode Pointer to the XML Result Node.
 *
 * @return A newly created LocationInfo entry on success, or NULL on failure.
 */
static gpointer
processResultNode(xmlNodePtr pNode)
{
  if (!pNode)
    {
      return NULL;
    }

  LocationInfo * pEntry = (LocationInfo *)g_try_new0(LocationInfo, 1);

  if (!pEntry)
    {
      return NULL;
    }

  xmlNodePtr pCurr = pNode->xmlChildrenNode;

  for (; pCurr != NULL; pCurr = pCurr->next)
    {
      if (pCurr->type == XML_ELEMENT_NODE)
        {
          const char * pczContent = CONSTCHAR_P(xmlNodeListGetString(pCurr->doc, 
                                                                     pCurr->xmlChildrenNode, 
                                                                     1));
          
          gsize contentLength = ((pczContent)?strlen(pczContent):0); // 1 is null char

          if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("name")))
            {
              pEntry->pcCity_ = g_strndup(pczContent, contentLength);
            }
          else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("admin1")))
            {
              pEntry->pcState_ = g_strndup(pczContent, contentLength);
            }
          else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("country")))
            {
              pEntry->pcCountry_ = g_strndup(pczContent, contentLength);
            }
          else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("woeid")))
            {
              pEntry->pcWOEID_ = g_strndup(pczContent, contentLength);
            }
          else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("locality1")))
            {
              pEntry->pcAlias_ = g_strndup(pczContent, contentLength);
            }

            xmlFree(XMLCHAR_P(pczContent));
        }

    }

  return pEntry;
}

/**
 * Processes the passed-in node to generate a LocationInfo entry
 *
 * @param pEntry Pointer to the pointer to the ForecastInfo entry being filled.
 * @param pNode Pointer to the XML Item Node.
 *
 * @return 0 on success, -1 on failure
 */
static gint
processItemNode(gpointer * pEntry, xmlNodePtr pNode)
{
  if (!pNode || !pEntry)
    {
      return -1;
    }

  ForecastInfo * pInfo = *((ForecastInfo **)pEntry);

  xmlNodePtr pCurr = pNode->xmlChildrenNode;

  int iForecastCount = 0;

  for (; pCurr != NULL; pCurr = pCurr->next)
    {
      if (pCurr->type == XML_ELEMENT_NODE)
        {
          if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("condition")))
            {
              const char * pczDate = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("date")));
              const char * pczTemp = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("temp")));
              const char * pczText = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("text")));

              gsize dateLength = ((pczDate)?strlen(pczDate):0);
              gsize textLength = ((pczText)?strlen(pczText):0);

              setStringIfDifferent(&pInfo->pcTime_, pczDate, dateLength);

              setIntIfDifferent(&pInfo->iTemperature_, pczTemp);

              setStringIfDifferent(&pInfo->pcConditions_, pczText, textLength);

              xmlFree(XMLCHAR_P(pczDate));
              xmlFree(XMLCHAR_P(pczTemp));
              xmlFree(XMLCHAR_P(pczText));
            }
          else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("description")))
            {
              char * pcContent = CHAR_P(xmlNodeListGetString(pCurr->doc, 
                                                             pCurr->xmlChildrenNode, 
                                                             1));
          
              char * pcSavePtr = NULL;

              // initial call to find the first '"'
              strtok_r(pcContent, "\"", &pcSavePtr);

              // second call to find the second '"'
              char * pcImageURL = strtok_r(NULL, "\"", &pcSavePtr);

              // found the image
              if (pcImageURL && strstr(pcImageURL, "yimg.com"))
                {
                  LXW_LOG(LXW_DEBUG, "yahooutil::processItemNode(): IMG URL: %s",
                          pcImageURL);

                  setImageIfDifferent(&pInfo->pcImageURL_, 
                                      &pInfo->pImage_,
                                      pcImageURL, 
                                      strlen(pcImageURL));
                }
                  
              xmlFree(XMLCHAR_P(pcContent));
            }
          else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("forecast")))
            {
              ++iForecastCount;

              const char * pczDay = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("day")));
              const char * pczHigh = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("high")));
              const char * pczLow = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("low")));
              const char * pczText = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("text")));

              gsize dayLength = ((pczDay)?strlen(pczDay):0);
              gsize textLength = ((pczText)?strlen(pczText):0);

              if (iForecastCount == 1)
                {
                  setStringIfDifferent(&pInfo->today_.pcDay_, pczDay, dayLength);

                  setIntIfDifferent(&pInfo->today_.iHigh_, pczHigh);

                  setIntIfDifferent(&pInfo->today_.iLow_, pczLow);

                  setStringIfDifferent(&pInfo->today_.pcConditions_, pczText, textLength);
                }
              else
                {
                  setStringIfDifferent(&pInfo->tomorrow_.pcDay_, pczDay, dayLength);

                  setIntIfDifferent(&pInfo->tomorrow_.iHigh_, pczHigh);

                  setIntIfDifferent(&pInfo->tomorrow_.iLow_, pczLow);

                  setStringIfDifferent(&pInfo->tomorrow_.pcConditions_, pczText, textLength);
                }

              xmlFree(XMLCHAR_P(pczDay));
              xmlFree(XMLCHAR_P(pczHigh));
              xmlFree(XMLCHAR_P(pczLow));
              xmlFree(XMLCHAR_P(pczText));
            }

        }

    }

  return 0;
}


/**
 * Processes the passed-in node to generate a ForecastInfo entry
 *
 * @param pNode Pointer to the XML Channel Node.
 * @param pEntry Pointer to the ForecastInfo entry to be filled in.
 *
 * @return A newly created ForecastInfo entry on success, or NULL on failure.
 */
static gpointer
processChannelNode(xmlNodePtr pNode, ForecastInfo * pEntry)
{
  if (!pNode)
    {
      return NULL;
    }

  xmlNodePtr pCurr = pNode->xmlChildrenNode;

  for (; pCurr != NULL; pCurr = pCurr->next)
    {
      if (pCurr->type == XML_ELEMENT_NODE)
        {
          if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("title")))
            {
              /* Evaluate title to see if there was an error */
              char * pcContent = CHAR_P(xmlNodeListGetString(pCurr->doc, 
                                                             pCurr->xmlChildrenNode, 
                                                             1));
              
              if (strstr(pcContent, "Error"))
                {
                  xmlFree(XMLCHAR_P(pcContent));
                  
                  do
                    {
                      pCurr = pCurr->next;
                    } while (pCurr && !xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("item")));

                  xmlNodePtr pChild = (pCurr)?pCurr->xmlChildrenNode:NULL;
                  
                  for (; pChild != NULL; pChild = pChild->next)
                    {
                      if (pChild->type == XML_ELEMENT_NODE && 
                          xmlStrEqual(pChild->name, CONSTXMLCHAR_P("title")))
                        {
                          pcContent = CHAR_P(xmlNodeListGetString(pChild->doc, 
                                                                  pChild->xmlChildrenNode, 
                                                                  1));

                          LXW_LOG(LXW_ERROR, "yahooutil::processChannelNode(): Forecast retrieval error: %s",
                                  pcContent);


                          xmlFree(XMLCHAR_P(pcContent));
                        }
                    }

                  return NULL;
                }
              else
                {
                  xmlFree(XMLCHAR_P(pcContent));
                  /* ...and continue... */
                }

            }
          else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("item")))
            {
              /* item child element gets 'special' treatment */
              processItemNode((gpointer *)&pEntry, pCurr);
            }
          else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("units")))
            {
              // distance
              const char * pczDistance = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("distance")));

              gsize distanceLength = ((pczDistance)?strlen(pczDistance):0);

              setStringIfDifferent(&pEntry->units_.pcDistance_, pczDistance, distanceLength);

              xmlFree(XMLCHAR_P(pczDistance));

              // pressure
              const char * pczPressure = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("pressure")));

              gsize pressureLength = ((pczPressure)?strlen(pczPressure):0);

              setStringIfDifferent(&pEntry->units_.pcPressure_, pczPressure, pressureLength);

              xmlFree(XMLCHAR_P(pczPressure));

              // speed
              const char * pczSpeed = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("speed")));

              gsize speedLength = ((pczSpeed)?strlen(pczSpeed):0);

              setStringIfDifferent(&pEntry->units_.pcSpeed_, pczSpeed, speedLength);

              xmlFree(XMLCHAR_P(pczSpeed));

              // temperature
              const char * pczTemperature = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("temperature")));

              gsize temperatureLength = ((pczTemperature)?strlen(pczTemperature):0);

              setStringIfDifferent(&pEntry->units_.pcTemperature_, pczTemperature, temperatureLength);

              xmlFree(XMLCHAR_P(pczTemperature));
            }
          else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("wind")))
            {
              // chill
              const char * pczChill = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("chill")));

              setIntIfDifferent(&pEntry->iWindChill_, pczChill);

              xmlFree(XMLCHAR_P(pczChill));

              // direction
              const char * pczDirection = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("direction")));

              gint iValue = (gint)g_ascii_strtoll((pczDirection)?pczDirection:"999", NULL, 10);

              const gchar * pczDir = WIND_DIRECTION(iValue);

              setStringIfDifferent(&pEntry->pcWindDirection_, pczDir, strlen(pczDir));

              xmlFree(XMLCHAR_P(pczDirection));

              // speed
              const char * pczSpeed = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("speed")));

              setIntIfDifferent(&pEntry->iWindSpeed_, pczSpeed);

              xmlFree(XMLCHAR_P(pczSpeed));
            }
          else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("atmosphere")))
            {
              // humidity
              const char * pczHumidity = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("humidity")));

              setIntIfDifferent(&pEntry->iHumidity_, pczHumidity);

              xmlFree(XMLCHAR_P(pczHumidity));

              // pressure
              const char * pczPressure = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("pressure")));

              pEntry->dPressure_ = g_ascii_strtod((pczPressure)?pczPressure:"0", NULL);

              xmlFree(XMLCHAR_P(pczPressure));

              // visibility
              const char * pczVisibility = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("visibility")));

              pEntry->dVisibility_ = g_ascii_strtod((pczVisibility)?pczVisibility:"0", NULL);

              // need to divide by 100
              //pEntry->dVisibility_ = pEntry->dVisibility_/100;

              xmlFree(XMLCHAR_P(pczVisibility));

              // state
              const char * pczState = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("rising")));
                            
              pEntry->pressureState_ = (PressureState)g_ascii_strtoll((pczState)?pczState:"0", NULL, 10);

              xmlFree(XMLCHAR_P(pczState));
            }
          else if (xmlStrEqual(pCurr->name, CONSTXMLCHAR_P("astronomy")))
            {
              // sunrise
              const char * pczSunrise = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("sunrise")));

              gsize sunriseLength = ((pczSunrise)?strlen(pczSunrise):0);

              setStringIfDifferent(&pEntry->pcSunrise_, pczSunrise, sunriseLength);

              xmlFree(XMLCHAR_P(pczSunrise));

              // sunset
              const char * pczSunset = CONSTCHAR_P(xmlGetProp(pCurr, XMLCHAR_P("sunset")));

              gsize sunsetLength = ((pczSunset)?strlen(pczSunset):0);

              setStringIfDifferent(&pEntry->pcSunset_, pczSunset, sunsetLength);

              xmlFree(XMLCHAR_P(pczSunset));
            }
          
        }

    }

  return pEntry;
}

/**
 * Evaluates an XPath expression on the passed-in context.
 *
 * @param pContext Pointer to the context.
 * @param pczExpression The XPath expression to evaluate
 *
 * @return xmlNodeSetPtr pointing to the resulting node set, must be
 *         freed by the caller.
 */
static xmlNodeSetPtr
evaluateXPathExpression(xmlXPathContextPtr pContext, const char * pczExpression)
{
  xmlXPathObjectPtr pObject = xmlXPathEval(CONSTXMLCHAR_P(pczExpression), 
                                           pContext);

  if (!pObject || !pObject->nodesetval)
    {
      return NULL;
    }

  xmlNodeSetPtr pNodeSet = pObject->nodesetval;

  xmlXPathFreeNodeSetList(pObject);

  return pNodeSet;
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
parseResponse(gpointer pResponse, GList ** pList, ForecastInfo ** pForecast)
{
  int iLocation = (pList)?1:0;

  xmlDocPtr pDoc = xmlReadMemory(CONSTCHAR_P(pResponse),
                                 strlen(pResponse),
                                 "",
                                 NULL,
                                 0);

  if (!pDoc)
    {
      // failed
      LXW_LOG(LXW_ERROR, "yahooutil::parseResponse(): Failed to parse response %s",
              CONSTCHAR_P(pResponse));

      return -1;
    }

  xmlNodePtr pRoot = xmlDocGetRootElement(pDoc);
  
  // the second part of the if can be broken out
  if (!pRoot || !xmlStrEqual(pRoot->name, CONSTXMLCHAR_P("query")))
    {
      // failed
      LXW_LOG(LXW_ERROR, "yahooutil::parseResponse(): Failed to retrieve root %s",
              CONSTCHAR_P(pResponse));

      xmlFreeDoc(pDoc);

      return -1;
    }

  // use xpath to find /query/results/Result
  xmlXPathInit();

  xmlXPathContextPtr pXCtxt = xmlXPathNewContext(pDoc);

  const char * pczExpression = "/query/results/channel";

  if (iLocation)
    {
      xmlXPathRegisterNs(pXCtxt, CONSTXMLCHAR_P("ns1"),
                         CONSTXMLCHAR_P("http://where.yahooapis.com/v1/schema.rng"));

      pczExpression = "///ns1:place";
    }

  // have some results...
  xmlNodeSetPtr pNodeSet = evaluateXPathExpression(pXCtxt, pczExpression);

  if (!pNodeSet)
    {
      // error, or no results found -- failed
      xmlXPathFreeContext(pXCtxt);

      xmlFreeDoc(pDoc);

      return -1;
    }

  int iCount = 0;
  int iSize = pNodeSet->nodeNr;

  gint iRetVal = 0;

  for (; iCount < iSize; ++iCount)
    {
      if (pNodeSet->nodeTab)
        {
          xmlNodePtr pNode = pNodeSet->nodeTab[iCount];

          if (pNode && pNode->type == XML_ELEMENT_NODE)
            {
              if (xmlStrEqual(pNode->name, CONSTXMLCHAR_P("place")))
                {
                  gpointer pEntry = processResultNode(pNode);
                  
                  if (pEntry && pList)
                    {
                      *pList = g_list_prepend(*pList, pEntry);
                    }
                }
              else if (xmlStrEqual(pNode->name, CONSTXMLCHAR_P("channel")))
                {
                  ForecastInfo * pEntry = NULL;
                  
                  gboolean bNewed = FALSE;

                  /* Check if forecast is allocated, if not, 
                   * allocate and populate 
                   */
                  if (pForecast)
                    {
                      if (*pForecast)
                        {
                          pEntry = (ForecastInfo *)*pForecast;
                        }
                      else
                        {
                          pEntry = (ForecastInfo *)g_try_new0(ForecastInfo, 1);

                          bNewed = TRUE;
                        }
                  
                      if (!pEntry)
                        {
                          iRetVal = -1;
                        }
                      else
                        {
                          *pForecast = processChannelNode(pNode, pEntry);
                          
                          if (!*pForecast)
                            {
                              /* Failed, forecast is freed by caller */
                              
                              /* Unless it was just newed... */
                              if (bNewed)
                                {
                                  g_free(pEntry);
                                }
                          
                              iRetVal = -1;
                            }
                        }

                    }// end else if pForecast

                }// end else if 'channel'

            }// end if element

        }// end if nodeTab

    }// end for noteTab size

  xmlXPathFreeNodeSet(pNodeSet);

  xmlXPathFreeContext(pXCtxt);

  xmlFreeDoc(pDoc);

  return iRetVal;
}

/**
 * Initializes the internals: XML 
 *
 */
static ProviderInfo *
initializeYahooUtil(void)
{
#if 0 /* does not work anymore */
  if (!g_iInitialized)
    {
      xmlInitParser();

      g_iInitialized = 1;
    }
  return (ProviderInfo *)1;
#else
  return NULL;
#endif
}

/**
 * Cleans up the internals: XML 
 *
 */
static void
cleanupYahooUtil(ProviderInfo *instance G_GNUC_UNUSED)
{
  if (g_iInitialized)
    {
      // xmlCleanupParser(); // will crash if there is more than one libxml user

      g_iInitialized = 0;
    }
}

/**
 * Retrieves the details for the specified location
 *
 * @param pczLocation The string containing the name/code of the location
 *
 * @return A pointer to a list of LocationInfo entries, possibly empty, 
 *         if no details were found. Caller is responsible for freeing the list.
 */
static GList *
getLocationInfo(ProviderInfo * instance G_GNUC_UNUSED, const gchar * pczLocation)
{
  CURLcode iRetCode;
  gint iDataSize = 0;

  GList * pList = NULL;

  gchar * pcEscapedLocation = convertToASCII(pczLocation); 

  gsize len = getWOEIDQueryLength(pcEscapedLocation);

  gchar * cQueryBuffer = g_malloc0(len);

  gint iRet = getWOEIDQuery(cQueryBuffer, pcEscapedLocation);

  g_free(pcEscapedLocation);

  LXW_LOG(LXW_DEBUG, "yahooutil::getLocationInfo(%s): query[%d]: %s",
          pczLocation, iRet, cQueryBuffer);

  gpointer pResponse = NULL;

  iRetCode = getURL(cQueryBuffer, &pResponse, &iDataSize, NULL);

  if (!pResponse || iRetCode != CURLE_OK)
    {
      LXW_LOG(LXW_ERROR, "yahooutil::getLocationInfo(%s): Failed with error code %d",
              pczLocation, iRetCode);
    }
  else
    {
      LXW_LOG(LXW_DEBUG, "yahooutil::getLocationInfo(%s): Response code: %d, size: %d",
              pczLocation, iRetCode, iDataSize);

      LXW_LOG(LXW_VERBOSE, "yahooutil::getLocation(%s): Contents: %s", 
              pczLocation, (const char *)pResponse);

      iRet = parseResponse(pResponse, &pList, NULL);
      
      LXW_LOG(LXW_DEBUG, "yahooutil::getLocation(%s): Response parsing returned %d",
              pczLocation, iRet);

      if (iRet)
        {
          // failure
          g_list_free_full(pList, (GDestroyNotify)freeLocation);
        }

    }

  g_free(cQueryBuffer);
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
static ForecastInfo *getForecastInfo(ProviderInfo *instance G_GNUC_UNUSED,
                                     LocationInfo *location,
                                     ForecastInfo *lastForecast)
{
  CURLcode iRetCode;
  gint iDataSize = 0;
  ForecastInfo * pForecast = lastForecast;

  gsize len = getForecastQueryLength(location->pcWOEID_);

  gchar * cQueryBuffer = g_malloc(len + 1);

  gint iRet = getForecastQuery(cQueryBuffer, location->pcWOEID_, location->cUnits_);

  LXW_LOG(LXW_DEBUG, "yahooutil::getForecastInfo(%s): query[%d]: %s",
          location->pcWOEID_, iRet, cQueryBuffer);

  gpointer pResponse = NULL;

  iRetCode = getURL(cQueryBuffer, &pResponse, &iDataSize, NULL);

  if (!pResponse || iRetCode != CURLE_OK)
    {
      LXW_LOG(LXW_ERROR, "yahooutil::getForecastInfo(%s): Failed with error code %d",
              location->pcWOEID_, iRetCode);
    }
  else
    {
      LXW_LOG(LXW_DEBUG, "yahooutil::getForecastInfo(%s): Response code: %d, size: %d",
              location->pcWOEID_, iRetCode, iDataSize);

      LXW_LOG(LXW_VERBOSE, "yahooutil::getForecastInfo(%s): Contents: %s",
              location->pcWOEID_, (const char *)pResponse);

      iRet = parseResponse(pResponse, NULL, &pForecast);
    
      LXW_LOG(LXW_DEBUG, "yahooutil::getForecastInfo(%s): Response parsing returned %d",
              location->pcWOEID_, iRet);

      if (iRet)
        {
          freeForecast(pForecast);

          pForecast = NULL;
        }

    }

  g_free(cQueryBuffer);
  g_free(pResponse);

  return pForecast;
}

provider_callback_info YahooCallbacks = {
  .name = "yahoo",
  .description = N_("Yahoo! Weather"),
  .initProvider = initializeYahooUtil,
  .freeProvider = cleanupYahooUtil,
  .getLocationInfo = getLocationInfo,
  .getForecastInfo = getForecastInfo,
  .supports_woeid = TRUE
};
