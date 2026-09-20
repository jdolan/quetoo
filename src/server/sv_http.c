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

#include "sv_local.h"
#include "net/net_http_server.h"

static int32_t svHttpSocket = -1;

/**
 * @brief Allowed download patterns, matching the former UDP download allowlist.
 */
static const char *svHttpAllowedPatterns[] = {
	"*.pk3",
	"docs/*",
	"maps/*",
	"mapshots/*",
	"models/*",
	"sounds/*",
	"sky/*",
	"textures/*",
	NULL
};

/**
 * @return True if the filename matches the download allowlist.
 */
static bool Sv_HttpIsAllowed(const char *filename) {

	const char **pattern = svHttpAllowedPatterns;
	while (*pattern) {
		if (GlobMatch(*pattern, filename, GLOB_FLAGS_NONE)) {
			return true;
		}
		pattern++;
	}

	return false;
}

/**
 * @brief Send an HTTP error response and close the connection.
 */
static void Sv_HttpSendError(ServerHttpClient *http, int32_t code, const char *reason) {

	Net_HttpSendError(http->socket, code, reason);

	Net_CloseSocket(http->socket);
	memset(http, 0, sizeof(*http));
}

/**
 * @brief Parse the completed HTTP request and begin the response.
 */
static void Sv_HttpHandleRequest(ServerHttpClient *http) {

	// null-terminate the request
	http->request[http->requestLen] = '\0';

	// parse the request line
	char method[16], filename[MAX_OS_PATH];
	if (!Net_HttpParseRequestLine(http->request, method, sizeof(method), filename, sizeof(filename))) {
		Sv_HttpSendError(http, 400, "Bad Request");
		return;
	}

	if (q_strcmp(method, "GET") != 0) {
		Sv_HttpSendError(http, 405, "Method Not Allowed");
		return;
	}

	// validate the filename
	if (!Com_IsValidDownload(filename)) {
		Com_Warn("HTTP: Invalid download request: %s\n", filename);
		Sv_HttpSendError(http, 403, "Forbidden");
		return;
	}

	if (!Sv_HttpIsAllowed(filename)) {
		Com_Warn("HTTP: Disallowed download request: %s\n", filename);
		Sv_HttpSendError(http, 403, "Forbidden");
		return;
	}

	// load the file
	void *fileData = NULL;
	const int64_t fileSize = Fs_Load(filename, &fileData);
	if (fileSize == -1 || !fileData) {
		Com_Debug(DEBUG_SERVER, "HTTP: File not found: %s\n", filename);
		Sv_HttpSendError(http, 404, "Not Found");
		return;
	}

	// build the response header
	char header[256];
	const int32_t headerLen = Net_HttpFormatResponse(200, "OK",
		"application/octet-stream", fileSize, header, sizeof(header));

	// allocate a single buffer for header + file data
	http->size = headerLen + (int32_t) fileSize;
	http->data = Mem_Malloc(http->size);
	memcpy(http->data, header, headerLen);
	memcpy(http->data + headerLen, fileData, fileSize);
	http->count = 0;

	Fs_Free(fileData);

	Com_Debug(DEBUG_SERVER, "HTTP: Serving %s (%" PRId64 " bytes)\n", filename, fileSize);
}

/**
 * @brief Accept new HTTP connections and associate them with connected clients.
 */
static void Sv_HttpAccept(void) {

	NetAddr from;
	const int32_t sock = Net_Accept(svHttpSocket, &from);
	if (sock == -1) {
		return;
	}

	// match the source IP to a connected client
	ServerClient *cl = svs.clients;
	for (int32_t i = 0; i < sv_maxClients->integer; i++, cl++) {

		if (cl->state == SV_CLIENT_FREE) {
			continue;
		}

		if (cl->netChan.remoteAddress.addr != from.addr) {
			continue;
		}

		if (cl->http.socket > 0) {
			continue; // already has an active HTTP connection
		}

		memset(&cl->http, 0, sizeof(cl->http));
		cl->http.socket = sock;

		Com_Debug(DEBUG_SERVER, "HTTP: Accepted connection from %s\n", cl->name);
		return;
	}

	// no matching client found, reject
	Com_Debug(DEBUG_SERVER, "HTTP: Rejected connection from %s\n", Net_NetaddrToIpString(&from));

	Net_HttpSendError(sock, 403, "Forbidden");
	Net_CloseSocket(sock);
}

/**
 * @brief Process a single client's HTTP connection.
 */
static void Sv_HttpClientThink(ServerHttpClient *http) {

	// still reading the request
	if (!http->data) {
		const ssize_t received = Net_Recv(http->socket,
			http->request + http->requestLen,
			sizeof(http->request) - 1 - http->requestLen);

		if (received > 0) {
			http->requestLen += (int32_t) received;

			// check for end of HTTP request
			if (q_strstr(http->request, "\r\n\r\n")) {
				Sv_HttpHandleRequest(http);
			} else if (http->requestLen >= (int32_t) sizeof(http->request) - 1) {
				Sv_HttpSendError(http, 400, "Bad Request");
			}
		} else if (received == 0) {
			// client closed connection
			Net_CloseSocket(http->socket);
			memset(http, 0, sizeof(*http));
		} else {
			if (Net_GetError() != EWOULDBLOCK) {
				Net_CloseSocket(http->socket);
				memset(http, 0, sizeof(*http));
			}
		}
		return;
	}

	// sending the response
	const int32_t remaining = http->size - http->count;
	if (remaining <= 0) {
		Net_CloseSocket(http->socket);
		Mem_Free(http->data);
		memset(http, 0, sizeof(*http));
		return;
	}

	const ssize_t sent = Net_Send(http->socket, http->data + http->count, remaining);
	if (sent > 0) {
		http->count += (int32_t) sent;
	} else if (sent == -1) {
		if (Net_GetError() != EWOULDBLOCK) {
			Com_Debug(DEBUG_SERVER, "HTTP: Send error, closing connection\n");
			Net_CloseSocket(http->socket);
			Mem_Free(http->data);
			memset(http, 0, sizeof(*http));
		}
	}
}

/**
 * @brief Process all active HTTP connections. Called once per server frame.
 */
void Sv_HttpThink(void) {

	if (svHttpSocket == -1 || svs.clients == NULL) {
		return;
	}

	Sv_HttpAccept();

	ServerClient *cl = svs.clients;
	for (int32_t i = 0; i < sv_maxClients->integer; i++, cl++) {

		if (cl->http.socket <= 0) {
			continue;
		}

		Sv_HttpClientThink(&cl->http);
	}
}

/**
 * @brief Close a client's active HTTP connection. Called when the client disconnects.
 */
void Sv_HttpClientDisconnect(ServerHttpClient *http) {

	if (http->socket > 0) {
		Net_CloseSocket(http->socket);
	}

	if (http->data) {
		Mem_Free(http->data);
	}

	memset(http, 0, sizeof(*http));
}

/**
 * @brief Initialize the HTTP server.
 */
void Sv_InitHttp(void) {

	const Cvar *netPort = Cvar_Add("net_port", va("%i", PORT_SERVER), CVAR_NO_SET, NULL);

	const in_port_t port = netPort->integer;

	svHttpSocket = Net_SocketListen(NULL, port, 8);
	if (svHttpSocket == -1) {
		Com_Warn("HTTP: Failed to create listen socket on port %d\n", port);
		return;
	}

	Com_Print("HTTP server listening on port %d\n", port);
}

/**
 * @brief Shutdown the HTTP server.
 */
void Sv_ShutdownHttp(void) {

	if (svHttpSocket == -1) {
		return;
	}

	// close all active client HTTP connections
	ServerClient *cl = svs.clients;
	for (int32_t i = 0; i < sv_maxClients->integer; i++, cl++) {
		Sv_HttpClientDisconnect(&cl->http);
	}

	Net_CloseSocket(svHttpSocket);
	svHttpSocket = -1;

	Com_Print("HTTP server stopped\n");
}
