/**
 * Copyright (c) 2012-2014 Piotr Sipika; see the AUTHORS file for more.
 * Copyright (c) 2019 Andriy Grytsenko <andrej@rep.kiev.ua>
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

#include "httputil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct wdata_t {
    char *buff;
    size_t alloc;
};

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    struct wdata_t *data = userp;
    size_t todo = size * nmemb;
    size_t new_alloc = data->alloc + todo;

    if (todo == 0)
        return 0;
    data->buff = realloc(data->buff, new_alloc + 1);
    if (data->buff == NULL)
        return 0; /* is that correct? */
    memcpy(&data->buff[data->alloc], buffer, todo);
    data->alloc = new_alloc;
    return todo;
}

/**
 * Returns the contents of the requested URL
 *
 * @param pczURL The URL to retrieve.
 * @param piRetCode The return code supplied with the response.
 * @param piDataSize The resulting data length [out].
 *
 * @return A pointer to a null-terminated buffer containing the textual 
 *         representation of the response. Must be freed by the caller.
 */
CURLcode
getURL(const gchar * pczURL, gchar ** pcData, gint * piDataSize, const gchar ** pccHeaders)
{
    struct curl_slist *headers=NULL;
    CURL *curl;
    CURLcode res;
    struct wdata_t data = { NULL, 0 };

    if (!pczURL)
        return CURLE_URL_MALFORMAT;

    if (pccHeaders)
    {
        while (*pccHeaders)
            headers = curl_slist_append(headers, *pccHeaders++);
    }
    curl_global_init(CURL_GLOBAL_SSL);
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, pczURL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    res = curl_easy_perform(curl);
    if (data.buff)
        data.buff[data.alloc] = '\0';

    if (pcData)
        *pcData = data.buff;
    else
        g_free(data.buff);
    if (piDataSize)
        *piDataSize = data.alloc;

    //if (res != CURLE_OK)
      //fprintf(stderr, "curl_easy_perform() failed: %s\n",
              //curl_easy_strerror(res));

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return res;
}
