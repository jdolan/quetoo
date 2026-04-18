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

#pragma once

#include "net_sock.h"

/**
 * @brief The asynchronous HTTP callback type.
 * @param status The HTTP response code.
 * @param body The response body.
 * @param length The response body length in bytes.
 * @param user_data The user data pointer passed to Net_HttpGetAsync.
 */
typedef void (*Net_HttpCallback)(int32_t status, void *body, size_t length, void *user_data);

/**
 * @brief Synchronously GET the specified URL string, writing data and len.
 * @param url_string The URL string to GET.
 * @param data The returned data, which should be Mem_Free'd when no longer needed.
 * @param length The returned data length in bytes.
 * @return The HTTP response code.
 */
int32_t Net_HttpGet(const char *url_string, void **data, size_t *length);

/**
 * @brief Asynchronously GET the specified URL string.
 * @param url_string The URL string to GET.
 * @param callback The completion callback.
 * @param user_data User data pointer passed through to the callback.
 */
void Net_HttpGetAsync(const char *url_string, Net_HttpCallback callback, void *user_data);

/**
 * @brief Construct an HTTP URL from a net_addr_t and path.
 * @param addr The server address.
 * @param path The URL path (e.g. "maps/foo.bsp").
 * @param buf The output buffer.
 * @param buf_size The size of the output buffer.
 * @return The number of characters written, or -1 on error.
 */
int32_t Net_HttpUrl(const net_addr_t *addr, const char *path, char *buf, size_t buf_size);

/**
 * @brief Parse the request line of an HTTP request.
 * @param request The raw HTTP request buffer (must be null-terminated).
 * @param method The parsed method (e.g. "GET").
 * @param method_size The size of the method buffer.
 * @param path The parsed path (e.g. "maps/foo.bsp"), without leading slash.
 * @param path_size The size of the path buffer.
 * @return True if the request line was successfully parsed.
 */
bool Net_HttpParseRequestLine(const char *request, char *method, size_t method_size,
                              char *path, size_t path_size);

/**
 * @brief Format an HTTP/1.0 response header into a buffer.
 * @param status The HTTP status code.
 * @param reason The HTTP reason phrase.
 * @param content_type The Content-Type header value, or NULL for none.
 * @param content_length The Content-Length value.
 * @param buf The output buffer.
 * @param buf_size The size of the output buffer.
 * @return The number of characters written.
 */
int32_t Net_HttpFormatResponse(int32_t status, const char *reason,
                               const char *content_type, int64_t content_length,
                               char *buf, size_t buf_size);

/**
 * @brief Send an HTTP error response on a socket.
 * @param sock The socket to send on.
 * @param status The HTTP status code.
 * @param reason The HTTP reason phrase.
 */
void Net_HttpSendError(int32_t sock, int32_t status, const char *reason);
