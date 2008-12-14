/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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

#include "common.h"

#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/param.h>

#ifdef _WIN32
#include <winsock.h>
#include <ws2tcpip.h>
#define Net_GetError() WSAGetLastError()
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#define Net_GetError() errno
#endif

netadr_t net_local_adr = {
	NA_LOOPBACK, {127, 0, 0, 1}, 0
};

#define MAX_LOOPBACK 4

typedef struct {
	byte data[MAX_MSGLEN];
	int datalen;
} loopmsg_t;

typedef struct {
	loopmsg_t msgs[MAX_LOOPBACK];
	int get, send;
} loopback_t;

static loopback_t loopbacks[2];
static int ip_sockets[2];

static cvar_t *net_ip;
static cvar_t *net_port;


/*
Net_ErrorString
*/
static const char *Net_ErrorString(void){
	const int code = Net_GetError();
	return strerror(code);
}


/*
NetadrToSockadr
*/
static void NetadrToSockadr(const netadr_t *a, struct sockaddr_in *s){
	memset(s, 0, sizeof(*s));

	if(a->type == NA_IP_BROADCAST){
		s->sin_family = AF_INET;

		s->sin_port = a->port;
		*(int *)&s->sin_addr = -1;
	} else if(a->type == NA_IP){
		s->sin_family = AF_INET;

		*(int *)&s->sin_addr = *(int *)&a->ip;
		s->sin_port = a->port;
	}
}


/*
SockadrToNetadr
*/
static void SockadrToNetadr(const struct sockaddr_in *s, netadr_t *a){
	*(int *)&a->ip = *(int *)&s->sin_addr;
	a->port = s->sin_port;
	a->type = NA_IP;
}


/*
Net_CompareAdr
*/
qboolean Net_CompareAdr(netadr_t a, netadr_t b){
	if(a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2]
			&& a.ip[3] == b.ip[3] && a.port == b.port)
		return true;
	return false;
}


/*
Net_CompareBaseAdr
*/
qboolean Net_CompareBaseAdr(netadr_t a, netadr_t b){

	if(a.type != b.type)
		return false;

	if(a.type == NA_LOOPBACK)
		return true;

	if(a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2]
			&& a.ip[3] == b.ip[3])
		return true;

	return false;
}


/*
Net_AdrToString
*/
char *Net_AdrToString(netadr_t a){
	static char s[64];

	snprintf(s, sizeof(s), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1],
			a.ip[2], a.ip[3], ntohs(a.port));
	return s;
}

/*
Net_StringToSockaddr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
*/
static qboolean Net_StringToSockaddr(const char *s, struct sockaddr *sadr){
	struct hostent *h;
	char *colon;
	char copy[128];

	memset(sadr, 0, sizeof(*sadr));
	((struct sockaddr_in *)sadr)->sin_family = AF_INET;

	((struct sockaddr_in *)sadr)->sin_port = 0;

	strcpy(copy, s);
	// strip off a trailing :port if present
	for(colon = copy; *colon; colon++){
		if(*colon == ':'){
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = htons((short)atoi(colon + 1));
		}
	}

	if(copy[0] >= '0' && copy[0] <= '9'){
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	} else {
		if(!(h = gethostbyname(copy)))
			return false;
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}

	return true;
}


/*
Net_StringToAdr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
*/
qboolean Net_StringToAdr(const char *s, netadr_t *a){
	struct sockaddr_in sadr;

	if(!strcmp(s, "localhost")){
		memset(a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return true;
	}

	if(!Net_StringToSockaddr(s, (struct sockaddr *)&sadr))
		return false;

	SockadrToNetadr(&sadr, a);

	return true;
}


/*
Net_IsLocalAddress
*/
qboolean Net_IsLocalAddress(netadr_t adr){
	return Net_CompareAdr(adr, net_local_adr);
}


/*
Net_GetLoopPacket
*/
static qboolean Net_GetLoopPacket(netsrc_t source, netadr_t *from, sizebuf_t *message){
	int i;
	loopback_t *loop;

	loop = &loopbacks[source];

	if(loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if(loop->get >= loop->send)
		return false;

	i = loop->get & (MAX_LOOPBACK - 1);
	loop->get
	++;

	memcpy(message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	message->cursize = loop->msgs[i].datalen;
	*from = net_local_adr;
	return true;
}


/*
Net_SendLoopPacket
*/
static void Net_SendLoopPacket(netsrc_t source, size_t length, void *data, netadr_t to){
	int i;
	loopback_t *loop;

	loop = &loopbacks[source ^ 1];

	i = loop->send &(MAX_LOOPBACK - 1);
	loop->send++;

	memcpy(loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}


/*
Net_GetPacket
*/
qboolean Net_GetPacket(netsrc_t source, netadr_t *from, sizebuf_t *message){
	int ret, err;
	struct sockaddr_in from_addr;
	socklen_t from_len;
	char *s;

	if(Net_GetLoopPacket(source, from, message))
		return true;

	if(!ip_sockets[source])
		return false;

	from_len = sizeof(from_addr);

	ret = recvfrom(ip_sockets[source], (void *)message->data,
			message->maxsize, 0, (struct sockaddr *)&from_addr, &from_len);

	SockadrToNetadr(&from_addr, from);

	if(ret == -1){
		err = Net_GetError();

		if(err == EWOULDBLOCK || err == ECONNREFUSED)
			return false;  // not terribly abnormal

		s = source == NS_SERVER ? "server" : "client";
		Com_Warn("Net_GetPacket: %s: %s from %s\n", s, Net_ErrorString(), Net_AdrToString(*from));
		return false;
	}

	if(ret == message->maxsize){
		Com_Warn("Oversize packet from %s\n", Net_AdrToString(*from));
		return false;
	}

	message->cursize = ret;
	return true;
}


/*
Net_SendPacket
*/
void Net_SendPacket(netsrc_t source, size_t length, void *data, netadr_t to){
	struct sockaddr_in to_addr;
	int sock, ret;

	if(to.type == NA_LOOPBACK){
		Net_SendLoopPacket(source, length, data, to);
		return;
	}

	if(to.type == NA_IP_BROADCAST){
		sock = ip_sockets[source];
		if(!sock)
			return;
	} else if(to.type == NA_IP){
		sock = ip_sockets[source];
		if(!sock)
			return;
	} else {
		Com_Error(ERR_DROP, "Net_SendPacket: Bad address type.");
	}

	NetadrToSockadr(&to, &to_addr);

	ret = sendto(sock, data, length, 0,
			(struct sockaddr *)&to_addr, sizeof(to_addr));

	if(ret == -1)
		Com_Warn("Net_SendPacket: %s to %s.\n", Net_ErrorString(), Net_AdrToString(to));
}


/*
Net_Sleep

Sleeps for msec or until server socket is ready.
*/
void Net_Sleep(int msec){
	struct timeval timeout;
	fd_set fdset;

	if(!ip_sockets[NS_SERVER] || !dedicated->value)
		return;  // we're not a server, just run full speed

	FD_ZERO(&fdset);
	FD_SET(ip_sockets[NS_SERVER], &fdset);  // network socket
	timeout.tv_sec = msec / 1000;
	timeout.tv_usec = (msec % 1000) * 1000;
	select(ip_sockets[NS_SERVER] + 1, &fdset, NULL, NULL, &timeout);
}


/*
Net_Socket
*/
static int Net_Socket(const char *net_interface, int port){
	int sock;
	struct sockaddr_in addr;
	qboolean _true = true;
	int i = 1;

	if((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
		Com_Printf("ERROR: UDP_OpenSocket: socket: %s", Net_ErrorString());
		return 0;
	}

	// make it non-blocking
	if(ioctl(sock, FIONBIO, &_true) == -1){
		Com_Printf("ERROR: UDP_OpenSocket: ioctl FIONBIO: %s\n", Net_ErrorString());
		return 0;
	}

	// make it broadcast capable
	if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST,(char *)&i, sizeof(i)) == -1){
		Com_Printf("ERROR: UDP_OpenSocket: setsockopt SO_BROADCAST: %s\n", Net_ErrorString());
		return 0;
	}

	if(!net_interface || !net_interface[0] || !strcasecmp(net_interface, "localhost"))
		addr.sin_addr.s_addr = INADDR_ANY;
	else
		Net_StringToSockaddr(net_interface, (struct sockaddr *)&addr);

	if(port == PORT_ANY)
		addr.sin_port = 0;
	else
		addr.sin_port = htons((short)port);

	addr.sin_family = AF_INET;

	if(bind(sock, (void *)&addr, sizeof(addr)) == -1){
		Com_Printf("ERROR: UDP_OpenSocket: bind: %s\n", Net_ErrorString());
		close(sock);
		return 0;
	}

	return sock;
}


/*
Net_Config
*/
void Net_Config(netsrc_t source, qboolean up){
	int p;

	p = source == NS_CLIENT ? PORT_ANY : (int)net_port->value;

	if(up){  // open the socket
		if(!ip_sockets[source])
			ip_sockets[source] = Net_Socket(net_ip->string, p);
		return;
	}

	// or close it
	if(ip_sockets[source])
		close(ip_sockets[source]);

	ip_sockets[source] = 0;
}


/*
Net_Init
*/
void Net_Init(void){

#ifdef _WIN32
	WORD v;
	WSADATA d;

	v = MAKEWORD(2, 2);
	WSAStartup(v, &d);
#endif

	net_port = Cvar_Get("net_port", va("%i", PORT_SERVER), CVAR_NOSET, NULL);
	net_ip = Cvar_Get("net_ip", "localhost", CVAR_NOSET, NULL);
}


/*
Net_Shutdown
*/
void Net_Shutdown(void){

	Net_Config(NS_CLIENT, false);  // close client socket
	Net_Config(NS_SERVER, false);  // and server socket

#ifdef _WIN32
	WSACleanup();
#endif
}
