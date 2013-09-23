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

#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>

#include "net.h"

#define MAX_NET_UDP_LOOPS 4

typedef struct {
	byte data[MAX_MSG_SIZE];
	size_t size;
} net_udp_loop_message_t;

typedef struct {
	net_udp_loop_message_t messages[MAX_NET_UDP_LOOPS];
	int32_t send, recv;
} net_udp_loopback_t;

static net_udp_loopback_t net_udp_loopbacks[2];
static int32_t net_udp_sockets[2];

/*
 * @brief
 */
static _Bool Net_ReceiveDatagram_Loop(net_src_t source, net_addr_t *from, size_buf_t *message) {
	net_udp_loopback_t *loop = &net_udp_loopbacks[source];

	if (loop->send - loop->recv > MAX_NET_UDP_LOOPS)
		loop->recv = loop->send - MAX_NET_UDP_LOOPS;

	if (loop->recv >= loop->send)
		return false;

	const uint32_t i = loop->recv & (MAX_NET_UDP_LOOPS - 1);
	loop->recv++;

	memcpy(message->data, loop->messages[i].data, loop->messages[i].size);
	message->size = loop->messages[i].size;

	from->type = NA_LOOP;

	from->addr = inet_addr("127.0.0.1");
	from->port = 0;

	return true;
}

/*
 * @brief Receive a datagram from the specified address.
 */
_Bool Net_ReceiveDatagram(net_src_t source, net_addr_t *from, size_buf_t *message) {

	memset(from, 0, sizeof(*from));

	if (Net_ReceiveDatagram_Loop(source, from, message))
		return true;

	const int32_t sock = net_udp_sockets[source];

	if (!sock)
		return false;

	struct sockaddr_in from_saddr;
	socklen_t from_len = sizeof(from_saddr);

	ssize_t len = recvfrom(sock, (void *) message->data, message->max_size, 0,
			(struct sockaddr *) &from_saddr, &from_len);

	from->type = NA_DATAGRAM;

	from->addr = from_saddr.sin_addr.s_addr;
	from->port = from_saddr.sin_port;

	if (len == -1) {
		int32_t err = Net_GetError();

		if (err == EWOULDBLOCK || err == ECONNREFUSED)
			return false; // not terribly abnormal

		const char *s = source == NS_UDP_SERVER ? "server" : "client";
		Com_Warn("%s: %s from %s\n", s, Net_GetErrorString(), Net_NetaddrToString(from));
		return false;
	}

	if (len == ((ssize_t) message->max_size)) {
		Com_Warn("Oversized packet from %s\n", Net_NetaddrToString(from));
		return false;
	}

	message->read = 0;
	message->size = (size_t) len;

	return true;
}

/*
 * @brief
 */
static _Bool Net_SendDatagram_Loop(net_src_t source, const void *data, size_t len) {
	net_udp_loopback_t *loop = &net_udp_loopbacks[source ^ 1];

	const uint32_t i = loop->send & (MAX_NET_UDP_LOOPS - 1);
	loop->send++;

	memcpy(loop->messages[i].data, data, len);
	loop->messages[i].size = len;

	return true;
}

/*
 * @brief Send a datagram to the specified address.
 */
_Bool Net_SendDatagram(net_src_t source, const net_addr_t *to, const void *data, size_t len) {

	if (to->type == NA_LOOP) {
		return Net_SendDatagram_Loop(source, data, len);
	}

	int32_t sock;
	if (to->type == NA_BROADCAST || to->type == NA_DATAGRAM) {
		if (!(sock = net_udp_sockets[source]))
			return false;
	} else {
		Com_Error(ERR_DROP, "Bad address type\n");
	}

	struct sockaddr_in to_addr;
	Net_NetAddrToSockaddr(to, &to_addr);

	ssize_t ret = sendto(sock, data, len, 0, (const struct sockaddr *) &to_addr, sizeof(to_addr));

	if (ret == -1) {
		Com_Warn("sendto: %s to %s\n", Net_GetErrorString(), Net_NetaddrToString(to));
		return false;
	}

	return true;
}

/*
 * @brief Sleeps for msec or until server socket is ready.
 */
void Net_Sleep(uint32_t msec) {
	struct timeval timeout;
	fd_set fdset;

	if (!net_udp_sockets[NS_UDP_SERVER] || !dedicated->value)
		return; // we're not a server, simply return

	FD_ZERO(&fdset);
	FD_SET((uint32_t) net_udp_sockets[NS_UDP_SERVER], &fdset); // network socket

	timeout.tv_sec = msec / 1000;
	timeout.tv_usec = (msec % 1000) * 1000;

	select(FD_SETSIZE, &fdset, NULL, NULL, &timeout);
}

/*
 * @brief Opens or closes the managed UDP socket for the given net_src_t.
 */
void Net_Config(net_src_t source, _Bool up) {
	int32_t *sock = &net_udp_sockets[source];

	if (up) {
		if (*sock == 0) {
			const char *interface = Cvar_GetString("net_interface");
			const in_port_t port = source == NS_UDP_SERVER ? Cvar_GetValue("net_port") : 0;

			*sock = Net_Socket(NA_DATAGRAM, interface, port);
		}
	} else {
		if (*sock != 0) {
			Net_CloseSocket(*sock);
			*sock = 0;
		}
	}
}

