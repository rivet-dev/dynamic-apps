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

#ifndef _OPENWEATHERMAP_H_
#define _OPENWEATHERMAP_H_ 1

#include "providers.h"

/* retrieved from openweathermap.org */
#define WEATHER_APPID "42005bf4482b716fb9646286ca99a2a7"

extern provider_callback_info OpenWeatherMapCallbacks;

GList *getOSMLocationInfo(ProviderInfo * instance, const gchar * pczLocation);

#endif /* _OPENWEATHERMAP_H_ */
