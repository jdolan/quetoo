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

#include <assert.h>
#include <string.h>

#include <Objectively/JSONSerialization.h>
#include <Objectively/Once.h>
#include <Objectively/URLCache.h>
#include <Objectively/URLRequest.h>
#include <Objectively/URLSession.h>

/**
 * @brief Returns Quetoo's shared HTTP session, enabling response caching on demand.
 */
static URLSession *session;
static Once sessionOnce;

static int32_t Net_HttpJsonFirstByte(const Data *data) {

  if (data && data->bytes) {
    for (size_t i = 0; i < data->length; i++) {
      switch (data->bytes[i]) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
          continue;
        default:
          return (int32_t) data->bytes[i];
      }
    }
  }

  return -1;
}

static URLSession *Net_HttpSession(void) {

  do_once(&sessionOnce, {
    session = $$(URLSession, sharedInstance);

    if (session->configuration->urlCache == NULL) {
      session->configuration->urlCache = $(alloc(URLCache), init);
      assert(session->configuration->urlCache);
    }
  });

  return session;
}

/**
 * @brief Clears the shared HTTP response cache, if one exists.
 */
void Net_HttpClearCache(void) {

  if (session && session->configuration->urlCache) {
    $(session->configuration->urlCache, removeAllCachedResponses);
  }
}

/**
 * @brief Synchronously performs an HTTP `GET` request and returns the response body.
 * @return The HTTP status code.
 */
int32_t Net_HttpGet(const char *url_string, void **body, size_t *length) {

  Com_Debug(DEBUG_NET, "%s\n", url_string);

  URL *url = $(alloc(URL), initWithCharacters, url_string);
  URLSession *session = Net_HttpSession();
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

/**
 * @brief Synchronously performs an HTTP `GET` request and deserializes a single JSON object.
 */
int32_t Net_HttpGetInstance(const char *url_string, const JsonProperty *properties, void *instance) {

  assert(url_string);
  assert(properties);
  assert(instance);

  void *body = NULL;
  size_t length = 0;

  const int32_t status = Net_HttpGet(url_string, &body, &length);
  if (status == 200) {
    Data *data = $$(Data, dataWithConstMemory, body, length);
    const int32_t first_byte = Net_HttpJsonFirstByte(data);
    if (first_byte != '{') {
      Com_Warn("%s: Failed to parse JSON object\n", url_string);
      release(data);
      Mem_Free(body);
      return 0;
    }

    if ($$(JSONSerialization, instanceFromData, properties, data, instance) != 1) {
      Com_Warn("%s: Failed to parse JSON object\n", url_string);
      release(data);
      Mem_Free(body);
      return 0;
    }

    release(data);
  } else if (status) {
    Com_Warn("%s: HTTP %d\n", url_string, status);
  } else {
    Com_Warn("%s: HTTP request failed\n", url_string);
  }

  Mem_Free(body);
  return status;
}

/**
 * @brief Synchronously performs an HTTP `GET` request and deserializes a JSON array.
 */
int32_t Net_HttpGetInstances(const char *url_string, const JsonProperty *properties,
                             void *instances, size_t stride, size_t count, size_t *instances_count) {

  assert(url_string);
  assert(properties);
  assert(instances);
  assert(instances_count);

  *instances_count = 0;
  if (count) {
    memset(instances, 0, stride * count);
  }

  void *body = NULL;
  size_t length = 0;

  const int32_t status = Net_HttpGet(url_string, &body, &length);
  if (status == 200) {
    Data *data = $$(Data, dataWithConstMemory, body, length);
    const int32_t first_byte = Net_HttpJsonFirstByte(data);
    if (first_byte != '[') {
      Com_Warn("%s: Failed to parse JSON array\n", url_string);
      release(data);
      Mem_Free(body);
      return 0;
    }

    *instances_count = $$(JSONSerialization, instancesFromData, properties, data, instances, stride, count);
    release(data);
  } else if (status) {
    Com_Warn("%s: HTTP %d\n", url_string, status);
  } else {
    Com_Warn("%s: HTTP request failed\n", url_string);
  }

  Mem_Free(body);
  return status;
}

typedef struct {
  Net_HttpCallback callback;
  void *user_data;
} Net_HttpGetAsync_State;

/**
 * @brief URLSessionTaskCompletion for `Net_HttpGetAsync`.
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
 * @brief Initiates an asynchronous HTTP `GET` request and invokes `callback` on completion.
 */
void Net_HttpGetAsync(const char *url_string, Net_HttpCallback callback, void *user_data) {

  Com_Debug(DEBUG_NET, "%s\n", url_string);

  URL *url = $(alloc(URL), initWithCharacters, url_string);
  URLSession *session = Net_HttpSession();
  URLSessionDataTask *task = $(session, dataTaskWithURL, url, Net_HttpGetAsync_Completion);
  release(url);

  Net_HttpGetAsync_State *state = Mem_Malloc(sizeof(Net_HttpGetAsync_State));
  state->callback = callback;
  state->user_data = user_data;
  task->urlSessionTask.data = state;

  $((URLSessionTask *) task, resume);
}

typedef struct {
  const JsonProperty *properties;
  size_t size;
  Net_HttpInstanceCallback callback;
  void *user_data;
} Net_HttpGetInstanceAsync_State;

/**
 * @brief URLSessionTaskCompletion for `Net_HttpGetInstanceAsync`.
 */
static void Net_HttpGetInstanceAsync_Completion(URLSessionTask *task, bool success) {

  int32_t status = task->response->httpStatusCode;

  const char *url_string = task->request->url->urlString->chars;

  Com_Debug(DEBUG_NET, "%s: HTTP %d\n", url_string, status);

  Net_HttpGetInstanceAsync_State *state = (Net_HttpGetInstanceAsync_State *) task->data;

  void *instance = NULL;

  if (status == 200) {
    const Data *data = ((URLSessionDataTask *) task)->data;
    if (Net_HttpJsonFirstByte(data) == '{') {
      instance = Mem_Malloc(state->size);
      if ($$(JSONSerialization, instanceFromData, state->properties, data, instance) != 1) {
        Com_Warn("%s: Failed to parse JSON object\n", url_string);
        Mem_Free(instance);
        instance = NULL;
        status = 0;
      }
    } else {
      Com_Warn("%s: Failed to parse JSON object\n", url_string);
      status = 0;
    }
  } else if (status) {
    Com_Warn("%s: HTTP %d\n", url_string, status);
  } else {
    Com_Warn("%s: HTTP request failed\n", url_string);
  }

  state->callback(status, instance, state->user_data);

  Mem_Free(instance);
  Mem_Free(state);
  release(task);
}

/**
 * @brief Initiates an asynchronous HTTP `GET` request, deserializing a single JSON object.
 */
void Net_HttpGetInstanceAsync(const char *url_string, const JsonProperty *properties,
                              size_t size, Net_HttpInstanceCallback callback, void *user_data) {

  assert(url_string);
  assert(properties);
  assert(callback);

  Com_Debug(DEBUG_NET, "%s\n", url_string);

  URL *url = $(alloc(URL), initWithCharacters, url_string);
  URLSession *session = Net_HttpSession();
  URLSessionDataTask *task = $(session, dataTaskWithURL, url, Net_HttpGetInstanceAsync_Completion);
  release(url);

  Net_HttpGetInstanceAsync_State *state = Mem_Malloc(sizeof(Net_HttpGetInstanceAsync_State));
  state->properties = properties;
  state->size = size;
  state->callback = callback;
  state->user_data = user_data;
  task->urlSessionTask.data = state;

  $((URLSessionTask *) task, resume);
}

typedef struct {
  const JsonProperty *properties;
  size_t stride;
  size_t count;
  Net_HttpInstancesCallback callback;
  void *user_data;
} Net_HttpGetInstancesAsync_State;

/**
 * @brief URLSessionTaskCompletion for `Net_HttpGetInstancesAsync`.
 */
static void Net_HttpGetInstancesAsync_Completion(URLSessionTask *task, bool success) {

  int32_t status = task->response->httpStatusCode;

  const char *url_string = task->request->url->urlString->chars;

  Com_Debug(DEBUG_NET, "%s: HTTP %d\n", url_string, status);

  Net_HttpGetInstancesAsync_State *state = (Net_HttpGetInstancesAsync_State *) task->data;

  void *instances = NULL;
  size_t instances_count = 0;

  if (status == 200) {
    const Data *data = ((URLSessionDataTask *) task)->data;
    if (Net_HttpJsonFirstByte(data) == '[') {
      instances = Mem_Malloc(state->stride * state->count);
      instances_count = $$(JSONSerialization, instancesFromData, state->properties, data,
                           instances, state->stride, state->count);
    } else {
      Com_Warn("%s: Failed to parse JSON array\n", url_string);
      status = 0;
    }
  } else if (status) {
    Com_Warn("%s: HTTP %d\n", url_string, status);
  } else {
    Com_Warn("%s: HTTP request failed\n", url_string);
  }

  state->callback(status, instances, instances_count, state->user_data);

  Mem_Free(instances);
  Mem_Free(state);
  release(task);
}

/**
 * @brief Initiates an asynchronous HTTP `GET` request, deserializing a JSON array.
 */
void Net_HttpGetInstancesAsync(const char *url_string, const JsonProperty *properties,
                               size_t stride, size_t count,
                               Net_HttpInstancesCallback callback, void *user_data) {

  assert(url_string);
  assert(properties);
  assert(callback);

  Com_Debug(DEBUG_NET, "%s\n", url_string);

  URL *url = $(alloc(URL), initWithCharacters, url_string);
  URLSession *session = Net_HttpSession();
  URLSessionDataTask *task = $(session, dataTaskWithURL, url, Net_HttpGetInstancesAsync_Completion);
  release(url);

  Net_HttpGetInstancesAsync_State *state = Mem_Malloc(sizeof(Net_HttpGetInstancesAsync_State));
  state->properties = properties;
  state->stride = stride;
  state->count = count;
  state->callback = callback;
  state->user_data = user_data;
  task->urlSessionTask.data = state;

  $((URLSessionTask *) task, resume);
}

typedef struct {
  Net_HttpCallback callback;
  void *user_data;
} Net_HttpPostAsync_State;

/**
 * @brief URLSessionTaskCompletion for `Net_HttpPostAsync`.
 */
static void Net_HttpPostAsync_Completion(URLSessionTask *task, bool success) {

  const int32_t status = task->response->httpStatusCode;

  Com_Debug(DEBUG_NET, "%s: HTTP %d\n", task->request->url->urlString->chars, status);

  Net_HttpPostAsync_State *state = (Net_HttpPostAsync_State *) task->data;

  if (state->callback) {
    const Data *data = ((URLSessionDataTask *) task)->data;
    if (data) {
      state->callback(status, (void *) data->bytes, data->length, state->user_data);
    } else {
      state->callback(status, NULL, 0, state->user_data);
    }
  }

  Mem_Free(state);
  release(task);
}

/**
 * @brief Initiates an asynchronous HTTP `POST` request and invokes `callback` on completion.
 */
void Net_HttpPostAsync(const char *url_string, const void *body, size_t length,
                       const char *content_type, Net_HttpCallback callback, void *user_data) {

  Com_Debug(DEBUG_NET, "POST %s (%zu bytes)\n", url_string, length);

  URL *url = $(alloc(URL), initWithCharacters, url_string);
  URLRequest *request = $(alloc(URLRequest), initWithURL, url);
  release(url);

  request->httpMethod = HTTP_POST;
  request->httpBody = $$(Data, dataWithBytes, (const uint8_t *) body, length);

  $(request, setValueForHTTPHeaderField, content_type, "Content-Type");

  URLSession *session = Net_HttpSession();
  URLSessionDataTask *task = $(session, dataTaskWithRequest, request, Net_HttpPostAsync_Completion);
  release(request);

  Net_HttpPostAsync_State *state = Mem_Malloc(sizeof(Net_HttpPostAsync_State));
  state->callback = callback;
  state->user_data = user_data;
  task->urlSessionTask.data = state;

  $((URLSessionTask *) task, resume);
}

/**
 * @brief Construct an HTTP URL from a `net_addr_t` and path.
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
