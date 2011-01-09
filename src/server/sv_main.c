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

#include "server.h"
#include "console.h"

sv_client_t *sv_client;  // current client

cvar_t *sv_rcon_password;  // password for remote server commands

cvar_t *sv_download_url;
cvar_t *sv_enforcetime;
cvar_t *sv_extensions;
cvar_t *sv_hostname;
cvar_t *sv_maxclients;
cvar_t *sv_packetrate;
cvar_t *sv_public;
cvar_t *sv_timeout;
cvar_t *sv_udpdownload;


/*
 * Sv_DropClient
 *
 * Called when the player is totally leaving the server, either willingly
 * or unwillingly.  This is NOT called if the entire server is quitting
 * or crashing.
 */
void Sv_DropClient(sv_client_t *cl){

	if(cl->state > cs_free){  // send the disconnect

		if(cl->state == cs_spawned){  // after informing the game module
			ge->ClientDisconnect(cl->edict);
		}

		Msg_WriteByte(&cl->netchan.message, svc_disconnect);
		Netchan_Transmit(&cl->netchan, cl->netchan.message.size, cl->netchan.message.data);
	}

	if(cl->download){
		Fs_FreeFile(cl->download);
		cl->download = NULL;
	}

	cl->state = cs_free;
	cl->last_message = 0;
	cl->last_frame = -1;
	cl->name[0] = 0;
}


/*
 *
 * CONNECTIONLESS COMMANDS
 *
 */


/*
 * Sv_StatusString
 *
 * Builds the string that is sent as heartbeats and status replies
 */
static const char *Sv_StatusString(void){
	char player[1024];
	static char status[MAX_MSGLEN - 16];
	int i;
	sv_client_t *cl;
	int statusLength;
	int playerLength;

	strcpy(status, Cvar_ServerInfo());
	strcat(status, "\n");
	statusLength = strlen(status);

	for(i = 0; i < sv_maxclients->value; i++){
		cl = &svs.clients[i];
		if(cl->state == cs_connected || cl->state == cs_spawned){
			snprintf(player, sizeof(player), "%i %i \"%s\"\n",
						 cl->edict->client->ps.stats[STAT_FRAGS], cl->ping, cl->name);
			playerLength = strlen(player);
			if(statusLength + playerLength >= sizeof(status))
				break;  // can't hold any more
			strcpy(status + statusLength, player);
			statusLength += playerLength;
		}
	}

	return status;
}


/*
 * Svc_Status
 *
 * Responds with all the info that qplug or qspy can see
 */
static void Svc_Status(void){
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "print\n%s", Sv_StatusString());
}


/*
 * Svc_Ack
 */
static void Svc_Ack(void){
	Com_Print("Ping acknowledge from %s\n", Net_NetaddrToString(net_from));
}


/*
 * Svc_Info
 *
 * Responds with short info for broadcast scans
 */
static void Svc_Info(void){
	char string[256];
	char hostname[256];
	int i, count;
	int prot;

	if(sv_maxclients->value == 1)
		return;  // ignore in single player

	prot = atoi(Cmd_Argv(1));

	if(prot != PROTOCOL)
		snprintf(string, sizeof(string), "%s: wrong protocol version\n", sv_hostname->string);
	else {
		count = 0;

		for(i = 0; i < sv_maxclients->value; i++){
			if(svs.clients[i].state >= cs_connected)
				count++;
		}

		Com_StripColor(sv_hostname->string, hostname);

		snprintf(string, sizeof(string), "%-24.24s %-12.12s %02d/%02d\n",
				hostname, sv.name, count, (int)sv_maxclients->value);
	}

	Netchan_OutOfBandPrint(NS_SERVER, net_from, "info\n%s", string);
}


/*
 * Svc_Ping
 *
 * Just responds with an acknowledgement
 */
static void Svc_Ping(void){
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
static void Svc_GetChallenge(void){
	int i;
	int oldest;
	int oldestTime;

	oldest = 0;
	oldestTime = 0x7fffffff;

	// see if we already have a challenge for this ip
	for(i = 0; i < MAX_CHALLENGES; i++){

		if(Net_CompareClientNetaddr(net_from, svs.challenges[i].addr))
			break;

		if(svs.challenges[i].time < oldestTime){
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if(i == MAX_CHALLENGES){
		// overwrite the oldest
		svs.challenges[oldest].challenge = rand() & 0x7fff;
		svs.challenges[oldest].addr = net_from;
		svs.challenges[oldest].time = quake2world.time;
		i = oldest;
	}

	// send it back
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "challenge %i", svs.challenges[i].challenge);
}


/*
 * Svc_Connect
 *
 * A connection request that did not come from the master.
 */
static void Svc_Connect(void){
	char user_info[MAX_INFO_STRING];
	netaddr_t addr;
	int i;
	sv_client_t *cl, *newcl;
	edict_t *ent;
	int edictnum;
	int version;
	int qport;
	int challenge;

	Com_Debug("Svc_Connect()\n");

	addr = net_from;

	version = atoi(Cmd_Argv(1));

	// resolve protocol
	if(version != PROTOCOL){
		Netchan_OutOfBandPrint(NS_SERVER, addr,
				"print\nServer is version %d.\n", PROTOCOL);
		return;
	}

	qport = atoi(Cmd_Argv(2));

	challenge = atoi(Cmd_Argv(3));

	//copy user_info, leave room for ip stuffing
	strncpy(user_info, Cmd_Argv(4), sizeof(user_info) - 1 - 25);
	user_info[sizeof(user_info) - 1] = 0;

	if(*user_info == '\0'){  // catch empty user_info
		Com_Print("Empty user_info from %s\n", Net_NetaddrToString(addr));
		Netchan_OutOfBandPrint(NS_SERVER, addr, "print\nConnection refused.\n");
		return;
	}

	if(strchr(user_info, '\xFF')){  // catch end of message in string exploit
		Com_Print("Illegal user_info contained xFF from %s\n", Net_NetaddrToString(addr));
		Netchan_OutOfBandPrint(NS_SERVER, addr, "print\nConnection refused.\n");
		return;
	}

	if(strlen(Info_ValueForKey(user_info, "ip"))){  // catch spoofed ips
		Com_Print("Illegal user_info contained ip from %s\n", Net_NetaddrToString(addr));
		Netchan_OutOfBandPrint(NS_SERVER, addr, "print\nConnection refused.\n");
		return;
	}

	if(!Info_Validate(user_info)){  // catch otherwise invalid user_info
		Com_Print("Invalid user_info from %s\n", Net_NetaddrToString(addr));
		Netchan_OutOfBandPrint(NS_SERVER, addr, "print\nConnection refused.\n");
		return;
	}

	// force the ip so the game can filter on it
	Info_SetValueForKey(user_info, "ip", Net_NetaddrToString(addr));

	// enforce a valid challenge on remote clients to avoid dos attack
	if(!Net_IsLocalNetaddr(addr)){
		for(i = 0; i < MAX_CHALLENGES; i++){
			if(Net_CompareClientNetaddr(addr, svs.challenges[i].addr)){
				if(challenge == svs.challenges[i].challenge){
					svs.challenges[i].challenge = 0;
					break;  // good
				}
				Netchan_OutOfBandPrint(NS_SERVER, addr, "print\nBad challenge.\n");
				return;
			}
		}
		if(i == MAX_CHALLENGES){
			Netchan_OutOfBandPrint(NS_SERVER, addr, "print\nNo challenge for address.\n");
			return;
		}
	}

	// resolve the client slot
	newcl = NULL;

	// first check for an ungraceful reconnect (client crashed, perhaps)
	for(i = 0, cl = svs.clients; i < sv_maxclients->value; i++, cl++){

		const netchan_t *ch = &cl->netchan;

		if(cl->state == cs_free)  // not in use, not interested
			continue;

		// the base address and either the qport or real port must match
		if(Net_CompareClientNetaddr(addr, ch->remote_address) &&
				(qport == ch->qport || ch->remote_address.port == addr.port)){
			newcl = cl;
			break;
		}
	}

	// otherwise, treat as a fresh connect to a new slot
	if(!newcl){
		for(i = 0, cl = svs.clients; i < sv_maxclients->value; i++, cl++){
			if(cl->state == cs_free){  // we have a free one
				newcl = cl;
				break;
			}
		}
	}

	// no soup for you, next!!
	if(!newcl){
		Netchan_OutOfBandPrint(NS_SERVER, addr, "print\nServer is full.\n");
		Com_Debug("Rejected a connection.\n");
		return;
	}

	// this is the only place a client_t is ever initialized
	sv_client = newcl;
	edictnum = (newcl - svs.clients) + 1;
	ent = EDICT_FOR_NUM(edictnum);
	newcl->edict = ent;
	newcl->challenge = challenge; // save challenge for checksumming

	// get the game a chance to reject this connection or modify the user_info
	if(!(ge->ClientConnect(ent, user_info))){
		if(*Info_ValueForKey(user_info, "rejmsg"))
			Netchan_OutOfBandPrint(NS_SERVER, addr, "print\n%s\nConnection refused.\n",
									Info_ValueForKey(user_info, "rejmsg"));
		else
			Netchan_OutOfBandPrint(NS_SERVER, addr, "print\nConnection refused.\n");
		Com_Debug("Game rejected a connection.\n");
		return;
	}

	// parse some info from the info strings
	strncpy(newcl->user_info, user_info, sizeof(newcl->user_info) - 1);
	Sv_UserinfoChanged(newcl);

	// send the connect packet to the client
	Netchan_OutOfBandPrint(NS_SERVER, addr, "client_connect %s", sv_download_url->string);

	Netchan_Setup(NS_SERVER, &newcl->netchan, addr, qport);

	Sb_Init(&newcl->datagram, newcl->datagram_buf, sizeof(newcl->datagram_buf));
	newcl->datagram.allow_overflow = true;

	newcl->last_message = newcl->last_connect = svs.real_time;  // don't timeout

	newcl->state = cs_connected;
}


/*
 * Sv_RconAuthenticate
 */
static qboolean Sv_RconAuthenticate(void){

	// a password must be set for rcon to be available
	if(*sv_rcon_password->string == '\0')
		return false;

	// and of course the passwords must match
	if(strcmp(Cmd_Argv(1), sv_rcon_password->string))
		return false;

	return true;
}


/*
 * Svc_RemoteCommand
 *
 * A client issued an rcon command.  Shift down the remaining args and
 * redirect all output to the invoking client.
 */
static void Svc_RemoteCommand(void){
	const qboolean auth = Sv_RconAuthenticate();

	// first print to the server console
	if(auth)
		Com_Print("Rcon from %s:\n%s\n", Net_NetaddrToString(net_from), net_message.data + 4);
	else
		Com_Print("Bad rcon from %s:\n%s\n", Net_NetaddrToString(net_from), net_message.data + 4);

	// then redirect the remaining output back to the client
	Com_BeginRedirect(RD_PACKET, sv_outputbuf, SV_OUTPUTBUF_LENGTH, Sv_FlushRedirect);

	if(auth){
		char remaining[MAX_STRING_CHARS];
		int i;

		remaining[0] = 0;

		for(i = 2; i < Cmd_Argc(); i++){
			strcat(remaining, Cmd_Argv(i));
			strcat(remaining, " ");
		}

		Cmd_ExecuteString(remaining);
	}
	else {
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
static void Sv_ConnectionlessPacket(void){
	char *s;
	char *c;

	Msg_BeginReading(&net_message);
	Msg_ReadLong(&net_message);  // skip the -1 marker

	s = Msg_ReadStringLine(&net_message);

	Cmd_TokenizeString(s);

	c = Cmd_Argv(0);
	Com_Debug("Packet from %s: %s\n", Net_NetaddrToString(net_from), c);

	if(!strcmp(c, "ping"))
		Svc_Ping();
	else if(!strcmp(c, "ack"))
		Svc_Ack();
	else if(!strcmp(c, "status"))
		Svc_Status();
	else if(!strcmp(c, "info"))
		Svc_Info();
	else if(!strcmp(c, "getchallenge"))
		Svc_GetChallenge();
	else if(!strcmp(c, "connect"))
		Svc_Connect();
	else if(!strcmp(c, "rcon"))
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
static void Sv_UpdatePings(void){
	int i, j;
	sv_client_t *cl;
	int total, count;

	for(i = 0; i < sv_maxclients->value; i++){

		cl = &svs.clients[i];

		if(cl->state != cs_spawned)
			continue;

		total = count = 0;
		for(j = 0; j < LATENCY_COUNTS; j++){
			if(cl->frame_latency[j] > 0){
				total += cl->frame_latency[j];
				count++;
			}
		}

		if(!count)
			cl->ping = 0;
		else
			cl->ping = total / count;

		// let the game module know about the ping
		cl->edict->client->ping = cl->ping;
	}
}


/*
 * Sv_GiveMsec
 *
 * Every few frames, gives all clients an allotment of milliseconds
 * for their command moves.  If they exceed it, assume cheating.
 */
static void Sv_GiveMsec(void){
	int i;
	sv_client_t *cl;

	if(sv.frame_num & 15)  // TODO: this clearly does not work with variable packet rates
		return;

	for(i = 0; i < sv_maxclients->value; i++){

		cl = &svs.clients[i];

		if(cl->state == cs_free)  // nothing to worry about
			continue;

		if(!sv_enforcetime->value){
			cl->cmd_msec = MSEC_OKAY_MAX;  // reset for next cycle
			continue;
		}

		if(cl->cmd_msec < MSEC_OKAY_MIN || cl->cmd_msec > MSEC_OKAY_MAX){
			cl->cmd_msec_errors += MSEC_ERROR_STEP;  // irregular movement

			if(cl->cmd_msec_errors >= sv_enforcetime->value){
				Sv_KickClient(cl, " for irregular movement.");
	            continue;
			}
		}
		else if(cl->cmd_msec_errors > 0)  // normal movement
			cl->cmd_msec_errors += MSEC_OKAY_STEP;

		cl->cmd_msec = MSEC_OKAY_MAX;  // reset for next cycle
	}
}


/*
 * Sv_ReadPackets
 */
static void Sv_ReadPackets(void){
	int i;
	sv_client_t *cl;
	int qport;

	while(Net_GetPacket(NS_SERVER, &net_from, &net_message)){

		// check for connectionless packet(0xffffffff) first
		if(*(int *)net_message.data == -1){
			Sv_ConnectionlessPacket();
			continue;
		}

		// read the qport out of the message so we can fix up
		// stupid address translating routers
		Msg_BeginReading(&net_message);

		Msg_ReadLong(&net_message);  // sequence number
		Msg_ReadLong(&net_message);  // sequence number

		qport = Msg_ReadByte(&net_message) & 0xff;

		// check for packets from connected clients
		for(i = 0, cl = svs.clients; i < sv_maxclients->value; i++, cl++){

			if(cl->state == cs_free)
				continue;

			if(!Net_CompareClientNetaddr(net_from, cl->netchan.remote_address))
				continue;

			if(cl->netchan.qport != qport)
				continue;

			if(cl->netchan.remote_address.port != net_from.port){
				Com_Warn("Sv_ReadPackets: Fixing up a translated port.\n");
				cl->netchan.remote_address.port = net_from.port;
			}

			// this is a valid, sequenced packet, so process it
			if(Netchan_Process(&cl->netchan, &net_message)){
				cl->last_message = svs.real_time;  // nudge timeout
				Sv_ExecuteClientMessage(cl);
			}

			// we've processed the packet for the correct client, so break
			break;
		}
	}
}


/*
 * Sv_CheckTimeouts
 */
static void Sv_CheckTimeouts(void){
	int i;
	sv_client_t *cl;
	int timeout;

	timeout = svs.real_time - 1000 * sv_timeout->value;

	for(i = 0, cl = svs.clients; i < sv_maxclients->value; i++, cl++){

		if(cl->state == cs_free)
			continue;

		// wrap message times across level changes
		if(cl->last_message > svs.real_time)
			cl->last_message = svs.real_time;

		// enforce timeouts by dropping the client
		if(cl->last_message < timeout){
			Sv_BroadcastPrint(PRINT_HIGH, "%s timed out\n", cl->name);
			Sv_DropClient(cl);
		}
	}
}


/*
 * Sv_PrepWorldFrame
 *
 * This has to be done before the world logic, because
 * player processing happens outside RunWorldFrame
 */
static void Sv_PrepWorldFrame(void){
	int i;

	for(i = 0; i < ge->num_edicts; i++){

		edict_t *edict = EDICT_FOR_NUM(i);

		// events only last for a single message
		edict->s.event = 0;
	}
}


/*
 * Sv_RunGameFrame
 */
static void Sv_RunGameFrame(void){

	// we always need to bump frame_num, even if we don't run the world,
	// otherwise the delta compression can get confused when a client
	// has the "current" frame
	sv.frame_num++;
	sv.time = sv.frame_num * 1000 / svs.packet_rate;

	if(sv.state == ss_game)
		ge->RunFrame();

	// slow down for the game if need be
	if(sv.time < svs.real_time){
		Com_Debug("Sv_RunGameFrame: High clamp: +%dms\n", sv.time - svs.real_time);
		svs.real_time = sv.time;
	}
}


/*
 * Sv_InitMasters
 */
static void Sv_InitMasters(void){

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
static void Sv_HeartbeatMasters(void){
	const char *string;
	int i;

	if(!dedicated->value)
		return;  // only dedicated servers report to masters

	if(!sv_public->value)
		return;  // a private dedicated game

	if(svs.last_heartbeat > svs.real_time)  // catch wraps
		svs.last_heartbeat = svs.real_time;

	if(svs.last_heartbeat) {  // if we've sent one, wait a while
		if(svs.real_time - svs.last_heartbeat < HEARTBEAT_SECONDS * 1000)
			return;  // not time to send yet
	}

	svs.last_heartbeat = svs.real_time;

	// send the same string that we would give for a status command
	string = Sv_StatusString();

	// send to each master server
	for(i = 0; i < MAX_MASTERS; i++){
		if(svs.masters[i].port){
			Com_Print("Sending heartbeat to %s\n", Net_NetaddrToString(svs.masters[i]));
			Netchan_OutOfBandPrint(NS_SERVER, svs.masters[i], "heartbeat\n%s", string);
		}
	}
}


/*
 * Sv_ShutdownMasters
 *
 * Informs master servers that this server is halting.
 */
static void Sv_ShutdownMasters(void){
	int i;

	if(!dedicated->value)
		return;  // only dedicated servers send heartbeats

	if(!sv_public->value)
		return;  // a private dedicated game

	// send to group master
	for(i = 0; i < MAX_MASTERS; i++){
		if(svs.masters[i].port){
			Com_Print("Sending shutdown to %s\n", Net_NetaddrToString(svs.masters[i]));
			Netchan_OutOfBandPrint(NS_SERVER, svs.masters[i], "shutdown");
		}
	}
}


/*
 * Sv_KickClient
 */
void Sv_KickClient(sv_client_t *cl, const char *msg){
	char c[1024];

	if(!cl)
		return;

	if(cl->state < cs_connected)
		return;

	if(*cl->name == '\0')  // force a name to kick
		strcpy(cl->name, "player");

	memset(c, 0, sizeof(c));

	if(msg && *msg != '\0')
		snprintf(c, sizeof(c), ": %s", msg);

	Sv_BroadcastPrint(PRINT_HIGH, "%s was kicked%s\n", cl->name, c);
	Sv_ClientPrint(EDICT_FOR_CLIENT(cl), PRINT_HIGH, "You were kicked%s\n", c);

	Sv_DropClient(cl);
}


/*
 * Sv_NetaddrToString
 *
 * A convenience function for printing out client addresses.
 */
char *Sv_NetaddrToString(sv_client_t *cl){
	return Net_NetaddrToString(cl->netchan.remote_address);
}


#define MIN_RATE 8000
#define DEFAULT_RATE 20000

/*
 * Sv_UserinfoChanged
 *
 * Enforces safe user_info data before passing onto game module.
 */
void Sv_UserinfoChanged(sv_client_t *cl){
	char *val;
	int i;

	if(*cl->user_info == '\0'){  // catch empty user_info
		Com_Print("Empty user_info from %s\n", Sv_NetaddrToString(cl));
		Sv_KickClient(cl, NULL);
		return;
	}

	if(strchr(cl->user_info, '\xFF')){  // catch end of message exploit
		Com_Print("Illegal user_info contained xFF from %s\n", Sv_NetaddrToString(cl));
		Sv_KickClient(cl, NULL);
		return;
	}

	if(!Info_Validate(cl->user_info)){  // catch otherwise invalid user_info
		Com_Print("Invalid user_info from %s\n", Sv_NetaddrToString(cl));
		Sv_KickClient(cl, NULL);
		return;
	}

	val = Info_ValueForKey(cl->user_info, "skin");
	if(strstr(val, ".."))  // catch malformed skins
		Info_SetValueForKey(cl->user_info, "skin", "ichabod/ichabod");

	// call prog code to allow overrides
	ge->ClientUserinfoChanged(cl->edict, cl->user_info);

	// name for C code, mask off high bit
	strncpy(cl->name, Info_ValueForKey(cl->user_info, "name"), sizeof(cl->name) - 1);
	for(i = 0; i < sizeof(cl->name); i++){
		cl->name[i] &= 127;
	}

	// rate command
	val = Info_ValueForKey(cl->user_info, "rate");
	if(*val != '\0'){
		cl->rate = atoi(val);

		if(cl->rate > CLIENT_RATE_MAX)
			cl->rate = CLIENT_RATE_MAX;
		else if(cl->rate < CLIENT_RATE_MIN)
			cl->rate = CLIENT_RATE_MIN;
	}

	// limit the print messages the client receives
	val = Info_ValueForKey(cl->user_info, "message_level");
	if(*val != '\0'){
		cl->message_level = atoi(val);
	}

	// start/stop sending view angles for demo recording
	val = Info_ValueForKey(cl->user_info, "recording");
	cl->recording = atoi(val) == 1;

	// quake2world extensions
	val = Info_ValueForKey(cl->user_info, "extensions");
	cl->extensions = atoi(val);
}


/*
 * Sv_Frame
 */
void Sv_Frame(int msec){
	int frame_millis;

	// if server is not active, do nothing
	if(!svs.initialized)
		return;

	// update time reference
	svs.real_time += msec;

	// keep the random time dependent
	rand();

	// check timeouts
	Sv_CheckTimeouts();

	// get packets from clients
	Sv_ReadPackets();

	frame_millis = 1000 / svs.packet_rate;

	// move autonomous things around if enough time has passed
	if(!timedemo->value && svs.real_time < sv.time){
		// never let the time get too far off
		if(sv.time - svs.real_time > frame_millis){
			Com_Debug("Sv_Frame: High clamp: +%dms.\n", (sv.time - svs.real_time - frame_millis));
			svs.real_time = sv.time - frame_millis;
		}
		Net_Sleep(sv.time - svs.real_time);
		return;
	}

	// update ping based on the last known frame from all clients
	Sv_UpdatePings();

	// give the clients some timeslices
	Sv_GiveMsec();

	// let everything in the world think and move
	Sv_RunGameFrame();

	// send messages back to the clients that had packets read this frame
	Sv_SendClientMessages();

	// were we shutdown this frame?
	if(!svs.initialized)
		return;

	// send a heartbeat to the master if needed
	Sv_HeartbeatMasters();

	// clear teleport flags, etc for next frame
	Sv_PrepWorldFrame();

#ifdef HAVE_CURSES
	Curses_Frame(msec);
#endif
}


/*
 * Sv_Init
 *
 * Only called at Quake2World startup, not for each game.
 */
void Sv_Init(void){
	int bits;

	memset(&svs, 0, sizeof(svs));

	sv_rcon_password = Cvar_Get("rcon_password", "", 0, NULL);

	sv_download_url = Cvar_Get("sv_download_url", "", CVAR_SERVER_INFO, NULL);
	sv_enforcetime = Cvar_Get("sv_enforcetime", va("%d", MSEC_ERROR_MAX), 0, NULL);

	bits = QUAKE2WORLD_ZLIB;
	sv_extensions = Cvar_Get("sv_extensions", va("%d", bits), 0, NULL);

	sv_hostname = Cvar_Get("sv_hostname", "Quake2World", CVAR_SERVER_INFO | CVAR_ARCHIVE, NULL);
	sv_public = Cvar_Get("sv_public", "0", 0, NULL);

	if(dedicated->value)
		sv_maxclients = Cvar_Get("sv_maxclients", "8", CVAR_SERVER_INFO | CVAR_LATCH, NULL);
	else
		sv_maxclients = Cvar_Get("sv_maxclients", "1", CVAR_SERVER_INFO | CVAR_LATCH, NULL);

	sv_packetrate = Cvar_Get("sv_packetrate", va("%d", SERVER_PACKETRATE), CVAR_SERVER_INFO | CVAR_LATCH, NULL);
	sv_timeout = Cvar_Get("sv_timeout", va("%d", SERVER_TIMEOUT), 0, NULL);
	sv_udpdownload = Cvar_Get("sv_udpdownload", "1", CVAR_ARCHIVE, NULL);

	// set this so clients and server browsers can see it
	Cvar_Get("sv_protocol", va("%i", PROTOCOL), CVAR_SERVER_INFO | CVAR_NOSET, NULL);

	Sv_InitOperatorCommands();

	Sv_InitMasters();

	// initialize net buffer
	Sb_Init(&net_message, net_message_buffer, sizeof(net_message_buffer));
}


/*
 * Sv_FinalMessage
 *
 * Used by Sv_Shutdown and Sv_RestartGame to send a final message to all
 * connected clients.  The messages are sent immediately, because the server
 * could completely exit after returning from this function.
 */
void Sv_FinalMessage(const char *msg, qboolean reconnect){
	sv_client_t *cl;
	int i;

	if(!svs.initialized)
		return;

	Sb_Clear(&net_message);

	if(msg){  // send message
		Msg_WriteByte(&net_message, svc_print);
		Msg_WriteByte(&net_message, PRINT_HIGH);
		Msg_WriteString(&net_message, msg);
	}

	if(reconnect)  // send reconnect
		Msg_WriteByte(&net_message, svc_reconnect);
	else  // or just disconnect
		Msg_WriteByte(&net_message, svc_disconnect);

	for(i = 0, cl = svs.clients; i < sv_maxclients->value; i++, cl++)
		if(cl->state >= cs_connected)
			Netchan_Transmit(&cl->netchan, net_message.size, net_message.data);
}


/*
 * Sv_Shutdown
 *
 * Called when server is shutting down due to error or an explicit `quit`.
 */
void Sv_Shutdown(const char *msg){

	Sv_FinalMessage(msg, false);

	Sv_ShutdownMasters();

	Sv_ShutdownGameProgs();

	Net_Config(NS_SERVER, false);

	// close any open demo files
	if(sv.demo_file)
		Fs_CloseFile(sv.demo_file);

	memset(&sv, 0, sizeof(sv));
	Com_SetServerState(sv.state);

	// free server static data
	if(svs.clients)
		Z_Free(svs.clients);

	if(svs.entity_states)
		Z_Free(svs.entity_states);

	memset(&svs, 0, sizeof(svs));
}
