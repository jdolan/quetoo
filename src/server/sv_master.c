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

#if defined(_WIN32)
  #include <winsock2.h> // for htons
#endif

#include "sv_local.h"

#define HEARTBEAT_SECONDS 5

/**
 * @brief Sends a heartbeat to the master server every 5s, echoing whatever
 * challenge it last issued so that it will list us.
 */
void Sv_HeartbeatMaster(void) {

  if (!sv_public->value) {
    return; // a private dedicated game
  }

  if (!svs.master.addr.port) {
    return; // no master configured
  }

  if (svs.state != SV_ACTIVE_GAME) { // we're not up yet
    return;
  }

  if (svs.next_heartbeat > quetoo.ticks) {
    return; // not time to send yet
  }

  svs.next_heartbeat = quetoo.ticks + HEARTBEAT_SECONDS * 1000;

  Com_Debug(DEBUG_SERVER, "Sending heartbeat to %s\n", Net_NetaddrToString(&svs.master.addr));

  // send the same string that we would give for a status command
  Netchan_OutOfBandPrint(NS_UDP_SERVER, &svs.master.addr, "heartbeat %u\n%s",
                         svs.master.challenge, Sv_StatusString());
}

/**
 * @brief Records the challenge issued by the master server, to be echoed in our
 * heartbeats until it lists us.
 */
void Sv_Challenge(const net_addr_t *from, uint32_t challenge) {

  if (!challenge) {
    Com_Debug(DEBUG_SERVER, "Empty challenge from %s\n", Net_NetaddrToString(from));
    return; // the master never issues zero, so this can only be noise or forgery
  }

  if (!svs.master.addr.port || !Net_CompareNetaddr(from, &svs.master.addr)) {
    Com_Debug(DEBUG_SERVER, "Challenge from %s, which is not our master\n",
              Net_NetaddrToString(from));
    return;
  }

  svs.master.challenge = challenge;

  // answer at once, but throttled, so that a flood of forged challenges
  // cannot turn us into a heartbeat flood aimed at the master
  if (quetoo.ticks >= svs.master.challenge_time) {
    svs.master.challenge_time = quetoo.ticks + 1000;
    svs.next_heartbeat = 0;
  }
}

/**
 * @brief Resolves the master server named by `sv_master`.
 */
void Sv_InitMaster(void) {

  memset(&svs.master, 0, sizeof(svs.master));

  if (!q_strlen(sv_master->string)) {
    Com_Print("Master server disabled\n");
    return;
  }

  if (!Net_StringToNetaddr(sv_master->string, &svs.master.addr)) {
    Com_Warn("Failed to resolve master server %s\n", sv_master->string);
    return;
  }

  if (!svs.master.addr.port) {
    svs.master.addr.port = htons(PORT_MASTER);
  }

  svs.master.addr.type = NA_DATAGRAM;

  Com_Print("Master server at %s\n", Net_NetaddrToString(&svs.master.addr));
}

/**
 * @brief Informs the master server that this server is halting.
 */
void Sv_ShutdownMaster(void) {

  if (!sv_public->value) {
    return;    // a private dedicated game
  }

  if (!svs.master.addr.port) {
    return; // no master configured
  }

  Com_Print("Sending shutdown to %s\n", Net_NetaddrToString(&svs.master.addr));
  Netchan_OutOfBandPrint(NS_UDP_SERVER, &svs.master.addr, "shutdown %u", svs.master.challenge);
}
