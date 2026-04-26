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
 * @brief Synchronously performs an HTTP GET request and returns the response body.
 * @return The HTTP status code.
 */
int32_t Net_HttpGet(const char *url_string, void **body, size_t *length) {

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
    *body = Mem_Malloc(task->data->length);
    memcpy(*body, task->data->bytes, task->data->length);
    *length = task->data->length;
  } else {
    *body = NULL;
    *length = 0;
  }

  release(task);
  return status;
}

typedef struct {
  Net_HttpCallback callback;
  void *user_data;
} Net_HttpGetAsync_State;

/**
 * @brief URLSessionTaskCompletion for Net_HttpGetAsync.
 */
static void Net_HttpGetAsync_Completion(URLSessionTask *task, bool success) {

  const int32_t status = task->response->httpStatusCode;

  Com_Debug(DEBUG_NET, "%s: HTTP %d\n", task->request->url->urlString->chars, status);

  Net_HttpGetAsync_State *state = (Net_HttpGetAsync_State *) task->data;

  const Data *data = ((URLSessionDataTask *) task)->data;
  if (data) {
    state->callback(status, (void *) data->bytes, data->length, state->user_data);
  } else {
    state->callback(status, NULL, 0, state->user_data);
  }

  Mem_Free(state);
  release(task);
}

/**
 * @brief Initiates an asynchronous HTTP GET request and invokes `callback` on completion.
 */
void Net_HttpGetAsync(const char *url_string, Net_HttpCallback callback, void *user_data) {

  Com_Debug(DEBUG_NET, "%s\n", url_string);

  URL *url = $(alloc(URL), initWithCharacters, url_string);
  URLSession *session = $$(URLSession, sharedInstance);
  URLSessionDataTask *task = $(session, dataTaskWithURL, url, Net_HttpGetAsync_Completion);
  release(url);

  Net_HttpGetAsync_State *state = Mem_Malloc(sizeof(Net_HttpGetAsync_State));
  state->callback = callback;
  state->user_data = user_data;
  task->urlSessionTask.data = state;

  $((URLSessionTask *) task, resume);
}

/**
 * @brief Construct an HTTP URL from a net_addr_t and path.
 */
int32_t Net_HttpUrl(const net_addr_t *addr, const char *path, char *buf, size_t buf_size) {

  return g_snprintf(buf, buf_size, "http://%s:%d/%s",
                    Net_NetaddrToIpString(addr),
                    ntohs(addr->port),
                    path);
}

/**
 * @brief Parse the request line of an HTTP request.
 */
bool Net_HttpParseRequestLine(const char *request, char *method, size_t method_size,
                              char *path, size_t path_size) {

  const char *space = strchr(request, ' ');
  if (!space) {
    return false;
  }

  const size_t method_len = space - request;
  if (method_len >= method_size) {
    return false;
  }

  memcpy(method, request, method_len);
  method[method_len] = '\0';

  // skip the space and leading slash
  const char *path_start = space + 1;
  if (*path_start == '/') {
    path_start++;
  }

  const char *path_end = strchr(path_start, ' ');
  if (!path_end) {
    return false;
  }

  const size_t path_len = path_end - path_start;
  if (path_len >= path_size) {
    return false;
  }

  memcpy(path, path_start, path_len);
  path[path_len] = '\0';

  return true;
}

/**
 * @brief Format an HTTP/1.0 response header into a buffer.
 */
int32_t Net_HttpFormatResponse(int32_t status, const char *reason,
                               const char *content_type, int64_t content_length,
                               char *buf, size_t buf_size) {

  if (content_type) {
    return g_snprintf(buf, buf_size,
      "HTTP/1.0 %d %s\r\n"
      "Connection: close\r\n"
      "Content-Length: %" PRId64 "\r\n"
      "Content-Type: %s\r\n"
      "\r\n",
      status, reason, content_length, content_type);
  } else {
    return g_snprintf(buf, buf_size,
      "HTTP/1.0 %d %s\r\n"
      "Connection: close\r\n"
      "Content-Length: %" PRId64 "\r\n"
      "\r\n",
      status, reason, content_length);
  }
}

/**
 * @brief Send an HTTP error response on a socket.
 */
void Net_HttpSendError(int32_t sock, int32_t status, const char *reason) {

  char header[256];
  const int32_t len = Net_HttpFormatResponse(status, reason, NULL, 0, header, sizeof(header));

  Net_Send(sock, header, len);
}
