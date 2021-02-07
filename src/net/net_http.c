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

#include "net_http.h"

#include <Objectively/URLSession.h>

/**
 * @brief
 */
int32_t Net_HttpGet(const char *url_string, void **data, size_t *length) {

	Com_Debug(DEBUG_NET, "%s\n", url_string);

	URL *url = $(alloc(URL), initWithCharacters, url_string);
	URLSession *session = $$(URLSession, sharedInstance);
	URLSessionDataTask *task = $(session, dataTaskWithURL, url, NULL);
	release(url);

	$((URLSessionTask *) task, execute);
	const int32_t status = task->urlSessionTask.response->httpStatusCode;

	Com_Debug(DEBUG_NET, "%s: HTTP %d: %d bytes\n",
			  url_string,
			  status,
			  task->data ? (int32_t) task->data->length : 0);

	if (task->data) {
		*data = Mem_Malloc(task->data->length);
		memcpy(*data, task->data->bytes, task->data->length);
		*length = task->data->length;
	} else {
		*data = NULL;
		*length = 0;
	}

	release(task);
	return status;
}

/**
 * @brief URLSessionTaskCompletionn for Net_HttpGetAsync.
 */
static void Net_HttpGetAsync_Completion(URLSessionTask *task, _Bool success) {

	const int32_t status = task->response->httpStatusCode;

	Com_Debug(DEBUG_NET, "%s: HTTP %d\n", task->request->url->urlString->chars, status);

	const Net_HttpCallback callback = task->data;

	const Data *data = ((URLSessionDataTask *) task)->data;
	if (data) {
		callback(task->response->httpStatusCode, data->bytes, data->length);
	} else {
		callback(task->response->httpStatusCode, NULL, 0);
	}

	release(task);
}

/**
 * @brief
 */
void Net_HttpGetAsync(const char *url_string, Net_HttpCallback callback) {

	Com_Debug(DEBUG_NET, "%s\n", url_string);

	URL *url = $(alloc(URL), initWithCharacters, url_string);
	URLSession *session = $$(URLSession, sharedInstance);
	URLSessionDataTask *task = $(session, dataTaskWithURL, url, Net_HttpGetAsync_Completion);
	release(url);

	task->urlSessionTask.data = callback;

	$((URLSessionTask *) task, resume);
}
