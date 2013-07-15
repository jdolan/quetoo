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

#include <errno.h>
#include <sys/time.h>
#include <sys/param.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#undef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#undef ECONNREFUSED
#define ECONNREFUSED WSAECONNREFUSED
#define Net_GetError() WSAGetLastError()
#define Net_CloseSocket closesocket
#define ioctl ioctlsocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#define Net_GetError() errno
#define Net_CloseSocket close
#endif

#include "net.h"

static net_addr_t net_local_addr = { NA_LOCAL, { 127, 0, 0, 1 }, 0 };

#define MAX_LOOPBACK 4

typedef struct {
	byte data[MAX_MSG_SIZE];
	size_t size;
} loopback_msg_t;

typedef struct {
	loopback_msg_t msgs[MAX_LOOPBACK];
	int32_t get, send;
} loopback_t;

static loopback_t loopbacks[2];
static int32_t ip_sockets[2];

static cvar_t *net_ip;
static cvar_t *net_port;

/*
 * @brief
 */
static const char *Net_ErrorString(void) {
	const int32_t code = Net_GetError();
	return strerror(code);
}

/*
 * @brief
 */
static void Net_NetAddrToSockaddr(const net_addr_t *a, struct sockaddr_in *s) {
	memset(s, 0, sizeof(*s));

	if (a->type == NA_IP_BROADCAST) {
		s->sin_family = AF_INET;
		s->sin_port = a->port;
		*(uint32_t *) &s->sin_addr = -1;
	} else if (a->type == NA_IP) {
		s->sin_family = AF_INET;
		s->sin_port = a->port;
		memcpy(&s->sin_addr, a->ip, sizeof(a->ip));
	}
}

/*
 * @brief
 */
static void Net_SockaddrToNetaddr(const struct sockaddr_in *s, net_addr_t *a) {
	memcpy(a->ip, &s->sin_addr, sizeof(a->ip));
	a->port = s->sin_port;
	a->type = NA_IP;
}

/*
 * @brief
 */
_Bool Net_CompareNetaddr(net_addr_t a, net_addr_t b) {

	if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3]
			&& a.port == b.port)
		return true;

	return false;
}

/*
 * @brief Similar to Net_CompareNetaddr, but omits port checks.
 */
_Bool Net_CompareClientNetaddr(net_addr_t a, net_addr_t b) {

	if (a.type != b.type)
		return false;

	if (a.type == NA_LOCAL)
		return true;

	if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3])
		return true;

	return false;
}

/*
 * @brief
 */
char *Net_NetaddrToString(net_addr_t a) {
	static char s[64];

	g_snprintf(s, sizeof(s), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));

	return s;
}

/*
 * @brief localhost
 * idnewt
 * idnewt:28000
 * 192.246.40.70
 * 192.246.40.70:28000
 */
static _Bool Net_StringToSockaddr(const char *s, struct sockaddr *saddr) {
	struct hostent *h;
	char *colon;
	char copy[128];

	memset(saddr, 0, sizeof(*saddr));
	((struct sockaddr_in *) saddr)->sin_family = AF_INET;

	((struct sockaddr_in *) saddr)->sin_port = 0;

	strcpy(copy, s);
	// strip off a trailing :port if present
	for (colon = copy; *colon; colon++) {
		if (*colon == ':') {
			*colon = 0;
			((struct sockaddr_in *) saddr)->sin_port = htons((int16_t)atoi(colon + 1));
		}
	}

	if (copy[0] >= '0' && copy[0] <= '9') {
		*(uint32_t *) &((struct sockaddr_in *) saddr)->sin_addr = inet_addr(copy);
	} else {
		if (!(h = gethostbyname(copy)))
			return false;
		*(uint32_t *) &((struct sockaddr_in *) saddr)->sin_addr = *(uint32_t *) h->h_addr_list[0];
	}

	return true;
}

/*
 * @brief localhost
 * idnewt
 * idnewt:28000
 * 192.246.40.70
 * 192.246.40.70:28000
 */
_Bool Net_StringToNetaddr(const char *s, net_addr_t *a) {
	struct sockaddr_in saddr;

	if (!g_strcmp0(s, "localhost")) {
		memset(a, 0, sizeof(*a));
		a->type = NA_LOCAL;
		return true;
	}

	if (!Net_StringToSockaddr(s, (struct sockaddr *) &saddr))
		return false;

	Net_SockaddrToNetaddr(&saddr, a);

	return true;
}

/*
 * @brief
 */
_Bool Net_IsLocalNetaddr(net_addr_t addr) {
	return Net_CompareNetaddr(addr, net_local_addr);
}

/*
 * @brief
 */
static _Bool Net_GetLocalPacket(net_src_t source, net_addr_t *from, size_buf_t *message) {
	uint32_t i;
	loopback_t *loop;

	loop = &loopbacks[source];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if (loop->get >= loop->send)
		return false;

	i = loop->get & (MAX_LOOPBACK - 1);
	loop->get++;

	memcpy(message->data, loop->msgs[i].data, loop->msgs[i].size);
	message->size = loop->msgs[i].size;
	*from = net_local_addr;
	return true;
}

/*
 * @brief
 */
static void Net_SendLocalPacket(net_src_t source, size_t length, void *data) {
	uint32_t i;
	loopback_t *loop;

	loop = &loopbacks[source ^ 1];

	i = loop->send & (MAX_LOOPBACK - 1);
	loop->send++;

	memcpy(loop->msgs[i].data, data, length);
	loop->msgs[i].size = length;
}

/*
 * @brief
 */
_Bool Net_GetPacket(net_src_t source, net_addr_t *from, size_buf_t *message) {
	ssize_t ret;
	int32_t err;
	struct sockaddr_in from_addr;
	socklen_t from_len;
	char *s;

	if (Net_GetLocalPacket(source, from, message))
		return true;

	if (!ip_sockets[source])
		return false;

	from_len = sizeof(from_addr);

	ret = recvfrom(ip_sockets[source], (void *) message->data, message->max_size, 0,
			(struct sockaddr *) &from_addr, &from_len);

	Net_SockaddrToNetaddr(&from_addr, from);

	if (ret == -1) {
		err = Net_GetError();

		if (err == EWOULDBLOCK || err == ECONNREFUSED)
			return false; // not terribly abnormal

		s = source == NS_SERVER ? "server" : "client";
		Com_Warn("%s: %s from %s\n", s, Net_ErrorString(),
				Net_NetaddrToString(*from));
		return false;
	}

	if (ret == ((ssize_t) message->max_size)) {
		Com_Warn("Oversized packet from %s\n", Net_NetaddrToString(*from));
		return false;
	}

	message->size = (uint32_t) ret;
	return true;
}

/*
 * @brief
 */
void Net_SendPacket(net_src_t source, size_t size, void *data, net_addr_t to) {
	struct sockaddr_in to_addr;
	int32_t sock, ret;

	if (to.type == NA_LOCAL) {
		Net_SendLocalPacket(source, size, data);
		return;
	}

	if (to.type == NA_IP_BROADCAST) {
		sock = ip_sockets[source];
		if (!sock)
			return;
	} else if (to.type == NA_IP) {
		sock = ip_sockets[source];
		if (!sock)
			return;
	} else {
		Com_Error(ERR_DROP, "Bad address type\n");
		return;
	}

	Net_NetAddrToSockaddr(&to, &to_addr);

	ret = sendto(sock, data, size, 0, (struct sockaddr *) &to_addr, sizeof(to_addr));

	if (ret == -1)
		Com_Warn("%s to %s.\n", Net_ErrorString(), Net_NetaddrToString(to));
}

/*
 * @brief Sleeps for msec or until server socket is ready.
 */
void Net_Sleep(uint32_t msec) {
	struct timeval timeout;
	fd_set fdset;

	if (!ip_sockets[NS_SERVER] || !dedicated->value)
		return; // we're not a server, simply return

	FD_ZERO(&fdset);
	FD_SET(ip_sockets[NS_SERVER], &fdset); // network socket
	timeout.tv_sec = msec / 1000;
	timeout.tv_usec = (msec % 1000) * 1000;
	select(ip_sockets[NS_SERVER] + 1, &fdset, NULL, NULL, &timeout);
}

/*
 * @brief
 */
static int32_t Net_Socket(const char *net_interface, uint16_t port) {
	int32_t sock;
	struct sockaddr_in addr;
	int32_t i = 1;

	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		Com_Error(ERR_FATAL, "Call socket: %s\n", Net_ErrorString());
		return 0;
	}

	// make it non-blocking
	if (ioctl(sock, FIONBIO, (char *) &i) == -1) {
		Com_Error(ERR_FATAL, "Call ioctl: %s\n", Net_ErrorString());
		return 0;
	}

	// make it broadcast capable
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *) &i, sizeof(i)) == -1) {
		Com_Error(ERR_FATAL, "Call setsockopt: %s\n", Net_ErrorString());
		return 0;
	}

	if (!net_interface || !net_interface[0] || !g_ascii_strcasecmp(net_interface, "localhost"))
		addr.sin_addr.s_addr = INADDR_ANY;
	else
		Net_StringToSockaddr(net_interface, (struct sockaddr *) &addr);

	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;

	if (bind(sock, (void *) &addr, sizeof(addr)) == -1) {
		Com_Print("ERROR: UDP_OpenSocket: bind: %s\n", Net_ErrorString());
		close(sock);
		return 0;
	}

	return sock;
}

/*
 * @brief
 */
void Net_Config(net_src_t source, _Bool up) {
	uint16_t p = 0;

	if (source == NS_SERVER) {
		p = (uint16_t) net_port->integer;
	}

	if (up) { // open the socket
		if (!ip_sockets[source])
			ip_sockets[source] = Net_Socket(net_ip->string, p);
		return;
	}

	// or close it
	if (ip_sockets[source])
		Net_CloseSocket(ip_sockets[source]);

	ip_sockets[source] = 0;
}

/*
 * @brief
 */
void Net_Init(void) {

#ifdef _WIN32
	WORD v;
	WSADATA d;

	v = MAKEWORD(2, 2);
	WSAStartup(v, &d);
#endif

	net_port = Cvar_Get("net_port", va("%i", PORT_SERVER), CVAR_NO_SET, NULL);
	net_ip = Cvar_Get("net_ip", "localhost", CVAR_NO_SET, NULL);
}

/*
 * @brief
 */
void Net_Shutdown(void) {

	Net_Config(NS_CLIENT, false); // close client socket
	Net_Config(NS_SERVER, false); // and server socket

#ifdef _WIN32
	WSACleanup();
#endif
}
