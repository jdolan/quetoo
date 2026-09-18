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

cvar_t *sv_voice;
cvar_t *sv_voice_rate;

/**
 * @brief Charges a client's voice budget, returning false once it is spent.
 * @details A client on a poor connection bursting is not an attacker, so an exhausted budget
 * discards the frame rather than dropping the client. The bucket refills at sv_voice_rate and is
 * never allowed to bank more than a second of it.
 */
static bool Sv_ChargeVoice(sv_client_t *cl, int32_t bytes) {

  const int32_t rate = Maxi(sv_voice_rate->integer, 0);

  if (!rate) {
    return false;
  }

  if (cl->voice_time) {
    cl->voice_bytes += (int32_t) ((quetoo.ticks - cl->voice_time) * rate / 1000);
    cl->voice_bytes = Mini(cl->voice_bytes, rate);
  } else {
    cl->voice_bytes = rate;
  }

  cl->voice_time = quetoo.ticks;

  if (cl->voice_bytes < bytes) {
    return false;
  }

  cl->voice_bytes -= bytes;
  return true;
}

/**
 * @brief Relays one voice frame to the recipients its sender chose.
 * @details The payload is never decoded here: the server has no use for the audio, and leaving Opus
 * out of the dedicated server keeps it dependency-light. The sender's mask is intersected with
 * reality, and the sender is never echoed back to itself.
 */
static void Sv_RelayVoice(const sv_client_t *from, uint64_t recipients, uint8_t seq, uint8_t flags,
                          const byte *data, int32_t len) {

  const int32_t speaker = (int32_t) (from - svs.clients);

  mem_buf_t buf;
  byte bytes[VOICE_MAX_PAYLOAD + 16];

  Mem_InitBuffer(&buf, bytes, sizeof(bytes));

  if (!from->gclient || from->gclient->ai) {
    flags |= VOICE_NO_POS;
  }

  Net_WriteByte(&buf, SV_CMD_VOICE);
  Net_WriteByte(&buf, speaker);
  Net_WriteByte(&buf, seq);
  Net_WriteByte(&buf, flags);

  if (!(flags & VOICE_NO_POS)) {
    Net_WritePosition(&buf, from->gclient->ps.pm_state.origin);
  }

  Net_WriteByte(&buf, len);
  Net_WriteData(&buf, data, len);

  sv_client_t *cl = svs.clients;
  for (int32_t i = 0; i < sv_max_clients->integer; i++, cl++) {

    if (cl == from || cl->state != SV_CLIENT_ACTIVE) {
      continue;
    }

    if (cl->gclient && cl->gclient->ai) {
      continue;
    }

    if (!(recipients & ((uint64_t) 1 << i))) {
      continue;
    }

    Sv_ClientDatagramMessage(cl, buf.data, buf.size);
  }
}

/**
 * @brief Parses a voice frame from a client, validating and relaying it.
 */
void Sv_ParseVoice(sv_client_t *cl) {

  const uint32_t low = (uint32_t) Net_ReadLong(&net_message);
  const uint32_t high = (uint32_t) Net_ReadLong(&net_message);
  const uint8_t seq = Net_ReadByte(&net_message);
  const uint8_t flags = Net_ReadByte(&net_message);
  const int32_t len = Net_ReadByte(&net_message);

  if (len <= 0 || len > VOICE_MAX_PAYLOAD) {
    Com_Warn("Bad voice frame of %d bytes from %s\n", len, Sv_NetaddrToString(cl));
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

  const uint64_t recipients = ((uint64_t) high << 32) | low;

  Sv_RelayVoice(cl, recipients, seq, flags, data, len);
}

/**
 * @brief Initializes the server side of voice chat.
 */
void Sv_InitVoice(void) {

  sv_voice = Cvar_Add("sv_voice", "1", CVAR_SERVER_INFO, "Enables voice chat relaying on this server");
  sv_voice_rate = Cvar_Add("sv_voice_rate", "4000", 0, "The per-client voice chat budget, in bytes per second");
}
