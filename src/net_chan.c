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

#include "net.h"

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
 * retransmitted.  It will not be retransmitted again until a message after
 * the retransmit has been acknowledged and the reliable still failed to get theve.
 *
 * if the sequence number is -1, the packet should be handled without a netcon
 *
 * The reliable message can be added to at any time by doing
 * Msg_Write*(&netchan->message, <data>).
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
 * not kill the connection.  This, combined with the tight window of valid
 * reliable acknowledgement numbers provides protection against malicious
 * address spoofing.
 *
 * The qport field is a workaround for bad address translating routers that
 * sometimes remap the client's source port on a packet during gameplay.
 *
 * If the base part of the net address matches and the qport matches, then the
 * channel matches even if the IP port differs.  The IP port should be updated
 * to the new value before sending out any replies.
 *
 * If there is no information that needs to be transfered on a given frame,
 * such as during the connection stage while waiting for the client to load,
 * then a packet only needs to be delivered if there is something in the
 * unacknowledged reliable
 */

cvar_t *net_showpackets;
cvar_t *net_showdrop;
cvar_t *net_qport;

net_addr_t net_from;
size_buf_t net_message;
byte net_message_buffer[MAX_MSG_SIZE];

/*
 * Netchan_Init
 *
 */
void Netchan_Init(void) {
	byte p;

	net_showpackets = Cvar_Get("net_showpackets", "0", 0, NULL);
	net_showdrop = Cvar_Get("net_showdrop", "0", 0, NULL);

	// assign a small random number for the qport
	p = ((unsigned int) time(NULL)) & 255;
	net_qport = Cvar_Get("net_qport", va("%d", p), CVAR_NO_SET, NULL);
}

/*
 * Netchan_OutOfBand
 *
 * Sends an out-of-band datagram
 */
void Netchan_OutOfBand(int net_socket, net_addr_t addr, size_t size, byte *data) {
	size_buf_t send;
	byte send_buffer[MAX_MSG_SIZE];

	// write the packet header
	Sb_Init(&send, send_buffer, sizeof(send_buffer));

	Msg_WriteLong(&send, -1); // -1 sequence means out of band
	Sb_Write(&send, data, size);

	// send the datagram
	Net_SendPacket(net_socket, send.size, send.data, addr);
}

/*
 * Netchan_OutOfBandPrint
 *
 * Sends a text message in an out-of-band datagram
 */
void Netchan_OutOfBandPrint(int net_socket, net_addr_t addr,
		const char *format, ...) {
	va_list args;
	char string[MAX_MSG_SIZE - 4];

	memset(string, 0, sizeof(string));

	va_start(args, format);
	vsnprintf(string, sizeof(string), format, args);
	va_end(args);

	Netchan_OutOfBand(net_socket, addr, strlen(string), (byte *) string);
}

/*
 * Netchan_Setup
 *
 * called to open a channel to a remote system
 */
void Netchan_Setup(net_src_t source, net_chan_t *chan, net_addr_t addr,
		byte qport) {
	memset(chan, 0, sizeof(*chan));

	chan->source = source;
	chan->remote_address = addr;
	chan->qport = qport;
	chan->last_received = quake2world.time;
	chan->incoming_sequence = 0;
	chan->outgoing_sequence = 1;

	Sb_Init(&chan->message, chan->message_buffer, sizeof(chan->message_buffer));
	chan->message.allow_overflow = true;
}

/*
 * Netchan_CanReliable
 *
 * Returns true if the last reliable message has acked
 */
boolean_t Netchan_CanReliable(net_chan_t *chan) {
	if (chan->reliable_size)
		return false; // waiting for ack
	return true;
}

/*
 * Netchan_NeedReliable
 */
boolean_t Netchan_NeedReliable(net_chan_t *chan) {
	boolean_t send_reliable;

	// if the remote side dropped the last reliable message, resend it
	send_reliable = false;

	if (chan->incoming_acknowledged > chan->last_reliable_sequence
			&& chan->incoming_reliable_acknowledged != chan->reliable_sequence)
		send_reliable = true;

	// if the reliable transmit buffer is empty, copy the current message out
	if (!chan->reliable_size && chan->message.size) {
		send_reliable = true;
	}

	return send_reliable;
}

/*
 * Netchan_Transmit
 *
 * Tries to send an unreliable message to a connection, and handles the
 * transmition / retransmition of the reliable messages.
 *
 * A 0 size will still generate a packet and deal with the reliable messages.
 */
void Netchan_Transmit(net_chan_t *chan, size_t size, byte *data) {
	size_buf_t send;
	byte send_buffer[MAX_MSG_SIZE];
	boolean_t send_reliable;
	unsigned int w1, w2;

	// check for message overflow
	if (chan->message.overflowed) {
		chan->fatal_error = true;
		Com_Print("%s:Outgoing message overflow\n",
				Net_NetaddrToString(chan->remote_address));
		return;
	}

	send_reliable = Netchan_NeedReliable(chan);

	if (!chan->reliable_size && chan->message.size) {
		memcpy(chan->reliable_buffer, chan->message_buffer, chan->message.size);
		chan->reliable_size = chan->message.size;
		chan->message.size = 0;
		chan->reliable_sequence ^= 1;
	}

	// write the packet header
	Sb_Init(&send, send_buffer, sizeof(send_buffer));

	w1 = (chan->outgoing_sequence & ~(1 << 31)) | (send_reliable << 31);
	w2 = (chan->incoming_sequence & ~(1 << 31))
			| (chan->incoming_reliable_sequence << 31);

	chan->outgoing_sequence++;
	chan->last_sent = quake2world.time;

	Msg_WriteLong(&send, w1);
	Msg_WriteLong(&send, w2);

	// send the qport if we are a client
	if (chan->source == NS_CLIENT)
		Msg_WriteByte(&send, chan->qport);

	// copy the reliable message to the packet first
	if (send_reliable) {
		Sb_Write(&send, chan->reliable_buffer, chan->reliable_size);
		chan->last_reliable_sequence = chan->outgoing_sequence;
	}

	// add the unreliable part if space is available
	if (send.max_size - send.size >= size)
		Sb_Write(&send, data, size);
	else
		Com_Print("Netchan_Transmit: dumped unreliable\n");

	// send the datagram
	Net_SendPacket(chan->source, send.size, send.data, chan->remote_address);

	if (net_showpackets->value) {
		if (send_reliable)
			Com_Print("send "Q2W_SIZE_T" : s=%i reliable=%i ack=%i rack=%i\n",
					send.size, chan->outgoing_sequence - 1,
					chan->reliable_sequence, chan->incoming_sequence,
					chan->incoming_reliable_sequence);
		else
			Com_Print("send "Q2W_SIZE_T" : s=%i ack=%i rack=%i\n", send.size,
					chan->outgoing_sequence - 1, chan->incoming_sequence,
					chan->incoming_reliable_sequence);
	}
}

/*
 * Netchan_Process
 *
 * called when the current net_message is from remote_address
 * modifies net_message so that it points to the packet payload
 */
boolean_t Netchan_Process(net_chan_t *chan, size_buf_t *msg) {
	unsigned int sequence, sequence_ack;
	unsigned int reliable_ack, reliable_message;
	byte qport;

	// get sequence numbers
	Msg_BeginReading(msg);

	sequence = Msg_ReadLong(msg);
	sequence_ack = Msg_ReadLong(msg);

	// read the qport if we are a server
	if (chan->source == NS_SERVER)
		qport = Msg_ReadByte(msg);

	reliable_message = sequence >> 31;
	reliable_ack = sequence_ack >> 31;

	sequence &= ~(1 << 31);
	sequence_ack &= ~(1 << 31);

	if (net_showpackets->value) {
		if (reliable_message)
			Com_Print("recv "Q2W_SIZE_T" : s=%i reliable=%i ack=%i rack=%i\n",
					msg->size, sequence, chan->incoming_reliable_sequence ^ 1,
					sequence_ack, reliable_ack);
		else
			Com_Print("recv "Q2W_SIZE_T" : s=%i ack=%i rack=%i\n", msg->size,
					sequence, sequence_ack, reliable_ack);
	}

	// discard stale or duplicated packets
	if (sequence <= chan->incoming_sequence) {
		if (net_showdrop->value)
			Com_Print("%s:Out of order packet %i at %i\n",
					Net_NetaddrToString(chan->remote_address), sequence,
					chan->incoming_sequence);
		return false;
	}

	// dropped packets don't keep the message from being used
	chan->dropped = sequence - (chan->incoming_sequence + 1);
	if (chan->dropped > 0) {
		if (net_showdrop->value)
			Com_Print("%s:Dropped %i packets at %i\n",
					Net_NetaddrToString(chan->remote_address), chan->dropped,
					sequence);
	}

	// if the current outgoing reliable message has been acknowledged
	// clear the buffer to make way for the next
	if (reliable_ack == chan->reliable_sequence)
		chan->reliable_size = 0; // it has been received

	// if this message contains a reliable message, bump incoming_reliable_sequence
	chan->incoming_sequence = sequence;
	chan->incoming_acknowledged = sequence_ack;
	chan->incoming_reliable_acknowledged = reliable_ack;
	if (reliable_message) {
		chan->incoming_reliable_sequence ^= 1;
	}

	// the message can now be read from the current message pointer
	chan->last_received = quake2world.time;

	return true;
}

