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
 * 30  sequence
 * 1  is this datagram a fragment
 * 1  does this message contain a reliable payload
 * 31  acknowledge sequence
 * 1  acknowledge receipt of even/odd message
 * 8  qport, client to server only
 * 16  fragment offset, fragments only
 * 16  fragment length, fragments only
 *
 * A message to a remote address that does not fit in MAX_MSG_FRAGMENT_SIZE is
 * sent as fragments of MAX_MSG_FRAGMENT_PAYLOAD bytes, all with the same
 * sequence. A shorter fragment is the last one, so a message whose length is
 * an exact multiple of the payload ends with an empty fragment. The receiver
 * keeps one partial message, accepts its fragments in order only, and drops
 * it if one is lost. A lost fragment loses the message, as a lost datagram
 * does: the reliable part is retransmitted, the unreliable part is not.
 * Loopback messages are never fragmented, since they never reach the network.
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
NetMessage netMessage;

/**
 * @brief Sends an out-of-band datagram
 */
void NetChan_OutOfBand(int32_t sock, const NetAddr *addr, const void *data, size_t len) {
  MemBuf send;
  byte sendBuffer[MAX_MSG_SIZE_OOB];

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
void NetChan_OutOfBandPrint(int32_t sock, const NetAddr *addr, const char *format, ...) {
  va_list args;
  char string[MAX_MSG_SIZE_OOB - 4];

  memset(string, 0, sizeof(string));

  va_start(args, format);
  vsnprintf(string, sizeof(string), format, args);
  va_end(args);

  NetChan_OutOfBand(sock, addr, (const void *) string, Str_Length(string));
}

/**
 * @brief Called to open a channel to a remote system.
 */
void NetChan_Setup(NetSrc source, NetChan *chan, NetAddr *addr, uint8_t qport) {

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
static bool NetChan_CheckRetransmit(NetChan *chan) {

  // if the remote side dropped the last reliable message, re-send it
  if (chan->incomingAcknowledged > chan->reliableOutgoing && chan->reliableAcknowledged
          != chan->reliableSequence) {
    return true;
  }

  return false;
}

/**
 * @brief Writes the header and `length` bytes of `payload` at `offset` to one datagram, and sends it.
 * @param fragment True to send the bytes as a fragment, with its offset and length in the header.
 * @return True if the datagram was sent.
 */
static bool NetChan_Transmit_datagram(NetChan *chan, uint32_t w1, uint32_t w2, const MemBuf *payload,
                                      size_t offset, size_t length, bool fragment) {
  MemBuf send;
  byte sendBuffer[MAX_MSG_SIZE];

  Mem_InitBuffer(&send, sendBuffer, sizeof(sendBuffer));

  Net_WriteLong(&send, fragment ? w1 | NETCHAN_FRAGMENT_BIT : w1);
  Net_WriteLong(&send, w2);

  if (chan->source == NS_UDP_CLIENT) {
    Net_WriteByte(&send, chan->qport);
  }

  if (fragment) {
    Net_WriteShort(&send, (int32_t) offset);
    Net_WriteShort(&send, (int32_t) length);
  }

  Mem_WriteBuffer(&send, payload->data + offset, length);

  if (net_showPackets->value) {
    Com_Print("Send %u bytes: s=%i%s ack=%i rack=%i", (uint32_t) send.size,
              w1 & ~NETCHAN_RELIABLE_BIT, (w1 & NETCHAN_RELIABLE_BIT) ? " reliable" : "",
              w2 & ~NETCHAN_RELIABLE_BIT, w2 >> 31u);
    if (fragment) {
      Com_Print(" fragment=%u+%u", (uint32_t) offset, (uint32_t) length);
    }
    Com_Print("\n");
  }

  return Net_SendDatagram(chan->source, &chan->remoteAddress, send.data, send.size);
}

/**
 * @brief Tries to send an unreliable message to a connection, and handles the
 * transmission / retransmission of the reliable messages.
 *
 * A 0 size will still generate a packet and deal with the reliable messages.
 * A message to a remote address that does not fit in `MAX_MSG_FRAGMENT_SIZE` is sent as fragments.
 */
void NetChan_Transmit(NetChan *chan, byte *data, size_t len) {
  MemBuf payload;
  byte payloadBuffer[MAX_MSG_SIZE - 10];

  // check for re-transmission of reliable message
  bool sendReliable = NetChan_CheckRetransmit(chan);

  // or for transmission of a new one
  if (!chan->reliableSize && chan->message.size) {
    memcpy(chan->reliableBuffer, chan->messageBuffer, chan->message.size);
    chan->reliableSize = chan->message.size;
    chan->message.size = 0;
    chan->reliableSequence ^= 1;
    sendReliable = true;
  }

  const uint32_t w1 = (chan->outgoingSequence & ~(NETCHAN_RELIABLE_BIT | NETCHAN_FRAGMENT_BIT))
                    | (sendReliable ? NETCHAN_RELIABLE_BIT : 0);
  const uint32_t w2 = (chan->incomingSequence & ~NETCHAN_RELIABLE_BIT) | ((uint32_t) chan->reliableIncoming << 31);

  chan->outgoingSequence++;
  chan->lastSent = quetoo.ticks;

  Mem_InitBuffer(&payload, payloadBuffer, sizeof(payloadBuffer));

  // copy the reliable message to the packet first
  if (sendReliable) {
    Mem_WriteBuffer(&payload, chan->reliableBuffer, chan->reliableSize);
    chan->reliableOutgoing = chan->outgoingSequence;
  }

  // add the unreliable part if space is available
  if (payload.maxSize - payload.size >= len) {
    Mem_WriteBuffer(&payload, data, len);
  } else {
    Com_Warn("NetChan_Transmit: dumped unreliable\n");
  }

  const size_t header = chan->source == NS_UDP_CLIENT ? 9 : 8;

  if (chan->remoteAddress.type == NA_LOOP || header + payload.size <= MAX_MSG_FRAGMENT_SIZE) {
    NetChan_Transmit_datagram(chan, w1, w2, &payload, 0, payload.size, false);
    return;
  }

  for (size_t offset = 0; ; offset += MAX_MSG_FRAGMENT_PAYLOAD) {
    const size_t length = Minz(payload.size - offset, MAX_MSG_FRAGMENT_PAYLOAD);

    if (!NetChan_Transmit_datagram(chan, w1, w2, &payload, offset, length, true)) {
      break;
    }

    if (length < MAX_MSG_FRAGMENT_PAYLOAD) {
      break;
    }
  }
}

/**
 * @brief Appends the fragment in `msg` to the partial message of `chan`.
 * @return True if the fragment completed the message, which then replaces the contents of `msg`.
 */
static bool NetChan_Process_reassemble(NetChan *chan, NetMessage *msg, uint32_t sequence) {

  const size_t offset = (uint16_t) Net_ReadShort(msg);
  const size_t length = (uint16_t) Net_ReadShort(msg);

  if (msg->read + length > msg->size) {
    if (net_showDrop->value) {
      Com_Print("%s:Truncated fragment %i\n", Net_NetaddrToString(&chan->remoteAddress), sequence);
    }
    return false;
  }

  if (sequence < chan->fragmentSequence) {
    return false;
  }

  if (sequence > chan->fragmentSequence) {
    chan->fragmentSequence = sequence;
    chan->fragmentSize = 0;
  }

  if (offset != chan->fragmentSize || chan->fragmentSize + length > sizeof(chan->fragmentBuffer)) {
    if (net_showDrop->value) {
      Com_Print("%s:Dropped fragment %i at %u\n", Net_NetaddrToString(&chan->remoteAddress),
                sequence, (uint32_t) offset);
    }
    chan->fragmentSize = 0;
    return false;
  }

  memcpy(chan->fragmentBuffer + chan->fragmentSize, msg->data + msg->read, length);
  chan->fragmentSize += length;

  if (length == MAX_MSG_FRAGMENT_PAYLOAD) {
    return false;
  }

  memcpy(msg->data, chan->fragmentBuffer, chan->fragmentSize);
  msg->size = chan->fragmentSize;
  msg->read = 0;

  chan->fragmentSize = 0;
  return true;
}

/**
 * @brief Called when the current `netMessage` is from `remoteAddress`
 * modifies `netMessage` so that it points to the packet payload
 */
bool NetChan_Process(NetChan *chan, NetMessage *msg) {
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

  const bool fragment = sequence & NETCHAN_FRAGMENT_BIT;

  sequence &= ~(NETCHAN_RELIABLE_BIT | NETCHAN_FRAGMENT_BIT);
  sequenceAck &= ~NETCHAN_RELIABLE_BIT;

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

  if (fragment && !NetChan_Process_reassemble(chan, msg, sequence)) {
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
void NetChan_Init(void) {
  static byte buffer[MAX_MSG_SIZE];

  Net_Init();

  net_showPackets = Cvar_Add("net_showPackets", "0", 0, NULL);
  net_showDrop = Cvar_Add("net_showDrop", "0", 0, NULL);

  Mem_InitBuffer(&netMessage, buffer, sizeof(buffer));
}

/**
 * @brief Shuts down the network channel subsystem and releases all socket resources.
 */
void NetChan_Shutdown(void) {

  Net_Config(NS_UDP_CLIENT, false);
  Net_Config(NS_UDP_SERVER, false);

  Net_Shutdown();
}

