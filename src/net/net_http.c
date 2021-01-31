/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "common.h"
#include "net_http.h"

/**
 * @brief
 */
URLSessionTask *Net_HttpDataTask(const char *uri, URLSessionTaskCompletion completion) {

	URL *url = $(alloc(URL), initWithCharacters, uri);
	if (url == NULL) {
		Com_Warn("Invalid URL: %s\n", uri);
		return NULL;
	}

	URLSession *session = $$(URLSession, sharedInstance);

	URLSessionTask *task = (URLSessionTask *) $(session, dataTaskWithURL, url, completion);

	release(url);

	return task;
}

/**
 * @brief
 */
int32_t Net_HttpGet(const char *uri, void **data, size_t *len) {

	URLSessionTask *task = Net_HttpDataTask(uri, NULL);

	$(task, execute);

	const int32_t res = task->response->httpStatusCode;
	if (res >= 200 && res < 400) {

		const Data *received = ((URLSessionDataTask *) task)->data;

		if (data) {
			*data = Mem_Malloc(received->length);
			memcpy(*data, received->bytes, received->length);
		}

		if (len) {
			*len = received->length;
		}
	} else {
		if (data) {
			*data = NULL;
		}
		if (len) {
			*len = 0;
		}
	}

	release(task);

	return res;
}
