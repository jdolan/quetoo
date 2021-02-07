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

#include <SDL_timer.h>

#include "sv_local.h"

sv_static_t svs; // persistent server info
sv_server_t sv; // per-level server info

sv_client_t *sv_client; // current client

cvar_t *sv_demo_list;
cvar_t *sv_download_url;
cvar_t *sv_enforce_time;
cvar_t *sv_hostname;
cvar_t *sv_max_clients;
cvar_t *sv_public;
cvar_t *sv_rcon_password; // password for remote server commands
cvar_t *sv_timeout;
cvar_t *sv_udp_download;

/**
 * @brief Called when the player is totally leaving the server, either willingly
 * or unwillingly. This is NOT called if the entire server is quitting
 * or crashing.
 */
void Sv_DropClient(sv_client_t *cl) {
	g_entity_t *ent;

	Mem_ClearBuffer(&cl->net_chan.message);
	Mem_ClearBuffer(&cl->datagram.buffer);

	if (cl->datagram.messages) {
		g_list_free_full(cl->datagram.messages, Mem_Free);
	}

	if (cl->state > SV_CLIENT_FREE) { // send the disconnect

		if (cl->state == SV_CLIENT_ACTIVE) { // after informing the game module
			svs.game->ClientDisconnect(cl->entity);
		}

		Net_WriteByte(&cl->net_chan.message, SV_CMD_DROP);
		Netchan_Transmit(&cl->net_chan, cl->net_chan.message.data, cl->net_chan.message.size);
	}

	if (cl->download.buffer) {
		Fs_Free(cl->download.buffer);
	}

	ent = cl->entity;

	memset(cl, 0, sizeof(*cl));

	cl->entity = ent;
	cl->last_frame = -1;
}

/**
 * @brief Returns a string fit for heartbeats and status replies.
 */
const char *Sv_StatusString(void) {
	static char status[MAX_MSG_SIZE - 16];
	int32_t i;

	g_snprintf(status, sizeof(status), "%s\n", Cvar_ServerInfo());
	size_t status_len = strlen(status);

	for (i = 0; i < sv_max_clients->integer; i++) {

		const sv_client_t *cl = &svs.clients[i];

		if (cl->state == SV_CLIENT_CONNECTED || cl->state == SV_CLIENT_ACTIVE) {
			char player[MAX_TOKEN_CHARS];
			const uint32_t ping = cl->entity->client->ping;

			g_snprintf(player, sizeof(player), "%d %u \"%s\"\n", i, ping, cl->name);
			const size_t player_len = strlen(player);

			if (status_len + player_len + 1 >= sizeof(status)) {
				break;    // can't hold any more
			}

			strcat(status, player);
			status_len += player_len;
		}
	}

	return status;
}

/**
 * @brief Responds with all the info that qplug or qspy can see.
 */
static void Sv_Status_f(void) {
	Netchan_OutOfBandPrint(NS_UDP_SERVER, &net_from, "print\n%s", Sv_StatusString());
}

/**
 * @brief
 */
static void Sv_Ack_f(void) {
	Com_Print("Ping acknowledge from %s\n", Net_NetaddrToString(&net_from));
}

/**
 * @brief Responds with brief info for broadcast scans.
 */
static void Sv_Info_f(void) {
	char string[MAX_MSG_SIZE];

	if (sv.demo_file) {
		Com_Debug(DEBUG_SERVER, "Demo server ignoring server info request\n");
		return;
	}

	const int32_t p = atoi(Cmd_Argv(1));
	if (p != PROTOCOL_MAJOR) {
		g_snprintf(string, sizeof(string), "%s: Wrong protocol: %d != %d", sv_hostname->string, p,
		           PROTOCOL_MAJOR);
	} else {
		int32_t i, count = 0;

		for (i = 0; i < sv_max_clients->integer; i++) {
			if (svs.clients[i].state >= SV_CLIENT_CONNECTED) {
				count++;
			}
		}

		g_snprintf(string, sizeof(string), "%s\\%s\\%s\\%d\\%d", sv_hostname->string,
		           sv.name, svs.game->GameName(), count, sv_max_clients->integer);
	}

	Netchan_OutOfBandPrint(NS_UDP_SERVER, &net_from, "info\n%s", string);
}

/**
 * @brief Just responds with an acknowledgment.
 */
static void Sv_Ping_f(void) {
	Netchan_OutOfBandPrint(NS_UDP_SERVER, &net_from, "ack");
}

/**
 * @brief Returns a challenge number that can be used in a subsequent client_connect
 * command.
 *
 * We do this to prevent denial of service attacks that flood the server with
 * invalid connection IPs. With a challenge, they must give a valid address.
 */
static void Sv_GetChallenge_f(void) {
	uint16_t i, oldest;
	uint32_t oldest_time;

	oldest = 0;
	oldest_time = UINT32_MAX;

	// see if we already have a challenge for this ip
	for (i = 0; i < MAX_CHALLENGES; i++) {

		if (Net_CompareClientNetaddr(&net_from, &svs.challenges[i].addr)) {
			break;
		}

		if (svs.challenges[i].time < oldest_time) {
			oldest_time = svs.challenges[i].time;
			oldest = i;
		}
	}

	if (i == MAX_CHALLENGES) { // overwrite the oldest
		svs.challenges[oldest].challenge = Randomu();
		svs.challenges[oldest].addr = net_from;
		svs.challenges[oldest].time = quetoo.ticks;
		i = oldest;
	}

	// send it back
	Netchan_OutOfBandPrint(NS_UDP_SERVER, &net_from, "challenge %i", svs.challenges[i].challenge);
}

/**
 * @brief A connection request that did not come from the master.
 */
static void Sv_Connect_f(void) {
	char user_info[MAX_USER_INFO_STRING];
	sv_client_t *cl, *client;
	int32_t i;

	Com_Debug(DEBUG_SERVER, "Svc_Connect()\n");

	net_addr_t *addr = &net_from;

	const int32_t version = (int32_t) strtol(Cmd_Argv(1), NULL, 0);

	// resolve protocol
	if (version != PROTOCOL_MAJOR) {
		Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nServer is version %d.\n", PROTOCOL_MAJOR);
		return;
	}

	const uint8_t qport = (uint8_t) strtoul(Cmd_Argv(2), NULL, 0);
	const uint32_t challenge = (uint32_t) strtoul(Cmd_Argv(3), NULL, 0);

	// copy user_info, leave room for ip stuffing
	g_strlcpy(user_info, Cmd_Argv(4), sizeof(user_info) - 25);

	if (*user_info == '\0') { // catch empty user_info
		Com_Print("Empty user_info from %s\n", Net_NetaddrToString(addr));
		Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nConnection refused\n");
		return;
	}

	if (strchr(user_info, '\xFF')) { // catch end of message in string exploit
		Com_Print("Illegal user_info contained xFF from %s\n", Net_NetaddrToString(addr));
		Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nConnection refused\n");
		return;
	}

	if (strlen(GetUserInfo(user_info, "ip"))) { // catch spoofed ips
		Com_Print("Illegal user_info contained ip from %s\n", Net_NetaddrToString(addr));
		Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nConnection refused\n");
		return;
	}

	if (!ValidateUserInfo(user_info)) { // catch otherwise invalid user_info
		Com_Print("Invalid user_info from %s\n", Net_NetaddrToString(addr));
		Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nConnection refused\n");
		return;
	}

	// force the ip so the game can filter on it
	SetUserInfo(user_info, "ip", Net_NetaddrToString(addr));

	// enforce a valid challenge to avoid denial of service attack
	for (i = 0; i < MAX_CHALLENGES; i++) {
		if (Net_CompareClientNetaddr(addr, &svs.challenges[i].addr)) {
			if (challenge == svs.challenges[i].challenge) {
				svs.challenges[i].challenge = 0;
				break; // good
			}
			Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nBad challenge\n");
			return;
		}
	}
	if (i == MAX_CHALLENGES) {
		Com_Print("Connection without challenge from %s\n", Net_NetaddrToString(addr));
		Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nNo challenge for address\n");
		return;
	}

	// resolve the client slot
	client = NULL;

	// first check for an ungraceful reconnect (client crashed, perhaps)
	for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++) {

		const net_chan_t *ch = &cl->net_chan;

		if (cl->state == SV_CLIENT_FREE) { // not in use, not interested
			continue;
		}

		// the base address and either the qport or real port must match
		if (Net_CompareClientNetaddr(addr, &ch->remote_address)) {

			if (addr->port == ch->remote_address.port || qport == ch->qport) {
				client = cl;
				break;
			}
		}
	}

	// otherwise, treat as a fresh connect to a new slot
	if (!client) {
		for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++) {
			if (cl->state == SV_CLIENT_FREE && !cl->entity->client->ai) { // we have a free one
				client = cl;
				break;
			}
		}
	}

	// no free slots, see if there's an AI slot ready to go and boot them.
	if (!client) {
		for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++) {
			if (cl->state == SV_CLIENT_FREE && cl->entity->client->ai) { // we have a free one
				client = cl;
				svs.game->ClientDisconnect(cl->entity);
				break;
			}
		}
	}

	// no soup for you, next!!
	if (!client) {
		Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nServer is full\n");
		Com_Debug(DEBUG_SERVER, "Rejected a connection\n");
		return;
	}

	// give the game a chance to reject this connection or modify the user_info
	if (!(svs.game->ClientConnect(client->entity, user_info))) {
		const char *rejmsg = GetUserInfo(user_info, "rejmsg");

		if (strlen(rejmsg)) {
			Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\n%s\nConnection refused\n", rejmsg);
		} else {
			Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "print\nConnection refused\n");
		}

		Com_Debug(DEBUG_SERVER, "Game rejected a connection\n");
		return;
	}

	// parse some info from the info strings
	g_strlcpy(client->user_info, user_info, sizeof(client->user_info));
	Sv_UserInfoChanged(client);

	// send the connect packet to the client
	Netchan_OutOfBandPrint(NS_UDP_SERVER, addr, "client_connect %s", sv_download_url->string);

	Netchan_Setup(NS_UDP_SERVER, &client->net_chan, addr, qport);

	Mem_InitBuffer(&client->datagram.buffer, client->datagram.data, sizeof(client->datagram.data));
	client->datagram.buffer.allow_overflow = true;

	client->last_message = quetoo.ticks;

	client->state = SV_CLIENT_CONNECTED;
}

/**
 * @brief
 */
static _Bool Sv_RconAuthenticate(void) {

	// a password must be set for rcon to be available
	if (*sv_rcon_password->string == '\0') {
		return false;
	}

	// and of course the passwords must match
	if (g_strcmp0(Cmd_Argv(1), sv_rcon_password->string)) {
		return false;
	}

	return true;
}

static char sv_rcon_buffer[MAX_PRINT_MSG];

/**
 * @brief Console appender for remote console.
 */
static void Sv_Rcon_Print(const console_string_t *str) {

	g_strlcat(sv_rcon_buffer, str->chars, sizeof(sv_rcon_buffer));
}

/**
 * @brief A client issued an rcon command. Shift down the remaining args and
 * redirect all output to the invoking client.
 */
static void Sv_Rcon_f(void) {

	const _Bool auth = Sv_RconAuthenticate();

	const char *addr = Net_NetaddrToString(&net_from);

	// first print to the server console
	if (auth) {
		Com_Print("Rcon from %s:\n%s\n", addr, net_message.data + 4);
	} else {
		Com_Print("Bad rcon from %s:\n%s\n", addr, net_message.data + 4);
	}

	// then redirect the remaining output back to the client

	console_t rcon = { .Append = Sv_Rcon_Print };
	sv_rcon_buffer[0] = '\0';

	Con_AddConsole(&rcon);

	if (auth) {
		char cmd[MAX_STRING_CHARS];
		cmd[0] = '\0';

		for (int32_t i = 2; i < Cmd_Argc(); i++) {
			g_strlcat(cmd, Cmd_Argv(i), sizeof(cmd));
			g_strlcat(cmd, " ", sizeof(cmd));
		}

		Cmd_ExecuteString(cmd);
	} else {
		Com_Print("Bad rcon_password\n");
	}

	Netchan_OutOfBandPrint(NS_UDP_SERVER, &net_from, "print\n%s", sv_rcon_buffer);

	Con_RemoveConsole(&rcon);
}

/**
 * @brief A connection-less packet has four leading 0xff bytes to distinguish
 * it from a game channel. Clients that are in the game can still send these,
 * and they will be handled here.
 */
static void Sv_ConnectionlessPacket(void) {

	Net_BeginReading(&net_message);
	Net_ReadLong(&net_message); // skip the -1 marker

	const char *s = Net_ReadStringLine(&net_message);

	Cmd_TokenizeString(s);

	const char *c = Cmd_Argv(0);
	const char *a = Net_NetaddrToString(&net_from);

	Com_Debug(DEBUG_SERVER, "Packet from %s: %s\n", a, c);

	if (!g_strcmp0(c, "ping")) {
		Sv_Ping_f();
	} else if (!g_strcmp0(c, "ack")) {
		Sv_Ack_f();
	} else if (!g_strcmp0(c, "status")) {
		Sv_Status_f();
	} else if (!g_strcmp0(c, "info")) {
		Sv_Info_f();
	} else if (!g_strcmp0(c, "get_challenge")) {
		Sv_GetChallenge_f();
	} else if (!g_strcmp0(c, "connect")) {
		Sv_Connect_f();
	} else if (!g_strcmp0(c, "rcon")) {
		Sv_Rcon_f();
	} else {
		Com_Print("Bad connectionless packet from %s:\n%s\n", a, s);
	}
}

/**
 * @brief Updates the "ping" times for all spawned clients.
 */
static void Sv_UpdatePings(void) {

	for (int32_t i = 0; i < sv_max_clients->integer; i++) {

		const sv_client_t *cl = &svs.clients[i];

		if (cl->state != SV_CLIENT_ACTIVE) {
			continue;
		}

		int32_t total = 0, count = 0;
		for (int32_t j = 0; j < SV_CLIENT_LATENCY_COUNT; j++) {
			if (cl->frame_latency[j] > 0) {
				total += cl->frame_latency[j];
				count++;
			}
		}

		if (!count) {
			cl->entity->client->ping = 0;
		} else {
			cl->entity->client->ping = total / (float) count;
		}
	}
}

/**
 * @brief Once per second, gives all clients an allotment of 1000 milliseconds
 * for their movement commands which will be decremented as we receive
 * new information from them. If they drift by a significant margin
 * over the next interval, assume they are trying to cheat.
 */
static void Sv_CheckCommandTimes(void) {
	static uint32_t last_check_time;

	// see if its time to check the movements
	if (quetoo.ticks - last_check_time < CMD_MSEC_CHECK_INTERVAL) {
		return;
	}

	last_check_time = quetoo.ticks;

	// inspect each client, ensuring they are reasonably in sync with us
	for (int32_t i = 0; i < sv_max_clients->integer; i++) {
		sv_client_t *cl = &svs.clients[i];

		if (cl->state < SV_CLIENT_ACTIVE) {
			continue;
		}

		if (sv_enforce_time->value) { // check them

			if (cl->cmd_msec > CMD_MSEC_ALLOWABLE_DRIFT) { // irregular movement
				cl->cmd_msec_errors++;

				Com_Debug(DEBUG_SERVER, "%s drifted %dms\n", Sv_NetaddrToString(cl), cl->cmd_msec);

				if (cl->cmd_msec_errors >= sv_enforce_time->value) {
					Com_Warn("Too many errors from %s\n", Sv_NetaddrToString(cl));
					Sv_KickClient(cl, "Irregular movement");
					continue;
				}
			} else { // normal movement

				if (cl->cmd_msec_errors) {
					cl->cmd_msec_errors--;
				}
			}
		}

		cl->cmd_msec = 0; // reset for next cycle
	}
}

/**
 * @brief
 */
static void Sv_ReadPackets(void) {

	while (Net_ReceiveDatagram(NS_UDP_SERVER, &net_from, &net_message)) {

		// check for connectionless packet (0xffffffff) first
		if (*(uint32_t *) net_message.data == 0xffffffff) {
			Sv_ConnectionlessPacket();
			continue;
		}

		// read the qport out of the message so we can fix up
		// stupid address translating routers
		Net_BeginReading(&net_message);

		Net_ReadLong(&net_message); // sequence number
		Net_ReadLong(&net_message); // sequence number

		const byte qport = Net_ReadByte(&net_message) & 0xff;

		// check for packets from connected clients
		sv_client_t *cl = svs.clients;
		for (int32_t i = 0; i < sv_max_clients->integer; i++, cl++) {

			if (cl->state == SV_CLIENT_FREE) {
				continue;
			}

			if (!Net_CompareClientNetaddr(&net_from, &cl->net_chan.remote_address)) {
				continue;
			}

			if (cl->net_chan.qport != qport) {
				continue;
			}

			if (cl->net_chan.remote_address.port != net_from.port) {
				cl->net_chan.remote_address.port = net_from.port;
				Com_Warn("Fixed translated port for %s\n", Net_NetaddrToString(&net_from));
			}

			// this is a valid, sequenced packet, so process it
			if (Netchan_Process(&cl->net_chan, &net_message)) {
				cl->last_message = quetoo.ticks; // nudge timeout
				Sv_ParseClientMessage(cl);
			}

			// we've processed the packet for the correct client, so break
			break;
		}
	}
}

/**
 * @brief
 */
static void Sv_CheckTimeouts(void) {

	const uint32_t timeout = 1000 * sv_timeout->value;

	if (timeout > quetoo.ticks) {
		return;
	}

	const uint32_t whence = quetoo.ticks - timeout;

	sv_client_t *cl = svs.clients;
	for (int32_t i = 0; i < sv_max_clients->integer; i++, cl++) {

		if (cl->state == SV_CLIENT_FREE) {
			continue;
		}

		if (cl->last_message < whence) {
			Sv_BroadcastPrint(PRINT_MEDIUM, "%s timed out\n", cl->name);
			Sv_DropClient(cl);
		}
	}
}

/**
 * @brief Resets entity flags and other state which should only last one frame.
 */
static void Sv_ResetEntities(void) {

	if (sv.state != SV_ACTIVE_GAME) {
		return;
	}

	for (uint16_t i = 0; i < svs.game->num_entities; i++) {
		g_entity_t *edict = ENTITY_FOR_NUM(i);

		// events only last for a single message
		edict->s.event = 0;
	}
}

/**
 * @brief Updates the game module's time and runs its frame function.
 */
static void Sv_RunGameFrame(void) {

	sv.frame_num++;
	sv.time = sv.frame_num * QUETOO_TICK_MILLIS;

	if (sv.state == SV_ACTIVE_GAME) {
		svs.game->Frame();
	}
}


/**
 * @brief
 */
void Sv_KickClient(sv_client_t *cl, const char *msg) {
	char buf[MAX_STRING_CHARS], name[32];

	if (!cl) {
		return;
	}

	if (cl->state < SV_CLIENT_CONNECTED) {
		return;
	}

	if (*cl->name == '\0') { // force a name to kick
		strcpy(name, "player");
	} else {
		g_strlcpy(name, cl->name, sizeof(name));
	}

	memset(buf, 0, sizeof(buf));

	if (msg && *msg != '\0') {
		g_snprintf(buf, sizeof(buf), ": %s", msg);
	}

	Sv_ClientPrint(cl->entity, PRINT_HIGH, "You were kicked%s\n", buf);

	Sv_DropClient(cl);

	Sv_BroadcastPrint(PRINT_MEDIUM, "%s was kicked%s\n", name, buf);
}

/**
 * @brief A convenience function for printing out client addresses.
 */
const char *Sv_NetaddrToString(const sv_client_t *cl) {
	return Net_NetaddrToString(&cl->net_chan.remote_address);
}

/**
 * @brief Enforces safe user_info data before passing onto game module.
 */
void Sv_UserInfoChanged(sv_client_t *cl) {
	char *val;
	size_t i;

	if (*cl->user_info == '\0') { // catch empty user_info
		Com_Print("Empty user_info from %s\n", Sv_NetaddrToString(cl));
		Sv_KickClient(cl, "Bad user info");
		return;
	}

	if (strchr(cl->user_info, '\xFF')) { // catch end of message exploit
		Com_Print("Illegal user_info contained xFF from %s\n", Sv_NetaddrToString(cl));
		Sv_KickClient(cl, "Bad user info");
		return;
	}

	if (!ValidateUserInfo(cl->user_info)) { // catch otherwise invalid user_info
		Com_Print("Invalid user_info from %s\n", Sv_NetaddrToString(cl));
		Sv_KickClient(cl, "Bad user info");
		return;
	}

	// call game code to allow overrides
	svs.game->ClientUserInfoChanged(cl->entity, cl->user_info);

	// name for C code, mask off high bit
	g_strlcpy(cl->name, GetUserInfo(cl->user_info, "name"), sizeof(cl->name));
	for (i = 0; i < sizeof(cl->name); i++) {
		cl->name[i] &= 127;
	}

	// rate command
	val = GetUserInfo(cl->user_info, "rate");
	if (*val != '\0') {
		cl->rate = (uint32_t) strtoul(val, NULL, 10);
		if (cl->rate > 0 && cl->rate < CLIENT_RATE_MIN) {
			cl->rate = CLIENT_RATE_MIN;
		}
	}

	// limit the print messages the client receives
	val = GetUserInfo(cl->user_info, "message_level");
	if (*val != '\0') {
		cl->message_level = (int32_t) strtol(val, NULL, 10);
	}
}

/**
 * @brief
 */
void Sv_Frame(const uint32_t msec) {
	static uint32_t frame_delta;

	// if server is not active, do nothing
	if (!svs.initialized) {
		return;
	}

	if (time_demo->value) { // always run a frame
		frame_delta = QUETOO_TICK_MILLIS;
	} else { // keep simulation time in sync with reality

		frame_delta += msec;

		if (frame_delta < QUETOO_TICK_MILLIS) {
			if (dedicated->value) {
				SDL_Delay(QUETOO_TICK_MILLIS - frame_delta);
			}

			return;
		}
	}

	// clamp the frame interval to 1 second of simulation
	frame_delta = Minf(frame_delta, (uint32_t) (QUETOO_TICK_MILLIS * QUETOO_TICK_RATE));

	// read any pending packets from clients
	Sv_ReadPackets();

	// check timeouts
	Sv_CheckTimeouts();

	// check command times for attempted cheating
	Sv_CheckCommandTimes();

	// update ping based on the last known frame from all clients
	Sv_UpdatePings();

	// send a heartbeat to the master if needed
	Sv_HeartbeatMasters();

	// let everything in the world think and move
	while (frame_delta >= QUETOO_TICK_MILLIS) {

		// run the simulation
		Sv_RunGameFrame();

		// send the resulting frame to connected clients
		Sv_SendClientPackets();

		// decrement the simulation time
		frame_delta -= QUETOO_TICK_MILLIS;
	}

	// clear entity flags, etc for next frame
	Sv_ResetEntities();

	// redraw the console
	Sv_DrawConsole();
}

/**
 * @brief
 */
static void Sv_InitLocal(void) {

	sv_demo_list = Cvar_Add("sv_demo_list", "", CVAR_SERVER_INFO,
	                        "A list of demo names to cycle through");
	sv_download_url = Cvar_Add("sv_download_url", "", CVAR_SERVER_INFO,
	                           "The base URL for in-game HTTP downloads");
	sv_enforce_time = Cvar_Add("sv_enforce_time", va("%d", CMD_MSEC_MAX_DRIFT_ERRORS), 0,
	                           "Prevents the most blatant form of speed cheating, disable at your own risk");
	sv_hostname = Cvar_Add("sv_hostname", "Quetoo", CVAR_SERVER_INFO | CVAR_ARCHIVE,
	                       "The server hostname, visible in the server browser");
	sv_max_clients = Cvar_Add("sv_max_clients", "8", CVAR_SERVER_INFO | CVAR_LATCH,
	                          "The maximum number of clients the server will allow");
	sv_public = Cvar_Add("sv_public", "0", CVAR_SERVER_INFO,
	                     "Set to 1 to to advertise this server via the master server");
	sv_rcon_password = Cvar_Add("rcon_password", "", 0,
	                            "The remote console password. If set, only give this to trusted clients");
	sv_timeout = Cvar_Add("sv_timeout", va("%d", SV_TIMEOUT), 0, NULL);
	sv_udp_download = Cvar_Add("sv_udp_download", "1", CVAR_ARCHIVE,
	                           "If set, in-game UDP downloads will be allowed when HTTP downloads fail");

	if (dedicated->value) {
		Cvar_SetInteger(sv_public->name, 1);
	}

	// set this so clients and server browsers can see it
	Cvar_Add("sv_protocol", va("%i", PROTOCOL_MAJOR), CVAR_SERVER_INFO | CVAR_NO_SET, NULL);
}

/**
 * @brief Only called at Quetoo startup, not for each game.
 */
void Sv_Init(void) {

	memset(&svs, 0, sizeof(svs));

	Cm_LoadBspModel(NULL, NULL);

	Sv_InitConsole();

	Sv_InitLocal();

	Sv_InitAdmin();

	Sv_InitMasters();

	if (dedicated->value && Fs_Exists("server.cfg")) {
		Cbuf_AddText("exec server.cfg\n");
		Cbuf_Execute();
	}
}

/**
 * @brief Called when server is shutting down due to error or an explicit `quit`.
 */
void Sv_Shutdown(const char *msg) {

	Sv_ShutdownServer(msg);

	Sv_ShutdownConsole();

	memset(&svs, 0, sizeof(svs));

	Cmd_RemoveAll(CMD_SERVER);

	Mem_FreeTag(MEM_TAG_SERVER);
}
