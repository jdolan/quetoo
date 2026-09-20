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

#include "net_chan.h"

/*
 *
 * packet header
 * -------------
 * 31  sequence
 * 1  does this message contain a reliable payload
 * 31  acknowledge sequence
 * 1  acknowledge receipt of even/odd message
 * 8  qport
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

static Cvar *net_showPackets;
static Cvar *net_showDrop;

NetAddr netFrom;
MemBuf netMessage;
static byte netMessageBuffer[MAX_MSG_SIZE];

/**
 * @brief Sends an out-of-band datagram
 */
void Netchan_OutOfBand(int32_t sock, const NetAddr *addr, const void *data, size_t len) {
  MemBuf send;
  byte sendBuffer[MAX_MSG_SIZE];

  // write the packet header
  Mem_InitBuffer(&send, sendBuffer, sizeof(sendBuffer));

  Net_WriteLong(&send, -1); // -1 sequence means out of band
  Mem_WriteBuffer(&send, data, len);

  // send the datagram
  Net_SendDatagram(sock, addr, send.data, send.size);
}

/**
 * @brief Sends a text message in an out-of-band datagram
 */
void Netchan_OutOfBandPrint(int32_t sock, const NetAddr *addr, const char *format, ...) {
  va_list args;
  char string[MAX_MSG_SIZE - 4];

  memset(string, 0, sizeof(string));

  va_start(args, format);
  vsnprintf(string, sizeof(string), format, args);
  va_end(args);

  Netchan_OutOfBand(sock, addr, (const void *) string, q_strlen(string));
}

/**
 * @brief Called to open a channel to a remote system.
 */
void Netchan_Setup(NetSrc source, NetChan *chan, NetAddr *addr, uint8_t qport) {

  memset(chan, 0, sizeof(*chan));

  chan->source = source;
  chan->remoteAddress = *addr;
  chan->qport = qport;

  chan->lastReceived = quetoo.ticks;
  chan->incomingSequence = 0;
  chan->outgoingSequence = 1;

  Mem_InitBuffer(&chan->message, chan->messageBuffer, sizeof(chan->messageBuffer));
}

/**
 * @return True if reliable data must be transmitted this frame, false
 * otherwise.
 */
static bool Netchan_CheckRetransmit(NetChan *chan) {

  // if the remote side dropped the last reliable message, re-send it
  if (chan->incomingAcknowledged > chan->reliableOutgoing && chan->reliableAcknowledged
          != chan->reliableSequence) {
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
void Netchan_Transmit(NetChan *chan, byte *data, size_t len) {
  MemBuf send;
  byte sendBuffer[MAX_MSG_SIZE];

  // check for re-transmission of reliable message
  bool sendReliable = Netchan_CheckRetransmit(chan);

  // or for transmission of a new one
  if (!chan->reliableSize && chan->message.size) {
    memcpy(chan->reliableBuffer, chan->messageBuffer, chan->message.size);
    chan->reliableSize = chan->message.size;
    chan->message.size = 0;
    chan->reliableSequence ^= 1;
    sendReliable = true;
  }

  // write the packet header
  Mem_InitBuffer(&send, sendBuffer, sizeof(sendBuffer));

  const uint32_t w1 = (chan->outgoingSequence & ~(1u << 31)) | ((uint32_t) sendReliable << 31);
  const uint32_t w2 = (chan->incomingSequence & ~(1u << 31)) | ((uint32_t) chan->reliableIncoming << 31);

  chan->outgoingSequence++;
  chan->lastSent = quetoo.ticks;

  Net_WriteLong(&send, w1);
  Net_WriteLong(&send, w2);

  // send the qport if we are a client
  if (chan->source == NS_UDP_CLIENT) {
    Net_WriteByte(&send, chan->qport);
  }

  // copy the reliable message to the packet first
  if (sendReliable) {
    Mem_WriteBuffer(&send, chan->reliableBuffer, chan->reliableSize);
    chan->reliableOutgoing = chan->outgoingSequence;
  }

  // add the unreliable part if space is available
  if (send.maxSize - send.size >= len) {
    Mem_WriteBuffer(&send, data, len);
  } else {
    Com_Warn("Netchan_Transmit: dumped unreliable\n");
  }

  // send the datagram
  Net_SendDatagram(chan->source, &chan->remoteAddress, send.data, send.size);

  if (net_showPackets->value) {
    if (sendReliable)
      Com_Print("Send %u bytes: s=%i reliable=%i ack=%i rack=%i\n", (uint32_t) send.size,
                chan->outgoingSequence - 1, chan->reliableSequence, chan->incomingSequence,
                chan->reliableIncoming);
    else
      Com_Print("Send %u bytes : s=%i ack=%i rack=%i\n", (uint32_t) send.size,
                chan->outgoingSequence - 1, chan->incomingSequence, chan->reliableIncoming);
  }
}

/**
 * @brief Called when the current `netMessage` is from `remoteAddress`
 * modifies `netMessage` so that it points to the packet payload
 */
bool Netchan_Process(NetChan *chan, MemBuf *msg) {
  uint32_t sequence, sequenceAck;
  uint32_t reliableAck, reliableMessage;

  // get sequence numbers
  Net_BeginReading(msg);

  sequence = Net_ReadLong(msg);
  sequenceAck = Net_ReadLong(msg);

  // read the qport if we are a server
  if (chan->source == NS_UDP_SERVER) {
    Net_ReadByte(msg);
  }

  reliableMessage = sequence >> 31u;
  reliableAck = sequenceAck >> 31u;

  sequence &= ~(1u << 31);
  sequenceAck &= ~(1u << 31);

  if (net_showPackets->value) {
    if (reliableMessage)
      Com_Print("Recv %u bytes: s=%i reliable=%i ack=%i rack=%i\n", (uint32_t) msg->size,
                sequence, chan->reliableIncoming ^ 1, sequenceAck, reliableAck);
    else
      Com_Print("Recv %u bytes : s=%i ack=%i rack=%i\n", (uint32_t) msg->size, sequence,
                sequenceAck, reliableAck);
  }

  // discard stale or duplicated packets
  if (sequence <= chan->incomingSequence) {
    if (net_showDrop->value)
      Com_Print("%s:Out of order packet %i at %i\n",
                Net_NetaddrToString(&chan->remoteAddress), sequence, chan->incomingSequence);
    return false;
  }

  // dropped packets don't keep the message from being used
  chan->dropped = sequence - (chan->incomingSequence + 1);
  if (chan->dropped > 0) {
    if (net_showDrop->value)
      Com_Print("%s:Dropped %i packets at %i\n", Net_NetaddrToString(&chan->remoteAddress),
                chan->dropped, sequence);
  }

  // if the current outgoing reliable message has been acknowledged
  // clear the buffer to make way for the next
  if (reliableAck == chan->reliableSequence) {
    chan->reliableSize = 0;    // it has been received
  }

  // if this message contains a reliable message, bump reliableIncoming
  chan->incomingSequence = sequence;
  chan->incomingAcknowledged = sequenceAck;
  chan->reliableAcknowledged = reliableAck;
  if (reliableMessage) {
    chan->reliableIncoming ^= 1;
  }

  // the message can now be read from the current message pointer
  chan->lastReceived = quetoo.ticks;

  return true;
}

/**
 * @brief Initializes the network channel subsystem, the global message buffer, and debug cvars.
 */
void Netchan_Init(void) {

  Net_Init();

  net_showPackets = Cvar_Add("net_showPackets", "0", 0, NULL);
  net_showDrop = Cvar_Add("net_showDrop", "0", 0, NULL);

  Mem_InitBuffer(&netMessage, netMessageBuffer, sizeof(netMessageBuffer));
}

/**
 * @brief Shuts down the network channel subsystem and releases all socket resources.
 */
void Netchan_Shutdown(void) {

  Net_Config(NS_UDP_CLIENT, false);
  Net_Config(NS_UDP_SERVER, false);

  Net_Shutdown();
}

