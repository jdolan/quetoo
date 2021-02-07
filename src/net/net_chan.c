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

#include "net_chan.h"

/*
 *
 * packet header
 * -------------
 * 31	sequence
 * 1	does this message contain a reliable payload
 * 31	acknowledge sequence
 * 1	acknowledge receipt of even/odd message
 * 8	qport
 *
 * The remote connection never knows if it missed a reliable message, the
 * local side detects that it has been dropped by seeing a sequence acknowledge
 * higher than the last reliable sequence, but without the correct even/odd
 * bit for the reliable set.
 *
 * If the sender notices that a reliable message has been dropped, it will be
 * retransmitted. It will not be retransmitted again until a message after
 * the retransmit has been acknowledged and the reliable still failed to get theve.
 *
 * if the sequence number is -1, the packet should be handled without a netcon
 *
 * The reliable message can be added to at any time by doing
 * Net_Write*(&netchan->message, <data>).
 *
 * If the message buffer is overflowed, either by a single message, or by
 * multiple frames worth piling up while the last reliable transmit goes
 * unacknowledged, the netchan signals a fatal error.
 *
 * Reliable messages are always placed first in a packet, then the unreliable
 * message is included if there is sufficient room.
 *
 * To the receiver, there is no distinction between the reliable and unreliable
 * parts of the message, they are just processed out as a single larger message.
 *
 * Illogical packet sequence numbers cause the packet to be dropped, but do
 * not kill the connection. This, combined with the tight window of valid
 * reliable acknowledgement numbers provides protection against malicious
 * address spoofing.
 *
 * The qport field is a workaround for bad address translating routers that
 * sometimes remap the client's source port on a packet during gameplay.
 *
 * If the base part of the net address matches and the qport matches, then the
 * channel matches even if the IP port differs. The IP port should be updated
 * to the new value before sending out any replies.
 *
 * If there is no information that needs to be transfered on a given frame,
 * such as during the connection stage while waiting for the client to load,
 * then a packet only needs to be delivered if there is something in the
 * unacknowledged reliable
 */

static cvar_t *net_show_packets;
static cvar_t *net_show_drop;

net_addr_t net_from;
mem_buf_t net_message;
static byte net_message_buffer[MAX_MSG_SIZE];

/**
 * @brief Sends an out-of-band datagram
 */
void Netchan_OutOfBand(int32_t sock, const net_addr_t *addr, const void *data, size_t len) {
	mem_buf_t send;
	byte send_buffer[MAX_MSG_SIZE];

	// write the packet header
	Mem_InitBuffer(&send, send_buffer, sizeof(send_buffer));

	Net_WriteLong(&send, -1); // -1 sequence means out of band
	Mem_WriteBuffer(&send, data, len);

	// send the datagram
	Net_SendDatagram(sock, addr, send.data, send.size);
}

/**
 * @brief Sends a text message in an out-of-band datagram
 */
void Netchan_OutOfBandPrint(int32_t sock, const net_addr_t *addr, const char *format, ...) {
	va_list args;
	char string[MAX_MSG_SIZE - 4];

	memset(string, 0, sizeof(string));

	va_start(args, format);
	vsnprintf(string, sizeof(string), format, args);
	va_end(args);

	Netchan_OutOfBand(sock, addr, (const void *) string, strlen(string));
}

/**
 * @brief Called to open a channel to a remote system.
 */
void Netchan_Setup(net_src_t source, net_chan_t *chan, net_addr_t *addr, uint8_t qport) {

	memset(chan, 0, sizeof(*chan));

	chan->source = source;
	chan->remote_address = *addr;
	chan->qport = qport;

	chan->last_received = quetoo.ticks;
	chan->incoming_sequence = 0;
	chan->outgoing_sequence = 1;

	Mem_InitBuffer(&chan->message, chan->message_buffer, sizeof(chan->message_buffer));
	chan->message.allow_overflow = true;
}

/**
 * @return True if reliable data must be transmitted this frame, false
 * otherwise.
 */
static _Bool Netchan_CheckRetransmit(net_chan_t *chan) {

	// if the remote side dropped the last reliable message, re-send it
	if (chan->incoming_acknowledged > chan->reliable_outgoing && chan->reliable_acknowledged
	        != chan->reliable_sequence) {
		return true;
	}

	return false;
}

/**
 * @brief Tries to send an unreliable message to a connection, and handles the
 * transmission / retransmission of the reliable messages.
 *
 * A 0 size will still generate a packet and deal with the reliable messages.
 */
void Netchan_Transmit(net_chan_t *chan, byte *data, size_t len) {
	mem_buf_t send;
	byte send_buffer[MAX_MSG_SIZE];

	// check for message overflow
	if (chan->message.overflowed) {
		Com_Error(ERROR_DROP, "%s: Overflow\n", Net_NetaddrToString(&chan->remote_address));
	}

	// check for re-transmission of reliable message
	_Bool send_reliable = Netchan_CheckRetransmit(chan);

	// or for transmission of a new one
	if (!chan->reliable_size && chan->message.size) {
		memcpy(chan->reliable_buffer, chan->message_buffer, chan->message.size);
		chan->reliable_size = chan->message.size;
		chan->message.size = 0;
		chan->reliable_sequence ^= 1;
		send_reliable = true;
	}

	// write the packet header
	Mem_InitBuffer(&send, send_buffer, sizeof(send_buffer));

	const uint32_t w1 = (chan->outgoing_sequence & ~(1u << 31)) | (send_reliable << 31);
	const uint32_t w2 = (chan->incoming_sequence & ~(1u << 31)) | (chan->reliable_incoming << 31);

	chan->outgoing_sequence++;
	chan->last_sent = quetoo.ticks;

	Net_WriteLong(&send, w1);
	Net_WriteLong(&send, w2);

	// send the qport if we are a client
	if (chan->source == NS_UDP_CLIENT) {
		Net_WriteByte(&send, chan->qport);
	}

	// copy the reliable message to the packet first
	if (send_reliable) {
		Mem_WriteBuffer(&send, chan->reliable_buffer, chan->reliable_size);
		chan->reliable_outgoing = chan->outgoing_sequence;
	}

	// add the unreliable part if space is available
	if (send.max_size - send.size >= len) {
		Mem_WriteBuffer(&send, data, len);
	} else {
		Com_Warn("Netchan_Transmit: dumped unreliable\n");
	}

	// send the datagram
	Net_SendDatagram(chan->source, &chan->remote_address, send.data, send.size);

	if (net_show_packets->value) {
		if (send_reliable)
			Com_Print("Send %u bytes: s=%i reliable=%i ack=%i rack=%i\n", (uint32_t) send.size,
			          chan->outgoing_sequence - 1, chan->reliable_sequence, chan->incoming_sequence,
			          chan->reliable_incoming);
		else
			Com_Print("Send %u bytes : s=%i ack=%i rack=%i\n", (uint32_t) send.size,
			          chan->outgoing_sequence - 1, chan->incoming_sequence, chan->reliable_incoming);
	}
}

/**
 * @brief Called when the current net_message is from remote_address
 * modifies net_message so that it points to the packet payload
 */
_Bool Netchan_Process(net_chan_t *chan, mem_buf_t *msg) {
	uint32_t sequence, sequence_ack;
	uint32_t reliable_ack, reliable_message;

	// get sequence numbers
	Net_BeginReading(msg);

	sequence = Net_ReadLong(msg);
	sequence_ack = Net_ReadLong(msg);

	// read the qport if we are a server
	if (chan->source == NS_UDP_SERVER) {
		Net_ReadByte(msg);
	}

	reliable_message = sequence >> 31u;
	reliable_ack = sequence_ack >> 31u;

	sequence &= ~(1u << 31);
	sequence_ack &= ~(1u << 31);

	if (net_show_packets->value) {
		if (reliable_message)
			Com_Print("Recv %u bytes: s=%i reliable=%i ack=%i rack=%i\n", (uint32_t) msg->size,
			          sequence, chan->reliable_incoming ^ 1, sequence_ack, reliable_ack);
		else
			Com_Print("Recv %u bytes : s=%i ack=%i rack=%i\n", (uint32_t) msg->size, sequence,
			          sequence_ack, reliable_ack);
	}

	// discard stale or duplicated packets
	if (sequence <= chan->incoming_sequence) {
		if (net_show_drop->value)
			Com_Print("%s:Out of order packet %i at %i\n",
			          Net_NetaddrToString(&chan->remote_address), sequence, chan->incoming_sequence);
		return false;
	}

	// dropped packets don't keep the message from being used
	chan->dropped = sequence - (chan->incoming_sequence + 1);
	if (chan->dropped > 0) {
		if (net_show_drop->value)
			Com_Print("%s:Dropped %i packets at %i\n", Net_NetaddrToString(&chan->remote_address),
			          chan->dropped, sequence);
	}

	// if the current outgoing reliable message has been acknowledged
	// clear the buffer to make way for the next
	if (reliable_ack == chan->reliable_sequence) {
		chan->reliable_size = 0;    // it has been received
	}

	// if this message contains a reliable message, bump reliable_incoming
	chan->incoming_sequence = sequence;
	chan->incoming_acknowledged = sequence_ack;
	chan->reliable_acknowledged = reliable_ack;
	if (reliable_message) {
		chan->reliable_incoming ^= 1;
	}

	// the message can now be read from the current message pointer
	chan->last_received = quetoo.ticks;

	return true;
}

/**
 * @brief
 */
void Netchan_Init(void) {

	Net_Init();

	net_show_packets = Cvar_Add("net_show_packets", "0", 0, NULL);
	net_show_drop = Cvar_Add("net_show_drop", "0", 0, NULL);

	Mem_InitBuffer(&net_message, net_message_buffer, sizeof(net_message_buffer));
}

/**
 * @brief
 */
void Netchan_Shutdown(void) {

	Net_Config(NS_UDP_CLIENT, false);
	Net_Config(NS_UDP_SERVER, false);

	Net_Shutdown();
}

