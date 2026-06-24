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

/* Provides http protocol utility functions */

#ifndef LXWEATHER_HTTPUTIL_HEADER
#define LXWEATHER_HTTPUTIL_HEADER

#include <glib.h>
#include <curl/curl.h>

/**
 * Returns the contents of the requested URL
 *
 * @param pczURL The URL to retrieve [in].
 * @param pcData A pointer to a null-terminated buffer containing the textual
 *         representation of the response. Must be freed by the caller. [out].
 * @param piDataSize The resulting data length [out].
 * @param headers Extra headers for GET request [in].
 *
 * @return The return code supplied by CURL
 */
CURLcode
getURL(const gchar * pczURL, gchar ** pcData, gint * piDataSize, const gchar ** headers);

#endif
