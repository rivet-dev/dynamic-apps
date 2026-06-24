/*
 * Copyright (C) 2019 Andriy Grutsenko <andrej@rep.kiev.ua>
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

#ifndef _PROVIDERS_H_
#define _PROVIDERS_H_ 1

#include "forecast.h"
#include "location.h"

#include <glib.h>

typedef struct ProviderInfo ProviderInfo;

typedef struct {
    const char *name;
    const char *description;
    ProviderInfo * (*initProvider)(void);
    void (*freeProvider)(ProviderInfo *instance);
    GList * (*getLocationInfo)(ProviderInfo *instance, const gchar *pattern);
    ForecastInfo * (*getForecastInfo)(ProviderInfo *instance,
                                      LocationInfo *location,
                                      ForecastInfo *last);
    gboolean supports_woeid;
} provider_callback_info;

#endif /* _PROVIDERS_H_ */
