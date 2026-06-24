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

/* Provides implementation for LocationInfo-specific functions */

#include "location.h" // includes glib.h
#include "logutil.h"

#include <stdio.h>
#include <string.h>

const gchar * LocationInfoFieldNames[] = { "alias",
                                           "city",
                                           "state",
                                           "country",
                                           "woeid",
                                           "units",
                                           "interval",
                                           "enabled",
                                           NULL};

/**
 * Provides the mechanism to free any data associated with 
 * the LocationInfo structure
 *
 * @param pData Entry to free.
 *
 */
void
freeLocation(LocationInfo * pEntry)
{
  if (!pEntry)
    {
      return;
    }

  g_free(pEntry->pcAlias_);
  g_free(pEntry->pcCity_);
  g_free(pEntry->pcState_);
  g_free(pEntry->pcCountry_);
  g_free(pEntry->pcWOEID_);

  g_free(pEntry);
}

/**
 * Prints the contents of the supplied entry to stdout
 *
 * @param pEntry Entry contents of which to print.
 *
 */
void
printLocation(LocationInfo * pInfo G_GNUC_UNUSED)
{
#ifdef DEBUG
  if (!pInfo)
    {
      LXW_LOG(LXW_ERROR, "location::printLocation(): Entry: NULL");
      
      return;
    }

  LXW_LOG(LXW_VERBOSE, "Entry:");
  LXW_LOG(LXW_VERBOSE, "\tAlias: %s", (const char *)pInfo->pcAlias_);
  LXW_LOG(LXW_VERBOSE, "\tCity: %s", (const char *)pInfo->pcCity_);
  LXW_LOG(LXW_VERBOSE, "\tState: %s", (const char *)pInfo->pcState_);
  LXW_LOG(LXW_VERBOSE, "\tCountry: %s", (const char *)pInfo->pcCountry_);
  LXW_LOG(LXW_VERBOSE, "\tWOEID: %s", (const char *)pInfo->pcWOEID_);
  LXW_LOG(LXW_VERBOSE, "\tUnits: %c", pInfo->cUnits_);
  LXW_LOG(LXW_VERBOSE, "\tInterval: %u", pInfo->uiInterval_);
  LXW_LOG(LXW_VERBOSE, "\tEnabled: %s", (pInfo->bEnabled_)?"yes":"no");
#endif
}


/**
 * Sets the alias for the location
 *
 * @param pEntry Pointer to the location to modify
 * @param pData  Alias value to use
 *
 */
void
setLocationAlias(LocationInfo * pLocation, const gchar * pczAlias)
{
  if (!pLocation)
    {
      LXW_LOG(LXW_ERROR, "Location: NULL");

      return;
    }

  gsize aliasLength = (pczAlias)?strlen(pczAlias):0;

  if (pLocation->pcAlias_)
    {
      g_free(pLocation->pcAlias_);
    }

  pLocation->pcAlias_ = g_strndup(pczAlias, aliasLength);
}

/**
 * Copies a location entry.
 *
 * @param pDestination Address of the pointer to the location to set.
 * @param pSource      Pointer to the location to use/copy.
 *
 * @note Destination is first freed, if non-NULL, otherwise a new allocation
 *       is made. Both source and destination locations must be released by
 *       the caller.
 */
void
copyLocation(LocationInfo ** pDestination, LocationInfo * pSource)
{
  if (!pSource || !pDestination)
    {
      return;
    }

  if (*pDestination)
    {
      /* Check if the two are the same, first */
      LocationInfo * pDstLocation = *pDestination;

      if (pSource->pcWOEID_ && !g_strcmp0(pDstLocation->pcWOEID_, pSource->pcWOEID_))
        {
          /* they're the same, no need to copy, just assign alias */
          setLocationAlias(*pDestination, pSource->pcAlias_);

          return;
        }

      freeLocation(*pDestination);

      *pDestination = NULL;
    }

  /* allocate new */
  *pDestination = g_try_new0(LocationInfo, 1);

  if (*pDestination)
    {
      LocationInfo * pDest = (LocationInfo *)*pDestination;
      LocationInfo * pSrc  = (LocationInfo *)pSource;

      pDest->pcAlias_ = g_strndup(pSrc->pcAlias_, 
                                          (pSrc->pcAlias_)?strlen(pSrc->pcAlias_):0);

      pDest->pcCity_ = g_strndup(pSrc->pcCity_, 
                                         (pSrc->pcCity_)?strlen(pSrc->pcCity_):0);

      pDest->pcState_ = g_strndup(pSrc->pcState_, 
                                          (pSrc->pcState_)?strlen(pSrc->pcState_):0);

      pDest->pcCountry_ = g_strndup(pSrc->pcCountry_, 
                                          (pSrc->pcCountry_)?strlen(pSrc->pcCountry_):0);

      pDest->pcWOEID_ = g_strndup(pSrc->pcWOEID_, 
                                          (pSrc->pcWOEID_)?strlen(pSrc->pcWOEID_):0);

      pDest->cUnits_ = (pSrc->cUnits_) ? pSrc->cUnits_ : 'f';

      pDest->dLongitude_ = pSrc->dLongitude_;

      pDest->dLatitude_ = pSrc->dLatitude_;

      pDest->uiInterval_ = pSrc->uiInterval_;

      pDest->bEnabled_ = pSrc->bEnabled_;
    }
  
}
