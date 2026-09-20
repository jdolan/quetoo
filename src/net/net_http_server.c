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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "net_http_server.h"

#include <string.h>
/**
 * @brief Construct an HTTP URL from a `NetAddr` and path.
 */
int32_t Net_HttpUrl(const NetAddr *addr, const char *path, char *buf, size_t bufSize) {

  return q_snprintf(buf, bufSize, "http://%s:%d/%s",
                    Net_NetaddrToIpString(addr),
                    ntohs(addr->port),
                    path);
}

/**
 * @brief Parse the request line of an HTTP request.
 */
bool Net_HttpParseRequestLine(const char *request, char *method, size_t methodSize,
                              char *path, size_t pathSize) {

  const char *space = q_strchr(request, ' ');
  if (!space) {
    return false;
  }

  const size_t methodLen = space - request;
  if (methodLen >= methodSize) {
    return false;
  }

  memcpy(method, request, methodLen);
  method[methodLen] = '\0';

  // skip the space and leading slash
  const char *pathStart = space + 1;
  if (*pathStart == '/') {
    pathStart++;
  }

  const char *pathEnd = q_strchr(pathStart, ' ');
  if (!pathEnd) {
    return false;
  }

  const size_t pathLen = pathEnd - pathStart;
  if (pathLen >= pathSize) {
    return false;
  }

  memcpy(path, pathStart, pathLen);
  path[pathLen] = '\0';

  return true;
}

/**
 * @brief Format an HTTP/1.0 response header into a buffer.
 */
int32_t Net_HttpFormatResponse(int32_t status, const char *reason,
                               const char *contentType, int64_t contentLength,
                               char *buf, size_t bufSize) {

  if (contentType) {
    return q_snprintf(buf, bufSize,
      "HTTP/1.0 %d %s\r\n"
      "Connection: close\r\n"
      "Content-Length: %" PRId64 "\r\n"
      "Content-Type: %s\r\n"
      "\r\n",
      status, reason, contentLength, contentType);
  } else {
    return q_snprintf(buf, bufSize,
      "HTTP/1.0 %d %s\r\n"
      "Connection: close\r\n"
      "Content-Length: %" PRId64 "\r\n"
      "\r\n",
      status, reason, contentLength);
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
