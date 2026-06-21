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

#include "sv_local.h"
#include "common/demo.h"

/**
 * @brief Sends text across to be displayed if the level filter passes.
 */
void Sv_ClientPrint(const g_client_t *cl, const int32_t level, const char *fmt, ...) {

  if (cl->ai) {
    return;
  }

  sv_client_t *client = svs.clients + cl->ps.client;

  if (client->state != SV_CLIENT_ACTIVE) {
    Com_Debug(DEBUG_SERVER, "Issued to unspawned client\n");
    return;
  }

  if (level < client->message_level) {
    Com_Debug(DEBUG_SERVER, "Filtered by message level\n");
    return;
  }

  va_list args;
  va_start(args, fmt);

  char string[MAX_STRING_CHARS];
  vsnprintf(string, sizeof(string), fmt, args);

  va_end(args);

  Net_WriteByte(&client->net_chan.message, SV_CMD_PRINT);
  Net_WriteByte(&client->net_chan.message, level);
  Net_WriteString(&client->net_chan.message, string);
}

/**
 * @brief Sends text to all active clients over their unreliable channels.
 */
void Sv_BroadcastPrint(const int32_t level, const char *fmt, ...) {

  va_list args;
  va_start(args, fmt);

  char string[MAX_STRING_CHARS];
  vsnprintf(string, sizeof(string), fmt, args);

  va_end(args);

  // echo to console
  if (dedicated->value) {
    char copy[MAX_STRING_CHARS];
    int32_t j;

    // mask off high bits
    for (j = 0; j < MAX_STRING_CHARS - 1 && string[j]; j++) {
      copy[j] = string[j] & 127;
    }
    copy[j] = 0;
    Com_Print("%s", copy);
  }

  sv_client_t *cl = svs.clients;
  for (int32_t i = 0; i < sv_max_clients->integer; i++, cl++) {

    if (level < cl->message_level) {
      continue;
    }

    if (cl->state != SV_CLIENT_ACTIVE) {
      continue;
    }

    if (cl->gclient->ai) {
      continue;
    }

    Net_WriteByte(&cl->net_chan.message, SV_CMD_PRINT);
    Net_WriteByte(&cl->net_chan.message, level);
    Net_WriteString(&cl->net_chan.message, string);
  }
}

/**
 * @brief Sends text to all active clients
 */
void Sv_BroadcastCommand(const char *fmt, ...) {
  char string[MAX_STRING_CHARS];
  va_list args;

  if (svs.state == SV_UNINITIALIZED) {
    return;
  }

  va_start(args, fmt);
  vsnprintf(string, sizeof(string), fmt, args);
  va_end(args);

  Net_WriteByte(&sv.multicast, SV_CMD_CBUF_TEXT);
  Net_WriteString(&sv.multicast, string);
  Sv_Multicast(Vec3_Zero(), MULTICAST_ALL_R);
}

/**
 * @brief Writes to the specified datagram, noting the offset of the message.
 */
static void Sv_ClientDatagramMessage(sv_client_t *cl, byte *data, size_t len) {

  // Ensure the datagram buffer is initialized
  if (!cl->datagram.buffer.data) {
    Mem_InitBuffer(&cl->datagram.buffer, cl->datagram.data, sizeof(cl->datagram.data));
    cl->datagram.buffer.allow_overflow = true;
  }

  if (len > MAX_MSG_SIZE_UDP) {
    Com_Debug(DEBUG_SERVER, "Single datagram message exceeds MAX_MSG_SIZE_UDP\n");

    if (len > MAX_MSG_SIZE) {
      Com_Error(ERROR_DROP, "Single datagram message exceeded MAX_MSG_SIZE\n");
    }
  }

  sv_client_message_t *msg = Mem_TagMalloc(sizeof(*msg), MEM_TAG_SERVER);

  msg->offset = cl->datagram.buffer.size;
  msg->len = len;

  if (!cl->datagram.messages) {
    cl->datagram.messages = $(alloc(List), init);
    cl->datagram.messages->destroy = (ListDestroyFunc) Mem_Free;
  }
  $(cl->datagram.messages, appendElement, msg);

  Mem_WriteBuffer(&cl->datagram.buffer, data, len);

  if (cl->datagram.buffer.overflowed) {
    Com_Warn("Client datagram overflow for %s\n", cl->name);

    msg->offset = 0;
    cl->datagram.buffer.overflowed = false;

    $(cl->datagram.messages, removeAll); release(cl->datagram.messages); cl->datagram.messages = NULL;
    cl->datagram.messages = NULL;
  }
}

/**
 * @brief Sends the contents of the mutlicast buffer to a single client.
 */
void Sv_Unicast(const g_client_t *cl, const bool reliable) {

  if (cl->ai) {
    Mem_ClearBuffer(&sv.multicast);
    return;
  }

  sv_client_t *client = svs.clients + cl->ps.client;

  if (reliable) {
    Mem_WriteBuffer(&client->net_chan.message, sv.multicast.data, sv.multicast.size);
  } else {
    Sv_ClientDatagramMessage(client, sv.multicast.data, sv.multicast.size);
  }

  Mem_ClearBuffer(&sv.multicast);
}

/**
 * @brief Sends the contents of `sv.multicast` to a subset of the clients,
 * then clears `sv.multicast`.
 */
void Sv_Multicast(const vec3_t origin, multicast_t to) {

  bool reliable = false;

  switch (to) {
    case MULTICAST_ALL_R:
      reliable = true;
      __attribute__((fallthrough));
    case MULTICAST_ALL:
      break;

    case MULTICAST_PHS_R:
      reliable = true;
      __attribute__((fallthrough));
    case MULTICAST_PHS:
      break;

    case MULTICAST_PVS_R:
      reliable = true;
      __attribute__((fallthrough));
    case MULTICAST_PVS:
      break;

    default:
      Com_Warn("Bad multicast: %i\n", to);
      Mem_ClearBuffer(&sv.multicast);
      return;
  }

  // send the data to all relevant clients
  sv_client_t *cl = svs.clients;
  for (int32_t j = 0; j < sv_max_clients->integer; j++, cl++) {

    if (cl->state == SV_CLIENT_FREE) {
      continue;
    }

    if (cl->net_chan.message.max_size == 0) {
      continue;
    }

    if (cl->state != SV_CLIENT_ACTIVE && !reliable) {
      continue;
    }

    if (svs.clients[j].gclient->ai) {
      continue;
    }

    if (to != MULTICAST_ALL && to != MULTICAST_ALL_R) {
      // TODO: Some basic tracing or just distance attenuation?
    }

    if (reliable) {
      Mem_WriteBuffer(&cl->net_chan.message, sv.multicast.data, sv.multicast.size);
    } else {
      Sv_ClientDatagramMessage(cl, sv.multicast.data, sv.multicast.size);
    }
  }

  Mem_ClearBuffer(&sv.multicast);
}

/**
 * @brief Builds and transmits the current frame packet to the specified client.
 */
static void Sv_SendClientDatagram(sv_client_t *cl) {
  byte buffer[MAX_MSG_SIZE];
  mem_buf_t buf;

  Sv_BuildClientFrame(cl);

  Mem_InitBuffer(&buf, buffer, sizeof(buffer));
  buf.allow_overflow = true;

  // send over all the relevant entity_state_t and the player_state_t
  Sv_WriteClientFrame(cl, &buf);

  // the frame itself (player state and delta entities) must fit into a single message,
  // since it is parsed as a single command by the client
  if (buf.overflowed || buf.size > MAX_MSG_SIZE - 16) {
    Com_Error(ERROR_DROP, "Frame exceeds MAX_MSG_SIZE (%u)\n", (uint32_t) buf.size);
  }

  // but we can packetize the remaining datagram messages, which are parsed individually
  if (cl->datagram.messages) {
    for (const ListNode *node = cl->datagram.messages->head; node; node = node->next) {
      const sv_client_message_t *msg = (const sv_client_message_t *) node->element;

      // if we would overflow the packet, flush it first
      if (buf.size + msg->len > (MAX_MSG_SIZE - 16)) {
        Com_Debug(DEBUG_SERVER, "Fragmenting datagram @ %u bytes\n", (uint32_t) buf.size);

        Netchan_Transmit(&cl->net_chan, buf.data, buf.size);

        Mem_ClearBuffer(&buf);
      }

      Mem_WriteBuffer(&buf, cl->datagram.buffer.data + msg->offset, msg->len);
    }
  }

  // send the pending packet, which may include reliable messages
  Netchan_Transmit(&cl->net_chan, buf.data, buf.size);
}

/**
 * @brief Advances to the next demo in the playlist or restarts from the beginning.
 */
static void Sv_DemoCompleted(void) {

  if (sv_demo_list->string[0]) {

    const char *current_demo = sv.name;
    const char *next_demo = q_strstr(sv_demo_list->string, current_demo);
    char demo_token[MAX_QPATH];

    if (!next_demo) {

      next_demo = sv_demo_list->string;
    } else {

      next_demo += q_strlen(current_demo);

      if (next_demo[0] == ' ') {
        next_demo++;
      } else if (!next_demo[0]) {
        next_demo = sv_demo_list->string;
      }
    }

    const char *space = q_strchr(next_demo, ' ') ? : (next_demo + q_strlen(next_demo));
    size_t len = space - next_demo;

    q_strlcpy(demo_token, next_demo, len + 1);

    if (demo_token[0]) {
      Sv_InitServer(demo_token, NULL, SV_ACTIVE_DEMO);
    } else {
      Sv_ShutdownServer("Demo complete\n");
    }
  } else {
    Sv_ShutdownServer("Demo complete\n");
  }
}

/**
 * @brief Reads the next frame from the current demo file into the specified buffer,
 * returning the size of the frame in bytes.
 *
 * FIXME: This doesn't work with the new packetized overflow avoidance. Multiple
 * messages can constitute a frame. We need a mechanism to indicate frame
 * completion, or we need a timecode in our demos.
 */
static size_t Sv_GetDemoMessage(byte *buffer) {
  int32_t size;
  int64_t r;

  r = Fs_Read(sv.demo_file, &size, sizeof(size), 1);

  if (r != 1) { // improperly terminated demo file
    Com_Warn("Failed to read demo file\n");
    Sv_DemoCompleted();
    return 0;
  }

  size = LittleLong(size);

  if (size == -1) { // properly terminated demo file
    Sv_DemoCompleted();
    return 0;
  }

  if (size > MAX_MSG_SIZE) { // corrupt demo file
    Com_Warn("%d > MAX_MSG_SIZE\n", size);
    Sv_DemoCompleted();
    return 0;
  }

  r = Fs_Read(sv.demo_file, buffer, size, 1);

  if (r != 1) {
    Com_Warn("Incomplete or corrupt demo file\n");
    Sv_DemoCompleted();
    return 0;
  }

  return size;
}

/**
 * @brief Largest number of demo records transmitted to a client per server tick.
 * A seek makes a large backlog of records due at once (the keyframe plus every
 * frame up to the target); transmitting them all at once would overrun the
 * loopback receive ring (MAX_NET_UDP_LOOPS == 64), silently dropping frames and
 * breaking the delta chain ("Delta frame too old"). Capping the burst spreads
 * the catch-up over a few ticks -- a brief, smooth fast-forward -- while keeping
 * every frame in order so the chain never breaks.
 *
 * Sv_Frame may run up to 4 ticks before the client drains the ring once (and the
 * client can skip draining entirely under its frame-rate cap), so the bound is
 * 4 * (SV_DEMO_MAX_BURST + 1 status) records between drains. 12 keeps that at 52,
 * safely under the 64-slot ring for any realistic frame rate.
 */
#define SV_DEMO_MAX_BURST 12

/**
 * @brief Transmits the v2 demo records whose timecode has been reached by the
 * demo clock, reframing each as the client expects. The records are the exact
 * v1 wire messages, so the client parses them identically to live play. At most
 * SV_DEMO_MAX_BURST are sent per frame so a seek's backlog cannot overrun the
 * loopback ring.
 * @return False when the demo has completed (end of records).
 */
static bool Sv_SendDemoV2Records(sv_client_t *cl) {
  byte buffer[MAX_MSG_SIZE];
  demo_record_t rec;
  int32_t sent = 0;

  for (;;) {
    const int64_t offset = Fs_Tell(sv.demo_file);

    if (!Demo_ReadRecord(sv.demo_file, &rec, buffer, sizeof(buffer))) {
      Sv_DemoCompleted();
      return false;
    }

    if (rec.timecode > sv.demo_time) { // not due yet; rewind and wait
      Fs_Seek(sv.demo_file, offset);
      return true;
    }

    if (sent >= SV_DEMO_MAX_BURST) { // throttle the catch-up; resume next frame
      Fs_Seek(sv.demo_file, offset);
      return true;
    }

    Netchan_Transmit(&cl->net_chan, buffer, rec.length);
    sent++;
  }
}

/**
 * @brief Transmits the demo playback status (time, duration, paused, speed) to
 * the client as a config string, so the cgame can draw the timeline scrubber.
 * Sent explicitly here because the demo path does not flush the multicast.
 */
static void Sv_SendDemoStatus(sv_client_t *cl) {
  byte buffer[256];
  mem_buf_t msg;

  Mem_InitBuffer(&msg, buffer, sizeof(buffer));

  Net_WriteByte(&msg, SV_CMD_CONFIG_STRING);
  Net_WriteShort(&msg, CS_DEMO_STATUS);
  Net_WriteString(&msg, va("%u %u %d %.3f", sv.demo_time, sv.demo_index.duration,
                           sv.demo_paused ? 1 : 0, sv.demo_speed));

  Netchan_Transmit(&cl->net_chan, msg.data, msg.size);
}

/**
 * @brief Send the frame and all pending datagram messages since the last frame.
 */
void Sv_SendClientPackets(void) {

  if (svs.state == SV_UNINITIALIZED) {
    return;
  }

  // send a message to each connected client
  sv_client_t *cl = svs.clients;
  for (int32_t i = 0; i < sv_max_clients->integer; i++, cl++) {

    if (cl->state == SV_CLIENT_FREE) {
      continue;
    }

    if (svs.clients[i].gclient->ai) {
      continue;
    }

    // if the client's reliable message overflowed, we must drop them
    if (cl->net_chan.message.overflowed) {
      Sv_DropClient(cl);
      Sv_BroadcastPrint(PRINT_MEDIUM, "%s overflowed\n", cl->name);
      continue;
    }

    if (svs.state == SV_ACTIVE_DEMO) { // send the demo packet

      if (sv.demo_v2) {
        if (!Sv_SendDemoV2Records(cl)) {
          break; // demo complete
        }
        Sv_SendDemoStatus(cl);
      } else {
        byte buffer[MAX_MSG_SIZE];
        size_t size;

        if ((size = Sv_GetDemoMessage(buffer))) {
          Netchan_Transmit(&cl->net_chan, buffer, size);
        } else {
          break;    // recording is done, so we're done
        }
      }
    } else if (cl->state == SV_CLIENT_ACTIVE) { // send the game packet

      Sv_SendClientDatagram(cl);

      // clean up for the next frame
      Mem_ClearBuffer(&cl->datagram.buffer);

      if (cl->datagram.messages) {
        $(cl->datagram.messages, removeAll); release(cl->datagram.messages); cl->datagram.messages = NULL;
      }

      cl->datagram.messages = NULL;

    } else if (cl->net_chan.message.size) { // update reliable
      Netchan_Transmit(&cl->net_chan, NULL, 0);
    } else if (quetoo.ticks - cl->net_chan.last_sent > 1000) { // or just don't timeout
      Netchan_Transmit(&cl->net_chan, NULL, 0);
    }
  }
}
