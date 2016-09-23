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

#include <sys/time.h>
#include <sys/select.h>

#include "cvar.h"
#include "net_udp.h"

#define MAX_NET_UDP_LOOPS 4

typedef struct {
	byte data[MAX_MSG_SIZE];
	size_t size;
} net_udp_loop_message_t;

typedef struct {
	net_udp_loop_message_t messages[MAX_NET_UDP_LOOPS];
	int32_t send, recv;
} net_udp_loop_t;

typedef struct {
	net_udp_loop_t loops[2];
	int32_t sockets[2];
} net_udp_state_t;

static net_udp_state_t net_udp_state;

/**
 * @brief
 */
static _Bool Net_ReceiveDatagram_Loop(net_src_t source, net_addr_t *from, mem_buf_t *buf) {
	net_udp_loop_t *loop = &net_udp_state.loops[source];

	if (loop->send - loop->recv > MAX_NET_UDP_LOOPS)
		loop->recv = loop->send - MAX_NET_UDP_LOOPS;

	if (loop->recv >= loop->send)
		return false;

	const uint32_t i = loop->recv & (MAX_NET_UDP_LOOPS - 1);
	loop->recv++;

	memcpy(buf->data, loop->messages[i].data, loop->messages[i].size);
	buf->size = loop->messages[i].size;

	from->type = NA_LOOP;
	from->addr = net_lo;
	from->port = 0;

	return true;
}

/**
 * @brief Receive a datagram on the specified socket, populating the from
 * address with the sender.
 */
_Bool Net_ReceiveDatagram(net_src_t source, net_addr_t *from, mem_buf_t *buf) {

	buf->read = buf->size = 0;

	memset(from, 0, sizeof(*from));
	from->type = NA_DATAGRAM;

	if (Net_ReceiveDatagram_Loop(source, from, buf))
		return true;

	const int32_t sock = net_udp_state.sockets[source];

	if (!sock)
		return false;

	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);

	const ssize_t received = recvfrom(sock, (void *) buf->data, buf->max_size, 0,
			(struct sockaddr *) &addr, &addr_len);

	from->addr = addr.sin_addr.s_addr;
	from->port = addr.sin_port;

	if (received == -1) {
		const int32_t err = Net_GetError();

		if (err == EWOULDBLOCK || err == ECONNREFUSED)
			return false; // not terribly abnormal

		Com_Warn("%s\n", Net_GetErrorString());
		return false;
	}

	from->addr = addr.sin_addr.s_addr;
	from->port = addr.sin_port;

	if (received == ((ssize_t) buf->max_size)) {
		Com_Warn("Oversized packet from %s\n", Net_NetaddrToString(from));
		return false;
	}

	buf->size = received;

	return true;
}

/**
 * @brief
 */
static _Bool Net_SendDatagram_Loop(net_src_t source, const void *data, size_t len) {
	net_udp_loop_t *loop = &net_udp_state.loops[source ^ 1];

	const uint32_t i = loop->send & (MAX_NET_UDP_LOOPS - 1);
	loop->send++;

	memcpy(loop->messages[i].data, data, len);
	loop->messages[i].size = len;

	return true;
}

/**
 * @brief Send a datagram to the specified address.
 */
_Bool Net_SendDatagram(net_src_t source, const net_addr_t *to, const void *data, size_t len) {

	if (to->type == NA_LOOP) {
		return Net_SendDatagram_Loop(source, data, len);
	}

	int32_t sock;
	if (to->type == NA_BROADCAST || to->type == NA_DATAGRAM) {
		if (!(sock = net_udp_state.sockets[source]))
			return false;
	} else {
		Com_Error(ERR_DROP, "Bad address type\n");
	}

	struct sockaddr_in to_addr;
	Net_NetAddrToSockaddr(to, &to_addr);

	ssize_t sent = sendto(sock, data, len, 0, (const struct sockaddr *) &to_addr, sizeof(to_addr));

	if (sent == -1) {
		Com_Warn("%s to %s\n", Net_GetErrorString(), Net_NetaddrToString(to));
		return false;
	}

	return true;
}

/**
 * @brief Sleeps for msec or until the server socket is ready.
 */
void Net_Sleep(uint32_t msec) {
	struct timeval timeout;
	fd_set fdset;

	const uint32_t sock = net_udp_state.sockets[NS_UDP_SERVER];

	if (!sock || !dedicated->value)
		return; // we're not a server, simply return


	FD_ZERO(&fdset);
	FD_SET(sock, &fdset); // server socket

	timeout.tv_sec = msec / 1000;
	timeout.tv_usec = (msec % 1000) * 1000;

	select(sock + 1, &fdset, NULL, NULL, &timeout);
}

/**
 * @brief Opens or closes the managed UDP socket for the given net_src_t. The
 * interface and port are resolved from immutable console variables, optionally
 * set at the command line.
 */
void Net_Config(net_src_t source, _Bool up) {
	int32_t *sock = &net_udp_state.sockets[source];

	if (up) {

		const cvar_t *net_interface = Cvar_Get("net_interface", "", CVAR_NO_SET, NULL);
		const cvar_t *net_port = Cvar_Get("net_port", va("%i", PORT_SERVER), CVAR_NO_SET, NULL);

		if (*sock == 0) {
			const char *iface = strlen(net_interface->string) ? net_interface->string : NULL;
			const in_port_t port = source == NS_UDP_SERVER ? net_port->integer : 0;

			*sock = Net_Socket(NA_DATAGRAM, iface, port);
		}
	} else {
		if (*sock != 0) {
			Net_CloseSocket(*sock);
			*sock = 0;
		}
	}
}

