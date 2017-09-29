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

#include <errno.h>

#if defined(_WIN32)
	#include <winsock2.h>
	#include <ws2tcpip.h>

	#define ioctl ioctlsocket
#else
	#include <netdb.h>
	#include <netinet/tcp.h>
	#include <arpa/inet.h>
	#include <sys/ioctl.h>
	#include <sys/uio.h>
	#include <sys/socket.h>
#endif

#include "net.h"

in_addr_t net_lo;

int32_t Net_GetError(void) {
#if defined(_WIN32)
	return WSAGetLastError();
#else
	return errno;
#endif
}

/**
 * @return A printable error string for the most recent OS-level network error.
 */
const char *Net_GetErrorString(void) {
#if defined(_WIN32)
	static char s[MAX_STRING_CHARS] = { 0 };
	
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
				   NULL, Net_GetError(),
				   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				   s, sizeof(s), NULL);
	
	return s;
#else
	return strerror(Net_GetError());
#endif
}

/**
 * @brief Initializes the specified sockaddr_in according to the net_addr_t.
 */
void Net_NetAddrToSockaddr(const net_addr_t *a, net_sockaddr *s) {

	memset(s, 0, sizeof(*s));
	s->sin_family = AF_INET;

	if (a->type == NA_BROADCAST) {
		*(uint32_t *) &s->sin_addr = -1;
	} else if (a->type == NA_DATAGRAM) {
		*(in_addr_t *) &s->sin_addr = a->addr;
	}

	s->sin_port = a->port;
}

/**
 * @return True if the addresses share the same base and port.
 */
_Bool Net_CompareNetaddr(const net_addr_t *a, const net_addr_t *b) {
	return a->addr == b->addr && a->port == b->port;
}

/**
 * @return True if the addresses share the same type and base.
 */
_Bool Net_CompareClientNetaddr(const net_addr_t *a, const net_addr_t *b) {
	return a->type == b->type && a->addr == b->addr;
}

/**
 * @brief
 */
const char *Net_NetaddrToString(const net_addr_t *a) {
	static char s[64];

	g_snprintf(s, sizeof(s), "%s:%i", inet_ntoa(*(const struct in_addr *) &a->addr), ntohs(a->port));

	return s;
}

/**
 * @brief Resolve internet hostnames to sockaddr. Examples:
 *
 * localhost
 * idnewt
 * idnewt:28000
 * 192.246.40.70
 * 192.246.40.70:28000
 */
_Bool Net_StringToSockaddr(const char *s, net_sockaddr *saddr) {

	memset(saddr, 0, sizeof(*saddr));

	char *node = g_strdup(s);

	char *service = strchr(node, ':');
	if (service) {
		*service++ = '\0';
	}

	const struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_DGRAM,
	};

	struct addrinfo *info;
	if (getaddrinfo(node, service, &hints, &info) == 0) {
		memcpy(saddr, info->ai_addr, sizeof(*saddr));
		freeaddrinfo(info);
	}

	g_free(node);

	return saddr->sin_addr.s_addr != 0;
}

/**
 * @brief Parses the hostname and port into the specified net_addr_t.
 */
_Bool Net_StringToNetaddr(const char *s, net_addr_t *a) {
	net_sockaddr saddr;

	if (!Net_StringToSockaddr(s, &saddr)) {
		return false;
	}

	a->addr = saddr.sin_addr.s_addr;

	if (g_strcmp0(s, "localhost") == 0) {
		a->port = 0;
		a->type = NA_LOOP;
	} else {
		a->port = saddr.sin_port;
		a->type = NA_DATAGRAM;
	}

	return true;
}

/**
 * @brief Creates and binds a new network socket for the specified protocol.
 */
int32_t Net_Socket(net_addr_type_t type, const char *iface, in_port_t port) {
	int32_t sock, i = 1;

	switch (type) {
		case NA_BROADCAST:
		case NA_DATAGRAM:
			if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
				Com_Error(ERROR_DROP, "socket: %s\n", Net_GetErrorString());
			}

			if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const void *) &i, sizeof(i)) == -1) {
				Com_Error(ERROR_DROP, "setsockopt: %s\n", Net_GetErrorString());
			}

			Net_SetNonBlocking(sock, true);
			break;

		case NA_STREAM:
			if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
				Com_Error(ERROR_DROP, "socket: %s\n", Net_GetErrorString());
			}

			if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const void *) &i, sizeof(i)) == -1) {
				Com_Error(ERROR_DROP, "setsockopt: %s\n", Net_GetErrorString());
			}
			break;

		default:
			Com_Error(ERROR_DROP, "Invalid socket type: %d", type);
	}

	net_sockaddr addr;
	memset(&addr, 0, sizeof(addr));

	if (iface) {
		Net_StringToSockaddr(iface, &addr);
	} else {
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
	}

	addr.sin_port = htons(port);

	if (bind(sock, (void *) &addr, sizeof(addr)) == -1) {
		Com_Error(ERROR_DROP, "bind: %s\n", Net_GetErrorString());
	}

	return sock;
}

/**
 * @brief Make the specified socket non-blocking.
 */
void Net_SetNonBlocking(int32_t sock, _Bool non_blocking) {
	int32_t i = non_blocking;

	if (ioctl(sock, FIONBIO, (void *) &i) == -1) {
		Com_Error(ERROR_DROP, "ioctl: %s\n", Net_GetErrorString());
	}
}

/**
 * @brief
 */
void Net_CloseSocket(int32_t sock) {
#if defined(_WIN32)
	closesocket(sock);
#else
	close(sock);
#endif
}

/**
 * @brief
 */
void Net_Init(void) {

#if defined(_WIN32)
	WORD v;
	WSADATA d;

	v = MAKEWORD(2, 2);
	WSAStartup(v, &d);
#endif

	net_lo = inet_addr("127.0.0.1");
}

/**
 * @brief
 */
void Net_Shutdown(void) {

#if defined(_WIN32)
	WSACleanup();
#endif

}
