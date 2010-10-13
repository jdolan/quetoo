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

#include "cvar.h"

#define PORT_ANY	-1

#define MAX_MSGLEN 	1400  // max length of a message

#define PACKET_HEADER 10  // two ints and a short

typedef enum {
	NA_LOCAL,
	NA_IP_BROADCAST,
	NA_IP
} netadrtype_t;

typedef enum {
	NS_CLIENT,
	NS_SERVER
} netsrc_t;

typedef struct {
	netadrtype_t type;
	byte ip[4];
	unsigned short port;
} netaddr_t;

void Net_Init(void);
void Net_Shutdown(void);

void Net_Config(netsrc_t source, qboolean up);

qboolean Net_GetPacket(netsrc_t source, netaddr_t *from, sizebuf_t *message);
void Net_SendPacket(netsrc_t source, size_t length, void *data, netaddr_t to);

qboolean Net_CompareNetaddr(netaddr_t a, netaddr_t b);
qboolean Net_CompareClientNetaddr(netaddr_t a, netaddr_t b);
qboolean Net_IsLocalNetaddr(netaddr_t adr);
char *Net_NetaddrToString(netaddr_t a);
qboolean Net_StringToNetaddr(const char *s, netaddr_t *a);
void Net_Sleep(int msec);


typedef struct {
	qboolean fatal_error;

	netsrc_t source;

	int dropped;  // between last packet and previous

	int last_received;  // for timeouts
	int last_sent;  // for retransmits

	netaddr_t remote_address;

	int qport;  // qport value to write when transmitting

	// sequencing variables
	int incoming_sequence;
	int incoming_acknowledged;
	int incoming_reliable_acknowledged;  // single bit

	int incoming_reliable_sequence;  // single bit, maintained local

	int outgoing_sequence;
	int reliable_sequence;  // single bit
	int last_reliable_sequence;  // sequence number of last send

	// reliable staging and holding areas
	sizebuf_t message;  // writing buffer to send to server
	byte message_buf[MAX_MSGLEN - 16];  // leave space for header

	// message is copied to this buffer when it is first transfered
	int reliable_length;
	byte reliable_buf[MAX_MSGLEN - 16];  // unacked reliable message
} netchan_t;

extern netaddr_t net_from;
extern sizebuf_t net_message;
extern byte net_message_buffer[MAX_MSGLEN];

void Netchan_Init(void);
void Netchan_Setup(netsrc_t source, netchan_t *chan, netaddr_t adr, int qport);
void Netchan_Transmit(netchan_t *chan, int length, byte *data);
void Netchan_OutOfBand(int net_socket, netaddr_t adr, int length, byte *data);
void Netchan_OutOfBandPrint(int net_socket, netaddr_t adr, const char *format, ...) __attribute__((format(printf, 3, 4)));
qboolean Netchan_Process(netchan_t *chan, sizebuf_t *msg);
qboolean Netchan_CanReliable(netchan_t *chan);
qboolean Netchan_NeedReliable(netchan_t *chan);

#endif /* __NET_H__ */
