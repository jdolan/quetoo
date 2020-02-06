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

/**
 * @brief Sends text across to be displayed if the level filter passes.
 */
void Sv_ClientPrint(const g_entity_t *ent, const int32_t level, const char *fmt, ...) {
	sv_client_t *cl;
	va_list args;
	char string[MAX_STRING_CHARS];
	ptrdiff_t n;

	n = NUM_FOR_ENTITY(ent);
	if (n < 1 || n > sv_max_clients->integer) {
		Com_Warn("Issued to non-client %" PRIuPTR "\n", n);
		return;
	}

	cl = &svs.clients[n - 1];

	if (cl->state != SV_CLIENT_ACTIVE) {
		Com_Debug(DEBUG_SERVER, "Issued to unspawned client\n");
		return;
	}

	if (level < cl->message_level) {
		Com_Debug(DEBUG_SERVER, "Filtered by message level\n");
		return;
	}

	va_start(args, fmt);
	vsprintf(string, fmt, args);
	va_end(args);

	Net_WriteByte(&cl->net_chan.message, SV_CMD_PRINT);
	Net_WriteByte(&cl->net_chan.message, level);
	Net_WriteString(&cl->net_chan.message, string);
}

/**
 * @brief Sends text to all active clients over their unreliable channels.
 */
void Sv_BroadcastPrint(const int32_t level, const char *fmt, ...) {
	char string[MAX_STRING_CHARS];
	va_list args;
	sv_client_t *cl;
	int32_t i;

	va_start(args, fmt);
	vsprintf(string, fmt, args);
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

	for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++) {

		if (level < cl->message_level) {
			continue;
		}

		if (cl->state != SV_CLIENT_ACTIVE) {
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

	if (!sv.state) {
		return;
	}

	va_start(args, fmt);
	vsprintf(string, fmt, args);
	va_end(args);

	Net_WriteByte(&sv.multicast, SV_CMD_CBUF_TEXT);
	Net_WriteString(&sv.multicast, string);
	Sv_Multicast(NULL, MULTICAST_ALL_R, NULL);
}

/**
 * @brief Writes to the specified datagram, noting the offset of the message.
 */
static void Sv_ClientDatagramMessage(sv_client_t *cl, byte *data, size_t len) {

	if (len > MAX_MSG_SIZE) {
		Com_Error(ERROR_DROP, "Single datagram message exceeded MAX_MSG_LEN\n");
	}

	sv_client_message_t *msg = g_malloc0(sizeof(*msg));

	msg->offset = cl->datagram.buffer.size;
	msg->len = len;

	cl->datagram.messages = g_list_append(cl->datagram.messages, msg);

	Mem_WriteBuffer(&cl->datagram.buffer, data, len);

	if (cl->datagram.buffer.overflowed) {
		Com_Warn("Client datagram overflow for %s\n", cl->name);

		msg->offset = 0;
		cl->datagram.buffer.overflowed = false;

		g_list_free_full(cl->datagram.messages, g_free);
		cl->datagram.messages = NULL;
	}
}

/**
 * @brief Sends the contents of the mutlicast buffer to a single client
 */
void Sv_Unicast(const g_entity_t *ent, const _Bool reliable) {

	if (ent && !ent->client->ai) {

		const uint16_t n = NUM_FOR_ENTITY(ent);
		if (n < 1 || n > sv_max_clients->integer) {
			Com_Warn("Non-client: %s\n", etos(ent));
			return;
		}

		sv_client_t *cl = svs.clients + (n - 1);

		if (reliable) {
			Mem_WriteBuffer(&cl->net_chan.message, sv.multicast.data, sv.multicast.size);
		} else {
			Sv_ClientDatagramMessage(cl, sv.multicast.data, sv.multicast.size);
		}
	}

	Mem_ClearBuffer(&sv.multicast);
}

/**
 * @brief Sends the contents of sv.multicast to a subset of the clients,
 * then clears sv.multicast.
 */
void Sv_Multicast(const vec3_t origin, multicast_t to, EntityFilterFunc filter) {
	byte vis[MAX_BSP_LEAFS >> 3];
	int32_t area;

	if (!origin) {
		origin = vec3_zero().xyz;
	}

	_Bool reliable = false;

	switch (to) {
		case MULTICAST_ALL_R:
			reliable = true;
                        /* FALLTHRU */
		case MULTICAST_ALL:
			memset(vis, 0xff, sizeof(vis));
			area = 0;
			break;

		case MULTICAST_PHS_R:
			reliable = true;
                        /* FALLTHRU */
		case MULTICAST_PHS: {
				const int32_t leaf = Cm_PointLeafnum(origin, 0);
				const int32_t cluster = Cm_LeafCluster(leaf);
				Cm_ClusterPHS(cluster, vis);
				area = Cm_LeafArea(leaf);
			}

			break;

		case MULTICAST_PVS_R:
			reliable = true;
                        /* FALLTHRU */
		case MULTICAST_PVS: {
				const int32_t leaf = Cm_PointLeafnum(origin, 0);
				const int32_t cluster = Cm_LeafCluster(leaf);
				Cm_ClusterPVS(cluster, vis);
				area = Cm_LeafArea(leaf);
			}
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

		if (cl->state != SV_CLIENT_ACTIVE && !reliable) {
			continue;
		}

		if (cl->entity->client->ai) {
			continue;
		}

		if (to != MULTICAST_ALL && to != MULTICAST_ALL_R) {
			const pm_state_t *pm = &cl->entity->client->ps.pm_state;
			vec3_t org, off;

			UnpackVector(pm->view_offset, off);
			VectorAdd(pm->origin, off, org);

			const int32_t leaf = Cm_PointLeafnum(org, 0);

			const int32_t client_area = Cm_LeafArea(leaf);
			if (!Cm_AreasConnected(area, client_area)) {
				continue;
			}

			const int32_t cluster = Cm_LeafCluster(leaf);
			if (!(vis[cluster >> 3] & (1 << (cluster & 7)))) {
				continue;
			}
		}

		if (filter) { // allow the game module to filter the recipients
			if (!filter(cl->entity)) {
				continue;
			}
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
 * @brief An attenuation of 0 will play full volume everywhere in the level.
 * Larger attenuation will drop off (max 4 attenuation).
 *
 * If origin is NULL, the origin is determined from the entity origin
 * or the midpoint of the entity box for BSP sub-models.
 */
void Sv_PositionedSound(const vec3_t origin, const g_entity_t *ent, const uint16_t index, const uint16_t atten, const int8_t pitch) {

	assert(origin || ent);

	uint32_t flags = 0;

	uint16_t at = atten;
	if ((at & 0x0f) > ATTEN_STATIC) {
		Com_Warn("Bad attenuation %d\n", at & 0x0f);
		at = ((at & 0xf0) | ATTEN_DEFAULT);
	}

	if (origin) {
		flags |= S_ORIGIN;
	}

	if (ent) {
		flags |= S_ENTITY;

		if (ent->sv_flags & SVF_NO_CLIENT) {
			flags |= S_ORIGIN;
			origin = ent->s.origin;
		}
	}

	if (pitch) {
		flags |= S_PITCH;
	}

	Net_WriteByte(&sv.multicast, SV_CMD_SOUND);
	Net_WriteByte(&sv.multicast, flags);
	Net_WriteByte(&sv.multicast, index);

	Net_WriteByte(&sv.multicast, at);

	if (flags & S_ENTITY) {
		Net_WriteShort(&sv.multicast, (int32_t) NUM_FOR_ENTITY(ent));
	}

	if (flags & S_ORIGIN) {
		Net_WritePosition(&sv.multicast, origin);
	}

	if (flags & S_PITCH) {
		Net_WriteByte(&sv.multicast, pitch);
	}

	vec3_t broadcast_origin;
	if (origin) {
		VectorCopy(origin, broadcast_origin);
	} else {
		if (ent->solid == SOLID_BSP) {
			VectorLerp(ent->abs_mins, ent->abs_maxs, 0.5, broadcast_origin);
		} else {
			VectorCopy(ent->s.origin, broadcast_origin);
		}
	}

	if ((atten & 0x0f) != ATTEN_NONE) {
		Sv_Multicast(broadcast_origin, MULTICAST_PHS, NULL);
	} else {
		Sv_Multicast(broadcast_origin, MULTICAST_ALL, NULL);
	}
}

/**
 * @brief
 */
static void Sv_SendClientDatagram(sv_client_t *cl) {
	byte buffer[MAX_MSG_SIZE];
	mem_buf_t buf;

	Sv_BuildClientFrame(cl);

	Mem_InitBuffer(&buf, buffer, sizeof(buffer));
	buf.allow_overflow = true;

	// accumulate the total size for rate throttling
	size_t frame_size = 0;

	// send over all the relevant entity_state_t and the player_state_t
	Sv_WriteClientFrame(cl, &buf);

	// the frame itself (player state and delta entities) must fit into a single message,
	// since it is parsed as a single command by the client
	if (buf.overflowed || buf.size > MAX_MSG_SIZE - 16) {
		Com_Error(ERROR_DROP, "Frame exceeds MAX_MSG_SIZE (%u)\n", (uint32_t) buf.size);
	}

	// but we can packetize the remaining datagram messages, which are parsed individually
	const GList *e = cl->datagram.messages;
	while (e) {
		const sv_client_message_t *msg = (sv_client_message_t *) e->data;

		// if we would overflow the packet, flush it first
		if (buf.size + msg->len > (MAX_MSG_SIZE - 16)) {
			Com_Debug(DEBUG_SERVER, "Fragmenting datagram @ %u bytes\n", (uint32_t) buf.size);

			Netchan_Transmit(&cl->net_chan, buf.data, buf.size);
			frame_size += buf.size;

			Mem_ClearBuffer(&buf);
		}

		Mem_WriteBuffer(&buf, cl->datagram.buffer.data + msg->offset, msg->len);
		e = e->next;
	}

	// send the pending packet, which may include reliable messages
	Netchan_Transmit(&cl->net_chan, buf.data, buf.size);
	frame_size += buf.size;

	// record the total size for rate estimation
	cl->frame_size[sv.frame_num % QUETOO_TICK_RATE] = frame_size;
}

/**
 * @brief
 */
static void Sv_DemoCompleted(void) {

	if (sv_demo_list->string[0]) {

		const char *current_demo = sv.name;
		const char *next_demo = g_strrstr(sv_demo_list->string, current_demo);
		char demo_token[MAX_QPATH];

		if (!next_demo) {

			next_demo = sv_demo_list->string;
		} else {

			next_demo += strlen(current_demo);

			if (next_demo[0] == ' ') {
				next_demo++;
			} else if (!next_demo[0]) {
				next_demo = sv_demo_list->string;
			}
		}

		const char *space = strchr(next_demo, ' ') ? : (next_demo + strlen(next_demo));
		size_t len = space - next_demo;

		strncpy(demo_token, next_demo, len);
		demo_token[len] = 0;

		if (demo_token[0]) {
			Sv_InitServer(demo_token, SV_ACTIVE_DEMO);
		} else {
			Sv_ShutdownServer("Demo complete\n");
		}
	} else {
		Sv_ShutdownServer("Demo complete\n");
	}
}

/**
 * @brief Returns true if the client is over its current bandwidth estimation
 * and should not be sent another packet.
 */
static _Bool Sv_RateDrop(sv_client_t *cl) {

	if (sv.frame_num < lengthof(cl->frame_size)) {
		return false;
	}

	if (cl->rate == 0) {
		return false;
	}

	if (cl->net_chan.remote_address.type == NA_LOOP) {
		return false;
	}

	size_t total = 0;

	for (size_t i = 0; i < lengthof(cl->frame_size); i++) {
		total += cl->frame_size[(sv.frame_num - i) % lengthof(cl->frame_size)];
		if (total > cl->rate) {
			cl->suppress_count++;
			return true;
		}
	}

	return false;
}

/**
 * @brief Reads the next frame from the current demo file into the specified buffer,
 * returning the size of the frame in bytes.
 *
 * FIXME This doesn't work with the new packetized overflow avoidance. Multiple
 * messages can constitute a frame. We need a mechanism to indicate frame
 * completion, or we need a timecode in our demos.
 */
static size_t Sv_GetDemoMessage(byte *buffer) {
	int32_t size;
	size_t r;

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
 * @brief Send the frame and all pending datagram messages since the last frame.
 */
void Sv_SendClientPackets(void) {
	sv_client_t *cl;
	int32_t i;

	if (!svs.initialized) {
		return;
	}

	// send a message to each connected client
	for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++) {

		if (cl->state == SV_CLIENT_FREE) { // don't bother
			continue;
		}

		// if the client's reliable message overflowed, we must drop them
		if (cl->net_chan.message.overflowed) {
			Sv_DropClient(cl);
			Sv_BroadcastPrint(PRINT_MEDIUM, "%s overflowed\n", cl->name);
			continue;
		}

		if (sv.state == SV_ACTIVE_DEMO) { // send the demo packet
			byte buffer[MAX_MSG_SIZE];
			size_t size;

			if ((size = Sv_GetDemoMessage(buffer))) {
				Netchan_Transmit(&cl->net_chan, buffer, size);
			} else {
				break;    // recording is done, so we're done
			}
		} else if (cl->state == SV_CLIENT_ACTIVE) { // send the game packet

			if (Sv_RateDrop(cl)) { // enforce rate throttle
				cl->frame_size[sv.frame_num % lengthof(cl->frame_size)] = 0;
			} else {
				Sv_SendClientDatagram(cl);
			}

			// clean up for the next frame
			Mem_ClearBuffer(&cl->datagram.buffer);

			if (cl->datagram.messages) {
				g_list_free_full(cl->datagram.messages, g_free);
			}

			cl->datagram.messages = NULL;

		} else if (cl->net_chan.message.size) { // update reliable
			Netchan_Transmit(&cl->net_chan, NULL, 0);
		} else if (quetoo.ticks - cl->net_chan.last_sent > 1000) { // or just don't timeout
			Netchan_Transmit(&cl->net_chan, NULL, 0);
		}
	}
}
