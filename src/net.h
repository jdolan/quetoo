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

#ifndef __NET_H__
#define __NET_H__

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#ifndef in_addr_t
typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;
#endif

#undef  EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#undef  ECONNREFUSED
#define ECONNREFUSED WSAECONNREFUSED

#define Net_GetError() WSAGetLastError()
#define Net_CloseSocket closesocket
#define ioctl ioctlsocket

#else

#include <netinet/in.h>

#define Net_GetError() errno
#define Net_CloseSocket close

#endif

#include "common.h"
#include "cvar.h"

typedef enum {
	NA_LOOP,
	NA_BROADCAST,
	NA_DATAGRAM,
	NA_STREAM
} net_addr_type_t;

typedef struct {
	net_addr_type_t type;
	in_addr_t addr;
	in_port_t port;
} net_addr_t;

typedef enum {
	NS_UDP_CLIENT,
	NS_UDP_SERVER
} net_src_t;

/*
 * @brief Max length of a single packet, due to UDP fragmentation. No single
 * net message can exceed this length. However, large frames can be split
 * into multiple messages and sent in series. See Sv_SendClientDatagram.
 */
#define MAX_MSG_SIZE 1400

const char *Net_GetErrorString(void);
_Bool Net_CompareNetaddr(const net_addr_t *a, const net_addr_t *b);
_Bool Net_CompareClientNetaddr(const net_addr_t *a, const net_addr_t *b);
_Bool Net_IsLocalNetaddr(const net_addr_t *a);

void Net_NetAddrToSockaddr(const net_addr_t *a, struct sockaddr_in *s);
const char *Net_NetaddrToString(const net_addr_t *a);
_Bool Net_StringToSockaddr(const char *s, struct sockaddr_in *saddr);
_Bool Net_StringToNetaddr(const char *s, net_addr_t *a);

int32_t Net_Socket(net_addr_type_t type, const char *interface, in_port_t port);

void Net_Init(void);
void Net_Shutdown(void);

#endif /* __NET_H__ */
