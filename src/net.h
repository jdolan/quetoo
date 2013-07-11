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

#include "common.h"
#include "cvar.h"

typedef enum {
	NA_LOCAL,
	NA_IP_BROADCAST,
	NA_IP
} net_adr_type_t;

typedef enum {
	NS_CLIENT,
	NS_SERVER
} net_src_t;

typedef struct {
	net_adr_type_t type;
	byte ip[4];
	uint16_t port;
} net_addr_t;

void Net_Init(void);
void Net_Shutdown(void);

void Net_Config(net_src_t source, _Bool up);

_Bool Net_GetPacket(net_src_t source, net_addr_t *from, size_buf_t *message);
void Net_SendPacket(net_src_t source, size_t length, void *data, net_addr_t to);

_Bool Net_CompareNetaddr(net_addr_t a, net_addr_t b);
_Bool Net_CompareClientNetaddr(net_addr_t a, net_addr_t b);
_Bool Net_IsLocalNetaddr(net_addr_t adr);
char *Net_NetaddrToString(net_addr_t a);
_Bool Net_StringToNetaddr(const char *s, net_addr_t *a);
void Net_Sleep(uint32_t msec);

/*
 * @brief Max length of a single packet, due to UDP fragmentation. No single
 * net message can exceed this length. However, large frames can be split
 * into multiple messages and sent in series. See Sv_SendClientDatagram.
 */
#define MAX_MSG_SIZE 1400

typedef struct {
	_Bool fatal_error;

	net_src_t source;

	uint32_t dropped; // between last packet and previous

	uint32_t last_received; // for timeouts
	uint32_t last_sent; // for retransmits

	net_addr_t remote_address;

	uint8_t qport; // qport value to write when transmitting

	// sequencing variables
	uint32_t incoming_sequence;
	uint32_t incoming_acknowledged;
	uint32_t incoming_reliable_acknowledged; // single bit

	uint32_t incoming_reliable_sequence; // single bit, maintained local

	uint32_t outgoing_sequence;
	uint32_t reliable_sequence; // single bit
	uint32_t last_reliable_sequence; // sequence number of last send

	// reliable staging and holding areas
	size_buf_t message; // writing buffer to send to server
	byte message_buffer[MAX_MSG_SIZE - 16]; // leave space for header

	// message is copied to this buffer when it is first transfered
	size_t reliable_size;
	byte reliable_buffer[MAX_MSG_SIZE - 16]; // un-acked reliable message
} net_chan_t;

extern net_addr_t net_from;
extern size_buf_t net_message;
extern byte net_message_buffer[MAX_MSG_SIZE];

void Netchan_Init(void);
void Netchan_Setup(net_src_t source, net_chan_t *chan, net_addr_t addr, uint8_t qport);
void Netchan_Transmit(net_chan_t *chan, size_t size, byte *data);
void Netchan_OutOfBand(int32_t net_socket, net_addr_t addr, size_t size, byte *data);
void Netchan_OutOfBandPrint(int32_t net_socket, net_addr_t addr, const char *format, ...) __attribute__((format(printf, 3, 4)));
_Bool Netchan_Process(net_chan_t *chan, size_buf_t *msg);
_Bool Netchan_CanReliable(net_chan_t *chan);
_Bool Netchan_NeedReliable(net_chan_t *chan);

#endif /* __NET_H__ */
