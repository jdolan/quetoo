/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "net_tcp.h"

/*
 * @brief Establishes a TCP connection to the specified host.
 *
 * @param host The host to connect to. See Net_StringToNetaddr.
 */
int32_t Net_Connect(const char *host) {
	struct sockaddr_in addr;

	int32_t sock = Net_Socket(NA_STREAM, NULL, 0);

	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = 0;

	struct sockaddr_in to;
	Net_StringToSockaddr(host, &to);

	if (connect(sock, (const struct sockaddr *) &to, sizeof(to)) == -1) {
		Com_Error(ERR_DROP, "connect: %s", Net_GetErrorString());
	}

	return sock;
}

/*
 * @brief Send data to the specified TCP stream.
 */
_Bool Net_SendStream(int32_t sock, const void *buf, size_t len) {

	const ssize_t sent = send(sock, buf, len, 0);

	if (sent == -1) {
		Com_Warn("%s", Net_GetErrorString());
		return false;
	}

	return true;
}

/*
 * @brief Receive data from the specified TCP stream.
 */
_Bool Net_ReceiveStream(int32_t sock, size_buf_t *buf) {

	buf->size = buf->read = 0;

	const ssize_t received = recv(sock, (void *) buf->data, buf->max_size, 0);
	if (received == -1) {

		if (Net_GetError() == EWOULDBLOCK)
			return false; // no data, don't crap our pants

		Com_Warn("%s", Net_GetErrorString());
		return false;
	}

	buf->size = received;
	return true;
}
