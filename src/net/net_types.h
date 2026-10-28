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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
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

#include "common/common.h"

/**
 * @brief Max length of a single netchan message, the ceiling of a UDP datagram. A message to a
 * remote address is sent as fragments of `MAX_MSG_FRAGMENT_SIZE`. See `NetChan_Transmit`.
 */
#define MAX_MSG_SIZE 65536

/**
 * @brief Max length of an out-of-band (connectionless) datagram. These are never fragmented.
 */
#define MAX_MSG_SIZE_OOB 16384

/**
 * @brief The max size of one fragment datagram, header included. Messages to a remote address
 * that do not fit are fragmented, so that no datagram depends on IP fragmentation.
 */
#define MAX_MSG_FRAGMENT_SIZE 1200

/**
 * @brief The fragment header: sequence, acknowledge, qport, offset and length.
 */
#define MAX_MSG_FRAGMENT_HEADER 13

/**
 * @brief The message bytes in each fragment. Both directions use this value, so that the receiver
 * knows that a shorter fragment is the last one.
 */
#define MAX_MSG_FRAGMENT_PAYLOAD (MAX_MSG_FRAGMENT_SIZE - MAX_MSG_FRAGMENT_HEADER)

/**
 * @brief The sequence word flags of the netchan header.
 */
#define NETCHAN_RELIABLE_BIT (1u << 31)
#define NETCHAN_FRAGMENT_BIT (1u << 30)

// A typedef for net_sockaddr, to reduce "struct" everywhere and silence Windows warning.
typedef struct sockaddr_in net_sockaddr;

typedef enum {
  NA_LOOP,
  NA_BROADCAST,
  NA_DATAGRAM,
  NA_STREAM
} NetAddrType;

typedef struct {
  NetAddrType type;
  in_addr_t addr;
  in_port_t port;
} NetAddr;

typedef enum {
  NS_UDP_CLIENT,
  NS_UDP_SERVER
} NetSrc;

typedef MemBuf NetMessage;

/**
 * @brief The network channel provides a conduit for packet sequencing and
 * optional reliable message delivery. The client and server speak explicitly
 * through this interface.
 */
typedef struct {
  NetSrc source;

  uint32_t dropped; // between last packet and previous

  uint32_t lastReceived; // for timeouts
  uint32_t lastSent; // for retransmits

  NetAddr remoteAddress;

  uint8_t qport; // to differentiate multiple clients behind NAT

  // sequencing variables
  uint32_t incomingSequence;
  uint32_t incomingAcknowledged;
  uint32_t outgoingSequence;

  uint32_t reliableSequence; // single bit
  uint32_t reliableAcknowledged; // single bit
  uint32_t reliableIncoming; // single bit
  uint32_t reliableOutgoing; // outgoing sequence number of last reliable

  NetMessage message; // writing buffer to send to server
  byte messageBuffer[MAX_MSG_SIZE - 10]; // leave space for header

  // message is copied to this buffer when it is first transfered
  size_t reliableSize;
  byte reliableBuffer[MAX_MSG_SIZE - 10]; // un-acked reliable message

  // the partial message being reassembled from fragments
  uint32_t fragmentSequence;
  size_t fragmentSize;
  byte fragmentBuffer[MAX_MSG_SIZE];
} NetChan;
