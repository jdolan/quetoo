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

#include "cl_local.h"
#include "server/server.h"
#include "parse.h"

cvar_t *cl_chat_sound;
cvar_t *cl_draw_counters;
cvar_t *cl_draw_position;
cvar_t *cl_draw_net_graph;
cvar_t *cl_editor;
cvar_t *cl_ignore;
cvar_t *cl_max_fps;
cvar_t *cl_no_lerp;
cvar_t *cl_team_chat_sound;
cvar_t *cl_timeout;

cvar_t *name;
cvar_t *message_level;
cvar_t *password;
cvar_t *rate;

cvar_t *qport;

cvar_t *rcon_address;
cvar_t *rcon_password;

cvar_t *cl_draw_net_messages;
cvar_t *cl_draw_renderer_stats;
cvar_t *cl_draw_sound_stats;

cl_static_t cls;
cl_client_t cl;

/**
 * @brief We have gotten a challenge from the server, so try and connect.
 */
static void Cl_SendConnect(void) {
	net_addr_t addr;

	memset(&addr, 0, sizeof(addr));

	if (!Net_StringToNetaddr(cls.server_name, &addr)) {
		Com_Print("Bad server address\n");
		cls.connect_time = 0;
		return;
	}

	if (addr.port == 0) {
		addr.port = htons(PORT_SERVER);
	}

	Netchan_OutOfBandPrint(NS_UDP_CLIENT, &addr, "connect %i %i %u \"%s\"\n", PROTOCOL_MAJOR,
	                       qport->integer, cls.challenge, Cvar_UserInfo());

	cvar_user_info_modified = false;
}

/**
 * @brief Re-send a connect message if the last one has timed out.
 */
static void Cl_AttemptConnect(void) {

	// if the local server is running and we aren't then connect
	if (Com_WasInit(QUETOO_SERVER) && g_strcmp0(cls.server_name, "localhost")) {

		if (cls.state > CL_DISCONNECTED) {
			Cl_Disconnect();
		}

		g_strlcpy(cls.server_name, "localhost", sizeof(cls.server_name));

		cls.state = CL_CONNECTING;
		cls.connect_time = 0;
	}

	// re-send if we haven't received a reply yet
	if (cls.state != CL_CONNECTING) {
		return;
	}

	// don't flood connection packets
	if (cls.connect_time && (quetoo.ticks - cls.connect_time < 1000)) {
		return;
	}

	net_addr_t addr;

	if (!Net_StringToNetaddr(cls.server_name, &addr)) {
		Com_Warn("Bad server address: %s\n", cls.server_name);
		cls.state = CL_DISCONNECTED;
		return;
	}

	if (addr.port == 0) {
		addr.port = htons(PORT_SERVER);
	}

	cls.connect_time = quetoo.ticks;

	const char *s = Net_NetaddrToString(&addr);
	if (g_strcmp0(cls.server_name, s)) {
		Com_Print("Connecting to %s (%s)...\n", cls.server_name, s);
	} else {
		Com_Print("Connecting to %s...\n", cls.server_name);
	}

	Netchan_OutOfBandPrint(NS_UDP_CLIENT, &addr, "get_challenge\n");
}

/**
 * @brief Initiates the connection process to the specified server.
 */
void Cl_Connect(const net_addr_t *addr) {

	if (Com_WasInit(QUETOO_SERVER)) { // if running a local server, kill it
		Sv_ShutdownServer("Server quit\n");
	}

	Cl_Disconnect();

	g_strlcpy(cls.server_name, Net_NetaddrToString(addr), sizeof(cls.server_name));

	cls.state = CL_CONNECTING;
	cls.connect_time = 0;
}

/**
 * @brief
 */
static void Cl_Connect_f(void) {
	net_addr_t addr;

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <address>\n", Cmd_Argv(0));
		return;
	}

	if (Net_StringToNetaddr(Cmd_Argv(1), &addr)) {
		Cl_Connect(&addr);
	} else {
		Com_Print("Invalid server address: %s\n", Cmd_Argv(1));
	}
}

/**
 * @brief Send the rest of the command line over as an unconnected command.
 */
static void Cl_Rcon_f(void) {
	char message[1024];
	int32_t i;
	net_addr_t to;

	if (!rcon_password->string) {
		Com_Print("No rcon_password set\n");
		return;
	}

	memset(&to, 0, sizeof(to));

	message[0] = (char) 255;
	message[1] = (char) 255;
	message[2] = (char) 255;
	message[3] = (char) 255;
	message[4] = 0;

	strcat(message, "rcon ");
	strcat(message, rcon_password->string);
	strcat(message, " ");

	for (i = 1; i < Cmd_Argc(); i++) {
		strcat(message, Cmd_Argv(i));
		strcat(message, " ");
	}

	if (cls.state >= CL_CONNECTED) {
		to = cls.net_chan.remote_address;
	} else {
		if (*rcon_address->string == '\0') {
			Com_Print("Not connected and no rcon_address set\n");
			return;
		}

		if (!Net_StringToNetaddr(rcon_address->string, &to)) {
			Com_Warn("Invalid rcon_address: %s\n", rcon_address->string);
			return;
		}

		if (to.port == 0) {
			to.port = htons(PORT_SERVER);
		}
	}

	Net_SendDatagram(NS_UDP_CLIENT, &to, message, strlen(message) + 1);
}

/**
 * @brief Client implementation of Cmd_ForwardToServer. Any commands not recognized
 * locally by the client will be sent to the server. Some will undergo parameter
 * expansion so that players can use macros for locations, weapons, etc.
 */
static void Cl_ForwardCmdToServer(void) {

	if (cls.state <= CL_DISCONNECTED) {
		Com_Print("%s: Not connected\n", Cmd_Argv(0));
		return;
	}

	const char *cmd = Cmd_Argv(0);
	const char *args = Cmd_Args();

	Net_WriteByte(&cls.net_chan.message, CL_CMD_STRING);
	Net_WriteString(&cls.net_chan.message, va("%s %s", cmd, args));

	//Com_Debug("Forwarding '%s %s'\n", cmd, args);
}

/**
 * @brief
 */
void Cl_ClearState(void) {

	if (Com_WasInit(QUETOO_CGAME)) {
		cls.cgame->ClearState();
	}

	Cl_ClearInput();

	S_Stop();

	// wipe the entire cl_client_t structure
	memset(&cl, 0, sizeof(cl));

	Mem_ClearBuffer(&cls.net_chan.message);
}

/**
 * @brief Sends the disconnect command to the server.
 */
void Cl_SendDisconnect(void) {
	byte cmd[16];

	cmd[0] = CL_CMD_STRING;
	strcpy((char *) cmd + 1, "disconnect");

	Netchan_Transmit(&cls.net_chan, cmd, strlen((char *) cmd));
}

/**
 * @brief Sends a disconnect message to the current server, stops any pending
 * demo recording, and updates cls.state so that we drop to console.
 */
void Cl_Disconnect(void) {

	if (cls.state <= CL_DISCONNECTED) {
		return;
	}

	Com_Print("Disconnecting from %s...\n", cls.server_name);

	Cl_SendDisconnect();

	Cl_ClearState();

	if (cls.demo_file) {
		Cl_Stop_f();
	}

	if (cls.download.file) {

		if (cls.download.http) {
			Cl_HttpDownload_Complete();
		} else {
			Fs_Close(cls.download.file);
		}

		memset(&cls.download, 0, sizeof(cls.download));
	}

	memset(cls.server_name, 0, sizeof(cls.server_name));

	cls.broadcast_time = 0;
	cls.connect_time = 0;
	cls.state = CL_DISCONNECTED;

	if (time_demo->value) {
		const vec_t s = (quetoo.ticks - cl.time_demo_start) / 1000.0;
		Com_Print("%i frames, %3.2f seconds: %4.2ffps\n", cl.time_demo_frames, s,
				  cl.time_demo_frames / s);

		cl.time_demo_frames = cl.time_demo_start = 0;
	}

	Cl_SetKeyDest(KEY_UI);
}

/**
 * @brief
 */
static void Cl_Disconnect_f(void) {

	if (Com_WasInit(QUETOO_SERVER)) { // if running a local server, kill it
		Sv_ShutdownServer("Disconnected\n");
	}

	Cl_Disconnect();
}

/**
 * @brief
 */
void Cl_Reconnect_f(void) {

	if (cls.download.file) { // don't disrupt downloads
		return;
	}

	if (cls.server_name[0] != '\0') { // already connected

		if (cls.state >= CL_CONNECTING) {
			Cl_Disconnect();
		}

		cls.connect_time = 0; // fire immediately
		cls.state = CL_CONNECTING;
	} else {
		Com_Print("No server to reconnect to\n");
	}
}

/**
 * @brief Tells the cgame to show the last ERROR_DROP message.
 */
void Cl_Drop(const char *text) {
	//cls.cgame->Dialog(text, "Ok", "Reconnect", Cl_Reconnect_f);
}

/**
 * @brief Responses to broadcasts, etc
 */
static void Cl_ConnectionlessPacket(void) {

	Net_BeginReading(&net_message);
	Net_ReadLong(&net_message); // skip the -1

	const char *s = Net_ReadStringLine(&net_message);

	Cmd_TokenizeString(s);

	const char *c = Cmd_Argv(0);

	Com_Debug(DEBUG_CLIENT, "%s: %s\n", Net_NetaddrToString(&net_from), c);

	// server connection
	if (!g_strcmp0(c, "client_connect")) {

		if (cls.state == CL_CONNECTED) {
			Com_Warn("Ignoring duplicate connect from %s\n", Net_NetaddrToString(&net_from));
			return;
		}

		Netchan_Setup(NS_UDP_CLIENT, &cls.net_chan, &net_from, qport->integer);

		Net_WriteByte(&cls.net_chan.message, CL_CMD_STRING);
		Net_WriteString(&cls.net_chan.message, "new");

		cls.state = CL_CONNECTED;

		if (Cmd_Argc() == 2) { // http download url
			g_strlcpy(cls.download_url, Cmd_Argv(1), sizeof(cls.download_url));
		} else {
			cls.download_url[0] = '\0';
		}

		return;
	}

	// server responding to a status broadcast
	if (!g_strcmp0(c, "info")) {
		Cl_ParseServerInfo();
		return;
	}

	// print command from somewhere
	if (!g_strcmp0(c, "print")) {
		s = Net_ReadString(&net_message);
		Com_Print("%s", s);
		return;
	}

	// ping from somewhere
	if (!g_strcmp0(c, "ping")) {
		Netchan_OutOfBandPrint(NS_UDP_CLIENT, &net_from, "ack");
		return;
	}

	// servers list from master
	if (!g_strcmp0(c, "servers")) {
		Cl_ParseServers();
		return;
	}

	// challenge from the server we are connecting to
	if (!g_strcmp0(c, "challenge")) {
		if (cls.state != CL_CONNECTING) {
			Com_Warn("Ignoring challenge from %s\n", Net_NetaddrToString(&net_from));
			return;
		}
		cls.challenge = (uint32_t) strtoul(Cmd_Argv(1), NULL, 10);
		Cl_SendConnect();
		return;
	}

	Com_Warn("Unknown command: %s from %s\n", c, Net_NetaddrToString(&net_from));
}

/**
 * @brief
 */
static void Cl_ReadPackets(void) {

	memset(&net_from, 0, sizeof(net_from));

	while (Net_ReceiveDatagram(NS_UDP_CLIENT, &net_from, &net_message)) {

		// remote command packet
		if (*(int32_t *) net_message.data == -1) {
			Cl_ConnectionlessPacket();
			continue;
		}

		// dump it if not connected
		if (cls.state <= CL_CONNECTING) {
			Com_Debug(DEBUG_CLIENT, "%s: Unsolicited packet\n", Net_NetaddrToString(&net_from));
			continue;
		}

		// check for runt packets
		if (net_message.size < 8) {
			Com_Debug(DEBUG_CLIENT, "%s: Runt packet\n", Net_NetaddrToString(&net_from));
			continue;
		}

		// packet from server
		if (!Net_CompareNetaddr(&net_from, &cls.net_chan.remote_address)) {
			Com_Debug(DEBUG_CLIENT, "%s: Sequenced packet without connection\n", Net_NetaddrToString(&net_from));
			continue;
		}

		if (!Netchan_Process(&cls.net_chan, &net_message)) {
			continue; // wasn't accepted for some reason
		}

		Cl_ParseServerMessage();
	}

	// check timeout
	if (cls.state >= CL_CONNECTED) {

		const uint32_t delta = quetoo.ticks - cls.net_chan.last_received;
		if (delta > cl_timeout->value * 1000) {
			Com_Warn("%s: Timed out.\n", Net_NetaddrToString(&net_from));
			Cl_Disconnect();
		}
	}
}

/**
 * @brief
 */
static const char *Cl_Username(void) {
	const char *username = Sys_Username();

	if (username[0] == '\0') {
		username = "newbie";
	}

	return username;
}

/**
 * @brief Writes key bindings and archived cvars to quetoo.cfg.
 */
static void Cl_WriteConfiguration(void) {
	file_t *f;

	if (cls.state == CL_UNINITIALIZED) {
		return;
	}

	if (!(f = Fs_OpenWrite("quetoo.cfg"))) {
		Com_Warn("Couldn't write quetoo.cfg\n");
		return;
	}

	Fs_Print(f, "// generated by Quetoo, do not modify\n");
	Cl_WriteBindings(f);
	Cvar_WriteAll(f);
	Fs_Close(f);
}

/**
 * @brief
 */
static void Cl_InitLocal(void) {

	// register our variables
	cl_chat_sound = Cvar_Add("cl_chat_sound", "misc/chat", CVAR_ARCHIVE, "Path to the sound that is made when a chat message is received");
	cl_draw_counters = Cvar_Add("cl_draw_counters", "1", CVAR_ARCHIVE, "Draw the speed, fps and pps counters at the bottom-right");
	cl_draw_position = Cvar_Add("cl_draw_position", "0", CVAR_DEVELOPER, "Draw your current position to the screen");
	cl_draw_net_graph = Cvar_Add("cl_draw_net_graph", "1", CVAR_ARCHIVE, "Draw the net graph at the bottom-right");
	cl_editor = Cvar_Add("cl_editor", "0", CVAR_DEVELOPER, "Activate the in-game editor");
	cl_ignore = Cvar_Add("cl_ignore", "", 0, "A list of patterns that will be matched against incoming messages and ignored by your client");
	cl_max_fps = Cvar_Add("cl_max_fps", "0", CVAR_ARCHIVE, "The max FPS that your client will attempt to run at");
	cl_no_lerp = Cvar_Add("cl_no_lerp", "0", CVAR_DEVELOPER, "Disable frame interpolation");
	cl_team_chat_sound = Cvar_Add("cl_team_chat_sound", "misc/teamchat", CVAR_ARCHIVE, "Path to the sound that is made when a team chat message is received");
	cl_timeout = Cvar_Add("cl_timeout", "15.0", CVAR_ARCHIVE, "Time, in seconds, that you'll remain connected to a potentially dead server");

	// user info

	name = Cvar_Add("name", Cl_Username(), CVAR_USER_INFO | CVAR_ARCHIVE, "Your player name");
	message_level = Cvar_Add("message_level", "0", CVAR_USER_INFO | CVAR_ARCHIVE, "The lowest message level you'll receive");
	password = Cvar_Add("password", "", CVAR_USER_INFO, "Password to the server you want to connect to");
	rate = Cvar_Add("rate", "0", CVAR_USER_INFO | CVAR_ARCHIVE, "Your bandwidth throttle, or 0 for none");

	qport = Cvar_Add("qport", va("%d", Random() & 0xff), 0, NULL);

	rcon_address = Cvar_Add("rcon_address", "", 0, NULL);
	rcon_password = Cvar_Add("rcon_password", "", 0, NULL);

	cl_draw_net_messages = Cvar_Add("cl_draw_net_messages", "0", CVAR_DEVELOPER, NULL);
	cl_draw_renderer_stats = Cvar_Add("cl_draw_renderer_stats", "0", CVAR_DEVELOPER, NULL);
	cl_draw_sound_stats = Cvar_Add("cl_draw_sound_stats", "0", CVAR_DEVELOPER, NULL);

	// register our commands
	Cmd_Add("ping", Cl_Ping_f, CMD_CLIENT, NULL);
	Cmd_Add("servers", Cl_Servers_f, CMD_CLIENT, NULL);
	Cmd_Add("record", Cl_Record_f, CMD_CLIENT, NULL);
	Cmd_Add("fast_forward", Cl_FastForward_f, CMD_CLIENT, NULL);
	Cmd_Add("servers_list", Cl_Servers_List_f, CMD_CLIENT, NULL);
	Cmd_Add("slow_motion", Cl_SlowMotion_f, CMD_CLIENT, NULL);
	Cmd_Add("stop", Cl_Stop_f, CMD_CLIENT, NULL);
	Cmd_Add("connect", Cl_Connect_f, CMD_CLIENT, NULL);
	Cmd_Add("reconnect", Cl_Reconnect_f, CMD_CLIENT, NULL);
	Cmd_Add("disconnect", Cl_Disconnect_f, CMD_CLIENT, NULL);
	Cmd_Add("rcon", Cl_Rcon_f, CMD_CLIENT, NULL);
	Cmd_Add("precache", Cl_Precache_f, CMD_CLIENT, NULL);
	Cmd_Add("download", Cl_Download_f, CMD_CLIENT, NULL);
	Cmd_Add("save_config", Cl_WriteConfiguration, CMD_CLIENT, "Forces the configuration file to be written to disk");

	// forward anything we don't handle locally to the server
	Cmd_ForwardToServer = Cl_ForwardCmdToServer;
}

/**
 * @brief
 */
void Cl_Frame(const uint32_t msec) {
	static uint32_t frame_timestamp;
	static uint32_t frame_msec;

	if (dedicated->value) {
		return;
	}

	// update the simulation time
	cl.time += msec;

	// and the unclamped simulation time
	cl.unclamped_time += msec;

	// and the pending command duration
	frame_msec += msec;

	if (time_demo->value) { // accumulate timed demo statistics
		if (!cl.time_demo_start) {
			cl.time_demo_start = quetoo.ticks;
		}
		cl.time_demo_frames++;
	} else if (cl_max_fps->value > 0.0) { // cap render frame rate
		if (quetoo.ticks - frame_timestamp < 1000.0 / cl_max_fps->value) {
			return;
		}
	}

	Cl_AttemptConnect();

	Cl_HttpThink();

	Cl_UpdateMedia();

	Cl_ReadPackets();

	Cl_HandleEvents();

	if (cls.state == CL_ACTIVE) {

		Cl_UpdateMovementCommand(frame_msec);

		Cl_SendCommands();

		Cl_Interpolate();

		Cl_PredictMovement();

		Cl_UpdateView();
	} else {
		Cl_SendCommands();
	}

	Cl_UpdateScreen();

	S_Frame();

	cls.cgame->UpdateDiscord();

	frame_timestamp = quetoo.ticks;
	frame_msec = 0;
}

/**
 * @brief
 */
void Cl_Init(void) {

	Com_Print("Client initialization...\n");

	memset(&cls, 0, sizeof(cls));

	if (dedicated->value) {
		return; // nothing running on the client
	}

	cls.state = CL_DISCONNECTED;

	Cl_InitConsole();

	Cl_InitLocal();

	Cl_InitKeys();

	Net_Config(NS_UDP_CLIENT, true);

	S_Init();

	R_Init();

	Ui_Init();

	Cl_InitView();

	Cl_InitInput();

	Cl_InitHttp();

	Cl_ClearState();

	Cl_InitCgame();

	Cl_SetKeyDest(KEY_UI);

	Com_Print("Client initialized\n");
	Com_InitSubsystem(QUETOO_CLIENT);
}

/**
 * @brief
 */
void Cl_Shutdown(void) {

	if (dedicated->value) {
		return;
	}

	Com_Print("Client shutdown...\n");

	Cl_Disconnect();

	Cl_ShutdownHttp();

	Cl_ShutdownCgame();

	Ui_Shutdown();

	S_Shutdown();

	R_Shutdown();

	Cl_FreeServers();

	Cl_WriteConfiguration();

	Cl_ShutdownKeys();

	Cl_ShutdownConsole();

	Cmd_RemoveAll(CMD_CLIENT);

	Mem_FreeTag(MEM_TAG_CLIENT);

	Com_Print("Client down\n");
	Com_QuitSubsystem(QUETOO_CLIENT);
}
