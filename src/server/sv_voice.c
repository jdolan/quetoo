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

Cvar *sv_voice;
Cvar *sv_voice_rate;

/**
 * @brief Mutes or unmutes a speaker for one listener.
 * @details Filtering at the source means a muted player's audio is never relayed, so muting saves
 * the listener the bandwidth rather than merely the annoyance.
 */
void Sv_MuteVoice(const GameClient *listener, const GameClient *speaker, bool mute) {

  if (!listener || !speaker) {
    return;
  }

  ServerClient *cl = svs.clients + listener->ps.client;
  const uint64_t bit = (uint64_t) 1 << speaker->ps.client;

  if (mute) {
    cl->voiceMutes |= bit;
  } else {
    cl->voiceMutes &= ~bit;
  }
}

/**
 * @brief Forgets every mute involving the given client.
 * @details Client numbers are reused, so a mute left behind would silence whoever takes the slot
 * next, and would follow the muted player back in when they reconnect.
 */
void Sv_ClearVoiceMutes(const ServerClient *client) {

  const int32_t num = (int32_t) (client - svs.clients);
  const uint64_t bit = (uint64_t) 1 << num;

  ServerClient *cl = svs.clients;
  for (int32_t i = 0; i < sv_max_clients->integer; i++, cl++) {
    cl->voiceMutes &= ~bit;
  }

  svs.clients[num].voiceMutes = 0;
}

/**
 * @brief Charges a client's voice budget, returning false once it is spent.
 * @details A client on a poor connection bursting is not an attacker, so an exhausted budget
 * discards the frame rather than dropping the client. The bucket refills at sv_voice_rate and is
 * never allowed to bank more than a second of it.
 */
static bool Sv_ChargeVoice(ServerClient *cl, int32_t bytes) {

  const int32_t rate = Maxi(sv_voice_rate->integer, 0);

  if (!rate) {
    return false;
  }

  if (cl->voiceTime) {
    cl->voiceBytes += (int32_t) ((quetoo.ticks - cl->voiceTime) * rate / 1000);
    cl->voiceBytes = Mini(cl->voiceBytes, rate);
  } else {
    cl->voiceBytes = rate;
  }

  cl->voiceTime = quetoo.ticks;

  if (cl->voiceBytes < bytes) {
    return false;
  }

  cl->voiceBytes -= bytes;
  return true;
}

/**
 * @brief Relays one voice frame to whoever the game says may hear it.
 * @details The payload is never decoded here: the server has no use for the audio, and leaving Opus
 * out of the dedicated server keeps it dependency-light. Who may hear a channel is the game's to
 * say, beside the rules that already govern chat, rather than a client's to propose.
 */
static void Sv_RelayVoice(const ServerClient *from, uint8_t channel, uint8_t seq, uint8_t flags,
                          const byte *data, int32_t len) {

  const int32_t speaker = (int32_t) (from - svs.clients);

  MemBuf buf;
  byte bytes[VOICE_MAX_PAYLOAD + 32]; // command, speaker, seq, flags, length

  Mem_InitBuffer(&buf, bytes, sizeof(bytes));

  Net_WriteByte(&buf, SV_CMD_VOICE);
  Net_WriteByte(&buf, speaker);
  Net_WriteByte(&buf, seq);
  Net_WriteByte(&buf, flags);
  Net_WriteByte(&buf, len);
  Net_WriteData(&buf, data, len);

  ServerClient *cl = svs.clients;
  for (int32_t i = 0; i < sv_max_clients->integer; i++, cl++) {

    if (cl == from || cl->state != SV_CLIENT_ACTIVE) {
      continue;
    }

    if (!cl->gclient || cl->gclient->ai) {
      continue;
    }

    if (cl->voiceMutes & ((uint64_t) 1 << speaker)) {
      continue;
    }

    if (!svs.game->ClientCanHearVoice(from->gclient, cl->gclient, channel)) {
      continue;
    }

    Sv_ClientDatagramMessage(cl, buf.data, buf.size);
  }
}

/**
 * @brief Parses a voice frame from a client, validating and relaying it.
 */
void Sv_ParseVoice(ServerClient *cl) {

  const uint8_t channel = Net_ReadByte(&net_message);
  const uint8_t seq = Net_ReadByte(&net_message);
  const uint8_t flags = Net_ReadByte(&net_message);
  const int32_t len = Net_ReadByte(&net_message);

  if (len <= 0 || len > VOICE_MAX_PAYLOAD) {
    Com_Warn("Bad voice frame of %d bytes from %s\n", len, Sv_NetaddrToString(cl));
    Sv_DropClient(cl);
    return;
  }

  if (net_message.read + (size_t) len > net_message.size) {
    Com_Warn("Truncated voice frame from %s\n", Sv_NetaddrToString(cl));
    Sv_DropClient(cl);
    return;
  }

  byte data[VOICE_MAX_PAYLOAD];
  Net_ReadData(&net_message, data, len);

  if (!sv_voice->integer || cl->state != SV_CLIENT_ACTIVE) {
    return;
  }

  if (!Sv_ChargeVoice(cl, len)) {
    Com_Debug(DEBUG_SERVER, "Voice budget exceeded for %s\n", Sv_NetaddrToString(cl));
    return;
  }

  Sv_RelayVoice(cl, channel, seq, flags, data, len);
}

/**
 * @brief Initializes the server side of voice chat.
 */
void Sv_InitVoice(void) {

  sv_voice = Cvar_Add("sv_voice", "1", CVAR_SERVER_INFO, "Enables voice chat relaying on this server");
  sv_voice_rate = Cvar_Add("sv_voice_rate", "4000", 0, "The per-client voice chat budget, in bytes per second");
}
