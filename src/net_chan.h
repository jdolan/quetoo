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

#ifndef __NET_CHAN_H__
#define __NET_CHAN_H__

#include "net_udp.h"
#include "net_message.h"

/*
 * @brief The network channel provides a conduit for packet sequencing and
 * optional reliable message delivery. The client and server speak explicitly
 * through this interface.
 */
typedef struct {
	_Bool fatal_error;

	net_src_t source;

	uint32_t dropped; // between last packet and previous

	uint32_t last_received; // for timeouts
	uint32_t last_sent; // for retransmits

	net_addr_t remote_address;

	uint8_t qport; // to differentiate multiple clients behind NAT

	// sequencing variables
	uint32_t incoming_sequence;
	uint32_t incoming_acknowledged;
	uint32_t outgoing_sequence;

	uint32_t reliable_sequence; // single bit
	uint32_t reliable_acknowledged; // single bit
	uint32_t reliable_incoming; // single bit
	uint32_t reliable_outgoing; // outgoing sequence number of last reliable

	// reliable staging and holding areas
	mem_buf_t message; // writing buffer to send to server
	byte message_buffer[MAX_MSG_SIZE - 16]; // leave space for header

	// message is copied to this buffer when it is first transfered
	size_t reliable_size;
	byte reliable_buffer[MAX_MSG_SIZE - 16]; // un-acked reliable message
} net_chan_t;

extern net_addr_t net_from;
extern mem_buf_t net_message;

void Netchan_Setup(net_src_t source, net_chan_t *chan, net_addr_t *addr, uint8_t qport);
void Netchan_Transmit(net_chan_t *chan, byte *data, size_t len);
void Netchan_OutOfBand(int32_t sock, const net_addr_t *addr, const void *data, size_t len);
void Netchan_OutOfBandPrint(int32_t sock, const net_addr_t *addr, const char *format, ...) __attribute__((format(printf, 3, 4)));
_Bool Netchan_Process(net_chan_t *chan, mem_buf_t *msg);
void Netchan_Init(void);
void Netchan_Shutdown(void);

#endif /* __NET_CHAN_H__ */
