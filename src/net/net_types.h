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

#pragma once

#if defined(_WIN32)
	#include <inttypes.h>

	typedef uint32_t in_addr_t;
	typedef uint16_t in_port_t;

	#undef  EWOULDBLOCK
	#define EWOULDBLOCK  WSAEWOULDBLOCK
	#undef  ECONNREFUSED
	#define ECONNREFUSED WSAECONNREFUSED
	#undef  EINPROGRESS
	#define EINPROGRESS  WSAEINPROGRESS

#else

	#include <errno.h>
	#include <sys/select.h>
	#include <netinet/in.h>

#endif

#include "common.h"

/**
 * @brief Max length of a single packet. No individual command can exceed
 * this length. However, large server messages can be split into multiple
 * messages and sent in series. See Sv_SendClientDatagram.
 */
#define MAX_MSG_SIZE		16384

/**
 * @brief The max UDP message size, bounded by Windows MTU. Warn if we exceed this.
 */
#define MAX_MSG_SIZE_UDP	1450

// A typedef for net_sockaddr, to reduce "struct" everywhere and silence Windows warning.
typedef struct sockaddr_in net_sockaddr;

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

/**
 * @brief The network channel provides a conduit for packet sequencing and
 * optional reliable message delivery. The client and server speak explicitly
 * through this interface.
 */
typedef struct {
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

	mem_buf_t message; // writing buffer to send to server
	byte message_buffer[MAX_MSG_SIZE - 10]; // leave space for header

	// message is copied to this buffer when it is first transfered
	size_t reliable_size;
	byte reliable_buffer[MAX_MSG_SIZE - 10]; // un-acked reliable message
} net_chan_t;
