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
 #include <WinSock2.h> // for htons
#endif

#include "sv_local.h"

#define HEARTBEAT_SECONDS 300

/**
 * @brief Sends heartbeat messages to master servers every 300s.
 */
void Sv_HeartbeatMasters(void) {
	const char *string;
	int32_t i;

	if (!dedicated->value) {
		return;    // only dedicated servers report to masters
	}

	if (!sv_public->value) {
		return;    // a private dedicated game
	}

	if (!svs.initialized) { // we're not up yet
		return;
	}

	if (svs.next_heartbeat > quetoo.ticks) {
		return;    // not time to send yet
	}

	svs.next_heartbeat = quetoo.ticks + HEARTBEAT_SECONDS * 1000;

	// send the same string that we would give for a status command
	string = Sv_StatusString();

	// send to each master server
	for (i = 0; i < MAX_MASTERS; i++) {
		if (svs.masters[i].port) {
			Com_Print("Sending heartbeat to %s\n", Net_NetaddrToString(&svs.masters[i]));
			Netchan_OutOfBandPrint(NS_UDP_SERVER, &svs.masters[i], "heartbeat\n%s", string);
		}
	}
}

/**
 * @brief
 */
void Sv_InitMasters(void) {

	memset(&svs.masters, 0, sizeof(svs.masters));

	// set default master server
	Net_StringToNetaddr(HOST_MASTER, &svs.masters[0]);
	svs.masters[0].port = htons(PORT_MASTER);
}

/**
 * @brief Informs master servers that this server is halting.
 */
void Sv_ShutdownMasters(void) {
	int32_t i;

	if (!dedicated->value) {
		return;    // only dedicated servers send heartbeats
	}

	if (!sv_public->value) {
		return;    // a private dedicated game
	}

	// send to group master
	for (i = 0; i < MAX_MASTERS; i++) {
		if (svs.masters[i].port) {
			Com_Print("Sending shutdown to %s\n", Net_NetaddrToString(&svs.masters[i]));
			Netchan_OutOfBandPrint(NS_UDP_SERVER, &svs.masters[i], "shutdown");
		}
	}
}
