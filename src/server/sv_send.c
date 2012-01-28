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

#include "sv_local.h"

char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

/*
 * Sv_FlushRedirect
 *
 * Handles Com_Print output redirection, allowing the server to send output
 * from any command to a connected client or even a foreign one.
 */
void Sv_FlushRedirect(int target, char *outputbuf) {

	switch (target) {
	case RD_PACKET:
		Netchan_OutOfBandPrint(NS_SERVER, net_from, "print\n%s", outputbuf);
		break;
	case RD_CLIENT:
		Msg_WriteByte(&sv_client->netchan.message, SV_CMD_PRINT);
		Msg_WriteByte(&sv_client->netchan.message, PRINT_HIGH);
		Msg_WriteString(&sv_client->netchan.message, outputbuf);
		break;
	default:
		Com_Debug("Sv_FlushRedirect: %d\n", target);
		break;
	}
}

/*
 * Sv_ClientPrint
 *
 * Sends text across to be displayed if the level filter passes.
 */
void Sv_ClientPrint(g_edict_t *ent, int level, const char *fmt, ...) {
	sv_client_t *cl;
	va_list args;
	char string[MAX_STRING_CHARS];
	int n;

	n = NUM_FOR_EDICT(ent);
	if (n < 1 || n > sv_max_clients->integer) {
		Com_Warn("Sv_ClientPrint: Issued to non-client %d.\n", n);
		return;
	}

	cl = &svs.clients[n - 1];

	if (cl->state != SV_CLIENT_ACTIVE) {
		Com_Debug("Sv_ClientPrint: Issued to unspawned client.\n");
		return;
	}

	if (level < cl->message_level) {
		Com_Debug("Sv_ClientPrint: Filtered by message level.\n");
		return;
	}

	va_start(args, fmt);
	vsprintf(string, fmt, args);
	va_end(args);

	Msg_WriteByte(&cl->netchan.message, SV_CMD_PRINT);
	Msg_WriteByte(&cl->netchan.message, level);
	Msg_WriteString(&cl->netchan.message, string);
}

/*
 * Sv_ClientCenterPrint
 *
 * Center-print to a single client.  This is sent via Sv_Unicast so that it
 * is transmitted over the reliable channel; center-prints are important.
 */
void Sv_ClientCenterPrint(g_edict_t *ent, const char *fmt, ...) {
	char msg[1024];
	va_list args;
	int n;

	n = NUM_FOR_EDICT(ent);
	if (n < 1 || n > sv_max_clients->integer) {
		Com_Warn("Sv_ClientCenterPrint: ClientCenterPrintf to non-client.\n");
		return;
	}

	va_start(args, fmt);
	vsprintf(msg, fmt, args);
	va_end(args);

	Msg_WriteByte(&sv.multicast, SV_CMD_CENTER_PRINT);
	Msg_WriteString(&sv.multicast, msg);

	Sv_Unicast(ent, true);
}

/*
 * Sv_BroadcastPrint
 *
 * Sends text to all active clients over their unreliable channels.
 */
void Sv_BroadcastPrint(int level, const char *fmt, ...) {
	va_list args;
	char string[MAX_STRING_CHARS];
	sv_client_t *cl;
	int i;

	va_start(args, fmt);
	vsprintf(string, fmt, args);
	va_end(args);

	// echo to console
	if (dedicated->value) {
		char copy[MAX_STRING_CHARS];
		int j;

		// mask off high bits
		for (j = 0; j < MAX_STRING_CHARS - 1 && string[j]; j++)
			copy[j] = string[j] & 127;
		copy[j] = 0;
		Com_Print("%s", copy);
	}

	for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++) {

		if (level < cl->message_level)
			continue;

		if (cl->state != SV_CLIENT_ACTIVE)
			continue;

		Msg_WriteByte(&cl->netchan.message, SV_CMD_PRINT);
		Msg_WriteByte(&cl->netchan.message, level);
		Msg_WriteString(&cl->netchan.message, string);
	}
}

/*
 * Sv_BroadcastCommand
 *
 * Sends text to all active clients
 */
void Sv_BroadcastCommand(const char *fmt, ...) {
	va_list args;
	char string[MAX_STRING_CHARS];

	if (!sv.state)
		return;
	va_start(args, fmt);
	vsprintf(string, fmt, args);
	va_end(args);

	Msg_WriteByte(&sv.multicast, SV_CMD_CBUF_TEXT);
	Msg_WriteString(&sv.multicast, string);
	Sv_Multicast(NULL, MULTICAST_ALL_R);
}

/*
 * Sv_Unicast
 *
 * Sends the contents of the mutlicast buffer to a single client
 */
void Sv_Unicast(g_edict_t *ent, boolean_t reliable) {
	int n;
	sv_client_t *cl;

	if (!ent)
		return;

	n = NUM_FOR_EDICT(ent);
	if (n < 1 || n > sv_max_clients->integer)
		return;

	cl = svs.clients + (n - 1);

	if (reliable)
		Sb_Write(&cl->netchan.message, sv.multicast.data, sv.multicast.size);
	else
		Sb_Write(&cl->datagram, sv.multicast.data, sv.multicast.size);

	Sb_Clear(&sv.multicast);
}

/*
 * Sv_Multicast
 *
 * Sends the contents of sv.multicast to a subset of the clients,
 * then clears sv.multicast.
 *
 * MULTICAST_ALL	same as broadcast (origin can be NULL)
 * MULTICAST_PVS	send to clients potentially visible from org
 * MULTICAST_PHS	send to clients potentially hearable from org
 */
void Sv_Multicast(vec3_t origin, multicast_t to) {
	sv_client_t *client;
	byte *mask;
	int leaf_num, cluster;
	int j;
	boolean_t reliable;
	int area1, area2;

	reliable = false;

	if (to != MULTICAST_ALL_R && to != MULTICAST_ALL) {
		leaf_num = Cm_PointLeafnum(origin);
		area1 = Cm_LeafArea(leaf_num);
	} else {
		leaf_num = 0; // just to avoid compiler warnings
		area1 = 0;
	}

	switch (to) {
	case MULTICAST_ALL_R:
		reliable = true; // intentional fallthrough
	case MULTICAST_ALL:
		leaf_num = 0;
		mask = NULL;
		break;

	case MULTICAST_PHS_R:
		reliable = true; // intentional fallthrough
	case MULTICAST_PHS:
		leaf_num = Cm_PointLeafnum(origin);
		cluster = Cm_LeafCluster(leaf_num);
		mask = Cm_ClusterPHS(cluster);
		break;

	case MULTICAST_PVS_R:
		reliable = true; // intentional fallthrough
	case MULTICAST_PVS:
		leaf_num = Cm_PointLeafnum(origin);
		cluster = Cm_LeafCluster(leaf_num);
		mask = Cm_ClusterPVS(cluster);
		break;

	default:
		Com_Warn("Sv_Multicast: bad multicast: %i.\n", to);
		return;
	}

	// send the data to all relevent clients
	for (j = 0, client = svs.clients; j < sv_max_clients->integer; j++, client++) {

		if (client->state == SV_CLIENT_FREE)
			continue;

		if (client->state != SV_CLIENT_ACTIVE && !reliable)
			continue;

		if (mask) {
			leaf_num = Cm_PointLeafnum(client->edict->s.origin);
			cluster = Cm_LeafCluster(leaf_num);
			area2 = Cm_LeafArea(leaf_num);
			if (!Cm_AreasConnected(area1, area2))
				continue;
			if (mask && (!(mask[cluster >> 3] & (1 << (cluster & 7)))))
				continue;
		}

		if (reliable)
			Sb_Write(&client->netchan.message, sv.multicast.data,
					sv.multicast.size);
		else
			Sb_Write(&client->datagram, sv.multicast.data, sv.multicast.size);
	}

	Sb_Clear(&sv.multicast);
}

/*
 * Sv_PositionedSound
 *
 * FIXME: if entity isn't in PHS, they must be forced to be sent or
 * have the origin explicitly sent.
 *
 * An attenuation of 0 will play full volume everywhere in the level.
 * Larger attenuation will drop off (max 4 attenuation).
 *
 * If origin is NULL, the origin is determined from the entity origin
 * or the midpoint of the entity box for bmodels.
 */
void Sv_PositionedSound(vec3_t origin, g_edict_t *entity, unsigned short index,
		unsigned short atten) {
	unsigned int flags;
	int i;
	vec3_t org;

	if (atten > ATTN_STATIC) {
		Com_Warn("Sv_PositionedSound: attenuation %d.\n", atten);
		atten = DEFAULT_SOUND_ATTENUATION;
	}

	flags = 0;
	if (atten != DEFAULT_SOUND_ATTENUATION)
		flags |= S_ATTEN;

	// the client doesn't know that bsp models have weird origins
	// the origin can also be explicitly set
	if ((entity->sv_flags & SVF_NO_CLIENT) || (entity->solid == SOLID_BSP)
			|| origin)
		flags |= S_ORIGIN;
	else
		flags |= S_ENTNUM;

	// use the entity origin unless it is a bsp model or explicitly specified
	if (origin)
		VectorCopy(origin, org);
	else {
		if (entity->solid == SOLID_BSP) {
			for (i = 0; i < 3; i++)
				org[i] = entity->s.origin[i] + 0.5 * (entity->mins[i]
						+ entity->maxs[i]);
		} else {
			VectorCopy(entity->s.origin, org);
		}
	}

	Msg_WriteByte(&sv.multicast, SV_CMD_SOUND);
	Msg_WriteByte(&sv.multicast, flags);
	Msg_WriteByte(&sv.multicast, index);

	if (flags & S_ATTEN)
		Msg_WriteByte(&sv.multicast, atten);

	if (flags & S_ENTNUM)
		Msg_WriteShort(&sv.multicast, NUM_FOR_EDICT(entity));

	if (flags & S_ORIGIN)
		Msg_WritePos(&sv.multicast, org);

	if (atten != ATTN_NONE)
		Sv_Multicast(org, MULTICAST_PHS);
	else
		Sv_Multicast(org, MULTICAST_ALL);
}

/*
 *
 * FRAME UPDATES
 *
 */

/*
 * Sv_SendClientDatagram
 */
static boolean_t Sv_SendClientDatagram(sv_client_t *client) {
	byte msg_buf[MAX_MSG_SIZE];
	size_buf_t msg;

	Sv_BuildClientFrame(client);

	Sb_Init(&msg, msg_buf, sizeof(msg_buf));
	msg.allow_overflow = true;

	// send over all the relevant entity_state_t
	// and the player_state_t
	Sv_WriteFrameToClient(client, &msg);

	// copy the accumulated multicast datagram
	// for this client out to the message
	// it is necessary for this to be after the WriteEntities
	// so that entity references will be current
	if (client->datagram.overflowed)
		Com_Warn("Datagram overflowed for %s.\n", client->name);
	else
		Sb_Write(&msg, client->datagram.data, client->datagram.size);
	Sb_Clear(&client->datagram);

	if (msg.overflowed) { // must have room left for the packet header
		Com_Warn("Message overflowed for %s.\n", client->name);
		Sv_DropClient(client);
		return false;
	}

	// send the datagram
	Netchan_Transmit(&client->netchan, msg.size, msg.data);

	// record the size for rate estimation
	client->message_size[sv.frame_num % CLIENT_RATE_MESSAGES] = msg.size;
	return true;
}

/*
 * Sv_DemoCompleted
 */
static void Sv_DemoCompleted(void) {
	Sv_ShutdownServer("Demo complete.\n");
}

/*
 * Sv_RateDrop
 *
 * Returns true if the client is over its current
 * bandwidth estimation and should not be sent another packet
 */
static boolean_t Sv_RateDrop(sv_client_t *c) {
	unsigned int total;
	unsigned short i;

	// never drop over the loopback
	if (c->netchan.remote_address.type == NA_LOCAL)
		return false;

	total = 0;

	for (i = 0; i < CLIENT_RATE_MESSAGES; i++) {
		total += c->message_size[i];
	}

	if (total > c->rate) {
		c->surpress_count++;
		c->message_size[sv.frame_num % CLIENT_RATE_MESSAGES] = 0;
		return true;
	}

	return false;
}

/*
 * Sv_DemoMessage
 *
 * Reads the next frame from the current demo file into the specified buffer,
 * returning the size of the frame in bytes.
 */
static size_t Sv_GetDemoMessage(byte *buffer) {
	int size;
	size_t r;

	r = Fs_Read(&size, 4, 1, sv.demo_file);

	if (r != 1) { // improperly terminated demo file
		Com_Warn("Sv_GetDemoMessage: Failed to read demo file.\n");
		Sv_DemoCompleted();
		return 0;
	}

	size = LittleLong(size);

	if (size == -1) { // properly terminated demo file
		Sv_DemoCompleted();
		return 0;
	}

	if (size > MAX_MSG_SIZE) { // corrupt demo file
		Com_Warn("Sv_GetDemoMessage: %d > MAX_MSG_SIZE.\n", size);
		Sv_DemoCompleted();
		return 0;
	}

	r = Fs_Read(buffer, size, 1, sv.demo_file);

	if (r != 1) {
		Com_Warn("Sv_GetDemoMessage: Incomplete or corrupt demo file.\n");
		Sv_DemoCompleted();
		return 0;
	}

	return size;
}

/*
 * Sv_SendClientMessages
 */
void Sv_SendClientMessages(void) {
	sv_client_t *c;
	int i;

	if (!svs.initialized)
		return;

	// send a message to each connected client
	for (i = 0, c = svs.clients; i < sv_max_clients->integer; i++, c++) {

		if (!c->state) // don't bother
			continue;

		if (c->netchan.message.overflowed) { // drop the client
			Sb_Clear(&c->netchan.message);
			Sb_Clear(&c->datagram);
			Sv_BroadcastPrint(PRINT_HIGH, "%s overflowed\n", c->name);
			Sv_DropClient(c);
		}

		if (sv.state == SV_ACTIVE_DEMO) { // send the demo packet
			byte buffer[MAX_MSG_SIZE];
			size_t size;

			if ((size = Sv_GetDemoMessage(buffer))) {
				Netchan_Transmit(&c->netchan, size, buffer);
			}
		} else if (c->state == SV_CLIENT_ACTIVE) { // send the game packet

			if (Sv_RateDrop(c)) // don't overrun bandwidth
				continue;

			Sv_SendClientDatagram(c);
		} else { // just update reliable if needed
			if (c->netchan.message.size || quake2world.time
					- c->netchan.last_sent > 1000)
				Netchan_Transmit(&c->netchan, 0, NULL);
		}
	}
}

