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
#include "console.h"

sv_static_t svs; // persistent server info
sv_server_t sv; // per-level server info

sv_client_t *sv_client; // current client
g_edict_t *sv_player; // current client edict

cvar_t *sv_rcon_password; // password for remote server commands

cvar_t *sv_download_url;
cvar_t *sv_enforce_time;
cvar_t *sv_hostname;
cvar_t *sv_max_clients;
cvar_t *sv_framerate;
cvar_t *sv_public;
cvar_t *sv_timeout;
cvar_t *sv_udp_download;

/*
 * Sv_DropClient
 *
 * Called when the player is totally leaving the server, either willingly
 * or unwillingly.  This is NOT called if the entire server is quitting
 * or crashing.
 */
void Sv_DropClient(sv_client_t *cl) {
	g_edict_t *ent;

	if (cl->state > SV_CLIENT_FREE) { // send the disconnect

		if (cl->state == SV_CLIENT_ACTIVE) { // after informing the game module
			svs.game->ClientDisconnect(cl->edict);
		}

		Msg_WriteByte(&cl->netchan.message, SV_CMD_DISCONNECT);
		Netchan_Transmit(&cl->netchan, cl->netchan.message.size,
				cl->netchan.message.data);
	}

	if (cl->download) {
		Fs_FreeFile(cl->download);
		cl->download = NULL;
	}

	ent = cl->edict;

	memset(cl, 0, sizeof(*cl));

	cl->edict = ent;
	cl->last_frame = -1;
}

/*
 * Sv_StatusString
 *
 * Returns a string fit for heartbeats and status replies.
 */
static const char *Sv_StatusString(void) {
	char player[1024];
	static char status[MAX_MSG_SIZE - 16];
	sv_client_t *cl;
	size_t status_len, player_len;
	int i;

	strcpy(status, Cvar_ServerInfo());
	strcat(status, "\n");

	status_len = strlen(status);

	for (i = 0; i < sv_max_clients->integer; i++) {

		cl = &svs.clients[i];

		if (cl->state == SV_CLIENT_CONNECTED || cl->state == SV_CLIENT_ACTIVE) {

			snprintf(player, sizeof(player), "%d %u \"%s\"\n",
					cl->edict->client->ps.stats[STAT_FRAGS], cl->ping, cl->name);

			player_len = strlen(player);

			if (status_len + player_len >= sizeof(status))
				break; // can't hold any more

			strcpy(status + status_len, player);
			status_len += player_len;
		}
	}

	return status;
}

/*
 * Svc_Status
 *
 * Responds with all the info that qplug or qspy can see
 */
static void Svc_Status(void) {
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "print\n%s", Sv_StatusString());
}

/*
 * Svc_Ack
 */
static void Svc_Ack(void) {
	Com_Print("Ping acknowledge from %s\n", Net_NetaddrToString(net_from));
}

/*
 * Svc_Info
 *
 * Responds with short info for broadcast scans.
 */
static void Svc_Info(void) {
	char string[MAX_MSG_SIZE];
	int i, count;
	int prot;

	if (sv_max_clients->integer == 1)
		return; // ignore in single player

	prot = atoi(Cmd_Argv(1));

	if (prot != PROTOCOL)
		snprintf(string, sizeof(string), "%s: wrong protocol version\n", sv_hostname->string);
	else {
		count = 0;

		for (i = 0; i < sv_max_clients->integer; i++) {
			if (svs.clients[i].state >= SV_CLIENT_CONNECTED)
				count++;
		}

		snprintf(string, sizeof(string), "%-63s\\%-31s\\%-31s\\%d\\%d",
				sv_hostname->string, sv.name, Cvar_GetString("g_gameplay"),
				count, sv_max_clients->integer);
	}

	Netchan_OutOfBandPrint(NS_SERVER, net_from, "info\n%s", string);
}

/*
 * Svc_Ping
 *
 * Just responds with an acknowledgement
 */
static void Svc_Ping(void) {
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "ack");
}

/*
 * Svc_GetChallenge
 *
 * Returns a challenge number that can be used in a subsequent client_connect
 * command.
 *
 * We do this to prevent denial of service attacks that flood the server with
 * invalid connection IPs.  With a challenge, they must give a valid address.
 */
static void Svc_GetChallenge(void) {
	unsigned short i, oldest;
	unsigned int oldest_time;

	oldest = 0;
	oldest_time = 0x7fffffff;

	// see if we already have a challenge for this ip
	for (i = 0; i < MAX_CHALLENGES; i++) {

		if (Net_CompareClientNetaddr(net_from, svs.challenges[i].addr))
			break;

		if (svs.challenges[i].time < oldest_time) {
			oldest_time = svs.challenges[i].time;
			oldest = i;
		}
	}

	if (i == MAX_CHALLENGES) {
		// overwrite the oldest
		svs.challenges[oldest].challenge = rand() & 0x7fff;
		svs.challenges[oldest].addr = net_from;
		svs.challenges[oldest].time = quake2world.time;
		i = oldest;
	}

	// send it back
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "challenge %i",
			svs.challenges[i].challenge);
}

/*
 * Svc_Connect
 *
 * A connection request that did not come from the master.
 */
static void Svc_Connect(void) {
	char user_info[MAX_USER_INFO_STRING];
	sv_client_t *cl, *client;
	net_addr_t addr;
	int version;
	byte qport;
	unsigned int challenge;
	int i;

	Com_Debug("Svc_Connect()\n");

	addr = net_from;

	version = atoi(Cmd_Argv(1));

	// resolve protocol
	if (version != PROTOCOL) {
		Netchan_OutOfBandPrint(NS_SERVER, addr,
				"print\nServer is version %d.\n", PROTOCOL);
		return;
	}

	qport = strtoul(Cmd_Argv(2), NULL, 0) & 0xff;

	challenge = strtoul(Cmd_Argv(3), NULL, 0);

	//copy user_info, leave room for ip stuffing
	strncpy(user_info, Cmd_Argv(4), sizeof(user_info) - 1 - 25);
	user_info[sizeof(user_info) - 1] = 0;

	if (*user_info == '\0') { // catch empty user_info
		Com_Print("Empty user_info from %s\n", Net_NetaddrToString(addr));
		Netchan_OutOfBandPrint(NS_SERVER, addr, "print\nConnection refused.\n");
		return;
	}

	if (strchr(user_info, '\xFF')) { // catch end of message in string exploit
		Com_Print("Illegal user_info contained xFF from %s\n",
				Net_NetaddrToString(addr));
		Netchan_OutOfBandPrint(NS_SERVER, addr, "print\nConnection refused.\n");
		return;
	}

	if (strlen(GetUserInfo(user_info, "ip"))) { // catch spoofed ips
		Com_Print("Illegal user_info contained ip from %s\n",
				Net_NetaddrToString(addr));
		Netchan_OutOfBandPrint(NS_SERVER, addr, "print\nConnection refused.\n");
		return;
	}

	if (!ValidateUserInfo(user_info)) { // catch otherwise invalid user_info
		Com_Print("Invalid user_info from %s\n", Net_NetaddrToString(addr));
		Netchan_OutOfBandPrint(NS_SERVER, addr, "print\nConnection refused.\n");
		return;
	}

	// force the ip so the game can filter on it
	SetUserInfo(user_info, "ip", Net_NetaddrToString(addr));

	// enforce a valid challenge to avoid denial of service attack
	for (i = 0; i < MAX_CHALLENGES; i++) {
		if (Net_CompareClientNetaddr(addr, svs.challenges[i].addr)) {
			if (challenge == svs.challenges[i].challenge) {
				svs.challenges[i].challenge = 0;
				break; // good
			}
			Netchan_OutOfBandPrint(NS_SERVER, addr, "print\nBad challenge.\n");
			return;
		}
	}
	if (i == MAX_CHALLENGES) {
		Netchan_OutOfBandPrint(NS_SERVER, addr,
				"print\nNo challenge for address.\n");
		return;
	}

	// resolve the client slot
	client = NULL;

	// first check for an ungraceful reconnect (client crashed, perhaps)
	for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++) {

		const net_chan_t *ch = &cl->netchan;

		if (cl->state == SV_CLIENT_FREE) // not in use, not interested
			continue;

		// the base address and either the qport or real port must match
		if (Net_CompareClientNetaddr(addr, ch->remote_address) &&
				(qport == ch->qport || ch->remote_address.port == addr.port)) {
			client = cl;
			break;
		}
	}

	// otherwise, treat as a fresh connect to a new slot
	if (!client) {
		for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++) {
			if (cl->state == SV_CLIENT_FREE) { // we have a free one
				client = cl;
				break;
			}
		}
	}

	// no soup for you, next!!
	if (!client) {
		Netchan_OutOfBandPrint(NS_SERVER, addr, "print\nServer is full.\n");
		Com_Debug("Rejected a connection.\n");
		return;
	}

	// give the game a chance to reject this connection or modify the user_info
	if (!(svs.game->ClientConnect(client->edict, user_info))) {

		if (*GetUserInfo(user_info, "rejmsg")) {
			Netchan_OutOfBandPrint(NS_SERVER, addr,
					"print\n%s\nConnection refused.\n",
					GetUserInfo(user_info, "rejmsg"));
		} else {
			Netchan_OutOfBandPrint(NS_SERVER, addr,
					"print\nConnection refused.\n");
		}

		Com_Debug("Game rejected a connection.\n");
		return;
	}

	// parse some info from the info strings
	strncpy(client->user_info, user_info, sizeof(client->user_info) - 1);
	Sv_UserInfoChanged(client);

	// send the connect packet to the client
	Netchan_OutOfBandPrint(NS_SERVER, addr, "client_connect %s",
			sv_download_url->string);

	Netchan_Setup(NS_SERVER, &client->netchan, addr, qport);

	Sb_Init(&client->datagram, client->datagram_buf,
			sizeof(client->datagram_buf));
	client->datagram.allow_overflow = true;

	client->last_message = svs.real_time; // don't timeout

	client->state = SV_CLIENT_CONNECTED;
}

/*
 * Sv_RconAuthenticate
 */
static boolean_t Sv_RconAuthenticate(void) {

	// a password must be set for rcon to be available
	if (*sv_rcon_password->string == '\0')
		return false;

	// and of course the passwords must match
	if (strcmp(Cmd_Argv(1), sv_rcon_password->string))
		return false;

	return true;
}

/*
 * Svc_RemoteCommand
 *
 * A client issued an rcon command.  Shift down the remaining args and
 * redirect all output to the invoking client.
 */
static void Svc_RemoteCommand(void) {
	const boolean_t auth = Sv_RconAuthenticate();

	// first print to the server console
	if (auth)
		Com_Print("Rcon from %s:\n%s\n", Net_NetaddrToString(net_from),
				net_message.data + 4);
	else
		Com_Print("Bad rcon from %s:\n%s\n", Net_NetaddrToString(net_from),
				net_message.data + 4);

	// then redirect the remaining output back to the client
	Com_BeginRedirect(RD_PACKET, sv_outputbuf, SV_OUTPUTBUF_LENGTH,
			Sv_FlushRedirect);

	if (auth) {
		char remaining[MAX_STRING_CHARS];
		int i;

		remaining[0] = 0;

		for (i = 2; i < Cmd_Argc(); i++) {
			strcat(remaining, Cmd_Argv(i));
			strcat(remaining, " ");
		}

		Cmd_ExecuteString(remaining);
	} else {
		Com_Print("Bad rcon_password.\n");
	}

	Com_EndRedirect();
}

/*
 * Sv_ConnectionlessPacket
 *
 * A connectionless packet has four leading 0xff bytes to distinguish it from
 * a game channel.  Clients that are in the game can still send these, and they
 * will be handled here.
 */
static void Sv_ConnectionlessPacket(void) {
	char *s;
	char *c;

	Msg_BeginReading(&net_message);
	Msg_ReadLong(&net_message); // skip the -1 marker

	s = Msg_ReadStringLine(&net_message);

	Cmd_TokenizeString(s);

	c = Cmd_Argv(0);
	Com_Debug("Packet from %s: %s\n", Net_NetaddrToString(net_from), c);

	if (!strcmp(c, "ping"))
		Svc_Ping();
	else if (!strcmp(c, "ack"))
		Svc_Ack();
	else if (!strcmp(c, "status"))
		Svc_Status();
	else if (!strcmp(c, "info"))
		Svc_Info();
	else if (!strcmp(c, "get_challenge"))
		Svc_GetChallenge();
	else if (!strcmp(c, "connect"))
		Svc_Connect();
	else if (!strcmp(c, "rcon"))
		Svc_RemoteCommand();
	else
		Com_Print("Bad connectionless packet from %s:\n%s\n",
				Net_NetaddrToString(net_from), s);
}

/*
 * Sv_UpdatePings
 *
 * Updates the "ping" times for all spawned clients.
 */
static void Sv_UpdatePings(void) {
	int i, j;
	sv_client_t *cl;
	int total, count;

	for (i = 0; i < sv_max_clients->integer; i++) {

		cl = &svs.clients[i];

		if (cl->state != SV_CLIENT_ACTIVE)
			continue;

		total = count = 0;
		for (j = 0; j < CLIENT_LATENCY_COUNTS; j++) {
			if (cl->frame_latency[j] > 0) {
				total += cl->frame_latency[j];
				count++;
			}
		}

		if (!count)
			cl->ping = 0;
		else
			cl->ping = total / count;

		// let the game module know about the ping
		cl->edict->client->ping = cl->ping;
	}
}

/*
 * Sv_CheckCommandTimes
 *
 * Once per second, gives all clients an allotment of 1000 milliseconds
 * for their movement commands which will be decremented as we receive
 * new information from them.  If they drift by a significant margin
 * over the next interval, assume they are trying to cheat.
 */
static void Sv_CheckCommandTimes(void) {
	static unsigned int last_check_time = -9999;
	int i;

	if (svs.real_time < last_check_time) { // wrap around from last level
		last_check_time = -9999;
	}

	// see if its time to check the movements
	if (svs.real_time - last_check_time < CMD_MSEC_CHECK_INTERVAL) {
		return;
	}

	last_check_time = svs.real_time;

	// inspect each client, ensuring they are reasonably in sync with us
	for (i = 0; i < sv_max_clients->integer; i++) {
		sv_client_t *cl = &svs.clients[i];

		if (cl->state < SV_CLIENT_ACTIVE) {
			continue;
		}

		if (sv_enforce_time->value) { // check them

			if (abs(cl->cmd_msec) > CMD_MSEC_ALLOWABLE_DRIFT) { // irregular movement
				cl->cmd_msec_errors++;

				Com_Debug("%s drifted %dms\n", Sv_NetaddrToString(cl),
						cl->cmd_msec);

				if (cl->cmd_msec_errors >= sv_enforce_time->value) {
					Com_Warn("Sv_CheckCommandTimes: Too many errors from %s\n",
							Sv_NetaddrToString(cl));
					Sv_KickClient(cl, "Irregular movement");
					continue;
				}
			} else { // normal movement

				if (cl->cmd_msec_errors) {
					cl->cmd_msec_errors--;
				}
			}
		}

		cl->cmd_msec = CMD_MSEC_CHECK_INTERVAL; // reset for next cycle
	}
}

/*
 * Sv_ReadPackets
 */
static void Sv_ReadPackets(void) {
	int i;
	sv_client_t *cl;
	byte qport;

	while (Net_GetPacket(NS_SERVER, &net_from, &net_message)) {

		// check for connectionless packet(0xffffffff) first
		if (*(int *) net_message.data == -1) {
			Sv_ConnectionlessPacket();
			continue;
		}

		// read the qport out of the message so we can fix up
		// stupid address translating routers
		Msg_BeginReading(&net_message);

		Msg_ReadLong(&net_message); // sequence number
		Msg_ReadLong(&net_message); // sequence number

		qport = Msg_ReadByte(&net_message) & 0xff;

		// check for packets from connected clients
		for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++) {

			if (cl->state == SV_CLIENT_FREE)
				continue;

			if (!Net_CompareClientNetaddr(net_from, cl->netchan.remote_address))
				continue;

			if (cl->netchan.qport != qport)
				continue;

			if (cl->netchan.remote_address.port != net_from.port) {
				Com_Warn("Sv_ReadPackets: Fixing up a translated port.\n");
				cl->netchan.remote_address.port = net_from.port;
			}

			// this is a valid, sequenced packet, so process it
			if (Netchan_Process(&cl->netchan, &net_message)) {
				cl->last_message = svs.real_time; // nudge timeout
				Sv_ParseClientMessage(cl);
			}

			// we've processed the packet for the correct client, so break
			break;
		}
	}
}

/*
 * Sv_CheckTimeouts
 */
static void Sv_CheckTimeouts(void) {
	sv_client_t *cl;
	int i;

	const unsigned int timeout = svs.real_time - 1000 * sv_timeout->value;

	if (timeout > svs.real_time) {
		// the server is just starting, don't bother
		return;
	}

	for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++) {

		if (cl->state == SV_CLIENT_FREE)
			continue;

		// enforce timeouts by dropping the client
		if (cl->last_message < timeout) {
			Sv_BroadcastPrint(PRINT_HIGH, "%s timed out\n", cl->name);
			Sv_DropClient(cl);
		}
	}
}

/*
 * Sv_ResetEntities
 *
 * Resets entity flags and other state which should only last one frame.
 */
static void Sv_ResetEntities(void) {
	unsigned int i;

	if (sv.state != SV_ACTIVE_GAME)
		return;

	for (i = 0; i < svs.game->num_edicts; i++) {

		g_edict_t *edict = EDICT_FOR_NUM(i);

		// events only last for a single message
		edict->s.event = 0;
	}
}

/*
 * Sv_RunGameFrame
 *
 * Updates the game module's time and runs its frame function once per
 * server frame.
 */
static void Sv_RunGameFrame(void) {

	sv.frame_num++;
	sv.time = sv.frame_num * 1000 / svs.frame_rate;

	if (sv.time < svs.real_time) {
		Com_Debug("Sv_RunGameFrame: High clamp: %dms\n",
				svs.real_time - sv.time);
		svs.real_time = sv.time;
	}

	if (sv.state == SV_ACTIVE_GAME) {
		svs.game->Frame();
	}
}

/*
 * Sv_InitMasters
 */
static void Sv_InitMasters(void) {

	memset(&svs.masters, 0, sizeof(svs.masters));

	// set default master server
	Net_StringToNetaddr(IP_MASTER, &svs.masters[0]);
	svs.masters[0].port = BigShort(PORT_MASTER);
}

#define HEARTBEAT_SECONDS 300

/*
 * Sv_HeartbeatMasters
 *
 * Sends heartbeat messages to master servers every 300s.
 */
static void Sv_HeartbeatMasters(void) {
	const char *string;
	int i;

	if (!dedicated->value)
		return; // only dedicated servers report to masters

	if (!sv_public->value)
		return; // a private dedicated game

	if (!svs.initialized) // we're not up yet
		return;

	if (svs.last_heartbeat > svs.real_time) // catch wraps
		svs.last_heartbeat = svs.real_time;

	if (svs.last_heartbeat) { // if we've sent one, wait a while
		if (svs.real_time - svs.last_heartbeat < HEARTBEAT_SECONDS * 1000)
			return; // not time to send yet
	}

	svs.last_heartbeat = svs.real_time;

	// send the same string that we would give for a status command
	string = Sv_StatusString();

	// send to each master server
	for (i = 0; i < MAX_MASTERS; i++) {
		if (svs.masters[i].port) {
			Com_Print("Sending heartbeat to %s\n",
					Net_NetaddrToString(svs.masters[i]));
			Netchan_OutOfBandPrint(NS_SERVER, svs.masters[i], "heartbeat\n%s",
					string);
		}
	}
}

/*
 * Sv_ShutdownMasters
 *
 * Informs master servers that this server is halting.
 */
static void Sv_ShutdownMasters(void) {
	int i;

	if (!dedicated->value)
		return; // only dedicated servers send heartbeats

	if (!sv_public->value)
		return; // a private dedicated game

	// send to group master
	for (i = 0; i < MAX_MASTERS; i++) {
		if (svs.masters[i].port) {
			Com_Print("Sending shutdown to %s\n",
					Net_NetaddrToString(svs.masters[i]));
			Netchan_OutOfBandPrint(NS_SERVER, svs.masters[i], "shutdown");
		}
	}
}

/*
 * Sv_KickClient
 */
void Sv_KickClient(sv_client_t *cl, const char *msg) {
	char buf[1024], name[32];

	if (!cl)
		return;

	if (cl->state < SV_CLIENT_CONNECTED)
		return;

	if (*cl->name == '\0') // force a name to kick
		strncpy(name, "player", sizeof(name) - 1);
	else
		strncpy(name, cl->name, sizeof(name) - 1);

	memset(buf, 0, sizeof(buf));

	if (msg && *msg != '\0')
		snprintf(buf, sizeof(buf), ": %s", msg);

	Sv_ClientPrint(cl->edict, PRINT_HIGH, "You were kicked%s\n", buf);

	Sv_DropClient(cl);

	Sv_BroadcastPrint(PRINT_HIGH, "%s was kicked%s\n", name, buf);
}

/*
 * Sv_NetaddrToString
 *
 * A convenience function for printing out client addresses.
 */
char *Sv_NetaddrToString(sv_client_t *cl) {
	return Net_NetaddrToString(cl->netchan.remote_address);
}

#define MIN_RATE 8000
#define DEFAULT_RATE 20000

/*
 * Sv_UserInfoChanged
 *
 * Enforces safe user_info data before passing onto game module.
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
		Com_Print("Illegal user_info contained xFF from %s\n",
				Sv_NetaddrToString(cl));
		Sv_KickClient(cl, "Bad user info");
		return;
	}

	if (!ValidateUserInfo(cl->user_info)) { // catch otherwise invalid user_info
		Com_Print("Invalid user_info from %s\n", Sv_NetaddrToString(cl));
		Sv_KickClient(cl, "Bad user info");
		return;
	}

	val = GetUserInfo(cl->user_info, "skin");
	if (strstr(val, "..")) // catch malformed skins
		SetUserInfo(cl->user_info, "skin", "enforcer/qforcer");

	// call game code to allow overrides
	svs.game->ClientUserInfoChanged(cl->edict, cl->user_info);

	// name for C code, mask off high bit
	strncpy(cl->name, GetUserInfo(cl->user_info, "name"), sizeof(cl->name) - 1);
	for (i = 0; i < sizeof(cl->name); i++) {
		cl->name[i] &= 127;
	}

	// rate command
	val = GetUserInfo(cl->user_info, "rate");
	if (*val != '\0') {
		cl->rate = atoi(val);

		if (cl->rate > CLIENT_RATE_MAX)
			cl->rate = CLIENT_RATE_MAX;
		else if (cl->rate < CLIENT_RATE_MIN)
			cl->rate = CLIENT_RATE_MIN;
	}

	// limit the print messages the client receives
	val = GetUserInfo(cl->user_info, "message_level");
	if (*val != '\0') {
		cl->message_level = atoi(val);
	}

	// start/stop sending view angles for demo recording
	val = GetUserInfo(cl->user_info, "recording");
	cl->recording = atoi(val) == 1;
}

/*
 * Sv_Frame
 */
void Sv_Frame(unsigned int msec) {
	unsigned int frame_millis;

	// if server is not active, do nothing
	if (!svs.initialized)
		return;

	// update time reference
	svs.real_time += msec;

	// keep the random time dependent
	rand();

	// check timeouts
	Sv_CheckTimeouts();

	// get packets from clients
	Sv_ReadPackets();

	frame_millis = 1000 / svs.frame_rate;

	// keep the game module's time in sync with reality
	if (!time_demo->value && svs.real_time < sv.time) {

		// if the server has fallen far behind the game, try to catch up
		if (sv.time - svs.real_time > frame_millis) {
			Com_Debug("Sv_Frame: Low clamp: %dms.\n",
					(sv.time - svs.real_time - frame_millis));
			svs.real_time = sv.time - frame_millis;
		} else { // wait until its time to run the next frame
			Net_Sleep(sv.time - svs.real_time);
			return;
		}
	}

	// update ping based on the last known frame from all clients
	Sv_UpdatePings();

	// give the clients some timeslices
	Sv_CheckCommandTimes();

	// let everything in the world think and move
	Sv_RunGameFrame();

	// send messages back to the clients that had packets read this frame
	Sv_SendClientMessages();

	// send a heartbeat to the master if needed
	Sv_HeartbeatMasters();

	// clear entity flags, etc for next frame
	Sv_ResetEntities();

#ifdef HAVE_CURSES
	Curses_Frame(msec);
#endif
}

/*
 * Sv_Init
 *
 * Only called at Quake2World startup, not for each game.
 */
void Sv_Init(void) {

	memset(&svs, 0, sizeof(svs));

	sv_rcon_password = Cvar_Get("rcon_password", "", 0, NULL);

	sv_download_url = Cvar_Get("sv_download_url", "", CVAR_SERVER_INFO, NULL);
	sv_enforce_time = Cvar_Get("sv_enforce_time",
			va("%d", CMD_MSEC_MAX_DRIFT_ERRORS), 0, NULL);

	sv_hostname = Cvar_Get("sv_hostname", "Quake2World",
			CVAR_SERVER_INFO | CVAR_ARCHIVE, NULL);
	sv_public = Cvar_Get("sv_public", "0", 0, NULL);

	if (dedicated->value)
		sv_max_clients = Cvar_Get("sv_max_clients", "8",
				CVAR_SERVER_INFO | CVAR_LATCH, NULL);
	else
		sv_max_clients = Cvar_Get("sv_max_clients", "1",
				CVAR_SERVER_INFO | CVAR_LATCH, NULL);

	sv_framerate = Cvar_Get("sv_framerate", va("%d", SERVER_FRAME_RATE),
			CVAR_SERVER_INFO | CVAR_LATCH, NULL);
	sv_timeout = Cvar_Get("sv_timeout", va("%d", SERVER_TIMEOUT), 0, NULL);
	sv_udp_download = Cvar_Get("sv_udp_download", "1", CVAR_ARCHIVE, NULL);

	// set this so clients and server browsers can see it
	Cvar_Get("sv_protocol", va("%i", PROTOCOL), CVAR_SERVER_INFO | CVAR_NO_SET,
			NULL);

	Sv_InitCommands();

	Sv_InitMasters();

	Sb_Init(&net_message, net_message_buffer, sizeof(net_message_buffer));

	Net_Config(NS_SERVER, true);
}

/*
 * Sv_Shutdown
 *
 * Called when server is shutting down due to error or an explicit `quit`.
 */
void Sv_Shutdown(const char *msg) {

	Sv_ShutdownServer(msg);

	Sv_ShutdownMasters();

	Net_Config(NS_SERVER, false);

	Sb_Init(&net_message, net_message_buffer, sizeof(net_message_buffer));

	memset(&svs, 0, sizeof(svs));
}
